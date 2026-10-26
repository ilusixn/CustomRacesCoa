// AscGeosets.cpp (see Geosets.h): creature geoset overrides.
#include "CoA/Source/Modules/Geosets.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/PatchData.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Dbc.h"
#include <cstring>
#include <unordered_map>

namespace {
    CoA::Geosets::Deps s_deps;
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint32_t>> s_geosets;   // 0x10BDF4D4
    bool s_built = false;
    uint32_t s_display = 0;         // 0x10BDED60
    void* s_lastModel = nullptr;    // 0x10BDED64

    void LastModelCreated(void* model) {   // 0x101A14F0
        s_lastModel = model;
    }
}

CoA::Geosets::Deps& CoA::Geosets::Dependencies() {
    return s_deps;
}

void CoA::Geosets::Build() {
    s_built = true;
    s_geosets.clear();
    CoA::Dbc::Table& t = CoA::Dbc::Get("DBFilesClient\\CreatureDisplayInfoGeosetData.dbc");

    for (uint32_t i = 0; i < t.Count(); ++i) {
        const uint8_t* r = t.RowAt(i);
        const uint32_t display = CoA::Dbc::Table::U32(r, 0xC);
        const uint32_t group = CoA::Dbc::Table::U32(r, 4);
        const uint32_t geoset = CoA::Dbc::Table::U32(r, 8);
        auto d = s_geosets.find(display);

        if (d != s_geosets.end() && d->second.count(group)) {
            continue;
        }

        if (geoset - 1 < 100) {
            s_geosets[display][group] = geoset;
        }
    }
}

void CoA::Geosets::Rebuild(const uint8_t*) {
    Build();
}

int32_t CoA::Geosets::Lookup(uint32_t display, uint32_t group) {
    if (!s_built) {
        Build();
    }

    auto d = s_geosets.find(display);

    if (d == s_geosets.end()) {
        return -1;
    }

    auto g = d->second.find(group);
    return g == d->second.end() ? -1 : static_cast<int32_t>(g->second);
}

void CoA::Geosets::Apply(void* model) {
    if (!s_display || !s_deps.setGeometryVisible) {
        return;
    }

    for (uint32_t group = 0; group < 0x14; ++group) {
        const int32_t geoset = Lookup(s_display, group);

        if (geoset == -1) {
            continue;
        }

        const uint32_t base = group * 100;
        s_deps.setGeometryVisible(model, base, base + 99, 0);
        s_deps.setGeometryVisible(model, base + static_cast<uint32_t>(geoset), base + static_cast<uint32_t>(geoset), 1);
    }
}

uint32_t CoA::Geosets::Display() {
    return s_display;
}

void CoA::Geosets::SetDisplay(uint32_t display) {
    s_display = display;
}

void* CoA::Geosets::LastModel() {
    return s_lastModel;
}

CoA::Geosets::DisplayScope::DisplayScope(uint32_t display) {
    s_display = display;
}

CoA::Geosets::DisplayScope::~DisplayScope() {
    s_display = 0;
}

void CoA::Geosets::ModelCreated(void* model) {
    LastModelCreated(model);
}

void CoA::Geosets::AfterApplyMonsterGeosets(void* model) {
    // 0x101A12D0 hands FUN_101a1510 the model unchecked; with a display that has entries, a null model would be
    // dereferenced by the 0x82C7C0 replacement (ledgered ruling).
    if (model) {
        Apply(model);
    }
}

void CoA::Geosets::AfterShowCharacter(bool haveEntry, uint32_t petDisplay, void* petModel) {
    if (!haveEntry || !petModel) {
        return;
    }

    s_display = petDisplay;
    Apply(petModel);
    s_display = 0;
}

void CoA::Geosets::AfterSetUpModel(uint32_t display) {
    if (!s_lastModel) {
        return;
    }

    s_display = display;
    Apply(s_lastModel);
    s_display = 0;
}

bool CoA::Geosets::SetRange(uint32_t count, const uint8_t* ids, size_t stride, uint32_t* flags, uint32_t first, uint32_t last,
    uint32_t visible) {
    bool changed = false;

    for (uint32_t i = 0; i < count; ++i) {
        uint16_t id;
        memcpy(&id, ids + i * stride, sizeof(id));

        if (first <= id && id <= last && flags[i] != visible) {
            flags[i] = visible;
            changed = true;
        }
    }

    return changed;
}

void CoA::Geosets::Init() {
    // FUN_101a1570's last detour (0x81F8F0 -> 0x101A14F0) and SMSG 0x6FC's rebuild.
    CoA::ClientHooks::afterCreateModel.Add(LastModelCreated);
    CoA::PatchData::Dependencies().geosetsRebuild = CoA::Geosets::Rebuild;
}

void CoA::Geosets::Reset() {
    s_deps = Deps();
    s_geosets.clear();
    s_built = false;
    s_display = 0;
    s_lastModel = nullptr;
}
