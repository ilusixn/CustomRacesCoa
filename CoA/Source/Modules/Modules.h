#ifndef COA_SOURCE_MODULES_MODULES_H
#define COA_SOURCE_MODULES_MODULES_H

#include <cstdint>

// Every ported module's entry point (AscBindings::Module's init + its binding table), one per reconstruction file.
// CoA.cpp lists them in s_modules. The original runs them in static-initialisation order, which nothing observes.
namespace CoA::RealmInfo { void Init(); }
namespace CoA::Account { void Init(); }
namespace CoA::Config { void Init(); }
namespace CoA::GameEvents { void Init(); }
namespace CoA::Misc { void Init(); }
namespace CoA::GlobalsB { void Init(); }
namespace CoA::GlobalsD { void Init(); }
namespace CoA::GlobalsG { void Init(); }
namespace CoA::Bindings { void Init(); }
namespace CoA::GlobalsJ { void Init(); }
namespace CoA::Json { void Init(); }
namespace CoA::LoggerApi { void Init(); }
namespace CoA::SmallApis { void Init(); }
namespace CoA::GlueCVars { void Init(); }
namespace CoA::Profiler { void Init(); }
namespace CoA::GlobalsH { void Init(); }
namespace CoA::GlobalsC { void Init(); }
namespace CoA::PatchData { void Init(); }
namespace CoA::Specs { void Init(); }
namespace CoA::LuaDbc { void Init(); }
namespace CoA::CharacterList { void Init(); }
namespace CoA::RealmMerge { void Init(); }
namespace CoA::EventPackets { void Init(); uint32_t Value72D(); }
namespace CoA::ObjectAddon { void Init(); }
namespace CoA::AuraAmounts { void Init(); }
namespace CoA::GameMode { void Init(); }
namespace CoA::SpellActivationOverlay { void Init(); }
namespace CoA::SuperTrack { void Init(); }
namespace CoA::GroupFinder { void Init(); }
namespace CoA::BridgeStore { void Init(); }
namespace CoA::VanityCollection { void Init(); }
namespace CoA::MythicPlus { void Init(); }
namespace CoA::CharacterCreate { void Init(); }

// Client-only halves (in *Client.cpp): natives and hooks that are thin passthroughs to client code. Only CoA.cpp
// runs them; the tests never link them.
namespace CoA::MiscClient { void Init(); }
namespace CoA::GlobalsBClient { void Init(); }
namespace CoA::EventPacketsClient { void Init(); }
namespace CoA::PatchDataClient { void Init(); }
namespace CoA::ClientDbcPatchClient { void Init(); }
namespace CoA::ObjectAddonClient { void Init(); }
namespace CoA::AuraAmountsClient { void Init(); }
namespace CoA::GameModeClient { void Init(); }
namespace CoA::GlobalsEClient { void Init(); }
namespace CoA::WorldMapClient { void Init(); }
namespace CoA::SpellActivationOverlayClient { void Init(); }
namespace CoA::SuperTrackClient { void Init(); }
namespace CoA::GroupFinderClient { void Init(); }
namespace CoA::GlobalsGClient { void Init(); }
namespace CoA::BindingsClient { void Init(); }
namespace CoA::LibraryClient { void Init(); }
namespace CoA::SpellTextClient { void Init(); }
namespace CoA::ClassRaceClient { void Init(); }
namespace CoA::GlobalsJClient { void Init(); }
namespace CoA::CVarHooksClient { void Init(); }
namespace CoA::RealmDataClient { void Init(); }
namespace CoA::CharacterCreateClient { void Init(); }
namespace CoA::AttachLateClient { void Init(); }
namespace CoA::AttachFixesClient { void Init(); }
namespace CoA::AttachLuaClient { void Init(); }
namespace CoA::AttachMiscClient { void Init(); }
namespace CoA::ProfilerClient { void Init(); }
namespace CoA::GlueCVarsClient { void Init(); }
namespace CoA::GlobalsCClient { void Init(); }
namespace CoA::GlobalsDClient { void Init(); }
namespace CoA::GlobalsHClient { void Init(); }
namespace CoA::SmallApisClient { void Init(); }

#endif
