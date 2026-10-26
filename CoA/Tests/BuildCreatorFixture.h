#ifndef COA_TESTS_BUILDCREATORFIXTURE_H
#define COA_TESTS_BUILDCREATORFIXTURE_H

// The shared setup of the Build Creator tests: the CA fixture (a Warrior player, CharacterAdvancement rows 100 / 102 and
// a patched two-rank talent 160), a MysticEnchant.dbc with three enchants, fake rank chains and a fake client behind
// CoA::BuildCreator::Deps. BuildCreator::Init runs after CA's setup, so the CA Deps it fills are the real ones.
//
//   enchant 1001  rare (stock column), realm flag live      enchant 1002  legendary, realm flag live
//   enchant 1003  uncommon, no realm flag
//   spell chains  5000 -> 5001 (row 100), 6000 -> 6001 (row 160, a talent); every other spell is its own chain

#include "CoA/Source/Modules/Account.h"
#include "CoA/Source/Modules/BuildCreator.h"
#include "CoA/Source/Modules/GameEvents.h"
#include "CoA/Source/Modules/MysticEnchant.h"
#include "CoA/Tests/CAFixture.h"
#include <string>
#include <vector>

namespace BCFixture {
    using V = std::vector<std::string>;
    using CAFixture::Feed;
    using CAFixture::Run;

    inline bool s_bcPlayer = true;
    inline const char* s_bcName = "Tester";
    inline int64_t s_now = 1700000000;

    // ---- an entry on the wire (FUN_100f9330) -------------------------------------------------------------------
    struct S {
        uint32_t spell = 0;
        uint32_t level = 0;
        uint8_t core = 0;
        uint8_t optimal = 0;
        uint8_t empowering = 0;
        uint8_t synergistic = 0;
        uint32_t flags = 0;
        std::string comment;
    };

    struct C {
        uint32_t id = 0;
        uint32_t stacks = 0;
        uint32_t level = 0;
        uint32_t flags = 0;
        std::string comment;
    };

    struct T {
        uint32_t type = 0;
        std::string comment;
    };

    struct E {
        std::string id;
        std::string leveling;
        std::string pve;
        std::string pvp;
        uint32_t cls = 1;
        std::string author = "Someone";
        std::string name = "A build";
        std::string subtext;
        std::string description = "Desc";
        std::string icon = "icon";
        uint32_t upvotes = 0;
        uint64_t created = 0;
        uint64_t updated = 0;
        uint32_t category = 0;
        uint32_t roles = 8;
        uint32_t stat = 0;
        std::vector<S> spells;
        std::vector<C> enchants;
        std::vector<T> weapons;
        std::vector<T> armor;
        uint8_t hidden = 0;
        uint32_t flags = 0;
        uint32_t difficulty = 0;
    };

    inline void Put(CoATest::Bytes& b, const E& e) {
        b.Str(e.id.c_str()).Str(e.leveling.c_str()).Str(e.pve.c_str()).Str(e.pvp.c_str()).U32(e.cls);
        b.Str(e.author.c_str()).Str(e.name.c_str()).Str(e.subtext.c_str()).Str(e.description.c_str()).Str(e.icon.c_str());
        b.U32(e.upvotes).U64(e.created).U64(e.updated).U32(e.category).U32(e.roles).U32(e.stat);
        b.U32(static_cast<uint32_t>(e.spells.size()));

        for (const S& s : e.spells) {
            b.U32(s.spell).U32(s.level).U8(s.core).U8(s.optimal).U8(s.empowering).U8(s.synergistic).U32(s.flags).Str(s.comment.c_str());
        }

        b.U32(static_cast<uint32_t>(e.enchants.size()));

        for (const C& c : e.enchants) {
            b.U32(c.id).U32(c.stacks).U32(c.level).U32(c.flags).Str(c.comment.c_str());
        }

        for (const std::vector<T>* list : { &e.weapons, &e.armor }) {
            b.U32(static_cast<uint32_t>(list->size()));

            for (const T& t : *list) {
                b.U32(t.type).Str(t.comment.c_str());
            }
        }

        b.U8(e.hidden).U32(e.flags).U32(e.difficulty);
    }

    inline E Build(const std::string& id, std::vector<S> spells = { { 5000, 10 } }) {
        E e;
        e.id = id;
        e.spells = spells;
        return e;
    }

    // SMSG 0x631 with one entry.
    inline V Deliver631(const E& e, const char* text = "q") {
        CoATest::Bytes b;
        b.Str(text);
        Put(b, e);
        return Feed(0x631, b);
    }

    // SMSG 0x62F with one page of entries.
    inline V DeliverPage(const std::vector<E>& entries, uint32_t page = 0, uint32_t pages = 1, const char* text = "p") {
        CoATest::Bytes b;
        b.Str(text).U32(page).U32(pages).U32(static_cast<uint32_t>(entries.size()));

        for (const E& e : entries) {
            Put(b, e);
        }

        return Feed(0x62F, b);
    }

    // ---- the tables --------------------------------------------------------------------------------------------
    inline std::vector<uint32_t> EnchantRow(uint32_t spell, uint32_t quality, uint32_t realm) {
        std::vector<uint32_t> r(31, 0);
        r[0] = spell - 1000;
        r[1] = spell;
        r[3] = quality;
        r[4] = quality;
        r[8] = realm;
        return r;
    }

    // String block offsets (CoATest::Wdbc puts "" at 0).
    const uint32_t kRare = 1;          // "RE_QUALITY_RARE" (15)
    const uint32_t kLegendary = 17;    // "RE_QUALITY_LEGENDARY" (20)
    const uint32_t kUncommon = 38;     // "RE_QUALITY_UNCOMMON"

    inline bool ReadTables(const char* path, std::vector<uint8_t>& out) {
        if (strstr(path, "MysticEnchant.dbc")) {
            out = CoATest::Wdbc({ EnchantRow(1001, kRare, 1), EnchantRow(1002, kLegendary, 1), EnchantRow(1003, kUncommon, 0) },
                std::string("RE_QUALITY_RARE") + '\0' + "RE_QUALITY_LEGENDARY" + '\0' + "RE_QUALITY_UNCOMMON" + '\0');
            return true;
        }

        return CAFixture::ReadTables(path, out);
    }

    inline CoA::Dbc::Table& Enchants() {
        return CoA::Dbc::Get("DBFilesClient\\MysticEnchant.dbc");
    }

    inline CoA::Dbc::Table& Essence() {
        return CoA::Dbc::Get("DBFilesClient\\CharacterAdvancementEssence.dbc");
    }

    inline CoA::Dbc::Table& Advancement() {
        return CoA::Dbc::Get("DBFilesClient\\CharacterAdvancement.dbc");
    }

    // ---- fake rank chains --------------------------------------------------------------------------------------
    inline uint32_t FirstRank(uint32_t spell) {
        return spell == 5001 ? 5000 : spell == 6001 ? 6000 : spell;
    }

    inline uint32_t RankNumber(uint32_t spell) {
        return spell == 5001 || spell == 6001 ? 2 : 1;
    }

    inline const std::vector<uint32_t>& Chain(uint32_t first) {
        static const std::vector<uint32_t> a = { 5000, 5001 };
        static const std::vector<uint32_t> t = { 6000, 6001 };
        static std::vector<uint32_t> one;
        one = { first };
        return first == 5000 ? a : first == 6000 ? t : one;
    }

    inline void InstallDeps() {
        CoA::BuildCreator::Deps& d = CoA::BuildCreator::Dependencies();
        d.hasPlayer = [] { return s_bcPlayer; };
        d.playerClass = [] { return CAFixture::s_class; };
        d.playerName = [] { return s_bcName; };
        d.firstRank = FirstRank;
        d.rankNumber = RankNumber;
        d.rankChain = Chain;
        d.now = [] { return s_now; };
        CoA::CA::Deps& ca = CoA::CA::Dependencies();
        ca.firstRank = FirstRank;
        ca.rankChain = Chain;
        ca.rankNumber = RankNumber;
        CoA::MysticEnchant::Deps& me = CoA::MysticEnchant::Dependencies();
        me.hasPlayer = [] { return true; };
        me.playerClass = [] { return CAFixture::s_class; };
    }

    // Row 160: row 102 (a Warrior talent) as a two-rank talent 6000 / 6001 with no requirement.
    inline void PatchTalent() {
        CoA::Dbc::Table& t = Advancement();
        const uint8_t* src = t.Row(102);
        std::vector<uint8_t> row(src, src + 0x1FC);
        const uint32_t words[4][2] = { { 0, 160 }, { 8, 0 }, { 0x14, 6000 }, { 0x18, 6001 } };

        for (const auto& w : words) {
            memcpy(&row[w[0]], &w[1], 4);
        }

        t.Upsert(160, row);
        CoA::CA::detail::BuildIndex();
    }

    // The CA fixture's Warrior with an active spec (0x725) and no entries (0x726), then the Build Creator.
    inline lua_State* Start(uint8_t cls = 1, uint32_t level = 10) {
        Enchants().Unload();
        CAFixture::Setup();
        CoA::Dbc::SetFileReader(ReadTables);
        CAFixture::s_class = cls;
        CAFixture::s_level = level;
        s_bcPlayer = true;
        s_bcName = "Tester";
        s_now = 1700000000;
        CoA::GameEvents::Reset();
        CoA::Account::Reset();
        CoA::MysticEnchant::Reset();
        CoA::MysticEnchant::Init();
        CoA::BuildCreator::Reset();
        CoA::CA::Init();
        CoA::BuildCreator::Init();
        InstallDeps();
        PatchTalent();
        // Essence at level 60 for the Warrior (the server's max level here): 600 AE, 30 TE.
        const uint32_t essence[9] = { 4, 60, 1, 0, 0, 0, 0, 600, 30 };
        Essence().Upsert(4, std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(essence), reinterpret_cast<const uint8_t*>(essence) + 0x24));
        Feed(0x725, CoATest::Bytes().U32(0).U32(1));
        Feed(0x726, CoATest::Bytes().U32(0));
        return CoATest::NewState(false);
    }

    inline void Stop(lua_State* L) {
        lua_close(L);
        CoATest::StopRecordingPackets();
        CoA::BuildCreator::Reset();
        CoA::MysticEnchant::Reset();
        CoA::CA::Reset();
        CoA::GameEvents::Reset();
        CoA::Account::Reset();
        Enchants().Unload();
        Advancement().Unload();   // drops row 160; the next CA test loads the fixture's file again
        Essence().Unload();
        CAFixture::Teardown();
    }

    inline V R(lua_State* L, const std::string& chunk) {
        return Run(L, chunk.c_str());
    }

    inline void Gm(uint32_t level) {
        CoATest::Bytes b;
        b.U32(0).U32(level).U8(0).U8(0).U32(0);
        CoATest::Deliver(CoA::Account::OnAccountInfo, 0x9BB, b);
    }

    // A level-cap game event active (0x1E6: the server max level is 25).
    inline void GameEvent(uint16_t id, bool active) {
        CoATest::Bytes b;
        b.U16(id).U8(0).U64(0).U64(0).U8(active ? 1 : 0).I32(0);
        CoATest::Deliver(CoA::GameEvents::OnGameEventInfo, 0x9BD, b);
    }

    // The Lua table of a pending-build spell argument.
    inline std::string SpellArg(uint32_t spell, uint32_t level, const char* extra = "") {
        return "{ Spell = " + std::to_string(spell) + ", Level = " + std::to_string(level) + extra + " }";
    }
}

#endif
