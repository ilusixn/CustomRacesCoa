#include "CoA/Source/Modules/GameMode.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <cstring>
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;
    int32_t s_spec = -1;
    int s_listenerCalls = 0;

    lua_State* Setup() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
        CoA::GameMode::Reset();
        CoA::GameMode::Init();
        s_spec = -1;
        s_listenerCalls = 0;
        CoA::GameMode::Deps& deps = CoA::GameMode::Dependencies();
        deps.activeSpecIndex = [] { return s_spec; };
        deps.equipmentSetId = [](const char* name, uint32_t& id) {
            id = 77;
            return strcmp(name, "Tank") == 0;
        };
        deps.equipmentSetName = [](uint32_t id) -> const char* { return id == 77 ? "Tank" : nullptr; };
        CoA::ClientState::SetInWorldProbe([] { return true; });
        return CoATest::NewState(false);
    }

    V Feed(uint32_t opcode, CoATest::Bytes bytes) {
        CoATest::RecordSignals();
        CDataStore msg(bytes.data.data(), static_cast<uint32_t>(bytes.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result);
        CoATest::StopRecordingSignals();
        return CoATest::Signals();
    }
}

// 0x90B: mode and mask, the listeners, then ASCENSION_CUSTOM_GAME_MODE_CHANGED while in world. GetCustomGameMode hides
// bit 0x800 unless the active spec has build draft on.
COA_TEST(GameMode_ModeChanged) {
    lua_State* L = Setup();
    CoA::GameMode::OnModeChanged([] { ++s_listenerCalls; });
    CHECK(Feed(0x90B, CoATest::Bytes().U32(0x848)) == V{ "ASCENSION_CUSTOM_GAME_MODE_CHANGED:2120" });
    CHECK(s_listenerCalls == 1);
    CHECK(CoA::GameMode::Mode() == 0x848 && CoA::GameMode::ActiveMask() == 0x848);
    CHECK(CoATest::Call(L, "", "GetCustomGameMode")[0] == std::to_string(0x48));
    CHECK(CoATest::Call(L, "", "IsDraftMode")[0] == "true");
    s_spec = 2;
    Feed(0x5CC, CoATest::Bytes().U32(2).U32(1));
    CHECK(CoATest::Call(L, "", "GetCustomGameMode")[0] == std::to_string(0x848));
    // Outside the world the event is not fired.
    CoA::ClientState::SetInWorldProbe([] { return false; });
    CHECK(Feed(0x90B, CoATest::Bytes().U32(0)).empty());
    CoA::ClientState::SetInWorldProbe(nullptr);
}

// 0x5CC / 0x737 / 0x738: the per-spec records, their events, and the getters (1-based specs).
COA_TEST(GameMode_SpecLinks) {
    lua_State* L = Setup();
    CHECK((Feed(0x5CC, CoATest::Bytes().U32(3).U32(1)) ==
        V{ "SPECIALIZATION_FLAGS_UPDATED:3,1", "ASCENSION_CUSTOM_GAME_MODE_CHANGED:0" }));
    CHECK(Feed(0x5CC, CoATest::Bytes().U32(3).U32(3)) == V{ "SPECIALIZATION_FLAGS_UPDATED:3,3" });
    CHECK(CoATest::Call(L, "", "IsBuildDraftModeEnabled", { "n:4" })[0] == "true");
    CHECK(CoA::GameMode::SpecBuildDraft(3) && !CoA::GameMode::SpecBuildDraft(20));
    CHECK(Feed(0x5CC, CoATest::Bytes().U32(20).U32(1)).empty());

    CHECK(Feed(0x737, CoATest::Bytes().U32(1).U8(1).U32(55)) == V{ "MYSTIC_ENCHANT_SPECIALIZATION_LINK_UPDATED" });
    CHECK(CoATest::Call(L, "", "GetMysticEnchantPresetIDForSpecialization", { "n:2" })[0] == "55");
    Feed(0x737, CoATest::Bytes().U32(1).U8(0));
    CHECK(CoATest::Call(L, "", "GetMysticEnchantPresetIDForSpecialization", { "n:2" })[0] == "nil");

    CHECK(Feed(0x738, CoATest::Bytes().U32(0).U8(1).U64(77)) == V{ "EQUIPMENT_SET_SPECIALIZATION_LINK_UPDATED" });
    CHECK(CoATest::Call(L, "", "GetEquipmentSetIDForSpecialization", { "n:1" })[0] == "Tank");
    Feed(0x738, CoATest::Bytes().U32(0).U8(1).U64(0x100000077ull));   // a high word never matches
    CHECK(CoATest::Call(L, "", "GetEquipmentSetIDForSpecialization", { "n:1" })[0] == "nil");
}

// The toggles and the link setters send the original's CMSG bytes.
COA_TEST(GameMode_Toggles) {
    lua_State* L = Setup();
    CoATest::RecordPackets();
    CHECK(CoATest::Call(L, "", "ToggleIronmanMode", { "b:1" })[0] == "true");
    CoATest::Call(L, "", "ToggleBuildDraftMode", { "b:0" });
    CoATest::Call(L, "", "SetMysticEnchantPresetIDForSpecialization", { "n:2", "n:9" });
    CoATest::Call(L, "", "SetMysticEnchantPresetIDForSpecialization", { "n:2" });
    CoATest::Call(L, "", "SetEquipmentSetIDForSpecialization", { "n:1", "s:Tank" });
    CoATest::Call(L, "", "SetEquipmentSetIDForSpecialization", { "n:1", "s:Nope" });
    CoATest::Call(L, "", "SetEquipmentSetIDForSpecialization", { "n:21" });
    CoATest::StopRecordingPackets();
    const auto& sent = CoATest::SentPackets();
    CHECK(sent.size() == 5);
    CHECK(sent[0] == CoATest::Expected(0x5A4, CoATest::Bytes().Name("Ironman").U8(1)));
    CHECK(sent[1] == CoATest::Expected(0x5A4, CoATest::Bytes().Name("BuildDraft").U8(0)));
    CHECK(sent[2] == CoATest::Expected(0x739, CoATest::Bytes().U32(1).U8(1).U32(9)));
    CHECK(sent[3] == CoATest::Expected(0x739, CoATest::Bytes().U32(1).U8(0)));
    CHECK(sent[4] == CoATest::Expected(0x73A, CoATest::Bytes().U32(0).U8(1).U32(77).U32(0)));
}

// The glue screen clears the mode and every spec record.
COA_TEST(GameMode_GlueResets) {
    lua_State* L = Setup();
    Feed(0x90B, CoATest::Bytes().U32(8));
    Feed(0x737, CoATest::Bytes().U32(1).U8(1).U32(55));
    CoA::Lifecycle::RunGlueScreen();
    CHECK(CoA::GameMode::Mode() == 0);
    CHECK(CoATest::Call(L, "", "GetMysticEnchantPresetIDForSpecialization", { "n:2" })[0] == "nil");
    CoA::ClientState::SetInWorldProbe(nullptr);
}
