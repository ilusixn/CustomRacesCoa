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

// Predicate lists run in call-site order and stop at the first false.
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
