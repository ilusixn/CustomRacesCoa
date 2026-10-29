/*
 * mod-barde - the Bard secondary profession for AzerothCore.
 * Licensed under the GNU AGPL v3, see LICENSE.
 *
 * v0.1: the rhythm mini-game. The addon plays and judges every note on the
 * player's PC; this module sends it the score, checks the result it sends back
 * and applies the buff. Nothing the client says is trusted without a check.
 */

#ifndef MOD_BARDE_H
#define MOD_BARDE_H

#include "Define.h"
#include "ObjectGuid.h"

#include <map>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class GameObject;
class Item;
class Player;
class WorldObject;

namespace Barde
{

// The players within `range` yards of this object (the object itself, if it is a player, is not included).
std::vector<Player*> PlayersAround(WorldObject* center, float range);

// ---------------------------------------------------------------- config

// The spell that stands for the profession, taught by « Apprenti barde » (1950200) at the Book of Artisans.
constexpr uint32 PROFESSION_SPELL = 1950201;
constexpr uint32 PROFESSION_LEARN_SPELL = 1950200;
// Hidden auras that only carry a spell visual, to show that a bard plays: 1950301 and the following ones,
// one per candidate look (see tools/barde_dbc.py). `.barde aura <n>` shows them one by one.
constexpr uint32 PLAY_AURA_FIRST = 1950301;
constexpr uint32 PLAY_AURA_COUNT = 7;

struct Config
{
    bool enable = true;
    bool requireInstrument = true;
    bool requireProfession = true;      // the profession spell must be learned (Book of Artisans)

    float minAccuracyBuff = 0.50f;      // under this the bard plays a wrong note
    float skillMinAccuracy = 0.70f;     // a song must be played at least this well to earn skill
    uint32 skillMax = 300;              // 300 for a level 60 server, 450 for a WotLK one
    float durationSlackSec = 2.0f;      // a result may arrive this much before the song ends
    uint32 resultGraceSec = 60;         // ... and at most this much after it

    uint32 buffBaseMinutes = 10;
    uint32 buffPerTierMinutes = 5;
    float buffCap = 1.2f;               // a chant never exceeds cap x its base
    float crescendoStep = 0.02f;        // per streak of 25
    float crescendoCap = 0.12f;
    float arenaFactor = 0.0f;
    float battlegroundFactor = 0.5f;

    float trainingTempo = 0.75f;
    float trainingBuffFactor = 0.5f;

    float malusFraction = 0.25f;
    uint32 malusSeconds = 120;

    // What the others see of a bard who plays: an aura (a spell id, 0 for none) and a line of emote text.
    uint32 playAura = 1950307;          // the look: a blue ring with a note
    uint32 playSoundAura = 0;           // a second hidden aura for a sound (0 for none; the only one tried, 1950302, made the bard dance)
    bool playEmote = true;
    std::vector<uint32> playEmoteStates = { 10, 400, 413 };   // animations (emote states): one is drawn for each song; none = no dance

    // Listening: the notes a bard strikes are sent to the players near him (their addon plays the sound).
    float listenRange = 7.5f;          // yards: who hears the notes, and who sees the line of emote text
    uint32 listenMax = 8;               // listeners per bard

    uint32 crystalEntry = 12363;        // the Arcane Crystal that goes with the gem for the best chances (arcanite tier)

    // The bard's camp (v0.4d).
    uint32 campSeconds = 1800;          // how long it stays
    uint32 campCooldownSec = 3600;      // before the same bard may make another
    float campRange = 8.0f;             // yards around the fire
    uint32 campTune = 1;                // percent of tuning an instrument regains every 5 seconds near the fire
    float campListenerFraction = 0.5f;  // share of the bard's chant the listeners get

    // Specializations (v1.0): from this skill, a path (the first choice free); changing it costs this much (copper); the bonus of the chants (%).
    uint32 specMinSkill = 180;
    uint32 specChangeCost = 100000;
    uint32 specChantPercent = 10;

    // Ovations: an applause lengthens the listeners' chants and is counted for the bard.
    uint32 ovationSeconds = 60;
    uint32 ovationMaxSeconds = 300;

    // Concerts (v0.5): a purse on the ground at a camp or in a rest area (tavern, city).
    bool concertEnable = true;
    uint32 concertGraceSec = 180;       // the purse stays this long after the last song, then it is mailed to the bard
    uint32 concertMaxSec = 1800;        // and never longer than this
    uint32 applausePercent = 0;         // more tips per spectator who applauds ... (off: the tips are what the players give)
    uint32 applauseMax = 0;             // ... up to this
    uint32 campTipPercent = 0;          // more tips at a camp (off)
    // The bonuses above are money made out of nothing: they are capped, so that nobody can farm gold with friends.
    uint32 bonusMaxPerConcert = 5000;   // copper of bonus at most for one concert (0: no bonus at all)
    uint32 bonusMaxPerDay = 20000;      // copper of bonus at most per bard and per day (0: no bonus at all)
    bool allowSameAccount = false;      // may a player tip, or applaud, the characters of his own account?
    float concertListenerFraction = 0.25f;  // share of the bard's chant the listeners get in a tavern
    float concertRange = 15.0f;         // yards around the bard in a tavern

    // Reading a scroll: the look (a hidden aura's spell id, 0 for none) kept for a few seconds, and a sound (0 for none).
    uint32 scrollEffect = 1950307;
    uint32 scrollEffectSec = 3;
    uint32 scrollSound = 888;

    // Playing together (v0.3).
    float duoMultiplier = 1.6f;
    float trioMultiplier = 2.0f;
    uint32 composeMax = 10;             // compositions per player
    uint32 composeMinNotes = 16;
    uint32 composeMinSeconds = 20;
    uint32 composeMaxSeconds = 240;
    uint32 maxChants = 3;               // different chants on one person
    uint32 ensembleInviteSec = 20;      // time given to answer an invitation
    uint32 ensembleCountdownSec = 5;    // preparation: the song starts this long after everybody has answered
    float ensembleRange = 60.0f;        // yards between the leader and an invited bard

    void Load();
};

Config const& GetConfig();
void LoadConfig();

// ----------------------------------------------------------------- data

enum Tier : uint8
{
    TIER_APPRENTI   = 0,
    TIER_COMPAGNON  = 1,
    TIER_MENESTREL  = 2,
    TIER_MAITRE     = 3,
    TIER_GRAND      = 4,
};

char const* TierName(uint8 tier);

// A held note lasts `hold` sixteenths and must be kept down for HOLD_FRACTION of it. It is
// judged twice, like in the prototype: once when struck, once when released.
constexpr float HOLD_FRACTION = 0.8f;

struct Note
{
    uint32 step;    // position in sixteenth notes from the start
    uint8 midi;     // pitch, also the sound the addon plays
    uint32 hold;    // length of a held note in sixteenths, 0 for a tap
    uint8 lane;     // 1..4, low pitches on the left
};

// The parts of a song played together. The melody is the song itself; the rhythm (the drum) and the harmony
// are made from it when the songs are loaded (see BardeStore.cpp).
enum Part : uint8
{
    PART_MELODY  = 0,
    PART_RHYTHM  = 1,
    PART_HARMONY = 2,
    PART_AUTO    = 255,     // the instrument's own part
};

// One playable sequence of notes: what the addon shows and what the result is checked against.
struct Chart
{
    std::vector<Note> notes;

    float endStep = 0.0f;       // last note, plus the held part of a held note
    uint32 holdCount = 0;
    uint32 maxScore = 0;        // every note perfect, every hold kept

    // Number of judgements: every note, plus the release of every held note.
    uint32 Judgements() const { return uint32(notes.size()) + holdCount; }
};

// A song is its melody (the Chart it is made of) plus the two other parts.
struct Song : Chart
{
    uint32 id = 0;
    std::string name;
    uint8 tier = TIER_APPRENTI;
    uint16 skillReq = 0;
    uint16 bpm = 0;
    std::string subtitle;       // "lai, mode de ré"
    float power = 0.0f;         // 0 for the first song of the book, 1 for the hardest
    ObjectGuid::LowType owner = 0;  // the composer, for a composition (0 for the book's songs)
    float factor = 1.0f;        // a multiplier of the chant (1 for every song for now)
    bool removed = false;       // a deleted composition stays in memory (a song may be playing) but is no longer offered
    Chart rhythm;
    Chart harmony;

    Chart const& GetChart(uint8 part) const
    {
        return part == PART_RHYTHM ? rhythm : part == PART_HARMONY ? harmony : static_cast<Chart const&>(*this);
    }

    // Real length in milliseconds at the given tempo factor (1 normal, 0.75 training).
    uint32 LengthMs(float tempo) const { return ChartLengthMs(*this, tempo); }
    uint32 ChartLengthMs(Chart const& chart, float tempo) const;
};

struct Instrument
{
    uint32 id = 0;
    std::string name;
    uint8 tier = TIER_APPRENTI;
    uint32 itemEntry = 0;
    std::string buffName;
    std::string effect;         // "mana toutes les 5 s", "de vitesse de déplacement"
    std::string unit;           // "%" when the value is a percentage
    std::string sound;          // prefix of the addon's note sounds
    uint32 buffSpell = 0;
    uint32 buffAura = 0;
    uint32 buffAura2 = 0;       // second effect set to the same value (0 = none)
    float minSolo = 0.0f;       // solo buff on the first song of the book
    float maxSolo = 0.0f;       // ... and on the hardest one

    float Base(Song const& song) const { return (minSolo + (maxSolo - minSolo) * song.power) * song.factor; }

    // The drum plays the rhythm; every other instrument plays the melody unless an ensemble gives it the harmony.
    uint8 NaturalPart() const { return sound == "tamb" ? PART_RHYTHM : PART_MELODY; }
    std::string malusName;
    uint32 malusSpell = 0;
};

// A tier of instruments (copper, silver, mithril, thorium, arcanite). An instrument of tier t is the item
// `itemEntry + 1000 * t` of the copper one; making it takes the bard skill and the materials below (none: it is offered).
struct InstrumentTier
{
    uint8 tier = 0;
    std::string suffix;                                         // "de cuivre", "d'argent"...
    uint16 skillReq = 0;
    uint32 gemEntry = 0;                                        // the quality gem of the tier (optional)
    std::vector<std::pair<uint32, uint32>> materials;           // item entry, count
};

class Store
{
public:
    static Store& Instance();

    // Loads songs and instruments from the world database; returns false and
    // logs every bad row when something cannot be used.
    bool Load();

    Song const* GetSong(uint32 id) const;
    std::vector<Song const*> GetSongs() const;
    // The songs a player is offered: the book's, and his own compositions.
    std::vector<Song const*> GetSongsFor(ObjectGuid::LowType guid) const;

    // Compositions (see BardeCompose.cpp): the first id of the compositions, and the store of the notes the player made.
    static constexpr uint32 COMPOSITION_FIRST_ID = 1000000;
    // Makes a song out of the notes ("step:midi" tokens) and keeps it; returns the song, or null with a reason in `error`.
    Song const* AddComposition(uint32 id, ObjectGuid::LowType owner, std::string const& title, std::string const& composer,
                               uint8 tier, std::string const& notes, std::string& error);
    void RemoveComposition(uint32 id);

    // Songs a player learned from a scroll (a composition of somebody else). A player plays his own compositions and the ones he learned.
    bool Knows(ObjectGuid::LowType guid, Song const& song) const;
    void Learn(ObjectGuid::LowType guid, uint32 songId);
    void Unlearn(ObjectGuid::LowType guid, uint32 songId);
    Instrument const* GetDefaultInstrument() const;     // the lowest id, the lute
    Instrument const* GetInstrument(uint32 id) const;
    std::vector<Instrument const*> GetInstruments() const;  // by id

    std::vector<InstrumentTier> const& GetTiers() const { return _tiers; }
    InstrumentTier const* GetTier(uint8 tier) const;
    uint32 ItemFor(uint32 instrumentId, uint8 tier) const;  // 0 when there is none

private:
    std::unordered_map<uint32, Song> _songs;
    std::map<ObjectGuid::LowType, std::set<uint32>> _learned;       // player -> songs learned from scrolls
    std::unordered_map<uint32, Instrument> _instruments;
    std::vector<InstrumentTier> _tiers;
    std::map<std::pair<uint32, uint8>, uint32> _items;     // (instrument, tier) -> item entry
};

// -------------------------------------------------------------- scoring

namespace Score
{
    constexpr uint32 PERFECT = 100;
    constexpr uint32 GOOD = 50;
    constexpr uint32 HOLD = 50;
    constexpr uint32 COMBO_STEP = 25;

    // Multiplier of a note played with `comboBefore` notes already in a row.
    inline float ComboMultiplier(uint32 comboBefore) { return 1.0f + 0.1f * float(comboBefore / COMBO_STEP); }
}

// ------------------------------------------------------------- protocol

namespace Protocol
{
    constexpr char const* PREFIX = "BARDE";
    constexpr char SEP = '^';

    void Send(Player* player, std::string const& body);
    std::vector<std::string_view> Split(std::string_view body);

    // Handles one message from the addon. Called from the chat hook.
    void Handle(Player* player, std::string_view body);
}

// ----------------------------------------------------------------- buff

struct BuffOutcome
{
    bool malus = false;
    int32 amount = 0;
    uint32 durationSec = 0;
    float base = 0.0f;
    float crescendo = 0.0f;
    bool capped = false;
    float quality = 1.0f;   // the instrument's quality multiplier, already in `base`
    float tuning = 1.0f;    // how well tuned the instrument is after this song (0 to 1)
    float tuneMult = 1.0f;  // what its tuning did to the chant before this song, already in `base`
    uint32 mastery = 0;     // the bard's mastery of this song: 0 none, 1 bronze, 2 silver, 3 gold (already in `base`)
    std::string message;    // why there is no buff, when there is none
};

// A song played alone: the chant goes to the bard.
BuffOutcome ApplyBuff(Player* player, Song const& song, Instrument const& instrument,
                      float accuracy, uint32 maxCombo, bool training);

// A song played together: the base is multiplied by `groupMultiplier` and the bard's chant goes to every
// recipient. `granted` (optional) receives how many of them got it.
BuffOutcome ApplyGroupBuff(Player* bard, std::vector<Player*> const& recipients, Song const& song,
                           Instrument const& instrument, float accuracy, uint32 maxCombo, bool training,
                           float groupMultiplier, uint32* granted);


// -------------------------------------------------------------- session

namespace Session
{
    // Why this player cannot play this song with this instrument right now; empty when he can.
    std::string CanPlay(Player* player, Song const& song, Instrument const& instrument);

    // Starts a song for the player and sends its score to the addon.
    // Returns an error text for the player, empty on success. `ensembleId` and `groupSize`
    // are set when the song is played together (see Ensemble).
    std::string Begin(Player* player, uint32 songId, bool training, uint32 instrumentId,
                      uint32 ensembleId = 0, uint32 groupSize = 1, uint8 part = PART_AUTO, uint32 duelId = 0);

    void HandleResult(Player* player, std::vector<std::string_view> const& fields);

    // A result that passed every check: what the bard played, to be turned into a buff.
    struct Accepted
    {
        uint32 token = 0;
        float accuracy = 0.0f;
        uint32 maxCombo = 0;
        uint32 score = 0;
        uint32 best = 0;
        bool newBest = false;
        uint32 gained = 0;      // skill points
        uint32 skill = 0;
    };

    // OK message to the bard: his result and what the song gave. `group` is the size of the ensemble
    // (1 alone) and `groupAccuracy` its average accuracy.
    void SendOutcome(Player* player, Song const& song, Instrument const& instrument, Accepted const& result,
                     BuffOutcome const& outcome, uint32 group, float groupAccuracy, float groupMultiplier);

    // The instrument is drawn in the bard's hand while he plays (it replaces his weapon's appearance,
    // not the weapon itself). ReleaseHands puts his own look back; it is safe to call at any time.
    void ReleaseHands(Player* player);

    // Improvisation: the bard plays freely (no score, no chant) with the instrument in hand; his notes go to the players near him (HITS).
    std::string BeginImprov(Player* player, uint32 instrumentId, std::string_view voice);
    void EndImprov(Player* player);

    // A bard struck notes: his ensemble partners hear them (HEAR, kind 1), and so do the players near him (kind 0).
    void RelayNotes(Player* player, std::string_view midis);

    // To try animations (GM): the instrument in hand, the playing aura and the given emote state, for 25 seconds
    // or until Pose(player, 0). The emote states come from `.barde pose`.
    void Pose(Player* player, uint32 emoteState);
    void Abort(Player* player, uint32 token);
    void Forget(Player* player);

    void SendSongList(Player* player);

    // The bard's skill, 1 to Barde.SkillMax. A new bard starts at 1.
    uint32 GetSkill(Player* player);

    // Mastery of a song: bronze after 5 successes (+1 % to the chant), silver after 15 (+2 %), gold after 40 (+3 %). A modest bonus.
    uint32 GetWins(Player* player, uint32 songId);
    uint32 MasteryTier(uint32 wins);

    // The ranking of a song (the five best scores of the server): TOP^songId^name,score,accuracy‰;...
    void SendTop(Player* player, uint32 songId);
}

// -------------------------------------------------------------- atelier

namespace Atelier
{
    // The best tier of this instrument the player carries, -1 when he has none.
    int BestTier(Player* player, Instrument const& instrument);

    // Which tiers of this instrument he carries: bit t set for tier t.
    uint32 OwnedMask(Player* player, Instrument const& instrument);

    // The item entry of the best one, 0 when he has none.
    uint32 BestItem(Player* player, Instrument const& instrument);

    // Tuning: an instrument loses 4 % of its tuning per song (3 % the drum; never the legendary), kept as the item's durability. Under
    // 50 % the chant weakens down to x0.85; a tuning key (made at the workshop) tunes it back to 100 %.
    constexpr uint32 TUNING_KEY = 1950020;
    float Tuning(Player* player, Instrument const& instrument);             // of the best one he carries, 0 to 1
    float TuningMultiplier(float tuning);
    float WearOut(Player* player, Instrument const& instrument, bool training);   // after a song; returns the new tuning
    std::string Tune(Player* player, uint32 instrumentId);
    std::string CraftKeys(Player* player);                                  // 5 keys for a copper bar and a coarse thread

    // Instrument quality: 0 plain, 1 uncommon (x1.05), 2 rare (x1.10), 3 epic (x1.15), 4 legendary (x1.20). It is a permanent
    // enchantment of the item (ENCHANT_FIRST + quality - 1) that the client shows in the tooltip.
    constexpr uint32 ENCHANT_FIRST = 1950501;
    uint8 QualityOf(Item const* item);
    float QualityMultiplier(Player* player, Instrument const& instrument);
    char const* QualityName(uint8 quality);
    void SetQuality(Item* item, uint8 quality);

    // Makes a camp kit (4 wool cloth, 2 medium leather, 1 flint and tinder), and gives the instruments he carries `percent` of tuning back.
    std::string CraftCamp(Player* player);
    void TuneStep(Player* player, uint32 percent);

    // Makes the instrument of this tier for the player: needs the profession, the skill and the materials, and room in the
    // bags. `gemMode` 0 none, 1 with the tier's gem, 2 with the gem and the Arcane Crystal (arcanite only); the quality is drawn
    // at random, better with a gem, a little better with skill. Returns an error text, empty on success, and the quality's name.
    std::string Craft(Player* player, uint32 instrumentId, uint8 tier, uint8 gemMode, std::string& qualityName);

    // TIER^tier^suffix^skillReq^item:count:name,item:count:name^gemItem:gemName^crystalItem:crystalName   one per tier
    void SendTiers(Player* player);
}

// ----------------------------------------------------------------- camp

// Objects of the camp (gameobject_template, see data/sql/db-world/mod_barde_world_camp.sql) and the camp kit item.
constexpr uint32 CAMP_FIRE = 1950401;       // a cooking fire, like the world's campfires
constexpr uint32 CAMP_BENCH = 1950402;      // a bench to sit on
constexpr uint32 CAMP_KIT = 1950030;

namespace Camp
{
    // Puts the camp down where the player stands. Returns an error text, empty on success.
    std::string Place(Player* player);

    // Is this player near a camp's fire? Who else is near it (listeners)?
    bool IsAtCamp(Player* player);
    std::vector<Player*> Listeners(Player* bard);

    // Called every second: ends the old camps and tunes the instruments of the bards at the fire.
    void Update();
}

// --------------------------------------------------------------- concert

constexpr uint32 CONCERT_PURSE = 1950403;       // the purse (gameobject_template, see mod_barde_world_camp.sql)

namespace Concert
{
    // Can this bard give a concert where he stands (a camp, a tavern, a city)?
    bool Eligible(Player* player);

    // A song starts (the purse appears) or is over (the purse stays a few minutes), a bard leaves; called every second.
    void Begin(Player* bard, Song const& song);
    void End(Player* bard);
    void Forget(Player* bard);
    void Update();

    // After a duel: the tips of the two bards' purses go to the winner, or are shared in two in a draw.
    void SettleDuel(ObjectGuid winner, ObjectGuid loser, bool draw);

    // A spectator applauds near a purse.
    void Applaud(Player* player);

    // The listeners of a concert and the share of the chant they get (the camp's own share at a camp).
    std::vector<Player*> Listeners(Player* bard, float& fraction);

    // The clicks on the purse (gossip menu).
    bool Hello(Player* player, GameObject* go);
    bool Select(Player* player, GameObject* go, uint32 sender, uint32 action);
}

// ---------------------------------------------------------------- duel

// Two bards play the same song at the same moment; the better score wins, and the tips of the purses.
namespace Duel
{
    // The challenger has selected the other bard. Returns an error text, empty on success.
    std::string Propose(Player* challenger, uint32 songId, uint32 instrumentId);
    void Answer(Player* player, uint32 duelId, bool accept, uint32 instrumentId);

    // A bard's result passed the checks.
    void Report(Player* player, uint32 duelId, uint32 score);

    // A bard leaves (aborts, logs out); called every second for the time-outs.
    void Drop(Player* player);
    void Update();
    bool InDuel(Player* player);
}

// ----------------------------------------------------------- composition

// The bard records an improvisation, gives it a title, and the server keeps it as a song of his own. See docs/v0.6-creation.md.
namespace Compose
{
    // The addon sends the notes in pieces: COMPBEGIN, COMPN..., COMPEND.
    void Begin(Player* player, std::string_view title);
    void Add(Player* player, std::string_view notes);
    std::string End(Player* player);                // makes the composition; an error text on failure
    std::string Delete(Player* player, uint32 songId);          // a composition of one's own, or a learned one (forgotten)

    // A scroll of a song: copying one of one's compositions onto a scroll (1 Light Parchment, 1 Moonglow Ink), reading it to learn the song.
    constexpr uint32 SCROLL_ITEM = 1950040;
    std::string Copy(Player* player, uint32 songId);
    std::string Read(Player* player, Item* scroll);            // learns the song; the caller then uses the scroll up
    void SendScrollInfo(Player* player, uint32 songId);
    void Abort(Player* player);                     // the bard left
    uint32 Count(Player* player);
}

// ---------------------------------------------------------------- titles

// What a bard has done (ovations, tips earned, duels won) and the titles it earns.
namespace Titles
{
    void Add(ObjectGuid::LowType guid, char const* column, uint64 amount);   // column: ovations, tips, duels_won
    uint64 Get(ObjectGuid::LowType guid, char const* column);
    void Check(Player* player);                  // gives the titles the bard has earned
    void GiveLegend(Player* player);             // the title of the legendary instrument
}

// ------------------------------------------------------------ the legendary instrument

namespace Legend
{
    bool IsVirtuoso(ObjectGuid::LowType guid);                                   // gold mastery on three songs of the book
    void OnDuelWon(ObjectGuid::LowType winner, ObjectGuid::LowType loser);       // the duel step, when the loser is a virtuoso
    void OnTrio(std::vector<Player*> const& bards, Song const& song, std::vector<float> const& accuracies);   // the trio step
    void Send(Player* player);                                                   // LEGEND^...
    std::string Deliver(Player* player);                                         // hands in the materials; empty when done, else the reason
    std::string Forge(Player* player, uint32 instrumentId);                      // the last step; empty when done, else the reason
    std::string GmTest(Player* player, std::string const& what);                 // .barde legende tout|reset: for tests, a GM validates the steps
}

// ------------------------------------------------------------ specializations

namespace Spec
{
    constexpr uint32 NONE = 0, WAR = 1, COURT = 2, TALE = 3;
    uint32 Get(Player* player);
    void Forget(Player* player);
    float ChantFactor(Player* player, Instrument const& instrument);    // x1.1 on the chants of the path
    float MoodFactor(Player* player);                                  // the share of the chant the listeners get
    float RangeFactor(Player* player);                                 // the range at which the bard is heard
    uint32 ListenMax(Player* player);
    uint32 OvationWeight(Player* player);                              // what an applause counts for
    std::string Choose(Player* player, uint32 spec);                   // empty when done, else the reason
    void Send(Player* player);                                         // SPEC^current^minSkill^cost
}

// ------------------------------------------------------------- ensemble

// Bards of one party playing the same song at the same time (duo, trio). The leader proposes a song,
// the other bards of the party accept with an instrument, everybody plays, and the average accuracy
// decides the chants, multiplied and given to the whole party. See docs/v0.3-ensemble.md.
namespace Ensemble
{
    // The leader asks the party to play `songId` together. Returns an error text, empty on success.
    std::string Propose(Player* leader, uint32 songId, uint32 instrumentId);

    // A bard answers an invitation. `instrumentId` 0 lets the server pick one he owns.
    void Answer(Player* player, uint32 ensembleId, bool accept, uint32 instrumentId);

    // A bard's result passed the checks; the ensemble waits for the others, then gives the chants.
    // The other bards of the ensemble this player plays in, while the song is on (none when he plays alone).
    std::vector<Player*> Partners(Player* player);

    // Returns false when the ensemble no longer exists; the song is then treated as played alone.
    bool Report(Player* player, uint32 ensembleId, Session::Accepted const& result);

    // A bard leaves (aborts, logs out): the others go on without him.
    void Drop(Player* player);

    // Called every world tick: invitations and songs that ran out of time.
    void Update();

    // For testing alone (GM): a duo with a virtual partner on another instrument, who hands in a result of
    // the given accuracy (0..1) when the song's length has passed. His chant is not applied.
    std::string TestDuo(Player* leader, uint32 songId, float accuracy);
}

} // namespace Barde

#endif
