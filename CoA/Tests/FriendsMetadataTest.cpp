// GetFriendsMetadata / SetFriendsMetadata (FriendsMetadata.cpp): SMSG 0x9C0, FRIEND_METADATA_CHANGED and CMSG 0x556.
#include "CoA/Source/Modules/FriendsMetadata.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>
#include <utility>
#include <vector>

namespace {
    using V = std::vector<std::string>;

    lua_State* Start() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::FriendsMetadata::Reset();
        CoA::FriendsMetadata::Init();
        return CoATest::NewState(false);
    }

    void Stop(lua_State* L) {
        lua_close(L);
        CoATest::StopRecordingPackets();
        CoA::FriendsMetadata::Reset();
    }

    V Feed(const std::vector<std::pair<std::string, std::string>>& entries, uint32_t count = 0xFFFFFFFF) {
        CoATest::Bytes b;
        b.U32(count == 0xFFFFFFFF ? static_cast<uint32_t>(entries.size()) : count);

        for (const auto& e : entries) {
            b.Name(e.first.c_str()).Name(e.second.c_str());
        }

        CoATest::RecordSignals();
        CDataStore msg(b.data.data(), static_cast<uint32_t>(b.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(0x9C0, 0, &msg, result);
        CoATest::StopRecordingSignals();
        return CoATest::Signals();
    }

    V Get(lua_State* L, const char* name) {
        return CoATest::Call(L, "", "GetFriendsMetadata", { std::string("s:") + name });
    }
}

// A changed value is stored and signalled; an unchanged one is not; an unknown name with an empty value reads as
// unchanged (the original compares against "").
COA_TEST(FriendsMetadata_List) {
    lua_State* L = Start();
    CHECK((Feed({ { "Bob", "a" }, { "Ann", "b" } }) == V{ "FRIEND_METADATA_CHANGED:Bob,a", "FRIEND_METADATA_CHANGED:Ann,b" }));
    CHECK(Get(L, "Bob") == V{ "a" });
    CHECK(Feed({ { "Bob", "a" }, { "Ann", "c" } }) == V{ "FRIEND_METADATA_CHANGED:Ann,c" });
    CHECK(Get(L, "Ann") == V{ "c" });
    CHECK(Feed({ { "Cid", "" } }).empty());
    CHECK(Get(L, "Cid") == V{ "nil" });
    CHECK(Feed({ { "Bob", "" } }) == V{ "FRIEND_METADATA_CHANGED:Bob," });
    CHECK(Get(L, "Bob") == V{ "" });
    // An empty name is stored but never read back.
    CHECK(Feed({ { "", "x" } }) == V{ "FRIEND_METADATA_CHANGED:,x" });
    CHECK(Get(L, "") == V{ "nil" });
    CHECK(Get(L, "Zed") == V{ "nil" });
    // A count past the bytes left stops at the end.
    CHECK(Feed({ { "Dan", "d" } }, 1000) == V{ "FRIEND_METADATA_CHANGED:Dan,d" });
    Stop(L);
}

// CMSG 0x556 { u32 len + name, u32 len + value }; the native returns its top argument.
COA_TEST(FriendsMetadata_Set) {
    lua_State* L = Start();
    CoATest::RecordPackets();
    CHECK(CoATest::Call(L, "", "SetFriendsMetadata", { "s:Bob", "s:hello" }) == V{ "hello" });
    CHECK(CoATest::Call(L, "", "SetFriendsMetadata", { "s:", "s:" }) == V{ "" });
    CHECK(CoATest::Call(L, "", "SetFriendsMetadata", { "s:Bob" }).at(0).compare(0, 7, "<error>") == 0);
    CoATest::StopRecordingPackets();
    CHECK((CoATest::SentPackets() == std::vector<std::vector<uint8_t>>{ CoATest::Expected(0x556, CoATest::Bytes().Name("Bob").Name("hello")),
                                                                      CoATest::Expected(0x556, CoATest::Bytes().U32(0).U32(0)) }));
    CHECK(Get(L, "Bob") == V{ "nil" });   // nothing stored locally
    Stop(L);
}
