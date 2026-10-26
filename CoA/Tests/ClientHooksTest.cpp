#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Logger.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <cstdio>
#include <string>
#include <vector>

namespace {
    std::string ReadAll(const char* file) {
        std::string text;
        FILE* f = fopen(file, "rb");

        if (!f) {
            return text;
        }

        char buf[512];
        size_t n;

        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
            text.append(buf, n);
        }

        fclose(f);
        return text;
    }

    std::vector<std::string> s_calls;
}

// FUN_102766c0: a signal without an event object is refused and logged; a valid one runs its stock callbacks.
COA_TEST(ClientHooks_SignalGuard) {
    CoATest::TempCwd cwd;
    CoA::Logger::Init();
    CoA::ClientHooks::Reset();
    s_calls.clear();
    CoA::ClientHooks::stockEvents.push_back({ 5, [](lua_State*) { s_calls.push_back("five"); } });
    lua_State* L = reinterpret_cast<lua_State*>(0x1000);

    CHECK(!CoA::ClientHooks::AllowSignalEvent(900, L, 722, true, false, nullptr));
    CHECK(!CoA::ClientHooks::AllowSignalEvent(5, nullptr, 722, true, true, "PLAYER_LOGIN"));
    CHECK(s_calls.empty());

    CHECK(CoA::ClientHooks::AllowSignalEvent(5, L, 722, true, true, "PLAYER_LOGIN"));
    CHECK(s_calls.size() == 1);
    CHECK(std::string(CoA::ClientHooks::LastSignalEvent()) == "PLAYER_LOGIN");

    CoA::Logger::Shutdown();
    CHECK(ReadAll("Logs\\Fatal.txt") ==
        "Blocked invalid FrameScript signal event at 0x0081AA00: eventId=900, registeredEventCount=722, luaState=0x1000, "
        "hasEventData=true, hasEventObject=false\r\n"
        "Blocked invalid FrameScript signal event at 0x0081AA00: eventId=5, registeredEventCount=722, luaState=0x0, "
        "hasEventData=true, hasEventObject=true\r\n");
    CoA::ClientHooks::Reset();
}

// Sites outside a list's measured order run in call-site order; AllPass stops at the first false.
COA_TEST(ClientHooks_PredicatesInSiteOrder) {
    CoA::ClientHooks::Reset();
    s_calls.clear();
    CoA::ClientHooks::addAuraEffect.Add([](void*, uint32_t, const void*) { s_calls.push_back("b"); return false; }, 0x200);
    CoA::ClientHooks::addAuraEffect.Add([](void*, uint32_t, const void*) { s_calls.push_back("a"); return true; }, 0x100);
    CoA::ClientHooks::addAuraEffect.Add([](void*, uint32_t, const void*) { s_calls.push_back("c"); return true; }, 0x200);

    CHECK(!CoA::ClientHooks::addAuraEffect.AllPass(nullptr, 0, nullptr));
    CHECK((s_calls == std::vector<std::string>{ "a", "b" }));
    CoA::ClientHooks::Reset();
}

// The aura-application list runs in the original's measured order whatever the registration order (SpellMods'
// 0x10324D9A before SpellVisuals' 0x10323151 here, SpawnVisibility before Appearance); a site outside it runs last and
// a callback registered twice runs once.
COA_TEST(ClientHooks_PredicatesInMeasuredOrder) {
    CoA::ClientHooks::Reset();
    s_calls.clear();
    using P = bool (*)(void*, uint32_t, const void*);
    const P spawn = [](void*, uint32_t, const void*) { s_calls.push_back("spawn"); return true; };
    const P mods = [](void*, uint32_t, const void*) { s_calls.push_back("mods"); return true; };
    CoA::ClientHooks::addAuraEffect.Add([](void*, uint32_t, const void*) { s_calls.push_back("other"); return true; }, 0x10000000);
    CoA::ClientHooks::addAuraEffect.Add(spawn, 0x101A2025);
    CoA::ClientHooks::addAuraEffect.Add([](void*, uint32_t, const void*) { s_calls.push_back("appearance"); return true; }, 0x100C5B88);
    CoA::ClientHooks::addAuraEffect.Add(mods, 0x10324D9A);
    CoA::ClientHooks::addAuraEffect.Add([](void*, uint32_t, const void*) { s_calls.push_back("visuals"); return true; }, 0x10323151);
    CoA::ClientHooks::addAuraEffect.Add([](void*, uint32_t, const void*) { s_calls.push_back("aura137"); return true; }, 0x10A683D0);
    CoA::ClientHooks::addAuraEffect.Add(spawn, 0x101A2025);

    CHECK(CoA::ClientHooks::addAuraEffect.AllPass(nullptr, 0, nullptr));
    CHECK((s_calls == std::vector<std::string>{ "aura137", "visuals", "mods", "appearance", "spawn", "other" }));
    CoA::ClientHooks::Reset();
}

// 0x6DF050: the alternate list runs on the second, fourth, ... packet.
COA_TEST(ClientHooks_InitialSpellsEverySecond) {
    CoA::ClientHooks::Reset();
    s_calls.clear();
    CoA::ClientHooks::initialSpellsAlternate.Add([]() { s_calls.push_back("x"); });

    CoA::ClientHooks::InitialSpellsReceived();
    CHECK(s_calls.empty());
    CoA::ClientHooks::InitialSpellsReceived();
    CHECK(s_calls.size() == 1);
    CoA::ClientHooks::InitialSpellsReceived();
    CHECK(s_calls.size() == 1);
    CoA::ClientHooks::InitialSpellsReceived();
    CHECK(s_calls.size() == 2);
    CoA::ClientHooks::Reset();
}

// CNetClient.cpp FUN_102c3a90 (orig hook 0x632A40): the NetClient constructor keeps its object (DAT_10be3564) for the
// registrar FUN_102c4590; until a NetClient is built (never in the tests) there is none, and Reset leaves it alone.
COA_TEST(ClientHooks_KeptNetClient) {
    CHECK(CoA::ClientHooks::keptNetClient == nullptr);
    void* const kept = reinterpret_cast<void*>(0x1000);
    CoA::ClientHooks::keptNetClient = kept;
    CoA::ClientHooks::Reset();
    CHECK(CoA::ClientHooks::keptNetClient == kept);
    CoA::ClientHooks::keptNetClient = nullptr;
}
