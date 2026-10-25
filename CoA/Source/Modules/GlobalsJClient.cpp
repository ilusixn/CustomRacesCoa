#include "CoA/Source/Modules/GlobalsJ.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/SpellText.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include <string>

namespace {
    // GetSpellData(spell [, a, b]) (FUN_10a4da30): accepts {number, boolean, boolean}, then {number, boolean} (the
    // boolean is read and ignored), then {number}. Pushes the Spell.dbc name (+0x220), the description
    // (FUN_101ac910(spell, a, 0, b) = SpellText::Description) and the SpellIcon path (+0x214 -> SpellIcon.dbc +4,
    // default the question mark). Nothing for an unknown spell.
    int GetSpellData(lua_State* L) {
        uint32_t spell = 0;
        bool a = false;
        bool b = false;

        if (CoA::Script::ValidateInput(L, { LUA_TNUMBER, LUA_TBOOLEAN, LUA_TBOOLEAN })) {
            spell = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1)));
            a = lua_toboolean(L, 2) != 0;
            b = lua_toboolean(L, 3) != 0;
        } else if (CoA::Script::ValidateInput(L, { LUA_TNUMBER, LUA_TBOOLEAN })) {
            spell = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1)));
            lua_toboolean(L, 2);
        } else if (!CoA::Script::ReadNumber(L, spell)) {
            return 0;
        }

        SpellRec rec;

        if (!g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec)) {
            return 0;
        }

        std::string icon = "Interface\\Icons\\INV_Misc_QuestionMark";

        if (const SpellIconRec* row = g_spellIconDB.GetRecord(rec.m_spellIconID)) {
            icon = row->m_textureFilename ? row->m_textureFilename : "";
        }

        CoA::Script::PushStr(L, rec.m_name);
        CoA::Script::PushStr(L, CoA::SpellText::Description(spell, a, false, b ? 1 : 0).c_str());
        CoA::Script::PushStr(L, icon.c_str());
        return 3;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "GetSpellData", GetSpellData),
    };
}

void CoA::GlobalsJ::InstallClient() {
    // AscRuntime::ReplaceFunction(0x819730, ScriptErrorHandler).
    g_frameScriptErrorHook = CoA::GlobalsJ::OnErrorHandler;
}

void CoA::GlobalsJClient::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
