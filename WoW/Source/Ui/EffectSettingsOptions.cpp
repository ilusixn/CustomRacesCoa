#include "WoW/Source/Ui/Visuals.h"
#include <cstdio>
#include <string>
#include "Engine/Source/FrameScript/FrameScript.h"

#if defined(_WIN32)
#include <windows.h>
#endif

// Lumenveil64.dll and LuxVelum64.dll re-read their ini within a second, so cvars that write the ini act as live settings.
namespace {

struct Setting {
    const char* cvar;
    const wchar_t* file;
    const wchar_t* section;
    const wchar_t* key;
    const char* standard;
};

const Setting kSettings[] = {
    { "lvDensity", L"CoAVolFog.ini", L"CoAVolFog", L"Density", "1.0" },
    { "lvHaze", L"CoAVolFog.ini", L"CoAVolFog", L"Haze", "1.0" },
    { "lvGroundFog", L"CoAVolFog.ini", L"CoAVolFog", L"GroundFog", "0.6" },
    { "lvFarFog", L"CoAVolFog.ini", L"CoAVolFog", L"FarFog", "1.0" },
    { "lvMorningMist", L"CoAVolFog.ini", L"CoAVolFog", L"MorningMist", "2.0" },
    { "lvExposure", L"CoAVolFog.ini", L"CoAVolFog", L"Exposure", "1.0" },
    { "lvSaturation", L"CoAVolFog.ini", L"CoAVolFog", L"Saturation", "1.0" },
    { "lvSkySaturation", L"CoAVolFog.ini", L"CoAVolFog", L"SkySaturation", "1.0" },
    { "lvLandBrightness", L"CoAVolFog.ini", L"CoAVolFog", L"LandBrightness", "1.0" },
    { "lvSkyBrightness", L"CoAVolFog.ini", L"CoAVolFog", L"SkyBrightness", "1.0" },
    { "lvShafts", L"CoAVolFog.ini", L"CoAVolFog", L"Shafts", "1.0" },
    { "lvCloudShadows", L"CoAVolFog.ini", L"CoAVolFog", L"CloudShadows", "0.6" },
    { "lxDensity", L"LuxVelum.ini", L"LuxVelum", L"Density", "1.0" },
    { "lxHaze", L"LuxVelum.ini", L"LuxVelum", L"Haze", "1.0" },
    { "lxGroundFog", L"LuxVelum.ini", L"LuxVelum", L"GroundFog", "0.6" },
    { "lxFarFog", L"LuxVelum.ini", L"LuxVelum", L"FarFog", "1.0" },
    { "lxExposure", L"LuxVelum.ini", L"LuxVelum", L"Exposure", "1.0" },
    { "lxAmbient", L"LuxVelum.ini", L"LuxVelum", L"Ambient", "1.0" },
    { "lxSunScatter", L"LuxVelum.ini", L"LuxVelum", L"SunScatter", "1.0" },
};

bool s_loading = false;

#if defined(_WIN32)
std::wstring IniPath(const wchar_t* file) {
    wchar_t path[MAX_PATH] = {};
    DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring dir(path, length);
    size_t slash = dir.find_last_of(L"\\/");
    return (slash == std::wstring::npos ? std::wstring() : dir.substr(0, slash + 1)) + file;
}
#endif

bool Changed(CVar*, const char*, const char* value, void* arg) {
    if (!value || !arg) {
        return false;
    }

#if defined(_WIN32)
    if (!s_loading) {
        auto* setting = static_cast<const Setting*>(arg);
        wchar_t text[64] = {};
        MultiByteToWideChar(CP_UTF8, 0, value, -1, text, 63);
        WritePrivateProfileStringW(setting->section, setting->key, text, IniPath(setting->file).c_str());
    }
#endif
    return true;
}

void RegisterSettings() {
    for (const Setting& setting : kSettings) {
        if (CVar::LookupRegistered(setting.cvar)) {
            continue;
        }

        // The default is the engine's, so the options' Defaults button undoes a player's tuning.
        CVar* cvar = CVar::Register(setting.cvar, "Graphics effects: a key of the pack's ini", 0, setting.standard, Changed, CATEGORY_GRAPHICS, false, const_cast<Setting*>(&setting), false);
        if (!cvar) {
            continue;
        }

#if defined(_WIN32)
        wchar_t text[64] = {};
        GetPrivateProfileStringW(setting.section, setting.key, L"", text, 63, IniPath(setting.file).c_str());
        if (text[0]) {
            char value[64] = {};
            WideCharToMultiByte(CP_UTF8, 0, text, -1, value, 63, nullptr, nullptr);
            s_loading = true;
            cvar->Set(value, true, false, false, false);
            s_loading = false;
        }
#endif
    }
}

const char kPage[] = R"lua(
if not VideoOptionsFrame or not CoALivingBuildPage or CoAEffectsPanel then return end

local function Times(v) return string.format("%.2fx", v) end

local p = CoALivingBuildPage("CoAEffectsPanel", "Graphics Effects", "Fog, light and colour of Lumenveil and LuxVelum, as their Ctrl+F7 windows set them. Okay applies them within a second; Defaults goes back to each pack's own values. Azeroth Remastered has its settings on the Living Azeroth pages.")
p:Header("Lumenveil")
p:Slider("lvDensity", "Fog density", "Multiplier on all of Lumenveil's fog.", 0, 3, 0.05, Times, 0.05)
p:Slider("lvHaze", "Distance haze", "The haze that builds up with distance.", 0, 3, 0.05, Times, 0.05)
p:Slider("lvGroundFog", "Ground fog", "Low mist lying on the ground.", 0, 3, 0.05, Times, 0.05)
p:Slider("lvFarFog", "Far fog", "The fog that replaces the game's own at the edge of view.", 0, 3, 0.05, Times, 0.05)
p:Slider("lvMorningMist", "Morning mist", "Mist in valleys around dawn.", 0, 8, 0.1, Times, 0.1)
p:Slider("lvExposure", "Exposure", "Overall brightness of the lit fog and scene.", 0.2, 3, 0.05, Times, 0.05)
p:Slider("lvSaturation", "Colour saturation", "How colourful the world is. Below 1 is paler, above 1 more vivid.", 0, 2, 0.05, Times, 0.05)
p:Slider("lvSkySaturation", "Sky saturation", "How colourful the sky is.", 0, 2, 0.05, Times, 0.05)
p:Slider("lvLandBrightness", "Land brightness", "Brightness of the ground and everything on it.", 0.25, 2, 0.05, Times, 0.05)
p:Slider("lvSkyBrightness", "Sky brightness", "Brightness of the sky.", 0.25, 4, 0.05, Times, 0.05)
p:Slider("lvShafts", "Sun shafts", "Strength of the light shafts through fog and trees.", 0, 4, 0.05, Times, 0.05)
p:Slider("lvCloudShadows", "Cloud shadows", "Shadows of passing clouds on the ground.", 0, 1, 0.05, Times, 0.05)
p:Header("LuxVelum")
p:Slider("lxDensity", "Fog density", "Multiplier on all of LuxVelum's fog.", 0, 3, 0.05, Times, 0.05)
p:Slider("lxHaze", "Distance haze", "The haze that builds up with distance.", 0, 3, 0.05, Times, 0.05)
p:Slider("lxGroundFog", "Ground fog", "Low mist lying on the ground.", 0, 3, 0.05, Times, 0.05)
p:Slider("lxFarFog", "Far fog", "The fog that replaces the game's own at the edge of view.", 0, 3, 0.05, Times, 0.05)
p:Slider("lxExposure", "Exposure", "Overall brightness of the lit fog.", 0.2, 3, 0.05, Times, 0.05)
p:Slider("lxAmbient", "Ambient light", "Light the fog takes from the sky.", 0, 3, 0.05, Times, 0.05)
p:Slider("lxSunScatter", "Sun glow", "How much the fog glows towards the sun.", 0, 3, 0.05, Times, 0.05)
p:Done()
)lua";

}  // namespace

void Visuals::InitializeEffectOptions() {
    RegisterSettings();
    FrameScript_Execute(kPage, "CoAEffectsPage", nullptr);
}
