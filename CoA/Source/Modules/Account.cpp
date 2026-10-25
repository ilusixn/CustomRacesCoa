#include "CoA/Source/Modules/Account.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>
#include <vector>

namespace {
    struct Character {
        std::string name;
        std::string sex;
        uint32_t level = 0;
        std::string cls;
        std::string race;
        std::string faction;
    };

    struct AccountInfo {
        uint32_t field4 = 0;
        uint32_t gmLevel = 0;
        uint8_t newcomer = 0;
        uint8_t guide = 0;
        uint8_t gm = 0;   // +0x0E
        std::vector<Character> characters;
    };

    AccountInfo s_account;
    void (*s_gmEffect)() = nullptr;

    int GetGMLevel(lua_State* L) {
        CoA::Script::PushInt(L, static_cast<int32_t>(s_account.gmLevel));
        return 1;
    }

    int IsGM(lua_State* L) {
        CoA::Script::PushBool(L, s_account.gm != 0);
        return 1;
    }

    int IsGuide(lua_State* L) {
        CoA::Script::PushBool(L, s_account.guide != 0);
        return 1;
    }

    int IsNewcomer(lua_State* L) {
        CoA::Script::PushBool(L, s_account.newcomer != 0);
        return 1;
    }

    int GetNumCharacters(lua_State* L) {
        CoA::Script::PushInt(L, static_cast<int32_t>(s_account.characters.size()));
        return 1;
    }

    void SetStr(lua_State* L, const char* key, const std::string& value) {
        lua_pushstring(L, key);
        lua_pushstring(L, value.c_str());
        lua_settable(L, -3);
    }

    // FUN_1008d1b0: 1-based; nothing is returned for index 0 or out of range.
    int GetCharacterAtIndex(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TNUMBER })) {
            return 0;
        }

        int32_t i = CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1));

        if (i == 0 || static_cast<uint32_t>(i - 1) >= s_account.characters.size()) {
            return 0;
        }

        const Character& c = s_account.characters[i - 1];
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        SetStr(L, "CharacterName", c.name);
        SetStr(L, "Sex", c.sex);
        CoA::Script::SetField(L, "Level", static_cast<int32_t>(c.level));
        SetStr(L, "Class", c.cls);
        SetStr(L, "Race", c.race);
        SetStr(L, "Faction", c.faction);
        return 1;
    }

    // C_Realm: RealmInfo +0x40..+0x44 (FUN_102fc660 / 6b0 / 650 / 670 / 640, FUN_102fc680).
    const uint8_t* Gates() {
        return CoA::RealmInfo::Get().gates;
    }

    int IsLive(lua_State* L) {
        CoA::Script::PushBool(L, Gates()[0] != 0);
        return 1;
    }

    int IsSeasonal(lua_State* L) {
        CoA::Script::PushBool(L, Gates()[1] != 0);
        return 1;
    }

    int IsLeague(lua_State* L) {
        CoA::Script::PushBool(L, Gates()[2] != 0);
        return 1;
    }

    int IsPTR(lua_State* L) {
        CoA::Script::PushBool(L, Gates()[3] != 0);
        return 1;
    }

    int IsDevelopment(lua_State* L) {
        CoA::Script::PushBool(L, Gates()[4] != 0);
        return 1;
    }

    int IsProduction(lua_State* L) {
        CoA::Script::PushBool(L, Gates()[0] || Gates()[1] || Gates()[2]);
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_AccountInfo", "GetGMLevel", GetGMLevel),
        COA_NATIVE("C_AccountInfo", "IsGM", IsGM),
        COA_NATIVE("C_AccountInfo", "IsGuide", IsGuide),
        COA_NATIVE("C_AccountInfo", "IsNewcomer", IsNewcomer),
        COA_NATIVE("C_AccountInfo", "GetNumCharacters", GetNumCharacters),
        COA_NATIVE("C_AccountInfo", "GetCharacterAtIndex", GetCharacterAtIndex),
        COA_NATIVE("C_Realm", "IsLive", IsLive),
        COA_NATIVE("C_Realm", "IsSeasonal", IsSeasonal),
        COA_NATIVE("C_Realm", "IsLeague", IsLeague),
        COA_NATIVE("C_Realm", "IsPTR", IsPTR),
        COA_NATIVE("C_Realm", "IsDevelopment", IsDevelopment),
        COA_NATIVE("C_Realm", "IsProduction", IsProduction),
    };
}

uint32_t CoA::Account::GmLevel() {
    return s_account.gmLevel;
}

// handler_0x09bb.
int32_t CoA::Account::OnAccountInfo(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    s_account.field4 = CoA::Read::U32(msg);
    s_account.gmLevel = CoA::Read::U32(msg);
    s_account.newcomer = CoA::Read::U8(msg);
    s_account.guide = CoA::Read::U8(msg);
    s_account.characters.clear();

    for (uint32_t n = CoA::Read::U32(msg); n && !CoA::Read::Failed(msg); --n) {
        Character c;
        c.name = CoA::Read::Str(msg);
        c.sex = CoA::Read::Str(msg);
        c.level = CoA::Read::U32(msg);
        c.cls = CoA::Read::Str(msg);
        c.race = CoA::Read::Str(msg);
        c.faction = CoA::Read::Str(msg);

        if (CoA::Read::Failed(msg)) {
            break;
        }

        s_account.characters.push_back(std::move(c));
    }

    return 1;
}

// SMSG 0x5C0 (FUN_1008c2b0): u8 -> AccountInfo +0x0E. When set, the client's short-range teleport limit becomes
// FLT_MAX and the chat language / dead checks are skipped (never undone).
int32_t CoA::Account::OnGMStatus(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    s_account.gm = CoA::Read::U8(msg);

    if (s_account.gm && s_gmEffect) {
        s_gmEffect();
    }

    return 1;
}

void CoA::Account::SetGmEffect(void (*effect)()) {
    s_gmEffect = effect;
}

void CoA::Account::Reset() {
    s_account = AccountInfo();
}

void CoA::Account::Init() {
    COA_PACKET(0x5C0, CoA::Account::OnGMStatus);
    COA_PACKET(0x9BB, CoA::Account::OnAccountInfo);
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
