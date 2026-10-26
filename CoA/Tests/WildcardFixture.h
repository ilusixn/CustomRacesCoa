#ifndef COA_TESTS_WILDCARDFIXTURE_H
#define COA_TESTS_WILDCARDFIXTURE_H

// The shared setup of the C_Wildcard tests: the CA fixture's Warrior (rows 100 .. 151), a wildcard realm (game mode bit
// 6) with an active spec, wildcard Essence rows, the server config, the token amounts, a recording timer backend and a
// fake client behind CoA::Wildcard::Deps. Wildcard::Init runs after CA's, so the CA Deps it fills are the real ones.
//
//   Essence (wildcard)  level 10: 20 AE / 5 TE (id 4), level 60: 600 AE / 30 TE (id 5)
//   spell chains        5000 -> 5001 (row 100); every other spell is its own chain

#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Modules/CAInternal.h"
#include "CoA/Source/Modules/Config.h"
#include "CoA/Source/Modules/GameEvents.h"
#include "CoA/Source/Modules/GameMode.h"
#include "CoA/Source/Modules/Token.h"
#include "CoA/Source/Modules/Wildcard.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Timer.h"
#include "CoA/Tests/CAFixture.h"
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace WCFixture {
    using V = std::vector<std::string>;
    using Packets = std::vector<std::vector<uint8_t>>;
    using CAFixture::Feed;
    using CAFixture::Run;

    inline bool s_player = true;
    inline uint32_t s_level = 10;
    inline uint32_t s_money = 0;
    inline uint32_t s_marks = 0;
    inline uint32_t s_tick = 0;
    inline bool s_skillCard = false;
    inline uint32_t s_skillCardSpec = 0;

    struct Add {
        uint32_t spell;
        uint32_t notify;
        uint32_t replace;
        int suppressed;   // ClientHooks::suppressLearnMessages during the add
    };

    inline std::vector<Add> s_adds;
    inline std::vector<uint32_t> s_afterAdds;

    // ---- the recording timer backend -------------------------------------------------------------------------
    struct Scheduled {
        uint32_t id;
        uint32_t ms;
        CoA::Timer::Fn fn;
        void* param;
    };

    struct Cancelled {
        uint32_t id;
        CoA::Timer::Fn fn;
        std::string param;   // the cancel's param read as a name ("" when null)
    };

    inline std::vector<Scheduled> s_scheduled;
    inline std::vector<Cancelled> s_cancelled;
    inline uint32_t s_nextTimer = 0;

    inline uint32_t ScheduleTimer(uint32_t ms, CoA::Timer::Fn fn, void* param) {
        s_scheduled.push_back({ ++s_nextTimer, ms, fn, param });
        return s_nextTimer;
    }

    inline void CancelTimer(uint32_t id, CoA::Timer::Fn fn, void* param) {
        s_cancelled.push_back({ id, fn, param && fn ? std::string(static_cast<const char*>(param)) : std::string() });
    }

    // Runs the last scheduled timer's callback (as the client's timer would when it fires).
    inline void FireLast() {
        Scheduled s = s_scheduled.back();
        s.fn(nullptr, s.param);
    }

    inline void Fire(size_t i) {
        Scheduled s = s_scheduled[i];
        s.fn(nullptr, s.param);
    }

    // ---- config / tokens / essence -----------------------------------------------------------------------------
    inline void Configs(const std::map<std::string, int32_t>& ints, const std::map<std::string, bool>& bools) {
        CoATest::Bytes b;
        b.U32(static_cast<uint32_t>(ints.size()));

        for (const auto& kv : ints) {
            b.Name(kv.first.c_str()).I32(kv.second);
        }

        b.U32(static_cast<uint32_t>(bools.size()));

        for (const auto& kv : bools) {
            b.Name(kv.first.c_str()).U8(kv.second ? 1 : 0);
        }

        b.U32(0).U32(0);
        CoATest::Deliver(CoA::Config::OnUpdateConfigs, 0x58D, b);
    }

    inline void SetToken(uint32_t index, uint32_t amount, uint32_t extra = 0) {
        Feed(0x670, CoATest::Bytes().Str(CoA::Token::TypeName(index)).U32(amount).U32(extra));
    }

    inline CoA::Dbc::Table& Essence() {
        return CoA::Dbc::Get("DBFilesClient\\CharacterAdvancementEssence.dbc");
    }

    // A wildcard Essence row for the Warrior (class 1): id, level, class, draft, wildcard, specDraft, u2c, AE, TE.
    inline void SetEssence(uint32_t id, uint32_t level, uint32_t ae, uint32_t te) {
        const uint32_t row[9] = { id, level, 1, 0, 1, 0, 0, ae, te };
        Essence().Upsert(id, std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(row), reinterpret_cast<const uint8_t*>(row) + 0x24));
    }

    // ---- entries ----------------------------------------------------------------------------------------------
    struct E {
        uint32_t id;
        uint32_t rank = 1;
        uint8_t locked = 0;
        uint32_t order = 0;   // +0x18 (the learn order key, low half)
    };

    inline CoATest::Bytes Entries(const std::vector<E>& entries) {
        CoATest::Bytes b;
        b.U32(static_cast<uint32_t>(entries.size()));

        for (const E& e : entries) {
            b.U32(e.id).U32(e.rank).U32(e.rank).U8(e.locked).U32(e.order).U32(0);
        }

        return b;
    }

    inline uint32_t FirstRank(uint32_t spell) {
        return spell == 5001 ? 5000 : spell;
    }

    inline void InstallDeps() {
        CoA::Wildcard::Deps& d = CoA::Wildcard::Dependencies();
        d.hasPlayer = [] { return s_player; };
        d.playerLevel = [] { return s_level; };
        d.playerMoney = [] { return s_money; };
        d.itemCount = [](uint32_t entry) { return entry == 0x5B9D2 ? s_marks : 0u; };
        d.tickCount = [] { return s_tick; };
        d.firstRank = FirstRank;
        d.addKnownSpell = [](uint32_t spell, uint32_t notify, uint32_t replace) {
            s_adds.push_back({ spell, notify, replace, CoA::ClientHooks::suppressLearnMessages });
        };
        d.skillCardUnblocked = [](uint32_t spec) {
            s_skillCardSpec = spec;
            return s_skillCard;
        };
        CoA::CA::Dependencies().firstRank = FirstRank;
    }

    inline void Mode(uint32_t mode) {
        Feed(0x90B, CoATest::Bytes().U32(mode));
    }

    // The CA fixture's Warrior at `level` on a wildcard realm (unless !wildcard), spec 0 active with `entries` known
    // (unless !enter: no player record, spec -1), then C_Wildcard.
    inline lua_State* Start(const std::vector<E>& entries = {}, bool wildcard = true, bool enter = true, uint32_t level = 10) {
        Essence().Unload();
        CAFixture::Setup();
        CoA::ClientHooks::Reset();
        CoA::ClientHooks::enterWorld.Clear();
        CoA::ClientHooks::suppressLearnMessages = 0;
        CoA::GameEvents::Reset();
        CoA::Config::Reset();
        CoA::Token::Reset();
        CoA::Token::Init();
        CoA::GameMode::Reset();
        CoA::GameMode::Init();
        CoA::CA::Init();
        CoA::Wildcard::Reset();
        CoA::Wildcard::Init();
        InstallDeps();
        s_player = true;
        s_level = level;
        CAFixture::s_level = level;
        s_money = 0;
        s_marks = 0;
        s_tick = 0;
        s_skillCard = false;
        s_skillCardSpec = 0;
        s_adds.clear();
        s_afterAdds.clear();
        s_scheduled.clear();
        s_cancelled.clear();
        s_nextTimer = 0;
        CoA::Timer::SetBackend(ScheduleTimer, CancelTimer);
        CoA::ClientHooks::afterAddKnownSpell.Add([](uint32_t spell) { s_afterAdds.push_back(spell); });
        // Id 5 first: growing the index past its maximum drops the old last row (CoA::Dbc::Table::Upsert copies
        // [min, max) -- reported, not changed here).
        SetEssence(5, 60, 600, 30);
        SetEssence(4, 10, 20, 5);

        if (wildcard) {
            Mode(0x40);
        }

        if (enter) {
            Feed(0x725, CoATest::Bytes().U32(0).U32(1));
            Feed(0x726, Entries(entries));
        }

        return CoATest::NewState(false);
    }

    inline void Stop(lua_State* L) {
        lua_close(L);
        CoATest::StopRecordingPackets();
        Mode(0);   // GameMode outlives the test
        CoA::Timer::SetBackend(nullptr, nullptr);
        CoA::Wildcard::Reset();
        CoA::CA::Reset();
        CoA::Config::Reset();
        CoA::Token::Reset();
        CoA::GameEvents::Reset();
        CoA::GameMode::Reset();
        CoA::ClientHooks::Reset();
        CoA::ClientHooks::enterWorld.Clear();
        CoA::ClientHooks::suppressLearnMessages = 0;
        Essence().Unload();
        CAFixture::Teardown();
    }

    inline V R(lua_State* L, const std::string& chunk) {
        return Run(L, chunk.c_str());
    }

    // The packets a chunk sends (and its results).
    inline Packets Sent(lua_State* L, const std::string& chunk, V* results = nullptr) {
        CoATest::RecordPackets();
        V r = Run(L, chunk.c_str());
        CoATest::StopRecordingPackets();

        if (results) {
            *results = r;
        }

        return CoATest::SentPackets();
    }

    // The signals a chunk fires.
    inline V Fired(lua_State* L, const std::string& chunk) {
        CoATest::RecordSignals();
        Run(L, chunk.c_str());
        CoATest::StopRecordingSignals();
        return CoATest::Signals();
    }

    // The signals and packets a delivered packet causes.
    inline V FeedSent(uint32_t opcode, const CoATest::Bytes& bytes, Packets& sent) {
        CoATest::RecordPackets();
        V s = Feed(opcode, bytes);
        CoATest::StopRecordingPackets();
        sent = CoATest::SentPackets();
        return s;
    }

    inline uint32_t MaxLevel() {
        return CoA::GameEvents::ServerMaxLevel();
    }

    // A player who may repurchase: at the max level with repurchasing enabled.
    inline void AtMax() {
        s_level = MaxLevel();
        CAFixture::s_level = s_level;
    }
}

#endif
