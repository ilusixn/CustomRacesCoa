// AscAlphaAmbience.cpp (see AlphaAmbience.h): the alpha ambience sounds.
#include "CoA/Source/Modules/AlphaAmbience.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/CVars.h"
#include <cstring>

namespace {
    CoA::AlphaAmbience::Deps s_deps;
    CVar* s_cvar = nullptr;   // 0x10BE4128

    // 0x10BE410C (FUN_1007dd80; the second 0x2A entry {0x33FAF, 0x33FB0} is dropped by the map).
    const CoA::AlphaAmbience::Entry kAlpha[] = {
        { 0x19, 0x33F77, 0x33F78 }, { 0x1C, 0x33F7F, 0x33F80 }, { 0x1E, 0x33F75, 0x33F76 }, { 0x1F, 0x33F6F, 0x33F70 },
        { 0x20, 0x33F9F, 0x33FA0 }, { 0x21, 0x33FA1, 0x33FA2 }, { 0x25, 0x33F93, 0x33F94 }, { 0x26, 0x33F79, 0x33F7A },
        { 0x28, 0x33F83, 0x33F84 }, { 0x2A, 0x33F73, 0x33F74 }, { 0x2E, 0x33F7B, 0x33F7C }, { 0x2F, 0x33FAD, 0x33FAE },
        { 0x30, 0x33F88, 0x33F89 }, { 0x31, 0x33F81, 0x33F82 },
    };
}

CoA::AlphaAmbience::Deps& CoA::AlphaAmbience::Dependencies() {
    return s_deps;
}

const CoA::AlphaAmbience::Entry* CoA::AlphaAmbience::Table(size_t& count) {
    count = sizeof(kAlpha) / sizeof(kAlpha[0]);
    return kAlpha;
}

bool CoA::AlphaAmbience::OnChanged(CVar*, const char*, const char* value, void*) {
    if (value && strcmp(value, "1") == 0 && s_deps.setAmbience) {
        for (const Entry& a : kAlpha) {
            s_deps.setAmbience(a.id, a.day, a.night);
        }
    }

    return true;
}

void CoA::AlphaAmbience::Init() {
    // name, help, flags, default, callback, category, setCommand, slot, world
    COA_CVAR("useAlphaAmbienceSound", nullptr, 1, "0", CoA::AlphaAmbience::OnChanged, 7, true, &s_cvar, true);
}

void CoA::AlphaAmbience::Reset() {
    s_deps = Deps();
    s_cvar = nullptr;
}
