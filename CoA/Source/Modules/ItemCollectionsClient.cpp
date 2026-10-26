// AscItemCollections.cpp's client questions and hook halves: the character component's model, race and sex, a model's
// attached children, AddLink, the ChrRaces prefix, the model scale, the model hooks on CM2Scene::CreateModel /
// CM2Model::Release (Engine hook pointers), and the player / unit hooks.
#include "CoA/Source/Modules/ItemCollectionsClient.h"
#include "CoA/Source/Modules/ItemCollections.h"
#include "CoA/Source/Modules/Modules.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/ChrRacesRec.h"
#include "WoW/Source/DB/Rec/ItemDisplayInfoRec.h"
#include "WoW/Source/Object/ItemStats.h"
#include "WoW/Source/Object/ObjectClient/Item_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Types.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include <vector>

namespace {
    CCharacterComponent* Component(void* component) {
        return static_cast<CCharacterComponent*>(component);
    }

    void* ComponentModel(void* component) {   // +0x38
        return Component(component)->m_data.m_model;
    }

    uint32_t ComponentRace(void* component) {   // +0x18
        return static_cast<uint32_t>(Component(component)->m_data.m_preferences.m_raceID);
    }

    uint32_t ComponentSex(void* component) {   // +0x1C
        return static_cast<uint32_t>(Component(component)->m_data.m_preferences.m_sexID);
    }

    void Children(void* model, std::vector<void*>& out) {   // +0x58, linked through +0x60
        for (CM2Model* m = static_cast<CM2Model*>(model)->m_attachList; m; m = m->m_attachNext) {
            out.push_back(m);
        }
    }

    void Detach(void* model) {   // 0x8274F0
        static_cast<CM2Model*>(model)->DetachFromParent();
    }

    void AddLink(void* parent, uint32_t link, const char* model, const char* texture, const void* displayRec) {   // 0x4EAA70
        CCharacterComponent::AddLink(static_cast<CM2Model*>(parent), static_cast<GEOCOMPONENTLINKS>(link), model, texture, 0,
            static_cast<const ItemDisplayInfoRec*>(displayRec));
    }

    const char* RacePrefix(uint32_t race) {   // FUN_100b13f0: ChrRaces +0x18
        ChrRacesRec* rec = g_chrRacesDB.GetRecord(static_cast<int32_t>(race));
        return rec ? rec->m_clientPrefix : nullptr;
    }

    void ScaleModel(void* model, float scale) {   // FUN_102bd4d0 + 0x4D8630
        C44Matrix m;

        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                (&m.a0)[row * 4 + col] *= scale;
            }
        }

        static_cast<CM2Model*>(model)->SetWorldTransform(m);
    }

    // CM2Scene::CreateModel's and CM2Model::Release's Engine hook pointers.
    void ModelCreated(const char* name, CM2Model* model) {
        CoA::ItemCollections::ModelCreated(model);
    }

    void ModelReleasing(CM2Model* model) {
        CoA::ItemCollections::ModelReleasing(model, model->m_refCount);
    }
}

void CoA::ItemCollectionsClient::BeforeEquipVisibleItem(CGPlayer_C* player, int32_t invSlot) {
    CCharacterComponent* component = player->m_characterComponent;

    if (player->m_typeId != ID_PLAYER || !component) {   // +0x14 == 4, +0xB4C
        return;
    }

    const int32_t index = CoA::ItemCollections::SlotIndex(static_cast<uint32_t>(invSlot));

    if (index == -1) {
        return;
    }

    ItemDisplayInfoRec rec;
    const uint32_t displayId = component->m_itemDisplayID[index];
    const bool found = g_itemDisplayInfoDB.GetRecord(static_cast<int32_t>(displayId), &rec);
    CoA::ItemCollections::Attach(component, static_cast<uint32_t>(index), found ? &rec : nullptr, found ? static_cast<uint32_t>(rec.m_id) : 0);
}

void CoA::ItemCollectionsClient::RefreshRanged(CGUnit_C* unit) {
    const WOWGUID ranged = static_cast<const CGPlayerLocalData*>(static_cast<CGPlayer_C*>(unit)->m_player)->m_playerFieldInvSlotHead[17];   // +0x598

    if (ranged.m_guid == 0) {
        return;
    }

    CGItem_C* item = static_cast<CGItem_C*>(ClntObjMgrObjectPtr(ranged, TYPE_ITEM, __FILE__, __LINE__));

    if (!item || !item->GetSheatheType()) {
        return;
    }

    const uint8_t state = (unit->m_typeId == ID_UNIT || unit->m_typeId == ID_PLAYER) ? unit->m_unit->m_unitFieldBytes2[0] : 0;   // +0x1E8
    unit->ShowRangedWeapon(state != 2, nullptr);
}

void CoA::ItemCollectionsClient::AfterShowRangedWeapon(CGUnit_C* unit, bool sheathed) {
    if (sheathed || unit->m_typeId != ID_PLAYER || unit == ClntObjMgrGetActivePlayerObj()) {
        return;
    }

    // +0x4EC: the visible item 16's entry (the DLL's offset; slot 17 is the ranged one).
    const int32_t entry = static_cast<CGPlayer_C*>(unit)->m_player->m_playerVisibleItems[16].m_itemId;
    WOWGUID none = WOWGUID();
    const ItemStats_C* stats = g_itemDBCache.GetRecord(entry, none, nullptr, nullptr, false);

    if (!stats || stats->m_class != 2 || stats->m_subClass != 2) {
        return;
    }

    ItemDisplayInfoRec display;

    if (g_itemDisplayInfoDB.GetRecord(0x5350, &display)) {
        CCharacterComponent::AddLink(unit->m_worldModel, ATTACH_SHEATH_MAINHAND, "Item\\ObjectComponents\\Quiver\\Quiver_A.mdx",
            "Item\\ObjectComponents\\Quiver\\Quiver_Alliance_A_04Brown.blp", 0, &display);
    }
}

void CoA::ItemCollectionsClient::Init() {
    CoA::ItemCollections::Deps& d = CoA::ItemCollections::Dependencies();
    d.componentModel = ComponentModel;
    d.componentRace = ComponentRace;
    d.componentSex = ComponentSex;
    d.children = Children;
    d.detach = Detach;
    d.addLink = AddLink;
    d.racePrefix = RacePrefix;
    d.scaleModel = ScaleModel;
    g_m2CreateModelHook = ModelCreated;
    g_m2ReleaseHook = ModelReleasing;
}
