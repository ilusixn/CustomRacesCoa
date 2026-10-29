/*
 * mod-barde - playing together: duo and trio.
 * Licensed under the GNU AGPL v3, see LICENSE.
 *
 * The leader of a party proposes a song. Every other bard of the party who is close enough, alive, out of
 * combat and skilled enough for the song gets an invitation and answers with an instrument (two bards cannot
 * play the same instrument). When everybody has answered, or the time is up, the song starts for those who
 * said yes, at the same moment. Each bard plays and is checked exactly as when playing alone; the ensemble
 * then waits for every result, averages the accuracies, and gives each bard's chant to the whole party with
 * the duo (x1.6) or trio (x2) multiplier. A bard who leaves is simply not counted.
 */

#include "Barde.h"

#include "Chat.h"
#include "Group.h"
#include "GroupReference.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "StringConvert.h"
#include "Timer.h"
#include "Tokenize.h"
#include "WorldSession.h"

#include <algorithm>
#include <mutex>
#include <unordered_map>

namespace Barde::Ensemble
{

namespace
{

constexpr uint32 MAX_BARDS = 3;
constexpr uint32 RESULT_WAIT_MS = 20000;        // after the song's length, before the missing bards are given up
constexpr float GROUP_RANGE = 100.0f;           // the party members this close to the bards receive the chants

struct Member
{
    ObjectGuid guid;
    std::string name;
    bool answered = false;
    bool joined = false;
    bool dropped = false;
    bool reported = false;
    bool virtualBard = false;       // GM test partner: no player behind, answers by himself
    uint32 instrumentId = 0;
    uint8 part = PART_MELODY;
    Session::Accepted result;
};

struct Gathering
{
    uint32 id = 0;
    ObjectGuid leader;
    uint32 songId = 0;
    bool playing = false;
    bool starting = false;      // everybody answered: the countdown before the song runs
    uint32 countdownStartMs = 0;
    uint32 createdMs = 0;       // invitations sent
    uint32 startMs = 0;         // song started
    uint32 durationMs = 0;      // how long the song, and the wait for results, may last
    uint32 lengthMs = 0;        // the song alone
    float virtualAccuracy = 0.9f;
    std::vector<Member> members;        // the leader first
};

std::recursive_mutex sLock;
std::unordered_map<uint32, Gathering> sGatherings;
std::unordered_map<ObjectGuid, uint32> sByPlayer;       // bard -> ensemble he is in
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

Member* FindMember(Gathering& g, ObjectGuid guid)
{
    for (Member& member : g.members)
        if (member.guid == guid)
            return &member;
    return nullptr;
}

uint32 JoinedCount(Gathering const& g)
{
    uint32 count = 0;
    for (Member const& member : g.members)
        if (member.joined && !member.dropped)
            ++count;
    return count;
}

bool InstrumentTaken(Gathering const& g, uint32 instrumentId)
{
    for (Member const& member : g.members)
        if (member.joined && !member.dropped && member.instrumentId == instrumentId)
            return true;
    return false;
}

void Erase(Gathering const& g)
{
    uint32 const id = g.id;     // g lives in the map: read what is needed before it goes
    for (Member const& member : g.members)
        sByPlayer.erase(member.guid);
    sGatherings.erase(id);
}

// The state of the session, for the window of every bard who is in it:
// ENSTATE^ensembleId^phase^songName^bards^averageAccuracy‰^chantGiven^name|state|instrumentId|part|accuracy‰;...^multiplier‰^countdownMs
//   (phase 4: everybody answered, the song starts after the countdown)
//   phase: 0 waiting for the answers, 1 playing, 2 over (accuracies and average known), 3 cancelled
//   state: 0 has not answered, 1 plays, 2 does not play (refused, gone), 3 has finished his song
void Broadcast(Gathering const& g, uint32 phase, float average = 0.0f, bool chantGiven = false, uint32 countdownMs = 0)
{
    Song const* song = Store::Instance().GetSong(g.songId);
    std::string members;
    for (Member const& member : g.members)
    {
        uint32 const state = member.dropped ? 2 : !member.answered ? 0 : !member.joined ? 2 : member.reported ? 3 : 1;
        if (!members.empty())
            members += ';';
        members += member.name + '|' + std::to_string(state) + '|' + std::to_string(member.instrumentId) + '|' + std::to_string(member.part)
            + '|' + std::to_string(member.reported ? uint32(std::lround(member.result.accuracy * 1000.0f)) : 0);
    }
    std::string const body = "ENSTATE^" + std::to_string(g.id) + '^' + std::to_string(phase) + '^' + (song ? song->name : std::string()) + '^'
        + std::to_string(JoinedCount(g)) + '^' + std::to_string(uint32(std::lround(average * 1000.0f))) + '^' + (chantGiven ? "1" : "0") + '^' + members
        + '^' + std::to_string(uint32(std::lround((JoinedCount(g) >= 3 ? GetConfig().trioMultiplier : JoinedCount(g) == 2 ? GetConfig().duoMultiplier : 1.0f) * 1000.0f)))
        + '^' + std::to_string(countdownMs);
    for (Member const& member : g.members)
        if (!member.virtualBard && (member.guid == g.leader || (member.joined && !member.dropped)))
            if (Player* player = Find(member.guid))
                Protocol::Send(player, body);
}

void Begin(Gathering& g);

// Everybody has answered, or the time is up: prepare the start for those who said yes (a countdown, then Begin).
void Start(Gathering& g)
{
    Song const* song = Store::Instance().GetSong(g.songId);
    Player* leader = Find(g.leader);

    // Last check, at the moment of the start: things may have changed since the answers.
    for (Member& member : g.members)
    {
        if (!member.joined || member.dropped || member.virtualBard)     // the GM test partner has no player to check
            continue;
        Player* player = Find(member.guid);
        Instrument const* instrument = Store::Instance().GetInstrument(member.instrumentId);
        std::string error = !player ? "absent" : (!song || !instrument) ? "air ou instrument inconnu"
            : Session::CanPlay(player, *song, *instrument);
        if (!error.empty())
        {
            member.dropped = true;
            sByPlayer.erase(member.guid);
            Say(player, "Vous ne pouvez plus jouer cet ensemble : " + error);
        }
    }

    uint32 const size = JoinedCount(g);
    if (!song || size < 2)
    {
        Broadcast(g, 3);
        Say(leader, "L'ensemble est annulé : il faut au moins deux bardes.");
        for (Member const& member : g.members)
            if (member.joined && !member.dropped)
                Say(Find(member.guid), "L'ensemble est annulé : il faut au moins deux bardes.");
        Erase(g);
        return;
    }

    // The parts. The drum plays the rhythm. The melody is the first other bard's instrument (the leader's, or
    // the next one if the leader has the drum). The harmony goes to the vièle, or to the harpe when the melody
    // is already on the vièle. Anybody else plays the melody too.
    Instrument const* melody = nullptr;
    for (Member const& member : g.members)
    {
        Instrument const* instrument = Store::Instance().GetInstrument(member.instrumentId);
        if (!melody && member.joined && !member.dropped && instrument && instrument->NaturalPart() == PART_MELODY)
            melody = instrument;
    }
    std::string const harmonySound = melody && melody->sound == "viele" ? "harpe" : "viele";
    for (Member& member : g.members)
    {
        Instrument const* instrument = Store::Instance().GetInstrument(member.instrumentId);
        if (!instrument)
            continue;
        member.part = instrument->NaturalPart() == PART_RHYTHM ? PART_RHYTHM
            : (melody && instrument->id != melody->id && instrument->sound == harmonySound) ? PART_HARMONY
            : PART_MELODY;
    }

    // The preparation: a countdown, so that everybody sees who is in and gets ready before the song runs.
    uint32 const countdownMs = GetConfig().ensembleCountdownSec * 1000;
    g.starting = true;
    g.countdownStartMs = getMSTime();
    Broadcast(g, 4, 0.0f, false, countdownMs);
    if (countdownMs)
    {
        for (Member const& member : g.members)
            if (member.joined && !member.dropped && !member.virtualBard)
                Say(Find(member.guid), "L'ensemble est prêt : le morceau commence dans " + std::to_string(GetConfig().ensembleCountdownSec)
                    + " secondes. Préparez-vous !");
        return;
    }
    Begin(g);
}

// The countdown is over: the song runs for those who are still there.
void Begin(Gathering& g)
{
    Song const* song = Store::Instance().GetSong(g.songId);
    Player* leader = Find(g.leader);
    g.starting = false;

    uint32 const size = JoinedCount(g);
    if (!song || size < 2)
    {
        Broadcast(g, 3);
        Say(leader, "L'ensemble est annulé : il faut au moins deux bardes.");
        for (Member const& member : g.members)
            if (member.joined && !member.dropped)
                Say(Find(member.guid), "L'ensemble est annulé : il faut au moins deux bardes.");
        Erase(g);
        return;
    }

    g.playing = true;
    Broadcast(g, 1);
    g.startMs = getMSTime();
    g.lengthMs = song->LengthMs(1.0f);
    g.durationMs = g.lengthMs + RESULT_WAIT_MS;
    for (Member& member : g.members)
    {
        if (!member.joined || member.dropped || member.virtualBard)
            continue;
        Player* player = Find(member.guid);
        std::string error = Session::Begin(player, g.songId, false, member.instrumentId, g.id, size, member.part);
        if (!error.empty())
        {
            member.dropped = true;
            sByPlayer.erase(member.guid);
            Say(player, error);
        }
    }
}

void Finalize(uint32 id)
{
    auto itr = sGatherings.find(id);
    if (itr == sGatherings.end())
        return;
    Gathering g = itr->second;
    Erase(g);

    Song const* song = Store::Instance().GetSong(g.songId);
    Config const& config = GetConfig();
    if (!song)
        return;

    std::vector<Member*> bards;
    for (Member& member : g.members)
        if (member.joined && !member.dropped && member.reported && (member.virtualBard || Find(member.guid)))
            bards.push_back(&member);
    if (bards.empty())
        return;

    uint32 const size = uint32(bards.size());
    float const multiplier = size >= 3 ? config.trioMultiplier : size == 2 ? config.duoMultiplier : 1.0f;
    float accuracy = 0.0f;
    uint32 combo = 0;
    for (Member const* member : bards)
    {
        accuracy += member->result.accuracy;
        combo += member->result.maxCombo;
    }
    accuracy /= float(size);
    combo /= size;
    Broadcast(g, 2, accuracy, accuracy >= config.minAccuracyBuff);

    // The quest of the legendary instrument: a song of the Grand bard level, played by three real bards, each at 100 %.
    {
        std::vector<Player*> real;
        std::vector<float> accuracies;
        for (Member const* member : bards)
            if (!member->virtualBard)
                if (Player* player = Find(member->guid))
                {
                    real.push_back(player);
                    accuracies.push_back(member->result.accuracy);
                }
        if (real.size() == bards.size())
            Legend::OnTrio(real, *song, accuracies);
    }

    // Recipients: the bards, and the party members near them. Alone (the others gave up), only the bard.
    std::vector<Player*> recipients;
    auto add = [&](Player* player)
    {
        if (player && std::find(recipients.begin(), recipients.end(), player) == recipients.end())
            recipients.push_back(player);
    };
    for (Member const* member : bards)
        if (!member->virtualBard)
            add(Find(member->guid));
    if (size >= 2)
    {
        Member const* realBard = nullptr;
        for (Member const* member : bards)
            if (!realBard && !member->virtualBard)
                realBard = member;
        if (Player* first = realBard ? Find(realBard->guid) : nullptr)
            if (Group* group = first->GetGroup())
                for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                    if (Player* other = ref->GetSource())
                        if (other->GetMapId() == first->GetMapId() && first->IsWithinDist(other, GROUP_RANGE))
                            add(other);
    }

    for (Member const* member : bards)
    {
        if (member->virtualBard)
            continue;       // the test partner has no one to give a chant to
        Player* bard = Find(member->guid);
        Instrument const* instrument = Store::Instance().GetInstrument(member->instrumentId);
        if (!bard || !instrument)
            continue;

        // The group is under the minimum: nobody gets a chant. Only a bard who himself played under the
        // minimum gets his instrument's malus; the others are not punished for a partner's failure.
        if (accuracy < config.minAccuracyBuff)
        {
            BuffOutcome failed;
            if (member->result.accuracy < config.minAccuracyBuff)
                failed = ApplyBuff(bard, *song, *instrument, member->result.accuracy, member->result.maxCombo, false);
            else
                failed.message = "Le groupe est sous " + std::to_string(int(config.minAccuracyBuff * 100.0f))
                    + " % de justesse : pas de chant";
            Session::SendOutcome(bard, *song, *instrument, member->result, failed, size, accuracy, multiplier);
            continue;
        }

        uint32 granted = 0;
        BuffOutcome outcome = ApplyGroupBuff(bard, recipients, *song, *instrument, accuracy, combo, false,
            multiplier, &granted);
        Session::SendOutcome(bard, *song, *instrument, member->result, outcome, size, accuracy, multiplier);

        if (!outcome.malus && outcome.amount > 0)
            for (Player* other : recipients)
                if (other != bard)
                    Say(other, bard->GetName() + " joue pour le groupe : " + instrument->buffName + " (+"
                        + std::to_string(outcome.amount) + (instrument->unit == "%" ? " %" : "") + " "
                        + instrument->effect + ").");
    }
}

} // namespace

std::string Propose(Player* leader, uint32 songId, uint32 instrumentId)
{
    Config const& config = GetConfig();
    Song const* song = Store::Instance().GetSong(songId);
    if (!song)
        return "Air inconnu.";
    Instrument const* instrument = Store::Instance().GetInstrument(instrumentId);
    if (!instrument)
        return "Instrument inconnu.";
    if (std::string error = Session::CanPlay(leader, *song, *instrument); !error.empty())
        return error;

    Group* group = leader->GetGroup();
    if (!group)
        return "Il faut être en groupe pour jouer ensemble.";

    std::lock_guard<std::recursive_mutex> guard(sLock);
    if (sByPlayer.count(leader->GetGUID()))
        return "Vous participez déjà à un ensemble.";

    Gathering g;
    g.id = sNextId++;
    g.leader = leader->GetGUID();
    g.songId = song->id;
    g.createdMs = getMSTime();

    Member first;
    first.guid = leader->GetGUID();
    first.name = leader->GetName();
    first.answered = first.joined = true;
    first.instrumentId = instrument->id;
    g.members.push_back(first);

    uint32 invited = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* other = ref->GetSource();
        if (!other || other == leader || sByPlayer.count(other->GetGUID()))
            continue;
        if (other->GetMapId() != leader->GetMapId() || !leader->IsWithinDist(other, config.ensembleRange))
            continue;
        if (!other->IsAlive() || other->IsInCombat())
            continue;
        if (config.requireProfession && !other->HasSpell(PROFESSION_SPELL))
            continue;
        if (Session::GetSkill(other) < song->skillReq)
            continue;

        Member member;
        member.guid = other->GetGUID();
        member.name = other->GetName();
        g.members.push_back(member);
        ++invited;

        // INVITE^ensembleId^leaderName^songId^songName^seconds
        Protocol::Send(other, "INVITE^" + std::to_string(g.id) + '^' + leader->GetName() + '^'
            + std::to_string(song->id) + '^' + song->name + '^' + std::to_string(config.ensembleInviteSec));
        Say(other, leader->GetName() + " propose de jouer « " + song->name + " » ensemble.");
    }
    if (!invited)
        return "Aucun barde du groupe ne peut se joindre : il faut qu'il soit près de vous, vivant, hors combat, "
               "qu'il connaisse le métier et ait assez de compétence pour cet air.";

    sByPlayer[leader->GetGUID()] = g.id;
    uint32 const id = g.id;
    sGatherings[id] = std::move(g);
    Broadcast(sGatherings[id], 0);
    Say(leader, "Invitation envoyée à " + std::to_string(invited) + " barde(s). Réponse attendue sous "
        + std::to_string(config.ensembleInviteSec) + " secondes.");
    return {};
}

void Answer(Player* player, uint32 ensembleId, bool accept, uint32 instrumentId)
{
    std::lock_guard<std::recursive_mutex> guard(sLock);
    auto itr = sGatherings.find(ensembleId);
    Member* member = itr != sGatherings.end() && !itr->second.playing && !itr->second.starting ? FindMember(itr->second, player->GetGUID()) : nullptr;
    if (!member || member->answered || member->guid == itr->second.leader)
    {
        Say(player, "Cette invitation n'est plus valable.");
        return;
    }

    Gathering& g = itr->second;
    member->answered = true;
    Player* leader = Find(g.leader);

    auto ready = [&]()
    {
        bool all = true;
        for (Member const& m : g.members)
            all = all && m.answered;
        if (all || JoinedCount(g) >= MAX_BARDS)
            Start(g);
    };

    if (!accept)
    {
        Say(leader, player->GetName() + " ne joue pas.");
        Broadcast(g, 0);
        ready();
        return;
    }

    Song const* song = Store::Instance().GetSong(g.songId);
    Instrument const* instrument = nullptr;
    if (instrumentId)
        instrument = Store::Instance().GetInstrument(instrumentId);
    if (!instrument || (GetConfig().requireInstrument && Atelier::BestTier(player, *instrument) < 0)
        || InstrumentTaken(g, instrument->id))
    {
        // Own choice not possible: the first instrument he owns that nobody plays yet.
        instrument = nullptr;
        for (Instrument const* candidate : Store::Instance().GetInstruments())
            if (!instrument && (!GetConfig().requireInstrument || Atelier::BestTier(player, *candidate) >= 0)
                && !InstrumentTaken(g, candidate->id))
                instrument = candidate;
    }

    std::string error;
    if (!song)
        error = "Air inconnu.";
    else if (!instrument)
        error = "Vous n'avez pas d'instrument libre pour cet ensemble (deux bardes ne jouent pas le même).";
    else if (JoinedCount(g) >= MAX_BARDS)
        error = "L'ensemble est complet.";
    else
        error = Session::CanPlay(player, *song, *instrument);
    if (!error.empty())
    {
        Say(player, error);
        Say(leader, player->GetName() + " ne peut pas se joindre : " + error);
        Broadcast(g, 0);
        ready();
        return;
    }

    member->joined = true;
    member->instrumentId = instrument->id;
    sByPlayer[player->GetGUID()] = g.id;
    Say(leader, player->GetName() + " se joint à l'ensemble (" + instrument->name + ").");
    Say(player, "Vous jouez : " + instrument->name + ". Le morceau commence quand tout le monde a répondu.");
    Broadcast(g, 0);
    ready();
}

std::vector<Player*> Partners(Player* player)
{
    std::vector<Player*> partners;
    std::lock_guard<std::recursive_mutex> guard(sLock);
    auto by = sByPlayer.find(player->GetGUID());
    if (by == sByPlayer.end())
        return partners;
    auto itr = sGatherings.find(by->second);
    if (itr == sGatherings.end() || !itr->second.playing)
        return partners;

    Gathering& g = itr->second;
    Member* self = FindMember(g, player->GetGUID());
    if (!self || self->dropped || self->reported)
        return partners;
    for (Member const& member : g.members)
        if (member.joined && !member.dropped && !member.virtualBard && member.guid != self->guid)
            if (Player* other = Find(member.guid))
                partners.push_back(other);
    return partners;
}

bool Report(Player* player, uint32 ensembleId, Session::Accepted const& result)
{
    std::lock_guard<std::recursive_mutex> guard(sLock);
    auto itr = sGatherings.find(ensembleId);
    Member* member = itr != sGatherings.end() && itr->second.playing ? FindMember(itr->second, player->GetGUID()) : nullptr;
    if (!member || member->dropped)
        return false;       // the ensemble is gone (time ran out): the caller treats the song as played alone

    member->reported = true;
    member->result = result;

    bool all = true;
    for (Member const& m : itr->second.members)
        if (m.joined && !m.dropped && !m.reported)
            all = false;
    if (all)
        Finalize(ensembleId);
    else
    {
        Say(player, "Votre morceau est fini : on attend les autres bardes...");
        Broadcast(itr->second, 1);
    }
    return true;
}

void Drop(Player* player)
{
    std::lock_guard<std::recursive_mutex> guard(sLock);
    auto bard = sByPlayer.find(player->GetGUID());
    if (bard == sByPlayer.end())
        return;

    auto itr = sGatherings.find(bard->second);
    sByPlayer.erase(bard);
    if (itr == sGatherings.end())
        return;

    Gathering& g = itr->second;
    if (Member* member = FindMember(g, player->GetGUID()))
        member->dropped = true;

    if (!g.playing)
    {
        // Not started: if the leader goes, everything is cancelled; otherwise it goes on without him.
        if (player->GetGUID() == g.leader)
        {
            for (Member const& member : g.members)
                if (member.joined && !member.dropped)
                    Say(Find(member.guid), "L'ensemble est annulé : celui qui l'avait proposé est parti.");
            Broadcast(g, 3);
            Erase(g);
        }
        return;
    }

    bool any = false, all = true;
    for (Member const& member : g.members)
        if (member.joined && !member.dropped)
        {
            any = true;
            all = all && member.reported;
        }
    if (!any)
        Erase(g);
    else if (all)
        Finalize(g.id);
    else
        Broadcast(g, 1);
}

std::string TestDuo(Player* leader, uint32 songId, float accuracy)
{
    Song const* song = Store::Instance().GetSong(songId);
    if (!song)
        return "Air inconnu.";

    // The leader plays the first instrument he owns, the partner the first other one.
    Instrument const* own = nullptr;
    for (Instrument const* candidate : Store::Instance().GetInstruments())
        if (!own && (!GetConfig().requireInstrument || Atelier::BestTier(leader, *candidate) >= 0))
            own = candidate;
    if (!own)
        return "Il vous faut au moins un instrument (.barde instruments).";
    Instrument const* partner = nullptr;
    for (Instrument const* candidate : Store::Instance().GetInstruments())
        if (!partner && candidate->id != own->id)
            partner = candidate;
    if (!partner)
        return "Il faut au moins deux instruments configurés.";
    if (std::string error = Session::CanPlay(leader, *song, *own); !error.empty())
        return error;

    std::lock_guard<std::recursive_mutex> guard(sLock);
    if (sByPlayer.count(leader->GetGUID()))
        return "Vous participez déjà à un ensemble.";

    Gathering g;
    g.id = sNextId++;
    g.leader = leader->GetGUID();
    g.songId = song->id;
    g.createdMs = getMSTime();
    g.virtualAccuracy = std::clamp(accuracy, 0.0f, 1.0f);

    Member first;
    first.guid = leader->GetGUID();
    first.name = leader->GetName();
    first.answered = first.joined = true;
    first.instrumentId = own->id;
    g.members.push_back(first);

    Member ghost;
    ghost.name = "Partenaire virtuel";
    ghost.answered = ghost.joined = ghost.virtualBard = true;
    ghost.instrumentId = partner->id;
    g.members.push_back(ghost);

    sByPlayer[leader->GetGUID()] = g.id;
    uint32 const id = g.id;
    sGatherings[id] = std::move(g);
    Say(leader, "Duo de test : vous jouez " + own->name + ", le partenaire virtuel " + partner->name + " ("
        + std::to_string(int(std::lround(accuracy * 100.0f))) + " % de justesse).");
    Start(sGatherings[id]);
    return {};
}

void Update()
{
    std::lock_guard<std::recursive_mutex> guard(sLock);
    if (sGatherings.empty())
        return;

    uint32 const now = getMSTime();
    std::vector<uint32> due;
    std::vector<uint32> beginning;
    for (auto const& [id, g] : sGatherings)
    {
        if (g.starting)
        {
            if (getMSTimeDiff(g.countdownStartMs, now) >= GetConfig().ensembleCountdownSec * 1000)
                beginning.push_back(id);
            continue;
        }
        if (g.playing)
        {
            // The virtual partner hands in his result when the song is over.
            for (Member& member : const_cast<Gathering&>(g).members)
                if (member.virtualBard && !member.reported && getMSTimeDiff(g.startMs, now) >= g.lengthMs)
                {
                    member.reported = true;
                    member.result.accuracy = g.virtualAccuracy;
                    member.result.maxCombo = 40;
                }
            bool all = true;
            for (Member const& member : g.members)
                if (member.joined && !member.dropped && !member.reported)
                    all = false;
            if (all)
            {
                due.push_back(id);
                continue;
            }
        }
        if (g.playing ? getMSTimeDiff(g.startMs, now) >= g.durationMs
                      : getMSTimeDiff(g.createdMs, now) >= GetConfig().ensembleInviteSec * 1000)
            due.push_back(id);
    }
    for (uint32 id : beginning)
    {
        auto itr = sGatherings.find(id);
        if (itr != sGatherings.end() && itr->second.starting)
            Begin(itr->second);
    }
    for (uint32 id : due)
    {
        auto itr = sGatherings.find(id);
        if (itr == sGatherings.end())
            continue;
        if (itr->second.playing)
            Finalize(id);       // the missing bards are given up
        else
            Start(itr->second); // the unanswered invitations are given up
    }
}

} // namespace Barde::Ensemble
