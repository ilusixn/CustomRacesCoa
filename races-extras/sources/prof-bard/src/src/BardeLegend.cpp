/*
 * mod-barde - the quest of a thousand voices: the legendary instrument.
 * Licensed under the GNU AGPL v3, see LICENSE.
 *
 * One legendary instrument per character, for ever, bound to the soul: it cannot be traded, sold or bought, there is no other way to
 * have one. The bard chooses which instrument becomes legendary. Five things, all hard:
 *   1. every song of the book played at gold mastery (40 successes) AND once at 100 %;
 *   2. 10 arcanite bars, 5 essences of water and 1 living ember, handed in;
 *   3. a duel won against a virtuoso (the title, or the gold mastery of three songs that earns it);
 *   4. a song of the Grand bard level played in a trio of real bards, each of them at 100 %;
 *   5. the forge: the epic arcanite instrument he chooses is consumed and returns legendary.
 */

#include "Barde.h"

#include "Chat.h"
#include "DatabaseEnv.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"

namespace Barde::Legend
{

namespace
{

constexpr uint32 ITEM_ARCANITE_BAR = 12360;
constexpr uint32 ITEM_ESSENCE_OF_WATER = 7080;
constexpr uint32 ITEM_LIVING_EMBER = 69237;
constexpr uint32 BARS = 10, ESSENCES = 5, EMBERS = 1;
constexpr uint32 GOLD_WINS = 40;
constexpr uint32 PERFECT = 1000;                // permille

void Say(Player* player, std::string const& text)
{
    if (player->GetSession())
        ChatHandler(player->GetSession()).SendSysMessage(text);
}

struct State
{
    bool delivered = false, duel = false, trio = false, forged = false;
};

State Load(ObjectGuid::LowType guid)
{
    State state;
    if (QueryResult result = CharacterDatabase.Query("SELECT delivered, duel, trio, forged FROM mod_barde_legend WHERE guid = {}", guid))
    {
        Field* fields = result->Fetch();
        state.delivered = fields[0].Get<uint8>(), state.duel = fields[1].Get<uint8>();
        state.trio = fields[2].Get<uint8>(), state.forged = fields[3].Get<uint8>();
    }
    return state;
}

void Mark(ObjectGuid::LowType guid, char const* column)
{
    CharacterDatabase.Execute("INSERT INTO mod_barde_legend (guid, {0}) VALUES ({1}, 1) ON DUPLICATE KEY UPDATE {0} = 1", column, guid);
}

// How many songs of the book are at gold mastery, and how many have been played at 100 %.
void CountSongs(ObjectGuid::LowType guid, uint32& gold, uint32& perfect)
{
    gold = perfect = 0;
    if (QueryResult result = CharacterDatabase.Query(
            "SELECT SUM(wins >= {}), SUM(best_accuracy >= {}) FROM mod_barde_char_song WHERE guid = {} AND song_id < {}",
            GOLD_WINS, PERFECT, guid, Store::COMPOSITION_FIRST_ID))
    {
        Field* fields = result->Fetch();
        gold = fields[0].IsNull() ? 0 : fields[0].Get<uint32>();
        perfect = fields[1].IsNull() ? 0 : fields[1].Get<uint32>();
    }
}

uint32 BookSize()
{
    uint32 count = 0;
    for (Song const* song : Store::Instance().GetSongs())
        if (!song->owner)
            ++count;
    return count;
}

// Every item of the bags (and the backpack) with this entry.
std::vector<Item*> ItemsOf(Player* player, uint32 entry)
{
    std::vector<Item*> items;
    auto check = [&](Item* item) { if (item && item->GetEntry() == entry) items.push_back(item); };
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        check(player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
        if (Bag* container = player->GetBagByPos(bag))
            for (uint32 slot = 0; slot < container->GetBagSize(); ++slot)
                check(container->GetItemByPos(slot));
    return items;
}

} // namespace

bool IsVirtuoso(ObjectGuid::LowType guid)
{
    uint32 gold, perfect;
    CountSongs(guid, gold, perfect);
    return gold >= 3;
}

void OnDuelWon(ObjectGuid::LowType winner, ObjectGuid::LowType loser)
{
    if (Load(winner).forged || !IsVirtuoso(loser))
        return;
    Mark(winner, "duel");
    if (Player* player = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(winner)))
        Say(player, "Quête des Mille Voix : vous avez battu un virtuose.");
}

void OnTrio(std::vector<Player*> const& bards, Song const& song, std::vector<float> const& accuracies)
{
    if (bards.size() < 3 || song.owner || song.tier != TIER_GRAND || accuracies.size() != bards.size())
        return;
    for (float accuracy : accuracies)
        if (accuracy < 0.9995f)
            return;
    for (Player* bard : bards)
        if (!Load(bard->GetGUID().GetCounter()).forged)
        {
            Mark(bard->GetGUID().GetCounter(), "trio");
            Say(bard, "Quête des Mille Voix : un trio parfait, sans une fausse note.");
        }
}

std::string GmTest(Player* player, std::string const& what)
{
    ObjectGuid::LowType const guid = player->GetGUID().GetCounter();
    if (what == "reset")
    {
        CharacterDatabase.Execute("DELETE FROM mod_barde_legend WHERE guid = {}", guid);
        return "Quête réinitialisée (les airs ne sont pas touchés ; un instrument déjà forgé reste légendaire).";
    }
    if (what != "tout")
        return "Utilisation : .barde legende tout | reset";
    for (Song const* song : Store::Instance().GetSongs())
        if (!song->owner)
            CharacterDatabase.Execute(
                "INSERT INTO mod_barde_char_song (guid, song_id, plays, wins, best_score, best_accuracy) VALUES ({0}, {1}, {2}, {2}, 1, 1000) "
                "ON DUPLICATE KEY UPDATE wins = GREATEST(wins, {2}), best_accuracy = 1000", guid, song->id, GOLD_WINS);
    Mark(guid, "duel");
    Mark(guid, "trio");
    return {};
}

// LEGEND^forged^delivered^duel^trio^gold^perfect^total^bars^essences^embers^epic (epic: bit of the instruments with an epic arcanite one)
void Send(Player* player)
{
    State const state = Load(player->GetGUID().GetCounter());
    uint32 gold, perfect;
    CountSongs(player->GetGUID().GetCounter(), gold, perfect);

    uint32 epic = 0;
    for (Instrument const* instrument : Store::Instance().GetInstruments())
        if (uint32 const entry = Store::Instance().ItemFor(instrument->id, 4))
            for (Item* item : ItemsOf(player, entry))
                if (Atelier::QualityOf(item) == 3)
                    epic |= 1u << instrument->id;

    Protocol::Send(player, "LEGEND^" + std::to_string(state.forged) + '^' + std::to_string(state.delivered) + '^' + std::to_string(state.duel)
        + '^' + std::to_string(state.trio) + '^' + std::to_string(gold) + '^' + std::to_string(perfect) + '^' + std::to_string(BookSize())
        + '^' + std::to_string(std::min(player->GetItemCount(ITEM_ARCANITE_BAR), BARS)) + '^' + std::to_string(std::min(player->GetItemCount(ITEM_ESSENCE_OF_WATER), ESSENCES))
        + '^' + std::to_string(std::min(player->GetItemCount(ITEM_LIVING_EMBER), EMBERS)) + '^' + std::to_string(epic));
}

std::string Deliver(Player* player)
{
    State const state = Load(player->GetGUID().GetCounter());
    if (state.forged)
        return "Votre instrument légendaire est déjà forgé.";
    if (state.delivered)
        return "Vous avez déjà remis les matériaux.";
    if (player->GetItemCount(ITEM_ARCANITE_BAR) < BARS || player->GetItemCount(ITEM_ESSENCE_OF_WATER) < ESSENCES || player->GetItemCount(ITEM_LIVING_EMBER) < EMBERS)
        return "Il vous faut 10 barres d'arcanite, 5 essences d'eau et 1 braise vivante.";
    player->DestroyItemCount(ITEM_ARCANITE_BAR, BARS, true);
    player->DestroyItemCount(ITEM_ESSENCE_OF_WATER, ESSENCES, true);
    player->DestroyItemCount(ITEM_LIVING_EMBER, EMBERS, true);
    Mark(player->GetGUID().GetCounter(), "delivered");
    return {};
}

std::string Forge(Player* player, uint32 instrumentId)
{
    ObjectGuid::LowType const guid = player->GetGUID().GetCounter();
    State const state = Load(guid);
    if (state.forged)
        return "Vous avez déjà forgé votre instrument légendaire : on n'en forge qu'un par personnage.";

    uint32 gold, perfect;
    CountSongs(guid, gold, perfect);
    uint32 const total = BookSize();
    if (gold < total || perfect < total)
        return "Il faut la maîtrise or et un passage à 100 % sur les " + std::to_string(total) + " airs du carnet.";
    if (!state.delivered)
        return "Remettez d'abord les matériaux.";
    if (!state.duel)
        return "Battez d'abord un virtuose en duel.";
    if (!state.trio)
        return "Jouez d'abord un air de Grand barde en trio parfait.";

    Instrument const* instrument = Store::Instance().GetInstrument(instrumentId);
    uint32 const entry = instrument ? Store::Instance().ItemFor(instrument->id, 4) : 0;
    if (!entry)
        return "Instrument inconnu.";
    Item* chosen = nullptr;
    for (Item* item : ItemsOf(player, entry))
        if (Atelier::QualityOf(item) == 3)
            chosen = item;
    if (!chosen)
        return "Il vous faut un instrument d'arcanite de qualité épique de ce type dans vos sacs.";

    Atelier::SetQuality(chosen, 4);
    chosen->SetBinding(true);                                   // bound to the soul: no trade, no sale, no mail
    chosen->SetState(ITEM_CHANGED, player);
    Mark(guid, "forged");
    Titles::GiveLegend(player);
    return {};
}

} // namespace Barde::Legend
