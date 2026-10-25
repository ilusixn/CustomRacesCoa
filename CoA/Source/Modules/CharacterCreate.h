#ifndef COA_SOURCE_MODULES_CHARACTERCREATE_H
#define COA_SOURCE_MODULES_CHARACTERCREATE_H

#include <cstdint>
#include <initializer_list>
#include <vector>

// C_CharacterCreate (port of AscCharacterCreate.cpp). The data half -- realm creation gates, the archetype and
// class-guide lookups, the creation manager (static 0x10BDB700) with its three override layers and their merge --
// lives in CharacterCreate.cpp; what touches the glue character model is the Preview backend that
// CharacterCreateClient.cpp installs (tests run without one).
namespace CoA::CharacterCreate {
    // pos xyz, rotation xyz (radians), scale (AscModelKit::Transform).
    struct Transform {
        float pos[3];
        float rot[3];
        float scale;
    };

    extern const Transform kIdentity;   // FUN_1018b590

    // The merge of the three layers (manager +0x224), defaults as FUN_1018d080 builds them.
    struct Merged {
        uint32_t spell = 0;          // state-kit spell on the character
        uint32_t anim = 0;
        uint32_t petCreature = 0;    // creature entry of the pet model
        uint32_t shapeshift = 0;     // creature entry of the shapeshift model
        uint32_t petDisplay = 0;     // CreatureDisplayInfo of the pet model
        bool weaponsDrawn = false;
        std::vector<uint32_t> items;
        uint32_t unk248 = 0;
        bool hasCharacter = false;
        Transform character = { { 0, 0, 0 }, { 0, 0, 0 }, 1.0f };
        bool hasShape = false;
        Transform shape = { { 0, 0, 0 }, { 0, 0, 0 }, 1.0f };
        bool hasCamera = false;
        float camera[3] = { 0, 0, 0 };
        bool hasFacing = false;
        float facing = 0;
        bool hasUndress = false;
        bool undress = false;
        uint32_t cast = 0;
        uint32_t petCast = 0;
    };

    // The glue character component (CCharacterCreation::m_character) and its models. Null entries: no component.
    struct Preview {
        bool (*raceSex)(uint32_t& race, uint32_t& sex) = nullptr;           // component +0x18 / +0x1C
        void (*apply)(const Merged& m) = nullptr;                             // FUN_101890d0
        void (*applyTransforms)(const Merged& m) = nullptr;                   // FUN_1018f5a0's model part
        bool (*cycle)(uint32_t type, int32_t dir) = nullptr;                  // FUN_1018fc80
        void (*reloadComponent)() = nullptr;                                  // 0x4E6AE0(component, 1)
        bool (*attachmentPosition)(uint32_t attach, float out[3]) = nullptr; // false without a model
        void (*releaseModels)() = nullptr;                                    // the pet / shapeshift models freed
        void (*forgetModels)() = nullptr;                                     // the scene is gone: dropped unfreed
        bool (*item)(uint32_t id, uint32_t& itemClass, uint32_t& invType) = nullptr;   // Item.dbc +4 / +0x18
        void (*classFiles)(void (*add)(const char* file, uint32_t id)) = nullptr;     // ChrClasses filenames
    };

    Preview& Backend();

    void Init();
    // The manager back to its constructed state (tests).
    void Reset();

    const Merged& Current();

    // Shared with the preview half.
    const uint8_t* FindDetailsRow(const char* table, std::initializer_list<uint32_t> keys);
    Transform TransformAt(const uint8_t* row, uint32_t off);

    // The replaced glue scripts GetCharacterCreateFacing (LAB_1018e250) / SetCharacterCreateFacing (LAB_1018ec80).
    float FacingDegrees();
    void SetFacingDegrees(float degrees);

    // The client hook points (CharacterCreation.cpp, Camera.cpp, CSimpleTop's after-paint list, every glue screen).
    void AfterSetSelectedSex();     // LAB_1018f9b0
    void AfterSetSelectedRace();    // LAB_1018f990
    void AfterSetSelectedClass();   // LAB_1018ae20
    void AfterRandomize();          // LAB_1018ae50
    void AfterCameraCoord(int32_t id, float* out);   // FUN_1018af70
    void WatchComponent();          // FUN_1018b160
    void ResetPreview();            // FUN_1018cad0

    // Runs fn when the scope ends, on every return path -- what the original's detours do after the original returns.
    struct RunAfter {
        void (*fn)();
        ~RunAfter() { this->fn(); }
    };
}

// CharacterCreateClient.cpp: the preview backend, the camera hook and the after-paint watch.
namespace CoA::CharacterCreateClient {
    void Init();
}

#endif
