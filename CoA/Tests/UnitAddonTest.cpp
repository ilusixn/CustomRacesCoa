#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/ObjectAddon.h"
#include "CoA/Source/Modules/UnitAddon.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <cstring>
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;

    // SpellAlternativePowerType.dbc: id, int, max.
    bool ReadPowerTypes(const char* path, std::vector<uint8_t>& out) {
        if (strstr(path, "SpellAlternativePowerType.dbc") == nullptr) {
            return false;
        }

        out = CoATest::Wdbc({ { 4, 0, 250 } }, "");
        return true;
    }

    lua_State* Setup() {
        CoA::LuaReg::Reset();
        CoA::ObjectAddon::Reset();
        CoA::Dbc::SetFileReader(ReadPowerTypes);
        CoA::UnitAddon::Init();
        CoA::UnitAddon::Deps& deps = CoA::UnitAddon::Dependencies();
        deps.tokenGuid = [](const char* token, uint64_t& guid) {
            guid = 0;

            if (strcmp(token, "player") == 0) {
                guid = 0x11;
                return true;
            }

            return false;
        };
        deps.activePlayerGuid = [] { return static_cast<uint64_t>(0x11); };
        return CoATest::NewState(false);
    }

    void SetField(uint64_t guid, uint32_t index, uint32_t value) {
        CoA::ObjectAddon::Unit(guid)[index].value = value;
    }
}

// The unit addon fields: 0 power type, 1 power, 2 / 3 prestige, 5 average item level (float), 35 glory.
COA_TEST(UnitAddon_Fields) {
    lua_State* L = Setup();
    float ilvl = 213.5f;
    uint32_t ilvlBits;
    memcpy(&ilvlBits, &ilvl, 4);
    SetField(0x11, 0, 4);
    SetField(0x11, 1, 40);
    SetField(0x11, 2, 3);
    SetField(0x11, 3, 9);
    SetField(0x11, 5, ilvlBits);
    SetField(0x11, 35, 1200);

    CHECK(CoATest::Call(L, "", "GetGloryPoints", { "s:player" }) == V{ "1200" });
    CHECK(CoATest::Call(L, "", "UnitAverageItemLevel", { "s:player" }) == V{ "213.5" });
    CHECK(CoATest::Call(L, "", "UnitAlternativePower", { "s:player" }) == V{ "40" });
    CHECK(CoATest::Call(L, "", "UnitAlternativePowerMax", { "s:player" }) == V{ "250" });
    CHECK(CoATest::Call(L, "", "UnitAlternativePowerType", { "s:player" }) == V{ "4" });
    CHECK((CoATest::Call(L, "", "GetPrestigeLevel") == V{ "3", "9" }));
    CoA::Dbc::SetFileReader(nullptr);
}

// An unknown token: the glory / item level readers use GUID 0 (the original's uninitialised GUID) and return 0; the
// alternative-power natives return nothing at all; power type 0 is nil.
COA_TEST(UnitAddon_UnknownToken) {
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "", "GetGloryPoints", { "s:boss1" }) == V{ "0" });
    CHECK(CoATest::Call(L, "", "UnitAverageItemLevel", { "s:boss1" }) == V{ "0" });
    CHECK(CoATest::Call(L, "", "UnitAlternativePower", { "s:boss1" }).empty());
    CHECK(CoATest::Call(L, "", "UnitAlternativePowerMax", { "nil" }).empty());   // not a string
    CHECK(CoATest::Call(L, "", "UnitAlternativePowerType", { "s:player" }) == V{ "nil" });
    CHECK(CoATest::Call(L, "", "UnitAlternativePowerMax", { "s:player" }) == V{ "0" });
    CoA::Dbc::SetFileReader(nullptr);
}
