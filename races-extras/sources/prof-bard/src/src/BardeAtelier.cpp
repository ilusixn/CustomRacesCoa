/*
 * mod-barde - the workshop: instruments in five tiers, made from materials.
 * Licensed under the GNU AGPL v3, see LICENSE.
 *
 * Every instrument exists as one item per tier (copper, silver, mithril, thorium, arcanite). The copper one is
 * offered; the others take the bard skill of the tier and its materials (mod_barde_tier, mod_barde_tier_material),
 * taken from the bags. The tier of an instrument decides which songs it can play: a song of tier t wants an
 * instrument of tier t or better. A bard who carries several tiers of one instrument plays with the best one.
 */

#include "Barde.h"

#include "Item.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Random.h"

#include <algorithm>
#include <iterator>

namespace Barde::Atelier
{

namespace
{

std::string ItemName(uint32 entry)
{
    ItemTemplate const* item = sObjectMgr->GetItemTemplate(entry);
    return item ? item->Name1 : "objet " + std::to_string(entry);
}

// The name goes into a message whose fields and lists use these characters.
std::string Plain(std::string text)
{
    std::replace_if(text.begin(), text.end(), [](char c) { return c == '^' || c == ',' || c == ':'; }, ' ');
    return text;
}

} // namespace

uint32 OwnedMask(Player* player, Instrument const& instrument)
{
    uint32 mask = 0;
    for (InstrumentTier const& tier : Store::Instance().GetTiers())
    {
        uint32 const entry = Store::Instance().ItemFor(instrument.id, tier.tier);
        if (entry && player->HasItemCount(entry))
            mask |= 1u << tier.tier;
    }
    return mask;
}

int BestTier(Player* player, Instrument const& instrument)
{
    int best = -1;
    for (InstrumentTier const& tier : Store::Instance().GetTiers())
    {
        uint32 const entry = Store::Instance().ItemFor(instrument.id, tier.tier);
        if (entry && player->HasItemCount(entry))
            best = std::max(best, int(tier.tier));
    }
    if (best < 0 && instrument.itemEntry && player->HasItemCount(instrument.itemEntry))
        best = 0;           // the copper item, when the tier tables are missing
    return best;
}

uint32 BestItem(Player* player, Instrument const& instrument)
{
    int const best = BestTier(player, instrument);
    if (best < 0)
        return 0;
    uint32 const entry = Store::Instance().ItemFor(instrument.id, uint8(best));
    return entry ? entry : instrument.itemEntry;
}

uint8 QualityOf(Item const* item)
{
    if (!item)
        return 0;
    uint32 const id = item->GetEnchantmentId(PERM_ENCHANTMENT_SLOT);
    return id >= ENCHANT_FIRST && id < ENCHANT_FIRST + 4 ? uint8(id - ENCHANT_FIRST + 1) : 0;
}

namespace
{

constexpr uint32 KEY_BAR = 2840;        // Copper Bar
constexpr uint32 KEY_THREAD = 2320;     // Coarse Thread
constexpr uint32 KEYS_MADE = 5;

// The quality drawn at fabrication, in percent: common / uncommon / rare / epic; without a gem, with the gem, with the gem and the
// crystal. Skill helps: BONUS_PER skill above the tier's moves one point from common to uncommon, up to BONUS_MAX points.
constexpr uint32 QUALITY_CHANCES[3][4] = { { 60, 30, 10, 0 }, { 10, 50, 40, 0 }, { 0, 25, 50, 25 } };
constexpr uint32 BONUS_MAX = 10;
constexpr uint32 BONUS_PER = 15;

Item* BestItemOf(Player* player, Instrument const& instrument)
{
    uint32 const entry = BestItem(player, instrument);
    return entry ? player->GetItemByEntry(entry) : nullptr;
}

// Instruments made before tuning existed have no durability: they start fully tuned.
uint32 EnsureDurability(Item* item)
{
    uint32 max = item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);
    if (!max)
    {
        max = 100;
        item->SetUInt32Value(ITEM_FIELD_MAXDURABILITY, max);
        item->SetUInt32Value(ITEM_FIELD_DURABILITY, max);
    }
    return max;
}

} // namespace

float Tuning(Player* player, Instrument const& instrument)
{
    Item* item = BestItemOf(player, instrument);
    if (!item || QualityOf(item) == 4)
        return 1.0f;                    // the legendary is never out of tune
    uint32 const max = item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);
    return max ? std::clamp(float(item->GetUInt32Value(ITEM_FIELD_DURABILITY)) / float(max), 0.0f, 1.0f) : 1.0f;
}

float TuningMultiplier(float tuning)
{
    return tuning >= 0.5f ? 1.0f : 0.85f + 0.15f * (tuning / 0.5f);
}

float WearOut(Player* player, Instrument const& instrument, bool training)
{
    Item* item = BestItemOf(player, instrument);
    if (!item || QualityOf(item) == 4)
        return 1.0f;
    uint32 const max = EnsureDurability(item);
    if (!training)
    {
        uint32 const wear = instrument.NaturalPart() == PART_RHYTHM ? 3 : 4;
        uint32 const now = item->GetUInt32Value(ITEM_FIELD_DURABILITY);
        item->SetUInt32Value(ITEM_FIELD_DURABILITY, now > wear ? now - wear : 0);
        item->SetState(ITEM_CHANGED, player);
    }
    return std::clamp(float(item->GetUInt32Value(ITEM_FIELD_DURABILITY)) / float(max), 0.0f, 1.0f);
}

std::string Tune(Player* player, uint32 instrumentId)
{
    Instrument const* instrument = Store::Instance().GetInstrument(instrumentId);
    if (!instrument)
        return "Instrument inconnu.";
    Item* item = BestItemOf(player, *instrument);
    if (!item)
        return "Vous n'avez pas cet instrument.";
    if (QualityOf(item) == 4)
        return "Un instrument légendaire ne se désaccorde jamais.";
    uint32 const max = EnsureDurability(item);
    if (item->GetUInt32Value(ITEM_FIELD_DURABILITY) >= max)
        return "Cet instrument est déjà accordé.";
    if (!player->HasItemCount(TUNING_KEY))
        return "Il vous faut une clé d'accordage (à fabriquer dans l'onglet Accordage de l'atelier).";
    player->DestroyItemCount(TUNING_KEY, 1, true);
    item->SetUInt32Value(ITEM_FIELD_DURABILITY, max);
    item->SetState(ITEM_CHANGED, player);
    return {};
}

std::string CraftKeys(Player* player)
{
    Config const& config = GetConfig();
    if (!config.enable)
        return "Le métier de barde est désactivé sur ce serveur.";
    if (config.requireProfession && !player->HasSpell(PROFESSION_SPELL))
        return "Vous n'avez pas appris le métier de barde : ouvrez le Livre des artisans et apprenez « Apprenti barde ».";
    if (!player->HasItemCount(KEY_BAR) || !player->HasItemCount(KEY_THREAD))
        return "Il vous manque : 1 x " + ItemName(KEY_BAR) + " et 1 x " + ItemName(KEY_THREAD) + ".";

    ItemPosCountVec positions;
    InventoryResult const result = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, positions, TUNING_KEY, KEYS_MADE);
    if (result != EQUIP_ERR_OK)
    {
        player->SendEquipError(result, nullptr, nullptr, TUNING_KEY);
        return "Vos sacs sont pleins.";
    }
    player->DestroyItemCount(KEY_BAR, 1, true);
    player->DestroyItemCount(KEY_THREAD, 1, true);
    if (Item* keys = player->StoreNewItem(positions, TUNING_KEY, true))
        player->SendNewItem(keys, KEYS_MADE, true, true);
    return {};
}

void TuneStep(Player* player, uint32 percent)
{
    if (!percent)
        return;
    for (Instrument const* instrument : Store::Instance().GetInstruments())
    {
        Item* item = BestItemOf(player, *instrument);
        if (!item || QualityOf(item) == 4)
            continue;
        uint32 const max = EnsureDurability(item);
        uint32 const now = item->GetUInt32Value(ITEM_FIELD_DURABILITY);
        if (now >= max)
            continue;
        item->SetUInt32Value(ITEM_FIELD_DURABILITY, std::min(max, now + percent * max / 100));
        item->SetState(ITEM_CHANGED, player);
    }
}

std::string CraftCamp(Player* player)
{
    Config const& config = GetConfig();
    if (!config.enable)
        return "Le métier de barde est désactivé sur ce serveur.";
    if (config.requireProfession && !player->HasSpell(PROFESSION_SPELL))
        return "Vous n'avez pas appris le métier de barde : ouvrez le Livre des artisans et apprenez « Apprenti barde ».";

    static std::pair<uint32, uint32> const needed[] = { { 2592, 4 }, { 2319, 2 }, { 4471, 1 } };    // wool cloth, medium leather, flint and tinder
    std::string missing;
    for (auto const& [item, count] : needed)
        if (!player->HasItemCount(item, count))
            missing += (missing.empty() ? "" : ", ") + std::to_string(count) + " x " + ItemName(item);
    if (!missing.empty())
        return "Il vous manque : " + missing + ".";

    ItemPosCountVec positions;
    InventoryResult const result = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, positions, CAMP_KIT, 1);
    if (result != EQUIP_ERR_OK)
    {
        player->SendEquipError(result, nullptr, nullptr, CAMP_KIT);
        return "Vos sacs sont pleins.";
    }
    for (auto const& [item, count] : needed)
        player->DestroyItemCount(item, count, true);
    if (Item* kit = player->StoreNewItem(positions, CAMP_KIT, true))
        player->SendNewItem(kit, 1, true, false);
    return {};
}

float QualityMultiplier(Player* player, Instrument const& instrument)
{
    uint32 const entry = BestItem(player, instrument);
    return 1.0f + 0.05f * float(entry ? QualityOf(player->GetItemByEntry(entry)) : 0);
}

char const* QualityName(uint8 quality)
{
    switch (quality)
    {
        case 1:  return "inhabituel";
        case 2:  return "rare";
        case 3:  return "épique";
        case 4:  return "légendaire";
        default: return "commun";
    }
}

void SetQuality(Item* item, uint8 quality)
{
    if (!item || !quality || quality > 4)
        return;
    item->SetEnchantment(PERM_ENCHANTMENT_SLOT, ENCHANT_FIRST + quality - 1, 0, 0);
}

std::string Craft(Player* player, uint32 instrumentId, uint8 tierNumber, uint8 gemMode, std::string& qualityName)
{
    Config const& config = GetConfig();
    if (!config.enable)
        return "Le métier de barde est désactivé sur ce serveur.";
    if (config.requireProfession && !player->HasSpell(PROFESSION_SPELL))
        return "Vous n'avez pas appris le métier de barde : ouvrez le Livre des artisans et apprenez « Apprenti barde ».";

    Instrument const* instrument = Store::Instance().GetInstrument(instrumentId);
    InstrumentTier const* tier = Store::Instance().GetTier(tierNumber);
    uint32 const entry = Store::Instance().ItemFor(instrumentId, tierNumber);
    if (!instrument || !tier || !entry || !sObjectMgr->GetItemTemplate(entry))
        return "Cet instrument n'existe pas.";

    if (!player->IsAlive())
        return "Impossible de fabriquer en étant mort.";
    if (player->IsInCombat())
        return "Impossible de fabriquer en combat.";
    if (player->HasItemCount(entry))
        return "Vous avez déjà : " + ItemName(entry) + ".";

    uint32 const skill = Session::GetSkill(player);
    if (skill < tier->skillReq)
        return "Compétence de barde insuffisante : cet instrument demande " + std::to_string(tier->skillReq)
            + ", vous avez " + std::to_string(skill) + ".";

    // What is taken: the materials, then the gem and the crystal if asked for.
    std::vector<std::pair<uint32, uint32>> needed = tier->materials;
    if (gemMode >= 1)
    {
        if (!tier->gemEntry)
            return "Ce palier n'a pas de gemme de qualité.";
        needed.emplace_back(tier->gemEntry, 1);
    }
    if (gemMode >= 2)
    {
        if (tierNumber != 4 || !config.crystalEntry)
            return "Le cristal ne s'utilise que pour le palier arcanite.";
        needed.emplace_back(config.crystalEntry, 1);
    }
    std::string missing;
    for (auto const& [item, count] : needed)
        if (!player->HasItemCount(item, count))
            missing += (missing.empty() ? "" : ", ") + std::to_string(count) + " x " + ItemName(item);
    if (!missing.empty())
        return "Il vous manque : " + missing + ".";

    ItemPosCountVec positions;
    InventoryResult const result = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, positions, entry, 1);
    if (result != EQUIP_ERR_OK)
    {
        player->SendEquipError(result, nullptr, nullptr, entry);
        return "Vos sacs sont pleins.";
    }

    // The quality, drawn at random: common / uncommon / rare / epic, in percent, without a gem, with a gem, with the gem and
    // the crystal. Skill helps: up to 10 points move from common to uncommon (one per 15 skill above the tier's).
    uint32 chance[4];
    std::copy(std::begin(QUALITY_CHANCES[std::min<uint8>(gemMode, 2)]), std::end(QUALITY_CHANCES[std::min<uint8>(gemMode, 2)]), chance);
    uint32 const shift = std::min<uint32>(chance[0], std::min<uint32>(BONUS_MAX, (skill - tier->skillReq) / BONUS_PER));
    chance[0] -= shift;
    chance[1] += shift;
    uint32 const roll = urand(1, 100);
    uint8 quality = 0;
    for (uint32 sum = 0, i = 0; i < 4; ++i)
    {
        sum += chance[i];
        if (roll <= sum)
        {
            quality = uint8(i);
            break;
        }
    }

    for (auto const& [item, count] : needed)
        player->DestroyItemCount(item, count, true);
    if (Item* made = player->StoreNewItem(positions, entry, true))
    {
        SetQuality(made, quality);
        made->SetState(ITEM_CHANGED, player);
        player->SendNewItem(made, 1, true, false);
    }
    qualityName = QualityName(quality);
    return {};
}

void SendTiers(Player* player)
{
    // QUAL^none^gem^gem and crystal^bonusMax^bonusPer   (each: common,uncommon,rare,epic in percent)
    std::string chances = "QUAL";
    for (auto const& mode : QUALITY_CHANCES)
    {
        chances += '^';
        for (uint32 i = 0; i < 4; ++i)
            chances += (i ? "," : "") + std::to_string(mode[i]);
    }
    Protocol::Send(player, chances + '^' + std::to_string(BONUS_MAX) + '^' + std::to_string(BONUS_PER));

    for (InstrumentTier const& tier : Store::Instance().GetTiers())
    {
        std::string materials;
        for (auto const& [item, count] : tier.materials)
            materials += (materials.empty() ? "" : ",") + std::to_string(item) + ':' + std::to_string(count) + ':'
                + Plain(ItemName(item));
        std::string const gem = tier.gemEntry ? std::to_string(tier.gemEntry) + ':' + Plain(ItemName(tier.gemEntry)) : "";
        std::string const crystal = tier.tier == 4 && GetConfig().crystalEntry
            ? std::to_string(GetConfig().crystalEntry) + ':' + Plain(ItemName(GetConfig().crystalEntry)) : "";
        Protocol::Send(player, "TIER^" + std::to_string(tier.tier) + '^' + tier.suffix + '^'
            + std::to_string(tier.skillReq) + '^' + materials + '^' + gem + '^' + crystal);
    }
}

} // namespace Barde::Atelier
