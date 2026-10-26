// AscLoadingScreen.cpp (see LoadingScreen.h): loading-screen variations and the start-up sleep gate.
#include "CoA/Source/Modules/LoadingScreen.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Dbc.h"
#include <random>
#include <vector>

namespace {
    CoA::LoadingScreen::Deps s_deps;
    bool s_open = true;   // 0x10BCB8C8, 1 in the DLL's .data

    std::mt19937& Rng() {   // 0x10BE0CA8, seeded once
        static std::mt19937 rng(std::random_device{}());
        return rng;
    }
}

CoA::LoadingScreen::Deps& CoA::LoadingScreen::Dependencies() {
    return s_deps;
}

uint32_t CoA::LoadingScreen::PickVariation(const uint8_t* row) {
    std::vector<uint32_t> ids;

    for (uint32_t i = 0; i < 0x10; ++i) {
        if (const uint32_t id = CoA::Dbc::Table::U32(row, 4 + i * 4)) {
            ids.push_back(id);
        }
    }

    if (ids.empty()) {
        return 0;
    }

    std::uniform_int_distribution<int> pick(0, static_cast<int>(ids.size()) - 1);
    return ids[pick(Rng())];
}

void CoA::LoadingScreen::ApplyVariation(uint32_t map) {
    const uint8_t* variation = CoA::Dbc::Get("DBFilesClient\\LoadingScreenVariation.dbc").Row(map);

    if (!variation || !s_deps.hasMap || !s_deps.hasMap(map)) {
        return;
    }

    if (const uint32_t screen = CoA::LoadingScreen::PickVariation(variation)) {
        if (s_deps.setMapLoadingScreen) {
            s_deps.setMapLoadingScreen(map, screen);
        }
    }
}

void CoA::LoadingScreen::AfterLoadingScreenDisable() {
    s_open = true;
}

void CoA::LoadingScreen::AfterLoadingScreenEnable() {
    s_open = false;
}

bool CoA::LoadingScreen::SleepAllowed() {
    return s_open;
}

void CoA::LoadingScreen::Init() {
    // The hooks are the decomp / Engine sites (LoadingScreenClient.cpp installs OsSleep's).
}

void CoA::LoadingScreen::Reset() {
    s_deps = Deps();
    s_open = true;
}
