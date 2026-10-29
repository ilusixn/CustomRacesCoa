CHAR_CREATE_HELP_NEXT_BUTTON = CHAR_CREATE_HELP_NEXT_BUTTON or "You can go to the next step\n\nYou can change these choices at any time"
CHAR_CREATE_CUSTOMIZE_YOUR_APPEARANCE = CHAR_CREATE_CUSTOMIZE_YOUR_APPEARANCE or "Customize Your Appearance "
CHAR_CREATE_CHANGE_ANY_TIME = CHAR_CREATE_CHANGE_ANY_TIME or "You can change these choices at any time!"
CHAR_CREATE_HELP_CHOOSE_CLASS = CHAR_CREATE_HELP_CHOOSE_CLASS or "Help Me Choose"
CLASS_GUIDE_ROLE_SELECT_TEXT = CLASS_GUIDE_ROLE_SELECT_TEXT or "Select Role"
CLASS_GUIDE_CATEGORY_SELECT_TEXT = CLASS_GUIDE_CATEGORY_SELECT_TEXT or "Select Category"
CLASS_GUIDE_CLASS_SELECT_TEXT = CLASS_GUIDE_CLASS_SELECT_TEXT or "Select Class"
CLASS_GUIDE_ROLE_SELECTED_TEXT = CLASS_GUIDE_ROLE_SELECTED_TEXT or "Selected Role:\n%s"
CLASS_GUIDE_CATEGORY_SELECTED_TEXT = CLASS_GUIDE_CATEGORY_SELECTED_TEXT or "Selected Category:\n%s"
CLASS_GUIDE_CLASS_SELECTED_TEXT = CLASS_GUIDE_CLASS_SELECTED_TEXT or "Selected Class:\n%s"

function C_CharacterCreate.CanCreateArchetype()
	return GetRealmName() == "Area 52 - Free-Pick" and C_Config.GetBoolConfig("CONFIG_CHARACTER_CREATION_ARCHETYPES_ENABLED")
end

CHARACTER_FACING_INCREMENT = 2;
MAX_RACES = 128;

-- racials shown for the extra races: those of the base race they borrow (same rule as the server)
CHAR_CREATE_EXTRA_RACE_RACIALS = {
	["WORGEN"] = "NIGHTELF", ["GOBLIN"] = "ORC", ["TUSKARR"] = "DWARF", ["KULTIRAN"] = "HUMAN", ["TAUNKA"] = "TAUREN",
	["VRYKUL"] = "HUMAN", ["VULPERA"] = "TROLL", ["NAGA_"] = "SCOURGE", ["BROKEN"] = "DRAENEI", ["FELORC"] = "ORC",
	["FORESTTROLL"] = "TROLL", ["ICETROLL"] = "TROLL", ["SKELETON"] = "SCOURGE", ["NPC_EARTHEN"] = "DWARF",
	["DRAKKARITROLL"] = "TROLL", ["ZANDALARI TROLL"] = "TROLL", ["PANDAREN"] = "DRAENEI", ["TROGLODYTE"] = "SCOURGE",
}
CHAR_CREATE_EXTRA_RACE_RACIALS_HORDE = { ["PANDAREN"] = "TAUREN", ["TROGLODYTE"] = "SCOURGE" }
CHAR_CREATE_EXTRA_RACE_INFO = {
	["WORGEN"] = "Cursed Gilneans who learned to master the beast within. Worgen fight with the speed and stealth of the Night Elves.",
	["GOBLIN"] = "Cunning merchants and inventors from Kezan, as tough as any Orc when the profit is right.",
	["TUSKARR"] = "Hardy fisher-folk of Northrend's coasts, as stubborn and cold-resistant as the Dwarves.",
	["KULTIRAN"] = "Proud sailors from the island kingdom of Kul Tiras, sharing the human spirit of their cousins.",
	["TAUNKA"] = "Northern kin of the Tauren, forged by the harsh winters of the Borean Tundra.",
	["VRYKUL"] = "Towering warriors of Northrend, distant ancestors of humanity.",
	["VULPERA"] = "Nimble desert scavengers from Vol'dun who survive with troll-like resilience.",
	["PANDAREN"] = "Wandering monks and brewers of Pandaria. Alliance Pandaren share the gifts of the Draenei, Horde Pandaren those of the Tauren.",
	["NAGA_"] = "Serpentine children of the deep, as unyielding as the Forsaken.",
	["BROKEN"] = "Draenei twisted by the fel, who still carry the Light of the Naaru.",
	["FELORC"] = "Orcs corrupted by demon blood, stronger and angrier than their brothers.",
	["FORESTTROLL"] = "Trolls of the Hinterlands and Eversong, regenerating and ferocious.",
	["ICETROLL"] = "Trolls of the frozen north, hardened by ice and hunger.",
	["SKELETON"] = "Restless dead risen once more, bound by the will of the Forsaken.",
	["NPC_EARTHEN"] = "Titan-forged guardians of stone, kin to the Dwarves.",
	["DRAKKARITROLL"] = "Trolls of the fallen Drakkari empire of Zul'Drak.",
	["ZANDALARI TROLL"] = "Proud trolls of Zandalar, eldest and wisest of their kind.",
	["TROGLODYTE"] = "Murlocs! Mrglglgl! Creatures of the shallows who breathe underwater, sharing the gifts of the Forsaken.",
}

-- signature racial borrowed from another race (server: playercreateinfo_spell_custom "racial combo")
CHAR_CREATE_EXTRA_RACE_SIGNATURE = {
	["WORGEN"] = {"TROLL", "Berserking"}, ["GOBLIN"] = {"GNOME", "Escape Artist"}, ["TUSKARR"] = {"TAUREN", "War Stomp"},
	["KULTIRAN"] = {"DWARF", "Stoneform"}, ["TAUNKA"] = {"DWARF", "Frost Resistance"}, ["VRYKUL"] = {"ORC", "Blood Fury"},
	["VULPERA"] = {"GNOME", "Escape Artist"}, ["NAGA_"] = {"BLOODELF", "Arcane Torrent"}, ["BROKEN"] = {"SCOURGE", "Will of the Forsaken"},
	["FELORC"] = {"BLOODELF", "Arcane Torrent"}, ["FORESTTROLL"] = {"NIGHTELF", "Shadowmeld"}, ["ICETROLL"] = {"DWARF", "Stoneform"},
	["SKELETON"] = {"DWARF", "Stoneform"}, ["NPC_EARTHEN"] = {"TAUREN", "War Stomp"}, ["DRAKKARITROLL"] = {"ORC", "Blood Fury"},
	["ZANDALARI TROLL"] = {"DRAENEI", "Gift of the Naaru"}, ["PANDAREN"] = {"TAUREN", "War Stomp"}, ["TROGLODYTE"] = {"NIGHTELF", "Shadowmeld"},
}
CHAR_CREATE_EXTRA_RACE_SIGNATURE_HORDE = { ["PANDAREN"] = {"DRAENEI", "Gift of the Naaru"}, ["TROGLODYTE"] = {"GNOME", "Escape Artist"} }

function CharacterCreate_AliasExtraRacials(faction)
	for race, base in pairs(CHAR_CREATE_EXTRA_RACE_RACIALS) do
		if faction == "Horde" and CHAR_CREATE_EXTRA_RACE_RACIALS_HORDE[race] then
			base = CHAR_CREATE_EXTRA_RACE_RACIALS_HORDE[race]
		end
		for i = 1, 10 do
			_G["ABILITY_INFO_"..race..i] = _G["ABILITY_INFO_"..base..i]
		end
		local passives = CHAR_CREATE_PASSIVES and CHAR_CREATE_PASSIVES[base]
		local count = 0
		while _G["ABILITY_INFO_"..race..(count + 1)] do
			count = count + 1
		end
		local signature = (faction == "Horde" and CHAR_CREATE_EXTRA_RACE_SIGNATURE_HORDE[race]) or CHAR_CREATE_EXTRA_RACE_SIGNATURE[race]
		local signaturePassive
		if signature then
			for i = 1, 10 do
				local text = _G["ABILITY_INFO_"..signature[1]..i]
				if text and string.find(text, signature[2], 1, true) then
					count = count + 1
					_G["ABILITY_INFO_"..race..count] = text
					signaturePassive = CHAR_CREATE_PASSIVES and CHAR_CREATE_PASSIVES[signature[1]] and CHAR_CREATE_PASSIVES[signature[1]][i]
					break
				end
			end
			for i = count + 1, 10 do
				_G["ABILITY_INFO_"..race..i] = nil
			end
		end
		if CHAR_CREATE_PASSIVES then
			local list = {}
			for i, spell in ipairs(passives or {}) do
				list[i] = spell
			end
			if signaturePassive then
				list[count] = signaturePassive
			end
			CHAR_CREATE_PASSIVES[race] = list
		end
		if not _G["RACE_INFO_"..race] then
			_G["RACE_INFO_"..race] = CHAR_CREATE_EXTRA_RACE_INFO[race]
		end
	end
end

CHAR_CREATE_EXTRA_RACE_ICONS = {
	["Worgen"] = "Interface\\Icons\\_BeastTaming_Worgen",
	["Goblin"] = "Interface\\Icons\\INV_Gizmo_GoblinBoomBox_01",
	["High Elf"] = "Interface\\Icons\\_HighElf_01",
	["Tuskarr"] = "Interface\\Icons\\INV_Misc_Head_Tuskarr",
	["Kul Tiran"] = "Interface\\Icons\\achievement_alliedrace_kultiranhuman",
	["Taunka"] = "Interface\\Icons\\INV_Misc_Head_Tauren_02",
	["Vrykul"] = "Interface\\Icons\\INV_Misc_Head_Vrykul",
	["Vulpera"] = "Interface\\Icons\\achievement_alliedrace_vulpera",
	["Pandaren"] = "Interface\\Icons\\achievement_character_pandaren_female",
	["Naga"] = "Interface\\Icons\\achievement_boss_elitenagamale",
	["Broken"] = "Interface\\Icons\\Achievement_Character_Draenei_Male",
	["Fel Orc"] = "Interface\\Icons\\achievement_character_orc_male_brn",
	["Forest Troll"] = "Interface\\Icons\\INV_Misc_Head_Troll_01",
	["Ice Troll"] = "Interface\\Icons\\INV_Misc_Head_Troll_02",
	["Skeleton"] = "Interface\\Icons\\SkeletonPowerful",
	["Earthen"] = "Interface\\Icons\\ability_earthen_titanwroughtframe",
	["Drakkari Troll"] = "Interface\\Icons\\Achievement_Character_Troll_Male",
	["Zandalari Troll"] = "Interface\\Icons\\achievement_alliedrace_zandalaritroll",
	["Murloc"] = "Interface\\Icons\\INV_Misc_Head_Murloc_01",
}

-- Local server: extra playable races. Buttons beyond the 11 of the XML are created on demand
-- and laid out in columns, Alliance on the left and Horde on the right.
function CharacterCreate_EnsureRaceButtons()
	for i = 1, MAX_RACES do
		if not _G["CharCreateRaceButton"..i] then
			local button = CreateFrame("CheckButton", "CharCreateRaceButton"..i, CharCreateRaceButtonsFrame, "CharCreateRaceButtonTemplate")
			button:SetID(i)
			button:Hide()
		end
	end
end

function CharacterCreate_LayoutRaceButtons(numRaces)
	local hidden = CharacterCreate_GetHiddenRaceSet and CharacterCreate_GetHiddenRaceSet() or {}
	local perColumn, columnWidth, rowHeight, scale = 14, 128, 34, 0.54
	local count = { Alliance = 0, Horde = 0 }
	local shown = 0
	for i = 1, numRaces do
		local button = _G["CharCreateRaceButton"..i]
		if button then
			local name = button.Text and button.Text:GetText() or ""
			local hide = hidden["#"..i] or hidden[name] or hidden[strlower(name or "")]
			if hide then
				button.coaUserHidden = true
				button:Hide()
			else
				button.coaUserHidden = nil
				local ok, _, faction = pcall(GetFactionForRace, i)
				if not ok or faction ~= "Horde" then
					faction = "Alliance"
				end
				local k = count[faction]
				count[faction] = k + 1
				local column, row = math.floor(k / perColumn), k % perColumn
				button:SetScale(scale)
				button:ClearAllPoints()
				if faction == "Alliance" then
					button:SetPoint("TOPLEFT", CharCreateRaceButtonsFrame, "TOPLEFT", (28 + column * columnWidth) / scale, (-62 - row * rowHeight) / scale)
				else
					button:SetPoint("TOPRIGHT", CharCreateRaceButtonsFrame, "TOPRIGHT", (-28 - column * columnWidth) / scale, (-62 - row * rowHeight) / scale)
				end
				button:SetFaction(faction)
				button:Show()
				shown = shown + 1
			end
		end
	end
	if shown == 0 then
		for i = 1, numRaces do
			local button = _G["CharCreateRaceButton"..i]
			if button then
				button.coaUserHidden = nil
				button:Show()
			end
		end
	end
	if CharacterCreate_EnsureRaceFilterButton then
		CharacterCreate_EnsureRaceFilterButton()
	end
end
MAX_CLASSES_PER_RACE = 32;
NUM_CHAR_CUSTOMIZATIONS = 5;
MIN_CHAR_NAME_LENGTH = 2;
CHARACTER_CREATE_ROTATION_START_X = nil;
CHARACTER_CREATE_INITIAL_FACING = nil;
NUM_PREVIEW_FRAMES = 14;

CUSTOM_CLASSES_START_INDEX = 12
HERO_CLASS_ID = 10

-- Local server: let the classic (vanilla) classes be created next to the CoA classes.
local VANILLA_CLASS_IDS = {[1]=true,[2]=true,[3]=true,[4]=true,[5]=true,[6]=true,[7]=true,[8]=true,[9]=true,[11]=true}
function CharacterCreate_EnableVanillaClasses()
	if not C_CharacterCreate then
		return
	end
	C_CharacterCreate.CanCreateCoA = function() return true end
	C_CharacterCreate.CanCreateWCR = function() return true end
	if CharacterCreateUtil and CharacterCreateUtil.IsRaceClassEnabledAndValid and not CharacterCreateUtil.coaRaceClassWrapped then
		local origValid = CharacterCreateUtil.IsRaceClassEnabledAndValid
		CharacterCreateUtil.IsRaceClassEnabledAndValid = function(race, classID, ...)
			if type(classID) == "number" and classID >= CUSTOM_CLASSES_START_INDEX then
				return true
			end
			return origValid(race, classID, ...)
		end
		CharacterCreateUtil.coaRaceClassWrapped = true
	end
	if C_CharacterCreate.vanillaClassesEnabled then
		return
	end
	local origCanCreateClass = C_CharacterCreate.CanCreateClass
	C_CharacterCreate.CanCreateClass = function(classID, ...)
		if VANILLA_CLASS_IDS[classID] then
			return true
		end
		-- Native CanCreateClass follows the WCR flag and hides CoA ids 12+ when both
		-- models are advertised. Keep the CoA row populated; the server still validates.
		if type(classID) == "number" and classID >= CUSTOM_CLASSES_START_INDEX then
			return true
		end
		return origCanCreateClass(classID, ...)
	end
	-- CharBaseInfo.dbc holds at most 30 classes per race in this client, so Death Knight
	-- is left out of it; accept it here, the server validates the real combination.
	local origIsRaceClassValid = IsRaceClassValid
	IsRaceClassValid = function(race, classID, ...)
		if classID == 6 or (type(classID) == "number" and classID >= CUSTOM_CLASSES_START_INDEX) then
			return true
		end
		return origIsRaceClassValid(race, classID, ...)
	end
	C_CharacterCreate.vanillaClassesEnabled = true
end
CharacterCreate_EnableVanillaClasses()

IS_ZOOMED = false
local featureIndex = 1
local FeatureType = 1

PAID_CHARACTER_CUSTOMIZATION = 1;
PAID_RACE_CHANGE = 3;
PAID_FACTION_CHANGE = 2;
PAID_SERVICE_CHARACTER_ID = nil;
PAID_SERVICE_TYPE = nil;

PREVIEW_FRAME_HEIGHT = 130;
PREVIEW_FRAME_X_OFFSET = 19;
PREVIEW_FRAME_Y_OFFSET = -7;

FACTION_SHADOW_COLORS = {
	Horde = CreateColor(0.90, 0.05, 0.07),
	Alliance = CreateColor(0.29, 0.33, 0.91),
	Player = CreateColor(1, 1, 1),
}

FACTION_BACKDROP_COLOR_TABLE = {
	["Alliance"] = {0.5, 0.5, 0.5, 0.09, 0.09, 0.19, 0, 0, 0.2, 0.29, 0.33, 0.91},
	["Horde"] = {0.5, 0.2, 0.2, 0.19, 0.05, 0.05, 0.2, 0, 0, 0.90, 0.05, 0.07},
	["Player"] = {0.2, 0.5, 0.2, 0.05, 0.2, 0.05, 0.05, 0.2, 0.05, 1, 1, 1},
};

FRAMES_TO_BACKDROP_COLOR = {
	"CharacterCreateCharacterRace",
	"CharacterCreateCharacterClass",
--	"CharacterCreateCharacterFaction",
	"CharacterCreateNameEdit",
};

BANNER_DEFAULT_TEXTURE_COORDS = {0.109375, 0.890625, 0.201171875, 0.80078125};
BANNER_DEFAULT_SIZE = {200, 308};

CHAR_CUSTOMIZE_HAIR_COLOR = 4;

COA_CLASS_ORDER = {
	"NECROMANCER",
	"PYROMANCER",
	"CULTIST",
	"STARCALLER",
	"SUNCLERIC",
	"TINKER",
	"SPIRITMAGE",
	"WILDWALKER",
	"REAPER",
	"PROPHET",
	"CHRONOMANCER",
	"SONOFARUGAL",
	"GUARDIAN",
	"STORMBRINGER",
	"DEMONHUNTER",
	"BARBARIAN",
	"WITCHDOCTOR",
	"WITCHHUNTER",
	"FLESHWARDEN",
	"MONK",
	"RANGER",
}

CLASSIC_CLASS_ORDER = {
	"WARRIOR",
	"PALADIN",
	"HUNTER",
	"ROGUE",
	"PRIEST",
	"DEATHKNIGHT",
	"SHAMAN",
	"MAGE",
	"WARLOCK",
	"DRUID",
	"HERO"
}

--
-- Character create steps:
--

CHAR_CREATE_STEPS_DEFAULT = {
	Enum.CharCreateSteps.ClassRace,
	Enum.CharCreateSteps.Customization,
}

CHAR_CREATE_STEPS_CLASS_GUIDE = {
	Enum.CharCreateSteps.ClassRace,
	Enum.CharCreateSteps.NameEdit,
}

CHAR_CREATE_STEPS_EXTEND = {
	Enum.CharCreateSteps.ClassRace,
	Enum.CharCreateSteps.Customization,
	Enum.CharCreateSteps.Role,
	Enum.CharCreateSteps.Subrole,
	Enum.CharCreateSteps.Archetype,
	Enum.CharCreateSteps.NameEdit,
}

CHAR_CREATE_CAMERA_ZOOM_IN = { ["FacelessHorde"] = 1.25, ["QirajiHorde"] = 1.3, ["Vrykul"] = 1.3, ["KyrianAlliance"] = 1.3, ["VenthyrHorde"] = 1.5, ["CentaurAlliance"] = 1.2, ["CentaurHorde"] = 1.2, ["HighmountainTauren"] = 1.5,
	["Human"] = HD_PATCH_ENABLED and 3 or 1.5,
	["Orc"] = 1.5,
	["Troll"] = 2.5,
	["Scourge"] = 1,
	["BloodElf"] = 2.5,
	["Tauren"] = 1.5,
}

CHAR_CREATE_ZOOM = 2

--
-- Zoom/model change
--
local ZoomController = CreateFrame("FRAME")
ZoomController.zoom = 0

ZoomController:SetScript("OnUpdate", function(self, elapsed)
	local _, fileString = GetNameForRace()
	local zoomInValue = self.manualZoomIn or CHAR_CREATE_CAMERA_ZOOM_IN[fileString] or CHAR_CREATE_ZOOM
	local zoomOutValue = self.manualZoomOut or 0 

	if IS_ZOOMED and (self.zoom < zoomInValue) then
		local step = (zoomInValue-self.zoom)*0.02
		C_CharacterCreate.ZoomIn(step)
		self.zoom = self.zoom + step

		if math.NearlyEquals(self.zoom, zoomInValue, 0.1) then
			self:Hide()
		end
	elseif not(IS_ZOOMED) and (self.zoom > zoomOutValue) then
		local step = self.zoom*0.02
		C_CharacterCreate.ZoomOut(step)
		self.zoom = self.zoom - step

		if math.NearlyEquals(self.zoom, 0, 0.1) then
			self:Hide()
		end
	else
		self:Hide()
	end
end)

ZoomController:Hide()

local function CharCreate_ResetCamera()
	C_CharacterCreate.ResetCameraPosition()
	ZoomController.zoom = 0
	ZoomController.manualZoomIn = nil
	ZoomController.manualZoomOut = nil
	ZoomController:Hide()
	IS_ZOOMED = false
end

local CLASS_GUIDE_CHOICE_BUTTON_WIDTH = 96
local CLASS_GUIDE_CHOICE_BUTTON_HEIGHT = 128
local CLASS_GUIDE_CHOICE_BUTTON_SPACING = 56
local CLASS_GUIDE_CHOICE_BUTTON_Y_OFFSET = -28
local CLASS_GUIDE_FALLBACK_ATLAS = "ExperienceIconAscension"
local CLASS_GUIDE_COMBAT_HEALER_ATLAS = "ExperienceIconBattleHeal"
local CLASS_GUIDE_COMBAT_HEALER_TOOLTIP_TITLE = "Combat Healer"
local CLASS_GUIDE_COMBAT_HEALER_TOOLTIP_TEXT = "Restores allies by actively fighting and weaving offensive abilities in between healing casts."
local CLASS_GUIDE_ROLE_ATLAS = {
	TANK = "ExperienceIconTank",
	HEALER = "ExperienceIconHeal",
	DAMAGE = "ExperienceIconDamage",
	SUPPORT = "ExperienceIconFreepick",
}
local CLASS_GUIDE_SUBROLE_ATLAS = {
	MELEE_DPS = "ExperienceIconMeleeDD",
	RANGED_DPS = "ExperienceIconRanged",
	CASTER_DPS = "ExperienceIconMagic",
	HYBRID = "ExperienceIconHybridDD",
	TANK = "ExperienceIconTank",
	PURE_HEALER = "ExperienceIconSuperHeal",
	HEALER = "ExperienceIconHeal",
	COMBAT_HEALER = "ExperienceIconBattleHeal",
	SUPPORT = "ExperienceIconFreepick",
}
local CLASS_GUIDE_COMBAT_HEALER_CLASS_FILES = {
	CULTIST = true,
	STARCALLER = true,
}

local function CharacterCreate_CreateClassGuideButton(parent)
	local button = CreateFrame("CheckButton", nil, parent, "CharacterCreateArchetypeCoiceButtonTemplate")
	button:SetSize(CLASS_GUIDE_CHOICE_BUTTON_WIDTH, CLASS_GUIDE_CHOICE_BUTTON_HEIGHT)

	return button
end

local function CharacterCreate_LayoutClassGuideButtons(buttons, count, yOffset)
	local width = CLASS_GUIDE_CHOICE_BUTTON_WIDTH
	local spacing = CLASS_GUIDE_CHOICE_BUTTON_SPACING
	local totalWidth = count * width + math.max(count - 1, 0) * spacing
	local startX = -totalWidth / 2 + width / 2

	for i = 1, count do
		local button = buttons[i]
		button:ClearAllPoints()
		button:SetPoint("TOP", CharCreateClassGuideFrame, "TOP", startX + (i - 1) * (width + spacing), yOffset)
		button:Show()
	end
end

local function CharacterCreate_HideClassGuide()
	if CharCreateClassGuideFrame then
		CharCreateClassGuideFrame:Hide()
	end
end

local function CharacterCreate_HasClassGuideAPI()
	return C_CharacterCreate.GetClassGuideRoles
		and C_CharacterCreate.GetClassGuideRoleInfo
		and C_CharacterCreate.GetClassGuideSubroles
		and C_CharacterCreate.GetClassGuideSubroleInfo
		and C_CharacterCreate.GetClassGuideSubroleClasses
end

local function CharacterCreate_SetClassGuideEntryShown(shown)
	if CharCreateClassGuideEntryButton then
		CharCreateClassGuideEntryButton:SetEnabled(CharacterCreate_HasClassGuideAPI() and true or false)
		CharCreateClassGuideEntryButton:SetShown(shown)
	end
end

local function CharacterCreate_HasClassGuideData()
	if not CharacterCreate_HasClassGuideAPI() then
		return false
	end

	local roles = C_CharacterCreate.GetClassGuideRoles()
	return roles and #roles > 0
end

local function CharacterCreate_SetClassGuideRaceLocked(locked)
	for i = 1, MAX_RACES do
		local button = _G["CharCreateRaceButton"..i]
		if button then
			button:EnableMouse(not locked)
		end
	end
end

local function CharacterCreate_ClassMatchesGuideSubrole(classID, classFileName, subroleID)
	if not subroleID or not C_CharacterCreate.GetClassGuideSubroleInfo then
		return true
	end

	local _, _, classRoleKey = C_CharacterCreate.GetClassGuideSubroleInfo(subroleID)
	if not classRoleKey then
		return false
	end

	if C_CharacterCreate.GetClassGuideSubroleClasses then
		local classes = C_CharacterCreate.GetClassGuideSubroleClasses(subroleID)
		if classes and #classes > 0 then
			for _, guideClassID in ipairs(classes) do
				if guideClassID == classID then
					return true
				end
			end

			return false
		end
	end

	if classRoleKey == "Hybrid" then
		if not C_CharacterCreate.GetClassGuideSubroleClasses then
			return false
		end

		local classes = C_CharacterCreate.GetClassGuideSubroleClasses(subroleID)
		for _, guideClassID in ipairs(classes) do
			if guideClassID == classID then
				return true
			end
		end

		return false
	end

	local roles = ClassInfoUtil.GetClassRoles(classFileName)
	return roles and roles[classRoleKey]
end

local function CharacterCreate_ClassPassesGuideFilter(classID, classFileName)
	if not CharacterCreate.classGuideActive then
		return true
	end

	if CharacterCreate.selectedClassGuideSubroleID then
		return CharacterCreate_ClassMatchesGuideSubrole(classID, classFileName, CharacterCreate.selectedClassGuideSubroleID)
	end

	return false
end

local function CharacterCreate_IsClassGuideHealerSubrole(subroleID)
	if not CharacterCreate.selectedClassGuideRoleID
		or not subroleID
		or not C_CharacterCreate.GetClassGuideRoleInfo
		or not C_CharacterCreate.GetClassGuideSubroleInfo then
		return false
	end

	local roleKey = C_CharacterCreate.GetClassGuideRoleInfo(CharacterCreate.selectedClassGuideRoleID)
	if roleKey ~= "HEALER" then
		return false
	end

	local _, subroleKey = C_CharacterCreate.GetClassGuideSubroleInfo(subroleID)
	return subroleKey == "HEALER" or subroleKey == "PURE_HEALER"
end

local function CharacterCreate_IsClassGuideCombatHealer(classFileName)
	return CLASS_GUIDE_COMBAT_HEALER_CLASS_FILES[classFileName] and true or false
end

local function CharacterCreate_ClassGuideClassIsAvailableForRace(classID, raceID)
	if not classID or not CharacterCreate.selectedClassGuideSubroleID then
		return false
	end

	if not C_CharacterCreate.CanCreateClass(classID)
		or not CharacterCreateUtil.IsRaceClassEnabledAndValid(raceID or CharacterCreate.selectedRace, classID) then
		return false
	end

	local _, classFileName = GetClassInfo(classID)
	return classFileName and CharacterCreate_ClassMatchesGuideSubrole(classID, classFileName, CharacterCreate.selectedClassGuideSubroleID)
end

local function CharacterCreate_GetFirstAvailableClassGuideClassForRace(raceID)
	if not CharacterCreate.selectedClassGuideSubroleID then
		return nil
	end

	for i = 1, 21 do
		local classID = Enum.Class[COA_CLASS_ORDER[i]]
		if CharacterCreate_ClassGuideClassIsAvailableForRace(classID, raceID) then
			return classID
		end
	end
end

local function CharacterCreate_GetSingleClassGuideSubrole(roleID)
	if not roleID or not C_CharacterCreate.GetClassGuideSubroles or not C_CharacterCreate.GetClassGuideSubroleInfo then
		return nil
	end

	local singleSubroleID
	local subroleCount = 0
	local subroles = C_CharacterCreate.GetClassGuideSubroles(roleID)
	for _, subroleID in ipairs(subroles) do
		local _, _, _, _, name = C_CharacterCreate.GetClassGuideSubroleInfo(subroleID)
		if name then
			subroleCount = subroleCount + 1
			singleSubroleID = subroleID
		end
	end

	if subroleCount == 1 then
		return singleSubroleID
	end
end

local function CharacterCreate_SelectClassGuideRole(roleID)
	CharacterCreate.selectedClassGuideRoleID = roleID
	CharacterCreate.selectedClassGuideSubroleID = CharacterCreate_GetSingleClassGuideSubrole(roleID)
	CharacterCreate.selectedClassGuideClassID = nil
	CharCreateOkayButton:Disable()
	CharacterCreate_RefreshClassButtons(CharacterCreate)
end

local function CharacterCreate_SelectClassGuideSubrole(subroleID)
	CharacterCreate.selectedClassGuideSubroleID = subroleID
	CharacterCreate.selectedClassGuideClassID = nil
	CharCreateOkayButton:Disable()
	CharacterCreate_RefreshClassButtons(CharacterCreate)
end

local function CharacterCreate_OpenClassGuide()
	if not CharacterCreate_HasClassGuideData() then
		return
	end

	CharacterCreate.classGuideActive = true
	CharacterCreate.selectedClassGuideRoleID = nil
	CharacterCreate.selectedClassGuideSubroleID = nil
	CharacterCreate.selectedClassGuideClassID = nil
	CharCreateOkayButton:SetText(CONTINUE)
	CharCreateOkayButton:Disable()
	CharacterCreate_SetClassGuideRaceLocked(true)
	CharacterCreate_RefreshClassButtons(CharacterCreate)
end

local function CharacterCreate_CloseClassGuide()
	CharacterCreate.classGuideActive = false
	CharacterCreate.selectedClassGuideRoleID = nil
	CharacterCreate.selectedClassGuideSubroleID = nil
	CharacterCreate.selectedClassGuideClassID = nil
	CharCreateOkayButton:SetText(CUSTOMIZE)
	CharCreate_EnableNextButton(true)
	CharacterCreate_SetClassGuideRaceLocked(false)
	if CharacterCreateFrame.state == Enum.CharCreateSteps.ClassRace and CharacterCreate_LoadNavigation then
		CharacterCreate_LoadNavigation(CHAR_CREATE_STEPS_DEFAULT)
		CharacterCreateNavigationFrame:GoToIndex(Enum.CharCreateSteps.ClassRace, true)
	end
	CharacterCreate_RefreshClassButtons(CharacterCreate)
end

local function CharacterCreate_ClassGuideBack()
	if CharacterCreate.selectedClassGuideSubroleID then
		CharacterCreate.selectedClassGuideSubroleID = nil
		CharacterCreate.selectedClassGuideClassID = nil
		if CharacterCreate_GetSingleClassGuideSubrole(CharacterCreate.selectedClassGuideRoleID) then
			CharacterCreate.selectedClassGuideRoleID = nil
		end
	elseif CharacterCreate.selectedClassGuideRoleID then
		CharacterCreate.selectedClassGuideRoleID = nil
	else
		CharacterCreate_CloseClassGuide()
		return
	end

	CharCreateOkayButton:Disable()
	CharacterCreate_RefreshClassButtons(CharacterCreate)
end

local function CharacterCreate_CreateClassGuideFrame()
	if not CharCreateClassGuideEntryButton then
		CharCreateClassGuideEntryButton = CreateFrame("Button", "CharCreateClassGuideEntryButton", CharacterCreateFrame, "CharCreateNavButtonTemplate")
		CharCreateClassGuideEntryButton:SetSize(150, 50)
		CharCreateClassGuideEntryButton:SetPoint("BOTTOM", CharCreateBackButton, "TOP", 0, 8)
		CharCreateClassGuideEntryButton:SetText(CHAR_CREATE_HELP_CHOOSE_CLASS)
		CharCreateClassGuideEntryButton:SetScript("OnClick", CharacterCreate_OpenClassGuide)
		CharCreateClassGuideEntryButton:Hide()
	end

	if not CharCreateClassGuideFrame then
		CharCreateClassGuideFrame = CreateFrame("Frame", "CharCreateClassGuideFrame", CharCreateClassFrame)
		CharCreateClassGuideFrame:SetSize(760, 184)
		CharCreateClassGuideFrame:SetPoint("CENTER", CharCreateClassFrame, "CENTER", 0, 0)
		CharCreateClassGuideFrame.roleButtons = {}
		CharCreateClassGuideFrame.subroleButtons = {}

		CharCreateClassGuideFrame.BackButton = CreateFrame("Button", nil, CharCreateClassGuideFrame, "CharCreateNavButtonTemplate")
		CharCreateClassGuideFrame.BackButton:SetSize(150, 50)
		CharCreateClassGuideFrame.BackButton:SetPoint("BOTTOMLEFT", CharCreateClassGuideFrame, "TOPLEFT", 0, 8)
		CharCreateClassGuideFrame.BackButton:SetText(BACK)
		CharCreateClassGuideFrame.BackButton:SetScript("OnClick", CharacterCreate_ClassGuideBack)
		CharCreateClassGuideFrame.BackButton:Hide()
		CharCreateClassGuideFrame:Hide()
	end
end

local function CharacterCreate_GetClassGuideAtlas(atlas)
	if atlas and AtlasUtil:AtlasExists(atlas) then
		return atlas
	end

	return CLASS_GUIDE_FALLBACK_ATLAS
end

local function CharacterCreate_SetClassGuideButton(button, id, text, atlas, description, onClick)
	local normalAtlas = CharacterCreate_GetClassGuideAtlas(atlas)

	button:SetID(id)
	button.step = id
	button:Enable()
	button:SetChecked(false)
	button:SetIconNormal(normalAtlas)
	button:SetIconHighlighted(normalAtlas)
	button.Highlight:Hide()
	button.Checked:Hide()

	button:UpdateText(text, "")
	button:SetScript("OnClick", onClick)
	button:SetScript("OnEnter", function(self)
		self.NameFrame.Text:SetFontObject(self.highlightFontObject or GameFontHighlightOutlineLarge)
		if text then
			GlueTooltip:SetOwner(self, "ANCHOR_TOP")
			GlueTooltip:AddLine(text, 1, 1, 1, 1, true)
			if description and description ~= "" then
				GlueTooltip:AddLine(description, 1, 0.82, 0, 1, true)
			end
			GlueTooltip:Show()
		end
	end)
	button:SetScript("OnLeave", function(self)
		self.Highlight:Hide()
		self:SetIconNormal()
		self.NameFrame.Text:SetFontObject(self.normalFontObject or GameFontNormalOutlineLarge)
		GlueTooltip:Hide()
	end)
end

local function CharacterCreate_HideClassGuideButtons(buttons)
	for _, button in ipairs(buttons) do
		button:Hide()
	end
end

local function CharacterCreate_UpdateClassGuideFrame()
	if not CharacterCreate.classGuideActive or not CharCreateClassGuideFrame or not CharacterCreate_HasClassGuideData() then
		CharacterCreate_HideClassGuide()
		return
	end

	local roles = C_CharacterCreate.GetClassGuideRoles()
	CharCreateClassGuideFrame:Show()
	if CharCreateClassGuideFrame.BackButton then
		CharCreateClassGuideFrame.BackButton:Hide()
	end

	if CharacterCreate.selectedClassGuideSubroleID then
		CharacterCreate_SetClassGuideRaceLocked(false)
		CharacterCreate_HideClassGuideButtons(CharCreateClassGuideFrame.roleButtons)
		CharacterCreate_HideClassGuideButtons(CharCreateClassGuideFrame.subroleButtons)
		return
	end

	CharacterCreate_SetClassGuideRaceLocked(true)

	local roleCount = 0
	if not CharacterCreate.selectedClassGuideRoleID then
		for _, roleID in ipairs(roles) do
			local roleKey, atlas, name, description = C_CharacterCreate.GetClassGuideRoleInfo(roleID)
			if name then
				roleCount = roleCount + 1
				local button = CharCreateClassGuideFrame.roleButtons[roleCount]
				if not button then
					button = CharacterCreate_CreateClassGuideButton(CharCreateClassGuideFrame)
					CharCreateClassGuideFrame.roleButtons[roleCount] = button
				end

				CharacterCreate_SetClassGuideButton(button, roleID, name, CLASS_GUIDE_ROLE_ATLAS[roleKey] or atlas, description, function(self)
					CharacterCreate_SelectClassGuideRole(self:GetID())
				end)
			end
		end
	end

	for i = roleCount + 1, #CharCreateClassGuideFrame.roleButtons do
		CharCreateClassGuideFrame.roleButtons[i]:Hide()
	end

	if roleCount > 0 then
		CharacterCreate_LayoutClassGuideButtons(CharCreateClassGuideFrame.roleButtons, roleCount, CLASS_GUIDE_CHOICE_BUTTON_Y_OFFSET)
	end

	local subroleCount = 0
	if CharacterCreate.selectedClassGuideRoleID and C_CharacterCreate.GetClassGuideSubroles then
		local subroles = C_CharacterCreate.GetClassGuideSubroles(CharacterCreate.selectedClassGuideRoleID)
		for _, subroleID in ipairs(subroles) do
			local _, subroleKey, _, atlas, name, description = C_CharacterCreate.GetClassGuideSubroleInfo(subroleID)
			if name then
				subroleCount = subroleCount + 1
				local button = CharCreateClassGuideFrame.subroleButtons[subroleCount]
				if not button then
					button = CharacterCreate_CreateClassGuideButton(CharCreateClassGuideFrame)
					CharCreateClassGuideFrame.subroleButtons[subroleCount] = button
				end

				CharacterCreate_SetClassGuideButton(button, subroleID, name, CLASS_GUIDE_SUBROLE_ATLAS[subroleKey] or atlas, description, function(self)
					CharacterCreate_SelectClassGuideSubrole(self:GetID())
				end)
			end
		end
	end

	for i = subroleCount + 1, #CharCreateClassGuideFrame.subroleButtons do
		CharCreateClassGuideFrame.subroleButtons[i]:Hide()
	end

	if subroleCount > 0 then
		CharacterCreate_LayoutClassGuideButtons(CharCreateClassGuideFrame.subroleButtons, subroleCount, CLASS_GUIDE_CHOICE_BUTTON_Y_OFFSET)
	end
end

local function CharacterCreate_SetGuideClassButtonPoint(button, index, rowCount, yOffset)
	local width = 52
	local spacing = 36
	local totalWidth = rowCount * width + math.max(rowCount - 1, 0) * spacing
	local startX = -totalWidth / 2 + width / 2

	button:ClearAllPoints()
	button:SetPoint("TOP", CharCreateClassGuideFrame, "TOP", startX + (index - 1) * (width + spacing), yOffset)
end

local function CharacterCreate_UpdateClassGuideClassButtonSelection(self)
	if not self or not self.classButtonPool then
		return
	end

	for button in self.classButtonPool:EnumerateActive() do
		button:SetSelected(button:GetID() == CharacterCreate.selectedClassGuideClassID)
	end
end

local function CharacterCreate_GetClassGuideExplicitClassOrder(subroleID)
	local classOrder = {}
	if C_CharacterCreate.GetClassGuideSubroleClasses then
		local classes = C_CharacterCreate.GetClassGuideSubroleClasses(subroleID)
		if classes then
			for index, classID in ipairs(classes) do
				classOrder[classID] = index
			end
		end
	end

	return classOrder
end

local function CharacterCreate_ShowClassGuideClasses(self)
	if not CharacterCreate.selectedClassGuideSubroleID then
		return
	end

	local pool = self.classButtonPool
	local classes = {}
	local selectedClassIsAvailable = false
	local explicitClassOrder = CharacterCreate_GetClassGuideExplicitClassOrder(CharacterCreate.selectedClassGuideSubroleID)
	local isHealerSubrole = CharacterCreate_IsClassGuideHealerSubrole(CharacterCreate.selectedClassGuideSubroleID)

	for i = 1, 21 do
		local classID = Enum.Class[COA_CLASS_ORDER[i]]
		if classID
			and C_CharacterCreate.CanCreateClass(classID) then
			local className, classFile = GetClassInfo(classID)
			if className
				and classFile
				and CharacterCreate_ClassPassesGuideFilter(classID, classFile) then
				if CharacterCreateUtil.IsRaceClassEnabledAndValid(CharacterCreate.selectedRace, classID) then
					classes[#classes + 1] = {
						id = classID,
						name = className,
						file = classFile,
						order = explicitClassOrder[classID] or i,
						isCombatHealer = isHealerSubrole and CharacterCreate_IsClassGuideCombatHealer(classFile),
					}
					if classID == CharacterCreate.selectedClassGuideClassID then
						selectedClassIsAvailable = true
					end
				end
			end
		end
	end

	if CharacterCreate.selectedClassGuideClassID and not selectedClassIsAvailable then
		CharacterCreate.selectedClassGuideClassID = nil
	end

	CharCreate_EnableNextButton(CharacterCreate.selectedClassGuideClassID and true or false)
	table.sort(classes, function(left, right)
		if left.isCombatHealer ~= right.isCombatHealer then
			return not left.isCombatHealer
		end

		if left.order ~= right.order then
			return left.order < right.order
		end

		return left.id < right.id
	end)

	local firstRowCount = math.min(#classes, 10)
	local secondRowCount = math.max(#classes - firstRowCount, 0)
	for index, classInfo in ipairs(classes) do
		local button = pool:Acquire()
		button:SetID(classInfo.id)
		button:SetParent(CharCreateClassGuideFrame)
		button:SetClass(classInfo.name, classInfo.file, true)
		button:SetClassGuideCombatHealer(classInfo.isCombatHealer)
		button:SetSelected(classInfo.id == CharacterCreate.selectedClassGuideClassID)
		button:Show()

		if index <= firstRowCount then
			CharacterCreate_SetGuideClassButtonPoint(button, index, firstRowCount, -124)
		else
			CharacterCreate_SetGuideClassButtonPoint(button, index - firstRowCount, secondRowCount, -182)
		end
	end

	CharacterCreate_UpdateClassGuideClassButtonSelection(self)
end

local function CharacterCreate_IsCustomizationOptionLocked(customizationId)
	if not C_CharacterCustomizationUnlocks or not C_CharacterCustomizationUnlocks.GetCurrentOptionInfo then
		return false
	end

	local hasRequirement, isUnlocked = C_CharacterCustomizationUnlocks.GetCurrentOptionInfo(customizationId)
	return hasRequirement and not isUnlocked
end

local function HandleFactionBackground()
	local name, faction = GetFactionForRace(CharacterCreate.selectedRace);

	if not C_CharacterCreate.CanCreateCoA() then
		faction = faction.."REGULAR"
	end

	SetBackgroundModel(CharacterCreate, string.upper(faction));
	IS_ZOOMED = false
	--SetBackgroundModel(CharacterCreate, string.upper(faction));
end

local function SetFaceCustomizeCamera(flag)
	ZoomController.manualZoomIn = nil 
	ZoomController.manualZoomOut = nil
	if (flag) and not(IS_ZOOMED) then
		ZoomController:Show()
		IS_ZOOMED = true
	elseif not(flag) and (IS_ZOOMED) then
		ZoomController:Show()
		IS_ZOOMED = false
	end
end

local function CharacterCreate_UndressCustomizationHeadSlot()
	if CharacterCreateFrame.state ~= Enum.CharCreateSteps.Customization
		and not (CharacterCreate.classGuideActive and CharacterCreateFrame.state == Enum.CharCreateSteps.NameEdit) then
		return
	end

	if not CharCreateShowEquipmentToggle or not CharCreateShowEquipmentToggle:GetChecked() then
		return
	end

	if not CharacterCreate or type(CharacterCreate.UndressSlot) ~= "function" then
		return
	end

	pcall(CharacterCreate.UndressSlot, CharacterCreate, INVSLOT_HEAD)
end

function CharacterCreate_UpdateCustomizationEquipment()
	if CharacterCreateFrame.state ~= Enum.CharCreateSteps.Customization
		and not (CharacterCreate.classGuideActive and CharacterCreateFrame.state == Enum.CharCreateSteps.NameEdit) then
		return
	end

	if CharCreateShowEquipmentToggle and CharCreateShowEquipmentToggle:GetChecked() then
		C_CharacterCreate.Dress()
	else
		C_CharacterCreate.Undress()
	end

	CharacterCreate_UndressCustomizationHeadSlot()
end

function CharCreate_ZoomOutArchetypes()
	local x, y, z = C_CharacterCreate.GetCameraPosition()

	local shiftX, shiftY, shiftZ

	local _, fileString = GetNameForRace()
	local zoomData = CHAR_CREATE_CAMERA_ZOOM_OUTS[fileString]

	if HD_PATCH_ENABLED and fileString == "Human" then -- in HD patch don't change human camera
		zoomData = nil
	end

	if zoomData then
		C_CharacterCreate.SetCameraPosition(unpack(zoomData))
	end

	CHAR_CREATE_ZOOMED_OUT = true
end

--
-- Navigation logic
--
function CharacterCreate_LoadStep()
	CharCreateRoleSelectFrame:Hide()
	CharCreateSubroleSelectFrame:Hide()
	CharCreateArchetypeSelectFrame:Hide()

	CharCreateArchetypeFrame:Hide()
	CharCreateCustomizationFrame:Hide();
	CharCreatePreviewFrame:Hide();
	CharacterCreateNameEdit:Hide();
	CharacterCreateRandomName:Hide();
	CustomizationBG:Hide()
	CharCreateRandomizeButton:Hide()
	CharCreateClassFrame:Hide();
	CharCreateRaceFrame:Hide();
	CharCreateSwapClassesButton:Hide()
	CharacterTemplateConfirmDialog:Hide();
	CharCreateMaleButton:Hide()
	CharCreateFemaleButton:Hide()
	CustomizationLogoAlliance:Hide()
	CustomizationTextAlliance:Hide()
	CustomizationLogoHorde:Hide()
	CustomizationTextHorde:Hide()
	CharCreateDescriptionFrame:Hide()
	CharacterCreateNavigationFrame:Hide()
	CharCreateOkayButton:SetText(CONTINUE)
	CharCreateOkayButton:Enable()

	CharacterCreateFrame.StepName:Show()
	CharacterCreateFrame.StepSubText:Show()
	
	CharCreateMovieFrame:StopMovie()
	if CharCreateMovieFrame:IsShown() then
		CharCreateMovieFrame:Hide()
	end

	CharCreateDescriptionFrame:ClearAndSetPoint("TOPRIGHT", -32, -64)
	CharCreateDescriptionFrame:SetHeight(545)
	C_CharacterCreate.Dress()
	
	CharCreate_ResetCamera()
	local _, classFileName = GetSelectedClass()
	local isHero = classFileName == "HERO"

	if CharacterCreateFrame.state == Enum.CharCreateSteps.ClassRace then
		CharacterCreateFrame.StepName:Hide()
		CharacterCreateFrame.StepSubText:Hide()

		CharCreateClassFrame:Show();
		CharCreateRaceFrame:Show();
		CharCreateMaleButton:SetShown(not CharacterCreate_IsFemaleOnlyRace())
		CharCreateFemaleButton:SetShown(not CharacterCreate_IsMaleOnlyRace())
		CharCreateOkayButton:SetText(CUSTOMIZE);
		CustomizationLogoAlliance:Show()
		CustomizationTextAlliance:Show()
		CustomizationLogoHorde:Show()
		CustomizationTextHorde:Show()
		if CharacterCreate.classGuideActive then
			CharCreateOkayButton:SetText(CONTINUE)
			CharacterCreate_RefreshClassButtons(CharacterCreate)
			CharCreate_EnableNextButton(CharacterCreate.selectedClassGuideClassID and true or false)
			CharacterCreate_SetClassGuideRaceLocked(not CharacterCreate.selectedClassGuideSubroleID)
		else
			CharacterCreate_RefreshClassButtons(CharacterCreate)
			CharCreate_EnableNextButton(true)
		end
		if isHero then
			C_CharacterCreate.ClearClassPreview()
			C_CharacterCreate.SetSelectedArchetype(0, 0, 0)
		else
			C_CharacterCreate.PreviewClassOutfit(classFileName)
		end
	elseif ( CharacterCreateFrame.state == Enum.CharCreateSteps.Customization ) then
		CharacterCreateFrame.StepName:SetText(CHAR_CREATE_CUSTOMIZE_YOUR_APPEARANCE)

		CharacterCreateNavigationFrame:Show()
		CharCreateCustomizationFrame:Show();
		CharacterCreate_UpdateHairCustomization()
		CharCreate_PrepPreviewModels();
		if ( CharacterCreateFrame.customizationType ) then
			CharCreate_ResetFeaturesDisplay();
		else
			CharCreateSelectCustomizationType(1);
		end

		if not CharacterCreateNavigationFrame:GetButtonByIndex(Enum.CharCreateSteps.NameEdit) then
			CharacterCreateNameEdit:Show()
			if ( ALLOW_RANDOM_NAME_BUTTON ) then
				CharacterCreateRandomName:Show();
			end
		end
		
		CustomizationBG:Show()
		CharCreateRandomizeButton:Show()
		CharCreate_EnableNextButton(true)
		CHAR_CREATE_ZOOMED_OUT = false

		C_CharacterCreate.SetSelectedArchetype(0, 0, 0)
		CharacterCreate_UpdateCustomizationEquipment()
	elseif ( CharacterCreateFrame.state == Enum.CharCreateSteps.Role ) then
		if CharacterCreate.classGuideActive then
			CharacterCreateFrame.StepName:SetText(CLASS_GUIDE_ROLE_SELECT_TEXT)

			CharacterCreateNavigationFrame:Show()
			CharCreateClassFrame:Show()
			CharCreateClassContainer:Hide()
			CharacterCreate_SetClassGuideEntryShown(false)
			CharacterCreate_UpdateClassGuideFrame()
			CharCreateOkayButton:Disable()
		else
			CharacterCreateFrame.StepName:SetText(ROLE_SELECT_TEXT)

			CharacterCreateNavigationFrame:Show()
			CharCreateRoleSelectFrame:Show()

			local roleID = CharCreateRoleSelectFrame:GetCurrentIndex()

			if not roleID then
				CharCreateDescriptionFrame:Hide()
				CharCreateOkayButton:Disable()
			else
				C_CharacterCreate.SetSelectedArchetype(roleID, 0, 0)
				CharCreateDescriptionFrame:SetRole(roleID)
				CharCreateDescriptionFrame:Show()
				CharCreate_EnableNextButton(true)
			end
		end

		CharCreate_ZoomOutArchetypes()
	elseif ( CharacterCreateFrame.state == Enum.CharCreateSteps.Subrole ) then
		if CharacterCreate.classGuideActive then
			CharacterCreateFrame.StepName:SetText(CLASS_GUIDE_CATEGORY_SELECT_TEXT)

			CharacterCreateNavigationFrame:Show()
			CharCreateClassFrame:Show()
			CharCreateClassContainer:Hide()
			CharacterCreate_SetClassGuideEntryShown(false)
			CharacterCreate_UpdateClassGuideFrame()
			CharCreateOkayButton:Disable()
		else
			CharacterCreateFrame.StepName:SetText(SUBROLE_SELECT_TEXT)

			CharacterCreateNavigationFrame:Show()
			CharCreateSubroleSelectFrame:Show()

			local categoryID = CharCreateSubroleSelectFrame:GetCurrentIndex()
			if not categoryID then
				CharCreateDescriptionFrame:Hide()
				CharCreateOkayButton:Disable()
			else
				CharCreateDescriptionFrame:SetCategory(categoryID)
				CharCreateDescriptionFrame:Show()
				CharCreate_EnableNextButton(true)

				local roleID = CharCreateRoleSelectFrame:GetCurrentIndex()
				C_CharacterCreate.SetSelectedArchetype(roleID, categoryID, 0)
			end
		end

		CharCreate_ZoomOutArchetypes()
	elseif ( CharacterCreateFrame.state == Enum.CharCreateSteps.Archetype ) then
		if CharacterCreate.classGuideActive then
			CharacterCreateFrame.StepName:SetText(CLASS_GUIDE_CLASS_SELECT_TEXT)

			CharacterCreateNavigationFrame:Show()
			CharCreateClassFrame:Show()
			CharCreateClassContainer:Hide()
			CharacterCreate_SetClassGuideRaceLocked(false)
			CharacterCreate_SetClassGuideEntryShown(false)
			CharacterCreate_UpdateClassGuideFrame()
			CharacterCreate.classButtonPool:ReleaseAll()
			CharacterCreate_ShowClassGuideClasses(CharacterCreate)
			CharCreate_EnableNextButton(CharacterCreate.selectedClassGuideClassID and true or false)
		else
			CharacterCreateFrame.StepName:SetText(ARCHETYPE_SELECT_TEXT)

			CharacterCreateNavigationFrame:Show()
			CharCreateArchetypeSelectFrame:Show()

			local archetypeID = CharCreateArchetypeSelectFrame:GetCurrentIndex()

			if not archetypeID then
				CharCreateDescriptionFrame:Hide()
				CharCreateOkayButton:Disable()
			else
				CharacterCreate_OnSelectArchetype(nil, archetypeID)
				CharCreate_EnableNextButton(true)
			end
		end

		CharCreate_ZoomOutArchetypes()
	elseif CharacterCreateFrame.state == Enum.CharCreateSteps.NameEdit then
		CharacterCreateNavigationFrame:Show()
		CharacterCreateNameEdit:Show()
		if ( ALLOW_RANDOM_NAME_BUTTON ) then
			CharacterCreateRandomName:Show();
		end

		if CharacterCreate.classGuideActive then
			CharCreateCustomizationFrame:Show()
			CharacterCreate_UpdateHairCustomization()
			CharCreate_PrepPreviewModels();
			if ( CharacterCreateFrame.customizationType ) then
				CharCreate_ResetFeaturesDisplay();
			else
				CharCreateSelectCustomizationType(1);
			end

			CustomizationBG:Show()
			CharCreateRandomizeButton:Show()
			CharCreate_EnableNextButton(true)
			CHAR_CREATE_ZOOMED_OUT = false
			CharacterCreate_UpdateCustomizationEquipment()
		else
			if not isHero then
				C_CharacterCreate.PreviewClassOutfit(classFileName)
			else
				C_CharacterCreate.ClearClassPreview()
			end
			CharCreate_ZoomOutArchetypes()
		end
	end

	CharacterCreate_UndressCustomizationHeadSlot()

	if not CharacterCreateNavigationFrame:GetNextIndex() then
		CharCreateOkayButton:SetText(FINISH)
	end
end

function CharCreate_CheckExtraSteps()
	if CharacterCreate.classGuideActive then
		return
	end

	local rolesButton = CharacterCreateNavigationFrame:GetButtonByIndex(Enum.CharCreateSteps.Role)
	local subRolesButton = CharacterCreateNavigationFrame:GetButtonByIndex(Enum.CharCreateSteps.Subrole)
	local archeTypeButton = CharacterCreateNavigationFrame:GetButtonByIndex(Enum.CharCreateSteps.Archetype)
	local nameButton = CharacterCreateNavigationFrame:GetButtonByIndex(Enum.CharCreateSteps.NameEdit)

	if rolesButton and not(CharCreateRoleSelectFrame:GetCurrentIndex()) then
		rolesButton:SetText(ROLE_SELECT_TEXT)

		if subRolesButton then
			subRolesButton:Disable()
		end

		rolesButton.IconAdd:SetAtlas("ExperienceIconBeginner", Const.TextureKit.IgnoreAtlasSize)
	end

	if subRolesButton and not(CharCreateSubroleSelectFrame:GetCurrentIndex()) then
		subRolesButton:SetText(SUBROLE_SELECT_TEXT)

		if archeTypeButton then
			archeTypeButton:Disable()
		end

		subRolesButton.IconAdd:SetAtlas("ExperienceIconBeginner", Const.TextureKit.IgnoreAtlasSize)
	end

	if archeTypeButton and not(CharCreateArchetypeSelectFrame:GetCurrentIndex()) then
		archeTypeButton:SetText(ARCHETYPE_SELECT_TEXT)

		if nameButton then
			nameButton:Disable()
		end

		archeTypeButton:SetUp()
	end
end

function CharacterCreate_UpdateNavigtationRaceButton()
	local button = _G["CharCreateRaceButton"..CharacterCreate.selectedRace]

	if button then
		local raceButton = CharacterCreateNavigationFrame:GetButtonByIndex(Enum.CharCreateSteps.ClassRace)
		if not raceButton then
			return
		end

		raceButton.IconAdd:Hide()

		raceButton:SetText(string.format(RACE_SELECTED_TEXT, button.Text:GetText() or ""))

		local texture = button:GetNormalTexture():GetTexture()
		local ULx,ULy,LLx,LLy,URx,URy,LRx,LRy = button:GetNormalTexture():GetTexCoord()

		raceButton.Icon:SetTexture(texture)
		raceButton.Icon:SetTexCoord(ULx,ULy,LLx,LLy,URx,URy,LRx,LRy)
	end
end

function CharacterCreate_LoadNavigation(steps)
	CharacterCreateNavigationFrame:ClearAll()

	for i = 1, #steps do
		CharacterCreateNavigationFrame:AddIndex(steps[i])
	end

	CharacterCreate_UpdateNavigtationRaceButton()

	local customizationButton = CharacterCreateNavigationFrame:GetButtonByIndex(Enum.CharCreateSteps.Customization)
	if customizationButton then
		customizationButton.IconAdd:SetSize(56, 56)
		customizationButton.IconAdd:SetTexture("interface\\glues\\charactercreate\\CharacterCreate")
		customizationButton.IconAdd:SetTexCoord(0.29150390625, 0.357421875, 0.7880859375, 0.919921875)
		customizationButton:SetText(CUSTOMIZE)
	end

	local nameEditButton = CharacterCreateNavigationFrame:GetButtonByIndex(Enum.CharCreateSteps.NameEdit)

	if nameEditButton then
		nameEditButton.IconAdd:SetSize(56, 56)
		nameEditButton.IconAdd:SetTexture("interface\\glues\\charactercreate\\CharacterCreate")
		nameEditButton.IconAdd:SetTexCoord(0.42431640625, 0.47314453125, 0.8408203125, 0.94140625)
		nameEditButton:SetText(CHARACTER_NAME)
	end

	CharCreateRoleSelectFrame:ClearSelected()
	CharCreateSubroleSelectFrame:ClearAll()
	CharCreateArchetypeSelectFrame:ClearAll()

	CharCreate_CheckExtraSteps()
end

function CharacterCreate_OnStepChanged(_, step, isForced)
	print("CharacterCreate_OnStepChanged "..step)
	if CharacterCreateFrame.state == Enum.CharCreateSteps.Archetype then
		CharCreateMovieFrame:StopMovie()
		if CharCreateMovieFrame:IsShown() then
			CharCreateMovieFrame:Hide()
		end
	end

	CharacterCreateFrame.state = step

	if (step == Enum.CharCreateSteps.NameEdit) or isForced then -- don't flash light for name edit
		CharacterCreate_LoadStep()
		return
	end

	GlueFrameFadeIn(CharacterCreateFrameFade, 0.3, function()
		if step == Enum.CharCreateSteps.ClassRace then
			HandleFactionBackground()
		else
			local race, fileString = GetNameForRace()
			-- Grummle has no dedicated glue-background entry. Passing its race
			-- name makes GlueParent send a numeric fallback to PlayGlueAmbience.
			SetBackgroundModel(CharacterCreate, (race == "High Elf" and "NIGHTELF") or (fileString == "Grummle" and "ALLIANCE" or string.upper(fileString)))
		end

		GlueFrameFadeOut(CharacterCreateFrameFade, 0.25)
		CharacterCreate_LoadStep()
	end)
end

function CharacterCreate_OnArchetypeBaseChanged(_, archetypeChoice)
	if archetypeChoice == Enum.CharCreateArchetypeBases.Freepick then
		CharacterCreate_LoadNavigation(CHAR_CREATE_STEPS_DEFAULT)
	elseif archetypeChoice == Enum.CharCreateArchetypeBases.UseArchetype then
		CharacterCreate_LoadNavigation(CHAR_CREATE_STEPS_EXTEND)
	end

	if not CharacterCreateNavigationFrame:GetCurrentIndex() or (CharacterCreateNavigationFrame:GetCurrentIndex() ~= CHAR_CREATE_STEPS_DEFAULT[1]) then
		CharacterCreateNavigationFrame:GoToIndex(CHAR_CREATE_STEPS_DEFAULT[1], true)
	end
end

function CharacterCreate_OnSelectRole(self, roleID)
	if not roleID then
		return
	end

	-- clear subrole/architect 
	CharCreateSubroleSelectFrame:ClearSelected()
	CharCreateArchetypeSelectFrame:ClearSelected()
	CharacterCreateNavigationFrame:GetButtonByIndex(Enum.CharCreateSteps.Archetype):Disable()

	CharCreateSubroleSelectFrame:LoadSubroles(roleID)
	CharacterCreateNavigationFrame:GetButtonByIndex(Enum.CharCreateSteps.Subrole):Enable()

	if (CharCreateOkayButton:IsEnabled() == 0) then
		CharCreate_EnableNextButton(true)
	end

	local artwork, name = C_CharacterCreate.GetArchetypeRoleInfo(roleID)
	CharacterCreateNavigationFrame:GetButtonByIndex(Enum.CharCreateSteps.Role).IconAdd:SetAtlas(artwork)
	CharacterCreateNavigationFrame:GetButtonByIndex(Enum.CharCreateSteps.Role):SetText(string.format(ROLE_SELECTED_TEXT, name))

	CharCreateDescriptionFrame:SetRole(roleID)
	CharCreateDescriptionFrame:Show()

	CharCreate_CheckExtraSteps()

	C_CharacterCreate.SetSelectedArchetype(roleID, 0, 0)
end

function CharacterCreate_OnSelectCategory(self, categoryID)
	CharCreateArchetypeSelectFrame:ClearSelected()

	CharCreateArchetypeSelectFrame:LoadArchetypes(categoryID)
	CharacterCreateNavigationFrame:GetButtonByIndex(Enum.CharCreateSteps.Archetype):Enable()

	if (CharCreateOkayButton:IsEnabled() == 0) then
		CharCreate_EnableNextButton(true)
	end

	local iconArtwork, _, name = C_CharacterCreate.GetArchetypeCategoryInfo(categoryID)
	CharacterCreateNavigationFrame:GetButtonByIndex(Enum.CharCreateSteps.Subrole).IconAdd:SetAtlas(iconArtwork)
	CharacterCreateNavigationFrame:GetButtonByIndex(Enum.CharCreateSteps.Subrole):SetText(string.format(SUBROLE_SELECTED_TEXT, name))

	CharCreateDescriptionFrame:SetCategory(categoryID)
	CharCreateDescriptionFrame:Show()

	CharCreate_CheckExtraSteps()

	C_CharacterCreate.SetSelectedArchetype(CharCreateRoleSelectFrame:GetCurrentIndex(), categoryID, 0)
end

function CharacterCreate_OnSelectArchetype(_, archetypeID)
	CharacterCreateNavigationFrame:GetButtonByIndex(Enum.CharCreateSteps.NameEdit):Enable()

	if (CharCreateOkayButton:IsEnabled() == 0) then
		CharCreate_EnableNextButton(true)
	end

	local _, _, _, _, _, icon, _, _, _, name = C_CharacterCreate.GetArchetypeInfo(archetypeID)
	local archetypeButton = CharacterCreateNavigationFrame:GetButtonByIndex(Enum.CharCreateSteps.Archetype)

	if name and archetypeButton then
		archetypeButton.IconAdd:Hide()
		archetypeButton:SetText(string.format(ARCHETYPE_SELECTED_TEXT, name))
		archetypeButton.Icon:SetPortraitTexture("Interface\\Icons\\"..icon)
		archetypeButton.Icon:SetTexCoord(0, 1, 0, 1)
	end

	CharCreateDescriptionFrame:ClearAndSetPoint("TOP", CharCreateMovieFrame, "BOTTOM", 0, -16)
	CharCreateDescriptionFrame:SetHeight(361)
	CharCreateDescriptionFrame:SetArchetype(archetypeID)
	CharCreateDescriptionFrame:Show()

	-- @andrew Disabled because the videos just crash the game for a bunch of people.
	--[[CharCreateMovieFrame:Show()
	CharCreateMovieFrame:StartMovieForArchetype(archetypeID)]]

	C_CharacterCreate.SetSelectedArchetype(CharCreateRoleSelectFrame:GetCurrentIndex(), CharCreateSubroleSelectFrame:GetCurrentIndex(), archetypeID)
end

function CharacterCreate_OnLoad(self)
	self.classButtonPool = CreateFramePool("CheckButton", CharCreateClassFrame, "CharCreateClassButtonTemplate")
	CharacterCreate_CreateClassGuideFrame()
	self:RegisterEvent("RANDOM_CHARACTER_NAME_RESULT");
	self:RegisterEvent("GLUE_UPDATE_EXPANSION_LEVEL");

	self:SetSequence(0);
	self:SetCamera(0);

	CharacterCreate.numRaces = 0;
	CharacterCreate.selectedRace = 0;
	CharacterCreate.numClasses = 0;
	CharacterCreate.selectedClass = 0;
	CharacterCreate.selectedGender = 0;

	SetCharCustomizeFrame("CharacterCreate");

	for i=1, NUM_CHAR_CUSTOMIZATIONS, 1 do
		_G["CharCreateCustomizationButton"..i].text:SetText(_G["CHAR_CUSTOMIZATION"..i.."_DESC"]);
	end

	CharacterCreateRandomName:SetParent(CharacterCreateNameEdit)

	-- Color edit box backdrop
	local backdropColor = FACTION_BACKDROP_COLOR_TABLE["Alliance"];
	CharacterCreateNameEdit:SetBackdropBorderColor(backdropColor[1], backdropColor[2], backdropColor[3]);
	CoACharacterNames.InitCreation()
	CharacterCreateNameEdit:SetBackdropColor(backdropColor[4], backdropColor[5], backdropColor[6]);
	CharacterCreateSecondNameEdit:SetBackdropColor(backdropColor[4], backdropColor[5], backdropColor[6]);
	--[[CharacterCreateNameEdit:SetParent(CharacterCreateFrame)
	CharacterCreateNameEdit:SetPoint("TOPLEFT", CharacterCreateFrame, 635, -30)]]--
	CharCreateCustomizationFrame:SetPoint("RIGHT", CharacterCreateFrame, -50, -10)

	-- load navigation
	CharacterCreateNavigationFrame:RegisterCallback("OnIndexSelected", CharacterCreate_OnStepChanged, self)
	CharacterCreateNavigationFrame:Hide()
	CharCreateArchetypeFrame:RegisterCallback("OnIndexSelected", CharacterCreate_OnArchetypeBaseChanged, self)

	-- set up extra menus
	CharCreateRoleSelectFrame:RegisterCallback("OnIndexSelected", CharacterCreate_OnSelectRole, self)
	CharCreateSubroleSelectFrame:RegisterCallback("OnIndexSelected", CharacterCreate_OnSelectCategory, self)
	CharCreateArchetypeSelectFrame:RegisterCallback("OnIndexSelected", CharacterCreate_OnSelectArchetype, self)

	CharCreatePreviewFrame.previews = { };

	-- custom Lua
	_G["CharCreateMaleButtonNormalTexture"]:SetDrawLayer("BACKGROUND")
	_G["CharCreateFemaleButtonNormalTexture"]:SetDrawLayer("BACKGROUND")
	_G["CharCreateMaleButtonPushedTexture"]:SetDrawLayer("BACKGROUND")
	_G["CharCreateFemaleButtonPushedTexture"]:SetDrawLayer("BACKGROUND")
	_G["CharCreateRandomizeButtonNormalTexture"]:SetDrawLayer("BACKGROUND")
	_G["CharacterCreateRandomNameNormalTexture"]:SetDrawLayer("BACKGROUND")
	_G["CharCreateRandomizeButtonPushedTexture"]:SetDrawLayer("BACKGROUND")
	_G["CharacterCreateRandomNamePushedTexture"]:SetDrawLayer("BACKGROUND")
	--CharCreateMaleButton:Hide()
	--CharCreateFemaleButton:Hide()

	CustomizationBG = CharacterCreateFrame:CreateTexture("CustomizationBG", "BACKGROUND")
	CustomizationBG:SetSize(512, GlueParent:GetHeight())
    CustomizationBG:SetTexture("Interface\\Glues\\CharacterCreate\\Shadowv")
    CustomizationBG:SetPoint("RIGHT")
    CustomizationBG:Hide()

    CharacterCreateFrameFade = CreateFrame("FRAME", "CharacterCreateFrameFade", CharacterCreateFrame)
    CharacterCreateFrameFade:SetPoint("CENTER", 0, 0)
    CharacterCreateFrameFade:SetSize(GlueParent:GetWidth(), GlueParent:GetHeight())
    CharacterCreateFrameFade:SetFrameStrata("DIALOG")
    CharacterCreateFrameFade:SetFrameLevel(10)
    --CharacterCreateFrameFade:Hide()

    --[[CharacterCreateTopNavi = CreateFrame("FRAME", "CharacterCreateTopNavi", CharacterCreateFrame)
    CharacterCreateTopNavi:SetPoint("BOTTOM", 0, 4)
    CharacterCreateTopNavi:SetSize(GlueParent:GetWidth(), 128)

    CharCreateBackButton:Hide()
	CharCreateOkayButton:Hide()

	CharCreateBackButtonTopNavi = CreateFrame("Button", "CharCreateBackButtonTopNavi", CharacterCreateTopNavi, "CharCreateNavButtonTemplate")
	CharCreateBackButtonTopNavi:SetPoint("BOTTOMLEFT", 54, -6)
	CharCreateBackButtonTopNavi:SetText(BACK)
	CharCreateBackButtonTopNavi:SetScript("OnClick", function(self)
		CharCreateBackButton:GetScript("OnClick")(CharCreateBackButton)
	end)

	CharCreateOkayButtonTopNavi = CreateFrame("Button", "CharCreateOkayButtonTopNavi", CharacterCreateTopNavi, "CharCreateNavButtonTemplate")
	CharCreateOkayButtonTopNavi:SetPoint("BOTTOMRIGHT", -52, -6)
	CharCreateOkayButtonTopNavi:SetText("Customize")
	CharCreateOkayButtonTopNavi:SetScript("OnClick", function(self)
		CharCreateOkayButton:GetScript("OnClick")(CharCreateOkayButton)
	end)

	CharacterCreateTopNaviBG = CharacterCreateTopNavi:CreateTexture("CharacterCreateTopNaviBG", "BACKGROUND")
	CharacterCreateTopNaviBG:SetSize(1360, 85)
    CharacterCreateTopNaviBG:SetTexture("Interface\\Glues\\CharacterCreate\\TopNavi")
    CharacterCreateTopNaviBG:SetPoint("BOTTOM")
    CharacterCreateTopNaviBG:SetTexCoord(1,0,1,0)

	CharacterCreateTopNavi.Text = CharacterCreateTopNavi:CreateFontString("CharacterCreateTopNavi.Text", "OVERLAY")
    CharacterCreateTopNavi.Text:SetFontObject(GlueFontNormal)
    CharacterCreateTopNavi.Text:SetFont("Fonts\\FRIZQT__.TTF", 16)
    CharacterCreateTopNavi.Text:SetText("CHARACTER CREATION - |cffFFFFFFSELECT RACE/CLASS|r")
    CharacterCreateTopNavi.Text:SetPoint("BOTTOM", 0, 8)]]--

    --CharacterCreateTopNavi:SetFrameStrata("DIALOG")
    --CharacterCreateTopNavi:SetFrameLevel(10)

	CustomizationBG2 = CharacterCreateFrame:CreateTexture("CustomizationBG2", "BACKGROUND")
	CustomizationBG2:SetSize(GlueParent:GetWidth(), GlueParent:GetHeight())
    CustomizationBG2:SetTexture("Interface\\Glues\\CharacterCreate\\MainShadow")
    CustomizationBG2:SetPoint("CENTER")
    CustomizationBG2:SetAlpha(1)

    CustomizationBGAddRight = CharacterCreateFrame:CreateTexture("CustomizationBGAddRight", "BACKGROUND")
	CustomizationBGAddRight:SetSize(300, GlueParent:GetHeight())
    CustomizationBGAddRight:SetTexture("Interface\\Glues\\CharacterCreate\\Shadowv")
    CustomizationBGAddRight:SetPoint("RIGHT")
    CustomizationBGAddRight:SetAlpha(1)

    CustomizationBGAddRight = CharacterCreateFrame:CreateTexture("CustomizationBGAddRight2", "BACKGROUND")
	CustomizationBGAddRight:SetSize(300, GlueParent:GetHeight())
    CustomizationBGAddRight:SetTexture("Interface\\Glues\\CharacterCreate\\Shadowv")
    CustomizationBGAddRight:SetPoint("RIGHT")
    CustomizationBGAddRight:SetAlpha(1)

    CustomizationBGAddLeft = CharacterCreateFrame:CreateTexture("CustomizationBGAddLeft", "BACKGROUND")
	CustomizationBGAddLeft:SetSize(300, GlueParent:GetHeight())
    CustomizationBGAddLeft:SetTexture("Interface\\Glues\\CharacterCreate\\Shadowv")
    CustomizationBGAddLeft:SetPoint("LEFT")
    CustomizationBGAddLeft:SetTexCoord(1, 0, 1, 0)
    CustomizationBGAddLeft:SetAlpha(1)

	CharacterCreateFrameFadeBG = CharacterCreateFrameFade:CreateTexture("CharacterCreateFrameFadeBG", "OVERLAY")
	CharacterCreateFrameFadeBG:SetSize(GlueParent:GetWidth(), GlueParent:GetHeight())
    CharacterCreateFrameFadeBG:SetTexture("Interface\\Glues\\CharacterCreate\\Black")
    CharacterCreateFrameFadeBG:SetPoint("CENTER")

	CustomizationLogoAlliance = CharacterCreateFrame:CreateTexture("CustomizationLogoAlliance", "ARTWORK")
	CustomizationLogoAlliance:SetSize(100, 100)
    CustomizationLogoAlliance:SetTexture("Interface\\Glues\\CharacterCreate\\AllianceLogo")
    CustomizationLogoAlliance:SetPoint("TOPLEFT", -16, 16)

	CustomizationTextAlliance = CharacterCreateFrame:CreateFontString("CustomizationTextAlliance", "OVERLAY")
    CustomizationTextAlliance:SetFontObject(GlueFontNormal)
    CustomizationTextAlliance:SetFont("Fonts\\FRIZQT__.TTF", 16)
    CustomizationTextAlliance:SetText(string.upper(ALLIANCE))
    CustomizationTextAlliance:SetPoint("LEFT", CustomizationLogoAlliance, "RIGHT", -24, 0)

	CustomizationLogoHorde = CharacterCreateFrame:CreateTexture("CustomizationLogoHorde", "ARTWORK")
	CustomizationLogoHorde:SetSize(100, 100)
    CustomizationLogoHorde:SetTexture("Interface\\Glues\\CharacterCreate\\HordeLogo")
    CustomizationLogoHorde:SetPoint("TOPRIGHT", 16, 16)

	CustomizationTextHorde = CharacterCreateFrame:CreateFontString("CustomizationTextHorde", "OVERLAY")
    CustomizationTextHorde:SetFontObject(GlueFontNormal)
    CustomizationTextHorde:SetFont("Fonts\\FRIZQT__.TTF", 16)
    CustomizationTextHorde:SetText(string.upper(HORDE))
    CustomizationTextHorde:SetPoint("RIGHT", CustomizationLogoHorde, "LEFT", 24, 0)

	CharCreateMaleButton.nameFrame.text:SetText(MALE)
	CharCreateMaleButton.nameFrame:Show()

	CharCreateFemaleButton.nameFrame.text:SetText(FEMALE)
	CharCreateFemaleButton.nameFrame:Show()

	--[[CustomizationLogoClass = CharacterCreateFrame:CreateTexture("CustomizationLogoClass", "ARTWORK")
	CustomizationLogoClass:SetSize(100, 100)
    --CustomizationLogoClass:SetTexture("Interface\\Glues\\CharacterCreate\\ClassLogo")
    CustomizationLogoClass:SetPoint("TOP", -16, 16)

	CustomizationTextClass = CharacterCreateFrame:CreateFontString("CustomizationTextClass", "OVERLAY")
    CustomizationTextClass:SetFontObject(GlueFontHighlight)
    CustomizationTextClass:SetFont("Fonts\\FRIZQT__.TTF", 16)
    CustomizationTextClass:SetText(string.upper("class"))
    CustomizationTextClass:SetPoint("CENTER", CustomizationLogoClass, "RIGHT", -24, 0)]]--

	CustomizationLineBottom = CharacterCreateFrame:CreateTexture("CustomizationLineBottom", "ARTWORK")
	CustomizationLineBottom:SetSize(GlueParent:GetWidth(), 96)
    CustomizationLineBottom:SetTexture("Interface\\Glues\\CharacterCreate\\bottomline")
    CustomizationLineBottom:SetPoint("BOTTOM", 0, 0)

	CustomizationLineTop = CharacterCreateFrame:CreateTexture("CustomizationLineTop", "ARTWORK")
	CustomizationLineTop:SetSize(GlueParent:GetWidth(), 96)
    CustomizationLineTop:SetTexture("Interface\\Glues\\CharacterCreate\\bottomline")
    CustomizationLineTop:SetPoint("TOP", 0, 0)
    CustomizationLineTop:SetTexCoord(1,0,1,0)

    --CharacterCreateNameEdit:SetScript("OnTextChanged", CharCreationNameCheck)
    --GlueFrameFadeOut(CharacterCreateFrameFade, 0.5)
end

function CharCustomizeButtonClick(id, button)
	if (button == 'LeftButton') then
		for i = 1, math.random(1, 5) do
			CharacterCustomization_Left(id)
		end
	elseif (button == 'RightButton') then
		for i = 1, math.random(1, 5) do
			CharacterCustomization_Right(id)
		end
	end

	if (id > 1) then
		SetFaceCustomizeCamera(true)
	else
		SetFaceCustomizeCamera(false)
	end

	-- CycleCharCustomization(id, 1);
	--[[FeatureType = id
	for i=1,5 do
		_G["CharCreateCustomizationButton"..i]:SetChecked(0);
	end
	_G["CharCreateCustomizationButton"..id]:SetChecked(1);]]

end

function CharacterCreate_RefreshClassButtons(self)
	CharacterCreate_EnableVanillaClasses()
	local isCoA = C_CharacterCreate.CanCreateCoA()
	local isClassicClass = C_CharacterCreate.CanCreateWCR()
	local canCreateHero = C_CharacterCreate.CanCreateHero()

	CharCreateSwapClassesButton:SetShown(isCoA and isClassicClass)

	if isCoA and isClassicClass then
		if CharacterCreate.classesVisible == "REGULAR" then
			CharacterCreate_ShowRegularClasses(self)
		else
			CharacterCreate_ShowCoAClasses(self)
		end
	elseif isCoA then
		CharacterCreate_ShowCoAClasses(self)
	elseif isClassicClass then
		CharacterCreate_ShowRegularClasses(self)
	elseif canCreateHero then
		CharacterCreate_ShowFreepickClasses(self)
	end
end

function RefreshSelectedClass()
	local classID = select(3, GetSelectedClass())
	if not CharacterCreateUtil.IsRaceClassEnabledAndValid(CharacterCreate.selectedRace, classID) then
		classID = CharacterCreateUtil.GetRandomAvailableClassForRace(CharacterCreate.selectedRace)
		SetSelectedClass(classID)
		SetCharacterClass(classID)
	end
    
	--CharacterCreate_RefreshClassButtons(CharacterCreate) -- TODO: Review, causes crash?
end

function CharacterCreate_OnShow()
	NewCharacterSetupUtil.ClearPendingData()
	CharacterCreate_EnsureRaceButtons()
	for i=1, MAX_RACES, 1 do
		local button = _G["CharCreateRaceButton"..i]
		if button then
			button:EnableMouse(true)
			button:Enable()
			SetButtonDesaturated(button, false)
		end
	end

	if ( PAID_SERVICE_TYPE ) then
		CustomizeExistingCharacter( PAID_SERVICE_CHARACTER_ID );
		CoACharacterNames.SetCreationName(PaidChange_GetName());
		CharCreateArchetypeFrame:GoToIndex(Enum.CharCreateArchetypeBases.Freepick)
	else
		NewCharacterSetupUtil.SetPendingKeyValue("enableTransmog", true)
        NewCharacterSetupUtil.SetPendingKeyValue("enableLevelScaling", true)
		--randomly selects a combination
		ResetCharCustomize();
		CoACharacterNames.SetCreationName("");
		CharCreateRandomizeButton:Show();
	end

	CharCreateShowEquipmentToggle:SetChecked(true)

	if not CharCreateArchetypeFrame:GetCurrentIndex() or (CharCreateArchetypeFrame:GetCurrentIndex() ~= Enum.CharCreateArchetypeBases.Freepick) then
		CharCreateArchetypeFrame:GoToIndex(Enum.CharCreateArchetypeBases.Freepick)
	end

	CharacterCreateEnumerateRaces(GetAvailableRaces());
	SetCharacterRace(GetSelectedRace());
	HandleFactionBackground()
	GlueFrameFadeOut(CharacterCreateFrameFade, 0.5)

	CharacterCreate_RefreshClassButtons(CharacterCreate)

	if C_CharacterCreate.CanCreateHero() then
		SetSelectedClass(HERO_CLASS_ID)
		SetCharacterClass(HERO_CLASS_ID)

		if not PAID_SERVICE_TYPE then
			if C_CharacterCreate.CanCreateArchetype() then
				CharCreateArchetypeFrame:GoToIndex(Enum.CharCreateArchetypeBases.UseArchetype)
			else
				CharCreateArchetypeFrame:GoToIndex(Enum.CharCreateArchetypeBases.Freepick)
			end
		end
	else
		local _,_,index = GetSelectedClass()
		SetCharacterClass(index)
	end

	SetCharacterGender(GetSelectedSex());
	--end

	-- Hair customization stuff
	CharacterCreate_UpdateHairCustomization();
	SetCharacterCreateFacing(-15);
	
	CharacterChangeFixup(); -- this will fix class if druid is a wrong class for this realm

	CharCreateRoleSelectFrame:ClearSelected()
	CharCreateSubroleSelectFrame:ClearAll()
	CharCreateArchetypeSelectFrame:ClearAll()

	if CharacterCreateFrame.state and (CharacterCreateFrame.state ~= CHAR_CREATE_STEPS_DEFAULT[1]) then
		CharacterCreateNavigationFrame:GoToIndex(CHAR_CREATE_STEPS_DEFAULT[1])
	end
end

function CharacterCreate_OnHide()
	PAID_SERVICE_CHARACTER_ID = nil;
	PAID_SERVICE_TYPE = nil;
	if ( CharacterCreateFrame.state == "CUSTOMIZATION" ) then
		CharacterCreate_Back();
	end
	-- character previews will need to be redone if coming back to character create. One reason is all the memory used for
	-- tracking the frames (on the c side) will get released if the user returns to the login screen
	CharCreatePreviewFrame.rebuildPreviews = true;
	CharCreate_ResetCamera()
end

function CharacterCreate_OnEvent(event, arg1, arg2, arg3)
	if ( event == "RANDOM_CHARACTER_NAME_RESULT" ) then
		if ( arg1 == 0 ) then
			-- Failed.  Generate a random name locally.
			CharacterCreateNameEdit:SetText(GenerateRandomName());
		else
			-- Succeeded.  Use what the server sent.
			CharacterCreateNameEdit:SetText(arg2);
		end
		CharacterCreateRandomName:Enable();
		CharCreateOkayButton:Enable();
		PlaySound("gsCharacterCreationLook");
	elseif ( event == "GLUE_UPDATE_EXPANSION_LEVEL" ) then
		-- Expansion level changed while online, so enable buttons as needed
		if ( CharacterCreateFrame:IsShown() ) then
			CharacterCreateEnumerateRaces(GetAvailableRaces());
			CharacterCreate_RefreshClassButtons(CharacterCreate)
		end
	end
end

function CharacterCreateFrame_OnMouseWheel(delta)
	if CharacterCreateFrame.state ~= Enum.CharCreateSteps.Customization then
		return
	end

	local _, fileString = GetNameForRace()
	local zoomInValue = math.abs(ZoomController.zoom - (CHAR_CREATE_CAMERA_ZOOM_IN[fileString] or CHAR_CREATE_ZOOM))*0.5
	local zoomOutValue = ZoomController.zoom*0.5

	if delta > 0 then
		ZoomController.manualZoomOut = nil
		ZoomController.manualZoomIn = ZoomController.zoom + zoomInValue
		IS_ZOOMED = true
	else
		ZoomController.manualZoomIn = nil 
		ZoomController.manualZoomOut = math.max(0, ZoomController.zoom - zoomOutValue)
		IS_ZOOMED = false
	end
	
	ZoomController:Show()
end

function CharacterCreateFrame_OnMouseDown(button)
	if ( button == "LeftButton" ) then
		CHARACTER_CREATE_ROTATION_START_X = GetCursorPosition();
		CHARACTER_CREATE_INITIAL_FACING = GetCharacterCreateFacing();
	end
end

function CharacterCreateFrame_OnMouseUp(button)
	if ( button == "LeftButton" ) then
		CHARACTER_CREATE_ROTATION_START_X = nil
	end
end

function CharacterCreateFrame_OnUpdate(self, elapsed)
	if ( CHARACTER_CREATE_ROTATION_START_X ) then
		local x = GetCursorPosition();
		local diff = (x - CHARACTER_CREATE_ROTATION_START_X) * CHARACTER_ROTATION_CONSTANT;
		CHARACTER_CREATE_ROTATION_START_X = GetCursorPosition();
		SetCharacterCreateFacing(GetCharacterCreateFacing() + diff);
		CharCreate_RotatePreviews();
	end
	CharacterCreateWhileMouseDown_Update(elapsed);
end

function CharacterCreateEnumerateRaces(...)
	CharacterCreate.numRaces = select("#", ...)/3;
	if ( CharacterCreate.numRaces > MAX_RACES ) then
		message("Too many races!  Update MAX_RACES");
		return;
	end
	local button, gender;
	local index = 1;
	local selectedSex = GetSelectedSex();
	if ( selectedSex == SEX_MALE ) then
		gender = "MALE";
	else
		gender = "FEMALE";
	end

	for i=1, select("#", ...), 3 do
		local raceName, raceEnglishName, enabled = select(i, ...)
		enabled = toboolean(enabled)
		-- Engine can return nil raceName (male-only race on female, empty ChrRaces name). Empty English
		-- name used to crash SetRace concatenating nil flavor text.
		raceName = raceName or raceEnglishName or ("Race"..index)
		raceEnglishName = (raceEnglishName or raceName or ""):upper()
		button = _G["CharCreateRaceButton"..index]
		local okFaction, _, buttonFaction = pcall(GetFactionForRace, index)
		CharacterCreate_AliasExtraRacials(okFaction and buttonFaction or "Alliance")
		
		CHAR_CREATE_LORE_FACTION = strlower(tostring((okFaction and buttonFaction) or "Alliance"))
		button:SetRace(raceName, raceEnglishName, gender, enabled)
		index = index + 1
	end

	for i = CharacterCreate.numRaces + 1, MAX_RACES, 1 do
		_G["CharCreateRaceButton"..i]:Hide()
	end
	CharacterCreate_LayoutRaceButtons(CharacterCreate.numRaces)
end

function CharacterCreate_ShowRegularClasses(self)
	CharCreateClassFrame:Show()
	CharCreateArchetypeFrame:Hide()
	CharacterCreate.classGuideActive = false
	CharacterCreate_SetClassGuideEntryShown(false)
	CharacterCreate_HideClassGuide()
	CharCreateClassContainer:Show()

	local pool = self.classButtonPool
	pool:ReleaseAll()

	CharCreateClassContainer.Row2:Hide()

	local layoutIndex = 0
	local classID, button, className, classFile, enabled
	for i = 1, 11 do
		classID = Enum.Class[CLASSIC_CLASS_ORDER[i]]
		if classID ~= 6 and C_CharacterCreate.CanCreateClass(classID) then
			layoutIndex = layoutIndex + 1
			button = pool:Acquire()
			button:SetID(classID)
			button.layoutIndex = layoutIndex
			button:SetParent(CharCreateClassContainer.Row1)
			button:Show()
			className, classFile = GetClassInfo(classID)
			button:SetClass(className, classFile, true)
		end
	end
	
	CharCreateClassContainer.Row1:MarkDirty()
	CharCreateClassContainer.Row2:MarkDirty()
	CharCreateClassContainer:MarkDirty()

	CharacterCreate.classesVisible = "REGULAR"
	CharCreateSwapClassesButton:SetText("CoA Classes")
end

function CharacterCreate_ShowCoAClasses(self)
	CharCreateClassFrame:Show()
	CharCreateArchetypeFrame:Hide()

	local pool = self.classButtonPool
	pool:ReleaseAll()

	if CharacterCreate.classGuideActive and CharacterCreate_HasClassGuideData() then
		CharacterCreate_SetClassGuideEntryShown(false)
		CharCreateClassContainer:Hide()
		CharacterCreate_UpdateClassGuideFrame()
		CharacterCreate_ShowClassGuideClasses(self)
		self.classesVisible = "COA"
		return
	end

	CharacterCreate.classGuideActive = false
	CharacterCreate_SetClassGuideEntryShown(true)
	CharacterCreate_HideClassGuide()
	CharCreateClassContainer:Show()

	CharCreateClassContainer.Row2:Show()

	local numInRow1 = 10
	local layoutIndex = 0
	local classID, row, button, className, classFile, enabled
	for i = 1, 21 do
		classID = Enum.Class[COA_CLASS_ORDER[i]]
		if classID and C_CharacterCreate.CanCreateClass(classID) then
			className, classFile, enabled = GetClassInfo(classID)
			if className and classFile then
				layoutIndex = layoutIndex + 1
				button = pool:Acquire()
				if numInRow1 > 0 then
					row = CharCreateClassContainer.Row1
					numInRow1 = numInRow1 - 1
					button.layoutIndex = layoutIndex
				else
					row = CharCreateClassContainer.Row2
					button.layoutIndex = layoutIndex - 10
				end
				button:SetID(classID)
				button:SetParent(row)
				button:Show()
				button:SetClass(className, classFile, true)
			end
		end
	end

	CharCreateClassContainer.Row1:MarkDirty()
	CharCreateClassContainer.Row2:MarkDirty()
	CharCreateClassContainer:MarkDirty()

	self.classesVisible = "COA"
	CharCreateSwapClassesButton:SetText("Classic Classes")
end

function CharacterCreate_ShowFreepickClasses(self)
	CharCreateClassFrame:Hide()
	CharacterCreate.classGuideActive = false
	CharacterCreate_SetClassGuideEntryShown(false)
	CharacterCreate_HideClassGuide()
	
	if C_CharacterCreate.CanCreateArchetype() then
		CharCreateArchetypeFrame:Show()
	else
		CharCreateArchetypeFrame:Hide()
	end

	CharacterCreate.classesVisible = "FREEPICK"
end

function CharacterCreate_SwapClasses()
	if (CharacterCreate.classesVisible == "REGULAR") then
		CharacterCreate_ShowCoAClasses(CharacterCreate)
	else
		CharacterCreate_ShowRegularClasses(CharacterCreate)
	end
end

CHAR_CREATE_FEMALE_ONLY_RACES = { ["Harpy"] = true, ["Banshee"] = true }

function CharacterCreate_IsFemaleOnlyRace(id)
	local button = _G["CharCreateRaceButton"..(id or CharacterCreate.selectedRace or 0)]
	local name = button and button.Text and button.Text:GetText()
	return name and CHAR_CREATE_FEMALE_ONLY_RACES[name] or false
end

CHAR_CREATE_MALE_ONLY_RACES = {
	["Tuskarr"] = true, ["Taunka"] = true, ["Vrykul"] = true, ["Forest Troll"] = true, ["Ice Troll"] = true,
	["Fel Orc"] = true, ["Skeleton"] = true, ["Faceless One"] = true, -- Murloc: male = Whim murloc, female = classic murloc
}

function CharacterCreate_IsMaleOnlyRace(id)
	local button = _G["CharCreateRaceButton"..(id or CharacterCreate.selectedRace or 0)]
	local name = button and button.Text and button.Text:GetText()
	return name and CHAR_CREATE_MALE_ONLY_RACES[name] or false
end

function SetCharacterRace(id)

	CharacterCreate.selectedRace = id;
	for i=1, CharacterCreate.numRaces, 1 do
		local button = _G["CharCreateRaceButton"..i];
		if button then
			if ( i == id ) then
				button:SetChecked(1);
			else
				button:SetChecked(0);
			end
		end
	end

	if CharacterCreate_IsFemaleOnlyRace(id) then
		CharCreateMaleButton:Hide()
		CharacterCreate.maleHidden = true
		CharCreateFemaleButton:Show()
		if GetSelectedSex() ~= SEX_FEMALE then
			SetCharacterGender(SEX_FEMALE)
			return
		end
	elseif CharacterCreate.maleHidden then
		CharacterCreate.maleHidden = nil
		CharCreateMaleButton:Show()
	end

	if CharacterCreate_IsMaleOnlyRace(id) then
		CharCreateFemaleButton:Hide()
		if GetSelectedSex() ~= SEX_MALE then
			SetCharacterGender(SEX_MALE)
			return
		end
	elseif CharCreateMaleButton:IsShown() then
		CharCreateFemaleButton:Show()
	end

	local name, faction = GetFactionForRace(CharacterCreate.selectedRace);

	if faction == nil then
		faction = "Alliance";
	end
	CharacterCreate_AliasExtraRacials(faction)

	-- Set background

	--HandleFactionBackground()

	-- Set backdrop colors based on faction
	local backdropColor = FACTION_BACKDROP_COLOR_TABLE[faction];
	--CharCreateRaceFrame.factionBg:SetGradient("VERTICAL", 0, 0, 0, backdropColor[7], backdropColor[8], backdropColor[9]);
	--CharCreateClassFrame.factionBg:SetGradient("VERTICAL", 0, 0, 0, backdropColor[7], backdropColor[8], backdropColor[9]);
	--CharCreateCustomizationFrame.factionBg:SetGradient("VERTICAL", 0, 0, 0, backdropColor[7], backdropColor[8], backdropColor[9]);
	--harCreatePreviewFrame.factionBg:SetGradient("VERTICAL", 0, 0, 0, backdropColor[7], backdropColor[8], backdropColor[9]);
	CharCreateCustomizationFrameBanner:SetVertexColor(backdropColor[10], backdropColor[11], backdropColor[12]);
	CharacterCreateNameEdit:SetBackdropColor(backdropColor[4], backdropColor[5], backdropColor[6]);
	CharacterCreateSecondNameEdit:SetBackdropColor(backdropColor[4], backdropColor[5], backdropColor[6]);
	--CharCreateRaceInfoFrame.factionBg:SetGradient("VERTICAL", 0, 0, 0, backdropColor[7], backdropColor[8], backdropColor[9]);
	--CharCreateClassInfoFrame.factionBg:SetGradient("VERTICAL", 0, 0, 0, backdropColor[7], backdropColor[8], backdropColor[9]);

	-- race info
	local frame = CharCreateRaceInfoFrame;
	local race, fileString = GetNameForRace();
	frame.title:SetText(race);
	fileString = strupper(fileString);
	local gender;
	if ( GetSelectedSex() == SEX_MALE ) then
		gender = "MALE";
	else
		gender = "FEMALE";
	end
	local raceText = _G["RACE_INFO_"..fileString];
	local abilityIndex = 1;
	local tempText = _G["ABILITY_INFO_"..fileString..abilityIndex];
	abilityText = "";
	while ( tempText ) do
		abilityText = abilityText..tempText.."\n\n";
		abilityIndex = abilityIndex + 1;
		tempText = _G["ABILITY_INFO_"..fileString..abilityIndex];
	end
	CharCreateRaceInfoFrameScrollFrameScrollBar:SetValue(0);
	local text
	text = GetFlavorText("RACE_INFO_"..strupper(fileString), GetSelectedSex())
	if not text then
		text = "Not in GlueXML."
	end
	CharCreateRaceInfoFrame.scrollFrame.scrollChild.infoText:SetText(text.."|n|n");
	if ( abilityText and abilityText ~= "" ) then
		CharCreateRaceInfoFrame.scrollFrame.scrollChild.bulletText:SetText(abilityText);
	else
		CharCreateRaceInfoFrame.scrollFrame.scrollChild.bulletText:SetText("");
	end

	CharacterCreate_UpdateNavigtationRaceButton()
end

function SetCharacterClass(id)
	CharacterCreate.selectedClass = id;
	EventRegistry:TriggerEvent("CharacterCreate.ClassChanged", id)

	-- class info
	local frame = CharCreateClassInfoFrame;
	local className, classFileName, classIndex, tank, healer, damage = GetSelectedClass();
	local abilityIndex = 0;
	if not classFileName then
		classFileName = "WARRIOR"
	end
	if classFileName ~= "HERO" then
		C_CharacterCreate.PreviewClassOutfit(classFileName)
	else
		C_CharacterCreate.ClearClassPreview()
	end
	local tempText = _G["CLASS_INFO_"..classFileName..abilityIndex];
	abilityText = "";
	while ( tempText ) do
		abilityText = abilityText..tempText.."\n\n";
		abilityIndex = abilityIndex + 1;
		tempText = _G["CLASS_INFO_"..classFileName..abilityIndex];
	end
	CharCreateClassInfoFrame.title:SetText(className);
	CharCreateClassInfoFrame.scrollFrame.scrollChild.bulletText:SetText(abilityText);
	CharCreateClassInfoFrame.scrollFrame.scrollChild.infoText:SetText(GetFlavorText("CLASS_"..strupper(classFileName), GetSelectedSex()).."|n|n");
	CharCreateClassInfoFrameScrollFrameScrollBar:SetValue(0);

	if (classIndex ~= Enum.Class.HERO) then
		CharCreateArchetypeFrame:GoToIndex(Enum.CharCreateArchetypeBases.Freepick)
	end
end

function CharacterCreate_OnChar()
end

function CharacterCreate_OnKeyDown(key)
	if ( key == "ESCAPE" ) then
		CharacterCreate_Back();
	elseif ( key == "ENTER" ) then
		CharacterCreate_Forward();
	elseif ( key == "PRINTSCREEN" ) then
		Screenshot();
	end
end

function CharacterCreate_UpdateModel(self)
	UpdateCustomizationScene();
	self:AdvanceTime();
end

function CharacterCreate_Finish()
	PlaySound("gsCharacterCreationCreateChar");

	-- If something disabled this button, ignore this message.
	-- This can happen if you press enter while it's disabled, for example.
	if ( not CharCreateOkayButton:IsEnabled() ) then
		return;
	end

	if ( PAID_SERVICE_TYPE ) then
		GlueDialog_Show("CONFIRM_PAID_SERVICE");
	else
		-- if using templates, pandaren must pick a faction
		--local _, faction = GetFactionForRace(CharacterCreate.selectedRace);

		CoACharacterNames.CreateCharacter();

		TutorialExperienceEventListener:Show()
		
		local archetypeID = CharCreateArchetypeSelectFrame:GetCurrentIndex()

		if archetypeID then
			local buildID = C_CharacterCreate.GetArchetypeInfo(archetypeID)
			if buildID then
				NewCharacterSetupUtil.SetPendingKeyValue("archetypeBuildID", buildID)
			end
		end
	end
end

function CharacterCreate_Back()
	local prevStep = CharacterCreateNavigationFrame:GetPrevIndex()

	if CharacterCreate.classGuideActive and CharacterCreateFrame.state == Enum.CharCreateSteps.NameEdit and not prevStep then
		PlaySound("gsCharacterCreationCancel");
		CharacterCreateNavigationFrame:GoToIndex(Enum.CharCreateSteps.ClassRace)
		return;
	end

	if CharacterCreate.classGuideActive and CharacterCreateFrame.state == Enum.CharCreateSteps.ClassRace then
		PlaySound("gsCharacterCreationCancel");
		CharacterCreate_ClassGuideBack()
		return;
	end

	if (prevStep) then
		PlaySound("gsCharacterCreationCancel");
		CharacterCreateNavigationFrame:GoToIndex(prevStep)
		return;
	end

	PlaySound("gsCharacterCreationCancel");
	CHARACTER_SELECT_BACK_FROM_CREATE = true;
	NewCharacterSetupUtil.ClearPendingData()
	SetGlueScreen("charselect");
end

function CharacterCreate_UpdateFacialHairCustomization()
	if ( GetFacialHairCustomization() == "NONE" ) then
		CharacterCustomizationButtonFrame5:Hide();
		--CharCreateRandomizeButton:SetPoint("TOP", "CharacterCustomizationButtonFrame5", "BOTTOM", 0, -5);
	else
		CharacterCustomizationButtonFrame5Text:SetText(_G["FACIAL_HAIR_"..GetFacialHairCustomization()]);
		CharacterCustomizationButtonFrame5:Show();
		--CharCreateRandomizeButton:SetPoint("TOP", "CharacterCustomizationButtonFrame5", "BOTTOM", 0, -5);
	end
end

function CharacterCreate_UpdateHairCustomization()
	if not _G["HAIR_"..GetHairCustomization().."_STYLE"] or _G["HAIR_"..GetHairCustomization().."_STYLE"] == "" then
		CharCreateCustomizationButton3:Hide()
		CharCreateCustomizationButton4:SetPoint("TOP", CharCreateCustomizationButton2, "BOTTOM", 0, -20)
	else
		CharCreateCustomizationButton3:Show()
		CharCreateCustomizationButton3.text:SetText(_G["HAIR_"..GetHairCustomization().."_STYLE"])
		--CharCreateCustomizationButton4:SetPoint("TOP", CharCreateCustomizationButton3, "BOTTOM", 0, -20)
	end

	if not _G["HAIR_"..GetHairCustomization().."_COLOR"] or _G["HAIR_"..GetHairCustomization().."_COLOR"] == "" then
		CharCreateCustomizationButton4:Hide()
		if CharCreateCustomizationButton3:IsShown() then
			CharCreateCustomizationButton5:SetPoint("TOP", CharCreateCustomizationButton4, "BOTTOM", 0, 0)
		else
			CharCreateCustomizationButton5:SetPoint("TOP", CharCreateCustomizationButton3, "BOTTOM", 0, 0)
		end
	else
		CharCreateCustomizationButton4:Show()
		CharCreateCustomizationButton4.text:SetText(_G["HAIR_"..GetHairCustomization().."_COLOR"])
		CharCreateCustomizationButton5:SetPoint("TOP", CharCreateCustomizationButton4, "BOTTOM", 0, 0)
	end

	if not _G["FACIAL_HAIR_"..GetFacialHairCustomization()] or _G["FACIAL_HAIR_"..GetFacialHairCustomization()] == "" then
		CharCreateCustomizationButton5:Hide()
	else
		CharCreateCustomizationButton5:Show()
		CharCreateCustomizationButton5.text:SetText(_G["FACIAL_HAIR_"..GetFacialHairCustomization()])
	end

end

function CharacterCreate_Forward()
	local nextStep = CharacterCreateNavigationFrame:GetNextIndex()

	if CharCreateOkayButton:IsEnabled() == 0 then
		return
	end

	if CharacterCreate.classGuideActive and CharacterCreateFrame.state == Enum.CharCreateSteps.ClassRace then
		if not CharacterCreate.selectedClassGuideClassID then
			return
		end

		PlaySound("gsCharacterSelectionCreateNew");
		CharCreateOkayButton:Disable()
		CharacterCreate_LoadNavigation(CHAR_CREATE_STEPS_CLASS_GUIDE)
		CharacterCreateNavigationFrame:GoToIndex(Enum.CharCreateSteps.NameEdit)
		return
	end

	if (nextStep) then
		PlaySound("gsCharacterSelectionCreateNew");
		CharCreateOkayButton:Disable()
		CharacterCreateNavigationFrame:GoToIndex(nextStep)
		return;
	end

	CharacterCreate_Finish()
end

function CharCreateCustomizationFrame_OnShow ()
	-- reset size/tex coord to default to facilitate switching between genders for Pandaren
	CharCreateCustomizationFrameBanner:SetSize(BANNER_DEFAULT_SIZE[1], BANNER_DEFAULT_SIZE[2]);
	CharCreateCustomizationFrameBanner:SetTexCoord(BANNER_DEFAULT_TEXTURE_COORDS[1], BANNER_DEFAULT_TEXTURE_COORDS[2], BANNER_DEFAULT_TEXTURE_COORDS[3], BANNER_DEFAULT_TEXTURE_COORDS[4]);

	-- check each button and hide it if there are no values select
	local resize = 0;
	local lastGood = 0;
	local isSkinVariantHair = false --GetSkinVariationIsHairColor(CharacterCreate.selectedRace);
	local isDefaultSet = 0;
	local checkedButton = 1;

	-- check if this was set, if not, default to 1
	if ( CharacterCreateFrame.customizationType == 0 or CharacterCreateFrame.customizationType == nil ) then
		CharacterCreateFrame.customizationType = 1;
	end
	for i=1, NUM_CHAR_CUSTOMIZATIONS, 1 do
		if ( ( --[[GetNumFeatureVariationsForType(i)]]5 <= 1 ) or ( isSkinVariantHair and i == CHAR_CUSTOMIZE_HAIR_COLOR ) ) then
			resize = resize + 1;
			_G["CharCreateCustomizationButton"..i]:Hide();
		else
			_G["CharCreateCustomizationButton"..i]:Show();
			--_G["CharCreateCustomizationButton"..i]:SetChecked(0); -- we will handle default selection
			-- this must be done since a selected button can 'disappear' when swapping genders
			if ( isDefaultSet == 0 and CharacterCreateFrame.customizationType == i) then
				isDefaultSet = 1;
				checkedButton = i;
			end
			-- set your anchor to be the last good, this currently means button 1 HAS to be shown
			if (i > 1) then
				_G["CharCreateCustomizationButton"..i]:SetPoint( "TOP",_G["CharCreateCustomizationButton"..lastGood]:GetName() , "BOTTOM");
			end
			lastGood = i;
		end
	end

	if (isDefaultSet == 0) then
		CharacterCreateFrame.customizationType = lastGood;
		checkedButton = lastGood;
	end
	--_G["CharCreateCustomizationButton"..checkedButton]:SetChecked(1);

	if (resize > 0) then
	-- we need to resize and remap the banner texture
		local buttonx, buttony = CharCreateCustomizationButton1:GetSize()
		local screenamount = resize*buttony;
		print(screenamount);
		local frameX, frameY = CharCreateCustomizationFrameBanner:GetSize();
		local pctShrink = .2*resize;
		local uvXDefaultSize = BANNER_DEFAULT_TEXTURE_COORDS[2] - BANNER_DEFAULT_TEXTURE_COORDS[1];
		local uvYDefaultSize = BANNER_DEFAULT_TEXTURE_COORDS[4] - BANNER_DEFAULT_TEXTURE_COORDS[3];
		local newYUV = pctShrink*uvYDefaultSize + BANNER_DEFAULT_TEXTURE_COORDS[3];
		-- end coord stay the same
		CharCreateCustomizationFrameBanner:SetTexCoord(BANNER_DEFAULT_TEXTURE_COORDS[1], BANNER_DEFAULT_TEXTURE_COORDS[2], newYUV, BANNER_DEFAULT_TEXTURE_COORDS[4]);
		print(pctShrink);
		CharCreateCustomizationFrameBanner:SetSize(frameX, frameY - screenamount);
		print(CharCreateCustomizationFrameBanner:GetTexCoord());
	end

	--CharCreateRandomizeButton:SetPoint("TOP", _G["CharCreateCustomizationButton"..lastGood]:GetName(), "BOTTOM", 0, 0);
end

function CharacterClass_OnClick(self, id)
	if( self:IsEnabled() ) then
		PlaySound("gsCharacterCreationClass");
		local _,_,currClass = GetSelectedClass();
		if ( currClass ~= id ) then
			SetSelectedClass(id);
			SetCharacterClass(id);
			SetCharacterRace(GetSelectedRace());
			CharacterChangeFixup();
		else
			self:SetChecked(1);
		end
	else
		self:SetChecked(0);
	end
end

function CharacterRace_OnClick(self, id, forceSelect)
	if( self:IsEnabled() == 1 ) then
		PlaySound("gsCharacterCreationClass");
		if ( GetSelectedRace() ~= id or forceSelect ) then
			local isClassGuideClassSelection = C_CharacterCreate.CanCreateCoA()
				and CharacterCreate.classGuideActive
				and CharacterCreate.selectedClassGuideSubroleID
				and (CharacterCreateFrame.state == Enum.CharCreateSteps.ClassRace
					or CharacterCreateFrame.state == Enum.CharCreateSteps.Archetype)
			local selectedGuideClassID = CharacterCreate.selectedClassGuideClassID

			SetSelectedRace(id);
			SetCharacterRace(id);
			if isClassGuideClassSelection then
				CharacterCreateEnumerateRaces(GetAvailableRaces());
				SetCharacterRace(GetSelectedRace());
				SetCharacterCreateFacing(-15);

				if selectedGuideClassID and CharacterCreate_ClassGuideClassIsAvailableForRace(selectedGuideClassID, CharacterCreate.selectedRace) then
					CharacterCreate.selectedClassGuideClassID = selectedGuideClassID
					SetSelectedClass(selectedGuideClassID)
					SetCharacterClass(selectedGuideClassID)
					CharCreate_EnableNextButton(true)
				else
					local fallbackClassID = CharacterCreate_GetFirstAvailableClassGuideClassForRace(CharacterCreate.selectedRace)
					CharacterCreate.selectedClassGuideClassID = fallbackClassID
					if fallbackClassID then
						SetSelectedClass(fallbackClassID)
						SetCharacterClass(fallbackClassID)
						CharCreate_EnableNextButton(true)
					else
						local _,_,classIndex = GetSelectedClass();
						SetCharacterClass(classIndex)
						CharCreateOkayButton:Disable()
					end
				end

				CharacterCreate_RefreshClassButtons(CharacterCreate)
				CharacterCreate_UpdateHairCustomization();
				CharacterChangeFixup();
				return
			end

			SetCharacterGender(GetSelectedSex());
			SetCharacterCreateFacing(-15);
			CharacterCreate_RefreshClassButtons(CharacterCreate)
			local _,_,classIndex = GetSelectedClass();
			if ( PAID_SERVICE_TYPE ) then
				classIndex = PaidChange_GetCurrentClassIndex();
				SetSelectedClass(classIndex);	-- selecting a race would have changed class to default
			end
			SetCharacterClass(classIndex);

			-- Hair customization stuff
			CharacterCreate_UpdateHairCustomization();

			CharacterChangeFixup();
		else
			self:SetChecked(1);
		end
	else
		self:SetChecked(0);
	end
end

function SetCharacterGender(sex)
	local gender;

	if ( sex == SEX_MALE ) then
		CharCreateMaleButton:SetChecked(1);
		CharCreateFemaleButton:SetChecked(0);
	else
		CharCreateMaleButton:SetChecked(0);
		CharCreateFemaleButton:SetChecked(1);
	end
	SetSelectedSex(sex);

	-- Update race images to reflect gender
	CharacterCreateEnumerateRaces(GetAvailableRaces());
	CharacterCreate_RefreshClassButtons(CharacterCreate)
 	SetCharacterRace(GetSelectedRace());

	local _,_,classIndex = GetSelectedClass();
	if ( PAID_SERVICE_TYPE ) then
		classIndex = PaidChange_GetCurrentClassIndex();
		-- PandarenFactionButtons_SetTextures();
	end
	SetCharacterClass(classIndex);

	CharacterCreate_UpdateHairCustomization();
	CharacterChangeFixup();

	-- Update preview models if on customization step
	if ( CharCreatePreviewFrame:IsShown() ) then
		CharCreateCustomizationFrame_OnShow(); -- buttons may need to reset for dirty Pandarens
		CharCreate_PrepPreviewModels();
		CharCreate_ResetFeaturesDisplay();
	end
end

local function CharacterCustomization_Cycle(id, direction)
	C_CharacterCreate.CycleCustomizationSkipLocked(id, direction)
end

function CharacterCustomization_Left(id)
	PlaySound("gsCharacterCreationLook");
	CharacterCustomization_Cycle(id, -1);
	CharacterCreate_UndressCustomizationHeadSlot()
	if (id > 1) then
		SetFaceCustomizeCamera(true)
	else
		SetFaceCustomizeCamera(false)
	end
end

function CharacterCustomization_Right(id)
	PlaySound("gsCharacterCreationLook");
	CharacterCustomization_Cycle(id, 1);
	CharacterCreate_UndressCustomizationHeadSlot()
	if (id > 1) then
		SetFaceCustomizeCamera(true)
	else
		SetFaceCustomizeCamera(false)
	end
end

function CharacterCreate_GenerateRandomName(button)
	CharacterCreateNameEdit:SetText(GetRandomName());
end

function CharacterCreate_Randomize()
	PlaySound("gsCharacterCreationLook");
	RandomizeCharCustomization();
	CharCreate_ResetFeaturesDisplay();
	CharacterCreate_UpdateCustomizationEquipment();
end

function CharacterCreateRotateRight_OnUpdate(self)
	if ( self:GetButtonState() == "PUSHED" ) then
		SetCharacterCreateFacing(GetCharacterCreateFacing() + CHARACTER_FACING_INCREMENT);
		CharCreate_RotatePreviews();
	end
end

function CharacterCreateRotateLeft_OnUpdate(self)
	if ( self:GetButtonState() == "PUSHED" ) then
		SetCharacterCreateFacing(GetCharacterCreateFacing() - CHARACTER_FACING_INCREMENT);
		CharCreate_RotatePreviews();
	end
end

function SetButtonDesaturated(button, desaturated)
	if ( not button ) then
		return;
	end
	local icon = button:GetNormalTexture();
	if ( not icon ) then
		return;
	end

	icon:SetDesaturated(desaturated);
end

function GetFlavorText(tagname, sex)
	local primary, secondary;
	if ( sex == SEX_MALE ) then
		primary = "";
		secondary = "_FEMALE";
	else
		primary = "_FEMALE";
		secondary = "";
	end
	local text = _G[tagname..primary];
	if ( (text == nil) or (text == "") ) then
		text = _G[tagname..secondary];
	end
	return text;
end

function CharacterChangeFixup()
	if ( PAID_SERVICE_TYPE ) then
		-- no class changing as a paid service
		EventRegistry:TriggerEvent("CharacterCreate.PaidServiceActive")
		for i=1, MAX_RACES, 1 do
			local allow = false;
			if ( PAID_SERVICE_TYPE == PAID_FACTION_CHANGE ) then
				local faction = GetFactionForRace(PaidChange_GetCurrentRaceIndex());
				if ( (i == PaidChange_GetCurrentRaceIndex()) or ((GetFactionForRace(i) ~= faction) and (IsRaceClassValid(i,CharacterCreate.selectedClass))) ) then
					allow = true;
				end
			elseif ( PAID_SERVICE_TYPE == PAID_RACE_CHANGE ) then
				local faction = GetFactionForRace(PaidChange_GetCurrentRaceIndex());
				if ( (i == PaidChange_GetCurrentRaceIndex()) or ((GetFactionForRace(i) == faction) and (IsRaceClassValid(i,CharacterCreate.selectedClass))) ) then
					allow = true
				end
			elseif ( PAID_SERVICE_TYPE == PAID_CHARACTER_CUSTOMIZATION ) then
				if ( i == CharacterCreate.selectedRace ) then
					allow = true
				end
			end

			if (not allow) then
				local button = _G["CharCreateRaceButton"..i];
				button:Disable();
				SetButtonDesaturated(button, true)
			end
		end
	end

	RefreshSelectedClass()
end

function CharCreateSelectCustomizationType(newType)
	-- deselect previous type selection
	if ( CharacterCreateFrame.customizationType and CharacterCreateFrame.customizationType ~= newType ) then
		--_G["CharCreateCustomizationButton"..CharacterCreateFrame.customizationType]:SetChecked(0);
	end
	--_G["CharCreateCustomizationButton"..newType]:SetChecked(1);
	CharacterCreateFrame.customizationType = newType;
	CharCreate_ResetFeaturesDisplay();

	--[[if (newType > 1) then
		SetFaceCustomizeCamera(true);
	else
		SetFaceCustomizeCamera(false);
	end]]
end

function CharCreate_ResetFeaturesDisplay()
	--SetPreviewFramesFeature(CharacterCreateFrame.customizationType);
	-- set the previews scrollframe container height
	-- since the first and the last previews need to be in the center position when scrolled all the way
	-- to the top or to the bottom, there will be gaps of height equal to 2 previews on each side
	local numTotalButtons = 4--GetNumFeatureVariations() + 4;
	CharCreatePreviewFrame.scrollFrame.container:SetHeight(numTotalButtons * PREVIEW_FRAME_HEIGHT - PREVIEW_FRAME_Y_OFFSET);

	for _, previewFrame in pairs(CharCreatePreviewFrame.previews) do
		previewFrame.featureType = 0;
	end

	CharCreate_DisplayPreviewModels();
end

function CharCreate_PrepPreviewModels(reloadModels)
	local displayFrame = CharCreatePreviewFrame;

	-- clear models if rebuildPreviews got flagged
	local rebuildPreviews = displayFrame.rebuildPreviews;
	displayFrame.rebuildPreviews = nil;

	-- need to reload models class was swapped to or from DK
	local classSwap = false;
	local _, class = GetSelectedClass();
	--[[if ( class == "DEATHKNIGHT" or displayFrame.lastClass == "DEATHKNIGHT" ) and ( class ~= displayFrame.lastClass ) then
		classSwap = true;
	end]]

	-- always clear the featureType
	for index, previewFrame in pairs(displayFrame.previews) do
		previewFrame.featureType = 0;
		-- force model reload if class changed
		if ( classSwap ) then
			previewFrame.race = nil;
			previewFrame.gender = nil;
		end
		if ( rebuildPreviews ) then
			--SetPreviewFrame(previewFrame.model:GetName(), index);
		end
	end
end

function CharCreate_DisplayPreviewModels(selectionIndex)
	if ( not selectionIndex ) then
		selectionIndex = featureIndex--GetSelectedFeatureVariation();
	end

	local displayFrame = CharCreatePreviewFrame;
	local previews = displayFrame.previews;
	local numVariations = 8--GetNumFeatureVariations();
	local currentFeatureType = CharacterCreateFrame.customizationType;

	local race = GetSelectedRace();
	local gender = GetSelectedSex();

	-- selection index is the center preview
	-- there are 2 previews above and 2 below, and will pad it out to 1 more on each side, for a total of 7 previews to set up
	for index = selectionIndex - 3, selectionIndex + 3 do
		-- there is empty space both at the beginning and at end of the list, each gap the height of 2 previews
		if ( index > 0 and index <= numVariations ) then
			local previewFrame = previews[index];
			-- create button if we don't have it yet
			if ( not previewFrame ) then
				previewFrame = CreateFrame("BUTTON", "PreviewFrame"..index, displayFrame.scrollFrame.container, "CharCreatePreviewFrameTemplate");
				-- index + 1 because of 2 gaps at the top and -1 for the current preview
				previewFrame:SetPoint("TOPLEFT", PREVIEW_FRAME_X_OFFSET, (index + 1) * -PREVIEW_FRAME_HEIGHT + PREVIEW_FRAME_Y_OFFSET);
				previewFrame.button.index = index;
				previews[index] = previewFrame;
				--SetPreviewFrame(previewFrame.model:GetName(), index);
				-- no texture as of yet
				--previewFrame:SetNormalTexture("Interface\\Glues\\CharacterCreate\\UI-CharacterCreate-Classes")
			end
			-- load model if needed, may have been cleared by different race/gender selection
			if ( previewFrame.race ~= race or previewFrame.gender ~= gender ) then
				--SetPreviewFrameModel(index);
				previewFrame.race = race;
				previewFrame.gender = gender;
				-- apply settings
				local model = previewFrame.model;
				--model:SetCustomCamera(cameraID);
				local scale = 1--model:GetWorldScale();
				--model:SetCameraTarget(config.tx * scale, config.ty * scale, config.tz * scale);
				--model:SetCameraDistance(config.distance * scale);
				local cx, cy, cz = model:GetPosition();
				-- model:SetPosition(cx-15, cy, cz)
				--model:SetCameraPosition(cx, cy, config.cz * scale);
				previewFrame.model:SetLight(1, 0, 0, 0, 0, 1, 1.0, 1.0, 1.0);
			end
			-- need to reset the model if it was last used to preview a different feature
			if ( previewFrame.featureType ~= currentFeatureType ) then
				--ResetPreviewFrameModel(index);
				--ShowPreviewFrameVariation(index);
				previewFrame.featureType = currentFeatureType;
			end
			previewFrame:Show();
		else
			-- need to hide tail previews when going to features with fewer styles
			if ( previews[index] ) then
				previews[index]:Hide();
			end
		end
	end
	displayFrame.border.number:SetText("Option "..selectionIndex.."                     ");
	displayFrame.selectionIndex = selectionIndex;
	CharCreate_RotatePreviews();
	CharCreatePreviewFrame_UpdateStyleButtons();
	-- scroll to center the selection
	if ( not displayFrame.animating ) then
		displayFrame.scrollFrame:SetVerticalScroll((selectionIndex - 1) * PREVIEW_FRAME_HEIGHT);
	end
end


function CharCreate_RotatePreviews()
	if ( CharCreatePreviewFrame:IsShown() ) then
		local facing = ((GetCharacterCreateFacing())/ -180) * math.pi;
		local previews = CharCreatePreviewFrame.previews;
		--CharCreatePreviewFrame.selectionIndex = 0;
		for index = CharCreatePreviewFrame.selectionIndex - 3, CharCreatePreviewFrame.selectionIndex + 3 do
			local previewFrame = previews[index];
			if ( previewFrame ) then -- and previewFrame.model:HasCustomCamera()
				--previewFrame.model:SetCameraFacing(facing);
			end
		end
	end
end

function CharCreate_ChangeFeatureVariation(delta)
	local numVariations = 8--GetNumFeatureVariations();
	local startIndex = featureIndex--GetSelectedFeatureVariation();
	local endIndex = startIndex + delta;
	if ( endIndex < 1 or endIndex > numVariations ) then
		return;
	end
	PlaySound("gsCharacterCreationClass");
	featureIndex = endIndex
	CharCreatePreviewFrame_SelectFeatureVariation(endIndex);
end

function CharCreatePreviewFrameButton_OnClick(self)
	PlaySound("gsCharacterCreationClass");
	CharCreatePreviewFrame_SelectFeatureVariation(self.index);
end

function CharCreatePreviewFrame_SelectFeatureVariation(endIndex)
	local self = CharCreatePreviewFrame;
	if ( self.animating ) then
		if ( not self.queuedIndex ) then
			self.queuedIndex = endIndex;
		end
	else
		local startIndex = featureIndex--GetSelectedFeatureVariation();
		--SelectFeatureVariation(endIndex);
		for i=1,endIndex do
			CycleCharCustomization(FeatureType, 1);
		end
		CharacterCreate_UndressCustomizationHeadSlot()
		CharCreatePreviewFrame_UpdateStyleButtons();
		featureIndex = endIndex
		CharCreatePreviewFrame_StartAnimating(startIndex, endIndex);
	end
end

function CharCreatePreviewFrame_StartAnimating(startIndex, endIndex)
	local self = CharCreatePreviewFrame;
	if ( self.animating ) then
		return;
	else
		self.startIndex = startIndex;
		self.currentIndex = startIndex;
		self.endIndex = endIndex;
		self.queuedIndex = nil;
		self.direction = 1;
		if ( self.startIndex > self.endIndex ) then
			self.direction = -1;
		end
		self.movedTotal = 0;
		self.moveUntilUpdate = PREVIEW_FRAME_HEIGHT;
		self.animating = true;
	end
end

function CharCreatePreviewFrame_StopAnimating()
	local self = CharCreatePreviewFrame;
	if ( self.animating ) then
		self.animating = false;
	end
end

local ANIMATION_SPEED = 5;
function CharCreatePreviewFrame_OnUpdate(self, elapsed)
	if ( self.animating ) then
		local moveIncrement = PREVIEW_FRAME_HEIGHT * elapsed * ANIMATION_SPEED;
		self.movedTotal = self.movedTotal + moveIncrement;
		self.scrollFrame:SetVerticalScroll((self.startIndex - 1) * PREVIEW_FRAME_HEIGHT + self.movedTotal * self.direction);
		self.moveUntilUpdate = self.moveUntilUpdate - moveIncrement;
		if ( self.moveUntilUpdate <= 0 ) then
			self.currentIndex = self.currentIndex + self.direction;
			self.moveUntilUpdate = PREVIEW_FRAME_HEIGHT;
			-- reset movedTotal to account for rounding errors
			self.movedTotal = abs(self.startIndex - self.currentIndex) * PREVIEW_FRAME_HEIGHT;
			CharCreate_DisplayPreviewModels(self.currentIndex);
		end
		if ( self.currentIndex == self.endIndex ) then
			self.animating = false;
			CharCreate_DisplayPreviewModels();
			if ( self.queuedIndex ) then
				local newIndex = self.queuedIndex;
				self.queuedIndex = nil;
				--SelectFeatureVariation(newIndex);
				featureIndex = newIndex
				CycleCharCustomization(FeatureType, featureIndex);
				CharacterCreate_UndressCustomizationHeadSlot()
				CharCreatePreviewFrame_UpdateStyleButtons();
				CharCreatePreviewFrame_StartAnimating(self.endIndex, newIndex);
			end
		end
	end
end

function CharCreatePreviewFrame_UpdateStyleButtons()
	local selectionIndex = math.random(1,5)--GetSelectedFeatureVariation();
	local numVariations = 8--GetNumFeatureVariations();
	if ( selectionIndex == 1 ) then
		CharCreateStyleUpButton:Enable();
		CharCreateStyleUpButton.arrow:SetDesaturated(true);
	else
		CharCreateStyleUpButton:Enable();
		CharCreateStyleUpButton.arrow:SetDesaturated(false);
	end
	if ( selectionIndex == numVariations ) then
		CharCreateStyleDownButton:Disable();
		CharCreateStyleDownButton.arrow:SetDesaturated(true);
	else
		CharCreateStyleDownButton:Disable(true);
		CharCreateStyleDownButton.arrow:SetDesaturated(false);
	end
end

local TotalTime = 0;
local KeepScrolling = nil;
local TIME_TO_SCROLL = 0.5;
function CharacterCreateWhileMouseDown_OnMouseDown(direction)
	TIME_TO_SCROLL = 0.5;
	TotalTime = 0;
	KeepScrolling = direction;
end
function CharacterCreateWhileMouseDown_OnMouseUp()
	KeepScrolling = nil;
end
function CharacterCreateWhileMouseDown_Update(elapsed)
	if ( KeepScrolling ) then
		TotalTime = TotalTime + elapsed;
		if ( TotalTime >= TIME_TO_SCROLL ) then
			CharCreate_ChangeFeatureVariation(KeepScrolling);
			TIME_TO_SCROLL = 0.25;
			TotalTime = 0;
		end
	end
end

function CharCreate_EnableNextButton(enabled)
	local button = CharCreateOkayButton;
	if enabled then
		button:Enable();
	else
		button:Disable();
	end
	button.arrow:SetDesaturated(not enabled);

	--[[if enabled then
		button.TopGlow:Hide();
		button.BottomGlow:Hide();
	else
		button.TopGlow:Show();
		button.BottomGlow:Show();
	end]]--
end

CharCreateButtonMixin = {}

function CharCreateButtonMixin:OnEnter()
	GlueTooltip_GenericTooltip(self)
end

function CharCreateButtonMixin:OnLeave()
	GlueTooltip:Hide()
end

CharCreateRaceButtonMixin = CreateFromMixins(CharCreateButtonMixin)

function CharCreateRaceButtonMixin:OnLoad()
	self:GetNormalTexture():SetDrawLayer("BACKGROUND")
	self:GetPushedTexture():SetDrawLayer("BACKGROUND")

	local faction
	if self:GetID() > 0 then
		local ok, _, f = pcall(GetFactionForRace, self:GetID())
		faction = ok and f or nil
	end

	if faction == nil then
		faction = "Alliance"
	end
	
	self:SetFaction(faction)
end

function CharCreateRaceButtonMixin:OnClick()
	local _, factionSelected = GetFactionForRace(CharacterCreate.selectedRace)

	local _, faction = GetFactionForRace(self:GetID())

	if (factionSelected ~= faction) then
		if not C_CharacterCreate.CanCreateCoA() then
			faction = faction.."REGULAR"
		end
		GlueFrameFadeIn(CharacterCreateFrameFade, 0.3, function()
			SetBackgroundModel(CharacterCreate, string.upper(faction));
			CharacterRace_OnClick(self, self:GetID());
			GlueFrameFadeOut(CharacterCreateFrameFade, 0.5)
		end);
	else
		CharacterRace_OnClick(self, self:GetID());
	end
end

function CharCreateRaceButtonMixin:OnEnable()
	self.Text:SetFontObject("GameFontNormal")
end

function CharCreateRaceButtonMixin:OnDisable()
	self.Text:SetFontObject("GameFontDisable")
end

function CharCreateRaceButtonMixin:OnEnter()
	GlueTooltip_GenericTooltip(self, "ANCHOR_NONE")
	if self.faction == "Alliance" then
		GlueTooltip:SetPoint("LEFT", self, "RIGHT", 0, 0)
	else
		GlueTooltip:SetPoint("RIGHT", self, "LEFT", 0, 0)
	end
end

function CharCreateRaceButtonMixin:SetFaction(faction)
	self.faction = faction
	local backdropColor = FACTION_SHADOW_COLORS[faction]
	self.Shadow:SetVertexColor(backdropColor:GetRGB())

	if (faction == "Alliance") then
		self.Shadow:SetTexCoord(0, 1, 0, 1)
		self.Shadow:ClearAndSetPoint("LEFT", self, "RIGHT", -8, -8)
		self.Text:ClearAndSetPoint("LEFT", self, "RIGHT", 16, 0)
	else
		self.Shadow:SetTexCoord(1, 0, 0, 1)
		self.Shadow:ClearAndSetPoint("RIGHT", self, "LEFT", 8, -8)
		self.Text:ClearAndSetPoint("RIGHT", self, "LEFT", -16, 0)
	end
end

function CharCreateRaceButtonMixin:SetRace(raceName, raceEnglishName, gender, enabled)
	raceName = raceName or ""
	raceEnglishName = raceEnglishName or ""
	gender = gender or "MALE"
	local coords = RACE_ICON_TCOORDS[raceEnglishName.."_"..gender] or RACE_ICON_TCOORDS["HUMAN_"..gender] or {0, 1, 0, 1}

	local iconFile = (CHAR_CREATE_COA_GENDER_ICONS and CHAR_CREATE_COA_GENDER_ICONS[string.upper(raceEnglishName) .. "_" .. gender]) or CHAR_CREATE_EXTRA_RACE_ICONS[raceName]
	if iconFile then
		if SetPortraitToTexture then
			-- crop the square icon to a circle like the built-in race icons
			SetPortraitToTexture(self:GetNormalTexture(), iconFile)
			SetPortraitToTexture(self:GetPushedTexture(), iconFile)
			coords = {0, 1, 0, 1}
		else
			self:SetNormalTexture(iconFile)
			self:SetPushedTexture(iconFile)
			self:GetNormalTexture():SetSize(40, 40)
			self:GetPushedTexture():SetSize(40, 40)
			coords = {0.1, 0.9, 0.1, 0.9}
		end
	else
		self:SetNormalTexture("Interface\\Glues\\CharacterCreate\\UI-CHARACTERCREATE-RACES_Round")
		self:SetPushedTexture("Interface\\Glues\\CharacterCreate\\UI-CHARACTERCREATE-RACES_Round")
	end
	self:GetNormalTexture():SetTexCoord(unpack(coords))
	self:GetPushedTexture():SetTexCoord(unpack(coords))

	self:SetText(raceName)
	self.tooltipText = {}

	self:SetEnabled(enabled)
	SetButtonDesaturated(self, not enabled)

	self.tooltipTitle = raceName
	if not enabled then
		local disabledReason = _G[raceEnglishName.."_".."DISABLED"]
		if disabledReason then
			tinsert(self.tooltipText, disabledReason)
		end
	end

	local abilityIndex = 1
	local text = GetRealmSpecificGlobalString("ABILITY_INFO_"..raceEnglishName..abilityIndex)
	local spell = CHAR_CREATE_PASSIVES[raceEnglishName] and CHAR_CREATE_PASSIVES[raceEnglishName][abilityIndex]

	if text and spell then
		local _, _, spellIconPath = GetSpellData(spell, false, true)
		text = "|T"..spellIconPath..":24:24:0:0|t"..text.."\n"
	end

	while text do
		tinsert(self.tooltipText, text)
		abilityIndex = abilityIndex + 1
        
		text = GetRealmSpecificGlobalString("ABILITY_INFO_"..raceEnglishName..abilityIndex)
		spell = CHAR_CREATE_PASSIVES[raceEnglishName] and CHAR_CREATE_PASSIVES[raceEnglishName][abilityIndex]

		if text and spell then
			local _, _, spellIconPath = GetSpellData(spell, false, true)
			text = "|T"..spellIconPath..":24:24:0:0|t"..text.."\n"
		end
	end

	tinsert(self.tooltipText, 1, ((CHAR_CREATE_RACE_LORE and CHAR_CREATE_RACE_LORE[CHAR_CREATE_LORE_FACTION or "alliance"] and CHAR_CREATE_RACE_LORE[CHAR_CREATE_LORE_FACTION or "alliance"][raceName]) or GetFlavorText("RACE_INFO_"..raceEnglishName, gender) or raceName or "").."\n\n")
end

function CharCreateRaceButtonMixin:SetText(text)
	self.Text:SetText(text)
end

CharCreateClassButtonMixin = CreateFromMixins(CharCreateButtonMixin)

function CharCreateClassButtonMixin:OnLoad()
	self:GetNormalTexture():SetDrawLayer("BACKGROUND")
	self:GetPushedTexture():SetDrawLayer("BACKGROUND")
	EventRegistry:RegisterCallback("CharacterCreate.PaidServiceActive", self.RefreshEnabled, self)
	EventRegistry:RegisterCallback("CharacterCreate.ClassChanged", function(self, classID)
		self:SetSelected(classID == self:GetID())
	end, self)

end

function CharCreateClassButtonMixin:RefreshEnabled()
	self:SetEnabled(not PAID_SERVICE_TYPE)
end

function CharCreateClassButtonMixin:OnEnable()
	self.Text:SetFontObject("GameFontNormal")
	self.Border:SetVertexColor(1, 1, 1, 1)
	self:GetNormalTexture():SetVertexColor(1, 1, 1, 1)
end

function CharCreateClassButtonMixin:OnDisable()
	self.Text:SetFontObject("GameFontDisable")
	self.Border:SetVertexColor(0.5, 0.5, 0.5, 1)
	self:GetNormalTexture():SetVertexColor(0.5, 0.5, 0.5, 1)
end

function CharCreateClassButtonMixin:SetClassGuideCombatHealer(isCombatHealer)
	self.classGuideCombatHealer = isCombatHealer and true or false

	if not self.ClassGuideCombatHealerBadge then
		local badge = CreateFrame("Button", nil, self)
		badge:SetSize(38, 38)
		badge:SetPoint("BOTTOMRIGHT", self, "BOTTOMRIGHT", 11, -11)
		badge:SetFrameLevel(self:GetFrameLevel() + 3)
		badge:SetScript("OnClick", function(badgeSelf)
			badgeSelf:GetParent():Click()
		end)
		badge:SetScript("OnEnter", function(badgeSelf)
			GlueTooltip:SetOwner(badgeSelf, "ANCHOR_TOP")
			GlueTooltip:AddLine(CLASS_GUIDE_COMBAT_HEALER_TOOLTIP_TITLE, 1, 1, 1, 1, true)
			GlueTooltip:AddLine(CLASS_GUIDE_COMBAT_HEALER_TOOLTIP_TEXT, 1, 0.82, 0, 1, true)
			GlueTooltip:Show()
		end)
		badge:SetScript("OnLeave", function()
			GlueTooltip:Hide()
		end)

		local icon = badge:CreateTexture(nil, "OVERLAY")
		icon:SetAllPoints()
		icon:SetAtlas(CharacterCreate_GetClassGuideAtlas(CLASS_GUIDE_COMBAT_HEALER_ATLAS))
		badge.Icon = icon
		self.ClassGuideCombatHealerBadge = badge
	end

	self.ClassGuideCombatHealerBadge:SetShown(self.classGuideCombatHealer)
end

function CharCreateClassButtonMixin:OnClick()
	CharacterClass_OnClick(self, self:GetID())

	if CharacterCreate.classGuideActive then
		CharacterCreate.selectedClassGuideClassID = self:GetID()
		CharCreate_EnableNextButton(true)
		CharacterCreate_UpdateClassGuideClassButtonSelection(CharacterCreate)
	end
end

local function WrapResourceTooltipText(resource, text)
	local color = POWER_COLORS and POWER_COLORS[resource]
	if color then
		return color:WrapText(text)
	end

	return text
end

function CharCreateClassButtonMixin:OnEnter()
	GlueTooltip:SetOwner(self, "ANCHOR_TOP")
	GlueTooltip:SetText(self.tooltipTitle)
	GlueTooltip:SetMinWidth(340)

	-- Role
	do
		local roles = ClassInfoUtil.GetClassRoles(self.class)

		if roles.MeleeDPS then
			GlueTooltip:AddLine(ROLE_TEXT_WITH_ICON.MELEE_DAMAGER)
		end

		if roles.RangedDPS then
			GlueTooltip:AddLine(ROLE_TEXT_WITH_ICON.RANGED_DAMAGER)
		end

		if roles.CasterDPS then
			GlueTooltip:AddLine(ROLE_TEXT_WITH_ICON.CASTER_DAMAGER)
		end

		if roles.Tank then
			GlueTooltip:AddLine(ROLE_TEXT_WITH_ICON.TANK)
		end

		if roles.Healer then
			GlueTooltip:AddLine(ROLE_TEXT_WITH_ICON.HEALER)
		end

		if roles.Support then
			GlueTooltip:AddLine(ROLE_TEXT_WITH_ICON.SUPPORT)
		end

		GlueTooltip:AddLine(" ")
	end

	if self.classGuideCombatHealer then
		GlueTooltip:AddLine(CLASS_GUIDE_COMBAT_HEALER_TOOLTIP_TITLE, 1, 0.82, 0, 1, true)
		GlueTooltip:AddLine(CLASS_GUIDE_COMBAT_HEALER_TOOLTIP_TEXT, 1, 1, 1, 1, true)
		GlueTooltip:AddLine(" ")
	end

	-- Primary
	local hasResource = false
	do
		local resources = ClassInfoUtil.GetClassPrimaryResources(self.class)
		local string
		for _, resource in ipairs(resources) do
			local text = _G[resource]
			if text then
				if string then
					string = string.." / "
				else
					string = ""
				end
				text = WrapResourceTooltipText(resource, text)
				string = string..text
			end
		end

		if string then
			hasResource = true
			GlueTooltip:AddLine(PRIMARY_RESOURCE:format(string), 1, 0.82, 0, 1, true)
		end
	end

	-- Secondary
	do
		local resources = ClassInfoUtil.GetClassSecondaryResources(self.class)
		local string
		for _, resource in ipairs(resources) do
			local text = _G[resource]
			if text then
				if string then
					string = string.." / "
				else
					string = ""
				end
				text = WrapResourceTooltipText(resource, text)
				string = string..text
			end
		end

		if string then
			hasResource = true
			GlueTooltip:AddLine(SECONDARY_RESOURCE:format(string), 1, 0.82, 0, 1, true)
		end
	end

	-- Special Perks
	do
		local combined
		local i = 1
		while true do
			local perk = _G["CLASS_SPECIAL_PERKS_"..self.class..i]
			if not perk then
				break
			end
			if combined then
				combined = combined..", "..perk
			else
				combined = perk
			end
			i = i + 1
		end

		if combined then
			hasResource = true
			GlueTooltip:AddLine(CLASS_SPECIAL_PERKS:format(combined), 1, 1, 1, 1, true)
		end
	end

	if hasResource then
		GlueTooltip:AddLine(" ")
	end

	-- Armor Proficiency
	do
		local armorTypes = ClassInfoUtil.GetClassArmorProficiency(self.class)

		if armorTypes.Plate then
			GlueTooltip:AddLine(ARMOR_TYPE_WITH_ICON.PLATE, 1, 1, 1, 1, true)
		end

		if armorTypes.Mail then
			GlueTooltip:AddLine(ARMOR_TYPE_WITH_ICON.MAIL, 1, 1, 1, 1, true)
		end

		if armorTypes.Leather then
			GlueTooltip:AddLine(ARMOR_TYPE_WITH_ICON.LEATHER, 1, 1, 1, 1, true)
		end

		if armorTypes.Cloth then
			GlueTooltip:AddLine(ARMOR_TYPE_WITH_ICON.CLOTH, 1, 1, 1, 1, true)
		end

		GlueTooltip:AddLine(" ")
	end

	-- Combat Style
	do
		local text = _G["CLASS_COMBAT_STYLE_"..self.class.."0"]
		if text then
			GlueTooltip:AddLine(COMBAT_STYLE, 1, 0.82, 0)

			local i = 0
			while text do
				GlueTooltip:AddLine("- " .. text, 1, 1, 1, 1, true)
				i = i + 1
				text = _G["CLASS_COMBAT_STYLE_"..self.class..i]
			end
		end
	end

	GlueTooltip:AddLine(" ")
	GlueTooltip:AddLine(GetFlavorText("CLASS_"..self.class, "MALE"), 1, 0.82, 0, 1, true)
	GlueTooltip:Show()
	
	self:HighlightValidRaces()
end

function CharCreateClassButtonMixin:OnLeave()
	CharCreateButtonMixin.OnLeave(self)
	self:StopHighlightingRaces()
end

function CharCreateClassButtonMixin:HighlightValidRaces()
	for i = 1, 10 do
		local button = _G["CharCreateRaceButton"..i]
		if button:IsEnabled() then
			if IsRaceClassValid(button:GetID(), self:GetID()) then
				button.Glow.Anim:Play()
			else
				button.Glow.Anim:Stop()
			end
		end
	end
end

function CharCreateClassButtonMixin:StopHighlightingRaces()
	for i = 1, 10 do
		local button = _G["CharCreateRaceButton"..i]
		button.Glow.Anim:Stop()
	end
end

function CharCreateClassButtonMixin:SetText(text)
	self.Text:SetText(text)
end

function CharCreateClassButtonMixin:SetSelected(selected)
	self:SetChecked(selected)
end

function CharCreateClassButtonMixin:SetClass(className, classFileName, enabled)
	self.class = classFileName
	local coords = CLASS_ICON_TCOORDS_ROUND[classFileName]
	local classColor = RAID_CLASS_COLORS[classFileName] or HIGHLIGHT_FONT_COLOR
	
	enabled = enabled and CharacterCreateUtil.IsRaceClassEnabledAndValid(CharacterCreate.selectedRace, self:GetID()) and not PAID_SERVICE_TYPE

	self:GetNormalTexture():SetTexCoord(unpack(coords))
	self:GetPushedTexture():SetTexCoord(unpack(coords))
	
	self:SetText(className:gsub(" ([^ ]*)$", "\n%1", 1))
	local textColor = enabled and NORMAL_FONT_COLOR or DISABLED_FONT_COLOR
	self.Text:SetTextColor(textColor:GetRGB())

	self.tooltipTitle = classColor:WrapText(className)

	self:SetEnabled(enabled)
	SetButtonDesaturated(self, not enabled)
	self.DisableTexture:SetShown(not enabled)
	self:SetClassGuideCombatHealer(false)
	self:SetSelected(CharacterCreate.selectedClass == self:GetID())
end

-- Local server: Murloc armor is a human-layout paint; Grummle has no player armor geosets.
-- Class outfits (Ranger cloak/shoulders) sample missing textures (checkerboard) and can crash.
local function CharacterCreate_IsMurlocSelected()
	local _, fileString = GetNameForRace()
	return fileString == "Troglodyte"
end
local function CharacterCreate_KeepUndressed()
	local _, fileString = GetNameForRace()
	return fileString == "Troglodyte"
end
do
	local dress, previewClassOutfit = C_CharacterCreate.Dress, C_CharacterCreate.PreviewClassOutfit
	C_CharacterCreate.Dress = function(...)
		if CharacterCreate_KeepUndressed() then return C_CharacterCreate.Undress() end
		return dress(...)
	end
	C_CharacterCreate.PreviewClassOutfit = function(...)
		if CharacterCreate_KeepUndressed() then return C_CharacterCreate.Undress() end
		-- Ranger outfit: its cloak/shoulder model has no fitted version for the Esteria draconic races (error cube)
		local _, raceFile = GetNameForRace()
		local lowRace = strlower(tostring(raceFile or ""))
		local _, selClass = GetSelectedClass()
		-- every race: the Ranger preview outfit shows an error cube on one shoulder or no armor at all
		if true
			and (strupper(tostring((...) or "")) == "RANGER" or strupper(tostring(selClass or "")) == "RANGER") then
			return C_CharacterCreate.Undress()
		end
		return previewClassOutfit(...)
	end
	local raceOnClick = CharacterRace_OnClick
	function CharacterRace_OnClick(...)
		raceOnClick(...)
		if CharacterCreate_KeepUndressed() then C_CharacterCreate.Undress() end
	end
end

-- Local server: Murloc armor option
HAIR_MURLOCARMOR_STYLE = "Armor"
do
	local updateHair = CharacterCreate_UpdateHairCustomization
	function CharacterCreate_UpdateHairCustomization(...)
		updateHair(...)
		if CharacterCreate_IsMurlocSelected() and GetSelectedSex() ~= SEX_MALE then
			CharCreateCustomizationButton3:Hide()
		end
	end
end

-- Local server: bonus racial of the extra races (race id = {source race, ability index, spell name})
CHAR_CREATE_EXTRA_RACE_SIGNATURE, CHAR_CREATE_EXTRA_RACE_SIGNATURE_HORDE = {}, {}
CHAR_CREATE_RACE_BONUS = {
	[9] = {"GNOME", 1, "Escape Artist"}, [12] = {"DRAENEI", 2, "Gift of the Naaru"}, [13] = {"TROLL", 1, "Berserking"},
	[14] = {"HUMAN", 5, "Every Man for Himself"}, [15] = {"TAUREN", 1, "War Stomp"}, [16] = {"DWARF", 1, "Stoneform"},
	[17] = {"DWARF", 3, "Frost Resistance"}, [18] = {"ORC", 1, "Blood Fury"}, [19] = {"GNOME", 1, "Escape Artist"},
	[20] = {"DRAENEI", 2, "Gift of the Naaru"}, [21] = {"BLOODELF", 3, "Arcane Torrent"}, [22] = {"SCOURGE", 1, "Will of the Forsaken"},
	[23] = {"BLOODELF", 3, "Arcane Torrent"}, [24] = {"NIGHTELF", 1, "Shadowmeld"}, [25] = {"DWARF", 1, "Stoneform"},
	[26] = {"DWARF", 1, "Stoneform"}, [27] = {"TAUREN", 1, "War Stomp"}, [28] = {"ORC", 1, "Blood Fury"},
	[29] = {"TAUREN", 1, "War Stomp"}, [30] = {"NIGHTELF", 1, "Shadowmeld"}, [31] = {"GNOME", 1, "Escape Artist"},
}
-- The buttons are numbered by their place in GetAvailableRaces(), which is not the race id (CoA Bots 1.8 lists the
-- Alliance races first), so each button finds its race id by name. Pandaren and Murloc exist twice: the Alliance one
-- (Pandaren 29, Murloc 30) comes first.
CHAR_CREATE_RACE_IDS = {
	["Human"] = {1}, ["Orc"] = {2}, ["Dwarf"] = {3}, ["Night Elf"] = {4}, ["Undead"] = {5}, ["Tauren"] = {6},
	["Gnome"] = {7}, ["Troll"] = {8}, ["Goblin"] = {9}, ["Blood Elf"] = {10}, ["Draenei"] = {11},
	["Zandalari Troll"] = {12}, ["Worgen"] = {13}, ["High Elf"] = {14}, ["Tuskarr"] = {15}, ["Kul Tiran"] = {16},
	["Taunka"] = {17}, ["Vrykul"] = {18}, ["Vulpera"] = {19}, ["Pandaren"] = {29, 20}, ["Naga"] = {21},
	["Broken"] = {22}, ["Fel Orc"] = {23}, ["Forest Troll"] = {24}, ["Ice Troll"] = {25}, ["Skeleton"] = {26},
	["Earthen"] = {27}, ["Drakkari Troll"] = {28}, ["Murloc"] = {30, 31},
	["Forgotten"] = {32},
}
CHAR_CREATE_RACE_BONUS_FROM = {
	GNOME = "Gnome", DRAENEI = "Draenei", TROLL = "Troll", HUMAN = "Human", TAUREN = "Tauren", DWARF = "Dwarf",
	ORC = "Orc", BLOODELF = "Blood Elf", SCOURGE = "Undead", NIGHTELF = "Night Elf",
}

function CharacterCreate_RaceBonusText(raceID, withIcon)
	local bonus = raceID and CHAR_CREATE_RACE_BONUS[raceID]
	if not bonus then
		return nil
	end
	local source, index, name = bonus[1], bonus[2], bonus[3]
	local text = "|cff00ff96Bonus racial (" .. (CHAR_CREATE_RACE_BONUS_FROM[source] or source) .. "): " .. name .. "|r"
	local description = _G["ABILITY_INFO_" .. source .. index]
	if description then
		text = text .. "|n" .. description
	end
	local spell = CHAR_CREATE_PASSIVES and CHAR_CREATE_PASSIVES[source] and CHAR_CREATE_PASSIVES[source][index]
	if withIcon and spell and GetSpellData then
		local ok, _, _, icon = pcall(GetSpellData, spell, false, true)
		if ok and icon then
			text = "|T" .. icon .. ":24:24:0:0|t" .. text
		end
	end
	return text
end

do
	local setRace = CharCreateRaceButtonMixin.SetRace
	function CharCreateRaceButtonMixin:SetRace(...)
		setRace(self, ...)
		local raceName = ...
		if self:GetID() == 1 then
			CharacterCreate.bonusRaceSeen = {}
		end
		local seen = CharacterCreate.bonusRaceSeen or {}
		CharacterCreate.bonusRaceSeen = seen
		local ids = raceName and CHAR_CREATE_RACE_IDS[raceName]
		seen[raceName or ""] = (seen[raceName or ""] or 0) + 1
		self.bonusRaceID = ids and ids[seen[raceName]]
		local text = CharacterCreate_RaceBonusText(self.bonusRaceID, true)
		if text and self.tooltipText then
			tinsert(self.tooltipText, text .. "\n")
		end
	end
	local setCharacterRace = SetCharacterRace
	function SetCharacterRace(id, ...)
		setCharacterRace(id, ...)
		local button = _G["CharCreateRaceButton" .. (id or CharacterCreate.selectedRace or 0)]
		local text = CharacterCreate_RaceBonusText(button and button.bonusRaceID)
		local bullets = CharCreateRaceInfoFrame and CharCreateRaceInfoFrame.scrollFrame.scrollChild.bulletText
		if text and bullets then
			bullets:SetText((bullets:GetText() or "") .. text .. "\n\n")
		end
	end
end

-- Local server: Eunoia races: icons of the new race names
for name, icon in pairs({
	["Dracthyr"] = "Interface\\Icons\\INV_Misc_Head_Dragon_Bronze",
	["Ogre"] = "Interface\\Icons\\Achievement_Boss_GruulTheDragonkiller",
	["Dark Iron Dwarf"] = "Interface\\Icons\\INV_Misc_Head_Dwarf_01",
	["Illidari Night Elf"] = "Interface\\Icons\\Achievement_Boss_Illidan",
	["Illidari Blood Elf"] = "Interface\\Icons\\Achievement_Boss_Illidan",
	["Lightforged Draenei"] = "Interface\\Icons\\Spell_Holy_SurgeOfLight",
	["Void Elf"] = "Interface\\Icons\\Spell_Shadow_ShadowWordPain",
	["Eredar"] = "Interface\\Icons\\Achievement_Boss_Archimonde",
	["Nightborne"] = "Interface\\Icons\\Spell_Arcane_StarFire",
}) do
	if CHAR_CREATE_EXTRA_RACE_ICONS then CHAR_CREATE_EXTRA_RACE_ICONS[name] = icon end
end

-- Local server: extra races
CHAR_CREATE_RACE_IDS["Alpha Worgen"] = {47}
CHAR_CREATE_RACE_IDS["Alpha Goblin"] = {57}
CHAR_CREATE_RACE_IDS["Furbolg"] = {50}
if CHAR_CREATE_EXTRA_RACE_ICONS then
	CHAR_CREATE_EXTRA_RACE_ICONS["Alpha Worgen"] = "Interface\\Icons\\Ability_Mount_WhiteDireWolf"
	CHAR_CREATE_EXTRA_RACE_ICONS["Alpha Goblin"] = "Interface\\Icons\\INV_Misc_Bomb_04"
	CHAR_CREATE_EXTRA_RACE_ICONS["Furbolg"] = "Interface\\Icons\\Ability_Racial_BearForm"
end

-- Local server: extra races (2)
CHAR_CREATE_RACE_IDS["Alpha Worgen"] = {47}
CHAR_CREATE_RACE_IDS["Alpha Goblin"] = {57}
CHAR_CREATE_RACE_IDS["Furbolg"] = {50}
CHAR_CREATE_RACE_IDS["Forgotten"] = {32}
if CHAR_CREATE_MALE_ONLY_RACES then CHAR_CREATE_MALE_ONLY_RACES["Forgotten"] = true end
if CHAR_CREATE_MALE_ONLY_RACES then CHAR_CREATE_MALE_ONLY_RACES["Grummle"] = true end
if CHAR_CREATE_EXTRA_RACE_ICONS then
	CHAR_CREATE_EXTRA_RACE_ICONS["Alpha Worgen"] = "Interface\\Icons\\Ability_Mount_WhiteDireWolf"
	CHAR_CREATE_EXTRA_RACE_ICONS["Alpha Goblin"] = "Interface\\Icons\\INV_Misc_Bomb_04"
	CHAR_CREATE_EXTRA_RACE_ICONS["Furbolg"] = "Interface\\Icons\\Ability_Racial_BearForm"
	CHAR_CREATE_EXTRA_RACE_ICONS["Forgotten"] = "Interface\\Icons\\Achievement_Character_Human_Male"
end

-- Local server: extra races (3)
CHAR_CREATE_RACE_IDS["Alpha Worgen"] = {47}
CHAR_CREATE_RACE_IDS["Alpha Goblin"] = {57}
CHAR_CREATE_RACE_IDS["Furbolg"] = {50}
CHAR_CREATE_RACE_IDS["Forgotten"] = {32}
if CHAR_CREATE_MALE_ONLY_RACES then CHAR_CREATE_MALE_ONLY_RACES["Forgotten"] = true end
if CHAR_CREATE_EXTRA_RACE_ICONS then
	CHAR_CREATE_EXTRA_RACE_ICONS["Alpha Worgen"] = "Interface\\Icons\\Ability_Mount_WhiteDireWolf"
	CHAR_CREATE_EXTRA_RACE_ICONS["Alpha Goblin"] = "Interface\\Icons\\INV_Misc_Bomb_04"
	CHAR_CREATE_EXTRA_RACE_ICONS["Furbolg"] = "Interface\\Icons\\Ability_Racial_BearForm"
	CHAR_CREATE_EXTRA_RACE_ICONS["Forgotten"] = "Interface\\Icons\\Achievement_Character_Human_Male"
end

-- Local server: recomposite the head once the textures are loaded
COA_RECOMPOSE = { timer = nil, shots = 0 }
do
	local frame = CreateFrame("Frame")
	frame:SetScript("OnUpdate", function(self, elapsed)
		local r = COA_RECOMPOSE
		if not r.timer then return end
		r.timer = r.timer - elapsed
		if r.timer > 0 then return end
		if r.shots > 0 then
			r.shots = r.shots - 1
			r.timer = (r.shots > 0) and 0.9 or nil
			if CharacterCreateFrame and CharacterCreateFrame:IsShown() and C_CharacterCreate and C_CharacterCreate.CycleCustomizationSkipLocked then
				pcall(function()
					C_CharacterCreate.CycleCustomizationSkipLocked(1, 1)
					C_CharacterCreate.CycleCustomizationSkipLocked(1, -1)
				end)
			end
		else
			r.timer = nil
		end
	end)
	local function arm()
		COA_RECOMPOSE.timer = 0.5
		COA_RECOMPOSE.shots = 3
	end
	local oldRace = SetCharacterRace
	function SetCharacterRace(...)
		local a, b, c, d, e = oldRace(...)
		arm()
		return a, b, c, d, e
	end
	local oldSex = SetSelectedSex
	if oldSex then
		function SetSelectedSex(...)
			local a, b, c = oldSex(...)
			arm()
			return a, b, c
		end
	end
end

-- Local server: Medviten races
CHAR_CREATE_RACE_IDS["Dark Iron Dwarf"] = {48}
CHAR_CREATE_RACE_IDS["Mag'har Orc"] = {49}
CHAR_CREATE_RACE_IDS["Ogre"] = {53}
if CHAR_CREATE_EXTRA_RACE_ICONS then
	CHAR_CREATE_EXTRA_RACE_ICONS["Dark Iron Dwarf"] = "Interface\\Icons\\INV_Misc_Head_Dwarf_01"
	CHAR_CREATE_EXTRA_RACE_ICONS["Mag'har Orc"] = "Interface\\Icons\\Achievement_Character_Orc_Male"
	CHAR_CREATE_EXTRA_RACE_ICONS["Ogre"] = "Interface\\Icons\\Achievement_Boss_GruulTheDragonkiller"
end

-- Local server: camera for oversized races
CHAR_CREATE_BIG_RACES = { ["FacelessHorde"] = 1.5, ["QirajiHorde"] = 1.5, ["Vrykul"] = 1.65, ["KyrianAlliance"] = 1.75, ["VenthyrHorde"] = 1.2, ["CentaurAlliance"] = 1.5, ["CentaurHorde"] = 1.5, ["HighmountainTauren"] = 1.2 }
do
	local f = CreateFrame("Frame")
	local pending
	f:SetScript("OnUpdate", function(self, elapsed)
		if not pending then return end
		pending = pending - elapsed
		if pending > 0 then return end
		pending = nil
		local ok, _, fileString = pcall(GetNameForRace)
		local k = ok and fileString and CHAR_CREATE_BIG_RACES[fileString]
		if k and C_CharacterCreate and C_CharacterCreate.GetCameraPosition then
			pcall(function()
				local x, y, z = C_CharacterCreate.GetCameraPosition()
				C_CharacterCreate.SetCameraPosition(x * k, y * k, z * (1 + (k - 1) * 0.6))
			end)
		end
	end)
	local old = SetCharacterRace
	function SetCharacterRace(...)
		local a, b, c, d, e = old(...)
		pending = 0.6
		return a, b, c, d, e
	end
end

-- Local server: Undead Gnome
CHAR_CREATE_RACE_IDS["Undead Gnome"] = {51}
if CHAR_CREATE_EXTRA_RACE_ICONS then CHAR_CREATE_EXTRA_RACE_ICONS["Undead Gnome"] = "Interface\\Icons\\Achievement_Character_Gnome_Male" end

-- Local server: creature races
CHAR_CREATE_RACE_IDS["Vampyr"] = {54}
if CHAR_CREATE_EXTRA_RACE_ICONS then CHAR_CREATE_EXTRA_RACE_ICONS["Vampyr"] = "Interface\\Icons\\Spell_Shadow_VampiricAura" end
CHAR_CREATE_RACE_IDS["Eredar"] = {55}
if CHAR_CREATE_EXTRA_RACE_ICONS then CHAR_CREATE_EXTRA_RACE_ICONS["Eredar"] = "Interface\\Icons\\Spell_Fire_FelFlameRing" end
CHAR_CREATE_RACE_IDS["Harpy"] = {56}
if CHAR_CREATE_EXTRA_RACE_ICONS then CHAR_CREATE_EXTRA_RACE_ICONS["Harpy"] = "Interface\\Icons\\INV_Feather_04" end

-- Local server: Eunoia races (back)
CHAR_CREATE_RACE_IDS["Dracthyr"] = {54, 58}
if CHAR_CREATE_EXTRA_RACE_ICONS then CHAR_CREATE_EXTRA_RACE_ICONS["Dracthyr"] = "Interface\\Icons\\INV_Misc_Head_Dragon_Bronze" end
CHAR_CREATE_RACE_IDS["Void Elf"] = {59}
if CHAR_CREATE_EXTRA_RACE_ICONS then CHAR_CREATE_EXTRA_RACE_ICONS["Void Elf"] = "Interface\\Icons\\Spell_Shadow_ShadowWordPain" end
CHAR_CREATE_RACE_IDS["Illidari Night Elf"] = {61}
if CHAR_CREATE_EXTRA_RACE_ICONS then CHAR_CREATE_EXTRA_RACE_ICONS["Illidari Night Elf"] = "Interface\\Icons\\Achievement_Boss_Illidan" end
CHAR_CREATE_RACE_IDS["Illidari Blood Elf"] = {60}
if CHAR_CREATE_EXTRA_RACE_ICONS then CHAR_CREATE_EXTRA_RACE_ICONS["Illidari Blood Elf"] = "Interface\\Icons\\Achievement_Boss_Illidan" end
CHAR_CREATE_RACE_IDS["Lightforged Draenei"] = {62}
if CHAR_CREATE_EXTRA_RACE_ICONS then CHAR_CREATE_EXTRA_RACE_ICONS["Lightforged Draenei"] = "Interface\\Icons\\Spell_Holy_SurgeOfLight" end
CHAR_CREATE_RACE_IDS["Nightborne"] = {63}
if CHAR_CREATE_EXTRA_RACE_ICONS then CHAR_CREATE_EXTRA_RACE_ICONS["Nightborne"] = "Interface\\Icons\\Spell_Arcane_StarFire" end

-- Local server: race ids above 63 (test)
CHAR_CREATE_RACE_IDS["Test Race"] = {65}

-- Local server: Esteria races

-- Local server: Esteria races (2)
CHAR_CREATE_RACE_IDS["Mechagnome"] = {67}
if CHAR_CREATE_EXTRA_RACE_ICONS then CHAR_CREATE_EXTRA_RACE_ICONS["Mechagnome"] = "Interface\\Icons\\INV_Misc_Gear_08" end

-- Local server: Esteria native races
CHAR_CREATE_RACE_IDS["Highmountain Tauren"] = {66}
if CHAR_CREATE_EXTRA_RACE_ICONS then CHAR_CREATE_EXTRA_RACE_ICONS["Highmountain Tauren"] = "Interface\\Icons\\Achievement_Character_Tauren_Male" end
CHAR_CREATE_RACE_IDS["Earthen"] = {68, 69}
CHAR_CREATE_RACE_IDS["Haranir"] = {71, 70}
if CHAR_CREATE_EXTRA_RACE_ICONS then CHAR_CREATE_EXTRA_RACE_ICONS["Haranir"] = "Interface\\Icons\\INV_Misc_Herb_19" end
CHAR_CREATE_RACE_IDS["Vulpera"] = {74}





























































































































































-- Local server: bonus racial of the races above 32
if CHAR_CREATE_RACE_BONUS then
	CHAR_CREATE_RACE_BONUS[32] = {"NIGHTELF", 1, "Shadowmeld"}
	CHAR_CREATE_RACE_BONUS[47] = {"ORC", 1, "Blood Fury"}
	CHAR_CREATE_RACE_BONUS[48] = {"ORC", 1, "Blood Fury"}
	CHAR_CREATE_RACE_BONUS[49] = {"TROLL", 1, "Berserking"}
	CHAR_CREATE_RACE_BONUS[50] = {"NIGHTELF", 1, "Shadowmeld"}
	CHAR_CREATE_RACE_BONUS[51] = {"GNOME", 1, "Escape Artist"}
	CHAR_CREATE_RACE_BONUS[53] = {"TAUREN", 1, "War Stomp"}
	CHAR_CREATE_RACE_BONUS[54] = {"TAUREN", 1, "War Stomp"}
	CHAR_CREATE_RACE_BONUS[55] = {"BLOODELF", 3, "Arcane Torrent"}
	CHAR_CREATE_RACE_BONUS[56] = {"GNOME", 1, "Escape Artist"}
	CHAR_CREATE_RACE_BONUS[57] = {"GNOME", 1, "Escape Artist"}
	CHAR_CREATE_RACE_BONUS[58] = {"TAUREN", 1, "War Stomp"}
	CHAR_CREATE_RACE_BONUS[59] = {"NIGHTELF", 1, "Shadowmeld"}
	CHAR_CREATE_RACE_BONUS[60] = {"TROLL", 1, "Berserking"}
	CHAR_CREATE_RACE_BONUS[61] = {"BLOODELF", 3, "Arcane Torrent"}
	CHAR_CREATE_RACE_BONUS[62] = {"HUMAN", 5, "Every Man for Himself"}
	CHAR_CREATE_RACE_BONUS[63] = {"BLOODELF", 3, "Arcane Torrent"}
	CHAR_CREATE_RACE_BONUS[66] = {"DWARF", 1, "Stoneform"}
	CHAR_CREATE_RACE_BONUS[67] = {"DWARF", 1, "Stoneform"}
	CHAR_CREATE_RACE_BONUS[68] = {"TAUREN", 1, "War Stomp"}
	CHAR_CREATE_RACE_BONUS[69] = {"TAUREN", 1, "War Stomp"}
	CHAR_CREATE_RACE_BONUS[70] = {"TROLL", 1, "Berserking"}
	CHAR_CREATE_RACE_BONUS[71] = {"TROLL", 1, "Berserking"}
	CHAR_CREATE_RACE_BONUS[74] = {"GNOME", 1, "Escape Artist"}
end
-- end bonus racial of the races above 32

-- Local server: CoA racials of the added races
-- CHAR_CREATE_COA_RACIALS (gen_coa_racials.py): race id -> { {name, icon, "active"/"passive", text} x4 }.
-- The racial list of an added race (12 and up) shows its four CoA racials instead of the borrowed ones.
-- The race buttons get their functions copied when they are made (before this runs), so hooking the button mixin
-- did nothing: the tooltips are filled from here instead, by race name (twin races share their racials).
CHAR_CREATE_COA_RACIALS = {
	[12] = {{"Regeneratin'", "ability_racial_regeneratin", "active", "Channel for 6 sec to restore 50% of your maximum health. Direct damage interrupts the channel; periodic damage does not."}, {"City of Gold", "ability_racial_cityofgold", "passive", "Gold found on creatures increased by 2%."}, {"Pterrordax Swoop", "ability_racial_pterrordaxswoop", "active", "Spread your arms and glide, slowing your fall for 20 sec."}, {"Embrace of the Loa", "Spell_Holy_FlashHeal", "active", "Choose which Loa you embrace, gaining its boon. You can change your Loa once every 5 days."}},
	[13] = {{"Darkflight", "ability_worgen_darkflight", "active", "The worgen curse frees those who embrace the beast, as Rangers and Reapers know. Escapes any immobilization or movement slowing effect."}, {"Two Forms", "ability_racial_twoforms", "active", "Between the man and the beast, fear finds no hold. Removes Charm, Fear and Sleep effects."}, {"Viciousness", "ability_racial_viciousness", "passive", "Gilneas hunts with the instincts of the wolf. Increases chance to hit by 1%."}, {"Aberration", "ability_racial_runningwild", "passive", "The curse hardens body and blood. Reduces Nature damage taken by 4%."}, {"Two Forms", "ability_racial_viciousness", "active", "Switch between your human and worgen forms."}},
	[14] = {{"Arcane Affinity", "spell_holy_mindvision", "passive", "May restore mana, energy, runic power, or rage."}, {"Arcane Acuity", "spell_shadow_charm", "passive", "Agility increased by 2%."}, {"Bow Specialization", "inv_weapon_bow_07", "passive", "Increased critical chance with Bows."}, {"Enchanting", "trade_engraving", "active", "Enchanting skill increased."}},
	[15] = {{"Harpoon Toss", "Ability_Throw", "active", "Hurl a harpoon up to 30 yards, dealing Physical damage and slowing the target by 50% for 6 sec."}, {"Thick Blubber", "Spell_Nature_WispSplode", "passive", "Stamina increased by 1% and Frost damage taken reduced by 1%."}, {"Snowstrider", "Spell_Nature_WispSplode", "passive", "Duration of movement-slowing effects on you reduced by 10%."}, {"Master Angler", "Trade_Fishing", "passive", "Fishing skill increased by 15."}, {"Hearty Meals", "INV_Misc_Fork&Knife", "passive", "Well Fed bonuses are 10% stronger."}},
	[16] = {{"Haymaker", "ability_racial_haymaker", "active", "Kul Tiran sailors settle things with their fists, a habit Barbarians admire. Stuns nearby enemies."}, {"Brush It Off", "ability_racial_brushitoff", "passive", "A Kul Tiran shrugs off what would drop anyone else. Base health increased by 5%."}, {"Child of the Sea", "ability_racial_childofthesea", "passive", "Raised on the waves of Boralus. Breath lasts much longer underwater."}, {"Rime of the Ancient Mariner", "ability_racial_rimeoftheancientmariner", "passive", "Cold salt spray never bothers them. Reduces Frost damage taken by 4%."}},
	[17] = {{"Thunderhoof", "ability_racial_bullrush", "active", "The Taunka shake the tundra like their southern cousins. Stuns nearby enemies."}, {"Hardened by Northrend", "spell_frost_frostward", "passive", "Generations of ice and wind. Reduces Frost damage taken by 4%."}, {"Taunka Endurance", "ability_racial_bullrush", "passive", "Built to survive the Borean winter. Base health increased by 5%."}, {"Hunter of the Tundra", "ability_hunter_pet_bear", "passive", "The beasts of Northrend are their prey. Damage against beasts increased by 5%."}},
	[18] = {{"Runic Warcry", "Ability_Warrior_BattleShout", "active", "Increases melee, ranged and spell haste by 10% for 10 sec."}, {"Giant's Blood", "Spell_Nature_UnyeildingStamina", "passive", "Strength and Stamina increased by 1%."}, {"Runic Heritage", "Ability_Warrior_BattleShout", "passive", "Critical strike chance increased by 1%."}, {"Iron Will", "Spell_Nature_UnyeildingStamina", "passive", "Duration of stun effects on you reduced by 10%."}, {"Runecarver", "Trade_BlackSmithing", "passive", "Blacksmithing skill increased by 5."}},
	[20] = {{"Quaking Palm", "pandarenracial_quiveringpain", "active", "Incapacitates an enemy for 4 sec. Damage may break the effect."}, {"Epicurean", "pandarenracial_epicurean", "passive", "Well Fed stat bonuses are doubled."}, {"Gourmand", "pandarenracial_gourmand", "passive", "Cooking skill increased by 5."}, {"Bouncy", "pandarenracial_bouncy", "passive", "Fall damage reduced by roughly half."}, {"Inner Peace", "INV_Misc_Fork&Knife", "passive", "You earn twice as much rested experience."}},
	[21] = {{"Coiling Grasp", "Spell_Nature_StrangleVines", "active", "Immobilizes the target for 2 sec."}, {"Scaled Hide", "Spell_Shadow_UnholyStrength", "passive", "Armor from equipped items increased by 2%."}, {"Highborne Legacy", "Spell_Nature_EnchantArmor", "passive", "Magical damage dealt increased by 1%."}, {"Children of the Depths", "Ability_Racial_ShadowMeld", "passive", "Swim speed increased by 50% and you can breathe underwater indefinitely."}, {"Serpentine Weaponry", "INV_Axe_02", "passive", "Expertise with swords and polearms increased by 3."}},
	[22] = {{"Salvager", "trade_engineering", "passive", "Engineering and Mining skill increased by 10, and repair costs reduced by 10%."}, {"Krokul Cunning", "ability_hunter_misdirection", "passive", "Reduces the radius at which enemies detect you by 5 yards."}, {"Fel-Scarred", "spell_shadow_antimagicshell", "passive", "Magic effects that would drain your mana are 10% less effective, and Shadow resistance increased by 10."}, {"Echo of the Naaru", "spell_holy_holyprotection", "active", "Restore 15% maximum health over 10 seconds. 3 minute cooldown."}},
	[23] = {{"Fel Rage", "spell_shadow_demonform", "active", "Fel blood burns in their veins, a fire every Felsworn knows. Increases attack power and spell power for a short time."}, {"Demonic Endurance", "spell_shadow_ritualofsacrifice", "passive", "Fel-forged bodies shrug off blows that stun others. Stun effects last 15% less."}, {"Fel-Hardened", "ability_racial_demonbane", "passive", "The fel already took what it could. Reduces Shadow damage taken by 4%."}, {"Axe of the Horde", "achievement_boss_hellfire_felorccouncil", "passive", "The Fel Horde fights with axes. Expertise with axes and fist weapons increased."}},
	[24] = {{"Headhunter's Frenzy", "ability_racial_bloodrage", "active", "The Amani fight in a blood frenzy. Increases attack and casting speed for a short time."}, {"Jungle Stalker", "ability_racial_shadowmeld", "passive", "Forest Trolls vanish among the trees. Increases dodge chance by 2%."}, {"Troll Regeneration", "ability_racial_wardoftheloa", "passive", "Troll flesh mends itself. Health regeneration increased; part of it continues in combat."}, {"Headhunter", "inv_misc_head_troll_01", "passive", "An Amani axe thrown never misses. Critical strike chance with throwing weapons increased by 1%."}},
	[25] = {{"Frostmane Frenzy", "spell_frost_frostward", "active", "The trolls of the frozen north fight hot-blooded. Increases attack and casting speed for a short time."}, {"Winter's Blood", "ability_racial_rimeoftheancientmariner", "passive", "Born in the snows of Zul'Drak. Reduces Frost damage taken by 4%."}, {"Troll Regeneration", "ability_racial_wardoftheloafrost", "passive", "Troll flesh mends itself. Health regeneration increased; part of it continues in combat."}, {"Ice Hunter", "inv_misc_head_troll_01", "passive", "The beasts of the north are their prey. Damage against beasts increased by 5%."}},
	[26] = {{"Bone Shield", "ability_racial_skeletonpinkie", "active", "No mind left to frighten, as every Necromancer intends. Removes Charm, Fear and Sleep effects."}, {"Bone Pile", "spell_shadow_raisedead", "active", "A skeleton lying still is just another pile of bones. Slip into the shadows, dropping out of combat."}, {"Unliving", "spell_shadow_antishadow", "passive", "Death holds no more terror. Reduces Shadow damage taken by 4%."}, {"No Lungs", "spell_shadow_raisedead", "passive", "Bones have nothing to drown. Breath lasts much longer underwater."}},
	[28] = {{"Blood of the Gods", "ability_racial_bloodrage", "active", "The Drakkari drink the blood of their gods, a rite their Witch Doctors kept. Increases attack and casting speed for a short time."}, {"Drakkari Regeneration", "ability_racial_wardoftheloa", "passive", "Troll flesh mends itself. Health regeneration increased; part of it continues in combat."}, {"Frozen Empire", "ability_racial_wardoftheloafrost", "passive", "Zul'Drak never thaws. Reduces Frost damage taken by 4%."}, {"Da Voodoo Shuffle", "inv_misc_head_troll_01", "passive", "Troll feet never stay rooted for long. Movement impairing effects last 15% less."}},
	[29] = {{"Quaking Palm", "pandarenracial_quiveringpain", "active", "Incapacitates an enemy for 4 sec. Damage may break the effect."}, {"Epicurean", "pandarenracial_epicurean", "passive", "Well Fed stat bonuses are doubled."}, {"Gourmand", "pandarenracial_gourmand", "passive", "Cooking skill increased by 5."}, {"Bouncy", "pandarenracial_bouncy", "passive", "Fall damage reduced by roughly half."}, {"Inner Peace", "INV_Misc_Fork&Knife", "passive", "You earn twice as much rested experience."}},
	[30] = {{"Mrgglrrgl!", "inv_misc_head_murloc_01", "active", "Nothing holds a slippery murloc for long. Escapes any immobilization or movement slowing effect."}, {"Murloc Swarm", "achievement_reputation_murlocoracle", "active", "Where there is one murloc, there are ten. Increases attack and casting speed for a short time."}, {"Amphibious", "ability_racial_childofthesea", "passive", "Quick and hard to catch. Increases dodge chance by 2%."}, {"Born in the Shallows", "inv_misc_head_murloc_01", "passive", "The sea is home. Breath lasts much longer underwater."}},
	[31] = {{"Mrgglrrgl!", "inv_misc_head_murloc_01", "active", "Nothing holds a slippery murloc for long. Escapes any immobilization or movement slowing effect."}, {"Murloc Swarm", "achievement_reputation_murlocoracle", "active", "Where there is one murloc, there are ten. Increases attack and casting speed for a short time."}, {"Amphibious", "ability_racial_childofthesea", "passive", "Quick and hard to catch. Increases dodge chance by 2%."}, {"Born in the Shallows", "inv_misc_head_murloc_01", "passive", "The sea is home. Breath lasts much longer underwater."}},
	[32] = {{"Desperate Flight", "Ability_Rogue_Sprint", "active", "Removes roots and snares and increases movement speed by 20% for 6 sec."}, {"Wiry Frame", "Ability_Racial_ShadowMeld", "passive", "Dodge chance increased by 1%."}, {"Nervous Energy", "Ability_Rogue_Sprint", "passive", "Haste increased by 1%."}, {"Make Do", "INV_Misc_Fork&Knife", "passive", "Food, drink and bandages restore 10% more health or mana."}, {"Field Dressing", "Spell_Holy_SealOfSacrifice", "passive", "First Aid skill increased by 15 (bandage healing is covered by Make Do)."}},
	[47] = {{"Pack Howl", "ability_racial_runningwild", "active", "The alpha leads the pack into battle. Increases attack power and spell power for a short time."}, {"Alpha Instinct", "ability_racial_twoforms", "passive", "Nothing escapes the alpha's senses. Increases stealth detection."}, {"Viciousness", "ability_racial_viciousness", "passive", "An alpha never misses its prey. Increases chance to hit by 1%."}, {"Thick Pelt", "ability_worgen_darkflight", "passive", "The wild hardened them. Reduces Nature damage taken by 4%."}},
	[48] = {{"Fireblood", "ability_racial_fireblood", "active", "Removes all Poison, Disease, Curse, Magic and Bleed effects and increases your primary stats by 2% for 8 sec, plus 1% for each effect removed (up to 5)."}, {"Forged in Flames", "ability_racial_foregedinflames", "passive", "Physical damage taken reduced by 1%."}, {"Mass Production", "ability_racial_massproduction", "passive", "Blacksmithing skill increased by 5."}, {"Mole Machine", "Trade_BlackSmithing", "active", "Climb into a Mole Machine and burrow to a destination of your choice. (Teleports only you; usable outside combat and instances.)"}, {"Dungeon Delver", "Trade_BlackSmithing", "passive", "Movement speed increased by 4% while indoors."}},
	[49] = {{"Ancestral Call", "spell_nature_ancestralguardian", "active", "Calls on your uncorrupted ancestors to increase attack power and spell damage."}, {"Savage Blood", "Racial_Orc_BerserkerStrength", "passive", "Grants a chance to resist Curse, Disease, and Poison effects."}, {"Sympathetic Vigor", "ability_hunter_beastcall", "passive", "Increases your pet's maximum health."}, {"Unwavering Will", "inv_helmet_23", "passive", "Reduces the duration of Stun effects."}},
	[50] = {{"Ursine Fury", "ability_racial_bearform", "active", "The Furbolg fight with the strength of the bear. Increases attack and casting speed for a short time."}, {"Thick Fur", "inv_cape_special_furbolg_d_01", "passive", "Children of the wild, at home in nature. Reduces Nature damage taken by 4%."}, {"Furbolg Endurance", "ability_racial_bearform", "passive", "Big, and hard to bring down. Base health increased by 5%."}, {"Gift of the Grove", "inv_cape_special_furbolg_d_01", "passive", "The forests of Ashenvale share their herbs. Herbalism skill increased by 15."}},
	[51] = {{"Gnomish Defiance", "spell_shadow_raisedead", "active", "Death did not dull a gnome's stubborn mind, the first rule of any undead Tinker. Removes Charm, Fear and Sleep effects."}, {"Cannibalize", "ability_racial_cannibalize", "active", "Undeath has its appetites. Regenerates health while eating the dead."}, {"Necrotic Intellect", "ability_racial_betterlivingthroughchemistry", "passive", "Brilliance outlives the body. Intellect increased by 5%."}, {"Undead Engineering", "inv_misc_head_clockworkgnome_01", "passive", "A gnome tinkers even after death. Engineering skill increased by 15."}},
	[53] = {{"Headbutt", "Ability_Warrior_DecisiveStrike", "active", "Stuns a nearby enemy for 2 sec."}, {"Brute Strength", "Spell_Nature_UnyeildingStamina", "passive", "Strength increased by 2%."}, {"Thick Hide", "Spell_Shadow_UnholyStrength", "passive", "Armor from equipped items increased by 2%."}, {"Heavy Hands", "INV_Axe_02", "passive", "Expertise with maces and fist weapons increased by 3."}, {"Ogre Cuisine", "Spell_Fire_Fire", "passive", "Cooking skill increased by 5."}},
	[54] = {{"Wing Buffet", "ability_racial_wingbuffet", "active", "Knocks enemies in front of you away."}, {"Glide", "ability_racial_glide", "active", "Spread your wings and glide, slowing your fall for 10 sec."}, {"Awakened", "ability_racial_awakened", "passive", "Damage dealt increased by 3%. (Mastery has no WotLK equivalent.)"}, {"Discerning Eye", "ability_evoker_discerningeye", "passive", "Herbalism and Mining skill increased by 2."}, {"Soar", "Ability_Rogue_Sprint", "active", "Surge forward, increasing movement speed by 60% and slowing your fall for 6 sec. (Ground-bound: WotLK has no free flight.)"}, {"Visage", "Spell_Nature_UnyeildingStamina", "active", "Assume your visage, a mortal form. Use again to return to dracthyr form."}},
	[55] = {{"Felblood Surge", "Spell_Nature_BloodLust", "active", "Increases haste by 10% for 10 sec."}, {"Man'ari Blood", "Spell_Shadow_ShadowBolt", "passive", "Shadow and Fire damage dealt increased by 1%."}, {"Demonic Resilience", "Spell_Shadow_DetectInvisibility", "passive", "Shadow and Fire damage taken reduced by 1%."}, {"Unbroken Will", "Spell_Shadow_RaiseDead", "passive", "Duration of fear effects on you reduced by 10%."}, {"Forbidden Knowledge", "Trade_Engraving", "passive", "Enchanting skill increased by 5."}},
	[56] = {{"Wing Gust", "inv_feather_04", "active", "Harpies take to the air to escape any snare. Escapes any immobilization or movement slowing effect."}, {"Screech", "ability_hunter_pet_raptor", "active", "A harpy's shriek leaves foes reeling. Stuns nearby enemies."}, {"Swift Talons", "ability_hunter_pet_raptor", "passive", "Hard to catch, quick to strike. Increases dodge chance by 2%."}, {"Keen Eyes", "inv_feather_04", "passive", "Nothing hides from a harpy circling above. Increases stealth detection."}},
	[57] = {{"Rocket Jump", "ability_racial_rocketjump", "active", "Every goblin carries a rocket for emergencies, a Tinker's favorite. Escapes any immobilization or movement slowing effect."}, {"Rocket Barrage", "ability_racial_rocketbarrage", "active", "When in doubt, fire everything. Increases attack and casting speed for a short time."}, {"Time is Money", "ability_racial_timeismoney", "passive", "A goblin never wastes a swing. Increases chance to hit by 1%."}, {"Best Deals Anywhere", "ability_racial_bestdealsanywhere", "passive", "Every faction is a customer. Reputation gains increased by 10%."}},
	[58] = {{"Wing Buffet", "ability_racial_wingbuffet", "active", "Knocks enemies in front of you away."}, {"Glide", "ability_racial_glide", "active", "Spread your wings and glide, slowing your fall for 10 sec."}, {"Awakened", "ability_racial_awakened", "passive", "Damage dealt increased by 3%. (Mastery has no WotLK equivalent.)"}, {"Discerning Eye", "ability_evoker_discerningeye", "passive", "Herbalism and Mining skill increased by 2."}, {"Soar", "Ability_Rogue_Sprint", "active", "Surge forward, increasing movement speed by 60% and slowing your fall for 6 sec. (Ground-bound: WotLK has no free flight.)"}, {"Visage", "Spell_Nature_UnyeildingStamina", "active", "Assume your visage, a mortal form. Use again to return to dracthyr form."}},
	[59] = {{"Entropic Embrace", "ability_racial_entropicembrace", "passive", "Your damaging and healing abilities have a chance to increase damage and healing done by 5% for 12 sec."}, {"Preternatural Calm", "ability_racial_preturnaturalcalm", "passive", "Spellcasting is not delayed by taking damage."}, {"Chill of Night", "ability_racial_chillofnight", "passive", "Shadow damage taken reduced by 1%."}, {"Spatial Rift", "Spell_Arcane_Blink", "active", "Tear a rift in space up to 35 yards ahead. Reactivate this ability within 8 sec to teleport through the rift. (Recharges over 3 min; the rift has no visual in WotLK.)"}, {"Ethereal Connection", "Spell_Nature_UnyeildingStamina", "passive", "Reduces the cost of Transmogrification by 50%."}},
	[60] = {{"Spectral Sight", "ability_demonhunter_spectralsight", "active", "Illidari eyes see what hides. Greatly increases stealth detection for a short time."}, {"Fel Rush", "ability_demonhunter_illidansgrasp", "active", "The Illidari dash through any trap. Escapes any immobilization or movement slowing effect."}, {"Demon Hunter's Resolve", "ability_racial_demonbane", "passive", "Trained by Illidan to endure. Stun effects last 15% less."}, {"Felblood", "spell_shadow_demonform", "passive", "The fel they consumed protects them. Reduces Shadow damage taken by 4%."}},
	[61] = {{"Mana Break", "achievement_boss_illidan", "active", "The Illidari tear the magic from their foes. Silences nearby enemies and restores mana."}, {"Illidari Shadows", "ability_racial_shadowmeld", "active", "Hunters of the night, like their kaldorei kin. Slip into the shadows, dropping out of combat."}, {"Demonbane", "ability_racial_demonbane", "passive", "Sworn enemies of the Legion. Reduces Shadow damage taken by 4%."}, {"Demonic Senses", "ability_demonhunter_spectralsight", "passive", "No demon hides from them. Increases stealth detection."}},
	[62] = {{"Light's Judgment", "Spell_Holy_RighteousFury", "active", "Calls down Holy energy on the target after 3 sec, damaging enemies within 10 yards."}, {"Light's Reckoning", "ability_racial_finalverdict", "passive", "On death, you erupt with Light, damaging enemies and healing allies within 15 yards."}, {"Holy Providence", "ability_racial_holyresistance", "passive", "Holy damage taken reduced by 1% and healing done increased by 1%."}, {"Demonbane", "ability_racial_demonbane", "passive", "Experience gained from killing demons increased by 20%."}, {"Forge of Light", "Trade_BlackSmithing", "active", "Summon a Forge of Light for 2 min, enabling Blacksmithing and Engineering, and raise your Blacksmithing skill by 5."}},
	[63] = {{"Arcane Pulse", "ability_racial_forceshield", "active", "Deals Arcane damage to enemies within 8 yards and slows them by 80% for 8 sec."}, {"Magical Affinity", "ability_racial_arcaneaffinity", "passive", "Magical damage dealt increased by 1%."}, {"Arcane Resistance", "ability_racial_magicalresistance", "passive", "Arcane damage taken reduced by 1%."}, {"Ancient History", "ability_racial_ancienthistory", "passive", "Inscription skill increased by 5."}, {"Cantrips", "INV_Misc_Rune_01", "active", "Conjure up a mailbox, allowing you mail access for 1.5 min. (Other players can use it too.)"}},
	[66] = {{"Bull Rush", "ability_racial_bullrush", "active", "Charge forward for 1 sec, knocking enemies in your path down for 1.5 sec."}, {"Mountaineer", "ability_racial_mountaineer", "passive", "Damage dealt and healing done increased by 1% and damage taken reduced by 1% (Versatility has no WotLK equivalent)."}, {"Rugged Tenacity", "ability_racial_ruggedtenacity", "passive", "Reduces each hit you take by an amount that grows with your Stamina."}, {"Pride of Ironhorn", "Spell_Nature_Earthquake", "passive", "Increases your Mining skill by 5. (The retail Deftness mining speed bonus is not implemented.)"}, {"Waste Not, Want Not", "Spell_Nature_UnyeildingStamina", "passive", "You have a chance to loot additional meat and fish."}},
	[67] = {{"Combat Analysis", "ability_racial_combatanalysis", "passive", "Every 5 sec in combat, gain 1% to all stats, stacking up to 10 times. Stacks fade soon after combat ends."}, {"Emergency Failsafe", "ability_racial_rearm", "passive", "Falling below 20% health restores 15% of your maximum health. Can occur once every 2.5 min."}, {"Skeleton Pinkie", "ability_racial_skeletonpinkie", "passive", "You are able to pick locks."}, {"Mastercraft", "Trade_BlackSmithing", "passive", "You function as a personal Blacksmithing Anvil, Cooking Fire, and Mining Forge."}, {"Hyper Organic Light Originator", "Spell_Shadow_Teleport", "active", "Summon Organic Light duplicates to distract your foes for 15 sec."}},
	[68] = {{"Azerite Surge", "ability_earthen_azeritesurge", "active", "After 2.5 sec, unleash a surge of Fire damage on enemies within 15 yards. At level 40 the surge also heals allies, and at level 70 it additionally strikes the healthiest enemy."}, {"Titan-Wrought Frame", "ability_earthen_titanwroughtframe", "passive", "Armor from equipped items increased by 10%."}, {"Wide-Eyed Wonder", "ability_earthen_wideeyedwonder", "passive", "Experience gained from exploring increased by 200%."}, {"Hyper Productive", "ability_earthen_hyperproductive", "passive", "Herbalism, Mining and Skinning skill increased by 3."}, {"Ingest Minerals", "Spell_Nature_Earthquake", "active", "Consume a gem to gain an Earthen Well Fed benefit for 1 hour: Ruby (red/orange) Critical Strike, Sapphire (blue) damage and healing, Amber (yellow) Stamina, Emerald (green) Haste, Onyx (purple/prismatic) attack and spell power."}, {"Quiet Contemplation", "INV_Misc_Fork&Knife", "active", "Draws upon your inner energy, restoring 35% of your health and mana over 20 sec. You must remain seated."}},
	[69] = {{"Azerite Surge", "ability_earthen_azeritesurge", "active", "After 2.5 sec, unleash a surge of Fire damage on enemies within 15 yards. At level 40 the surge also heals allies, and at level 70 it additionally strikes the healthiest enemy."}, {"Titan-Wrought Frame", "ability_earthen_titanwroughtframe", "passive", "Armor from equipped items increased by 10%."}, {"Wide-Eyed Wonder", "ability_earthen_wideeyedwonder", "passive", "Experience gained from exploring increased by 200%."}, {"Hyper Productive", "ability_earthen_hyperproductive", "passive", "Herbalism, Mining and Skinning skill increased by 3."}, {"Ingest Minerals", "Spell_Nature_Earthquake", "active", "Consume a gem to gain an Earthen Well Fed benefit for 1 hour: Ruby (red/orange) Critical Strike, Sapphire (blue) damage and healing, Amber (yellow) Stamina, Emerald (green) Haste, Onyx (purple/prismatic) attack and spell power."}, {"Quiet Contemplation", "INV_Misc_Fork&Knife", "active", "Draws upon your inner energy, restoring 35% of your health and mana over 20 sec. You must remain seated."}},
	[70] = {{"Thorn Bloom", "inv_12_haranir_ability_thornbloom", "active", "Thorns erupt around the target, damaging up to 8 enemies within 8 yards and healing up to 8 allies, pulsing 3 times over 4 sec."}, {"Lash Out", "Spell_Nature_UnyeildingStamina", "passive", "Critical strike damage and healing increased by 1%."}, {"Subterranean Predator", "inv_12_haranir_ability_subterraneanpredator", "passive", "Experience from Beasts and Elementals increased by 10% and damage dealt to them increased by 1%. (WotLK has no Aberration type.)"}, {"One with Nature", "inv_12_haranir_ability_onewithnature", "passive", "Herbalism skill increased by 5."}, {"Rootwalking", "Spell_Nature_HealingTouch", "active", "Perform a ritualistic dance to travel through the roots to the World Tree. (Retail goes to the Cradle in Harandar and has no cooldown.)"}},
	[71] = {{"Thorn Bloom", "inv_12_haranir_ability_thornbloom", "active", "Thorns erupt around the target, damaging up to 8 enemies within 8 yards and healing up to 8 allies, pulsing 3 times over 4 sec."}, {"Lash Out", "Spell_Nature_UnyeildingStamina", "passive", "Critical strike damage and healing increased by 1%."}, {"Subterranean Predator", "inv_12_haranir_ability_subterraneanpredator", "passive", "Experience from Beasts and Elementals increased by 10% and damage dealt to them increased by 1%. (WotLK has no Aberration type.)"}, {"One with Nature", "inv_12_haranir_ability_onewithnature", "passive", "Herbalism skill increased by 5."}, {"Rootwalking", "Spell_Nature_HealingTouch", "active", "Perform a ritualistic dance to travel through the roots to the World Tree. (Retail goes to the Cradle in Harandar and has no cooldown.)"}},
	[74] = {{"Bag of Tricks", "ability_racial_bagoftricks", "active", "Pull a trick from your bag. On an enemy: deals damage and slows by 80% for 4 sec. On an ally: heals and grants 20% movement speed for 4 sec."}, {"Nose for Trouble", "ability_racial_nosefortrouble", "passive", "The first hit you take from an enemy is reduced by 5% of your maximum health. Rearms after 30 sec."}, {"Fire Resistance", "ability_racial_fireresist", "passive", "Fire damage taken reduced by 2%."}, {"Make Camp", "Spell_Fire_Fire", "active", "Make a camp at this location. Can only be used outdoors. The camp lasts 1 hour."}, {"Return to Camp", "INV_Misc_Rune_01", "active", "Return to your camp."}, {"Rummage Your Bag", "INV_Ammo_Snowball", "active", "Change the contents of your Bag of Tricks (Physical, Fire or Frost). Now where did you put that...?"}, {"Alpaca Saddlebags", "Spell_Nature_UnyeildingStamina", "passive", "Grants an 8 slot Woolen Bag once. (WotLK cannot widen the backpack by 8 slots.)"}},
	[78] = {{"Tidal Mend", "INV_Spear_04", "active", "Heals you or an ally for 15% of maximum health over 6 sec."}, {"Flow of the River", "Ability_Rogue_Sprint", "passive", "Haste increased by 1%."}, {"Fluid Footwork", "Ability_Racial_ShadowMeld", "passive", "Dodge chance increased by 1%."}, {"Riverborn", "Ability_Racial_ShadowMeld", "passive", "Swim speed increased by 50% and you can breathe underwater indefinitely."}, {"Master Angler", "Trade_Fishing", "passive", "Fishing skill increased by 15."}},
	[79] = {{"Outcast's Curse", "Spell_Shadow_DetectInvisibility", "passive", "The curse of Sethe has hardened you against the dark. Reduces the damage taken by Shadow spells by 2%."}, {"Feathered Descent", "INV_Feather_01", "passive", "Your feathers slow your fall. Reduces damage from falling."}, {"Lore of Skettis", "INV_Enchant_EssenceEternalLarge", "passive", "The secrets of Skettis sharpen your mind. Intellect increased by 5%."}, {"Flight of the Raven", "Spell_Magic_FeatherFall", "active", "Call on the memory of your winged ancestors: movement speed increased by 40% and you fall slowly for ."}, {"Skyward Glide", "Ability_Druid_FlightForm", "active", "Leap into the air and glide forward on spread wings, falling slowly for ."}},
	[80] = {{"Crimson Thirst", "Spell_Shadow_DeathPact", "active", "Awakens your Darkfallen hunger for .  Your damaging attacks and spells have a chance to restore % of your maximum health, but no more than once every 2 seconds."}, {"Children of the Night", "Spell_Shadow_DeathPact", "passive", "Slightly increases your stealth detection and makes you harder to detect while stealthed.  Increases your movement speed while stealthed by 10% and while dead by 40%."}},
	[81] = {{"Crimson Thirst", "Spell_Shadow_DeathPact", "active", "Awakens your Darkfallen hunger for .  Your damaging attacks and spells have a chance to restore % of your maximum health, but no more than once every 2 seconds."}, {"Children of the Night", "Spell_Shadow_DeathPact", "passive", "Slightly increases your stealth detection and makes you harder to detect while stealthed.  Increases your movement speed while stealthed by 10% and while dead by 40%."}},
	[82] = {{"Static Coil", "Spell_Nature_ChainLightning", "active", "Discharges electricity around you, dealing Nature damage to enemies within 8 yards and slowing them by 40% for 6 sec."}, {"Scaled Hide", "Spell_Shadow_UnholyStrength", "passive", "Armor from equipped items increased by 2%."}, {"Serpentine Reflexes", "Ability_Racial_ShadowMeld", "passive", "Dodge chance increased by 1%."}, {"Stormblood", "Spell_Nature_SpiritArmor", "passive", "Nature damage and healing done increased by 1%."}, {"Venomous Constitution", "Spell_Nature_SpiritArmor", "passive", "Duration of Poison effects on you reduced by 15%."}},
	[83] = {{"Savage Bite", "Ability_GhoulFrenzy", "active", "Bite the target, dealing Physical damage and causing it to bleed for 6 sec."}, {"Pack Hunter", "Ability_Racial_ShadowMeld", "passive", "Critical strike chance increased by 1%."}, {"Keen Nose", "Ability_Tracking", "passive", "Stealth detection increased."}, {"Scavenger", "INV_Misc_Pelt_Wolf_01", "passive", "Skinning skill increased by 5."}, {"Scrappy", "Spell_Nature_UnyeildingStamina", "passive", "Duration of bleeds on you reduced by 10% and bleed damage taken reduced by 1%."}},
	[97] = {{"Pounce", "Ability_Druid_Dash", "active", "Leap to an enemy within 15 yards and daze it for 2 sec."}, {"Predatory Instincts", "Ability_Racial_ShadowMeld", "passive", "Critical strike chance increased by 1%."}, {"Feline Agility", "Ability_Racial_ShadowMeld", "passive", "Dodge chance increased by 1%."}, {"Hunter's Pace", "Ability_Rogue_Sprint", "passive", "Movement speed increased by 5% while out of combat."}, {"Thick Fur", "Spell_Shadow_UnholyStrength", "passive", "Physical damage taken reduced by 1%."}},
	[98] = {{"Draconic Charge", "Ability_Warrior_Charge", "active", "Charge forward, knocking enemies aside and stunning the first enemy struck for 1 sec. On landing you slam the ground, damaging enemies within 8 yards and slowing them by 50% for 4 sec."}, {"Draconic Hide", "Spell_Holy_Devotion", "passive", "Increases armor from items by 3% and reduces physical damage taken by 2%."}, {"Dragonkin", "INV_Misc_Head_Dragon_01", "passive", "Increases damage dealt to Dragonkin by 3%."}, {"Treasure Hoarder", "INV_Misc_Bag_11", "passive", "Increases gold found on creatures by 5%."}},
	[99] = {{"Chromatic Ancestry", "INV_Misc_Head_Dragon_01", "passive", "Every Drakkin carries the blood of an ancient draconic lineage. Choose a Blood while out of combat; only one can be active at a time, and it also sets the school of your Lesser Dragonbreath."}, {"Lesser Dragonbreath", "INV_Misc_Head_Dragon_01", "active", "Exhale draconic energy in a cone, dealing damage in the school of your active Blood and exposing enemies for 6 sec, reducing their armor by 5% and their magic resistances by 20."}, {"Drakkin Together Strong", "Spell_Holy_WordFortitude", "passive", "Party members within 30 yards gain 1% Stamina for each other Drakkin in your party, up to 4%."}, {"Ancient Broodblood", "Spell_Holy_AshesToAshes", "passive", "When struck below 30% health, your draconic instincts awaken, increasing movement speed and healing received by 10% for 8 sec. Can occur once every 60 sec."}},
	[100] = {{"Draconic Charge", "Ability_Warrior_Charge", "active", "Charge forward, knocking enemies aside and stunning the first enemy struck for 1 sec. On landing you slam the ground, damaging enemies within 8 yards and slowing them by 50% for 4 sec."}, {"Draconic Hide", "Spell_Holy_Devotion", "passive", "Increases armor from items by 3% and reduces physical damage taken by 2%."}, {"Dragonkin", "INV_Misc_Head_Dragon_01", "passive", "Increases damage dealt to Dragonkin by 3%."}, {"Treasure Hoarder", "INV_Misc_Bag_11", "passive", "Increases gold found on creatures by 5%."}},
	[103] = {{"Pack Toss", "ability_warstomp", "active", "A grummle's pack lands as hard as a yeti. Stuns nearby enemies."}, {"Trail Snack", "spell_misc_food", "active", "There is always something to eat in a grummle pack. Regenerates health while eating the dead."}, {"Lucky Foot", "spell_nature_unyeildingstamina", "passive", "The luck of the mountain trail steps past the blade. Increases dodge chance by 2%."}, {"Heavy Pack", "inv_misc_bag_10_green", "passive", "They carry more than they weigh. Base health increased by 5%."}},
}
if CHAR_CREATE_EXTRA_RACE_ICONS then
	CHAR_CREATE_EXTRA_RACE_ICONS["Alpha Goblin"] = "Interface\\Icons\\CoA_Race_AlphaGoblin"
	CHAR_CREATE_EXTRA_RACE_ICONS["Alpha Worgen"] = "Interface\\Icons\\CoA_Race_AlphaWorgen"
	CHAR_CREATE_EXTRA_RACE_ICONS["Broken"] = "Interface\\Icons\\CoA_Race_Broken"
	CHAR_CREATE_EXTRA_RACE_ICONS["Dark Iron Dwarf"] = "Interface\\Icons\\CoA_Race_DarkIronDwarf"
	CHAR_CREATE_EXTRA_RACE_ICONS["Darkfallen"] = "Interface\\Icons\\CoA_Race_Darkfallen"
	CHAR_CREATE_EXTRA_RACE_ICONS["Dracthyr"] = "Interface\\Icons\\CoA_Race_Dracthyr"
	CHAR_CREATE_EXTRA_RACE_ICONS["Drakkari Troll"] = "Interface\\Icons\\CoA_Race_DrakkariTroll"
	CHAR_CREATE_EXTRA_RACE_ICONS["Earthen"] = "Interface\\Icons\\CoA_Race_Earthen"
	CHAR_CREATE_EXTRA_RACE_ICONS["Eredar"] = "Interface\\Icons\\CoA_Race_Eredar"
	CHAR_CREATE_EXTRA_RACE_ICONS["Fel Orc"] = "Interface\\Icons\\CoA_Race_FelOrc"
	CHAR_CREATE_EXTRA_RACE_ICONS["Forest Troll"] = "Interface\\Icons\\CoA_Race_ForestTroll"
	CHAR_CREATE_EXTRA_RACE_ICONS["Furbolg"] = "Interface\\Icons\\CoA_Race_Furbolg"
	CHAR_CREATE_EXTRA_RACE_ICONS["Gnoll"] = "Interface\\Icons\\CoA_Race_Gnoll"
	CHAR_CREATE_EXTRA_RACE_ICONS["Grummle"] = "Interface\\Icons\\CoA_Race_Grummle"
	CHAR_CREATE_EXTRA_RACE_ICONS["Haranir"] = "Interface\\Icons\\CoA_Race_Haranir"
	CHAR_CREATE_EXTRA_RACE_ICONS["Harpy"] = "Interface\\Icons\\CoA_Race_Harpy"
	CHAR_CREATE_EXTRA_RACE_ICONS["High Elf"] = "Interface\\Icons\\CoA_Race_HighElf"
	CHAR_CREATE_EXTRA_RACE_ICONS["Highmountain Tauren"] = "Interface\\Icons\\CoA_Race_HighmountainTauren"
	CHAR_CREATE_EXTRA_RACE_ICONS["Ice Troll"] = "Interface\\Icons\\CoA_Race_IceTroll"
	CHAR_CREATE_EXTRA_RACE_ICONS["Illidari Blood Elf"] = "Interface\\Icons\\CoA_Race_IllidariBloodElf"
	CHAR_CREATE_EXTRA_RACE_ICONS["Illidari Night Elf"] = "Interface\\Icons\\CoA_Race_IllidariNightElf"
	CHAR_CREATE_EXTRA_RACE_ICONS["Jinyu"] = "Interface\\Icons\\CoA_Race_Jinyu"
	CHAR_CREATE_EXTRA_RACE_ICONS["Kul Tiran"] = "Interface\\Icons\\CoA_Race_KulTiran"
	CHAR_CREATE_EXTRA_RACE_ICONS["Lightforged Draenei"] = "Interface\\Icons\\CoA_Race_LightforgedDraenei"
	CHAR_CREATE_EXTRA_RACE_ICONS["Mag'har Orc"] = "Interface\\Icons\\CoA_Race_MagHarOrc"
	CHAR_CREATE_EXTRA_RACE_ICONS["Mechagnome"] = "Interface\\Icons\\CoA_Race_Mechagnome"
	CHAR_CREATE_EXTRA_RACE_ICONS["Murloc"] = "Interface\\Icons\\CoA_Race_Murloc"
	CHAR_CREATE_EXTRA_RACE_ICONS["Naga"] = "Interface\\Icons\\CoA_Race_Naga"
	CHAR_CREATE_EXTRA_RACE_ICONS["Nightborne"] = "Interface\\Icons\\CoA_Race_Nightborne"
	CHAR_CREATE_EXTRA_RACE_ICONS["Ogre"] = "Interface\\Icons\\CoA_Race_Ogre"
	CHAR_CREATE_EXTRA_RACE_ICONS["Pandaren"] = "Interface\\Icons\\CoA_Race_Pandaren"
	CHAR_CREATE_EXTRA_RACE_ICONS["Saberon"] = "Interface\\Icons\\CoA_Race_Saberon"
	CHAR_CREATE_EXTRA_RACE_ICONS["Kyrian"] = "Interface\\Icons\\CoA_Race_Kyrian"
	CHAR_CREATE_EXTRA_RACE_ICONS["Venthyr"] = "Interface\\Icons\\CoA_Race_Venthyr"
	CHAR_CREATE_EXTRA_RACE_ICONS["Slime Giant"] = "Interface\\Icons\\CoA_Race_SlimeGiant"
	CHAR_CREATE_EXTRA_RACE_ICONS["Centaur"] = "Interface\\Icons\\CoA_Race_Centaur"
	CHAR_CREATE_EXTRA_RACE_ICONS["Hozen"] = "Interface\\Icons\\CoA_Race_Hozen"
	CHAR_CREATE_EXTRA_RACE_ICONS["Faceless One"] = "Interface\\Icons\\CoA_Race_Faceless"
	CHAR_CREATE_EXTRA_RACE_ICONS["Banshee"] = "Interface\\Icons\\CoA_Race_Banshee"
	CHAR_CREATE_EXTRA_RACE_ICONS["Sylvari"] = "Interface\\Icons\\CoA_Race_Sylvari"
	CHAR_CREATE_EXTRA_RACE_ICONS["Mantid"] = "Interface\\Icons\\CoA_Race_Mantid"
	CHAR_CREATE_EXTRA_RACE_ICONS["Ethereal"] = "Interface\\Icons\\CoA_Race_Ethereal"
	CHAR_CREATE_EXTRA_RACE_ICONS["Sethrak"] = "Interface\\Icons\\CoA_Race_Sethrak"
	CHAR_CREATE_EXTRA_RACE_ICONS["Skeleton"] = "Interface\\Icons\\CoA_Race_Skeleton"
	CHAR_CREATE_EXTRA_RACE_ICONS["Taunka"] = "Interface\\Icons\\CoA_Race_Taunka"
	CHAR_CREATE_EXTRA_RACE_ICONS["Forgotten"] = "Interface\\Icons\\CoA_Race_ThinHuman"
	CHAR_CREATE_EXTRA_RACE_ICONS["Tuskarr"] = "Interface\\Icons\\CoA_Race_Tuskarr"
	CHAR_CREATE_EXTRA_RACE_ICONS["Undead Gnome"] = "Interface\\Icons\\CoA_Race_UndeadGnome"
	CHAR_CREATE_EXTRA_RACE_ICONS["Void Elf"] = "Interface\\Icons\\CoA_Race_VoidElf"
	CHAR_CREATE_EXTRA_RACE_ICONS["Vrykul"] = "Interface\\Icons\\CoA_Race_Vrykul"
	CHAR_CREATE_EXTRA_RACE_ICONS["Vulpera"] = "Interface\\Icons\\CoA_Race_Vulpera"
	CHAR_CREATE_EXTRA_RACE_ICONS["Worgen"] = "Interface\\Icons\\CoA_Race_Worgen"
	CHAR_CREATE_EXTRA_RACE_ICONS["Zandalari Troll"] = "Interface\\Icons\\CoA_Race_ZandalariTroll"
end
do
	local function RaceRacials(name)
		local ids = name and CHAR_CREATE_RACE_IDS and CHAR_CREATE_RACE_IDS[name]
		return ids and CHAR_CREATE_COA_RACIALS and CHAR_CREATE_COA_RACIALS[ids[1]]
	end

	local function RacialLines(racials)
		local text = ""
		for _, racial in ipairs(racials) do
			text = text .. "|TInterface\\Icons\\" .. racial[2] .. ":24:24:0:0|t |cffffd100" .. racial[1] .. "|r |cff9d9d9d(" ..
				racial[3] .. ")|r|n" .. racial[4] .. "|n|n"
		end
		return text
	end

	if CHAR_CREATE_RACE_BONUS then
		for raceID in pairs(CHAR_CREATE_COA_RACIALS or {}) do
			CHAR_CREATE_RACE_BONUS[raceID] = nil              -- the old borrowed racial line
		end
	end

	-- race button tooltips: flavor line + the four CoA racials (redone whenever the button rebuilds its tooltip)
	local function PatchButtons()
		for i = 1, 128 do
			local button = _G["CharCreateRaceButton" .. i]
			if button and button.tooltipText and button.coaTip ~= button.tooltipText then
				button.coaTip = button.tooltipText
				local racials = RaceRacials(button.tooltipTitle)
				if racials then
					local flavor = button.tooltipText[1]
					for k = #button.tooltipText, 1, -1 do
						button.tooltipText[k] = nil
					end
					if flavor then
						tinsert(button.tooltipText, flavor)
					end
					for _, racial in ipairs(racials) do
						tinsert(button.tooltipText, "|TInterface\\Icons\\" .. racial[2] .. ":24:24:0:0|t|cffffd100" ..
							racial[1] .. "|r |cff9d9d9d(" .. racial[3] .. ")|r|n" .. racial[4] .. "\n")
					end
				end
			end
		end
	end
	local ticker = CreateFrame("Frame")
	ticker:SetScript("OnUpdate", function(self, elapsed)
		self.elapsed = (self.elapsed or 0) + elapsed
		if self.elapsed > 0.3 then
			self.elapsed = 0
			if CharacterCreate and CharacterCreate:IsShown() then
				local ok, err = pcall(PatchButtons)
				if not ok then COA_UI_ERROR = "racials: " .. tostring(err) end
			end
		end
	end)

	-- "Selected Race" step button: the added races' face portrait (their button texture is painted with
	-- SetPortraitToTexture, so copying it gave a broken icon)
	if CharacterCreate_UpdateNavigtationRaceButton then
		local updateNav = CharacterCreate_UpdateNavigtationRaceButton
		function CharacterCreate_UpdateNavigtationRaceButton(...)
			updateNav(...)
			pcall(function()
				local button = _G["CharCreateRaceButton" .. CharacterCreate.selectedRace]
				local name = button and (button.tooltipTitle or (button.Text and button.Text:GetText()))
				local icon = name and CHAR_CREATE_EXTRA_RACE_ICONS and CHAR_CREATE_EXTRA_RACE_ICONS[name]
				local nav = icon and CharacterCreateNavigationFrame:GetButtonByIndex(Enum.CharCreateSteps.ClassRace)
				if nav and nav.Icon then
					if SetPortraitToTexture then
						SetPortraitToTexture(nav.Icon, icon)
					else
						nav.Icon:SetTexture(icon)
					end
					nav.Icon:SetTexCoord(0, 1, 0, 1)
				end
			end)
		end
	end

	-- the race info panel of the selected race
	local setCharacterRace = SetCharacterRace
	function SetCharacterRace(id, ...)
		local a, b, c, d, e = setCharacterRace(id, ...)
		local ok, name = pcall(GetNameForRace)
		local racials = ok and RaceRacials(name)
		local bullets = CharCreateRaceInfoFrame and CharCreateRaceInfoFrame.scrollFrame.scrollChild.bulletText
		if racials and bullets then
			bullets:SetText(RacialLines(racials))
		end
		return a, b, c, d, e
	end
end
-- end CoA racials of the added races

-- Local server: Esteria native appearance controls
-- The retail options of the Esteria races (our ids 66+: Highmountain, Mechagnome, Earthen, Haranir, Vulpera) come
-- from EsteriaAppearance.dll ("EA_" calls of CycleCharCustomization). Ascension's creation screen has its own rows
-- (CharCreateCustomizationButton1-5, CharCreateCustomizationFrame), so the extra options get rows of their own built
-- from the same template, six per page, in place of the stock rows while "Extra Customization" is on (the toggle
-- under the Customize step button). Every other race keeps the stock screen untouched.
do
	local BY_FILE = {NAGA_ = 54, HIGHMOUNTAINTAUREN = 46, MECHAGNOME = 47, EARTHEN = 48, EARTHENHORDE = 49, HARANIR = 50,
		HARANIRHORDE = 51, VULPERAESTERIA = 20, SETHRAKALLIANCE = 62, GNOLLHORDE = 63, SABERONHORDE = 64, KYRIANALLIANCE = 60, VENTHYRHORDE = 61, SLIMEGIANTHORDE = 65, CENTAURALLIANCE = 68, CENTAURHORDE = 68, HOZENALLIANCE = 73, SYLVARIALLIANCE = 74, MANTIDHORDE = 75, ETHEREALALLIANCE = 76, BANSHEEHORDE = 77, FACELESSHORDE = 78, QIRAJIHORDE = 79}
	local NAMED = {[20] = true, [46] = true, [48] = true, [49] = true, [50] = true, [51] = true}
	local LABELS = {"Skin Color", "Face", "Hair Style", "Hair Color", "Eye Color", "Eyebrow Style",
		"Feathers", "Feather Color", "Facial Hair", "Ears"}
	local MECH_LABELS = {"Skin Color", "Face", "Hair Style", "Hair Color", "Facial Hair", "Arm Upgrade", "Leg Upgrade",
		"Modification", "Eye Color", "Paint", "Eyesight", "Eye Style"}
	local MAX, PER_PAGE, SPACING = 29, 6, 52
	local extraOn, page, active = true, 1, false
	local bodyOn, bodyToggle, bodyHidden = false, nil, false     -- Esteria body-slider page (COA_BodyPanel), every race
	local BodyToggle
	local rows, pager, toggle = {}, nil, nil

	-- the race file name, asked from the game only when the selected race or sex changes (asking it several times a
	-- second crashed the client after a few minutes inside Extensions.dll)
	local cachedKey, cachedFile, cachedRetail
	local function RaceFile()
		if not (CharacterCreate and CharacterCreate.selectedRace) then
			return nil
		end
		local key = CharacterCreate.selectedRace .. ":" .. (CharacterCreate.selectedGender or 0)
		if key ~= cachedKey then
			cachedKey = key
			local ok, _, file = pcall(GetNameForRace)
			cachedFile = ok and file or nil
			local owned, flag = pcall(CycleCharCustomization, "EA_RETAIL", 1)   -- races built from the Retail files (Worgen, Dracthyr)
			cachedRetail = owned and flag == 1
		end
		return cachedFile
	end
	local function EsteriaRace()
		local file = RaceFile()
		return (file and BY_FILE[strupper(file)]) or (cachedRetail and 200) or nil
	end
	COA_EsteriaRace = function()        -- the choice list (coa_picker.lua): Esteria options only while they are shown
		return active and EsteriaRace()
	end

	local function Get(i)
		local ok, value, count, label = pcall(CycleCharCustomization, "EA_GET", i)
		if ok then
			return value, count, label
		end
	end

	local function Options()
		local list = {}
		for i = 1, MAX do
			local _, count = Get(i)
			if count and count > 1 then
				list[#list + 1] = i
			end
		end
		return list
	end

	local function Stock(show)
		for i = 1, 5 do
			local row = _G["CharCreateCustomizationButton" .. i]
			if row then
				if show then row:Show() else row:Hide() end
			end
		end
		if show and CharacterCreate_UpdateHairCustomization then
			pcall(CharacterCreate_UpdateHairCustomization)  -- the stock screen decides which of its rows to show
		end
	end

	local function Row(i)
		if not rows[i] then
			local parent = CharCreateCustomizationFrame
			local row = CreateFrame("CheckButton", "CoACustomizationRow" .. i, parent, "CharCreateCustomizationButtonTemplate")
			row:SetPoint("TOP", parent, "TOP", 0, -15 - (i - 1) * SPACING)
			row:SetFrameLevel((_G.CharCreateCustomizationButton1 or parent):GetFrameLevel())
			rows[i] = row
		end
		return rows[i]
	end

	local function Pager()
		if not pager then
			local parent = CharCreateCustomizationFrame
			pager = CreateFrame("Frame", "CoACustomizationPager", parent)
			pager:SetSize(190, 24)
			pager:SetPoint("BOTTOM", parent, "TOP", 0, 8)
			pager.text = pager:CreateFontString(nil, "ARTWORK", "GlueFontNormalSmall")
			pager.text:SetPoint("CENTER")
			for _, side in ipairs({"Prev", "Next"}) do
				local b = CreateFrame("Button", nil, pager)
				b:SetSize(28, 28)
				b:SetPoint(side == "Prev" and "LEFT" or "RIGHT")
				b:SetNormalTexture("Interface\\Buttons\\UI-SpellbookIcon-" .. side .. "Page-Up")
				b:SetPushedTexture("Interface\\Buttons\\UI-SpellbookIcon-" .. side .. "Page-Down")
				b:SetHighlightTexture("Interface\\Buttons\\UI-Common-MouseHilight", "ADD")
				b:SetScript("OnClick", function()
					page = page + (side == "Prev" and -1 or 1)
					PlaySound("igMainMenuOptionCheckBoxOn")
					COA_EsteriaRefresh()
				end)
				pager[side] = b
			end
		end
		return pager
	end

	local function HideOurs()
		for _, row in pairs(rows) do
			row:Hide()
		end
		if pager then
			pager:Hide()
		end
	end

	-- draws the current page of Esteria options (or gives the stock rows back)
	function COA_EsteriaRefresh()
		local race = EsteriaRace()
		local panel = CharCreateCustomizationFrame
		if bodyOn and panel and panel:IsVisible() then
			COA_BodyPageOn = true
			if active then active = false; HideOurs() end
			Stock(false)
			bodyHidden = true
			if COA_BodyPanel then COA_BodyPanel(true) end
			return
		end
		COA_BodyPageOn = false
		if COA_BodyPanel then COA_BodyPanel(false) end
		if bodyHidden then bodyHidden = false; Stock(true) end
		local options = race and extraOn and panel and panel:IsVisible() and Options() or {}
		if #options == 0 then
			if active then
				active = false
				HideOurs()
				Stock(true)
			end
			return
		end
		active = true
		Stock(false)
		local pages = math.ceil(#options / PER_PAGE)
		page = math.max(1, math.min(page, pages))
		for slot = 1, PER_PAGE do
			local option = options[(page - 1) * PER_PAGE + slot]
			local row = Row(slot)
			if option then
				local value, count, label = Get(option)
				row:SetID(option)
				local name = (label and label ~= "" and label) or (race == 47 and MECH_LABELS[option])
					or (not NAMED[race] and LABELS[option]) or ("Option " .. option)
				if (race == 48 or race == 49) and label == "Belt" then
					row.text:SetText(name .. "  " .. (value == 0 and "None" or "Gem"))
				else
					row.text:SetText(name .. "  " .. ((value or 0) + 1) .. "/" .. (count or 0))
				end
				row:Show()
			else
				row:Hide()
			end
		end
		local p = Pager()
		if pages > 1 then
			p.text:SetText("Page " .. page .. " / " .. pages)
			if page > 1 then p.Prev:Show() else p.Prev:Hide() end
			if page < pages then p.Next:Show() else p.Next:Hide() end
			p:Show()
		else
			p:Hide()
		end
	end

	-- "Extra Customization" toggle, under the Customize step button
	local function Toggle()
		if not toggle then
			local anchor = CharacterCreateNavigationFrame and Enum and Enum.CharCreateSteps
				and CharacterCreateNavigationFrame:GetButtonByIndex(Enum.CharCreateSteps.Customization)
			toggle = CreateFrame("Button", "CoAExtraCustomizationButton", CharacterCreateFrame or CharacterCreate)
			toggle:SetSize(260, 48)
			if anchor then
				toggle:SetPoint("TOPLEFT", anchor, "BOTTOMLEFT", 0, -16)
			else
				toggle:SetPoint("TOPLEFT", CharacterCreateFrame or CharacterCreate, "TOPLEFT", 45, -330)
			end
			toggle.icon = toggle:CreateTexture(nil, "ARTWORK")
			toggle.icon:SetSize(44, 44)
			toggle.icon:SetPoint("LEFT")
			toggle.icon:SetTexture("Interface\\Icons\\INV_Misc_Gear_01")
			toggle.text = toggle:CreateFontString(nil, "ARTWORK", "GlueFontNormalSmall")
			toggle.text:SetPoint("LEFT", toggle.icon, "RIGHT", 12, 0)
			toggle.text:SetJustifyH("LEFT")
			toggle:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight")
			toggle:SetScript("OnClick", function()
				extraOn = not extraOn
				page = 1
				PlaySound("gsCharacterCreationLook")
				COA_EsteriaRefresh()
			end)
		end
		return toggle
	end
	BodyToggle = function()
		if not bodyToggle then
			local anchor = CharacterCreateNavigationFrame and Enum and Enum.CharCreateSteps
				and CharacterCreateNavigationFrame:GetButtonByIndex(Enum.CharCreateSteps.Customization)
			bodyToggle = CreateFrame("Button", "CoABodySliderButton", CharacterCreateFrame or CharacterCreate)
			bodyToggle:SetSize(260, 48)
			if anchor then
				bodyToggle:SetPoint("TOPLEFT", anchor, "BOTTOMLEFT", 0, -88)
			else
				bodyToggle:SetPoint("TOPLEFT", CharacterCreateFrame or CharacterCreate, "TOPLEFT", 45, -400)
			end
			bodyToggle.icon = bodyToggle:CreateTexture(nil, "ARTWORK")
			bodyToggle.icon:SetSize(44, 44)
			bodyToggle.icon:SetPoint("LEFT")
			bodyToggle.icon:SetTexture("Interface\\Icons\\INV_Misc_Note_01")
			bodyToggle.text = bodyToggle:CreateFontString(nil, "ARTWORK", "GlueFontNormalSmall")
			bodyToggle.text:SetPoint("LEFT", bodyToggle.icon, "RIGHT", 12, 0)
			bodyToggle.text:SetJustifyH("LEFT")
			bodyToggle:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight")
			bodyToggle:SetScript("OnClick", function()
				bodyOn = not bodyOn
				PlaySound("gsCharacterCreationLook")
				COA_EsteriaRefresh()
				if bodyOn then bodyToggle.text:SetText("|cff00ff00Body Sliders: ON|r|nClick to go back to the appearance") else bodyToggle.text:SetText("Body Sliders: OFF|n|cffffd100Click to change your build and face|r") end
			end)
		end
		return bodyToggle
	end
	function COA_BodyBack()
		bodyOn = false
		if bodyToggle then bodyToggle.text:SetText("Body Sliders: OFF|n|cffffd100Click to change your build and face|r") end
		COA_EsteriaRefresh()
	end
	local function UpdateToggle()
		do
			local panelB = CharCreateCustomizationFrame
			if panelB and panelB:IsVisible() then
				local b = BodyToggle()
				if bodyOn then b.text:SetText("|cff00ff00Body Sliders: ON|r|nClick to go back to the appearance") else b.text:SetText("Body Sliders: OFF|n|cffffd100Click to change your build and face|r") end
				b:Show()
			else
				if bodyToggle then bodyToggle:Hide() end
				if bodyOn then bodyOn = false; if COA_BodyPanel then COA_BodyPanel(false) end end
			end
		end
		local race = EsteriaRace()
		local panel = CharCreateCustomizationFrame
		if not (race and panel and panel:IsVisible()) then
			if toggle then toggle:Hide() end
			return
		end
		local t = Toggle()
		if extraOn then
			t.text:SetText("|cff00ff00Extra Customization: ON|r|nClick for the basic options")
		else
			t.text:SetText("Extra Customization: OFF|n|cffffd100Click for all retail options|r")
		end
		t:Show()
	end

	-- the arrows of our rows (their ids are option numbers) cycle the Esteria option
	local left, right, randomize = CharacterCustomization_Left, CharacterCustomization_Right, CharacterCreate_Randomize
	function CharacterCustomization_Left(id)
		if active then
			PlaySound("gsCharacterCreationLook")
			pcall(CycleCharCustomization, "EA_CYCLE", id, -1)
			COA_EsteriaRefresh()
		else
			left(id)
		end
	end
	function CharacterCustomization_Right(id)
		if active then
			PlaySound("gsCharacterCreationLook")
			pcall(CycleCharCustomization, "EA_CYCLE", id, 1)
			COA_EsteriaRefresh()
		else
			right(id)
		end
	end
	function CharacterCreate_Randomize(...)
		if active then
			PlaySound("gsCharacterCreationLook")
			pcall(CycleCharCustomization, "EA_RANDOM", 1)
			COA_EsteriaRefresh()
		else
			return randomize(...)
		end
	end

	-- Highmountain keeps its real size: the Customize camera (set again on every option) steps back for it
	local ZOOM = {[46] = 1.25, [68] = 1.5, [60] = 1.75, [61] = 1.3, [65] = 1.3, VRYKUL = 1.65, FACELESSHORDE = 1.8, QIRAJIHORDE = 1.8}
	function ZOOMK()
		local id = EsteriaRace()
		return (id and ZOOM[id]) or ZOOM[strupper(RaceFile() or "")]
	end
	local zoomAt
	if SetFaceCustomizeCamera then
		local setFace = SetFaceCustomizeCamera
		function SetFaceCustomizeCamera(...)
			local a, b, c = setFace(...)
			if ZOOMK() then
				zoomAt = 0.35
			end
			return a, b, c
		end
	end
	local function Zoom(elapsed)
		if not zoomAt then
			return
		end
		zoomAt = zoomAt - elapsed
		if zoomAt > 0 then
			return
		end
		zoomAt = nil
		local k = ZOOMK()
		if k and C_CharacterCreate and C_CharacterCreate.GetCameraPosition then
			pcall(function()
				local x, y, z = C_CharacterCreate.GetCameraPosition()
				C_CharacterCreate.SetCameraPosition(x * k, y * k, z * (1 + (k - 1) * 0.6))
			end)
		end
	end

	-- only when something changed (race, sex, step, toggle) and once a second while our rows are up:
	-- asking the DLL every frame ran the 32-bit client out of memory
	local ticker = CreateFrame("Frame")
	ticker:SetScript("OnUpdate", function(self, elapsed)
		Zoom(elapsed)
		self.elapsed = (self.elapsed or 0) + elapsed
		if self.elapsed < 0.25 or not (CharacterCreate and CharacterCreate:IsShown()) then
			return
		end
		self.elapsed = 0
		local file = RaceFile()
		local panel = CharCreateCustomizationFrame
		local key = (file or "") .. ":" .. (CharacterCreate.selectedGender or 0) .. ":" ..
			tostring(panel and panel:IsVisible()) .. ":" .. tostring(extraOn)
		if key ~= self.key and self.file ~= file then
			page = 1
			self.file = file
		end
		if key ~= self.key and panel and panel:IsVisible() and ZOOMK() then
			zoomAt = 0.35                       -- Highmountain: step back once the Customize camera is set
		end
		self.slow = (self.slow or 0) + 1
		if key ~= self.key or (active and self.slow >= 4) then
			self.key, self.slow = key, 0
			local okRun, err = pcall(function()
				COA_EsteriaRefresh()
				UpdateToggle()
			end)
			if not okRun then
				COA_UI_ERROR = tostring(err)
			end
			if COA_UI_ERROR then                -- only shown when something failed (a screenshot tells what)
				if not self.status then
					self.status = (CharacterCreateFrame or CharacterCreate):CreateFontString(nil, "OVERLAY", "GlueFontNormalSmall")
					self.status:SetPoint("BOTTOMLEFT", CharacterCreateFrame or CharacterCreate, "BOTTOMLEFT", 70, 120)
				end
				self.status:SetText("|cffff4040CoA UI error: " .. COA_UI_ERROR .. "|r")
			end
		end
	end)
end
-- end Esteria native appearance controls


-- Two Forms (Worgen Wolf/Human, Dracthyr Dragon/Visage): a tab per form, each with its own options, and BOTH to see the
-- form that is not being edited behind it. The tabs only exist for races with two forms (EA_FORM_COUNT == 2).
do
	local tabs, both
	local function Tab(name, point, x, onClick)
		local b = CreateFrame("Button", name, CharacterCreateFrame or CharacterCreate)
		b:SetSize(110, 30)
		b:SetFrameLevel((CharacterCreateFrame or CharacterCreate):GetFrameLevel() + 5)
		b:SetPoint("TOP", CharacterCreateFrame or CharacterCreate, "TOP", x, -56)
		b:SetNormalTexture("Interface\Buttons\UI-DialogBox-Button-Up")
		b:SetPushedTexture("Interface\Buttons\UI-DialogBox-Button-Down")
		b:SetHighlightTexture("Interface\Buttons\UI-Common-MouseHilight", "ADD")
		b.label = b:CreateFontString(nil, "ARTWORK", "GlueFontNormalSmall")
		b.label:SetPoint("CENTER", b, "CENTER", 0, 2)
		b:SetScript("OnClick", function() PlaySound("gsCharacterCreationLook") onClick() end)
		return b
	end
	local function Update()
		local ok, count = pcall(CycleCharCustomization, "EA_FORM_COUNT", 1)
		local panel = CharCreateCustomizationFrame
		local show = ok and count == 2 and panel and panel:IsVisible()
		if show and not tabs then
			tabs = {}
			for index = 0, 1 do
				tabs[index] = Tab("CoAFormTab" .. index, "TOP", index == 0 and -120 or 0, function()
					pcall(CycleCharCustomization, "EA_FORM_SET", index)
					if COA_EsteriaRefresh then COA_EsteriaRefresh() end
				end)
			end
			both = Tab("CoAFormBoth", "TOP", 120, function()
				local _, on = pcall(CycleCharCustomization, "EA_FORM_DUAL", 2)
				pcall(CycleCharCustomization, "EA_FORM_DUAL", on == 1 and 0 or 1)
			end)
			both.label:SetText("BOTH")
		end
		if not tabs then return end
		for index = 0, 1 do
			if show then
				local _, current = pcall(CycleCharCustomization, "EA_FORM_GET", 1)
				local _, name = pcall(CycleCharCustomization, "EA_FORM_NAME", index)
				tabs[index].label:SetText(string.upper(name or ""))
				if current == index then tabs[index].label:SetTextColor(1, 1, 1) else tabs[index].label:SetTextColor(1, 0.82, 0) end
				tabs[index]:Show()
			else
				tabs[index]:Hide()
			end
		end
		if show then both:Show() else both:Hide() end
	end
	local refresh = COA_EsteriaRefresh
	function COA_EsteriaRefresh(...)
		local a, b, c = refresh(...)
		pcall(Update)
		return a, b, c
	end
end

-- Local server: customization choice list
-- Click the name of a customization row: a list of every valid choice opens, click one to wear it. The arrows only
-- step through the choices that fit the current look (and the Esteria rows through their own encoding), so many
-- looks were only reachable through Randomize. Esteria races (66+) use the DLL's own options (EA_CHOICES / EA_SET);
-- every other race the stock rows 1-5 (EA_STOCK_CHOICES, applied by stepping the row to the value).
-- Needs the Esteria-patched Ascension.exe (CycleCharCustomization "EA_*"); without it nothing opens.
do
	local VISIBLE, ROW_HEIGHT = 12, 22
	-- Ascension's rows (CharCreateCustomizationButton1-5) and the Esteria option rows (esteria_options.lua)
	local ROWS = {"CharCreateCustomizationButton1", "CharCreateCustomizationButton2", "CharCreateCustomizationButton3",
		"CharCreateCustomizationButton4", "CharCreateCustomizationButton5", "CoACustomizationRow1", "CoACustomizationRow2",
		"CoACustomizationRow3", "CoACustomizationRow4", "CoACustomizationRow5", "CoACustomizationRow6"}
	local picker

	local function Esteria()
		return COA_EsteriaRace and COA_EsteriaRace()
	end

	local function State(frame)
		local ok, value, count, label
		if Esteria() then
			ok, value, count, label = pcall(CycleCharCustomization, "EA_GET", frame:GetID())
		elseif not frame.coaEsteria and frame:GetID() >= 1 and frame:GetID() <= 5 then
			ok, value, count = pcall(CycleCharCustomization, "EA_STOCK_GET", frame:GetID())
		end
		if ok and value and count then
			return value, count, label
		end
	end

	local function Choices(frame)
		local ok, encoded = pcall(CycleCharCustomization, Esteria() and "EA_CHOICES" or "EA_STOCK_CHOICES", frame:GetID())
		local list = {}
		if ok and type(encoded) == "string" then
			for choice in string.gmatch(encoded, "%d+") do
				list[#list + 1] = tonumber(choice)
			end
		end
		return list
	end

	local function Apply(frame, target)
		local id = frame:GetID()
		local value, count = State(frame)
		if not value or value == target then
			return
		end
		if Esteria() then
			pcall(CycleCharCustomization, "EA_SET", id, target)
		else
			for i = 1, count do                   -- stock rows: step until the row shows the wanted value
				local before = value
				CycleCharCustomization(id, 1)
				value = State(frame)
				if not value or value == target or value == before then
					break
				end
			end
		end
		if Esteria() and COA_EsteriaRefresh then
			pcall(COA_EsteriaRefresh)
		elseif CharacterCreate_UpdateHairCustomization then
			pcall(CharacterCreate_UpdateHairCustomization)
		end
	end

	local function Signature()
		return tostring(CharacterCreate.selectedRace) .. ":" .. tostring(CharacterCreate.selectedGender)
	end

	local function Refresh()
		for i, row in ipairs(picker.rows) do
			local choice = picker.choices[picker.offset + i]
			if choice then
				row.choice = choice
				row:SetText((picker.label ~= "" and picker.label .. " " or "") .. (choice + 1) ..
					(choice == picker.selected and "  |cffffd100<|r" or ""))
				row:Show()
			else
				row:Hide()
			end
		end
	end

	local function Scroll(_, delta)
		local top = math.max(0, #picker.choices - VISIBLE)
		picker.offset = math.min(top, math.max(0, picker.offset - delta))
		Refresh()
	end

	local function Build()
		picker = CreateFrame("Frame", "CoACustomizationChoiceMenu", CharacterCreateFrame or CharacterCreate)
		picker:SetFrameStrata("TOOLTIP")
		picker:SetClampedToScreen(true)
		picker:EnableMouse(true)
		picker:EnableMouseWheel(true)
		picker:SetScript("OnMouseWheel", Scroll)
		picker:SetBackdrop({bgFile = "Interface\\Glues\\Common\\Glue-Tooltip-Background",
			edgeFile = "Interface\\Glues\\Common\\Glue-Tooltip-Border", tile = true, tileSize = 16, edgeSize = 16,
			insets = {left = 5, right = 5, top = 5, bottom = 5}})
		picker.rows = {}
		for i = 1, VISIBLE do
			local row = CreateFrame("Button", nil, picker)
			row:SetSize(200, ROW_HEIGHT)
			row:SetPoint("TOPLEFT", 10, -8 - (i - 1) * ROW_HEIGHT)
			row:SetNormalFontObject(GlueFontNormalSmall)
			row:SetHighlightFontObject(GlueFontHighlightSmall)
			row:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight")
			row:EnableMouseWheel(true)
			row:SetScript("OnMouseWheel", Scroll)
			row:SetScript("OnClick", function(self)
				local owner = picker.owner
				picker:Hide()
				PlaySound("gsCharacterCreationLook")
				Apply(owner, self.choice)
			end)
			picker.rows[i] = row
		end
		picker:SetScript("OnUpdate", function(self, elapsed)
			self.elapsed = (self.elapsed or 0) + elapsed
			if self.elapsed > 0.2 then
				self.elapsed = 0
				if not self.owner:IsVisible() or Signature() ~= self.signature then
					self:Hide()
				end
			end
		end)
		picker:Hide()
	end

	local function Open(frame)
		if picker and picker:IsShown() then
			local same = picker.owner == frame
			picker:Hide()
			if same then
				return
			end
		end
		local value, count, label = State(frame)
		local choices = Choices(frame)
		if not value or #choices < 2 then
			return
		end
		if not picker then
			Build()
		end
		local text = frame.text or _G[frame:GetName() .. "Text"]
		label = label or (text and text:GetText() or "")
		label = label:gsub("%s+%d+/%d+$", ""):gsub("%s+None$", ""):gsub("%s+Gem$", "")
		picker.owner, picker.label, picker.choices, picker.selected = frame, label, choices, value
		picker.signature = Signature()
		local selectedRow = 1
		for i, choice in ipairs(choices) do
			if choice == value then
				selectedRow = i
			end
		end
		picker.offset = math.min(math.max(0, selectedRow - 6), math.max(0, #choices - VISIBLE))
		picker:ClearAllPoints()
		picker:SetPoint("TOPRIGHT", frame, "BOTTOMRIGHT", 0, -2)
		picker:SetSize(220, 16 + math.min(VISIBLE, #choices) * ROW_HEIGHT)
		Refresh()
		picker:Show()
	end

	-- a click area over the name of every row (the stock template has none)
	local function Overlay(frame)
		if frame.coaPick then
			return
		end
		local text = frame.text or _G[frame:GetName() .. "Text"]
		local button = CreateFrame("Button", nil, frame)
		button:SetSize(170, 26)
		button:SetPoint("CENTER", text or frame, "CENTER", 0, 0)
		button:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight")
		button:SetScript("OnClick", function() Open(frame) end)
		button:SetScript("OnEnter", function(self)
			GlueTooltip:SetOwner(self, "ANCHOR_RIGHT")
			GlueTooltip:SetText("Click: choose from the list", 1, 1, 1)
			GlueTooltip:Show()
		end)
		button:SetScript("OnLeave", function() GlueTooltip:Hide() end)
		frame.coaPick = button
	end

	local ticker = CreateFrame("Frame")
	ticker:SetScript("OnUpdate", function(self, elapsed)
		self.elapsed = (self.elapsed or 0) + elapsed
		if self.elapsed < 0.5 then
			return
		end
		self.elapsed = 0
		for _, name in ipairs(ROWS) do
			local frame = _G[name]
			if frame then
				frame.coaEsteria = name:find("CoA") and true or nil
				Overlay(frame)
			end
		end
	end)

	local randomize = CharacterCreate_Randomize
	function CharacterCreate_Randomize(...)
		if picker then
			picker:Hide()
		end
		return randomize(...)
	end
end
-- end customization choice list

-- Local server: Jinyu id
CHAR_CREATE_RACE_IDS["Jinyu"] = {78}

-- Local server: Esteria package races
CHAR_CREATE_RACE_IDS["Darkfallen"] = {80, 81}
CHAR_CREATE_RACE_IDS["Sethrak"] = {82}
CHAR_CREATE_RACE_IDS["Gnoll"] = {83}
CHAR_CREATE_RACE_IDS["Saberon"] = {97}
CHAR_CREATE_RACE_IDS["Kyrian"] = {107}
CHAR_CREATE_RACE_IDS["Venthyr"] = {101}
CHAR_CREATE_RACE_IDS["Slime Giant"] = {104}
CHAR_CREATE_RACE_IDS["Centaur"] = {75, 102}
CHAR_CREATE_RACE_IDS["Hozen"] = {112}
CHAR_CREATE_RACE_IDS["Faceless One"] = {87}
CHAR_CREATE_RACE_IDS["Banshee"] = {90}
CHAR_CREATE_RACE_IDS["Sylvari"] = {110}
CHAR_CREATE_RACE_IDS["Mantid"] = {106}
if CHAR_CREATE_MALE_ONLY_RACES then CHAR_CREATE_MALE_ONLY_RACES["Mantid"] = true end
CHAR_CREATE_RACE_IDS["Ethereal"] = {109}
if CHAR_CREATE_MALE_ONLY_RACES then CHAR_CREATE_MALE_ONLY_RACES["Ethereal"] = true end
if CHAR_CREATE_MALE_ONLY_RACES then CHAR_CREATE_MALE_ONLY_RACES["Hozen"] = true end
if CHAR_CREATE_MALE_ONLY_RACES then CHAR_CREATE_MALE_ONLY_RACES["Slime Giant"] = true end
CHAR_CREATE_RACE_IDS["Grummle"] = {103}
if CHAR_CREATE_MALE_ONLY_RACES then CHAR_CREATE_MALE_ONLY_RACES["Gnoll"] = true; CHAR_CREATE_MALE_ONLY_RACES["Saberon"] = true end

-- Local server: Arakkoa (pack race 45 remapped; 45 is Leper Troll)
CHAR_CREATE_RACE_IDS["Arakkoa"] = {79}
if CHAR_CREATE_EXTRA_RACE_ICONS then CHAR_CREATE_EXTRA_RACE_ICONS["Arakkoa"] = "Interface\\Icons\\Race_Arakkoa_Male" end
if CHAR_CREATE_COA_RACIALS then CHAR_CREATE_COA_RACIALS[79] = {
{"Outcast's Curse", "spell_shadow_antishadow", "passive", "The curse of Sethe has hardened you against the dark."},
{"Feathered Descent", "inv_feather_08", "passive", "Your feathers slow your fall."},
{"Lore of Skettis", "inv_misc_book_07", "passive", "The secrets of Skettis sharpen your mind."},
{"Flight of the Raven", "ability_hunter_pet_owl", "active", "Movement speed increased for a short time."},
{"Skyward Glide", "ability_druid_flightform", "active", "Leap into the air and glide on spread wings."}
} end

-- Hide-races panel: WoW dialog look. Hidden list is kept in memory and written to CVars/C_Config when those APIs exist.
do
	local CVAR1, CVAR2 = 'coaHiddenRaces', 'coaHiddenRaces2'
	pcall(RegisterCVar, CVAR1, '')
	pcall(RegisterCVar, CVAR2, '')
	local saved = {}
	local loaded = false

	local function ReadCVar(name)
		local ok, v = pcall(GetCVar, name)
		if ok and v and v ~= '' and v ~= '0' then
			return v
		end
		return ''
	end

	local function WriteCVar(name, value)
		value = value or ''
		pcall(SetCVar, name, value)
		if C_Config then
			if C_Config.SetStringConfig then
				pcall(C_Config.SetStringConfig, name, value)
			end
			if C_Config.SetConfig then
				pcall(C_Config.SetConfig, name, value)
			end
			if C_Config.Save then
				pcall(C_Config.Save)
			end
		end
	end

	local function ParseHidden(raw, into)
		if not raw or raw == '' then
			return
		end
		for token in string.gmatch(raw, '[^|;,]+') do
			token = strtrim(token)
			if token ~= '' then
				into[token] = true
				into[strlower(token)] = true
			end
		end
	end

	local function LoadSaved()
		if loaded then
			return
		end
		loaded = true
		ParseHidden(ReadCVar(CVAR1)..ReadCVar(CVAR2), saved)
		if C_Config and C_Config.GetStringConfig then
			local ok, extra = pcall(C_Config.GetStringConfig, CVAR1)
			if ok then
				ParseHidden(extra, saved)
			end
		end
	end

	function CharacterCreate_GetHiddenRaceSet()
		LoadSaved()
		local copy = {}
		for k, v in pairs(saved) do
			if v then
				copy[k] = true
			end
		end
		return copy
	end

	function CharacterCreate_SaveHiddenRaces(set)
		saved = {}
		for k, v in pairs(set or {}) do
			if v and k and k ~= '' then
				saved[k] = true
				saved[strlower(k)] = true
			end
		end
		loaded = true
		local ids, names = {}, {}
		for k in pairs(saved) do
			if string.sub(k, 1, 1) == '#' then
				tinsert(ids, k)
			elseif k ~= strlower(k) or not tonumber(k) then
				if string.sub(k, 1, 1) ~= '#' then
					tinsert(names, k)
				end
			end
		end
		table.sort(ids)
		table.sort(names)
		local s = table.concat(ids, '|')
		if #names > 0 then
			if s ~= '' then s = s..'|' end
			s = s..table.concat(names, '|')
		end
		WriteCVar(CVAR1, s:sub(1, 240))
		WriteCVar(CVAR2, s:sub(241, 480))
	end

	local function RaceEntries()
		local n = CharacterCreate.numRaces or 0
		local rows = {}
		for i = 1, n do
			local button = _G['CharCreateRaceButton'..i]
			local name = button and button.Text and button.Text:GetText()
			if name and name ~= '' then
				local ok, _, faction = pcall(GetFactionForRace, i)
				local ids = CHAR_CREATE_RACE_IDS and CHAR_CREATE_RACE_IDS[name]
				local raceID = button.bonusRaceID or (ids and ids[1]) or i
				tinsert(rows, { id = i, raceID = raceID, name = name, faction = (ok and faction) or 'Alliance' })
			end
		end
		return rows
	end

	local function MakeGlueButton(parent, name, text, w, h)
		local b = CreateFrame('Button', name, parent)
		b:SetSize(w or 96, h or 24)
		b:SetNormalTexture('Interface/Glues/Common/Glue-Panel-Button-Up')
		b:SetPushedTexture('Interface/Glues/Common/Glue-Panel-Button-Down')
		b:SetHighlightTexture('Interface/Glues/Common/Glue-Panel-Button-Highlight')
		local nt = b:GetNormalTexture()
		if nt then nt:SetTexCoord(0, 0.578125, 0, 0.75) end
		local pt = b:GetPushedTexture()
		if pt then pt:SetTexCoord(0, 0.578125, 0, 0.75) end
		local ht = b:GetHighlightTexture()
		if ht then ht:SetTexCoord(0, 0.578125, 0, 0.75); ht:SetBlendMode('ADD') end
		local fs = b:CreateFontString(nil, 'OVERLAY', 'GlueFontNormal')
		fs:SetPoint('CENTER')
		b:SetFontString(fs)
		b:SetText(text)
		return b
	end

	function CharacterCreate_EnsureRaceFilterButton()
		local parent = CharacterCreateFrame or CharacterCreate
		local btn = CharCreateRaceFilterButton
		if not btn then
			btn = MakeGlueButton(parent, 'CharCreateRaceFilterButton', 'Hide Races', 120, 28)
			btn:SetFrameStrata('HIGH')
			btn:SetScript('OnClick', function()
				PlaySound('gsTitleOptionOK')
				CharacterCreate_ToggleRaceFilter()
			end)
		end
		btn:ClearAllPoints()
		if CharCreateHelpChooseClassButton then
			btn:SetPoint('BOTTOM', CharCreateHelpChooseClassButton, 'TOP', 0, 16)
		elseif CharacterCreateFrame then
			btn:SetPoint('BOTTOMLEFT', CharacterCreateFrame, 'BOTTOMLEFT', 40, 126)
		end
		btn:Show()
	end

	local filter
	local ROW_H, VISIBLE = 20, 18

	local function UpdateScrollBar()
		local f = filter
		if not f or not f.thumb then
			return
		end
		local total = #(f.entries or {})
		local maxOff = math.max(0, total - VISIBLE)
		local trackH = f.track:GetHeight() or 1
		local thumbH = 24
		f.thumb:SetHeight(thumbH)
		local travel = math.max(0, trackH - thumbH)
		local y = (maxOff > 0) and (-(f.offset or 0) / maxOff * travel) or 0
		f.thumb:ClearAllPoints()
		f.thumb:SetPoint('TOP', f.track, 'TOP', 0, y)
		if f.scrollUp then
			if (f.offset or 0) <= 0 then f.scrollUp:Disable() else f.scrollUp:Enable() end
		end
		if f.scrollDown then
			if (f.offset or 0) >= maxOff then f.scrollDown:Disable() else f.scrollDown:Enable() end
		end
	end

	local function EnsureFilterFrame()
		if filter then
			return filter
		end
		local parent = CharacterCreateFrame or CharacterCreate
		local f = CreateFrame('Frame', 'CharCreateRaceFilterFrame', parent)
		f:SetSize(460, 528)
		f:SetPoint('CENTER', parent, 'CENTER', 0, 8)
		f:SetFrameStrata('FULLSCREEN_DIALOG')
		pcall(function()
			f:SetBackdrop({
				bgFile = 'Interface/DialogFrame/UI-DialogBox-Background',
				edgeFile = 'Interface/DialogFrame/UI-DialogBox-Border',
				tile = true, tileSize = 32, edgeSize = 32,
				insets = { left = 11, right = 12, top = 12, bottom = 11 },
			})
		end)
		f:EnableMouse(true)
		f:Hide()

		local header = f:CreateTexture(nil, 'ARTWORK')
		header:SetTexture('Interface/DialogFrame/UI-DialogBox-Header')
		header:SetSize(320, 64)
		header:SetPoint('TOP', 0, 12)
		local title = f:CreateFontString(nil, 'OVERLAY', 'GlueFontNormal')
		title:SetPoint('TOP', header, 'TOP', 0, -14)
		title:SetText('|cffffd200Hide Races')

		local sub = f:CreateFontString(nil, 'OVERLAY', 'GlueFontNormal')
		sub:SetPoint('TOP', 0, -42)
		sub:SetWidth(340)
		sub:SetText('|cffffd200Uncheck a race to hide it on this screen.|nSave Preference keeps your list after you close the client.')

		local list = CreateFrame('Frame', nil, f)
		pcall(function()
			list:SetBackdrop({
				bgFile = 'Interface/Tooltips/UI-Tooltip-Background',
				edgeFile = 'Interface/Tooltips/UI-Tooltip-Border',
				tile = true, tileSize = 16, edgeSize = 16,
				insets = { left = 4, right = 4, top = 4, bottom = 4 },
			})
			list:SetBackdropColor(0, 0, 0, 0.85)
			list:SetBackdropBorderColor(0.6, 0.5, 0.3, 1)
		end)
		list:SetPoint('TOPLEFT', 22, -78)
		list:SetPoint('BOTTOMRIGHT', -44, 56)
		f.list = list
		f.rows = {}
		f.offset = 0
		for i = 1, VISIBLE do
			local row = CreateFrame('CheckButton', 'CharCreateRaceFilterRow'..i, list)
			row:SetSize(20, 20)
			row:SetNormalTexture('Interface/Buttons/UI-CheckBox-Up')
			row:SetPushedTexture('Interface/Buttons/UI-CheckBox-Down')
			row:SetHighlightTexture('Interface/Buttons/UI-CheckBox-Highlight')
			row:SetCheckedTexture('Interface/Buttons/UI-CheckBox-Check')
			row:SetPoint('TOPLEFT', 8, -6 - (i - 1) * ROW_H)
			local label = row:CreateFontString(nil, 'ARTWORK', 'GlueFontNormal')
			label:SetPoint('LEFT', row, 'RIGHT', 4, 0)
			label:SetJustifyH('LEFT')
			row.label = label
			row:SetScript('OnClick', function(self)
				if not f.draft or not self.raceName then
					return
				end
				local key = '#'..tostring(self.raceId or 0)
				if self:GetChecked() then
					f.draft[self.raceName] = nil
					f.draft[strlower(self.raceName)] = nil
					f.draft[key] = nil
				else
					f.draft[self.raceName] = true
					f.draft[key] = true
				end
			end)
			f.rows[i] = row
		end

		local function SkinScrollButton(b, up)
			local prefix = up and 'Interface/Buttons/UI-ScrollBar-ScrollUpButton-' or 'Interface/Buttons/UI-ScrollBar-ScrollDownButton-'
			b:SetNormalTexture(prefix..'Up')
			b:SetPushedTexture(prefix..'Down')
			b:SetDisabledTexture(prefix..'Disabled')
			b:SetHighlightTexture('Interface/Buttons/UI-Common-MouseHilight')
			local ht = b:GetHighlightTexture()
			if ht then ht:SetBlendMode('ADD') end
		end

		local scroll = CreateFrame('Frame', nil, f)
		scroll:SetWidth(22)
		scroll:SetPoint('TOPRIGHT', list, 'TOPRIGHT', 0, 0)
		scroll:SetPoint('BOTTOMRIGHT', list, 'BOTTOMRIGHT', 0, 0)

		local up = CreateFrame('Button', nil, scroll)
		up:SetSize(18, 16)
		up:SetPoint('TOP', 0, 2)
		SkinScrollButton(up, true)

		local down = CreateFrame('Button', nil, scroll)
		down:SetSize(18, 16)
		down:SetPoint('BOTTOM', 0, -2)
		SkinScrollButton(down, false)

		local track = CreateFrame('Button', nil, scroll)
		track:SetPoint('TOPLEFT', up, 'BOTTOMLEFT', 2, 0)
		track:SetPoint('BOTTOMRIGHT', down, 'TOPRIGHT', -2, 0)
		local trackTop = track:CreateTexture(nil, 'BACKGROUND')
		trackTop:SetTexture('Interface/PaperDollInfoFrame/UI-Character-ScrollBar')
		trackTop:SetTexCoord(0, 0.45, 0, 0.2)
		trackTop:SetPoint('TOPLEFT', -1, 0)
		trackTop:SetPoint('TOPRIGHT', 1, 0)
		trackTop:SetHeight(24)
		local trackBot = track:CreateTexture(nil, 'BACKGROUND')
		trackBot:SetTexture('Interface/PaperDollInfoFrame/UI-Character-ScrollBar')
		trackBot:SetTexCoord(0, 0.45, 0.82, 1)
		trackBot:SetPoint('BOTTOMLEFT', -1, 0)
		trackBot:SetPoint('BOTTOMRIGHT', 1, 0)
		trackBot:SetHeight(24)
		local trackMid = track:CreateTexture(nil, 'BACKGROUND')
		trackMid:SetTexture('Interface/PaperDollInfoFrame/UI-Character-ScrollBar')
		trackMid:SetTexCoord(0, 0.45, 0.2, 0.82)
		trackMid:SetPoint('TOPLEFT', trackTop, 'BOTTOMLEFT')
		trackMid:SetPoint('BOTTOMRIGHT', trackBot, 'TOPRIGHT')

		local thumb = CreateFrame('Button', nil, track)
		thumb:SetSize(18, 24)
		thumb:SetNormalTexture('Interface/Buttons/UI-ScrollBar-Knob')
		local nt = thumb:GetNormalTexture()
		if nt then nt:SetTexCoord(0.2, 0.8, 0.125, 0.875) end
		f.track = track
		f.thumb = thumb
		f.scrollUp = up
		f.scrollDown = down

		local function ScrollTo(off)
			local maxOff = math.max(0, #(f.entries or {}) - VISIBLE)
			f.offset = math.max(0, math.min(maxOff, off))
			CharacterCreate_RefreshRaceFilterRows()
		end

		list:EnableMouseWheel(true)
		list:SetScript('OnMouseWheel', function(_, delta)
			ScrollTo((f.offset or 0) - delta)
		end)
		track:EnableMouseWheel(true)
		track:SetScript('OnMouseWheel', function(_, delta)
			ScrollTo((f.offset or 0) - delta)
		end)
		up:SetScript('OnClick', function()
			pcall(PlaySound, 'UChatScrollButton')
			ScrollTo((f.offset or 0) - 1)
		end)
		down:SetScript('OnClick', function()
			pcall(PlaySound, 'UChatScrollButton')
			ScrollTo((f.offset or 0) + 1)
		end)
		track:SetScript('OnClick', function(_, _, _, _, y)
			local maxOff = math.max(0, #(f.entries or {}) - VISIBLE)
			if maxOff <= 0 then
				return
			end
			local h = track:GetHeight() or 1
			local _, cy = GetCursorPosition()
			local scale = track:GetEffectiveScale() or 1
			local top = select(2, track:GetCenter()) + h / 2
			-- fallback: jump one page
			ScrollTo((f.offset or 0) + VISIBLE)
		end)
		thumb:RegisterForDrag('LeftButton')
		thumb:SetScript('OnDragStart', function(self)
			self.dragging = true
			self:SetScript('OnUpdate', function()
				if not self.dragging then
					return
				end
				local h = track:GetHeight() or 1
				local thumbH = self:GetHeight() or 24
				local travel = math.max(1, h - thumbH)
				local _, cy = GetCursorPosition()
				local scale = track:GetEffectiveScale() or 1
				cy = cy / scale
				local top = select(4, track:GetPoint(1))
				local bottom = (track:GetBottom() or 0)
				local topY = (track:GetTop() or 0)
				local rel = (topY - cy - thumbH / 2) / travel
				local maxOff = math.max(0, #(f.entries or {}) - VISIBLE)
				ScrollTo(math.floor(rel * maxOff + 0.5))
			end)
		end)
		thumb:SetScript('OnDragStop', function(self)
			self.dragging = nil
			self:SetScript('OnUpdate', nil)
		end)

		local function SetAllChecked(checked)
			f.draft = {}
			if not checked then
				for _, info in ipairs(f.entries or {}) do
					f.draft['#'..info.id] = true
					f.draft[info.name] = true
				end
			end
			CharacterCreate_RefreshRaceFilterRows()
		end

		local save = MakeGlueButton(f, nil, 'Save Preference', 124, 24)
		save:SetPoint('BOTTOMLEFT', 16, 18)
		save:SetScript('OnClick', function()
			PlaySound('gsTitleOptionOK')
			CharacterCreate_SaveHiddenRaces(f.draft or {})
			f:Hide()
			CharacterCreate_LayoutRaceButtons(CharacterCreate.numRaces or 0)
		end)
		local checkAll = MakeGlueButton(f, nil, 'Check All', 86, 24)
		checkAll:SetPoint('LEFT', save, 'RIGHT', 8, 0)
		checkAll:SetScript('OnClick', function()
			PlaySound('gsTitleOptionOK')
			SetAllChecked(true)
		end)
		local uncheckAll = MakeGlueButton(f, nil, 'Uncheck All', 96, 24)
		uncheckAll:SetPoint('LEFT', checkAll, 'RIGHT', 8, 0)
		uncheckAll:SetScript('OnClick', function()
			PlaySound('gsTitleOptionOK')
			SetAllChecked(false)
		end)
		local close = MakeGlueButton(f, nil, 'Cancel', 80, 24)
		close:SetPoint('LEFT', uncheckAll, 'RIGHT', 8, 0)
		close:SetScript('OnClick', function()
			PlaySound('gsTitleOptionExit')
			f:Hide()
		end)
		filter = f
		return f
	end

	function CharacterCreate_RefreshRaceFilterRows()
		local f = filter
		if not f then
			return
		end
		local entries = f.entries or {}
		for i = 1, VISIBLE do
			local row = f.rows[i]
			local info = entries[i + (f.offset or 0)]
			if info then
				row.raceName = info.name
				row.raceId = info.id
				row.label:SetText((info.faction == 'Horde' and '|cffff2020' or '|cff00a0ff')..info.name..' |cffaaa37a(ID '..info.raceID..')|r')
				local hidden = f.draft and (f.draft['#'..info.id] or f.draft[info.name] or f.draft[strlower(info.name)])
				row:SetChecked(not hidden)
				row:Show()
			else
				row.raceName = nil
				row.raceId = nil
				row:Hide()
			end
		end
		UpdateScrollBar()
	end

	function CharacterCreate_ToggleRaceFilter()
		local f = EnsureFilterFrame()
		if f:IsShown() then
			f:Hide()
			return
		end
		f.entries = RaceEntries()
		f.draft = CharacterCreate_GetHiddenRaceSet()
		f.offset = 0
		CharacterCreate_RefreshRaceFilterRows()
		f:Show()
	end

	local origClick = CharacterRace_OnClick
	function CharacterRace_OnClick(self, id, forceSelect)
		local btn = id and _G['CharCreateRaceButton'..id]
		if btn and btn.coaUserHidden then
			for i = 1, (CharacterCreate.numRaces or 0) do
				local b = _G['CharCreateRaceButton'..i]
				if b and not b.coaUserHidden then
					self, id = b, i
					break
				end
			end
		end
		return origClick(self, id, forceSelect)
	end
end

-- Local server: Esteria Drakkin / Drakonid (Esteria racial port, 2026-10-16)
CHAR_CREATE_RACE_IDS["Drakkin"] = {99}
CHAR_CREATE_RACE_IDS["Drakonid"] = {100, 98}
if CHAR_CREATE_MALE_ONLY_RACES then
	CHAR_CREATE_MALE_ONLY_RACES["Drakonid"] = true
end
if CHAR_CREATE_EXTRA_RACE_ICONS then
	CHAR_CREATE_EXTRA_RACE_ICONS["Drakkin"] = "Interface\\Glues\\CharacterCreate\\ui-charactercreate-drakkinalliancemale"
	CHAR_CREATE_EXTRA_RACE_ICONS["Drakonid"] = "Interface\\Glues\\CharacterCreate\\ui-charactercreate-drakonidalliancemale"
end

-- Race lore shown at the top of the race tooltip (Alliance01.docx / Horde01.docx), keyed by faction then displayed race name
CHAR_CREATE_RACE_LORE = {
	alliance = {
		["Worgen"] = [==[Once proud citizens of the isolated kingdom of Gilneas, the worgen are humans afflicted by an ancient curse rooted in the wild druidic powers of the night elves. Transformed into savage, wolf-like creatures, many initially surrendered to their feral instincts before learning to reclaim their humanity through the aid of night elven druids. Though the curse has forever changed them, the worgen retain the cunning and determination of their human heritage, wielding their newfound strength and predatory instincts against those who threaten their people. Neither wholly man nor beast, they walk a precarious path between civilization and savagery.]==],
		["High Elf"] = [==[Descendants of the ancient exiles who founded Quel'Thalas, the High Elves once flourished beneath the light of the Sunwell. When the Scourge devastated their homeland, most survivors took the name Blood Elves to honor their fallen. Those who retained their ancient identity remained loyal to the Alliance, preserving their traditions even as their people became divided. Few in number but steadfast in purpose, these proud elves now fight alongside their allies to defend Azeroth and secure a future for their scattered kin.]==],
		["Tuskarr"] = [==[For generations, the hardy tuskarr have made their homes along the frozen shores of Northrend, enduring bitter winds and relentless winters where few others could survive. These peaceful but resilient fisher-folk live in close-knit communities, honoring their ancestors and the spirits of the land and sea. Though they prefer fishing and trade to warfare, the tuskarr are formidable defenders when their families and villages are threatened. Guided by ancient traditions, a deep respect for nature, and an unshakable sense of community, these steadfast wanderers now venture beyond their icy homeland to discover what awaits them in the wider world.]==],
		["Kul Tiran"] = [==[Hailing from the maritime kingdom of Kul Tiras, these hardy humans descend from settlers who braved the Great Sea to establish one of Azeroth's mightiest naval powers. Renowned for their formidable fleets, skilled shipwrights, and fearless sailors, the Kul Tirans have long defended their shores against pirates, sea monsters, and foreign invaders. Though political turmoil and bitter divisions once threatened to tear their kingdom apart, their enduring spirit and fierce independence have carried them through countless storms. Proud of their seafaring heritage and loyal to their homeland, these rugged adventurers now carry the strength and traditions of Kul Tiras to distant shores.]==],
		["Vrykul"] = [==[Forged by the titans to serve as mighty guardians of Azeroth, the vrykul are an ancient race of towering warriors whose bloodlines gave rise to humanity. Once servants of the titan-forged, they fell victim to the Curse of Flesh and established fierce clans throughout the frozen reaches of Northrend. Renowned for their strength, seafaring prowess, and reverence for glorious battle, the vrykul honor their ancestors and seek worthy deaths in combat. Though many have sworn allegiance to dark powers, others follow their own paths, driven by honor, ambition, and the promise of legendary deeds worthy of the sagas of old.]==],
		["Broken"] = [==[Once proud draenei of Argus, the Broken are survivors of the Burning Legion's relentless campaign against their people. Twisted by exposure to fel corruption and dark magic, their bodies were forever changed, and many lost their connection to the Holy Light. Shunned by their unaltered kin and hunted by their enemies, these resilient outcasts forged new lives amid the shattered remnants of Draenor. Though some turned to shamanism and the elemental spirits for guidance, others continue to seek the Light they once knew. Scarred but unbroken in spirit, they now venture beyond their troubled homeland, determined to reclaim their place in the world.]==],
		["Pandaren"] = [==[Hailing from the mist-shrouded lands of Pandaria and the wandering isle of Shen-zin Su, the pandaren are a wise and spirited people who cherish harmony, family, and the simple pleasures of life. Having endured centuries beneath the cruel rule of the mogu, they learned to overcome oppression through patience, ingenuity, and mastery of martial arts. Though traditionally reluctant to involve themselves in foreign conflicts, some have embraced the Alliance, finding kinship in its ideals of honor, loyalty, and cooperation. Guided by ancient wisdom and an adventurous spirit, these wandering heroes seek to bring balance to a world too often consumed by war.]==],
		["Murloc"] = [==[Dwelling along the shores, rivers, and hidden waterways of Azeroth, the murlocs are an ancient race of amphibious creatures whose strange customs and gurgling speech have long bewildered outsiders. Fiercely territorial and devoted to their tribes, these resourceful creatures rely on overwhelming numbers, primitive weapons, and an instinctive mastery of their aquatic surroundings. Though often dismissed as mindless pests, murlocs possess complex social bonds, spiritual traditions, and a surprising capacity for cooperation. While most remain wary of outsiders, a few adventurous members of their kind have ventured beyond their familiar waters, forging unlikely friendships and discovering that the world holds treasures far greater than fish.]==],
		["Forgotten"] = [==[Not all heroes are born with the strength of a warrior or the stature of a champion. Found among the crowded cities, remote villages, and rugged frontiers of Azeroth, these lean and wiry humans have learned to survive through cunning, agility, and sheer determination. Often underestimated for their slight frames, they possess a resilience forged through hardship and an uncanny talent for escaping danger. Whether driven by ambition, necessity, or the promise of a better life, these unlikely adventurers prove that courage is measured not by the size of one's body, but by the strength of one's spirit.]==],

		["Alpha Worgen"] = [==[Long before the curse spread through Gilneas, the first worgen emerged from the forbidden practices of the Druids of the Pack, who sought to harness the savage fury of the wolf Ancient Goldrinn. Unable to master their feral instincts, these ancient druids were transformed into fearsome beasts and imprisoned within the Emerald Dream. Unlike their Gilnean descendants, these primal worgen retain a deeper connection to the untamed power that first gave rise to their kind. Fierce, instinctive, and fiercely independent, they now roam Azeroth in search of freedom, purpose, and a world where neither man nor beast commands their destiny.]==],
		["Dark Iron Dwarf"] = [==[Deep beneath Blackrock Mountain, the Dark Iron dwarves forged a kingdom of fire and shadow, where ambition and ruthless determination became the foundations of their society. Descendants of the ancient Ironforge clans, they were driven into exile following the War of the Three Hammers, only to fall beneath the fiery dominion of Ragnaros the Firelord. For generations, their mastery of sorcery, engineering, and the forge made them feared enemies of their dwarven kin. Now freed from the Firelord's tyranny, these fiercely independent dwarves seek to reclaim their honor and forge a new legacy, proving that even the darkest flames can temper unbreakable steel.]==],
		["Furbolg"] = [==[Deep within the ancient forests of Kalimdor, the furbolgs have long lived in harmony with the natural world, honoring the spirits of the wild and the great bear Ancient Ursoc. Once peaceful guardians of their woodland homes, many tribes were driven to madness by the Burning Legion's corruption, turning against the very forests they had sworn to protect. Yet others resisted the darkness, preserving their ancestral traditions and enduring bonds with the night elves. Fierce in battle but deeply spiritual, these steadfast defenders now venture beyond their sacred groves, determined to heal the wounds of their homeland and protect the balance of nature.]==],
		["Dracthyr"] = [==[Created by the black Dragon Aspect Neltharion, the dracthyr are an ancient race of dragonkin forged to combine the power of dragons with the adaptability of mortal races. Trained as formidable soldiers and gifted with the magic of the five dragonflights, they were intended to serve as an unstoppable army. Yet their creator's fear of losing control led to their imprisonment within the Forbidden Reach, where they remained dormant for thousands of years. Now awakened to a world vastly different from the one they knew, these proud warriors seek to reclaim their freedom, uncover the truth of their origins, and forge destinies beyond their creator's designs.]==],
		["Void Elf"] = [==[Once blood elves of Silvermoon, the void elves are outcasts who dared to explore the forbidden mysteries of the Void. Led by the magister Umbric, their pursuit of forbidden knowledge nearly consumed them before Alleria Windrunner intervened, helping them master the shadowy powers that threatened their very existence. Exiled from Quel'Thalas and rejected by their former kin, these transformed elves found a new home among the Alliance. Though the whispers of the Void constantly threaten their sanity, they wield its dangerous energies with unwavering determination, seeking to prove that even the darkest powers can serve a greater purpose.]==],
		["Illidari Night Elf"] = [==[Once proud kaldorei, these night elves sacrificed their former lives to join Illidan Stormrage's relentless war against the Burning Legion. Through forbidden rituals, they bound demonic essence to their very souls, gaining extraordinary power at the cost of their sight, their innocence, and often their sanity. Feared and shunned by their own people, the Illidari endured unimaginable suffering to become the very weapons their enemies would dread. Though the Legion's defeat has left many searching for a new purpose, these fierce warriors remain ever vigilant, determined that their sacrifices will not have been in vain.]==],
		["Lightforged Draenei"] = [==[For thousands of years, the Lightforged draenei waged an unrelenting war against the Burning Legion, carrying the Holy Light into the darkest reaches of the Great Dark Beyond. Guided by the naaru and united beneath the banner of the Army of the Light, these devoted warriors endured sacred trials that infused their very bodies with radiant power. Though countless worlds fell before the Legion's armies, the Lightforged never abandoned their sacred mission. With their ancient enemy defeated, these steadfast champions now stand alongside the Alliance, determined to defend Azeroth against the darkness and bring hope to a universe scarred by war.]==],
		["Mechagnome"] = [==[Hidden away on the mechanical island of Mechagon, the mechagnomes are ingenious descendants of the gnomes who sought to overcome the limitations of flesh through technological advancement. Replacing their mortal limbs with intricate machinery, they embraced a future where invention and ingenuity could conquer nearly any obstacle. Yet their pursuit of perfection nearly became their undoing when King Mechagon sought to forcibly transform all living creatures into machines. Freed from his tyranny, these resourceful inventors now venture beyond their isolated homeland, determined to prove that the greatest innovations come not from abandoning their humanity, but from embracing the limitless possibilities of both flesh and steel.]==],
		["Earthen"] = [==[Forged from living stone by the titan-forged keepers, the earthen are an ancient people created to shape and safeguard the depths of Azeroth. For countless ages, they labored beneath mountains and within forgotten vaults, faithfully carrying out the duties entrusted to them by their makers. While many of their kind eventually succumbed to the Curse of Flesh, giving rise to the dwarves, others endured, preserving their stonebound forms and ancient traditions. Now, as the world changes and the purpose of their creation is called into question, these steadfast guardians venture beyond their ancestral halls, determined to forge destinies of their own.]==],
		["Haranir"] = [==[Hidden for countless generations within the ancient roots of Azeroth, the haranir are a mysterious people whose lives are deeply entwined with the world's primal energies. Dwelling in the shadowed depths beneath the surface, they guard sacred places and preserve traditions largely forgotten by the peoples above. Guided by their reverence for the roots of the world, these secretive guardians wield ancient forms of nature magic to protect the delicate balance of life. Though long wary of outsiders, some have begun emerging from their hidden sanctuaries, driven by curiosity, duty, and a growing determination to defend the world they have watched over for ages.]==],
		["Jinyu"] = [==[Dwelling among the rivers, lakes, and mist-covered shores of Pandaria, the jinyu are an ancient race of aquatic humanoids whose wisdom flows as deeply as the waters they call home. Evolved from humble murloc ancestors through the mysterious waters of the Vale of Eternal Blossoms, they developed a sophisticated civilization guided by tradition, spiritual insight, and reverence for the natural world. Their revered waterspeakers commune with rivers and streams, seeking guidance in their ever-changing currents. Though long content to remain within their secluded communities, these thoughtful and courageous people now venture beyond Pandaria, carrying the wisdom of the waters into an uncertain world.]==],
		["Arakkoa"] = [==[Hailing from the ancient spires and shadowed forests of Draenor, the arakkoa are a proud and mysterious race whose civilization was shaped by devotion to the gods of sun and shadow. Once divided between the winged adherents of Rukhmar and the cursed outcasts cast from their lofty cities, their history is steeped in religious conflict, betrayal, and forbidden magic. Though stripped of their ability to fly, the outcasts endured, mastering shadowy arts and preserving the forgotten wisdom of their ancestors. Now these cunning and resourceful survivors venture beyond their shattered homeland, seeking freedom, knowledge, and a future no longer dictated by the sins of their past.]==],
		["Ethereal"] = [==[Bound in bandages of woven stormlight, the ethereal drift between the worlds as traders of secrets and favours. Their bodies long since turned to energy, they walk the Alliance's roads to learn what the living have forgotten.]==],
		["Sylvari"] = [==[Children of the deep groves, the Sylvari are antlered wanderers whose hooves leave no mark on the moss. Gentle with the wild and merciless to those who burn it, they walk out of the old forests to stand with the Alliance and keep the green places from the axe.]==],
		["Mantid"] = [==[Chitin-shelled and tireless, the mantid swarm out of the thorned fields of the Kun-Lai in endless broods bound by the will of their hive. A few, driven off by war or called by the Horde, now carry the ancient claw and the old harvest hunger into the wider world.]==],
		["Banshee"] = [==[Hand-painted spirits of the restless dead, the banshee wail on the wind between the grave and the world of the living. Freed from the Lich King and the Forsaken alike, they carry old sorrows and older grudges to the Horde.]==],
		["Faceless One"] = [==[Servants of the Old Gods, the faceless ones have given up name, face and will to something older than the world. Shadow-drawn and unreadable, they walk with the Horde as guests of the void.]==],
		["Hozen"] = [==[Swift, sharp-tongued and fiercely loyal to their kin, the hozen swing out of the jungles of Pandaria as traders, scouts and brawlers. Quick to laugh and quicker to pick a fight, they have learned that an alliance with the other peoples of Azeroth is the best way to keep their families fed and safe.]==],
		["Centaur"] = [==[Half horse, half warrior, the centaur roam the open plains in clans bound by oath and hoofbeat. Swift, proud and fiercely free, they ride out of the wilds to see which of the other peoples are worth running beside.]==],
		["Slime Giant"] = [==[A towering mass of living slime wrapped around a restless skeleton, the slime giants are born where ooze and the dead meet. Slow to anger and hard to stop, they lumber out of the sewers and the swamps to find a place among the living.]==],
		["Kyrian"] = [==[Winged shepherds of the afterlife, the kyrian guide the souls of the worthy to their rest. Long bound to the discipline of the Bastion, a few of them now walk the living world, carrying their ancient duty of mercy and judgement with them.]==],
		["Venthyr"] = [==[Proud nobles of the shadowed halls of Revendreth, the venthyr rule through lineage, pride and a hunger that never fades. Some have left their dark courts for the open world, where their old grudges and appetites follow them.]==],
		["Sethrak"] = [==[Hailing from the unforgiving deserts of Vol'dun, the sethrak are an ancient race of serpentine people whose civilization was built upon devotion to the serpent loa Sethraliss. Once united in faith, their society was torn apart when the ruthless Faithless turned against their own kind, seeking power through conquest and dark alliances. Those who remained loyal to Sethraliss endured persecution and exile, preserving their traditions against overwhelming odds. Now, these resourceful and resilient survivors venture beyond the sands of Vol'dun, determined to forge their own destiny in a changing world.]==],
		["Saberon"] = [==[Stalking the untamed wilderness of Draenor, the saberon are a fierce race of feline hunters whose strength, cunning, and predatory instincts make them formidable adversaries. Organized into territorial prides, these savage creatures rely on stealth, sharpened claws, and lightning-fast reflexes to overwhelm their prey. Though often regarded as little more than bloodthirsty beasts, the saberon possess a primal intelligence and an instinctive understanding of the natural world. Driven by hunger, curiosity, and the relentless struggle for survival, some have ventured beyond their ancestral hunting grounds, discovering a world filled with unfamiliar dangers, powerful rivals, and prey unlike anything they have encountered before.]==],
	},
	horde = {
		["Goblin"] = [==[Hailing from the bustling trade empires of Kezan, the goblins are a cunning and industrious people whose insatiable ambition is matched only by their talent for invention. Once enslaved by the jungle trolls, they gained extraordinary intellect through exposure to the mysterious mineral kaja'mite, eventually overthrowing their masters and building a civilization fueled by commerce, engineering, and explosive innovation. Governed by powerful trade princes, goblin society prizes wealth, opportunity, and the fine art of making a deal. Whether seeking fortune, adventure, or the next great invention, these resourceful opportunists know that every problem has a solution—and there's probably money to be made finding it.]==],
		["Zandalari Troll"] = [==[From the golden temples of Zuldazar, the Zandalari trolls have ruled one of Azeroth's oldest and most powerful civilizations for thousands of years. Heirs to an empire that once stretched across much of ancient Kalimdor, they preserve the traditions of their ancestors through devotion to the loa, powerful beings who guide their people in matters of faith and war. Though their empire has weathered rebellion, betrayal, and the rise of younger nations, the Zandalari remain fiercely proud of their heritage. Now allied with the Horde, these formidable warriors and mystics venture beyond their sacred homeland, determined to secure their kingdom's future and restore the glory of their ancient empire.]==],
		["Taunka"] = [==[For countless generations, the taunka have endured the frozen wilderness of Northrend, surviving where few others could. Distant kin of the tauren, these rugged and fiercely independent people have been shaped by the merciless winters of the Borean Tundra and Dragonblight. Unlike their southern cousins, who seek harmony with the elements, the taunka learned to bend the elements to their will through sheer determination. Hardened by centuries of hardship and united by an unyielding spirit, they have pledged themselves to the Horde, bringing the strength of the tundra and the fury of the northern storms to its cause.]==],
		["Pandaren"] = [==[Hailing from the mist-shrouded lands of Pandaria, the pandaren are a peaceful yet formidable people whose traditions celebrate harmony, discipline, and the simple pleasures of life. For generations, they lived sheltered from the conflicts of the wider world, perfecting the arts of brewing, cooking, and martial combat. When the mists parted, adventurous pandaren emerged to discover a world divided by war. Those who joined the Horde found kinship in its fierce determination, loyalty, and unyielding spirit. Though their paths may differ from those of their Alliance brethren, these wandering warriors remain guided by the wisdom of their ancestors and the belief that true strength comes from balance within.]==],
		["Naga"] = [==[Once the Highborne elite of Queen Azshara's ancient empire, the naga were transformed into serpentine creatures when the Great Sundering swallowed their civilization beneath the sea. Saved from destruction through a dark bargain with the Old God N'Zoth, they built a vast underwater empire in the depths of Azeroth, preserving their queen's ambitions and their mastery of powerful magic. For ten thousand years, these cunning and ruthless survivors plotted beneath the waves, emerging to reclaim lost treasures, conquer new territories, and serve their queen's designs. Now, as ancient loyalties are tested and new powers rise, some venture beyond the depths to carve out destinies of their own.]==],
		["Fel Orc"] = [==[Twisted by the corrupting power of demon blood, the fel orcs are a terrifying reminder of the Burning Legion's influence over their people. Their crimson skin and monstrous strength bear witness to the terrible price of surrendering to demonic power. Once warriors of Draenor's proud clans, many became savage servants of the Legion, driven by an insatiable bloodlust and bound to the will of their masters. Yet even among these corrupted warriors, the spirit of the orcs endures. Whether seeking redemption, freedom, or greater power, those who have broken from their former masters now walk a dangerous path, struggling to forge their own destiny in a world that remembers what they once became.]==],
		["Forest Troll"] = [==[Once masters of the vast Amani Empire, the forest trolls are a fierce and ancient people whose dominion stretched across much of the Eastern Kingdoms. From the sacred stronghold of Zul'Aman, they waged bitter wars against the high elves and the human kingdoms that encroached upon their ancestral lands. Though their empire was shattered and their territories diminished, the forest trolls have never forgotten the glory of their ancestors or forgiven those who displaced them. Guided by powerful loa and an unyielding determination to reclaim their homeland, these formidable warriors now look beyond their ancient rivalries, seeking new alliances and the strength to restore their people's legacy.]==],
		["Ice Troll"] = [==[Born in the frozen mountains of Dun Morogh, the Frostmane ice trolls are a fierce and resilient people who have endured generations of bitter cold and relentless conflict with the dwarves of Ironforge. Driven from much of their ancestral territory, these hardy hunters survive through cunning, ferocity, and their remarkable regenerative abilities. Though their tribes have been scattered and their numbers diminished, the Frostmane remain fiercely protective of their traditions and determined to reclaim their place in the mountains they call home.]==],
		["Skeleton"] = [==[Stripped of flesh but denied the peace of death, skeletons are the skeletal remains of the fallen, animated once more through the dark arts of necromancy. Once raised as mindless servants of the Scourge, some have broken free of their masters, retaining fragments of the memories and identities they possessed in life. Neither truly living nor comfortably dead, these restless souls now walk among the Forsaken, seeking purpose in an existence that defies the natural order. Whether driven by vengeance, loyalty, or the simple desire to reclaim their own will, they stand as grim reminders that death is not always the end.]==],
		["Drakkari Troll"] = [==[Once masters of the mighty empire of Zul'Drak, the Drakkari trolls ruled the frozen reaches of Northrend for thousands of years. When the Scourge threatened their kingdom, desperate leaders turned against their own loa, sacrificing the gods they had worshipped for generations to steal their power. Their terrible bargain could not save Zul'Drak, and the empire fell into ruin. Now scattered and diminished, the surviving Drakkari carry the strength of their ancestors and the burden of their people's betrayal, determined to endure where their kingdom could not.]==],
		["Murloc"] = [==[Long regarded as little more than a nuisance along Azeroth's shores, the amphibious murlocs are far more resourceful than their enemies suspect. Fiercely territorial and bound by strong tribal loyalties, these creatures have survived countless threats through sheer numbers, remarkable adaptability, and an uncanny ability to overwhelm larger foes. Though their strange language and unpredictable behavior often lead to conflict with other races, some murlocs have ventured beyond their coastal villages, finding unlikely allies among the peoples of the Horde.]==],
		["Mag'har Orc"] = [==[Untainted by the blood of demons, the Mag'har are proud descendants of the ancient orcish clans of Draenor. Their name means "uncorrupted" in the orcish tongue, a testament to their refusal to surrender their heritage to the Burning Legion. Hardened by generations of warfare and guided by the wisdom of their ancestors, these fierce warriors preserve the traditions of their people through strength, honor, and loyalty to their clans. Now fighting alongside their kin in the Horde, the Mag'har seek to forge a future worthy of the ancestors whose legacy they carry.]==],
		["Undead Gnome"] = [==[Not even death can extinguish a gnome's curiosity. Raised into undeath by the Scourge or other practitioners of necromancy, these diminutive inventors have traded the workshops of their former lives for an uncertain existence among the Forsaken. Though their bodies have decayed, their brilliant minds and irrepressible ingenuity remain intact. Rejected by many of their living kin, undead gnomes have found new purpose in a world that fears them, proving that neither the grave nor the laws of nature can keep a determined tinkerer from their work.]==],
		["Ogre"] = [==[Hailing from the shattered world of Draenor, the mighty ogres are descendants of an ancient race whose empires once rivaled the greatest civilizations of their world. Renowned for their immense strength, brutal tempers, and surprising aptitude for magic, these towering warriors have long served as both enemies and allies of the orcs. Though many ogre clans remain fiercely independent, others have pledged their strength to the Horde, seeking glory, conquest, and a place in a world where might often makes right.]==],
		["Eredar"] = [==[Once among the most gifted civilizations in the cosmos, the eredar of Argus were forever divided when their leaders were offered unimaginable power by the dark titan Sargeras. While some fled with Velen to become the draenei, countless others embraced the Burning Legion, becoming the man'ari—fearsome sorcerers and commanders of its demonic armies. With the Legion's defeat, these former servants of Sargeras face an uncertain future. Some remain devoted to their dark masters, while others seek a new purpose beyond the Legion's shadow, carrying both the power of their ancient heritage and the burden of their terrible past.]==],
		["Harpy"] = [==[Descendants of the ancient harpy queen Aviana, these fierce, winged creatures inhabit the mountains and wild places of Azeroth. Known for their shrill cries, razor-sharp talons, and territorial nature, harpies fiercely defend their nesting grounds against intruders, sometimes sending their bravest warriors to carry the fight into enemy territory. Though often hostile to outsiders, their intelligence and mastery of the skies make them formidable allies—or dangerous enemies.]==],
		["Alpha Goblin"] = [==[Bold entrepreneurs and reckless inventors, Alpha Goblins are driven by ambition, ingenuity, and the promise of profit. Armed with experimental gadgets and an explosive disregard for caution, these resourceful schemers venture across Azeroth in search of opportunity, fortune, and the next big deal.]==],
		["Dracthyr"] = [==[Created by Neltharion to serve as soldiers of the dragonflights, the Dracthyr were sealed away for thousands of years before awakening to a changed Azeroth. Wielding draconic magic and gifted with the power of flight, these formidable warriors now forge their own destinies, choosing their allies and discovering their place in a world that has long forgotten them.]==],
		["Illidari Blood Elf"] = [==[Blood Elves who sacrificed everything to follow Illidan Stormrage, the Illidari wield the very fel powers they swore to destroy. Hardened by brutal training and forever changed by the demons they consumed, these relentless hunters turn darkness against itself, determined to protect Azeroth at any cost.]==],
		["Nightborne"] = [==[For ten thousand years, the Nightborne lived beneath a magical barrier in the ancient city of Suramar, sustained by the arcane energies of the Nightwell. Masters of illusion and sorcery, these proud elves have emerged from their long isolation to reclaim their place in Azeroth, bringing centuries of accumulated knowledge and formidable magical power to the Horde.]==],
		["Highmountain Tauren"] = [==[Descendants of the legendary hero Huln Highmountain, these proud Tauren have called the rugged peaks of Highmountain home for thousands of years. Blessed with the mighty antlers of the great eagle spirit Ohn'ahra, they honor their ancestors, defend their sacred lands, and stand united against those who threaten their people. Now allied with the Horde, they bring the strength and resilience of the mountains to the wider world.]==],
		["Earthen"] = [==[Forged from living stone by the titans, the Earthen were created to shape and safeguard the world of Azeroth. For countless ages, these steadfast beings served their ancient purpose, guided by the edicts of their makers. Now, with their old directives challenged and new paths before them, the Earthen venture beyond their ancestral halls to discover what it means to forge a destiny of their own.]==],
		["Haranir"] = [==[An ancient and mysterious people dwelling among the great roots beneath Azeroth, the Haranir have long guarded the hidden pathways and sacred places of the world. Guided by their deep connection to nature and the teachings of their Primalists, these elusive guardians wield the power of root and bark to protect their ancestral home. Now emerging from centuries of secrecy, they venture into the wider world to confront the threats reaching even the deepest roots.]==],
		["Vulpera"] = [==[Resourceful nomads of the harsh deserts of Vol'dun, the Vulpera have survived generations of hardship through cunning, adaptability, and loyalty to their own. Skilled scavengers and traders, these clever fox-like wanderers turn even the simplest discoveries into opportunities. Having earned their place among the Horde, they now venture across Azeroth in search of adventure, friendship, and fortune.]==],
		["Gnoll"] = [==[Native to Azeroth, gnolls are fierce, hyena-like creatures found throughout the wilds of the Eastern Kingdoms and Kalimdor. Organized into unruly packs, they are notorious for raiding settlements, scavenging battlefields, and attacking anything they believe they can overpower. Though often dismissed as dimwitted brutes, gnolls possess a cunning born of hunger and survival. Fiercely loyal to their packs and always eager for a fight, these unpredictable creatures now seek fortune and fresh prey beyond their traditional hunting grounds.]==],
	},
}

-- Local server: placeholder racials of Kyrian / Venthyr / Slime Giant / Centaur (gen_new_racials.py), and their lore
do
	CHAR_CREATE_COA_RACIALS[107] = {{"Ascended Wings", "Spell_Magic_FeatherFall", "passive", "Reduces falling damage by 50%."}, {"Purity of Bastion", "Spell_Holy_Purify", "passive", "Disease, Poison, and Curse effects last 10% less time."}, {"Virtue of Courage", "Spell_Holy_BlessingOfStrength", "passive", "Increases critical strike chance by 1%."}, {"Soul Guide", "Ability_Rogue_Sprint", "passive", "Move 40% faster as a released spirit."}, {"Steward's Craft", "INV_Misc_Gem_01", "passive", "Increases Jewelcrafting skill by 5."}, {"Ascendant Charge", "Ability_Warrior_Charge", "active", "Draw your weapon and leap to an enemy within 25 yards, dealing Holy impact damage and slowing nearby enemies by 30% for 3 seconds. While hovering, dive instead for 20% more impact damage; Hover ends after the dive. Interrupted dives end Hover safely, and its cooldown starts on landing. The impact creates a 5-yard Consecration for 3 seconds, dealing Holy damage each second. Damage scales with your higher attack power or spell power. 2-minute cooldown."}}
	if CHAR_CREATE_RACE_BONUS then CHAR_CREATE_RACE_BONUS[107] = nil end
	CHAR_CREATE_COA_RACIALS[101] = {{"Crimson Feast", "Spell_Shadow_LifeDrain02", "active", "Wound enemies within 8 yards, causing them to bleed for 8 seconds. Heals you for 50% of damage dealt. Total base damage is 2 per level plus 15% of the higher of attack power or spell power, divided over four ticks. Healing modifiers apply."}, {"Blood Scent", "Spell_Nature_Sleep", "passive", "Increases stealth detection by 20."}, {"Children of Revendreth", "Ability_Racial_Cannibalize", "passive", "Reduces Shadow damage taken by 2%."}, {"Anima Hunger", "Ability_Racial_Cannibalize", "passive", "Killing an enemy eligible for experience or honor restores 3% of maximum health."}, {"Courtly Artifice", "Ability_Racial_Cannibalize", "passive", "Increases Enchanting skill by 5."}, {"Sanguine Hover", "Ability_Racial_Cannibalize", "active", "Hover just above the ground without free flight. Lasts indefinitely outside combat and up to 15 total seconds in combat per activation. Activating from the ground while in combat grants Stun immunity for 3 seconds. A successful stun ends Hover. Ending Hover starts a 2-minute cooldown. Activate again to land."}, {"Vampire Bat Form", "Ability_Racial_Cannibalize", "active", "Become a vampire bat, increasing flight speed by 60% and ground speed by 40%. Usable at any time outside combat. Ends on combat, damage, casting, mounting or changing maps; ending while airborne grants safe descent until landing. Activate again to return to your Venthyr form."}}
	if CHAR_CREATE_RACE_BONUS then CHAR_CREATE_RACE_BONUS[101] = nil end
	CHAR_CREATE_COA_RACIALS[112] = {{"Azerite Surge", "ability_earthen_azeritesurge", "active", "After 2.5 sec, unleash a surge of Fire damage on enemies within 15 yards. At level 40 the surge also heals allies, and at level 70 it additionally strikes the healthiest enemy."}, {"Titan-Wrought Frame", "ability_earthen_titanwroughtframe", "passive", "Armor from equipped items increased by 10%."}, {"Bone Shield", "ability_racial_skeletonpinkie", "active", "No mind left to frighten, as every Necromancer intends. Removes Charm, Fear and Sleep effects."}, {"Bone Pile", "spell_shadow_raisedead", "active", "A skeleton lying still is just another pile of bones. Slip into the shadows, dropping out of combat."}}
	if CHAR_CREATE_RACE_BONUS then CHAR_CREATE_RACE_BONUS[112] = nil end
	CHAR_CREATE_COA_RACIALS[104] = {{"Azerite Surge", "ability_earthen_azeritesurge", "active", "After 2.5 sec, unleash a surge of Fire damage on enemies within 15 yards. At level 40 the surge also heals allies, and at level 70 it additionally strikes the healthiest enemy."}, {"Titan-Wrought Frame", "ability_earthen_titanwroughtframe", "passive", "Armor from equipped items increased by 10%."}, {"Bone Shield", "ability_racial_skeletonpinkie", "active", "No mind left to frighten, as every Necromancer intends. Removes Charm, Fear and Sleep effects."}, {"Bone Pile", "spell_shadow_raisedead", "active", "A skeleton lying still is just another pile of bones. Slip into the shadows, dropping out of combat."}}
	if CHAR_CREATE_RACE_BONUS then CHAR_CREATE_RACE_BONUS[104] = nil end
	CHAR_CREATE_COA_RACIALS[75] = {{"Bull Rush", "ability_racial_bullrush", "active", "Charge forward for 1 sec, knocking enemies in your path down for 1.5 sec."}, {"Mountaineer", "ability_racial_mountaineer", "passive", "Damage dealt and healing done increased by 1% and damage taken reduced by 1% (Versatility has no WotLK equivalent)."}, {"Pounce", "Ability_Druid_Dash", "active", "Leap to an enemy within 15 yards and daze it for 2 sec."}, {"Predatory Instincts", "Ability_Racial_ShadowMeld", "passive", "Critical strike chance increased by 1%."}}
	if CHAR_CREATE_RACE_BONUS then CHAR_CREATE_RACE_BONUS[75] = nil end
	CHAR_CREATE_COA_RACIALS[102] = {{"Bull Rush", "ability_racial_bullrush", "active", "Charge forward for 1 sec, knocking enemies in your path down for 1.5 sec."}, {"Mountaineer", "ability_racial_mountaineer", "passive", "Damage dealt and healing done increased by 1% and damage taken reduced by 1% (Versatility has no WotLK equivalent)."}, {"Pounce", "Ability_Druid_Dash", "active", "Leap to an enemy within 15 yards and daze it for 2 sec."}, {"Predatory Instincts", "Ability_Racial_ShadowMeld", "passive", "Critical strike chance increased by 1%."}}
	if CHAR_CREATE_RACE_BONUS then CHAR_CREATE_RACE_BONUS[102] = nil end
	CHAR_CREATE_RACE_LORE["Kyrian"] = [==[In the radiant citadels of Bastion the Kyrian serve the Archon, guiding the souls of the worthy to their rest. Winged in spirit and bound by vows of service, they shepherd the departed with patience and with spear. Those who walk among the living carry that duty with them: steadfast, watchful, and slow to forgive a promise broken.]==]
	CHAR_CREATE_RACE_LORE["Venthyr"] = [==[The nobles of Revendreth rule the shadowed halls of their gothic realm, immortal and exacting. Venthyr court and cruelty are one and the same, and every favor is a debt. In the world of mortals they are pale, proud and patient hunters, who feed on fear as lightly as others sip wine.]==]
	CHAR_CREATE_RACE_LORE["Slime Giant"] = [==[Nobody knows who first poured the ooze over the old giant's bones, only that it never stopped moving. The Slime Giants are towering skeletons wrapped in living slime, colored by whatever swamp, vat or ruin gave them life. Slow to anger and impossible to bruise, they carry their strange second skin proudly into every fight.]==]
	CHAR_CREATE_RACE_LORE["Centaur"] = [==[Swift of hoof and proud of lineage, the centaurs have roamed the open plains since before the sundering of the world. Half warrior and half steed, they hold that no horizon is a border and no rider a master. Those who join the wider world do so with the speed of a charge and the long memory of the herd.]==]
end

-- Local server: the Hozen racials (gen_hozen_racials.py)
CHAR_CREATE_COA_RACIALS[112] = {{"Prehensile Tail", "INV_Misc_MonsterTail_03", "passive", "Your tail keeps you balanced on any branch or battlefield. Increases your chance to dodge by 3%."}, {"Nimble Climber", "Ability_Druid_Dash", "passive", "Hozen swing and scramble where others walk. Increases your movement speed by 3%."}, {"Bristling Fur", "Spell_Druid_BristlingFur", "passive", "Thick fur turns away the elements. Reduces Frost and Nature damage taken by 3%."}, {"Monkey Business", "Ability_Hunter_AspectOfTheMonkey", "passive", "The hozen are quick to find a weakness. Increases your chance to critically strike with melee and ranged attacks by 2%."}}
if CHAR_CREATE_RACE_BONUS then CHAR_CREATE_RACE_BONUS[112] = nil end

-- Local server: the Vorkai and Mantid racials (gen_newraces_racials.py)
CHAR_CREATE_COA_RACIALS[110] = {{"Thornhide", "Spell_Nature_Thorns", "passive", "Bark-hard fur turns aside the wild. Reduces Nature damage taken by 3%."}, {"Fleet of Hoof", "Ability_Druid_Dash", "passive", "Cloven hooves find purchase on any trail. Increases your movement speed by 3%."}, {"Fey Grace", "Spell_Nature_Regeneration", "passive", "The Sylvari move like deer through the brush. Increases your chance to dodge by 3%."}, {"Moonlit Sight", "Spell_Nature_StarFall", "passive", "Eyes that never lose the quarry in the dark. Increases your chance to critically strike with melee and ranged attacks by 2%."}}
if CHAR_CREATE_RACE_BONUS then CHAR_CREATE_RACE_BONUS[110] = nil end
CHAR_CREATE_COA_RACIALS[109] = {{"Ethereal Warding", "Spell_Arcane_Arcane04", "passive", "Bandages woven with stormlight turn aside the arcane. Reduces Arcane and Shadow damage taken by 3%."}, {"Wisp Step", "Spell_Nature_InvisibilityTotem", "passive", "The ethereal drift more than they walk. Increases your movement speed by 3%."}, {"Phase Shift", "Spell_Arcane_Blink", "passive", "A flicker out of phase turns the blow aside. Increases your chance to dodge by 3%."}, {"Void Insight", "Spell_Shadow_Charm", "passive", "Traders of secrets see the weak point in every deal and every foe. Increases your chance to critically strike with melee and ranged attacks by 2%."}}
if CHAR_CREATE_RACE_BONUS then CHAR_CREATE_RACE_BONUS[109] = nil end
CHAR_CREATE_COA_RACIALS[106] = {{"Chitin Carapace", "INV_Shield_06", "passive", "A hard shell of chitin turns aside blows. Reduces physical damage taken by 3%."}, {"Mantis Strike", "Ability_Hunter_SwiftStrike", "passive", "Scythe-quick claws find every gap. Increases your chance to critically strike with melee and ranged attacks by 2%."}, {"Skittering Reflexes", "Ability_Rogue_Sprint", "passive", "Mantid reflexes outrun a blade. Increases your chance to dodge by 3%."}, {"Swarm Stride", "Ability_Rogue_Rupture", "passive", "Six sure legs eat the miles. Increases your movement speed by 3%."}}
if CHAR_CREATE_RACE_BONUS then CHAR_CREATE_RACE_BONUS[106] = nil end

-- Local server: one unique active racial per added race (gen_active_racials.py)
do
	if CHAR_CREATE_COA_RACIALS[112] then table.insert(CHAR_CREATE_COA_RACIALS[112], {"Nimble Escape", "Ability_Rogue_Sprint", "active", "Escape the effects of any immobilization or movement speed reduction effect."}) end
	if CHAR_CREATE_COA_RACIALS[110] then table.insert(CHAR_CREATE_COA_RACIALS[110], {"Verdant Renewal", "Spell_Nature_HealingTouch", "active", "Wild growth knits your wounds, healing you over 15 sec. The amount grows with your level."}) end
	if CHAR_CREATE_COA_RACIALS[106] then table.insert(CHAR_CREATE_COA_RACIALS[106], {"Chitin Hardening", "INV_Misc_Shell_03", "active", "Removes all poison, disease and bleed effects and hardens your carapace, increasing your armor for 8 sec."}) end
	if CHAR_CREATE_COA_RACIALS[109] then table.insert(CHAR_CREATE_COA_RACIALS[109], {"Ethereal Torrent", "Spell_Arcane_ManaTap", "active", "Silences all enemies within 8 yards for 2 sec and restores energy or mana."}) end


	if CHAR_CREATE_COA_RACIALS[104] then table.insert(CHAR_CREATE_COA_RACIALS[104], {"Gooey Slam", "Spell_Nature_Earthquake", "active", "Slams the ground, stunning all enemies within 8 yards for 2 sec."}) end
	if CHAR_CREATE_COA_RACIALS[75] then table.insert(CHAR_CREATE_COA_RACIALS[75], {"Plains Charge", "Ability_Mount_RidingHorse", "active", "Increases your haste by 10% for 10 sec."}) end
	if CHAR_CREATE_COA_RACIALS[102] then table.insert(CHAR_CREATE_COA_RACIALS[102], {"Plains Charge", "Ability_Mount_RidingHorse", "active", "Increases your haste by 10% for 10 sec."}) end
end

-- Local server: the Vorkai and Mantid racials (gen_newraces_racials.py)
CHAR_CREATE_COA_RACIALS[90] = {{"Deathless Shroud", "Spell_Shadow_AntiShadow", "passive", "The dead feel no cold. Reduces Frost and Shadow damage taken by 3%."}, {"Wailing Gale", "Spell_Nature_Cyclone", "passive", "A keening wind carries you onward. Increases your movement speed by 3%."}, {"Spectral Sway", "Spell_Shadow_Teleport", "passive", "Half in the grave, half in the air. Increases your chance to dodge by 3%."}, {"Banshee Wail", "Spell_Shadow_PsychicScream", "active", "A wail that freezes the living: stuns all enemies within 8 yards for 2 sec."}, {"Spectral Glide", "Spell_Shadow_SoulLeech_3", "active", "Slip between worlds: removes snares and roots and hardens your spirit, reducing damage taken for 8 sec."}}
if CHAR_CREATE_RACE_BONUS then CHAR_CREATE_RACE_BONUS[90] = nil end
CHAR_CREATE_COA_RACIALS[87] = {{"Faceless Hide", "Spell_Shadow_Possession", "passive", "A hide that drinks the light. Reduces Shadow and Nature damage taken by 3%."}, {"Creeping Dread", "Spell_Shadow_PsychicHorrors", "passive", "You glide where others walk. Increases your movement speed by 3%."}, {"Formless", "Spell_Shadow_MindTwisting", "passive", "No face, no weak point. Increases your chance to dodge by 3%."}, {"Shapeless Squirm", "Spell_Shadow_ShadowWordDominate", "active", "Without a face there is nothing to hold: escape all immobilizing and movement-slowing effects."}, {"Mind Rot", "Spell_Shadow_MindRot", "active", "Silences all enemies within 8 yards for 2 sec and restores energy or mana."}}
if CHAR_CREATE_RACE_BONUS then CHAR_CREATE_RACE_BONUS[87] = nil end

-- Local server: Centaur Gallop (gen_gallop.py)
do
	if CHAR_CREATE_COA_RACIALS[75] then table.insert(CHAR_CREATE_COA_RACIALS[75], {"Gallop", "Ability_Mount_RidingHorse", "active", "Break into a full gallop, increasing your run speed by 100%. Takes as long to summon as a mount, cannot be used in combat and can be dismissed like a mount."}) end
	if CHAR_CREATE_COA_RACIALS[102] then table.insert(CHAR_CREATE_COA_RACIALS[102], {"Gallop", "Ability_Mount_RidingHorse", "active", "Break into a full gallop, increasing your run speed by 100%. Takes as long to summon as a mount, cannot be used in combat and can be dismissed like a mount."}) end
end

-- Local server: race icons per race / faction / sex
CHAR_CREATE_COA_GENDER_ICONS = {["CENTAURALLIANCE_FEMALE"] = "Interface\\Icons\\CoA_RaceG_CENTAURALLIANCE_FEMALE", ["CENTAURHORDE_FEMALE"] = "Interface\\Icons\\CoA_RaceG_CENTAURHORDE_FEMALE", ["CENTAURALLIANCE_MALE"] = "Interface\\Icons\\CoA_RaceG_CENTAURALLIANCE_MALE", ["CENTAURHORDE_MALE"] = "Interface\\Icons\\CoA_RaceG_CENTAURHORDE_MALE", ["DRAKONIDALLIANCE_MALE"] = "Interface\\Icons\\CoA_RaceG_DRAKONIDALLIANCE_MALE", ["DRAKONIDHORDE_MALE"] = "Interface\\Icons\\CoA_RaceG_DRAKONIDHORDE_MALE", ["FURBOLG_MALE"] = "Interface\\Icons\\CoA_RaceG_FURBOLG_MALE", ["SLIMEGIANTHORDE_MALE"] = "Interface\\Icons\\CoA_RaceG_SLIMEGIANTHORDE_MALE", ["SLIMEGIANTHORDE_FEMALE"] = "Interface\\Icons\\CoA_RaceG_SLIMEGIANTHORDE_FEMALE", ["KYRIANALLIANCE_MALE"] = "Interface\\Icons\\CoA_RaceG_KYRIANALLIANCE_MALE", ["KYRIANALLIANCE_FEMALE"] = "Interface\\Icons\\CoA_RaceG_KYRIANALLIANCE_FEMALE", ["VENTHYRHORDE_MALE"] = "Interface\\Icons\\CoA_RaceG_VENTHYRHORDE_MALE", ["VENTHYRHORDE_FEMALE"] = "Interface\\Icons\\CoA_RaceG_VENTHYRHORDE_FEMALE", ["VORKAIALLIANCE_MALE"] = "Interface\\Icons\\CoA_RaceG_VORKAIALLIANCE_MALE", ["VORKAIALLIANCE_FEMALE"] = "Interface\\Icons\\CoA_RaceG_VORKAIALLIANCE_FEMALE", ["MANTIDHORDE_MALE"] = "Interface\\Icons\\CoA_RaceG_MANTIDHORDE_MALE", ["MANTIDHORDE_FEMALE"] = "Interface\\Icons\\CoA_RaceG_MANTIDHORDE_FEMALE", ["ETHEREALALLIANCE_MALE"] = "Interface\\Icons\\CoA_RaceG_ETHEREALALLIANCE_MALE", ["ETHEREALALLIANCE_FEMALE"] = "Interface\\Icons\\CoA_RaceG_ETHEREALALLIANCE_FEMALE", ["SYLVARIALLIANCE_MALE"] = "Interface\\Icons\\CoA_RaceG_SYLVARIALLIANCE_MALE", ["SYLVARIALLIANCE_FEMALE"] = "Interface\\Icons\\CoA_RaceG_SYLVARIALLIANCE_FEMALE"}

-- Local server: the Goblin shows a goblin face like the other races (it showed a boombox item icon)
CHAR_CREATE_EXTRA_RACE_ICONS["Goblin"] = "Interface\\Icons\\CoA_Race_AlphaGoblin"

-- Body sliders page, ported from the Esteria client's creator (STRHercules/wowdecomp, Tools/RetailAssets/glue/body_page.lua) into the
-- CoA creator: the "Body Sliders" button under Extra Customization opens it (COA_BodyPanel). Body column and triangle on the right,
-- face column to its left; the stock option rows are hidden meanwhile. Every slider is shown only when the race's skeleton supports it.
local BS = { visible = false, rows = {}, built = false, broken = nil }

local function BS_Log(text)
    if BodyShapeLog then BodyShapeLog("lua: " .. tostring(text)) end
end

local function BS_Pretty(name)
    return (name:gsub("(%l)(%u)", "%1 %2"))
end

local function BS_Install()
    local BACKDROP = {
        bgFile = "Interface\\Glues\\Common\\Glue-Tooltip-Background",
        edgeFile = "Interface\\Glues\\Common\\Glue-Tooltip-Border",
        tile = true, tileSize = 16, edgeSize = 16,
        insets = { left = 5, right = 5, top = 5, bottom = 5 },
    };

    local panel = CreateFrame("Frame", "BodyShapePanel", CharacterCreateFrame);
    panel:SetAllPoints(CharacterCreateFrame);
    panel:Hide();

    local note = CharacterCreateFrame:CreateFontString(nil, "OVERLAY", "GlueFontHighlightSmall");
    note:SetPoint("BOTTOMLEFT", CharacterCreateFrame, "BOTTOMLEFT", 20, 8);
    note:SetTextColor(1, 0.6, 0.4);
    note:Hide();

    local function MakeButton(name, text, width, x, y)
        local button = CreateFrame("Button", name, panel);
        button:SetSize(width, 26);
        button:SetPoint("TOPRIGHT", CharacterCreateFrame, "TOPRIGHT", x, y or -78);
        button:SetNormalFontObject(GlueFontNormalSmall);
        button:SetHighlightFontObject(GlueFontHighlightSmall);
        button:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight");
        button:SetBackdrop(BACKDROP);
        button:SetText(text);
        return button;
    end

    local buttonRandom = MakeButton("BodyShapeRandomButton", "Randomize Body", 130, -175, -108);
    local buttonReset = MakeButton("BodyShapeResetButton", "Reset Body", 120, -40, -108);
    buttonRandom:SetScript("OnClick", function()
        PlaySound("gsCharacterCreationLook");
        BodyShapeRandomize();
        if BS.ResetTriangle then BS.ResetTriangle(); end
    end);
    buttonReset:SetScript("OnClick", function()
        PlaySound("gsCharacterCreationCancel");
        ResetBodyShape();
        if BS.ResetTriangle then BS.ResetTriangle(); end
    end);

    BS.tab = "body";
    local tabBody = MakeButton("BodyShapeTabBody", "Body", 100, -150, -76);
    local tabFace = MakeButton("BodyShapeTabFace", "Face", 100, -40, -76);
    tabBody:SetScript("OnClick", function() BS.tab = "body"; PlaySound("gsCharacterCreationLook"); if BS.layout then BS.layout(); end end);
    tabFace:SetScript("OnClick", function() BS.tab = "face"; PlaySound("gsCharacterCreationLook"); if BS.layout then BS.layout(); end end);
    BS.tabBody, BS.tabFace = tabBody, tabFace;

    local columnTitle = {};
    for _, side in ipairs({ "Body", "Face" }) do
        local title = panel:CreateFontString(nil, "ARTWORK", "GlueFontNormal");
        title:SetText(side);
        columnTitle[side] = title;
    end

    -- Body-type triangle: Muscular (top), Large (bottom left), Thin (bottom right). The knob's
    -- barycentric weights blend three presets over a few body sliders. Its contribution is added on
    -- top of the sliders, so manual fine-tuning survives moving the knob.
    local TRI_W, TRI_H, TRI_SPACE = 112, 96, 120;
    local TRI_PRESETS = {
        { ShoulderWidth = 0.8, ArmGirth = 0.8, ChestSize = 0.7, WaistWidth = -0.2, HipWidth = -0.1, LegGirth = 0.4 },
        { ShoulderWidth = 0.2, ArmGirth = 0.4, ChestSize = 0.5, WaistWidth = 0.9, HipWidth = 0.7, LegGirth = 0.7 },
        { ShoulderWidth = -0.2, ArmGirth = -0.4, ChestSize = -0.3, WaistWidth = -0.35, HipWidth = -0.25, LegGirth = -0.35 },
    };
    local CORNERS = { { TRI_W / 2, TRI_H }, { 0, 0 }, { TRI_W, 0 } };
    BS.tri = { w = { 1 / 3, 1 / 3, 1 / 3 }, applied = {} };

    local tri = CreateFrame("Frame", "BodyShapeTriangle", panel);
    tri:SetSize(TRI_W, TRI_H);
    tri:EnableMouse(true);
    local STRIP = 3;
    for y = 0, TRI_H - STRIP, STRIP do
        local width = TRI_W * (1 - (y + STRIP / 2) / TRI_H);
        local edge = tri:CreateTexture(nil, "BACKGROUND");
        edge:SetTexture(0.75, 0.85, 0.8, 0.55);
        edge:SetSize(width + 4, STRIP);
        edge:SetPoint("BOTTOM", tri, "BOTTOMLEFT", TRI_W / 2, y);
        if width > 6 and y > 1 then
            local fill = tri:CreateTexture(nil, "BORDER");
            fill:SetTexture(0.04, 0.07, 0.09, 0.85);
            fill:SetSize(width - 4, STRIP);
            fill:SetPoint("BOTTOM", tri, "BOTTOMLEFT", TRI_W / 2, y);
        end
    end
    local function CornerLabel(text, point, x, y)
        local label = tri:CreateFontString(nil, "ARTWORK", "GlueFontNormalSmall");
        label:SetText(text);
        label:SetPoint(point, tri, "BOTTOMLEFT", x, y);
    end
    CornerLabel("Muscular", "BOTTOM", TRI_W / 2, TRI_H + 4);
    CornerLabel("Large", "TOPLEFT", -6, -4);
    CornerLabel("Thin", "TOPRIGHT", TRI_W + 6, -4);

    local knob = tri:CreateTexture(nil, "OVERLAY");
    knob:SetTexture("Interface\\Buttons\\UI-RadioButton");
    knob:SetTexCoord(0.25, 0.5, 0, 1);
    knob:SetSize(18, 18);

    local function PlaceKnob()
        local w = BS.tri.w;
        local x = w[1] * CORNERS[1][1] + w[2] * CORNERS[2][1] + w[3] * CORNERS[3][1];
        local y = w[1] * CORNERS[1][2] + w[2] * CORNERS[2][2] + w[3] * CORNERS[3][2];
        knob:ClearAllPoints();
        knob:SetPoint("CENTER", tri, "BOTTOMLEFT", x, y);
    end

    -- Weights for a local point, clamped onto the triangle.
    local function Weights(x, y)
        local ax, ay = CORNERS[1][1], CORNERS[1][2];
        local bx, by = CORNERS[2][1], CORNERS[2][2];
        local cx, cy = CORNERS[3][1], CORNERS[3][2];
        local d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy);
        local a = ((by - cy) * (x - cx) + (cx - bx) * (y - cy)) / d;
        local b = ((cy - ay) * (x - cx) + (ax - cx) * (y - cy)) / d;
        local w = { math.max(a, 0), math.max(b, 0), math.max(1 - a - b, 0) };
        local sum = w[1] + w[2] + w[3];
        return { w[1] / sum, w[2] / sum, w[3] / sum };
    end

    local function ApplyTriangle()
        local w = BS.tri.w;
        local wanted = {};
        -- Each corner's preset fades in from the centre (0) to its corner (1), so a corner shows
        -- exactly its own preset rather than also subtracting the other two.
        for k = 1, 3 do
            local share = math.max(0, (w[k] - 1 / 3) * 1.5);
            for name, amount in pairs(TRI_PRESETS[k]) do
                wanted[name] = (wanted[name] or 0) + share * amount;
            end
        end
        for name, value in pairs(wanted) do
            if GetBodyShapeSupport(name) ~= false then
                local lo, hi = GetBodyShapeRange(name);
                local base = GetBodyShape(name) - (BS.tri.applied[name] or 0);
                local target = math.max(lo or -1, math.min(hi or 1, base + value));
                SetBodyShape(name, target);
                BS.tri.applied[name] = target - base;
            end
        end
        PlaceKnob();
    end

    -- Back to the centre without touching the sliders (used after Reset/Randomize/new character).
    function BS.ResetTriangle()
        BS.tri.w = { 1 / 3, 1 / 3, 1 / 3 };
        BS.tri.applied = {};
        PlaceKnob();
    end

    local function TrackCursor()
        local scale = tri:GetEffectiveScale();
        local cx, cy = GetCursorPosition();
        BS.tri.w = Weights(cx / scale - tri:GetLeft(), cy / scale - tri:GetBottom());
        ApplyTriangle();
    end
    tri:SetScript("OnMouseDown", function(self, button)
        if button == "RightButton" then
            BS.tri.w = { 1 / 3, 1 / 3, 1 / 3 };
            ApplyTriangle();
            return;
        end
        self.dragging = true;
        TrackCursor();
    end);
    tri:SetScript("OnMouseUp", function(self) self.dragging = false; end);
    tri:SetScript("OnHide", function(self) self.dragging = false; end);
    tri:SetScript("OnUpdate", function(self)
        if self.dragging then TrackCursor(); end
    end);
    BS.triangle = tri;
    PlaceKnob();

    local function BuildRows()
        local index = 0;
        for _, group in ipairs({ "body", "face" }) do
            for _, name in ipairs({ GetBodyShapeNames(group) }) do
                index = index + 1;
                local row = CreateFrame("Frame", nil, panel);
                row:SetSize(300, 16);
                row.name, row.group = name, group;

                local label = row:CreateFontString(nil, "ARTWORK", "GlueFontHighlightSmall");
                label:SetPoint("LEFT", 0, 0);
                label:SetWidth(100);
                label:SetJustifyH("LEFT");
                label:SetText(BS_Pretty(name));

                local value = row:CreateFontString(nil, "ARTWORK", "GlueFontNormalSmall");
                value:SetPoint("RIGHT", 0, 0);
                value:SetWidth(52);
                value:SetJustifyH("RIGHT");

                local slider = CreateFrame("Slider", "BodyShapeSlider" .. index, row);
                slider:SetOrientation("HORIZONTAL");
                slider:SetSize(135, 14);
                slider:SetPoint("LEFT", 104, 0);
                slider:SetBackdrop({
                    bgFile = "Interface\\Buttons\\UI-SliderBar-Background",
                    edgeFile = "Interface\\Buttons\\UI-SliderBar-Border",
                    tile = true, tileSize = 8, edgeSize = 8,
                    insets = { left = 3, right = 3, top = 6, bottom = 6 },
                });
                slider:SetThumbTexture("Interface\\Buttons\\UI-SliderBar-Button-Horizontal");
                local lo, hi = GetBodyShapeRange(name);
                slider:SetMinMaxValues(lo or -1, hi or 1);
                slider:SetValueStep(0.05);
                -- The client only places the knob once a value has been set; Sync skips unchanged values.
                slider:SetValue(GetBodyShape(name) or 0);
                slider:EnableMouse(true);
                slider:SetScript("OnValueChanged", function(self, v)
                    v = math.floor(v * 20 + 0.5) / 20;
                    value:SetText(string.format("%d%%", math.floor(v * 100 + 0.5)));
                    if not self.syncing then SetBodyShape(name, v); end
                end);
                -- Right click puts a slider back to its default.
                slider:SetScript("OnMouseUp", function(self, button)
                    if button == "RightButton" then self:SetValue(0); end
                end);

                row.slider, row.value = slider, value;
                BS.rows[#BS.rows + 1] = row;
            end
        end
    end

    local function Sync(row)
        local v = GetBodyShape(row.name);
        if math.abs(row.slider:GetValue() - v) > 0.001 then
            row.slider.syncing = true;
            row.slider:SetValue(v);
            row.slider.syncing = false;
        end
        row.value:SetText(string.format("%d%%", math.floor(v * 100 + 0.5)));
    end

    local function Layout()
        local counts = { body = 0, face = 0 };
        for _, row in ipairs(BS.rows) do
            if GetBodyShapeSupport(row.name) ~= false then counts[row.group] = counts[row.group] + 1; end
        end
        -- only one group at a time: the tabs switch, so nothing sits over the model or the stock buttons
        if counts[BS.tab] == 0 then BS.tab = counts.body > 0 and "body" or "face"; end
        local triShown = false;
        for name in pairs({ ShoulderWidth = 1, ArmGirth = 1, ChestSize = 1, WaistWidth = 1, HipWidth = 1, LegGirth = 1 }) do
            if GetBodyShapeSupport(name) ~= false then triShown = true; end
        end
        triShown = triShown and BS.tab == "body";
        BS.triangle:ClearAllPoints();
        BS.triangle:SetPoint("TOPRIGHT", CharacterCreateFrame, "TOPRIGHT", -125, -148);
        if triShown then BS.triangle:Show(); else BS.triangle:Hide(); end
        local top = triShown and 262 or 150;
        local placed = 0;
        local spacing = 17;
        for _, row in ipairs(BS.rows) do
            if row.group ~= BS.tab or GetBodyShapeSupport(row.name) == false then
                row:Hide();
            else
                row:ClearAllPoints();
                row:SetPoint("TOPRIGHT", CharacterCreateFrame, "TOPRIGHT", -40, -top - placed * spacing);
                placed = placed + 1;
                Sync(row);
                row:Show();
            end
        end
        for _, title in pairs(columnTitle) do title:Hide(); end
        if BS.tabBody then
            BS.tabBody:SetAlpha(BS.tab == "body" and 1 or 0.55);
            BS.tabFace:SetAlpha(BS.tab == "face" and 1 or 0.55);
            if counts.face == 0 then BS.tabFace:Hide(); else BS.tabFace:Show(); end
            if counts.body == 0 then BS.tabBody:Hide(); else BS.tabBody:Show(); end
        end
    end

    if not BS.built then
        BuildRows()
        BS.built = true
    end
    BS.layout = Layout
    Layout()
end

local installed = false
function COA_BodyPanel(show)
    if type(GetBodyShapeNames) ~= "function" then return false end
    if not installed and show then
        installed = true
        local ok, err = pcall(BS_Install)
        if not ok then BS.broken = tostring(err); BS_Log("install failed: " .. BS.broken) end
    end
    BS.visible = show and not BS.broken and true or false
    local panel = _G.BodyShapePanel
    if panel then
        if BS.visible then panel:Show() else panel:Hide() end
    end
    return BS.visible
end

-- Keep the rows in step with the model (support changes when the race changes) while the page is open.
local driver = CreateFrame("Frame")
driver:SetScript("OnUpdate", function(self, elapsed)
    self.elapsed = (self.elapsed or 0) + elapsed
    if self.elapsed < 0.15 or not BS.visible then return end
    self.elapsed = 0
    if BS.layout then
        local ok, err = pcall(BS.layout)
        if not ok then BS.broken = tostring(err); BS.visible = false; BS_Log("layout error: " .. BS.broken) end
    end
end)

-- A new character starts with default sliders.
do
    local onShow = CharacterCreate_OnShow
    if onShow then
        function CharacterCreate_OnShow(...)
            if ResetBodyShape then ResetBodyShape() end
            if BS.ResetTriangle then BS.ResetTriangle() end
            return onShow(...)
        end
    end
end

-- body sliders: remember what the Body page showed for the character that is about to be created
do
	local finish = CharacterCreate_Finish
	if finish then
		function CharacterCreate_Finish(...)
			pcall(CycleCharCustomization, "EA_BODY_SAVE", 0)
			return finish(...)
		end
	end
end

-- Oversized races (centaur, vrykul, dragon forms...) are shrunk on the stage so they stay on screen (EA_PREVIEW_SCALE).
do
	local fit = CreateFrame("Frame", "COAFitPreview")
	local waited = 0
	fit:SetScript("OnUpdate", function(self, elapsed)
		waited = waited + elapsed
		if waited < 0.3 then return end
		waited = 0
		if CycleCharCustomization and CharacterCreateFrame and CharacterCreateFrame:IsVisible() then
			pcall(CycleCharCustomization, "EA_PREVIEW_SCALE", 0)
		end
	end)
end
