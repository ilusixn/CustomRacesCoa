// ClientHooks' client half: the Engine hook pointers (Engine does not link CoA), the effect sweep after each idle
// tick (0x10277970) and the morph portrait refresh.
#include "CoA/Source/Runtime/ClientHooksClient.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "WoW/Source/Object/ObjectClient/Effect_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/UIUtil/PortraitButton.h"
#include "WoW/Source/Ui/GameUI.h"
#include <vector>

namespace {
    // 0x4943C0 / 0x494490 / 0x494530: the original's lists run only in the world (0xBD0792).
    void KeyEvent(int32_t kind, const void* data) {
        if (!CGGameUI::s_inWorld) {
            return;
        }

        if (kind == 0) {
            CoA::ClientHooks::beforeKeyDown.Run(data);
        } else if (kind == 1) {
            CoA::ClientHooks::beforeKeyUp.Run(data);
        } else {
            CoA::ClientHooks::beforeKeyRepeat.Run(data);
        }
    }

    // 0x1019C4C0: the visible units.
    int32_t CollectUnit(WOWGUID guid, void* param) {
        if (CGObject_C* unit = ClntObjMgrObjectPtr(guid, TYPE_UNIT, __FILE__, __LINE__)) {
            static_cast<std::vector<CGObject_C*>*>(param)->push_back(unit);
        }

        return 1;
    }

    // 0x81F8F0 CM2Scene::CreateModel, after: the afterCreateModel list.
    void ModelCreated(const char*, CM2Model* model) {
        CoA::ClientHooks::afterCreateModel.Run(model);
    }

    // 0x10277970 (after ClientIdle): every visible unit's effects are filtered. The original's guard compares
    // _time64 with a value nothing writes, so it always runs; with no filter registered there is nothing to do.
    void Sweep() {
        if (CoA::ClientHooks::removeEffect.Empty()) {
            return;
        }

        std::vector<CGObject_C*> units;
        ClntObjMgrEnumVisibleObjects(CollectUnit, &units);

        for (CGObject_C* unit : units) {
            CoA::ClientHooksClient::FilterEffects(unit);
        }
    }
}

void CoA::ClientHooksClient::FilterEffects(CGObject_C* object) {
    CEffect* effect = object->m_effectList;

    while (effect) {
        CEffect* next = effect->m_next;

        if (CoA::ClientHooks::removeEffect.AnyTrue(object, effect)) {
            effect->MoveToDestroyList();
        }

        effect = next;
    }
}

void CoA::ClientHooksClient::RefreshMorphPortrait(CGUnit_C* unit) {
    if (!unit || (unit->m_obj->m_type & TYPE_PLAYER) || unit->m_unit->m_unitFieldDisplayid == unit->m_unit->m_unitFieldNativedisplayid) {
        return;
    }

    const uint32_t displayId = unit->GetPortraitDisplayId();

    if (displayId && PortraitMarkDirty(displayId) && unit->m_obj->m_guid) {
        CGGameUI::UnitModelUpdate(unit->m_obj->m_guid, 3);
    }
}

void CoA::ClientHooks::InstallClient() {
    g_signalEventHook = CoA::ClientHooks::AllowSignalEvent;
    g_keyEventHook = KeyEvent;
    g_m2CreateModelHook = ModelCreated;
    CoA::Lifecycle::OnAfterIdle(Sweep);
}
