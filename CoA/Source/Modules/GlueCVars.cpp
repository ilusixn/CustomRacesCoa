// The DLL's glue CVar vector (port of the scaffold's CVar::FillCustomGlueCVarVector): registered after
// ClientRegisterConsoleCommands (orig hook 0x401B60), in this order. All have no help text, flags 1.
//
//   The raid video settings (FUN_102dc960, AscRaidVideo): useRaidVideoSettings "0" and the eleven <name>_raid,
//     category 1, callback FUN_102dcdb0 -- registered here without it until the raid video module is ported.
//   loadUnknownAddOns (the AddOn-taint module FUN_10312150): "1", category 5, no callback, slot 0x10BE3F7C.
//   The render module's five glue CVars (FUN_10265d20, AscGraphics), category 1: fogOverride / fogDistance /
//     animateClouds with their callbacks and slots (Graphics.cpp); entityShadows / lodObjectCull get theirs in
//     GlueCVarsClient.
//   C_Logger's level gates logDebug / logInfo (FUN_101141e0): "1", category 5, no callback.
//   cursorSizePreferred (FUN_10a1e950, AscCursor): "0", category 1, callback FUN_10a1e320 -- registered here
//     without it until the cursor module is ported.
#include "CoA/Source/Modules/AddonTaint.h"
#include "CoA/Source/Modules/Graphics.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/CVars.h"

void CoA::GlueCVars::Init() {
    // name, help, flags, default, callback, category, setCommand, slot, world
    COA_CVAR("useRaidVideoSettings", nullptr, 1, "0", nullptr, 1, false, nullptr, false);
    COA_CVAR("farclip_raid", nullptr, 1, "507", nullptr, 1, false, nullptr, false);
    COA_CVAR("particleDensity_raid", nullptr, 1, "0.4", nullptr, 1, false, nullptr, false);
    COA_CVAR("extShadowQuality_raid", nullptr, 1, "0", nullptr, 1, false, nullptr, false);
    COA_CVAR("environmentDetail_raid", nullptr, 1, "0.75", nullptr, 1, false, nullptr, false);
    COA_CVAR("groundEffectDensity_raid", nullptr, 1, "24", nullptr, 1, false, nullptr, false);
    COA_CVAR("groundEffectDist_raid", nullptr, 1, "80.0", nullptr, 1, false, nullptr, false);
    COA_CVAR("textureFilteringMode_raid", nullptr, 1, "1", nullptr, 1, false, nullptr, false);
    COA_CVAR("projectedTextures_raid", nullptr, 1, "1", nullptr, 1, false, nullptr, false);
    COA_CVAR("fogDistance_raid", nullptr, 1, "548", nullptr, 1, false, nullptr, false);
    COA_CVAR("entityShadows_raid", nullptr, 1, "0", nullptr, 1, false, nullptr, false);
    COA_CVAR("animateClouds_raid", nullptr, 1, "0", nullptr, 1, false, nullptr, false);
    COA_CVAR("loadUnknownAddOns", nullptr, 1, "1", nullptr, 5, false, CoA::AddonTaint::LoadUnknownAddOnsSlot(), false);
    COA_CVAR("entityShadows", nullptr, 1, "1", nullptr, 1, false, nullptr, false);
    COA_CVAR("fogOverride", nullptr, 1, "0", CoA::Graphics::OnFogOverride, 1, true, CoA::Graphics::FogOverrideSlot(), false);
    COA_CVAR("fogDistance", nullptr, 1, "498", CoA::Graphics::OnFogDistance, 1, false, CoA::Graphics::FogDistanceSlot(), false);
    COA_CVAR("animateClouds", nullptr, 1, "1", CoA::Graphics::OnAnimateClouds, 1, false, CoA::Graphics::AnimateCloudsSlot(), false);
    COA_CVAR("lodObjectCull", nullptr, 1, "1", nullptr, 1, false, nullptr, false);
    COA_CVAR("logDebug", nullptr, 1, "1", nullptr, 5, false, nullptr, false);
    COA_CVAR("logInfo", nullptr, 1, "1", nullptr, 5, false, nullptr, false);
    COA_CVAR("cursorSizePreferred", nullptr, 1, "0", nullptr, 1, false, nullptr, false);
}
