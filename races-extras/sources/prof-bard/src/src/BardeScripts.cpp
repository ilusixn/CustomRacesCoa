/*
 * mod-barde - hooks into the core: config, startup, chat and commands.
 * Licensed under the GNU AGPL v3, see LICENSE.
 */

#include "Barde.h"

#include "Chat.h"
#include "GameObjectScript.h"
#include "Item.h"
#include "DatabaseEnv.h"
#include "CommandScript.h"
#include "Log.h"
#include "Player.h"
#include "PlayerScript.h"
#include "ScriptMgr.h"
#include "SpellAuras.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "WorldScript.h"

#include <algorithm>

using namespace Acore::ChatCommands;

namespace
{

class BardeWorldScript : public WorldScript
{
public:
    BardeWorldScript() : WorldScript("BardeWorldScript",
        { WORLDHOOK_ON_AFTER_CONFIG_LOAD, WORLDHOOK_ON_STARTUP, WORLDHOOK_ON_UPDATE }) { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        Barde::LoadConfig();
    }

    void OnStartup() override
    {
        Barde::Store::Instance().Load();
    }

    // Once a second: invitations and ensembles that ran out of time.
    void OnUpdate(uint32 diff) override
    {
        _elapsed += diff;
        if (_elapsed < 1000)
            return;
        _elapsed = 0;
        Barde::Ensemble::Update();
        Barde::Camp::Update();
        Barde::Concert::Update();
        Barde::Duel::Update();
    }

private:
    uint32 _elapsed = 0;
};

class BardePlayerScript : public PlayerScript
{
public:
    BardePlayerScript() : PlayerScript("BardePlayerScript",
        { PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT, PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_LOGOUT, PLAYERHOOK_ON_SPELL_CAST, PLAYERHOOK_ON_TEXT_EMOTE }) { }

    // Addon messages arrive as whispers in LANG_ADDON. Ours are swallowed here,
    // including those whispered to someone else, so nobody can fake a server
    // answer to another player's addon.
    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 language, std::string& msg, Player* receiver) override
    {
        if (language != LANG_ADDON || type != CHAT_MSG_WHISPER)
            return true;

        std::string_view text(msg);
        std::string_view const prefix = Barde::Protocol::PREFIX;
        if (text.size() <= prefix.size() || text.substr(0, prefix.size()) != prefix || text[prefix.size()] != '\t')
            return true;

        if (receiver == player)
            Barde::Protocol::Handle(player, text.substr(prefix.size() + 1));
        return false;
    }

    // The look of a song is saved with the character's auras: when the server stopped (or he left) in the middle of one, it would come back
    // at the next login. Nobody is playing at login, so take it away; and the titles he has earned meanwhile.
    void OnPlayerLogin(Player* player) override
    {
        Barde::Config const& config = Barde::GetConfig();
        for (uint32 spell : { config.playAura, config.playSoundAura })
            if (spell)
                player->RemoveAurasDueToSpell(spell);
        Barde::Titles::Check(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        Barde::Session::Forget(player);
        Barde::Spec::Forget(player);
    }

    // /applaud near a concert's purse raises the tips.
    void OnPlayerTextEmote(Player* player, uint32 textEmote, uint32 /*emoteNum*/, ObjectGuid /*guid*/) override
    {
        if (textEmote == TEXT_EMOTE_APPLAUD)
            Barde::Concert::Applaud(player);
    }

    // Clicking « Barde » in the spellbook opens the songbook, as a profession's spell opens its window.
    void OnPlayerSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        if (spell && spell->GetSpellInfo()->Id == Barde::PROFESSION_SPELL)
            Barde::Protocol::Send(player, "OPEN");
    }
};

// The purse of a concert: the spectators tip, the bard collects.
class BardePurse : public GameObjectScript
{
public:
    BardePurse() : GameObjectScript("go_barde_purse") { }

    bool OnGossipHello(Player* player, GameObject* go) override { return Barde::Concert::Hello(player, go); }
    bool OnGossipSelect(Player* player, GameObject* go, uint32 sender, uint32 action) override
    {
        return Barde::Concert::Select(player, go, sender, action);
    }
};

// A scroll of a song: reading it teaches the song to the reader; the scroll is used up.
class BardeScroll : public ItemScript
{
public:
    BardeScroll() : ItemScript("item_barde_scroll") { }

    bool OnUse(Player* player, Item* item, SpellCastTargets const& /*targets*/) override
    {
        std::string error = Barde::Compose::Read(player, item);
        if (!error.empty())
        {
            ChatHandler(player->GetSession()).SendSysMessage(error);
            return true;
        }
        player->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
        // The effect of learning: the blue ring for a few seconds, and a sound.
        Barde::Config const& config = Barde::GetConfig();
        if (config.scrollEffect)
            if (Aura* aura = player->AddAura(config.scrollEffect, player))
            {
                aura->SetMaxDuration(config.scrollEffectSec * IN_MILLISECONDS);
                aura->SetDuration(config.scrollEffectSec * IN_MILLISECONDS);
            }
        if (config.scrollSound)
            player->PlayDirectSound(config.scrollSound, player);
        ChatHandler(player->GetSession()).SendSysMessage("Vous apprenez un nouvel air : il est dans le groupe Custom de votre carnet.");
        return true;
    }
};

// The camp kit: using it puts the camp down; the item is used up only if it worked.
class BardeCampKit : public ItemScript
{
public:
    BardeCampKit() : ItemScript("item_barde_camp") { }

    bool OnUse(Player* player, Item* item, SpellCastTargets const& /*targets*/) override
    {
        std::string error = Barde::Camp::Place(player);
        if (!error.empty())
        {
            ChatHandler(player->GetSession()).SendSysMessage(error);
            return true;
        }
        player->DestroyItemCount(item->GetEntry(), 1, true);
        ChatHandler(player->GetSession()).SendSysMessage("Le camp est posé : le feu et les bancs durent une demi-heure.");
        return true;
    }
};

class BardeCommandScript : public CommandScript
{
public:
    BardeCommandScript() : CommandScript("BardeCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable bardeTable =
        {
            { "airs",      HandleSongs,  SEC_PLAYER,        Console::No },
            { "jouer",     HandlePlay,   SEC_PLAYER,        Console::No },
            { "luth",      HandleLute,   SEC_GAMEMASTER,    Console::No },
            { "instruments", HandleInstruments, SEC_GAMEMASTER, Console::No },
            { "apprendre", HandleLearn,  SEC_GAMEMASTER,    Console::No },
            { "duo",       HandleDuo,    SEC_GAMEMASTER,    Console::No },
            { "aura",      HandleAura,   SEC_GAMEMASTER,    Console::No },
            { "pose",      HandlePose,   SEC_GAMEMASTER,    Console::No },
            { "competence", HandleSkill, SEC_GAMEMASTER,    Console::No },
            { "legende",   HandleLegend, SEC_GAMEMASTER,    Console::No },
            { "recharger", HandleReload, SEC_ADMINISTRATOR, Console::Yes },
        };

        static ChatCommandTable commandTable =
        {
            { "barde", bardeTable },
        };

        return commandTable;
    }

    static bool HandleSongs(ChatHandler* handler)
    {
        for (Barde::Song const* song : Barde::Store::Instance().GetSongs())
            handler->PSendSysMessage("{} - {} ({}, {}, {} BPM, {} notes dont {} tenues, {} s)", song->id, song->name,
                song->subtitle, Barde::TierName(song->tier), song->bpm, song->notes.size(), song->holdCount,
                song->LengthMs(1.0f) / 1000);
        handler->SendSysMessage("Jouer : .barde jouer <numéro> [entrainement], ou /barde avec l'addon.");
        return true;
    }

    static bool HandlePlay(ChatHandler* handler, uint32 songId, Optional<std::string> mode)
    {
        Player* player = handler->GetPlayer();
        bool training = mode && (*mode == "entrainement" || *mode == "entraînement" || *mode == "1");
        std::string error = Barde::Session::Begin(player, songId, training, 1);
        if (!error.empty())
        {
            handler->SendErrorMessage("{}", error);
            return false;
        }
        handler->SendSysMessage("Partition envoyée à l'addon Barde.");
        return true;
    }

    static bool HandleLute(ChatHandler* handler)
    {
        Player* player = handler->getSelectedPlayerOrSelf();
        Barde::Instrument const* instrument = Barde::Store::Instance().GetDefaultInstrument();
        if (!player || !instrument)
            return false;

        if (!player->AddItem(instrument->itemEntry, 1))
        {
            handler->SendErrorMessage("Impossible de donner l'instrument (sacs pleins ?).");
            return false;
        }
        handler->PSendSysMessage("{} donné à {}.", instrument->name, player->GetName());
        return true;
    }

    // .barde instruments [palier 0 à 4] [qualité 0 à 4] : donne un exemplaire de chaque instrument de ce palier (0 : cuivre).
    static bool HandleInstruments(ChatHandler* handler, Optional<uint32> tier, Optional<uint32> quality)
    {
        Player* player = handler->getSelectedPlayerOrSelf();
        if (!player)
            return false;

        uint32 given = 0;
        for (Barde::Instrument const* instrument : Barde::Store::Instance().GetInstruments())
        {
            uint32 const entry = Barde::Store::Instance().ItemFor(instrument->id, uint8(tier.value_or(0)));
            if (entry && !player->HasItemCount(entry) && player->AddItem(entry, 1))
            {
                ++given;
                Barde::Atelier::SetQuality(player->GetItemByEntry(entry), uint8(std::min<uint32>(quality.value_or(0), 4)));
            }
        }
        handler->PSendSysMessage("{} instrument(s) donné(s) à {}.", given, player->GetName());
        return true;
    }

    // .barde competence <valeur> : fixe la compétence de barde du joueur visé (ou de soi), de 1 au maximum du serveur.
    static bool HandleSkill(ChatHandler* handler, uint32 value)
    {
        Player* player = handler->getSelectedPlayerOrSelf();
        if (!player)
            return false;
        uint32 const skill = std::clamp<uint32>(value, 1, Barde::GetConfig().skillMax);
        CharacterDatabase.Execute("REPLACE INTO mod_barde_char_skill (guid, skill) VALUES ({}, {})",
            player->GetGUID().GetCounter(), skill);
        handler->PSendSysMessage("Compétence de barde de {} : {}.", player->GetName(), skill);
        return true;
    }

    // .barde legende tout | reset : pour les essais, valide d'un coup les airs (or et 100 %), le duel et le trio de la quête des Mille Voix
    // (les matériaux se remettent, eux, en jeu) ; reset efface les étapes franchies.
    static bool HandleLegend(ChatHandler* handler, std::string what)
    {
        Player* player = handler->getSelectedPlayerOrSelf();
        if (!player)
            return false;
        std::string const error = Barde::Legend::GmTest(player, what);
        handler->SendSysMessage(error.empty() ? "Étapes de la quête validées : rouvrez la fenêtre de la quête légendaire." : error);
        return true;
    }

    // .barde pose <1 à 3> : l'instrument en main, l'aura de jeu et l'une des danses du jeu, 25 secondes. 0 arrête.
    static bool HandlePose(ChatHandler* handler, uint32 index)
    {
        struct Pose { uint32 state; char const* name; };
        static Pose const poses[] =
        {
            { 10,  "danse" },
            { 400, "danse spéciale" },
            { 413, "danse personnalisée" },
        };
        constexpr uint32 count = sizeof(poses) / sizeof(poses[0]);
        Player* player = handler->GetPlayer();
        if (index == 0)
        {
            Barde::Session::Pose(player, 0);
            handler->SendSysMessage("Pose arrêtée.");
            return true;
        }
        if (index > count)
        {
            handler->SendErrorMessage("Choisissez un numéro de 1 à {} (0 pour arrêter).", count);
            return false;
        }
        Barde::Session::Pose(player, poses[index - 1].state);
        handler->PSendSysMessage("Pose {} : {} (état {}), 25 secondes. Pour la garder pendant les morceaux : Barde.PlayEmoteState = {}.",
            index, poses[index - 1].name, poses[index - 1].state, poses[index - 1].state);
        return true;
    }

    // .barde aura <1 à 7> : montre, 20 secondes, l'une des apparences possibles de l'aura de jeu.
    static bool HandleAura(ChatHandler* handler, uint32 index)
    {
        Player* player = handler->GetPlayer();
        if (index < 1 || index > Barde::PLAY_AURA_COUNT)
        {
            handler->SendErrorMessage("Choisissez un numéro de 1 à {}.", Barde::PLAY_AURA_COUNT);
            return false;
        }
        for (uint32 i = 0; i < Barde::PLAY_AURA_COUNT; ++i)
            player->RemoveAurasDueToSpell(Barde::PLAY_AURA_FIRST + i);
        if (Aura* aura = player->AddAura(Barde::PLAY_AURA_FIRST + index - 1, player))
        {
            aura->SetMaxDuration(20 * IN_MILLISECONDS);
            aura->SetDuration(20 * IN_MILLISECONDS);
        }
        handler->PSendSysMessage("Apparence {} pendant 20 secondes (sort {}).", index, Barde::PLAY_AURA_FIRST + index - 1);
        return true;
    }

    // .barde duo <air> [justesse en %] : un duo de test avec un partenaire virtuel (voir Ensemble::TestDuo).
    static bool HandleDuo(ChatHandler* handler, uint32 songId, Optional<uint32> accuracy)
    {
        std::string error = Barde::Ensemble::TestDuo(handler->GetPlayer(), songId, float(accuracy.value_or(90)) / 100.0f);
        if (!error.empty())
        {
            handler->SendErrorMessage("{}", error);
            return false;
        }
        return true;
    }

    static bool HandleLearn(ChatHandler* handler)
    {
        Player* player = handler->getSelectedPlayerOrSelf();
        if (!player)
            return false;
        player->learnSpell(Barde::PROFESSION_SPELL);
        handler->PSendSysMessage("{} connaît maintenant le métier de barde.", player->GetName());
        return true;
    }

    static bool HandleReload(ChatHandler* handler)
    {
        bool ok = Barde::Store::Instance().Load();
        handler->PSendSysMessage("mod-barde : {} airs rechargés{}.", Barde::Store::Instance().GetSongs().size(),
            ok ? "" : " (avec des erreurs, voir le journal)");
        return true;
    }
};

} // namespace

void AddSC_barde()
{
    new BardeWorldScript();
    new BardePlayerScript();
    new BardeCommandScript();
    new BardeCampKit();
    new BardeScroll();
    new BardePurse();
}
