/*
 * mod-barde - concerts: the purse on the ground.
 * Licensed under the GNU AGPL v3, see LICENSE.
 *
 * A bard who plays a song (outside training) at a camp or in a rest area (a tavern, a city) gives a concert: a purse appears at his feet.
 * The spectators click on it and leave a tip (their own gold, taken when they confirm). The bard clicks on it to collect: the tips, plus
 * a bonus for the applause (+2 % per spectator who applauds, up to +20 %) and, at a camp, a camp bonus (+10 %). A purse that nobody
 * collects is mailed to the bard a few minutes after his last song.
 * The players who listen to a concert in a tavern get a part of his chant (the camp has its own, see BardeCamp.cpp).
 */

#include "Barde.h"

#include "Chat.h"
#include "DatabaseEnv.h"
#include "GameObject.h"
#include "Mail.h"
#include "Map.h"
#include "MapMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "WorldSession.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <mutex>
#include <set>
#include <unordered_map>

namespace Barde::Concert
{

namespace
{

constexpr uint32 SENDER_TIP = 1, SENDER_COLLECT = 2, SENDER_APPLAUD = 3;
constexpr uint32 TIPS[3] = { 1000, 10000, 50000 };              // 10 silver, 1 gold, 5 gold
constexpr uint32 MAIL_SENDER_ENTRY = 6740;                      // an innkeeper, as the sender of the mail

struct Purse
{
    ObjectGuid bard;
    ObjectGuid go;
    uint32 mapId = 0;
    time_t startedAt = 0;
    time_t endsAt = 0;              // 0 while the bard is still playing
    uint64 tips = 0;
    bool camp = false;
    std::set<ObjectGuid::LowType> applauders;
    uint32 extraSeconds = 0;        // how much the ovations have lengthened the listeners' chants
    std::set<ObjectGuid::LowType> tippers;
};

std::mutex sLock;
std::unordered_map<ObjectGuid::LowType, Purse> sPurses;         // per bard

std::string Coins(uint64 copper)
{
    std::string text;
    if (uint64 gold = copper / 10000)
        text += std::to_string(gold) + " po ";
    if (uint64 silver = (copper / 100) % 100)
        text += std::to_string(silver) + " pa ";
    if (uint64 rest = copper % 100)
        text += std::to_string(rest) + " pc";
    if (text.empty())
        text = "0";
    while (!text.empty() && text.back() == ' ')
        text.pop_back();
    return text;
}

void Say(Player* player, std::string const& text)
{
    if (player && player->GetSession())
        ChatHandler(player->GetSession()).SendSysMessage(text);
}

// What the bard gets for the tips: the applause and the camp add to them.
uint64 Bonus(Purse const& purse)
{
    Config const& config = GetConfig();
    if (!config.bonusMaxPerConcert || !config.bonusMaxPerDay)
        return 0;
    uint32 percent = std::min(config.applauseMax, config.applausePercent * uint32(purse.applauders.size()));
    if (purse.camp)
        percent += config.campTipPercent;
    return std::min<uint64>(purse.tips * percent / 100, config.bonusMaxPerConcert);
}

uint64 Total(Purse const& purse)
{
    return purse.tips + Bonus(purse);
}

// What the bard is paid: the tips, and the bonus within what is left of his day. The bonus used is written down.
uint64 Payout(Purse const& purse, ObjectGuid::LowType bardLow, bool& limited)
{
    limited = false;
    uint64 bonus = Bonus(purse);
    if (bonus)
    {
        uint32 const day = uint32(time(nullptr) / 86400);
        uint64 used = 0;
        if (QueryResult result = CharacterDatabase.Query("SELECT copper FROM mod_barde_char_bonus WHERE guid = {} AND day = {}", bardLow, day))
            used = result->Fetch()[0].Get<uint32>();
        uint64 const left = GetConfig().bonusMaxPerDay > used ? GetConfig().bonusMaxPerDay - used : 0;
        if (bonus > left)
        {
            bonus = left;
            limited = true;
        }
        if (bonus)
            CharacterDatabase.Execute("REPLACE INTO mod_barde_char_bonus (guid, day, copper) VALUES ({}, {}, {})", bardLow, day, uint32(used + bonus));
    }
    if (purse.tips)
        Titles::Add(bardLow, "tips", purse.tips);               // the tips the bard really received count for his titles
    return purse.tips + bonus;
}

// An ovation: the applause of a spectator lengthens, for every listener of the concert, the chant the bard gave them, and it is counted for the bard.
void Ovation(Purse& purse, Player* spectator)
{
    Config const& config = GetConfig();
    Player* bard = ObjectAccessor::FindConnectedPlayer(purse.bard);
    if (!bard)
        return;

    uint32 const weight = Spec::OvationWeight(bard);                // a court minstrel's applause counts double
    Titles::Add(purse.bard.GetCounter(), "ovations", weight);

    uint32 add = config.ovationSeconds * weight;
    if (purse.extraSeconds + add > config.ovationMaxSeconds)
        add = config.ovationMaxSeconds > purse.extraSeconds ? config.ovationMaxSeconds - purse.extraSeconds : 0;
    if (add && bard->GetMap())
    {
        GameObject* go = bard->GetMap()->GetGameObject(purse.go);
        float const range = std::max(config.concertRange, config.campRange);
        uint32 lengthened = 0;
        for (Player* listener : go ? PlayersAround(go, range) : std::vector<Player*>())
            for (Instrument const* instrument : Store::Instance().GetInstruments())
                if (Aura* aura = listener->GetAura(instrument->buffSpell, bard->GetGUID()))
                {
                    aura->SetMaxDuration(aura->GetMaxDuration() + add * IN_MILLISECONDS);
                    aura->SetDuration(aura->GetDuration() + add * IN_MILLISECONDS);
                    ++lengthened;
                }
        if (lengthened)
            purse.extraSeconds += add;
    }
    Titles::Check(bard);
    (void)spectator;
}

// Players of the same account: the check that stops somebody from feeding his own purse with a second character.
bool SameAccount(Player* a, ObjectGuid bard)
{
    if (GetConfig().allowSameAccount)
        return false;
    Player* other = ObjectAccessor::FindConnectedPlayer(bard);
    return other && a->GetSession() && other->GetSession() && a->GetSession()->GetAccountId() == other->GetSession()->GetAccountId();
}

Purse* FindByGo(ObjectGuid go)
{
    for (auto& [bard, purse] : sPurses)
        if (purse.go == go)
            return &purse;
    return nullptr;
}

// Takes the purse away and gives the bard his money: into his hands, or by mail when he is not there.
void Close(ObjectGuid::LowType bardLow, bool mail)
{
    auto itr = sPurses.find(bardLow);
    if (itr == sPurses.end())
        return;
    Purse purse = itr->second;
    sPurses.erase(itr);

    if (Map* map = sMapMgr->FindMap(purse.mapId, 0))
        if (GameObject* go = map->GetGameObject(purse.go))
            go->Delete();

    bool limited = false;
    uint64 const total = mail ? Payout(purse, bardLow, limited) : 0;
    if (mail && total > 0)
    {
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        MailDraft("Pourboires de votre concert", "Voici ce que vos spectateurs ont laissé dans votre bourse : " + Coins(total) + ".")
            .AddMoney(uint32(std::min<uint64>(total, 0x7FFFFFFF)))
            .SendMailTo(trans, MailReceiver(nullptr, bardLow), MailSender(MAIL_CREATURE, MAIL_SENDER_ENTRY));
        CharacterDatabase.CommitTransaction(trans);
    }
}

} // namespace

bool Eligible(Player* player)
{
    Map* map = player->GetMap();
    if (!map || map->Instanceable())
        return false;
    return Camp::IsAtCamp(player) || player->HasRestFlag(REST_FLAG_IN_TAVERN) || player->HasRestFlag(REST_FLAG_IN_CITY);
}

void Begin(Player* bard, Song const& song)
{
    Config const& config = GetConfig();
    if (!config.concertEnable || !Eligible(bard))
        return;

    std::lock_guard<std::mutex> guard(sLock);
    auto existing = sPurses.find(bard->GetGUID().GetCounter());
    if (existing != sPurses.end())
    {
        existing->second.endsAt = 0;            // another song for the same concert
        return;
    }

    float const o = bard->GetOrientation();
    float x = bard->GetPositionX() + 1.3f * std::cos(o);
    float y = bard->GetPositionY() + 1.3f * std::sin(o);
    float z = bard->GetPositionZ();
    bard->UpdateGroundPositionZ(x, y, z);
    GameObject* go = bard->SummonGameObject(CONCERT_PURSE, x, y, z, o, 0.0f, 0.0f, std::sin(o / 2), std::cos(o / 2),
        config.concertMaxSec, false);
    if (!go)
        return;

    Purse purse;
    purse.bard = bard->GetGUID();
    purse.go = go->GetGUID();
    purse.mapId = bard->GetMapId();
    purse.startedAt = time(nullptr);
    purse.camp = Camp::IsAtCamp(bard);
    sPurses[bard->GetGUID().GetCounter()] = purse;
    Say(bard, "Une bourse est posée devant vous pendant votre concert : les spectateurs peuvent y laisser un pourboire. "
        "Cliquez dessus pour la ramasser quand vous avez fini.");
    (void)song;
}

void End(Player* bard)
{
    std::lock_guard<std::mutex> guard(sLock);
    auto itr = sPurses.find(bard->GetGUID().GetCounter());
    if (itr != sPurses.end())
        itr->second.endsAt = time(nullptr) + GetConfig().concertGraceSec;
}

void Update()
{
    time_t const now = time(nullptr);
    std::lock_guard<std::mutex> guard(sLock);
    std::vector<ObjectGuid::LowType> due;
    for (auto const& [bard, purse] : sPurses)
        if ((purse.endsAt && now >= purse.endsAt) || now >= purse.startedAt + GetConfig().concertMaxSec)
            due.push_back(bard);
    for (ObjectGuid::LowType bard : due)
        Close(bard, true);
}

void Applaud(Player* player)
{
    std::lock_guard<std::mutex> guard(sLock);
    for (auto& [bardLow, purse] : sPurses)
    {
        if (purse.bard == player->GetGUID() || purse.mapId != player->GetMapId())
            continue;
        Map* map = player->GetMap();
        GameObject* go = map ? map->GetGameObject(purse.go) : nullptr;
        if (go && player->IsWithinDist(go, 12.0f))
        {
            if (SameAccount(player, purse.bard))
                return;
            if (purse.applauders.insert(player->GetGUID().GetCounter()).second)
            {
                Say(player, "Vos applaudissements prolongent le chant des auditeurs du concert.");
                Ovation(purse, player);
            }
            return;
        }
    }
}

std::vector<Player*> Listeners(Player* bard, float& fraction)
{
    Config const& config = GetConfig();
    if (Camp::IsAtCamp(bard))
    {
        fraction = config.campListenerFraction;
        return Camp::Listeners(bard);
    }
    std::vector<Player*> listeners;
    fraction = config.concertListenerFraction;
    if (!config.concertEnable || fraction <= 0.0f || !Eligible(bard))
        return listeners;
    for (Player* other : PlayersAround(bard, config.concertRange * Spec::RangeFactor(bard)))
        if (other != bard && other->IsAlive())
            listeners.push_back(other);
    return listeners;
}

bool Hello(Player* player, GameObject* go)
{
    std::lock_guard<std::mutex> guard(sLock);
    Purse* purse = FindByGo(go->GetGUID());
    if (!purse)
    {
        go->Delete();
        return true;
    }

    ClearGossipMenuFor(player);
    if (purse->bard == player->GetGUID())
    {
        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Ramasser la bourse : " + Coins(Total(*purse)) + " ("
            + std::to_string(purse->applauders.size()) + " applaudissement(s))", SENDER_COLLECT, 0);
    }
    else
    {
        static char const* const labels[3] = { "Laisser 10 pièces d'argent", "Laisser 1 pièce d'or", "Laisser 5 pièces d'or" };
        for (uint32 i = 0; i < 3; ++i)
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, labels[i], SENDER_TIP, i, "", TIPS[i], false);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Applaudir : une ovation pour le barde, et son chant dure plus longtemps pour le public", SENDER_APPLAUD, 0);
        if (!player->HasEnoughMoney(TIPS[0]))
            Say(player, "Vous n'avez pas d'argent à laisser. Applaudissez (/applaud, /applaudir en français, ou le menu de la bourse) pour encourager le barde : "
                "chaque ovation compte pour ses titres, et prolonge le chant de tous les auditeurs du concert.");
    }
    SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, go->GetGUID());
    return true;
}

bool Select(Player* player, GameObject* go, uint32 sender, uint32 action)
{
    CloseGossipMenuFor(player);
    std::lock_guard<std::mutex> guard(sLock);
    Purse* purse = FindByGo(go->GetGUID());
    if (!purse)
        return true;

    if (sender == SENDER_COLLECT)
    {
        if (purse->bard != player->GetGUID())
            return true;
        ObjectGuid::LowType const bardLow = player->GetGUID().GetCounter();
        // The money comes into the bard's hands; the purse goes (Close with no mail).
        Purse copy = *purse;
        bool limited = false;
        uint64 const total = Payout(copy, bardLow, limited);
        Close(bardLow, false);
        if (total > 0)
        {
            player->ModifyMoney(int64(std::min<uint64>(total, 0x7FFFFFFF)));
            Say(player, "Vous ramassez la bourse : " + Coins(total) + " (pourboires : " + Coins(copy.tips) + ")."
                + (limited ? " Votre bonus de concert du jour est au plafond." : ""));
        }
        else
            Say(player, "La bourse est vide.");
        return true;
    }

    if (sender == SENDER_APPLAUD)
    {
        if (purse->bard == player->GetGUID())
            return true;
        player->HandleEmoteCommand(EMOTE_ONESHOT_APPLAUD);
        if (SameAccount(player, purse->bard))
            Say(player, "Vous applaudissez, mais votre propre compte ne compte pas.");
        else if (purse->applauders.insert(player->GetGUID().GetCounter()).second)
        {
            Say(player, "Vous applaudissez : le chant des auditeurs du concert est prolongé.");
            if (Player* bard = ObjectAccessor::FindConnectedPlayer(purse->bard))
                Say(bard, player->GetName() + " vous applaudit : une ovation de plus.");
            Ovation(*purse, player);
        }
        else
            Say(player, "Vous avez déjà applaudi ce concert.");
        return true;
    }

    if (sender != SENDER_TIP || action >= 3 || purse->bard == player->GetGUID())
        return true;
    if (SameAccount(player, purse->bard))
    {
        Say(player, "Vous ne pouvez pas laisser de pourboire à un personnage de votre propre compte.");
        return true;
    }
    uint32 const amount = TIPS[action];
    if (!player->HasEnoughMoney(amount))
    {
        Say(player, "Vous n'avez pas assez d'argent.");
        return true;
    }
    player->ModifyMoney(-int64(amount));
    purse->tips += amount;
    purse->tippers.insert(player->GetGUID().GetCounter());
    Say(player, "Vous laissez " + Coins(amount) + " dans la bourse. Merci pour le barde !");
    if (Player* bard = ObjectAccessor::FindConnectedPlayer(purse->bard))
        Say(bard, player->GetName() + " laisse " + Coins(amount) + " dans votre bourse (total : " + Coins(Total(*purse)) + ").");
    return true;
}

void SettleDuel(ObjectGuid winner, ObjectGuid loser, bool draw)
{
    std::lock_guard<std::mutex> guard(sLock);
    auto w = sPurses.find(winner.GetCounter());
    auto l = sPurses.find(loser.GetCounter());
    uint64 const total = (w != sPurses.end() ? w->second.tips : 0) + (l != sPurses.end() ? l->second.tips : 0);
    if (!total)
        return;
    if (w != sPurses.end())
        w->second.tips = draw ? total / 2 : total;
    if (l != sPurses.end())
        l->second.tips = draw ? total - total / 2 : 0;
    // A winner who gave no concert (no purse) cannot take the loser's tips: they stay where they are.
    if (w == sPurses.end() && l != sPurses.end())
        l->second.tips = total;
}

void Forget(Player* bard)
{
    // A bard who logs out: his purse is mailed to him.
    std::lock_guard<std::mutex> guard(sLock);
    Close(bard->GetGUID().GetCounter(), true);
}

} // namespace Barde::Concert
