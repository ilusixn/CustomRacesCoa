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
| `0x4DCCF0` | `ScanDLLStart` returns 0 | ported | `GlueScriptEvents_Scripts.cpp` |
| `0x4E0F53`–`0x4E0FC0` | random-class array 10 → 30 | ported | `CharacterCreation.cpp` |
| `0x40B7D0` → cave `0x4E5CB0` | sets `m_scandllOkayToLogIn = 1`, `m_scandllShouldBlockPlaying = 0`, loads `Extensions.dll` | ported (the CoA code is linked in; `CoA::Attach` runs at this point) | `Win64/WinMain.cpp` |
| `0x510089` | `jne` | not Ascension's: Stormforge has the same bytes; only the HD exe differs | — |
| `0x727431`/`0x72743E` | `cmp eax,4` / `ret 0` in `ClickToMove`'s autofollow block | dead code in the 32-bit client: Extensions.dll overwrites `0x727431` with `jmp` (`AscAttachLate`), so the block never runs. Ported with the DLL's patches | Plan for world modules |
| `0x68EDD7` | call target `0x68C321` instead of `0x684C21` | undecided: a one-byte change into the middle of a function inside `CGxDeviceD3d`; reached only on a D3D device path. Not ported; to be revisited if a renderer difference shows | — |
| `.rsrc` | Ascension icon and version info | not ported | — |

The glue digest-mismatch exit at `0x4DA8EB` is an Extensions.dll patch (`AscAttachPatches`), ported with the exe
patches because the login screen cannot load without it.
