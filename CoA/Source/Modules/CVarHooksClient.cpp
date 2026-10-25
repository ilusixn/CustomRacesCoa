// The CVar module's client hooks (port of AscCVarHooks.cpp) and AscSmallHooks.cpp's login-state hook.
//
//   0x4E4610  first loads "WTF\Account\<account>\config-cache.wtf" when an account name is known; wrapped by the
//             "-character" select (a second hook object on the same target)
//   0x512890  SaveGameCVars: a file handle of -1 is refused with a Fatal log line; after a save with category bit
//             0x10, lastCharacterIndex is written as the character-select index -- the one name on the DLL's list
//   0x766640  writing a CVar line: names on that list are skipped
//   0x76A220  the loading screen's two aspect values follow the resolution string before the client applies it
//   0x6B2F60  the login-state step (FUN_10194af0)
#include "CoA/Source/Modules/CVarHooks.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Logger.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "WoW/Source/Glue/CGlueMgr.h"
#include "WoW/Source/Glue/CharacterSelection.h"
#include "WoW/Source/LoadingScreen.h"
#include "WoW/Source/WowServices/Types.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"
#include <cstdio>
#include <cstring>

namespace {
    const char* const s_savedNames[] = { "lastCharacterIndex" };   // 0x10BDD188

    bool Listed(const char* name) {
        for (const char* n : s_savedNames) {
            if (strcmp(name, n) == 0) {
                return true;
            }
        }

        return false;
    }

    bool s_characterDone = false;   // 0x10BDEC0C
    bool s_realmArgDone = false;    // 0x10BDEC0A

    float AspectBits(uint32_t bits) {
        float value;
        memcpy(&value, &bits, sizeof(value));
        return value;
    }
}

// FUN_10113040.
void CoA::CVarHooks::BeforeUpdateCharacterList() {
    const char* account = ClientServices::GetAccountName();

    if (account && *account) {
        char path[0x104] = {};
        _snprintf_s(path, sizeof(path), _TRUNCATE, "WTF\\Account\\%s\\config-cache.wtf", account);
        CVar::Load(path);
    }
}

// LAB_10194a40: on the first call only (set whether or not the argument is given), "-character name" picks the
// first character in the list with exactly that name, makes it the selection and enters the world. The original
// steps the records by 0x188 while the client's are 0x198, so only the first record is read correctly: here only the
// first character can match.
void CoA::CVarHooks::AfterUpdateCharacterList() {
    if (s_characterDone) {
        return;
    }

    s_characterDone = true;
    const char* name = CoA::Script::CommandLineArg("character");

    if (!name || !CCharacterSelection::s_characterList.Count()) {
        return;
    }

    if (strcmp(CCharacterSelection::s_characterList[0].m_characterInfo.m_name, name) == 0) {
        CCharacterSelection::s_selectionIndex = 0;
        CGlueMgr::EnterWorld();
    }
}

void CoA::CVarHooks::InvalidSaveFile() {
    CoA::Logger::Write(CoA::Logger::Fatal,
        "SaveGameCVarsCallBack_: handle is -1 (invalid). Skipping save to prevent System_File__write_overlapped access violation crash.");
}

// FUN_10114640's tail.
void CoA::CVarHooks::AfterSaveGameCVars(void* file, uint32_t category) {
    if (!(category & 0x10)) {
        return;
    }

    for (const char* name : s_savedNames) {
        CVar* var = CVar::Lookup(name);

        if (!var) {
            continue;
        }

        const char* value = var->m_stringValue.m_str;
        char index[16];

        if (strcmp(name, "lastCharacterIndex") == 0) {
            snprintf(index, sizeof(index), "%d", CCharacterSelection::s_selectionIndex);
            value = index;
        } else if (!value) {
            value = "0";
        }

        CVar::WriteCvarLine(name, value, file);
    }
}

// FUN_101130c0.
void CoA::CVarHooks::BeforeResolution(const char* resolution) {
    static const char* const wideModes[] = { "1920x1080", "1600x900", "1360x768", "1366x768", "1280x720", "1176x664" };
    bool wide = false;

    for (const char* w : wideModes) {
        wide = wide || strcmp(resolution, w) == 0;
    }

    uint32_t first = 0x3F800000;
    uint32_t second;

    if (wide) {
        second = 0x3FAAAAA8;
    } else if (strcmp(resolution, "1768x992") == 0) {
        second = 0x3FAB851F;
    } else if (strcmp(resolution, "1280x768") == 0 || strcmp(resolution, "1600x1024") == 0) {
        second = 0x3FAA3D71;
    } else {
        first = 0x3FAAAAA8;
        second = 0x3FE38E32;
    }

    s_normalAspect = AspectBits(first);
    s_wideAspect = AspectBits(second);
}

// FUN_10194af0, before the original: the jne at 0x6B2FD4 is NOPed while realmList has no value.
bool CoA::CVarHooks::RealmListMissing() {
    CVar* realmList = CVar::Lookup("realmList");
    return realmList && realmList->m_stringValue.m_str == nullptr;
}

// FUN_10194af0, after the original: once per process, a non-empty "-realm <name>" and the realmList value go to
// CGlueMgr::ChangeRealm ({6 unused bytes, name[256], realmList[...]}; the original copies both unbounded, the address
// here is truncated to its 32 bytes).
void CoA::CVarHooks::AfterRealmEnumCallback() {
    if (s_realmArgDone) {
        return;
    }

    s_realmArgDone = true;
    const char* realm = CoA::Script::CommandLineArg("realm");

    if (!realm || !*realm) {
        return;
    }

    REALM_INFO select = {};
    strncpy(select.m_realmName, realm, sizeof(select.m_realmName) - 1);
    CVar* realmList = CVar::Lookup("realmList");

    if (realmList && realmList->m_stringValue.m_str) {
        strncpy(select.m_realmAddress, realmList->m_stringValue.m_str, sizeof(select.m_realmAddress) - 1);
    }

    CGlueMgr::ChangeRealm(&select);
}

void CoA::CVarHooksClient::Init() {
    // 0x766640: lines for the listed names are not written by the client's CVar save.
    g_cvarSaveFilterHook = Listed;
}
