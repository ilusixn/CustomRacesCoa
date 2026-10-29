# Known issues (from the 2026-10-12 audits)

Severity: CRITICAL = cannot create/play · IMPORTANT = wrong item/ability · VISUAL = appearance/UI.
No CRITICAL issue is known. 626 valid race/class pairs were audited (106 without any issue; most of the
rest only had the starting action bar problem, fixed in v0.1.0).

| # | Issue | Severity | Status |
|---|---|---|---|
| 1 | Classic starting action bars pointed at stock spell IDs (character knows the 11xxxxx "twin") | VISUAL | fixed (sql/world, untested in game) |
| 2 | Warrior received Devastate (Protection identity) from the book at level 1 | IMPORTANT | fixed (sql/world) |
| 3 | Judgement 10321 shown green at trainers but duplicates what the tree grants | IMPORTANT | fixed (sql/world) |
| 4 | City trainers list stock IDs; rank chains may look trainable but fail. The Warcraft Reborn trainer rewriter already maps them in memory on `coa wcr` realms — needs in-game confirmation before changing data (`tools/reference/b1_treinadores.py`) | IMPORTANT | open |
| 5 | Starting weapon without the matching weapon skill (Rogue, Hunter, one Warrior body type) | IMPORTANT | open (plan: swap the item, no extra proficiency) |
| 6 | Some CoA starting weapons have zero weapon damage (7 classes, 14 items) | IMPORTANT | open (verify first) |
| 7 | Missing/incorrect racials for some races (classic Draenei/Goblin; Vulpera, Earthen, High Order Skyborne; Windshaper Skyborne gets Troll racials) | IMPORTANT | open (part needs a core build) |
| 8 | Same starting item granted twice on 18 pairs | minor | open |
| 9 | A Ranger starting item requires a reagent that does not exist | minor | open |
| 10 | Character-creation preview shows naked characters (client outfit table empty for many pairs); real gear is fine | VISUAL | open (client-only fix planned) |
| 11 | Class Guide (role/subrole view) does not list the 9 classic classes | VISUAL | open |
| 12 | 6 race/class pairs accepted by the server but hidden in the client | minor | open |
| 13 | Paladin tree still uses ranks 1-2-3 (others 1/3/5) | balance | open |
| 14 | Not every DB change made during development is exported to `sql/` yet (e.g. classic starting skills for custom races, some trainer/book rows, skill-line renames) | packaging | open |
| 15 | Support specs frozen; Paladin Support v2 (29 nodes) untested in game | — | frozen |
