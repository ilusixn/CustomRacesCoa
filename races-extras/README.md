# races-extras

Everything outside the C++ client that the custom race work depends on. Development stage; not stable.

- `glue/`   GlueXML / AddOn Lua (character creation and selection: form tabs, racial lists, preview fit). Packed into a client MPQ.
- `server/` AzerothCore (CoA Custom 1.9 base) source changes: `races-core.patch` against `src/`, and the full changed files under `files/`.
- `sql/`    CoA Custom SQL (racials, races, Worgen Two Forms spell 110690).
- `tools/`  Python/C++ helper scripts used to build the client data patches and merge the pieces.

Not included: models, textures, DBC files, MPQs (large, and many are other authors' work).
