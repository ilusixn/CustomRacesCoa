/*
 * mod-barde - compositions: a bard records an improvisation and keeps it as a song of his own.
 * Licensed under the GNU AGPL v3, see LICENSE.
 *
 * The addon records the notes the bard strikes while he improvises (the time of each, in milliseconds, and the pitch) and sends them to the
 * server in pieces. The server decides everything that matters: it puts the notes on a grid of sixteenth notes at 84 BPM, refuses a
 * recording that is too short (Barde.Compose.MinNotes notes and MinSeconds seconds), too long or out of range, works out the tier of the
 * song from its density (notes per second) and never gives a tier above what the bard's skill allows. A composition is a song like the
 * others, played in the same mini-game and checked in the same way, but it gives no chant and no skill, does not enter the ranking and belongs
 * to its composer. It is for pleasure, for the score and for concerts.
 */

#include "Barde.h"

#include "DatabaseEnv.h"
#include "Item.h"
#include "Player.h"
#include "StringConvert.h"
#include "Tokenize.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <unordered_map>

namespace Barde::Compose
{

namespace
{

constexpr double BPM = 84.0;
constexpr double STEP_MS = 15000.0 / BPM;            // a sixteenth note
constexpr uint32 MAX_NOTES = 400;
constexpr uint32 MIN_PITCH = 55, MAX_PITCH = 84;     // the range of the instruments' sounds
constexpr uint32 MIN_GAP_MS = 60;                    // two notes closer than this are one

struct Pending
{
    std::string title;
    std::vector<std::pair<uint32, uint8>> notes;     // milliseconds from the start of the recording, pitch
    uint32 pieces = 0;
    uint32 startMs = 0;
};

std::mutex sLock;
std::unordered_map<ObjectGuid::LowType, Pending> sPending;

// The tier a bard's skill allows: the same thresholds as the book's songs.
uint8 SkillTier(uint32 skill)
{
    return skill >= 240 ? 4 : skill >= 180 ? 3 : skill >= 120 ? 2 : skill >= 60 ? 1 : 0;
}

// The tier of a recording from its density, in notes per second.
uint8 DensityTier(double perSecond)
{
    return perSecond >= 3.0 ? 4 : perSecond >= 2.4 ? 3 : perSecond >= 1.8 ? 2 : perSecond >= 1.2 ? 1 : 0;
}

// A title the bard may keep: no control character or separator, 3 to 40 characters.
std::string CleanTitle(std::string_view raw)
{
    std::string title;
    for (char c : raw)
        if (uint8(c) >= 32 && c != Protocol::SEP && c != '|' && c != '\\')
            title += c;
    while (!title.empty() && title.front() == ' ')
        title.erase(title.begin());
    while (!title.empty() && title.back() == ' ')
        title.pop_back();
    if (title.size() > 40)
        title.resize(40);
    return title;
}

} // namespace

uint32 Count(Player* player)
{
    uint32 count = 0;
    for (Song const* song : Store::Instance().GetSongsFor(player->GetGUID().GetCounter()))
        if (song->owner == player->GetGUID().GetCounter())
            ++count;
    return count;
}

void Begin(Player* player, std::string_view title)
{
    std::lock_guard<std::mutex> guard(sLock);
    Pending pending;
    pending.title = CleanTitle(title);
    sPending[player->GetGUID().GetCounter()] = std::move(pending);
}

void Add(Player* player, std::string_view notes)
{
    std::lock_guard<std::mutex> guard(sLock);
    auto itr = sPending.find(player->GetGUID().GetCounter());
    if (itr == sPending.end())
        return;
    Pending& pending = itr->second;
    if (++pending.pieces > 60)                       // 60 pieces of 20 notes: more than a recording can hold
        return;
    for (std::string_view token : Acore::Tokenize(notes, ';', false))
    {
        std::vector<std::string_view> parts = Acore::Tokenize(token, ':', false);
        if (parts.size() != 2)
            continue;
        Optional<uint32> ms = Acore::StringTo<uint32>(parts[0]);
        Optional<uint32> midi = Acore::StringTo<uint32>(parts[1]);
        if (!ms || !midi || *midi < MIN_PITCH || *midi > MAX_PITCH || pending.notes.size() >= MAX_NOTES + 100)
            continue;
        pending.notes.emplace_back(*ms, uint8(*midi));
    }
}

void Abort(Player* player)
{
    std::lock_guard<std::mutex> guard(sLock);
    sPending.erase(player->GetGUID().GetCounter());
}

std::string End(Player* player)
{
    Config const& config = GetConfig();
    Pending pending;
    {
        std::lock_guard<std::mutex> guard(sLock);
        auto itr = sPending.find(player->GetGUID().GetCounter());
        if (itr == sPending.end())
            return "Aucun enregistrement à valider.";
        pending = std::move(itr->second);
        sPending.erase(itr);
    }

    if (config.requireProfession && !player->HasSpell(PROFESSION_SPELL))
        return "Vous n'avez pas appris le métier de barde.";
    if (pending.title.size() < 3)
        return "Donnez un titre d'au moins 3 caractères à votre composition.";
    if (Count(player) >= config.composeMax)
        return "Vous avez déjà " + std::to_string(config.composeMax) + " compositions : supprimez-en une pour en faire une autre.";

    // Order the notes in time and join the ones that are too close; the first note is at step 0.
    std::sort(pending.notes.begin(), pending.notes.end());
    std::vector<std::pair<uint32, uint8>> notes;
    for (auto const& note : pending.notes)
        if (notes.empty() || note.first - notes.back().first >= MIN_GAP_MS)
            notes.push_back(note);
    if (notes.size() < config.composeMinNotes)
        return "Il faut au moins " + std::to_string(config.composeMinNotes) + " notes (vous en avez " + std::to_string(notes.size()) + ").";
    if (notes.size() > MAX_NOTES)
        return "Trop de notes : " + std::to_string(MAX_NOTES) + " au plus.";

    // Quantise on the grid: one note per step at most.
    uint32 const origin = notes.front().first;
    std::vector<std::pair<uint32, uint8>> placed;                 // step, pitch
    for (auto const& [ms, midi] : notes)
    {
        uint32 const step = uint32(std::lround(double(ms - origin) / STEP_MS));
        if (placed.empty() || placed.back().first != step)
            placed.emplace_back(step, midi);
    }
    if (placed.size() < config.composeMinNotes)
        return "Il faut au moins " + std::to_string(config.composeMinNotes) + " notes distinctes sur la grille de croches.";

    double const seconds = double(placed.back().first) * STEP_MS / 1000.0;
    if (seconds < double(config.composeMinSeconds))
        return "La composition est trop courte : " + std::to_string(config.composeMinSeconds) + " secondes au moins (elle fait "
            + std::to_string(uint32(seconds)) + " s).";
    if (seconds > double(config.composeMaxSeconds))
        return "La composition est trop longue : " + std::to_string(config.composeMaxSeconds) + " secondes au plus.";

    uint8 const tier = std::min(DensityTier(double(placed.size()) / seconds), SkillTier(Session::GetSkill(player)));

    std::string text;
    for (auto const& [step, midi] : placed)
        text += (text.empty() ? "" : " ") + std::to_string(step) + ':' + std::to_string(midi);

    std::string safeTitle = pending.title;
    CharacterDatabase.EscapeString(safeTitle);
    CharacterDatabase.DirectExecute("INSERT INTO mod_barde_composition (guid, title, tier, notes) VALUES ({}, '{}', {}, '{}')",
        player->GetGUID().GetCounter(), safeTitle, uint32(tier), text);
    uint32 id = 0;
    if (QueryResult result = CharacterDatabase.Query("SELECT MAX(id) FROM mod_barde_composition WHERE guid = {}", player->GetGUID().GetCounter()))
        id = result->Fetch()[0].Get<uint32>();
    if (!id)
        return "La composition n'a pas pu être enregistrée.";

    std::string error;
    if (!Store::Instance().AddComposition(Store::COMPOSITION_FIRST_ID + id, player->GetGUID().GetCounter(), pending.title,
            player->GetName(), tier, text, error))
    {
        CharacterDatabase.DirectExecute("DELETE FROM mod_barde_composition WHERE id = {}", id);
        return "La composition est invalide : " + error;
    }
    Titles::Check(player);
    return {};
}

std::string Delete(Player* player, uint32 songId)
{
    Song const* song = Store::Instance().GetSong(songId);
    if (!song || !song->owner || songId < Store::COMPOSITION_FIRST_ID)
        return "Cette composition est inconnue.";
    ObjectGuid::LowType const guid = player->GetGUID().GetCounter();

    if (song->owner != guid)
    {
        // A song learned from a scroll: the player only forgets it.
        if (!Store::Instance().Knows(guid, *song))
            return "Vous ne connaissez pas cet air.";
        CharacterDatabase.DirectExecute("DELETE FROM mod_barde_char_learned WHERE guid = {} AND song_id = {}", guid, songId);
        CharacterDatabase.DirectExecute("DELETE FROM mod_barde_char_song WHERE guid = {} AND song_id = {}", guid, songId);
        Store::Instance().Unlearn(guid, songId);
        return {};
    }

    // His own composition: gone for him, and for those who learned it.
    CharacterDatabase.DirectExecute("DELETE FROM mod_barde_composition WHERE id = {} AND guid = {}", songId - Store::COMPOSITION_FIRST_ID, guid);
    CharacterDatabase.DirectExecute("DELETE FROM mod_barde_char_song WHERE song_id = {}", songId);
    CharacterDatabase.DirectExecute("DELETE FROM mod_barde_char_learned WHERE song_id = {}", songId);
    Store::Instance().RemoveComposition(songId);
    return {};
}

std::string Copy(Player* player, uint32 songId)
{
    Config const& config = GetConfig();
    Song const* song = Store::Instance().GetSong(songId);
    if (!song || song->removed || !song->owner || song->owner != player->GetGUID().GetCounter() || songId < Store::COMPOSITION_FIRST_ID)
        return "Vous ne pouvez copier que vos propres compositions.";
    if (config.requireProfession && !player->HasSpell(PROFESSION_SPELL))
        return "Vous n'avez pas appris le métier de barde.";

    constexpr uint32 PARCHMENT = 39354, INK = 39469;          // Light Parchment, Moonglow Ink
    if (!player->HasItemCount(PARCHMENT) || !player->HasItemCount(INK))
        return "Il vous faut 1 Parchemin léger (Light Parchment) et 1 Encre de lune (Moonglow Ink).";

    ItemPosCountVec positions;
    InventoryResult const result = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, positions, SCROLL_ITEM, 1);
    if (result != EQUIP_ERR_OK)
    {
        player->SendEquipError(result, nullptr, nullptr, SCROLL_ITEM);
        return "Vos sacs sont pleins.";
    }
    player->DestroyItemCount(PARCHMENT, 1, true);
    player->DestroyItemCount(INK, 1, true);
    if (Item* scroll = player->StoreNewItem(positions, SCROLL_ITEM, true))
    {
        scroll->SetEnchantment(PERM_ENCHANTMENT_SLOT, songId, 0, 0);       // which song is on the scroll
        scroll->SetState(ITEM_CHANGED, player);
        player->SendNewItem(scroll, 1, true, false);
    }
    return {};
}

std::string Read(Player* player, Item* scroll)
{
    uint32 const songId = scroll ? scroll->GetEnchantmentId(PERM_ENCHANTMENT_SLOT) : 0;
    Song const* song = Store::Instance().GetSong(songId);
    if (!song || song->removed || !song->owner || songId < Store::COMPOSITION_FIRST_ID)
        return "Ce parchemin est vide ou son air n'existe plus.";
    Config const& config = GetConfig();
    if (config.requireProfession && !player->HasSpell(PROFESSION_SPELL))
        return "Il faut connaître le métier de barde pour lire ce parchemin.";
    ObjectGuid::LowType const guid = player->GetGUID().GetCounter();
    if (Store::Instance().Knows(guid, *song))
        return "Vous connaissez déjà « " + song->name + " ».";

    CharacterDatabase.DirectExecute("REPLACE INTO mod_barde_char_learned (guid, song_id) VALUES ({}, {})", guid, songId);
    Store::Instance().Learn(guid, songId);
    return {};
}

void SendScrollInfo(Player* player, uint32 songId)
{
    Song const* song = Store::Instance().GetSong(songId);
    if (!song || song->removed || !song->owner || songId < Store::COMPOSITION_FIRST_ID)
        return;
    // SCROLLINFO^songId^title^tier^composer
    std::string composer = song->subtitle;
    std::string const prefix = "composition de ";
    if (composer.compare(0, prefix.size(), prefix) == 0)
        composer = composer.substr(prefix.size());
    Protocol::Send(player, "SCROLLINFO^" + std::to_string(songId) + '^' + song->name + '^' + std::to_string(song->tier) + '^' + composer);
}

} // namespace Barde::Compose
