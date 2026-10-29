-- Barde : l'atelier, où l'on fabrique et l'on accorde ses instruments.
-- Licence AGPL-3.0, voir LICENSE.
--
-- Cinq paliers (cuivre, argent, mithril, thorium, arcanite). Le serveur envoie les paliers (compétence demandée, matériaux,
-- gemme de qualité) et les chances de qualité, et dit quels instruments le joueur possède ; la fenêtre les affiche et envoie
-- « fabriquer ». Une gemme (et, pour l'arcanite, le cristal arcanique) augmente les chances d'une meilleure qualité.
-- Un sixième onglet, « Accordage », montre l'accord de chaque instrument et fabrique les clés qui le remettent à 100 %.
-- Le serveur vérifie tout.

local ADDON, ns = ...

local Atelier = {}
ns.Atelier = Atelier

local TABS = ns.Lazy({ [0] = "Cuivre", "Argent", "Mithril", "Thorium", "Arcanite", "Accordage" })
local TUNING_TAB = 5
local TUNING_KEY = 1950020
local KEY_BAR, KEY_THREAD = 2840, 2320
local CAMP_ITEMS = { { 2592, 4, "Wool Cloth" }, { 2319, 2, "Medium Leather" }, { 4471, 1, "Flint and Tinder" } }
local ROWS = 10
local ROW_H = 24
local FIRST_ROW = -214

local QUALITY_NAMES = ns.Lazy({ "commun", "inhabituel", "rare", "épique" })
local QUALITY_COLORS = { "|cffffffff", "|cff1eff00", "|cff0070dd", "|cffa335ee" }

local frame
local tab = 0
local useGem, useCrystal = false, false

local function BaseName(ins)
    return (string.gsub(ins.name, " de cuivre$", ""))
end

local function Skill()
    return ns.info.skill or 1
end

local function Name(item, fallback)
    local name = GetItemInfo(item)
    return name or fallback
end

-- Tout ce qu'il faut pour ce palier : les matériaux, puis la gemme et le cristal si on les utilise.
local function Needs(info)
    local list = {}
    if not info then return list end
    for _, mat in ipairs(info.mats) do
        table.insert(list, { item = mat.item, count = mat.count, name = mat.name })
    end
    if useGem and info.gem then
        table.insert(list, { item = info.gem.item, count = 1, name = info.gem.name })
    end
    if useGem and useCrystal and info.crystal then
        table.insert(list, { item = info.crystal.item, count = 1, name = info.crystal.name })
    end
    return list
end

-- Peut-on fabriquer à ce palier ? Sinon, pourquoi.
local function Check(info)
    if not info then return false, ns.T("palier inconnu") end
    if Skill() < info.skill then return false, ns.T("compétence ") .. info.skill end
    for _, need in ipairs(Needs(info)) do
        if GetItemCount(need.item) < need.count then return false, ns.T("matériaux") end
    end
    return true
end

local function GemMode(info)
    if useGem and info and info.gem then
        return (useCrystal and info.crystal) and 2 or 1
    end
    return 0
end

-- Les chances de chaque qualité pour ce palier, avec ou sans gemme, et le petit bonus de compétence.
local function ChancesText(info)
    local q = ns.qualityChances
    if not q or not info then return "" end
    local c = {}
    for i = 1, 4 do c[i] = q[GemMode(info) + 1][i] end
    local bonus = math.min(q.bonusMax, math.floor(math.max(0, Skill() - info.skill) / q.bonusPer))
    local shift = math.min(c[1], bonus)
    c[1], c[2] = c[1] - shift, c[2] + shift
    local parts = {}
    for i = 1, 4 do
        if c[i] > 0 then
            table.insert(parts, QUALITY_COLORS[i] .. QUALITY_NAMES[i] .. " " .. c[i] .. " %|r")
        end
    end
    return ns.T("Chances : ") .. table.concat(parts, " · ")
end

local function TuningColor(percent)
    if percent >= 80 then return "|cff9fef9f" end
    if percent >= 50 then return "|cffffd100" end
    return "|cffff4040"
end

-- L'onglet Accordage : les clés, et chaque instrument avec son accord.
local function RefreshTuning()
    local keys = GetItemCount(TUNING_KEY)
    frame.need:SetText(ns.T("Clés d'accordage : |cffffffff") .. keys .. "|r")
    frame.mats[1]:SetText(ns.T("Une clé remet un instrument à 100 %. Il perd 4 % par morceau (3 % tambour) ;"))
    frame.mats[2]:SetText(ns.T("sous 50 %, le chant faiblit (jusqu'à ×0,85). Légendaire : jamais désaccordé."))
    local haveBar, haveThread = GetItemCount(KEY_BAR), GetItemCount(KEY_THREAD)
    frame.mats[3]:SetText(ns.T("Fabriquer 5 clés : ") .. (haveBar >= 1 and "|cff9fef9f" or "|cffff4040") .. "1 × " .. Name(KEY_BAR, "Copper Bar")
        .. ns.T("|r, ") .. (haveThread >= 1 and "|cff9fef9f" or "|cffff4040") .. "1 × " .. Name(KEY_THREAD, "Coarse Thread") .. "|r")
    local campOk = true
    local parts = {}
    for _, need in ipairs(CAMP_ITEMS) do
        local have = GetItemCount(need[1])
        if have < need[2] then campOk = false end
        table.insert(parts, (have >= need[2] and "|cff9fef9f" or "|cffff4040") .. need[2] .. " × " .. Name(need[1], need[3]) .. "|r")
    end
    frame.mats[4]:SetText(ns.T("Camp de barde : ") .. table.concat(parts, ", "))
    frame.mats[5]:SetText(ns.T("|cff9fa0b0Le camp dure 30 min, un par zone : feu de cuisine, bancs, accordage seul.|r"))
    frame.makeCamp:Show()
    if campOk then frame.makeCamp:Enable() else frame.makeCamp:Disable() end
    frame.makeKeys:Show()
    if haveBar >= 1 and haveThread >= 1 then frame.makeKeys:Enable() else frame.makeKeys:Disable() end

    for i = 1, ROWS do
        local row = frame.rows[i]
        local ins = ns.instruments[i]
        if not ins then
            row:Hide()
        else
            row:Show()
            row.name:SetText(ns.InstrumentName(ins))
            if not ins.has then
                row.state:SetText(ns.T("|cff808080absent|r"))
                row.button:Hide()
            else
                local percent = ins.tuning or 100
                row.state:SetText(ns.T("accordage ") .. TuningColor(percent) .. math.floor(percent + 0.5) .. " %|r")
                row.button:Show()
                row.button:SetText(ns.T("Accorder"))
                if percent < 99.5 and keys > 0 then row.button:Enable() else row.button:Disable() end
                row.button.mode = "tune"
                row.button.instrument = ins.id
            end
        end
    end
end

function Atelier.Refresh()
    if not frame or not frame:IsShown() then return end

    for t = 0, TUNING_TAB do
        if t == tab then frame.tabs[t]:LockHighlight() else frame.tabs[t]:UnlockHighlight() end
    end

    if tab == TUNING_TAB then
        frame.gem:Hide()
        frame.crystal:Hide()
        frame.chances:SetText("")
        RefreshTuning()
        return
    end
    frame.makeKeys:Hide()
    frame.makeCamp:Hide()
    local info = ns.tiers[tab + 1]

    -- Les cases de la gemme : la gemme du palier, et le cristal seulement quand le palier en prévoit un.
    if info and info.gem then
        frame.gem:Show()
        frame.gem.text:SetText(ns.T("Avec une gemme : ") .. Name(info.gem.item, info.gem.name))
        frame.gem:SetChecked(useGem)
    else
        frame.gem:Hide()
    end
    if info and info.crystal and useGem then
        frame.crystal:Show()
        frame.crystal.text:SetText(ns.T("et ") .. Name(info.crystal.item, info.crystal.name))
        frame.crystal:SetChecked(useCrystal)
    else
        frame.crystal:Hide()
    end
    frame.chances:SetText(ChancesText(info))

    if not info then
        frame.need:SetText(ns.T("Demande des paliers au serveur..."))
        for i = 1, 5 do frame.mats[i]:SetText("") end
    else
        local skillOk = Skill() >= info.skill
        frame.need:SetText(ns.T("Compétence de barde demandée : ") .. (skillOk and "|cffffffff" or "|cffff2020") .. info.skill
            .. ns.T("|r (vous : ") .. Skill() .. ")")
        local needs = Needs(info)
        if #needs == 0 then
            frame.mats[1]:SetText(ns.T("|cff9fef9fOffert par le formateur : aucun matériau.|r"))
            for i = 2, 5 do frame.mats[i]:SetText("") end
        else
            for i = 1, 5 do
                local need = needs[i]
                if need then
                    local have = GetItemCount(need.item)
                    frame.mats[i]:SetText((have >= need.count and "|cff9fef9f" or "|cffff4040") .. need.count .. " × "
                        .. Name(need.item, need.name) .. ns.T("|r  (vous : ") .. have .. ")")
                else
                    frame.mats[i]:SetText("")
                end
            end
        end
    end

    local craftable, why = Check(info)
    for i = 1, ROWS do
        local row = frame.rows[i]
        local ins = ns.instruments[i]
        if not ins then
            row:Hide()
        else
            row:Show()
            row.name:SetText(ns.TierInstrument(ins, info and info.suffix or "de cuivre"))
            local owned = ins.owned and ins.owned[tab]
            if owned then
                row.state:SetText(ns.T("|cff808080possédé|r"))
                row.button:Hide()
            else
                row.button:Show()
                row.button:SetText(ns.T("Fabriquer"))
                row.state:SetText(craftable and "" or ("|cffff4040" .. (why or "") .. "|r"))
                if craftable then row.button:Enable() else row.button:Disable() end
                row.button.mode = "craft"
                row.button.instrument = ins.id
            end
        end
    end
end

local function CheckBox(parent, x, y, onClick)
    local box = CreateFrame("CheckButton", nil, parent, "UICheckButtonTemplate")
    ns.Size(box, 24, 24)
    box:SetPoint("TOPLEFT", x, y)
    box.text = box:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    box.text:SetPoint("LEFT", box, "RIGHT", 0, 1)
    box:SetScript("OnClick", function(self) onClick(self:GetChecked() and true or false) end)
    return box
end

local function Create()
    local f = CreateFrame("Frame", "BardeAtelierFrame", UIParent)
    ns.Size(f, 440, 520)
    f:SetPoint("CENTER", UIParent, "CENTER", 0, 20)
    f:SetFrameStrata("DIALOG")
    f:SetToplevel(true)
    f:SetMovable(true)
    f:EnableMouse(true)
    f:SetClampedToScreen(true)
    f:RegisterForDrag("LeftButton")
    f:SetScript("OnDragStart", f.StartMoving)
    f:SetScript("OnDragStop", f.StopMovingOrSizing)
    f:SetBackdrop({
        bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background",
        edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
        tile = true, tileSize = 32, edgeSize = 32,
        insets = { left = 11, right = 12, top = 12, bottom = 11 },
    })
    f:Hide()
    tinsert(UISpecialFrames, "BardeAtelierFrame")      -- Échap ferme l'atelier

    local header = f:CreateTexture(nil, "ARTWORK")
    ns.Size(header, 300, 64)
    header:SetPoint("TOP", f, "TOP", 0, 12)
    header:SetTexture("Interface\\DialogFrame\\UI-DialogBox-Header")
    local title = f:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    title:SetPoint("TOP", header, "TOP", 0, -14)
    title:SetText(ns.T("Atelier du barde"))

    local close = CreateFrame("Button", nil, f, "UIPanelCloseButton")
    close:SetPoint("TOPRIGHT", -8, -8)

    -- Les onglets : cinq paliers, puis l'accordage.
    f.tabs = {}
    for t = 0, TUNING_TAB do
        local button = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
        ns.Size(button, 66, 22)
        button:SetPoint("TOPLEFT", 22 + t * 68, -44)
        button:SetText(TABS[t])
        button:SetScript("OnClick", function()
            tab = t
            if t ~= 4 then useCrystal = false end
            Atelier.Refresh()
        end)
        f.tabs[t] = button
    end

    f.need = f:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    f.need:SetPoint("TOPLEFT", 26, -80)
    f.need:SetJustifyH("LEFT")
    f.mats = {}
    for i = 1, 5 do
        local line = f:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
        line:SetPoint("TOPLEFT", 26, -98 - (i - 1) * 13)
        line:SetJustifyH("LEFT")
        f.mats[i] = line
    end

    -- La gemme de qualité, facultative, et les chances de chaque qualité.
    f.gem = CheckBox(f, 20, -164, function(on)
        useGem = on
        if not on then useCrystal = false end
        Atelier.Refresh()
    end)
    f.crystal = CheckBox(f, 270, -164, function(on)
        useCrystal = on
        Atelier.Refresh()
    end)
    f.chances = f:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    f.chances:SetPoint("TOPLEFT", 26, -192)
    f.chances:SetJustifyH("LEFT")

    -- Les clés d'accordage (onglet Accordage).
    f.makeKeys = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    ns.Size(f.makeKeys, 130, 22)
    f.makeKeys:SetPoint("TOPLEFT", 26, -168)
    f.makeKeys:SetText(ns.T("Fabriquer 5 clés"))
    f.makeKeys:SetScript("OnClick", function() ns.MakeKeys() end)
    f.makeKeys:Hide()

    f.makeCamp = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    ns.Size(f.makeCamp, 150, 22)
    f.makeCamp:SetPoint("LEFT", f.makeKeys, "RIGHT", 10, 0)
    f.makeCamp:SetText(ns.T("Fabriquer un camp"))
    f.makeCamp:SetScript("OnClick", function() ns.MakeCamp() end)
    f.makeCamp:Hide()

    local rule = f:CreateTexture(nil, "ARTWORK")
    ns.Size(rule, 390, 1)
    rule:SetPoint("TOPLEFT", 26, -208)
    rule:SetTexture(0.851, 0.643, 0.255, 0.4)

    -- Les instruments : un par ligne, avec « Fabriquer » ou « Accorder ».
    f.rows = {}
    for i = 1, ROWS do
        local row = CreateFrame("Frame", nil, f)
        ns.Size(row, 400, ROW_H)
        row:SetPoint("TOPLEFT", 22, FIRST_ROW - (i - 1) * ROW_H)
        row.name = row:CreateFontString(nil, "OVERLAY", "GameFontNormal")
        row.name:SetPoint("LEFT", 6, 0)
        row.name:SetJustifyH("LEFT")
        row.state = row:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
        row.state:SetPoint("RIGHT", -100, 0)
        row.state:SetJustifyH("RIGHT")
        row.button = CreateFrame("Button", nil, row, "UIPanelButtonTemplate")
        ns.Size(row.button, 90, 20)
        row.button:SetPoint("RIGHT", -2, 0)
        row.button:SetText(ns.T("Fabriquer"))
        row.button:SetScript("OnClick", function(self)
            if not self.instrument then return end
            if self.mode == "tune" then
                ns.Tune(self.instrument)
            else
                ns.Craft(self.instrument, tab, GemMode(ns.tiers[tab + 1]))
            end
        end)
        f.rows[i] = row
    end

    -- La quête de l'instrument légendaire.
    f.legend = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    ns.Size(f.legend, 170, 22)
    f.legend:SetPoint("BOTTOM", f, "BOTTOM", 0, 28)
    f.legend:SetText(ns.IsEnglish() and "Legendary quest" or "Quête légendaire")
    f.legend:SetScript("OnClick", function() ns.Legend.Toggle() end)

    f:SetScript("OnShow", function() PlaySound("igCharacterInfoOpen") end)
    f:SetScript("OnHide", function() PlaySound("igCharacterInfoClose") end)
    return f
end

function Atelier.Show()
    frame = frame or Create()
    frame:Show()
    ns.RequestSongs()
    Atelier.Refresh()
end

function Atelier.Toggle()
    if frame and frame:IsShown() then frame:Hide() else Atelier.Show() end
end

-- Les sacs changent (matériaux ramassés, instrument reçu ou fabriqué) : un instant plus tard, on redemande l'état au serveur,
-- qui dit quels instruments on possède.
local events = CreateFrame("Frame")
local refreshAt
events:RegisterEvent("BAG_UPDATE")
events:SetScript("OnEvent", function() refreshAt = GetTime() + 0.8 end)
events:SetScript("OnUpdate", function()
    if refreshAt and GetTime() >= refreshAt then
        refreshAt = nil
        if frame and frame:IsShown() then ns.RequestSongs() end
    end
end)
