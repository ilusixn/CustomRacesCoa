#include "CoA/Source/Modules/SmallHooks.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include <cstring>

namespace {
    bool s_havePlayer = true;
    uint32_t s_castEnd = 0;
    uint32_t s_now = 1000;
    uint64_t s_banker = 0;
    std::vector<uint64_t> s_bankerWrites;
    uint64_t s_playerGuid = 0x42;

    bool CastEnd(uint32_t& out) {
        out = s_castEnd;
        return s_havePlayer;
    }
    uint32_t Now() { return s_now; }
    uint64_t Banker() { return s_banker; }
    void SetBanker(uint64_t guid) {
        s_banker = guid;
        s_bankerWrites.push_back(guid);
    }
    uint64_t PlayerGuid() { return s_playerGuid; }

    void Set(const char* name, const char* value) {
        CVar* var = CVar::Lookup(name);
        CHECK(var != nullptr);

        if (var) {
            var->Set(value, true, false, false, false);
        }
    }

    void Start() {
        CoA::CVars::Reset();
        CoA::ClientHooks::enterWorld.Clear();
        CoA::SmallHooks::Reset();
        s_havePlayer = true;
        s_castEnd = 0;
        s_now = 1000;
        s_banker = 0;
        s_bankerWrites.clear();
        s_playerGuid = 0x42;
        CoA::SmallHooks::Deps& d = CoA::SmallHooks::Dependencies();
        d.castEndTime = CastEnd;
        d.now = Now;
        d.bankerGuid = Banker;
        d.setBankerGuid = SetBanker;
        d.activePlayerGuid = PlayerGuid;
        CoA::SmallHooks::Init();
        CoA::CVars::RegisterWorld();
        Set("SpellQueueWindow", "400");
        Set("bankEquipmentManager", "0");
        CoATest::RecordPackets();
    }
}

COA_TEST(SmallHooks_QueuesTheTwoWorldCVars) {
    Start();
    size_t glue = 0;
    CoA::CVars::Glue(glue);
    CHECK(glue == 0);
    size_t count = 0;
    const CoA::CVarEntry* world = CoA::CVars::World(count);
    CHECK(count == 2);

    if (count == 2) {
        CHECK(strcmp(world[0].name, "SpellQueueWindow") == 0);
        CHECK(strcmp(world[0].defaultValue, "400") == 0);
        CHECK(world[0].flags == 1 && world[0].category == 3 && world[0].callback == nullptr && !world[0].setCommand);
        CHECK(world[0].help && strcmp(world[0].help, "Sets how early you can pre-activate/queue a spell/ability. (In Milliseconds)") == 0);
        CHECK(strcmp(world[1].name, "bankEquipmentManager") == 0);
        CHECK(strcmp(world[1].defaultValue, "0") == 0);
        CHECK(world[1].flags == 1 && world[1].category == 4 && world[1].callback == CoA::SmallHooks::OnBankManager);
        CHECK(world[1].help == nullptr && !world[1].setCommand);
    }

    CHECK(CVar::Lookup("SpellQueueWindow") != nullptr);
}

// Review focus: no player / no cast end -> false; elapsed -> true; else whether it is more than the window away.
COA_TEST(SmallHooks_SpellQueueWindowReplacement) {
    Start();
    s_havePlayer = false;
    s_castEnd = 5000;
    CHECK(!CoA::SmallHooks::SpellQueueReady());
    s_havePlayer = true;
    s_castEnd = 0;
    CHECK(!CoA::SmallHooks::SpellQueueReady());
    s_castEnd = 1000;
    CHECK(CoA::SmallHooks::SpellQueueReady());    // ends now
    s_castEnd = 900;
    CHECK(CoA::SmallHooks::SpellQueueReady());    // ended
    s_castEnd = 1400;
    CHECK(!CoA::SmallHooks::SpellQueueReady());   // 400 away: inside the window
    s_castEnd = 1401;
    CHECK(CoA::SmallHooks::SpellQueueReady());    // 401 away: outside
    Set("SpellQueueWindow", "0");
    s_castEnd = 1001;
    CHECK(CoA::SmallHooks::SpellQueueReady());
    // A negative window compares unsigned: never outside it.
    Set("SpellQueueWindow", "-1");
    s_castEnd = 900000;
    CHECK(!CoA::SmallHooks::SpellQueueReady());
}

COA_TEST(SmallHooks_BankEquipmentManagerAroundUseEquipmentSet) {
    Start();
    // Off: the GUID is never touched.
    CHECK(!CoA::SmallHooks::BeforeUseEquipmentSet());
    CoA::SmallHooks::AfterUseEquipmentSet(false);
    CHECK(s_bankerWrites.empty());

    Set("bankEquipmentManager", "1");
    CoATest::SentPackets().clear();
    CHECK(CoA::SmallHooks::BeforeUseEquipmentSet());
    CHECK(s_banker == 1);
    CoA::SmallHooks::AfterUseEquipmentSet(true);
    CHECK(s_banker == 0);
    CHECK(s_bankerWrites.size() == 2);

    // A real banker is left alone.
    s_banker = 0x77;
    s_bankerWrites.clear();
    CHECK(!CoA::SmallHooks::BeforeUseEquipmentSet());
    CoA::SmallHooks::AfterUseEquipmentSet(false);
    CHECK(s_banker == 0x77 && s_bankerWrites.empty());

    // Switched off while the set was used: the 1 stays, as in the original.
    s_banker = 0;
    CHECK(CoA::SmallHooks::BeforeUseEquipmentSet());
    Set("bankEquipmentManager", "0");
    CoA::SmallHooks::AfterUseEquipmentSet(true);
    CHECK(s_banker == 1);
}

COA_TEST(SmallHooks_BankManagerCallbackAndEnterWorld) {
    Start();
    CoATest::SentPackets().clear();
    CHECK(CoA::SmallHooks::OnBankManager(nullptr, "0", "1", nullptr));
    CHECK(CoA::SmallHooks::OnBankManager(nullptr, "1", "yes", nullptr));
    CoATest::Bytes one;
    one.U8(1);
    CoATest::Bytes zero;
    zero.U8(0);
    CHECK(CoATest::SentPackets().size() == 2);

    if (CoATest::SentPackets().size() == 2) {
        CHECK(CoATest::SentPackets()[0] == CoATest::Expected(0x55D, one));
        CHECK(CoATest::SentPackets()[1] == CoATest::Expected(0x55D, zero));
    }

    // Out of the world: nothing.
    s_playerGuid = 0;
    CoATest::SentPackets().clear();
    CHECK(CoA::SmallHooks::OnBankManager(nullptr, "0", "1", nullptr));
    CHECK(CoATest::SentPackets().empty());

    // Entering the world sends 1 while the CVar is on, nothing while off.
    s_playerGuid = 0x42;
    CoA::ClientHooks::enterWorld.Run();
    CHECK(CoATest::SentPackets().empty());
    Set("bankEquipmentManager", "1");
    CoATest::SentPackets().clear();
    CoA::ClientHooks::enterWorld.Run();
    CHECK(CoATest::SentPackets().size() == 1 && CoATest::SentPackets()[0] == CoATest::Expected(0x55D, one));
    CoATest::StopRecordingPackets();
}
