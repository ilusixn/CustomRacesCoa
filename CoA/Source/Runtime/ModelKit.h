#ifndef COA_SOURCE_RUNTIME_MODELKIT_H
#define COA_SOURCE_RUNTIME_MODELKIT_H

#include <cstdint>

class CM2Model;

// The DLL's CM2Model helpers (AscModelKit, in AscModelMethods.cpp), shared by the character-creation preview and the
// Model widget methods. Client only (ModelKitClient.cpp).
namespace CoA::ModelKit {
    // pos xyz, rotation xyz (radians), scale.
    struct Transform {
        float pos[3];
        float rot[3];
        float scale;
    };

    // Matrix from identity: translate (FUN_102bd530), rotate x / y / z (FUN_102bd230 / 310 / 3f0), scale
    // (FUN_102bd4d0); set on the model (SetWorldTransform, 0x4D8630).
    void SetTransform(CM2Model* model, const Transform& t);
    // FUN_103097f0: the spell's SpellVisual state kit through the kit player, looping.
    bool PlayStateKit(CM2Model* model, uint32_t spell, bool noAnim);
    // FUN_10309760: the spell's SpellVisual precast kit through the kit player.
    bool PlayPrecastKit(CM2Model* model, uint32_t spell, bool noAnim);
    // FUN_10309880: each SpellVisualKitModelAttach row of the kit, then the kit's twelve effect columns at fixed
    // attachment points; then the kit's animation. True when anything was attached or played.
    bool PlayKit(CM2Model* model, uint32_t kitId, bool noAnim, bool loop);
    // SetBoneSequence(-1, anim, NaN bits, 0, speed, 1, 1)
    void PlayAnim(CM2Model* model, uint32_t anim, float speed = 1.0f);
    void Detach(CM2Model* model, uint32_t attach);
}

#endif
