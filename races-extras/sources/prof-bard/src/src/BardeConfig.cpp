/*
 * mod-barde - reading mod_barde.conf.
 * Licensed under the GNU AGPL v3, see LICENSE.
 */

#include "Barde.h"

#include "Config.h"
#include "StringConvert.h"
#include "Tokenize.h"

#include <algorithm>

namespace Barde
{

namespace
{
Config sConfig;
}

Config const& GetConfig()
{
    return sConfig;
}

void LoadConfig()
{
    sConfig.Load();
}

void Config::Load()
{
    enable = sConfigMgr->GetOption<bool>("Barde.Enable", true);
    requireInstrument = sConfigMgr->GetOption<bool>("Barde.RequireInstrument", true);
    requireProfession = sConfigMgr->GetOption<bool>("Barde.RequireProfession", true);

    skillMinAccuracy = std::clamp(sConfigMgr->GetOption<float>("Barde.Skill.MinAccuracy", 0.70f), 0.0f, 1.0f);
    skillMax = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("Barde.SkillMax", 300));
    minAccuracyBuff = std::clamp(sConfigMgr->GetOption<float>("Barde.Buff.MinAccuracy", 0.50f), 0.0f, 1.0f);
    durationSlackSec = std::max(0.0f, sConfigMgr->GetOption<float>("Barde.Check.DurationSlackSeconds", 2.0f));
    resultGraceSec = sConfigMgr->GetOption<uint32>("Barde.Check.ResultGraceSeconds", 60);

    buffBaseMinutes = sConfigMgr->GetOption<uint32>("Barde.Buff.BaseMinutes", 10);
    buffPerTierMinutes = sConfigMgr->GetOption<uint32>("Barde.Buff.PerTierMinutes", 5);
    buffCap = std::max(1.0f, sConfigMgr->GetOption<float>("Barde.Buff.Cap", 1.2f));
    crescendoStep = std::max(0.0f, sConfigMgr->GetOption<float>("Barde.Buff.CrescendoStep", 0.02f));
    crescendoCap = std::max(0.0f, sConfigMgr->GetOption<float>("Barde.Buff.CrescendoCap", 0.12f));
    arenaFactor = std::max(0.0f, sConfigMgr->GetOption<float>("Barde.Buff.Arena", 0.0f));
    battlegroundFactor = std::max(0.0f, sConfigMgr->GetOption<float>("Barde.Buff.Battleground", 0.5f));

    trainingTempo = std::clamp(sConfigMgr->GetOption<float>("Barde.Training.Tempo", 0.75f), 0.25f, 1.0f);
    trainingBuffFactor = std::clamp(sConfigMgr->GetOption<float>("Barde.Training.BuffFactor", 0.5f), 0.0f, 1.0f);

    malusFraction = std::clamp(sConfigMgr->GetOption<float>("Barde.Malus.Fraction", 0.25f), 0.0f, 1.0f);
    malusSeconds = sConfigMgr->GetOption<uint32>("Barde.Malus.Seconds", 120);

    playAura = sConfigMgr->GetOption<uint32>("Barde.PlayAura", 1950307);
    playSoundAura = sConfigMgr->GetOption<uint32>("Barde.PlaySoundAura", 0);
    playEmoteStates.clear();
    std::string const dances = sConfigMgr->GetOption<std::string>("Barde.PlayEmoteState", "10,400,413");   // the tokens point into it
    for (std::string_view token : Acore::Tokenize(dances, ',', false))
        if (Optional<uint32> state = Acore::StringTo<uint32>(token); state && *state)
            playEmoteStates.push_back(*state);
    playEmote = sConfigMgr->GetOption<bool>("Barde.PlayEmote", true);

    listenRange = std::clamp(sConfigMgr->GetOption<float>("Barde.ListenRange", 7.5f), 0.0f, 100.0f);
    listenMax = std::min<uint32>(sConfigMgr->GetOption<uint32>("Barde.ListenMax", 8), 40);

    crystalEntry = sConfigMgr->GetOption<uint32>("Barde.Craft.CrystalEntry", 12363);

    campSeconds = std::clamp<uint32>(sConfigMgr->GetOption<uint32>("Barde.Camp.Seconds", 1800), 60, 86400);
    campCooldownSec = sConfigMgr->GetOption<uint32>("Barde.Camp.Cooldown", 3600);
    campRange = std::clamp(sConfigMgr->GetOption<float>("Barde.Camp.Range", 8.0f), 3.0f, 30.0f);
    campTune = std::min<uint32>(sConfigMgr->GetOption<uint32>("Barde.Camp.TunePer5Seconds", 1), 100);
    campListenerFraction = std::clamp(sConfigMgr->GetOption<float>("Barde.Camp.ListenerFraction", 0.5f), 0.0f, 1.0f);

    concertEnable = sConfigMgr->GetOption<bool>("Barde.Concert.Enable", true);
    concertGraceSec = std::clamp<uint32>(sConfigMgr->GetOption<uint32>("Barde.Concert.GraceSeconds", 180), 10, 3600);
    concertMaxSec = std::clamp<uint32>(sConfigMgr->GetOption<uint32>("Barde.Concert.MaxSeconds", 1800), 60, 86400);
    applausePercent = std::min<uint32>(sConfigMgr->GetOption<uint32>("Barde.Concert.ApplausePercent", 0), 50);
    applauseMax = std::min<uint32>(sConfigMgr->GetOption<uint32>("Barde.Concert.ApplauseMax", 0), 100);
    campTipPercent = std::min<uint32>(sConfigMgr->GetOption<uint32>("Barde.Camp.TipPercent", 0), 100);
    bonusMaxPerConcert = sConfigMgr->GetOption<uint32>("Barde.Concert.BonusMaxPerConcert", 5000);
    bonusMaxPerDay = sConfigMgr->GetOption<uint32>("Barde.Concert.BonusMaxPerDay", 20000);
    allowSameAccount = sConfigMgr->GetOption<bool>("Barde.Concert.AllowSameAccount", false);
    concertListenerFraction = std::clamp(sConfigMgr->GetOption<float>("Barde.Concert.ListenerFraction", 0.25f), 0.0f, 1.0f);
    concertRange = std::clamp(sConfigMgr->GetOption<float>("Barde.Concert.Range", 15.0f), 3.0f, 40.0f);

    composeMax = std::min<uint32>(sConfigMgr->GetOption<uint32>("Barde.Compose.MaxPerPlayer", 10), 100);
    composeMinNotes = std::clamp<uint32>(sConfigMgr->GetOption<uint32>("Barde.Compose.MinNotes", 16), 4, 400);
    composeMinSeconds = std::clamp<uint32>(sConfigMgr->GetOption<uint32>("Barde.Compose.MinSeconds", 20), 5, 600);
    composeMaxSeconds = std::clamp<uint32>(sConfigMgr->GetOption<uint32>("Barde.Compose.MaxSeconds", 240), 30, 1200);

    scrollEffect = sConfigMgr->GetOption<uint32>("Barde.Scroll.Effect", 1950307);
    scrollEffectSec = std::clamp<uint32>(sConfigMgr->GetOption<uint32>("Barde.Scroll.EffectSeconds", 3), 1, 30);
    scrollSound = sConfigMgr->GetOption<uint32>("Barde.Scroll.Sound", 888);

    specMinSkill = std::min<uint32>(sConfigMgr->GetOption<uint32>("Barde.Spec.MinSkill", 180), 300);
    specChangeCost = sConfigMgr->GetOption<uint32>("Barde.Spec.ChangeCost", 100000);
    specChantPercent = std::min<uint32>(sConfigMgr->GetOption<uint32>("Barde.Spec.ChantPercent", 10), 50);
    ovationSeconds = std::min<uint32>(sConfigMgr->GetOption<uint32>("Barde.Ovation.Seconds", 60), 3600);
    ovationMaxSeconds = std::min<uint32>(sConfigMgr->GetOption<uint32>("Barde.Ovation.MaxSeconds", 300), 7200);

    duoMultiplier = std::max(1.0f, sConfigMgr->GetOption<float>("Barde.Buff.DuoMultiplier", 1.6f));
    trioMultiplier = std::max(1.0f, sConfigMgr->GetOption<float>("Barde.Buff.TrioMultiplier", 2.0f));
    maxChants = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("Barde.Buff.MaxChants", 3));
    ensembleInviteSec = std::clamp<uint32>(sConfigMgr->GetOption<uint32>("Barde.Ensemble.InviteSeconds", 20), 5, 120);
    ensembleCountdownSec = std::clamp<uint32>(sConfigMgr->GetOption<uint32>("Barde.Ensemble.CountdownSeconds", 5), 0, 30);
    ensembleRange = std::max(5.0f, sConfigMgr->GetOption<float>("Barde.Ensemble.Range", 60.0f));
}

} // namespace Barde
