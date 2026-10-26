// AscItemStats.cpp (see ItemStats.h): the scaled item-stat store behind CMSG 0x6FF / SMSG 0x700, and
// GetScalingItemStats / GetAuctionItemRequiredLevel / SetInspectedSlot.
#include "CoA/Source/Modules/ItemStats.h"
#include "CoA/Source/Modules/AttachLua.h"
#include "CoA/Source/Modules/Json.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <iterator>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
    using CoA::Script::CheckNumber;
    using CoA::Script::ToInt;

    using Callback = void (*)(uint32_t item, uint32_t level, void* arg);

    struct Entry {
        uint8_t record[0xA8] = {};                         // +0x10
        std::vector<std::pair<Callback, void*>> waiting;   // +0xC0
        bool loaded = false;                               // +0xC8
    };

    CoA::ItemStats::Deps s_deps;
    std::unordered_map<uint64_t, Entry> s_stats;
    uint32_t s_version = 0;                  // 0x10BDF384
    uint64_t s_tooltipGuid = 0;              // 0x10BE2EB8
    uint32_t s_inspectedSlot = 0;            // 0x10BE2F08
    std::vector<uint32_t> s_lootRollLevels;  // 0x10BE2F74

    uint32_t U32(const uint8_t* r, uint32_t off) { uint32_t v; memcpy(&v, r + off, 4); return v; }
    int32_t I32(const uint8_t* r, uint32_t off) { int32_t v; memcpy(&v, r + off, 4); return v; }
    float F32(const uint8_t* r, uint32_t off) { float v; memcpy(&v, r + off, 4); return v; }

    void OnLoaded(uint32_t item, uint32_t, void*) {   // FUN_10280500
        CoA::Events::Signal("GET_ITEM_INFO_RECEIVED", "%u%b", item, 1);
        CoA::Events::Signal("AUCTION_ITEM_LIST_UPDATE", nullptr);
        CoA::Events::Signal("AUCTION_BIDDER_LIST_UPDATE", nullptr);
        CoA::Events::Signal("AUCTION_OWNER_LIST_UPDATE", nullptr);
    }

    // FUN_102816e0 GetScalingItemStats(item, level): nil until the record is cached, else FUN_102817e0's JSON object
    // as a table.
    int GetScalingItemStats(lua_State* L) {
        const uint32_t item = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));
        const uint32_t level = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));
        const uint8_t* r = CoA::ItemStats::Get(item, level);

        if (!r) {
            lua_pushnil(L);
            return 1;
        }

        nlohmann::json j = nlohmann::json::object();

        for (uint32_t i = 0; i < 10; ++i) {
            j["ItemStatsType" + std::to_string(i)] = static_cast<int64_t>(I32(r, 0x10 + i * 8));
            j["ItemStatsValue" + std::to_string(i)] = static_cast<int64_t>(I32(r, 0x14 + i * 8));
        }

        for (uint32_t i = 0; i < 2; ++i) {
            j["ItemDamageMin" + std::to_string(i)] = static_cast<double>(F32(r, 0x60 + i * 0xC));
            j["ItemDamageMax" + std::to_string(i)] = static_cast<double>(F32(r, 0x64 + i * 0xC));
        }

        static const char* const kInts[] = { "ItemArmor", "ItemArmorReborn", "ItemHolyRes", "ItemFireRes", "ItemNatureRes",
            "ItemFrostRes", "ItemShadowRes", "ItemArcaneRes" };

        for (uint32_t i = 0; i < 8; ++i) {
            j[kInts[i]] = static_cast<int64_t>(I32(r, 0x78 + i * 4));
        }

        j["ItemBlock"] = static_cast<uint64_t>(U32(r, 0x98));
        j["RandomProperty"] = static_cast<int64_t>(I32(r, 0x9C));
        j["RequiredLevel"] = static_cast<uint64_t>(U32(r, 0xA0));
        j["SellPrice"] = static_cast<uint64_t>(U32(r, 0xA4));
        CoA::Json::Push(L, j);
        return 1;
    }

    // FUN_10281500 GetAuctionItemRequiredLevel(list, index): "bidder" / "owner" / "list". The scaled record's required
    // level, else the item cache's (+0x38), else nil. The bound check lets index == count + 1 through, as the
    // original's does (that entry is one past the list).
    int GetAuctionItemRequiredLevel(lua_State* L) {
        const std::string type = CoA::Script::CheckString(L, 1);
        const int32_t index = ToInt(CheckNumber(L, 2));
        const int list = type == "bidder" ? 0 : type == "owner" ? 1 : type == "list" ? 2 : -1;
        uint32_t count = 0;

        if (list >= 0 && s_deps.auctionList && s_deps.auctionList(list, count) && count != 0 &&
            static_cast<uint32_t>(index - 1) < count + 1) {
            uint32_t item = 0;
            uint32_t level = 0;

            if (s_deps.auctionEntry) {
                s_deps.auctionEntry(list, static_cast<uint32_t>(index - 1), item, level);
            }

            if (const uint8_t* r = CoA::ItemStats::Get(item, level)) {
                CoA::Script::PushInt(L, I32(r, 0xA0));
                return 1;
            }

            int32_t required = 0;

            if (s_deps.itemRequiredLevel && s_deps.itemRequiredLevel(item, required)) {
                CoA::Script::PushInt(L, required);
                return 1;
            }
        }

        lua_pushnil(L);
        return 1;
    }

    // FUN_102817a0 SetInspectedSlot(slot): 1..20 -> 0x10BE2F08 = slot - 1.
    int SetInspectedSlot(lua_State* L) {
        const uint32_t slot = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));

        if (slot - 1 < 0x14) {
            s_inspectedSlot = slot - 1;
        }

        return 0;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "GetScalingItemStats", GetScalingItemStats),
        COA_NATIVE("", "GetAuctionItemRequiredLevel", GetAuctionItemRequiredLevel),
        COA_NATIVE("", "SetInspectedSlot", SetInspectedSlot),
    };
}

CoA::ItemStats::Deps& CoA::ItemStats::Dependencies() {
    return s_deps;
}

const uint8_t* CoA::ItemStats::Get(uint32_t item, uint32_t level) {
    if (item == 0 && level == 0) {
        return nullptr;
    }

    const uint64_t key = static_cast<uint64_t>(level) << 32 | item;
    auto it = s_stats.find(key);

    if (it == s_stats.end()) {
        s_stats[key].waiting.emplace_back(OnLoaded, nullptr);
        CoA::Packet(0x6FF).U64(key).Send();
        return nullptr;
    }

    if (it->second.loaded) {
        return it->second.record;
    }

    it->second.waiting.emplace_back(OnLoaded, nullptr);
    return nullptr;
}

void CoA::ItemStats::CacheVersion(uint32_t version) {
    if (s_version != version) {
        s_stats.clear();
        s_version = version;
    }
}

uint32_t& CoA::ItemStats::Version() {
    return s_version;
}

size_t CoA::ItemStats::Count() {
    return s_stats.size();
}

void CoA::ItemStats::ForEachRecord(void (*cb)(const uint8_t* record, void* ctx), void* ctx) {
    for (auto& kv : s_stats) {
        cb(kv.second.record, ctx);
    }
}

void CoA::ItemStats::InsertLoaded(const uint8_t* record) {
    uint64_t key;
    memcpy(&key, record, 8);
    Entry& e = s_stats[key];
    memcpy(e.record, record, sizeof(e.record));
    e.loaded = true;
}

void CoA::ItemStats::PruneUnloaded() {
    for (auto it = s_stats.begin(); it != s_stats.end();) {
        it = it->second.loaded ? std::next(it) : s_stats.erase(it);
    }
}

void CoA::ItemStats::ClearStore() {
    s_stats.clear();
}

uint64_t& CoA::ItemStats::TooltipGuid() {
    return s_tooltipGuid;
}

uint32_t& CoA::ItemStats::InspectedSlot() {
    return s_inspectedSlot;
}

std::vector<uint32_t>& CoA::ItemStats::LootRollLevels() {
    return s_lootRollLevels;
}

// 0x101AF5B0 reads the record field by field, unchecked (a short packet reads zeros -- CoA::Read).
int32_t CoA::ItemStats::OnItemStats(void*, uint32_t, uint32_t, CDataStore* msg) {
    uint8_t r[0xA8];
    const uint64_t key = CoA::Read::U64(msg);
    memcpy(r, &key, 8);
    const uint32_t a = CoA::Read::U32(msg);
    const uint32_t b = CoA::Read::U32(msg);
    memcpy(r + 8, &a, 4);
    memcpy(r + 0xC, &b, 4);
    CoA::Read::Data(msg, r + 0x10, 0x50);
    CoA::Read::Data(msg, r + 0x60, 0x18);

    for (uint32_t off = 0x78; off < 0xA8; off += 4) {
        const uint32_t v = CoA::Read::U32(msg);
        memcpy(r + off, &v, 4);
    }

    auto it = s_stats.find(key);

    if (it == s_stats.end()) {
        return 1;
    }

    Entry& e = it->second;
    memcpy(e.record, r, sizeof(r));
    e.loaded = true;
    const std::vector<std::pair<Callback, void*>> waiting = std::move(e.waiting);
    e.waiting.clear();

    for (const auto& w : waiting) {
        w.first(static_cast<uint32_t>(key), static_cast<uint32_t>(key >> 32), w.second);
    }

    return 1;
}

// handler_0x073f: the level (CDataStore::GetInt32) for the client's next loot-roll index.
int32_t CoA::ItemStats::OnLootRollAddon(void*, uint32_t, uint32_t, CDataStore* msg) {
    const uint32_t roll = static_cast<uint32_t>(s_deps.nextLootRollId ? s_deps.nextLootRollId() : 0);

    if (s_lootRollLevels.size() <= roll) {
        s_lootRollLevels.resize(static_cast<size_t>(roll) + 1, 0);
    }

    s_lootRollLevels[roll] = static_cast<uint32_t>(CoA::Read::I32(msg));
    return 1;
}

void CoA::ItemStats::Init() {
    COA_PACKET(0x700, CoA::ItemStats::OnItemStats);
    COA_PACKET(0x73F, CoA::ItemStats::OnLootRollAddon);
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    CoA::AttachLua::clientCacheVersion.Add(CoA::ItemStats::CacheVersion);
}

void CoA::ItemStats::Reset() {
    s_deps = Deps();
    s_stats.clear();
    s_version = 0;
    s_tooltipGuid = 0;
    s_inspectedSlot = 0;
    s_lootRollLevels.clear();
}
