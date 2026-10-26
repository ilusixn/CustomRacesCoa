#ifndef COA_SOURCE_MODULES_GEOSETS_H
#define COA_SOURCE_MODULES_GEOSETS_H

#include <cstddef>
#include <cstdint>

// AscGeosets.cpp (installer FUN_101a1570): CreatureDisplayInfoGeosetData.dbc picks, per display id, one geoset in any
// of the 20 geoset groups of a unit's model.
//
// Map (FUN_101ceee0 into 0x10BDF4D4, cleared first): the table's records in file order, display (+0xC) -> group (+4) ->
// geoset (+8, 1..100); the first record for a display / group wins. Built on first use (FUN_101ced80) and again after
// every SMSG 0x6FC (PatchData's geosetsRebuild). FUN_101a1510(model): for each group 0..19 with an entry for the current
// display (0x10BDED60), the group's geosets i*100..i*100+99 are hidden and i*100+geoset shown (0x82C7C0).
//
// The current display is set around the client paths that build a unit's model (each zeroes it after, not restoring):
//   0x73D5D0 CGUnit_C::CreateUnitMount (descriptor +0x114), 0x73E410 UpdateDisplayInfo / 0x73FCC0 PostInit (+0x10C;
//   0x73E410's argument forced to 1 when called from 0x740309, CGUnit_C::PostReenable), 0x4E3CD0
//   CCharacterSelection::ShowCharacter (after: the selected entry's pet, display +0x164, model +0x18C), 0x597700
//   CGCharacterModelBase::SetUpModel (after: its argument +0x24, on the last model 0x81F8F0 made).
// 0x4E7790 CCharacterComponent::ApplyMonsterGeosets applies it, after, to the model it gets. 0x81F8F0
// CM2Scene::CreateModel: its result (also nullptr) is the last model (0x10BDED64, detour 0x101A14F0). 0x82C7C0
// CM2Model::SetGeometryVisible is REPLACED for loaded models (+0x10 bit 0, 0x101A1440): each geoset in range whose flag
// (+0x9C) differs from the value is set, then 0x825D70 if any changed.
namespace CoA::Geosets {
    struct Deps {
        // 0x82C7C0 (through the replacement).
        void (*setGeometryVisible)(void* model, uint32_t first, uint32_t last, uint32_t visible) = nullptr;
    };

    Deps& Dependencies();

    void Build();                                    // FUN_101ceee0
    void Rebuild(const uint8_t* row);                // AscGeosets_Rebuild (SMSG 0x6FC, every row)
    int32_t Lookup(uint32_t display, uint32_t group);   // FUN_101ced80: the geoset, or -1
    void Apply(void* model);                         // FUN_101a1510

    // 0x10BDED60 / 0x10BDED64.
    uint32_t Display();
    void SetDisplay(uint32_t display);
    void* LastModel();

    // The display around a client path: set on construction, zeroed (not restored) on destruction.
    struct DisplayScope {
        explicit DisplayScope(uint32_t display);
        ~DisplayScope();
        DisplayScope(const DisplayScope&) = delete;
        DisplayScope& operator=(const DisplayScope&) = delete;
    };

    void ModelCreated(void* model);                  // 0x101A14F0, after 0x81F8F0
    void AfterApplyMonsterGeosets(void* model);      // 0x101A12D0, after 0x4E7790
    // 0x101A12F0, after 0x4E3CD0: the selected entry (nullptr when its address is 0) and its pet display / model.
    void AfterShowCharacter(bool haveEntry, uint32_t petDisplay, void* petModel);
    void AfterSetUpModel(uint32_t display);          // 0x101A1340, after 0x597700 (display = argument +0x24)

    // 0x101A1440: the geosets of a loaded model. ids points at the first skin section's id (u16), stride bytes apart;
    // flags are the model's per-section visibility (+0x9C). True when one changed (the caller then runs 0x825D70).
    bool SetRange(uint32_t count, const uint8_t* ids, size_t stride, uint32_t* flags, uint32_t first, uint32_t last,
        uint32_t visible);

    void Init();
    // Tests only.
    void Reset();
}

#endif
