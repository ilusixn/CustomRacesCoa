/*
 * mod-barde - one song in progress per player, and the check of its result.
 * Licensed under the GNU AGPL v3, see LICENSE.
 *
 * The server remembers when it sent the score. The addon's result is only
 * accepted if it is possible: the song really lasted, the counts add up to
 * the score's notes, and the score is not above what those counts can give.
 * The server then computes the accuracy itself from the counts.
 */

#include "Barde.h"

#include "CellImpl.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "Field.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QueryResult.h"
#include "Random.h"
#include "StringConvert.h"
#include "Tokenize.h"
#include "SpellAuras.h"
#include "Timer.h"
#include "WorldSession.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <mutex>
#include <unordered_map>

namespace Barde
{

std::vector<Player*> PlayersAround(WorldObject* center, float range)
{
    std::vector<Player*> found;
    if (range <= 0.0f || !center)
        return found;
    Acore::AnyPlayerInObjectRangeCheck check(center, range, false);
    Acore::PlayerListSearcher<Acore::AnyPlayerInObjectRangeCheck> searcher(center, found, check);
    Cell::VisitObjects(center, searcher, range);
    return found;
}

} // namespace Barde

namespace Barde::Session
{

namespace
{

struct Active
{
    uint32 token = 0;
    uint32 songId = 0;
    bool training = false;
    uint32 instrumentId = 0;
    uint32 ensembleId = 0;      // 0 when played alone
    uint32 duelId = 0;
    uint32 groupSize = 1;
    uint8 part = PART_MELODY;
    uint32 startMs = 0;
};

std::mutex sLock;
std::unordered_map<ObjectGuid::LowType, Active> sActive;
std::unordered_map<ObjectGuid::LowType, std::pair<uint32, uint32>> sHits;   // per bard: start of the second, notes in it
std::unordered_map<ObjectGuid::LowType, std::pair<uint32, std::string>> sImprov;       // bards who improvise -> the instrument, the voice (sound set) heard
std::unordered_map<ObjectGuid::LowType, uint32> sLastEmote;   // when each bard last announced a song
std::unordered_map<ObjectGuid::LowType, uint8> sHolding;    // bards holding an instrument -> their sheath state before

constexpr std::size_t CHUNK_CHARS = 200;

std::string FormatPermille(float value)
{
    return std::to_string(uint32(std::lround(value * 1000.0f)));
}

std::vector<Player*> PlayersNear(Player* player, float range)
{
    std::vector<Player*> found = PlayersAround(player, range);
    found.erase(std::remove(found.begin(), found.end(), player), found.end());
    return found;
}

void HoldInstrument(Player* player, Instrument const& instrument, Song const* song, uint32 emoteState = 0)
{
    {
        std::lock_guard<std::mutex> guard(sLock);
        sHolding.emplace(player->GetGUID().GetCounter(), uint8(player->GetSheath()));   // only the first time
    }
    uint16 const field = PLAYER_VISIBLE_ITEM_1_ENTRYID + EQUIPMENT_SLOT_MAINHAND * 2;
    uint32 const entry = Atelier::BestItem(player, instrument);
    player->SetUInt32Value(field, entry ? entry : instrument.itemEntry);     // the one he carries, the best tier
    player->SetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENCHANTMENT + EQUIPMENT_SLOT_MAINHAND * 2, 0);
    player->SetSheath(SHEATH_STATE_MELEE);                      // weapon out, so the instrument shows in the hand

    // What the others see: the aura goes on until the song is over (ReleaseHands takes it away), and a line of
    // emote text tells them what is going on.
    Config const& config = GetConfig();
    for (uint32 spell : { config.playAura, config.playSoundAura })
        if (spell)
            if (Aura* aura = player->AddAura(spell, player))
            {
                aura->SetMaxDuration(20 * MINUTE * IN_MILLISECONDS);
                aura->SetDuration(20 * MINUTE * IN_MILLISECONDS);
            }
    // One line only, and not again within 30 seconds: somebody who replays a song in a row does not flood the area.
    uint32 state = emoteState;
    if (!state && !config.playEmoteStates.empty())
        state = config.playEmoteStates[urand(0, uint32(config.playEmoteStates.size() - 1))];     // a dance drawn for each song
    if (state)
        player->SetUInt32Value(UNIT_NPC_EMOTESTATE, state);
    if (config.playEmote && song)
    {
        uint32 const now = getMSTime();
        std::lock_guard<std::mutex> guard(sLock);
        uint32& last = sLastEmote[player->GetGUID().GetCounter()];
        if (!last || getMSTimeDiff(last, now) >= 30000)
        {
            last = now;
            // An ordinary /me line, but only for the players close enough (the core's emote range is wider).
            // The name of the instrument he really plays: the best tier he carries ("Luth d'argent").
            ItemTemplate const* played = sObjectMgr->GetItemTemplate(entry ? entry : instrument.itemEntry);
            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_EMOTE, LANG_UNIVERSAL, player, nullptr,
                " commence à jouer « " + song->name + " » (" + (played ? played->Name1 : instrument.name) + ").");
            player->SendDirectMessage(&data);
            for (Player* other : PlayersNear(player, config.listenRange))
                other->SendDirectMessage(&data);
        }
    }
}

void Refuse(Player* player, uint32 token, uint32 songId, std::string const& reason)
{
    LOG_WARN("module", "mod-barde: result refused for {} (guid {}) on song {}: {}",
        player->GetName(), player->GetGUID().GetCounter(), songId, reason);
    Protocol::Send(player, "KO^" + std::to_string(token) + "^Résultat refusé : " + reason);
}

} // namespace

std::string CanPlay(Player* player, Song const& song, Instrument const& instrument)
{
    Config const& config = GetConfig();
    if (!config.enable)
        return "Le métier de barde est désactivé sur ce serveur.";

    if (config.requireProfession && !player->HasSpell(PROFESSION_SPELL))
        return "Vous n'avez pas appris le métier de barde : ouvrez le Livre des artisans et apprenez « Apprenti barde ».";

    if (song.removed || !Store::Instance().Knows(player->GetGUID().GetCounter(), song))
        return "Vous ne connaissez pas cet air.";

    if (config.requireInstrument)
    {
        int const tier = Atelier::BestTier(player, instrument);
        if (tier < 0)
            return "Il vous faut cet instrument dans vos sacs : " + instrument.name + " (à fabriquer à l'atelier du carnet).";
        if (!song.owner && tier < int(song.tier))       // a composition has no chant: any instrument will do
        {
            InstrumentTier const* needed = Store::Instance().GetTier(song.tier);
            return "Cet air demande un instrument " + std::string(needed ? needed->suffix : "d'un palier supérieur")
                + " ou mieux.";
        }
    }

    uint32 const skill = GetSkill(player);
    if (!song.owner && skill < song.skillReq)           // a composition asks for no skill: it gives none
        return "Compétence de barde insuffisante : cet air demande " + std::to_string(song.skillReq)
            + ", vous avez " + std::to_string(skill) + ".";

    if (!player->IsAlive())
        return "Impossible de jouer en étant mort.";

    if (player->IsInCombat())
        return "Impossible de jouer en combat.";

    return {};
}

std::string Begin(Player* player, uint32 songId, bool training, uint32 instrumentId, uint32 ensembleId,
                  uint32 groupSize, uint8 part, uint32 duelId)
{
    Config const& config = GetConfig();
    if (!config.enable)
        return "Le métier de barde est désactivé sur ce serveur.";

    Song const* song = Store::Instance().GetSong(songId);
    if (!song)
        return "Air inconnu.";

    Instrument const* instrument = Store::Instance().GetInstrument(instrumentId);
    if (!instrument)
        return "Instrument inconnu.";

    if (std::string error = CanPlay(player, *song, *instrument); !error.empty())
        return error;

    if (part == PART_AUTO)
        part = instrument->NaturalPart();
    Chart const& chart = song->GetChart(part);
    if (chart.notes.empty())
        return "Cet air n'a pas de partie pour cet instrument.";

    Active active;
    active.token = urand(1, 0x7FFFFFFF);
    active.songId = song->id;
    active.training = training;
    active.instrumentId = instrument->id;
    active.ensembleId = ensembleId;
    active.duelId = duelId;
    active.groupSize = groupSize;
    active.part = part;

    float tempo = training ? config.trainingTempo : 1.0f;

    // Notes, in chunks short enough for one addon message each.
    std::vector<std::string> chunks(1);
    for (Note const& note : chart.notes)
    {
        std::string item = std::to_string(note.step) + ',' + std::to_string(note.lane) + ','
            + std::to_string(note.midi) + ',' + std::to_string(note.hold);
        if (chunks.back().size() + item.size() + 1 > CHUNK_CHARS)
            chunks.emplace_back();
        if (!chunks.back().empty())
            chunks.back() += ';';
        chunks.back() += item;
    }

    std::string const token = std::to_string(active.token);
    Protocol::Send(player, "BEGIN^" + token + '^' + std::to_string(song->id) + '^' + std::to_string(song->bpm)
        + '^' + FormatPermille(tempo) + '^' + std::to_string(chart.notes.size()) + '^' + std::to_string(chunks.size())
        + '^' + (training ? "1" : "0") + '^' + std::to_string(song->tier) + '^' + song->name
        + '^' + std::to_string(groupSize) + '^' + std::to_string(part) + '^' + std::to_string(instrument->id));
    for (std::size_t i = 0; i < chunks.size(); ++i)
        Protocol::Send(player, "N^" + token + '^' + std::to_string(i + 1) + '^' + chunks[i]);

    HoldInstrument(player, *instrument, song);
    if (!training)
        Concert::Begin(player, *song);

    // The clock starts once the score is on its way: the addon cannot finish sooner.
    active.startMs = getMSTime();
    {
        std::lock_guard<std::mutex> guard(sLock);
        sActive[player->GetGUID().GetCounter()] = active;
    }
    return {};
}

void HandleResult(Player* player, std::vector<std::string_view> const& fields)
{
    Config const& config = GetConfig();
    uint32 const now = getMSTime();

    // RES^token^perfect^good^miss^holdsKept^maxCombo^score
    constexpr std::size_t VALUES = 7;
    Optional<uint32> values[VALUES];
    if (fields.size() != VALUES + 1)
        return;
    for (std::size_t i = 0; i < VALUES; ++i)
    {
        values[i] = Acore::StringTo<uint32>(fields[i + 1]);
        if (!values[i])
            return;
    }
    uint32 const token = *values[0], perfect = *values[1], good = *values[2], miss = *values[3],
        holdsKept = *values[4], maxCombo = *values[5], score = *values[6];

    ReleaseHands(player);       // the song is over, whatever the verdict

    Active active;
    {
        std::lock_guard<std::mutex> guard(sLock);
        auto itr = sActive.find(player->GetGUID().GetCounter());
        if (itr == sActive.end() || itr->second.token != token)
        {
            Protocol::Send(player, "KO^" + std::to_string(token) + "^Aucun morceau en cours.");
            return;
        }
        active = itr->second;
        sActive.erase(itr);     // a token is good for one result only
    }

    Song const* song = Store::Instance().GetSong(active.songId);
    Instrument const* instrument = Store::Instance().GetInstrument(active.instrumentId);
    if (!song || !instrument)
        return;

    // ---- is this result possible? A held note is judged twice: struck, then released;
    // a kept release counts as a perfect worth 50 points.
    Chart const& chart = song->GetChart(active.part);
    uint32 const total = chart.Judgements();
    uint32 const hits = perfect + good;
    float const tempo = active.training ? config.trainingTempo : 1.0f;
    uint32 const lengthMs = song->ChartLengthMs(chart, tempo);
    uint32 const elapsed = getMSTimeDiff(active.startMs, now);

    if (uint64(perfect) + good + miss != total)
        return Refuse(player, token, song->id, "le nombre de notes ne correspond pas à la partition ("
            + std::to_string(uint64(perfect) + good + miss) + " pour " + std::to_string(total) + ")");
    if (holdsKept > chart.holdCount || holdsKept > perfect)
        return Refuse(player, token, song->id, "trop de notes tenues");
    if (maxCombo > hits)
        return Refuse(player, token, song->id, "série impossible");
    if (double(elapsed) + config.durationSlackSec * 1000.0 < double(lengthMs))
        return Refuse(player, token, song->id, "morceau joué trop vite (" + std::to_string(elapsed)
            + " ms pour " + std::to_string(lengthMs) + " ms)");
    if (uint64(elapsed) > uint64(lengthMs) + uint64(config.resultGraceSec) * 1000)
        return Refuse(player, token, song->id, "résultat arrivé trop tard");

    double const comboBound = maxCombo ? Score::ComboMultiplier(maxCombo - 1) : 1.0;
    double const scoreBound = (double(perfect - holdsKept) * Score::PERFECT + double(good) * Score::GOOD
        + double(holdsKept) * Score::HOLD) * comboBound;
    if (score > chart.maxScore || double(score) > scoreBound + 0.5)
        return Refuse(player, token, song->id, "score supérieur au maximum possible");

    // ---- accepted
    float const accuracy = (float(perfect) + 0.6f * float(good)) / float(total);

    uint32 best = 0;
    bool newBest = false;
    uint32 gained = 0;
    uint32 skill = GetSkill(player);
    if (!active.training)
    {
        ObjectGuid::LowType const guid = player->GetGUID().GetCounter();
        uint32 previousAccuracy = 0;
        if (QueryResult result = CharacterDatabase.Query(
                "SELECT best_score, best_accuracy FROM mod_barde_char_song WHERE guid = {} AND song_id = {}", guid, song->id))
        {
            best = result->Fetch()[0].Get<uint32>();
            previousAccuracy = result->Fetch()[1].Get<uint32>();
        }
        newBest = score > best;
        best = std::max(best, score);

        // Skill, as for a recipe: the colour of the song (how far the skill is above its requirement)
        // gives the chance of a point; the first success of a song gives one more.
        if (!song->owner && accuracy >= config.skillMinAccuracy && skill < config.skillMax)       // a composition gives no skill
        {
            uint32 const above = skill - std::min(skill, uint32(song->skillReq));
            float const chance = above < 20 ? 100.0f : above < 40 ? 75.0f : above < 60 ? 25.0f : 0.0f;
            if (roll_chance_f(chance))
                ++gained;
            if (!song->owner && previousAccuracy < uint32(std::lround(config.skillMinAccuracy * 1000.0f)))      // no bonus point for one's own compositions
                ++gained;
            if (gained)
            {
                skill = std::min(config.skillMax, skill + gained);
                CharacterDatabase.Execute("REPLACE INTO mod_barde_char_skill (guid, skill) VALUES ({}, {})", guid, skill);
            }
        }

        uint32 const win = accuracy >= config.minAccuracyBuff ? 1 : 0;
        CharacterDatabase.Execute(
            "INSERT INTO mod_barde_char_song (guid, song_id, plays, wins, best_score, best_accuracy) "
            "VALUES ({}, {}, 1, {}, {}, {}) ON DUPLICATE KEY UPDATE plays = plays + 1, wins = wins + {}, "
            "best_score = GREATEST(best_score, {}), best_accuracy = GREATEST(best_accuracy, {})",
            guid, song->id, win, score, FormatPermille(accuracy), win, score, FormatPermille(accuracy));
    }

    Accepted accepted;
    accepted.token = token;
    accepted.accuracy = accuracy;
    accepted.maxCombo = maxCombo;
    accepted.score = score;
    accepted.best = best;
    accepted.newBest = newBest;
    accepted.gained = gained;
    accepted.skill = skill;

    // Played together: the ensemble waits for the other bards, then gives the chants and answers everybody.
    if (active.duelId)
        Duel::Report(player, active.duelId, score);

    if (active.ensembleId && Ensemble::Report(player, active.ensembleId, accepted))
        return;

    BuffOutcome outcome = ApplyBuff(player, *song, *instrument, accuracy, maxCombo, active.training);
    SendOutcome(player, *song, *instrument, accepted, outcome, 1, accuracy, 1.0f);
    if (!active.training)
        Titles::Check(player);
}

void SendOutcome(Player* player, Song const& /*song*/, Instrument const& instrument, Accepted const& result,
                 BuffOutcome const& outcome, uint32 group, float groupAccuracy, float groupMultiplier)
{
    // OK^token^accuracy‰^amount^seconds^score^best^newBest^malus^base×100^crescendo‰^capped^text^skillGained^skill
    //    ^groupSize^groupAccuracy‰^groupMultiplier‰^quality‰^tuning‰^tuneMult‰^mastery
    std::string const text = outcome.malus ? instrument.malusName
        : outcome.amount ? instrument.buffName : outcome.message;
    Protocol::Send(player, "OK^" + std::to_string(result.token) + '^' + FormatPermille(result.accuracy) + '^'
        + std::to_string(outcome.amount) + '^' + std::to_string(outcome.durationSec) + '^' + std::to_string(result.score)
        + '^' + std::to_string(result.best) + '^' + (result.newBest ? "1" : "0") + '^' + (outcome.malus ? "1" : "0")
        + '^' + std::to_string(std::lround(outcome.base * 100.0f)) + '^' + FormatPermille(outcome.crescendo)
        + '^' + (outcome.capped ? "1" : "0") + '^' + text + '^' + std::to_string(result.gained)
        + '^' + std::to_string(result.skill) + '^' + std::to_string(group) + '^' + FormatPermille(groupAccuracy)
        + '^' + FormatPermille(groupMultiplier) + '^' + FormatPermille(outcome.quality)
        + '^' + FormatPermille(outcome.tuning) + '^' + FormatPermille(outcome.tuneMult) + '^' + std::to_string(outcome.mastery));
}

void ReleaseHands(Player* player)
{
    uint8 sheath;
    {
        std::lock_guard<std::mutex> guard(sLock);
        sImprov.erase(player->GetGUID().GetCounter());
        auto itr = sHolding.find(player->GetGUID().GetCounter());
        if (itr == sHolding.end())
            return;
        sheath = itr->second;
        sHolding.erase(itr);
    }
    player->SetVisibleItemSlot(EQUIPMENT_SLOT_MAINHAND, player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND));
    player->SetSheath(SheathState(sheath));

    // The song is over: the concert's purse stays a few minutes for the tips.
    Concert::End(player);

    // The aura and the animation leave with the instrument: the song is over, whatever the way it ended.
    for (uint32 i = 0; i < PLAY_AURA_COUNT; ++i)
        player->RemoveAurasDueToSpell(PLAY_AURA_FIRST + i);
    player->SetUInt32Value(UNIT_NPC_EMOTESTATE, 0);
}

std::string BeginImprov(Player* player, uint32 instrumentId, std::string_view voice)
{
    Config const& config = GetConfig();
    if (!config.enable)
        return "Le métier de barde est désactivé sur ce serveur.";
    if (config.requireProfession && !player->HasSpell(PROFESSION_SPELL))
        return "Vous n'avez pas appris le métier de barde.";
    Instrument const* instrument = Store::Instance().GetInstrument(instrumentId);
    if (!instrument)
        return "Instrument inconnu.";
    if (config.requireInstrument && Atelier::BestTier(player, *instrument) < 0)
        return "Il vous faut cet instrument dans vos sacs : " + instrument->name + ".";
    if (!player->IsAlive() || player->IsInCombat())
        return "Impossible de jouer maintenant.";
    {
        std::lock_guard<std::mutex> guard(sLock);
        if (sActive.count(player->GetGUID().GetCounter()))
            return "Un morceau est en cours.";
    }

    HoldInstrument(player, *instrument, nullptr);
    std::lock_guard<std::mutex> guard(sLock);
    // The voice: the instrument's own, or the piano (a sound set the addon has, for improvising and composing).
    sImprov[player->GetGUID().GetCounter()] = { instrument->id, voice == "piano" ? "piano" : "" };
    return {};
}

void EndImprov(Player* player)
{
    ReleaseHands(player);
}

void RelayNotes(Player* player, std::string_view midis)
{
    Config const& config = GetConfig();
    uint32 instrumentId;
    std::string voice;
    {
        std::lock_guard<std::mutex> guard(sLock);
        ObjectGuid::LowType const guid = player->GetGUID().GetCounter();
        auto active = sActive.find(guid);
        auto improv = sImprov.find(guid);
        if (!sHolding.count(guid) || (active == sActive.end() && improv == sImprov.end()))
            return;                                     // not playing
        instrumentId = active != sActive.end() ? active->second.instrumentId : improv->second.first;
        if (active == sActive.end())
            voice = improv->second.second;
    }

    // At most 12 notes per message and 40 per second: a keyboard cannot do more.
    std::string list;
    uint32 count = 0;
    for (std::string_view token : Acore::Tokenize(midis, ',', false))
    {
        Optional<uint32> midi = Acore::StringTo<uint32>(token);
        if (!midi || *midi > 127 || ++count > 12)
            return;
        list += (list.empty() ? "" : ",") + std::string(token);
    }
    if (list.empty())
        return;
    {
        std::lock_guard<std::mutex> guard(sLock);
        auto& [windowMs, notes] = sHits[player->GetGUID().GetCounter()];
        uint32 const now = getMSTime();
        if (getMSTimeDiff(windowMs, now) >= 1000)
        {
            windowMs = now;
            notes = 0;
        }
        notes += count;
        if (notes > 40)
            return;
    }

    // HEAR^instrumentId^midi,midi,...^kind^voice   (kind 1: a partner of the ensemble, 0: somebody who just listens; voice: "piano" or empty)
    std::string const body = "HEAR^" + std::to_string(instrumentId) + '^' + list;
    std::vector<Player*> partners = Ensemble::Partners(player);
    for (Player* partner : partners)
        Protocol::Send(partner, body + "^1^" + voice);

    if (config.listenRange <= 0.0f || !config.listenMax)
        return;
    uint32 sent = 0;
    uint32 const listenMax = Spec::ListenMax(player);
    for (Player* listener : PlayersNear(player, config.listenRange * Spec::RangeFactor(player)))
    {
        if (std::find(partners.begin(), partners.end(), listener) != partners.end())
            continue;
        if (++sent > listenMax)
            break;
        Protocol::Send(listener, body + "^0^" + voice);
    }
}

void Pose(Player* player, uint32 emoteState)
{
    ReleaseHands(player);
    if (!emoteState)
        return;

    Instrument const* instrument = nullptr;
    for (Instrument const* candidate : Store::Instance().GetInstruments())
        if (!instrument && Atelier::BestTier(player, *candidate) >= 0)
            instrument = candidate;
    if (!instrument)
        instrument = Store::Instance().GetDefaultInstrument();
    if (!instrument)
        return;

    HoldInstrument(player, *instrument, nullptr, emoteState);
    player->m_Events.AddEventAtOffset([player]() { ReleaseHands(player); }, std::chrono::milliseconds(25000));
}

void Abort(Player* player, uint32 token)
{
    {
        std::lock_guard<std::mutex> guard(sLock);
        auto itr = sActive.find(player->GetGUID().GetCounter());
        if (itr != sActive.end() && itr->second.token == token)
            sActive.erase(itr);
    }
    ReleaseHands(player);
    Ensemble::Drop(player);
    Duel::Drop(player);
    Concert::Forget(player);
}

void Forget(Player* player)
{
    {
        std::lock_guard<std::mutex> guard(sLock);
        sActive.erase(player->GetGUID().GetCounter());
    }
    Compose::Abort(player);
    ReleaseHands(player);
    Ensemble::Drop(player);
    Duel::Drop(player);
}

void SendSongList(Player* player)
{
    struct Stats { uint32 best = 0; uint32 accuracy = 0; uint32 wins = 0; };
    std::unordered_map<uint32, Stats> stats;
    if (QueryResult result = CharacterDatabase.Query(
            "SELECT song_id, best_score, best_accuracy, wins FROM mod_barde_char_song WHERE guid = {}",
            player->GetGUID().GetCounter()))
    {
        do
        {
            Field* f = result->Fetch();
            stats[f[0].Get<uint32>()] = { f[1].Get<uint32>(), f[2].Get<uint32>(), f[3].Get<uint32>() };
        } while (result->NextRow());
    }

    std::vector<Song const*> songs = Store::Instance().GetSongsFor(player->GetGUID().GetCounter());
    for (Song const* song : songs)
    {
        Stats const& s = stats[song->id];
        // AIR^id^tier^bpm^notes^holds^skill^best^bestAccuracy‰^wins^power‰^subtitle^name^kind^factor‰   (kind 0 the book's, 1 composed by the player, 2 bought)
        Protocol::Send(player, "AIR^" + std::to_string(song->id) + '^' + std::to_string(song->tier) + '^'
            + std::to_string(song->bpm) + '^' + std::to_string(song->notes.size()) + '^' + std::to_string(song->holdCount)
            + '^' + std::to_string(song->skillReq) + '^' + std::to_string(s.best) + '^' + std::to_string(s.accuracy)
            + '^' + std::to_string(s.wins) + '^' + FormatPermille(song->power)
            + '^' + song->subtitle + '^' + song->name + '^' + (song->owner ? (song->owner == player->GetGUID().GetCounter() ? "1" : "2") : "0")
            + '^' + FormatPermille(song->factor));
    }

    // INS^id^name^sound^buff^effect^unit^min*100^max*100^buffSpell^has^bestTier^ownedMask^tuning‰   (bestTier -1 without the instrument)
    for (Instrument const* instrument : Store::Instance().GetInstruments())
    {
        int const best = Atelier::BestTier(player, *instrument);
        bool const has = best >= 0;
        Protocol::Send(player, "INS^" + std::to_string(instrument->id) + '^' + instrument->name + '^' + instrument->sound
            + '^' + instrument->buffName + '^' + instrument->effect + '^' + instrument->unit
            + '^' + std::to_string(std::lround(instrument->minSolo * 100.0f))
            + '^' + std::to_string(std::lround(instrument->maxSolo * 100.0f))
            + '^' + std::to_string(instrument->buffSpell) + '^' + (has ? "1" : "0") + '^' + std::to_string(best)
            + '^' + std::to_string(Atelier::OwnedMask(player, *instrument))
            + '^' + FormatPermille(Atelier::Tuning(player, *instrument)));
    }

    Atelier::SendTiers(player);

    // AIRS_END^count^skill^skillMax^gm   (gm 1 lets the addon offer its test autoplay)
    bool const gm = player->GetSession() && player->GetSession()->GetSecurity() >= SEC_GAMEMASTER;
    Protocol::Send(player, "AIRS_END^" + std::to_string(songs.size()) + '^' + std::to_string(GetSkill(player))
        + '^' + std::to_string(GetConfig().skillMax) + '^' + (gm ? "1" : "0"));
    Spec::Send(player);
}

uint32 GetWins(Player* player, uint32 songId)
{
    if (QueryResult result = CharacterDatabase.Query(
            "SELECT wins FROM mod_barde_char_song WHERE guid = {} AND song_id = {}", player->GetGUID().GetCounter(), songId))
        return result->Fetch()[0].Get<uint32>();
    return 0;
}

uint32 MasteryTier(uint32 wins)
{
    return wins >= 40 ? 3 : wins >= 15 ? 2 : wins >= 5 ? 1 : 0;
}

void SendTop(Player* player, uint32 songId)
{
    std::string list;
    if (songId >= Store::COMPOSITION_FIRST_ID)
    {
        Protocol::Send(player, "TOP^" + std::to_string(songId) + '^');        // the compositions are not ranked
        return;
    }
    if (QueryResult result = CharacterDatabase.Query(
            "SELECT c.name, s.best_score, s.best_accuracy FROM mod_barde_char_song s JOIN characters c ON c.guid = s.guid "
            "WHERE s.song_id = {} AND s.best_score > 0 ORDER BY s.best_score DESC, s.best_accuracy DESC LIMIT 5", songId))
    {
        do
        {
            Field* f = result->Fetch();
            list += (list.empty() ? "" : ";") + f[0].Get<std::string>() + ',' + std::to_string(f[1].Get<uint32>()) + ','
                + std::to_string(f[2].Get<uint32>());
        } while (result->NextRow());
    }
    Protocol::Send(player, "TOP^" + std::to_string(songId) + '^' + list);
}

uint32 GetSkill(Player* player)
{
    if (QueryResult result = CharacterDatabase.Query(
            "SELECT skill FROM mod_barde_char_skill WHERE guid = {}", player->GetGUID().GetCounter()))
        return std::clamp<uint32>(result->Fetch()[0].Get<uint32>(), 1, GetConfig().skillMax);
    return 1;
}

} // namespace Barde::Session
