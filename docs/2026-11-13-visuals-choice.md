# Choosing the graphics mod (2026-11-13)

The client ships two visual mods and must not run both: **LuxVelum** (`LuxVelum64.dll`, fog, light and water, the host
from 2026-11-12) and **Lumenveil** (CoA Custom 2.0's native Vulkan post passes). `CoA::Visuals::Start()` in WinMain picks
one at startup:

| `visuals` | Meaning |
| --- | --- |
| 0 | stock, no graphics mod |
| 1 | Lumenveil |
| 2 | LuxVelum (default, so existing installs behave as before) |

The choice comes from `-visuals <n>` (also `-visuals=<n>`; the old `-noluxvelum` still means 0), else the first number
in `Visuals.txt` beside the exe, else 2. `CoA::Visuals::Set(n)` writes that file (next start).

Lumenveil has no x64 module in this build: its engine is 32-bit and hashes the stock 12340 client's code
(`lumenveil/Lumenveil`), and CoA Custom 2.0's native port is not in the pushed source. Selecting 1 therefore logs that
the game renders stock instead of loading anything; when an x64 host exists, `Visuals::Start()` is the one place that
loads it. LuxVelum keeps its own `LuxVelum.ini` switch (`Enable=`) and the `-noluxvelum` argument.
