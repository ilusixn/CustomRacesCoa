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

## Server integration

1. Back up affected source and database rows. Use a scratch copy to review the patch and resolve any race/spell/display ID collisions. The server must already support race IDs above the stock range, both race creation records/classes, matching faction/reputation policy, and the extended appearance protocol. Do not import unrelated full module trees.
2. Copy the six dedicated files under source/ to the matching server source paths. Review integration/race-hooks.patch. Preserve your existing AddCustomScripts entries and add only the three included registration functions; do not duplicate existing registrations. Include the current headers and compile the scripts with your established custom-script CMake discovery.
3. Apply/port the core hooks for Kyrian falling damage and Venthyr fractional periodic-leech healing. They are scoped to the owned race/aura/spell IDs. The shared core baseline and recipient fork may differ; check the patch in a scratch source tree. No executable is provided.
4. Review the eight migrations in package.json order. Files end in .prepared so merely copying them into a repository does not enqueue a live update. They are historical incremental migrations for this extended AzerothCore schema. Never automatically replay them over unverified occupied IDs; preserve previously applied migrations and use only missing compatible steps or a separately reviewed upgrade.
5. Confirm all 27 final spell records agree with the paired client package, and all script/bonus bindings match the included source. Back up affected rows, apply the applicable migration steps in a controlled transaction, verify readback, and record rollback data. Do not execute the old local-machine installers; none is included here.
6. Build/deploy the matched client/server/data set under the recipient project's guarded procedure. Aetheria uses explicit build authorization, PowerShell 7, four workers, server then client. Close affected running programs normally before replacing them. Do not start/stop programs automatically.

Venthyr Bat Form depends on existing creature 9501110/display 95110 and matching client assets. It is out-of-combat travel with flight only in legal areas, riding requirements and safe descent. Sanguine Hover remains ground-only. Both dependencies and spell IDs must agree with the client package.

## Recovery and tests

Database rollback is restoration of the recipient's backed-up rows and matching prior code/data; no blanket delete/reset is provided. Preserve previous applied migration history. Test all racials, existing/new character grants, reconnect/death, Kyrian charge/dive and falling damage, Venthyr Hover combat allowance/transitions, Bat legal-flight restrictions/cancellation/descent, Crimson Feast injured-target healing including low-damage ticks, and Anima Hunger eligibility/owner attribution.
