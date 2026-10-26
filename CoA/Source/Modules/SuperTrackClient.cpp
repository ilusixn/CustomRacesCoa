// AscSuperTrack.cpp's client questions (positions, the quest log, the world frame), the VisitedSuperTracks int list over
// the client's CVarIntListWrapper, PositionFrame's patch and the world-entry timers.
#include "CoA/Source/Modules/CrashContext.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/SuperTrack.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/Frame/CScriptRegion.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/QuestLog.h"
#include "WoW/Source/Ui/QuestLogLeaderBoardParams.h"
#include "WoW/Source/Ui/WorldFrame.h"
#include <cstring>

namespace {
    bool PlayerPosition(float out[3]) {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (!player) {
            return false;
        }

        const C3Vector p = player->GetPosition();
        out[0] = p.x;
        out[1] = p.y;
        out[2] = p.z;
        return true;
    }

    int32_t CurrentMap() {
        return CGGameUI::m_iCurrentMapID;
    }

    bool CorpsePosition(float out[3]) {
        out[0] = CGGameUI::m_corpsePos.x;
        out[1] = CGGameUI::m_corpsePos.y;
        out[2] = CGGameUI::m_corpsePos.z;
        return true;
    }

    // FUN_102179e0: GetScreenCoordinates on the world frame, then the pixel rounding.
    bool WorldToScreen(const float world[3], float& x, float& y) {
        C3Vector w;
        w.x = world[0];
        w.y = world[1];
        w.z = world[2];
        C3Vector screen;
        screen.x = screen.y = screen.z = 0.0f;
        CGWorldFrame::s_currentWorldFrame->GetScreenCoordinates(w, &screen, nullptr);
        x = CSimpleTop::RoundToPixelWidth(screen.x);
        y = CSimpleTop::RoundToPixelHeight(screen.y);
        return true;
    }

    bool QuestPoi(uint32_t quest, bool& complete, int32_t& map, int32_t& x, int32_t& y) {
        uint32_t floor = 0;
        int32_t objective = -1;
        return CGQuestLog::GetQuestPOIIcon(static_cast<int32_t>(quest), &complete, &map, &floor, &x, &y, &objective);
    }

    // FUN_10194090 -> GetQuestLeaderBoardByID over a query with the objective index, "require log entry" and "show
    // progress" set; the query's completion flag.
    uint32_t ObjectiveComplete(uint32_t quest, uint32_t objective) {
        QuestLogLeaderBoardParams params;
        params.m_index = static_cast<int32_t>(objective);
        params.m_requireLogEntry = 1;
        params.m_countEmpty = 0;
        params.m_showProgress = 1;
        CGQuestLog::GetQuestLeaderBoardByID(static_cast<int32_t>(quest), &params);
        return static_cast<uint32_t>(params.m_complete);
    }

    CVarIntListWrapper Wrapper() {
        CVarIntListWrapper list;
        list.m_cvar = CoA::SuperTrack::VisitedCVar();
        list.m_pos = 2;
        memset(list.m_buffer, 0, sizeof(list.m_buffer));
        list.m_state = 0;
        return list;
    }

    // FUN_10335020's walk: every value of the list (GetNextValue until 0).
    std::vector<uint32_t> Visited() {
        std::vector<uint32_t> ids;

        if (!CoA::SuperTrack::VisitedCVar()) {
            return ids;
        }

        CVarIntListWrapper list = Wrapper();

        for (uint32_t v = list.GetNextValue(); v; v = list.GetNextValue()) {
            ids.push_back(v);
        }

        return ids;
    }

    // A fresh wrapper: each value added (AddNextValue), then the version header and terminator and CVar::Set(buffer, 1, 0,
    // 0, 1) -- unless the wrapper was read from (state 1) or failed (-1).
    void StoreVisited(const std::vector<uint32_t>& ids) {
        CVar* cvar = CoA::SuperTrack::VisitedCVar();

        if (!cvar) {
            return;
        }

        CVarIntListWrapper list = Wrapper();

        for (uint32_t v : ids) {
            list.AddNextValue(v);
        }

        if (list.m_state == static_cast<uint32_t>(-1) || list.m_state == 1) {
            return;
        }

        list.m_buffer[0] = 0x76;
        list.m_buffer[1] = 0x01;
        list.m_buffer[list.m_pos] = 0;

        // FUN_10114c40, with its length assert (CrashContext::SetCVar).
        CoA::CrashContext::SetCVar(cvar, list.m_buffer, true);
    }

    // FUN_10335400: on -> client flag 0, SetPoint's offsets unscaled; off -> flag 1, scaled.
    void PositionFrame(bool on) {
        FrameScript_SetProfileFlag(on ? 0 : 1);
        g_setPointRawOffsets = on;
    }
}

void CoA::SuperTrackClient::Init() {
    CoA::SuperTrack::Deps& deps = CoA::SuperTrack::Dependencies();
    deps.playerPosition = PlayerPosition;
    deps.currentMap = CurrentMap;
    deps.corpsePosition = CorpsePosition;
    deps.worldToScreen = WorldToScreen;
    deps.questPoi = QuestPoi;
    deps.objectiveComplete = ObjectiveComplete;
    deps.visited = Visited;
    deps.storeVisited = StoreVisited;
    deps.positionFrame = PositionFrame;
    CoA::ClientHooks::enterWorld.Add(CoA::SuperTrack::Start);
}
