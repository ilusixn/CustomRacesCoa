// AscTerrainScale.cpp (see TerrainScale.h): the per-layer terrain texture scale.
#include "CoA/Source/Modules/TerrainScale.h"
#include "CoA/Source/Modules/Modules.h"
#include <cmath>

void CoA::TerrainScale::Apply(uint32_t layerCount, const uint32_t* layerFlags, const bool* animated, uint32_t chunkX,
    uint32_t chunkY, float (*offset)[4], float (*scale)[4]) {
    for (uint32_t i = 0; i < layerCount; ++i) {
        const uint32_t flags = layerFlags[i];
        const uint32_t n = (flags >> 11) & 7;

        if (!n) {
            continue;
        }

        const int32_t e = n < 6 ? static_cast<int32_t>(n) : static_cast<int32_t>(n | 0xFFFFFFF8u);
        const float factor = 1.0f / static_cast<float>(pow(2.0, static_cast<double>(static_cast<float>(e))));
        scale[i][0] = factor * scale[i][0];
        scale[i][1] = factor * scale[i][1];

        if (animated[i]) {
            offset[i][0] = factor * offset[i][0];
            offset[i][1] = factor * offset[i][1];

            if (!(flags & 0x38)) {
                offset[i][0] = 0.0f;
                offset[i][1] = 0.0f;
            }
        }

        float du = 0.0f;
        float dv = 0.0f;

        if (e == 4 || e == 5) {
            const uint32_t period = e != 4 ? 4 : 2;
            float tile = 1.0f;

            for (int32_t k = e; k > 3; --k) {
                tile *= -0.5f;
            }

            du = static_cast<float>(static_cast<double>(chunkY % period)) * tile;
            dv = static_cast<float>(static_cast<double>(chunkX % period)) * tile;
        }

        offset[i][0] = du + offset[i][0];
        offset[i][1] = dv + offset[i][1];
    }
}

void CoA::TerrainScale::Init() {
    // The hook is the decomp site after 0x7D0050 (TerrainScaleClient.cpp); nothing to queue.
}
