#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/Log.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include <vector>

namespace {
    std::vector<CoA::CVarEntry> s_glue;    // sDC.GetGlueCVarVector()
    std::vector<CoA::CVarEntry> s_world;   // list 0x10BDD1A0

    void RegisterList(const std::vector<CoA::CVarEntry>& list) {
        for (const CoA::CVarEntry& e : list) {
            // 0x767FC0(name, help, flags, default, callback, category, setCommand, 0, 0)
            CVar* var = CVar::Register(e.name, e.help, e.flags, e.defaultValue, e.callback, e.category, e.setCommand, nullptr, false);

            if (e.slot) {
                *e.slot = var;
            }
        }
    }
}

void CoA::CVars::Queue(const CoA::CVarEntry& entry, bool world) {
    (world ? s_world : s_glue).push_back(entry);
}

void CoA::CVars::Reset() {
    s_glue.clear();
    s_world.clear();
}

void CoA::CVars::RegisterGlue() {
    RegisterList(s_glue);
    CoA::Log::Printf("CVars: %u glue CVars registered", static_cast<unsigned>(s_glue.size()));
}

void CoA::CVars::RegisterWorld() {
    RegisterList(s_world);
    CoA::Log::Printf("CVars: %u world CVars registered", static_cast<unsigned>(s_world.size()));
}

const CoA::CVarEntry* CoA::CVars::Glue(size_t& count) {
    count = s_glue.size();
    return s_glue.data();
}

const CoA::CVarEntry* CoA::CVars::World(size_t& count) {
    count = s_world.size();
    return s_world.data();
}
