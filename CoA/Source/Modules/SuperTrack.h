#ifndef COA_SOURCE_MODULES_SUPERTRACK_H
#define COA_SOURCE_MODULES_SUPERTRACK_H

#include <cstdint>
#include <vector>

struct CVar;

// AscSuperTrack.cpp: C_SuperTrack. The tracker (static 0x10BCC640): the quest id, then the target -- a SuperTrack.dbc
// row's image when it came from the DBC: +0 SuperTrack id, +4 map id (-1 = none), +8 / +0xC / +0x10 x / y / z (z 0 = "the
// player's z + 2"), +0x14 arrival radius, +0x18 next SuperTrack id in the chain, +0x1C ?. Cleared: map -1, position 0,
// radius 5.5. Setters fire SUPER_TRACKING_CHANGED.
//
// Module init (0x10334FB0): the library, the event, the "VisitedSuperTracks" CVar (flags 0x21, default ""), SMSG 0x740,
// and three lifecycle callbacks -- world Lua init (0x103345A0): tracker cleared, "fresh" set, last quest 0; world entry
// (0x103362A0, Start here): three client timers, each re-arming itself -- 1000 ms 0x10336410 re-resolves the tracked
// quest's target, 100 ms 0x103362F0 adds the target's SuperTrack id to the visited list once inside its radius, 1000 ms
// 0x10336340 clears the visited list when the tracked quest changed since the last tick; glue Lua init (0x103350C0):
// cancel the three timers.
//
// The visited list is the client's CVar int list (CVarIntListWrapper over VisitedSuperTracks): at most the last 20 ids,
// values < 0xC4604. SuperTrackClient.cpp reads and writes it and answers the other client questions.
namespace CoA::SuperTrack {
    struct Deps {
        bool (*playerPosition)(float out[3]) = nullptr;                // the active player's position
        int32_t (*currentMap)() = nullptr;                            // CGGameUI::m_iCurrentMapID (0xBD088C)
        bool (*corpsePosition)(float out[3]) = nullptr;               // CGGameUI::m_corpsePos (0xBD0A58)
        bool (*worldToScreen)(const float world[3], float& x, float& y) = nullptr;   // 0x4F6D20 + 0x493D70 / 0x493E00
        // CGQuestLog::GetQuestPOIIcon (0x5DEEE0): complete, map, floor, x, y, objective.
        bool (*questPoi)(uint32_t quest, bool& complete, int32_t& map, int32_t& x, int32_t& y) = nullptr;
        uint32_t (*objectiveComplete)(uint32_t quest, uint32_t objective) = nullptr;   // 0x5E1460's m_complete
        std::vector<uint32_t> (*visited)() = nullptr;                 // the VisitedSuperTracks int list
        void (*storeVisited)(const std::vector<uint32_t>& ids) = nullptr;
        void (*positionFrame)(bool on) = nullptr;                     // PositionFrame's code patch + client flag
    };

    Deps& Dependencies();

    // The VisitedSuperTracks CVar (0x10D3C1A4), set by each world registration.
    CVar* VisitedCVar();

    // FUN_10335020 / FUN_10334020: the visited list.
    bool Visited(uint32_t id);
    void MarkVisited(uint32_t id);

    // 0x103362A0: arm the three timers (world entry).
    void Start();

    void Init();
    // Tests only.
    void Reset();
}

#endif
