// AscCustomStore.cpp's client questions: the active player (present, money, held items), the visible units / game
// objects near it (0x4D4B30), the completed-achievement list (0xACFDB0) and a spell's family (FetchSpell +0x240).
#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Modules/CustomStore.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/MysticEnchant.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/AchievementInfo.h"
#include <cmath>
#include <vector>

namespace {
    bool HasPlayer() {
        return ClntObjMgrGetActivePlayerObj() != nullptr;
    }

    uint32_t PlayerMoney() {   // PLAYER_FIELD_COINAGE (+0x1248)
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player ? static_cast<const CGPlayerLocalData*>(player->m_player)->m_playerFieldCoinage : 0;
    }

    uint32_t HeldCount(uint32_t item) {   // FUN_10308470 (bags) + FUN_10308570 (currency-token slots)
        return CoA::MysticEnchant::BagItemCount(item) + CoA::CA::UnitItemCount(ClntObjMgrGetActivePlayer().m_guid, item);
    }

    // FUN_101a3d50 / FUN_101a3bf0: collect the visible units (mask 8) / game objects (0x20), then the first of `entry`
    // within 5.5 yd of the player (the distance through pow / sqrt in double, compared as float).
    struct Collect {
        OBJECT_TYPE mask;
        std::vector<CGObject_C*> objects;
    };

    int32_t CollectObject(WOWGUID guid, void* param) {
        Collect* c = static_cast<Collect*>(param);

        if (CGObject_C* o = ClntObjMgrObjectPtr(guid, c->mask, __FILE__, __LINE__)) {
            c->objects.push_back(o);
        }

        return 1;
    }

    bool Nearby(uint32_t entry, bool gameObject) {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (!player) {
            return false;
        }

        const C3Vector p = player->GetPosition();
        Collect c{ gameObject ? TYPE_GAMEOBJECT : TYPE_UNIT, {} };
        ClntObjMgrEnumVisibleObjects(CollectObject, &c);

        for (CGObject_C* o : c.objects) {
            const C3Vector q = o->GetPosition();
            const double d = std::sqrt(std::pow(q.x - p.x, 2.0) + std::pow(q.y - p.y, 2.0) + std::pow(q.z - p.z, 2.0));

            if (static_cast<float>(d) <= 5.5f && o->m_obj->m_entryID == entry) {
                return true;
            }
        }

        return false;
    }

    bool AchievementDone(uint32_t id) {   // FUN_103087e0 over 0xACFDB0
        for (CLIENT_ACHIEVEMENTS* node = s_completedAchievements.Head(); node; node = s_completedAchievements.Next(node)) {
            if (static_cast<uint32_t>(node->m_id) == id) {
                return true;
            }
        }

        return false;
    }

    bool SpellFamily(uint32_t spell, uint32_t& family) {
        SpellRec rec;

        if (!g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec)) {
            return false;
        }

        family = static_cast<uint32_t>(rec.m_spellClassSet);
        return true;
    }
}

void CoA::CustomStoreClient::Init() {
    CoA::CustomStore::Deps& deps = CoA::CustomStore::Dependencies();
    deps.hasPlayer = HasPlayer;
    deps.playerMoney = PlayerMoney;
    deps.heldCount = HeldCount;
    deps.nearby = Nearby;
    deps.achievementDone = AchievementDone;
    deps.spellFamily = SpellFamily;
}
