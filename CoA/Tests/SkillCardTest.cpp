// C_SkillCardCollection / C_SkillCard (SkillCardCollection.cpp / SkillCardSlots.cpp): every SMSG and its events, every
// CMSG byte-exact, the card store and its patches, the collection filter container (argument groups, quality words,
// the lua_toarray reader, populate-key order, text over name / description, the mode / class gates), the sealed-card and
// booster checks, the slot blocks, the quest groups and the glue resets.
#include "CoA/Source/Modules/BridgeStore.h"
#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Modules/CustomStore.h"
#include "CoA/Source/Modules/GameMode.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Modules/SkillCard.h"
#include "CoA/Source/Modules/Wildcard.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/MemoryBridge.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;
    using Packets = std::vector<std::vector<uint8_t>>;

    // ---- the string block --------------------------------------------------------------------------------------------
    std::string s_strings;   // Wdbc puts the leading "" in front
    std::map<std::string, uint32_t> s_offsets;

    uint32_t S(const std::string& s) {
        auto it = s_offsets.find(s);

        if (it != s_offsets.end()) {
            return it->second;
        }

        const uint32_t at = static_cast<uint32_t>(s_strings.size()) + 1;
        s_strings += s;
        s_strings += '\0';
        s_offsets[s] = at;
        return at;
    }

    std::vector<uint32_t> Card(uint32_t id, uint32_t card, uint32_t rank, uint32_t item, uint32_t spell, const char* type,
        const char* expansion, const char* cls, const char* quality, uint32_t wildcard, uint32_t draft, uint32_t starter) {
        return { id, card, rank, item, spell, S(type), S(expansion), S(cls), S(quality), wildcard, draft, starter };
    }

    std::vector<std::vector<uint32_t>> Cards() {
        return {
            Card(1, 100, 1, 5001, 9001, "SKILL_CARD_DEFAULT_NORMAL", "EXPANSION_CLASSIC", "CLASS_WARRIOR", "SKILL_CARD_COMMON", 1, 0, 0),
            Card(2, 100, 2, 5002, 9007, "SKILL_CARD_DEFAULT_NORMAL", "EXPANSION_CLASSIC", "CLASS_WARRIOR", "SKILL_CARD_COMMON", 1, 0, 0),
            Card(3, 200, 1, 5003, 9002, "SKILL_CARD_DEFAULT_NORMAL", "EXPANSION_CLASSIC", "CLASS_WARRIOR", "SKILL_CARD_RARE", 1, 0, 0),
            Card(4, 300, 1, 5004, 9003, "SKILL_CARD_DEFAULT_NORMAL", "EXPANSION_THE_BURNING_CRUSADE", "CLASS_NONE", "SKILL_CARD_LEGENDARY", 1, 0, 0),
            Card(5, 400, 1, 5005, 9004, "SKILL_CARD_DEFAULT_GOLDEN", "EXPANSION_CLASSIC", "CLASS_WARRIOR", "SKILL_CARD_UNCOMMON", 1, 0, 1),
            Card(6, 500, 1, 5006, 9005, "SKILL_CARD_DEFAULT_NORMAL", "EXPANSION_CLASSIC", "CLASS_WARRIOR", "SKILL_CARD_EPIC", 0, 1, 0),
            Card(7, 600, 1, 5007, 9006, "SKILL_CARD_LUCKY_NORMAL", "EXPANSION_CLASSIC", "CLASS_WARRIOR", "SKILL_CARD_COMMON", 1, 0, 0),
        };
    }

    // SealedCardCosts: row id then the 24 per-type costs (+4 + type * 4).
    std::vector<uint32_t> CostRow(uint32_t id, std::map<uint32_t, uint32_t> costs) {
        std::vector<uint32_t> row(25, 0);
        row[0] = id;

        for (const auto& c : costs) {
            row[1 + c.first] = c.second;
        }

        return row;
    }

    std::vector<uint32_t> QuestRow(uint32_t id, uint32_t group8, uint32_t group12, uint32_t level) {
        std::vector<uint32_t> row(0x1D, 0);
        row[0] = id;
        row[2] = group8;
        row[3] = group12;
        row[5] = level;      // +0x14
        row[6] = id + 1;     // +0x18 reward item 1
        row[10] = 3;         // +0x28 reward amount 1
        row[14] = id + 2;    // +0x38 required item 1
        row[20] = 4;         // +0x50 required count 1
        return row;
    }

    bool ReadTables(const char* path, std::vector<uint8_t>& out) {
        if (strstr(path, "SkillCard.dbc")) {
            const std::vector<std::vector<uint32_t>> cards = Cards();
            out = CoATest::Wdbc(cards, s_strings);
            return true;
        }

        if (strstr(path, "SealedCardCosts.dbc")) {
            out = CoATest::Wdbc({ CostRow(1, { { 0, 10 }, { 4, 5 }, { 20, 50 } }), CostRow(2, { { 0, 20 }, { 4, 5 }, { 20, 60 } }),
                CostRow(3, { { 20, 70 } }) }, std::string(1, '\0'));
            return true;
        }

        if (strstr(path, "Quest.dbc")) {
            out = CoATest::Wdbc({ QuestRow(40, 0x65, 0, 10), QuestRow(41, 0x66, 0xC9 + 3, 20), QuestRow(42, 7, 0x191, 30) },
                std::string(1, '\0'));
            return true;
        }

        return false;
    }

    // ---- the client ----------------------------------------------------------------------------------------------------
    bool s_player = true;
    uint8_t s_class = 1;
    uint32_t s_level = 30;
    uint32_t s_money = 0;
    int32_t s_spec = 0;
    std::map<uint32_t, uint32_t> s_held;

    void Realm(bool dev, uint32_t ruleset = 2) {
        CoATest::Bytes b;
        b.U32(1).U32(ruleset).F32(0).F32(0).F32(0).U32(0).F32(0).F32(0).U32(0);

        for (int i = 0; i < 8; ++i) {
            b.U8(i == 4 && dev ? 1 : 0);
        }

        b.Str("").Str("Realm").U8(0);
        CoATest::Deliver(CoA::RealmInfo::OnRealmInfo, 0x9BC, b);
    }

    void Unload() {
        CoA::Dbc::Get("DBFilesClient\\SkillCard.dbc").Unload();
        CoA::Dbc::Get("DBFilesClient\\SealedCardCosts.dbc").Unload();
    }

    V Dispatch(uint32_t opcode, CoATest::Bytes b) {
        CoATest::RecordSignals();
        CDataStore msg(b.data.data(), static_cast<uint32_t>(b.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result);
        CoATest::StopRecordingSignals();
        return CoATest::Signals();
    }

    void Mode(uint32_t mode) {
        Dispatch(0x90B, CoATest::Bytes().U32(mode));
    }

    lua_State* Start() {
        Cards();   // the string block
        CoA::Dbc::SetFileReader(ReadTables);
        Unload();
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
        CoA::RealmInfo::Reset();
        Realm(false);
        CoA::GameMode::Reset();
        CoA::GameMode::Init();
        CoA::GameMode::Dependencies().activeSpecIndex = [] { return s_spec; };
        CoA::CustomStore::Reset();
        CoA::Wildcard::Reset();
        CoA::SkillCard::Reset();
        CoA::SkillCardSlots::Reset();
        CoA::SkillCard::Init();
        CoA::SkillCardSlots::Init();
        s_player = true;
        s_class = 1;
        s_level = 30;
        s_money = 0;
        s_spec = 0;
        s_held.clear();
        CoA::SkillCard::Deps& d = CoA::SkillCard::Dependencies();
        d.hasPlayer = [] { return s_player; };
        d.playerClass = [] { return s_class; };
        d.playerLevel = [] { return s_level; };
        d.playerMoney = [] { return s_money; };
        d.heldCount = [](uint32_t item) { return s_held[item]; };
        d.spellName = [](uint32_t spell, std::string& name) {
            static const std::map<uint32_t, const char*> names = { { 9001, "Mortal Strike" }, { 9007, "Mortal Strike X" },
                { 9002, "Charge" }, { 9003, "Execute" }, { 9004, "Bash" }, { 9005, "Zeal" } };
            auto it = names.find(spell);

            if (it == names.end()) {
                return false;
            }

            name = it->second;
            return true;
        };
        d.spellDescription = [](uint32_t spell) { return std::string(spell == 9002 ? "Rush to an ENEMY." : ""); };
        Mode(0x40);
        return CoATest::NewState(false);
    }

    void Stop(lua_State* L) {
        lua_close(L);
        CoATest::StopRecordingPackets();
        CoATest::StopRecordingSignals();
        CoA::SkillCard::Reset();
        CoA::SkillCardSlots::Reset();
        CoA::CustomStore::Reset();
        CoA::Wildcard::Reset();
        CoA::GameMode::Reset();
        CoA::RealmInfo::Reset();
        CoA::Lifecycle::Reset();
        CoA::Packets::Clear();
        Unload();
        CoA::Dbc::SetFileReader(nullptr);
        CoA::LuaReg::Reset();
    }

    V Run(lua_State* L, const std::string& chunk) {
        V out;
        const int base = lua_gettop(L);

        if (luaL_loadstring(L, chunk.c_str()) != 0 || lua_pcall(L, 0, LUA_MULTRET, 0) != 0) {
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

    Packets Sent(lua_State* L, const std::string& chunk, V* results = nullptr) {
        CoATest::RecordPackets();
        V r = Run(L, chunk);
        CoATest::StopRecordingPackets();

        if (results) {
            *results = r;
        }

        return CoATest::SentPackets();
    }

    // The collected list: bonus, 24 purchase counts, {card, progress, rank}.
    CoATest::Bytes List(uint32_t bonus, std::vector<std::vector<uint32_t>> collected, uint32_t purchased = 0) {
        CoATest::Bytes b;
        b.U32(bonus);

        for (int i = 0; i < 24; ++i) {
            b.U32(purchased);
        }

        b.U32(static_cast<uint32_t>(collected.size()));

        for (const auto& c : collected) {
            b.U32(c[0]).U32(c[1]).U32(c[2]);
        }

        return b;
    }

    V Collect(uint32_t card, uint32_t rank, uint8_t reveal = 0) {
        return Dispatch(0x649, CoATest::Bytes().U32(1).U32(card).U32(rank).U8(reveal));
    }

    // The type's filtered card ids in order, after SetSkillCardFilter(type, text, filters).
    std::string Filter(lua_State* L, const std::string& type, const std::string& text, const std::string& filters) {
        Run(L, "C_SkillCardCollection.SetSkillCardFilter('" + type + "', '" + text + "', {" + filters + "})");
        const V r = Run(L, "local s = '' for i = 1, C_SkillCardCollection.GetNumSkillCards('" + type + "') do "
            "local c = C_SkillCardCollection.GetSkillCardAtIndex('" + type + "', i) s = s .. (i > 1 and ',' or '') .. c.CardID .. '/' .. c.Rank end return s");
        return r.empty() ? "<none>" : r[0];
    }

    const char* kDump =
        "local function dump(t) local keys = {} for k in pairs(t) do local i = #keys + 1 keys[i] = k "
        "while i > 1 and tostring(keys[i - 1]) > tostring(keys[i]) do keys[i - 1], keys[i] = keys[i], keys[i - 1] i = i - 1 end end "
        "local out = '' for i, k in ipairs(keys) do local v = t[k] if i > 1 then out = out .. ' ' end "
        "out = out .. tostring(k) .. '=' .. (type(v) == 'table' and ('{' .. dump(v) .. '}') or tostring(v)) end "
        "return out end ";

    const std::string kNormal = "SKILL_CARD_DEFAULT_NORMAL";
}

// ---- the collection -----------------------------------------------------------------------------------------------

// SMSG 0x648 replaces the collection: progress (rounded hundredths of the quality's requirement, 1 at max rank),
// IsCollected (rank at least, 1 by default), HasAnySkillCardsCollected (any / of a type), the bonus progress.
COA_TEST(SkillCard_ListAndQueries) {
    lua_State* L = Start();
    CHECK(Run(L, "return C_SkillCardCollection.HasAnySkillCardsCollected(), C_SkillCardCollection.GetProgress(100)") == (V{ "false", "0", "0" }));
    CHECK(Dispatch(0x648, List(7, { { 100, 3, 1 }, { 400, 0, 1 } })).empty());
    CHECK(Run(L, "return C_SkillCardCollection.GetProgress(100)") == (V{ "1", "0.6" }));
    CHECK(Run(L, "return C_SkillCardCollection.GetProgress(400)") == (V{ "1", "1" }));   // max rank 1
    CHECK(Run(L, "return C_SkillCardCollection.GetProgress(999), select('#', C_SkillCardCollection.GetProgress('x'))") == (V{ "0", "0" }));
    CHECK(Run(L, "return C_SkillCardCollection.IsCollected(100), C_SkillCardCollection.IsCollected(100, 2), "
                 "C_SkillCardCollection.IsCollected(100, 1), C_SkillCardCollection.IsCollected(200)") == (V{ "true", "false", "true", "false" }));
    CHECK(Run(L, "return select('#', C_SkillCardCollection.IsCollected('x'))") == V{ "0" });
    CHECK(Run(L, "return C_SkillCardCollection.HasAnySkillCardsCollected(), C_SkillCardCollection.HasAnySkillCardsCollected('SKILL_CARD_DEFAULT_GOLDEN'), "
                 "C_SkillCardCollection.HasAnySkillCardsCollected('SKILL_CARD_LUCKY_NORMAL'), C_SkillCardCollection.HasAnySkillCardsCollected('BOGUS')") ==
        (V{ "true", "true", "false", "false" }));
    CHECK(Run(L, "return C_SkillCardCollection.GetMaxRank(100), C_SkillCardCollection.GetMaxRank(200), C_SkillCardCollection.GetMaxRank(1)") ==
        (V{ "2", "1", "0" }));
    CHECK(Run(L, "return C_SkillCardCollection.GetBonusSealedCardPacksProgress()") == V{ "0.07" });
    CHECK(CoA::SkillCard::CollectedAtLeast(100, 1) && !CoA::SkillCard::CollectedAtLeast(100, 2) && CoA::SkillCard::CollectedRank(400) == 1);
    // A second list replaces the first.
    Dispatch(0x648, List(0, { { 200, 0, 1 } }));
    CHECK(Run(L, "return C_SkillCardCollection.IsCollected(100), C_SkillCardCollection.IsCollected(200)") == (V{ "false", "true" }));
    // The 7a guard: a count past the bytes left reads the records that are there.
    CoATest::Bytes b = List(0, { { 300, 0, 1 } });
    b.data[4 + 24 * 4] = 5;   // n = 5
    Dispatch(0x648, b);
    CHECK(Run(L, "return C_SkillCardCollection.IsCollected(300), C_SkillCardCollection.IsCollected(200)") == (V{ "true", "false" }));
    Stop(L);
}

// SMSG 0x649 (FUN_10317ef0): a new card is added, a higher rank upgrades, a duplicate's progress (COMMON grants 1,
// needs 5) upgrades through the ranks, at max rank it feeds the bonus progress (100 -> a bonus pack). The auto-reveal
// event comes first, SKILL_CARD_COLLECTED last; an unknown card is skipped.
COA_TEST(SkillCard_Collected) {
    lua_State* L = Start();
    Dispatch(0x648, List(7, { { 100, 3, 1 } }));
    CHECK(Collect(100, 1, 1) == (V{ "SKILL_CARD_AUTO_REVEALED:100,1", "SKILL_CARD_PROGRESS_UPDATED:100,1", "SKILL_CARD_COLLECTED:100,1" }));
    CHECK(Run(L, "return C_SkillCardCollection.GetProgress(100)") == (V{ "1", "0.8" }));
    CHECK(Collect(100, 1) == (V{ "SKILL_CARD_UPGRADED:100,2", "SKILL_CARD_COLLECTED:100,1" }));
    CHECK(Run(L, "return C_SkillCardCollection.GetProgress(100)") == (V{ "2", "1" }));
    CHECK(Collect(100, 1) == (V{ "BONUS_SEALED_CARD_PACK_PROGRESS_UPDATE:100,1,7,8", "SKILL_CARD_COLLECTED:100,1" }));
    CHECK(Run(L, "return C_SkillCardCollection.GetBonusSealedCardPacksProgress()") == V{ "0.08" });
    CHECK(Collect(200, 1) == (V{ "SKILL_CARD_ADDED:200,1", "SKILL_CARD_COLLECTED:200,1" }));
    CHECK(Run(L, "return C_SkillCardCollection.IsCollected(200), C_SkillCardCollection.GetProgress(200)") == (V{ "true", "1", "1" }));
    CHECK(Collect(999, 1).empty());
    CHECK(Collect(100, 3).empty());   // no such rank
    // A higher rank than held: upgraded, progress 0.
    Dispatch(0x648, List(0, { { 100, 4, 1 } }));
    CHECK(Collect(100, 2) == (V{ "SKILL_CARD_UPGRADED:100,2", "SKILL_CARD_COLLECTED:100,2" }));
    // The bonus pack: 99 + 1 reaches the required 100.
    Dispatch(0x648, List(99, { { 100, 0, 2 } }));
    CHECK(Collect(100, 1) == (V{ "BONUS_SEALED_CARD_PACK_REWARDED", "BONUS_SEALED_CARD_PACK_PROGRESS_UPDATE:100,1,99,0", "SKILL_CARD_COLLECTED:100,1" }));
    // A legendary duplicate at its only rank grants 10.
    Dispatch(0x648, List(5, { { 300, 0, 1 } }));
    CHECK(Collect(300, 1) == (V{ "BONUS_SEALED_CARD_PACK_PROGRESS_UPDATE:300,1,5,15", "SKILL_CARD_COLLECTED:300,1" }));
    // The 7a guard: a count past the bytes left stops.
    CHECK(Dispatch(0x649, CoATest::Bytes().U32(3).U32(400).U32(1).U8(0)) == (V{ "SKILL_CARD_ADDED:400,1", "SKILL_CARD_COLLECTED:400,1" }));
    Stop(L);
}

// Pending cards: SMSG 0x653 replaces, 0x654 adds (event with the new count), 0x655 removes by name (event with the
// 1-based index) and stops at the first unknown name; CMSG 0x656 claims the named ones in the table's order.
COA_TEST(SkillCard_Pending) {
    lua_State* L = Start();
    Dispatch(0x653, CoATest::Bytes().U32(2).Str("a").U32(1).U32(2).Str("b").U32(3).U32(4));
    CHECK(Run(L, "return C_SkillCardCollection.GetNumPendingSkillCards()") == V{ "2" });
    CHECK(Run(L, "return C_SkillCardCollection.GetPendingSkillCardAtIndex(2)") == (V{ "b", "3", "4" }));
    CHECK(Run(L, "return C_SkillCardCollection.GetPendingSkillCardAtIndex(3)") == (V{ "nil", "nil" }));
    CHECK(Run(L, "return select('#', C_SkillCardCollection.GetPendingSkillCardAtIndex(0))") == V{ "0" });
    CHECK(Dispatch(0x654, CoATest::Bytes().U32(1).Str("c").U32(5).U32(6)) == V{ "PENDING_SKILL_CARD_ADDED:3,5,6" });
    CHECK(Dispatch(0x655, CoATest::Bytes().U32(3).Str("b").Str("zz").Str("a")) == V{ "PENDING_SKILL_CARD_REMOVED:2,3,4" });
    CHECK(Run(L, "return C_SkillCardCollection.GetNumPendingSkillCards(), C_SkillCardCollection.GetPendingSkillCardAtIndex(2)") == (V{ "2", "c", "5", "6" }));
    V r;
    CHECK(Sent(L, "return C_SkillCardCollection.ClaimPendingSkillCard({'c', 'x', 'a'})", &r) ==
        Packets{ CoATest::Expected(0x656, CoATest::Bytes().U32(2).Str("c").U32(5).U32(6).Str("a").U32(1).U32(2)) });
    CHECK(r == V{ "true" });
    // lua_toarray: a value that is not a string or number empties the list.
    CHECK(Sent(L, "return C_SkillCardCollection.ClaimPendingSkillCard({'c', {}})") == Packets{ CoATest::Expected(0x656, CoATest::Bytes().U32(0)) });
    CHECK(Sent(L, "return C_SkillCardCollection.ClaimPendingSkillCard('c')", &r).empty());
    CHECK(r.empty());
    CHECK(Run(L, "return C_SkillCardCollection.CanClaimPendingSkillCardAtIndex(9)") == (V{ "true", "nil" }));
    // ClaimAllPendingSkillCards only on a development realm: every pending card, 150 a packet.
    CHECK(Sent(L, "C_SkillCardCollection.ClaimAllPendingSkillCards()").empty());
    Realm(true);
    CHECK(Sent(L, "C_SkillCardCollection.ClaimAllPendingSkillCards()") ==
        Packets{ CoATest::Expected(0x656, CoATest::Bytes().U32(2).Str("a").U32(1).U32(2).Str("c").U32(5).U32(6)) });
    CoATest::Bytes many;
    many.U32(151);

    for (int i = 0; i < 151; ++i) {
        many.Str("p").U32(i).U32(0);
    }

    Dispatch(0x653, many);
    const Packets sent = Sent(L, "C_SkillCardCollection.ClaimAllPendingSkillCards()");
    CHECK(sent.size() == 2 && sent[1] == CoATest::Expected(0x656, CoATest::Bytes().U32(1).Str("p").U32(150).U32(0)));
    // The 7a guard on the lists.
    Dispatch(0x653, CoATest::Bytes().U32(4).Str("q").U32(1).U32(1));
    CHECK(Run(L, "return C_SkillCardCollection.GetNumPendingSkillCards()") == V{ "1" });
    Stop(L);
}

// The results: 0x657 / 0x6DE signal their string; 0x66B adds the purchase in flight to its type's count on OK only.
COA_TEST(SkillCard_Results) {
    lua_State* L = Start();
    CHECK(Dispatch(0x657, CoATest::Bytes().Str("CLAIM_X")) == V{ "CLAIM_SKILL_CARD_RESULT:CLAIM_X" });
    CHECK(Dispatch(0x6DE, CoATest::Bytes().Str("PURCHASE_SEALED_CARD_BOOSTER_PACK_OK")) ==
        V{ "PURCHASE_SEALED_CARD_BOOSTER_PACK_RESULT:PURCHASE_SEALED_CARD_BOOSTER_PACK_OK" });
    const std::string type = "'PURCHASE_SEALED_CARD_TYPE_NORMAL_COMMON_ABILITIES'";
    CHECK(Run(L, "return C_SkillCardCollection.GetSealedCardCost(" + type + ", 2)") == (V{ "246190", "10" }));
    V r;
    CHECK(Sent(L, "return C_SkillCardCollection.PurchaseSealedCard(" + type + ", 1.9)", &r) ==
        Packets{ CoATest::Expected(0x66A, CoATest::Bytes().Str("PURCHASE_SEALED_CARD_TYPE_NORMAL_COMMON_ABILITIES").U32(1)) });
    CHECK(r == V{ "true" });
    CHECK(Dispatch(0x66B, CoATest::Bytes().Str("PURCHASE_SEALED_CARD_NO_TOKEN")) == V{ "PURCHASE_SEALED_CARD_RESULT:PURCHASE_SEALED_CARD_NO_TOKEN" });
    CHECK(Run(L, "return C_SkillCardCollection.GetSealedCardCost(" + type + ", 1)") == (V{ "246190", "5" }));
    CHECK(Dispatch(0x66B, CoATest::Bytes().Str("BOGUS")) == V{ "PURCHASE_SEALED_CARD_RESULT:BOGUS" });
    Dispatch(0x66B, CoATest::Bytes().Str("PURCHASE_SEALED_CARD_OK"));
    CHECK(Run(L, "return C_SkillCardCollection.GetSealedCardCost(" + type + ", 1)") == (V{ "246190", "5" }));   // row 2
    Dispatch(0x66B, CoATest::Bytes().Str("PURCHASE_SEALED_CARD_OK"));
    CHECK(Run(L, "return C_SkillCardCollection.GetSealedCardCost(" + type + ", 1)") == (V{ "246190", "0" }));   // row 3 has none
    // SMSG 0x66C patches a whole SealedCardCosts row.
    CoATest::Bytes patch;

    for (uint32_t v : CostRow(3, { { 4, 9 } })) {
        patch.U32(v);
    }

    Dispatch(0x66C, patch);
    CHECK(Run(L, "return C_SkillCardCollection.GetSealedCardCost(" + type + ", 1)") == (V{ "246190", "9" }));
    // An unknown name is type 0x18 (no token, no cost); PurchaseSealedCard refuses it.
    CHECK(Run(L, "return C_SkillCardCollection.GetSealedCardCost('X', 1)") == (V{ "0", "0" }));
    CHECK(Sent(L, "return C_SkillCardCollection.PurchaseSealedCard('X', 1)", &r).empty() && r.empty());
    CHECK(Run(L, "return C_SkillCardCollection.GetSealedCardCost('PURCHASE_SEALED_CARD_TYPE_DEFAULT_GOLDEN_PACK', 1), "
                 "C_SkillCardCollection.GetSealedCardCost('PURCHASE_SEALED_CARD_TYPE_GOLDEN_RARE_TALENTS', 0)") == (V{ "778998", "97399", "0" }));
    Stop(L);
}

// CanPurchaseSealedCard (FUN_103177c0): every PURCHASE_SEALED_CARD_* reason in order.
COA_TEST(SkillCard_CanPurchase) {
    lua_State* L = Start();
    const std::string d = kDump;
    const std::string common = "'PURCHASE_SEALED_CARD_TYPE_NORMAL_COMMON_ABILITIES'";
    // No tokens.
    CHECK(Run(L, d + "local ok, r = C_SkillCardCollection.CanPurchaseSealedCard(" + common + ", 1) return ok, dump(r)") ==
        (V{ "false", "1=PURCHASE_SEALED_CARD_NO_TOKEN" }));
    s_held[0x3C1AE] = 5;
    CHECK(Run(L, "return C_SkillCardCollection.CanPurchaseSealedCard(" + common + ", 1)") == (V{ "true", "nil" }));
    // Two at once: too many for a single card type, and two rows' cost.
    CHECK(Run(L, d + "local ok, r = C_SkillCardCollection.CanPurchaseSealedCard(" + common + ", 2) return ok, dump(r)") ==
        (V{ "false", "1=PURCHASE_SEALED_CARD_NO_TOKEN 2=PURCHASE_SEALED_CARD_BAD_AMOUNT" }));
    CHECK(Run(L, d + "local ok, r = C_SkillCardCollection.CanPurchaseSealedCard(" + common + ", 0) return ok, dump(r)") ==
        (V{ "false", "1=PURCHASE_SEALED_CARD_BAD_COST 2=PURCHASE_SEALED_CARD_BAD_AMOUNT" }));
    // Every common normal ability collected (card 100 rank 1): none left of the quality. Card 600 is lucky, not default.
    Dispatch(0x648, List(0, { { 100, 0, 2 } }));
    CHECK(Run(L, d + "local ok, r = C_SkillCardCollection.CanPurchaseSealedCard(" + common + ", 1) return ok, dump(r)") ==
        (V{ "false", "1=PURCHASE_SEALED_CARD_NO_UNCOLLECTED_CARDS_OF_QUALITY" }));
    Dispatch(0x648, List(0, {}));
    // Legendary: card 300 is CLASS_NONE, the player a warrior; no cost row entry for type 0 rows 1/2 is 10 / 20.
    s_held[0x3C1AE] = 100;
    CHECK(Run(L, d + "local ok, r = C_SkillCardCollection.CanPurchaseSealedCard('PURCHASE_SEALED_CARD_TYPE_NORMAL_LEGENDARY_ABILITIES', 1) return ok, dump(r)") ==
        (V{ "false", "1=PURCHASE_SEALED_CARD_NO_UNCOLLECTED_CARDS_OF_QUALITY" }));
    s_class = 0;
    CHECK(Run(L, "return C_SkillCardCollection.CanPurchaseSealedCard('PURCHASE_SEALED_CARD_TYPE_NORMAL_LEGENDARY_ABILITIES', 1)") == (V{ "true", "nil" }));
    Realm(false, 0);   // the classic ruleset: a TBC card is not for this expansion
    CHECK(Run(L, d + "local ok, r = C_SkillCardCollection.CanPurchaseSealedCard('PURCHASE_SEALED_CARD_TYPE_NORMAL_LEGENDARY_ABILITIES', 1) return ok, dump(r)") ==
        (V{ "false", "1=PURCHASE_SEALED_CARD_NO_UNCOLLECTED_CARDS_OF_QUALITY" }));
    Realm(false);
    s_class = 1;
    // No cost entry for the next purchase (type 1 has no costs in any row).
    CHECK(Run(L, d + "local ok, r = C_SkillCardCollection.CanPurchaseSealedCard('PURCHASE_SEALED_CARD_TYPE_NORMAL_EPIC_ABILITIES', 1) return ok, dump(r)") ==
        (V{ "false", "1=PURCHASE_SEALED_CARD_BAD_COST 2=PURCHASE_SEALED_CARD_NO_UNCOLLECTED_CARDS_OF_QUALITY" }));
    // Pending cards, an unknown type, the mode off.
    Dispatch(0x653, CoATest::Bytes().U32(1).Str("a").U32(1).U32(2));
    CHECK(Run(L, d + "local ok, r = C_SkillCardCollection.CanPurchaseSealedCard(" + common + ", 1) return ok, dump(r)") ==
        (V{ "false", "1=PURCHASE_SEALED_CARD_UNCLAIMED_CARDS" }));
    Dispatch(0x653, CoATest::Bytes().U32(0));
    CHECK(Run(L, d + "local ok, r = C_SkillCardCollection.CanPurchaseSealedCard('NOPE', 1) return ok, dump(r)") ==
        (V{ "false", "1=PURCHASE_SEALED_CARD_BAD_TYPE" }));
    Mode(0);
    CHECK(Run(L, d + "local ok, r = C_SkillCardCollection.CanPurchaseSealedCard(" + common + ", 1) return ok, dump(r)") ==
        (V{ "false", "1=PURCHASE_SEALED_CARD_RANDOM_MODE_NOT_ENABLED 2=PURCHASE_SEALED_CARD_NO_UNCOLLECTED_CARDS_OF_QUALITY" }));
    Mode(8);   // draft mode: card 500 is the draft epic
    CHECK(Run(L, "return C_SkillCardCollection.CanPurchaseSealedCard(" + common + ", 1)") == (V{ "false", "table" }));
    CHECK(Run(L, "return select('#', C_SkillCardCollection.CanPurchaseSealedCard(1))") == V{ "0" });
    Stop(L);
}

// Packs (types 20..23): up to 200 at once; PurchaseAllSealedCards (development realms) asks for as many as the tokens buy.
COA_TEST(SkillCard_PurchaseAll) {
    lua_State* L = Start();
    const std::string pack = "PURCHASE_SEALED_CARD_TYPE_DEFAULT_PACK";
    s_held[0x17E5D] = 115;
    CHECK(Sent(L, "C_SkillCardCollection.PurchaseAllSealedCards('" + pack + "')").empty());
    Realm(true);
    CHECK(Sent(L, "C_SkillCardCollection.PurchaseAllSealedCards('" + pack + "')") ==
        Packets{ CoATest::Expected(0x66A, CoATest::Bytes().Str(pack.c_str()).U32(2)) });
    Dispatch(0x66B, CoATest::Bytes().Str("PURCHASE_SEALED_CARD_OK"));   // purchased 2: row 3 next (70)
    CHECK(Run(L, "return C_SkillCardCollection.GetSealedCardCost('" + pack + "', 1)") == (V{ "97885", "70" }));
    s_held[0x17E5D] = 10;
    CHECK(Sent(L, "C_SkillCardCollection.PurchaseAllSealedCards('" + pack + "')").empty());
    Stop(L);
}

// Booster packs: 500000 copper each, 1..0x10C6 at once; the money check only with a player and a count.
COA_TEST(SkillCard_BoosterPacks) {
    lua_State* L = Start();
    const std::string d = kDump;
    CHECK(Run(L, d + "local ok, r = C_SkillCardCollection.CanPurchaseSealedCardBoosterPack(0) return ok, dump(r)") ==
        (V{ "false", "1=PURCHASE_SEALED_CARD_BOOSTER_PACK_TOO_FEW" }));
    s_money = 999999;
    CHECK(Run(L, d + "local ok, r = C_SkillCardCollection.CanPurchaseSealedCardBoosterPack(2) return ok, dump(r)") ==
        (V{ "false", "1=PURCHASE_SEALED_CARD_BOOSTER_PACK_NO_MONEY" }));
    s_money = 1000000;
    CHECK(Run(L, "return C_SkillCardCollection.CanPurchaseSealedCardBoosterPack(2)") == (V{ "true", "nil" }));
    CHECK(Run(L, d + "local ok, r = C_SkillCardCollection.CanPurchaseSealedCardBoosterPack(4295) return ok, dump(r)") ==
        (V{ "false", "1=PURCHASE_SEALED_CARD_BOOSTER_PACK_TOO_MANY 2=PURCHASE_SEALED_CARD_BOOSTER_PACK_NO_MONEY" }));
    s_player = false;
    CHECK(Run(L, "return C_SkillCardCollection.CanPurchaseSealedCardBoosterPack(3)") == (V{ "true", "nil" }));
    s_player = true;
    Mode(0);
    CHECK(Run(L, d + "local ok, r = C_SkillCardCollection.CanPurchaseSealedCardBoosterPack(1) return ok, dump(r)") ==
        (V{ "false", "1=PURCHASE_SEALED_CARD_BOOSTER_PACK_RANDOM_MODE_NOT_ENABLED" }));
    Mode(0x40);
    CHECK(Run(L, "return C_SkillCardCollection.GetSealedCardBoosterPackCost(0), C_SkillCardCollection.GetSealedCardBoosterPackCost(3), "
                 "C_SkillCardCollection.GetSealedCardBoosterPackCost(4295)") == (V{ "0", "1500000", "4294967295" }));
    s_money = 1250000;
    CHECK(Run(L, "return C_SkillCardCollection.GetMaxNumPurchasableSealedCardBoosterPacks()") == V{ "2" });
    s_player = false;
    CHECK(Run(L, "return C_SkillCardCollection.GetMaxNumPurchasableSealedCardBoosterPacks()") == V{ "4294" });
    V r;
    CHECK(Sent(L, "return C_SkillCardCollection.PurchaseSealedCardBoosterPack(4)", &r) == Packets{ CoATest::Expected(0x6DD, CoATest::Bytes().U32(4)) });
    CHECK(r == V{ "true" });
    CHECK(Sent(L, "return C_SkillCardCollection.PurchaseSealedCardBoosterPack()", &r).empty() && r.empty());
    Stop(L);
}

// SetSkillCardFilter (FUN_1031dc30): the type's own filter and FILTER_ACTIVE_CLASS always; the mode's cards only; populate
// order by the lowercase spell name as a big integer (length, then bytes); text over the name, then the description.
COA_TEST(SkillCard_Filter) {
    lua_State* L = Start();
    CHECK(Run(L, "return C_SkillCardCollection.GetNumSkillCards('" + kNormal + "')") == V{ "0" });   // nothing before a filter
    CHECK(Filter(L, kNormal, "", "") == "200/1,100/1,100/2");   // 300: CLASS_NONE, 500: draft only
    CHECK(Filter(L, kNormal, "CHARGE", "") == "200/1");
    CHECK(Filter(L, kNormal, "enemy", "") == "200/1");   // the description
    CHECK(Filter(L, kNormal, "x", "") == "100/2");
    // Quality words become filters and leave the text ("uncommon" first: it hides "common").
    CHECK(Filter(L, kNormal, "rare", "") == "200/1");
    CHECK(Filter(L, kNormal, "  strike  common  ", "") == "100/1,100/2");
    CHECK(Filter(L, kNormal, "uncommon", "") == "");
    CHECK(Filter(L, kNormal, "", "'FILTER_RARE', 'FILTER_COMMON'") == "200/1,100/1,100/2");   // one group: any
    // FILTER_COLLECTED / NOT_COLLECTED (group 0: each must match).
    Dispatch(0x648, List(0, { { 100, 0, 1 } }));
    CHECK(Filter(L, kNormal, "", "'FILTER_COLLECTED'") == "100/1");
    // The engine's group 0 (FUN_100fca40's): its first matching argument ends the group, so FILTER_ACTIVE_CLASS (added
    // after the table's names) is not asked once FILTER_NOT_COLLECTED matched -- card 300 (CLASS_NONE) passes.
    CHECK(Filter(L, kNormal, "", "'FILTER_NOT_COLLECTED'") == "200/1,300/1,100/2");
    CHECK(Filter(L, kNormal, "", "'FILTER_COLLECTED', 'FILTER_NOT_COLLECTED'") == "100/1");
    // An unknown name is FILTER_NONE, which matches nothing; a value that is not a string empties the list.
    CHECK(Filter(L, kNormal, "", "'FILTER_BOGUS'") == "");
    CHECK(Filter(L, kNormal, "", "'FILTER_COLLECTED', {}") == "200/1,100/1,100/2");
    CHECK(Filter(L, kNormal, "", "5") == "");   // the number 5 is the string "5": FILTER_NONE
    // The golden type keeps its starter card; the player's class decides FILTER_ACTIVE_CLASS.
    CHECK(Filter(L, "SKILL_CARD_DEFAULT_GOLDEN", "", "'FILTER_STARTER'") == "400/1");
    s_class = 2;
    CHECK(Filter(L, kNormal, "", "") == "");
    s_class = 0;
    CHECK(Filter(L, kNormal, "", "") == "300/1");   // CLASS_NONE is class 0 (the expansion is not checked here)
    s_class = 1;
    s_player = false;
    CHECK(Filter(L, kNormal, "", "") == "");
    s_player = true;
    CHECK(Filter(L, kNormal, "", "") == "200/1,100/1,100/2");
    // Draft mode: the draft card instead.
    Mode(8);
    CHECK(Run(L, "return C_SkillCardCollection.GetNumSkillCards('" + kNormal + "')") == V{ "0" });   // reset by the mode change
    CHECK(Filter(L, kNormal, "", "") == "500/1");
    // A card without a spell sorts first and never matches text.
    Mode(0x40);
    CHECK(Filter(L, "SKILL_CARD_LUCKY_NORMAL", "", "") == "600/1");
    CHECK(Filter(L, "SKILL_CARD_LUCKY_NORMAL", "a", "") == "");
    // Starter (and MAX) types share FUN_10319f80's function-local container, a SkillCardEntryContainer of its own: no type
    // filter, so every card of the mode and class.
    CHECK(Filter(L, "SKILL_CARD_STARTER_NORMAL", "", "") == "600/1,400/1,200/1,100/1,100/2");
    CHECK(Run(L, "return C_SkillCardCollection.GetNumSkillCards('SKILL_CARD_MAX')") == V{ "5" });
    CHECK(Run(L, "return select('#', C_SkillCardCollection.SetSkillCardFilter('" + kNormal + "', 'x'))") == V{ "0" });
    CHECK(Run(L, "return select('#', C_SkillCardCollection.GetNumSkillCards('BOGUS')), select('#', C_SkillCardCollection.GetSkillCardAtIndex('" + kNormal + "', 0))") ==
        (V{ "0", "0" }));
    CHECK(Run(L, "return C_SkillCardCollection.GetSkillCardAtIndex('SKILL_CARD_LUCKY_NORMAL', 2)") == V{ "nil" });
    Stop(L);
}

// The card table (FUN_1009bc60).
COA_TEST(SkillCard_CardTable) {
    lua_State* L = Start();
    Dispatch(0x648, List(0, { { 100, 2, 1 } }));
    Filter(L, kNormal, "", "");
    CHECK(Run(L, std::string(kDump) + "return dump(C_SkillCardCollection.GetSkillCardAtIndex('" + kNormal + "', 2))") ==
        V{ "CardID=100 Class=CLASS_WARRIOR CollectedProgress=0.4 CollectedRank=1 Expansion=EXPANSION_CLASSIC IsCollected=true IsDraftMode=false "
           "IsStarterCard=false IsWildcard=true ItemID=5001 MaxRank=2 Quality=SKILL_CARD_COMMON Rank=1 SpellID=9001 Type=SKILL_CARD_DEFAULT_NORMAL" });
    Stop(L);
}

// SMSG 0x647 patches a card row (an existing id in place, a new id added) and empties the containers; the glue reset
// clears the manager.
COA_TEST(SkillCard_PatchAndGlue) {
    lua_State* L = Start();
    CHECK(Filter(L, kNormal, "", "") == "200/1,100/1,100/2");
    CoATest::Bytes b;
    b.U32(8).U32(700).U32(1).U32(5008).U32(9004).U32(0).U32(0).U32(0).U32(0).U32(1).U32(0).U32(0);
    b.Str(kNormal.c_str()).Str("EXPANSION_CLASSIC").Str("CLASS_WARRIOR").Str("SKILL_CARD_EPIC");
    CHECK(Dispatch(0x647, b).empty());
    CHECK(Run(L, "return C_SkillCardCollection.GetNumSkillCards('" + kNormal + "')") == V{ "0" });
    CHECK(Filter(L, kNormal, "", "") == "700/1,200/1,100/1,100/2");   // "Bash" sorts first
    CoATest::Bytes p;
    p.U32(3).U32(200).U32(1).U32(5003).U32(9002).U32(0).U32(0).U32(0).U32(0).U32(1).U32(0).U32(0);
    p.Str(kNormal.c_str()).Str("EXPANSION_CLASSIC").Str("CLASS_WARRIOR").Str("SKILL_CARD_COMMON");
    Dispatch(0x647, p);
    CHECK(Run(L, "return C_SkillCard.GetSkillCardQuality(200)") == V{ "SKILL_CARD_COMMON" });
    CHECK(Run(L, "return C_SkillCard.GetCardID(5008)") == (V{ "700", "1" }));
    // The glue reset.
    Dispatch(0x648, List(30, { { 100, 2, 1 } }, 1));
    Dispatch(0x653, CoATest::Bytes().U32(1).Str("a").U32(1).U32(2));
    Filter(L, kNormal, "", "");
    CoA::Lifecycle::RunGlueScreen();
    CHECK(Run(L, "return C_SkillCardCollection.HasAnySkillCardsCollected(), C_SkillCardCollection.GetNumPendingSkillCards(), "
                 "C_SkillCardCollection.GetBonusSealedCardPacksProgress(), C_SkillCardCollection.GetNumSkillCards('" + kNormal + "'), "
                 "C_SkillCardCollection.GetSealedCardCost('PURCHASE_SEALED_CARD_TYPE_NORMAL_COMMON_ABILITIES', 1)") ==
        (V{ "false", "0", "0", "0", "246190", "5" }));
    Stop(L);
}

// ---- C_SkillCard ------------------------------------------------------------------------------------------------------

namespace {
    // SMSG 0x624: one spec; type 0 three slots, type 2 one starter slot.
    void Slots() {
        CoATest::Bytes b;
        b.U32(1);

        for (uint32_t t = 0; t < 9; ++t) {
            if (t == 0) {
                b.U32(3).U32(200).U32(1).U8(0).U32(100).U32(2).U8(1).U32(500).U32(1).U8(0);
            } else if (t == 2) {
                b.U32(1).U32(400).U32(1).U8(0);
            } else {
                b.U32(0);
            }
        }

        Dispatch(0x624, b);
    }
}

// The slot readers over SMSG 0x624 for the active spec: card / rank, the server's block flag, active (slotted and not
// blocked: starter-ness against the build's starting phase, the server's flag, default slots 2 / 3 below level 25 / 40).
COA_TEST(SkillCard_SlotReaders) {
    lua_State* L = Start();
    Slots();
    CHECK(Run(L, "return C_SkillCard.GetCardCount('" + kNormal + "'), C_SkillCard.GetCardCount('SKILL_CARD_STARTER_NORMAL'), "
                 "C_SkillCard.GetCardCount('SKILL_CARD_LUCKY_NORMAL')") == (V{ "3", "1", "0" }));
    CHECK(Run(L, "return C_SkillCard.GetCardAtIndex('" + kNormal + "', 2)") == (V{ "100", "2" }));
    CHECK(Run(L, "return C_SkillCard.GetCardAtIndex('" + kNormal + "', 4)") == (V{ "0", "0" }));
    CHECK(Run(L, "return C_SkillCard.GetCardRankAtIndex('" + kNormal + "', 2), C_SkillCard.IsCardAtIndexBlocked('" + kNormal + "', 2), "
                 "C_SkillCard.IsCardAtIndexBlocked('" + kNormal + "', 1)") == (V{ "2", "true", "false" }));
    CHECK(Run(L, "return C_SkillCard.IsCardAtIndexActive('" + kNormal + "', 1), C_SkillCard.IsCardAtIndexActive('" + kNormal + "', 2), "
                 "C_SkillCard.IsCardAtIndexActive('" + kNormal + "', 3)") == (V{ "true", "false", "false" }));   // slot 3 needs level 40
    s_level = 40;
    CHECK(Run(L, "return C_SkillCard.IsCardAtIndexActive('" + kNormal + "', 3)") == V{ "true" });
    s_player = false;
    CHECK(Run(L, "return C_SkillCard.IsCardAtIndexActive('" + kNormal + "', 1), C_SkillCard.IsCardAtIndexActive('" + kNormal + "', 3)") ==
        (V{ "true", "false" }));
    s_player = true;
    // The starter slot is blocked outside the starting phase (a wildcard build below the starting ability count).
    CHECK(Run(L, "return C_SkillCard.IsCardAtIndexActive('SKILL_CARD_STARTER_NORMAL', 1)") == V{ "false" });
    CoA::CA::Build* build = CoA::CA::ActiveBuild();
    build->wildcard = 1;
    build->u44 = 3;
    CHECK(Run(L, "return C_SkillCard.IsCardAtIndexActive('SKILL_CARD_STARTER_NORMAL', 1), C_SkillCard.IsCardAtIndexActive('" + kNormal + "', 1)") ==
        (V{ "true", "false" }));
    build->u44 = 4;
    CHECK(Run(L, "return C_SkillCard.IsCardAtIndexActive('SKILL_CARD_STARTER_NORMAL', 1)") == V{ "false" });
    build->wildcard = 0;
    build->u44 = 0;
    CHECK(Run(L, std::string(kDump) + "return dump(C_SkillCard.GetSkillCardInfoAtIndex('" + kNormal + "', 1))").size() == 1);
    CHECK(Run(L, "return C_SkillCard.GetSkillCardInfoAtIndex('" + kNormal + "', 1).CardID, C_SkillCard.GetSkillCardInfoAtIndex('" + kNormal + "', 9)") ==
        (V{ "200", "nil" }));
    CHECK(Run(L, "return select('#', C_SkillCard.GetCardAtIndex('" + kNormal + "', 0)), select('#', C_SkillCard.GetCardAtIndex('BOGUS', 1)), "
                 "select('#', C_SkillCard.GetCardCount(1))") == (V{ "0", "0", "0" }));
    // Another spec has no slots.
    s_spec = 3;
    CHECK(Run(L, "return C_SkillCard.GetCardCount('" + kNormal + "'), C_SkillCard.GetCardAtIndex('" + kNormal + "', 1)") == (V{ "0", "0", "0" }));
    s_spec = 0;
    // AnyUnblocked (Wildcard's skillCardUnblocked).
    CHECK(CoA::SkillCard::AnyUnblocked(0) && !CoA::SkillCard::AnyUnblocked(1));
    // The glue reset.
    CoA::Lifecycle::RunGlueScreen();
    CHECK(Run(L, "return C_SkillCard.GetCardCount('" + kNormal + "')") == V{ "0" });
    Stop(L);
}

// IsCardedSpellID / IsCardedID (FUN_102f6e00 / FUN_102f7300): an unblocked slot of the active spec holding the card ->
// true, its type, true; else false, SKILL_CARD_MAX, false. GetCardID / GetCardSpellID / GetSkillCardInfo / Quality.
COA_TEST(SkillCard_CardLookups) {
    lua_State* L = Start();
    Slots();
    CHECK(Run(L, "return C_SkillCard.IsCardedSpellID(9002)") == (V{ "true", kNormal, "true" }));
    CHECK(Run(L, "return C_SkillCard.IsCardedSpellID(9007)") == (V{ "false", "SKILL_CARD_MAX", "false" }));   // blocked slot
    CHECK(Run(L, "return C_SkillCard.IsCardedSpellID(9004)") == (V{ "false", "SKILL_CARD_MAX", "false" }));   // starter, blocked
    CHECK(Run(L, "return C_SkillCard.IsCardedID(1)") == (V{ "false", "SKILL_CARD_MAX", "false" }));   // no CharacterAdvancement row
    CHECK(Run(L, "return select('#', C_SkillCard.IsCardedSpellID('x'))") == V{ "0" });
    CHECK(Run(L, "return C_SkillCard.GetCardID(5003)") == (V{ "200", "1" }));
    CHECK(Run(L, "return C_SkillCard.GetCardID(1)") == (V{ "nil", "nil" }));
    CHECK(Run(L, "return C_SkillCard.GetCardSpellID(100, 2), C_SkillCard.GetCardSpellID(100, 9)") == (V{ "9007", "nil" }));
    CHECK(Run(L, "return select('#', C_SkillCard.GetCardSpellID(100))") == V{ "0" });
    CHECK(Run(L, "return C_SkillCard.GetSkillCardInfo(100).Rank, C_SkillCard.GetSkillCardInfo(100, 2).Rank") == (V{ "1", "2" }));
    CHECK(Run(L, "return select('#', C_SkillCard.GetSkillCardInfo(999))") == V{ "0" });
    CHECK(Run(L, "return C_SkillCard.GetSkillCardQuality(200), C_SkillCard.GetSkillCardQuality(999)") == (V{ "SKILL_CARD_RARE", "nil" }));
    Stop(L);
}

// SMSG 0x625 sets one slot (sizing the lists): SKILL_CARD_UPDATED when the card changes, SKILL_CARD_BLOCKED / _UNBLOCKED
// when the flag does; an unknown type is dropped. 0x623 signals its result.
COA_TEST(SkillCard_SlotUpdates) {
    lua_State* L = Start();
    Slots();
    auto update = [](uint32_t spec, const char* type, uint32_t slot, uint32_t card, uint32_t rank, uint8_t blocked) {
        return Dispatch(0x625, CoATest::Bytes().U32(spec).Str(type).U32(slot).U32(card).U32(rank).U8(blocked));
    };
    CHECK(update(0, kNormal.c_str(), 0, 600, 1, 1) ==
        (V{ "SKILL_CARD_UPDATED:1," + kNormal + ",1,200,1,600,1", "SKILL_CARD_BLOCKED:1," + kNormal + ",1" }));
    CHECK(update(0, kNormal.c_str(), 0, 600, 1, 0) == V{ "SKILL_CARD_UNBLOCKED:1," + kNormal + ",1" });
    CHECK(update(0, kNormal.c_str(), 0, 600, 1, 0).empty());
    CHECK(update(0, "BOGUS", 0, 1, 1, 1).empty());
    CHECK(update(2, "SKILL_CARD_LUCKY_NORMAL", 4, 300, 1, 0) == V{ "SKILL_CARD_UPDATED:3,SKILL_CARD_LUCKY_NORMAL,5,0,0,300,1" });
    s_spec = 2;
    CHECK(Run(L, "return C_SkillCard.GetCardCount('SKILL_CARD_LUCKY_NORMAL'), C_SkillCard.GetCardAtIndex('SKILL_CARD_LUCKY_NORMAL', 5)") ==
        (V{ "5", "300", "1" }));
    CHECK(Dispatch(0x623, CoATest::Bytes().Str("SET_SKILL_CARD_OK")) == V{ "SET_SKILL_CARD_RESULT:SET_SKILL_CARD_OK" });
    // A short 0x624: the slots whose 9 bytes are left.
    CoATest::Bytes b;
    b.U32(1).U32(5).U32(200).U32(1).U8(0);
    Dispatch(0x624, b);
    s_spec = 0;
    CHECK(Run(L, "return C_SkillCard.GetCardCount('" + kNormal + "'), C_SkillCard.GetCardCount('SKILL_CARD_DEFAULT_GOLDEN')") == (V{ "1", "0" }));
    Stop(L);
}

// The slot count per type (felforged: mode bit 7) and CMSG 0x622 {u32 spec, str type, u32 slot - 1, u32 card}.
COA_TEST(SkillCard_SlotCommands) {
    lua_State* L = Start();
    CHECK(Run(L, "return C_SkillCard.GetMaxCardCount('" + kNormal + "'), C_SkillCard.GetMaxCardCount('SKILL_CARD_STARTER_GOLDEN'), "
                 "C_SkillCard.GetMaxCardCount('SKILL_CARD_TALENT_NORMAL'), C_SkillCard.GetMaxCardCount('SKILL_CARD_MAX')") == (V{ "2", "2", "2", "0" }));
    Mode(0xC0);
    CHECK(Run(L, "return C_SkillCard.GetMaxCardCount('" + kNormal + "'), C_SkillCard.GetMaxCardCount('SKILL_CARD_LUCKY_GOLDEN'), "
                 "C_SkillCard.GetMaxCardCount('SKILL_CARD_TALENT_GOLDEN'), C_SkillCard.GetMaxCardCount('SKILL_CARD_STARTER_NORMAL')") == (V{ "3", "3", "3", "2" }));
    CHECK(Run(L, "return select('#', C_SkillCard.GetMaxCardCount('X'))") == V{ "0" });
    V r;
    s_spec = 2;
    CHECK(Sent(L, "return C_SkillCard.SetCardAtIndex('SKILL_CARD_LUCKY_NORMAL', 2, 300)", &r) ==
        Packets{ CoATest::Expected(0x622, CoATest::Bytes().U32(2).Str("SKILL_CARD_LUCKY_NORMAL").U32(1).U32(300)) });
    CHECK(r == V{ "true" });
    s_spec = -1;
    CHECK(Sent(L, "return C_SkillCard.RemoveCardAtIndex('ANY', 1)", &r) ==
        Packets{ CoATest::Expected(0x622, CoATest::Bytes().U32(0xFFFFFFFF).Str("ANY").U32(0).U32(0)) });
    CHECK(r == V{ "true" });
    CHECK(Sent(L, "return select('#', C_SkillCard.SetCardAtIndex('X', 0, 1)), select('#', C_SkillCard.RemoveCardAtIndex('X', 0)), "
                  "select('#', C_SkillCard.SetCardAtIndex('X', 1))", &r).empty());
    CHECK((r == V{ "0", "0", "0" }));
    Stop(L);
}

// GetHandOfFateQuests / GetScrollOfFortuneQuests: the quests of each spec's group (0x65 + spec / 0x191 + spec) with the
// leveling flag (a group in 0xC9.. / 0x1F5..).
COA_TEST(SkillCard_QuestGroups) {
    CoA::Dbc::SetFileReader(ReadTables);
    CoA::MemoryBridge::Reset();
    CoA::BridgeStore::Reset();
    CoA::BridgeStore::LoadTables();
    CoA::BridgeStore::BuildIndexes();
    lua_State* L = Start();
    const std::string d = kDump;
    CHECK(Run(L, d + "return dump(GetHandOfFateQuests())") ==
        V{ "1={ID=40 Leveling=false MinLevel=10 RequiredItemCount={1=4 2=0 3=0 4=0 5=0 6=0} RequiredItemId={1=42 2=0 3=0 4=0 5=0 6=0} "
           "RewardAmount={1=3 2=0 3=0 4=0} RewardItem={1=41 2=0 3=0 4=0} Specialization=0} "
           "2={ID=41 Leveling=true MinLevel=20 RequiredItemCount={1=4 2=0 3=0 4=0 5=0 6=0} RequiredItemId={1=43 2=0 3=0 4=0 5=0 6=0} "
           "RewardAmount={1=3 2=0 3=0 4=0} RewardItem={1=42 2=0 3=0 4=0} Specialization=1}" });
    CHECK(Run(L, "local t = GetScrollOfFortuneQuests() return #t, t[1].ID, t[1].Specialization, t[1].Leveling") ==
        (V{ "1", "42", "0", "false" }));
    Stop(L);
    CoA::BridgeStore::Reset();
    CoA::MemoryBridge::Reset();
}
