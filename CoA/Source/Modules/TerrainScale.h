#ifndef COA_SOURCE_MODULES_TERRAINSCALE_H
#define COA_SOURCE_MODULES_TERRAINSCALE_H

#include <cstdint>

// AscTerrainScale.cpp (installer FUN_1033be70; FUN_1033bef0 after 0x7D0050, CMapRenderChunk::SetVertexShader): each
// texture layer of the chunk (its MCLY entries, flags at +4) whose flags carry a scale in bits 11..13 -- a signed 3-bit
// exponent e: 1..5, 6 = -2, 7 = -1 -- has its shader transforms scaled by 1 / 2^e: the texture scale (the vertex
// constants +0x120 + i * 0x10, x / y) always, the texture offset (+0xD0 + i * 0x10) when the render layer (+0x34 +
// i * 0x14) has 0x40, zeroed when the MCLY flags lack 0x38. For e = 4 / 5 the map chunk's index (+0x28 for u, +0x24
// for v) modulo 2 / 4, times -0.5 / 0.25, is added to the offset. Then the constants are set again
// (0x408210(0, 0, table, 0x25)).
namespace CoA::TerrainScale {
    // offset = &m_texOffset[0], scale = &m_texScale[1]; chunkX / chunkY = the map chunk's +0x24 / +0x28.
    void Apply(uint32_t layerCount, const uint32_t* layerFlags, const bool* animated, uint32_t chunkX, uint32_t chunkY,
        float (*offset)[4], float (*scale)[4]);

    void Init();
}

#endif
