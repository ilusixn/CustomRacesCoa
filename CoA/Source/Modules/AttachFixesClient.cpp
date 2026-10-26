// AscAttachFixes.cpp's client half: the hooks' reads of client objects, the Engine hook pointers (Engine does not
// link CoA), the window centring on gxMonitor, the role conditions and SMSG 0x65C.
#include "CoA/Source/Modules/AttachFixesClient.h"
#include "CoA/Source/Modules/AttachFixes.h"
#include "CoA/Source/Modules/Config.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/SpellText.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/Gx/CGxDeviceD3d/CGxDeviceD3d.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/MapRec.h"
#include "WoW/Source/Glue/CGlueMgr.h"
#include "WoW/Source/Object/ObjectClient/AuraData.h"
#include "WoW/Source/Object/ObjectClient/Container_C.h"
#include "WoW/Source/Object/ObjectClient/Item_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/AuctionHouse.h"
#include "WoW/Source/Ui/ContainerFrame.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/QuestLog.h"
#include "WoW/Source/Ui/ScriptEvents.h"
#include <windows.h>

extern "C" IMAGE_DOS_HEADER __ImageBase;
// lapi.c (compiled as C++).
extern void (*g_luaPushCClosureHook)(lua_CFunction fn);

namespace {
    bool CVarOn(const char* name) {
        CVar* cvar = CVar::Lookup(name);
        return cvar && cvar->m_intValue == 1;
    }

    // FUN_10a4e970.
    bool RoleCondition(int32_t type, uint32_t value) {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (!player) {
            return false;
        }

        if (type == 0) {
            return true;
        }

        if (type == 1) {
            // AscCA::SpellKnown(value, true): character advancement's answer, false until it is ported.
            bool (*known)(uint32_t, bool) = CoA::SpellText::Dependencies().spellKnown;
            return value && known && known(value, true);
        }

        if (type == 2 && value) {
            for (uint32_t i = 0; i < player->GetAuraCount(); ++i) {
                AuraData* aura = player->GetAura(i);

                if (aura && static_cast<uint32_t>(aura->m_spellId) == value) {
                    return true;
                }
            }
        }

        return false;
    }

    // 0xBD0793 (set by CGGameUI::EnterWorld's load, cleared when it finishes) and 0xB6AA2C.
    bool WorldLoading() {
        return CGGameUI::m_loggingIn != 0 && CGlueMgr::m_suspended != 0;
    }

    // 0x81B5F0 over the 722 stock names; CoA::Events::ExtendList appends the customs.
    void RebuildEvents() {
        FrameScript_CreateEvents(g_scriptEvents, NUM_SCRIPT_EVENTS);
    }

    void RegisterScriptEvent(const char* name) {
        CoA::AttachFixes::AddUnknownEventWhileLoading(name);
    }

    void OpenFailed(const char* name) {
        CoA::AttachFixes::LogMissingFileOnce(name);
    }

    const char* MarkerModelPath() {
        return CoA::AttachFixes::CurrentMarkerModel();
    }

    // sub_10a78a30: a C function whose first byte is int3. The mute flag DAT_10BDB1B0 has no setter here.
    void PushCClosure(lua_CFunction fn) {
        if (*reinterpret_cast<const uint8_t*>(fn) == 0xCC) {
            CoA::AttachFixes::SendTamperReport("FrameScript_RegisterFunction");
        }
    }

    // handler_0x065c: u64 guid, u32 kind, u32 state. The object rebuilds its marker (UpdateInteractIcon, vtable +0x54)
    // with the path in force, which DuplicateModel loads; then the path is cleared.
    int32_t OnMarkerModel(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
        WOWGUID guid;
        guid.m_guid = CoA::Read::U64(msg);
        const uint32_t kind = CoA::Read::U32(msg);
        const int32_t state = static_cast<int32_t>(CoA::Read::U32(msg));

        if (const char* path = CoA::AttachFixes::MarkerModel(kind, state)) {
            CoA::AttachFixes::CurrentMarkerModel() = path;
        }

        if (CGObject_C* object = ClntObjMgrObjectPtr(guid, TYPE_OBJECT, __FILE__, __LINE__)) {
            object->UpdateInteractIcon();
        }

        CoA::AttachFixes::CurrentMarkerModel() = "";
        return 1;
    }

    // LAB_10a4ea10: counts up from gxMonitor's value and takes the monitor at which the count is 1.
    HMONITOR s_centreMonitor = nullptr;

    BOOL CALLBACK FindMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM data) {
        int32_t* count = reinterpret_cast<int32_t*>(data);

        if (*count == 1) {
            s_centreMonitor = monitor;
            return FALSE;
        }

        ++*count;
        return TRUE;
    }

    // FUN_10a46400 after CGxDeviceD3d::DeviceSetFormat: centre the foreground window on monitor gxMonitor.
    void CentreOnMonitor() {
        CVar* cvar = CVar::Lookup("gxMonitor");

        if (!cvar) {
            return;
        }

        int32_t count = cvar->m_intValue;
        HWND window;

        if (count == 0 || !(window = GetForegroundWindow())) {
            return;
        }

        s_centreMonitor = nullptr;
        EnumDisplayMonitors(nullptr, nullptr, &FindMonitor, reinterpret_cast<LPARAM>(&count));

        if (!s_centreMonitor) {
            return;
        }

        MONITORINFO info = {};
        info.cbSize = sizeof(info);
        RECT rect;

        if (!GetMonitorInfoA(s_centreMonitor, &info) || !GetWindowRect(window, &rect)) {
            return;
        }

        const LONG w = rect.right - rect.left;
        const LONG h = rect.bottom - rect.top;
        const RECT* area = &info.rcWork;

        if (info.rcWork.right - info.rcWork.left < w || info.rcWork.bottom - info.rcWork.top < h) {
            area = &info.rcMonitor;
        }

        LONG x = area->left;
        LONG y = area->top;
        const LONG cx = ((area->right - x) - w) / 2 + x;
        const LONG cy = ((area->bottom - y) - h) / 2 + y;
        const LONG px = cx <= x ? cx : cx - 1;

        if (x <= px) {
            x = px + w <= area->right ? px : area->right - w;
        }

        if (y <= cy) {
            y = cy + h <= area->bottom ? cy : area->bottom - h;
        }

        SetWindowPos(window, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    // FUN_1008d8c0: how many of an item the player carries in the backpack and the four equipped bags.
    int32_t CountItem(CGPlayer_C* player, uint32_t itemId) {
        int32_t n = 0;

        for (uint32_t i = 0; i < 16; ++i) {
            CGItem_C* item = static_cast<CGItem_C*>(ClntObjMgrObjectPtr(player->m_player->m_playerFieldPackSlot1[i], TYPE_ITEM, __FILE__, __LINE__));

            if (item && item->m_obj->m_entryID == itemId) {
                n += item->m_item->m_itemFieldStackCount;
            }
        }

        for (uint32_t i = 0; i < 4; ++i) {
            CGContainer_C* container = static_cast<CGContainer_C*>(ClntObjMgrObjectPtr(CGContainerInfo::m_containers[i], TYPE_CONTAINER, __FILE__, __LINE__));

            if (!container) {
                continue;
            }

            CGBag_C* bag = container->GetBag();

            if (!bag) {
                continue;
            }

            for (uint32_t k = 0; k < container->m_container->m_containerFieldNumSlots; ++k) {
                CGItem_C* item = bag->GetItemPointer(static_cast<int32_t>(k));

                if (item && item->m_obj->m_entryID == itemId) {
                    n += item->m_item->m_itemFieldStackCount;
                }
            }
        }

        return n;
    }
}

int32_t CoA::AttachFixesClient::CountCarried(CGPlayer_C* player, uint32_t itemId) {
    return CountItem(player, itemId);
}

bool CoA::AttachFixesClient::QuestRelevant(CGPlayer_C* player, uint32_t entry, bool gameObject) {
    const uint32_t* questItems = nullptr;

    if (gameObject) {
        GameObjectStats_C* stats = g_gameObjectDBCache.GetRecord(static_cast<int32_t>(entry), WOWGUID(), nullptr, nullptr, false);
        questItems = stats ? stats->m_questItems : nullptr;
    } else {
        CreatureStats_C* stats = g_creatureDBCache.GetRecord(static_cast<int32_t>(entry), WOWGUID(), nullptr, nullptr, false);
        questItems = stats ? stats->m_questItem : nullptr;
    }

    if (!questItems) {
        return false;
    }

    for (int32_t e = 0; e < CGQuestLog::m_numQuest; ++e) {
        const QuestLogEntry& log = CGQuestLog::m_quests[e];

        if (log.m_isHeader) {
            continue;
        }

        const uint32_t questId = static_cast<uint32_t>(log.m_questId);

        for (uint32_t slot = 0; slot < 25; ++slot) {
            const PlayerQuestLogEntry& logSlot = player->m_player->m_playerQuestLog[slot];
            const uint32_t id = static_cast<uint16_t>(logSlot.m_questId);   // the original reads 16 bits

            if (id == 0) {
                break;
            }

            if (id != questId) {
                continue;
            }

            QuestCache* quest = g_questDBCache.GetRecord(static_cast<int32_t>(questId), WOWGUID(), nullptr, nullptr, false);

            if (!quest) {
                break;
            }

            for (uint32_t i = 0; i < 4; ++i) {
                if (quest->m_objectiveId[i] == entry && quest->m_objectiveRequiredCount[i] != logSlot.m_counts[i]) {
                    return true;
                }
            }

            for (uint32_t k = 0; k < 6; ++k) {
                const uint32_t item = questItems[k];

                if (!item) {
                    continue;
                }

                for (uint32_t i = 0; i < 6; ++i) {
                    if (item == quest->m_collectItemId[i] && static_cast<int32_t>(quest->m_collectItemCount[i]) != CountItem(player, item)) {
                        return true;
                    }
                }
            }

            break;
        }
    }

    return false;
}

bool CoA::AttachFixesClient::ForceGameObjectName(const CGObject_C* object) {
    if (object->m_typeId != ID_GAMEOBJECT || !CVarOn("UnitNameGO")) {
        return false;
    }

    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
    return player && CoA::AttachFixesClient::QuestRelevant(player, object->m_obj->m_entryID, true);
}

int32_t CoA::AttachFixesClient::CreatureName(const CGUnit_C* unit, int32_t result) {
    if (result == 0 || unit->m_typeId != ID_UNIT) {
        return result;
    }

    // The five CVars are AscClientOptions' world CVars (ClientOptions.cpp); until the world registers them the client's
    // answer stands (the original reads their slots, which the world load has filled by then).
    if (!CVar::Lookup("UnitNameAllNPC")) {
        return result;
    }

    CGUnit_C* self = const_cast<CGUnit_C*>(unit);

    if (self->HasAuraBySpellId(0x214466) && unit->m_obj->m_guid != CGGameUI::m_lockedTarget) {
        return 0;
    }

    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    if (CVarOn("UnitNameInteractiveNPC") && player && player->CanInteract(unit)) {
        return 1;
    }

    if (CVarOn("UnitNameQuestNPC")) {
        if (unit->m_questStatus != 0 && unit->m_questStatus != 1) {
            return 1;
        }

        if (player && CoA::AttachFixesClient::QuestRelevant(player, unit->m_obj->m_entryID, false)) {
            return 1;
        }
    }

    if (CVarOn("UnitNameHostileNPC") && player && player->UnitReaction(unit) <= 1) {
        return 1;
    }

    return CVarOn("UnitNameAllNPC") ? 1 : 0;
}

uint32_t CoA::AttachFixesClient::AuctionDeposit(uint32_t depositRate, uint32_t sellPrice, uint32_t runTime) {
    if (!CGAuctionHouse::m_sellItem) {
        return 0;
    }

    CGItem_C* item = static_cast<CGItem_C*>(ClntObjMgrObjectPtr(CGAuctionHouse::m_sellItem, TYPE_ITEM, __FILE__, __LINE__));

    if (!item) {
        return 0;
    }

    ItemStats_C* stats = g_itemDBCache.GetRecord(static_cast<int32_t>(item->m_obj->m_entryID), WOWGUID(), nullptr, nullptr, false);

    if (!stats) {
        return 0;
    }

    const float* rate = CoA::Config::Rate("RATE_AUCTION_DEPOSIT");   // FUN_10196bd0(name, 1.0)
    return static_cast<uint32_t>(CoA::AttachFixes::AuctionDeposit(depositRate, sellPrice, runTime, stats->m_quality, rate ? *rate : 1.0f));
}

int32_t CoA::AttachFixesClient::CurrentMapInstanceType() {
    MapRec* map = g_mapDB.GetRecord(CGGameUI::m_iCurrentMapID);
    return map ? map->m_instanceType : 0;
}

bool CoA::AttachFixesClient::CalledFromOutside(const void* returnAddress) {
    const uint8_t* base = reinterpret_cast<const uint8_t*>(&__ImageBase);
    const IMAGE_NT_HEADERS* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + __ImageBase.e_lfanew);
    const uint8_t* at = static_cast<const uint8_t*>(returnAddress);
    return at < base || at >= base + nt->OptionalHeader.SizeOfImage;
}

void CoA::AttachFixesClient::Init() {
    CoA::AttachFixes::Deps& deps = CoA::AttachFixes::Dependencies();
    deps.roleCondition = RoleCondition;
    deps.worldLoading = WorldLoading;
    deps.eventId = CoA::Events::Id;
    deps.rebuildEvents = RebuildEvents;

    g_registerScriptEventHook = RegisterScriptEvent;
    g_sfileOpenFailedHook = OpenFailed;
    g_m2DuplicateModelHook = MarkerModelPath;
    g_gxDeviceSetFormatHook = CentreOnMonitor;
    g_luaPushCClosureHook = PushCClosure;

    COA_PACKET(0x65C, OnMarkerModel);

    // CoA: AscAttachFixes.cpp:sub_10a4c790 (orig hook 0x4181B0) -- not placed: in the 32-bit client 0x4181B0 is a CRT
    // 80-bit float helper (callers 0x41464F / 0x894875), not FrameScript_RegisterFunction, and its report needs a
    // caller outside the image, which a single-image x64 client cannot have. The 0x84E400 half is PushCClosure.
}
