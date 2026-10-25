#include "CoA/Tests/MysticEnchantFixture.h"

using namespace MysticFixture;

namespace {
    CoA::MysticEnchant::Item MakeItem(uint32_t low, uint32_t entry, uint8_t bag, uint8_t slot, uint32_t count = 1) {
        CoA::MysticEnchant::Item i;
        i.low = low;
        i.entry = entry;
        i.count = count;
        i.bag = bag;
        i.slot = slot;
        return i;
    }
}

// The configured costs at their defaults (no CONFIG_* values received).
COA_TEST(MysticEnchant_Costs) {
    lua_State* L = Setup();
    CHECK((Run(L, "return C_MysticEnchant.GetReforgeCost()") == V{ "250", "25000" }));
    CHECK((Run(L, "return C_MysticEnchant.GetCollectionReforgeSlotCost(1001)") == V{ "2400", "600000" }));
    CHECK((Run(L, "return C_MysticEnchant.GetCollectionReforgeItemCost(1001)") == V{ "4294967295", "600000" }));
    CHECK((Run(L, "return C_MysticEnchant.GetDisenchantCost(1001, true)") == V{ "5000", "1000" }));
    CHECK((Run(L, "return C_MysticEnchant.GetDisenchantCost(1001, false)") == V{ "5000", "nil" }));
    CHECK((Run(L, "return C_MysticEnchant.GetCollectionReforgeSlotCost(4242)") == V{ "nil", "nil" }));
    Teardown(L);
}

// CanReforgeItem: the first error of the original's list; the gates run after the item checks.
COA_TEST(MysticEnchant_CanReforgeItem) {
    lua_State* L = Setup();
    s_items = { MakeItem(7, kScroll, 0, 3), MakeItem(8, 5002, 1, 2) };
    CHECK((Run(L, "return C_MysticEnchant.CanReforgeItem(99)") == V{ "false", "RE_REFORGE_BAD_ITEM" }));
    CHECK((Run(L, "return C_MysticEnchant.CanReforgeItem(8)") == V{ "false", "RE_REFORGE_WORLDFORGED_SCROLL" }));
    CHECK((Run(L, "return C_MysticEnchant.CanReforgeItem(7)") == V{ "false", "RE_REFORGE_NO_MONEY" }));
    s_money = 30000;
    CHECK((Run(L, "return C_MysticEnchant.CanReforgeItem(7)") == V{ "false", "RE_REFORGE_BAD_CLASS" }));   // a CoA class
    s_class = 1;
    CHECK((Run(L, "return C_MysticEnchant.CanReforgeItem(7)") == V{ "true", "nil" }));
    s_altar = false;
    CHECK((Run(L, "return C_MysticEnchant.CanReforgeItem(7)") == V{ "false", "RE_REFORGE_NO_MYSTIC_ALTAR" }));
    Teardown(L);
}

// The item-addressed CMSGs: backpack slot n -> bag 0xFF, slot n + 0x16; bag b slot n -> b + 0x12, n - 1.
COA_TEST(MysticEnchant_ItemPacketsUseWireSlots) {
    lua_State* L = Setup();
    s_items = { MakeItem(7, kScroll, 0, 3), MakeItem(8, 5001, 2, 5) };
    auto a = Sent(L, "return C_MysticEnchant.ReforgeItem(7)");
    CHECK(a.size() == 1 && a[0] == CoATest::Expected(0x603, CoATest::Bytes().U8(0xFF).U8(0x19)));
    auto b = Sent(L, "return C_MysticEnchant.CollectionReforgeItem(8, 1002)");
    CHECK(b.size() == 1 && b[0] == CoATest::Expected(0x605, CoATest::Bytes().U8(0x14).U8(4).U32(1002)));
    auto c = Sent(L, "return C_MysticEnchant.DisenchantItem(8, true)");
    CHECK(c.size() == 1 && c[0] == CoATest::Expected(0x607, CoATest::Bytes().U8(1).U8(0x14).U8(4)));
    auto d = Sent(L, "return C_MysticEnchant.ApplyItem(3, 8)");
    CHECK(d.size() == 1 && d[0] == CoATest::Expected(0x60A, CoATest::Bytes().U32(2).U8(0x14).U8(4)));
    V r;
    CHECK(Sent(L, "return C_MysticEnchant.ReforgeItem(99)", &r).empty());
    CHECK(r == V{ "false" });
    auto e = Sent(L, "return C_MysticEnchant.ReforgeSlot(4), C_MysticEnchant.DisenchantSlot(2, false), C_MysticEnchant.Destroy(1)");
    CHECK(e.size() == 3);
    CHECK(e.size() == 3 && e[0] == CoATest::Expected(0x60E, CoATest::Bytes().U32(3)));
    CHECK(e.size() == 3 && e[1] == CoATest::Expected(0x608, CoATest::Bytes().U8(0).U32(1)));
    CHECK(e.size() == 3 && e[2] == CoATest::Expected(0x617, CoATest::Bytes().U32(0)));
    Teardown(L);
}

// CanApplySlot: stack limit against the slots, then the AE / TE investment and class checks in the original's order.
COA_TEST(MysticEnchant_CanApplySlot) {
    lua_State* L = Setup();
    s_items = { MakeItem(7, 5001, 0, 1), MakeItem(8, 5002, 0, 2), MakeItem(9, 777, 0, 3) };
    CHECK((Run(L, "return C_MysticEnchant.CanApplySlot(1, 7)") == V{ "true", "nil" }));
    Feed(0x5FD, CoATest::Bytes().U32(2).U32(0).U32(1001));
    CHECK((Run(L, "return C_MysticEnchant.CanApplySlot(1, 7)") == V{ "false", "RE_APPLY_STACK_LIMIT" }));
    CHECK((Run(L, "return C_MysticEnchant.CanApplySlot(1, 8)") == V{ "false", "RE_APPLY_REQUIRED_AE_INVESTMENT" }));
    s_investment = 10;
    CHECK((Run(L, "return C_MysticEnchant.CanApplySlot(1, 8)") == V{ "false", "RE_APPLY_BAD_CLASS" }));
    CHECK((Run(L, "return C_MysticEnchant.CanApplySlot(1, 9)") == V{ "false", "RE_APPLY_NOT_MYSTIC_SCROLL" }));
    CHECK((Run(L, "return C_MysticEnchant.CanApplySlot(18, 7)") == V{ "false", "RE_APPLY_BAD_SLOT" }));
    s_casting = true;
    CHECK((Run(L, "return C_MysticEnchant.CanApplySlot(3, 7)") == V{ "false", "RE_APPLY_STACK_LIMIT" }));
    CHECK((Run(L, "local ok, e = C_MysticEnchant.CanApplyAnySlot(7) return ok, e[1][1], e[1][2]") ==
        V{ "false", "RE_APPLY_STACK_LIMIT", "RE_APPLY_NOT_WHILE_CASTING" }));
    Teardown(L);
}

// The pending applies: ApplySlot upserts by slot, GetApplyChanges lists them, SaveApply sends 0x610, an OK result clears.
COA_TEST(MysticEnchant_PendingApply) {
    lua_State* L = Setup();
    s_items = { MakeItem(7, 5001, 0, 1), MakeItem(8, 5002, 1, 4) };
    Run(L, "C_MysticEnchant.ApplySlot(2, 7) C_MysticEnchant.ApplySlot(2, 8) C_MysticEnchant.ApplySlot(5, 7) C_MysticEnchant.ApplySlot(6, 99)");
    CHECK((Run(L, "local t = C_MysticEnchant.GetApplyChanges() return #t, t[1].RESlot, t[1].Enchant.SpellID, t[2].RESlot") ==
        V{ "2", "2", "1002", "5" }));
    V r;
    auto s = Sent(L, "return C_MysticEnchant.SaveApply()", &r);
    CHECK(r == V{ "true" });
    CHECK(s.size() == 1 && s[0] == CoATest::Expected(0x610, CoATest::Bytes().U32(2).U32(1).U8(0x13).U8(3).U32(4).U8(0xFF).U8(0x17)));
    CHECK(Feed(0x60B, CoATest::Bytes().Str("RE_APPLY_BAD_SLOT")) == V{ "MYSTIC_ENCHANT_APPLY_RESULT:RE_APPLY_BAD_SLOT" });
    CHECK(Run(L, "return #C_MysticEnchant.GetApplyChanges()") == V{ "2" });
    Run(L, "C_MysticEnchant.UndoLastApply()");
    CHECK(Run(L, "return #C_MysticEnchant.GetApplyChanges()") == V{ "1" });
    Feed(0x60B, CoATest::Bytes().Str("RE_APPLY_OK"));
    CHECK(Run(L, "return #C_MysticEnchant.GetApplyChanges()") == V{ "0" });
    CHECK(Run(L, "return C_MysticEnchant.SaveApply()") == V{ "false" });
    Teardown(L);
}

// The pending collection reforges: the checks, the cost deltas on NO_MONEY, and the save packet (which returns no
// values although it pushes true, as the original).
COA_TEST(MysticEnchant_CollectionReforgeSlot) {
    lua_State* L = Setup();
    CHECK((Run(L, "return C_MysticEnchant.CanCollectionReforgeSlot(1, 1001)") ==
        V{ "false", "RE_COLLECTION_REFORGE_NOT_KNOWN_ENCHANTMENT", "nil", "nil", "nil", "nil" }));
    Feed(0x5F9, CoATest::Bytes().U32(1).U32(1001));
    CHECK((Run(L, "return C_MysticEnchant.CanCollectionReforgeSlot(1, 1001)") ==
        V{ "false", "RE_COLLECTION_REFORGE_NO_MONEY", "0", "0", "0", "0" }));
    s_tokens[kMark] = 2400;
    CHECK((Run(L, "return C_MysticEnchant.CanCollectionReforgeSlot(1, 1001)") == V{ "true", "nil", "nil", "nil", "nil", "nil" }));
    Run(L, "C_MysticEnchant.CollectionReforgeSlot(1, 1001)");
    CHECK((Run(L, "return C_MysticEnchant.GetSaveCollectionReforgeSlotCost()") == V{ "0", "2400" }));
    CHECK((Run(L, "return C_MysticEnchant.CanSaveCollectionReforge()") == V{ "true", "nil" }));
    V r;
    auto s = Sent(L, "return C_MysticEnchant.SaveCollectionReforge()", &r);
    CHECK(r.empty());
    CHECK(s.size() == 1 && s[0] == CoATest::Expected(0x60F, CoATest::Bytes().U32(1).U32(0).U32(1001)));
    CHECK(Feed(0x606, CoATest::Bytes().Str("RE_COLLECTION_REFORGE_OK")) ==
        V{ "MYSTIC_ENCHANT_COLLECTION_REFORGE_RESULT:RE_COLLECTION_REFORGE_OK" });
    CHECK(Run(L, "return #C_MysticEnchant.GetCollectionReforgeChanges()") == V{ "0" });
    Teardown(L);
}

// Destroy, purchase, equip and disenchant checks.
COA_TEST(MysticEnchant_OtherChecks) {
    lua_State* L = Setup();
    s_items = { MakeItem(7, 5001, 0, 1) };
    CHECK((Run(L, "return C_MysticEnchant.CanDestroy(1)") == V{ "false", "RE_DESTROY_NO_ENCHANT_APPLIED" }));
    CHECK((Run(L, "return C_MysticEnchant.CanPurchaseMysticScroll()") == V{ "false", "RE_PURCHASE_NOT_ENOUGH_MONEY" }));
    s_money = 1000;
    CHECK((Run(L, "return C_MysticEnchant.CanPurchaseMysticScroll()") == V{ "false", "RE_PURCHASE_BAD_CLASS" }));
    CHECK((Run(L, "return C_MysticEnchant.GetMysticScrollCost()") == V{ "1000" }));
    CHECK((Run(L, "return C_MysticEnchant.CanEquipEnchant(1002)") == V{ "false", "RE_EQUIP_BAD_CLASS" }));
    CHECK((Run(L, "return C_MysticEnchant.CanEquipEnchant(1001)") == V{ "true", "nil" }));
    CHECK((Run(L, "return C_MysticEnchant.CanEquipEnchant(4242)") == V{ "false", "RE_EQUIP_BAD_ENCHANTMENT" }));
    CHECK((Run(L, "return C_MysticEnchant.CanDisenchantItem(7, false)") == V{ "false", "RE_DISENCHANT_NO_MONEY" }));
    CHECK((Run(L, "return C_MysticEnchant.CanDisenchantItem(7, true)") == V{ "true", "nil" }));
    s_tokens[kExtract] = 1;
    CHECK((Run(L, "return C_MysticEnchant.CanDisenchantItem(7, true)") ==
        V{ "false", "RE_PURCHASE_MYSTIC_EXTRACT_ALREADY_OBTAINED", "1000" }));
    CHECK((Run(L, "return C_MysticEnchant.CanPurchaseMysticExtract()") == V{ "false", "RE_PURCHASE_MYSTIC_EXTRACT_ALREADY_OBTAINED", "1000" }));
    Teardown(L);
}

// The scroll filter: the untarnished scroll is kept without an item record (named by the client), once.
COA_TEST(MysticEnchant_ScrollFilter) {
    lua_State* L = Setup();
    s_items = { MakeItem(7, kScroll, 0, 2), MakeItem(8, kScroll, 1, 1), MakeItem(9, 5001, 0, 1) };
    Run(L, "C_MysticEnchant.SetMysticScrollFilter('', {})");
    CHECK(Run(L, "return C_MysticEnchant.GetNumFilteredMysticScrolls()") == V{ "1" });
    CHECK((Run(L, "local s = C_MysticEnchant.GetFilteredMysticScrollAtIndex(1) return s.Guid, s.Entry, s.Name, s.Bag, s.Slot") ==
        V{ "7", "992720", "Item7", "0", "2" }));
    CHECK(Run(L, "return C_MysticEnchant.GetFilteredMysticScrollAtIndex(2)") == V{ "nil" });
    CHECK(Run(L, "return #C_MysticEnchant.GetFilteredMysticScrolls(0, 5)") == V{ "1" });
    Run(L, "C_MysticEnchant.SetMysticScrollFilter('nothing', {})");
    CHECK(Run(L, "return C_MysticEnchant.GetNumFilteredMysticScrolls()") == V{ "0" });
    Teardown(L);
}
