#ifndef ENGINE_SOURCE_MODEL2_ESTERIAHOOKS_H
#define ENGINE_SOURCE_MODEL2_ESTERIAHOOKS_H

#include <cstdint>

class CM2Shared;
class M2SkinSection;
class M2SkinProfile;
class CM2Model;

// CoA: the Engine does not link CoA, so the Esteria race hooks (EsteriaAppearance.dll's patches of the M2 index / skin code)
// are installed here by CoA::Esteria::Init.
class C44Matrix;

// Per-model body scale (x = depth, y = width, z = height) set by the Esteria body sliders; returns false for an unscaled model.
extern bool (*g_m2BodyScaleHook)(const CM2Model* model, float scale[3]);
C44Matrix M2BodyWorld(const CM2Model* model);

struct EsteriaModelHooks {
    uint32_t (*wideSource)(const M2SkinSection* section, const M2SkinProfile* skin);
    void (*skin)(CM2Shared* shared);
    uint32_t (*drawStart)(const M2SkinSection* section, CM2Model* instance);
};

extern EsteriaModelHooks g_esteriaModelHooks;

// The first index of a skin section (its 16-bit start, or the wide start Esteria keeps in the section id's high word).
uint32_t M2EsteriaWideSource(const M2SkinSection* section, const M2SkinProfile* skin);
uint32_t M2EsteriaDrawStart(const M2SkinSection* section, CM2Model* instance);
void M2EsteriaSkin(CM2Shared* shared);

// CoA debugging: names listed (one per line) in CoA64_switches.txt next to the executable turn a change of the Esteria port off
// ("wide", "optgeo", "combos", "paste"), so a rendering bug can be bisected without rebuilding.
bool EsteriaOff(const char* name);

#endif
