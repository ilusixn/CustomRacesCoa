#include "CoA/Source/Modules/AttachLua.h"
#include "CoA/Source/Modules/Callboard.h"
#include "CoA/Source/Modules/GlobalsJ.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/QuestAddon.h"
#include "CoA/Source/Modules/TemporalContracts.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;
    using Packets = std::vector<std::vector<uint8_t>>;

    std::vector<std::pair<uint32_t, void*>> s_called;

    void Called(uint32_t quest, void* arg) {
        s_called.emplace_back(quest, arg);
    }

    void Start() {
        CoA::Packets::Clear();
        CoA::QuestAddon::Reset();
        CoA::CallboardCache::Reset();
        CoA::TemporalContracts::Reset();
        CoA::QuestAddon::Init();
        s_called.clear();
    }

    void Stop() {
        CoATest::StopRecordingPackets();
        CoA::QuestAddon::Reset();
        CoA::CallboardCache::Reset();
        CoA::TemporalContracts::Reset();
        CoA::Packets::Clear();
    }

    // SMSG 0x732 through the packet table.
    void Respond(CoATest::Bytes b) {
        CDataStore msg(b.data.data(), static_cast<uint32_t>(b.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(0x732, 0, &msg, result);
    }

    std::vector<uint8_t> Query(uint32_t quest) {
        return CoATest::Expected(0x731, CoATest::Bytes().U32(quest));
    }
}

// FUN_10115fc0: an unknown quest is requested once (CMSG 0x731 {u32 quest}), a pending one is not requested again, quest 0
// is nothing; SMSG 0x732 loads the record {quest, points, unk, quest} and runs the queued callbacks once, in order.
COA_TEST(QuestAddon_GetAndResponse) {
    Start();
    CoATest::RecordPackets();
    CHECK(CoA::QuestAddon::Get(0) == nullptr);
    CHECK(CoA::QuestAddon::Get(77, Called, reinterpret_cast<void*>(1)) == nullptr);
    CHECK(CoA::QuestAddon::Get(77, Called, reinterpret_cast<void*>(2)) == nullptr);
    CHECK(CoA::QuestAddon::Get(77) == nullptr);
    CHECK(CoA::QuestAddon::Get(78) == nullptr);
    CHECK((CoATest::SentPackets() == Packets{ Query(77), Query(78) }));
    CHECK(CoA::QuestAddon::Count() == 2);
    CHECK(s_called.empty());
    Respond(CoATest::Bytes().U32(77).U32(15).U32(3));
    CHECK((s_called == std::vector<std::pair<uint32_t, void*>>{ { 77, reinterpret_cast<void*>(1) }, { 77, reinterpret_cast<void*>(2) } }));
    const CoA::QuestAddon::Record* r = CoA::QuestAddon::Get(77, Called, reinterpret_cast<void*>(3));
    CHECK(r && r->quest == 77 && r->points == 15 && r->unk == 3 && r->quest2 == 77);
    CHECK(s_called.size() == 2);   // a loaded record does not queue
    // A second response re-records and finds nothing queued.
    Respond(CoATest::Bytes().U32(77).U32(16).U32(0));
    CHECK(s_called.size() == 2);
    CHECK(CoA::QuestAddon::Get(77)->points == 16);
    // A response for a quest never requested is dropped.
    Respond(CoATest::Bytes().U32(90).U32(1).U32(1));
    CHECK(CoA::QuestAddon::Count() == 2);
    CHECK(CoA::QuestAddon::Get(90) == nullptr);   // now requested
    CHECK(CoATest::SentPackets().size() == 3);
    // A short response reads zeros past the end (CoA::Read): quest 0 was never requested.
    Respond(CoATest::Bytes().U32(78).U32(9));
    const CoA::QuestAddon::Record* s = CoA::QuestAddon::Get(78);
    CHECK(s && s->points == 9 && s->unk == 0);
    Stop();
}

// FUN_101b0070 on AttachLua's 0x635710 list: a different version empties the store and becomes the store's; the same
// version keeps it. The cache-file helpers prune unloaded entries.
COA_TEST(QuestAddon_CacheVersion) {
    Start();
    CoA::QuestAddon::Get(5);
    CoA::QuestAddon::Get(6);
    Respond(CoATest::Bytes().U32(5).U32(1).U32(0));
    CoA::AttachLua::ClientCacheVersionSet(0);
    CHECK(CoA::QuestAddon::Count() == 2);
    CoA::QuestAddon::PruneUnloaded();
    CHECK(CoA::QuestAddon::Count() == 1);
    uint32_t sum = 0;
    CoA::QuestAddon::ForEachRecord([](const CoA::QuestAddon::Record& r, void* ctx) { *static_cast<uint32_t*>(ctx) += r.quest; }, &sum);
    CHECK(sum == 5);
    CoA::AttachLua::ClientCacheVersionSet(12340);
    CHECK(CoA::QuestAddon::Count() == 0 && CoA::QuestAddon::Version() == 12340);
    CoA::QuestAddon::Get(5);
    CoA::AttachLua::ClientCacheVersionSet(12340);
    CHECK(CoA::QuestAddon::Count() == 1);
    Stop();
}

// The consumers' questAddonPoints: C_CallboardCache.GetPointsForQuest is nil (and requests the quest) until 0x732 answers.
COA_TEST(QuestAddon_CallboardCachePoints) {
    CoA::LuaReg::Reset();
    Start();
    CoA::CallboardCache::Init();
    lua_State* L = CoATest::NewState(false);
    CoATest::RecordPackets();
    CHECK((CoATest::Call(L, "C_CallboardCache", "GetPointsForQuest", { "n:41" }) == V{ "nil" }));
    CHECK((CoATest::SentPackets() == Packets{ Query(41) }));
    Respond(CoATest::Bytes().U32(41).U32(250).U32(0));
    CHECK((CoATest::Call(L, "C_CallboardCache", "GetPointsForQuest", { "n:41" }) == V{ "250" }));
    CHECK(CoATest::SentPackets().size() == 1);
    lua_close(L);
    Stop();
    CoA::LuaReg::Reset();
}

// GlobalsJ's IsQuestSequenced (FUN_1021eb20): nil while the record is fetched, then its +8 & 1.
COA_TEST(QuestAddon_IsQuestSequenced) {
    CoA::LuaReg::Reset();
    Start();
    CoA::GlobalsJ::Reset();
    CoA::GlobalsJ::Init();
    lua_State* L = CoATest::NewState(false);
    CoATest::RecordPackets();
    CHECK((CoATest::Call(L, "", "IsQuestSequenced", { "n:7.9" }) == V{ "nil" }));
    CHECK((CoATest::SentPackets() == Packets{ Query(7) }));
    CHECK((CoATest::Call(L, "", "IsQuestSequenced", { "n:8" }) == V{ "nil" }));
    Respond(CoATest::Bytes().U32(7).U32(0).U32(3));
    Respond(CoATest::Bytes().U32(8).U32(0).U32(2));
    CHECK((CoATest::Call(L, "", "IsQuestSequenced", { "n:7" }) == V{ "true" }));
    CHECK((CoATest::Call(L, "", "IsQuestSequenced", { "n:8" }) == V{ "false" }));
    CHECK((CoATest::Call(L, "", "IsQuestSequenced", { "n:0" }) == V{ "nil" }));
    CHECK(CoATest::SentPackets().size() == 2);
    lua_close(L);
    CoA::GlobalsJ::Reset();
    Stop();
    CoA::LuaReg::Reset();
}
