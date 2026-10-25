// SMSG_PATCH_* handlers outside the generated table (port of AscPatchData.cpp): registered outside the original's
// FUN_101e5880, missing from its decompile, or carrying a side effect. GlobalStrings.dbc is also applied to each new Lua
// state after its natives (FUN_1025afb0 glue, FUN_1025af40 world). The callbacks into modules not yet ported go through
// PatchData::Dependencies(); the game-object texture refresh is in PatchDataClient.cpp.
//   0x568 DUNGEON_ENCOUNTER_EXTRA  (no decompile; code in FUN_101e14f0's tail) 0x10
//   0x681 ZONE_LIGHT_POINT FUN_1021a8a0 0x14; 0x68C MANASTORM_PLAYER_GROUP_MODIFIERS FUN_102a3ff0 0x14;
//   0x68D MANASTORM_MODIFIERS FUN_102a3e60 0x3C
//   0x693 / 0x6EC ITEM_APPEARANCES / ITEM_SET_APPEARANCES, then the appearance managers refresh
//   0x579..0x57B LFG_*  struct image + u32-length strings
//   0x686 SPELL_ADDON FUN_101e36b0 (0x5C): an existing row is overwritten in place and nothing else happens; a new row
//         is upserted and also indexed by its +4 (the spell) through FUN_101da400
//   0x5F4 SPELL_CUSTOM_ATTR FUN_101e39a0 (0x2C): an existing row is overwritten in place and the spell -> row map
//         rebuilt (FUN_1020b080); a new row is only upserted, so the map misses it until the next rebuild
//   0x71F EXTRA_ACTION_BUTTONS (FUN_101e1740, UUSSSSUUUU) / 0x723 CHARACTER_ADVANCEMENT_CLASS_TYPES (FUN_101dd3b0,
//         USUUUUS): read field by field (U = u32, S = C string); overwritten in place or inserted
//   0x6C1 SPELL_TAGS FUN_101e40c0 ({id, spell, tag}): an existing row first drops its old tag and the new one is added,
//         then it is overwritten in place; a new row adds its tag and is upserted. Then the known tags are rebuilt and
//         SPELL_TAGS_CHANGED fires.
#include "CoA/Source/Modules/PatchData.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/DbcPatch.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
    CoA::Dbc::Table& GlobalStrings() {
        return CoA::Dbc::Get("DBFilesClient\\GlobalStrings.dbc");
    }

    // The client's current Lua state (FrameScript_GetContext): _G[key (+8)] = value (+0xC).
    void SetGlobal(lua_State* L, const uint8_t* row) {
        CoA::Dbc::Table& table = GlobalStrings();
        const char* value = table.Str(row, 0xC);
        lua_pushstring(L, value ? value : "");
        lua_setfield(L, LUA_GLOBALSINDEX, table.Str(row, 8));
    }

    // FUN_1025ad00's tail (SMSG 0x676): the row's flags (+4) pick the Lua states it applies to -- bit 1 in world,
    // bit 0 at glue -- and there the global is set in the current Lua state.
    void ApplyGlobalString(const uint8_t* row) {
        uint32_t flags = CoA::Dbc::Table::U32(row, 4);
        bool inWorld = CoA::ClientState::InWorld();

        if (!(inWorld ? (flags >> 1) & 1 : flags & 1)) {
            return;
        }

        if (lua_State* L = CoA::ClientState::MainLuaState()) {
            SetGlobal(L, row);
        }
    }

    // FUN_1025af40 (after the world natives) / FUN_1025afb0 (after the glue natives): every row whose flag (+4 bit 1 /
    // bit 0) names that Lua state.
    void ApplyGlobalStrings(uint32_t bit) {
        lua_State* L = CoA::ClientState::MainLuaState();

        if (!L) {
            return;
        }

        CoA::Dbc::Table& table = GlobalStrings();
        uint32_t count = table.Count();
        uint32_t applied = 0;

        for (uint32_t i = 0; i < count; ++i) {
            const uint8_t* row = table.RowAt(i);

            if (!row || !((CoA::Dbc::Table::U32(row, 4) >> bit) & 1)) {
                continue;
            }

            SetGlobal(L, row);
            ++applied;
        }

        CoA::Log::Printf("GlobalStrings.dbc: %u of %u rows applied to the %s state", applied, count, bit ? "world" : "glue");
    }

    CoA::PatchData::Deps s_deps;

    template <typename... Args>
    void Call(void (*fn)(Args...), Args... args) {
        if (fn) {
            fn(args...);
        }
    }

    // The raw wire image of n bytes.
    std::vector<uint8_t> Image(CDataStore* msg, uint32_t n) {
        std::vector<uint8_t> row(n);
        CoA::Read::Data(msg, row.data(), n);
        return row;
    }

    uint32_t Dword(const std::vector<uint8_t>& row, uint32_t offset) {
        uint32_t v;
        memcpy(&v, row.data() + offset, 4);
        return v;
    }

    CoA::Dbc::Table& SpellAddon() {
        return CoA::Dbc::Get("DBFilesClient\\SpellAddon.dbc");
    }

    // Map 0x10BE0708: spell (+4) -> SpellAddon.dbc row. Built by the DBC manager (FUN_101e7670 over every row in file
    // order, a later row replacing an earlier one), then kept current by 0x686. Rows are held by id so an in-place 0x686
    // overwrite is seen, as the original's row pointers see it.
    std::unordered_map<uint32_t, uint32_t>& SpellAddonIndex() {
        static std::unordered_map<uint32_t, uint32_t> index;
        static bool built = false;

        if (!built) {
            built = true;
            CoA::Dbc::Table& table = SpellAddon();

            for (uint32_t i = 0; i < table.Count(); ++i) {
                if (const uint8_t* row = table.RowAt(i)) {
                    index[CoA::Dbc::Table::U32(row, 4)] = CoA::Dbc::Table::U32(row, 0);
                }
            }
        }

        return index;
    }

    int32_t OnSpellAddon(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x686
        const std::vector<uint8_t> row = Image(msg, 0x5C);
        const uint32_t id = Dword(row, 0);
        const uint32_t spell = Dword(row, 4);
        CoA::Dbc::Table& table = SpellAddon();

        if (const uint8_t* live = table.Row(id)) {
            memcpy(const_cast<uint8_t*>(live), row.data(), row.size());
            return 1;
        }

        SpellAddonIndex();   // the manager built the index at DBC init, before any packet
        table.Upsert(id, row);
        SpellAddonIndex()[spell] = id;
        return 1;
    }

    int32_t OnSpellCustomAttr(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x5F4
        const std::vector<uint8_t> row = Image(msg, 0x2C);
        const uint32_t id = Dword(row, 0);
        CoA::Dbc::Table& table = CoA::Dbc::Get("DBFilesClient\\SpellCustomAttr.dbc");

        if (const uint8_t* live = table.Row(id)) {
            memcpy(const_cast<uint8_t*>(live), row.data(), row.size());
            Call(s_deps.rebuildSpellCustomAttr);
            return 1;
        }

        table.Upsert(id, row);
        return 1;
    }

    // 0x71F / 0x723: the table and its field layout.
    std::map<uint32_t, std::pair<const char*, const char*>>& FieldSpecs() {
        static std::map<uint32_t, std::pair<const char*, const char*>> specs;
        return specs;
    }

    int32_t OnFieldPatch(void*, uint32_t opcode, uint32_t, CDataStore* msg) {
        auto spec = FieldSpecs().find(opcode);

        if (spec == FieldSpecs().end()) {
            return 1;
        }

        CoA::Dbc::Table& table = CoA::Dbc::Get(spec->second.first);
        const char* layout = spec->second.second;
        std::vector<uint8_t> row(strlen(layout) * 4);

        for (size_t f = 0; layout[f]; ++f) {
            uint32_t v;

            if (layout[f] == 'S') {
                const std::string str = CoA::Read::Str(msg);
                v = table.AddString(str.c_str());
            } else {
                v = CoA::Read::U32(msg);
            }

            memcpy(&row[f * 4], &v, 4);
        }

        const uint32_t id = Dword(row, 0);

        if (const uint8_t* live = table.Row(id)) {
            memcpy(const_cast<uint8_t*>(live), row.data(), row.size());
            return 1;
        }

        table.Upsert(id, row);
        return 1;
    }

    void FieldLayout(uint32_t opcode, const char* table, const char* layout) {
        FieldSpecs()[opcode] = { table, layout };
    }

    int32_t OnSpellTag(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x6C1
        const std::vector<uint8_t> row = Image(msg, 0xC);
        const uint32_t id = Dword(row, 0);
        const uint32_t spell = Dword(row, 4);
        const uint32_t tag = Dword(row, 8);
        CoA::Dbc::Table& table = CoA::Dbc::Get("DBFilesClient/SpellTags.dbc");

        if (const uint8_t* live = table.Row(id)) {
            Call(s_deps.spellTagRemove, CoA::Dbc::Table::U32(live, 4), CoA::Dbc::Table::U32(live, 8));
            Call(s_deps.spellTagAdd, spell, tag);
            memcpy(const_cast<uint8_t*>(live), row.data(), row.size());
        } else {
            Call(s_deps.spellTagAdd, spell, tag);
            table.Upsert(id, row);
        }

        Call(s_deps.rebuildKnownTags);
        CoA::Events::Signal("SPELL_TAGS_CHANGED", nullptr);
        return 1;
    }

    void ItemAppearancesPatched(const uint8_t*) {
        Call(s_deps.itemAppearancesPatched);
    }

    void ItemSetAppearancesPatched(const uint8_t*) {
        Call(s_deps.itemSetAppearancesPatched);
    }

    void RebuildOutfits(const uint8_t*) {
        Call(s_deps.rebuildOutfits);
    }

    void TagTypesPatched(const uint8_t*) {
        Call(s_deps.tagTypesPatched);
    }

    void GeosetsRebuild(const uint8_t* row) {
        Call(s_deps.geosetsRebuild, row);
    }

    void GameObjectTextures(const uint8_t*) {
        Call(s_deps.gameObjectTextures);
    }

    void ApplyWorldGlobalStrings() {
        ApplyGlobalStrings(1);
    }

    void ApplyGlueGlobalStrings() {
        ApplyGlobalStrings(0);
    }
}

CoA::PatchData::Deps& CoA::PatchData::Dependencies() {
    return s_deps;
}

const uint8_t* CoA::PatchData::SpellAddonForSpell(uint32_t spell) {
    auto it = SpellAddonIndex().find(spell);
    return it != SpellAddonIndex().end() ? SpellAddon().Row(it->second) : nullptr;
}

void CoA::PatchData::Init() {
    CoA::LuaReg::OnWorldRegistered(ApplyWorldGlobalStrings);
    CoA::LuaReg::OnGlueRegistered(ApplyGlueGlobalStrings);
    // 0x676 GLOBAL_STRINGS (FUN_1025ad00): {id, flags, key, value} with the two strings after the image.
    CoA::DbcPatch::Register(0x676, "DBFilesClient\\GlobalStrings.dbc", 0x10, (1ull << 2) | (1ull << 3), ApplyGlobalString);
    CoA::DbcPatch::Register(0x568, "DBFilesClient\\DungeonEncounterExtra.dbc", 0x10, 0, nullptr);
    // 0x56F (FUN_101e2a30, a whole Quest.dbc row) patches the MemoryBridge quest store: BridgeStore.
    CoA::DbcPatch::Register(0x681, "DBFilesClient\\ZoneLightPoint.dbc", 0x14, 0, nullptr);
    CoA::DbcPatch::Register(0x68C, "DBFilesClient\\ManastormPlayerGroupModifiers.dbc", 0x14, 0, nullptr);
    CoA::DbcPatch::Register(0x68D, "DBFilesClient\\ManastormModifiers.dbc", 0x3C, 0, nullptr);
    CoA::DbcPatch::Register(0x693, "DBFilesClient\\ItemAppearances.dbc", 0x0C, 0, ItemAppearancesPatched);
    CoA::DbcPatch::Register(0x6EC, "DBFilesClient\\ItemSetAppearances.dbc", 0x0C, 0, ItemSetAppearancesPatched);
    // 0x6FC (handler_0x06fc): then FUN_101ceee0 rebuilds the geoset map.
    CoA::DbcPatch::Register(0x6FC, "DBFilesClient\\CreatureDisplayInfoGeosetData.dbc", 0x10, 0, GeosetsRebuild);
    // 0x5F3 CHR_SPECS (handler_0x05f3): 0x84-byte image, then 15 u32-length strings.
    CoA::DbcPatch::RegisterSized(0x5F3, "DBFilesClient\\ChrSpecs.dbc", 0x84, (7ull << 1) | (7ull << 8) | (0x7Full << 17) | (3ull << 29), nullptr);
    FieldLayout(0x71F, "DBFilesClient/ExtraActionButtons.dbc", "UUSSSSUUUU");
    FieldLayout(0x723, "DBFilesClient/CharacterAdvancementClassTypes.dbc", "USUUUUS");
    COA_PACKET(0x71F, OnFieldPatch);
    COA_PACKET(0x723, OnFieldPatch);
    // 0x6ED APPEARANCE_DETAILS (FUN_101dcd30): 0x14-byte image, then one C string (+8).
    CoA::DbcPatch::Register(0x6ED, "DBFilesClient/AppearanceDetails.dbc", 0x14, 1ull << 2, nullptr);
    // 0x6FB GAMEOBJECT_DISPLAY_INFO_ADDON (FUN_1020c190): {id, C string}; then every game object's texture.
    CoA::DbcPatch::Register(0x6FB, "DBFilesClient/GameObjectDisplayInfoAddon.dbc", 0x08, 1ull << 1, GameObjectTextures);
    // Plain patches registered outside FUN_101e5880: 0x678 ZONE_STORY (FUN_1021a9b0), 0x66E TOKEN_TYPES (FUN_1033c2c0).
    CoA::DbcPatch::Register(0x678, "DBFilesClient/ZoneStory.dbc", 0x14, 1ull << 4, nullptr);
    CoA::DbcPatch::Register(0x66E, "DBFilesClient/TokenTypes.dbc", 0x24, 0x7Eull, nullptr);
    // 0x6D6 CHARACTER_CREATION_ARCHETYPES (FUN_101df460) is registered with the creation tables in CharacterCreate.
    // 0x692 APPEARANCES (FUN_101dced0): 0x44-byte image, then one C string (+8); a NEW row also runs the manager's
    // FUN_100c6750.
    CoA::DbcPatch::RegisterInsertHook(0x692, "DBFilesClient/Appearances.dbc", 0x44, 1ull << 2, RebuildOutfits);
    // 0x6C0 SPELL_TAG_TYPES (FUN_101e3bd0): 0x74-byte image, then six C strings (+0x5C..+0x70); then FUN_10217220
    // rebuilds the per-type stat lists.
    CoA::DbcPatch::Register(0x6C0, "DBFilesClient/SpellTagTypes.dbc", 0x74, 0x3Full << 23, TagTypesPatched);
    // Struct image then u32-length strings (FUN_101e1df0 / FUN_101e1f60 / FUN_101e20a0).
    CoA::DbcPatch::RegisterSized(0x579, "DBFilesClient\\LFGActivities.dbc", 0x1C, 1ull << 4, nullptr);
    CoA::DbcPatch::RegisterSized(0x57A, "DBFilesClient\\LFGActivityCategories.dbc", 0x10, 1ull << 2, nullptr);
    CoA::DbcPatch::RegisterSized(0x57B, "DBFilesClient\\LFGActivityGroupType.dbc", 0x0C, 1ull << 1, nullptr);
    COA_PACKET(0x686, OnSpellAddon);
    COA_PACKET(0x5F4, OnSpellCustomAttr);
    COA_PACKET(0x6C1, OnSpellTag);
    COA_EVENT("SPELL_TAGS_CHANGED");
}
