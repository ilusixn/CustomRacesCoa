# Troubleshooting

- **Client ignores a file in a patch MPQ** — internal paths must use backslashes (`Interface\GlueXML\X.lua`). DBCs only load from MPQs, never loose.
- **Patch "does nothing"** — the last `patch-*.MPQ` in alphabetical order wins; our patches are `patch-ZZ*`. Locale MPQs (enUS) can win for some DBCs (e.g. SkillLine names).
- **Installed a patch and the game behaves oddly** — install client patches only with the game closed, then start the game again.
- **New build has no effect** — use `docker compose up -d --force-recreate ac-worldserver`; `restart` keeps the old image.
- **Talent window empty / "BuildTree() No Tab Set"** — the classic classes use the CoA talent window (Reborn class types); a new tab must exist in the client enum (General2/General3 were the only free ones) and in ClassConversions. Read the client error log first (`WTF/Account/<acc>/SavedVariables.lua`, `Logs/`).
- **Trainer shows a rank green but it will not stick** — classic characters know the 11xxxxx twin of a spell; check whether the trainer row teaches the stock ID. On `coa wcr` realms the Warcraft Reborn rewriter maps class trainers in memory; confirm in game before editing `trainer_spell`.
- **Spellmods do nothing** — a spell modifier only affects spells of the same SpellFamilyName with an overlapping mask.
- **Talent points look wrong after editing trees** — the server identifies learned talents by known spells; keep IDs stable (`prepara_regen.py`, `regen_check.sh` must report 0 differences).
- **Inline `#` comment in a .conf value line** breaks parsing — put comments on their own line.
- **Character data** — edit characters only with the worldserver stopped.
