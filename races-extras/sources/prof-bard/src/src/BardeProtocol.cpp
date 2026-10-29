/*
 * mod-barde - the addon message channel.
 * Licensed under the GNU AGPL v3, see LICENSE.
 *
 * Both directions use addon whispers with the prefix BARDE, the player
 * whispering to himself. The chat hook in BardeScripts.cpp swallows them
 * before they reach anyone. Fields are separated by '^':
 *
 *   addon -> server
 *     LIST                                      list the songs
 *     PLAY^songId^training^instrumentId         start a song (training 0/1)
 *     RES^token^perfect^good^miss^holdsKept^maxCombo^score
 *                                               (a held note counts twice: struck and released)
 *     ABORT^token
 *     ENS^songId^instrumentId                   propose the song to the party (play together)
 *     ENSJOIN^ensembleId^instrumentId           accept an invitation (instrumentId 0: the server picks)
 *     ENSNO^ensembleId                          decline an invitation
 *     CRAFT^instrumentId^tier^gem               make an instrument at the workshop (gem 0 none, 1 gem, 2 gem and crystal)
 *     IMPROV^instrumentId^voice                 start improvising (voice: "piano" or empty for the instrument's own sound)
 *     IMPROVOLD^instrumentId                       start improvising (instrument in hand, notes heard by those near)
 *     COMPBEGIN^title / COMPN^ms:midi;ms:midi... / COMPEND   send a recorded improvisation to be kept as a composition
 *     COPY^songId                               copy one of one's compositions onto a scroll
 *     SCROLL^songId                             ask what a scroll holds
 *     DELCOMP^songId                            delete one of one's compositions
 *     IMPROVEND                                 stop improvising
 *     DUEL^songId^instrumentId                  challenge the selected bard to this song
 *     DUELJOIN^duelId^instrumentId / DUELNO^duelId   answer a challenge
 *     TOP^songId                                ask the ranking of a song
 *     SPEC / SPECSET^n                          ask / choose the specialization (1 war, 2 court, 3 storyteller)
 *     LEGEND / LEGENDGIVE / LEGENDFORGE^instrumentId   the quest of the legendary instrument: state, hand in the materials, forge
 *     TUNE^instrumentId                         tune an instrument with a key
 *     KEYS                                      make tuning keys at the workshop
 *     CAMP                                      make a camp kit at the workshop
 *     HITS^midi,midi,...                        notes struck, to be heard by the ensemble and the players near
 *
 *   server -> addon
 *     AIR^id^tier^bpm^notes^holds^skill^best^bestAcc^wins^power‰^subtitle^name^kind^factor‰   one per song
 *     INS^id^name^sound^buff^effect^unit^min*100^max*100^buffSpell^has^bestTier^ownedMask   one per instrument
 *     TIER^tier^suffix^skillReq^item:count:name,...   one per tier of instruments, with its materials
 *     IMPROV_OK                                 improvisation accepted
 *     DUELINV^duelId^challenger^songId^songName^seconds   a challenge
 *     SCROLLINFO^songId^title^tier^composer      what a scroll holds
 *     TOP^songId^name,score,accuracy‰;...        the five best scores of a song
 *     SPEC^current^minSkill^changeCost          the specialization (0 none) and what it asks
 *     LEGEND^forged^delivered^duel^trio^gold^perfect^total^bars^essences^embers^epicMask   the quest
 *     ENSTATE^ensembleId^phase^songName^bards^averageAccuracy‰^chantGiven^name|state|instrumentId|part|accuracy‰;...   the state of the ensemble (phase 0 waiting, 1 playing, 2 over, 3 cancelled)
 *     DONE^text                                 tuned or keys made (the list is asked again)
 *     CRAFTED^quality                           an instrument was made, of this quality (the list is asked again)
 *     AIRS_END^count^skill^skillMax^gm
 *     BEGIN^token^songId^bpm^tempoPermille^noteCount^chunks^training^tier^name^groupSize^part^instrumentId
 *     N^token^index^step,lane,midi,len;...      the notes, in chunks (len 0 for a tap)
 *     OK^token^acc^amount^seconds^score^best^newBest^malus^base*100^crescendo^capped^text^skillGained^skill
 *        ^groupSize^groupAcc‰^groupMultiplier‰
 *     INVITE^ensembleId^leaderName^songId^songName^seconds    an invitation to play together
 *     HEAR^instrumentId^midi,midi,...^kind^voice  notes a bard struck: kind 1 a partner of the ensemble, 0 a bard near you
 *     KO^token^text                             result refused or song not started
 *     OPEN                                      open the songbook (the Barde spell was cast)
 *     MSG^text
 */

#include "Barde.h"

#include "Chat.h"
#include "Player.h"
#include "StringConvert.h"
#include "Tokenize.h"
#include "WorldPacket.h"

namespace Barde::Protocol
{

void Send(Player* player, std::string const& body)
{
    std::string text = std::string(PREFIX) + '\t' + body;
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, text);
    player->SendDirectMessage(&data);
}

std::vector<std::string_view> Split(std::string_view body)
{
    return Acore::Tokenize(body, SEP, true);
}

void Handle(Player* player, std::string_view body)
{
    std::vector<std::string_view> fields = Split(body);
    if (fields.empty())
        return;

    std::string_view const command = fields[0];

    if (!GetConfig().enable)
    {
        Send(player, "MSG^Le métier de barde est désactivé sur ce serveur.");
        return;
    }

    if (command == "LIST")
    {
        Session::ReleaseHands(player);      // safety net: opening the book always puts the instrument away
        Session::SendSongList(player);
    }
    else if (command == "PLAY" && fields.size() >= 3)
    {
        Optional<uint32> songId = Acore::StringTo<uint32>(fields[1]);
        bool training = fields[2] == "1";
        Optional<uint32> instrumentId = fields.size() >= 4 ? Acore::StringTo<uint32>(fields[3]) : Optional<uint32>(1);
        std::string error = !songId ? "Air inconnu."
            : !instrumentId ? "Instrument inconnu."
            : Session::Begin(player, *songId, training, *instrumentId);
        if (!error.empty())
            Send(player, "KO^0^" + error);
    }
    else if (command == "ENS" && fields.size() >= 3)
    {
        Optional<uint32> songId = Acore::StringTo<uint32>(fields[1]);
        Optional<uint32> instrumentId = Acore::StringTo<uint32>(fields[2]);
        std::string error = (!songId || !instrumentId) ? "Air ou instrument inconnu."
            : Ensemble::Propose(player, *songId, *instrumentId);
        if (!error.empty())
            Send(player, "KO^0^" + error);
    }
    else if ((command == "ENSJOIN" || command == "ENSNO") && fields.size() >= 2)
    {
        Optional<uint32> ensembleId = Acore::StringTo<uint32>(fields[1]);
        Optional<uint32> instrumentId = fields.size() >= 3 ? Acore::StringTo<uint32>(fields[2]) : Optional<uint32>(0);
        if (ensembleId && instrumentId)
            Ensemble::Answer(player, *ensembleId, command == "ENSJOIN", *instrumentId);
    }
    else if (command == "CRAFT" && fields.size() >= 3)
    {
        Optional<uint32> instrumentId = Acore::StringTo<uint32>(fields[1]);
        Optional<uint32> tier = Acore::StringTo<uint32>(fields[2]);
        Optional<uint32> gem = fields.size() >= 4 ? Acore::StringTo<uint32>(fields[3]) : Optional<uint32>(0);
        std::string quality;
        std::string error = (!instrumentId || !tier || *tier > 255 || !gem || *gem > 2) ? "Instrument inconnu."
            : Atelier::Craft(player, *instrumentId, uint8(*tier), uint8(*gem), quality);
        if (!error.empty())
            Send(player, "KO^0^" + error);
        else
            Send(player, "CRAFTED^" + quality);
    }
    else if (command == "IMPROV" && fields.size() >= 2)
    {
        Optional<uint32> instrumentId = Acore::StringTo<uint32>(fields[1]);
        std::string error = instrumentId ? Session::BeginImprov(player, *instrumentId, fields.size() >= 3 ? fields[2] : std::string_view()) : "Instrument inconnu.";
        Send(player, error.empty() ? "IMPROV_OK" : "KO^0^" + error);
    }
    else if (command == "COMPBEGIN" && fields.size() >= 2)
    {
        Compose::Begin(player, fields[1]);
    }
    else if (command == "COMPN" && fields.size() >= 2)
    {
        Compose::Add(player, fields[1]);
    }
    else if (command == "COMPEND")
    {
        std::string error = Compose::End(player);
        Send(player, error.empty() ? "DONE^Composition enregistrée : elle est dans votre carnet." : "KO^0^" + error);
    }
    else if (command == "COPY" && fields.size() >= 2)
    {
        Optional<uint32> songId = Acore::StringTo<uint32>(fields[1]);
        std::string error = songId ? Compose::Copy(player, *songId) : "Composition inconnue.";
        Send(player, error.empty() ? "DONE^Parchemin d'air copié." : "KO^0^" + error);
    }
    else if (command == "SCROLL" && fields.size() >= 2)
    {
        if (Optional<uint32> songId = Acore::StringTo<uint32>(fields[1]))
            Compose::SendScrollInfo(player, *songId);
    }
    else if (command == "DELCOMP" && fields.size() >= 2)
    {
        Optional<uint32> songId = Acore::StringTo<uint32>(fields[1]);
        std::string error = songId ? Compose::Delete(player, *songId) : "Composition inconnue.";
        Send(player, error.empty() ? "DONE^Composition supprimée." : "KO^0^" + error);
    }
    else if (command == "IMPROVEND")
    {
        Session::EndImprov(player);
    }
    else if (command == "DUEL" && fields.size() >= 3)
    {
        Optional<uint32> songId = Acore::StringTo<uint32>(fields[1]);
        Optional<uint32> instrumentId = Acore::StringTo<uint32>(fields[2]);
        std::string error = (!songId || !instrumentId) ? "Air ou instrument inconnu." : Duel::Propose(player, *songId, *instrumentId);
        if (!error.empty())
            Send(player, "KO^0^" + error);
    }
    else if ((command == "DUELJOIN" || command == "DUELNO") && fields.size() >= 2)
    {
        Optional<uint32> duelId = Acore::StringTo<uint32>(fields[1]);
        Optional<uint32> instrumentId = fields.size() >= 3 ? Acore::StringTo<uint32>(fields[2]) : Optional<uint32>(0);
        if (duelId && instrumentId)
            Duel::Answer(player, *duelId, command == "DUELJOIN", *instrumentId);
    }
    else if (command == "TOP" && fields.size() >= 2)
    {
        if (Optional<uint32> songId = Acore::StringTo<uint32>(fields[1]))
            Session::SendTop(player, *songId);
    }
    else if (command == "TUNE" && fields.size() >= 2)
    {
        Optional<uint32> instrumentId = Acore::StringTo<uint32>(fields[1]);
        std::string error = instrumentId ? Atelier::Tune(player, *instrumentId) : "Instrument inconnu.";
        Send(player, error.empty() ? "DONE^Instrument accordé." : "KO^0^" + error);
    }
    else if (command == "SPEC")
        Spec::Send(player);
    else if (command == "SPECSET" && fields.size() >= 2)
    {
        Optional<uint32> spec = Acore::StringTo<uint32>(fields[1]);
        std::string error = spec ? Spec::Choose(player, *spec) : "Spécialisation inconnue.";
        if (error.empty())
        {
            Send(player, "DONE^Spécialisation choisie.");
            Spec::Send(player);
            Titles::Check(player);
        }
        else
            Send(player, "KO^0^" + error);
    }
    else if (command == "LEGEND")
        Legend::Send(player);
    else if (command == "LEGENDGIVE")
    {
        std::string error = Legend::Deliver(player);
        Send(player, error.empty() ? "DONE^Matériaux remis." : "KO^0^" + error);
        Legend::Send(player);
    }
    else if (command == "LEGENDFORGE" && fields.size() >= 2)
    {
        Optional<uint32> instrumentId = Acore::StringTo<uint32>(fields[1]);
        std::string error = instrumentId ? Legend::Forge(player, *instrumentId) : "Instrument inconnu.";
        Send(player, error.empty() ? "DONE^Votre instrument légendaire est forgé. Que mille voix le chantent !" : "KO^0^" + error);
        Legend::Send(player);
    }
    else if (command == "CAMP")
    {
        std::string error = Atelier::CraftCamp(player);
        Send(player, error.empty() ? "DONE^Camp de barde fabriqué." : "KO^0^" + error);
    }
    else if (command == "KEYS")
    {
        std::string error = Atelier::CraftKeys(player);
        Send(player, error.empty() ? "DONE^5 clés d'accordage fabriquées." : "KO^0^" + error);
    }
    else if (command == "HITS" && fields.size() >= 2)
    {
        Session::RelayNotes(player, fields[1]);
    }
    else if (command == "RES")
    {
        Session::HandleResult(player, fields);
    }
    else if (command == "ABORT" && fields.size() >= 2)
    {
        if (Optional<uint32> token = Acore::StringTo<uint32>(fields[1]))
            Session::Abort(player, *token);
    }
}

} // namespace Barde::Protocol
