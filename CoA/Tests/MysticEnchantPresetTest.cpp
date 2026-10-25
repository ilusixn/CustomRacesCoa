#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/MysticEnchantPreset.h"
#include "CoA/Tests/MysticEnchantFixture.h"

using namespace MysticFixture;

namespace {
    lua_State* PresetSetup() {
        lua_close(Setup());
        CoA::MysticEnchantPreset::Reset();
        CoA::MysticEnchantPreset::Init();
        return CoATest::NewState(false);
    }

    CoATest::Bytes Presets() {
        return CoATest::Bytes().U32(2).U32(2).U32(1001).U32(1002).U32(0);
    }
}

// SMSG 0x5FF replaces the presets, 0x600 sets the active one; GetPresetData returns the spells and the active flag.
COA_TEST(MysticEnchantPreset_Data) {
    lua_State* L = PresetSetup();
    Feed(0x5FF, Presets());
    Feed(0x600, CoATest::Bytes().U32(1));
    CHECK(Run(L, "return C_MysticEnchantPreset.GetNumPresets()") == V{ "2" });
    CHECK((Run(L, "local t, a = C_MysticEnchantPreset.GetPresetData(1) return #t, t[2], a") == V{ "2", "1002", "false" }));
    CHECK((Run(L, "local t, a = C_MysticEnchantPreset.GetPresetData(2) return #t, a") == V{ "0", "true" }));
    CHECK((Run(L, "local t, a = C_MysticEnchantPreset.GetPresetData(9) return #t, a") == V{ "0", "false" }));
    CHECK(Run(L, "return C_MysticEnchantPreset.GetPresetData(0)").empty());
    Teardown(L);
}

// Activate sends CMSG 0x60C and remembers the index; an OK SMSG 0x60D makes it active, any result clears the wait.
COA_TEST(MysticEnchantPreset_Activate) {
    lua_State* L = PresetSetup();
    Feed(0x5FF, Presets());
    V r;
    auto s = Sent(L, "return C_MysticEnchantPreset.Activate(2)", &r);
    CHECK(r == V{ "true" });
    CHECK(s.size() == 1 && s[0] == CoATest::Expected(0x60C, CoATest::Bytes().U32(1)));
    CHECK(Feed(0x60D, CoATest::Bytes().Str("RE_PRESET_SET_ACTIVE_OK")) == V{ "MYSTIC_ENCHANT_PRESET_SET_ACTIVE_RESULT:RE_PRESET_SET_ACTIVE_OK" });
    CHECK(Run(L, "local _, a = C_MysticEnchantPreset.GetPresetData(2) return a") == V{ "true" });
    Run(L, "C_MysticEnchantPreset.Activate(1)");
    Feed(0x60D, CoATest::Bytes().Str("RE_PRESET_SET_ACTIVE_BUILD_DRAFT"));
    CHECK(Run(L, "local _, a = C_MysticEnchantPreset.GetPresetData(2) return a") == V{ "true" });
    Teardown(L);
}

// An OK save result copies the slot enchants into the active preset; an OK unlock result adds an empty preset.
COA_TEST(MysticEnchantPreset_SaveAndUnlockResults) {
    lua_State* L = PresetSetup();
    Feed(0x5FF, Presets());
    Feed(0x600, CoATest::Bytes().U32(1));
    Feed(0x5FD, CoATest::Bytes().U32(3).U32(1001).U32(0).U32(1002));
    CHECK(Feed(0x602, CoATest::Bytes().Str("RE_PRESET_SAVE_OK")) == V{ "MYSTIC_ENCHANT_PRESET_SAVE_RESULT:RE_PRESET_SAVE_OK" });
    CHECK((Run(L, "local t = C_MysticEnchantPreset.GetPresetData(2) return #t, t[1], t[3]") == V{ "3", "1001", "1002" }));
    Feed(0x616, CoATest::Bytes().Str("RE_PRESET_UNLOCK_OK"));
    Feed(0x616, CoATest::Bytes().Str("RE_PRESET_UNLOCK_NO_MONEY"));
    CHECK(Run(L, "return C_MysticEnchantPreset.GetNumPresets()") == V{ "3" });
    auto s = Sent(L, "return C_MysticEnchantPreset.Unlock()");
    CHECK(s.size() == 1 && s[0] == CoATest::Expected(0x615, CoATest::Bytes()));
    Teardown(L);
}

// The checks return every error as a list; the unlock token counts only in the backpack and bags.
COA_TEST(MysticEnchantPreset_Checks) {
    lua_State* L = PresetSetup();
    CHECK((Run(L, "local ok, e = C_MysticEnchantPreset.CanUnlock() return ok, #e, e[1], e[2]") ==
        V{ "false", "2", "RE_PRESET_UNLOCK_NO_MONEY", "RE_PRESET_UNLOCK_BAD_CLASS" }));
    s_class = 1;
    CoA::MysticEnchant::Item equipped;
    equipped.low = 5;
    equipped.entry = 0x1B9271;
    equipped.count = 1;
    s_items = { equipped };   // not in a bag: does not count
    CHECK((Run(L, "local ok, e = C_MysticEnchantPreset.CanUnlock() return ok, e[1]") == V{ "false", "RE_PRESET_UNLOCK_NO_MONEY" }));
    s_items[0].slot = 4;
    CHECK((Run(L, "return C_MysticEnchantPreset.CanUnlock()") == V{ "true", "nil" }));
    s_casting = true;
    CHECK((Run(L, "local ok, e = C_MysticEnchantPreset.CanActivate(1) return ok, e[1]") == V{ "false", "RE_PRESET_SET_ACTIVE_NOT_WHILE_CASTING" }));
    CHECK((Run(L, "return C_MysticEnchantPreset.CanSave(1)") == V{ "true", "nil" }));
    CHECK(Run(L, "return C_MysticEnchantPreset.CanSave(0)").empty());
    Teardown(L);
}

// The glue screen: no presets, active -1, nothing pending.
COA_TEST(MysticEnchantPreset_GlueResets) {
    lua_State* L = PresetSetup();
    Feed(0x5FF, Presets());
    Feed(0x600, CoATest::Bytes().U32(0));
    CoA::Lifecycle::RunGlueScreen();
    CHECK(Run(L, "return C_MysticEnchantPreset.GetNumPresets()") == V{ "0" });
    Feed(0x5FF, Presets());
    CHECK(Run(L, "local _, a = C_MysticEnchantPreset.GetPresetData(1) return a") == V{ "false" });
    Teardown(L);
}
