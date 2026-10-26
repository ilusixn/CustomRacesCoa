#include "CoA/Source/Modules/MovementForce.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace {
    struct FakeUnit;

    struct FakeMove {
        uint32_t flags = 0;
        float pos[3] = {};
        float anchor[3] = {};
        uint32_t transport = 0;
        FakeUnit* unit = nullptr;
    };

    struct FakeUnit {
        uint64_t guid = 0;
        uint32_t type = 3;
        FakeMove move;
    };

    std::map<uint64_t, FakeUnit> s_units;
    std::vector<std::string> s_calls;
    uint64_t s_mover = 0;
    int32_t s_now = 1000;
    uint32_t s_idle = 0;
    constexpr uint64_t kPlayer = 0x0000000000000011ull;
    constexpr uint64_t kOther = 0xF130000000000022ull;
    constexpr uint64_t kPet = 0xF140000000000033ull;

    FakeMove* M(void* m) {
        return static_cast<FakeMove*>(m);
    }

    FakeUnit& AddUnit(uint64_t guid, uint32_t type) {
        FakeUnit& u = s_units[guid];
        u.guid = guid;
        u.type = type;
        u.move.unit = &u;
        return u;
    }

    bool Near(float a, float b) {
        return std::fabs(a - b) < 0.001f;
    }

    void Start() {
        CoA::Packets::Clear();
        CoA::ClientHooks::Reset();
        CoA::ClientHooks::leaveWorld.Clear();
        CoA::ClientHooks::beforeShutdown.Clear();
        CoA::ClientHooks::afterClientDestroy.Clear();
        CoA::MovementForce::Clear();
        CoA::MovementForce::Init();
        s_units.clear();
        s_calls.clear();
        s_mover = kPlayer;
        s_now = 1000;
        s_idle = 0;
        CoATest::RecordPackets();

        CoA::MovementForce::Deps& d = CoA::MovementForce::Dependencies();
        d.movementOf = [](uint64_t guid) -> void* {
            auto it = s_units.find(guid);
            return it != s_units.end() ? &it->second.move : nullptr;
        };
        d.activeMover = [] { return s_mover; };
        d.now = [] { return s_now; };
        d.idleTime = [] { return s_idle; };
        d.flags = [](void* m) { return &M(m)->flags; };
        d.position = [](void* m) { return M(m)->pos; };
        d.anchor = [](void* m) { return M(m)->anchor; };
        d.transportLow = [](void* m) { return M(m)->transport; };
        d.controller = [](void* m) -> void* { return M(m)->unit; };
        d.objectGuid = [](void* u) { return static_cast<FakeUnit*>(u)->guid; };
        d.typeId = [](void* u) { return static_cast<FakeUnit*>(u)->type; };
        d.isActiveMover = [](void* u) { return static_cast<FakeUnit*>(u)->guid == s_mover; };
        d.updateAnchors = [](void* m) { s_calls.push_back("anchors:" + std::to_string(M(m)->unit->guid & 0xFF)); };
        // The client's step: PlotUnitMovement (hooked) gives the move, which is added to the position.
        d.applyMovement = [](void* m, int32_t, uint32_t dt) {
            s_calls.push_back("apply:" + std::to_string(dt) + ":" + std::to_string(M(m)->flags));
            float out[3] = {};
            CoA::MovementForce::AfterPlot(m, dt, out, 0);
            for (int i = 0; i < 3; ++i) {
                M(m)->pos[i] += out[i];
            }
        };
        d.onMoveUpdate = [](void* u, int32_t) { s_calls.push_back("moveupdate:" + std::to_string(static_cast<FakeUnit*>(u)->guid & 0xFF)); };
        d.stopAnimation = [](void* u) { s_calls.push_back("stop:" + std::to_string(static_cast<FakeUnit*>(u)->guid & 0xFF)); };
        d.heartbeat = [](void* u, int32_t now) { s_calls.push_back("heartbeat:" + std::to_string(static_cast<FakeUnit*>(u)->guid & 0xFF) + "@" + std::to_string(now)); };
        // The movement info here: u32 flags, 3 floats position.
        d.readMovement = [](CDataStore* msg, void* m) {
            uint32_t flags = 0;
            float p[3] = {};
            msg->Get(flags);
            msg->Get(p[0]);
            msg->Get(p[1]);
            msg->Get(p[2]);
            if (msg->m_read > msg->m_size) {
                return false;
            }
            if (m) {
                s_calls.push_back("status:" + std::to_string(M(m)->unit->guid & 0xFF));
                M(m)->flags = flags;
                for (int i = 0; i < 3; ++i) {
                    M(m)->pos[i] = p[i];
                }
            }
            return true;
        };
        d.writeMovement = [](void*, uint32_t opcode, int32_t time, CDataStore* msg) {
            msg->Put(static_cast<uint16_t>(opcode));
            msg->Put(static_cast<uint32_t>(time));
        };
    }

    CoATest::Bytes& Force(CoATest::Bytes& b, uint32_t id, float x, float y, float z, uint32_t filter, float magnitude, uint32_t type) {
        return b.U32(id).F32(x).F32(y).F32(z).U32(filter).F32(magnitude).U32(type);
    }

    CoATest::Bytes Packed(uint64_t guid) {
        CoATest::Bytes b;
        uint8_t mask = 0;
        std::vector<uint8_t> bytes;
        for (int i = 0; i < 8; ++i) {
            const uint8_t v = static_cast<uint8_t>(guid >> (i * 8));
            if (v) {
                mask |= 1 << i;
                bytes.push_back(v);
            }
        }
        b.U8(mask);
        for (uint8_t v : bytes) {
            b.U8(v);
        }
        return b;
    }

    int32_t Dispatch(uint32_t opcode, CoATest::Bytes& bytes) {
        CDataStore msg(bytes.data.data(), static_cast<uint32_t>(bytes.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result);
        return result;
    }

    CoATest::Bytes Ack(uint64_t guid, uint32_t counter, uint16_t opcode, uint32_t time) {
        CoATest::Bytes b = Packed(guid);
        b.U32(counter).U16(opcode).U32(time);
        return b;
    }
}

COA_TEST(MovementForce_ApplyStoresTheForceAndAcks) {
    Start();
    AddUnit(kPlayer, 4);
    CoATest::Bytes b = Packed(kPlayer);
    b.U32(7);
    Force(b, 3, 1.0f, 0.0f, 0.0f, 0, 10.0f, 0);
    CHECK(Dispatch(0x761, b) == 1);
    CHECK(CoA::MovementForce::ForceCount(kPlayer) == 1);
    CHECK((s_calls == std::vector<std::string>{ "anchors:17" }));
    CHECK(CoATest::SentPackets().size() == 1);
    CHECK(CoATest::SentPackets()[0] == CoATest::Expected(0x763, Ack(kPlayer, 7, 0x763, 1000)));

    // The movement clock's time when it runs.
    s_idle = 4242;
    CoATest::Bytes again = Packed(kPlayer);
    again.U32(8);
    Force(again, 4, 0.0f, 1.0f, 0.0f, 0, 10.0f, 0);
    Dispatch(0x761, again);
    CHECK(CoA::MovementForce::ForceCount(kPlayer) == 2);
    CHECK(CoATest::SentPackets().back() == CoATest::Expected(0x763, Ack(kPlayer, 8, 0x763, 4242)));
    CoATest::StopRecordingPackets();
}

COA_TEST(MovementForce_ShortApplyAndRemoveAreIgnored) {
    Start();
    AddUnit(kPlayer, 4);
    CoATest::Bytes b = Packed(kPlayer);
    b.U32(7).U32(3).F32(1.0f);   // the force cut short
    Dispatch(0x761, b);
    CHECK(CoA::MovementForce::ForceCount(kPlayer) == 0);
    CHECK(CoATest::SentPackets().empty());

    CoATest::Bytes empty;
    Dispatch(0x761, empty);
    Dispatch(0x762, empty);
    Dispatch(0x765, empty);
    Dispatch(0x766, empty);
    CHECK(CoATest::SentPackets().empty());
    CHECK(CoA::MovementForce::ForceCount(0) == 0);

    CoATest::Bytes full = Packed(kPlayer);
    full.U32(1);
    Force(full, 3, 1.0f, 0.0f, 0.0f, 0, 10.0f, 0);
    Dispatch(0x761, full);
    CoATest::Bytes cut = Packed(kPlayer);
    cut.U32(2);   // no id
    Dispatch(0x762, cut);
    CHECK(CoA::MovementForce::ForceCount(kPlayer) == 1);
    CHECK(CoATest::SentPackets().size() == 1);
    CoATest::StopRecordingPackets();
}

COA_TEST(MovementForce_RemoveErasesAndAcks) {
    Start();
    AddUnit(kPlayer, 4);
    CoATest::Bytes b = Packed(kPlayer);
    b.U32(1);
    Force(b, 3, 1.0f, 0.0f, 0.0f, 0, 10.0f, 0);
    Dispatch(0x761, b);
    s_calls.clear();

    CoATest::Bytes r = Packed(kPlayer);
    r.U32(2).U32(3);
    Dispatch(0x762, r);
    CHECK(CoA::MovementForce::ForceCount(kPlayer) == 0);
    CHECK((s_calls == std::vector<std::string>{ "anchors:17" }));
    CHECK(CoATest::SentPackets().back() == CoATest::Expected(0x764, Ack(kPlayer, 2, 0x764, 1000)));

    // An unknown guid still acks (when the unit exists).
    CoATest::Bytes unknown = Packed(kPlayer);
    unknown.U32(5).U32(9);
    Dispatch(0x762, unknown);
    CHECK(CoATest::SentPackets().size() == 3);
    CHECK(CoATest::SentPackets().back() == CoATest::Expected(0x764, Ack(kPlayer, 5, 0x764, 1000)));
    CoATest::StopRecordingPackets();
}

COA_TEST(MovementForce_DisplacementOnThePlotStep) {
    Start();
    FakeUnit& p = AddUnit(kPlayer, 4);
    p.move.flags = 1;   // moving forward
    float out[3] = { 1.0f, 2.0f, 3.0f };
    CHECK(CoA::MovementForce::AfterPlot(&p.move, 500, out, 0) == 0);   // no force: untouched
    CHECK(out[0] == 1.0f);

    CoATest::Bytes b = Packed(kPlayer);
    b.U32(1);
    Force(b, 1, 3.0f, 4.0f, 0.0f, 0, 10.0f, 0);   // direction (0.6, 0.8), 10 / s
    Dispatch(0x761, b);
    CHECK(CoA::MovementForce::Forced(&p.move));
    CHECK(CoA::MovementForce::AfterPlot(&p.move, 500, out, 0) == 1);
    CHECK(Near(out[0], 4.0f) && Near(out[1], 6.0f) && Near(out[2], 3.0f));   // added to the step while moving
    CHECK(CoA::MovementForce::AfterPlot(&p.move, 500, out, 5) == 5);

    // A point is reached, never overshot.
    CoATest::Bytes point = Packed(kPlayer);
    point.U32(2);
    Force(point, 1, 0.0f, 0.0f, 2.0f, 0, 10.0f, 1);
    Dispatch(0x761, point);
    float step[3] = {};
    CoA::MovementForce::AfterPlot(&p.move, 1000, step, 0);
    CHECK(Near(step[0], 0.0f) && Near(step[1], 0.0f) && Near(step[2], 2.0f));

    // Another transport filter is skipped; flags 0x800 or dt <= 0 leave the step.
    CoATest::Bytes filtered = Packed(kPlayer);
    filtered.U32(3);
    Force(filtered, 1, 1.0f, 0.0f, 0.0f, 77, 10.0f, 0);
    Dispatch(0x761, filtered);
    float none[3] = {};
    CoA::MovementForce::AfterPlot(&p.move, 1000, none, 0);
    CHECK(none[0] == 0.0f && none[1] == 0.0f && none[2] == 0.0f);
    p.move.transport = 77;
    CoA::MovementForce::AfterPlot(&p.move, 1000, none, 0);
    CHECK(Near(none[0], 10.0f));
    p.move.flags = 0x800;
    float frozen[3] = {};
    CoA::MovementForce::AfterPlot(&p.move, 1000, frozen, 0);
    CHECK(frozen[0] == 0.0f);
    CoATest::StopRecordingPackets();
}

COA_TEST(MovementForce_TickStepsTheIdlePlayer) {
    Start();
    FakeUnit& p = AddUnit(kPlayer, 4);
    CoATest::Bytes b = Packed(kPlayer);
    b.U32(1);
    Force(b, 1, 1.0f, 0.0f, 0.0f, 0, 10.0f, 0);
    Dispatch(0x761, b);
    s_calls.clear();

    CoA::MovementForce::Tick();   // the first frame only starts the clock
    CHECK(s_calls.empty());
    s_now = 1050;
    CoA::MovementForce::Tick();
    // Stopped tracking (the player controls itself), then the step with the moving bit, flags restored.
    CHECK((s_calls == std::vector<std::string>{ "stop:17", "anchors:17", "apply:50:1", "heartbeat:17@1050" }));
    CHECK(Near(p.move.pos[0], 0.5f));
    CHECK(p.move.flags == 0);
    CHECK(CoA::MovementForce::Stopped(kPlayer));

    // dt is capped at 100 ms; the heartbeat waits 0x80 ms.
    s_calls.clear();
    s_now = 1400;
    CoA::MovementForce::Tick();
    CHECK((s_calls == std::vector<std::string>{ "anchors:17", "apply:100:1", "heartbeat:17@1400" }));
    s_calls.clear();
    s_now = 1450;
    CoA::MovementForce::Tick();
    CHECK((s_calls == std::vector<std::string>{ "anchors:17", "apply:50:1" }));

    // A moving player is not stepped (its own movement plots the force); its stop is undone.
    s_calls.clear();
    p.move.flags = 1;
    s_now = 1500;
    CoA::MovementForce::Tick();
    CHECK((s_calls == std::vector<std::string>{ "stop:17" }));
    CHECK(!CoA::MovementForce::Stopped(kPlayer));

    // The last player move this frame skips the step.
    p.move.flags = 0;
    s_now = 1600;
    CoA::MovementForce::AfterExecuteMovement(&p.move, 1600);   // idle: not recorded
    s_calls.clear();
    CoA::MovementForce::Tick();
    CHECK(s_calls.size() == 4);
    p.move.flags = 1;
    CoA::MovementForce::AfterExecuteMovement(&p.move, 1700);
    p.move.flags = 0;
    s_now = 1700;
    s_calls.clear();
    CoA::MovementForce::Tick();
    CHECK(s_calls.empty());
    CoATest::StopRecordingPackets();
}

COA_TEST(MovementForce_OtherUnitsForceAndSmoothing) {
    Start();
    AddUnit(kPlayer, 4);
    FakeUnit& o = AddUnit(kOther, 3);
    o.move.pos[0] = 10.0f;
    CoATest::Bytes b = Packed(kOther);
    b.U32(0).F32(10.0f).F32(0.0f).F32(0.0f);   // movement info: idle at (10, 0, 0)
    Force(b, 5, 0.0f, 1.0f, 0.0f, 0, 20.0f, 0);
    Dispatch(0x765, b);
    CHECK(CoA::MovementForce::ForceCount(kOther) == 1);
    CHECK((s_calls == std::vector<std::string>{ "status:34", "anchors:34" }));
    CHECK(CoATest::SentPackets().empty());

    // Another unit's idle position update with a force on it: kept, then spread over the frames.
    s_now = 2000;
    CoA::MovementForce::StatusUpdate u = CoA::MovementForce::BeforeStatus(&o.move, 0);
    CHECK(u.keep);
    o.move.pos[0] = 11.0f;   // the update moved it a yard
    CoA::MovementForce::AfterStatus(u, &o.move, 0);
    CHECK(CoA::MovementForce::Smoothing(kOther));
    CHECK(o.move.pos[0] == 10.0f);   // put back

    s_calls.clear();
    CoA::MovementForce::Tick();   // clock start
    s_now = 2064;                 // half the default 0x80 interval
    CoA::MovementForce::Tick();
    // Not the player's: stepped as idle (the unit's controller is itself, which is not the mover: stop tracked).
    CHECK((s_calls == std::vector<std::string>{ "stop:34", "anchors:34", "apply:64:1", "moveupdate:34" }));
    CHECK(Near(o.move.pos[0], 10.5f));
    s_now = 2200;
    CoA::MovementForce::Tick();
    CHECK(Near(o.move.pos[0], 11.0f));
    CHECK(!CoA::MovementForce::Smoothing(kOther));

    // A moving update cancels the smoothing; the player's own updates are never kept.
    CHECK(!CoA::MovementForce::BeforeStatus(&o.move, 1).keep);

    // 0x766 removes it.
    CoATest::Bytes r = Packed(kOther);
    r.U32(0).F32(11.0f).F32(0.0f).F32(0.0f).U32(5);
    Dispatch(0x766, r);
    CHECK(CoA::MovementForce::ForceCount(kOther) == 0);
    CHECK(!CoA::MovementForce::BeforeStatus(&o.move, 0).keep);
    CoATest::StopRecordingPackets();
}

COA_TEST(MovementForce_ApplyOtherShortForceIsIgnored) {
    Start();
    AddUnit(kOther, 3);
    CoATest::Bytes b = Packed(kOther);
    b.U32(0).F32(1.0f).F32(2.0f).F32(3.0f).U32(5).F32(1.0f);   // the force cut short
    Dispatch(0x765, b);
    CHECK(CoA::MovementForce::ForceCount(kOther) == 0);
    CHECK((s_calls == std::vector<std::string>{ "status:34" }));
    CoATest::StopRecordingPackets();
}

COA_TEST(MovementForce_CollideRunsFlaggedWhileStanding) {
    Start();
    FakeUnit& p = AddUnit(kPlayer, 4);
    uint32_t saved = 0;
    CHECK(!CoA::MovementForce::BeforeCollide(&p.move, saved));
    CoATest::Bytes b = Packed(kPlayer);
    b.U32(1);
    Force(b, 1, 1.0f, 0.0f, 0.0f, 0, 10.0f, 0);
    Dispatch(0x761, b);
    p.move.flags = 0x40;
    CHECK(CoA::MovementForce::BeforeCollide(&p.move, saved));
    CHECK(p.move.flags == 0x41);
    CoA::MovementForce::AfterCollide(&p.move, true, saved);
    CHECK(p.move.flags == 0x40);
    p.move.flags = 0x1000;   // falling
    CHECK(!CoA::MovementForce::BeforeCollide(&p.move, saved));
    CoATest::StopRecordingPackets();
}

COA_TEST(MovementForce_ResetOnWorldChangeAndLeavingTheWorld) {
    Start();
    AddUnit(kPlayer, 4);
    CoATest::Bytes b = Packed(kPlayer);
    b.U32(1);
    Force(b, 1, 1.0f, 0.0f, 0.0f, 0, 10.0f, 0);
    Dispatch(0x761, b);
    CHECK(CoA::MovementForce::ForceCount(kPlayer) == 1);
    CoA::MovementForce::Reset();
    CHECK(CoA::MovementForce::ForceCount(kPlayer) == 0);

    CoATest::Bytes again = b;
    Dispatch(0x761, again);
    CoA::ClientHooks::leaveWorld.Run();
    CHECK(CoA::MovementForce::ForceCount(kPlayer) == 0);
    Dispatch(0x761, again);
    CoA::ClientHooks::beforeShutdown.Run();
    CHECK(CoA::MovementForce::ForceCount(kPlayer) == 0);
    Dispatch(0x761, again);
    CoA::ClientHooks::afterClientDestroy.Run();
    CHECK(CoA::MovementForce::ForceCount(kPlayer) == 0);
    CoATest::StopRecordingPackets();
}

// The original's bug, kept (review M3): 0x762 / 0x766 gate the controller's stop on a second erase of the guid from the
// stopped set right after the first, so the stop never fires even for a unit the tick had stopped.
COA_TEST(MovementForce_RemoveNeverFiresTheStop) {
    Start();
    FakeUnit& o = AddUnit(kOther, 3);
    CoATest::Bytes b = Packed(kOther);
    b.U32(0).F32(0.0f).F32(0.0f).F32(0.0f);
    Force(b, 5, 0.0f, 1.0f, 0.0f, 0, 20.0f, 0);
    Dispatch(0x765, b);
    CoA::MovementForce::Tick();
    s_now += 64;
    CoA::MovementForce::Tick();   // steps the idle unit: tracked as stopped
    CHECK(CoA::MovementForce::ForceCount(kOther) == 1);
    CHECK(std::find(s_calls.begin(), s_calls.end(), "stop:34") != s_calls.end());

    s_calls.clear();
    CoATest::Bytes r = Packed(kOther);
    r.U32(0).F32(o.move.pos[0]).F32(o.move.pos[1]).F32(o.move.pos[2]).U32(5);
    Dispatch(0x766, r);
    CHECK(CoA::MovementForce::ForceCount(kOther) == 0);
    CHECK(std::find(s_calls.begin(), s_calls.end(), "stop:34") == s_calls.end());
    CoATest::StopRecordingPackets();
}
