#ifndef COA_TESTS_CAFIXTURE_H
#define COA_TESTS_CAFIXTURE_H

// The shared setup of the character advancement tests: CharacterAdvancement / ClassTypes / TabTypes / Essence rows
// upserted as the DLL's structs, a live realm, and a fake client behind CoA::CA::Deps.

#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Modules/CAInternal.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace CAFixture {
    using V = std::vector<std::string>;

    const uint64_t kPlayer = 0x21;

    inline uint8_t s_class = 1;
    inline uint32_t s_level = 10;
    inline bool s_inCombat = false;
    inline bool s_dead = false;
    inline bool s_hasPlayer = true;
    inline int32_t s_mapType = -1;
    inline uint32_t s_money = 0;
    inline std::map<uint32_t, uint32_t> s_items;
    inline std::set<uint32_t> s_clientSpells;
    inline std::set<uint32_t> s_runeSpells;

    // A DLL-side struct row and its table's file image. Rows are written at their struct offsets; the file the fake reader
    // serves is the inverse of CoA::Dbc's layout conversion (a localized column fills all 16 locale slots).
    struct Table {
        const char* path;
        const char* spec;   // nullptr: no layout, rows as in the file
        uint32_t structSize;
        std::vector<std::vector<uint8_t>> rows;
        std::string strings = std::string(1, '\0');

        uint32_t Str(const char* s) {
            const uint32_t at = static_cast<uint32_t>(this->strings.size());
            this->strings.append(s);
            this->strings.push_back('\0');
            return at;
        }

        std::vector<uint8_t> File() const {
            uint32_t fileWidth = 0;

            if (this->spec) {
                for (const char* c = this->spec; *c; ++c) {
                    fileWidth += *c == 'b' ? 1 : *c == 'L' ? 68 : 4;
                }
            } else {
                fileWidth = this->structSize;
            }

            std::vector<uint8_t> out(20);
            const uint32_t cols = this->spec ? static_cast<uint32_t>(strlen(this->spec)) : this->structSize / 4;
            const uint32_t header[5] = { 0x43424457, static_cast<uint32_t>(this->rows.size()), cols, fileWidth,
                static_cast<uint32_t>(this->strings.size()) };
            memcpy(out.data(), header, 20);

            for (const std::vector<uint8_t>& row : this->rows) {
                if (!this->spec) {
                    out.insert(out.end(), row.begin(), row.end());
                    continue;
                }

                uint32_t at = 0;

                for (const char* c = this->spec; *c; ++c) {
                    if (*c == 'b') {
                        out.push_back(row[at++]);
                        continue;
                    }

                    at = (at + 3) & ~3u;

                    for (int k = 0; k < (*c == 'L' ? 16 : 1); ++k) {
                        out.insert(out.end(), row.begin() + at, row.begin() + at + 4);
                    }

                    if (*c == 'L') {
                        out.insert(out.end(), 4, 0);
                    }

                    at += 4;
                }
            }

            out.insert(out.end(), this->strings.begin(), this->strings.end());
            return out;
        }
    };

    inline const char* kCaLayout =
#include "CoA/Tests/CALayout.inc"
        ;

    inline Table& CaTable() {
        static Table t { "CharacterAdvancement.dbc", kCaLayout, 0x1FC, {} };
        return t;
    }

    inline Table& ClassTypeTable() {
        static Table t { "CharacterAdvancementClassTypes.dbc", "ddddddL", 0x1C, {} };
        return t;
    }

    inline Table& TabTypeTable() {
        static Table t { "CharacterAdvancementTabTypes.dbc", "ddL", 0xC, {} };
        return t;
    }

    inline Table& EssenceTable() {
        static Table t { "CharacterAdvancementEssence.dbc", nullptr, 0x24, {} };
        return t;
    }

    // CharacterAdvancementCategories: id, order, level, flag, +0x10 string, L, L.
    inline Table& CategoryTable() {
        static Table t { "CharacterAdvancementCategories.dbc", "dddddLL", 0x1C, {} };
        return t;
    }

    // SpellTagTypes: id, parent, order, hidden, ..., +0x68 icon, L name, L description.
    inline Table& TagTypeTable() {
        static Table t { "SpellTagTypes.dbc", "dddddddddddddddddddddddddddLL", 0x74, {} };
        return t;
    }

    // SpellSpellSuggestions / SpellStatSuggestions: id, source spell, suggested spell / stat, weight.
    inline Table& SpellSuggestionTable() {
        static Table t { "SpellSpellSuggestions.dbc", nullptr, 0x10, {} };
        return t;
    }

    inline Table& StatSuggestionTable() {
        static Table t { "SpellStatSuggestions.dbc", nullptr, 0x10, {} };
        return t;
    }

    inline void PutAt(Table& t, std::vector<std::pair<uint32_t, uint32_t>> words) {
        std::vector<uint8_t> row(t.structSize, 0);

        for (const auto& w : words) {
            memcpy(&row[w.first], &w.second, 4);
        }

        t.rows.push_back(row);
    }

    // A CharacterAdvancement.dbc struct row (0x1FC bytes).
    class CaRow {
    public:
        explicit CaRow(uint32_t id) : bytes(0x1FC, 0) {
            this->U32(0, id);
            this->Str(4, "Ability");
            this->Str(0x40, "Normal");
            this->Str(0x50, "Normal");
            this->Str(0x60, "Normal");
            this->Str(0x120, "EXPANSION_CLASSIC");
            this->U32(0x74, 1);       // wildcard group 1 (every row but a choice group's)
            this->U32(0x80, 3);       // class type: Warrior
            this->U32(0x84, 7);       // tab: Protection
            this->bytes[0x128] = 1;   // live
        }

        CaRow& U32(uint32_t off, uint32_t v) {
            memcpy(&this->bytes[off], &v, 4);
            return *this;
        }

        CaRow& Str(uint32_t off, const char* s) {
            return this->U32(off, CaTable().Str(s));
        }

        CaRow& Byte(uint32_t off, uint8_t v) {
            this->bytes[off] = v;
            return *this;
        }

        CaRow& Spell(uint32_t rank, uint32_t spell) {
            return this->U32(0x14 + (rank - 1) * 4, spell);
        }

        void Put() {
            CaTable().rows.push_back(this->bytes);
        }

        std::vector<uint8_t> bytes;
    };

    inline void PutWords(Table& t, const std::vector<uint32_t>& words) {
        std::vector<uint8_t> row(t.structSize, 0);
        memcpy(row.data(), words.data(), words.size() * 4);
        t.rows.push_back(row);
    }

    // Every row the CA tests share (the tables load once per process):
    //   100  Ability, spells 5000 / 5001 (2 ranks), AE 2
    //   101  Ability, spell 5000, Mage class type (not this player's class)
    //   102  Talent, spell 5100, TE 1, requires 100
    //   103  Ability, spell 5200, AE 100, level 20
    //   104 / 105  Ability, choice group 5, wildcard AE 3
    //   110  a mastery (flag 0x1000), spell 5400; 111 carries it (+0x1B0), spell 5401
    //   120  auto-learned (flag 0x100000), free, spell 5500
    //   121  cast requirement Warrior / Protection 5 points; 122 gives 5 points
    //   130..140  one row per learn-rule slot (see CAEdgeTest.cpp)
    inline void BuildTables() {
        static bool built = false;

        if (built) {
            return;
        }

        built = true;
        CaRow(100).Spell(1, 5000).Spell(2, 5001).U32(0x38, 2).U32(0x58, 2).Put();
        CaRow(101).Spell(1, 5000).U32(0x80, 5).Put();
        CaRow(102).Str(4, "Talent").Spell(1, 5100).U32(0x3C, 1).U32(0x08, 100).Put();
        CaRow(103).Spell(1, 5200).U32(0x38, 100).U32(0x68, 20).Put();
        CaRow(104).Spell(1, 5300).U32(0x74, 5).U32(0x58, 3).Put();
        CaRow(105).Spell(1, 5301).U32(0x74, 5).U32(0x58, 3).Put();
        CaRow(110).Spell(1, 5400).U32(0x124, 0x1000).Put();
        CaRow(111).Spell(1, 5401).U32(0x1B0, 110).Put();
        CaRow(120).Spell(1, 5500).U32(0x124, 0x100000).Put();   // auto-learned, free
        CaRow(121).Spell(1, 5600).U32(0x1D8, 3).U32(0x1E4, 7).U32(0x1F0, 5).Put();   // cast requirement: 5 points
        CaRow(122).Spell(1, 5601).U32(0xA0, 5).U32(0x44, 3).Put();               // 5 points in class 3 / tab 7, quality cost 3
        CaRow(130).Spell(1, 5700).U32(0x124, 0x20000).Put();                     // DISPLAY_ENTRY
        CaRow(131).Spell(1, 5701).U32(0x124, 8).Put();                           // DISABLED
        CaRow(132).Spell(1, 5702).Byte(0xE0, 1).Put();                     // starting nodes 132 / 133
        CaRow(133).Spell(1, 5703).Byte(0xE0, 1).Put();
        CaRow(134).Spell(1, 5704).U32(0xE4, 100).Put();                          // connected to 100
        CaRow(135).Spell(1, 5705).U32(0x74, 9).Put();                            // choice group 9: 135 / 136
        CaRow(136).Spell(1, 5706).U32(0x74, 9).Put();
        CaRow(139).Spell(1, 5709).U32(0x88, 50).Put();                           // needs 50 AE invested
        CaRow(140).Spell(1, 5710).U32(0xA4, 5).Put();                            // needs 5 class points
        CaRow(141).Spell(1, 5711).Put();                                         // rewritten by SMSG 0x64A
        CaRow(142).Str(4, "TalentAbility").Spell(1, 5712).Put();
        CaRow(150).Spell(1, 5800).U32(0x80, 9).Put();                                          // a CoA-class ability
        CaRow(151).Str(4, "Talent").Spell(1, 5801).Spell(2, 5802).U32(0x80, 9).Put();          // a CoA-class talent, 2 ranks                // a talent ability
        // ClassTypes: id, name, class, hero, coa, stock, L.
        PutWords(ClassTypeTable(), { 3, ClassTypeTable().Str("Warrior"), 1, 0, 0, 0, 0 });
        PutWords(ClassTypeTable(), { 5, ClassTypeTable().Str("Mage"), 8, 0, 0, 0, 0 });
        PutWords(ClassTypeTable(), { 9, ClassTypeTable().Str("General"), 0, 0, 1, 0, 0 });   // any CoA class (12..32)
        PutWords(TabTypeTable(), { 7, TabTypeTable().Str("Protection"), 0 });
        // Essence: id, level, class, draft, wildcard, specDraft, u2c, AE, TE.
        PutWords(EssenceTable(), { 1, 10, 1, 0, 0, 0, 0, 20, 5 });
        PutWords(EssenceTable(), { 2, 20, 1, 0, 0, 0, 0, 200, 10 });
        PutWords(EssenceTable(), { 3, 30, 1, 0, 0, 0, 0, 300, 15 });
        PutAt(CategoryTable(), { { 0, 1 }, { 4, 2 }, { 8, 30 }, { 0x10, CategoryTable().Str("c1") }, { 0x14, CategoryTable().Str("One") } });
        PutAt(CategoryTable(), { { 0, 2 }, { 4, 1 }, { 8, 40 }, { 0xC, 1 }, { 0x18, CategoryTable().Str("two") } });
        PutAt(CategoryTable(), { { 0, 3 }, { 4, 3 }, { 8, 10 } });
        PutAt(TagTypeTable(), { { 0, 1 }, { 8, 5 }, { 0x6C, TagTypeTable().Str("Root A") }, { 0x70, TagTypeTable().Str("desc") } });
        PutAt(TagTypeTable(), { { 0, 2 }, { 8, 1 }, { 0x68, TagTypeTable().Str("icon") }, { 0x6C, TagTypeTable().Str("Root B") } });
        PutAt(TagTypeTable(), { { 0, 3 }, { 4, 1 }, { 8, 0 } });
        PutAt(TagTypeTable(), { { 0, 4 }, { 4, 1 }, { 0xC, 1 } });   // hidden
        PutWords(SpellSuggestionTable(), { 1, 5000, 5200, 30 });   // 100 suggests 103
        PutWords(SpellSuggestionTable(), { 2, 5500, 5100, 20 });   // 120 suggests 102
        PutWords(StatSuggestionTable(), { 1, 5000, 3, 7 });
        PutWords(StatSuggestionTable(), { 2, 5000, 5, 2 });
    }

    inline bool ReadTables(const char* path, std::vector<uint8_t>& out) {
        for (Table* t : { &CaTable(), &ClassTypeTable(), &TabTypeTable(), &EssenceTable(), &CategoryTable(), &TagTypeTable(), &SpellSuggestionTable(), &StatSuggestionTable() }) {
            const size_t n = strlen(path);
            const size_t m = strlen(t->path);

            if (n > m && strcmp(path + n - m, t->path) == 0 && (path[n - m - 1] == '\\' || path[n - m - 1] == '/')) {
                out = t->File();
                return true;
            }
        }

        return false;
    }

    inline void Realm() {
        CoATest::Bytes b;
        b.U32(1).U32(0).F32(0).F32(0).F32(0).U32(0).F32(0).F32(0).U32(0);
        b.U8(1);

        for (int i = 1; i < 8; ++i) {
            b.U8(0);
        }

        b.Str("").Str("Realm").U8(0);
        CoATest::Deliver(CoA::RealmInfo::OnRealmInfo, 0x9BC, b);
    }

    inline const std::vector<uint32_t>& NoChain(uint32_t) {
        static const std::vector<uint32_t> none;
        return none;
    }

    inline void Setup() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
        BuildTables();
        CoA::Dbc::SetFileReader(ReadTables);
        Realm();
        s_class = 1;
        s_level = 10;
        s_inCombat = false;
        s_dead = false;
        s_hasPlayer = true;
        s_mapType = -1;
        s_money = 0;
        s_items.clear();
        s_clientSpells.clear();
        s_runeSpells.clear();
        CoA::CA::Reset();
        CoA::CA::Deps& d = CoA::CA::Dependencies();
        d.activePlayerGuid = [] { return s_hasPlayer ? kPlayer : 0; };
        d.unitExists = [](uint64_t guid) { return s_hasPlayer && guid == kPlayer; };
        d.unitClass = [](uint64_t guid) { return guid == kPlayer ? s_class : static_cast<uint8_t>(0); };
        d.unitLevel = [](uint64_t guid) { return guid == kPlayer ? s_level : 0u; };
        d.unitInCombat = [](uint64_t) { return s_inCombat; };
        d.unitDead = [](uint64_t) { return s_dead; };
        d.unitSchoolImmune = [](uint64_t) { return false; };
        d.unitItemCount = [](uint64_t, uint32_t item) {
            auto it = s_items.find(item);
            return it == s_items.end() ? 0u : it->second;
        };
        d.unitMoney = [](uint64_t) { return s_money; };
        d.mapType = [] { return s_mapType; };
        d.battlegroundState = [] { return 0; };
        d.clientKnowsSpell = [](uint32_t spell) { return s_clientSpells.count(spell) != 0; };
        d.spellExists = [](uint32_t spell) { return spell != 0; };
        d.spellRuneCost = [](uint32_t spell) { return s_runeSpells.count(spell) != 0; };
        d.spellHidden = [](uint32_t) { return false; };
        d.spellMaxId = [] { return 100000u; };
        d.firstRank = [](uint32_t spell) { return spell; };
        d.rankChain = NoChain;
        d.wildcardUndesired = [](uint32_t, uint8_t) { return false; };
        CoA::CA::detail::BuildIndex();
    }

    inline void Teardown() {
        CoA::Dbc::SetFileReader(nullptr);
    }

    // A prepared build of the fake player's class at a level.
    inline CoA::CA::Build* NewBuild(uint32_t level) {
        CoA::CA::Build* b = new CoA::CA::Build();
        b->classKey = 1;
        b->stock = 1;
        b->level = level;
        b->Prepare();
        return b;
    }

    // Lua results as strings ("nil", "true" / "false", numbers / strings as lua_tostring gives them, "table").
    inline V Run(lua_State* L, const char* chunk) {
        V out;
        const int base = lua_gettop(L);

        if (luaL_loadstring(L, chunk) != 0 || lua_pcall(L, 0, LUA_MULTRET, 0) != 0) {
            std::string msg = lua_tostring(L, -1);
            const size_t at = msg.compare(0, 8, "[string ") == 0 ? msg.find("]:") : std::string::npos;

            if (at != std::string::npos) {   // luaL_error's "where": the chunk and line
                msg.erase(0, msg.find(": ", at) + 2);
            }

            out.push_back("<error> " + msg);
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

    inline V Feed(uint32_t opcode, CoATest::Bytes bytes) {
        CoATest::RecordSignals();
        CDataStore msg(bytes.data.data(), static_cast<uint32_t>(bytes.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result);
        CoATest::StopRecordingSignals();
        return CoATest::Signals();
    }
}

#endif
