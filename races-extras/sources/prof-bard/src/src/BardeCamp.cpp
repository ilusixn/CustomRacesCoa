/*
 * mod-barde - the bard's camp.
 * Licensed under the GNU AGPL v3, see LICENSE.
 *
 * A bard makes a camp kit at the workshop and uses it: a campfire and two benches appear for Barde.Camp.Seconds (30 minutes),
 * one camp per zone, one per bard every Barde.Camp.Cooldown seconds. The fire is a cooking fire like the world's campfires, so
 * anybody can cook next to it. For the bards, it is a place to play:
 *   - the instruments of a bard near the fire tune themselves up, a little every five seconds (no tuning key needed);
 *   - the players near the fire who listen to a bard who plays there get a share of his chant (Barde.Camp.ListenerFraction).
 * The camp is lost on a server restart: it is made to last half an hour, not to stay.
 */

#include "Barde.h"

#include "GameObject.h"
#include "Map.h"
#include "MapMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "WorldSession.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <mutex>
#include <unordered_map>

namespace Barde::Camp
{

namespace
{

struct Info
{
    ObjectGuid owner;
    ObjectGuid fire;
    uint32 mapId = 0;
    uint32 zoneId = 0;
    float x = 0, y = 0, z = 0;
    time_t endsAt = 0;
};

std::mutex sLock;
std::vector<Info> sCamps;
std::unordered_map<ObjectGuid::LowType, time_t> sReadyAt;       // per bard: when he may make another camp
uint32 sTicks = 0;

} // namespace

std::string Place(Player* player)
{
    Config const& config = GetConfig();
    if (!config.enable)
        return "Le métier de barde est désactivé sur ce serveur.";
    if (config.requireProfession && !player->HasSpell(PROFESSION_SPELL))
        return "Vous n'avez pas appris le métier de barde.";
    if (!player->IsAlive() || player->IsInCombat())
        return "Impossible de poser un camp maintenant.";
    Map* map = player->GetMap();
    if (!map || map->Instanceable())
        return "On ne pose pas de camp dans un donjon ou un champ de bataille.";
    if (player->IsMounted() || player->IsInFlight() || player->IsInWater())
        return "Impossible de poser un camp ici.";

    time_t const now = time(nullptr);
    std::lock_guard<std::mutex> guard(sLock);
    sCamps.erase(std::remove_if(sCamps.begin(), sCamps.end(), [now](Info const& camp) { return camp.endsAt <= now; }), sCamps.end());

    auto ready = sReadyAt.find(player->GetGUID().GetCounter());
    if (ready != sReadyAt.end() && ready->second > now)
        return "Vous ne pouvez pas refaire de camp avant " + std::to_string((ready->second - now + 59) / 60) + " minute(s).";
    for (Info const& camp : sCamps)
        if (camp.mapId == player->GetMapId() && camp.zoneId == player->GetZoneId())
            return "Il y a déjà un camp de barde dans cette zone.";

    float const o = player->GetOrientation();
    float x = player->GetPositionX() + 2.5f * std::cos(o);
    float y = player->GetPositionY() + 2.5f * std::sin(o);
    float z = player->GetPositionZ();
    player->UpdateGroundPositionZ(x, y, z);

    uint32 const seconds = config.campSeconds;
    GameObject* fire = player->SummonGameObject(CAMP_FIRE, x, y, z, o, 0.0f, 0.0f, std::sin(o / 2), std::cos(o / 2), seconds, false);
    if (!fire)
        return "Le camp n'a pas pu être posé ici.";

    // Two benches on the sides of the fire, turned towards it.
    for (float side : { 1.0f, -1.0f })
    {
        float const angle = o + side * float(M_PI) / 2;
        float const bx = x + 2.6f * std::cos(angle);
        float const by = y + 2.6f * std::sin(angle);
        float bz = z;
        player->UpdateGroundPositionZ(bx, by, bz);
        float const facing = angle + float(M_PI);
        player->SummonGameObject(CAMP_BENCH, bx, by, bz, facing, 0.0f, 0.0f, std::sin(facing / 2), std::cos(facing / 2), seconds, false);
    }

    Info info;
    info.owner = player->GetGUID();
    info.fire = fire->GetGUID();
    info.mapId = player->GetMapId();
    info.zoneId = player->GetZoneId();
    info.x = x;
    info.y = y;
    info.z = z;
    info.endsAt = now + seconds;
    sCamps.push_back(info);
    sReadyAt[player->GetGUID().GetCounter()] = now + config.campCooldownSec;
    return {};
}

bool IsAtCamp(Player* player)
{
    float const range = GetConfig().campRange;
    time_t const now = time(nullptr);
    std::lock_guard<std::mutex> guard(sLock);
    for (Info const& camp : sCamps)
        if (camp.endsAt > now && camp.mapId == player->GetMapId() && player->GetExactDist(camp.x, camp.y, camp.z) <= range)
            return true;
    return false;
}

std::vector<Player*> Listeners(Player* bard)
{
    std::vector<Player*> listeners;
    Info found;
    {
        float const range = GetConfig().campRange;
        time_t const now = time(nullptr);
        std::lock_guard<std::mutex> guard(sLock);
        bool any = false;
        for (Info const& camp : sCamps)
            if (!any && camp.endsAt > now && camp.mapId == bard->GetMapId() && bard->GetExactDist(camp.x, camp.y, camp.z) <= range)
            {
                found = camp;
                any = true;
            }
        if (!any)
            return listeners;
    }
    Map* map = bard->GetMap();
    GameObject* fire = map ? map->GetGameObject(found.fire) : nullptr;
    if (!fire)
        return listeners;
    for (Player* other : PlayersAround(fire, GetConfig().campRange))
        if (other != bard && other->IsAlive())
            listeners.push_back(other);
    return listeners;
}

void Update()
{
    time_t const now = time(nullptr);
    std::vector<Info> active;
    {
        std::lock_guard<std::mutex> guard(sLock);
        if (sCamps.empty())
            return;
        sCamps.erase(std::remove_if(sCamps.begin(), sCamps.end(), [now](Info const& camp) { return camp.endsAt <= now; }), sCamps.end());
        active = sCamps;
    }

    // Every five seconds, the instruments of the bards at the fire get a little more in tune.
    if (++sTicks % 5)
        return;
    for (Info const& camp : active)
    {
        Map* map = sMapMgr->FindMap(camp.mapId, 0);
        GameObject* fire = map ? map->GetGameObject(camp.fire) : nullptr;
        if (!fire)
            continue;
        for (Player* player : PlayersAround(fire, GetConfig().campRange))
            if (player->IsAlive())
                Atelier::TuneStep(player, GetConfig().campTune);
    }
}

} // namespace Barde::Camp
