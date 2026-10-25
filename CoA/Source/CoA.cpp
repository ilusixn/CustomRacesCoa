#include "CoA/Source/CoA.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/Logger.h"

// The ported modules, in AscBindings::InitModules order. Filled as modules are ported.
static const CoA::Module s_modules[] = {
    { nullptr, nullptr },
};

void CoA::Attach() {
    CoA::Log::Init();
    CoA::Logger::Init();
    CoA::Log::Printf("CoA attach");

    for (const CoA::Module& module : s_modules) {
        if (!module.init) {
            continue;
        }

        CoA::Log::Printf("init %s", module.name);
        module.init();
    }
}
