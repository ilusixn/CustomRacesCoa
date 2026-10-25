#include "CoA/Source/Modules/CharacterCreate.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>

namespace {
    // Strings: 1 "Tank", 6 "Healer", 13 "Shield".
    const std::string kStrings = std::string("Tank") + '\0' + "Healer" + '\0' + "Shield" + '\0';

    std::vector<uint32_t> Cat(std::initializer_list<std::vector<uint32_t>> parts) {
        std::vector<uint32_t> out;

        for (const auto& p : parts) {
            out.insert(out.end(), p.begin(), p.end());
        }

        return out;
    }

    std::vector<uint32_t> Words(size_t n) {
        return std::vector<uint32_t>(n, 0);
    }

    // Layout "19d 3L d": +0x48 name, +0x58 order.
    std::vector<uint32_t> RoleRow(uint32_t id, uint32_t name, uint32_t order) {
        std::vector<uint32_t> head = Words(19);
        head[0] = id;
        head[18] = name;
        return Cat({ head, CoATest::Loc(0), CoATest::Loc(0), CoATest::Loc(0), { order } });
    }

    // Layout "21d 3L d": +4 role, +0x60 order.
    std::vector<uint32_t> CategoryRow(uint32_t id, uint32_t role, uint32_t order) {
        std::vector<uint32_t> head = Words(21);
        head[0] = id;
        head[1] = role;
        return Cat({ head, CoATest::Loc(0), CoATest::Loc(0), CoATest::Loc(0), { order } });
    }

    // Layout "19d 8L 2d": +4 category, +0x70 order.
    std::vector<uint32_t> ArchetypeRow(uint32_t id, uint32_t category, uint32_t order) {
        std::vector<uint32_t> head = Words(19);
        head[0] = id;
        head[1] = category;
        std::vector<uint32_t> row = head;

        for (int i = 0; i < 8; ++i) {
            row = Cat({ row, CoATest::Loc(0) });
        }

        return Cat({ row, { 0, order } });
    }

    uint32_t Bits(float f) {
        uint32_t v;
        memcpy(&v, &f, 4);
        return v;
    }

    // Raw rows: {id, archetype, race, sex, transform (pos, rot in degrees, scale), spell, anim, pet, shapeshift,
    // weapons drawn, 12 items}.
    std::vector<uint32_t> DetailsRow(uint32_t id, uint32_t archetype, uint32_t race, uint32_t sex, float yaw) {
        std::vector<uint32_t> row = { id, archetype, race, sex, 0, 0, 0, 0, 0, Bits(yaw), Bits(1.0f) };
        row.resize(28, 0);
        return row;
    }

    bool ReadTables(const char* path, std::vector<uint8_t>& out) {
        if (strstr(path, "CharacterCreationArchetypeRoles.dbc")) {
            out = CoATest::Wdbc({ RoleRow(1, 1, 30), RoleRow(2, 6, 10), RoleRow(3, 0, 20) }, kStrings, 23);
            return true;
        }

        if (strstr(path, "CharacterCreationArchetypeCategories.dbc")) {
            out = CoATest::Wdbc({ CategoryRow(10, 2, 5), CategoryRow(11, 2, 1), CategoryRow(12, 3, 1) }, kStrings, 25);
            return true;
        }

        if (strstr(path, "CharacterCreationArchetypes.dbc")) {
            out = CoATest::Wdbc({ ArchetypeRow(100, 11, 2), ArchetypeRow(101, 11, 1) }, kStrings, 29);
            return true;
        }

        if (strstr(path, "CharacterCreationArchetypeDetails.dbc")) {
            // The null preview reports race 0, sex 2.
            out = CoATest::Wdbc({ DetailsRow(1, 100, 0, 2, 90.0f), DetailsRow(2, 101, 1, 0, 10.0f) }, kStrings);
            return true;
        }

        if (strstr(path, "CharacterCreationClassGuideRoles.dbc")) {
            // Layout "3d 2L d": +0x14 order.
            out = CoATest::Wdbc({ Cat({ { 5, 1, 0 }, CoATest::Loc(0), CoATest::Loc(0), { 2 } }), Cat({ { 6, 6, 0 }, CoATest::Loc(0), CoATest::Loc(0), { 1 } }) },
                kStrings, 6);
            return true;
        }

        if (strstr(path, "CharacterCreationClassGuideSubroles.dbc")) {
            // Layout "5d 2L d": +4 role, +0x1C order.
            out = CoATest::Wdbc({ Cat({ { 50, 6, 13, 0, 0 }, CoATest::Loc(0), CoATest::Loc(0), { 1 } }) }, kStrings, 8);
            return true;
        }

        if (strstr(path, "CharacterCreationClassGuideSubroleClasses.dbc")) {
            out = CoATest::Wdbc({ { 1, 50, 8, 2 }, { 2, 50, 3, 1 }, { 3, 50, 11, 1 } }, kStrings);
            return true;
        }

        return false;
    }

    lua_State* Setup() {
        CoA::Dbc::SetFileReader(ReadTables);
        CoA::Packets::Clear();
        CoA::RealmInfo::Reset();
        CoA::CharacterCreate::Reset();
        CoA::LuaReg::Reset();
        CoA::CharacterCreate::Init();
        CoA::SmallApis::Init();
        return CoATest::NewState(true);
    }

    void Teardown(lua_State* L) {
        lua_close(L);
        CoA::Dbc::SetFileReader(nullptr);
    }

    std::vector<std::string> Run(lua_State* L, const char* chunk) {
        std::vector<std::string> out;
        int base = lua_gettop(L);

        if (luaL_loadstring(L, chunk) != 0 || lua_pcall(L, 0, LUA_MULTRET, 0) != 0) {
            out.push_back(std::string("<error> ") + lua_tostring(L, -1));
            lua_settop(L, base);
            return out;
        }

        for (int i = base + 1; i <= lua_gettop(L); ++i) {
            out.push_back(lua_isnil(L, i) ? "nil" : lua_isboolean(L, i) ? (lua_toboolean(L, i) ? "true" : "false") : lua_tostring(L, i));
        }

        lua_settop(L, base);
        return out;
    }

    void Realm(const uint8_t gates[8]) {
        CoATest::Bytes b;
        b.U32(1).U32(0).F32(0).F32(0).F32(0).U32(0).F32(0).F32(0).U32(0);

        for (int i = 0; i < 8; ++i) {
            b.U8(gates[i]);
        }

        b.Str("").Str("Realm").U8(0);
        CoATest::Deliver(CoA::RealmInfo::OnRealmInfo, 0x9BC, b);
    }
}

COA_TEST(CharCreate_ArchetypesSortedByOrder) {
    lua_State* L = Setup();
    CHECK(Run(L, "local t = C_CharacterCreate.GetArchetypeRoles() return #t, t[1], t[2], t[3]") == (std::vector<std::string>{ "3", "2", "3", "1" }));
    CHECK(Run(L, "local t = C_CharacterCreate.GetArchetypeCategories(2) return #t, t[1], t[2]") == (std::vector<std::string>{ "2", "11", "10" }));
    CHECK(Run(L, "local t = C_CharacterCreate.GetArchetypes(11) return #t, t[1], t[2]") == (std::vector<std::string>{ "2", "101", "100" }));
    CHECK(Run(L, "return #C_CharacterCreate.GetArchetypes(99)") == std::vector<std::string>{ "0" });
    CHECK(Run(L, "return C_CharacterCreate.GetArchetypeRoleInfo(2)") == (std::vector<std::string>{ "Healer", "", "", "" }));
    CHECK(Run(L, "return C_CharacterCreate.GetArchetypeRoleInfo(9)") == (std::vector<std::string>{ "nil", "nil", "nil", "nil" }));
    Teardown(L);
}

COA_TEST(CharCreate_ClassGuideTree) {
    lua_State* L = Setup();
    CHECK(Run(L, "local t = C_CharacterCreate.GetClassGuideRoles() return #t, t[1], t[2]") == (std::vector<std::string>{ "2", "6", "5" }));
    CHECK(Run(L, "local t = C_CharacterCreate.GetClassGuideSubroles(6) return #t, t[1]") == (std::vector<std::string>{ "1", "50" }));
    // Order, then row id: class 3 (order 1, row 2), class 11 (order 1, row 3), class 8 (order 2).
    CHECK(Run(L, "local t = C_CharacterCreate.GetClassGuideSubroleClasses(50) return #t, t[1], t[2], t[3]") ==
        (std::vector<std::string>{ "3", "3", "11", "8" }));
    CHECK(Run(L, "local role, name = C_CharacterCreate.GetClassGuideSubroleInfo(50) return role, name") == (std::vector<std::string>{ "6", "Shield" }));
    Teardown(L);
}

COA_TEST(CharCreate_PatchPacketReplacesRow) {
    lua_State* L = Setup();
    CHECK(Run(L, "return #C_CharacterCreate.GetArchetypeRoles()") == std::vector<std::string>{ "3" });
    // SMSG 0x6D4: the 0x5C-byte struct image, then the four strings (+0x48..+0x54).
    CoATest::Bytes patch;
    // Role 1 is replaced in place (its order stays 30: the tables outlive the test).
    std::vector<uint32_t> image = Words(23);
    image[0] = 1;
    image[22] = 30;

    for (uint32_t v : image) {
        patch.U32(v);
    }

    patch.Str("Brawler").Str("a").Str("b").Str("c");
    CoATest::Deliver([](void*, uint32_t opcode, uint32_t time, CDataStore* m) {
        int32_t r = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), time, m, r);
        return r;
    }, 0x6D4, patch);
    CHECK(Run(L, "local t = C_CharacterCreate.GetArchetypeRoles() return #t, t[3]") == (std::vector<std::string>{ "3", "1" }));
    CHECK(Run(L, "return C_CharacterCreate.GetArchetypeRoleInfo(1)") == (std::vector<std::string>{ "Brawler", "a", "b", "c" }));
    Teardown(L);
}

COA_TEST(CharCreate_LayerMergeOrder) {
    lua_State* L = Setup();
    CoATest::RecordSignals();
    // Layer 0 from archetype 100's details for race 0 / sex 2: facing 90 degrees.
    Run(L, "C_CharacterCreate.SetSelectedArchetype(2, 11, 100)");
    CHECK(static_cast<int>(CoA::CharacterCreate::FacingDegrees() + 0.5f) == 90);
    // Layer 1 (what the bindings set) wins over layer 0.
    CoA::CharacterCreate::SetFacingDegrees(45.0f);
    CHECK(static_cast<int>(CoA::CharacterCreate::FacingDegrees() + 0.5f) == 45);
    // A new selection rebuilds layer 0 only; the layer-1 facing stays.
    Run(L, "C_CharacterCreate.SetSelectedArchetype(2, 11, 101)");
    CHECK(static_cast<int>(CoA::CharacterCreate::FacingDegrees() + 0.5f) == 45);
    CoATest::StopRecordingSignals();
    // 101 has no details for this race / sex: layer 0 loses its transform, so the preview changes again.
    CHECK(CoATest::Signals() == (std::vector<std::string>{ "CHARACTER_CREATE_PREVIEW_CHANGED", "CHARACTER_CREATE_SELECTION_CHANGED:2,11,100",
        "CHARACTER_CREATE_PREVIEW_CHANGED", "CHARACTER_CREATE_SELECTION_CHANGED:2,11,101" }));
    // The camera override goes through layer 1 too.
    Run(L, "C_CharacterCreate.SetCameraPosition(1, 2, 3)");
    float out[3] = { 9, 9, 9 };
    CoA::CharacterCreate::AfterCameraCoord(7, out);
    CHECK(out[0] == 1.0f && out[1] == 2.0f && out[2] == 3.0f);
    CHECK(Run(L, "return C_CharacterCreate.GetCameraPosition()") == (std::vector<std::string>{ "1", "2", "3" }));
    Teardown(L);
}

COA_TEST(CharCreate_GatesFromRealmInfo) {
    lua_State* L = Setup();
    const uint8_t hero[8] = { 1, 0, 0, 0, 0, 0, 0, 0 };
    Realm(hero);
    CHECK(CoATest::Call(L, "C_CharacterCreate", "CanCreateCoA") == std::vector<std::string>{ "false" });
    CHECK(CoATest::Call(L, "C_CharacterCreate", "CanCreateHero") == std::vector<std::string>{ "true" });
    CHECK(CoATest::Call(L, "C_CharacterCreate", "CanCreateClass", { "n:10" }) == std::vector<std::string>{ "true" });
    CHECK(CoATest::Call(L, "C_CharacterCreate", "CanCreateClass", { "n:12" }) == std::vector<std::string>{ "false" });
    const uint8_t coa[8] = { 1, 0, 0, 0, 0, 0, 1, 0 };
    Realm(coa);
    CHECK(CoATest::Call(L, "C_CharacterCreate", "CanCreateCoA") == std::vector<std::string>{ "true" });
    CHECK(CoATest::Call(L, "C_CharacterCreate", "CanCreateHero") == std::vector<std::string>{ "false" });
    CHECK(CoATest::Call(L, "C_CharacterCreate", "CanCreateClass", { "n:12" }) == std::vector<std::string>{ "true" });
    CHECK(CoATest::Call(L, "C_CharacterCreate", "CanCreateClass", { "n:10" }) == std::vector<std::string>{ "false" });
    CHECK(CoATest::Call(L, "C_CharacterCreate", "CanCreateWCR") == std::vector<std::string>{ "false" });
    Teardown(L);
}

COA_TEST(CharCreate_CustomizationUnlocksAreConstant) {
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "C_CharacterCustomizationUnlocks", "GetCurrentOptionInfo", { "n:1" }) == (std::vector<std::string>{ "false", "true", "0" }));
    CHECK(CoATest::Call(L, "C_CharacterCustomizationUnlocks", "HasLockedSelections") == std::vector<std::string>{ "false" });
    CHECK(CoATest::Call(L, "C_CharacterCustomizationUnlocks", "GetFirstLockedRequirement") == std::vector<std::string>{ "0" });
    Teardown(L);
}
