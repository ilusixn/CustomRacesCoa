#include "WoW/Source/Ui/Visuals.h"
#include "Engine/Source/FrameScript/FrameScript.h"

// Esteria's Living Azeroth pages (UIUtil/DistanceOptions.cpp), renamed; every control is a CVar, so Okay/Cancel/Defaults work.
static const char kHelper[] = R"lua(
if not VideoOptionsFrame or not VideoOptionsPanel_OnLoad or CoALivingBuildPage then return end

function CoALivingBuildPage(name, title, subtitle)
    local page = CreateFrame("Frame", name, VideoOptionsFramePanelContainer)
    page:Hide()
    page.name = title
    page.options = {}
    page.checks = {}
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
        check:SetScript("OnClick", function(self) BlizzardOptionsPanel_CheckButton_OnClick(self) end)
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
        BlizzardOptionsPanel_RegisterControl(slider, self)
        self.y = self.y - 46
        return slider
    end

    function page:Done()
        self.child:SetHeight(-self.y + 16)
        VideoOptionsPanel_OnLoad(self)
    end

    return page
end

)lua";

static const char kTrees[] = R"lua(
if not VideoOptionsFrame or not CoALivingBuildPage or CoALivingTreesPanel then return end

local function Degrees(v) return string.format("%.2f deg", v) end
local function Heading(v) return string.format("%d deg", v + 0.5) end
local function Speed(v) return string.format("%.1f yd/s", v) end
local function Yards(v) return string.format("%.1f yd", v) end
local function Plain(v) return string.format("%.2f", v) end
local function Limit(v) if v < 1 then return "Unlimited" end return string.format("%d yd", v + 0.5) end

local function WindRows(p, prefix, what, one)
    p:Slider(prefix .. "Direction", "Wind direction", "Heading the wind blows toward, in the world.", 0, 360, 1, Heading, 5)
    p:Slider(prefix .. "Speed", "Wave speed", "How fast the gusts travel across the " .. what .. ".", 0, 12, 0.1, Speed, 0.1)
    p:Slider(prefix .. "Amplitude", "Sway", "How far the top sways. Keep it under about 2 degrees for a breeze.", 0, 6, 0.05, Degrees, 0.05)
    p:Slider(prefix .. "Wavelength", "Wave length", "Longer waves make broader, slower gusts.", 2, 80, 0.5, Yards, 1)
    p:Slider(prefix .. "Lean", "Downwind lean", "A constant lean with the wind on top of the sway.", 0, 4, 0.05, Degrees, 0.05)
    p:Slider(prefix .. "CrossAmplitude", "Cross-swell", "A second, smaller wave across the wind, so the motion is not a single rhythm.", 0, 3, 0.01, Degrees, 0.05)
    p:Slider(prefix .. "CrossWavelength", "Cross-swell length", "Length of the second wave.", 2, 40, 0.5, Yards, 1)
    p:Slider(prefix .. "CrossAngle", "Cross-swell angle", "Heading of the second wave from the wind.", 0, 180, 1, Heading, 5)
    p:Slider(prefix .. "Variance", "Variety", "How much the sway differs from one " .. one .. " to the next. 0 makes them all alike.", 0, 1, 0.05, Plain, 0.05)
    p:Slider(prefix .. "Gust", "Gusts", "Depth of the slow rise and fall of the wind. 0 is steady.", 0, 1, 0.05, Plain, 0.05)
    p:Slider(prefix .. "DistanceFade", "Fade with distance", "Sway lost per yard from the camera. 0 sways far ones as much as near ones.", 0, 0.05, 0.001, function(v) return string.format("%.3f", v) end, 0.001)
end

local p = CoALivingBuildPage("CoALivingTreesPanel", "Living Azeroth: Trees", "Placed trees and bushes sway gently in the wind. Purely visual: collision and selection do not move.")
p:Header("Trees")
p:Check("treeWind", "Trees sway", "Master switch for the trees.")
p:Check("treeWindFollowWorld", "Follow the world wind", "Where WindProfile.cdbc gives a place wind, its heading, strength and passing gusts drive the trees and bushes; calm places stand still. Elsewhere the sliders below apply as they are.")
p:Slider("treeWindWorldScale", "World wind response", "Sway per unit of world wind. At 2, a half-strength wind gives the sway set below.", 0, 10, 0.1, Plain, 0.1)
WindRows(p, "treeWind", "trees", "tree")
p:Header("What counts as a tree")
p:Check("treeWindDoodadsOnly", "Placed objects only", "Only trees placed in the map sway, never units, weapons or spell effects.")
p:Check("treeWindMatchNames", "Match tree names", "The model's name must name a tree (tree, pine, oak, willow and so on).")
p:Check("treeWindMatchTallThin", "Match tall models", "The model must also be tall and narrow. Off by default: names alone decide.")
p:Check("treeWindExcludeDead", "Skip dead and bare trees", "Dead, burnt, bare and felled trees, stumps and logs stay still.")
p:Slider("treeWindMinHeight", "Least height", "Tall-model test: the model must be at least this tall.", 0, 40, 0.5, Yards, 1)
p:Slider("treeWindMinAspect", "Least height to width", "Tall-model test: height divided by the widest side.", 0.5, 6, 0.05, Plain, 0.1)
p:Slider("treeWindMaxHeight", "Greatest height", "Trees taller than this keep still, so giant world trees do not lean across the zone. 0 is unlimited.", 0, 300, 5, Limit, 5)
p:Slider("treeWindMaxDistance", "Greatest distance", "Trees further away than this keep still. 0 is unlimited.", 0, 500, 5, Limit, 5)
p:Header("Bushes")
p:Check("bushWind", "Bushes sway", "Bushes, shrubs, ferns and hedges, matched by name only: the tree size tests never apply.")
WindRows(p, "bushWind", "bushes", "bush")
p:Check("bushWindDoodadsOnly", "Placed objects only", "Only bushes placed in the map sway.")
p:Check("bushWindExcludeDead", "Skip dead and bare bushes", "Dead, burnt and bare bushes stay still.")
p:Slider("bushWindMaxDistance", "Greatest distance", "Bushes further away than this keep still. 0 is unlimited.", 0, 500, 5, Limit, 5)
p:Done()
)lua";
)lua";

static const char kWind[] = R"lua(
if not VideoOptionsFrame or not CoALivingBuildPage or CoALivingWindPanel then return end

local function Plain(v) return string.format("%.2f", v) end
local function Times(v) return string.format("%.2fx", v) end
local function Heading(v) return string.format("%d deg", v + 0.5) end
local function Speed(v) return string.format("%.1f yd/s", v) end
local function Yards(v) return string.format("%.1f yd", v) end
local function Distance(v) return string.format("%d yd", v + 0.5) end

local p = CoALivingBuildPage("CoALivingWindPanel", "Living Azeroth: Wind & Grass", "Wind per zone from WindProfile.cdbc: stronger in storms, with gusts that travel across the land, calm under roofs and behind walls. It moves the grass and the trees.")
p:Header("World wind")
p:Check("worldWind", "Enable the world wind", "Off: the trees and the grass use their own fixed wind (the sliders on their pages).")
p:Slider("worldWindStrength", "Strength", "Your multiplier on every place's wind. 1 is as the table says.", 0, 3, 0.05, Times, 0.05)
p:Check("worldWindShelter", "Shelter under roofs", "Roofs and overhangs keep the ground below calm.")
p:Check("worldWindLee", "Wind shadow behind walls", "Walls, cliffs and steep rock calm the air on their downwind side.")
p:Header("Grass")
p:Check("hdGrassWind", "Grass sways", "Grass moves in the wind, in every visuals mode. Outside Azeroth Remastered the grass keeps the game's own lighting.")
p:Check("hdGrassWindFollowWorld", "Follow the world wind", "Where the world wind blows, it sets the grass's heading and strength, and gusts pass over it.")
p:Slider("hdGrassWindStrength", "Sway", "Multiplier on the grass sway.", 0, 5, 0.05, Times, 0.05)
p:Slider("hdGrassWindSpeed", "Wave speed", "How fast the waves cross the grass.", 0, 12, 0.1, Speed, 0.1)
p:Slider("hdGrassWindDirection", "Fixed wind direction", "Heading of the grass wind where the world wind does not blow.", 0, 360, 1, Heading, 5)
p:Check("hdGrassReact", "Grass parts around you", "Grass bends away from your character.")
p:Slider("hdGrassReactRadius", "Parting radius", "How far around you the grass bends away.", 0.1, 20, 0.1, Yards, 0.5)
p:Check("hdGrassReactUnits", "Grass parts around others", "Creatures and other players near you part the grass too, each by its own size.")
p:Slider("hdGrassReactUnitScale", "Their parting size", "Parting radius around a creature or player, as a multiple of its size.", 0.5, 5, 0.1, Times, 0.1)
p:Slider("hdGrassReactMountedScale", "Wider when mounted", "How much wider the parting is around anyone mounted, you included.", 1, 5, 0.1, Times, 0.1)
p:Slider("groundEffectDist", "Grass distance", "How far grass is drawn. The game's own limit of 140 yards is lifted. Grass in range is baked once and kept on the GPU, up to the memory below.", 0, 1024, 5, Distance, 10)
p:Slider("grassBufferMemory", "Grass memory", "Most memory the kept grass may use. Grass past it is baked again every frame, which costs CPU time. Takes effect when the grass distance changes or the world loads.", 16, 4096, 16, function(v) return string.format("%d MB", v + 0.5) end, 64)
p:Header("Grass density")
p:Check("grassDensity", "Denser grass near you", "Extra plants on each plant's own patch of ground, as GroundEffectDoodadDensity.cdbc asks for each place and plant. They share the grass memory, so the grass reaches less far.")
p:Slider("grassDensityMaxMultiplier", "Most plants per plant", "Your limit on how dense grass gets. 1 is never denser.", 1, 8, 0.5, Times, 0.5)
p:Slider("grassDensityMaxRadius", "Dense radius", "Your limit on how far around you grass is made denser.", 0, 500, 5, Distance, 10)
p:Slider("grassDensityMaxSpread", "Spread", "Your limit on how far an extra plant may sit from its plant.", 0, 10, 0.1, Yards, 0.5)
p:Header("Far grass")
p:Check("grassThinning", "Thin far grass", "Far away, grass draws an even sample of its plants: much less GPU time at long grass distances, hardly visible.")
p:Slider("grassThinStart", "Full grass within", "Every plant is drawn this close to you.", 0, 1000, 5, Distance, 10)
p:Slider("grassThinEnd", "Thinnest from", "Where the thinning reaches its lowest share.", 0, 2000, 10, Distance, 25)
p:Slider("grassThinMinShare", "Thinnest share", "Share of plants drawn at that distance and beyond.", 0.05, 1, 0.05, function(v) return string.format("%d%%", v * 100 + 0.5) end, 0.05)
p:Done()
)lua";

static const char kSnow[] = R"lua(
if not VideoOptionsFrame or not CoALivingBuildPage or CoALivingSnowPanel then return end

local function Times(v) return string.format("%.2fx", v) end
local function Plain(v) return string.format("%.2f", v) end
local function Yards(v) return string.format("%.2f yd", v) end

local p = CoALivingBuildPage("CoALivingSnowPanel", "Living Azeroth: Snow", "Painted snow lies on the ground as a layer of its own, from SurfaceCover.cdbc: a few inches deep on flat ground, thinning on slopes, burying the grass, lit and shadowed like the ground. Near you it is lumpy and takes trenches.")
p:Header("Cover")
p:Check("surfaceCover", "Snow lies on the ground", "Master switch. Needs its shaders translated once on Vulkan (surfaceCoverInfo says so).")
p:Check("surfaceCoverOthers", "Sand and ash too", "The table's other covers: a thin layer of sand in deserts, ash in burnt lands. Off: only snow.")
p:Slider("surfaceCoverDepth", "Depth", "Multiplier on how deep the table says the cover lies. 1 is as the table says.", 0, 3, 0.05, Times, 0.05)
p:Slider("surfaceCoverBreakup", "Ragged edges", "Multiplier on how patchy the cover's edges are. 0 gives smooth, rounded edges.", 0, 3, 0.05, Times, 0.05)
p:Slider("surfaceCoverBrightness", "Brightness", "How bright the cover is under the same light as the ground.", 0.2, 1.5, 0.01, Plain, 0.02)
p:Slider("surfaceCoverDistanceLift", "Lift far away", "Far away the cover is lifted this much so it never flickers with the ground under it. Keep it small: more makes distant snow float.", 0, 3, 0.05, Yards, 0.05)
p:Header("Near you")
p:Check("surfaceCoverNear", "Fine snow around you", "A detailed, lumpy patch of cover 48 yards across follows you, lit by its own bumps. Off: the coarser cover everywhere, and no trenches.")
p:Check("surfaceCoverTrenches", "Trenches", "You and the creatures and players near you press trenches into the cover, with pushed-up rims. They fill back in over time.")
p:Slider("surfaceCoverTrenchScale", "Trench width", "Width of a trench as a multiple of each one's size. Mounts make wider ones.", 0.2, 4, 0.05, Times, 0.05)
p:Slider("surfaceCoverNearRange", "Fine snow range", "How far across the fine patch reaches. 48 yards is the finest; wider reaches further but is coarser (trenches get blurrier) and costs more.", 24, 192, 4, Yards, 4)
p:Header("Fallen snow")
p:Check("surfaceCoverFallen", "Snow falls and lies", "In snowstorms on ground below freezing, snow builds up on bare ground in the fine patch, and melts when it is warm. Kept per zone.")
p:Check("surfaceCoverMelt", "Painted snow melts", "Snow painted on the ground thins in warm weather, its edges retreating, and grows back when it is cold or snowing.")
p:Slider("surfaceCoverFallenRate", "Snowfall and melting speed", "How many times faster snow builds up and melts. 1 is about 18 cm an hour in a full storm.", 0, 200, 1, Times, 1)
p:Done()
)lua";
)lua";

void Visuals::InitializeLivingAzerothOptions() {
    FrameScript_Execute(kHelper, "CoALivingPageHelper", nullptr);
    FrameScript_Execute(kTrees, "CoALivingTreesPage", nullptr);
    FrameScript_Execute(kWind, "CoALivingWindPage", nullptr);
    FrameScript_Execute(kSnow, "CoALivingSnowPage", nullptr);
}
