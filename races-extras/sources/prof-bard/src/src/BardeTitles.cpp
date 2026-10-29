/*
 * mod-barde - the bard's statistics and titles.
 * Licensed under the GNU AGPL v3, see LICENSE.
 *
 * The module counts what a bard does (ovations received, tips earned, duels won) and gives him titles when he reaches them. The titles are
 * rows of CharTitles.dbc that the game left free ("ReuseNN"); tools/barde_dbc.py gives them their names in the server's and the client's file.
 */

#include "Barde.h"

#include "Chat.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Player.h"
#include "WorldSession.h"

namespace Barde::Titles
{

namespace
{

// The title ids (CharTitles.dbc) and what earns them.
constexpr uint32 TITLE_MINSTREL = 226;      // 10 different songs of the book won
constexpr uint32 TITLE_COMPOSER = 227;      // one composition
constexpr uint32 TITLE_GOLD_VOICE = 231;    // 5 gold of tips received
constexpr uint32 TITLE_TAVERN_KING = 232;   // 10 duels won
constexpr uint32 TITLE_VIRTUOSO = 233;      // gold mastery on 3 songs
constexpr uint32 TITLE_IDOL = 234;          // 100 ovations
constexpr uint32 TITLE_LEGEND = 235;        // the legendary instrument (see the quest)

uint64 Value(ObjectGuid::LowType guid, char const* column)
{
    if (QueryResult result = CharacterDatabase.Query("SELECT {} FROM mod_barde_char_stats WHERE guid = {}", column, guid))
        return result->Fetch()[0].Get<uint64>();
    return 0;
}

uint64 Count(std::string const& where)
{
    if (QueryResult result = CharacterDatabase.Query("SELECT COUNT(*) FROM mod_barde_char_song WHERE {}", where))
        return result->Fetch()[0].Get<uint64>();
    return 0;
}

void Give(Player* player, uint32 titleId, char const* why)
{
    CharTitlesEntry const* title = sCharTitlesStore.LookupEntry(titleId);
    if (!title || player->HasTitle(title))
        return;
    player->SetTitle(title);
    if (player->GetSession())
        ChatHandler(player->GetSession()).PSendSysMessage("Nouveau titre de barde : {} (voir la fenêtre du personnage).", why);
}

} // namespace

void Add(ObjectGuid::LowType guid, char const* column, uint64 amount)
{
    CharacterDatabase.Execute("INSERT INTO mod_barde_char_stats (guid, {0}) VALUES ({1}, {2}) ON DUPLICATE KEY UPDATE {0} = {0} + {2}",
        column, guid, amount);
}

uint64 Get(ObjectGuid::LowType guid, char const* column)
{
    return Value(guid, column);
}

void Check(Player* player)
{
    if (!player || !GetConfig().enable)
        return;
    ObjectGuid::LowType const guid = player->GetGUID().GetCounter();
    std::string const mine = "guid = " + std::to_string(guid) + " AND song_id < " + std::to_string(Store::COMPOSITION_FIRST_ID);

    if (Count(mine + " AND wins > 0") >= 10)
        Give(player, TITLE_MINSTREL, "le Ménestrel");
    if (Compose::Count(player) >= 1)
        Give(player, TITLE_COMPOSER, "le Compositeur");
    if (Value(guid, "tips") >= 50000)
        Give(player, TITLE_GOLD_VOICE, "Voix d'or");
    if (Value(guid, "duels_won") >= 10)
        Give(player, TITLE_TAVERN_KING, "Roi de la taverne");
    if (Count(mine + " AND wins >= 40") >= 3)
        Give(player, TITLE_VIRTUOSO, "le Virtuose");
    if (Value(guid, "ovations") >= 100)
        Give(player, TITLE_IDOL, "l'Idole");
}

void GiveLegend(Player* player)
{
    Give(player, TITLE_LEGEND, "Barde légendaire");
}

} // namespace Barde::Titles
