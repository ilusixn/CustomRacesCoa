#include "CoA/Source/Modules/AttachFixes.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Logger.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
    std::string ReadAll(const char* file) {
        std::string text;
        FILE* f = fopen(file, "rb");

        if (!f) {
            return text;
        }

        char buf[512];
        size_t n;

        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
            text.append(buf, n);
        }

        fclose(f);
        return text;
    }

    bool s_loading = false;
    int32_t s_rebuilds = 0;

    // ChrClassesRoles.dbc: id, roles, extra roles [3], condition types [3], condition values [3].
    bool ReadRoles(const char* path, std::vector<uint8_t>& out) {
        if (strstr(path, "ChrClassesRoles") == nullptr) {
            return false;
        }

        out = CoATest::Wdbc({ { 12, 0x2, 0x4, 0, 0, 0, 0, 0, 0, 0, 0 }, { 13, 0x8, 0x2, 0, 0, 1, 0, 0, 777, 0, 0 } }, "");
        return true;
    }
}

// 0x424B50: a failed open is logged to MissingFiles once per name.
COA_TEST(AttachFixes_MissingFileLoggedOnce) {
    CoATest::TempCwd cwd;
    CoA::Logger::Init();
    CoA::AttachFixes::LogMissingFileOnce("World\\Missing.m2");
    CoA::AttachFixes::LogMissingFileOnce("World\\Missing.m2");
    CoA::Logger::Shutdown();
    CHECK(ReadAll("Logs\\MissingFiles.txt") == "World\\Missing.m2\r\n");
}

// 0x70CDF0: "$CN" names take the owner's name over their first three characters.
COA_TEST(AttachFixes_NameTokenReplaced) {
    std::string name = "$CN's Pet";
    CHECK(CoA::AttachFixes::ReplaceNameToken(name, "Thrall"));
    CHECK(name == "Thrall's Pet");

    // At position 0 even when the token is elsewhere, as the original.
    name = "Pet of $CN";
    CHECK(CoA::AttachFixes::ReplaceNameToken(name, "Jaina"));
    CHECK(name == "Jaina of $CN");

    name = "Bonfire";
    CHECK(!CoA::AttachFixes::ReplaceNameToken(name, "Thrall"));
    CHECK(name == "Bonfire");

    name = "$CN's Pet";
    CHECK(!CoA::AttachFixes::ReplaceNameToken(name, nullptr));
    CHECK(name == "$CN's Pet");
}

// 0x81B380: an unknown event registered while the world UI loads is added and the table rebuilt.
COA_TEST(AttachFixes_UnknownEventAddedWhileLoading) {
    CoA::Events::ResetQueue();
    CoA::AttachFixes::Deps& deps = CoA::AttachFixes::Dependencies();
    deps.worldLoading = []() { return s_loading; };
    deps.eventId = [](const char* name) { return strcmp(name, "PLAYER_LOGIN") == 0 ? 7 : -1; };
    deps.rebuildEvents = []() { ++s_rebuilds; };
    s_rebuilds = 0;

    s_loading = true;
    CHECK(CoA::AttachFixes::AddUnknownEventWhileLoading("MY_ADDON_EVENT"));
    CHECK(s_rebuilds == 1);
    CHECK(!CoA::AttachFixes::AddUnknownEventWhileLoading("PLAYER_LOGIN"));

    s_loading = false;
    CHECK(!CoA::AttachFixes::AddUnknownEventWhileLoading("OTHER_EVENT"));
    CHECK(s_rebuilds == 1);

    // The queued name follows the stock world list.
    std::vector<const char*> stock(0x2D2, "STOCK");
    uint32_t count = 0x2D2;
    const char** names = CoA::Events::ExtendList(stock.data(), count);
    CHECK(count == 0x2D3);
    CHECK(strcmp(names[0x2D2], "MY_ADDON_EVENT") == 0);

    deps = CoA::AttachFixes::Deps();
    CoA::Events::ResetQueue();
}

// 0x6B1080: ChrClassesRoles.dbc's roles pick a stock class for the LFG role checks.
COA_TEST(AttachFixes_LfgRoleClassFromDbc) {
    CoA::Dbc::SetFileReader(ReadRoles);
    CoA::AttachFixes::Deps& deps = CoA::AttachFixes::Dependencies();

    // Tank + healer (condition type 0: always, with a player) reads as a druid.
    deps.roleCondition = [](int32_t type, uint32_t) { return type == 0; };
    CHECK(CoA::AttachFixes::LfgRoleClass(12) == 11);
    // Without a player no condition holds: tank only, a warrior.
    deps.roleCondition = nullptr;
    CHECK(CoA::AttachFixes::LfgRoleClass(12) == 1);
    // Damage, plus tank when spell 777 is known.
    CHECK(CoA::AttachFixes::LfgRoleClass(13) == 4);
    deps.roleCondition = [](int32_t type, uint32_t value) { return type == 1 && value == 777; };
    CHECK(CoA::AttachFixes::LfgRoleClass(13) == 1);
    // A class without a row keeps its own class; mage and warlock carry fixed roles.
    CHECK(CoA::AttachFixes::LfgRoleClass(3) == 3);
    CHECK(CoA::AttachFixes::LfgRoleClass(8) == 5);

    deps = CoA::AttachFixes::Deps();
    CoA::Dbc::SetFileReader(nullptr);
}

// 0x76DDE0: the deposit, truncated, at least 1; 1 for quality-6 items and unknown run times.
COA_TEST(AttachFixes_AuctionDeposit) {
    CHECK(CoA::AttachFixes::AuctionDeposit(15, 10000, 720, 4, 1.0f) == 4500);
    CHECK(CoA::AttachFixes::AuctionDeposit(15, 10000, 1440, 4, 0.5f) == 4500);
    CHECK(CoA::AttachFixes::AuctionDeposit(15, 10000, 720, 6, 1.0f) == 1);
    CHECK(CoA::AttachFixes::AuctionDeposit(15, 10000, 600, 4, 1.0f) == 1);
    CHECK(CoA::AttachFixes::AuctionDeposit(1, 1, 720, 4, 1.0f) == 1);
}

// SMSG 0x65C: the marker paths by kind and state.
COA_TEST(AttachFixes_MarkerModels) {
    CHECK(strcmp(CoA::AttachFixes::MarkerModel(1, 2), "interface\\buttons\\talktome_new.m2") == 0);
    CHECK(strcmp(CoA::AttachFixes::MarkerModel(6, 10), "interface\\buttons\\talktome_questionmark_asc.m2") == 0);
    CHECK(strcmp(CoA::AttachFixes::MarkerModel(1, 1), "") == 0);
    CHECK(CoA::AttachFixes::MarkerModel(5, 2) == nullptr);
    CHECK(CoA::AttachFixes::MarkerModel(9, 2) == nullptr);
}

// 0x6E7B00 / 0x5AB120: the placement mode and the swallowed PutActionInSlot.
COA_TEST(AttachFixes_LearnedSpellPlacement) {
    CoA::AttachFixes::Deps& deps = CoA::AttachFixes::Dependencies();
    deps.spellCustomAttr = [](uint32_t spell, uint32_t& a10, uint32_t& a14, uint32_t& a18) {
        a10 = spell == 1 ? 0x10000 : 0;
        a14 = spell == 2 ? 0x1000000 : 0;
        a18 = 0;
        return true;
    };

    CoA::AttachFixes::BeginAddKnownSpell(1, 4, 1);
    CHECK(CoA::AttachFixes::PlacementJump() == CoA::AttachFixes::Placement::Always);
    CHECK(!CoA::AttachFixes::TakeSkipPutAction());
    CoA::AttachFixes::EndAddKnownSpell();
    CHECK(CoA::AttachFixes::PlacementJump() == CoA::AttachFixes::Placement::Stock);

    CoA::AttachFixes::BeginAddKnownSpell(2, 4, 1);
    CHECK(CoA::AttachFixes::PlacementJump() == CoA::AttachFixes::Placement::Never);
    CHECK(CoA::AttachFixes::TakeSkipPutAction());
    CHECK(!CoA::AttachFixes::TakeSkipPutAction());
    CoA::AttachFixes::EndAddKnownSpell();

    // No spell class set: the next placement becomes a cursor clear, the jump stays stock.
    CoA::AttachFixes::BeginAddKnownSpell(3, 0, 1);
    CHECK(CoA::AttachFixes::PlacementJump() == CoA::AttachFixes::Placement::Stock);
    CHECK(CoA::AttachFixes::TakeSkipPutAction());
    CoA::AttachFixes::EndAddKnownSpell();

    deps = CoA::AttachFixes::Deps();
}

// 0x8A65E0: the three aura-type clamps.
COA_TEST(AttachFixes_SpellEffectClamped) {
    int32_t aura = 0x13D, misc = 5;
    CoA::AttachFixes::ClampSpellEffect(aura, misc);
    CHECK(aura == 4 && misc == 5);
    aura = 0x6B, misc = 31;
    CoA::AttachFixes::ClampSpellEffect(aura, misc);
    CHECK(aura == 4 && misc == 0);
    aura = 0x6C, misc = 30;
    CoA::AttachFixes::ClampSpellEffect(aura, misc);
    CHECK(aura == 0x6C && misc == 30);
}

// CMSG 0x741 and CMSG 0x51F.
COA_TEST(AttachFixes_Packets) {
    CoATest::RecordPackets();
    CoA::AttachFixes::SendGuidCleared(0x1122334455667788ull);
    CoA::AttachFixes::SendTamperReport("IsOutdoors");
    CoATest::StopRecordingPackets();
    CHECK(CoATest::SentPackets().size() == 2);
    CHECK(CoATest::SentPackets()[0] == CoATest::Expected(0x741, CoATest::Bytes().U64(0x1122334455667788ull)));
    CHECK(CoATest::SentPackets()[1] == CoATest::Expected(0x51F, CoATest::Bytes().Str("Ascension").Str("IsOutdoors").Str("External call to IsOutdoors. Hacker/botter.")));
}

// 0x720010: 170^2 in a dungeon or raid, 100^2 elsewhere, for say / yell / monster say; 25^2 for the rest.
COA_TEST(AttachFixes_ChatBubbleRange) {
    CoA::AttachFixes::SetChatBubbleRange(0xC, 1);
    CHECK(CoA::AttachFixes::ChatBubbleRangeSq() == 28900.0f);
    CoA::AttachFixes::SetChatBubbleRange(0x33, 0);
    CHECK(CoA::AttachFixes::ChatBubbleRangeSq() == 10000.0f);
    CoA::AttachFixes::SetChatBubbleRange(1, 2);
    CHECK(CoA::AttachFixes::ChatBubbleRangeSq() == 625.0f);
}

// A "$CN" name copy replaces the owner's previous copy instead of piling up (review fix: the token past position 0
// stays in the name, so every GetObjectName call makes a new copy).
COA_TEST(AttachFixes_KeptNameReplacesPrevious) {
    int ownerA = 0;
    int ownerB = 0;
    const size_t before = CoA::AttachFixes::KeptNameCount();
    const char* first = CoA::AttachFixes::KeepName(&ownerA, "Thr Portal of $CN");
    CHECK(std::string(first) == "Thr Portal of $CN");
    CoA::AttachFixes::KeepName(&ownerA, "Thr Portal of $CN");
    CoA::AttachFixes::KeepName(&ownerA, "Thr Portal of $CN");
    CHECK(CoA::AttachFixes::KeptNameCount() == before + 1);
    CoA::AttachFixes::KeepName(&ownerB, "Jai Portal of $CN");
    CHECK(CoA::AttachFixes::KeptNameCount() == before + 2);
}
