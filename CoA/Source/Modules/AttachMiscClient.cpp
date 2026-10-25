// AscAttachMisc.cpp's client half: the Engine hook pointers for the formatted signal guard and CVar::Set.
#include "CoA/Source/Modules/AttachMisc.h"
#include "CoA/Source/Modules/Modules.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/FrameScript/FrameScript.h"

namespace {
    // FUN_10a4a780: scriptProfile forced to "0"; nameplateShowEnemies posts NAMEPLATES_MESSAGE_ALL_OFF /
    // _ALL_ON_AUTO as UI_INFO_MESSAGE (stock event 0xBC).
    const char* CVarSet(CVar* cvar, const char* value) {
        const char* name = cvar->m_key.m_str;
        const char* set = CoA::AttachMisc::CVarSetValue(name, value);

        if (const char* key = CoA::AttachMisc::NameplateMessage(name, set)) {
            if (const char* text = FrameScript_GetText(key, -1, GENDER_NOT_APPLICABLE)) {
                FrameScript_SignalEvent(0xBC, "%s", text);
            }
        }

        return set;
    }
}

void CoA::AttachMiscClient::Init() {
    g_formattedSignalHook = CoA::AttachMisc::AllowFormattedSignal;
    g_cvarSetHook = CVarSet;
}
