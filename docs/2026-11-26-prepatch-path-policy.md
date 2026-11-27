# Prepatch path policy (2026-11-26)

`WoW/Source/Glue/PatchCommandPolicy.h` is security hardening that the 32-bit client does not have.

**Why.** The patch-download `prepatch.lst` runs unauthenticated on Ascension (Extensions.dll patch 0x4E57FE), so
any server can ship one. RCEPatcher2 closes the server-controlled code execution this enables. CoA64 closes it in
source:

- "extract" and "delete" paths stay inside the download folder.
- "extract" may not write runnable files (`.exe`, `.dll`, `.bat`, ...).
- "execute" is refused (`CGlueMgr::PatchDownloadExecuteCommand`).

**Interface\ and WTF\ (user decision 2026-10-26).** The prepatch may not touch addon code or settings. The top-level
component is compared the way Windows resolves it: case-insensitive, with trailing dots and spaces ignored.

**Dot components (2026-11-26).** Win32 path APIs drop trailing dots and spaces from each component, so `.. `,
`.. .` and `...` resolve as `..`. A component made only of dots and spaces is refused unless it is exactly `.`.
Before this, `.. \evil` escaped the download folder, and `. \Interface\...` got past the Interface check, because
`. ` counted as the first real component. This was reported by the Codex review on STRHercules/wowdecomp#2.
Tests: `CoA/Tests/ClientHardeningTest.cpp`.
