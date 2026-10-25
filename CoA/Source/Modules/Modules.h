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

#endif
