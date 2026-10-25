#ifndef COA_TESTS_APPEARANCEFIXTURE_H
#define COA_TESTS_APPEARANCEFIXTURE_H

// The shared setup of the Appearance tests: the DLL DBCs served as WDBC files (loaded afresh per test, see Unload), a
// live realm with data path "ascension", and a fake client behind CoA::Appearance::Deps.
//
//   Categories  1 Head (ITEM, inv 1)   2 Chest (ITEM, inv 5)   3 Sets (ITEM_SET)   4 Pets (COSMETIC_PET, tier 1)
//               5 Mounts (MOUNT)       6 Glows (SPELL_VISUAL, spell 500)           7 Mage hats (ITEM, class 8 only)
//   Appearances 100 head 1000   101 chest 1001   102 thrown 1002   103 set 50   104 pet creature 77
//               105 visual 9000 (category 6)     106 head, no realm gate        107 mount
//   Items       1000 helm (armor, head)  1001 robe (armor, inv 20 = chest)  1002 thrown (weapon 0x10)  1003 alt helm
#include "CoA/Source/Modules/Appearance.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace AppearanceFixture {
    using V = std::vector<std::string>;
    using Packets = std::vector<std::vector<uint8_t>>;

    inline uint32_t s_class = 1;
    inline bool s_player = true;
    inline std::set<uint32_t> s_knownSpells;
    inline std::set<uint32_t> s_creatures;

    // A string block that hands out offsets (CoATest::Wdbc puts the leading NUL in front of it).
    struct Strings {
        std::string block;

        uint32_t operator()(const char* s) {
            const uint32_t at = static_cast<uint32_t>(this->block.size()) + 1;
            this->block.append(s);
            this->block.push_back('\0');
            return at;
        }
    };

    // A file row from the struct dwords: an 'L' column becomes 16 locale slots and a flags dword.
    inline std::vector<uint32_t> Expand(const char* spec, const std::vector<uint32_t>& dwords) {
        std::vector<uint32_t> out;

        for (size_t i = 0; spec[i]; ++i) {
            const uint32_t v = i < dwords.size() ? dwords[i] : 0;

            if (spec[i] == 'L') {
                out.insert(out.end(), 16, v);
                out.push_back(0);
            } else {
                out.push_back(v);
            }
        }

        return out;
    }

    inline std::vector<uint8_t> Categories() {
        Strings s;
        const char* spec = "ddLddddddd";
        auto row = [&](uint32_t id, const char* type, const char* name, uint32_t tier, uint32_t inv, const char* icon, uint32_t mask) {
            return Expand(spec, { id, s(type), s(name), tier, inv, 0, s(icon), mask, 0, 0 });
        };
        std::vector<std::vector<uint32_t>> rows = {
            row(1, "APPEARANCE_TYPE_ITEM", "Head", 0, 1, "icon_head", 1),
            row(2, "APPEARANCE_TYPE_ITEM", "Chest", 0, 5, "icon_chest", 1),
            row(3, "APPEARANCE_TYPE_ITEM_SET", "Sets", 0, 0, "icon_sets", 1),
            row(4, "APPEARANCE_TYPE_COSMETIC_PET", "Pets", 1, 0, "icon_pets", 1),
            row(5, "APPEARANCE_TYPE_MOUNT", "Mounts", 0, 0, "icon_mounts", 1),
            row(6, "APPEARANCE_TYPE_SPELL_VISUAL", "Glows", 500, 0, "icon_glows", 1),
            row(7, "APPEARANCE_TYPE_ITEM", "Mage hats", 0, 1, "icon_mage", 0x80),
        };
        return CoATest::Wdbc(rows, s.block, 10);
    }

    inline std::vector<uint8_t> Appearances() {
        Strings s;
        const uint32_t item = s("APPEARANCE_DISPLAY_TYPE_ITEM");
        const uint32_t creature = s("APPEARANCE_DISPLAY_TYPE_CREATURE");
        const uint32_t set = s("APPEARANCE_DISPLAY_TYPE_ITEM_SET");
        const uint32_t spell = s("APPEARANCE_DISPLAY_TYPE_SPELL");
        // id, source, display type, +0xC, camera, cats x3, +0x20, +0x24, +0x28, +0x2C, gates x5
        auto row = [](uint32_t id, uint32_t source, uint32_t type, uint32_t c, uint32_t cat, uint32_t x20, uint32_t live) {
            return std::vector<uint32_t> { id, source, type, c, 0, cat, 0, 0, x20, 0, 0, id, live, 0, 0, 0, 0 };
        };
        std::vector<std::vector<uint32_t>> rows = {
            row(100, 1000, item, 1000, 1, 1000, 1),
            row(101, 1001, item, 1001, 2, 1001, 1),
            row(102, 1002, item, 1002, 1, 1002, 1),
            row(103, 50, set, 0, 3, 50, 1),
            row(104, 77, creature, 0, 4, 77, 1),
            row(105, 0, spell, 0, 6, 9000, 1),
            row(106, 1000, item, 1000, 1, 1000, 0),
            row(107, 0, creature, 0, 5, 0, 1),
        };
        return CoATest::Wdbc(rows, s.block);
    }

    inline std::vector<uint8_t> Details() {
        Strings s;
        return CoATest::Wdbc({ Expand("ddLdd", { 1, 100, s("Nice hat"), 12, 1 }) }, s.block, 5);
    }

    inline std::vector<uint8_t> GlobalStrings() {
        Strings s;
        return CoATest::Wdbc({ Expand("dddL", { 1, 3, s("NEW_OUTFIT"), s("New Outfit") }) }, s.block, 4);
    }

    inline bool ReadTables(const char* path, std::vector<uint8_t>& out) {
        const std::string p(path);
        auto is = [&](const char* name) { return p.find(name) != std::string::npos; };

        if (is("AppearanceCategories.dbc")) {
            out = Categories();
        } else if (is("ItemSetAppearances.dbc")) {
            out = CoATest::Wdbc({ { 1, 50, 103 } }, std::string(1, '\0'));
        } else if (is("ItemAppearances.dbc")) {
            out = CoATest::Wdbc({ { 1, 1000, 100 }, { 2, 1001, 101 }, { 3, 1002, 102 }, { 4, 1003, 100 } }, std::string(1, '\0'));
        } else if (is("SeasonalAppearances.dbc")) {
            // id, appearance, season, chapter, order, gates x5
            out = CoATest::Wdbc({ { 1, 100, 1, 1, 2, 1, 0, 0, 0, 0 }, { 2, 101, 1, 2, 1, 1, 0, 0, 0, 0 }, { 3, 105, 2, 1, 1, 1, 0, 0, 0, 0 },
                                    { 4, 106, 3, 1, 1, 0, 0, 0, 0, 0 } },
                std::string(1, '\0'));
        } else if (is("AppearanceDetails.dbc")) {
            out = Details();
        } else if (is("Appearances.dbc")) {
            out = Appearances();
        } else if (is("GlobalStrings.dbc")) {
            out = GlobalStrings();
        } else {
            return false;
        }

        return true;
    }

    inline const char* const kTables[] = { "DBFilesClient/AppearanceCategories.dbc", "DBFilesClient/Appearances.dbc",
        "DBFilesClient/AppearanceDetails.dbc", "DBFilesClient/ItemAppearances.dbc", "DBFilesClient/ItemSetAppearances.dbc",
        "DBFilesClient/SeasonalAppearances.dbc", "DBFilesClient\\GlobalStrings.dbc", "DBFilesClient/UICameras.dbc" };

    inline void UnloadTables() {
        for (const char* path : kTables) {
            CoA::Dbc::Get(path).Unload();
        }
    }

    inline void Realm() {
        CoATest::Bytes b;
        b.U32(1).U32(0).F32(0).F32(0).F32(0).U32(0).F32(0).F32(0).U32(0);
        b.U8(1);

        for (int i = 1; i < 8; ++i) {
            b.U8(0);
        }

        b.Str("ascension").Str("Realm").U8(0);
        CoATest::Deliver(CoA::RealmInfo::OnRealmInfo, 0x9BC, b);
    }

    inline bool Item(uint32_t id, CoA::Appearance::ItemRecord& out) {
        static const std::map<uint32_t, CoA::Appearance::ItemRecord> items = {
            { 1000, { 4, 1, 1 } }, { 1001, { 4, 1, 20 } }, { 1002, { 2, 0x10, 1 } }, { 1003, { 4, 1, 1 } } };
        auto it = items.find(id);

        if (it == items.end()) {
            return false;
        }

        out = it->second;
        return true;
    }

    inline const std::map<uint32_t, std::string>& Names() {
        static const std::map<uint32_t, std::string> names = {
            { 1000, "Iron Helm" }, { 1001, "Robe of Stars" }, { 1002, "Throwing Axe" }, { 1003, "Copper Helm" } };
        return names;
    }

    inline lua_State* Setup() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
        CoA::Dbc::SetFileReader(ReadTables);
        UnloadTables();
        Realm();
        s_class = 1;
        s_player = true;
        s_knownSpells.clear();
        s_creatures = { 77 };
        CoA::Appearance::Reset();
        CoA::Appearance::Deps& d = CoA::Appearance::Dependencies();
        d.hasPlayer = [] { return s_player; };
        d.playerClass = [] { return s_class; };
        d.knowsSpell = [](uint32_t spell) { return s_knownSpells.count(spell) != 0; };
        d.spellExists = [](uint32_t spell) { return spell != 0; };
        d.item = Item;
        d.itemSet = [](uint32_t id, CoA::Appearance::ItemSetRecord& out) {
            if (id != 50) {
                return false;
            }

            out.id = 50;
            out.name = "Tier Set";
            out.items[0] = 1000;
            out.items[1] = 1001;
            return true;
        };
        d.itemSetRange = [](uint32_t& lo, uint32_t& hi) {
            lo = 50;
            hi = 50;
        };
        d.creatureKnown = [](uint32_t entry) { return s_creatures.count(entry) != 0; };
        d.fetchItemInfo = [](const uint32_t* items, uint32_t n, std::unordered_map<uint32_t, CoA::Appearance::ItemInfo>& out) {
            for (uint32_t i = 0; i < n; ++i) {
                auto it = Names().find(items[i]);

                if (it != Names().end()) {
                    out[items[i]] = { it->second, 2 };
                }
            }

            return true;
        };
        d.itemInfo = [](uint32_t item) {
            auto it = Names().find(item);
            return it == Names().end() ? CoA::Appearance::ItemInfo() : CoA::Appearance::ItemInfo { it->second, 2 };
        };
        d.itemGuid = [](const char* text, uint64_t& guid) {
            guid = 0xABC;
            return strcmp(text, "0x10") == 0;
        };
        d.isActivePlayer = [](void* unit) { return unit == reinterpret_cast<void*>(1); };
        CoA::Appearance::Init();
        return CoATest::NewState(false);
    }

    inline void Teardown(lua_State* L) {
        lua_close(L);
        UnloadTables();
        CoA::Dbc::SetFileReader(nullptr);
    }

    inline V Feed(uint32_t opcode, CoATest::Bytes bytes) {
        CoATest::RecordSignals();
        CDataStore msg(bytes.data.data(), static_cast<uint32_t>(bytes.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result);
        CoATest::StopRecordingSignals();
        return CoATest::Signals();
    }

    inline V Run(lua_State* L, const char* chunk) {
        V out;
        const int base = lua_gettop(L);

        if (luaL_loadstring(L, chunk) != 0 || lua_pcall(L, 0, LUA_MULTRET, 0) != 0) {
            out.push_back(std::string("<error> ") + lua_tostring(L, -1));
            lua_settop(L, base);
            return out;
        }

        for (int i = base + 1; i <= lua_gettop(L); ++i) {
            out.push_back(lua_isnil(L, i) ? "nil" : lua_isboolean(L, i) ? (lua_toboolean(L, i) ? "true" : "false")
                : lua_istable(L, i) ? "table" : lua_tostring(L, i));
        }

        lua_settop(L, base);
        return out;
    }

    inline Packets Sent(lua_State* L, const char* chunk, V* results = nullptr) {
        CoATest::RecordPackets();
        V r = Run(L, chunk);
        CoATest::StopRecordingPackets();

        if (results) {
            *results = r;
        }

        return CoATest::SentPackets();
    }

    // 0x69A: the applied appearances, one per category index 0..7.
    inline void Current(const std::vector<uint32_t>& v) {
        CoATest::Bytes b;
        b.U32(static_cast<uint32_t>(v.size()));

        for (uint32_t a : v) {
            b.U32(a);
        }

        Feed(0x69A, b);
    }

    inline void Collect(const std::vector<uint32_t>& ids) {   // 0x699
        CoATest::Bytes b;
        b.U32(static_cast<uint32_t>(ids.size()));

        for (uint32_t a : ids) {
            b.U32(a).U32(1000);
        }

        Feed(0x699, b);
    }
}

#endif
