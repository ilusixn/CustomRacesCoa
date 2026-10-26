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
- **Interface hot patches.** The reconstruction's `AscHotPatch` (SMSG `0x8F0`, the `0x5BC` formatter check; not in the
  original, `ASCENSION_HOT_PATCH` default off) is not ported.
- **Bridged table patches.** Server patches of the six bridged tables (SMSG `0x567`, `0x569`, `0x56D`, `0x56F`,
  `0x573`, `0x574`) take effect: `MMgr64.exe` refused the row write (PatchRecord `0x73`), so the 32-bit client dropped
  every such patch. The reconstruction applies them too (its `3c8f196`, by pinning). The creature (`0x567`) and
  keystone (`0x56D`) patches of a known row, which the original wrote only into its record cache until an eviction
  reverted them, are kept. A built vanity browser takes a patched row in place instead of being rebuilt per row.
- **Early fixes taken from the reconstruction.** Quest log ids are read as 32 bits (the original's low word never
  matched custom quests above 65535). `GameTooltip:InsertLine` / `InsertLines` hide the font strings past
  `NumLines()` while they run (`TooltipInsertFixClient`), so keystone tooltips keep every Dungeon Modifier; temporary,
  until patch-B's `Tooltip.lua` carries the fix.
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
