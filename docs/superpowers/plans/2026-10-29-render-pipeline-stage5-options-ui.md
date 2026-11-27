# Render pipeline stage 5 — Graphics-page options Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Three dropdowns (DLSS Mode, Frame Generation, NVIDIA Reflex) on the Video options' Graphics page, added at
runtime from C++, driving the existing `coaDlssMode` / `coaFrameGen` / `coaReflex` CVars, greyed out with a reason
where unsupported.

**Architecture:** A pure `RenderOptions` module decides, from facts gathered by a client (DXVK active, NVAPI GPU
architecture, what Streamline loaded and supports, Frame Generation's flip-present gate), each dropdown's state:
enabled or greyed, the reason, the entries offered, and whether a change needs a restart. A Lua native
`C_CoARender.GetOptionState()` hands that state to an injected Lua builder that creates the dropdowns on
`VideoOptionsResolutionPanel` and registers them with the panel's own Okay / Cancel / Default.

**Tech Stack:** C++20 (CoA static lib), NVAPI (`ThirdParty/nvapi`), the client's Lua 5.1 + FrameXML 3.3.5 options
framework, the `CoA/Tests` harness.

**Spec:** `docs/superpowers/specs/2026-10-26-streamline-rendering-design.md` (sections 2.5, 4, 6.6, 7.5, 9 Q-A).
Stage 4 outcome that changes the spec: Frame Generation is gated off (`FlipPresent()` false, commit 7968d71,
`docs/2026-10-29-render-pipeline-notes.md` "Frame Generation hang").

## Global Constraints

- C++ only; no MPQ / data-file changes; nothing written into `D:\WoW\CoA` (test in `D:\WoW\CoA-dev`).
- CVar names and values are fixed by stages 3–4: `coaDlssMode` 0 Off / 1 DLAA / 2 Quality / 3 Balanced /
  4 Performance / 5 Ultra Performance; `coaFrameGen` 0 Off / 1 = 2x / 2 = 3x / 3 = 4x; `coaReflex` 0 Off / 1 On /
  2 On + Boost. All default 0.
- Off means unchanged: with all three Off, the panel's other controls behave exactly as before and frames are
  pixel-identical.
- Streamline loads only when an option is non-zero at startup: going from all-Off to any option needs a restart;
  once loaded, mode changes are live (spec 9 Q-A).
- Greyed-out dropdowns show a reason (spec 4.3). Support is detected without Streamline: NVIDIA GPU via NVAPI,
  Turing (`NV_GPU_ARCHITECTURE_TU100` 0x160) or newer for DLSS and Reflex, Ada (0x190) or newer for Frame Generation,
  Blackwell (0x1B0) or newer for 3x / 4x, and DXVK active.
- Frame Generation stays unavailable while `FlipPresent()` is false (always, today).
- DLSS on disables Multisampling (the render pipeline runs only without MSAA).
- Inline comments 1–2 lines (`Tools/comment_budget.mjs`); rationale goes to `docs/2026-10-29-render-pipeline-notes.md`.

## Review Focus

1. **Okay / Cancel / Default round trip:** change a dropdown, press Cancel → CVar unchanged; Default → all three 0;
   Okay → CVars saved to `Config.wtf`. The stock panel's other controls keep working.
2. **Restart rule:** with Streamline not loaded, picking DLSS Quality must say it applies after a restart and must
   not try to turn DLSS on live; with Streamline loaded, switching modes applies on Okay without restart.
3. **Unsupported paths:** `-nodxvk` / non-NVIDIA / pre-Turing → all three greyed with the right reason, and an
   already-saved non-zero CVar is shown but not applied.
4. **Reflex lock:** Reflex shows at least On while Frame Generation is on (unreachable today, but the rule exists in
   `FrameGen::EffectiveReflex` and must not be contradicted by the UI).
5. **Multisampling:** DLSS on greys out the Multisampling dropdown and saves 1x; DLSS back to Off re-enables it
   without changing the user's MSAA choice back by itself.

---

### Task 1: RenderOptions decisions (pure)

**Files:**
- Create: `CoA/Source/Runtime/RenderOptions.h`, `CoA/Source/Runtime/RenderOptions.cpp`
- Test: `CoA/Tests/RenderOptionsTest.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace CoA::RenderOptions {
      enum class Reason { None, NoDxvk, NotNvidia, NeedsRtx20, NeedsRtx40, NotAvailableYet, DriverUnsupported };
      struct Facts {
          bool dxvk;               // DXVK renderer active
          bool nvidia;             // NVAPI initialised on an NVIDIA GPU
          uint32_t arch;           // NV_GPU_ARCHITECTURE_* (0: unknown)
          bool slLoaded;           // Streamline loaded at startup (CoA::Streamline::Loaded() any)
          CoA::Streamline::Features slSupported;   // per feature on this device (valid when slLoaded)
          bool flipPresent;        // Frame Generation's present gate
      };
      struct Option { bool enabled; Reason reason; uint32_t maxValue; };   // maxValue: highest CVar value offered
      struct State { Option dlss, frameGen, reflex; bool restartToApply; };
      State Decide(const Facts& f);
      // The saved values (dlss, fg, reflex) take effect only after a restart: Streamline was not loaded at startup
      // and at least one value is non-zero.
      bool NeedsRestart(bool slLoaded, int dlssAfter, int fgAfter, int reflexAfter);
      const char* ReasonText(Reason r);
  }
  ```

- [ ] **Step 1: Write the failing tests** in `CoA/Tests/RenderOptionsTest.cpp`:
  - `RenderOptions_NoDxvkGreysAll`: `Decide({false, true, 0x1B0, false, {}, false})` → all three `!enabled`,
    reason `NoDxvk`.
  - `RenderOptions_ArchitectureGates`: arch 0x130 (Pascal) → all `NeedsRtx20`; arch 0x170 → dlss/reflex enabled,
    frameGen `!enabled`; arch 0x1B0 → dlss enabled with `maxValue 5`, reflex `maxValue 2`.
  - `RenderOptions_FrameGenNeedsFlip`: arch 0x1B0, `flipPresent false` → frameGen `!enabled`, reason
    `NotAvailableYet`; `flipPresent true` → enabled, `maxValue 3`; arch 0x190 + flip → `maxValue 1`.
  - `RenderOptions_StreamlineOverrides`: `slLoaded true`, `slSupported.dlss false` → dlss `DriverUnsupported`.
  - `RenderOptions_RestartRule`: `NeedsRestart(false, 2, 0, 0)` true; `NeedsRestart(false, 0, 0, 0)` false;
    `NeedsRestart(true, 3, 0, 1)` false.
  - `RenderOptions_ReasonTextNonEmpty`: every `Reason` except `None` has non-empty `ReasonText`.

- [ ] **Step 2: Run** `out\claude_rp.bat` — Expected: compile failure (`RenderOptions.h` missing).

- [ ] **Step 3: Implement** `Decide`, `NeedsRestart`, `ReasonText`. Order of checks per option: DXVK, NVIDIA,
  architecture, (Frame Generation) flip gate, then — only when `slLoaded` — Streamline's per-feature support.
  `ReasonText` (approved by the user 2026-10-29): NoDxvk "Requires the Vulkan renderer", NotNvidia "Requires an
  NVIDIA graphics card", NeedsRtx20 "Requires an RTX 20 series or newer", NeedsRtx40 "Requires an RTX 40 series or
  newer", NotAvailableYet "Not available yet", DriverUnsupported "Not supported by your driver". Restart note:
  "Takes effect after restarting the game". The Frame Generation dropdown is shown greyed out, not hidden.

- [ ] **Step 4: Run** `out\claude_rp.bat` — Expected: all `RenderOptions_*` ok, suite green.

- [ ] **Step 5: Commit** `RenderOptions: per-option state for the Graphics page`.

### Task 2: Facts client (NVAPI architecture, DXVK, Streamline)

**Files:**
- Create: `CoA/Source/Runtime/RenderOptionsClient.cpp`; declare in `CoA/Source/Runtime/RenderOptions.h`:
  `namespace CoA::RenderOptionsClient { CoA::RenderOptions::Facts Facts(); }`
- Modify: `CoA/Source/CMakeLists.txt` (link `nvapi` into CoA for x64 and x86; the `nvapi` target exists in
  `ThirdParty/CMakeLists.txt`)
- Modify: `CoA/Source/Runtime/StreamlineClient.cpp` (expose `bool FlipPresent()` through
  `CoA::StreamlineClient::FlipPresent()` in `StreamlineClient.h`)

**Interfaces:**
- Consumes: `CoA::Dxvk::Active()`, `CoA::Streamline::Loaded()` / `Supported()`, `RenderOptions::Facts`.
- Produces: `CoA::RenderOptionsClient::Facts()` — cached after the first call except `slSupported`, which is read
  each call (device recreation).

- [ ] **Step 1:** Implement with `NvAPI_Initialize`, `NvAPI_EnumPhysicalGPUs` (first GPU), `NvAPI_GPU_GetArchInfo`
  (`NV_GPU_ARCH_INFO_VER`); any failure → `nvidia false`. Not unit-tested (thin OS glue, as the other clients);
  log one line `RenderOptions: NVIDIA arch 0x%x, DXVK %s` at first call.
- [ ] **Step 2: Run** `out\claude_rp.bat` — Expected: builds, suite green.
- [ ] **Step 3: Commit** `RenderOptions: facts from NVAPI, DXVK and Streamline`.

### Task 3: `C_CoARender.GetOptionState()` native

**Files:**
- Create: `CoA/Source/Modules/RenderOptionsUi.h`, `CoA/Source/Modules/RenderOptionsUi.cpp` (module with
  `s_bindings` and `Init`, registered like `Account.cpp:194`)
- Modify: the module init list that calls other modules' `Init` (find it with `grep -rn "Account::Init" CoA/Source`)
- Test: `CoA/Tests/RenderOptionsUiTest.cpp`

**Interfaces:**
- Produces: Lua `C_CoARender.GetOptionState()` → table
  `{ dlss = {enabled=bool, reason=string|nil, max=n}, frameGen = {...}, reflex = {...}, restart = bool }`
  where `restart` is `not slLoaded`. A pure helper `void PushState(lua_State*, const RenderOptions::State&, bool slLoaded)`
  builds it.

- [ ] **Step 1: Write the failing test** `RenderOptionsUi_PushStateTable`: create a Lua state as
  `HotPatchTest.cpp` does, `PushState` a known `State`, `lua_setglobal(L, "s")`, run
  `return s.dlss.enabled, s.frameGen.reason, s.reflex.max, s.restart` and check the four values.
- [ ] **Step 2: Run** — Expected: compile failure.
- [ ] **Step 3: Implement** `PushState` and the native (calls `RenderOptionsClient::Facts()` →
  `RenderOptions::Decide`). Check `Tools/coa/inventory.py` / its tests still pass with one more binding
  (`node --test "Tools/__tests__/*.test.mjs"` and `Tools/coa/tests`).
- [ ] **Step 4: Run** — Expected: green.
- [ ] **Step 5: Commit** `RenderOptions: C_CoARender.GetOptionState for the options UI`.

### Task 4: Injected Graphics-page dropdowns

**Files:**
- Create: `CoA/Source/Modules/RenderOptionsLua.inc` (the Lua builder as a raw string literal)
- Modify: `CoA/Source/Modules/RenderOptionsUi.cpp` (execute it once per world Lua state via
  `CoA::LuaReg::OnWorldRegistered`; the builder defers to `PLAYER_LOGIN` because the panel does not exist yet)
- Test: `CoA/Tests/RenderOptionsUiTest.cpp`

**Interfaces:**
- Consumes: `C_CoARender.GetOptionState()`; stock `UIDropDownMenu_*`, `BlizzardOptionsPanel_RegisterControl`,
  `VideoOptionsResolutionPanel` (measured in-game 2026-10-29: Resolution (0,-98), Refresh (199,-98),
  Multisampling `VideoOptionsResolutionPanelMultiSampleDropDown` (0,-154); each control has `.cvar`; the panel has
  `okay` / `cancel` / `default` and a `controls` list).
- Produces: frames `CoARenderDlssDropDown` at (199,-154), `CoARenderFrameGenDropDown` at (399,-98),
  `CoARenderReflexDropDown` at (399,-154) (TOPLEFT of `VideoOptionsResolutionPanel`), each with `.cvar` set and
  registered as a panel control; a small restart note under the DLSS dropdown when `state.restart` and the chosen
  value differs from the startup value.

- [ ] **Step 1: Write the failing tests:**
  - `RenderOptionsUi_LuaCompiles`: `luaL_loadbuffer` of the builder text succeeds (syntax).
  - `RenderOptionsUi_LuaNamesCVars`: the text contains `coaDlssMode`, `coaFrameGen`, `coaReflex`,
    `CoARenderDlssDropDown`, `BlizzardOptionsPanel_RegisterControl`, and does not contain `RestartGx`.
- [ ] **Step 2: Run** — Expected: compile failure (builder missing).
- [ ] **Step 3: Implement the builder:** entries per spec 6.6 (DLSS Off · DLAA · Quality · Balanced · Performance ·
  Ultra Performance; Frame Generation Off · 2x · 3x · 4x up to `max`; Reflex Off · On · On + Boost); greyed
  dropdown (`UIDropDownMenu_DisableDropDown`) with the reason in its tooltip and as grey text; Reflex shows at least
  On while Frame Generation's selection is non-zero; choosing a DLSS mode other than Off disables the Multisampling
  dropdown and selects its 1x entry; Off re-enables it.
- [ ] **Step 4: Run** — Expected: both tests ok, suite green.
- [ ] **Step 5: Commit** `RenderOptions: DLSS, Frame Generation and Reflex on the Graphics page`.

### Task 5: In-game proof and docs

**Files:**
- Modify: `CoA/DIFFERENCES.md` (the NVIDIA DLSS / Frame Generation entries: "on the Graphics page" instead of
  "console-only")
- Create: `docs/2026-10-29-render-pipeline-stage5-proof.md`

- [ ] **Step 1:** Deploy to `D:\WoW\CoA-dev`; CoATest plan: open `VideoOptionsFrame` on the Graphics page
  (`eval` `InterfaceOptionsFrame_OpenToCategory`/`VideoOptionsFrame:Show()` then select the panel), `shot`.
  Expected: three dropdowns in the free slots, not overlapping stock controls.
- [ ] **Step 2:** Review Focus 1–3 and 5 in game: Cancel / Default / Okay round trip (read CVars with `eval
  GetCVar("coaDlssMode")`), restart note from all-Off, live switch with Streamline loaded (log
  `DLSS: active (Balanced …)`), `-nodxvk` greyed state, Multisampling greyed with DLSS on.
- [ ] **Step 3:** Off-means-unchanged: all Off, `imgdiff.py` against a screenshot from the stage-4 exe at the same
  spot.
- [ ] **Step 4:** Write the proof page; update DIFFERENCES; commit `Stage 5 proof and docs`.
