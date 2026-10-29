/*
 * mod-barde - songs and instruments, loaded from the world database.
 * Licensed under the GNU AGPL v3, see LICENSE.
 *
 * A song's notes are stored as text, one note per token separated by spaces:
 *     step:midi        a tap, `step` in sixteenth notes from the start
 *     step:midi:len    a held note lasting `len` sixteenths (kept down for HOLD_FRACTION of it)
 *     step:midi:len:lane   the same with its lane, 1 to 4 from low to high
 * The prototype's songs carry their lanes. Without them (hand-written songs), the lane of each
 * note is computed here from the song's own pitches, the way the prototype does.
 */

#include "Barde.h"

#include "DatabaseEnv.h"
#include "Field.h"
#include "Log.h"
#include "QueryResult.h"
#include "SpellMgr.h"
#include "StringConvert.h"
#include "Tokenize.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace Barde
{

char const* TierName(uint8 tier)
{
    switch (tier)
    {
        case TIER_APPRENTI:  return "Apprenti";
        case TIER_COMPAGNON: return "Compagnon";
        case TIER_MENESTREL: return "Ménestrel";
        case TIER_MAITRE:    return "Maître";
        case TIER_GRAND:     return "Grand barde";
        default:             return "?";
    }
}

uint32 Song::ChartLengthMs(Chart const& chart, float tempo) const
{
    // A sixteenth lasts 15000 / bpm milliseconds.
    return uint32(std::lround(double(chart.endStep) * 15000.0 / (double(bpm) * double(tempo))));
}

namespace
{

// Lanes: walk the pitches from low to high and cut where a quarter of the notes has
// been reached, so the lanes get about as many notes each and a pitch always lands
// in the same lane. If that leaves a lane empty, split the pitches evenly instead.
void AssignLanes(std::vector<Note>& notes)
{
    std::map<uint8, uint32> pitchCount;
    for (Note const& note : notes)
        ++pitchCount[note.midi];
    std::map<uint8, uint8> pitchLane;
    std::set<uint8> usedLanes;
    uint32 before = 0;
    for (auto const& [midi, count] : pitchCount)
    {
        double middle = double(before) + double(count) / 2.0;
        uint8 lane = uint8(std::min<uint32>(3, uint32(4.0 * middle / double(notes.size()))));
        pitchLane[midi] = lane;
        usedLanes.insert(lane);
        before += count;
    }
    if (usedLanes.size() < 4 && pitchCount.size() >= 4)
    {
        uint32 rank = 0;
        for (auto& [midi, lane] : pitchLane)
            lane = uint8(std::min<uint32>(3, rank++ * 4 / uint32(pitchCount.size())));
    }
    for (Note& note : notes)
        note.lane = pitchLane[note.midi] + 1;
}

// End of the last note, number of holds and best possible score of a chart whose notes are in place.
void FinishChart(Chart& chart)
{
    chart.endStep = 0.0f;
    chart.holdCount = 0;
    for (Note const& note : chart.notes)
    {
        chart.endStep = std::max(chart.endStep, float(note.step) + float(note.hold) * HOLD_FRACTION);
        if (note.hold)
            ++chart.holdCount;
    }

    // Best possible score: every note perfect (100), every hold kept (50), combo unbroken.
    // The order of the 100s and 50s changes the combo bonus slightly; putting the 100s
    // last gives an upper bound, which is all the check needs.
    double total = 0.0;
    uint32 const judgements = chart.Judgements();
    for (uint32 i = 0; i < judgements; ++i)
        total += (i < chart.holdCount ? Score::HOLD : Score::PERFECT) * Score::ComboMultiplier(i);
    chart.maxScore = uint32(std::lround(total));   // exact in theory, rounded against float error
}

// The drum: a beat every quarter note (the bar's first on the low skin, the third on the second skin, the
// others on the third), plus a high skin on the off-beats where the melody has a note.
void BuildRhythm(Song& song)
{
    constexpr uint8 LANE_MIDI[4] = { 55, 60, 67, 74 };
    std::set<uint32> melodySteps;
    for (Note const& note : song.notes)
        melodySteps.insert(note.step);

    uint32 const last = uint32(song.endStep);
    for (uint32 step = 0; step <= last; step += 4)
    {
        uint8 const lane = step % 16 == 0 ? 1 : step % 16 == 8 ? 2 : 3;
        song.rhythm.notes.push_back({ step, LANE_MIDI[lane - 1], 0, lane });
        if (step + 2 <= last && melodySteps.count(step + 2))
            song.rhythm.notes.push_back({ step + 2, LANE_MIDI[3], 0, 4 });
    }
    FinishChart(song.rhythm);
}

// The second voice: the melody a fifth lower (organum), on the beats and wherever a note is held, with the
// held notes cut short so that none runs into the next note of its lane.
void BuildHarmony(Song& song)
{
    for (Note const& note : song.notes)
        if (note.hold || note.step % 4 == 0)
            song.harmony.notes.push_back({ note.step, uint8(note.midi >= 43 ? note.midi - 7 : note.midi), note.hold, 0 });
    if (song.harmony.notes.empty())
        return;

    AssignLanes(song.harmony.notes);
    std::vector<Note>& notes = song.harmony.notes;
    for (std::size_t i = 0; i < notes.size(); ++i)
    {
        if (!notes[i].hold)
            continue;
        for (std::size_t j = i + 1; j < notes.size(); ++j)
            if (notes[j].lane == notes[i].lane)
            {
                uint32 const room = notes[j].step > notes[i].step ? notes[j].step - notes[i].step - 1 : 0;
                notes[i].hold = std::min(notes[i].hold, room);
                break;
            }
        if (notes[i].hold < 2)
            notes[i].hold = 0;
    }
    FinishChart(song.harmony);
}

bool ParseNotes(Song& song, std::string const& text)
{
    bool explicitLanes = true;
    for (std::string_view token : Acore::Tokenize(text, ' ', false))
    {
        std::vector<std::string_view> parts = Acore::Tokenize(token, ':', true);
        if (parts.size() < 2 || parts.size() > 4)
            return false;

        Optional<uint32> step = Acore::StringTo<uint32>(parts[0]);
        Optional<uint32> midi = Acore::StringTo<uint32>(parts[1]);
        Optional<uint32> hold = parts.size() >= 3 ? Acore::StringTo<uint32>(parts[2]) : Optional<uint32>(0);
        Optional<uint32> lane = parts.size() == 4 ? Acore::StringTo<uint32>(parts[3]) : Optional<uint32>(0);
        if (!step || !midi || !hold || !lane || *midi > 127 || *lane > 4)
            return false;

        if (parts.size() < 4)
            explicitLanes = false;
        song.notes.push_back({ *step, uint8(*midi), *hold, uint8(*lane) });
    }

    if (song.notes.empty())
        return false;

    std::stable_sort(song.notes.begin(), song.notes.end(),
        [](Note const& a, Note const& b) { return a.step < b.step; });

    // Lanes, unless every note brought its own.
    if (!explicitLanes)
        AssignLanes(song.notes);
    FinishChart(song);

    BuildRhythm(song);
    BuildHarmony(song);
    return true;
}

bool Clean(std::string const& text)
{
    return text.find(Protocol::SEP) == std::string::npos;
}

} // namespace

Store& Store::Instance()
{
    static Store instance;
    return instance;
}

bool Store::Load()
{
    _songs.clear();
    _instruments.clear();
    bool ok = true;

    if (QueryResult result = WorldDatabase.Query(
            "SELECT id, name, subtitle, tier, skill_req, bpm, power, notes FROM mod_barde_song"))
    {
        do
        {
            Field* f = result->Fetch();
            Song song;
            song.id = f[0].Get<uint32>();
            song.name = f[1].Get<std::string>();
            song.subtitle = f[2].Get<std::string>();
            song.tier = f[3].Get<uint8>();
            song.skillReq = f[4].Get<uint16>();
            song.bpm = f[5].Get<uint16>();
            song.power = f[6].Get<float>();

            if (song.bpm == 0 || song.bpm > 130 || !Clean(song.name) || !Clean(song.subtitle)
                || song.power < 0.0f || song.power > 1.0f || !ParseNotes(song, f[7].Get<std::string>()))
            {
                LOG_ERROR("module", "mod-barde: song {} ({}) is invalid and was skipped "
                    "(bpm 1-130, power 0-1, no '^' in the texts, notes 'step:midi[:len]').", song.id, song.name);
                ok = false;
                continue;
            }

            _songs.emplace(song.id, std::move(song));
        } while (result->NextRow());
    }

    if (QueryResult result = WorldDatabase.Query(
            "SELECT id, name, sound, tier, item_entry, buff_name, effect, unit, buff_spell, buff_aura, buff_aura2, "
            "min_solo, max_solo, malus_name, malus_spell FROM mod_barde_instrument"))
    {
        do
        {
            Field* f = result->Fetch();
            Instrument instrument;
            instrument.id = f[0].Get<uint32>();
            instrument.name = f[1].Get<std::string>();
            instrument.sound = f[2].Get<std::string>();
            instrument.tier = f[3].Get<uint8>();
            instrument.itemEntry = f[4].Get<uint32>();
            instrument.buffName = f[5].Get<std::string>();
            instrument.effect = f[6].Get<std::string>();
            instrument.unit = f[7].Get<std::string>();
            instrument.buffSpell = f[8].Get<uint32>();
            instrument.buffAura = f[9].Get<uint32>();
            instrument.buffAura2 = f[10].Get<uint32>();
            instrument.minSolo = f[11].Get<float>();
            instrument.maxSolo = f[12].Get<float>();
            instrument.malusName = f[13].Get<std::string>();
            instrument.malusSpell = f[14].Get<uint32>();

            if (!sSpellMgr->GetSpellInfo(instrument.buffSpell) || !sSpellMgr->GetSpellInfo(instrument.malusSpell))
            {
                LOG_ERROR("module", "mod-barde: instrument {} ({}) names a spell missing from Spell.dbc and was skipped.",
                    instrument.id, instrument.name);
                ok = false;
                continue;
            }
            if (!Clean(instrument.name) || !Clean(instrument.buffName) || !Clean(instrument.effect)
                || !Clean(instrument.malusName) || !Clean(instrument.unit) || !Clean(instrument.sound))
            {
                LOG_ERROR("module", "mod-barde: instrument {} has a '^' in one of its texts and was skipped.", instrument.id);
                ok = false;
                continue;
            }

            _instruments.emplace(instrument.id, std::move(instrument));
        } while (result->NextRow());
    }

    _tiers.clear();
    _items.clear();
    if (QueryResult result = WorldDatabase.Query("SELECT tier, suffix, skill_req FROM mod_barde_tier ORDER BY tier"))
        do
        {
            Field* f = result->Fetch();
            InstrumentTier tier;
            tier.tier = f[0].Get<uint8>();
            tier.suffix = f[1].Get<std::string>();
            tier.skillReq = f[2].Get<uint16>();
            if (!Clean(tier.suffix))
            {
                LOG_ERROR("module", "mod-barde: tier {} has a '^' in its name and was skipped.", tier.tier);
                ok = false;
                continue;
            }
            _tiers.push_back(std::move(tier));
        } while (result->NextRow());
    if (QueryResult result = WorldDatabase.Query("SELECT tier, item_entry, count FROM mod_barde_tier_material"))
        do
        {
            Field* f = result->Fetch();
            for (InstrumentTier& tier : _tiers)
                if (tier.tier == f[0].Get<uint8>())
                    tier.materials.emplace_back(f[1].Get<uint32>(), f[2].Get<uint32>());
        } while (result->NextRow());
    if (QueryResult result = WorldDatabase.Query("SELECT tier, gem_entry FROM mod_barde_tier_gem"))
        do
        {
            Field* f = result->Fetch();
            for (InstrumentTier& tier : _tiers)
                if (tier.tier == f[0].Get<uint8>())
                    tier.gemEntry = f[1].Get<uint32>();
        } while (result->NextRow());
    if (QueryResult result = WorldDatabase.Query("SELECT item_entry, instrument_id, tier FROM mod_barde_instrument_item"))
        do
        {
            Field* f = result->Fetch();
            _items[{ f[1].Get<uint32>(), f[2].Get<uint8>() }] = f[0].Get<uint32>();
        } while (result->NextRow());

    // The compositions the players made.
    uint32 compositions = 0;
    if (QueryResult result = CharacterDatabase.Query(
            "SELECT c.id, c.guid, c.title, c.tier, c.notes, IFNULL(ch.name, 'un barde') FROM mod_barde_composition c "
            "LEFT JOIN characters ch ON ch.guid = c.guid"))
        do
        {
            Field* f = result->Fetch();
            std::string error;
            if (AddComposition(COMPOSITION_FIRST_ID + f[0].Get<uint32>(), f[1].Get<uint32>(), f[2].Get<std::string>(),
                    f[5].Get<std::string>(), f[3].Get<uint8>(), f[4].Get<std::string>(), error))
                ++compositions;
            else
                LOG_ERROR("module", "mod-barde: composition {} is invalid and was skipped ({}).", f[0].Get<uint32>(), error);
        } while (result->NextRow());

    _learned.clear();
    if (QueryResult result = CharacterDatabase.Query("SELECT guid, song_id FROM mod_barde_char_learned"))
        do
        {
            Field* f = result->Fetch();
            _learned[f[0].Get<uint32>()].insert(f[1].Get<uint32>());
        } while (result->NextRow());

    LOG_INFO("server.loading", ">> mod-barde: {} songs, {} compositions, {} instruments, {} tiers loaded.", _songs.size() - compositions,
        compositions, _instruments.size(), _tiers.size());
    if (_songs.empty() || _instruments.empty())
    {
        LOG_ERROR("module", "mod-barde: no song or no instrument; was data/sql/db-world applied?");
        ok = false;
    }
    return ok;
}

InstrumentTier const* Store::GetTier(uint8 tier) const
{
    for (InstrumentTier const& candidate : _tiers)
        if (candidate.tier == tier)
            return &candidate;
    return nullptr;
}

uint32 Store::ItemFor(uint32 instrumentId, uint8 tier) const
{
    auto itr = _items.find({ instrumentId, tier });
    return itr != _items.end() ? itr->second : 0;
}

Song const* Store::GetSong(uint32 id) const
{
    auto itr = _songs.find(id);
    return itr != _songs.end() ? &itr->second : nullptr;
}

std::vector<Song const*> Store::GetSongsFor(ObjectGuid::LowType guid) const
{
    std::vector<Song const*> songs;
    for (auto const& [id, song] : _songs)
        if (!song.removed && (!song.owner || Knows(guid, song)))
            songs.push_back(&song);
    std::sort(songs.begin(), songs.end(), [](Song const* a, Song const* b)
    {
        if (a->tier != b->tier)
            return a->tier < b->tier;
        if ((a->owner != 0) != (b->owner != 0))
            return a->owner == 0;               // the book's songs first, then the compositions
        return a->id < b->id;
    });
    return songs;
}

Song const* Store::AddComposition(uint32 id, ObjectGuid::LowType owner, std::string const& title, std::string const& composer,
                                  uint8 tier, std::string const& notes, std::string& error)
{
    // Skill needed and weight of the song, by tier: the first song of the tier's requirement, and the middle of its range.
    static uint16 const REQ[5] = { 1, 60, 120, 180, 240 };
    if (tier > 4 || !Clean(title) || !Clean(composer))
    {
        error = "Titre ou palier invalide.";
        return nullptr;
    }

    Song song;
    song.id = id;
    song.name = title;
    song.subtitle = "composition de " + composer;
    song.tier = tier;
    song.skillReq = REQ[tier];
    song.bpm = 84;
    song.power = (8.0f * float(tier) + 3.5f) / 39.0f;
    song.owner = owner;
    if (!ParseNotes(song, notes))
    {
        error = "Notes invalides.";
        return nullptr;
    }
    auto [itr, inserted] = _songs.insert_or_assign(id, std::move(song));
    return &itr->second;
}

bool Store::Knows(ObjectGuid::LowType guid, Song const& song) const
{
    if (!song.owner)
        return true;
    if (song.owner == guid)
        return true;
    auto itr = _learned.find(guid);
    return itr != _learned.end() && itr->second.count(song.id);
}

void Store::Learn(ObjectGuid::LowType guid, uint32 songId)
{
    _learned[guid].insert(songId);
}

void Store::Unlearn(ObjectGuid::LowType guid, uint32 songId)
{
    auto itr = _learned.find(guid);
    if (itr != _learned.end())
        itr->second.erase(songId);
}

void Store::RemoveComposition(uint32 id)
{
    auto itr = _songs.find(id);
    if (itr != _songs.end() && itr->second.owner)
        itr->second.removed = true;
}

std::vector<Song const*> Store::GetSongs() const
{
    std::vector<Song const*> songs;
    for (auto const& [id, song] : _songs)
        songs.push_back(&song);
    std::sort(songs.begin(), songs.end(), [](Song const* a, Song const* b)
    {
        return a->tier != b->tier ? a->tier < b->tier : a->id < b->id;
    });
    return songs;
}

Instrument const* Store::GetInstrument(uint32 id) const
{
    auto itr = _instruments.find(id);
    return itr != _instruments.end() ? &itr->second : nullptr;
}

std::vector<Instrument const*> Store::GetInstruments() const
{
    std::vector<Instrument const*> instruments;
    for (auto const& [id, instrument] : _instruments)
        instruments.push_back(&instrument);
    std::sort(instruments.begin(), instruments.end(), [](Instrument const* a, Instrument const* b) { return a->id < b->id; });
    return instruments;
}

Instrument const* Store::GetDefaultInstrument() const
{
    // v0.1 has a single instrument, the lute; the lowest id wins.
    Instrument const* best = nullptr;
    for (auto const& [id, instrument] : _instruments)
        if (!best || id < best->id)
            best = &instrument;
    return best;
}

} // namespace Barde
