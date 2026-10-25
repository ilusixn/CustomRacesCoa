// AscCharacterCreate.cpp, preview half: the merged preview put on the glue character component
// (CCharacterCreation::m_character; the original's *0xB6B1A0: +0x18 race, +0x1C sex, +0x28 skin, +0x38 its
// CM2Model), the pet and shapeshift models next to it, and the module init's hooks (FUN_1018b290).
#include "CoA/Source/Modules/BridgeStore.h"
#include "CoA/Source/Modules/CharacterCreate.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/ModelKit.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "Engine/Source/Model2/M2Shared.h"
#include "Engine/Source/Services/Camera.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/ChrClassesRec.h"
#include "WoW/Source/DB/Rec/CreatureDisplayInfoRec.h"
#include "WoW/Source/DB/Rec/CreatureModelDataRec.h"
#include "WoW/Source/DB/Rec/ItemDisplayInfoRec.h"
#include "WoW/Source/DB/Rec/ItemRec.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/DB/Rec/SpellVisualEffectNameRec.h"
#include "WoW/Source/DB/Rec/SpellVisualKitRec.h"
#include "WoW/Source/DB/Rec/SpellVisualRec.h"
#include "WoW/Source/Glue/CGlueLoading.h"
#include "WoW/Source/Glue/CharacterCreation.h"
#include "WoW/Source/Glue/CharacterSelection.h"
#include <cstring>
#include <string>

namespace {
    using CoA::CharacterCreate::Merged;
    using CoA::CharacterCreate::Transform;

    const char* const kPetDetails = "DBFilesClient\\CharacterCreationPetDetails.dbc";
    const char* const kShapeshiftDetails = "DBFilesClient\\CharacterCreationShapeshiftDetails.dbc";

    CM2Model* s_petModel = nullptr;     // manager +0x390
    CM2Model* s_shapeModel = nullptr;   // manager +0x394

    CCharacterComponent* Component() {
        return CCharacterCreation::m_character;
    }

    CM2Model* ModelOf(CCharacterComponent* c) {
        return c ? c->m_data.m_model : nullptr;
    }

    uint32_t U32(const uint8_t* r, uint32_t off) {
        uint32_t v;
        memcpy(&v, r + off, 4);
        return v;
    }

    CoA::ModelKit::Transform Kit(const Transform& t) {
        CoA::ModelKit::Transform k;
        memcpy(&k, &t, sizeof(k));
        return k;
    }

    void SetTransform(CM2Model* model, const Transform& t) {
        CoA::ModelKit::SetTransform(model, Kit(t));
    }

    // FUN_1018d3a0: the character's and the shapeshift model's transforms; the facing turns the shapeshift model
    // when there is one, else the character.
    void Transforms(const Merged& m, Transform& character, Transform& shape) {
        character = m.hasCharacter ? m.character : CoA::CharacterCreate::kIdentity;
        shape = m.hasShape ? m.shape : CoA::CharacterCreate::kIdentity;

        if (m.hasFacing) {
            if (m.shapeshift != 0) {
                shape.rot[2] = m.facing;
            } else {
                character.rot[2] = m.facing;
            }
        }
    }

    // 0x8274F0
    void Release(CM2Model*& model) {
        if (model) {
            model->DetachFromParent();
            model = nullptr;
        }
    }

    // FUN_103081f0: attachment points -1 .. 0x3F.
    void DetachAll(CM2Model* model) {
        for (int32_t a = -1; a <= 0x3F; ++a) {
            CoA::ModelKit::Detach(model, static_cast<uint32_t>(a));
        }
    }

    // FUN_103081c0
    void PlayStand(CM2Model* model) {
        CoA::ModelKit::PlayAnim(model, 0);
    }

    // FUN_10308220: the character model's attachments, then every one of the component's 0x1D item types.
    void ClearComponent(CCharacterComponent* c) {
        DetachAll(ModelOf(c));

        for (uint32_t i = 0; i <= 0x1C; ++i) {
            c->RemoveItemByType(static_cast<INVENTORY_TYPES>(i));
        }
    }

    // FUN_100b8c00: the creature record's display id, 0 when there is no such creature.
    uint32_t CreatureDisplayId(uint32_t entry) {
        const CoA::BridgeStore::CreatureRecord* r = CoA::BridgeStore::CreatureByEntry(entry);
        return r ? r->displayId : 0;
    }

    // FUN_1018b990: the spell's precast kit hand effects on attachment points 0x15 / 0x16 and its animation.
    bool CastVisual(CM2Model* model, uint32_t spell) {
        if (!model || !spell) {
            return false;
        }

        SpellRec rec;

        if (!g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec)) {
            return false;
        }

        const SpellVisualRec* v = g_spellVisualDB.GetRecord(rec.m_spellVisualID[0]);
        const SpellVisualKitRec* kit = v ? g_spellVisualKitDB.GetRecord(v->m_precastKit) : nullptr;

        if (!kit) {
            return false;
        }

        CoA::ModelKit::Detach(model, 0x15);
        CoA::ModelKit::Detach(model, 0x16);

        if (!model->m_scene) {
            return false;
        }

        const struct {
            int32_t effect;
            uint32_t attach;
        } hands[] = { { kit->m_leftHandEffect, 0x15 }, { kit->m_rightHandEffect, 0x16 } };

        for (const auto& h : hands) {
            if (h.effect) {
                if (const SpellVisualEffectNameRec* e = g_spellVisualEffectNameDB.GetRecord(h.effect)) {
                    if (CM2Model* fx = model->m_scene->CreateModel(e->m_fileName, 0)) {
                        fx->AttachToParent(model, h.attach, nullptr, 0);
                    }
                }
            }
        }

        CoA::ModelKit::PlayAnim(model, static_cast<uint32_t>(kit->m_animID));
        return true;
    }

    // Whether a live preview model already shows this file (FUN_10089a60: exact, case-sensitive).
    bool ShowsFile(CM2Model* model, const char* file) {
        return model->m_shared && strcmp(model->m_shared->m_fileName, file) == 0;
    }

    // The pet / shapeshift model: kept when it already shows the model data's file, otherwise released and recreated
    // in the character's scene and attached under the character's parent.
    CM2Model* Refresh(CM2Model*& slot, CM2Scene* scene, const CreatureDisplayInfoRec* display, const CreatureModelDataRec* modelData, CM2Model* character) {
        const char* file = modelData->m_modelName;
        bool fresh = !slot || !ShowsFile(slot, file);

        if (slot && fresh) {
            Release(slot);
        }

        if (fresh) {
            slot = scene->CreateModel(file, 0);
        }

        if (!slot) {
            return nullptr;
        }

        CCharacterComponent::ReplaceMonsterSkin(slot, display, modelData);

        if (fresh) {
            slot->AttachToParent(character->m_attachParent, 0, nullptr, 0);
        }

        DetachAll(slot);
        PlayStand(slot);
        return slot;
    }

    // +0x28 spell, +0x2C animation (-1 = none)
    void PlayDetailsKit(CM2Model* model, const uint8_t* r) {
        bool anim = U32(r, 0x2C) != 0xFFFFFFFF;

        if (!CoA::ModelKit::PlayStateKit(model, U32(r, 0x28), anim)) {
            CoA::ModelKit::PlayPrecastKit(model, U32(r, 0x28), anim);
        }
    }

    // ---- the backend -------------------------------------------------------------------------------------------
    bool RaceSex(uint32_t& race, uint32_t& sex) {
        CCharacterComponent* c = Component();

        if (!c) {
            return false;
        }

        race = c->m_data.m_preferences.m_raceID;
        sex = c->m_data.m_preferences.m_sexID;
        return true;
    }

    // FUN_101890d0: put the merged preview on the glue model.
    void Apply(const Merged& m) {
        CCharacterComponent* c = Component();
        CM2Model* model = ModelOf(c);

        if (!model) {
            return;
        }

        uint32_t race = c->m_data.m_preferences.m_raceID;
        Transform character;
        Transform shape;
        Transforms(m, character, shape);
        ClearComponent(c);
        PlayStand(model);

        if (m.spell) {
            bool anim = m.anim != 0;

            if (!CoA::ModelKit::PlayStateKit(model, m.spell, anim)) {
                CoA::ModelKit::PlayPrecastKit(model, m.spell, anim);
            }
        }

        if (m.anim) {
            CoA::ModelKit::PlayAnim(model, m.anim);
        }

        if (m.hasCharacter || m.hasFacing) {
            SetTransform(model, character);
        }

        // ---- pet: a display id outright, else a creature entry with its PetDetails placement ----
        if (m.petDisplay == 0 && m.petCreature == 0) {
            Release(s_petModel);
        } else {
            const CreatureDisplayInfoRec* display = nullptr;
            const CreatureModelDataRec* modelData = nullptr;
            const uint8_t* details = nullptr;

            if (m.petDisplay) {
                display = g_creatureDisplayInfoDB.GetRecord(static_cast<int32_t>(m.petDisplay));

                if (display) {
                    modelData = g_creatureModelDataDB.GetRecord(display->m_modelID);
                }
            } else if (uint32_t displayId = CreatureDisplayId(m.petCreature)) {
                display = g_creatureDisplayInfoDB.GetRecord(static_cast<int32_t>(displayId));

                if (display) {
                    modelData = g_creatureModelDataDB.GetRecord(display->m_modelID);
                    details = CoA::CharacterCreate::FindDetailsRow(kPetDetails, { m.petCreature, race });
                }
            }

            if (!display || !modelData || !model->m_scene) {
                Release(s_petModel);
            } else if (CM2Model* pet = Refresh(s_petModel, model->m_scene, display, modelData, model)) {
                if (details) {
                    PlayDetailsKit(pet, details);
                    SetTransform(pet, CoA::CharacterCreate::TransformAt(details, 0xC));

                    if (U32(details, 0x2C) != 0xFFFFFFFF) {
                        CoA::ModelKit::PlayAnim(pet, U32(details, 0x2C));
                    }
                } else {
                    SetTransform(pet, CoA::CharacterCreate::kIdentity);
                }

                if (m.petCast) {
                    CastVisual(pet, m.petCast);
                }
            }
        }

        // ---- items: weapons by slot, drawn into the hands when weaponsDrawn; the rest by inventory type ----
        bool mainHand = false;
        bool offHand = false;
        uint32_t slot = 0;

        for (uint32_t id : m.items) {
            const ItemRec* item = g_itemDB.GetRecord(static_cast<int32_t>(id));

            if (!item) {
                continue;
            }

            uint32_t inv = static_cast<uint32_t>(item->m_inventoryType);

            if (item->m_classID != 2) {
                c->AddItemByType(static_cast<INVENTORY_TYPES>(inv), item->m_displayInfoID);
                continue;
            }

            bool weapon = true;

            switch (inv) {
            case 0x0D:   // one-hand
                slot = (!mainHand || offHand) ? 0xF : 0x10;
                break;
            case 0x0F:   // ranged
            case 0x1A:
                slot = 0x11;
                break;
            case 0x11:   // two-hand, main hand
            case 0x15:
                slot = 0xF;
                break;
            case 0x16:   // off hand
                slot = 0x10;
                break;
            default:
                weapon = false;
                break;
            }

            bool bow = (inv == 0x0F || inv == 0x1A) && item->m_subclassID == 2;

            if (weapon) {
                if (slot == 0xF) {
                    mainHand = true;
                } else if (slot == 0x10) {
                    offHand = true;
                }
            }

            if (!m.weaponsDrawn) {
                if (weapon) {
                    c->RemoveItemBySlot(static_cast<INVENTORY_SLOTS>(slot));
                    c->SetHandItemDisplay(item->m_displayInfoID, 0, static_cast<INVENTORY_SLOTS>(slot), false);
                }
            } else if (weapon) {
                ItemDisplayInfoRec display;

                if (g_itemDisplayInfoDB.GetRecord(item->m_displayInfoID, &display)) {
                    GEOCOMPONENTLINKS attach = CCharacterComponent::GetSheatheLink(static_cast<SHEATHE_TYPE>(item->m_sheatheType), slot != 0x10);

                    if (static_cast<int32_t>(attach) != -1) {
                        // Off hand: the second model if any.
                        bool second = slot == 0x10 && display.m_modelName[1] && *display.m_modelName[1];
                        const char* modelName = display.m_modelName[second ? 1 : 0];

                        if (!modelName || !*modelName) {
                            modelName = display.m_modelName[0];
                        }

                        const char* texture = display.m_modelTexture[second ? 1 : 0];

                        if (!texture || !*texture) {
                            texture = display.m_modelTexture[0];
                        }

                        if (modelName && *modelName) {
                            CCharacterComponent::RemoveLinkpt(model, attach);
                            std::string texPath = std::string("Item\\ObjectComponents\\Weapon\\") + (texture ? texture : "");
                            std::string modelPath = std::string("Item\\ObjectComponents\\Weapon\\") + modelName;
                            CCharacterComponent::AddLink(model, attach, modelPath.c_str(), texPath.c_str(), 0, &display);
                        }
                    }
                }
            }

            // Bows get the string callback (0x1018A320 -> CCharacterSelection::ModelLoadedCallback).
            if (bow && model->m_attachList) {
                model->m_attachList->SetLoadedCallback(&CCharacterSelection::ModelLoadedCallback, nullptr);
            }
        }

        // ---- shapeshift model: its creature's display, placed by the merged shapeshift transform ----
        if (m.shapeshift == 0) {
            Release(s_shapeModel);
        } else if (uint32_t displayId = CreatureDisplayId(m.shapeshift)) {
            const CreatureDisplayInfoRec* display = g_creatureDisplayInfoDB.GetRecord(static_cast<int32_t>(displayId));
            const CreatureModelDataRec* modelData = display ? g_creatureModelDataDB.GetRecord(display->m_modelID) : nullptr;

            if (display && modelData && model->m_scene) {
                if (CM2Model* s = Refresh(s_shapeModel, model->m_scene, display, modelData, model)) {
                    if (const uint8_t* details = CoA::CharacterCreate::FindDetailsRow(kShapeshiftDetails, { m.shapeshift, race })) {
                        PlayDetailsKit(s, details);
                        SetTransform(s, shape);

                        if (U32(details, 0x2C) != 0xFFFFFFFF) {
                            CoA::ModelKit::PlayAnim(s, U32(details, 0x2C));
                        }
                    } else if (m.hasShape || m.hasFacing) {
                        SetTransform(s, shape);
                    }
                }
            }
        }

        // unk248 (to the model's attachment-list entries 1 and 2 through 0x7E4BF0 / 0x7E5080) is set by nothing in
        // the plain code; it is not applied here.
        if (m.cast) {
            CastVisual(model, m.cast);
        }

        if (m.hasUndress && m.undress) {
            for (uint32_t s = 0; s < 0x13; ++s) {
                c->RemoveItemBySlot(static_cast<INVENTORY_SLOTS>(s));
            }
        } else if (!m.spell && !m.anim && !m.petCreature && !m.shapeshift && !m.petDisplay && m.items.empty() && !m.unk248 && !m.cast && !m.petCast) {
            CCharacterCreation::Dress();   // nothing previewed: the glue's own outfit
        }
    }

    // FUN_1018f5a0's model part.
    void ApplyTransforms(const Merged& m) {
        CM2Model* model = ModelOf(Component());

        if (!model) {
            return;
        }

        Transform character;
        Transform shape;
        Transforms(m, character, shape);
        SetTransform(model, character);

        if (s_shapeModel && m.shapeshift) {
            SetTransform(s_shapeModel, shape);
        }
    }

    // FUN_1018fc80: one step of customization `type` (1 skin, 2 face, 3 hair style, 4 hair colour, 5 facial hair).
    bool Cycle(uint32_t type, int32_t dir) {
        CCharacterComponent* c = Component();

        if (!c) {
            return false;
        }

        bool next = dir > 0;
        int32_t skin = static_cast<int32_t>(c->m_data.m_preferences.m_skinID);

        switch (type) {
        case 1:
            return next ? c->SetNextSkin(COMPONENT_CONTEXT_0) : c->SetPrevSkin(COMPONENT_CONTEXT_0);
        case 2:
            return next ? c->SetNextFace(COMPONENT_CONTEXT_0, skin) : c->SetPrevFace(COMPONENT_CONTEXT_0, skin);
        case 3:
            return next ? c->SetNextHairStyle(COMPONENT_CONTEXT_0) : c->SetPrevHairStyle(COMPONENT_CONTEXT_0);
        case 4:
            return next ? c->SetNextHairColor(COMPONENT_CONTEXT_0) : c->SetPrevHairColor(COMPONENT_CONTEXT_0);
        case 5:
            return next ? c->SetNextFacialFeature(COMPONENT_CONTEXT_0) : c->SetPrevFacialFeature(COMPONENT_CONTEXT_0);
        default:
            return false;
        }
    }

    // 0x4E6AE0(component, 1)
    void ReloadComponent() {
        if (CCharacterComponent* c = Component()) {
            CGlueLoading::StartLoad(c, 1);
        }
    }

    bool AttachmentPosition(uint32_t attach, float out[3]) {
        CM2Model* model = ModelOf(Component());

        if (!model) {
            return false;
        }

        C3Vector v = model->GetAttachmentPosition(attach);
        out[0] = v.x;
        out[1] = v.y;
        out[2] = v.z;
        return true;
    }

    void ReleaseModels() {
        Release(s_petModel);
        Release(s_shapeModel);
    }

    void ForgetModels() {
        s_petModel = nullptr;
        s_shapeModel = nullptr;
    }

    bool Item(uint32_t id, uint32_t& itemClass, uint32_t& invType) {
        const ItemRec* item = g_itemDB.GetRecord(static_cast<int32_t>(id));

        if (!item) {
            return false;
        }

        itemClass = static_cast<uint32_t>(item->m_classID);
        invType = static_cast<uint32_t>(item->m_inventoryType);
        return true;
    }

    void ClassFiles(void (*add)(const char* file, uint32_t id)) {
        for (int32_t i = 0; i < g_chrClassesDB.m_numRecords; ++i) {
            const ChrClassesRec& r = g_chrClassesDB.m_records[i];
            add(r.m_filename, static_cast<uint32_t>(r.m_id));
        }
    }

    // FUN_1018af70 over DataMgrGetCoord (0x4C1290).
    void AfterGetCoord(uint32_t index, C3Vector* value) {
        if (!value) {
            return;
        }

        float out[3] = { value->x, value->y, value->z };
        CoA::CharacterCreate::AfterCameraCoord(static_cast<int32_t>(index), out);
        value->x = out[0];
        value->y = out[1];
        value->z = out[2];
    }
}

void CoA::CharacterCreateClient::Init() {
    CoA::CharacterCreate::Preview& p = CoA::CharacterCreate::Backend();
    p.raceSex = RaceSex;
    p.apply = Apply;
    p.applyTransforms = ApplyTransforms;
    p.cycle = Cycle;
    p.reloadComponent = ReloadComponent;
    p.attachmentPosition = AttachmentPosition;
    p.releaseModels = ReleaseModels;
    p.forgetModels = ForgetModels;
    p.item = Item;
    p.classFiles = ClassFiles;
    // AscRuntime::OnAfter495810: after every PaintScreen.
    CoA::Lifecycle::OnAfterPaint(CoA::CharacterCreate::WatchComponent);
    g_dataMgrGetCoordHook = AfterGetCoord;
}
