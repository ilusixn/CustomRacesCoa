#include "CoA/Source/Modules/WorldEntryTweaks.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <initializer_list>
#include <map>
#include <vector>

namespace {
    bool s_hasPlayer = true;
    uint8_t s_class = 0;
    std::map<uint32_t, bool> s_forms;

    void Setup(bool hasPlayer, uint8_t cls) {
        CoA::WorldEntryTweaks::Reset();
        s_hasPlayer = hasPlayer;
        s_class = cls;
        s_forms.clear();
        CoA::WorldEntryTweaks::Deps& d = CoA::WorldEntryTweaks::Dependencies();
        d.hasPlayer = [] { return s_hasPlayer; };
        d.playerClass = [] { return s_class; };
        d.setFormFlag = [](uint32_t form, bool on) { s_forms[form] = on; };
    }

    std::vector<std::vector<uint8_t>> Enter() {
        CoATest::RecordPackets();
        CoA::WorldEntryTweaks::OnEnteredWorld();
        CoATest::StopRecordingPackets();
        return CoATest::SentPackets();
    }
}

// FUN_100c6380: classes 1..9 and 11 are stock.
COA_TEST(WorldEntryTweaks_StockClass) {
    for (int c : { 1, 2, 5, 9, 11 }) {
        CHECK(CoA::WorldEntryTweaks::StockClass(static_cast<uint8_t>(c)));
    }

    for (int c : { 0, 10, 12, 20, 32 }) {
        CHECK(!CoA::WorldEntryTweaks::StockClass(static_cast<uint8_t>(c)));
    }
}

// A non-stock class: ammo and proficiency checks skipped, the stance forms flagged, CMSG 0x561 {0, 1} sent.
COA_TEST(WorldEntryTweaks_NonStockEntry) {
    Setup(true, 20);
    CHECK((Enter() == std::vector<std::vector<uint8_t>>{ CoATest::Expected(0x561, CoATest::Bytes().U32(0).U32(1)) }));
    CHECK(CoA::WorldEntryTweaks::SkipAmmoCheck());
    CHECK(CoA::WorldEntryTweaks::SkipProficiency());
    CHECK((s_forms == std::map<uint32_t, bool>{ { 0x11, true }, { 0x12, true }, { 0x13, true } }));
    CoA::WorldEntryTweaks::Reset();
}

// A stock class afterwards: proficiency and the forms follow it, the ammo skip stays (nothing puts 0x809572 back).
COA_TEST(WorldEntryTweaks_StockEntryAfterNonStock) {
    Setup(true, 20);
    Enter();
    s_class = 3;
    CHECK(Enter().size() == 1);
    CHECK(CoA::WorldEntryTweaks::SkipAmmoCheck());
    CHECK(!CoA::WorldEntryTweaks::SkipProficiency());
    CHECK((s_forms == std::map<uint32_t, bool>{ { 0x11, false }, { 0x12, false }, { 0x13, false } }));
    CoA::WorldEntryTweaks::Reset();
}

// A stock class from the start changes nothing but still sends the notice.
COA_TEST(WorldEntryTweaks_StockEntry) {
    Setup(true, 1);
    CHECK(Enter().size() == 1);
    CHECK(!CoA::WorldEntryTweaks::SkipAmmoCheck());
    CHECK(!CoA::WorldEntryTweaks::SkipProficiency());
    CoA::WorldEntryTweaks::Reset();
}

// Without an active player the class-dependent callbacks do nothing.
COA_TEST(WorldEntryTweaks_NoPlayer) {
    Setup(false, 20);
    CHECK(Enter().size() == 1);
    CHECK(!CoA::WorldEntryTweaks::SkipAmmoCheck());
    CHECK(!CoA::WorldEntryTweaks::SkipProficiency());
    CHECK(s_forms.empty());
    CoA::WorldEntryTweaks::Reset();
}
