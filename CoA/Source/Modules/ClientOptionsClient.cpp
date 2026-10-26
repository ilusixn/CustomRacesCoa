// AscClientOptions.cpp's client half: the Deps and what the decomp sites ask with client objects.
#include "CoA/Source/Modules/ClientOptions.h"
#include "CoA/Source/Modules/ClientOptionsClient.h"
#include "CoA/Source/Modules/Modules.h"
#include "WoW/Source/DB/Rec/SpellVisualEffectNameRec.h"
#include "WoW/Source/Game/GameClient/WorldText.h"
#include "WoW/Source/Object/ObjectClient/Effect_C.h"
#include "WoW/Source/Object/ObjectClient/Object_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Trade_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/Object/Unit.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/SoundInterface/SoundInterface2.h"
#include "WoW/Source/Ui/GameUI.h"

namespace {
    bool InWorld() {   // 0xBD0792
        return CGGameUI::s_inWorld != 0;
    }

    void RegisterUiScaleCVars() {   // 0x5238A0
        CGGameUI::Initialize();
    }

    void ReloadSelectionTexture() {   // 0x7460C0
        CGObject_C::Initialize();
    }

    // sub_10a3ab20: each visible unit's radius through the (hooked) 0x720330.
    int32_t RecomputeRadius(WOWGUID guid, void*) {
        if (CGUnit_C* unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(guid, TYPE_UNIT, __FILE__, __LINE__))) {
            unit->UpdateSelectionRadius();
        }

        return 1;
    }

    void RecomputeSelectionRadii() {   // 0x4D4B30
        ClntObjMgrEnumVisibleObjects(RecomputeRadius, nullptr);
    }

    void SetLootArtScale(float scale) {   // 0x6F75B0(4) +0x10
        if (const SpellVisualEffectNameRec* rec = GetSpecialSpellVisualEffectNameRec(4)) {
            const_cast<SpellVisualEffectNameRec*>(rec)->m_scale = scale;
        }
    }

    void SetWorldTextSize(uint32_t type, float width, float height) {   // 0xAF4748 + type * 0x1C, +0x10 / +0x14
        WORLDTEXTTYPE* style = WorldTextTypeStyle(type);
        style->m_startHeight = width;
        style->m_endHeight = height;
    }

    void SetWorldTextOther(uint32_t index, float value) {   // 0xAF487C + index * 4
        WorldTextSetScaleFloat(index, value);
    }

    void PlaySound(const char* kit, float x, float y, float z) {   // 0x4C74A0(kit, pos, 0, 0)
        C3Vector position(x, y, z);
        SI2::PlaySoundKit(kit, &position, nullptr, nullptr);
    }

    const char* TraderName() {   // 0xCA0FE8 as a player (0x10), vtable +0xD8
        CGObject_C* trader = ClntObjMgrObjectPtr(Trade_C_GetTradeTarget(), TYPE_PLAYER, __FILE__, __LINE__);
        return trader ? trader->GetObjectName() : nullptr;
    }
}

void CoA::ClientOptionsClient::BeforeChatBubbleUpdate(CGUnit_C* unit) {
    CoA::ClientOptions::BeforeChatBubbleUpdate(unit->IsActivePlayer(), unit->m_namePlateFrame != nullptr);
}

bool CoA::ClientOptionsClient::BuildArrowMatrix(const CGObject_C* object, C44Matrix* matrix) {
    return CoA::ClientOptions::BuildArrowMatrix(reinterpret_cast<float*>(matrix), object->m_obj != nullptr, object->GetRenderFacing());
}

bool CoA::ClientOptionsClient::UpdateSelectionRadius(CGUnit_C* unit) {
    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
    const float playerReach = player ? player->m_unit->m_unitFieldCombatreach : 0.0f;
    return CoA::ClientOptions::SelectionRadius(player != nullptr, playerReach, unit->m_unit->m_unitFieldCombatreach, unit->m_selectionRadius);
}

void CoA::ClientOptionsClient::AfterAddWorldText(CGUnit_C* unit, uint32_t type) {
    const C3Vector position = unit->GetPosition();
    CoA::ClientOptions::AfterAddWorldText(type, position.x, position.y, position.z);
}

void CoA::ClientOptionsClient::Init() {
    CoA::ClientOptions::Deps& d = CoA::ClientOptions::Dependencies();
    d.inWorld = InWorld;
    d.registerUiScaleCVars = RegisterUiScaleCVars;
    d.reloadSelectionTexture = ReloadSelectionTexture;
    d.recomputeSelectionRadii = RecomputeSelectionRadii;
    d.setLootArtScale = SetLootArtScale;
    d.setWorldTextSize = SetWorldTextSize;
    d.setWorldTextOther = SetWorldTextOther;
    d.playSound = PlaySound;
    d.traderName = TraderName;
}
