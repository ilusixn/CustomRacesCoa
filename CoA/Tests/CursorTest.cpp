#include "CoA/Source/Modules/Cursor.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>
#include <vector>

namespace {
    std::vector<std::string> s_loads;
    std::vector<std::string> s_calls;
    uint32_t s_mode = 1;
    bool s_item = false;
    uintptr_t s_next = 0x1000;

    void* Load(const char* path) {
        s_loads.push_back(path);
        return reinterpret_cast<void*>(s_next++);
    }

    void Start() {
        CoA::Cursor::Reset();
        s_loads.clear();
        s_calls.clear();
        s_mode = 1;
        s_item = false;
        s_next = 0x1000;
        CoA::Cursor::Deps& d = CoA::Cursor::Dependencies();
        d.load = Load;
        d.cursorName = [](uint32_t mode) -> const char* {
            static std::string name;
            name = "Cur" + std::to_string(mode);
            return name.c_str();
        };
        d.mode = [] { return s_mode; };
        d.setMode = [](uint32_t mode) { s_mode = mode; };
        d.itemOnCursor = [] { return s_item; };
        d.addRenderLayer = [] { s_calls.push_back("add"); };
        d.releaseRenderLayer = [] { s_calls.push_back("release"); };
        d.deviceCursorVisible = [](int32_t v) { s_calls.push_back("visible " + std::to_string(v)); };
        d.screenWidth = [] { return 1000.0f; };
        d.pixelToScreenHeight = [](int32_t px) { return px / 500.0f; };
    }
}

// FUN_10a1e260: "1" is the hardware cursor (layer released), anything else adds the software layer.
COA_TEST(Cursor_GxCursorSwitchesSoftware) {
    Start();
    CHECK(CoA::Cursor::Hardware());
    CoA::Cursor::OnGxCursor("0");
    CHECK(!CoA::Cursor::Hardware() && s_calls.size() == 1 && s_calls[0] == "add");
    CoA::Cursor::OnGxCursor("1");
    CHECK(CoA::Cursor::Hardware() && s_calls.size() == 2 && s_calls[1] == "release");
    CoA::Cursor::OnGxCursor("2");
    CHECK(!CoA::Cursor::Hardware());
    CoA::Cursor::Reset();
}

// FUN_10a1e610: hardware leaves the call to the client; software loads the texture itself (mode 0x35).
COA_TEST(Cursor_SetFromNameInSoftwareMode) {
    Start();
    CHECK(!CoA::Cursor::SetFromName("Interface\\Cursor\\Point"));
    CHECK(s_loads.empty() && s_mode == 1);
    CoA::Cursor::OnGxCursor("0");
    CHECK(CoA::Cursor::SetFromName("Interface\\Cursor\\Point"));
    CHECK(s_loads.size() == 1 && s_loads[0] == "Interface\\Cursor\\Point" && s_mode == CoA::Cursor::kCustomMode);
    CHECK(CoA::Cursor::DrawTexture() == reinterpret_cast<void*>(0x1000));
    // Without a size variant "Directions" names load as given.
    CHECK(CoA::Cursor::SetFromName("Interface\\Cursor\\UI-Cursor-Directions.blp"));
    CHECK(s_loads.back() == "Interface\\Cursor\\UI-Cursor-Directions.blp");
    // With one, the last four characters become the variant number.
    CoA::Cursor::SizePreferred("4", true);
    CHECK(CoA::Cursor::SetFromName("Interface\\Cursor\\UI-Cursor-Directions.blp"));
    CHECK(s_loads.back() == "Interface\\Cursor\\UI-Cursor-Directions4");
    CHECK(CoA::Cursor::SetFromName("Directions"));
    CHECK(s_loads.back() == "Direct4");
    CoA::Cursor::Reset();
}

// FUN_10a205f0 / FUN_10a20530.
COA_TEST(Cursor_VisibilityAndItemCursor) {
    Start();
    CHECK(CoA::Cursor::CursorVisible(1) == 1 && !CoA::Cursor::Show());   // hardware: stock
    CoA::Cursor::OnGxCursor("0");
    CHECK(CoA::Cursor::CursorVisible(1) == 0 && CoA::Cursor::Show());
    CHECK(CoA::Cursor::CursorVisible(0) == 0 && !CoA::Cursor::Show());
    s_item = true;
    CHECK(CoA::Cursor::CursorVisible(1) == 1 && !CoA::Cursor::Show());
    s_calls.clear();
    CoA::Cursor::AfterUpdateCursor();
    CHECK(s_calls.size() == 1 && s_calls[0] == "visible 1");
    s_item = false;
    CoA::Cursor::AfterUpdateCursor();
    CHECK(s_calls.size() == 1);
    CoA::Cursor::Reset();
}

// FUN_10a1e140 / FUN_10a1e250 / FUN_10a1eb80's texture choice.
COA_TEST(Cursor_TableAndDrawTexture) {
    Start();
    s_mode = 9;
    CoA::Cursor::LoadTable();
    CHECK(s_loads.size() == 0x34 * 3);
    CHECK(s_loads.size() > 2 && s_loads[0] == "Interface\\Cursor\\Cur1" && s_loads[1] == "Interface\\Cursor\\Cur12" && s_loads[2] == "Interface\\Cursor\\Cur14");
    CoA::Cursor::LoadTable();   // only empty slots load
    CHECK(s_loads.size() == 0x34 * 3);
    CoA::Cursor::ResetMode();
    CHECK(s_mode == 1);
    // Shown by default; mode 1 slot 0 (variant 0).
    CHECK(CoA::Cursor::DrawTexture() == CoA::Cursor::Slot(1, 0) && CoA::Cursor::Slot(1, 0) == reinterpret_cast<void*>(0x1000));
    s_mode = 0;   // mode 0 draws as 1
    CHECK(CoA::Cursor::DrawTexture() == CoA::Cursor::Slot(1, 0));
    CoA::Cursor::SizePreferred("2", true);
    s_mode = 3;
    CHECK(CoA::Cursor::DrawTexture() == CoA::Cursor::Slot(3, 2));
    CHECK(CoA::Cursor::Width() == 64.0f / 1000.0f && CoA::Cursor::Height() == 64 / 500.0f);
    CoA::Cursor::Dependencies().screenWidth = [] { return 1920.0f; };
    CHECK(CoA::Cursor::Width() == 64.0f / 1920.0f);
    CoA::Cursor::Dependencies().screenWidth = [] { return 1000.0f; };
    CoA::Cursor::OnGxCursor("0");
    CoA::Cursor::CursorVisible(0);
    CHECK(CoA::Cursor::DrawTexture() == nullptr);
    CoA::Cursor::SizePreferred("0", true);
    CoA::Cursor::Reset();
}

// FUN_10a1edf0: six loads into modes 4 and 30.
COA_TEST(Cursor_SetAttackCursor) {
    Start();
    CoA::LuaReg::Reset();
    CoA::GlobalsJ::Init();
    lua_State* L = CoATest::NewState(false);
    CoATest::Call(L, "", "SetAttackCursor", { "s:Sword" });
    const std::vector<std::string> expected = {
        "Interface\\Cursor\\Sword", "Interface\\Cursor\\UnableSword",
        "Interface\\Cursor\\Sword2", "Interface\\Cursor\\UnableSword2",
        "Interface\\Cursor\\Sword4", "Interface\\Cursor\\UnableSword4",
    };
    CHECK(s_loads == expected);
    CHECK(CoA::Cursor::Slot(4, 0) == reinterpret_cast<void*>(0x1000) && CoA::Cursor::Slot(30, 0) == reinterpret_cast<void*>(0x1001));
    CHECK(CoA::Cursor::Slot(4, 2) == reinterpret_cast<void*>(0x1002) && CoA::Cursor::Slot(30, 4) == reinterpret_cast<void*>(0x1005));
    lua_close(L);
    CoA::LuaReg::Reset();
    CoA::Cursor::Reset();
}
