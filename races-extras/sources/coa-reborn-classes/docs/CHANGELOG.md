# Changelog

## v0.1.0-study (2026-10-12) — first local package, not published
- Unified realm class model `coa wcr` (CoA + classic classes on one realm).
- Classic classes use the CoA Character Advancement path; talent loader skips DISABLED/DEPRECATED rows.
- Talent upload from the classic client accepted (automatic entries of every spec ignored); custom races get class-restricted starting weapon skills.
- Generators for the 9 classic trees (model + per-class JSON), stable IDs, automatic dependencies on the right margin, baseline abilities, level-60 rule, class quests not required.
- Stabilization: duplicate spells disabled structurally, audit gate in the regeneration pipeline.
- Bloody Book: classic-class level limit option; Hunter pet spells via the book.
- Support (4th spec, Paladin v2 with 29 nodes + Priest/Druid/Shaman prototypes) — **frozen** on 2026-10-12; Shaman Enhancement shown as DPS only.
- Block 1 fixes: classic starting action bars, Devastate out of the level-1 book, Judgement 10321 duplicate removed.
- Client: Lua diffs (talent window for classic classes, character creation, spec order, atlas backgrounds) and DBC row deltas; MPQ build scripts.
