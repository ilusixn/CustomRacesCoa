// AscLoadingScreen.cpp's client half: Map.dbc and the OsSleep gate (Engine does not link CoA).
#include "CoA/Source/Modules/LoadingScreen.h"
#include "CoA/Source/Modules/Modules.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/MapRec.h"

extern bool (*g_osSleepHook)();

namespace {
    bool HasMap(uint32_t map) {   // 0xAD4160
        return g_mapDB.GetRecord(static_cast<int32_t>(map)) != nullptr;
    }

    void SetMapLoadingScreen(uint32_t map, uint32_t screen) {   // +0x24
        if (MapRec* row = g_mapDB.GetRecord(static_cast<int32_t>(map))) {
            row->m_loadingScreenID = static_cast<int32_t>(screen);
        }
    }
}

void CoA::LoadingScreenClient::Init() {
    CoA::LoadingScreen::Deps& d = CoA::LoadingScreen::Dependencies();
    d.hasMap = HasMap;
    d.setMapLoadingScreen = SetMapLoadingScreen;
    g_osSleepHook = CoA::LoadingScreen::SleepAllowed;
}
