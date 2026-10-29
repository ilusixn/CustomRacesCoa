/*
 * mod-barde - turning a played song into a buff.
 * Licensed under the GNU AGPL v3, see LICENSE.
 *
 * buff = base x accuracy^1.5 x (1 + crescendo), capped at Barde.Buff.Cap x base, as in the
 * prototype: the base runs from the instrument's min_solo on the first song of the book to its
 * max_solo on the hardest one, and the crescendo is +2 % per 25 notes of the best combo.
 * Instrument quality, tuning, mastery and specialisation are all x1 for now. Under
 * Barde.Buff.MinAccuracy the bard plays a wrong note and gets the instrument's malus instead.
 *
 * A song played alone buffs the bard. A song played together (see BardeEnsemble.cpp) multiplies
 * the base by the duo or trio factor, and each bard's chant goes to the whole group. The rules of
 * chants, whoever plays:
 *   - a bard has one chant at a time: a new song replaces the chant he gave before;
 *   - the same instrument never stacks on a person: the stronger stays (a weaker one is refused,
 *     an equal one refreshes the duration);
 *   - chants of different instruments stack, up to Barde.Buff.MaxChants on one person.
 *
 * The aura comes from an existing spell named by the instrument row. Its
 * amount and duration are set here, so the client tooltip may show the
 * spell's own numbers; the buff itself is the one computed below.
 */

#include "Barde.h"

#include "Chat.h"
#include "Map.h"
#include "WorldSession.h"
#include "Player.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"

#include <algorithm>
#include <cmath>

namespace Barde
{

namespace
{

bool IsOurs(AuraEffect const* effect, Instrument const& instrument)
{
    AuraType const type = effect->GetAuraType();
    return type == AuraType(instrument.buffAura) || (instrument.buffAura2 && type == AuraType(instrument.buffAura2));
}

// Adds the aura (cast by `caster` on `target`) and sets every effect the instrument drives (one or two)
// to the same amount.
bool PutAura(Player* caster, Player* target, uint32 spellId, Instrument const& instrument, int32 amount,
             uint32 durationSec)
{
    Aura* aura = caster->AddAura(spellId, target);
    if (!aura)
        return false;

    bool set = false;
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        AuraEffect* effect = aura->GetEffect(i);
        if (effect && IsOurs(effect, instrument))
        {
            effect->ChangeAmount(amount);
            set = true;
        }
    }
    if (!set)
    {
        aura->Remove();
        return false;
    }

    int32 const durationMs = int32(durationSec * IN_MILLISECONDS);
    aura->SetMaxDuration(durationMs);
    aura->SetDuration(durationMs);
    return true;
}

// Amount of an instrument's chant if the player has it (from anyone), 0 otherwise.
int32 ActiveAmount(Player* player, Instrument const& instrument)
{
    Aura* aura = player->GetAura(instrument.buffSpell);
    if (!aura)
        return 0;
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (AuraEffect* effect = aura->GetEffect(i))
            if (IsOurs(effect, instrument))
                return effect->GetAmount();
    return 0;
}

// Puts the bard's chant on `target` following the rules above. Returns false, with the amount of the
// chant that is already there in `current`, when a stronger chant of the same instrument holds the place.
bool GrantChant(Player* bard, Player* target, Instrument const& instrument, int32 amount, uint32 durationSec,
                int32& current)
{
    current = ActiveAmount(target, instrument);
    if (current > amount)
        return false;

    // One chant per bard: his chants of other instruments leave this person.
    for (Instrument const* other : Store::Instance().GetInstruments())
        if (other->id != instrument.id)
            target->RemoveAurasDueToSpell(other->buffSpell, bard->GetGUID());

    target->RemoveAurasDueToSpell(instrument.buffSpell);

    // At most Barde.Buff.MaxChants different chants: the one closest to its end makes room.
    std::vector<Aura*> held;
    for (Instrument const* other : Store::Instance().GetInstruments())
        if (other->id != instrument.id)
            if (Aura* aura = target->GetAura(other->buffSpell))
                held.push_back(aura);
    while (!held.empty() && held.size() >= GetConfig().maxChants)
    {
        auto shortest = std::min_element(held.begin(), held.end(),
            [](Aura const* a, Aura const* b) { return a->GetDuration() < b->GetDuration(); });
        target->RemoveAurasDueToSpell((*shortest)->GetId());
        held.erase(shortest);
    }

    return PutAura(bard, target, instrument.buffSpell, instrument, amount, durationSec);
}

} // namespace

BuffOutcome ApplyGroupBuff(Player* bard, std::vector<Player*> const& recipients, Song const& song,
                           Instrument const& instrument, float accuracy, uint32 maxCombo, bool training,
                           float groupMultiplier, uint32* granted)
{
    Config const& config = GetConfig();
    BuffOutcome outcome;
    if (granted)
        *granted = 0;

    // A composition gives no chant, no malus: it is played for pleasure, for the score and for concerts.
    if (song.owner)
    {
        outcome.message = "Une composition ne donne pas de chant";
        return outcome;
    }

    // The instrument's quality and its tuning (before this song; the song then wears it out a little).
    float const quality = Atelier::QualityMultiplier(bard, instrument);
    float const tuneMult = Atelier::TuningMultiplier(Atelier::Tuning(bard, instrument));
    outcome.tuning = Atelier::WearOut(bard, instrument, training);
    // Mastery of the song, from the successes the bard has had at it.
    uint32 const mastery = Session::MasteryTier(Session::GetWins(bard, song.id));
    float const base = instrument.Base(song) * groupMultiplier * quality * tuneMult * (1.0f + 0.01f * float(mastery)) * Spec::ChantFactor(bard, instrument);
    outcome.mastery = mastery;
    outcome.base = base;
    outcome.quality = quality;
    outcome.tuneMult = tuneMult;

    float pvp = 1.0f;
    if (Map* map = bard->GetMap())
    {
        if (map->IsBattleArena())
            pvp = config.arenaFactor;
        else if (map->IsBattleground())
            pvp = config.battlegroundFactor;
    }

    if (accuracy < config.minAccuracyBuff)
    {
        // A wrong note undoes this instrument's own chant and malus on the bard; nobody else is touched.
        bard->RemoveAurasDueToSpell(instrument.buffSpell);
        bard->RemoveAurasDueToSpell(instrument.malusSpell);

        if (training)
        {
            outcome.message = "Fausse note (sans malus en entraînement)";
            return outcome;
        }

        outcome.malus = true;
        outcome.amount = -std::max<int32>(1, int32(std::lround(instrument.Base(song) * config.malusFraction)));
        outcome.durationSec = config.malusSeconds;
        if (!PutAura(bard, bard, instrument.malusSpell, instrument, outcome.amount, outcome.durationSec))
        {
            outcome = BuffOutcome();
            outcome.message = "Erreur : le malus n'a pas pu être appliqué";
        }
        return outcome;
    }

    if (pvp <= 0.0f)
    {
        outcome.message = "Les chants sont sans effet ici";
        return outcome;
    }

    float const crescendo = std::min(config.crescendoCap,
        config.crescendoStep * float(maxCombo / Score::COMBO_STEP));
    outcome.crescendo = crescendo;
    float value = base * std::pow(accuracy, 1.5f) * (1.0f + crescendo);
    if (value > base * config.buffCap)
    {
        value = base * config.buffCap;
        outcome.capped = true;
    }
    if (training)
        value *= config.trainingBuffFactor;
    value *= pvp;

    outcome.amount = std::max<int32>(1, int32(std::lround(value)));
    outcome.durationSec = (config.buffBaseMinutes + config.buffPerTierMinutes * song.tier) * MINUTE;

    // A good note on this instrument lifts its malus from the bard.
    bard->RemoveAurasDueToSpell(instrument.malusSpell);

    uint32 count = 0;
    int32 stronger = 0;
    for (Player* target : recipients)
    {
        if (!target || !target->IsAlive())
            continue;
        int32 current = 0;
        if (GrantChant(bard, target, instrument, outcome.amount, outcome.durationSec, current))
            ++count;
        else if (target == bard)
            stronger = current;
    }
    if (granted)
        *granted = count;

    // At a camp, the players who listen near the fire get a share of the chant.
    float fraction = 0.0f;
    std::vector<Player*> audience;
    if (count && !training && !outcome.malus)
        audience = Concert::Listeners(bard, fraction);
    fraction = std::min(1.0f, fraction * Spec::MoodFactor(bard));
    if (!audience.empty() && fraction > 0.0f)
    {
        int32 const share = std::max<int32>(1, int32(std::lround(float(outcome.amount) * fraction)));
        for (Player* listener : audience)
        {
            if (std::find(recipients.begin(), recipients.end(), listener) != recipients.end())
                continue;
            int32 current = 0;
            if (GrantChant(bard, listener, instrument, share, outcome.durationSec, current) && listener->GetSession())
                ChatHandler(listener->GetSession()).SendSysMessage(bard->GetName() + " joue : vous profitez de "
                    + instrument.buffName + " (+" + std::to_string(share) + (instrument.unit == "%" ? " %" : "") + " "
                    + instrument.effect + ").");
        }
    }

    if (!count)
    {
        outcome = BuffOutcome();
        outcome.message = stronger ? "Un chant plus puissant est déjà actif (+" + std::to_string(stronger) + ")"
            : "Erreur : le chant n'a pas pu être appliqué";
    }
    return outcome;
}

BuffOutcome ApplyBuff(Player* player, Song const& song, Instrument const& instrument,
                      float accuracy, uint32 maxCombo, bool training)
{
    return ApplyGroupBuff(player, { player }, song, instrument, accuracy, maxCombo, training, 1.0f, nullptr);
}

} // namespace Barde
