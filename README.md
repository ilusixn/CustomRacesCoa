# CoA64

A native 64-bit client for Conquest of Azeroth: the WoW 3.3.5a (build 12340) client with the functionality of
CoA's `Extensions.dll` built in.

It is built on two upstream sources (see `UPSTREAM.md`):

- **wowdecomp**, a C++ decompilation of the 3.3.5a client whose x64 build is a complete client. Every function
  keeps its original address in an `// OFFSET:` tag. Its README is kept at `docs/wowdecomp-README.md`.
- **ascension-extensions-reconstruction**, the function-by-function reconstruction of `Extensions.dll`.
  Its features are ported into `CoA/Source`. Each place where decomp code was changed for CoA carries a
  `// CoA:` comment naming the original hook or patch address.

Design: `docs/superpowers/specs/2026-10-25-coa64-client-design.md`. Deliberate differences from the 32-bit
client: `CoA/DIFFERENCES.md`.

## Building

Visual Studio 2022 (Desktop development with C++), from an x64 developer prompt:

```bat
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -S . -B out\build\x64-Release
cmake --build out\build\x64-Release --target Wow
```

Output: `bin\Win\CoA64.exe` (+ `.pdb`, `.map`) and `XvidDecoder.dll`. `BuildCoA64.bat` builds and packages a zip.

## Running

Copy `CoA64.exe` and `XvidDecoder.dll` into a CoA client folder (next to `Ascension.exe`, which is left untouched)
and start `CoA64.exe` from there. `-extlog 1` turns on the developer log `Logs\Extensions.log`.

## License

Proprietary — all rights reserved. No one may copy, distribute or modify this code without written permission;
see `LICENSE`. Third-party and upstream material keeps its own licenses; see `NOTICE.md`.
