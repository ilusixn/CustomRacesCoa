// AscBugTracker.cpp (see BugTracker.h): the bug report composer, its packets and the C_BugTracker / CreateBugReport
// natives, transcribed as the decompile has them.
#include "CoA/Source/Modules/BugTracker.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>

using CoA::Script::CheckNumber;
using CoA::Script::CheckString;
using CoA::Script::PushBool;
using CoA::Script::PushInt;
using CoA::Script::PushStr;
using CoA::Script::ToInt;
using CoA::Script::ValidateInput;

namespace {
    struct Report {
        uint32_t realmField = 0;   // +0x00
        uint32_t category = 9;     // +0x04
        uint32_t priority = 0;     // +0x08
        std::string title;         // +0x0C
        std::string description;   // +0x24
        bool isPublic = false;     // +0x3C
        int64_t lastSubmit = 0;    // +0x40
    };

    CoA::BugTracker::Deps s_deps;
    Report s_report;

    bool Gate() {   // FUN_10111ce0
        return CoA::ClientState::CanPerformAction(0x11);
    }

    int64_t Now() {
        return s_deps.now ? s_deps.now() : 0;
    }

    void Modified() {
        if (CoA::ClientState::InWorld()) {
            CoA::Events::Signal("BUG_REPORT_MODIFIED", nullptr);
        }
    }

    void Clear() {   // FUN_100e76f0
        s_report.realmField = CoA::RealmInfo::Get().field64;
        s_report.category = 9;
        s_report.priority = 0;
        s_report.title.clear();
        s_report.description.clear();
        s_report.isPublic = false;
        s_report.lastSubmit = 0;
        Modified();
    }

    void SetCategory(uint32_t v) {   // FUN_100e7fe0
        s_report.category = v;
        Modified();
    }

    void SetPriority(uint32_t v) {   // FUN_100e80e0
        s_report.priority = v;
        Modified();
    }

    void SetTitle(const std::string& v) {   // FUN_100e81e0
        s_report.title = v;
        Modified();
    }

    void SetDescription(const std::string& v) {   // FUN_100e8060
        s_report.description = v;
        Modified();
    }

    void SetPublic(bool v) {   // FUN_100e8160
        s_report.isPublic = v;
        Modified();
    }

    void Send() {   // FUN_100e71c0
        CoA::Packet p(0x562);
        p.U32(s_report.realmField).U32(s_report.category).U32(s_report.priority).U8(s_report.isPublic ? 1 : 0);
        p.U32(static_cast<uint32_t>(s_report.title.size()));

        if (!s_report.title.empty()) {
            p.Data(s_report.title.data(), static_cast<uint32_t>(s_report.title.size()));
        }

        p.U32(static_cast<uint32_t>(s_report.description.size()));

        if (!s_report.description.empty()) {
            p.Data(s_report.description.data(), static_cast<uint32_t>(s_report.description.size()));
        }

        p.Send();
        s_report.lastSubmit = Now();
    }

    bool ValidTitle(const std::string& s) {
        return !s.empty() && s.size() <= 0x80;
    }

    bool ValidDescription(const std::string& s) {
        return !s.empty() && s.size() <= 0x1000;
    }

    int ClearReport(lua_State*) {   // 0x100e7870
        if (Gate()) {
            Clear();
        }

        return 0;
    }

    // CreateBugReport(category, priority, title, description, public) (FUN_100e7890, argument parser FUN_100e6f50).
    int CreateBugReport(lua_State* L) {
        if (!Gate()) {
            return 0;
        }

        bool ok = ValidateInput(L, { LUA_TNUMBER, LUA_TNUMBER, LUA_TSTRING, LUA_TSTRING, LUA_TBOOLEAN });
        uint32_t category = 0;
        uint32_t priority = 0;
        std::string title;
        std::string description;
        bool isPublic = false;

        if (ok) {
            category = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));
            priority = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));
            title = CheckString(L, 3);
            description = CheckString(L, 4);
            isPublic = lua_toboolean(L, 5) != 0;
            ok = category <= 9 && priority <= 3 && ValidTitle(title) && ValidDescription(description);
        }

        if (ok) {
            SetCategory(category);
            SetPriority(priority);
            SetTitle(title);
            SetDescription(description);
            SetPublic(isPublic);
            Send();
        }

        PushBool(L, ok);
        return 1;
    }

    // FUN_100e7f30.
    int SubmitReport(lua_State* L) {
        if (!Gate()) {
            return 0;
        }

        bool ok = s_report.category <= 9 && s_report.priority <= 3 && ValidTitle(s_report.title) && ValidDescription(s_report.description);

        if (ok && s_report.lastSubmit != 0 && static_cast<double>(Now() - s_report.lastSubmit) < 2.0) {   // _difftime64
            ok = false;
        }

        if (ok) {
            Send();
        }

        PushBool(L, ok);
        return 1;
    }

    int GetReportCategory(lua_State* L) {   // 0x100e7a40
        if (!Gate()) {
            return 0;
        }

        PushInt(L, static_cast<int32_t>(s_report.category));
        return 1;
    }

    int GetReportPriority(lua_State* L) {   // 0x100e7ac0
        if (!Gate()) {
            return 0;
        }

        PushInt(L, static_cast<int32_t>(s_report.priority));
        return 1;
    }

    int GetReportTitle(lua_State* L) {   // 0x100e7af0
        if (!Gate()) {
            return 0;
        }

        PushStr(L, s_report.title.c_str());
        return 1;
    }

    int GetReportDescription(lua_State* L) {   // 0x100e7a70
        if (!Gate()) {
            return 0;
        }

        PushStr(L, s_report.description.c_str());
        return 1;
    }

    int IsReportPublic(lua_State* L) {   // 0x100e7b40
        if (!Gate()) {
            return 0;
        }

        PushBool(L, s_report.isPublic);
        return 1;
    }

    int SetReportCategory(lua_State* L) {   // 0x100e7b70
        if (!Gate()) {
            return 0;
        }

        uint32_t v;
        const bool ok = CoA::Script::ReadNumber(L, v) && v < 10;

        if (ok) {
            SetCategory(v);
        }

        PushBool(L, ok);
        return 1;
    }

    int SetReportPriority(lua_State* L) {   // 0x100e7ce0
        if (!Gate()) {
            return 0;
        }

        uint32_t v;
        const bool ok = CoA::Script::ReadNumber(L, v) && v < 4;

        if (ok) {
            SetPriority(v);
        }

        PushBool(L, ok);
        return 1;
    }

    int SetReportTitle(lua_State* L) {   // 0x100e7e30
        if (!Gate()) {
            return 0;
        }

        std::string v;
        const bool ok = CoA::Script::ReadString(L, v) && ValidTitle(v);

        if (ok) {
            SetTitle(v);
        }

        PushBool(L, ok);
        return 1;
    }

    int SetReportDescription(lua_State* L) {   // 0x100e7be0
        if (!Gate()) {
            return 0;
        }

        std::string v;
        const bool ok = CoA::Script::ReadString(L, v) && ValidDescription(v);

        if (ok) {
            SetDescription(v);
        }

        PushBool(L, ok);
        return 1;
    }

    int SetReportPublic(lua_State* L) {   // 0x100e7d50
        if (!Gate()) {
            return 0;
        }

        const bool ok = ValidateInput(L, { LUA_TBOOLEAN });

        if (ok) {
            SetPublic(lua_toboolean(L, 1) != 0);
        }

        PushBool(L, ok);
        return 1;
    }

    int32_t OnReportError(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x565 FUN_100e7340
        const std::string text = CoA::Read::Name(msg);
        CoA::Events::Signal("CREATE_BUG_REPORT_ERROR", "%s", text.c_str());
        return 1;
    }

    int32_t OnReportSuccess(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x566 FUN_100e7540
        const uint32_t id = CoA::Read::U32(msg);
        Clear();
        CoA::Events::Signal("CREATE_BUG_REPORT_SUCCESS", "%u", id);
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "CreateBugReport", CreateBugReport),
        COA_NATIVE("C_BugTracker", "SubmitReport", SubmitReport),
        COA_NATIVE("C_BugTracker", "ClearReport", ClearReport),
        COA_NATIVE("C_BugTracker", "GetReportCategory", GetReportCategory),
        COA_NATIVE("C_BugTracker", "SetReportCategory", SetReportCategory),
        COA_NATIVE("C_BugTracker", "GetReportPriority", GetReportPriority),
        COA_NATIVE("C_BugTracker", "SetReportPriority", SetReportPriority),
        COA_NATIVE("C_BugTracker", "GetReportTitle", GetReportTitle),
        COA_NATIVE("C_BugTracker", "SetReportTitle", SetReportTitle),
        COA_NATIVE("C_BugTracker", "GetReportDescription", GetReportDescription),
        COA_NATIVE("C_BugTracker", "SetReportDescription", SetReportDescription),
        COA_NATIVE("C_BugTracker", "IsReportPublic", IsReportPublic),
        COA_NATIVE("C_BugTracker", "SetReportPublic", SetReportPublic),
    };
}

CoA::BugTracker::Deps& CoA::BugTracker::Dependencies() {
    return s_deps;
}

void CoA::BugTracker::Init() {   // 0x100e76a0
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    COA_EVENT("BUG_REPORT_MODIFIED");
    COA_EVENT("CREATE_BUG_REPORT_ERROR");
    COA_EVENT("CREATE_BUG_REPORT_SUCCESS");
    CoA::Lifecycle::OnGlueScreen(Clear);         // FUN_10278760
    CoA::ClientHooks::enterWorld.Add(Clear);     // FUN_10278510
    COA_PACKET(0x565, OnReportError);
    COA_PACKET(0x566, OnReportSuccess);
}

void CoA::BugTracker::Reset() {
    s_report = Report();
    s_deps = Deps();
}
