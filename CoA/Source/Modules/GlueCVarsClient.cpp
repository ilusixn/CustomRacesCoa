// The glue CVars' callbacks (registered null in Plan 2 until their modules came): the raid video settings
// (AscRaidVideo.cpp: callback FUN_102dcdb0, the tick FUN_102dd130 on every second SMSG_INITIAL_SPELLS, the restore
// FUN_102dcb90 on every glue screen, the Config.wtf writer FUN_102dc5f0 over 0x766640), the render module's
// entityShadows / lodObjectCull and the cursor size.
#include "CoA/Source/Modules/Cursor.h"
#include "CoA/Source/Modules/Graphics.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/RaidVideo.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/MapRec.h"
#include "WoW/Source/Ui/GameUI.h"
#include <cstring>
#include <string>

namespace {
    using CoA::RaidVideo::kCount;

    // 0x10BE38B4..DC: the base CVars' strings kept when the raid values went in (the original keeps the string
    // pointers; copies here, the same text).
    std::string s_kept[kCount];
    bool s_haveKept[kCount] = {};
    bool s_inRaid = false;   // 0x10BE38E0

    CVar* Base(size_t i) {
        return CVar::Lookup(CoA::RaidVideo::Setting(i).base);
    }

    const char* Str(CVar* cvar) {
        return cvar ? cvar->m_stringValue.m_str : nullptr;
    }

    // FUN_10114c40: the length check, then CVar::Set(value, 1, 0, 0, 0).
    void Set(CVar* cvar, const char* value) {
        if (!cvar || !value || strlen(value) + strlen(cvar->m_key.m_str) > 0xF9) {
            return;
        }

        cvar->Set(value, true, false, false, false);
    }

    bool Enabled() {
        CVar* use = CVar::Lookup("useRaidVideoSettings");
        return !use || use->m_intValue != 0;
    }

    MapRec* CurrentMap() {
        return g_mapDB.GetRecord(CGGameUI::m_iCurrentMapID);
    }

    bool InRaidMap() {
        MapRec* map = CurrentMap();
        return map && (map->m_instanceType == 2 || map->m_instanceType == 3);
    }

    void ApplyRaidValues() {
        for (size_t i = 0; i < kCount; ++i) {
            if (CVar* base = Base(i)) {
                Set(base, Str(CVar::Lookup(CoA::RaidVideo::Setting(i).raidName)));
            }
        }
    }

    // FUN_102dcb90 (also every glue screen).
    void Restore() {
        if (!Enabled()) {
            return;
        }

        for (size_t i = 0; i < kCount; ++i) {
            if (s_haveKept[i]) {
                Set(Base(i), s_kept[i].c_str());
            }
        }
    }

    // FUN_102dd130: a missing map row ends the tick without touching the flag.
    void Tick() {
        if (!Enabled() || !CurrentMap()) {
            return;
        }

        if (!InRaidMap()) {
            s_inRaid = false;
            Restore();
            return;
        }

        s_inRaid = true;

        for (size_t i = 0; i < kCount; ++i) {
            CVar* base = Base(i);

            if (!base) {
                continue;
            }

            s_kept[i] = Str(base) ? Str(base) : "";
            s_haveKept[i] = true;
            Set(base, Str(CVar::Lookup(CoA::RaidVideo::Setting(i).raidName)));
        }
    }

    // FUN_102dcdb0: only with useRaidVideoSettings on and in a raid / battleground.
    bool OnRaidChanged(CVar* cvar, const char*, const char* value, void*) {
        if (!Enabled() || !CurrentMap() || !InRaidMap()) {
            return true;
        }

        const std::string base = CoA::RaidVideo::BaseName(cvar->m_key.m_str);

        if (base.empty()) {
            ApplyRaidValues();
            return true;
        }

        Set(CVar::Lookup(base.c_str()), value);
        return true;
    }

    // FUN_102dc5f0: in a raid the base CVars' Config.wtf lines carry the kept values.
    const char* SaveValue(const char* name, const char* value) {
        if (s_inRaid) {
            for (size_t i = 0; i < kCount; ++i) {
                if (strcmp(name, CoA::RaidVideo::Setting(i).base) == 0) {
                    return s_haveKept[i] ? s_kept[i].c_str() : value;
                }
            }
        }

        return value;
    }
}

void CoA::GlueCVarsClient::Init() {
    CoA::CVars::SetCallback("useRaidVideoSettings", OnRaidChanged);

    for (size_t i = 0; i < kCount; ++i) {
        CoA::CVars::SetCallback(CoA::RaidVideo::Setting(i).raidName, OnRaidChanged);
    }

    CoA::CVars::SetCallback("entityShadows", CoA::Graphics::OnEntityShadows);
    CoA::CVars::SetCallback("lodObjectCull", CoA::Graphics::OnLodObjectCull);
    CoA::CVars::SetCallback("cursorSizePreferred", CoA::Cursor::OnSizePreferred);

    CoA::ClientHooks::initialSpellsAlternate.Add(Tick);
    CoA::Lifecycle::OnGlueScreen(Restore);
    g_cvarSaveValueHook = SaveValue;
}
