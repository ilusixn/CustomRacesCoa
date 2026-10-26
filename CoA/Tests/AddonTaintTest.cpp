#include "CoA/Source/Modules/AddonTaint.h"
#include "CoA/Source/Modules/GlobalsJ.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <string>
#include <vector>

namespace {
    const char* const kWildCard = "Ascension_WildCard";   // listed secure
    const char* const kListed = "ListedAddon";            // listed, not secure
    const char* const kUnlisted = "SomeAddon";            // not listed

    std::vector<uint32_t> s_feedback;

    void RecordFeedback(uint32_t type) {
        s_feedback.push_back(type);
    }

    // SMSG 0x94E: u32 n x { cstring name, u8 secure }.
    void DeliverKnownList() {
        CoATest::Bytes b;
        b.U32(2).Str(kWildCard).U8(1).Str(kListed).U8(0);
        CoATest::Deliver(CoA::GlobalsJ::OnKnownAddonList, 0x94E, b);
    }

    // SMSG_REALM_INFO with the trailing addons byte (realm +0x48).
    void DeliverRealm(uint8_t addons) {
        CoATest::Bytes b;
        b.U32(1).U32(0).F32(1.0f).F32(0.0f).F32(0.0f).U32(0).F32(0.05f).F32(0.25f).U32(0);
        for (int i = 0; i < 8; ++i) {
            b.U8(0);
        }
        b.Str("").Str("Realm").U8(addons);
        CoATest::Deliver(CoA::RealmInfo::OnRealmInfo, 0x9BC, b);
    }

    void Setup() {
        lua_tainted = nullptr;
        s_feedback.clear();
        CoA::GlobalsJ::Reset();
        CoA::RealmInfo::Reset();
        CoA::AddonTaint::Reset();
        CoA::AddonTaint::Init();
        CoA::AddonTaint::Dependencies().blockedActionFeedback = RecordFeedback;
        DeliverKnownList();
    }

    CVar* LoadUnknownAddOns(const char* value) {
        CVar* var = CVar::Register("coaTestLoadUnknownAddOns", nullptr, 1, "1", nullptr, 5, false, nullptr, false);
        var->Set(value, true, false, false, false);
        *CoA::AddonTaint::LoadUnknownAddOnsSlot() = var;
        return var;
    }

    // Script_issecure (0x816DA0) as the engine has it, with its marked hook.
    int EngineIssecure(lua_State* L) {
        if (CoA::AddonTaint::Issecure(L)) {
            return 1;
        }

        if (lua_tainted) {
            lua_pushnil(L);
        } else {
            lua_pushnumber(L, 1.0);
        }

        return 1;
    }

    // Loads and runs "return issecure()" with `taint` running: the chunk is tainted by it and runs under it.
    std::string RunIssecure(lua_State* L, const char* taint) {
        lua_tainted = taint;
        std::string out;

        if (luaL_loadstring(L, "return issecure()") != 0 || lua_pcall(L, 0, 1, 0) != 0) {
            out = std::string("<error> ") + lua_tostring(L, -1);
        } else {
            out = lua_isnil(L, -1) ? "nil" : lua_tostring(L, -1);
        }

        lua_pop(L, 1);
        lua_tainted = nullptr;
        return out;
    }
}

COA_TEST(AddonTaint_SecureOnlyForListedSecureAddon) {
    Setup();
    lua_tainted = kWildCard;
    CHECK(CoA::AddonTaint::Secure());
    lua_tainted = kListed;
    CHECK(!CoA::AddonTaint::Secure());
    lua_tainted = kUnlisted;
    CHECK(!CoA::AddonTaint::Secure());
    lua_tainted = nullptr;
    CHECK(!CoA::AddonTaint::Secure());
}

COA_TEST(AddonTaint_SecureNeedsTheServerList) {
    Setup();
    CoA::GlobalsJ::Reset();
    lua_tainted = kWildCard;
    CHECK(!CoA::AddonTaint::Secure());
    lua_tainted = nullptr;
}

COA_TEST(AddonTaint_SecureMatchesTheWholeName) {
    Setup();
    lua_tainted = "Ascension_WildCard2";
    CHECK(!CoA::AddonTaint::Secure());
    lua_tainted = "Ascension_Wild";
    CHECK(!CoA::AddonTaint::Secure());
    lua_tainted = nullptr;
}

COA_TEST(AddonTaint_IssecurePushesOneForSecureAddon) {
    Setup();
    CoA::LuaReg::Reset();
    lua_State* L = CoATest::NewState(true);
    lua_tainted = kWildCard;
    const int top = lua_gettop(L);
    CHECK(CoA::AddonTaint::Issecure(L));
    CHECK(lua_gettop(L) == top + 1);
    CHECK(lua_isnumber(L, -1) && lua_tonumber(L, -1) == 1.0);
    lua_tainted = nullptr;
    lua_close(L);
}

COA_TEST(AddonTaint_IssecureLeavesOthersToTheClient) {
    Setup();
    CoA::LuaReg::Reset();
    lua_State* L = CoATest::NewState(true);
    const int top = lua_gettop(L);

    for (const char* taint : { kListed, kUnlisted, static_cast<const char*>(nullptr) }) {
        lua_tainted = taint;
        CHECK(!CoA::AddonTaint::Issecure(L));
        CHECK(lua_gettop(L) == top);
    }

    lua_tainted = nullptr;
    lua_close(L);
}

// The live C_Hook.lua recursion: issecure() under a server-listed secure addon's taint is 1, under others nil.
COA_TEST(AddonTaint_EngineIssecureUnderAddonTaint) {
    Setup();
    CoA::LuaReg::Reset();
    lua_State* L = CoATest::NewState(true);
    lua_pushcfunction(L, EngineIssecure);
    lua_setfield(L, LUA_GLOBALSINDEX, "issecure");

    CHECK(RunIssecure(L, kWildCard) == "1");
    CHECK(RunIssecure(L, kUnlisted) == "nil");
    CHECK(RunIssecure(L, kListed) == "nil");
    CHECK(RunIssecure(L, nullptr) == "1");

    // Before SMSG 0x94E nothing is secure.
    CoA::GlobalsJ::Reset();
    CHECK(RunIssecure(L, kWildCard) == "nil");
    lua_close(L);
}

COA_TEST(AddonTaint_ClearedSecureAddonRunsUntainted) {
    Setup();
    lua_tainted = kWildCard;
    const char* saved = CoA::AddonTaint::BeginCleared();
    CHECK(saved == kWildCard);
    CHECK(lua_tainted == nullptr);
    CoA::AddonTaint::EndCleared(saved);
    CHECK(lua_tainted == kWildCard);
    lua_tainted = nullptr;
}

COA_TEST(AddonTaint_ClearedOtherAddonKeepsItsTaint) {
    Setup();

    for (const char* taint : { kListed, kUnlisted }) {
        lua_tainted = taint;
        const char* saved = CoA::AddonTaint::BeginCleared();
        CHECK(saved == taint);
        CHECK(lua_tainted == taint);
        CoA::AddonTaint::EndCleared(saved);
        CHECK(lua_tainted == taint);
    }

    lua_tainted = nullptr;
}

COA_TEST(AddonTaint_ClearedNullTaintStaysNull) {
    Setup();
    const char* saved = CoA::AddonTaint::BeginCleared();
    CHECK(saved == nullptr);
    CHECK(lua_tainted == nullptr);
    CoA::AddonTaint::EndCleared(saved);
    CHECK(lua_tainted == nullptr);
}

// The saved taint is stored back whatever the call left running (FUN_10313b70 after the original, either way).
COA_TEST(AddonTaint_ClearedRestoresOverTheCallsChange) {
    Setup();
    lua_tainted = kUnlisted;
    const char* saved = CoA::AddonTaint::BeginCleared();
    lua_tainted = kListed;
    CoA::AddonTaint::EndCleared(saved);
    CHECK(lua_tainted == kUnlisted);

    lua_tainted = kWildCard;
    saved = CoA::AddonTaint::BeginCleared();
    lua_tainted = kUnlisted;
    CoA::AddonTaint::EndCleared(saved);
    CHECK(lua_tainted == kWildCard);

    lua_tainted = nullptr;
    saved = CoA::AddonTaint::BeginCleared();
    lua_tainted = kUnlisted;
    CoA::AddonTaint::EndCleared(saved);
    CHECK(lua_tainted == nullptr);
}

COA_TEST(AddonTaint_ClearTargetRefusedInsideRunScript) {
    Setup();
    CoA::GlobalsJ::SetInsideRunScript(true);
    CHECK(CoA::AddonTaint::ClearTargetRefused());
    CHECK(s_feedback == std::vector<uint32_t>{ 2 });
    CoA::GlobalsJ::SetInsideRunScript(false);
    CHECK(!CoA::AddonTaint::ClearTargetRefused());
    CHECK(s_feedback.size() == 1);
}

COA_TEST(AddonTaint_ClearTargetIgnoresTheTaint) {
    Setup();
    lua_tainted = kWildCard;
    CHECK(!CoA::AddonTaint::ClearTargetRefused());
    CoA::GlobalsJ::SetInsideRunScript(true);
    lua_tainted = nullptr;
    CHECK(CoA::AddonTaint::ClearTargetRefused());
    CoA::AddonTaint::Dependencies().blockedActionFeedback = nullptr;
    CHECK(CoA::AddonTaint::ClearTargetRefused());
    CHECK(s_feedback == std::vector<uint32_t>{ 2 });
    CoA::GlobalsJ::SetInsideRunScript(false);
}

COA_TEST(AddonTaint_TargetChangeClearsSecureAddonAndRestores) {
    Setup();
    lua_tainted = kWildCard;
    CoA::ClientHooks::beforeTarget.Run();
    CHECK(lua_tainted == nullptr);
    CoA::ClientHooks::afterTarget.Run();
    CHECK(lua_tainted == kWildCard);

    // The keep (0x10BE3F80) is zeroed after: a second after-run stores null.
    CoA::ClientHooks::afterTarget.Run();
    CHECK(lua_tainted == nullptr);
}

COA_TEST(AddonTaint_TargetChangeKeepsOtherTaint) {
    Setup();
    lua_tainted = kUnlisted;
    CoA::ClientHooks::beforeTarget.Run();
    CHECK(lua_tainted == kUnlisted);
    lua_tainted = kListed;
    CoA::ClientHooks::afterTarget.Run();
    CHECK(lua_tainted == kUnlisted);
    lua_tainted = nullptr;
}

COA_TEST(AddonTaint_AddOnLoadRefusedOnRealmWithoutAddons) {
    Setup();
    DeliverRealm(0);
    CHECK(CoA::AddonTaint::AddOnLoadRefused(kUnlisted));
    CHECK(!CoA::AddonTaint::AddOnLoadRefused(kWildCard));
    CHECK(!CoA::AddonTaint::AddOnLoadRefused(kListed));

    // No realm info yet: +0x48 is clear.
    CoA::RealmInfo::Reset();
    CHECK(CoA::AddonTaint::AddOnLoadRefused(kUnlisted));

    // No list: every addon is unknown.
    CoA::GlobalsJ::Reset();
    CHECK(CoA::AddonTaint::AddOnLoadRefused(kWildCard));
}

COA_TEST(AddonTaint_AddOnLoadAllowedOnRealmWithAddons) {
    Setup();
    DeliverRealm(1);
    CHECK(!CoA::AddonTaint::AddOnLoadRefused(kUnlisted));
    CoA::GlobalsJ::Reset();
    CHECK(!CoA::AddonTaint::AddOnLoadRefused(kUnlisted));
}

COA_TEST(AddonTaint_UnknownAddOnLoadFollowsTheCVar) {
    Setup();
    CHECK(!CoA::AddonTaint::UnknownAddOnLoadRefused(kUnlisted));   // not registered

    LoadUnknownAddOns("1");
    CHECK(!CoA::AddonTaint::UnknownAddOnLoadRefused(kUnlisted));

    LoadUnknownAddOns("0");
    CHECK(CoA::AddonTaint::UnknownAddOnLoadRefused(kUnlisted));
    CHECK(!CoA::AddonTaint::UnknownAddOnLoadRefused(kListed));
    CHECK(!CoA::AddonTaint::UnknownAddOnLoadRefused(kWildCard));

    // Independent of the realm flag.
    DeliverRealm(1);
    CHECK(CoA::AddonTaint::UnknownAddOnLoadRefused(kUnlisted));
    *CoA::AddonTaint::LoadUnknownAddOnsSlot() = nullptr;
}

COA_TEST(AddonTaint_LoadUnknownAddOnsRegistersIntoTheSlot) {
    CoA::CVars::Reset();
    CoA::GlueCVars::Init();
    size_t count = 0;
    const CoA::CVarEntry* entries = CoA::CVars::Glue(count);
    bool found = false;

    for (size_t i = 0; i < count; ++i) {
        if (strcmp(entries[i].name, "loadUnknownAddOns") == 0) {
            found = true;
            CHECK(entries[i].slot == CoA::AddonTaint::LoadUnknownAddOnsSlot());
            CHECK(strcmp(entries[i].defaultValue, "1") == 0);
        }
    }

    CHECK(found);
    CoA::CVars::Reset();
}
