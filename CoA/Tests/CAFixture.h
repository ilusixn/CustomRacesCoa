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
        // ClassTypes: id, name, class, hero, coa, stock, L.
        PutWords(ClassTypeTable(), { 3, ClassTypeTable().Str("Warrior"), 1, 0, 0, 0, 0 });
        PutWords(ClassTypeTable(), { 5, ClassTypeTable().Str("Mage"), 8, 0, 0, 0, 0 });
        PutWords(TabTypeTable(), { 7, TabTypeTable().Str("Protection"), 0 });
        // Essence: id, level, class, draft, wildcard, specDraft, u2c, AE, TE.
        PutWords(EssenceTable(), { 1, 10, 1, 0, 0, 0, 0, 20, 5 });
        PutWords(EssenceTable(), { 2, 20, 1, 0, 0, 0, 0, 200, 10 });
        PutWords(EssenceTable(), { 3, 30, 1, 0, 0, 0, 0, 300, 15 });
    }

    inline bool ReadTables(const char* path, std::vector<uint8_t>& out) {
        for (Table* t : { &CaTable(), &ClassTypeTable(), &TabTypeTable(), &EssenceTable() }) {
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
