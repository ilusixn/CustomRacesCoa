#!/usr/bin/env python3
"""Extract the original files the diffs/deltas apply to, from YOUR client MPQs.
Usage: extract_originals.py <out_dir> <winning MPQ for each file, highest priority first...>
Example: extract_originals.py orig Data/patch-T.MPQ Data/patch-M.MPQ Data/patch-B.MPQ
The base used to build the deltas/diffs is listed in docs/INSTALL.md (section 'Client')."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mpqread as m
FILES = [r"Interface\GlueXML\CharacterCreate.lua", r"Interface\SharedXML\AtlasInfo.lua",
         r"Interface\FrameXML\Util\CharacterAdvancementUtil.lua", r"Interface\AddOns\Ascension_Collections\Collections.lua",
         r"Interface\FrameXML\Util\GlobalOverwrites.lua", r"Interface\FrameXML\MainMenuBarMicroButtons.lua",
         r"Interface\FrameXML\UIParent.lua", r"DBFilesClient\CharBaseInfo.dbc", r"DBFilesClient\ChrRaces.dbc",
         r"DBFilesClient\SkillLine.dbc", r"DBFilesClient\SkillLineAbility.dbc", r"DBFilesClient\SkillRaceClassInfo.dbc"]
out, mpqs = sys.argv[1], sys.argv[2:]
hs = [(p, m.abre(p)) for p in mpqs]
for f in FILES:
    for p, h in hs:
        d = m.le(h, f) if h else None
        if d is not None:
            dst = os.path.join(out, *f.split("\\")); os.makedirs(os.path.dirname(dst), exist_ok=True)
            open(dst, "wb").write(d); print(f, "<-", p); break
    else:
        print(f, "NOT FOUND")
