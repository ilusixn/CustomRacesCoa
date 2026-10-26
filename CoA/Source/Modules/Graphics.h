#ifndef COA_SOURCE_MODULES_GRAPHICS_H
#define COA_SOURCE_MODULES_GRAPHICS_H

#include <cstddef>
#include <cstdint>
#include <vector>

class CDataStore;
struct CVar;
struct lua_State;

// AscGraphics.cpp, the original's render module (installer FUN_10265d20): nineteen render CVars whose callbacks rewrite
// client code, the lighting / sky / cloud / far-clip detours they feed, and the occlusion volumes (the DLL's
// OcclusionVolume(Points).dbc turned into the client's antiportal table, with a debug overlay).
//
// The original rewrites client bytes; here each rewrite is a switch the decomp site reads (Plan 13 ruling). Every switch
// starts at the stock bytes and the callbacks run at every CVar registration with the defaults, which leave the
// client's behaviour stock -- except terrainCulling "1", which sets CWorld::enables' culling bit as the original does.
//
//   Attach     0x9E8CFC <- 1400.0f (CWorldParam::GroundEffectDistCallback's upper bound; the WoW site).
//   Detours    0x834A40 CM2Light::CM2Light -> sub_1025d020 (in world: attenuation, tracked)
//              0x834AB0 CM2Light::Unlink -> sub_1025d0c0 (untracked)
//              0x7EB210 WowClientDB_RLE<LightIntBandRec>::GetRecord -> FUN_10264820 (the 16 colours x ambientShade; the
//                       function is folded with LightFloatBand's, so the float bands are scaled bytewise too)
//              0x7EB180 DayNight::GetLightParams -> sub_102648c0 (SMSG 0x650 skybox override, glow x ambientGlow)
//              0x7F1CD0 CWorldParam::CloudLODCallback -> sub_102647c0, 0x7F1010 DNClouds::Update -> sub_102647f0
//              0x780770 World::ValidateFarClip -> FUN_1026ad40 (replaced)
//              0x6815C0 / 0x681580 GxTexReusable -> sub_10265d10 (both replaced by "return 0")
//   Lifecycle  sub_1025b020 on every second 0x6DF050; sub_1025b0c0 after 0x5204C0; sub_1025b0f0 and the antiportal build
//              FUN_10264d10 on world entry; the overlay FUN_10266560 after 0x4F6F90.
//   Packets    SMSG 0x650 (FUN_10266530); SMSG_PATCH_OCCLUSION_VOLUME 0x9D2 (FUN_101e2810) and _POINTS 0x9D3
//              (FUN_101e2990) patch the tables and rebuild.
namespace CoA::Graphics {
    // ---- glue CVars (GlueCVars.cpp registers them) -------------------------------------------------------------
    // FUN_1025e290: "1" keeps entity shadows (stock); anything else skips them (CWorldScene::RenderEntitys 0x793A45,
    // CShadowQuery::GatherShadowCasters 0x7BBA22).
    bool OnEntityShadows(CVar* var, const char* oldValue, const char* value, void* arg);
    bool EntityShadows();
    // FUN_10261d60: "1" keeps the LOD doodad cull (stock); anything else skips it
    // (CWorldScene::AddDoodadDefModelToModelScene 0x791D6A).
    bool OnLodObjectCull(CVar* var, const char* oldValue, const char* value, void* arg);
    bool LodObjectCull();
    // sub_10260f40: fogDistance and fogDensity set again to their current values (their callbacks re-run).
    bool OnFogOverride(CVar* var, const char* oldValue, const char* value, void* arg);
    // FUN_10260400: with fogOverride on, the distance clamped to 498..1600 goes to DNInfo::m_bands.m_fogEnd (0xD38C1C);
    // above 498 the client's two stores to a CurrentLight's fog end (0x7ED982 operator=, 0x7EC15A LoadLightingBands)
    // are NOPed, otherwise restored.
    bool OnFogDistance(CVar* var, const char* oldValue, const char* value, void* arg);
    bool FogEndStores();
    // sub_1025d120: the cloud mark cleared.
    bool OnAnimateClouds(CVar* var, const char* oldValue, const char* value, void* arg);
    // The slots the original keeps (0x10BE26D0 / 0x10BE26D8 / 0x10BE2700).
    CVar** FogOverrideSlot();
    CVar** FogDistanceSlot();
    CVar** AnimateCloudsSlot();

    // ---- world CVars (Init queues them, FUN_10114540, flags 1, category 1) ---------------------------------------
    // FUN_1025d520: not "1" NOPs CWorldScene::CullEntitys' three cull jumps (0x793115 frustum, 0x793126 volumes,
    // 0x79313F buffer): every entity is visible and m_culled takes the buffer query's low byte.
    bool OnEntityCulling(CVar* var, const char* oldValue, const char* value, void* arg);
    bool EntityCulling();
    // FUN_102628b0: not "1" NOPs CMapObj::RenderInterior's exterior-portal group test (0x7AD2EE).
    bool OnPortalCulling(CVar* var, const char* oldValue, const char* value, void* arg);
    bool PortalCulling();
    // FUN_10262e10: CWorld::enables' culling bit (0x20) follows "1", and not "1" NOPs CWorldScene::Render's jnp
    // (0x79AA42): the viewer's interior always culls the sort table instead of clearing it.
    bool OnTerrainCulling(CVar* var, const char* oldValue, const char* value, void* arg);
    bool TerrainCulling();
    // FUN_102613f0: not "1" makes CM2Scene::SelectLights' stamp test (0x81E548) a jmp: every grid light is added.
    bool OnLightCulling(CVar* var, const char* oldValue, const char* value, void* arg);
    // FUN_10263380: not "1" NOPs 0x792AF2 / 0x792AFF (CWorldScene::AddMapObjDefGroup: no group-flag return, every
    // group goes to the pending exterior list) and 0x7B3AAF / 0x7B3A8E (CullMapObjDefGroupFromExterior: the 0x10000
    // render-callback path for every visible group).
    bool OnWmoCulling(CVar* var, const char* oldValue, const char* value, void* arg);
    bool WmoCulling();
    // FUN_1025ebf0: with fogOverride on and a value of at least 0.001, the density goes to DNInfo::m_fog.m_density
    // (0xD38B98); exactly 1.0 restores the client's five density stores in DayNight::UpdateFog, anything else NOPs them.
    bool OnFogDensity(CVar* var, const char* oldValue, const char* value, void* arg);
    bool FogDensityStores();
    // FUN_1025d130: not "1" makes CMapObjGroup::Create's name test (0x7D8444) a jmp: no antiportal group is handled.
    bool OnAntiportal(CVar* var, const char* oldValue, const char* value, void* arg);
    bool Antiportal();
    // FUN_102617e0: "1" skips CMapRenderChunk::SetVertexShader's three point lights (0x7D00BF).
    bool OnLightLimitFix(CVar* var, const char* oldValue, const char* value, void* arg);
    bool LightLimitFix();
    // FUN_10260f90 (lightLinearAttenuation / lightQuadraticAttenuation): any value but 1.0 makes 0x81E548 a jmp, 1.0
    // a je; then every tracked light is attenuated again. Called before the CVar stores the new value.
    bool OnLightAttenuation(CVar* var, const char* oldValue, const char* value, void* arg);
    // 0x81E548's state, shared by lightCulling and the attenuation CVars (the last write wins): true is the stock je.
    bool LightStampCulling();
    // sub_10262850 -> FUN_10266c30: from 0.1 up, the client's 24 day-progress reads (DNInfo::m_dayProgression,
    // 0xD38B04) read the DLL's float instead.
    bool OnOverrideDayProgress(CVar* var, const char* oldValue, const char* value, void* arg);
    float DayProgress(float stock);
    // FUN_102622d0: the mist density the Rain / Snow / Sand constructors pass (0x78CB05 / 0x78CDA5 / 0x78CF95) reads the
    // DLL's float, whatever the value; 12.0 (the stock constant) until the callback runs.
    bool OnMistDensity(CVar* var, const char* oldValue, const char* value, void* arg);
    float MistDensity();

    // ---- detours ---------------------------------------------------------------------------------------------------
    // sub_1025d020 after CM2Light::CM2Light: in world, attenuated (linear 0.7 x lightLinearAttenuation, quadratic 0.03 x
    // lightQuadraticAttenuation, 1.0 for a missing CVar) and tracked.
    void LightCreated(void* light);
    // sub_1025d0c0 before CM2Light::Unlink.
    void LightUnlinked(void* light);
    size_t TrackedLights();
    // FUN_10264820 after the band GetRecord: the 16 colours' b / g / r bytes at record +0x48 scaled by ambientShade.
    void ScaleBandRecord(uint8_t* record);
    // sub_102648c0 after DayNight::GetLightParams (a 9-dword LightParams row): the row is copied; with an SMSG 0x650
    // override the copy is returned with [1] = 1 and [2] = the skybox, and ambientGlow scales [4] (the glow) of whichever
    // row is returned -- the client's own row, in place, when there is no override.
    uint32_t* LightParams(uint32_t* row);
    // sub_102647c0 before CloudLODCallback: the clouds are marked.
    void CloudsSet();
    // sub_102647f0 in place of DNClouds::Update: true runs it (marked, or animateClouds 0), and clears the mark.
    bool CloudsTick();
    // FUN_1026ad40 in place of World::ValidateFarClip: 183.33 .. 791.66 on maps 0 / 1 / 530 / 571, .. 700 on 909,
    // .. 1583.33 elsewhere.
    float FarClipClamp(float farClip, uint32_t mapId);

    // ---- lifecycle -------------------------------------------------------------------------------------------------
    // sub_1025b020 (every second SMSG_INITIAL_SPELLS): clouds marked; in maps 907 / 909 the shadow mode (the CVar
    // CWorldParam::cvar_extShadowQuality, 0xCD85DC) is capped at 3 through CShadowCache::SetShadowMode, restored after.
    void MapCap();
    // sub_1025b0c0 (after CGGameUI::NewZoneFeedback): World::SetFarClip(farClip).
    void ReapplyFarClip();
    // sub_1025b0f0 (world entry): the five culling CVars set back to "1" when 0.
    void CullingOn();

    // ---- occlusion volumes (FUN_10264d10) ----------------------------------------------------------------------------
    struct Occluder {
        uint32_t mapId = 0;
        uint32_t flags = 0;
        float min[3] = {};
        float max[3] = {};
        std::vector<float> points;   // x, y, z per point, in point-id order
    };

    // Every OcclusionVolume row (id order) with at least three OcclusionVolumePoints becomes a record; with any, the
    // client's antiportal loops (InitOcclusionVolumes / CreateOcclusionVolumes) use them and the table is rebuilt.
    void BuildOccluders();
    const std::vector<Occluder>& Occluders();
    // FUN_10264930: the polygon's triangles (point indices) through mapbox earcut, on the plane its Newell normal is
    // largest across.
    std::vector<uint32_t> Triangulate(const std::vector<float>& points);
    // FUN_10266560 (after CGWorldFrame::ClearTrackedObjects): with the overlay on, every volume of the current map
    // filled (0x4040FF20) and outlined (0xB040FF20).
    void DrawOcclusion();
    bool OcclusionOverlay();
    // handler_SetRenderOcclusion (FUN_1026a510): exactly one argument, else ignored; returns nothing.
    int SetRenderOcclusion(lua_State* L);

    int32_t OnSkyOverride(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);        // SMSG 0x650
    int32_t OnPatchVolume(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);        // SMSG 0x9D2
    int32_t OnPatchVolumePoint(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);   // SMSG 0x9D3
    uint32_t SkyOverride();

    struct Deps {
        bool (*inWorld)() = nullptr;                                            // CGGameUI::s_inWorld (0xBD0792)
        void (*setAttenuation)(void* light, float linear, float quadratic) = nullptr;   // CM2Light +0x58 / +0x5C
        void (*setCullingEnable)(bool on) = nullptr;                            // CWorld::enables 0x20 (0xCD774C)
        void (*setFogDensity)(float density) = nullptr;                         // DNInfo::m_fog.m_density (0xD38B98)
        void (*setFogEnd)(float end) = nullptr;                                 // DNInfo::m_bands.m_fogEnd (0xD38C1C)
        uint32_t (*mapId)() = nullptr;                                          // CGGameUI::m_iCurrentMapID (0xBD088C)
        CVar* (*shadowCVar)() = nullptr;                                        // *0xCD85DC
        void (*setShadowMode)(int32_t mode) = nullptr;                          // 0x874210
        void (*setFarClip)(float farClip) = nullptr;                            // 0x780800
        void (*useOccluders)(const std::vector<Occluder>& occluders) = nullptr; // the antiportal table, rebuilt
        uint32_t (*currentMap)() = nullptr;                                     // s_mapId (0xADFBC4)
        // The overlay's two draws (the camera-relative positions are the client half's).
        void (*drawFill)(const Occluder& occluder, const std::vector<uint32_t>& triangles, uint32_t color) = nullptr;
        void (*drawLine)(const float a[3], const float b[3], uint32_t color) = nullptr;
    };

    Deps& Dependencies();
    void Init();
    // Tests only.
    void Reset();
}

#endif
