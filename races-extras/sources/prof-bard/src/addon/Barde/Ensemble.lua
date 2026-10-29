-- Barde : la fenêtre de préparation d'un ensemble (duo ou trio), dans l'esprit de la préparation de groupe d'un donjon :
-- une carte par barde (l'icône de son chant, une coche, une croix ou un sablier), puis la justesse de chacun à la fin du morceau.
local _, ns = ...

local Ensemble = {}
ns.Ensemble = Ensemble

local frame
local hideAt
local countdownEnd

local function L(fr, en)
    return ns.IsEnglish() and en or fr
end

local PARTS_FR = { [0] = "Mélodie", "Rythme", "Harmonie" }
local PARTS_EN = { [0] = "Melody", "Rhythm", "Harmony" }
local SLOTS = 3
local SLOT_WIDTH, SLOT_GAP = 104, 8

local TEXTURE_READY = "Interface\\RaidFrame\\ReadyCheck-Ready"
local TEXTURE_NOT_READY = "Interface\\RaidFrame\\ReadyCheck-NotReady"
local TEXTURE_WAITING = "Interface\\RaidFrame\\ReadyCheck-Waiting"
local TEXTURE_DEFAULT_ICON = "Interface\\Icons\\INV_Misc_Note_01"

local BORDER = {
    waiting = { 1, 0.82, 0 },
    ok = { 0.3, 0.9, 0.3 },
    no = { 0.9, 0.25, 0.25 },
    done = { 0.3, 0.9, 0.3 },
}

local function Percent(permille)
    return math.floor((permille or 0) / 10 + 0.5) .. " %"
end

local function AccuracyColor(permille)
    if permille >= 900 then return "|cff9fef9f" end
    if permille >= 700 then return "|cffffd100" end
    if permille >= 500 then return "|cffff9a3c" end
    return "|cffff4040"
end

local function CreateSlot(parent, index)
    local slot = CreateFrame("Frame", nil, parent)
    ns.Size(slot, SLOT_WIDTH, 132)
    slot:SetBackdrop({
        bgFile = "Interface\\ChatFrame\\ChatFrameBackground",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true, tileSize = 16, edgeSize = 14,
        insets = { left = 3, right = 3, top = 3, bottom = 3 },
    })
    slot:SetBackdropColor(0.05, 0.06, 0.1, 0.9)

    slot.frame = CreateFrame("Frame", nil, slot)
    ns.Size(slot.frame, 46, 46)
    slot.frame:SetPoint("TOP", 0, -10)
    slot.frame:SetBackdrop({ edgeFile = "Interface\\Buttons\\UI-Quickslot2", edgeSize = 14 })
    slot.icon = slot.frame:CreateTexture(nil, "ARTWORK")
    ns.Size(slot.icon, 40, 40)
    slot.icon:SetPoint("CENTER")
    slot.icon:SetTexCoord(0.07, 0.93, 0.07, 0.93)

    slot.badge = slot.frame:CreateTexture(nil, "OVERLAY")
    ns.Size(slot.badge, 24, 24)
    slot.badge:SetPoint("BOTTOMRIGHT", 8, -8)

    slot.name = slot:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    slot.name:SetPoint("TOP", slot.frame, "BOTTOM", 0, -8)
    slot.name:SetWidth(SLOT_WIDTH - 10)

    slot.instrument = slot:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    slot.instrument:SetPoint("TOP", slot.name, "BOTTOM", 0, -3)
    slot.instrument:SetWidth(SLOT_WIDTH - 10)

    slot.part = slot:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    slot.part:SetPoint("TOP", slot.instrument, "BOTTOM", 0, -2)
    slot.part:SetWidth(SLOT_WIDTH - 10)

    slot.accuracy = slot:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    slot.accuracy:SetPoint("BOTTOM", 0, 8)
    slot.accuracy:SetWidth(SLOT_WIDTH - 10)
    return slot
end

local function Create()
    local f = CreateFrame("Frame", "BardeEnsembleFrame", UIParent)
    ns.Size(f, 372, 276)
    f:SetPoint("TOP", UIParent, "TOP", 0, -90)
    f:SetFrameStrata("HIGH")
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

    local header = f:CreateTexture(nil, "ARTWORK")
    ns.Size(header, 300, 64)
    header:SetPoint("TOP", f, "TOP", 0, 12)
    header:SetTexture("Interface\\DialogFrame\\UI-DialogBox-Header")
    local title = f:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    title:SetPoint("TOP", header, "TOP", 0, -14)
    title:SetText(L("Préparation de l'ensemble", "Ensemble preparation"))
    local close = CreateFrame("Button", nil, f, "UIPanelCloseButton")
    close:SetPoint("TOPRIGHT", -6, -6)

    f.song = f:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    f.song:SetPoint("TOP", 0, -34)
    f.song:SetWidth(330)

    f.status = f:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    f.status:SetPoint("TOP", f.song, "BOTTOM", 0, -4)
    f.status:SetWidth(330)

    f.slots = {}
    for i = 1, SLOTS do
        f.slots[i] = CreateSlot(f, i)
    end

    f.result = f:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    f.result:SetPoint("BOTTOM", 0, 26)
    f.result:SetWidth(340)

    f:SetScript("OnUpdate", function(self)
        if countdownEnd then
            local left = math.ceil(countdownEnd - GetTime())
            if left <= 0 then
                countdownEnd = nil
                self.result:SetText(L("|cffffd100C'est parti !|r", "|cffffd100Here we go!|r"))
            else
                self.result:SetText(L("Le morceau commence dans |cffffd100", "The song starts in |cffffd100") .. left .. "|r s")
            end
        end
        if hideAt and GetTime() >= hideAt then
            hideAt = nil
            self:Hide()
        end
    end)
    return f
end

local function ShowSlot(slot, entry, phase)
    local state = tonumber(entry.state)
    local ins = ns.GetInstrument(tonumber(entry.instrumentId))
    local texture = ins and ins.buffSpell and GetSpellTexture(ins.buffSpell) or nil
    slot.icon:SetTexture(state == 0 and TEXTURE_DEFAULT_ICON or texture or TEXTURE_DEFAULT_ICON)
    slot.icon:SetDesaturated(state == 0 or state == 2)

    local kind
    if state == 0 then
        kind = "waiting"
        slot.badge:SetTexture(TEXTURE_WAITING)
    elseif state == 2 then
        kind = "no"
        slot.badge:SetTexture(TEXTURE_NOT_READY)
    elseif state == 3 then
        kind = "done"
        slot.badge:SetTexture(TEXTURE_READY)
    else
        kind = "ok"
        slot.badge:SetTexture(TEXTURE_READY)
    end
    local color = BORDER[kind]
    slot:SetBackdropBorderColor(color[1], color[2], color[3], 1)

    slot.name:SetText(entry.name)
    local parts = ns.IsEnglish() and PARTS_EN or PARTS_FR
    if state == 0 then
        slot.instrument:SetText(L("|cffffd100Réponse attendue...|r", "|cffffd100Waiting...|r"))
        slot.part:SetText("")
    elseif state == 2 then
        slot.instrument:SetText(L("|cffff4040Ne joue pas|r", "|cffff4040Not playing|r"))
        slot.part:SetText("")
    else
        slot.instrument:SetText(ins and ns.InstrumentName(ins) or "?")
        slot.part:SetText(phase >= 1 and (parts[tonumber(entry.part)] or "") or "")
    end

    if state == 3 then
        local permille = tonumber(entry.accuracy) or 0
        slot.accuracy:SetText(AccuracyColor(permille) .. Percent(permille) .. "|r")
    elseif state == 1 and phase == 1 then
        slot.accuracy:SetText("|cffb0b0c0...|r")
    else
        slot.accuracy:SetText("")
    end
    slot:Show()
end

-- ENSTATE^id^phase^air^bardes^moyenne‰^chant^nom|état|instrument|partie|justesse‰;...^multiplicateur‰
function Ensemble.Update(f)
    frame = frame or Create()
    local phase = tonumber(f[3]) or 0
    local song = f[4] or "?"
    local count = tonumber(f[5]) or 0
    local average = tonumber(f[6]) or 0
    local chant = f[7] == "1"
    local multiplier = tonumber(f[9])
    local countdown = tonumber(f[10]) or 0

    frame.song:SetText(song)

    local entries = {}
    for text in string.gmatch(f[8] or "", "[^;]+") do
        local name, state, instrumentId, part, accuracy = string.match(text, "^([^|]*)|(%d+)|(%d+)|(%d+)|(%d+)$")
        if name then
            table.insert(entries, { name = name, state = state, instrumentId = instrumentId, part = part, accuracy = accuracy })
        end
    end

    local group = (#entries >= 3 and L("Trio", "Trio") or L("Duo", "Duo"))
    local multiplierText = multiplier and multiplier > 1000 and (" |cffffd100x" .. string.format("%.1f", multiplier / 1000) .. "|r") or ""
    if phase == 0 then
        frame.status:SetText(group .. multiplierText .. L("  -  en attente des réponses", "  -  waiting for the answers"))
    elseif phase == 4 then
        frame.status:SetText(group .. multiplierText .. L("  -  tout le monde est prêt", "  -  everybody is ready"))
    elseif phase == 1 then
        frame.status:SetText(group .. multiplierText .. L("  -  le morceau est en cours", "  -  the song is playing"))
    elseif phase == 2 then
        frame.status:SetText(group .. multiplierText .. L("  -  morceau terminé", "  -  song over"))
    else
        frame.status:SetText(L("|cffff4040L'ensemble est annulé|r", "|cffff4040The ensemble is cancelled|r"))
    end

    local shown = math.min(math.max(#entries, 1), SLOTS)
    local total = shown * SLOT_WIDTH + (shown - 1) * SLOT_GAP
    local left = (372 - total) / 2
    for i = 1, SLOTS do
        local slot = frame.slots[i]
        if i <= shown and entries[i] then
            slot:ClearAllPoints()
            slot:SetPoint("TOPLEFT", frame, "TOPLEFT", left + (i - 1) * (SLOT_WIDTH + SLOT_GAP), -82)
            ShowSlot(slot, entries[i], phase)
        else
            slot:Hide()
        end
    end

    if phase == 2 then
        frame.result:SetText(L("Justesse du groupe : ", "Group accuracy: ") .. AccuracyColor(average) .. Percent(average) .. "|r\n"
            .. (chant and L("|cff9fef9fLe chant est donné au groupe|r", "|cff9fef9fThe chant is given to the group|r")
                or L("|cffff4040Sous le minimum : pas de chant|r", "|cffff4040Under the minimum: no chant|r")))
    elseif phase ~= 4 then
        frame.result:SetText("")
    end

    countdownEnd = (phase == 4 and countdown > 0) and (GetTime() + countdown / 1000) or nil
    if phase == 1 then
        -- Le morceau court : la scène de jeu prend l'écran, la fenêtre revient avec les résultats.
        frame:Hide()
        hideAt = nil
        return
    end
    frame:Show()
    hideAt = (phase == 2 and GetTime() + 25) or (phase == 3 and GetTime() + 5) or nil
end
