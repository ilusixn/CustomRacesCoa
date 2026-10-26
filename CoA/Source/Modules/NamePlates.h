#ifndef COA_SOURCE_MODULES_NAMEPLATES_H
#define COA_SOURCE_MODULES_NAMEPLATES_H

#include <cstdint>
#include <vector>

struct CVar;
struct lua_State;

// AscNamePlates.cpp: the nameplate module (installer FUN_102bf920) -- the "nameplateN" unit-token registry behind
// GetNamePlateForUnit / NAME_PLATE_UNIT_ADDED / NAME_PLATE_UNIT_REMOVED, its twelve CVars (checked against the genuine
// DLL: 0x102C03C5 .. 0x102C054E, FUN_10114540, help nullptr, flags 0x21, category 4, slots 0x10BE348C .. 0x10BE34B8)
// and the client detours that read them.
//
//   Patches   (ClientHooks::options, set by Init) 0x98E9F9 / 0x98EA07: OnLayerUpdate's target frame level skipped;
//             0x7258CE / 0x72594B: the reposition pass always anchors (nameplateAllowOverlap's path); 0x715739: the
//             plate height reads the DLL's float; 0x71557D / 0x715586: the interact icon always the raised sequence.
//   Detours   0x98E9F0 after  (FUN_102be570) distance fade and intersect opacity
//             0x725890 after  (FUN_102be880) distance and facing-angle culling
//             0x72B060 before (FUN_102c2630) personal plate, neutral / combat filters, fixed vertical offset
//             0x7256C0 around (sub_102beb70) a unit whose plate pointer's low byte was 0 joins the registry
//             0x725840 after  (FUN_102beb40) a unit whose plate pointer's low byte is now 0 leaves it
//             0x9900E0 after  (sub_102c28b0) client event 0x12 for the unit
//             0x715C30 / 0x98E910 (sub_102be530 / FUN_102c2e90) the player's own plate is never the focus
//             0x60ABF0 before (LAB_10a6ec00 / FUN_10a4ea40) "nameplateN" unit tokens
//   Lifecycle FUN_102c29a0 after 0x403340 (queued ADDED events, frame levels by distance); LAB_102bd620 before
//             0x528C30 (the registry cleared; queued ids kept).
namespace CoA::NamePlates {
    // A position and the model height (0x722AE0 with the display id) of an object.
    struct Body {
        uint64_t guid = 0;
        float pos[3] = { 0.0f, 0.0f, 0.0f };   // vtable +0x2C
        float height = 0.0f;
        float facing = 0.0f;                    // vtable +0x34
    };

    // One client plate (the list at 0xADAA88 / a unit's +0xC38).
    struct Plate {
        void* frame = nullptr;
        uint64_t guid = 0;                      // +0x2A8
    };

    // One enumerated unit (0x4D4B30, mask 0x18).
    struct Unit {
        void* unit = nullptr;
        uint64_t guid = 0;
        void* plate = nullptr;                  // +0xC38
        float pos[3] = { 0.0f, 0.0f, 0.0f };
    };

    struct Deps {
        bool (*player)(Body& out) = nullptr;                         // 0x4038F0; false without one
        uint64_t (*lockedTarget)() = nullptr;                        // 0xBD07B0
        bool (*cameraXY)(float& x, float& y) = nullptr;              // 0x4F5960 +8 / +0xC
        bool (*unitBody)(uint64_t guid, uint32_t mask, Body& out) = nullptr;   // 0x4D4DB0
        bool (*lineBlocked)(const float from[3], const float to[3]) = nullptr; // 0x77F310(.., 0x100070, 0)
        void (*setAlpha)(void* frame, uint32_t alpha) = nullptr;     // 0x48EA10
        void (*showPlate)(void* frame, bool shown) = nullptr;        // 0x48F660 / 0x48F620
        float (*bearing)(const float from[3], const float to[3]) = nullptr;    // 0x4F5130
        std::vector<Plate> (*plates)() = nullptr;                    // the client's list, in order
        std::vector<Unit> (*units)() = nullptr;                      // 0x4D4B30 order
        void (*setFrameLevel)(void* frame, int32_t level) = nullptr; // 0x4910A0(frame, level, 1)
        void (*rerunPlatePass)() = nullptr;                          // 0x725890(*0xB7436C)
        uint64_t (*tokenGuid)(const char* token) = nullptr;          // 0x60C1C0
        void (*pushPlate)(lua_State* L, void* frame) = nullptr;      // 0x819880 if unregistered, then its +8 ref
        void (*setRangeSq)(float rangeSq) = nullptr;                 // 0xADAA7C
    };

    Deps& Dependencies();

    // Registry (FUN_102c0bc0 / FUN_102c0d80 / FUN_102bf3c0).
    void AddPlate(void* unit);
    void RemovePlate(void* unit);
    uint32_t IdForUnit(const void* unit);
    void* UnitForToken(const char* token);   // FUN_10a4ea40; nullptr when the token is not a registered plate
    void ClearPlates();                      // LAB_102bd620
    size_t Count();

    // FUN_102c29a0 (after 0x403340).
    void Tick();

    // The detours' logic.
    void AfterPlateUpdate(void* frame, uint64_t plateGuid);   // FUN_102be570
    void AfterPlatePass();                                    // FUN_102be880

    // FUN_102c2630: what ShouldShowNamePlate does before (or instead of) the client's checks.
    struct ShowQuery {
        bool platesShown = false;      // *0xAC80A8
        bool havePlayer = false;
        bool isActivePlayer = false;   // unit == player
        int32_t reaction = 0;          // 0x7251C0(player, unit)
        bool inCombat = false;         // descriptor +0xEC bit 19
        bool isLockedTarget = false;   // the unit's GUID == 0xBD07B0
        float playerPos[3] = { 0.0f, 0.0f, 0.0f };   // vtable +0x2C
        float unitPos[3] = { 0.0f, 0.0f, 0.0f };
    };
    enum class Show {
        Original,         // the client's checks decide
        None,             // 0
        PersonalName,     // vtable +0x20 projected, 1
        PersonalFeet,     // vtable +0x2C projected, y - 0.04, 1
    };
    Show BeforeShow(const ShowQuery& q);

    // Plate pointer low-byte tests (sub_102beb70 / FUN_102beb40).
    void AfterPlateCreate(void* unit, uintptr_t frameBefore);
    void AfterPlateRelease(void* unit, uintptr_t frameAfter);
    // sub_102be530 / FUN_102c2e90: the plate is the player's own.
    bool IsOwnPlate(uint64_t plateGuid, uint64_t activePlayer);

    float VerticalOffset();   // DAT_10bcbbf4
    int32_t PersonalPosition();
    bool ShowNeutral();

    bool OnVerticalOffset(CVar* var, const char* oldValue, const char* value, void* arg);    // sub_102bf350
    bool OnDistance(CVar* var, const char* oldValue, const char* value, void* arg);          // FUN_102bebc0
    bool OnAngle(CVar* var, const char* oldValue, const char* value, void* arg);             // sub_102beba0
    bool OnShowNeutral(CVar* var, const char* oldValue, const char* value, void* arg);       // sub_102bf330
    bool OnShowDead(CVar* var, const char* oldValue, const char* value, void* arg);          // FUN_102bee60
    bool OnPersonalPosition(CVar* var, const char* oldValue, const char* value, void* arg);  // sub_102bee20

    int GetNamePlateForUnit(lua_State* L);   // FUN_102bf780

    void Init();
    // Tests only.
    void Reset();
}

#endif
