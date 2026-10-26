// AscAssetQuery.cpp (see AssetQuery.h): C_AssetQueryService.TryCacheItem / TryCacheQuest / TryCacheCreature.
#include "CoA/Source/Modules/AssetQuery.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/MysticEnchant.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <ctime>
#include <unordered_set>
#include <vector>

namespace {
    struct InFlight {
        uint32_t id;
        int64_t sent;
        bool succeeded;
        bool failed;
    };

    struct KindState {
        uint32_t opcode;
        const char* success;
        const char* failure;
        std::vector<uint32_t> queue;
        std::unordered_set<uint32_t> seen;
        uint32_t batches = 0;
        std::vector<InFlight> inFlight;
    };

    CoA::AssetQuery::Deps s_deps;
    KindState s_kinds[3] = {
        { 0x61B, "ITEM_CACHE_REQUEST_SUCCESS", "ITEM_CACHE_REQUEST_FAILURE" },
        { 0x61C, "QUEST_CACHE_REQUEST_SUCCESS", "QUEST_CACHE_REQUEST_FAILURE" },
        { 0x61A, "CREATURE_CACHE_REQUEST_SUCCESS", "CREATURE_CACHE_REQUEST_FAILURE" },
    };
    int64_t s_second = 0;   // +0x90

    int64_t Now() {
        return s_deps.now ? s_deps.now() : _time64(nullptr);
    }

    bool Cached(int kind, uint32_t id) {
        return s_deps.cached && s_deps.cached(kind, id);
    }

    void SendBatch(KindState& k) {
        if (k.queue.empty() || (!CoA::RealmInfo::Get().Dev() && k.batches >= 3)) {
            return;
        }

        const size_t n = std::min<size_t>(k.queue.size(), 50);
        std::vector<uint32_t> ids;

        for (size_t i = 0; i < n; ++i) {
            ids.push_back(k.queue[i]);
            k.inFlight.push_back({ k.queue[i], Now(), false, false });
        }

        k.queue.erase(k.queue.begin(), k.queue.begin() + static_cast<std::ptrdiff_t>(n));
        CoA::Packet p(k.opcode);   // FUN_1021c060 / FUN_1021e870 / FUN_1021b100
        p.U32(static_cast<uint32_t>(ids.size()));

        for (uint32_t id : ids) {
            p.U32(id);
        }

        p.Send();
        ++k.batches;
    }

    void CheckInFlight(int kind) {
        KindState& k = s_kinds[kind];

        if (k.inFlight.empty()) {
            return;
        }

        const int64_t now = Now();

        for (InFlight& f : k.inFlight) {
            if (now - f.sent < 31) {
                if (Cached(kind, f.id)) {
                    f.succeeded = true;
                    CoA::Events::Signal(k.success, "%u", f.id);
                }
            } else {
                k.seen.erase(f.id);
                f.failed = true;
                CoA::Events::Signal(k.failure, "%u", f.id);
            }
        }

        k.inFlight.erase(std::remove_if(k.inFlight.begin(), k.inFlight.end(),
            [](const InFlight& f) { return f.succeeded || f.failed; }), k.inFlight.end());
    }

    int TryCacheLua(lua_State* L, int kind, const char* usage) {
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            luaL_error(L, usage);
            return 0;
        }

        CoA::Script::PushBool(L, CoA::AssetQuery::TryCache(kind, id));
        return 1;
    }

    int TryCacheItem(lua_State* L) {   // FUN_100e28b0
        return TryCacheLua(L, CoA::AssetQuery::ITEM, "Usage: TryCacheItem(itemId)");
    }

    int TryCacheQuest(lua_State* L) {   // FUN_100e2910
        return TryCacheLua(L, CoA::AssetQuery::QUEST, "Usage: TryCacheQuest(questId)");
    }

    int TryCacheCreature(lua_State* L) {   // FUN_100e2850
        return TryCacheLua(L, CoA::AssetQuery::CREATURE, "Usage: TryCacheCreature(creatureId)");
    }

    void CacheMysticItem(uint32_t id) {   // MysticEnchant's prefetch of the untarnished scroll (0x102EA2A0)
        CoA::AssetQuery::TryCacheItem(id);
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_AssetQueryService", "TryCacheItem", TryCacheItem),
        COA_NATIVE("C_AssetQueryService", "TryCacheQuest", TryCacheQuest),
        COA_NATIVE("C_AssetQueryService", "TryCacheCreature", TryCacheCreature),
    };
}

CoA::AssetQuery::Deps& CoA::AssetQuery::Dependencies() {
    return s_deps;
}

bool CoA::AssetQuery::TryCache(int kind, uint32_t id) {
    KindState& k = s_kinds[kind];

    if (Cached(kind, id)) {
        return false;
    }

    if (k.seen.insert(id).second) {
        k.queue.push_back(id);
        return true;
    }

    auto it = std::find(k.queue.begin(), k.queue.end(), id);

    if (it != k.queue.end()) {
        k.queue.erase(it);
        k.queue.insert(k.queue.begin(), id);
    }

    return false;
}

bool CoA::AssetQuery::TryCacheItem(uint32_t id) {
    return CoA::AssetQuery::TryCache(ITEM, id);
}

void CoA::AssetQuery::Tick() {
    const int64_t now = Now();

    if (s_second < now) {
        s_second = Now();

        for (KindState& k : s_kinds) {
            k.batches = 0;
        }
    }

    for (KindState& k : s_kinds) {
        SendBatch(k);
    }

    for (int kind = 0; kind < 3; ++kind) {
        CheckInFlight(kind);
    }
}

void CoA::AssetQuery::OnGlueScreen() {
    for (KindState& k : s_kinds) {
        k.queue.clear();
        k.seen.clear();
        k.batches = 0;
        k.inFlight.clear();
    }

    s_second = 0;
}

void CoA::AssetQuery::Init() {
    CoA::Lifecycle::OnAfterIdle(CoA::AssetQuery::Tick);
    CoA::Lifecycle::OnGlueScreen(CoA::AssetQuery::OnGlueScreen);
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    CoA::MysticEnchant::Dependencies().cacheItem = CacheMysticItem;
    COA_EVENT("ITEM_CACHE_REQUEST_SUCCESS");
    COA_EVENT("ITEM_CACHE_REQUEST_FAILURE");
    COA_EVENT("QUEST_CACHE_REQUEST_SUCCESS");
    COA_EVENT("QUEST_CACHE_REQUEST_FAILURE");
    COA_EVENT("CREATURE_CACHE_REQUEST_SUCCESS");
    COA_EVENT("CREATURE_CACHE_REQUEST_FAILURE");
}

void CoA::AssetQuery::Reset() {
    s_deps = Deps();
    CoA::AssetQuery::OnGlueScreen();
}
