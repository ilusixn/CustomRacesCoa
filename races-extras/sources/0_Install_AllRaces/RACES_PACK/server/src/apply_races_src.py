#!/usr/bin/env python3
"""Server source edits for the 4 races (idempotent). Usage: py -3 apply_races_src.py "<path to azerothcore-wotlk-coa>"
 - AscensionRaceMaskData.h: BaseRaceFor[84..87] = base race (racials, start zone, reputation, quests)
 - coa/AscensionCompat.cpp: Spectral Glide in the Banshee racial list; login auras Elune's Veil / Wings of the Swarm"""
import re, sys
from pathlib import Path
SRC = Path(sys.argv[1]) / "src" / "server"
h = SRC / "game/Entities/Unit/AscensionRaceMaskData.h"; c = SRC / "coa/AscensionCompat.cpp"
BASE = {84: "5", 85: "4", 86: "2", 87: "8"}          # Undead, Night Elf, Orc, Troll
t = h.read_text(); m = re.search(r"BaseRaceFor = \{([^}]*)\}", t); a = [x.strip() for x in m.group(1).split(",")]
for r, b in BASE.items():
    a[r] = b
h.write_text(t[:m.start(1)] + " " + ", ".join(a) + " " + t[m.end(1):])
t = c.read_text()
glide = "    if (player->getRace() == 84) spells.push_back(13990131); // BANSHEE: Spectral Glide\n"
ga = "                    spells.push_back(ability->Spell);\n\n    std::sort(spells.begin(), spells.end());"
if glide not in t:
    assert ga in t, "GetAscensionRacialSpells anchor not found"
    t = t.replace(ga, "                    spells.push_back(ability->Spell);\n" + glide + "\n    std::sort(spells.begin(), spells.end());")
la = "  void OnPlayerLogin(Player *player) override {\n"
for hook in ("    if (player->getRace() == 85 && !player->HasAura(13990141)) player->CastSpell(player, 13990141, true); // ELUNE'S CHOSEN: Elune's Veil\n",
             "    if (player->getRace() == 87 && !player->HasAura(13990151)) player->CastSpell(player, 13990151, true); // QIRAJI: Wings of the Swarm\n"):
    if hook not in t:
        assert t.count(la) == 1, "OnPlayerLogin anchor not found"
        t = t.replace(la, la + hook)
c.write_text(t)
print("BaseRaceFor", {r: a[r] for r in BASE}, "| hooks ok:", all(s in t for s in ("13990131", "13990141", "13990151")))
