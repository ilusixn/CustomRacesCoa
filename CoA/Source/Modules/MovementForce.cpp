// AscMovementForce.cpp's forces, packets and hook logic (see MovementForce.h); the client half is MovementForceClient.cpp.
#include "CoA/Source/Modules/MovementForce.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

namespace {
    struct Force {   // map node +0x0C..+0x28
        uint32_t id = 0;
        float x = 0.0f, y = 0.0f, z = 0.0f;
        uint32_t filter = 0;     // 0, or the movement's transport (+8) it applies to
        uint32_t type = 0;       // 1 = point
        float magnitude = 0.0f;  // yards per second
        uint32_t counter = 0;    // 0x761's counter; 0 from 0x765
    };

    struct Smooth {   // map node +0x10..+0x57
        float pos[3] = {}, anchor[3] = {};     // +0x10 / +0x1C: where the update put the unit
        float dpos[3] = {}, danchor[3] = {};   // +0x28 / +0x34: the jump it made
        int32_t start = 0;                     // +0x40
        uint32_t interval = 0;                 // +0x44: ms to spread the jump over
        int32_t lastLocal = 0;                 // +0x48
        int32_t lastServer = 0;                // +0x4C
        float progress = 0.0f;                 // +0x50
        bool active = false;                   // +0x54
    };

    std::unordered_map<uint64_t, std::unordered_map<uint32_t, Force>> s_forces;   // 0x10be33c4
    std::unordered_map<uint64_t, Smooth> s_smooth;                                // 0x10be33f0
    std::unordered_set<uint64_t> s_stopped;                                       // 0x10be3410
    bool s_stepIdle = false;       // 0x10be33e4
    uint64_t s_stepGuid = 0;       // 0x10be33e8
    int32_t s_lastPlayerMove = 0;  // 0x10be3430
    int32_t s_lastHeartbeat = 0;   // 0x10be3434
    int32_t s_lastFrame = 0;       // 0x10be3438
    CoA::MovementForce::Deps s_deps;

    constexpr uint32_t kMoving = 0xC0100F;
    constexpr uint32_t kMovingOrFalling = 0xC0180F;

    uint64_t PlayerGuid() {
        return s_deps.activeMover ? s_deps.activeMover() : 0;
    }

    int32_t Now() {
        return s_deps.now ? s_deps.now() : 0;
    }

    void* MovementOf(uint64_t guid) {
        return s_deps.movementOf ? s_deps.movementOf(guid) : nullptr;
    }

    uint32_t& Flags(void* m) {
        return *s_deps.flags(m);
    }

    float* Pos(void* m) {
        return s_deps.position(m);
    }

    float* Anchor(void* m) {
        return s_deps.anchor(m);
    }

    void* Controller(void* m) {
        return s_deps.controller ? s_deps.controller(m) : nullptr;
    }

    uint64_t ControllerGuid(void* c) {
        return s_deps.objectGuid(c);
    }

    void ClearStep(void* m) {
        if (s_deps.updateAnchors) {
            s_deps.updateAnchors(m);
        }
    }

    void StopMove(void* c) {
        if (s_deps.stopAnimation) {
            s_deps.stopAnimation(c);
        }
    }

    // FUN_102ba0f0: whose control the movement answers to -- the step in progress, the unit's controller when it is of
    // kind 3 or 4, or the player when it is the player's own movement or the player controls it.
    uint64_t ControlGuid(void* m) {
        if (s_stepGuid) {
            return s_stepGuid;
        }

        if (!m) {
            return 0;
        }

        void* c = Controller(m);

        if (c) {
            const uint32_t kind = s_deps.typeId(c);

            if ((kind == 3 || kind == 4) && ControllerGuid(c)) {
                return ControllerGuid(c);
            }
        }

        const uint64_t player = PlayerGuid();

        if (player) {
            if (MovementOf(player) == m) {
                return player;
            }

            if (Controller(m) && s_deps.isActiveMover && s_deps.isActiveMover(Controller(m))) {
                return player;
            }
        }

        return 0;
    }

    bool IsForced(void* m) {   // FUN_102b97c0
        if (s_forces.empty()) {
            return false;
        }

        const uint64_t guid = ControlGuid(m);

        if (!guid) {
            return false;
        }

        auto it = s_forces.find(guid);
        return it != s_forces.end() && !it->second.empty();
    }

    // FUN_102b8800: the forces' displacement over dtMs, added to out.
    void Displacement(void* m, int32_t dtMs, float* out) {
        if (!m || !out || dtMs <= 0) {
            return;
        }

        const uint64_t guid = ControlGuid(m);

        if (!guid) {
            return;
        }

        auto it = s_forces.find(guid);
        const float seconds = static_cast<float>(dtMs) * 0.001f;

        if (it == s_forces.end() || !(seconds > 0.0f)) {
            return;
        }

        float sx = 0.0f, sy = 0.0f, sz = 0.0f;

        for (const auto& kv : it->second) {
            const Force& f = kv.second;

            if (!(f.magnitude > 0.0f) || (f.filter != 0 && f.filter != s_deps.transportLow(m))) {
                continue;
            }

            float dx = f.x, dy = f.y, dz = f.z;

            if (f.type == 1) {
                dx -= Pos(m)[0];
                dy -= Pos(m)[1];
                dz -= Pos(m)[2];
            }

            const float len2 = dy * dy + dx * dx + dz * dz;

            if (len2 < 1e-06f) {
                continue;
            }

            const float len = static_cast<float>(sqrt(static_cast<double>(len2)));
            float step = f.magnitude * seconds;

            if (f.type == 1 && step > len) {
                step = len;   // a point is reached, never overshot
            }

            const float k = step / len;
            sz = k * dz + sz;
            sx = k * dx + sx;
            sy = k * dy + sy;
        }

        if (sx != 0.0f || sy != 0.0f || sz != 0.0f) {
            out[0] = sx + out[0];
            out[1] = sy + out[1];
            out[2] = sz + out[2];
        }
    }

    // FUN_102ba610: a unit the player controls (not the player) was stepped; track whether it stood still, telling the
    // controller to stop when that changes.
    void TrackStopped(void* m, uint64_t guid, bool idle) {
        if (!m || guid == PlayerGuid()) {
            return;
        }

        void* c = Controller(m);

        if (!c || ControllerGuid(c) != guid) {
            return;
        }

        const bool changed = idle ? s_stopped.insert(guid).second : s_stopped.erase(guid) != 0;

        if (changed) {
            StopMove(c);
        }
    }

    // FUN_102ba4a0: advance the movement by dt with the forces applied (the plot hook sees s_stepIdle and replaces the
    // step by the displacement).
    void Step(void* m, uint64_t guid, int32_t now, uint32_t dt) {
        if (!m || !dt) {
            return;
        }

        const bool isPlayer = guid && guid == PlayerGuid();
        ClearStep(m);
        const uint32_t saved = Flags(m);
        const bool idle = (saved & kMoving) == 0;
        s_stepIdle = idle;
        s_stepGuid = guid;

        if (idle) {
            Flags(m) = saved | 1;

            if (s_deps.applyMovement) {
                s_deps.applyMovement(m, now, dt);
            }

            if (Controller(m) && !isPlayer && s_deps.onMoveUpdate) {
                s_deps.onMoveUpdate(Controller(m), now);
            }
        } else if (s_deps.applyMovement) {
            s_deps.applyMovement(m, now, dt);
        }

        s_stepGuid = 0;
        s_stepIdle = false;
        Flags(m) = saved;
        TrackStopped(m, guid, idle);
    }

    // FUN_102ba590: the player's own movement, when the player's controller is the player.
    void TrackPlayerStopped(void* m, uint64_t guid, bool idle) {
        if (!m || guid != PlayerGuid()) {
            return;
        }

        void* c = Controller(m);

        if (!c || ControllerGuid(c) != guid) {
            return;
        }

        const bool changed = idle ? s_stopped.insert(guid).second : s_stopped.erase(guid) != 0;

        if (changed) {
            StopMove(c);
        }
    }

    // FUN_102ba180: a position update just moved the unit; spread the jump over the time since the last one (server
    // time when it is newer, else local; 50..1000 ms). False when the jump is too small or too far to smooth.
    bool BeginSmooth(uint64_t guid, void* m, const float* oldPos, const float* oldAnchor, int32_t serverTime) {
        if (!m || !guid) {
            return false;
        }

        const int32_t now = Now();

        if (now < 1) {
            return false;
        }

        Smooth& s = s_smooth[guid];   // FUN_102b8000
        uint32_t interval = 0x80;
        bool clamp = true;

        if (s.lastServer >= 1 && serverTime > s.lastServer) {
            interval = static_cast<uint32_t>(serverTime - s.lastServer);
        } else if (s.lastLocal >= 1 && now > s.lastLocal) {
            interval = static_cast<uint32_t>(now - s.lastLocal);
        } else {
            clamp = false;
        }

        if (clamp) {
            interval = interval < 0x32 ? 0x32 : interval > 1000 ? 1000 : interval;
        }

        s.start = now;
        s.interval = interval;
        s.lastLocal = now;
        s.progress = 0.0f;

        if (serverTime > 0) {
            s.lastServer = serverTime;
        }

        memcpy(s.pos, Pos(m), 12);
        memcpy(s.anchor, Anchor(m), 12);

        for (int i = 0; i < 3; ++i) {
            s.dpos[i] = Pos(m)[i] - oldPos[i];
            s.danchor[i] = Anchor(m)[i] - oldAnchor[i];
        }

        const float dist2 = s.dpos[1] * s.dpos[1] + s.dpos[0] * s.dpos[0] + s.dpos[2] * s.dpos[2];
        s.active = true;
        const float reach = static_cast<float>(static_cast<double>(interval)) * 0.001f * 60.0f;

        if (0.0025f < dist2 && dist2 <= reach * reach + 9.536743e-07f) {
            return true;
        }

        s.active = false;
        return false;
    }

    // FUN_102b8a40: move the unit along the kept jump to where it should be by now.
    void ApplySmooth(void* m, Smooth& s, int32_t now) {
        if (!m || !s.active || static_cast<int32_t>(s.interval) <= 0) {
            return;
        }

        const int32_t elapsed = now - s.start < 0 ? 0 : now - s.start;
        auto add = [&](float k) {
            for (int i = 0; i < 3; ++i) {
                Pos(m)[i] = k * s.dpos[i] + Pos(m)[i];
                Anchor(m)[i] = k * s.danchor[i] + Anchor(m)[i];
            }
        };

        if (static_cast<int32_t>(s.interval) <= elapsed) {
            const float rest = 1.0f - s.progress;

            if (0.0f < rest) {
                add(rest);
            }

            s.active = false;
            s.progress = 0.0f;
            memcpy(Pos(m), s.pos, 12);
            memcpy(Anchor(m), s.anchor, 12);
            return;
        }

        float t = static_cast<float>(elapsed) / static_cast<float>(static_cast<int32_t>(s.interval));
        t = t < 0.0f ? 0.0f : t;
        t = t > 1.0f ? 1.0f : t;
        float step = t - s.progress;
        step = step < 0.0f ? 0.0f : step;
        step = step > 1.0f ? 1.0f : step;

        if (0.0f < step) {
            add(step);
        }

        s.progress = t;
    }

    // ---- packets --------------------------------------------------------------------------------------------------
    uint64_t ReadGuid(CDataStore* msg) {   // 0x76DC20
        const uint8_t mask = CoA::Read::U8(msg);
        uint64_t guid = 0;

        for (uint32_t bit = 0; bit < 8; ++bit) {
            if (mask & (1u << bit)) {
                guid |= static_cast<uint64_t>(CoA::Read::U8(msg)) << (bit * 8);
            }
        }

        return guid;
    }

    Force ReadForce(CDataStore* msg) {   // FUN_102ba040
        Force f;
        f.id = CoA::Read::U32(msg);
        f.x = CoA::Read::F32(msg);
        f.y = CoA::Read::F32(msg);
        f.z = CoA::Read::F32(msg);
        f.filter = CoA::Read::U32(msg);
        f.magnitude = CoA::Read::F32(msg);
        f.type = CoA::Read::U32(msg) == 1 ? 1 : 0;
        return f;
    }

    // FUN_102ba320: {packed guid, u32 counter} and the unit's movement block (0x7164B0), stamped with the movement clock
    // (0x6E9230, else the tick count).
    void SendAck(uint32_t opcode, uint64_t guid, uint32_t counter) {
        void* m = MovementOf(guid);

        if (!m) {
            return;
        }

        CoA::Packet pkt(opcode);
        pkt.PackedGuid(guid).U32(counter);
        int32_t time = s_deps.idleTime ? static_cast<int32_t>(s_deps.idleTime()) : 0;

        if (time == 0) {
            time = Now();
        }

        if (s_deps.writeMovement) {
            s_deps.writeMovement(m, opcode, time, pkt.Store());
        }

        pkt.Send();
    }

    // The movement info 0x765 / 0x766 carry, applied when the unit exists (not to the player). False when the packet
    // ran out.
    bool ReadAndApplyMovement(CDataStore* msg, uint64_t guid, void*& m) {
        m = (guid && guid == PlayerGuid()) ? nullptr : MovementOf(guid);
        return s_deps.readMovement && s_deps.readMovement(msg, m);
    }

    // The common tail of 0x762 / 0x766 once the unit's last force is gone.
    void ForgetUnit(uint64_t guid, bool isPlayer) {
        s_smooth.erase(guid);

        if (isPlayer) {
            s_stopped.erase(guid);
        }
    }
}

CoA::MovementForce::Deps& CoA::MovementForce::Dependencies() {
    return s_deps;
}

// 0x761. A packet shorter than its fields is ignored (7a guard): no force and no ack.
int32_t CoA::MovementForce::OnApply(void*, uint32_t, uint32_t, CDataStore* msg) {
    const uint64_t guid = ReadGuid(msg);
    const uint32_t counter = CoA::Read::U32(msg);
    Force f = ReadForce(msg);

    if (CoA::Read::Failed(msg)) {
        return 1;
    }

    f.counter = counter;
    s_forces[guid][f.id] = f;

    if (guid == PlayerGuid()) {
        if (void* m = MovementOf(guid)) {
            ClearStep(m);
        }
    }

    SendAck(0x763, guid, counter);
    return 1;
}

// 0x765. The movement info is applied as soon as it is read; a force cut short is ignored (7a guard).
int32_t CoA::MovementForce::OnApplyOther(void*, uint32_t, uint32_t, CDataStore* msg) {
    const uint64_t guid = ReadGuid(msg);

    if (CoA::Read::Failed(msg)) {
        return 1;
    }

    void* m = nullptr;

    if (!ReadAndApplyMovement(msg, guid, m)) {
        return 1;
    }

    Force f = ReadForce(msg);

    if (CoA::Read::Failed(msg)) {
        return 1;
    }

    f.counter = 0;
    s_forces[guid][f.id] = f;

    if (m) {
        ClearStep(m);
    }

    return 1;
}

// 0x762: the stop the controller would get is gated on a second erase of the same guid from the stopped set right after
// the first, so it never fires (the original's bug, kept). A packet shorter than its fields is ignored (7a guard).
int32_t CoA::MovementForce::OnRemove(void*, uint32_t, uint32_t, CDataStore* msg) {
    const uint64_t guid = ReadGuid(msg);
    const uint32_t counter = CoA::Read::U32(msg);
    const uint32_t id = CoA::Read::U32(msg);

    if (CoA::Read::Failed(msg)) {
        return 1;
    }

    auto it = s_forces.find(guid);

    if (it == s_forces.end()) {
        SendAck(0x764, guid, counter);
        return 1;
    }

    it->second.erase(id);

    if (it->second.empty()) {
        const bool isPlayer = guid == PlayerGuid();
        s_forces.erase(it);
        ForgetUnit(guid, isPlayer);
        void* m = MovementOf(guid);

        if (m && (isPlayer || s_stopped.erase(guid))) {
            void* c = Controller(m);

            if (c && ControllerGuid(c) == guid && s_stopped.erase(guid)) {
                StopMove(c);
            }
        }
    }

    if (guid == PlayerGuid()) {
        if (void* m = MovementOf(guid)) {
            ClearStep(m);
        }
    }

    SendAck(0x764, guid, counter);
    return 1;
}

// 0x766 (the same never-firing stop as 0x762).
int32_t CoA::MovementForce::OnRemoveOther(void*, uint32_t, uint32_t, CDataStore* msg) {
    const uint64_t guid = ReadGuid(msg);

    if (CoA::Read::Failed(msg)) {
        return 1;
    }

    void* m = nullptr;

    if (!ReadAndApplyMovement(msg, guid, m)) {
        return 1;
    }

    const uint32_t id = CoA::Read::U32(msg);

    if (CoA::Read::Failed(msg)) {
        return 1;
    }

    auto it = s_forces.find(guid);

    if (it == s_forces.end()) {
        return 1;
    }

    it->second.erase(id);

    if (!it->second.empty()) {
        return 1;
    }

    const bool isPlayer = guid == PlayerGuid();
    s_forces.erase(it);
    ForgetUnit(guid, isPlayer);

    if (!m) {
        if (guid) {
            m = MovementOf(guid);
        }

        if (!m) {
            return 1;
        }
    }

    if (!isPlayer && !s_stopped.erase(guid)) {
        return 1;
    }

    void* c = Controller(m);

    if (!c || ControllerGuid(c) != guid) {
        return 1;
    }

    if (s_stopped.erase(guid)) {   // never true: erased just above
        StopMove(c);
    }

    return 1;
}

void CoA::MovementForce::AfterExecuteMovement(void* m, uint32_t time) {
    if (!m) {
        return;
    }

    const uint64_t guid = ControlGuid(m);

    if (guid && guid == PlayerGuid() && (Flags(m) & kMoving)) {
        s_lastPlayerMove = static_cast<int32_t>(time);
    }
}

int32_t CoA::MovementForce::AfterPlot(void* m, uint32_t dt, float* out, int32_t r) {
    if (!m || !IsForced(m) || !out || static_cast<int32_t>(dt) <= 0 || (Flags(m) & 0x800)) {
        return r;
    }

    float d[3] = { 0.0f, 0.0f, 0.0f };
    Displacement(m, static_cast<int32_t>(dt), d);

    if (d[0] != 0.0f || d[1] != 0.0f || d[2] != 0.0f) {
        if (!s_stepIdle) {
            d[0] += out[0];
            d[1] += out[1];
            d[2] += out[2];
        }

        out[2] = d[2];
        out[1] = d[1];
        out[0] = d[0];
    }

    return r ? r : 1;
}

CoA::MovementForce::StatusUpdate CoA::MovementForce::BeforeStatus(void* m, uint32_t statusFlags) {
    StatusUpdate u;

    if (m && IsForced(m)) {
        u.guid = ControlGuid(m);

        if (u.guid && u.guid != PlayerGuid()) {
            if ((statusFlags & kMovingOrFalling) == 0) {
                memcpy(u.oldPos, Pos(m), 12);
                memcpy(u.oldAnchor, Anchor(m), 12);
                u.keep = true;
            } else {
                auto it = s_smooth.find(u.guid);   // FUN_102bb070

                if (it != s_smooth.end()) {
                    it->second.active = false;
                }
            }
        }
    }

    return u;
}

void CoA::MovementForce::AfterStatus(const StatusUpdate& u, void* m, int32_t serverTime) {
    if (u.keep && m && u.guid && BeginSmooth(u.guid, m, u.oldPos, u.oldAnchor, serverTime)) {
        memcpy(Pos(m), u.oldPos, 12);
        memcpy(Anchor(m), u.oldAnchor, 12);
    }
}

bool CoA::MovementForce::BeforeCollide(void* m, uint32_t& saved) {
    if (m && IsForced(m) && (Flags(m) & kMovingOrFalling) == 0) {
        saved = Flags(m);
        Flags(m) = saved | 1;
        return true;
    }

    return false;
}

void CoA::MovementForce::AfterCollide(void* m, bool flagged, uint32_t saved) {
    if (flagged) {
        Flags(m) = saved;
    }
}

void CoA::MovementForce::Reset() {
    s_forces.clear();
    s_smooth.clear();
    s_stopped.clear();
    s_stepIdle = false;
    s_stepGuid = 0;
    s_lastHeartbeat = 0;
    s_lastFrame = 0;
    s_lastPlayerMove = 0;
}

void CoA::MovementForce::Tick() {
    const int32_t now = Now();

    if (now <= 0) {
        return;
    }

    uint32_t dt = 0;

    if (s_lastFrame > 0 && now > s_lastFrame) {
        dt = static_cast<uint32_t>(now - s_lastFrame);
    }

    s_lastFrame = now;

    if (dt > 100) {
        dt = 100;
    } else if (dt == 0) {
        return;
    }

    const uint64_t player = PlayerGuid();

    if (player) {
        if (void* m = MovementOf(player)) {
            if (!IsForced(m)) {
                void* c = Controller(m);

                if (player == PlayerGuid() && c && ControllerGuid(c) == player && s_stopped.erase(player)) {
                    StopMove(c);
                }
            } else {
                const uint32_t flags = Flags(m);
                TrackPlayerStopped(m, player, (flags & kMovingOrFalling) == 0);

                if ((flags & kMovingOrFalling) == 0 && s_lastPlayerMove != now) {
                    float d[3] = { 0.0f, 0.0f, 0.0f };
                    Displacement(m, static_cast<int32_t>(dt), d);
                    Step(m, player, now, dt);

                    if ((d[0] != 0.0f || d[1] != 0.0f || d[2] != 0.0f) && now - s_lastHeartbeat >= 0x80 && Controller(m)) {
                        // MSG_MOVE_HEARTBEAT (0xEE) through the controller.
                        if (s_deps.heartbeat) {
                            s_deps.heartbeat(Controller(m), now);
                        }

                        s_lastHeartbeat = now;
                    }
                }
            }
        }
    }

    if (s_forces.empty()) {
        return;
    }

    for (auto it = s_forces.begin(); it != s_forces.end();) {
        const uint64_t guid = it->first;

        if (!guid || it->second.empty()) {
            s_smooth.erase(guid);
            s_stopped.erase(guid);
            it = s_forces.erase(it);   // FUN_102b8610
            continue;
        }

        if (guid == player) {
            ++it;
            continue;
        }

        void* m = MovementOf(guid);

        if (!m) {
            s_smooth.erase(guid);
            s_stopped.erase(guid);
            it = s_forces.erase(it);
            continue;
        }

        if (Flags(m) & 0x800) {
            ++it;
            continue;
        }

        const uint32_t moving = Flags(m) & kMoving;
        void* c = Controller(m);

        if (guid == PlayerGuid() || !c || ControllerGuid(c) != guid) {
            if (!moving) {
                Step(m, guid, now, dt);
            }
        } else if (!moving) {
            if (s_stopped.insert(guid).second) {
                StopMove(c);
            }

            Step(m, guid, now, dt);
        } else if (s_stopped.erase(guid)) {
            StopMove(c);
        }

        auto s = s_smooth.find(guid);

        if (s != s_smooth.end()) {
            ApplySmooth(m, s->second, now);
        }

        ++it;
    }
}

uint32_t CoA::MovementForce::ForceCount(uint64_t guid) {
    auto it = s_forces.find(guid);
    return it != s_forces.end() ? static_cast<uint32_t>(it->second.size()) : 0;
}

bool CoA::MovementForce::Smoothing(uint64_t guid) {
    auto it = s_smooth.find(guid);
    return it != s_smooth.end() && it->second.active;
}

bool CoA::MovementForce::Stopped(uint64_t guid) {
    return s_stopped.count(guid) != 0;
}

bool CoA::MovementForce::Forced(void* m) {
    return IsForced(m);
}

void CoA::MovementForce::Clear() {
    Reset();
    s_deps = Deps();
}

// FUN_102b9830. The detours are the decomp sites' calls (MovementForceClient.cpp).
void CoA::MovementForce::Init() {
    CoA::ClientHooks::leaveWorld.Add(CoA::MovementForce::Reset);
    CoA::ClientHooks::beforeShutdown.Add(CoA::MovementForce::Reset);
    CoA::ClientHooks::afterClientDestroy.Add(CoA::MovementForce::Reset);
    COA_PACKET(0x761, CoA::MovementForce::OnApply);
    COA_PACKET(0x762, CoA::MovementForce::OnRemove);
    COA_PACKET(0x765, CoA::MovementForce::OnApplyOther);
    COA_PACKET(0x766, CoA::MovementForce::OnRemoveOther);
}
