// AscBuildDraft.cpp: C_BuildDraft (namespace registered by FUN_10111b10) over the build-draft state (FUN_10111410 ->
// static 0x10BC9848): +0x00 completed builds, +0x0C draftable builds, +0x18 drafted build, +0x30 awaiting-role-prompt
// flag. All strings on the wire are NUL-terminated.
//   SMSG 0x636 DRAFTABLES:  list replaced (u32 n x str); DRAFTABLE_BUILDS_UPDATE if it differs.
//   SMSG 0x637 DRAFTABLE:   u32 index, str; the list grows to fit; DRAFTABLE_BUILD_UPDATE("%u", index + 1) if the slot
//                           changed.
//   SMSG 0x638 DRAFTED:     str; DRAFTED_BUILD_UPDATE.
//   SMSG 0x639 ROLE_PROMPT: BUILD_DRAFT_ROLE_PROMPT, then the flag is set.
//   SMSG 0x63B SELECT_ROLE_RESULT: str; BUILD_DRAFT_SELECT_ROLE_OK clears the flag; BUILD_DRAFT_SELECT_ROLE_RESULT("%s").
//   SMSG 0x63C COMPLETED_BUILDS: list replaced (u32 n x str), no event.
//   SMSG 0x63D COMPLETED_BUILD:  str appended; COMPLETED_BUILD_UPDATE("%s").
//   SelectRole(role): CMSG 0x63A {str}.
// Every glue screen clears both lists, the drafted build and the flag (FUN_10110250).
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <string>
#include <vector>

namespace {
    struct State {
        std::vector<std::string> completed;
        std::vector<std::string> draftable;
        std::string drafted;
        bool awaitingRole = false;
    };

    State s_build;

    // A 0x637 index past this is dropped: the original grows the list to fit any index.
    constexpr uint32_t kMaxDraftable = 0x10000;

    void ReadList(CDataStore* msg, std::vector<std::string>& out) {
        for (uint32_t n = CoA::Read::U32(msg); n && !CoA::Read::Failed(msg) && CoA::Read::Remaining(msg); --n) {
            out.push_back(CoA::Read::Str(msg));
        }
    }

    int32_t OnDraftables(void*, uint32_t, uint32_t, CDataStore* msg) {
        const std::vector<std::string> old = s_build.draftable;
        s_build.draftable.clear();
        ReadList(msg, s_build.draftable);

        if (old != s_build.draftable) {
            CoA::Events::Signal("DRAFTABLE_BUILDS_UPDATE", nullptr);
        }

        return 1;
    }

    int32_t OnDraftable(void*, uint32_t, uint32_t, CDataStore* msg) {
        const uint32_t index = CoA::Read::U32(msg);

        if (index >= kMaxDraftable) {
            return 1;
        }

        const std::string old = index < s_build.draftable.size() ? s_build.draftable[index] : std::string();

        if (index >= s_build.draftable.size()) {
            s_build.draftable.resize(index + 1);
        }

        s_build.draftable[index] = CoA::Read::Str(msg);

        if (s_build.draftable[index] != old) {
            CoA::Events::Signal("DRAFTABLE_BUILD_UPDATE", "%u", index + 1);
        }

        return 1;
    }

    int32_t OnDrafted(void*, uint32_t, uint32_t, CDataStore* msg) {
        s_build.drafted = CoA::Read::Str(msg);
        CoA::Events::Signal("DRAFTED_BUILD_UPDATE", nullptr);
        return 1;
    }

    int32_t OnRolePrompt(void*, uint32_t, uint32_t, CDataStore*) {
        CoA::Events::Signal("BUILD_DRAFT_ROLE_PROMPT", nullptr);
        s_build.awaitingRole = true;
        return 1;
    }

    int32_t OnSelectRoleResult(void*, uint32_t, uint32_t, CDataStore* msg) {
        const std::string result = CoA::Read::Str(msg);

        if (result == "BUILD_DRAFT_SELECT_ROLE_OK") {
            s_build.awaitingRole = false;
        }

        CoA::Events::Signal("BUILD_DRAFT_SELECT_ROLE_RESULT", "%s", result.c_str());
        return 1;
    }

    int32_t OnCompletedBuilds(void*, uint32_t, uint32_t, CDataStore* msg) {
        s_build.completed.clear();
        ReadList(msg, s_build.completed);
        return 1;
    }

    int32_t OnCompletedBuild(void*, uint32_t, uint32_t, CDataStore* msg) {
        const std::string name = CoA::Read::Str(msg);
        s_build.completed.push_back(name);
        CoA::Events::Signal("COMPLETED_BUILD_UPDATE", "%s", name.c_str());
        return 1;
    }

    bool Contains(const std::vector<std::string>& v, const std::string& s) {
        return std::find(v.begin(), v.end(), s) != v.end();
    }

    int IsCompletedBuild(lua_State* L) {
        std::string s;

        if (!CoA::Script::ReadString(L, s)) {
            return 0;
        }

        CoA::Script::PushBool(L, Contains(s_build.completed, s));
        return 1;
    }

    int IsDraftableBuild(lua_State* L) {
        std::string s;

        if (!CoA::Script::ReadString(L, s)) {
            return 0;
        }

        CoA::Script::PushBool(L, Contains(s_build.draftable, s));
        return 1;
    }

    int IsDraftedBuild(lua_State* L) {
        std::string s;

        if (!CoA::Script::ReadString(L, s)) {
            return 0;
        }

        CoA::Script::PushBool(L, s_build.drafted == s);
        return 1;
    }

    int GetNumDraftableBuilds(lua_State* L) {
        CoA::Script::PushInt(L, static_cast<int32_t>(s_build.draftable.size()));
        return 1;
    }

    // FUN_10111470: 1-based; out of range pushes "" (the static empty string 0x10BC9830).
    int GetDraftableBuild(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TNUMBER })) {
            return 0;
        }

        const int32_t i = CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1));

        if (i == 0) {
            return 0;
        }

        const uint32_t idx = static_cast<uint32_t>(i - 1);
        CoA::Script::PushStr(L, idx < s_build.draftable.size() ? s_build.draftable[idx].c_str() : "");
        return 1;
    }

    int GetDraftedBuild(lua_State* L) {
        CoA::Script::PushStr(L, s_build.drafted.c_str());
        return 1;
    }

    int IsAwaitingRolePrompt(lua_State* L) {
        CoA::Script::PushBool(L, s_build.awaitingRole);
        return 1;
    }

    int SelectRole(lua_State* L) {
        std::string role;

        if (!CoA::Script::ReadString(L, role)) {
            return 0;
        }

        CoA::Packet(0x63A).Data(role.c_str(), static_cast<uint32_t>(role.size() + 1)).Send();
        CoA::Script::PushBool(L, true);
        return 1;
    }

    void ResetState() {   // FUN_10110250
        s_build = State();
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_BuildDraft", "IsCompletedBuild", IsCompletedBuild),
        COA_NATIVE("C_BuildDraft", "IsDraftableBuild", IsDraftableBuild),
        COA_NATIVE("C_BuildDraft", "IsDraftedBuild", IsDraftedBuild),
        COA_NATIVE("C_BuildDraft", "GetNumDraftableBuilds", GetNumDraftableBuilds),
        COA_NATIVE("C_BuildDraft", "GetDraftableBuild", GetDraftableBuild),
        COA_NATIVE("C_BuildDraft", "GetDraftedBuild", GetDraftedBuild),
        COA_NATIVE("C_BuildDraft", "SelectRole", SelectRole),
        COA_NATIVE("C_BuildDraft", "IsAwaitingRolePrompt", IsAwaitingRolePrompt),
    };
}

void CoA::BuildDraft::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    CoA::Lifecycle::OnGlueScreen(ResetState);
    COA_PACKET(0x636, OnDraftables);
    COA_PACKET(0x637, OnDraftable);
    COA_PACKET(0x638, OnDrafted);
    COA_PACKET(0x639, OnRolePrompt);
    COA_PACKET(0x63B, OnSelectRoleResult);
    COA_PACKET(0x63C, OnCompletedBuilds);
    COA_PACKET(0x63D, OnCompletedBuild);
    COA_EVENT("DRAFTABLE_BUILDS_UPDATE");
    COA_EVENT("DRAFTABLE_BUILD_UPDATE");
    COA_EVENT("DRAFTED_BUILD_UPDATE");
    COA_EVENT("BUILD_DRAFT_ROLE_PROMPT");
    COA_EVENT("BUILD_DRAFT_SELECT_ROLE_RESULT");
    COA_EVENT("COMPLETED_BUILD_UPDATE");
}

void CoA::BuildDraft::Reset() {
    ResetState();
}
