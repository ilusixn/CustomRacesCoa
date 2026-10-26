#ifndef COA_SOURCE_MODULES_TERRAINSCALECLIENT_H
#define COA_SOURCE_MODULES_TERRAINSCALECLIENT_H

class CMapRenderChunk;

// AscTerrainScale.cpp's client half: FUN_1033bef0 after 0x7D0050 (CMapRenderChunk::SetVertexShader).
namespace CoA::TerrainScaleClient {
    void AfterSetVertexShader(CMapRenderChunk* renderChunk);
}

#endif
