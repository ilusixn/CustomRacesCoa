# Third-party and upstream material

The proprietary license in `LICENSE` covers CoA64's own work only. The material below keeps its own
copyright and license; those licenses, not `LICENSE`, govern it.

## Upstream sources (see `UPSTREAM.md`)

- **ascension-extensions-reconstruction** (ported into `CoA/Source`) — MIT License,
  Copyright (c) 2025 Alyst3r. The MIT notice below applies to the portions derived from it:

  > Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
  > documentation files (the "Software"), to deal in the Software without restriction, including without
  > limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
  > Software, and to permit persons to whom the Software is furnished to do so, subject to the following
  > conditions:
  >
  > The above copyright notice and this permission notice shall be included in all copies or substantial
  > portions of the Software.
  >
  > THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
  > TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  > THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
  > CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  > DEALINGS IN THE SOFTWARE.

- **wowdecomp** (the 3.3.5a client decompilation the engine and game code are based on) — copyright of its
  authors; no license is granted for it by `LICENSE`. The original World of Warcraft client is copyright
  Blizzard Entertainment, Inc.; CoA64 claims no rights in it.

## Third-party libraries (`ThirdParty/`, and code fetched at build time)

Each keeps its own license; see the license file in its folder or upstream project:

| Component | License |
|---|---|
| bzip2 | `ThirdParty/bzip2/LICENSE` |
| expat | MIT (upstream) |
| FreeType | FreeType License / GPLv2 (`ThirdParty/freetype/LICENSE.TXT`) |
| lzma | public domain (upstream) |
| mapbox | `ThirdParty/mapbox/LICENSE` |
| nlohmann/json | MIT (upstream) |
| NVAPI | NVIDIA license (`ThirdParty/nvapi/License.txt`) |
| PCRE | `ThirdParty/pcre/LICENCE` |
| xvidcore (XvidDecoder.dll) | GNU GPL v2 (`ThirdParty/xvidcore/licenses/xvid.txt`) |
| zlib | zlib License (upstream) |
| lgLcd, DivxDecoder | their vendors' terms |
| DXVK (fetched at build time, Raz0r1337/dxvk) | zlib/libpng License (upstream) |
| NVIDIA Streamline SDK and DLSS binaries (fetched at build time) | NVIDIA's licenses (upstream) |
