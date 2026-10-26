#ifndef COA_SOURCE_MODULES_MOVEMENTFORCE_H
#define COA_SOURCE_MODULES_MOVEMENTFORCE_H

#include <cstdint>

class CDataStore;

// AscMovementForce.cpp (module init FUN_102b9830): pushes and pulls the server applies to a unit's movement, integrated
// on the client, plus the smoothing of other units' position updates while a force acts on them. The client half
// (MovementForceClient.cpp) is what the decomp sites call; movements are the client's CMovement_C*, controllers its
// CGUnit_C*.
//
//   SMSG 0x761 (FUN_102b8fe0)  apply to a unit we control: packed guid, u32 counter, force; ack 0x763
//   SMSG 0x762 (FUN_102b9300)  remove from a unit we control: packed guid, u32 counter, u32 id; ack 0x764
//   SMSG 0x765 (FUN_102b9140)  apply to another unit: packed guid, movement info, force
//   SMSG 0x766 (FUN_102b9530)  remove from another unit: packed guid, movement info, u32 id
//   force (FUN_102ba040): u32 id, float x, y, z, u32 filter, float magnitude, u32 type (1 = a point the unit is pulled
//   towards, anything else a direction)
//
//   0x6F09F0 CMovement_C::ExecuteMovement     FUN_102b8be0  the time of the last player move while a force pushes it
//   0x987D00 CMovementShared::PlotUnitMovement FUN_102b8cb0  the forces' displacement added to the move step
//   0x6EB730 CMovement_C::UpdateStatusInternal FUN_102b8da0  another unit's position update kept and interpolated
//   0x762E00 CMovement_C::CollideRequestMove  FUN_102b8c30  runs with the unit flagged as moving while it stands still
//   0x403B70 LoadNewWorld                     FUN_102b9b90  resets everything, then the original
//   0x6F1490 MovementIdleMoveUnits            FUN_102b9c00  per-frame step: the player's forces, then every other unit
// The reset FUN_102b8f70 also runs on leaving the world, before 0x528F00 and after 0x402910.
namespace CoA::MovementForce {
    struct Deps {
        void* (*movementOf)(uint64_t guid) = nullptr;            // FUN_102b8fc0: 0x4D4DB0(guid, 0x18) +0xD8
        uint64_t (*activeMover)() = nullptr;                     // *0xCA1238 CGUnit_C::s_activeMover
        int32_t (*now)() = nullptr;                              // 0x86AE20 OsGetAsyncTimeMs
        uint32_t (*idleTime)() = nullptr;                        // 0x6E9230 MovementGetIdleTime
        uint32_t* (*flags)(void* m) = nullptr;                   // movement +0x44
        float* (*position)(void* m) = nullptr;                   // +0x10
        float* (*anchor)(void* m) = nullptr;                     // +0x4C
        uint32_t (*transportLow)(void* m) = nullptr;             // +0x8
        void* (*controller)(void* m) = nullptr;                  // +0x144, the unit
        uint64_t (*objectGuid)(void* unit) = nullptr;            // **(unit + 8)
        uint32_t (*typeId)(void* unit) = nullptr;                // unit +0x14
        bool (*isActiveMover)(void* unit) = nullptr;             // 0x4D43C0
        void (*updateAnchors)(void* m) = nullptr;                // 0x9881D0(m, 0)
        void (*applyMovement)(void* m, int32_t now, uint32_t dt) = nullptr;   // 0x6EAC40
        void (*onMoveUpdate)(void* unit, int32_t now) = nullptr;               // 0x73AB20(unit, now, 1, 0)
        void (*stopAnimation)(void* unit) = nullptr;                           // 0x73AC30(unit, 0, -1)
        void (*heartbeat)(void* unit, int32_t now) = nullptr;                  // 0x71F0C0(unit, now, 0xEE, 0, 0, 0, 0, 0xFF)
        // 0x4F4C50 / 0x4F4D40: the movement info read from the packet and, with m, applied through the original
        // 0x6EB730 (not the hook). False when the packet ran out.
        bool (*readMovement)(CDataStore* msg, void* m) = nullptr;
        void (*writeMovement)(void* m, uint32_t opcode, int32_t time, CDataStore* msg) = nullptr;   // 0x7164B0
    };

    Deps& Dependencies();

    // ---- packets -----------------------------------------------------------------------------------------------
    int32_t OnApply(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);         // 0x761
    int32_t OnRemove(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);        // 0x762
    int32_t OnApplyOther(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);    // 0x765
    int32_t OnRemoveOther(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);   // 0x766

    // ---- hooks -------------------------------------------------------------------------------------------------
    void AfterExecuteMovement(void* m, uint32_t time);                     // FUN_102b8be0
    int32_t AfterPlot(void* m, uint32_t dt, float* out, int32_t result);   // FUN_102b8cb0
    // FUN_102b8da0 around UpdateStatusInternal: Before keeps the old position when it will smooth.
    struct StatusUpdate {
        bool keep = false;
        uint64_t guid = 0;
        float oldPos[3] = {};
        float oldAnchor[3] = {};
    };
    StatusUpdate BeforeStatus(void* m, uint32_t statusFlags);
    void AfterStatus(const StatusUpdate& update, void* m, int32_t serverTime);
    // FUN_102b8c30 around CollideRequestMove: true when it flagged the movement (saved holds the flags to restore).
    bool BeforeCollide(void* m, uint32_t& saved);
    void AfterCollide(void* m, bool flagged, uint32_t saved);
    void Reset();   // FUN_102b8f70
    void Tick();    // FUN_102b9c00, after MovementIdleMoveUnits

    // Tests only.
    uint32_t ForceCount(uint64_t guid);
    bool Smoothing(uint64_t guid);
    bool Stopped(uint64_t guid);
    bool Forced(void* m);
    void Clear();

    void Init();
}

#endif
