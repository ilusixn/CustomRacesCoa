#include "CoA/Tests/MysticEnchantFixture.h"

using namespace MysticFixture;

// SMSG 0x5F9 replaces the known list, 0x5FA adds (silent flag skips the event), 0x5FB removes.
COA_TEST(MysticEnchant_KnownPackets) {
    lua_State* L = Setup();
    CHECK(Feed(0x5F9, CoATest::Bytes().U32(2).U32(1001).U32(1002)).empty());
    CHECK(Run(L, "return C_MysticEnchant.HasAnyCollected()") == V{ "true" });
    CHECK(Feed(0x5FA, CoATest::Bytes().U32(1003).U8(0)) == V{ "MYSTIC_ENCHANT_LEARNED:1003" });
    CHECK(Feed(0x5FA, CoATest::Bytes().U32(1004).U8(1)).empty());
    CHECK(Feed(0x5FB, CoATest::Bytes().U32(1001)) == V{ "MYSTIC_ENCHANT_UNLEARNED:1001" });
    CHECK(CoA::MysticEnchant::Collected(1002));
    CHECK(!CoA::MysticEnchant::Collected(1001));
    Teardown(L);
}

// SMSG 0x5FD replaces the slots; 0x5FE sets one and fires only on a change; a slot past the list is ignored.
COA_TEST(MysticEnchant_SlotPackets) {
    lua_State* L = Setup();
    Feed(0x5FD, CoATest::Bytes().U32(3).U32(0).U32(1001).U32(0));
    CHECK(Feed(0x5FE, CoATest::Bytes().U32(2).U32(1002)) == V{ "MYSTIC_ENCHANT_SLOT_UPDATE:3" });
    CHECK(Feed(0x5FE, CoATest::Bytes().U32(2).U32(1002)).empty());
    CHECK(Feed(0x5FE, CoATest::Bytes().U32(9).U32(1002)).empty());
    CHECK(CoA::MysticEnchant::SlotEnchant(1) == 1001);
    CHECK(CoA::MysticEnchant::Slotted(1002));
    CHECK(Run(L, "return C_MysticEnchant.GetAppliedEnchant('player', 2)") == V{ "1001" });
    CHECK(Run(L, "return C_MysticEnchant.HasAnySlotEnchanted()") == V{ "true" });
    Teardown(L);
}

// Progress: (progress - need(level - 1)) / (need(level) - need(level - 1)) * 100; need(1) = 361, need(0) = 1.
COA_TEST(MysticEnchant_Progress) {
    lua_State* L = Setup();
    CHECK(Feed(0x619, CoATest::Bytes().U64(181).U32(1)) == V{ "MYSTIC_ENCHANT_PROGRESS_UPDATE:50,1" });
    CHECK((Run(L, "return C_MysticEnchant.GetProgress()") == V{ "50", "1" }));
    // A rare reforge result adds 80 (from the +0x0C quality column) and levels up: 181 + 80 = 261 < 361.
    CHECK(Feed(0x604, CoATest::Bytes().Str("RE_REFORGE_OK").U32(1001)) == V{ "MYSTIC_ENCHANT_REFORGE_RESULT:RE_REFORGE_OK,1001" });
    CHECK((Run(L, "return C_MysticEnchant.GetProgress()") == V{ "72.222222222222", "1" }));
    Feed(0x5FC, CoATest::Bytes().U64(300).U32(1));
    Feed(0x604, CoATest::Bytes().Str("RE_REFORGE_OK").U32(1002));   // epic: +100 -> 400 >= 361, level 2
    CHECK(Run(L, "local _, l = C_MysticEnchant.GetProgress() return l") == V{ "2" });
    Teardown(L);
}

// The progress auras multiply the gain: (points + 1) / 100 + 1 each.
COA_TEST(MysticEnchant_ProgressAuras) {
    lua_State* L = Setup();
    s_auraPoints = 49;   // x1.5
    Feed(0x5FC, CoATest::Bytes().U64(1).U32(1));
    Feed(0x604, CoATest::Bytes().Str("RE_REFORGE_OK").U32(1001));   // 80 * 1.5 * 1.5 = 180
    CHECK((Run(L, "return C_MysticEnchant.GetProgress()") == V{ "50", "1" }));
    Teardown(L);
}

// GetEnchantInfoBySpell / ByItem: the info table fields, the class requirements grouped by class, the spec tags.
COA_TEST(MysticEnchant_EnchantInfo) {
    lua_State* L = Setup();
    Feed(0x5F9, CoATest::Bytes().U32(1).U32(1002));
    CHECK((Run(L, "local t = C_MysticEnchant.GetEnchantInfoBySpell(1002) return t.SpellID, t.SpellName, t.MaxStacks, "
                  "t.Quality, t.RequiredLevel, t.ItemID, t.Known, t.IsWorldforged, t.IsAvailableForCurrentClass") ==
        V{ "1002", "Spell1002", "2", "RE_QUALITY_EPIC", "60", "5002", "true", "true", "false" }));
    CHECK((Run(L, "local r = C_MysticEnchant.GetEnchantInfoBySpell(1002).ClassRequirements return #r, r[1].ClassType, "
                  "#r[1].TabTypes, r[1].TabTypes[1].Tab, r[1].TabTypes[1].RequiredAE, r[1].TabTypes[2].Tab, "
                  "r[1].TabTypes[2].RequiredTE") ==
        V{ "1", "Warrior", "2", "Protection", "5", "Arms", "2" }));
    CHECK((Run(L, "local t = C_MysticEnchant.GetEnchantInfoByItem(5001) return t.SpellID, t.Quality, #t.Spec, t.Spec[1], "
                  "#t.ClassRequirements, t.IsAvailableForCurrentClass") ==
        V{ "1001", "RE_QUALITY_RARE", "1", "FIRE", "0", "true" }));
    CHECK(Run(L, "return C_MysticEnchant.GetEnchantInfoBySpell(4242)") == V{ "nil" });
    CHECK(Run(L, "return C_MysticEnchant.GetEnchantInfoBySpell(0)").empty());
    Teardown(L);
}

// A stock-class player reads the "reborn" quality column (+0x10).
COA_TEST(MysticEnchant_RebornQuality) {
    lua_State* L = Setup();
    s_class = 1;
    CHECK(Run(L, "return C_MysticEnchant.GetEnchantInfoBySpell(1001).Quality") == V{ "RE_QUALITY_UNCOMMON" });
    Teardown(L);
}

// QueryEnchants: rows without a spell record or not allowed on the realm drop out; best quality first; paging and
// RE_FILTER arguments; an empty table outside the world.
COA_TEST(MysticEnchant_QueryEnchants) {
    lua_State* L = Setup();
    CHECK((Run(L, "local t, n = C_MysticEnchant.QueryEnchants(1, 1, '', {}) return #t, t[1].SpellID, n") == V{ "1", "1002", "2" }));
    CHECK((Run(L, "local t, n = C_MysticEnchant.QueryEnchants(1, 2, '', {}) return #t, t[1].SpellID, n") == V{ "1", "1001", "2" }));
    CHECK((Run(L, "local t, n = C_MysticEnchant.QueryEnchants(10, 1, '', { RE_FILTER_RARE = true }) return #t, t[1].SpellID, n") ==
        V{ "1", "1001", "1" }));
    CHECK((Run(L, "local t, n = C_MysticEnchant.QueryEnchants(10, 1, 'spell1002', {}) return #t, n") == V{ "1", "1" }));
    s_inWorld = false;
    CHECK((Run(L, "local t, n = C_MysticEnchant.QueryEnchants(10, 1, '', {}) return #t, n") == V{ "0", "nil" }));
    Teardown(L);
}

// SMSG 0x56B adds a row (strings appended), rebuilds both indexes and fires MYSTIC_ENCHANT_PATCHED.
COA_TEST(MysticEnchant_PatchAddsRow) {
    lua_State* L = Setup();
    CoATest::Bytes b;
    std::vector<uint32_t> row = Row(40, 1040, 0, 0, 20, 5040, 0, 1);
    for (uint32_t v : row) {
        b.U32(v);
    }
    b.Str("RE_QUALITY_LEGENDARY").Str("RE_QUALITY_LEGENDARY").Str("ARCANE").Str("NONE").Str("").Str("");
    s_patchedSpells = { 1040 };
    CHECK(Feed(0x56B, b) == V{ "MYSTIC_ENCHANT_PATCHED" });
    CHECK((Run(L, "local t = C_MysticEnchant.GetEnchantInfoByItem(5040) return t.SpellID, t.Quality, t.Spec[1]") ==
        V{ "1040", "RE_QUALITY_LEGENDARY", "ARCANE" }));
    // The new legendary row sorts first.
    CHECK((Run(L, "local t, n = C_MysticEnchant.QueryEnchants(10, 1, '', {}) return n, #t, t[1].SpellID, t[2].SpellID, t[3].SpellID") ==
        V{ "1", "3", "1040", "1002", "1001" }));
    Teardown(L);
}

// Inspect: an unknown result sends CMSG 0x613 with the guid; SMSG 0x614 stores it under the guid's counter and the
// next Inspect returns it; GetAppliedEnchant reads it for that unit. Without a cached result the original pushes
// (true, nil) or (false, nil) but returns 1, so Lua sees only the nil.
COA_TEST(MysticEnchant_Inspect) {
    lua_State* L = Setup();
    CoATest::RecordPackets();
    CHECK((Run(L, "return C_MysticEnchant.Inspect('target', false)") == V{ "nil" }));
    CoATest::StopRecordingPackets();
    CHECK(CoATest::SentPackets().size() == 1 && CoATest::SentPackets()[0] == CoATest::Expected(0x613, CoATest::Bytes().U64(kTarget)));
    CHECK(Feed(0x614, CoATest::Bytes().Str("RE_INSPECT_OK").U64(181).U32(1).U32(2).U32(1001).U32(0).U32(1).U32(1002)) ==
        V{ "MYSTIC_ENCHANT_INSPECT_RESULT:RE_INSPECT_OK" });
    CHECK((Run(L, "local ok, t = C_MysticEnchant.Inspect('target', false) return ok, t.Level, t.Progress, #t.AppliedEnchants, "
                  "t.KnownEnchants[1]") == V{ "true", "1", "0.5", "2", "1002" }));
    CHECK(Run(L, "return C_MysticEnchant.GetAppliedEnchant('target', 1)") == V{ "1001" });
    CHECK((Run(L, "return C_MysticEnchant.Inspect('nobody', false)") == V{ "nil" }));
    Teardown(L);
}

// The glue screen empties the manager.
COA_TEST(MysticEnchant_GlueResets) {
    lua_State* L = Setup();
    Feed(0x5F9, CoATest::Bytes().U32(1).U32(1001));
    Feed(0x5FD, CoATest::Bytes().U32(1).U32(1001));
    Feed(0x619, CoATest::Bytes().U64(181).U32(1));
    CoA::Lifecycle::RunGlueScreen();
    CHECK(Run(L, "return C_MysticEnchant.HasAnyCollected()") == V{ "false" });
    CHECK(Run(L, "return C_MysticEnchant.HasAnySlotEnchanted()") == V{ "false" });
    CHECK(Run(L, "local _, l = C_MysticEnchant.GetProgress() return l") == V{ "0" });
    Teardown(L);
}

// The altar: a used altar object opens it, a used scroll / preset token fires its event.
COA_TEST(MysticEnchant_AltarAndItemUse) {
    lua_State* L = Setup();
    CoATest::RecordSignals();
    CoA::MysticEnchant::OnGameObjectUse(0x13914);
    CoA::MysticEnchant::OnGameObjectUse(12);
    CoA::MysticEnchant::OnItemUse(0xF25D0);
    CoA::MysticEnchant::OnItemUse(0x1B9271);
    CoATest::StopRecordingSignals();
    CHECK((CoATest::Signals() == V{ "MYSTIC_ALTAR_USED", "MYSTIC_SCROLL_USED:992720", "MYSTIC_ENCHANT_UNLOCK_PRESET_USED" }));
    Teardown(L);
}
