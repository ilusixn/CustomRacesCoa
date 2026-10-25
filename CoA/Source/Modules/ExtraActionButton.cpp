// AscExtraActionButton.cpp (see ExtraActionButton.h).
#include "CoA/Source/Modules/ExtraActionButton.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "CoA/Source/Runtime/Timer.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <cstring>
#include <vector>

namespace {
    CoA::Dbc::Table& Buttons() {
        return CoA::Dbc::Get("DBFilesClient/ExtraActionButtons.dbc");
    }

    std::vector<uint32_t> s_active;   // 0x10be24f8
    uint32_t s_timer = 0;             // 0x10be24e8
    CoA::ExtraActionButton::Deps s_deps;

    // PTR_s_TRIGGER_TYPE_NONE_10b3c1a4, parsed by FUN_1020be20
    const char* const kTriggerTypes[] = {
        "TRIGGER_TYPE_NONE", "TRIGGER_TYPE_AURA_EXISTS", "TRIGGER_TYPE_AURA_DOES_NOT_EXIST", "TRIGGER_TYPE_COOLDOWN_READY",
        "TRIGGER_TYPE_ITEM_COOLDOWN_READY", "TRIGGER_TYPE_ITEM_IN_BAGS", "TRIGGER_TYPE_MAX",
    };

    // An unknown name logs and reads as NONE.
    uint32_t TriggerType(const char* name) {
        for (uint32_t i = 0; i < 7; ++i) {
            if (name && strcmp(name, kTriggerTypes[i]) == 0) {
                return i;
            }
        }

        CoA::Log::Printf("Unexpected Value: %s", name ? name : "");
        return 0;
    }

    uint32_t Now() {
        return s_deps.now ? s_deps.now() : 0;
    }

    bool Ready(const CoA::ExtraActionButton::Cooldown& c) {
        return c.duration + c.start <= Now();
    }

    // 0x10236840
    bool Triggered(uint32_t id) {
        CoA::Dbc::Table& t = Buttons();
        const uint8_t* row = t.Row(id);

        if (!row || !s_deps.hasPlayer || !s_deps.hasPlayer()) {
            return false;
        }

        const uint32_t arg = CoA::Dbc::Table::U32(row, 0x18);

        switch (TriggerType(t.Str(row, 0x14))) {
            case 1:
                return s_deps.hasAura && s_deps.hasAura(arg);
            case 2:
                return !(s_deps.hasAura && s_deps.hasAura(arg));
            case 3:
                return Ready(s_deps.spellCooldown ? s_deps.spellCooldown(arg) : CoA::ExtraActionButton::Cooldown {});
            case 4:
                return Ready(s_deps.itemCooldown ? s_deps.itemCooldown(arg) : CoA::ExtraActionButton::Cooldown {});
            case 5:
                return s_deps.hasItem && s_deps.hasItem(arg);
            default:
                return false;
        }
    }

    bool Contains(uint32_t id) {
        return std::find(s_active.begin(), s_active.end(), id) != s_active.end();
    }

    // 0x10236ca0: re-arms itself without storing the new id (so Stop cancels only the first one).
    int32_t OnTimer(const void*, void*) {
        CoA::ExtraActionButton::Tick();
        CoA::Timer::Schedule(100, OnTimer, nullptr);
        return 0;
    }

    // handler_GetExtraActionButtonInfo: row -> (str +8, str +0xC, int +4, str +0x10), else four nils.
    int GetExtraActionButtonInfo(lua_State* L) {
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        CoA::Dbc::Table& t = Buttons();
        const uint8_t* row = t.Row(id);

        if (!row) {
            return CoA::Script::PushNils(L, 4);
        }

        CoA::Script::PushStr(L, t.Str(row, 8));
        CoA::Script::PushStr(L, t.Str(row, 0xC));
        CoA::Script::PushInt(L, CoA::Dbc::Table::I32(row, 4));
        CoA::Script::PushStr(L, t.Str(row, 0x10));
        return 4;
    }

    // handler_GetExtraActionButtonAtIndex: 1-based; nil for an out-of-range index or a zero id, nothing for index 0.
    int GetExtraActionButtonAtIndex(lua_State* L) {
        uint32_t index;

        if (!CoA::Script::ReadNumber(L, index) || index == 0) {
            return 0;
        }

        if (index - 1 < s_active.size() && s_active[index - 1]) {
            CoA::Script::PushInt(L, static_cast<int32_t>(s_active[index - 1]));
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int GetNumExtraActionButtons(lua_State* L) {
        CoA::Script::PushInt(L, static_cast<int32_t>(s_active.size()));
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_ExtraActionButton", "GetExtraActionButtonInfo", GetExtraActionButtonInfo),
        COA_NATIVE("C_ExtraActionButton", "GetNumExtraActionButtons", GetNumExtraActionButtons),
        COA_NATIVE("C_ExtraActionButton", "GetExtraActionButtonAtIndex", GetExtraActionButtonAtIndex),
    };
}

CoA::ExtraActionButton::Deps& CoA::ExtraActionButton::Dependencies() {
    return s_deps;
}

void CoA::ExtraActionButton::Tick() {
    CoA::Dbc::Table& t = Buttons();
    std::vector<uint32_t> added;

    for (uint32_t id = t.MinId(); t.Loaded() && id <= t.MaxId(); ++id) {
        const bool active = Contains(id);
        const bool on = Triggered(id);

        if (!active) {
            if (on) {
                added.push_back(id);
            }
        } else if (!on) {
            s_active.erase(std::remove(s_active.begin(), s_active.end(), id), s_active.end());
            CoA::Events::Signal("EXTRA_ACTION_BUTTON_REMOVED", "%u", id);
        }
    }

    // FUN_10236580: introsort by the row's +0x24 order.
    std::sort(added.begin(), added.end(), [&](uint32_t a, uint32_t b) {
        const uint8_t* ra = t.Row(a);
        const uint8_t* rb = t.Row(b);
        return (ra ? CoA::Dbc::Table::U32(ra, 0x24) : 0) < (rb ? CoA::Dbc::Table::U32(rb, 0x24) : 0);
    });

    for (uint32_t id : added) {
        if (Contains(id)) {
            continue;
        }

        s_active.push_back(id);
        CoA::Events::Signal("EXTRA_ACTION_BUTTON_ADDED", "%u", id);
    }
}

void CoA::ExtraActionButton::Start() {
    s_timer = CoA::Timer::Schedule(100, OnTimer, nullptr);
}

void CoA::ExtraActionButton::Stop() {
    s_active.clear();

    if (s_timer) {
        CoA::Timer::Cancel(s_timer, OnTimer, nullptr);
        s_timer = 0;
    }
}

void CoA::ExtraActionButton::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    CoA::Lifecycle::OnGlueScreen(Stop);
    COA_EVENT("EXTRA_ACTION_BUTTON_ADDED");
    COA_EVENT("EXTRA_ACTION_BUTTON_REMOVED");
    COA_EVENT("HIDE_ALL_EXTRA_ACTION_BUTTONS");
}

void CoA::ExtraActionButton::Reset() {
    s_active.clear();
    s_timer = 0;
    s_deps = {};
}
