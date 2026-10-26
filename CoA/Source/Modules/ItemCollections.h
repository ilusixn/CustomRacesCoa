#ifndef COA_SOURCE_MODULES_ITEMCOLLECTIONS_H
#define COA_SOURCE_MODULES_ITEMCOLLECTIONS_H

#include <cstdint>
#include <vector>

class CDataStore;

// AscItemCollections.cpp: the character-component module (installer sub_102cb370) -- item display collections, extra
// component models attached to a character's equipment slots from ItemDisplayInfoCollections.dbc (rows 0x20: {id,
// displayId, attach, attachHD, model, texture, scale, scaleHD}; the HD columns while patch-Q is loaded), and the model
// bookkeeping, slot removal, quiver and small render hooks installed with it.
//
//   Natives   SetModelApplyComponents (0x102CBFC0), ReloadItemDisplayInfoCollections (0x1020A860).
//   Packet    SMSG_PATCH_ITEM_DISPLAY_INFO_COLLECTIONS 0x56A (FUN_101e1b50).
//   Hooks     (marked `orig hook` edits in the client source; ItemCollectionsClient.cpp has the client halves)
//     0x5989E0 CGDressUpModelFrame::SetModel / 0x597FC0 ::TryOn  flags around the call (DAT_10be35e0 / e1)
//     0x4F2640 CCharacterComponent::AddItem          FUN_102cbfe0 (Update3DAttachments) after it
//     0x81F8F0 CM2Scene::CreateModel                 FUN_102cb000: id, pending scale, capture
//     0x824ED0 CM2Model::Release                     FUN_102caed0: the last reference drops its id
//     0x4EE6D0 CCharacterComponent::RemoveItemBySlot FUN_102caaa0: the slot's collection models go
//     0x6E08C0 CGPlayer_C::EquipVisibleItem          FUN_102cab70: re-attach before
//     0x6E1E10 / 0x6E09E0 UpdateInventoryComponent / RefreshVisibleItems   ShowRangedWeapon again after
//     0x72B7F0 CGUnit_C::ShowRangedWeapon            FUN_102cad20: other players with a bow get a quiver
//     0x4EF840 / 0x4F9F70 / 0x832AB0 / 0x826E60      no observable effect (see the client source markers)
//
// Checked against the genuine DLL: FUN_102cbfe0, FUN_102cb000 (the pending scale starts at 1.0, DAT_10bcbedc), the model
// path builders FUN_102cc980 / FUN_102cc540, the id counter FUN_102ca9c0, the slot folders (0x10BCBDA8) and the packet.
namespace CoA::ItemCollections {
    struct Deps {
        // CCharacterComponent +0x38 (m_data.m_model), +0x18 race, +0x1C sex.
        void* (*componentModel)(void* component) = nullptr;
        uint32_t (*componentRace)(void* component) = nullptr;
        uint32_t (*componentSex)(void* component) = nullptr;
        // The model's attached children (+0x58, linked through +0x60) and CM2Model::DetachFromParent (0x8274F0).
        void (*children)(void* model, std::vector<void*>& out) = nullptr;
        void (*detach)(void* model) = nullptr;
        // CCharacterComponent::AddLink (0x4EAA70)(parent, link, model path, texture path, 0, display rec).
        void (*addLink)(void* parent, uint32_t link, const char* model, const char* texture, const void* displayRec) = nullptr;
        // ChrRaces.dbc +0x18 (the client prefix) through FUN_100b13f0; nullptr without the row.
        const char* (*racePrefix)(uint32_t race) = nullptr;
        // FUN_102bd4d0 + 0x4D8630: the identity with its 3x3 part scaled, as the model's world transform.
        void (*scaleModel)(void* model, float scale) = nullptr;
    };

    Deps& Dependencies();

    // FUN_102cb240: the inventory slot's ITEM_SLOT index (the DLL's slot index), or -1.
    int32_t SlotIndex(uint32_t invSlot);

    // The hooks' pure halves.
    void SetInDressUpSetModel(bool inside);    // 0x5989E0
    void SetInDressUpTryOn(bool inside);       // 0x597FC0
    void BeforeAddItem();                      // 0x4F2640: the slot was (re-)added
    // FUN_102cbfe0: the slot's previous collection models go, then the display's collections are attached.
    void Attach(void* component, uint32_t itemSlot, const void* displayRec, uint32_t displayId);
    void ModelCreated(void* model);                        // 0x81F8F0, after (model may be nullptr)
    void ModelReleasing(void* model, uint32_t refCount);   // 0x824ED0, before
    void BeforeRemoveItemBySlot();                         // 0x4EE6D0
    void AfterRemoveItemBySlot(void* component, uint32_t invSlot);

    // Bookkeeping (tests).
    uint64_t ModelId(void* model);                               // FUN_102cb2e0 (0 = none)
    std::vector<uint64_t> AttachedIds(void* component, uint32_t itemSlot);
    bool ApplyComponents();                                      // DAT_10bcbda4
    float PendingScale();                                        // DAT_10bcbedc

    int32_t OnPatchCollection(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);   // 0x56A

    void Init();
    // Tests only: everything as at attach, Deps forgotten.
    void Reset();
}

#endif
