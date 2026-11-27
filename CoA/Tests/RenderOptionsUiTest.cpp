#include "CoA/Source/Modules/RenderOptionsUi.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/lua-5.1/src/lualib.h"
#include <cstring>
#include <string>

using CoA::RenderOptions::Reason;

// C_CoARender.GetOptionState's table: per option enabled / reason / max, plus whether a choice waits for a restart.
COA_TEST(RenderOptionsUi_PushStateTable) {
    CoA::RenderOptions::State state {};
    state.dlss = { true, Reason::None, 5, true };
    state.frameGen = { false, Reason::NotAvailableYet, 0, false };
    state.reflex = { true, Reason::None, 2, false };

    lua_State* L = luaL_newstate();
    CoA::RenderOptionsUi::PushState(L, state);
    lua_setglobal(L, "s");
    const char* probe = "return s.dlss.enabled, s.frameGen.reason, s.reflex.max, s.dlss.restart, s.dlss.reason";
    CHECK(luaL_loadbuffer(L, probe, strlen(probe), "probe") == 0);
    CHECK(lua_pcall(L, 0, 5, 0) == 0);
    CHECK(lua_toboolean(L, 1) == 1);
    CHECK(std::string(lua_tostring(L, 2)) == "Not available yet");
    CHECK(lua_tonumber(L, 3) == 2);
    CHECK(lua_toboolean(L, 4) == 1);
    CHECK(lua_isnil(L, 5));
    lua_close(L);
}

// The injected builder is valid Lua.
COA_TEST(RenderOptionsUi_LuaCompiles) {
    lua_State* L = luaL_newstate();
    const char* source = CoA::RenderOptionsUi::BuilderLua();
    CHECK(luaL_loadbuffer(L, source, strlen(source), "RenderOptions") == 0);
    lua_close(L);
}

// The three CVars and frames, the panel's own Okay / Cancel / Default, and no graphics restart of its own.
COA_TEST(RenderOptionsUi_LuaNamesCVars) {
    const std::string source = CoA::RenderOptionsUi::BuilderLua();

    for (const char* needle : { "coaDlssMode", "coaFrameGen", "coaReflex", "CoARenderDlssDropDown", "CoARenderFrameGenDropDown",
             "CoARenderReflexDropDown", "VideoOptionsResolutionPanel", "C_CoARender.GetOptionState", ".okay", ".cancel",
             ".default", "gxMultisample", "Takes effect after restarting the game" }) {
        CHECK(source.find(needle) != std::string::npos);
    }

    CHECK(source.find("RestartGx") == std::string::npos);
}

namespace {
    // Just enough FrameXML for the builder: widgets, UIDropDownMenu, CVars and the option state, all in Lua tables.
    const char* kStubs = R"lua(
CVARS = {}
function GetCVar(n) return CVARS[n] end
function SetCVar(n, v) CVARS[n] = tostring(v) end
local function widget(name)
    local w = { scripts = {}, shown = true, enabled = true }
    function w:SetPoint() end
    function w:SetText(t) self.text = t end
    function w:Show() self.shown = true end
    function w:Hide() self.shown = false end
    function w:IsShown() return self.shown end
    function w:HookScript(ev, fn)
        local old = self.scripts[ev]
        self.scripts[ev] = function(...) if old then old(...) end fn(...) end
    end
    function w:CreateFontString(n) return widget(n) end
    if name then _G[name] = w end
    return w
end
function CreateFrame(kind, name) return widget(name) end
function UIDropDownMenu_SetWidth() end
function UIDropDownMenu_Initialize(dd, fn) dd.init = fn end
function UIDropDownMenu_SetSelectedValue(dd, v) dd.selected = v end
function UIDropDownMenu_SetText(dd, t) dd.text = t end
function UIDropDownMenu_DisableDropDown(dd) dd.enabled = false end
function UIDropDownMenu_EnableDropDown(dd) dd.enabled = true end
function UIDropDownMenu_CreateInfo() return {} end
function UIDropDownMenu_AddButton(info) MENU[#MENU + 1] = info end
widget("VideoOptionsFrame")
widget("VideoOptionsResolutionPanel")
widget("VideoOptionsResolutionPanelMultiSampleDropDown")
C_CoARender = { GetOptionState = function() return STATE end }
function Show(w) if w.scripts.OnShow then w.scripts.OnShow(w) end end
function Pick(dd, value)
    MENU = {}
    dd.init(dd)
    for _, info in ipairs(MENU) do
        if info.value == value then info.func({ value = value }) end
    end
end
function Opt(enabled, max, restart) return { enabled = enabled, max = max, restart = restart, reason = (not enabled) and "x" or nil } end
)lua";

    // Runs the stubs, `setup`, the builder (as the client does after entering the world), then `probe`.
    std::string Run(const char* setup, const char* probe) {
        lua_State* L = luaL_newstate();
        lua_pushcfunction(L, luaopen_base);
        lua_call(L, 0, 0);
        std::string result;

        for (const char* chunk : { kStubs, setup, CoA::RenderOptionsUi::BuilderLua(), probe }) {
            if (luaL_loadbuffer(L, chunk, strlen(chunk), "chunk") != 0 || lua_pcall(L, 0, 1, 0) != 0) {
                result = std::string("error: ") + lua_tostring(L, -1);
                break;
            }

            result = lua_isnil(L, -1) ? "" : lua_tostring(L, -1);
            lua_pop(L, 1);
        }

        lua_close(L);
        return result;
    }
}

// A saved DLSS mode the player cannot use neither greys out Multisampling nor shows the restart note.
COA_TEST(RenderOptionsUi_UnusableSavedOptionIsInert) {
    const std::string r = Run(
        "CVARS.coaDlssMode = '2' STATE = { dlss = Opt(false, 0, false), frameGen = Opt(false, 0, false), reflex = Opt(false, 0, false) }",
        "Show(VideoOptionsFrame) Show(VideoOptionsResolutionPanel) "
        "return tostring(CoARenderRestartNote:IsShown()) .. ' ' .. tostring(VideoOptionsResolutionPanelMultiSampleDropDown.enabled)");
    CHECK(r == "false true");
}

// The restart note shows only while a choice that waits for a restart differs from the saved value.
COA_TEST(RenderOptionsUi_RestartNoteOnlyForPendingChanges) {
    const std::string r = Run(
        "CVARS.coaDlssMode = '0' STATE = { dlss = Opt(true, 5, true), frameGen = Opt(false, 0, false), reflex = Opt(true, 2, false) }",
        "Show(VideoOptionsFrame) Show(VideoOptionsResolutionPanel) local a = tostring(CoARenderRestartNote:IsShown()) "
        "Pick(CoARenderDlssDropDown, 2) local b = tostring(CoARenderRestartNote:IsShown()) "
        "Pick(CoARenderDlssDropDown, 0) return a .. ' ' .. b .. ' ' .. tostring(CoARenderRestartNote:IsShown())");
    CHECK(r == "false true false");
}

// A choice survives switching Video options categories (the panel hides and shows again) and Okay saves it.
COA_TEST(RenderOptionsUi_ChoiceSurvivesCategorySwitch) {
    const std::string r = Run(
        "CVARS.coaDlssMode = '2' STATE = { dlss = Opt(true, 5, false), frameGen = Opt(false, 0, false), reflex = Opt(true, 2, false) }",
        "Show(VideoOptionsFrame) Show(VideoOptionsResolutionPanel) Pick(CoARenderDlssDropDown, 3) "
        "Show(VideoOptionsResolutionPanel) local p = VideoOptionsResolutionPanel p.okay(p) "
        "local a = CoARenderDlssDropDown.pending .. ' ' .. CVARS.coaDlssMode "
        "CVARS.coaDlssMode = '1' Show(VideoOptionsFrame) return a .. ' ' .. CoARenderDlssDropDown.pending");
    CHECK(r == "3 3 1");
}
