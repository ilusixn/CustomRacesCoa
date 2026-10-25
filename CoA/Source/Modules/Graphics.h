#ifndef COA_SOURCE_MODULES_GRAPHICS_H
#define COA_SOURCE_MODULES_GRAPHICS_H

struct CVar;

// The render module's glue CVar callbacks (AscGraphics.cpp, installer FUN_10265d20). The original rewrites client code;
// here the decomp sites read these switches. The fog and cloud callbacks come with the render module.
namespace CoA::Graphics {
    // FUN_1025e290: "1" keeps entity shadows (stock); anything else skips them (CWorldScene::RenderEntitys 0x793A45,
    // CShadowQuery::GatherShadowCasters 0x7BBA22).
    bool OnEntityShadows(CVar* var, const char* oldValue, const char* value, void* arg);
    bool EntityShadows();
    // FUN_10261d60: "1" keeps the LOD doodad cull (stock); anything else skips it
    // (CWorldScene::AddDoodadDefModelToModelScene 0x791D6A).
    bool OnLodObjectCull(CVar* var, const char* oldValue, const char* value, void* arg);
    bool LodObjectCull();
}

#endif
