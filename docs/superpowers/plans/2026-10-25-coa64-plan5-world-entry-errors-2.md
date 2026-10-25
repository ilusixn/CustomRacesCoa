# CoA64 Plan 5: World-Entry Errors II Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Clear the next layer of world-entry Lua errors: the widget methods deferred since Plan 3, and five small
modules. Each one is named by the Lua error capture (`-extlog` now logs every error text).

**Order change:** Plan 4's findings put character advancement next. The first error capture (2026-10-25, 3 minutes
in world) shows that the most frequent errors come from elsewhere:
- CombatFeedback.lua:107: 9,183 errors.
- C_MysticEnchant: 3,083.
- SendSystemMessage inside the error handler: 2,724.
- The widget methods `#Minimap:GetViewRadius` (259) and `#Frame:GetAllAttributes` (83).
- GlobalsF's UnitIsLevelScaling / UnitCastingSpellID.

So this plan takes the small modules first. MysticEnchant is Plan 6, and character advancement (CAMgr / CARules /
CAFilter / SpellApi) is Plan 7. The new order is ledgered as a ruling.

**Architecture, constraints, build command:** as in Plan 4. Pure module code goes in `X.cpp` with unit tests; client
code goes in `XClient.cpp`, reached through `Deps`. Original bugs are reproduced and ledgered. Decomp edits are marked.
Every task runs `cmd //c Tools\coa\check.bat`.

**Spec:** `docs/superpowers/specs/2026-10-25-coa64-client-design.md`. Reference:
`F:\WorkDir\ascension-extensions-reconstruction` (pin `1b9ebca`).

## Review Focus

1. Every widget whose registrar the original detours (Frame, Minimap, Model, MovieFrame) gets its methods. Minimap,
   Model and MovieFrame also receive Frame's methods, because their registrars call Frame's first.
2. `GetAllAttributes` on a frame with no attributes returns an empty table and never touches a null list.
3. Model methods on a model that has no M2 loaded yet: no crash.
4. A logout/relog resets each module's state.
5. The error capture stays off without `-extlog`.

---

### Task 1: Widget methods

**Files:**
- Create `CoA/Source/Modules/WidgetClient.cpp` (the registrar hook and `#Frame.GetAllAttributes`).
- Create `ModelMethodsClient.cpp` (AscModelMethods: `#Model` SetSpell / SetSpellVisual / GetSpell / SetUnitGUID / SetSheathe).
- Add the deferred `#Minimap` IsInside / GetViewRadius / SetPOIMaxRange (AscMisc) and `#MovieFrame` SetVolume (AscGlobalsB).
- Decomp edits: the four registrars (0x49E540 Frame, 0x57C040 Minimap, 0x9603D0 Model, 0x970610 MovieFrame) call a
  CoA hook, `g_widgetMethodsHook(widget, L)`, after their own table entries, as the original's detours do.

**Tests:** pure parts only (`LuaReg::ForEachWidgetMethod` already exists and is tested). Live: the GetViewRadius /
GetAllAttributes errors are gone.

### Task 2: GlobalsF, GlobalsI, AutoQuest, UnitAddon, ExtraActionButton

One module at a time: pure state and packets with tests, client natives through `Deps`, following each reconstruction
file. Each module's natives, packets and events must leave the inventory's missing list.

### Task 3: Re-measure; CombatFeedback

Live capture again. If CombatFeedback.lua:107 (`self.feedbackText` nil, so a frame runs `CombatFeedback_OnUpdate`
without `CombatFeedback_Initialize`) remains, find the frame and the reason its initialization did not run. Fix it, or
ledger it to its owning module. Also record the remaining errors, sorted by count, in the findings doc ("Plan 5
result").
