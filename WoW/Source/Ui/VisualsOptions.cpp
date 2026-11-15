#include "WoW/Source/Ui/Visuals.h"
#include <cstdio>
#include "Engine/Source/FrameScript/FrameScript.h"

// Modelled on wowdecomp's WowDecompVisualsDropDown, placed in the free strip under Global Settings.
// Okay writes coaVisuals (Visuals.txt); ACTIVE is the pack this run started with, for the restart note.
static const char kScript[] = R"lua(
if not VideoOptionsEffectsPanel or CoAVisualsDropDown then return end

local ACTIVE = "%u"
local panel = VideoOptionsEffectsPanel
local choices = {
    { value = "0", text = "Standard" },
    { value = "1", text = "Lumenveil" },
    { value = "2", text = "LuxVelum" },
    { value = "3", text = "Forever (Azeroth Remastered)" },
}

local dd = CreateFrame("Frame", "CoAVisualsDropDown", panel, "UIDropDownMenuTemplate")
dd:SetPoint("BOTTOMLEFT", panel, "BOTTOMLEFT", 0, 14)
UIDropDownMenu_SetWidth(dd, 230)

local note = dd:CreateFontString("CoAVisualsRestartNote", "BACKGROUND", "GameFontRedSmall")
note:SetPoint("LEFT", dd, "RIGHT", -6, 2)
note:SetText("Takes effect after a client restart")

local function NameOf(value)
    for _, choice in ipairs(choices) do
        if choice.value == value then return choice.text end
    end
    return choices[1].text
end

local function Show(self, value)
    self.shown = value
    UIDropDownMenu_SetSelectedValue(self, value)
    UIDropDownMenu_SetText(self, "Graphics: " .. NameOf(value))
    if value ~= ACTIVE then note:Show() else note:Hide() end
end

UIDropDownMenu_Initialize(dd, function()
    for _, choice in ipairs(choices) do
        local info = {}
        info.text = choice.text
        info.value = choice.value
        info.checked = dd.shown == choice.value
        info.func = function(button)
            dd.newValue = button.value
            Show(dd, button.value)
            if VideoOptionsFrameApply then VideoOptionsFrameApply:Enable() end
        end
        UIDropDownMenu_AddButton(info)
    end
end)

-- CoA's VideoOptions.lua enables/disables every registered control by method.
dd.Enable = function(self) self.disabled = nil; if UIDropDownMenu_EnableDropDown then UIDropDownMenu_EnableDropDown(self) end end
dd.Disable = function(self) self.disabled = true; if UIDropDownMenu_DisableDropDown then UIDropDownMenu_DisableDropDown(self) end end
dd.IsEnabled = function(self) return not self.disabled end
dd.SetEnabled = function(self, on) if on then self:Enable() else self:Disable() end end

dd.type = CONTROLTYPE_DROPDOWN
dd.defaultValue = "1"
dd.GetValue = function() return GetCVar("coaVisuals") end
dd.SetValue = function(self, value)
    self.value = value
    if GetCVar("coaVisuals") ~= value then SetCVar("coaVisuals", value) end
    Show(self, value)
end
dd.RefreshValue = function(self)
    self.value = GetCVar("coaVisuals") or ACTIVE
    self.newValue = nil
    Show(self, self.value)
end
if BlizzardOptionsPanel_RegisterControl then BlizzardOptionsPanel_RegisterControl(dd, panel) end
dd:RefreshValue()
)lua";

void Visuals::InitializeOptions() {
    static char script[sizeof(kScript) + 16];
    snprintf(script, sizeof(script), kScript, CoA::Visuals::Current());
    FrameScript_Execute(script, "CoAVisualsDropDown", nullptr);
}
