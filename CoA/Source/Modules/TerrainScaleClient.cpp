// AscTerrainScale.cpp's client half: the chunk's layers and the vertex shader constants (0xD250A0).
#include "CoA/Source/Modules/TerrainScale.h"
#include "CoA/Source/Modules/TerrainScaleClient.h"
#include "CoA/Source/Modules/Modules.h"
#include "Engine/Source/Gx/Gx.h"
#include "WoW/Source/WorldClient/MapChunk.h"
#include "WoW/Source/WorldClient/MapRenderChunk.h"
#include "WoW/Source/WorldClient/Types.h"

void CoA::TerrainScaleClient::AfterSetVertexShader(CMapRenderChunk* renderChunk) {
    CMapRenderChunk::SVertexShaderConstants& consts = CMapRenderChunk::s_vertexShaderConsts;
    const CMapChunk* chunk = renderChunk->m_mapChunks[0];

    // The original reads the MCLY chunk's size in front of the layers without a test.
    if (chunk && chunk->m_layers) {
        const uint8_t* layers = reinterpret_cast<const uint8_t*>(chunk->m_layers);
        uint32_t count = *reinterpret_cast<const uint32_t*>(layers - 4) >> 4;
        uint32_t flags[4];
        bool animated[4];

        // The original walks every MCLY entry; the render chunk has four layer records (a fifth would read past them).
        if (count > 4) {
            count = 4;
        }

        for (uint32_t i = 0; i < count; ++i) {
            flags[i] = chunk->m_layers[i].m_flags;
            animated[i] = (renderChunk->m_layers[i].m_flags & 0x40) != 0;
        }

        CoA::TerrainScale::Apply(count, flags, animated, static_cast<uint32_t>(chunk->m_aIndex.x), static_cast<uint32_t>(chunk->m_aIndex.y),
            reinterpret_cast<float (*)[4]>(&consts.m_texOffset[0]), reinterpret_cast<float (*)[4]>(&consts.m_texScale[1]));
    }

    // 0x408210(0, 0, table, 0x25)
    GxShaderConstantsSet(GxSh_Vertex, 0, reinterpret_cast<const float*>(&consts), sizeof(consts) / (4 * sizeof(float)));
}

void CoA::TerrainScaleClient::Init() {
}
