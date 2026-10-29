/*
 * mod-barde - duels between two bards.
 * Licensed under the GNU AGPL v3, see LICENSE.
 *
 * A bard selects another bard and challenges him to a song. If he accepts, both play the same song at the same moment (each with the
 * instrument of his choice), checked like any other song. The better score wins; a gap under 0.5 % is a draw. The tips that the
 * spectators left in the two bards' purses (see BardeConcert.cpp) go to the winner, or are shared in two in a draw.
 */

#include "Barde.h"

#include "Chat.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Timer.h"
#include "WorldSession.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <mutex>
#include <unordered_map>

namespace Barde::Duel
{

namespace
{

constexpr uint32 RESULT_WAIT_MS = 20000;
constexpr float DUEL_RANGE = 40.0f;
constexpr float DRAW_GAP = 0.005f;

struct Duelist
{
    ObjectGuid guid;
    uint32 instrumentId = 0;
    bool joined = false;
    bool reported = false;
    bool dropped = false;
    uint32 score = 0;
};

struct Match
{
    uint32 id = 0;
    Duelist a, b;                       // a: the challenger
    uint32 songId = 0;
    bool playing = false;
    uint32 createdMs = 0;
    uint32 startMs = 0;
    uint32 durationMs = 0;
};

std::recursive_mutex sLock;
std::unordered_map<uint32, Match> sMatches;
std::unordered_map<ObjectGuid, uint32> sByPlayer;
uint32 sNextId = 1;

Player* Find(ObjectGuid guid)
{
    return ObjectAccessor::FindConnectedPlayer(guid);
}

void Say(Player* player, std::string const& text)
{
    if (player && player->GetSession())
        ChatHandler(player->GetSession()).SendSysMessage(text);
}

Duelist* Other(Match& m, ObjectGuid guid)
{
    return m.a.guid == guid ? &m.b : &m.a;
}

Duelist* Mine(Match& m, ObjectGuid guid)
{
    return m.a.guid == guid ? &m.a : m.b.guid == guid ? &m.b : nullptr;
}

void Erase(uint32 id)
{
    auto itr = sMatches.find(id);
    if (itr == sMatches.end())
        return;
    sByPlayer.erase(itr->second.a.guid);
    sByPlayer.erase(itr->second.b.guid);
    sMatches.erase(itr);
}

// Both results are in (or one bard left): who wins, and the tips go to him.
void Resolve(uint32 id)
{
    auto itr = sMatches.find(id);
    if (itr == sMatches.end())
        return;
    Match m = itr->second;
    Erase(id);

    Player* pa = Find(m.a.guid);
    Player* pb = Find(m.b.guid);
    std::string const nameA = pa ? pa->GetName() : "le premier barde";
    std::string const nameB = pb ? pb->GetName() : "le second barde";

    bool const countA = m.a.reported && !m.a.dropped;
    bool const countB = m.b.reported && !m.b.dropped;
    Duelist const* winner = nullptr;
    Duelist const* loser = nullptr;
    bool draw = false;
    std::string text;

    if (countA && countB)
    {
        double const best = double(std::max(m.a.score, m.b.score));
        if (best <= 0.0 || std::abs(double(m.a.score) - double(m.b.score)) / best < DRAW_GAP)
            draw = true;
        else
        {
            winner = m.a.score > m.b.score ? &m.a : &m.b;
            loser = winner == &m.a ? &m.b : &m.a;
        }
        text = "Duel : " + nameA + " " + std::to_string(m.a.score) + " contre " + nameB + " " + std::to_string(m.b.score) + " - ";
        text += draw ? "égalité !" : (winner == &m.a ? nameA : nameB) + " l'emporte !";
    }
    else if (countA || countB)
    {
        winner = countA ? &m.a : &m.b;
        loser = countA ? &m.b : &m.a;
        text = "Duel : " + (countA ? nameA : nameB) + " l'emporte, son adversaire a abandonné.";
    }
    else
        return;

    for (Player* player : { pa, pb })
        Say(player, text);

    Concert::SettleDuel(winner ? winner->guid : m.a.guid, winner ? loser->guid : m.b.guid, draw);

    // A victory counts once per opponent and per day.
    if (winner && countA && countB)
    {
        uint32 const day = uint32(time(nullptr) / 86400);
        ObjectGuid::LowType const w = winner->guid.GetCounter(), l = loser->guid.GetCounter();
        bool fresh = true;
        if (QueryResult result = CharacterDatabase.Query("SELECT 1 FROM mod_barde_duel_log WHERE guid = {} AND opponent = {} AND day = {}", w, l, day))
            fresh = false;
        if (fresh)
        {
            CharacterDatabase.Execute("INSERT INTO mod_barde_duel_log (guid, opponent, day) VALUES ({}, {}, {})", w, l, day);
            Titles::Add(w, "duels_won", 1);
            Legend::OnDuelWon(w, l);
            Titles::Check(Find(winner->guid));
        }
    }
}

void Start(Match& m)
{
    Song const* song = Store::Instance().GetSong(m.songId);
    Player* pa = Find(m.a.guid);
    Player* pb = Find(m.b.guid);
    Instrument const* ia = Store::Instance().GetInstrument(m.a.instrumentId);
    Instrument const* ib = Store::Instance().GetInstrument(m.b.instrumentId);
    std::string error;
    if (!song || !pa || !pb || !ia || !ib)
        error = "Le duel est annulé.";
    else if (std::string e = Session::CanPlay(pa, *song, *ia); !e.empty())
        error = e;
    else if (std::string e = Session::CanPlay(pb, *song, *ib); !e.empty())
        error = e;
    if (!error.empty())
    {
        Say(pa, error);
        Say(pb, error);
        Erase(m.id);
        return;
    }

    m.playing = true;
    m.startMs = getMSTime();
    m.durationMs = song->LengthMs(1.0f) + RESULT_WAIT_MS;
    uint32 const id = m.id;
    std::string errorA = Session::Begin(pa, m.songId, false, m.a.instrumentId, 0, 1, PART_AUTO, id);
    std::string errorB = errorA.empty() ? Session::Begin(pb, m.songId, false, m.b.instrumentId, 0, 1, PART_AUTO, id) : "";
    if (!errorA.empty() || !errorB.empty())
    {
        if (!errorB.empty())
            Session::Abort(pa, 0);              // the challenger's song was started: stop it
        Say(pa, errorA.empty() ? errorB : errorA);
        Say(pb, errorA.empty() ? errorB : errorA);
        Erase(id);
    }
}

} // namespace

bool InDuel(Player* player)
{
    std::lock_guard<std::recursive_mutex> guard(sLock);
    return sByPlayer.count(player->GetGUID()) != 0;
}

std::string Propose(Player* challenger, uint32 songId, uint32 instrumentId)
{
    Song const* song = Store::Instance().GetSong(songId);
    Instrument const* instrument = Store::Instance().GetInstrument(instrumentId);
    if (!song || !instrument)
        return "Air ou instrument inconnu.";
    if (std::string error = Session::CanPlay(challenger, *song, *instrument); !error.empty())
        return error;

    Player* target = challenger->GetSelectedPlayer();
    if (!target || target == challenger)
        return "Sélectionnez d'abord le barde que vous défiez (clic sur son personnage).";
    if (target->GetMapId() != challenger->GetMapId() || !challenger->IsWithinDist(target, DUEL_RANGE))
        return "Ce barde est trop loin.";
    if (!target->IsAlive() || target->IsInCombat())
        return "Ce joueur ne peut pas relever un défi maintenant.";
    if (GetConfig().requireProfession && !target->HasSpell(PROFESSION_SPELL))
        return "Ce joueur n'est pas barde.";
    if (Session::GetSkill(target) < song->skillReq)
        return "Ce barde n'a pas la compétence pour cet air.";

    if (!GetConfig().allowSameAccount && challenger->GetSession() && target->GetSession()
        && challenger->GetSession()->GetAccountId() == target->GetSession()->GetAccountId())
        return "Vous ne pouvez pas défier un personnage de votre propre compte.";

    std::lock_guard<std::recursive_mutex> guard(sLock);
    if (sByPlayer.count(challenger->GetGUID()) || sByPlayer.count(target->GetGUID()))
        return "Un duel est déjà en cours ou proposé.";

    Match m;
    m.id = sNextId++;
    m.a.guid = challenger->GetGUID();
    m.a.instrumentId = instrument->id;
    m.a.joined = true;
    m.b.guid = target->GetGUID();
    m.songId = song->id;
    m.createdMs = getMSTime();
    sByPlayer[m.a.guid] = m.id;
    sByPlayer[m.b.guid] = m.id;
    uint32 const id = m.id;
    sMatches[id] = m;

    // DUELINV^id^challengerName^songId^songName^seconds
    Protocol::Send(target, "DUELINV^" + std::to_string(id) + '^' + challenger->GetName() + '^' + std::to_string(song->id) + '^'
        + song->name + '^' + std::to_string(GetConfig().ensembleInviteSec));
    Say(target, challenger->GetName() + " vous défie à « " + song->name + " ».");
    Say(challenger, "Défi envoyé à " + target->GetName() + ".");
    return {};
}

void Answer(Player* player, uint32 duelId, bool accept, uint32 instrumentId)
{
    std::lock_guard<std::recursive_mutex> guard(sLock);
    auto itr = sMatches.find(duelId);
    if (itr == sMatches.end() || itr->second.b.guid != player->GetGUID() || itr->second.playing)
    {
        Say(player, "Ce défi n'est plus valable.");
        return;
    }
    Match& m = itr->second;
    Player* challenger = Find(m.a.guid);
    if (!accept)
    {
        Say(challenger, player->GetName() + " refuse le duel.");
        Erase(duelId);
        return;
    }

    Instrument const* instrument = Store::Instance().GetInstrument(instrumentId);
    if (!instrument || (GetConfig().requireInstrument && Atelier::BestTier(player, *instrument) < 0))
    {
        instrument = nullptr;
        for (Instrument const* candidate : Store::Instance().GetInstruments())
            if (!instrument && (!GetConfig().requireInstrument || Atelier::BestTier(player, *candidate) >= 0))
                instrument = candidate;
    }
    if (!instrument)
    {
        Say(player, "Il vous faut un instrument pour relever ce défi.");
        Say(challenger, player->GetName() + " n'a pas d'instrument : le duel est annulé.");
        Erase(duelId);
        return;
    }
    m.b.instrumentId = instrument->id;
    m.b.joined = true;
    Start(m);
}

void Report(Player* player, uint32 duelId, uint32 score)
{
    std::lock_guard<std::recursive_mutex> guard(sLock);
    auto itr = sMatches.find(duelId);
    if (itr == sMatches.end() || !itr->second.playing)
        return;
    Match& m = itr->second;
    Duelist* me = Mine(m, player->GetGUID());
    if (!me)
        return;
    me->reported = true;
    me->score = score;
    Duelist* other = Other(m, player->GetGUID());
    if (other->reported || other->dropped)
        Resolve(duelId);
    else
        Say(player, "Votre morceau est fini : on attend votre adversaire...");
}

void Drop(Player* player)
{
    std::lock_guard<std::recursive_mutex> guard(sLock);
    auto by = sByPlayer.find(player->GetGUID());
    if (by == sByPlayer.end())
        return;
    uint32 const id = by->second;
    auto itr = sMatches.find(id);
    if (itr == sMatches.end())
        return;
    Match& m = itr->second;
    if (!m.playing)
    {
        Say(Find(Other(m, player->GetGUID())->guid), player->GetName() + " ne relève pas le défi.");
        Erase(id);
        return;
    }
    Duelist* me = Mine(m, player->GetGUID());
    if (me && !me->reported)
        me->dropped = true;
    Duelist* other = Other(m, player->GetGUID());
    if (other->reported || other->dropped)
        Resolve(id);
}

void Update()
{
    std::lock_guard<std::recursive_mutex> guard(sLock);
    if (sMatches.empty())
        return;
    uint32 const now = getMSTime();
    std::vector<uint32> due;
    for (auto const& [id, m] : sMatches)
    {
        if (!m.playing && getMSTimeDiff(m.createdMs, now) >= GetConfig().ensembleInviteSec * 1000)
            due.push_back(id);
        else if (m.playing && getMSTimeDiff(m.startMs, now) >= m.durationMs)
            due.push_back(id);
    }
    for (uint32 id : due)
    {
        auto itr = sMatches.find(id);
        if (itr == sMatches.end())
            continue;
        if (itr->second.playing)
        {
            for (Duelist* d : { &itr->second.a, &itr->second.b })
                if (!d->reported)
                    d->dropped = true;
            Resolve(id);
        }
        else
        {
            Say(Find(itr->second.a.guid), "Le défi n'a pas reçu de réponse.");
            Erase(id);
        }
    }
}

} // namespace Barde::Duel
