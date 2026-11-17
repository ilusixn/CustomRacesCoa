#ifndef WOW_SOURCE_UI_VISUALS_H
#define WOW_SOURCE_UI_VISUALS_H

#include <cstdlib>
#include "CoA/Source/Runtime/Visuals.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/ConsoleBase/Types.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/Gx.h"

// Not in the original binary. The HD passes follow CoA's startup pack choice (Visuals.txt, see
// docs/2026-11-15-visuals-switch.md); the `coaVisuals` cvar is the options dropdown's handle on that file.
namespace Visuals {

enum Mode : int32_t {
    Stock = 0,
    Lumenveil = 1,
    AzerothRemastered = 2,
};

inline CVar*& Cvar() {
    static CVar* cvar = nullptr;
    return cvar;
}

inline bool CvarChanged(CVar*, const char*, const char* value, void*) {
    if (!value) {
        return false;
    }

    // Unchanged (also the registration itself): leave Visuals.txt and its comments alone.
    const unsigned choice = static_cast<unsigned>(strtoul(value, nullptr, 10));
    return choice == CoA::Visuals::Pending() || CoA::Visuals::Set(choice);
}

inline void Register() {
    if (Cvar()) {
        return;
    }

    char value[4] = { static_cast<char>('0' + CoA::Visuals::Pending()), 0 };
    // Flag 0: Visuals.txt is the saved copy, Config.wtf is read too late for the device choice.
    Cvar() = CVar::Register("coaVisuals", "Graphics pack for the next start: 0 standard, 1 Lumenveil, 2 LuxVelum, 3 Azeroth Remastered", 0, value, CvarChanged, CATEGORY_GRAPHICS, false, nullptr, false);
}

// Azeroth Remastered only draws on the Vulkan device; anywhere else the HD passes stay off.
inline Mode Current() {
    if (CoA::Visuals::Current() != CoA::Visuals::AzerothRemastered || !g_theGxDevicePtr) {
        return Stock;
    }

    return g_theGxDevicePtr->DeviceApi() == GxApi_Vulkan ? AzerothRemastered : Stock;
}

// Azeroth Remastered needs Vulkan; Lumenveil64.dll and LuxVelum hook D3D9. Command-line API flags still win.
inline EGxApi PickApi(EGxApi api) {
    const unsigned pack = CoA::Visuals::Current();

    if (pack == CoA::Visuals::AzerothRemastered && GxApiSupported(GxApi_Vulkan)) {
        return GxApi_Vulkan;
    }

    if (api == GxApi_Vulkan && (pack == CoA::Visuals::Lumenveil || pack == CoA::Visuals::LuxVelum) && GxApiSupported(GxApi_D3d9)) {
        return GxApi_D3d9;
    }

    return api;
}

// Adds the pack dropdown to the Effects page of the video options.
void InitializeOptions();

// Living Azeroth pages in the video options (LivingAzerothOptions.cpp).
void InitializeLivingAzerothOptions();

// Lumenveil and LuxVelum sliders in the video options (EffectSettingsOptions.cpp).
void InitializeEffectOptions();

}  // namespace Visuals

#endif
