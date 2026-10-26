// AscAssetQuery.cpp's client question: the item / quest / creature caches (0x67CA30 / 0x67DE90 / 0x67B6A0 with no
// callback -- as in the original, a miss makes the client cache ask the server itself).
#include "CoA/Source/Modules/AssetQuery.h"
#include "CoA/Source/Modules/Modules.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"

namespace {
    bool Cached(int kind, uint32_t id) {
        const int32_t key = static_cast<int32_t>(id);

        switch (kind) {
            case CoA::AssetQuery::ITEM:
                return g_itemDBCache.GetRecord(key, WOWGUID(), nullptr, nullptr, false) != nullptr;
            case CoA::AssetQuery::QUEST:
                return g_questDBCache.GetRecord(key, WOWGUID(), nullptr, nullptr, false) != nullptr;
            default:
                return g_creatureDBCache.GetRecord(key, WOWGUID(), nullptr, nullptr, false) != nullptr;
        }
    }
}

void CoA::AssetQueryClient::Init() {
    CoA::AssetQuery::Dependencies().cached = Cached;
}
