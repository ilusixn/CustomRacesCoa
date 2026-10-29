/*
 * mod-barde - the bard's specializations.
 * Licensed under the GNU AGPL v3, see LICENSE.
 *
 * From skill 180 a bard chooses a path (the first choice is free, changing it costs 10 gold). No path gives money: each one bends the chants
 * or the audience.
 *   war bard      +10 % on the seven chants of battle (crumhorn, fiddle, piano, organ, ocarina, timpani, drum)
 *   storyteller   +10 % on the lute, harp and flute chants, and the listeners' share (the good mood) is doubled
 *   court minstrel the range at which one is heard x1.5, 16 listeners, the good mood x1.5, and an applause counts double
 */

#include "Barde.h"

#include "DatabaseEnv.h"
#include "Player.h"

#include <mutex>
#include <unordered_map>

namespace Barde::Spec
{

namespace
{

std::mutex sLock;
std::unordered_map<ObjectGuid::LowType, uint32> sCache;

bool IsWar(std::string const& sound)
{
    return sound == "crom" || sound == "viele" || sound == "piano" || sound == "orgue" || sound == "ocar" || sound == "tymp" || sound == "tamb";
}

bool IsTale(std::string const& sound)
{
    return sound == "luth" || sound == "harpe" || sound == "flute";
}

} // namespace

uint32 Get(Player* player)
{
    ObjectGuid::LowType const guid = player->GetGUID().GetCounter();
    {
        std::lock_guard<std::mutex> guard(sLock);
        auto itr = sCache.find(guid);
        if (itr != sCache.end())
            return itr->second;
    }
    uint32 spec = 0;
    if (QueryResult result = CharacterDatabase.Query("SELECT spec FROM mod_barde_char_stats WHERE guid = {}", guid))
        spec = result->Fetch()[0].Get<uint8>();
    if (spec > TALE)
        spec = NONE;
    std::lock_guard<std::mutex> guard(sLock);
    sCache[guid] = spec;
    return spec;
}

void Forget(Player* player)
{
    std::lock_guard<std::mutex> guard(sLock);
    sCache.erase(player->GetGUID().GetCounter());
}

float ChantFactor(Player* player, Instrument const& instrument)
{
    uint32 const spec = Get(player);
    float const bonus = 1.0f + 0.01f * float(GetConfig().specChantPercent);
    if ((spec == WAR && IsWar(instrument.sound)) || (spec == TALE && IsTale(instrument.sound)))
        return bonus;
    return 1.0f;
}

float MoodFactor(Player* player)
{
    switch (Get(player))
    {
        case TALE:  return 2.0f;
        case COURT: return 1.5f;
        default:    return 1.0f;
    }
}

float RangeFactor(Player* player)
{
    return Get(player) == COURT ? 1.5f : 1.0f;
}

uint32 ListenMax(Player* player)
{
    return Get(player) == COURT ? std::max<uint32>(GetConfig().listenMax, 16) : GetConfig().listenMax;
}

uint32 OvationWeight(Player* player)
{
    return Get(player) == COURT ? 2 : 1;
}

std::string Choose(Player* player, uint32 spec)
{
    Config const& config = GetConfig();
    if (spec < WAR || spec > TALE)
        return "Spécialisation inconnue.";
    if (Session::GetSkill(player) < config.specMinSkill)
        return "Il faut une compétence de barde de " + std::to_string(config.specMinSkill) + " pour se spécialiser.";

    uint32 const current = Get(player);
    if (current == spec)
        return "Vous avez déjà cette spécialisation.";
    uint32 const cost = current ? config.specChangeCost : 0;
    if (cost && !player->HasEnoughMoney(cost))
        return "Changer de spécialisation coûte 10 pièces d'or.";

    if (cost)
        player->ModifyMoney(-int64(cost));
    CharacterDatabase.Execute("INSERT INTO mod_barde_char_stats (guid, spec) VALUES ({0}, {1}) ON DUPLICATE KEY UPDATE spec = {1}",
        player->GetGUID().GetCounter(), spec);
    {
        std::lock_guard<std::mutex> guard(sLock);
        sCache[player->GetGUID().GetCounter()] = spec;
    }
    return {};
}

// SPEC^current^minSkill^changeCost(copper)
void Send(Player* player)
{
    Config const& config = GetConfig();
    Protocol::Send(player, "SPEC^" + std::to_string(Get(player)) + '^' + std::to_string(config.specMinSkill) + '^' + std::to_string(config.specChangeCost));
}

} // namespace Barde::Spec
