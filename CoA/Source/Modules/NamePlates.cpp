// AscNamePlates.cpp (see NamePlates.h): the nameplate registry, its CVars and the detours' logic.
#include "CoA/Source/Modules/NamePlates.h"
#include "CoA/Source/Modules/GlobalsG.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>

namespace {
    CoA::NamePlates::Deps s_deps;

    // The slots 0x10BE348C .. 0x10BE34B8, refilled at every world registration.
    CVar* s_verticalOffsetVar = nullptr;   // 0x10BE348C
    CVar* s_fixedOffset = nullptr;         // 0x10BE3490
    CVar* s_distance = nullptr;            // 0x10BE3494
    CVar* s_angle = nullptr;               // 0x10BE3498
    CVar* s_opacity = nullptr;             // 0x10BE349C
    CVar* s_useCamera = nullptr;           // 0x10BE34A0
    CVar* s_fadeIn = nullptr;              // 0x10BE34A4
    CVar* s_showNeutralVar = nullptr;      // 0x10BE34A8
    CVar* s_showDead = nullptr;            // 0x10BE34AC
    CVar* s_showPersonal = nullptr;        // 0x10BE34B0
    CVar* s_personalPositionVar = nullptr; // 0x10BE34B4
    CVar* s_inCombatOnly = nullptr;        // 0x10BE34B8

    float s_verticalOffset = 0.6666667f;   // DAT_10bcbbf4 (0x3F2AAAAA), read by the client at 0x715739
    int32_t s_personalPosition = 2;        // DAT_10bcbbf8
    bool s_showNeutral = true;             // DAT_10bcbbfc

    // 0x10BE3454: id -> unit. The original's 8-byte {id, unit} entries become values (it never frees the ones the
    // leave-world clear drops; nothing can observe that).
    std::unordered_map<uint32_t, void*> s_plates;
    std::vector<uint32_t> s_addedIds;      // 0x10BE34BC

    int32_t Int(const CVar* var) {
        return var ? var->m_intValue : 0;
    }

    float Float(const CVar* var) {
        return var ? var->m_floatValue : 0.0f;
    }

    // The original's inline distance: each axis difference squared through pow (in double), summed in float as
    // dy + dx + dz, the square root in double.
    float Distance(const float* a, const float* b) {
        const float dz = static_cast<float>(std::pow(static_cast<double>(a[2] - b[2]), 2.0));
        const float dx = static_cast<float>(std::pow(static_cast<double>(a[0] - b[0]), 2.0));
        const float dy = static_cast<float>(std::pow(static_cast<double>(a[1] - b[1]), 2.0));
        return static_cast<float>(std::sqrt(static_cast<double>(dy + dx + dz)));
    }

    std::string PlateToken(uint32_t id) {
        return "nameplate" + std::to_string(id);
    }

    uint64_t ActivePlayerGuid() {
        CoA::NamePlates::Body body;
        return s_deps.player && s_deps.player(body) ? body.guid : 0;
    }

    uint64_t LockedTarget() {
        return s_deps.lockedTarget ? s_deps.lockedTarget() : 0;
    }

    uint8_t AlphaByte(float x) {
        return static_cast<uint8_t>(static_cast<int>(x));
    }

    void SetAlpha(void* frame, uint32_t alpha) {
        if (s_deps.setAlpha) {
            s_deps.setAlpha(frame, alpha);
        }
    }

    void ShowFrame(void* frame, bool shown) {
        if (s_deps.showPlate) {
            s_deps.showPlate(frame, shown);
        }
    }

    struct PlateOrder {   // 0x18 bytes, FUN_102c0a70
        float distance;
        uint64_t guid;
        void* plate;
    };
}

CoA::NamePlates::Deps& CoA::NamePlates::Dependencies() {
    return s_deps;
}

// FUN_102c0bc0: the lowest free id from 1 up, a new entry, and the id queued for NAME_PLATE_UNIT_ADDED.
void CoA::NamePlates::AddPlate(void* unit) {
    uint32_t id = 1;

    do {
        if (s_plates.find(id) == s_plates.end()) {
            break;
        }

        ++id;
    } while (id != 0xFFFFFFFF);

    s_plates[id] = unit;
    s_addedIds.push_back(id);
}

// FUN_102c0d80: NAME_PLATE_UNIT_REMOVED("nameplateN") for the unit's first entry, which is then erased.
void CoA::NamePlates::RemovePlate(void* unit) {
    const uint32_t id = CoA::NamePlates::IdForUnit(unit);

    if (id == 0) {
        return;
    }

    CoA::Events::Signal("NAME_PLATE_UNIT_REMOVED", "%s", PlateToken(id).c_str());
    s_plates.erase(id);
}

void CoA::NamePlates::ForgetUnit(const void* unit) {
    for (auto it = s_plates.begin(); it != s_plates.end();) {
        it = it->second == unit ? s_plates.erase(it) : std::next(it);
    }
}

uint32_t CoA::NamePlates::IdForUnit(const void* unit) {
    for (const auto& kv : s_plates) {
        if (kv.second == unit) {
            return kv.first;
        }
    }

    return 0;
}

// FUN_10a4ea40: a token of at least 9 characters containing "nameplate" anywhere loses its first 9 characters; the
// rest is std::stoul(rest, nullptr, 0) (FUN_102bf410) and looked up. Ruling: stoul's invalid_argument / out_of_range
// would escape through the client's frames in the original; here such a token (and an empty rest) is not a plate.
void* CoA::NamePlates::UnitForToken(const char* token) {
    if (!token) {
        return nullptr;
    }

    std::string s(token);

    if (s.size() < 9 || s.find("nameplate") == std::string::npos) {
        return nullptr;
    }

    s.erase(0, 9);

    if (s.empty()) {
        return nullptr;
    }

    char* end = nullptr;
    errno = 0;
    const unsigned long id = strtoul(s.c_str(), &end, 0);

    if (end == s.c_str() || errno == ERANGE) {
        return nullptr;
    }

    auto it = s_plates.find(static_cast<uint32_t>(id));
    return it == s_plates.end() ? nullptr : it->second;
}

// LAB_102bd620 (list 0x10BE28AC, before 0x528C30): the registry is cleared; the queued ids stay.
void CoA::NamePlates::ClearPlates() {
    s_plates.clear();
}

size_t CoA::NamePlates::Count() {
    return s_plates.size();
}

// FUN_102c29a0: the queued NAME_PLATE_UNIT_ADDED events, then every plate's frame level (0x4910A0): 10, 20, ...
// farthest first, the target's last (FUN_102be160's order). Nothing without a player.
void CoA::NamePlates::Tick() {
    CoA::NamePlates::Body player;

    if (!s_deps.player || !s_deps.player(player)) {
        return;
    }

    const size_t queued = s_addedIds.size();

    for (size_t i = 0; i < queued && i < s_addedIds.size(); ++i) {
        CoA::Events::Signal("NAME_PLATE_UNIT_ADDED", "%s", PlateToken(s_addedIds[i]).c_str());
    }

    s_addedIds.clear();
    std::vector<PlateOrder> order;   // 0x10BE34C8

    if (s_deps.units) {
        for (const CoA::NamePlates::Unit& u : s_deps.units()) {
            if (u.plate) {
                order.push_back({ Distance(player.pos, u.pos), u.guid, u.plate });
            }
        }
    }

    if (order.empty()) {
        return;
    }

    const uint64_t target = LockedTarget();
    std::sort(order.begin(), order.end(), [target](const PlateOrder& a, const PlateOrder& b) {
        const bool aTarget = a.guid == target;
        const bool bTarget = b.guid == target;
        return !aTarget && (bTarget || a.distance > b.distance);
    });
    int32_t level = 10;

    for (const PlateOrder& o : order) {
        if (s_deps.setFrameLevel) {
            s_deps.setFrameLevel(o.plate, level);
        }

        level += 10;
    }
}

// FUN_102be570 after 0x98E9F0. With no target: fade by distance from the player's head (nameplateFadeIn 1) and dim to
// nameplateIntersectOpacity when the line from the player (or the camera's x / y, nameplateIntersectUseCamera 1) to
// the unit's mid-height is blocked. The target's and the player's own plates are always opaque.
void CoA::NamePlates::AfterPlateUpdate(void* frame, uint64_t plateGuid) {
    CoA::NamePlates::Body player;

    if (LockedTarget() == 0 && s_deps.player && s_deps.player(player)) {
        float head[3] = { player.pos[0], player.pos[1], player.height * 0.95f + player.pos[2] };
        float from[3] = { head[0], head[1], head[2] };
        float x = 0.0f;
        float y = 0.0f;

        if (s_useCamera && Int(s_useCamera) == 1 && s_deps.cameraXY && s_deps.cameraXY(x, y)) {
            from[0] = x;
            from[1] = y;
        }

        CoA::NamePlates::Body unit;

        if (s_deps.unitBody && s_deps.unitBody(plateGuid, 8, unit)) {
            const float at[3] = { unit.pos[0], unit.pos[1], unit.height * 0.5f + unit.pos[2] };

            if (s_fadeIn && Int(s_fadeIn) == 1) {
                const float d = Distance(head, at);
                const float range = s_distance ? Float(s_distance) : 43.0f;
                const float start = range - 13.5f;

                if (d < start) {
                    SetAlpha(frame, 0xFF);
                } else {
                    SetAlpha(frame, AlphaByte((1.0f - (d - start) / (range - start)) * 255.0f));
                }
            }

            if (s_opacity && s_deps.lineBlocked && s_deps.lineBlocked(from, at)) {
                SetAlpha(frame, AlphaByte(Float(s_opacity) * 255.0f));
            }
        }
    }

    if (plateGuid == LockedTarget()) {
        SetAlpha(frame, 0xFF);
    }

    if (plateGuid == ActivePlayerGuid()) {
        SetAlpha(frame, 0xFF);
    }
}

// FUN_102be880 after 0x725890: with nameplateAngle non-zero every plate is hidden beyond nameplateDistance (the CVar's
// raw value), or when further than 3 yards and outside nameplateAngle of the player's facing; shown otherwise (also
// when its unit is gone).
void CoA::NamePlates::AfterPlatePass() {
    if (!s_angle || !(Float(s_angle) != 0.0f)) {
        return;
    }

    CoA::NamePlates::Body player;

    if (!s_deps.player || !s_deps.player(player) || !s_deps.plates) {
        return;
    }

    for (const CoA::NamePlates::Plate& plate : s_deps.plates()) {
        CoA::NamePlates::Body unit;

        if (!s_deps.unitBody || !s_deps.unitBody(plate.guid, 0x18, unit)) {
            ShowFrame(plate.frame, true);
            continue;
        }

        if (Distance(player.pos, unit.pos) >= Float(s_distance)) {
            ShowFrame(plate.frame, false);
            continue;
        }

        float facing = player.facing;

        if (facing > 3.1415f) {
            facing = facing - 6.283f;
        }

        const float bearing = s_deps.bearing ? s_deps.bearing(player.pos, unit.pos) : 0.0f;
        const float delta = bearing - facing;
        ShowFrame(plate.frame, Distance(player.pos, unit.pos) <= 3.0f || std::fabs(delta) <= Float(s_angle));
    }
}

// FUN_102c2630 over 0x72B060. Nothing while the client's plates are off. The player's own plate (nameplateShowPersonal
// 1) is placed by nameplatePersonalPosition. Neutral units out of combat are skipped unless nameplateShowNeutral or
// targeted; nameplateInCombatOnly skips the rest out of combat. With nameplateFixedVerticalOffset the height offset
// shrinks 0.025 per yard.
CoA::NamePlates::Show CoA::NamePlates::BeforeShow(const ShowQuery& q) {
    if (!q.platesShown) {
        return Show::None;
    }

    if (!q.havePlayer) {
        return Show::Original;
    }

    if (Int(s_showPersonal) == 1 && q.isActivePlayer) {
        if (s_personalPosition == 1) {
            return Show::PersonalName;
        }

        if (s_personalPosition == 2) {
            return Show::PersonalFeet;
        }

        return Show::None;
    }

    if (!s_showNeutral && q.reaction == 3 && !q.inCombat && !q.isLockedTarget) {
        return Show::None;
    }

    if (Int(s_inCombatOnly) == 1 && !q.inCombat) {
        return Show::None;
    }

    if (s_verticalOffsetVar && s_fixedOffset && Int(s_fixedOffset) != 0) {
        s_verticalOffset = Float(s_verticalOffsetVar) - Distance(q.playerPos, q.unitPos) * 0.025f;
    }

    return Show::Original;
}

// sub_102beb70: only the low byte of the unit's plate pointer (+0xC38) before the original is tested.
void CoA::NamePlates::AfterPlateCreate(void* unit, uintptr_t frameBefore) {
    if (static_cast<uint8_t>(frameBefore) == 0) {
        CoA::NamePlates::AddPlate(unit);
    }
}

// FUN_102beb40: the low byte of the plate pointer after the original.
void CoA::NamePlates::AfterPlateRelease(void* unit, uintptr_t frameAfter) {
    if (static_cast<uint8_t>(frameAfter) == 0) {
        CoA::NamePlates::RemovePlate(unit);
    }
}

bool CoA::NamePlates::IsOwnPlate(uint64_t plateGuid, uint64_t activePlayer) {
    return plateGuid == activePlayer;
}

float CoA::NamePlates::VerticalOffset() {
    return s_verticalOffset;
}

int32_t CoA::NamePlates::PersonalPosition() {
    return s_personalPosition;
}

bool CoA::NamePlates::ShowNeutral() {
    return s_showNeutral;
}

bool CoA::NamePlates::OnVerticalOffset(CVar*, const char*, const char* value, void*) {
    s_verticalOffset = static_cast<float>(atof(value));
    return true;
}

// As SetNamePlateRange: 5..60 yards, stored squared (0xADAA7C). The default "1681" clamps to 60.
bool CoA::NamePlates::OnDistance(CVar*, const char*, const char* value, void*) {
    const float x = static_cast<float>(atof(value));
    const float lo = 5.0f <= x ? x : 5.0f;
    const float r = x <= 60.0f ? lo : 60.0f;
    CoA::GlobalsG::SetNamePlateRangeSq(r * r);
    return true;
}

bool CoA::NamePlates::OnAngle(CVar*, const char*, const char*, void*) {
    if (s_deps.rerunPlatePass) {
        s_deps.rerunPlatePass();
    }

    return true;
}

bool CoA::NamePlates::OnShowNeutral(CVar*, const char*, const char* value, void*) {
    s_showNeutral = atoi(value) != 0;
    return true;
}

// "1" NOPs the dead-unit skip at 0x72B07A (jle 0x72B0DB); anything else restores it.
bool CoA::NamePlates::OnShowDead(CVar*, const char*, const char* value, void*) {
    CoA::ClientHooks::options.namePlatesShowDead = strcmp(value, "1") == 0;
    return true;
}

bool CoA::NamePlates::OnPersonalPosition(CVar*, const char*, const char* value, void*) {
    s_personalPosition = atoi(value);

    if (s_personalPosition < 1) {
        s_personalPosition = 1;
    } else if (s_personalPosition > 2) {
        s_personalPosition = 2;
    }

    return true;
}

// FUN_102bf780: exactly one argument (a unit token), else nothing. The first enumerated unit with the token's GUID
// decides: its registry entry's current plate frame (registered with Lua first if it is not yet), or nothing.
int CoA::NamePlates::GetNamePlateForUnit(lua_State* L) {
    if (lua_gettop(L) != 1) {
        return 0;
    }

    const char* token = CoA::Script::CheckString(L, 1);
    const uint64_t guid = s_deps.tokenGuid ? s_deps.tokenGuid(token) : 0;

    if (!s_deps.units) {
        return 0;
    }

    for (const CoA::NamePlates::Unit& u : s_deps.units()) {
        if (u.guid != guid) {
            continue;
        }

        if (CoA::NamePlates::IdForUnit(u.unit) == 0 || !u.plate) {
            return 0;
        }

        if (s_deps.pushPlate) {
            s_deps.pushPlate(L, u.plate);
        }

        return 1;
    }

    return 0;
}

namespace {
    int GetNamePlateForUnitNative(lua_State* L) {
        return CoA::NamePlates::GetNamePlateForUnit(L);
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "GetNamePlateForUnit", GetNamePlateForUnitNative),
    };
}

void CoA::NamePlates::Init() {
    using namespace CoA::NamePlates;
    // FUN_102bf920 -> FUN_10114540 x12: no help, flags 0x21, category 4, world.
    // name, help, flags, default, callback, category, setCommand, slot, world
    COA_CVAR("nameplateVerticalOffset", nullptr, 0x21, "0.0", OnVerticalOffset, 4, false, &s_verticalOffsetVar, true);
    COA_CVAR("nameplateFixedVerticalOffset", nullptr, 0x21, "0", nullptr, 4, false, &s_fixedOffset, true);
    COA_CVAR("nameplateDistance", nullptr, 0x21, "1681", OnDistance, 4, false, &s_distance, true);
    COA_CVAR("nameplateAngle", nullptr, 0x21, "0.0", OnAngle, 4, false, &s_angle, true);
    COA_CVAR("nameplateIntersectOpacity", nullptr, 0x21, "0.5", nullptr, 4, false, &s_opacity, true);
    COA_CVAR("nameplateIntersectUseCamera", nullptr, 0x21, "0", nullptr, 4, false, &s_useCamera, true);
    COA_CVAR("nameplateFadeIn", nullptr, 0x21, "1", nullptr, 4, false, &s_fadeIn, true);
    COA_CVAR("nameplateShowNeutral", nullptr, 0x21, "1", OnShowNeutral, 4, false, &s_showNeutralVar, true);
    COA_CVAR("nameplateShowDead", nullptr, 0x21, "0", OnShowDead, 4, false, &s_showDead, true);
    COA_CVAR("nameplateShowPersonal", nullptr, 0x21, "0", nullptr, 4, false, &s_showPersonal, true);
    COA_CVAR("nameplatePersonalPosition", nullptr, 0x21, "2", OnPersonalPosition, 4, false, &s_personalPositionVar, true);
    COA_CVAR("nameplateInCombatOnly", nullptr, 0x21, "0", nullptr, 4, false, &s_inCombatOnly, true);

    // The installer's byte patches (see ClientHooks::OptionPatches).
    CoA::ClientHooks::options.namePlateTargetLevel = false;
    CoA::ClientHooks::options.namePlatesAnchored = true;
    CoA::ClientHooks::options.interactIconRaised = true;

    CoA::ClientHooks::leaveWorld.Add(CoA::NamePlates::ClearPlates);
    CoA::Lifecycle::OnAfterIdle(CoA::NamePlates::Tick);
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    COA_EVENT("NAME_PLATE_UNIT_ADDED");
    COA_EVENT("NAME_PLATE_UNIT_REMOVED");
}

void CoA::NamePlates::Reset() {
    s_deps = Deps();
    s_plates.clear();
    s_addedIds.clear();
    s_verticalOffset = 0.6666667f;
    s_personalPosition = 2;
    s_showNeutral = true;
    CoA::ClientHooks::options = CoA::ClientHooks::OptionPatches();

    for (CVar** slot : { &s_verticalOffsetVar, &s_fixedOffset, &s_distance, &s_angle, &s_opacity, &s_useCamera, &s_fadeIn,
             &s_showNeutralVar, &s_showDead, &s_showPersonal, &s_personalPositionVar, &s_inCombatOnly }) {
        *slot = nullptr;
    }
}
