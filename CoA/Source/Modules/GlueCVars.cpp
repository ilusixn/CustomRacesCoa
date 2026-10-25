// The DLL's glue CVar vector (port of the scaffold's CVar::FillCustomGlueCVarVector): registered after
// ClientRegisterConsoleCommands (orig hook 0x401B60), in this order. All have no help text, flags 1.
//
//   The raid video settings (FUN_102dc960, AscRaidVideo): useRaidVideoSettings "0" and the eleven <name>_raid,
//     category 1, callback FUN_102dcdb0 -- registered here without it until the raid video module is ported.
//   loadUnknownAddOns (the AddOn-taint module FUN_10312150): "1", category 5, no callback.
//   The render module's five glue CVars (FUN_10265d20, AscGraphics), category 1 -- registered here without their
//     callbacks until the render module is ported.
//   C_Logger's level gates logDebug / logInfo (FUN_101141e0): "1", category 5, no callback.
//   cursorSizePreferred (FUN_10a1e950, AscCursor): "0", category 1, callback FUN_10a1e320 -- registered here
//     without it until the cursor module is ported.
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
    COA_CVAR("loadUnknownAddOns", nullptr, 1, "1", nullptr, 5, false, nullptr, false);
    COA_CVAR("entityShadows", nullptr, 1, "1", nullptr, 1, false, nullptr, false);
    COA_CVAR("fogOverride", nullptr, 1, "0", nullptr, 1, true, nullptr, false);
    COA_CVAR("fogDistance", nullptr, 1, "498", nullptr, 1, false, nullptr, false);
    COA_CVAR("animateClouds", nullptr, 1, "1", nullptr, 1, false, nullptr, false);
    COA_CVAR("lodObjectCull", nullptr, 1, "1", nullptr, 1, false, nullptr, false);
    COA_CVAR("logDebug", nullptr, 1, "1", nullptr, 5, false, nullptr, false);
    COA_CVAR("logInfo", nullptr, 1, "1", nullptr, 5, false, nullptr, false);
    COA_CVAR("cursorSizePreferred", nullptr, 1, "0", nullptr, 1, false, nullptr, false);
}
