// Not in the 3.3.5a binary. Kalibros' (wowdecomp Esteria) Video options for the graphics port: the page-builder helper, the
// "Visuals" dropdown of the Effects panel and Lumenveil's pages (every setting of CoAVolFog.ini, built from OptionsMeta.inc).
// Runs as built-in FrameXML after the signed UI loads, on login and /reload. Adapted from WoW/Source/UIUtil/DistanceOptions.cpp.
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Lumenveil/OptionsPages.h"

void GraphicsInitializeOptions() {
    static const char helpersScript[] = R"lua(
if not VideoOptionsFrame or WowDecompOutlinePanel then return end

-- Esteria option pages: the unit outline, the loot beam and the performance overlay. Every control is a CVar, so Okay,
-- Cancel and Defaults treat the pages like any other video panel. Colours are RRGGBB boxes that apply at once.
function WowDecompBuildPage(name, title, subtitle)
    local page = CreateFrame("Frame", name, VideoOptionsFramePanelContainer)
    page:Hide()
    page.name = title
    page.options = {}
    page.checks = {}
    page.boxes = {}
    page.sliders = {}
    page.y = -4

    local heading = page:CreateFontString(nil, "ARTWORK", "GameFontNormalLarge")
    heading:SetPoint("TOPLEFT", 16, -16)
    heading:SetText(title)

    local sub = page:CreateFontString(nil, "ARTWORK", "GameFontHighlightSmall")
    sub:SetPoint("TOPLEFT", heading, "BOTTOMLEFT", 0, -8)
    sub:SetPoint("RIGHT", -32, 0)
    sub:SetJustifyH("LEFT")
    sub:SetText(subtitle)

    local scroll = CreateFrame("ScrollFrame", name .. "Scroll", page, "UIPanelScrollFrameTemplate")
    scroll:SetPoint("TOPLEFT", 8, -60)
    scroll:SetPoint("BOTTOMRIGHT", -30, 8)
    local child = CreateFrame("Frame", name .. "Child", scroll)
    child:SetSize(500, 10)
    scroll:SetScrollChild(child)
    page.child = child

    page:SetScript("OnShow", function(self)
        for _, check in ipairs(self.checks) do
            check:SetChecked(GetCVar(check.cvar) == "1")
        end
        for _, box in ipairs(self.boxes) do
            box:SetText(GetCVar(box.cvar) or "")
            box:UpdateSwatch()
        end
    end)

    function page:Header(text)
        self.y = self.y - 10
        local fs = self.child:CreateFontString(nil, "ARTWORK", "GameFontNormal")
        fs:SetPoint("TOPLEFT", 8, self.y)
        fs:SetText(text)
        self.y = self.y - 24
    end

    function page:Check(cvar, text, tooltip)
        local check = CreateFrame("CheckButton", name .. "Check" .. cvar, self.child, "OptionsCheckButtonTemplate")
        check:SetPoint("TOPLEFT", 8, self.y)
        _G[check:GetName() .. "Text"]:SetText(text)
        check.type = CONTROLTYPE_CHECKBOX
        check.cvar = cvar
        check.tooltipText = tooltip
        check:SetScript("OnClick", function(self) BlizzardOptionsPanel_CheckButton_OnClick(self); if VideoOptionsFrameApply then VideoOptionsFrameApply:Enable() end end)
        self.options[cvar] = { text = text, tooltip = tooltip }
        BlizzardOptionsPanel_RegisterControl(check, self)
        self.checks[#self.checks + 1] = check
        self.y = self.y - 28
        return check
    end

    function page:Slider(cvar, text, tooltip, low, high, step, format, wheel)
        local sliderName = name .. "Slider" .. cvar
        local slider = CreateFrame("Slider", sliderName, self.child, "OptionsSliderTemplate")
        slider:SetSize(280, 17)
        slider:SetPoint("TOPLEFT", 12, self.y - 14)
        slider.type = CONTROLTYPE_SLIDER
        slider.cvar = cvar
        slider.SetDisplayValue = slider.SetValue
        slider.SetValue = function(self, value)
            self:SetDisplayValue(value)
            BlizzardOptionsPanel_SetCVarSafe(self.cvar, value)
            if VideoOptionsFrameApply then VideoOptionsFrameApply:Enable() end
        end
        _G[sliderName .. "Text"]:SetText(text)
        _G[sliderName .. "Low"]:SetText("")
        _G[sliderName .. "High"]:SetText("")

        local readout = CreateFrame("EditBox", sliderName .. "Edit", slider, "InputBoxTemplate")
        readout:SetSize(70, 20)
        readout:SetPoint("LEFT", slider, "RIGHT", 16, 0)
        readout:SetAutoFocus(false)
        readout:SetJustifyH("CENTER")
        readout:SetScript("OnEnterPressed", function(box)
            local value = tonumber(box:GetText())
            if value then slider:SetDisplayValue(value) end
            box:ClearFocus()
            box:SetText(format(slider:GetValue()))
        end)
        readout:SetScript("OnEscapePressed", function(box)
            box:ClearFocus()
            box:SetText(format(slider:GetValue()))
        end)

        slider:EnableMouseWheel(true)
        slider:SetScript("OnMouseWheel", function(self, delta)
            self:SetDisplayValue(self:GetValue() + delta * (wheel or step))
        end)
        slider:SetScript("OnValueChanged", function(self, value)
            self.newValue = value
            if not readout:HasFocus() then readout:SetText(format(value)) end
        end)
        slider:SetScript("OnShow", function(self)
            self:SetDisplayValue(tonumber(GetCVar(self.cvar)) or low)
        end)
        slider:SetMinMaxValues(low, high)
        slider:SetValueStep(step)
        self.options[cvar] = { text = text, minValue = low, maxValue = high, valueStep = step, tooltip = tooltip }
        self.sliders[cvar] = slider
        BlizzardOptionsPanel_RegisterControl(slider, self)
        self.y = self.y - 46
        return slider
    end

    function page:Color(cvar, text, tooltip)
        local label = self.child:CreateFontString(nil, "ARTWORK", "GameFontHighlight")
        label:SetPoint("TOPLEFT", 12, self.y - 4)
        label:SetText(text)

        local box = CreateFrame("EditBox", name .. "Color" .. cvar, self.child, "InputBoxTemplate")
        box:SetSize(70, 20)
        box:SetPoint("TOPLEFT", 230, self.y)
        box:SetAutoFocus(false)
        box:SetMaxLetters(6)
        box.cvar = cvar

        local swatch = self.child:CreateTexture(nil, "ARTWORK")
        swatch:SetSize(18, 18)
        swatch:SetPoint("LEFT", box, "RIGHT", 10, 0)
        swatch:SetTexture(1, 1, 1)

        function box:UpdateSwatch()
            local hex = self:GetText()
            if string.match(hex, "^%x%x%x%x%x%x$") then
                swatch:SetTexture(tonumber(string.sub(hex, 1, 2), 16) / 255, tonumber(string.sub(hex, 3, 4), 16) / 255, tonumber(string.sub(hex, 5, 6), 16) / 255)
            end
        end
        box:SetScript("OnTextChanged", function(self) self:UpdateSwatch() end)
        box:SetScript("OnEnterPressed", function(self)
            local hex = string.upper(string.gsub(self:GetText(), "#", ""))
            if string.match(hex, "^%x%x%x%x%x%x$") then
                SetCVar(self.cvar, hex)
            else
                self:SetText(GetCVar(self.cvar) or "")
            end
            self:ClearFocus()
        end)
        box:SetScript("OnEscapePressed", function(self)
            self:SetText(GetCVar(self.cvar) or "")
            self:ClearFocus()
        end)
        self.boxes[#self.boxes + 1] = box
        self.y = self.y - 28
        return box
    end

    -- Reset / Save my settings / Load my settings. Reset = Lumenveil's built-in values; Save keeps the current Lumenveil settings as your own
    -- preset (CoAVolFog.preset.ini); Load puts that preset back. Every change enables Apply.
    function page:ResetButton(defaults)
        local function refresh()
            for cvar, slider in pairs(self.sliders) do slider:SetDisplayValue(tonumber(GetCVar(cvar)) or 0) end
            for _, check in ipairs(self.checks) do check:SetChecked(GetCVar(check.cvar) == "1") end
            if VideoOptionsFrameApply then VideoOptionsFrameApply:Enable() end
        end
        local function make(label, x, width, onclick)
            local button = CreateFrame("Button", nil, self, "UIPanelButtonTemplate")
            button:SetSize(width, 22)
            button:SetPoint("TOPRIGHT", x, -14)
            button:SetText(label)
            button:SetScript("OnClick", onclick)
            return button
        end
        make("Reset to defaults", -40, 125, function()
            for _, pair in ipairs(defaults) do
                local cvar, value = pair[1], pair[2]
                local slider = self.sliders[cvar]
                if slider then
                    slider:SetValue(tonumber(value) or 0)
                else
                    SetCVar(cvar, value)
                    for _, check in ipairs(self.checks) do
                        if check.cvar == cvar then check:SetChecked(value == "1") end
                    end
                end
            end
            if VideoOptionsFrameApply then VideoOptionsFrameApply:Enable() end
        end)
        make("Save my settings", -170, 120, function()
            SetCVar("lumenveilPresetSave", tostring(GetTime()))
            print("Lumenveil: your settings are saved as your preset.")
        end)
        make("Load my settings", -295, 115, function()
            SetCVar("lumenveilPresetLoad", tostring(GetTime()))
            refresh()
        end)
    end

    function page:Done()
        self.child:SetHeight(-self.y + 16)
        VideoOptionsPanel_OnLoad(self)
    end

    return page
end

)lua";

    static const char visualsScript[] = R"lua(
if not VideoOptionsFrame or not VideoOptionsEffectsPanelPlayerTexture or WowDecompVisualsDropDown then return end

local panel = VideoOptionsEffectsPanel
local choices = {
    { value = "0", text = "Stock (Vulkan)" },
    { value = "1", text = "Lumenveil" },
    { value = "2", text = "Azeroth Remastered" },
}

local dd = CreateFrame("Frame", "WowDecompVisualsDropDown", panel, "UIDropDownMenuTemplate")
dd:SetPoint("TOPLEFT", VideoOptionsEffectsPanelPlayerTexture, "TOPLEFT", -184, -118)
UIDropDownMenu_SetWidth(dd, 190)

local function NameOf(value)
    for _, choice in ipairs(choices) do
        if choice.value == value then return choice.text end
    end
    return choices[2].text
end

local function Show(self, value)
    self.shown = value
    UIDropDownMenu_SetSelectedValue(self, value)
    UIDropDownMenu_SetText(self, "Visuals: " .. NameOf(value))
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

dd.type = CONTROLTYPE_DROPDOWN
dd.defaultValue = "1"
dd.GetValue = function() return GetCVar("visuals") end
dd.SetValue = function(self, value)
    self.value = value
    if GetCVar("visuals") ~= value then SetCVar("visuals", value) end
    Show(self, value)
end
dd.RefreshValue = function(self)
    self.value = GetCVar("visuals") or "1"
    self.newValue = nil
    Show(self, self.value)
end
BlizzardOptionsPanel_RegisterControl(dd, panel)
dd:RefreshValue()
)lua";

    FrameScript_Execute(helpersScript, "NativeGraphicsPageHelpers", nullptr);
    FrameScript_Execute(visualsScript, "NativeVisualsDropDown", nullptr);
    const std::string lumenveilScript = LumenveilOptions::BuildScript();
    FrameScript_Execute(lumenveilScript.c_str(), "NativeLumenveilPages", nullptr);
}
