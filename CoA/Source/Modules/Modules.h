#ifndef COA_SOURCE_MODULES_MODULES_H
#define COA_SOURCE_MODULES_MODULES_H

// Every ported module's entry point (AscBindings::Module's init + its binding table), one per reconstruction file.
// CoA.cpp lists them in s_modules. The original runs them in static-initialisation order, which nothing observes.
namespace CoA::RealmInfo { void Init(); }
namespace CoA::Account { void Init(); }
namespace CoA::Config { void Init(); }
namespace CoA::GameEvents { void Init(); }
namespace CoA::Misc { void Init(); }
namespace CoA::GlobalsB { void Init(); }
namespace CoA::GlobalsD { void Init(); }
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
namespace CoA::EventPackets { void Init(); }
namespace CoA::BridgeStore { void Init(); }

// Client-only halves (in *Client.cpp): natives and hooks that are thin passthroughs to client code. Only CoA.cpp
// runs them; the tests never link them.
namespace CoA::MiscClient { void Init(); }
namespace CoA::GlobalsBClient { void Init(); }
namespace CoA::GlobalsGClient { void Init(); }
namespace CoA::BindingsClient { void Init(); }
namespace CoA::LibraryClient { void Init(); }
namespace CoA::SpellTextClient { void Init(); }
namespace CoA::ClassRaceClient { void Init(); }
namespace CoA::GlobalsJClient { void Init(); }
namespace CoA::CVarHooksClient { void Init(); }
namespace CoA::RealmDataClient { void Init(); }

#endif
