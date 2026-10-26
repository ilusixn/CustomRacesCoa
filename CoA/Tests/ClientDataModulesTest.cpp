// AscPOI, AscSpellCharges, AscAura137, AscAssetQuery and AscTemplates.
#include "CoA/Source/Modules/AssetQuery.h"
#include "CoA/Source/Modules/Aura137.h"
#include "CoA/Source/Modules/MysticEnchant.h"
#include "CoA/Source/Modules/POI.h"
#include "CoA/Source/Modules/SpellCharges.h"
#include "CoA/Source/Modules/Templates.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/ClientState.h"
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

namespace {
    using V = std::vector<std::string>;
    using Packets = std::vector<std::vector<uint8_t>>;

    void Deliver(uint32_t opcode, const CoATest::Bytes& b) {
        std::vector<uint8_t> data = b.data;
        CDataStore msg(data.data(), static_cast<uint32_t>(data.size()));
        int32_t result = 0;
        CHECK(CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result));
        CHECK(result == 1);
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

    void Begin() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
    }

    void End() {
        CoATest::StopRecordingPackets();
        CoATest::StopRecordingSignals();
        CoA::Lifecycle::Reset();
        CoA::Packets::Clear();
        CoA::LuaReg::Reset();
    }

    // ---- SpellCharges ----------------------------------------------------------------------------------------------
    uint32_t s_ms = 0;
    bool s_inWorld = true;

    bool ReadCharges(const char* path, std::vector<uint8_t>& out) {
        if (strstr(path, "SpellChargesCategory.dbc")) {
            // id, +4 max charges, +8 recharge ms
            out = CoATest::Wdbc({ { 10, 3, 8000 }, { 11, 0, 5000 }, { 12, 2, 1000 } }, std::string(1, '\0'));
            return true;
        }

        if (strstr(path, "SpellCharges.dbc")) {
            // id (spell), +4 category
            out = CoATest::Wdbc({ { 500, 10 }, { 501, 11 }, { 502, 99 }, { 503, 12 } }, std::string(1, '\0'));
            return true;
        }

        return false;
    }

    void ChargesStart() {
        Begin();
        CoA::ClientHooks::enterWorld.Clear();
        CoA::SpellCharges::Reset();
        CoA::SpellCharges::Init();
        CoA::Dbc::SetFileReader(ReadCharges);
        CoA::Dbc::Get("DBFilesClient\\SpellCharges.dbc").Unload();
        CoA::Dbc::Get("DBFilesClient\\SpellChargesCategory.dbc").Unload();
        s_ms = 100000;
        s_inWorld = true;
        CoA::SpellCharges::Dependencies().now = [] { return s_ms; };
        CoA::ClientState::SetInWorldProbe([] { return s_inWorld; });
    }

    void ChargesStop() {
        CoA::ClientState::SetInWorldProbe(nullptr);
        CoA::Dbc::Get("DBFilesClient\\SpellCharges.dbc").Unload();
        CoA::Dbc::Get("DBFilesClient\\SpellChargesCategory.dbc").Unload();
        CoA::Dbc::SetFileReader(nullptr);
        CoA::SpellCharges::Reset();
        CoA::ClientHooks::enterWorld.Clear();
        End();
    }

    // ---- AssetQuery -------------------------------------------------------------------------------------------------
    std::set<std::pair<int, uint32_t>> s_cached;
    int64_t s_now = 5000;

    void QueryStart() {
        Begin();
        CoA::AssetQuery::Reset();
        CoA::AssetQuery::Init();
        s_cached.clear();
        s_now = 5000;
        CoA::AssetQuery::Deps& d = CoA::AssetQuery::Dependencies();
        d.cached = [](int kind, uint32_t id) { return s_cached.count({ kind, id }) != 0; };
        d.now = [] { return s_now; };
    }

    void QueryStop() {
        CoA::AssetQuery::Reset();
        CoA::MysticEnchant::Reset();
        End();
    }

    std::vector<uint8_t> Batch(uint32_t opcode, uint32_t from, uint32_t n) {
        CoATest::Bytes b;
        b.U32(n);

        for (uint32_t i = 0; i < n; ++i) {
            b.U32(from + i);
        }

        return CoATest::Expected(opcode, b);
    }
}

// SMSG 0x90F adds or overwrites by key (ASCENSION_POI_ADD first), 0x910 removes, 0x911 clears; strings are u32 length +
// bytes. GetPOIInfo(index) walks the map in its iteration order (0-based); nothing past the end.
COA_TEST(POI_Packets) {
    Begin();
    CoA::POI::Reset();
    CoA::POI::Init();
    lua_State* L = CoATest::NewState(false);
    CoATest::RecordSignals();
    Deliver(0x90F, CoATest::Bytes().Name("camp").Name("Camp").U32(1).U32(2).U32(0x40400000).F32(4.5f).F32(-1.25f));
    CHECK((CoATest::Signals() == V{ "ASCENSION_POI_ADD:camp,Camp,1,2,1077936128,4.5,-1.25" }));
    CHECK((CoATest::Call(L, "", "GetNumActivePOI") == V{ "1" }));
    CHECK((CoATest::Call(L, "", "GetPOIInfo", { "n:0.7" }) == V{ "camp", "Camp", "1", "2", "1077936128", "4.5", "-1.25" }));
    CHECK((CoATest::Call(L, "", "GetPOIInfo", { "n:1" }) == V{}));
    Deliver(0x90F, CoATest::Bytes().Name("camp").Name("New").U32(9).U32(0).U32(0).F32(0.0f).F32(0.0f));
    Deliver(0x90F, CoATest::Bytes().Name("tower").Name("Tower").U32(3).U32(0).U32(0).F32(0.0f).F32(0.0f));
    CHECK((CoATest::Call(L, "", "GetNumActivePOI") == V{ "2" }));
    CHECK((Run(L, "local a = GetPOIInfo(0) local b = GetPOIInfo(1) return (a == 'camp' and b == 'tower') or (a == 'tower' and b == 'camp')")
        == V{ "true" }));
    CHECK((Run(L, "for i = 0, 1 do local k, t, a = GetPOIInfo(i) if k == 'camp' then return t, a end end") == V{ "New", "9" }));
    CoATest::Signals().clear();
    Deliver(0x910, CoATest::Bytes().Name("camp"));
    Deliver(0x910, CoATest::Bytes().Name("nope"));
    CHECK((CoATest::Call(L, "", "GetNumActivePOI") == V{ "1" }));
    CHECK((CoATest::Call(L, "", "GetPOIInfo", { "n:0" })[0] == "tower"));
    Deliver(0x911, CoATest::Bytes());
    CHECK((CoATest::Call(L, "", "GetNumActivePOI") == V{ "0" }));
    CHECK((CoATest::Signals() == V{ "ASCENSION_POI_REMOVE:camp", "ASCENSION_POI_REMOVE:nope", "ASCENSION_POI_CLEAR" }));
    // A short add reads "" and zeros past the end.
    Deliver(0x90F, CoATest::Bytes().Name("x"));
    CHECK((CoATest::Call(L, "", "GetPOIInfo", { "n:0" }) == V{ "x", "", "0", "0", "0", "0", "0" }));
    lua_close(L);
    CoA::POI::Reset();
    End();
}

// SMSG 0x9C4: u32 n x {category, i32 ms left, u8 used}, kept only for a known category with a max-charges column and a
// non-zero time; 0x9C5 {category, ms, used}: used 0 removes, a new category starts now, a known one keeps its start.
// GetSpellCharges(spell): left, max, start (s), duration (s); max, max, 0, recharge without an entry; 0 x 4 otherwise.
// Every handler fires SPELL_UPDATE_CHARGES while in world (0x9C5 only when it passed).
COA_TEST(SpellCharges_Packets) {
    ChargesStart();
    lua_State* L = CoATest::NewState(false);
    CoATest::RecordSignals();
    CHECK((CoATest::Call(L, "", "GetSpellCharges", { "n:500" }) == V{ "3", "3", "0", "8" }));
    CHECK((CoATest::Call(L, "", "GetSpellCharges", { "n:502" }) == V{ "0", "0", "0", "0" }));   // no category row
    CHECK((CoATest::Call(L, "", "GetSpellCharges", { "n:9" }) == V{ "0", "0", "0", "0" }));
    CHECK((CoATest::Call(L, "", "GetSpellCharges", { "s:x" }) == V{ "0", "0", "0", "0" }));
    CHECK((CoATest::Call(L, "", "GetSpellCharges", {}) == V{ "0", "0", "0", "0" }));
    Deliver(0x9C4, CoATest::Bytes().U32(4).U32(10).I32(3000).U8(2).U32(11).I32(1000).U8(1).U32(12).I32(0).U8(1).U32(77).I32(5).U8(1));
    CHECK((CoATest::Signals() == V{ "SPELL_UPDATE_CHARGES" }));
    // 10: 2 used, recharge started now (100 s), next at 103 s.
    CHECK((CoATest::Call(L, "", "GetSpellCharges", { "n:500" }) == V{ "1", "3", "100", "3" }));
    CHECK((CoATest::Call(L, "", "GetSpellCharges", { "n:501" }) == V{ "0", "0", "0", "5" }));   // max 0: never kept
    CHECK((CoATest::Call(L, "", "GetSpellCharges", { "n:503" }) == V{ "2", "2", "0", "1" }));   // time 0: not kept
    // 0x9C5 on a known category: used and next move, start stays.
    s_ms = 101000;
    Deliver(0x9C5, CoATest::Bytes().U32(10).U32(4000).U32(1));
    CHECK((CoATest::Call(L, "", "GetSpellCharges", { "n:500" }) == V{ "2", "3", "100", "5" }));
    // A new one starts now with the category's recharge; ms 0 / an unknown category / max 0 do nothing.
    Deliver(0x9C5, CoATest::Bytes().U32(12).U32(500).U32(2));
    CHECK((CoATest::Call(L, "", "GetSpellCharges", { "n:503" }) == V{ "0", "2", "101", "0.5" }));
    CoATest::Signals().clear();
    Deliver(0x9C5, CoATest::Bytes().U32(12).U32(0).U32(0));
    Deliver(0x9C5, CoATest::Bytes().U32(77).U32(10).U32(0));
    Deliver(0x9C5, CoATest::Bytes().U32(11).U32(10).U32(1));
    CHECK(CoATest::Signals().empty());
    Deliver(0x9C5, CoATest::Bytes().U32(12).U32(10).U32(0));   // used 0 removes
    CHECK((CoATest::Call(L, "", "GetSpellCharges", { "n:503" }) == V{ "2", "2", "0", "1" }));
    Deliver(0x9C3, CoATest::Bytes().U32(10));
    CHECK((CoATest::Call(L, "", "GetSpellCharges", { "n:500" }) == V{ "3", "3", "0", "8" }));
    CHECK(CoATest::Signals().size() == 2);
    s_inWorld = false;
    Deliver(0x9C2, CoATest::Bytes());
    CHECK(CoATest::Signals().size() == 2);
    s_inWorld = true;
    Deliver(0x9C2, CoATest::Bytes());
    CHECK(CoATest::Signals().size() == 3);
    lua_close(L);
    ChargesStop();
}

// FUN_10320680 after every idle tick: an entry whose recharge has ended gets a charge back and restarts its recharge
// (start now, next = start + duration); the last one removes it. World entry (0x1031FD70) empties the map.
COA_TEST(SpellCharges_TickAndEnterWorld) {
    ChargesStart();
    lua_State* L = CoATest::NewState(false);
    Deliver(0x9C4, CoATest::Bytes().U32(1).U32(10).I32(3000).U8(2));
    CoATest::RecordSignals();
    s_ms = 102999;
    CoA::Lifecycle::RunAfterIdle();
    CHECK(CoATest::Signals().empty());
    s_ms = 103000;
    CoA::Lifecycle::RunAfterIdle();
    CHECK((CoATest::Signals() == V{ "SPELL_UPDATE_CHARGES" }));
    CHECK((CoATest::Call(L, "", "GetSpellCharges", { "n:500" }) == V{ "2", "3", "103", "8" }));
    s_ms = 111000;
    CoA::Lifecycle::RunAfterIdle();
    CHECK((CoATest::Call(L, "", "GetSpellCharges", { "n:500" }) == V{ "3", "3", "0", "8" }));
    CHECK(CoATest::Signals().size() == 2);
    CoA::Lifecycle::RunAfterIdle();
    CHECK(CoATest::Signals().size() == 2);
    Deliver(0x9C4, CoATest::Bytes().U32(1).U32(10).I32(3000).U8(2));
    CoA::ClientHooks::enterWorld.Run();
    CHECK((CoATest::Call(L, "", "GetSpellCharges", { "n:500" }) == V{ "3", "3", "0", "8" }));
    // A count past the packet's end stops there.
    Deliver(0x9C4, CoATest::Bytes().U32(1000).U32(10).I32(3000).U8(1));
    CHECK((CoATest::Call(L, "", "GetSpellCharges", { "n:500" }) == V{ "2", "3", "111", "3" }));
    lua_close(L);
    ChargesStop();
}

// The aura-0x137 patch: on when the active player gains an aura whose spell has a 0x137 effect, off when such an aura
// goes (both always let the client continue); SMSG 0x6E9 / 0x6E8 skip the guid and turn it on / off.
COA_TEST(Aura137_Patch) {
    Begin();
    CoA::ClientHooks::Reset();
    CoA::ClientHooks::addAuraEffect.Clear();
    CoA::ClientHooks::removeAuraEffect.Clear();
    CoA::Aura137::Reset();
    CoA::Aura137::Init();
    int player = 0;
    int other = 0;
    const int with137 = 1;
    const int without = 0;
    static void* s_player;
    s_player = &player;
    CoA::Aura137::Deps& d = CoA::Aura137::Dependencies();
    d.isActivePlayer = [](void* unit) { return unit == s_player; };
    d.hasAura137 = [](const void* spell) { return *static_cast<const int*>(spell) == 1; };
    CHECK(CoA::ClientHooks::addAuraEffect.AllPass(&other, 0, &with137));
    CHECK(!CoA::ClientHooks::aura137TurnWhileFalling);
    CHECK(CoA::ClientHooks::addAuraEffect.AllPass(&player, 0, &without));
    CHECK(!CoA::ClientHooks::aura137TurnWhileFalling);
    CHECK(CoA::ClientHooks::addAuraEffect.AllPass(&player, 1, &with137));
    CHECK(CoA::ClientHooks::aura137TurnWhileFalling);
    CHECK(CoA::ClientHooks::removeAuraEffect.AllPass(&other, 0, &with137));
    CHECK(CoA::ClientHooks::removeAuraEffect.AllPass(&player, 0, &without));
    CHECK(CoA::ClientHooks::aura137TurnWhileFalling);
    CHECK(CoA::ClientHooks::removeAuraEffect.AllPass(&player, 0, &with137));
    CHECK(!CoA::ClientHooks::aura137TurnWhileFalling);
    Deliver(0x6E9, CoATest::Bytes().U64(5));
    CHECK(CoA::ClientHooks::aura137TurnWhileFalling);
    Deliver(0x6E8, CoATest::Bytes().U64(5));
    CHECK(!CoA::ClientHooks::aura137TurnWhileFalling);
    Deliver(0x6E9, CoATest::Bytes());
    CHECK(CoA::ClientHooks::aura137TurnWhileFalling);
    CoA::Aura137::Reset();
    CoA::ClientHooks::addAuraEffect.Clear();
    CoA::ClientHooks::removeAuraEffect.Clear();
    CoA::ClientHooks::Reset();
    End();
}

// TryCache*(id): false when cached; true and queued the first time; afterwards false, moving a still-waiting id to the
// front. The idle tick sends up to 50 ids per kind per batch (CMSG 0x61B / 0x61C / 0x61A {u32 n, ids}), at most 3
// batches a second (outside development realms).
COA_TEST(AssetQuery_TryCacheAndBatches) {
    QueryStart();
    lua_State* L = CoATest::NewState(false);
    CoATest::RecordPackets();
    s_cached.insert({ CoA::AssetQuery::ITEM, 1 });
    CHECK((CoATest::Call(L, "C_AssetQueryService", "TryCacheItem", { "n:1" }) == V{ "false" }));
    CHECK((CoATest::Call(L, "C_AssetQueryService", "TryCacheItem", { "n:2" }) == V{ "true" }));
    CHECK((CoATest::Call(L, "C_AssetQueryService", "TryCacheItem", { "n:3" }) == V{ "true" }));
    CHECK((CoATest::Call(L, "C_AssetQueryService", "TryCacheItem", { "n:3" }) == V{ "false" }));   // moved to the front
    CHECK((CoATest::Call(L, "C_AssetQueryService", "TryCacheQuest", { "n:2" }) == V{ "true" }));
    CHECK((CoATest::Call(L, "C_AssetQueryService", "TryCacheCreature", { "n:9" }) == V{ "true" }));
    CHECK((CoATest::Call(L, "C_AssetQueryService", "TryCacheItem", { "s:x" })[0].find("Usage: TryCacheItem(itemId)") != std::string::npos));
    CHECK((CoATest::Call(L, "C_AssetQueryService", "TryCacheQuest")[0].find("Usage: TryCacheQuest(questId)") != std::string::npos));
    CHECK((CoATest::Call(L, "C_AssetQueryService", "TryCacheCreature")[0].find("Usage: TryCacheCreature(creatureId)") != std::string::npos));
    CHECK(CoATest::SentPackets().empty());
    CoA::Lifecycle::RunAfterIdle();
    CHECK((CoATest::SentPackets() == Packets{ CoATest::Expected(0x61B, CoATest::Bytes().U32(2).U32(3).U32(2)),
        CoATest::Expected(0x61C, CoATest::Bytes().U32(1).U32(2)), CoATest::Expected(0x61A, CoATest::Bytes().U32(1).U32(9)) }));
    // In flight: no longer waiting, so asking again is false and moves nothing.
    CHECK((CoATest::Call(L, "C_AssetQueryService", "TryCacheItem", { "n:2" }) == V{ "false" }));
    CoATest::SentPackets().clear();

    for (uint32_t id = 100; id < 300; ++id) {
        CoA::AssetQuery::TryCache(CoA::AssetQuery::ITEM, id);
    }

    CoA::Lifecycle::RunAfterIdle();   // 2nd batch this second
    CoA::Lifecycle::RunAfterIdle();   // 3rd
    CoA::Lifecycle::RunAfterIdle();   // the limit
    CHECK((CoATest::SentPackets() == Packets{ Batch(0x61B, 100, 50), Batch(0x61B, 150, 50) }));
    s_now = 5001;
    CoA::Lifecycle::RunAfterIdle();
    CHECK(CoATest::SentPackets().size() == 3 && CoATest::SentPackets()[2] == Batch(0x61B, 200, 50));
    lua_close(L);
    QueryStop();
}

// In-flight ids: younger than 31 s and now cached -> <KIND>_CACHE_REQUEST_SUCCESS; 31 s or older -> _FAILURE and they
// leave the seen-set (TryCache queues them again). The glue screen clears everything.
COA_TEST(AssetQuery_InFlight) {
    QueryStart();
    CoATest::RecordSignals();
    CHECK(CoA::AssetQuery::TryCache(CoA::AssetQuery::ITEM, 7));
    CHECK(CoA::AssetQuery::TryCache(CoA::AssetQuery::QUEST, 8));
    CHECK(CoA::AssetQuery::TryCache(CoA::AssetQuery::CREATURE, 9));
    CoA::Lifecycle::RunAfterIdle();
    CHECK(CoATest::Signals().empty());
    s_cached.insert({ CoA::AssetQuery::QUEST, 8 });
    s_now = 5030;
    CoA::Lifecycle::RunAfterIdle();
    CHECK((CoATest::Signals() == V{ "QUEST_CACHE_REQUEST_SUCCESS:8" }));
    s_now = 5031;
    CoA::Lifecycle::RunAfterIdle();
    CHECK((CoATest::Signals() == V{ "QUEST_CACHE_REQUEST_SUCCESS:8", "ITEM_CACHE_REQUEST_FAILURE:7", "CREATURE_CACHE_REQUEST_FAILURE:9" }));
    CHECK(CoA::AssetQuery::TryCache(CoA::AssetQuery::ITEM, 7));    // forgotten after the failure
    CHECK(!CoA::AssetQuery::TryCache(CoA::AssetQuery::QUEST, 8));  // cached now
    CHECK(CoA::AssetQuery::TryCache(CoA::AssetQuery::CREATURE, 10));
    CHECK(!CoA::AssetQuery::TryCache(CoA::AssetQuery::CREATURE, 10));
    CoA::Lifecycle::RunGlueScreen();
    CHECK(CoA::AssetQuery::TryCache(CoA::AssetQuery::CREATURE, 10));   // seen-set cleared
    CoATest::RecordPackets();
    CoA::Lifecycle::RunAfterIdle();
    CHECK((CoATest::SentPackets() == Packets{ CoATest::Expected(0x61A, CoATest::Bytes().U32(1).U32(10)) }));
    QueryStop();
}

// MysticEnchant's world-entry prefetch (0x102EA2A0) goes through AscAssetQuery::TryCacheItem.
COA_TEST(AssetQuery_MysticEnchantPrefetch) {
    QueryStart();
    CoATest::RecordPackets();
    CoA::MysticEnchant::EnterWorld();
    CoA::Lifecycle::RunAfterIdle();
    CHECK((CoATest::SentPackets() == Packets{ CoATest::Expected(0x61B, CoATest::Bytes().U32(1).U32(0xF25D0)) }));
    QueryStop();
}

namespace {
    std::vector<uint8_t> s_quest;
    std::string s_name = "Hogger";
}

// GetQuestTemplate(id): the quest cache record as FUN_1021ecb0's table, read at the original's offsets: ObjectiveText 2
// and 3 and RewardTitleId .. Unknown land 0x200 / 0x400 past this client's fields; past the record they read 0 / "".
// Nothing without a record; a non-number argument is a Lua error (luaL_checknumber).
COA_TEST(Templates_Quest) {
    Begin();
    CoA::Templates::Reset();
    CoA::Templates::Init();
    lua_State* L = CoATest::NewState(false);
    s_quest.assign(0x28E4, 0);
    auto put = [](uint32_t off, int32_t v) { memcpy(&s_quest[off], &v, 4); };
    auto putf = [](uint32_t off, float v) { memcpy(&s_quest[off], &v, 4); };
    auto puts = [](uint32_t off, const char* s) { memcpy(&s_quest[off], s, strlen(s)); };
    put(0x00, 1234);
    put(0x08, 30);
    put(0x34, -50);
    putf(0x48, 2.5f);
    put(0x54, 111);
    put(0x60, 114);
    put(0x88, 66);
    putf(0xA8, 0.25f);
    puts(0xB4, "The Title");
    puts(0x2B4, "Kill things");
    puts(0x1A24, "Elwynn");
    put(0x1C24, 299);
    puts(0x1C94, "First");
    puts(0x1D94, "Second");    // the client's [1] -- never read
    puts(0x1E94, "Third");     // the client's [2] -- read as ObjectiveText[2]
    put(0x2094, 0x41);          // RewardTitleId in the client -- read as text: "A"
    puts(0x20A4 + 0x3F0, "Z"); // completion text +0x3F0 = the original's RewardTitleId (0x2494)
    puts(0x20A4 + 0x400, "Done");
    put(0x28CC, 77);
    put(0x3FC, 5);              // what 0x2CE0 would wrap to: past the record must not wrap            // the client's reputation overrides: past what the original reads there
    CoA::Templates::Dependencies().quest = [](uint32_t id, uint32_t& size) -> const uint8_t* {
        size = static_cast<uint32_t>(s_quest.size());
        return id == 1234 ? s_quest.data() : nullptr;
    };
    CHECK((Run(L, "local t = GetQuestTemplate(1234) return t.QuestID, t.QuestLevel, t.RewardMoney, t.RewardKillHonor, "
        "t.RewardItems[1], t.RewardItems[4], t.ChoiceItemAmount[0+1], t.POIx, t.Title, t.Objectives, t.Details, "
        "t.AreaDescription, t.RequiredNpcOrGo[1], #t.ObjectiveText, t.ObjectiveText[1], t.ObjectiveText[2], "
        "t.ObjectiveText[3], t.ObjectiveText[4], t.RewardTitleId, t.CompletedText, t.RewardFactionValue[1], "
        "t.RewardFactionValueOverride[5], t.Unknown, #t.RewardFactionID")
        == V{ "1234", "30", "-50", "2.5", "111", "114", "0", "0.25", "The Title", "Kill things", "", "Elwynn", "299", "4",
            "First", "Third", "A", "", "90", "Done", "0", "0", "0", "5" }));
    CHECK((Run(L, "return GetQuestTemplate(1)") == V{}));
    CHECK(Run(L, "return GetQuestTemplate('x')")[0].compare(0, 7, "<error>") == 0);
    lua_close(L);
    CoA::Templates::Reset();
    End();
}

// GetCreatureTemplate / GetCreatureModelData / GetCreatureDisplayInfo / GetCreatureDisplayInfoExtra: the record as a
// table (GeoBoxMax* read the GeoBoxMin* columns); nothing without one. ChallengeUtil's Creature:Query reads Name.
COA_TEST(Templates_Creature) {
    Begin();
    CoA::Templates::Reset();
    CoA::Templates::Init();
    lua_State* L = CoATest::NewState(false);
    CoA::Templates::Deps& d = CoA::Templates::Dependencies();
    d.creature = [](uint32_t id, CoA::Templates::Creature& c) {
        if (id != 448) {
            return false;
        }

        c.id = id;
        c.subName = nullptr;
        c.iconName = "Attack";
        c.flags = 0x80000000u;
        c.type = 7;
        c.rank = 1;
        c.proxy[1] = 9;
        c.display[0] = 384;
        c.health = 1.5f;
        c.racialLeader = 1;
        c.questItem[5] = 6;
        c.movement = 100;
        c.name = s_name.c_str();
        return true;
    };
    d.modelData = [](uint32_t id, CoA::Templates::ModelData& m) {
        m.id = static_cast<int32_t>(id);
        m.modelName = "Creature\\Hogger.m2";
        m.geoBoxMin[0] = -1.0f;
        m.geoBoxMin[2] = 3.0f;
        m.geoBoxMax[0] = 9.0f;
        m.missileRaise = 0.5f;
        return id == 5;
    };
    d.displayInfo = [](uint32_t id, CoA::Templates::DisplayInfo& i) {
        i.id = static_cast<int32_t>(id);
        i.textures[1] = "Skin2";
        i.portrait = nullptr;
        i.bloodLevel = 2;
        i.scale = 1.25f;
        return id == 6;
    };
    d.displayInfoExtra = [](uint32_t id, CoA::Templates::DisplayInfoExtra& e) {
        e.fields[0] = static_cast<int32_t>(id);
        e.fields[7] = 4;
        e.items[10] = 55;
        e.bakeName = "Bake.blp";
        return id == 8;
    };
    CHECK((Run(L, "local t = GetCreatureTemplate(448.6) return t.ID, t.Name, t.SubName, t.IconName, t.Flags, t.Type, t.Rank, "
        "t.ProxyCreatureID[2], #t.DisplayID, t.DisplayID[1], t.HealthModifier, t.PowerModifier, t.RacialLeader, t.QuestItem[6], "
        "t.MovementID, t.Family") == V{ "448", "Hogger", "", "Attack", "-2147483648", "7", "1", "9", "4", "384", "1.5", "0", "1",
            "6", "100", "0" }));
    CHECK((Run(L, "return GetCreatureTemplate(449)") == V{}));
    CHECK((Run(L, "local t = GetCreatureModelData(5) return t.ID, t.ModelName, t.GeoBoxMinX, t.GeoBoxMaxX, t.GeoBoxMaxZ, "
        "t.GeoBoxMaxY, t.MissileCollisionRaise") == V{ "5", "Creature\\Hogger.m2", "-1", "-1", "3", "0", "0.5" }));
    CHECK((Run(L, "return GetCreatureModelData(4)") == V{}));
    CHECK((Run(L, "local t = GetCreatureDisplayInfo(6) return t.ID, #t.TextureVariation, t.TextureVariation[1], "
        "t.TextureVariation[2], t.PortraitTextureName, t.BloodLevel, t.CreatureModelScale") == V{ "6", "3", "", "Skin2", "", "2", "1.25" }));
    CHECK((Run(L, "return GetCreatureDisplayInfo(7)") == V{}));
    CHECK((Run(L, "local t = GetCreatureDisplayInfoExtra(8) return t.ID, t.FacialHairID, #t.NPCItemDisplay, t.NPCItemDisplay[11], "
        "t.BakeName, t.Flags") == V{ "8", "4", "11", "55", "Bake.blp", "0" }));
    CHECK((Run(L, "return GetCreatureDisplayInfoExtra(9)") == V{}));
    lua_close(L);
    CoA::Templates::Reset();
    End();
}
