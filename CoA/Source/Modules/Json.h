#ifndef COA_SOURCE_MODULES_JSON_H
#define COA_SOURCE_MODULES_JSON_H

#include "ThirdParty/nlohmann/json.hpp"
#include <cstdint>
#include <string>

class CDataStore;
struct lua_State;

// The original's JSON surface over the same nlohmann::json 3.11.2 it links (port of AscJson.cpp).
//
//   GenericJsonDataStoreMgr (FUN_10258ba0 -> 0x10BE267C): category -> id -> payload string, both
//   std::unordered_maps (under MSVC the same hashes and iteration order as the original).
//     SMSG 0x77C AREA_POI_PAYLOAD, 0x77D MISC_PLAYER_DATA_PAYLOAD, 0x77E FELFORGED_RESET_PAYLOAD,
//     0x77F CHARACTER_ADVANCEMENT_LOADOUT_ACTIVE_PAYLOAD, 0x780 ..._LOADOUT_OWNED_PAYLOAD: {u32 id, cstring payload};
//     0x77B OVERWRITE_LFG_DATA_PAYLOAD: {cstring payload}, id 0.
//     The setter insert-or-assigns and, in world, signals client event 0x2CC (COMMENTATOR_SKIRMISH_QUEUE_REQUEST)
//     with ("%s%u", "ASCENSION_CUSTOM_JSON_<category>_UPDATE", id).
//     Every glue screen empties eleven BUILD_DRAFT_* / BUILD_CREATOR_* categories; the first name carries the
//     original's trailing space, so it creates and empties a category nothing else uses.
//   DecodeJSON(text): json::parse (no comments) inside a catch-all -> the table, or nothing.
//   DecodeJSONContent(name): JSONContentMgr name -> json; nothing in the plain code fills it, so it returns nothing.
//   LoadAscensionContentJSON(name): the text of <client dir>\Content\<name>.json, "" when missing.
//
// JSON -> Lua (FUN_100a4580 / FUN_100a3d60): objects and arrays become tables; a digit-only key k becomes the
// integer k + 1, any other key a string; strings, booleans; integers through lua_pushinteger; floats through
// lua_pushnumber((float)d); anything else nil.
namespace CoA::Json {
    // FUN_1025a6c0.
    void Set(const std::string& category, uint32_t id, const std::string& payload);
    // The cached payload, "" when none.
    std::string Get(const std::string& category, uint32_t id);
    // FUN_100a4580: the value as Lua (a table for objects and arrays).
    void Push(lua_State* L, const nlohmann::json& value);

    int32_t OnPayload(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);
}

#endif
