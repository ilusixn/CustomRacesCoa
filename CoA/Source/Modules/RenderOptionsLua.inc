// The Graphics page's DLSS / Frame Generation / Reflex dropdowns (RenderOptionsUi::BuilderLua). Lua 5.1, FrameXML 3.3.5.
R"LUA(
local RESTART_TEXT = "Takes effect after restarting the game"
local DEFS = {
    { key = "dlss", cvar = "coaDlssMode", name = "CoARenderDlssDropDown", label = "DLSS", x = 199, y = -154,
      entries = { "Off", "DLAA", "Quality", "Balanced", "Performance", "Ultra Performance" } },
    { key = "frameGen", cvar = "coaFrameGen", name = "CoARenderFrameGenDropDown", label = "Frame Generation", x = 399, y = -98,
      entries = { "Off", "2x", "3x", "4x" } },
    { key = "reflex", cvar = "coaReflex", name = "CoARenderReflexDropDown", label = "NVIDIA Reflex", x = 399, y = -154,
      entries = { "Off", "On", "On + Boost" } },
}

local state, note, msaaDisabled
local drops = {}

local function Saved(def)
    return tonumber(GetCVar(def.cvar)) or 0
end

local function Usable(dd)
    return dd.option and dd.option.enabled
end

-- Reflex at least On while Frame Generation is chosen; DLSS turns Multisampling off.
local function Apply()
    local fg, reflex, dlss = drops.frameGen, drops.reflex, drops.dlss

    if fg.pending > 0 and reflex.pending == 0 then
        reflex.pending = 1
    end

    for _, dd in pairs(drops) do
        UIDropDownMenu_SetSelectedValue(dd, dd.pending)
        UIDropDownMenu_SetText(dd, dd.def.entries[dd.pending + 1] or dd.def.entries[1])
    end

    local msaa = VideoOptionsResolutionPanelMultiSampleDropDown

    if msaa then
        if Usable(dlss) and dlss.pending > 0 then
            UIDropDownMenu_DisableDropDown(msaa)
            msaaDisabled = true
        elseif msaaDisabled then
            UIDropDownMenu_EnableDropDown(msaa)
            msaaDisabled = nil
        end
    end

    -- A pending change whose plugin is not loaded waits for a restart.
    local waits = false

    for _, dd in pairs(drops) do
        if Usable(dd) and dd.option.restart and dd.pending ~= Saved(dd.def) then
            waits = true
        end
    end

    if waits then
        note:Show()
    else
        note:Hide()
    end
end

-- Pending choices start from the saved values when the Video options open (not on a category switch).
local function ResetPending()
    for _, dd in pairs(drops) do
        dd.pending = Saved(dd.def)
    end
end

local function Refresh()
    state = C_CoARender.GetOptionState()

    for _, dd in pairs(drops) do
        local option = state[dd.def.key]
        dd.option = option

        if option.enabled then
            UIDropDownMenu_EnableDropDown(dd)
            dd.reason:SetText("")
        else
            UIDropDownMenu_DisableDropDown(dd)
            dd.reason:SetText(option.reason or "")
        end
    end

    Apply()
end

local function Initialize(dd)
    local option = dd.option
    local max = option and option.max or 0

    for value = 0, max do
        local info = UIDropDownMenu_CreateInfo()
        info.text = dd.def.entries[value + 1]
        info.value = value
        info.checked = dd.pending == value
        info.func = function(button)
            dd.pending = button.value
            Apply()
        end
        UIDropDownMenu_AddButton(info)
    end
end

local function Build()
    local panel = VideoOptionsResolutionPanel

    if not panel or CoARenderDlssDropDown then
        return
    end

    for _, def in ipairs(DEFS) do
        local dd = CreateFrame("Frame", def.name, panel, "UIDropDownMenuTemplate")
        dd.def = def
        dd.pending = Saved(def)
        dd:SetPoint("TOPLEFT", panel, "TOPLEFT", def.x, def.y)
        UIDropDownMenu_SetWidth(dd, 150)
        UIDropDownMenu_Initialize(dd, Initialize)

        local label = dd:CreateFontString(def.name .. "Label", "BACKGROUND", "GameFontNormalSmall")
        label:SetPoint("BOTTOMLEFT", dd, "TOPLEFT", 16, 3)
        label:SetText(def.label)

        dd.reason = dd:CreateFontString(def.name .. "Reason", "BACKGROUND", "GameFontDisableSmall")
        dd.reason:SetPoint("TOPLEFT", dd, "BOTTOMLEFT", 20, 2)
        drops[def.key] = dd
    end

    note = drops.dlss:CreateFontString("CoARenderRestartNote", "BACKGROUND", "GameFontRedSmall")
    note:SetPoint("TOPLEFT", drops.dlss, "BOTTOMLEFT", 20, 2)
    note:SetText(RESTART_TEXT)

    -- The panel's own buttons: ours run after the stock handlers.
    local okay, cancel, default = panel.okay, panel.cancel, panel.default

    panel.okay = function(self, ...)
        if okay then okay(self, ...) end

        for _, dd in pairs(drops) do
            if dd.option and dd.option.enabled and dd.pending ~= Saved(dd.def) then
                SetCVar(dd.def.cvar, dd.pending)
            end
        end

        local dlss = drops.dlss

        if dlss.option and dlss.option.enabled and dlss.pending > 0 and GetCVar("gxMultisample") ~= "1" then
            SetCVar("gxMultisample", "1")
        end
    end

    panel.cancel = function(self, ...)
        if cancel then cancel(self, ...) end
        ResetPending()
        Refresh()
    end

    panel.default = function(self, ...)
        if default then default(self, ...) end

        for _, dd in pairs(drops) do
            SetCVar(dd.def.cvar, 0)
        end

        ResetPending()
        Refresh()
    end

    panel:HookScript("OnShow", Refresh)

    if VideoOptionsFrame then
        VideoOptionsFrame:HookScript("OnShow", function()
            ResetPending()
            Refresh()
        end)
    end

    ResetPending()
    Refresh()
end

Build()
)LUA"
