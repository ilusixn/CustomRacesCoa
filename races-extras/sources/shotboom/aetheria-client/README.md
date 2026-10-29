# Kyrian and Venthyr integration package

## Credits

Special thanks to [**Kalibros**](https://github.com/STRHercules) for his frameworks and help, **Medviten** for his help and the **WHE module**, and **Saty** for the **x64 decompilation work**.

See [CREDITS.md](CREDITS.md) for acknowledgements and [PROVENANCE.md](PROVENANCE.md) for source and asset attribution.

Version: kyrian-venthyr-20261027-1. Both races are maintained together; later selected race updates will be normal commits to these package repositories.

This is a developer integration overlay for the Aetheria/Esteria extended WotLK 3.3.5a client and matched AzerothCore fork. It is not a complete game, full source mirror, stock-client installer, or universal AzerothCore module. It contains only race assets, race source/scripts, scoped integration material and owned spell data.

Local race IDs are Kyrian 60 and Venthyr 61. Owned spells are 111100-111113 and 111200-111212. Review collisions before importing. Existing native race support, extended appearance persistence/protocol, race creation/display records, and the native model/appearance/equipment loader are prerequisites; they are not recreated by these incremental packages. See package.json for source reference HEADs and integration details. Different client forks require porting.

**Status:** the matching Aetheria versions were compiled and installed locally on 2026-10-27. The newest robe/2H/Hover/Crimson Feast visual/gameplay acceptance remains pending the owner's testing. Publication does not mark those fixes accepted. Recipient builds are untested and must be validated on their own stack.

## Paired repositories

- Client: https://github.com/Shotboom12/aetheria-client
- Server: https://github.com/Shotboom12/aetheria-server

Keep matching packageVersion values on both sides. Both repositories are public and can be viewed or downloaded without an invitation. No launcher/CDN release is included.

## Client integration

1. Back up the recipient client's affected files and source. Confirm the prerequisite native-race framework and local race IDs. Do not replace a stock client executable with anything from this repository; no executable is provided.
2. Review integration/race-hooks.patch against the reference client HEAD in package.json. Run git apply --check on a scratch source tree before applying; port conflicts manually. The patch covers the race locomotion/state code and header, not unrelated full client changes.
3. Merge integration/KyrianVenthyrAppearance.inc into the existing CreatureAppearance namespace, replacing the matching race profiles/EncodeKyrian/DecodeKyrian definitions rather than defining them twice. Preserve the framework's Option type and unrelated race definitions. Use integration/FRAMEWORK-WIRING.md to wire the two profiles/catalogs into the existing creator, appearance, scale and equipment framework. Those numbered excerpts are reference material, not standalone compilable replacements. Review integration/shared-dependencies/EsteriaSelectorOutfits.inl as the current shared policy dependency, merging the robe rule rather than overwriting divergent shared policy.
4. Use Python 3.10+ to unpack into a new staging directory:

   python tools/unpack-assets.py --output restored-assets

   All 226 assets are confined to custom/kyrian and custom/venthyr. Models are compressed and, where needed, split into ordered parts so no full game archive is included. The tool verifies parts and reconstructed file hashes. It never writes to a live client automatically. Preserve all model, skin, animation and texture paths.
5. Copy/merge the four catalog files under client-files/Data/Retail into the recipient's Data/Retail only after confirming the matching appearance schema/profile code. All catalog material references are included in the asset manifest. Material and model files are MPQ paths, not proof that a stock client reads arbitrary loose files: use the recipient's established MPQ/override packer and load-order policy.
6. Extract the recipient's effective highest-priority Spell.dbc, then merge ONLY the 27 owned records:

   python tools/merge-spells.py --input baseline/Spell.dbc --output output/DBFilesClient/Spell.dbc

   If updating an already reviewed installation of these same owned spells, add --replace-owned. Existing owned names must match; otherwise collision handling stops. Input is never overwritten. Other records and the original string pool are preserved. Do not replace a recipient's entire Spell.dbc with an Aetheria global table; this repository deliberately does not contain one.
7. Wire the racial names/descriptions from data/spells.json into the recipient's existing creation UI. Preserve its other races and creator controls. Full shared CharacterCreate.lua/EsteriaRaceCards.lua replacements are deliberately excluded. Ensure both race IDs, both body types, control counts and the native model/display mappings agree. Race creation/database/DBC base records are prerequisites, not generated here.
8. Package the merged Spell.dbc and staged race assets with the recipient's existing pack tools. Rebuild matching client/server code under that project's build procedure, and deploy with backups and matching server migrations. Do not blindly rename the pack to override unknown later patches.

Venthyr Bat Form additionally requires the existing creature 9501110/display 95110 and its matching client bat model/skins/textures. That existing base-game/custom-pack dependency is not one of the two race namespaces and is not included. Remap the shared constant and matching data together if the recipient uses another owned bat display.

## Recovery and tests

Restore backed-up client/source/data as a matched set to roll back. Keep the previous effective Spell.dbc and asset/catalog files. Test both body types: creator controls, body/wing/hair palettes, equipment, robe coverage while standing/running/sitting, Venthyr 2H ready/attack/special, Hover transitions/movement/combat, and spells while injured. Test Bat legal/no-fly areas, riding requirements, combat cancellation and safe descent. Offline reconstruction and patch checks do not prove gameplay or visuals.
