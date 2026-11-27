# Known Issues

## KI-001: ACCESS_VIOLATION in `CalcRowStride` when a BLP declares retail pixel format 11 (BC5)

**Status:** Fixed in the working tree (2026-11-08). Not yet in a shipped build: the live client at `D:\WoW\CoA` is still the 2026-11-03 build.
**Severity:** High. It is a hard client crash, and it happens every time the texture streams in.

### Source logs
All three crashes on 2026-11-05 have the same signature:
- `D:\WoW\CoA\Errors\2026-11-05 17.56.23 Crash.txt` / `.dmp`
- `D:\WoW\CoA\Errors\2026-11-05 17.57.56 Crash.txt` / `.dmp`
- `D:\WoW\CoA\Errors\2026-11-05 17.59.04 Crash.txt` / `.dmp`

### Signature
| Field | Value |
|---|---|
| Exception | `0xC0000005 ACCESS_VIOLATION` (read) at `0x00007FF641C20C94`, reading `0x00007FF602603618` |
| Symbol | `CalcRowStride+0x34` (`Engine/Source/Gx/Blit.cpp`). Build `6aea162b` (2026-11-03 18:13), resolved with `CoA64.map` |
| Stack (from dump) | `UpdateBlpTextureAsync` → `PumpBlpTextureAsync+0x16b` → `CBLPFile::LockChain2+0x291` (inlined `Lock2`) → `CalcRowStride` |
| Args | `format = RCX = 0xF0000000`, `width = RDX = 0x100` |

### Root cause
The crash reports dump the `CBLPFile` header from the stack (the `BLP2` bytes at RBX): `colorEncoding=2` (DXT), `alphaSize=0`, `preferredFormat=0x0B`, 256×256 with mips. Mip 0 is 0x10000 bytes, which is 16 bytes per 4×4 block. It is the same texture in all three crashes.

1. Preferred format 11 is retail's `PIXEL_BC5`. The 3.3.5 `PIXEL_FORMAT` enum stops at `NUM_PIXEL_FORMATS = 10`.
2. The 2026-10-28 fix (`3017c3c`) made `TextureGetTextureFormats` fall back to `PIXEL_ARGB8888` for unknown formats. That stopped the old `ITexCreate` crash, but it sends the file into the DXT→ARGB decode in `CBLPFile::Lock2`.
3. `Lock2` calls `GetBlitFormat(m_preferredFormat)`, which indexes the 10-entry `blitFmt` table without a bounds check. In the shipped exe, `blitFmt[11]` is the next static, `0xF0000000`.
4. `CalcRowStride` takes that as a signed enum, which skips the DXT branch, and indexes `PIXELS_PER_BLOCK_SHIFT[-0x10000000]`. Table VA minus 1 GiB is exactly the faulting address.

The earlier hypotheses were ruled out: HelixCtx, the DLSS/Reflex hooks, D3D9 adapter enumeration, and the Scarlet Myrmidon / spell-effect combat path. The mob fight only happened to be when the texture streamed in.

### Fix
- `blp.cpp` `GetBlitFormat`: out-of-range formats map to `BlitFormat_Unknown`. `Lock` and `Lock2` return failure for an unknown DXT input format, and `Lock2` logs the file name.
- `Blit.cpp`: `CalcRowStride` returns 0, and `Blit` returns failure, for out-of-range formats.
- `Texture.cpp` `TextureLoadBlpMips` (the sync path): unknown preferred formats become `PIXEL_ARGB8888`. Before this, `Lock`'s `default:` returned success with uninitialized data.
- **BC5 support:** `PIXEL_BC5 = 11` now maps to `BlitFormat_Bc5`. `Blit_Bc5_Argb8888` decodes it on the CPU: two BC4 blocks, reusing `Dxt5AlphaTable`. The output is (R, G, 0, 255), matching D3D BC5 sampling. The texture uploads as `GxTex_Argb8888`.
- Any other unknown preferred format still fails cleanly. The async path reports "BLP Texture failure: … decompression failed" and calls `FillInSolidTexture`.
- Tests: `CoA/Tests/BlpUnknownFormatTest.cpp` covers BC5 decode, small-mip clipping, the LockChain2 path, unknown formats, and out-of-range blit formats. All 1009 CoATests pass.

### Still open
- **Which file it is.** These reports have no texture name, because the "Last BLP" hook is only set when Extensions.dll is loaded.
- **Not verified in-game yet.** Revisit the Scarlet Monastery cemetery to confirm the texture renders. A two-channel texture shows as red/green, which is correct BC5 output. If it is a normal map that the content expects drawn as color, that is a content issue.
- **Memory cost.** BC5 textures sit in memory uncompressed (4× their file size). Uploading them natively as D3D `ATI2` would avoid that, but needs a new `EGxTexFormat`.
