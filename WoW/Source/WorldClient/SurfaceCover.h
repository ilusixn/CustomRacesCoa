#ifndef WOW_SOURCE_WORLDCLIENT_SURFACECOVER_H
#define WOW_SOURCE_WORLDCLIENT_SURFACECOVER_H

class CMapRenderChunk;

// Not in the original binary. Painted snow (and sand, ash) lying on the terrain as a layer of its own, from
// seyris-living-azeroth's tables (GPL-3.0): GroundMaterialSelector.cdbc says which (full: wowdecomp 666d04be)
namespace SurfaceCover {

void RegisterCVars();

// After CMapRenderChunk::RenderSinglePassVertexPixelShader drew a chunk: its cover on top, with the chunk's terrain
// constants and samplers still in place.
void DrawAfter(CMapRenderChunk* chunk);

// Once per frame (CGWorldFrame::OnWorldUpdate): the near patch follows the player, trenches form and fill in.
void Update(float elapsedSec);

// After the terrain (CWorldScene::RenderChunksSinglePass): the near patch, with the last chunk's terrain constants.
void DrawNear();

// The 3rd-person camera's boom (CGCamera, after the client's own collision test): the eye sits at pivot - forward *
// distance. Returns the distance shortened so the eye stays above the near patch's cover, (full: wowdecomp 666d04be)
float LimitCameraDistance(const float pivot[3], const float forward[3], float distance);

// The render chunk's terrain vertices were made again, or it is going away: its cover is made again at its next draw.
void Forget(CMapRenderChunk* chunk);

}  // namespace SurfaceCover

#endif
