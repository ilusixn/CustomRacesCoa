#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/MysticEnchantPreset.h"
#include "CoA/Tests/MysticEnchantFixture.h"

using namespace MysticFixture;

namespace {
    // A patch packet: the row's dwords, then its two quality names and four tag names.
    CoATest::Bytes Patch(const std::vector<uint32_t>& row, const char* quality) {
        CoATest::Bytes b;

        for (uint32_t v : row) {
            b.U32(v);
        }

        b.Str(quality).Str(quality).Str("NONE").Str("").Str("").Str("");
        return b;
    }

    const char* kListed = "local n = 0 for _, e in ipairs((C_MysticEnchant.QueryEnchants(100, 1, '', {}))) do "
                          "if e.SpellID == %u then n = n + 1 end end return n";

    std::string Listed(uint32_t spell) {
        char chunk[256];
        snprintf(chunk, sizeof(chunk), kListed, spell);
        return chunk;
    }
}

// SMSG 0x56B: a new id resets the populated container and is found by spell and by item; patching that id again (now
// not allowed on the realm) keeps the cached list; the glue screen repopulates it.
COA_TEST(MysticEnchant_PatchResetsContainerOnlyForNewIds) {
    lua_State* L = Setup();
    s_patchedSpells = { 1041 };
    CHECK(Run(L, Listed(1001).c_str()) == V{ "1" });   // populated before the patch
    CHECK(Run(L, Listed(1041).c_str()) == V{ "0" });
    Feed(0x56B, Patch(Row(41, 1041, 0, 0, 5, 5041, 0, 1), "RE_QUALITY_RARE"));
    CHECK(Run(L, Listed(1041).c_str()) == V{ "1" });
    CHECK(Run(L, "return C_MysticEnchant.GetEnchantInfoBySpell(1041).ItemID") == V{ "5041" });
    CHECK(Run(L, "return C_MysticEnchant.GetEnchantInfoByItem(5041).SpellID") == V{ "1041" });
    Feed(0x56B, Patch(Row(41, 1041, 0, 0, 5, 5041, 0, 0), "RE_QUALITY_RARE"));   // existing id, realm flag cleared
    CHECK(Run(L, Listed(1041).c_str()) == V{ "1" });
    CoA::Lifecycle::RunGlueScreen();
    CHECK(Run(L, Listed(1041).c_str()) == V{ "0" });
    Teardown(L);
}

// An enum value past its table reads UNEXPECTED_ENUM_VALUE_<n>.
COA_TEST(MysticEnchant_UnexpectedEnumValue) {
    lua_State* L = Setup();
    std::vector<uint32_t> row = Row(43, 1043, 0, 0, 5, 5043, 0, 1);
    row[19] = 99;
    row[22] = 120;
    Feed(0x56B, Patch(row, "RE_QUALITY_EPIC"));
    CHECK((Run(L, "local r = C_MysticEnchant.GetEnchantInfoBySpell(1043).ClassRequirements[1] return r.ClassType, r.TabTypes[1].Tab") ==
        V{ "UNEXPECTED_ENUM_VALUE_99", "UNEXPECTED_ENUM_VALUE_120" }));
    Teardown(L);
}

// Without a player: no crash, and the player-dependent checks fall through as the original's null player does.
COA_TEST(MysticEnchant_NoPlayer) {
    lua_State* L = Setup();
    s_hasPlayer = false;
    CHECK(Run(L, "return #C_MysticEnchant.GetMysticScrolls()") == V{ "0" });
    CHECK((Run(L, "return C_MysticEnchant.CanReforgeItem(7)") == V{ "false", "RE_REFORGE_BAD_ITEM" }));
    CHECK(Run(L, "return C_MysticEnchant.HasNearbyMysticAltar()") == V{ "false" });
    CHECK(Run(L, "return C_MysticEnchant.GetAppliedEnchant('player', 1)") == V{ "0" });
    CHECK((Run(L, "return C_MysticEnchant.CanPurchaseMysticScroll()") == V{ "false", "RE_PURCHASE_NOT_ENOUGH_SPACE" }));
    CHECK(Run(L, "return C_MysticEnchant.GetEnchantInfoBySpell(1001).IsAvailableForCurrentClass") == V{ "false" });
    CHECK(Run(L, "local t = C_MysticEnchant.QueryEnchants(10, 1, '', {}) return #t > 0") == V{ "true" });
    Teardown(L);
}

// Short packets: a count larger than the bytes left leaves the list as it was; a truncated patch adds nothing.
COA_TEST(MysticEnchant_ShortPackets) {
    lua_State* L = Setup();
    Feed(0x5F9, CoATest::Bytes().U32(1).U32(1001));
    Feed(0x5F9, CoATest::Bytes().U32(5).U32(1002));
    CHECK(CoA::MysticEnchant::Collected(1001));
    CHECK(!CoA::MysticEnchant::Collected(1002));
    Feed(0x5FD, CoATest::Bytes().U32(0x40000000));
    CHECK(Run(L, "return C_MysticEnchant.HasAnySlotEnchanted()") == V{ "false" });
    CHECK(Feed(0x56B, CoATest::Bytes().U32(44).U32(1044)).empty());
    CHECK(Run(L, "return C_MysticEnchant.GetEnchantInfoBySpell(1044)") == V{ "nil" });
    CHECK(Feed(0x614, CoATest::Bytes().Str("RE_INSPECT_OK").U64(1)) == V{ "MYSTIC_ENCHANT_INSPECT_RESULT:RE_INSPECT_OK" });
    Teardown(L);
}

// The glue screen also drops the pending applies and reforges and the inspect results.
COA_TEST(MysticEnchant_GlueDropsPendingAndInspect) {
    lua_State* L = Setup();
    CoA::MysticEnchant::Item item;
    item.low = 7;
    item.entry = 5001;
    s_items = { item };
    Run(L, "C_MysticEnchant.ApplySlot(1, 7) C_MysticEnchant.CollectionReforgeSlot(2, 1001) C_MysticEnchant.Inspect('target', false)");
    Feed(0x614, CoATest::Bytes().Str("RE_INSPECT_OK").U64(1).U32(1).U32(1).U32(1001).U32(0));
    CHECK(Run(L, "return C_MysticEnchant.GetAppliedEnchant('target', 1)") == V{ "1001" });
    CoA::Lifecycle::RunGlueScreen();
    CHECK(Run(L, "return #C_MysticEnchant.GetApplyChanges()") == V{ "0" });
    CHECK(Run(L, "return #C_MysticEnchant.GetCollectionReforgeChanges()") == V{ "0" });
    CHECK(Run(L, "return C_MysticEnchant.GetAppliedEnchant('target', 1)") == V{ "0" });
    Teardown(L);
}

// Progress at level 0 divides by zero (+inf through the unsigned conversion); past level 249 the linear formula.
COA_TEST(MysticEnchant_ProgressEdges) {
    lua_State* L = Setup();
    Feed(0x619, CoATest::Bytes().U64(0).U32(0));
    CHECK((Run(L, "local p, l = C_MysticEnchant.GetProgress() return p > 1e308, l") == V{ "true", "0" }));
    Feed(0x619, CoATest::Bytes().U64(555201).U32(250));
    CHECK((Run(L, "return C_MysticEnchant.GetProgress()") == V{ "49.987795948255", "250" }));
    Teardown(L);
}

// The preset glue reset also sets the pending activation to 0 (FUN_102F2230), so a later OK result activates preset 1.
COA_TEST(MysticEnchantPreset_GlueResetsPending) {
    lua_close(Setup());
    CoA::MysticEnchantPreset::Reset();
    CoA::MysticEnchantPreset::Init();
    lua_State* L = CoATest::NewState(false);
    Feed(0x5FF, CoATest::Bytes().U32(2).U32(0).U32(0));
    Run(L, "C_MysticEnchantPreset.Activate(2)");
    CoA::Lifecycle::RunGlueScreen();
    Feed(0x5FF, CoATest::Bytes().U32(2).U32(0).U32(0));
    Feed(0x60D, CoATest::Bytes().Str("RE_PRESET_SET_ACTIVE_OK"));
    CHECK(Run(L, "local _, a = C_MysticEnchantPreset.GetPresetData(1) return a") == V{ "true" });
    Teardown(L);
}
