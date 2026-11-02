#include "CoA/Source/Modules/Json.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <Windows.h>
#include <cctype>
#include <cstdio>
#include <unordered_map>

using nlohmann::json;

namespace {
    // ---- json -> Lua ------------------------------------------------------------------------------------------
    void PushKey(lua_State* L, const std::string& key) {   // FUN_100a3d60's key half
        bool digits = !key.empty();

        for (char c : key) {
            if (!isdigit(static_cast<unsigned char>(c))) {
                digits = false;
                break;
            }
        }

        if (digits) {
            CoA::Script::PushInt(L, atoi(key.c_str()) + 1);
        } else {
            CoA::Script::PushStr(L, key.c_str());
        }
    }

    void PushItems(lua_State* L, const json& value) {
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);

        for (const auto& item : value.items()) {
            PushKey(L, item.key());
            CoA::Json::Push(L, item.value());
            lua_settable(L, -3);
        }
    }

    // ---- GenericJsonDataStoreMgr --------------------------------------------------------------------------------
    std::unordered_map<std::string, std::unordered_map<uint32_t, std::string>> s_cache;

    const char* CategoryFor(uint32_t opcode) {
        switch (opcode) {
            case 0x77B:
                return "OVERWRITE_LFG_DATA_PAYLOAD";
            case 0x77C:
                return "AREA_POI_PAYLOAD";
            case 0x77D:
                return "MISC_PLAYER_DATA_PAYLOAD";
            case 0x77E:
                return "FELFORGED_RESET_PAYLOAD";
            case 0x77F:
                return "CHARACTER_ADVANCEMENT_LOADOUT_ACTIVE_PAYLOAD";
            case 0x780:
                return "CHARACTER_ADVANCEMENT_LOADOUT_OWNED_PAYLOAD";
            default:
                return nullptr;
        }
    }

    void ResetBuildPayloads() {   // FUN_10258190
        static const char* const categories[] = {
            "BUILD_DRAFT_DRAFTABLE_PAYLOAD ",
            "BUILD_DRAFT_SINGLE_DRAFTABLE_PAYLOAD",
            "BUILD_DRAFT_ROLE_PROMPT_PAYLOAD",
            "BUILD_DRAFT_DRAFTED_PAYLOAD",
            "BUILD_DRAFT_COMPLETED_PAYLOAD",
            "BUILD_CREATOR_SUMMARY_PAYLOAD",
            "BUILD_CREATOR_DATA_PAYLOAD",
            "BUILD_CREATOR_ACTIVE_PAYLOAD",
            "BUILD_CREATOR_OWNED_PAYLOAD",
            "BUILD_CREATOR_UPVOTED_PAYLOAD",
            "BUILD_CREATOR_SAVE_RESULT_PAYLOAD",
        };

        for (const char* category : categories) {
            s_cache[category].clear();
        }
    }

    // ClearJsonCache([category [, id]]): everything, one category (created if missing), or one entry.
    int ClearJsonCache(lua_State* L) {
        switch (lua_gettop(L)) {
            case 0:
                s_cache.clear();
                break;
            case 1:
                s_cache[CoA::Script::CheckString(L, 1)].clear();
                break;
            case 2: {
                std::string category = CoA::Script::CheckString(L, 1);
                uint32_t id = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 2)));
                s_cache[category].erase(id);
                break;
            }
            default:
                break;
        }

        return 0;
    }

    int HasJsonCacheData(lua_State* L) {
        std::string category = CoA::Script::CheckString(L, 1);
        uint32_t id = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 2)));
        auto c = s_cache.find(category);
        CoA::Script::PushBool(L, c != s_cache.end() && c->second.count(id) != 0);
        return 1;
    }

    // GetJsonCacheCategory(category): the payloads (the category is created if missing), keyed 1..n.
    int GetJsonCacheCategory(lua_State* L) {
        std::string category = CoA::Script::CheckString(L, 1);
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        int32_t i = 0;

        for (const auto& e : s_cache[category]) {
            CoA::Script::PushNum(L, static_cast<double>(++i));
            CoA::Script::PushStr(L, e.second.c_str());
            lua_settable(L, -3);
        }

        return 1;
    }

    int GetJsonCacheData(lua_State* L) {
        std::string category = CoA::Script::CheckString(L, 1);
        uint32_t id = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 2)));
        const char* text = "";
        auto c = s_cache.find(category);

        if (c != s_cache.end()) {
            auto e = c->second.find(id);

            if (e != c->second.end()) {
                text = e->second.c_str();
            }
        }

        CoA::Script::PushStr(L, text);
        return 1;
    }

    // ---- decoding -----------------------------------------------------------------------------------------------
    int DecodeJSON(lua_State* L) {   // FUN_1028c4e0
        std::string text = CoA::Script::CheckString(L, 1);

        if (text.empty()) {
            return 0;
        }

        try {
            json root = json::parse(text, nullptr, true, false);
            PushItems(L, root);
            return 1;
        } catch (...) {
            return 0;
        }
    }

    // JSONContentMgr (FUN_1028c360): name -> {json, registry ref (-1 until built)}.
    struct Content {
        json value;
        int ref = -1;
    };

    std::unordered_map<std::string, Content> s_content;

    int DecodeJSONContent(lua_State* L) {   // FUN_1028c660
        std::string name = CoA::Script::CheckString(L, 1);

        if (name.empty()) {
            return 0;
        }

        auto it = s_content.find(name);

        if (it == s_content.end()) {
            return 0;
        }

        if (it->second.ref == -1) {
            lua_State* main = CoA::ClientState::MainLuaState();
            PushItems(main, it->second.value);
            it->second.ref = luaL_ref(main, LUA_REGISTRYINDEX);
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, it->second.ref);
        return 1;
    }

    int LoadAscensionContentJSON(lua_State* L) {   // FUN_100dc2d0
        std::string name = CoA::Script::CheckString(L, 1);
        std::string text;

        if (!name.empty()) {
            std::string path = CoA::ClientState::BasePath();

            if (!path.empty() && path.back() != '\\' && path.back() != '/') {
                path += '\\';
            }

            path += "Content\\" + name + ".json";
            DWORD attributes = GetFileAttributesA(path.c_str());

            if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
                if (FILE* f = _fsopen(path.c_str(), "r", 0x40)) {
                    char buffer[4096];
                    size_t n;

                    while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) {
                        text.append(buffer, n);
                    }

                    fclose(f);
                }
            }
        }

        CoA::Script::PushStr(L, text.c_str());
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "ClearJsonCache", ClearJsonCache),
        COA_NATIVE("", "HasJsonCacheData", HasJsonCacheData),
        COA_NATIVE("", "GetJsonCacheCategory", GetJsonCacheCategory),
        COA_NATIVE("", "GetJsonCacheData", GetJsonCacheData),
        COA_NATIVE("", "DecodeJSON", DecodeJSON),
        COA_NATIVE("", "DecodeJSONContent", DecodeJSONContent),
        // The world state also has it as _DecodeJSONContent (the same function; live capture 2026-09-27).
        COA_NATIVE("", "_DecodeJSONContent", DecodeJSONContent),
        COA_NATIVE("", "LoadAscensionContentJSON", LoadAscensionContentJSON),
    };
}

void CoA::Json::Push(lua_State* L, const nlohmann::json& value) {
    switch (value.type()) {
        case json::value_t::object:
        case json::value_t::array:
            PushItems(L, value);
            break;
        case json::value_t::string:
            CoA::Script::PushStr(L, value.get_ref<const std::string&>().c_str());
            break;
        case json::value_t::boolean:
            CoA::Script::PushBool(L, value.get<bool>());
            break;
        case json::value_t::number_integer:
            CoA::Script::PushInt(L, static_cast<int32_t>(value.get<int64_t>()));
            break;
        case json::value_t::number_unsigned:
            CoA::Script::PushInt(L, static_cast<int32_t>(value.get<uint64_t>()));
            break;
        case json::value_t::number_float:
            CoA::Script::PushNum(L, static_cast<double>(static_cast<float>(value.get<double>())));
            break;
        default:
            lua_pushnil(L);
            break;
    }
}

void CoA::Json::Set(const std::string& category, uint32_t id, const std::string& payload) {
    s_cache[category].insert_or_assign(id, payload);

    if (CoA::ClientState::InWorld()) {
        std::string name = "ASCENSION_CUSTOM_JSON_" + category + "_UPDATE";
        CoA::Events::Signal("COMMENTATOR_SKIRMISH_QUEUE_REQUEST", "%s%u", name.c_str(), id);
    }
}

std::string CoA::Json::Get(const std::string& category, uint32_t id) {
    auto c = s_cache.find(category);
    if (c == s_cache.end()) {
        return {};
    }
    auto e = c->second.find(id);
    return e == c->second.end() ? std::string() : e->second;
}

// FUN_10259430 (0x77B: no id) and the five {u32 id, cstring} payload handlers.
int32_t CoA::Json::OnPayload(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    const char* category = CategoryFor(opcode);

    if (!category) {
        return 1;
    }

    uint32_t id = opcode == 0x77B ? 0 : CoA::Read::U32(msg);
    CoA::Json::Set(category, id, CoA::Read::Str(msg));
    return 1;
}

void CoA::Json::Init() {
    COA_PACKET(0x77B, CoA::Json::OnPayload);
    COA_PACKET(0x77C, CoA::Json::OnPayload);
    COA_PACKET(0x77D, CoA::Json::OnPayload);
    COA_PACKET(0x77E, CoA::Json::OnPayload);
    COA_PACKET(0x77F, CoA::Json::OnPayload);
    COA_PACKET(0x780, CoA::Json::OnPayload);

    // FUN_102577d0 (-> FUN_10258190) on the glue-screen list.
    CoA::Lifecycle::OnGlueScreen(ResetBuildPayloads);
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
