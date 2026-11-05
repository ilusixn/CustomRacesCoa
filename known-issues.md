# Known Issues

## KI-001: ACCESS_VIOLATION (read) in CoA64.exe, Scarlet Monastery, while attacking Scarlet Myrmidon

**Status:** Open, reproduced 2 of 2 times in the latest crash logs.
**Severity:** High. It is a hard client crash.

### Source logs
- `D:\WoW\Coa\Errors\2026-11-05 17.57.56 Crash.txt` and `.dmp`
- `D:\WoW\Coa\Errors\2026-11-05 17.59.04 Crash.txt` and `.dmp`
- Related: `D:\WoW\Coa\MemoryBridge.log`. It shows nothing abnormal: one `invalid_argument` on command 3, then the comm test passed.

### Signature (identical in both crashes)
| Field | Value |
|---|---|
| Error | `#2232418436 (0x85100084) Fatal Exception` |
| Exception | `0xC0000005 ACCESS_VIOLATION` (read) |
| Faulting RIP | `0x00007FF641C20C94` (`0001:000FFC94`, CoA64.exe) |
| Module base | `0x00007FF641B20000` (R8 in the 17.59 dump). RVA is about `0x100C94`. |
| Faulting address | `0x00007FF602603618` |
| Build | WoWBuild 12340, DLL version `792084a2572c06bfd865feaf553c001f2bdd60fd` |

The trace shows only the crashing frame, with no callers. The `.dmp` files are needed for a real stack.

### Register hints (17.59.04)
`RAX=FFFFFFFFF0000000 RCX=RDI=00000000F0000000 RDX=RSI=R9=0x100 R10=0x400 RBP=0`.
- `F0000000` / `FFFFFFFFF0000000` looks like a sentinel or invalid index/handle that was sign-extended and used as an offset.
- The faulting address is about 0x3F_4000_0000 below the module base, so it is probably a bad base + (index*scale) computation.
- The `0x100` and `0x400` values look like size, stride or count constants.

### Context at crash
- Zone: Scarlet Monastery (cemetery WMO `MONESTARY_CEMETERY_023.wmo`).
- The player (Helix) was in melee, auto-attacking a **Scarlet Myrmidon** (`F1300010C7...`). The target was the same creature type in both crashes (different GUIDs).
- The current object track in crash 2 was a player or pet named Solaria.
- Last Lua event: `SPELL_UPDATE_USABLE`.
- Last M2s loaded: `spells\druid_infectedwounds.mdx` and `spells\cfx_paladin_sacredweapon_impact_recolored.m2`. These are spell effects, so a spell-visual or M2 path is a possible suspect.
- The last `FrameScript_Execute` was the `HelixCtx` secure-function snapshot script. It saves `CastSpellByName`, `StartAttack`, `TargetUnit` and similar into `_H.secure`, then calls `HelixLoad`. **This is our own injected code that ran right before the crash.** The crash is probably unrelated to it, but rule it out.
- Current Addon is `(null)`, so no addon Lua was running at the time.
- Lua memory was about 305 MB and process memory about 2.4–2.6 GB (peak up to 5.5 GB). That is not at the 4 GB limit for a 64-bit process, but the peak is high.

### Environment (possibly relevant)
- GPU: RTX 5080 plus AMD iGPU. D3D9 reports 3 adapters, all on `\\.\DISPLAY37`. Driver versions read `32767.65535.65535.65535`, which is a bogus or unparsed version.
- Custom CVars: `coaDlssMode=2`, `coaReflex=2`, `useNewNameplates=0` (only in the first crash), `gxFixLag=0`, `particleDensity_raid=0.4`, `maxFPS=145`, `gxResolution=2560x1440`.
- Addons loaded: DBM, Details, WeakAuras, DragonUI, Bagnon, LootCollector, Zygor and others.

### Hypotheses (most to least likely)
1. A bad index or handle (`0xF0000000`) is dereferenced in a client routine. Candidates are combat or spell-visual handling for a mob that has just died or been engaged, or an M2 or effect cache lookup.
2. Our hook or patch layer (HelixCtx, the `HD Patch: 1` hooks, the DLSS/Reflex integration) corrupts or invalidates a table or pointer. Check any hook near RVA `0x100C94`.
3. A specific effect (infected wounds or sacred weapon) hits a bad skin or M2 pointer.
4. A problem with the multi-adapter D3D9 enumeration. This is less likely, because the crash is not in the GX path.

### Next steps for the fixing agent
- Load both `.dmp` files in WinDbg or Visual Studio, or run `cdb -z <dmp> -c "!analyze -v; k; r; u @rip-40 @rip+20"`. Get the callers and the disassembly around RVA `0x100C94`.
- Map RVA `0x100C94` to a function with the project symbols or map file, and check which hook or patch covers it.
- Look at the `0x00007FF602603618` access and work out which structure it is indexing (RDI=`0xF0000000`, stride `0x100`/`0x400`).
- Reproduce: enter Scarlet Monastery (Cemetery) and melee Scarlet Myrmidons. Both crashes happened within about 1m30s of each other, so try the same pull. Then test with:
  - `HelixCtx` and Helix disabled
  - `coaDlssMode=0` and `coaReflex=0`
  - addons disabled (especially DBM and WeakAuras)
  - spell effect density lowered
- Add a null or sentinel guard (`idx == 0xF0000000` or similar) if the root cause is confirmed as an invalid handle.
- Also check the `invalid_argument` returned by MemoryBridge command 3. It is probably benign but should be confirmed.
