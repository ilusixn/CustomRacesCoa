// AscToken.cpp (see Token.h).
#include "CoA/Source/Modules/Token.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>
#include <vector>

namespace {
    // PTR_s_TOKEN_TYPE_SCROLL_OF_FORTUNE_I_10b58850: 20 tiers x {"", _ABILITIES, _TALENTS}, then CALLBOARD_CACHE_POINTS
    // and MAX.
    const std::vector<std::string>& TypeNames() {
        static std::vector<std::string> names;

        if (names.empty()) {
            static const char* const kRoman[20] = { "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XI", "XII", "XIII", "XIV", "XV",
                "XVI", "XVII", "XVIII", "XIX", "XX" };

            for (const char* suffix : { "", "_ABILITIES", "_TALENTS" }) {
                for (const char* r : kRoman) {
                    names.push_back(std::string("TOKEN_TYPE_SCROLL_OF_FORTUNE_") + r + suffix);
                }
            }

            names.push_back("TOKEN_TYPE_CALLBOARD_CACHE_POINTS");
            names.push_back("TOKEN_TYPE_MAX");
        }

        return names;
    }

    struct Entry {
        int32_t amount = 0;   // +8
        uint32_t extra = 0;   // +0xC
    };

    Entry s_tokens[CoA::Token::kCount];

    // FUN_1033c260 (every glue screen).
    void ResetAmounts() {
        for (Entry& e : s_tokens) {
            e = Entry();
        }
    }

    int32_t OnTokenList(void*, uint32_t, uint32_t, CDataStore* msg) {
        for (uint32_t n = CoA::Read::U32(msg); n && !CoA::Read::Failed(msg); --n) {
            const std::string name = CoA::Read::Str(msg);
            uint32_t i;

            if (!CoA::Token::TypeIndex(name.c_str(), i) || i >= CoA::Token::kCount) {
                continue;
            }

            s_tokens[i].amount = CoA::Read::I32(msg);
            s_tokens[i].extra = CoA::Read::U32(msg);
        }

        return 1;
    }

    int32_t OnTokenUpdate(void*, uint32_t, uint32_t, CDataStore* msg) {
        const std::string name = CoA::Read::Str(msg);
        uint32_t i;

        if (!CoA::Token::TypeIndex(name.c_str(), i) || i >= CoA::Token::kCount) {
            return 1;
        }

        Entry& e = s_tokens[i];
        const int32_t oldAmount = e.amount;
        const uint32_t oldExtra = e.extra;
        e.amount = CoA::Read::I32(msg);
        e.extra = CoA::Read::U32(msg);

        if (oldAmount != e.amount || oldExtra != e.extra) {
            CoA::Events::Signal("TOKEN_UPDATED", "%s%u%u", name.c_str(), oldAmount, e.amount);
        }

        return 1;
    }

    CoA::Dbc::Table& TokenTypes() {
        return CoA::Dbc::Get("DBFilesClient\\TokenTypes.dbc");
    }

    int GetTokenAmount(lua_State* L) {
        std::string name;

        if (!CoA::Script::ReadString(L, name)) {
            return 0;
        }

        uint32_t i;

        if (!CoA::Token::TypeIndex(name.c_str(), i)) {
            return 0;
        }

        CoA::Script::PushNum(L, CoA::Token::Amount(i));
        return 1;
    }

    // Row: +4 name, +8 / +0xC / +0x10 localized, +0x14 / +0x18 strings, +0x1C bool, +0x20 int.
    int GetTokenInfo(lua_State* L) {
        std::string name;

        if (!CoA::Script::ReadString(L, name)) {
            return 0;
        }

        CoA::Dbc::Table& t = TokenTypes();

        for (uint32_t id = t.MinId(); t.Loaded() && id <= t.MaxId(); ++id) {
            const uint8_t* row = t.Row(id);

            if (!row || name != t.Str(row, 4)) {
                continue;
            }

            for (uint32_t off = 8; off <= 0x18; off += 4) {
                CoA::Script::PushStr(L, t.Str(row, off));
            }

            CoA::Script::PushBool(L, CoA::Dbc::Table::I32(row, 0x1C) != 0);
            CoA::Script::PushInt(L, CoA::Dbc::Table::I32(row, 0x20));
            return 7;
        }

        return CoA::Script::PushNils(L, 7);
    }

    // FUN_100beb90: { [1] = name, ... } in id order.
    int GetTokenTypes(lua_State* L) {
        std::vector<std::string> names;
        CoA::Dbc::Table& t = TokenTypes();

        for (uint32_t id = t.MinId(); t.Loaded() && id <= t.MaxId(); ++id) {
            if (const uint8_t* row = t.Row(id)) {
                names.push_back(t.Str(row, 4));
            }
        }

        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);

        for (uint32_t i = 0; i < names.size(); ++i) {
            CoA::Script::PushNum(L, i + 1);
            CoA::Script::PushStr(L, names[i].c_str());
            lua_settable(L, -3);
        }

        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_Token", "GetTokenAmount", GetTokenAmount),
        COA_NATIVE("C_Token", "GetTokenInfo", GetTokenInfo),
        COA_NATIVE("C_Token", "GetTokenTypes", GetTokenTypes),
    };
}

bool CoA::Token::TypeIndex(const char* name, uint32_t& index) {
    const std::vector<std::string>& names = TypeNames();

    for (uint32_t i = 0; i < names.size(); ++i) {
        if (names[i] == name) {
            index = i;
            return true;
        }
    }

    return false;
}

const char* CoA::Token::TypeName(uint32_t index) {
    const std::vector<std::string>& names = TypeNames();
    return index < names.size() ? names[index].c_str() : nullptr;
}

int32_t CoA::Token::Amount(uint32_t index) {
    return index < kCount ? s_tokens[index].amount : 0;
}

uint64_t CoA::Token::Amount64(uint32_t index) {
    if (index >= kCount) {
        return 0;
    }

    return (static_cast<uint64_t>(s_tokens[index].extra) << 32) | static_cast<uint32_t>(s_tokens[index].amount);
}

bool CoA::Token::AtLeast(uint32_t index, uint32_t lo, uint32_t hi) {
    const uint32_t amount = index < kCount ? static_cast<uint32_t>(s_tokens[index].amount) : 0;
    const uint32_t extra = index < kCount ? s_tokens[index].extra : 0;
    return extra > hi || (extra == hi && amount >= lo);
}

void CoA::Token::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    CoA::Lifecycle::OnGlueScreen(ResetAmounts);
    COA_PACKET(0x66F, OnTokenList);
    COA_PACKET(0x670, OnTokenUpdate);
    COA_EVENT("TOKEN_UPDATED");
}

void CoA::Token::Reset() {
    ResetAmounts();
}
