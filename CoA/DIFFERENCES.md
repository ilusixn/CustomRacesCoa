# Differences from the 32-bit client

## Exe patches (Ascension.exe vs 3.3.5a 12340)

Found by diffing `.text`/`.rdata` of `Ascension.exe` against two other 3.3.5a executables
(`world of warcraft 3.3.5a hd\wow.exe`, `StormforgeWOTLK\wow.exe`) with `Tools/coa/exediff.py`, then checked against
the decomp source (the original 12340 logic). A site counts as Ascension's only when Ascension.exe differs from
both references, or from the decomp where the references disagree.

| Site | Ascension.exe | Verdict | Where |
|---|---|---|---|
| `.rdata 0x9E2724` | `patch-*.MPQ` instead of `patch-?.MPQ` | ported | `PatchFiles.cpp` |
| `0x405E2F`, `0x5D0046`/`0x5D0069` | archive handle array 64 → 127 (and its chunk clamp) | ported (the clamp is the array's growth chunk size; our container needs no change) | `Client.cpp` `OpenArchives` |
| `.rdata 0x9E2A38` | log title `Ascension.gg` | ported | `Client.cpp` |
| `.rdata 0x9FC672` | chat literal `|Htuest:` (quest links in chat not rewritten) | ported | `ChatFrame.cpp` |
| `0x4648B3` | `CMSG_AUTH_SESSION` build 12344 | ported as `g_coaAuthSessionBuild` | `RealmConnection.cpp` |
| `0x4DA7FC`/`809`/`816` | glue signature failures load the frames anyway | ported | `CGlueMgr.cpp` |
| `0x816625`, `0x81663E`, `0x816694`, `0x81675F` | `FrameXML_CheckSignature` always returns 3 (valid): add-on `.toc.sig` failures never mark an add-on CORRUPT, and every add-on that passes is marked secure (`AddOns.cpp`: status 0), so add-ons run untainted and may call protected functions | ported (both reference exes carry the same patch -- a common private-server edit -- but the decomp is stock 12340 and the 32-bit pair behaves this way; found from Blizzard_CombatLog's "Corrupt" dialog) | `LoadXML.cpp` |
| `0x4DCCF0` | `ScanDLLStart` returns 0 | ported | `GlueScriptEvents_Scripts.cpp` |
| `0x4E0F53`–`0x4E0FC0` | random-class array 10 → 30 | ported | `CharacterCreation.cpp` |
| `0x40B7D0` → cave `0x4E5CB0` | sets `m_scandllOkayToLogIn = 1`, `m_scandllShouldBlockPlaying = 0`, loads `Extensions.dll` | ported (the CoA code is linked in; `CoA::Attach` runs at this point) | `Win64/WinMain.cpp` |
| `0x510089` | `jne` | not Ascension's: Stormforge has the same bytes; only the HD exe differs | — |
| `0x727431`/`0x72743E` | `cmp eax,4` / `ret 0` in `ClickToMove`'s autofollow block | dead code in the 32-bit client: Extensions.dll overwrites `0x727431` with `jmp` (`AscAttachLate`), so the block never runs. Ported with the DLL's patches | Plan for world modules |
| `0x68EDD7` | call target `0x68C321` instead of `0x684C21` | undecided: a one-byte change into the middle of a function inside `CGxDeviceD3d`; reached only on a D3D device path. Not ported; to be revisited if a renderer difference shows | — |
| `.rsrc` | Ascension icon and version info | not ported | — |

The glue digest-mismatch exit at `0x4DA8EB` is an Extensions.dll patch (`AscAttachPatches`), ported with the exe
patches because the login screen cannot load without it.

## Deliberate differences

From the design (`docs/superpowers/specs/2026-10-25-coa64-client-design.md`) and the reconstruction's own list:

- **Logon protocol.** Ascension's logon hooks (`0x8CCE00`, `0x9A83E0`, `0x9A88C0`) are not built: stock SRP6, as the
  CoA server expects. The reconstruction's native logon (`AscNativeLogon`, its `ASCENSION_NATIVE_LOGON` build option,
  default off) is not ported.
- **Interface hot patches.** The reconstruction's `AscHotPatch` (its `ASCENSION_HOT_PATCH` build; not in the original)
  is ported, always on (`HotPatch.cpp`): SMSG `0x8F0` server-sent interface Lua (source only, guarded, run once per Lua
  session, replayed after a `/reload`, the player's choice in `WTF\HotPatch.txt` and `/hotpatch on|ask|off`), CMSG
  `0x8F1` capability and `0x8F2` results, and the `0x5BC` formatter check (`DbcPatch::SetValidator`): a
  ChallengeRequirementTypes formatter that is not one `return <expression>` over `value` and the seven shipped
  functions drops the row.
- **Bridged table patches.** Server patches of the six bridged tables (SMSG `0x567`, `0x569`, `0x56D`, `0x56F`,
  `0x573`, `0x574`) take effect: `MMgr64.exe` refused the row write (PatchRecord `0x73`), so the 32-bit client dropped
  every such patch. The reconstruction applies them too (its `3c8f196`, by pinning). The creature (`0x567`) and
  keystone (`0x56D`) patches of a known row, which the original wrote only into its record cache until an eviction
  reverted them, are kept. A built vanity browser takes a patched row in place instead of being rebuilt per row.
- **Early fixes taken from the reconstruction.** Quest log ids are read as 32 bits (the original's low word never
  matched custom quests above 65535). `GameTooltip:InsertLine` / `InsertLines` hide the font strings past
  `NumLines()` while they run (`TooltipInsertFixClient`), so keystone tooltips keep every Dungeon Modifier; temporary,
  until patch-B's `Tooltip.lua` carries the fix. `GetQuestTemplate` reads the quest record at the client's offsets
  (reconstruction `0fa250b`): the original read `ObjectiveText` at a 0x200 stride and `RewardTitleId` .. `Unknown`
  0x400 past the record, so those keys returned other fields' bytes (the Callboard's `RewardArenaPoints`). SMSG `0x693`
  marks the item appearance index stale for its next reader (reconstruction `ecdd916`): the original rebuilt the whole
  index per row, which froze login on a streamed catalog.
- **New rows in the DLL's tables.** `Dbc::Table::Upsert` fixes two defects of the original's insert (FUN_10133ac0), which
  the reconstruction keeps: growing the index no longer loses the row that held the old maximum id, and a new row joins
  file-order walks (`Count`, `RowAt`) after the file's rows, so indexes built by walking see server-added rows. The
  client tables' own insert (`ClientDbcPatch::Insert`) keeps the old maximum too.
- **Not ported from the reconstruction.** `AscDbcStream` (its `ASCENSION_DBC_STREAM` build, default off; SMSG
  `0x8F3`-`0x8F5` streamed GameObjectDisplayInfo / Map / MapDifficulty rows, not in the original) and
  `AscMinimapTargetKeep` (a workaround for a 32-bit-only indoor minimap defect CoA64 does not show).
- **Quest log of up to 64 quests (also in the reconstruction).** The DLL has the same feature (`ASCENSION_QUEST_LOG_64`,
  on in the ship preset; its `docs/QUEST_LOG_64.md`). After world entry the client sends CMSG `0x8F6` (HELLO); a
  server that answers with SMSG `0x8F7` fills quest log slots 25..63 and sets the log size (25..64), and
  `MAX_QUESTLOG_QUESTS` and `MAX_WATCHABLE_QUESTS` follow it. Slots 0..24 stay in the player descriptor; without a `0x8F7` the log is the
  native 25 (`docs/superpowers/specs/2026-11-25-quest-log-64-design.md`). Known limit: the zone-header collapse
  filter is the 25-bit `QuestLogCollapseFilter` mask, kept unchanged, so a header past the 25th distinct sort type
  cannot be collapsed.
- **Character advancement `0x726` handler order.** As in the reconstruction: unit parameters are synchronized before
  listeners run.
- **World headers.** The reconstruction's plaintext-header stub (`0x466BF0`) is disabled there and not ported.
- **Opcodes 0x51F..0x55E.** The 32-bit client indexes past its 0x51F-entry handler table for these (the DLL widens
  ProcessMessage's bound at `0x63200D`), reading and writing neighbouring memory. No server sends them; CoA64 treats
  them as unhandled and ignores registrations for them (`CoA::Packets::InGap`).
- **Warden.** Unchanged from wowdecomp's x64 client: Warden data is received and dropped. To be revisited.
- **Render pipeline (CoA64 addition).** The world can render to an off-screen colour target plus an INTZ depth texture
  and be copied back to the back buffer, pixel-identical to the direct path (checked with `Tools/coa/imgdiff.py`).
  Console-only, all default 0: `coaRenderPipeline`, `coaRenderDebug`, `coaRenderJitter`. Active only without MSAA, with
  INTZ, a 24-bit depth format and shaders; otherwise the direct path runs. Groundwork for DLSS
  (`docs/superpowers/specs/2026-10-26-streamline-rendering-design.md`).
  `coaRenderMotion` (default 0, needs `coaRenderPipeline 1`) adds per-pixel motion vectors (static geometry from depth
  and the camera; animated models redrawn with own shaders from this and last frame's bones) and a translucency mask,
  written to their own textures only — the colour image is unchanged. `coaRenderDebug` 3/4/5/6 = motion view, mask,
  model-pass coverage, motion ×16.
- **NVIDIA DLSS (CoA64 addition).** DLSS Super Resolution / DLAA through NVIDIA Streamline (MIT, v2.14.16), running
  inside DXVK. On the Video options' Display page (DLSS, Frame Generation, NVIDIA Reflex dropdowns, added at runtime;
  greyed out with the reason where unsupported; DLSS saves Multisampling 1x) and as console CVars, default off: `coaDlssMode` 0 off / 1 DLAA / 2 Quality / 3 Balanced / 4 Performance /
  5 Ultra Performance. Streamline is loaded only when `coaDlssMode` is non-zero at start-up, so turning it on from 0
  needs a restart; switching between non-zero modes is live. With 0 nothing of Streamline is loaded. It needs the
  DXVK renderer, an NVIDIA RTX GPU and driver, and the NVIDIA-signed `sl.interposer.dll`, `sl.common.dll`,
  `sl.dlss.dll` and `nvngx_dlss.dll` beside `d3d9.dll` in `x64\`; otherwise the log names why it stays off. A mode
  turns on the render pipeline and motion vectors itself, renders the world at a lower size (the UI stays at window
  size) and lets DLSS scale it up. While Streamline is active DXVK uses descriptor buffers instead of the descriptor
  heap (NGX rendered black with the heap). Developer CVars: `coaRenderScale` (world size without DLSS),
  `coaDlssMvecSign`, `coaDlssJitterSign` (default 1).
- **NVIDIA Frame Generation and Reflex (CoA64 addition).** Dropdowns on the Display page and console CVars, default off: `coaFrameGen` 0 off / 1 = 2x /
  2 = 3x / 3 = 4x (capped at what the GPU supports; RTX 40 for 2x, RTX 50 for more), `coaReflex` 0 off / 1 on /
  2 on + boost (at least on while Frame Generation runs). Streamline loads when any of `coaDlssMode`, `coaFrameGen`,
  `coaReflex` is non-zero at start-up (same restart rule). Frame Generation also works without DLSS (the world then
  renders at window size); it is off outside the world (loading screens, character select) and for the frame of a
  resize. Needs `sl.dlss_g.dll`, `nvngx_dlssg.dll`, `sl.reflex.dll`, `sl.pcl.dll` and `NvLowLatencyVk.dll` in `x64\`.
  **Frame Generation is currently always off** (the log says why): NVIDIA presents the client's Vulkan window by a
  composed copy, not flip model, and DLSS-G's pacer then deadlocks in the present's device flush
  (`docs/2026-10-29-render-pipeline-notes.md`, "Frame Generation hang"). Reflex and DLSS are unaffected.

## Forced by x64

- **MemoryBridge / `MMgr64.exe`.** No helper process, protocol or record cache: the six bridged DBCs (Creature, Quest,
  ItemAddon, VanityCollection, SpellAffect, MythicKeystones) are kept in the client as the x64 records `BridgeStore`
  hands out, answering lookups as the helper's indexes did (the first row holding the key; nothing for a field the
  original never indexed). The memory commands and the start-up self-test are gone.
- **HD content.** The eight HD archives and the ten HD model tables load only while the DXVK (Vulkan) renderer is
  active (`CoA::Hd`), and never with `-hd 0`. HD tables are read through the client's own record readers (x64
  records) and applied right after each one loads.
- **`DivxTac.dll` and the account kill switch.** Not loaded: a 64-bit process cannot load the 32-bit DLL.
- **Discord.** Needs the x64 `discord_game_sdk.dll`; without it the feature is off.
- **Realm-data DLLs.** The hot-swap's `LoadLibraryA` of `*.dll` under `Data\<path>` only succeeds for x64 DLLs;
  32-bit ones fail to load and are logged.
- **Build number.** The realm-data `buildinfo` patching of `0x9F5200`/`0x8CA5F0`/`0x4648B3` becomes a runtime variable.
- **Developer log.** `Logs\Extensions.log` (`-extlog 1`) is written by CoA64 itself; its first line reads
  `CoA64 attached` instead of the DLL's banner.
