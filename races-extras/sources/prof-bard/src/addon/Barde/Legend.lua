-- Barde : la quête des Mille Voix, qui mène à l'instrument légendaire (un seul par personnage, lié à l'âme).
local _, ns = ...

local Legend = {}
ns.Legend = Legend

local frame
local forgeInstrument

local function L(fr, en)
    return ns.IsEnglish() and en or fr
end

StaticPopupDialogs["BARDE_FORGE"] = {
    text = "%s",
    button1 = YES,
    button2 = NO,
    OnAccept = function() if forgeInstrument then ns.Send("LEGENDFORGE", forgeInstrument) end end,
    timeout = 0,
    whileDead = false,
    hideOnEscape = true,
}

local function HasEpic(mask, id)
    return math.floor((mask or 0) / 2 ^ id) % 2 == 1
end

local function Create()
    local f = CreateFrame("Frame", "BardeLegendFrame", UIParent)
    ns.Size(f, 520, 400)
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
    tinsert(UISpecialFrames, "BardeLegendFrame")

    local header = f:CreateTexture(nil, "ARTWORK")
    ns.Size(header, 300, 64)
    header:SetPoint("TOP", f, "TOP", 0, 12)
    header:SetTexture("Interface\\DialogFrame\\UI-DialogBox-Header")
    local title = f:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    title:SetPoint("TOP", header, "TOP", 0, -14)
    title:SetText(L("Quête des Mille Voix", "Quest of a Thousand Voices"))
    local close = CreateFrame("Button", nil, f, "UIPanelCloseButton")
    close:SetPoint("TOPRIGHT", -8, -8)

    f.intro = f:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    f.intro:SetPoint("TOPLEFT", 26, -34)
    f.intro:SetWidth(468)
    f.intro:SetJustifyH("LEFT")
    f.intro:SetText(L("Un seul instrument légendaire par personnage, lié à l'âme : on ne peut ni l'échanger, ni le vendre, ni l'acheter. Chant x1,2, jamais désaccordé, et le titre de Barde légendaire. Cinq épreuves, toutes difficiles.",
        "One legendary instrument per character, soulbound: it cannot be traded, sold or bought. Chant x1.2, never out of tune, and the Legendary Bard title. Five trials, all hard."))

    f.steps = {}
    for i = 1, 5 do
        local line = f:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
        line:SetPoint("TOPLEFT", 26, -86 - (i - 1) * 40)
        line:SetWidth(350)
        line:SetJustifyH("LEFT")
        f.steps[i] = line
    end

    f.give = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    ns.Size(f.give, 100, 22)
    f.give:SetPoint("TOPRIGHT", -26, -122)
    f.give:SetText(L("Remettre", "Hand in"))
    f.give:SetScript("OnClick", function() ns.Send("LEGENDGIVE") end)

    -- La forge : l'instrument d'arcanite épique qu'on choisit.
    local menu = CreateFrame("Frame", "BardeLegendInstrument", f, "UIDropDownMenuTemplate")
    menu:SetPoint("TOPLEFT", 10, -300)
    UIDropDownMenu_Initialize(menu, function()
        local mask = ns.legend and ns.legend.epic or 0
        for _, ins in ipairs(ns.instruments) do
            if HasEpic(mask, ins.id) then
                local info = UIDropDownMenu_CreateInfo()
                info.text = ns.InstrumentName(ins)
                info.value = ins.id
                info.checked = (forgeInstrument == ins.id)
                info.func = function(self)
                    forgeInstrument = self.value
                    Legend.Refresh()
                end
                UIDropDownMenu_AddButton(info)
            end
        end
    end)
    UIDropDownMenu_SetWidth(menu, 200)
    f.menu = menu

    f.forge = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    ns.Size(f.forge, 150, 24)
    f.forge:SetPoint("LEFT", menu, "RIGHT", -6, 2)
    f.forge:SetText(L("Forger la légende", "Forge the legend"))
    f.forge:SetScript("OnClick", function()
        if not forgeInstrument then return end
        local ins = ns.GetInstrument(forgeInstrument)
        local name = ins and ns.InstrumentName(ins) or "?"
        StaticPopup_Show("BARDE_FORGE", L(
            "Forger " .. name .. " en instrument légendaire ?\nC'est définitif : un seul par personnage, lié à l'âme.",
            "Forge " .. name .. " into your legendary instrument?\nThis is final: one per character, soulbound."))
    end)

    f.note = f:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    f.note:SetPoint("BOTTOMLEFT", 26, 24)
    f.note:SetWidth(468)
    f.note:SetJustifyH("LEFT")
    return f
end

local function Mark(done)
    return done and "|cff9fef9f[X]|r " or "|cffff4040[ ]|r "
end

function Legend.Refresh()
    if not frame or not frame:IsShown() then return end
    local s = ns.legend
    if not s then return end
    local total = s.total
    frame.steps[1]:SetText(Mark(s.gold >= total and s.perfect >= total) .. L(
        "1. Maîtrise or (40 réussites) et un passage à 100 % sur les " .. total .. " airs du carnet.\n   Or : " .. s.gold .. "/" .. total .. "  -  100 % : " .. s.perfect .. "/" .. total,
        "1. Gold mastery (40 successes) and one 100 % run on all " .. total .. " songs of the book.\n   Gold: " .. s.gold .. "/" .. total .. "  -  100%: " .. s.perfect .. "/" .. total))
    frame.steps[2]:SetText(Mark(s.delivered) .. L(
        "2. Remettre 10 barres d'arcanite, 5 essences d'eau, 1 braise vivante.\n   Vous avez : " .. s.bars .. "/10, " .. s.essences .. "/5, " .. s.embers .. "/1",
        "2. Hand in 10 arcanite bars, 5 essences of water, 1 living ember.\n   You have: " .. s.bars .. "/10, " .. s.essences .. "/5, " .. s.embers .. "/1"))
    frame.steps[3]:SetText(Mark(s.duel) .. L(
        "3. Battre un virtuose en duel (le titre, ou l'or sur 3 airs).",
        "3. Beat a virtuoso in a duel (the title, or gold on 3 songs)."))
    frame.steps[4]:SetText(Mark(s.trio) .. L(
        "4. Jouer un air de Grand barde en trio : trois bardes, 100 % chacun.",
        "4. Play a Grand Bard song as a trio: three bards, 100 % each."))
    frame.steps[5]:SetText(Mark(s.forged) .. L(
        "5. Forger : un instrument d'arcanite épique devient légendaire.",
        "5. Forge: an epic arcanite instrument becomes legendary."))
    if s.delivered or s.forged or s.bars < 10 or s.essences < 5 or s.embers < 1 then frame.give:Disable() else frame.give:Enable() end
    local ready = s.gold >= total and s.perfect >= total and s.delivered and s.duel and s.trio and not s.forged
    local hasEpic = (s.epic or 0) > 0
    if not hasEpic or (forgeInstrument and not HasEpic(s.epic, forgeInstrument)) then forgeInstrument = nil end
    local chosen = forgeInstrument and ns.GetInstrument(forgeInstrument)
    UIDropDownMenu_SetText(frame.menu, chosen and ns.InstrumentName(chosen) or L("Choisir l'instrument...", "Choose the instrument..."))
    if forgeInstrument and ready then frame.forge:Enable() else frame.forge:Disable() end
    if s.forged then
        frame.note:SetText(L("|cff9fef9fVotre instrument légendaire est forgé. Il n'y en a qu'un.|r", "|cff9fef9fYour legendary instrument is forged. There is only one.|r"))
    elseif not hasEpic then
        frame.note:SetText(L("Il faut aussi un instrument d'arcanite de qualité épique (le choix se fait au moment de forger).",
            "You also need an epic-quality arcanite instrument (you choose which one when forging)."))
    else
        frame.note:SetText("")
    end
end

function Legend.Show()
    frame = frame or Create()
    frame:Show()
    ns.Send("LEGEND")
end

function Legend.Toggle()
    if frame and frame:IsShown() then frame:Hide() else Legend.Show() end
end

-- Les sacs changent (matériaux reçus ou remis, instrument fabriqué) : un instant plus tard, on redemande l'état au serveur.
local events = CreateFrame("Frame")
local pending = false
events:RegisterEvent("BAG_UPDATE")
events:SetScript("OnEvent", function()
    if not frame or not frame:IsShown() or pending then return end
    pending = true
    local waited = 0
    events:SetScript("OnUpdate", function(self, elapsed)
        waited = waited + elapsed
        if waited >= 1 then
            self:SetScript("OnUpdate", nil)
            pending = false
            if frame:IsShown() then ns.Send("LEGEND") end
        end
    end)
end)
