#include "Engine/Source/Model2/EsteriaHooks.h"
#include "Engine/Source/Model2/M2Data.h"
#include "Engine/Source/Model2/M2Model.h"
#include <cstdio>
#include <cstring>

EsteriaModelHooks g_esteriaModelHooks;

uint32_t M2EsteriaWideSource(const M2SkinSection* section, const M2SkinProfile* skin) {
    return g_esteriaModelHooks.wideSource && !EsteriaOff("wide") ? g_esteriaModelHooks.wideSource(section, skin) : section->m_indexStart;
}

uint32_t M2EsteriaDrawStart(const M2SkinSection* section, CM2Model* instance) {
    uint32_t wide = (static_cast<uint32_t>(section->m_skinSectionId) >> 16 << 16) | section->m_indexStart;
    uint32_t total = 0;

    if (instance && instance->m_optGeo) {
        // OptimizeVisibleGeometry's compacted buffer: the sum of its sections' counts.
        for (uint32_t i = 0; i < instance->m_optGeo->m_skinSectionCount; i++)
            total += instance->m_optGeo->m_skinSections[i].m_indexCount;
    } else if (instance && instance->m_shared && instance->m_shared->m_skinData) {
        // CM2Shared::SetIndices' compacted buffer: every instance copies its source index list.
        total = instance->m_shared->m_instances * instance->m_shared->m_skinData->m_indices.m_count;
    }

    // A processed section's high word is its compacted start. The source total is far smaller than that buffer, so
    // validate against the buffer actually drawn (a source section's garbage high word still falls outside it).
    if (total && wide <= total && section->m_indexCount <= total - wide)
        return wide;

    return section->m_indexStart;
}

void M2EsteriaSkin(CM2Shared* shared) {
    if (g_esteriaModelHooks.skin) {
        g_esteriaModelHooks.skin(shared);
    }
}

bool EsteriaOff(const char* name) {
    static char listed[8][16];
    static int count = -1;

    if (count < 0) {
        count = 0;
        FILE* file = fopen("CoA64_switches.txt", "r");

        if (file) {
            char line[64];

            while (count < 8 && fgets(line, sizeof(line), file)) {
                size_t length = strcspn(line, "\r\n");
                line[length] = 0;

                if (length && length < sizeof(listed[0])) {
                    strcpy(listed[count++], line);
                }
            }

            fclose(file);
        }
    }

    for (int i = 0; i < count; i++) {
        if (!strcmp(listed[i], name)) {
            return true;
        }
    }

    return false;
}

bool (*g_m2BodyScaleHook)(const CM2Model* model, float scale[3]);

C44Matrix M2BodyWorld(const CM2Model* model) {
    C44Matrix world = model->m_worldTransform;
    float scale[3];

    if (g_m2BodyScaleHook && g_m2BodyScaleHook(model, scale)) {
        world.a0 *= scale[0]; world.a1 *= scale[0]; world.a2 *= scale[0];
        world.b0 *= scale[1]; world.b1 *= scale[1]; world.b2 *= scale[1];
        world.c0 *= scale[2]; world.c1 *= scale[2]; world.c2 *= scale[2];
    }

    return world;
}
