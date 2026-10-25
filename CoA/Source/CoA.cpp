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
    { "MiscClient", CoA::MiscClient::Init },
    { "GlobalsBClient", CoA::GlobalsBClient::Init },
    { "GlobalsGClient", CoA::GlobalsGClient::Init },
    { "BindingsClient", CoA::BindingsClient::Init },
    { "LibraryClient", CoA::LibraryClient::Init },
    { "SpellTextClient", CoA::SpellTextClient::Init },
    { "ClassRaceClient", CoA::ClassRaceClient::Init },
    { "GlobalsJClient", CoA::GlobalsJClient::Init },
    { "CVarHooksClient", CoA::CVarHooksClient::Init },
    { "RealmDataClient", CoA::RealmDataClient::Init },
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
