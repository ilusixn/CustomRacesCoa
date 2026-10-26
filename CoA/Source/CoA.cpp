#include "CoA/Source/CoA.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Modules/Account.h"
#include "CoA/Source/Modules/GlobalsJ.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/DbcPatch.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Timer.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "CoA/Source/Runtime/Events.h"
#include "Engine/Source/Storm/H/storm.h"
#include "CoA/Source/Runtime/Logger.h"
#include "CoA/Source/Runtime/LuaReg.h"

extern void (*g_luaBaseOpenedHook)(lua_State* L);
extern const char** (*g_createEventsHook)(const char** names, uint32_t& count);

// The ported modules, in AscBindings::InitModules order. Filled as modules are ported.
static const CoA::Module s_modules[] = {
    { "DbcPatch", CoA::DbcPatch::Init },
    // FUN_1019c510's camera CVars, then the attach init's option CVars, register before every other world CVar
    // (RegisterWorldQueue).
    { "Camera", CoA::Camera::Init },
    { "ClientOptions", CoA::ClientOptions::Init },
    // FUN_102bf920, called from the attach init (FUN_10a66100).
    { "NamePlates", CoA::NamePlates::Init },
    { "RealmInfo", CoA::RealmInfo::Init },
    { "Account", CoA::Account::Init },
    { "Config", CoA::Config::Init },
    { "GameEvents", CoA::GameEvents::Init },
    { "Misc", CoA::Misc::Init },
    { "GlobalsB", CoA::GlobalsB::Init },
    { "GlobalsD", CoA::GlobalsD::Init },
    { "GlobalsG", CoA::GlobalsG::Init },
    { "Bindings", CoA::Bindings::Init },
    { "GlobalsJ", CoA::GlobalsJ::Init },
    { "AddonTaint", CoA::AddonTaint::Init },
    { "Json", CoA::Json::Init },
    { "LoggerApi", CoA::LoggerApi::Init },
    { "SmallApis", CoA::SmallApis::Init },
    { "GlueCVars", CoA::GlueCVars::Init },
    { "Profiler", CoA::Profiler::Init },
    { "GlobalsH", CoA::GlobalsH::Init },
    { "GlobalsC", CoA::GlobalsC::Init },
    { "PatchData", CoA::PatchData::Init },
    { "Specs", CoA::Specs::Init },
    { "LuaDbc", CoA::LuaDbc::Init },
    { "CharacterList", CoA::CharacterList::Init },
    { "RealmMerge", CoA::RealmMerge::Init },
    { "EventPackets", CoA::EventPackets::Init },
    { "BridgeStore", CoA::BridgeStore::Init },
    { "VanityCollection", CoA::VanityCollection::Init },
    { "MythicPlus", CoA::MythicPlus::Init },
    { "CharacterCreate", CoA::CharacterCreate::Init },
    { "Token", CoA::Token::Init },
    { "HighRisk", CoA::HighRisk::Init },
    { "Seasonal", CoA::Seasonal::Init },
    { "CanToggle", CoA::CanToggle::Init },
    { "BuildDraft", CoA::BuildDraft::Init },
    { "DraftMode", CoA::DraftMode::Init },
    { "ScrollOfFortune", CoA::ScrollOfFortune::Init },
    { "Tutorial", CoA::Tutorial::Init },
    { "Challenge", CoA::Challenge::Init },
    { "TrialCreator", CoA::TrialCreator::Init },
    { "BuildCreator", CoA::BuildCreator::Init },
    { "Wildcard", CoA::Wildcard::Init },
    { "TemporalContracts", CoA::TemporalContracts::Init },
    { "GMTicket", CoA::GMTicket::Init },
    { "BugTracker", CoA::BugTracker::Init },
    { "PlayerPoll", CoA::PlayerPoll::Init },
    { "ChatCheck", CoA::ChatCheck::Init },
    { "FriendsMetadata", CoA::FriendsMetadata::Init },
    { "RecoveryService", CoA::RecoveryService::Init },
    { "CustomStore", CoA::CustomStore::Init },
    { "Callboard", CoA::Callboard::Init },
    { "CallboardCache", CoA::CallboardCache::Init },
    { "Discord", CoA::Discord::Init },
    { "GlobalsF", CoA::GlobalsF::Init },
    { "AutoQuest", CoA::AutoQuest::Init },
    { "UnitAddon", CoA::UnitAddon::Init },
    { "ExtraActionButton", CoA::ExtraActionButton::Init },
    { "MysticEnchant", CoA::MysticEnchant::Init },
    { "MysticEnchantPreset", CoA::MysticEnchantPreset::Init },
    { "CA", CoA::CA::Init },
    { "SpellApi", CoA::SpellApi::Init },
    { "Appearance", CoA::Appearance::Init },
    { "ObjectAddon", CoA::ObjectAddon::Init },
    { "AuraAmounts", CoA::AuraAmounts::Init },
    { "GameMode", CoA::GameMode::Init },
    { "SpellActivationOverlay", CoA::SpellActivationOverlay::Init },
    { "SuperTrack", CoA::SuperTrack::Init },
    { "GroupFinder", CoA::GroupFinder::Init },
    // After CallboardCache / TemporalContracts / CustomStore / Wildcard / ObjectAddon, whose Deps they fill.
    { "QuestAddon", CoA::QuestAddon::Init },
    { "CollectorCache", CoA::CollectorCache::Init },
    { "SkillCardCollection", CoA::SkillCard::Init },
    { "SkillCardSlots", CoA::SkillCardSlots::Init },
    { "AlternatePower", CoA::AlternatePower::Init },
    { "ItemCollections", CoA::ItemCollections::Init },
    // After MysticEnchant / GlobalsJ, whose Deps they fill.
    { "Manastorm", CoA::Manastorm::Init },
    { "LootLockout", CoA::LootLockout::Init },
    { "ItemStats", CoA::ItemStats::Init },
    { "POI", CoA::POI::Init },
    { "SpellCharges", CoA::SpellCharges::Init },
    { "Aura137", CoA::Aura137::Init },
    { "AssetQuery", CoA::AssetQuery::Init },
    { "Templates", CoA::Templates::Init },
    { "AlphaAmbience", CoA::AlphaAmbience::Init },
    { "TerrainScale", CoA::TerrainScale::Init },
    { "SceneTransform", CoA::SceneTransform::Init },
    { "LoadingScreen", CoA::LoadingScreen::Init },
    { "UnitSelect", CoA::UnitSelect::Init },
    { "TooltipScaling", CoA::TooltipScaling::Init },
    { "ModelNames", CoA::ModelNames::Init },
    { "SpawnVisibility", CoA::SpawnVisibility::Init },
    // After PatchData, whose geosetsRebuild it fills.
    { "Geosets", CoA::Geosets::Init },
    { "SpellShadow", CoA::SpellShadow::Init },
    { "SpellVisuals", CoA::SpellVisuals::Init },
    { "ZoneLight", CoA::ZoneLight::Init },
    { "FontRender", CoA::FontRender::Init },
    // FUN_10265d20 (the render module) and FUN_10a1e950 (the cursor module).
    { "Graphics", CoA::Graphics::Init },
    { "Cursor", CoA::Cursor::Init },
    // FUN_10324a90 (spell modifiers) and FUN_102b9830 (movement forces).
    { "SpellMods", CoA::SpellMods::Init },
    { "MovementForce", CoA::MovementForce::Init },
    // FUN_1030f1b0.. (the Lua packet API), FUN_103223a0 / FUN_10227970 (small hooks), FUN_100d4b90 / FUN_100e0cc0
    // (gossip and quest events) and FUN_10a11840 (spell-description tokens).
    { "PacketApi", CoA::PacketApi::Init },
    { "SmallHooks", CoA::SmallHooks::Init },
    { "GossipQuest", CoA::GossipQuest::Init },
    { "SpellTokens", CoA::SpellTokens::Init },
    // FUN_10a60cd0 / FUN_10296fd0 (client guards); AscCrashContext (FUN_1019dc80), AscInitHooks (FUN_10a60d20) and
    // AscLargeAlloc (FUN_102a85f0) are client sites only (their *Client halves).
    { "ClientGuards", CoA::ClientGuards::Init },
    { "MiscClient", CoA::MiscClient::Init },
    { "GlobalsBClient", CoA::GlobalsBClient::Init },
    { "GlobalsGClient", CoA::GlobalsGClient::Init },
    { "BindingsClient", CoA::BindingsClient::Init },
    { "LibraryClient", CoA::LibraryClient::Init },
    { "SpellTextClient", CoA::SpellTextClient::Init },
    { "ClassRaceClient", CoA::ClassRaceClient::Init },
    { "GlobalsJClient", CoA::GlobalsJClient::Init },
    { "AddonTaintClient", CoA::AddonTaintClient::Init },
    { "CVarHooksClient", CoA::CVarHooksClient::Init },
    { "RealmDataClient", CoA::RealmDataClient::Init },
    // CoA addition: the live addon list (Interface\AddOns watch).
    { "AddOnWatchClient", CoA::AddOnWatchClient::Init },
    { "CharacterCreateClient", CoA::CharacterCreateClient::Init },
    { "AttachLateClient", CoA::AttachLateClient::Init },
    { "AttachFixesClient", CoA::AttachFixesClient::Init },
    { "AttachLuaClient", CoA::AttachLuaClient::Init },
    { "AttachMiscClient", CoA::AttachMiscClient::Init },
    { "ProfilerClient", CoA::ProfilerClient::Init },
    { "GlueCVarsClient", CoA::GlueCVarsClient::Init },
    { "GlobalsCClient", CoA::GlobalsCClient::Init },
    { "GlobalsDClient", CoA::GlobalsDClient::Init },
    { "GlobalsHClient", CoA::GlobalsHClient::Init },
    { "SmallApisClient", CoA::SmallApisClient::Init },
    { "EventPacketsClient", CoA::EventPacketsClient::Init },
    { "PatchDataClient", CoA::PatchDataClient::Init },
    { "ClientDbcPatchClient", CoA::ClientDbcPatchClient::Init },
    { "HighRiskClient", CoA::HighRiskClient::Init },
    { "CanToggleClient", CoA::CanToggleClient::Init },
    { "DraftModeClient", CoA::DraftModeClient::Init },
    { "TutorialClient", CoA::TutorialClient::Init },
    { "ChallengeClient", CoA::ChallengeClient::Init },
    { "TrialCreatorClient", CoA::TrialCreatorClient::Init },
    { "BuildCreatorClient", CoA::BuildCreatorClient::Init },
    { "WildcardClient", CoA::WildcardClient::Init },
    { "TemporalContractsClient", CoA::TemporalContractsClient::Init },
    { "GMTicketClient", CoA::GMTicketClient::Init },
    { "BugTrackerClient", CoA::BugTrackerClient::Init },
    { "RecoveryServiceClient", CoA::RecoveryServiceClient::Init },
    { "CustomStoreClient", CoA::CustomStoreClient::Init },
    { "CallboardClient", CoA::CallboardClient::Init },
    { "DiscordClient", CoA::DiscordClient::Init },
    { "WidgetClient", CoA::WidgetClient::Init },
    { "ModelMethodsClient", CoA::ModelMethodsClient::Init },
    { "GlobalsFClient", CoA::GlobalsFClient::Init },
    { "GlobalsIClient", CoA::GlobalsIClient::Init },
    { "UnitAddonClient", CoA::UnitAddonClient::Init },
    { "ExtraActionButtonClient", CoA::ExtraActionButtonClient::Init },
    { "MysticEnchantClient", CoA::MysticEnchantClient::Init },
    { "CAClient", CoA::CAClient::Init },
    { "SpellApiClient", CoA::SpellApiClient::Init },
    { "AppearanceClient", CoA::AppearanceClient::Init },
    { "ObjectAddonClient", CoA::ObjectAddonClient::Init },
    { "AuraAmountsClient", CoA::AuraAmountsClient::Init },
    { "GameModeClient", CoA::GameModeClient::Init },
    { "GlobalsEClient", CoA::GlobalsEClient::Init },
    { "WorldMapClient", CoA::WorldMapClient::Init },
    { "SpellActivationOverlayClient", CoA::SpellActivationOverlayClient::Init },
    { "SuperTrackClient", CoA::SuperTrackClient::Init },
    { "GroupFinderClient", CoA::GroupFinderClient::Init },
    { "SkillCardClient", CoA::SkillCardClient::Init },
    { "AlternatePowerClient", CoA::AlternatePowerClient::Init },
    { "ItemCollectionsClient", CoA::ItemCollectionsClient::Init },
    { "ManastormClient", CoA::ManastormClient::Init },
    { "LootLockoutClient", CoA::LootLockoutClient::Init },
    { "ItemStatsClient", CoA::ItemStatsClient::Init },
    { "SpellChargesClient", CoA::SpellChargesClient::Init },
    { "Aura137Client", CoA::Aura137Client::Init },
    { "AssetQueryClient", CoA::AssetQueryClient::Init },
    { "TemplatesClient", CoA::TemplatesClient::Init },
    { "ClientOptionsClient", CoA::ClientOptionsClient::Init },
    { "CameraClient", CoA::CameraClient::Init },
    { "AlphaAmbienceClient", CoA::AlphaAmbienceClient::Init },
    { "TerrainScaleClient", CoA::TerrainScaleClient::Init },
    { "SceneTransformClient", CoA::SceneTransformClient::Init },
    { "LoadingScreenClient", CoA::LoadingScreenClient::Init },
    { "NamePlatesClient", CoA::NamePlatesClient::Init },
    { "UnitSelectClient", CoA::UnitSelectClient::Init },
    { "TooltipScalingClient", CoA::TooltipScalingClient::Init },
    { "ModelNamesClient", CoA::ModelNamesClient::Init },
    { "SpawnVisibilityClient", CoA::SpawnVisibilityClient::Init },
    { "GeosetsClient", CoA::GeosetsClient::Init },
    { "SpellShadowClient", CoA::SpellShadowClient::Init },
    { "SpellVisualsClient", CoA::SpellVisualsClient::Init },
    { "ZoneLightClient", CoA::ZoneLightClient::Init },
    { "FontRenderClient", CoA::FontRenderClient::Init },
    { "GraphicsClient", CoA::GraphicsClient::Init },
    { "CursorClient", CoA::CursorClient::Init },
    { "SpellModsClient", CoA::SpellModsClient::Init },
    { "MovementForceClient", CoA::MovementForceClient::Init },
    { "PacketApiClient", CoA::PacketApiClient::Init },
    { "SmallHooksClient", CoA::SmallHooksClient::Init },
    { "GossipQuestClient", CoA::GossipQuestClient::Init },
    { "SpellTokensClient", CoA::SpellTokensClient::Init },
    { "CrashContextClient", CoA::CrashContextClient::Init },
    { "InitHooksClient", CoA::InitHooksClient::Init },
    { "ClientGuardsClient", CoA::ClientGuardsClient::Init },
};

// Runtime/CrashTrace.cpp (developer diagnostic, -extlog only).
void CoA_InstallCrashTrace();

void CoA::Attach() {
    // DllMain (FUN_10a79300): Storm's log title "Ascension" (0x771890); the client sets its own later.
    SErrSetLogTitleString("Ascension");
    CoA::Log::Init();
    CoA::Logger::Init();
    CoA::Log::Printf("CoA attach");
    if (CoA::Log::Enabled()) {
        CoA_InstallCrashTrace();
    }

    // AscRuntime::InstallHooks: the event table extension (orig hook 0x81B5F0) and the DLL's own event names.
    g_createEventsHook = CoA::Events::ExtendList;
    CoA::Events::RegisterNames();
    CoA::Events::InstallClient();
    CoA::ClientHooks::InstallClient();
    // AscDbc: custom tables read through the client's SFile.
    CoA::Dbc::InstallClient();
    CoA::ClientState::InstallClient();
    CoA::Account::InstallClient();
    CoA::GlobalsJ::InstallClient();
    CoA::Packet::InstallClient();
    CoA::Timer::InstallClient();
    // The after-paint lifecycle list (orig hook 0x495810).
    g_paintScreenHook = CoA::Lifecycle::RunAfterPaint;

    for (const CoA::Module& module : s_modules) {
        if (!module.init) {
            continue;
        }

        CoA::Log::Printf("init %s", module.name);
        module.init();
    }

    // AscRegistry::OnAttach: natives go into each new Lua state from luaopen_base (orig hook 0x855060).
    g_luaBaseOpenedHook = CoA_OnLuaBaseOpened;
}
