-- Barde : le carnet, à l'image de la fenêtre des métiers (Cuisine, Forge...).
-- Licence AGPL-3.0, voir LICENSE.
--
-- Les mesures et les images sont celles de TradeSkillFrame dans le client CoA (patch-B.MPQ) :
-- cadre du formateur de classe, barre de progression, onglet « Tous », liste de 8 lignes, barre
-- horizontale, détail, boutons du bas. Seuls les contenus changent : airs au lieu de recettes,
-- « Chant » à la place des composants, « Jouer » à la place de « Créer ».

local ADDON, ns = ...

local UI = {}
ns.UI = UI

local TRAINER = "Interface\\ClassTrainerFrame\\UI-ClassTrainer-"
local PORTRAIT = "Interface\\Icons\\inv_mandolim"
local BUFF_ICON = "Interface\\Icons\\guitar"        -- la même icône que l'aura (sort 1950101)

local ROWS = 8
local ROW_H = 16
local ROW_W = 293
local FILTERS = ns.Lazy({
    all = "Tous les airs",
    avail = "Jouables",
    new = "Jamais joués",
    won = "Réussis",
})

local frame
local selected              -- identifiant de l'air sélectionné
local filter = "all"
local collapsed = {}        -- palier -> true quand sa liste est repliée
local rows = {}             -- lignes affichables : { header = true, tier = n } ou { song = ... }

-- Couleurs d'un air, comme celles d'une recette : plus la compétence dépasse l'exigence de l'air,
-- moins il fait gagner de points (orange : un point sûr, jaune : très probable, vert : peu probable, gris : plus rien).
local COLORS = {
    header = { 1.0, 0.82, 0.0 },
    locked = { 1.0, 0.13, 0.13 },   -- compétence insuffisante
    orange = { 1.0, 0.5, 0.25 },
    yellow = { 1.0, 1.0, 0.0 },
    green = { 0.25, 0.75, 0.25 },
    gray = { 0.5, 0.5, 0.5 },
    mine = { 0.85, 0.85, 0.95 },
}

-- ------------------------------------------------------------- données

local function Status(song)
    if song.wins > 0 then return "won" end
    if song.best > 0 then return "tried" end
    return "new"
end

local function Skill()
    return ns.info.skill or 1
end

local function Difficulty(song)
    if song.mine then return "mine" end           -- une composition ne demande aucune compétence
    local above = Skill() - song.skill
    if above < 0 then return "locked" end
    if above < 20 then return "orange" end
    if above < 40 then return "yellow" end
    if above < 60 then return "green" end
    return "gray"
end

local function Matches(song)
    local text = string.lower(BardeSearchBox and BardeSearchBox:GetText() or "")
    if text ~= "" and not string.find(string.lower(song.name), text, 1, true) then return false end
    if filter == "avail" then return Skill() >= song.skill end
    return filter == "all" or Status(song) == filter
end

local function BuildRows()
    wipe(rows)
    local lastGroup
    local ordered = {}
    for _, song in ipairs(ns.songs) do
        if Matches(song) then table.insert(ordered, song) end
    end
    table.sort(ordered, function(a, b)
        local ga, gb = ns.GroupOf(a), ns.GroupOf(b)
        if ga ~= gb then return ga < gb end
        if a.tier ~= b.tier then return a.tier < b.tier end
        return a.id < b.id
    end)
    for _, song in ipairs(ordered) do
        local group = ns.GroupOf(song)
        if group ~= lastGroup then
            lastGroup = group
            table.insert(rows, { header = true, tier = group })
        end
        if not collapsed[group] then
            table.insert(rows, { song = song })
        end
    end
end

local function FindSong(id)
    for _, song in ipairs(ns.songs) do
        if song.id == id then return song end
    end
end

-- ------------------------------------------------------------- affichage

local function ToggleTexture(isCollapsed)
    local E = UI.skin
    if E and E.Media and E.Media.Textures and E.Media.Textures.Plus then
        return isCollapsed and E.Media.Textures.Plus or E.Media.Textures.Minus
    end
    return isCollapsed and "Interface\\Buttons\\UI-PlusButton-Up" or "Interface\\Buttons\\UI-MinusButton-Up"
end

local function UpdateList()
    BuildRows()
    local offset = FauxScrollFrame_GetOffset(BardeListScroll)
    FauxScrollFrame_Update(BardeListScroll, #rows, ROWS, ROW_H)
    for i = 1, ROWS do
        local button = frame.rows[i]
        local entry = rows[i + offset]
        if not entry then
            button:Hide()
        else
            button.entry = entry
            button:Show()
            if entry.header then
                local c = COLORS.header
                button.name:SetText(ns.TIERS[entry.tier] or "?")
                button.name:SetTextColor(c[1], c[2], c[3])
                button.name:SetPoint("LEFT", 22, 0)
                button.toggle:Show()
                button.toggle:SetNormalTexture(ToggleTexture(collapsed[entry.tier]))
                button.sel:Hide()
            else
                local song = entry.song
                local level = Difficulty(song)
                local c = COLORS[level]
                local text = ns.N(song.name)
                if level == "locked" then
                    text = text .. " (" .. song.skill .. ")"
                elseif song.wins > 0 then
                    text = text .. " [" .. song.wins .. "]"
                end
                button.name:SetText(text)
                button.name:SetTextColor(c[1], c[2], c[3])
                button.name:SetPoint("LEFT", 30, 0)
                button.toggle:Hide()
                ns.Shown(button.sel, song.id == selected)
            end
        end
    end
end

local function UpdateRank()
    local max = ns.info.skillMax or 300
    BardeRankFrame:SetMinMaxValues(0, max)
    BardeRankFrame:SetValue(Skill())
    frame.rankText:SetText(ns.T("Compétence de barde : ") .. Skill() .. "/" .. max)
end

local function UpdateDetail()
    local song = selected and FindSong(selected)
    local d = frame.detail
    if not song then
        d:Hide()
        return
    end
    d:Show()
    d.name:SetText(ns.N(song.name))
    d.icon:SetTexture(PORTRAIT)

    local ins = ns.CurrentInstrument() or { name = ns.T("Luth de cuivre"), buff = "", effect = "", unit = "", min = 0, max = 0 }
    -- L'instrument demandé : celui du palier de l'air, ou mieux.
    local tierInfo = ns.tiers[song.tier + 1]
    local requires = ns.TierInstrument(ins, tierInfo and tierInfo.suffix or "de cuivre")
    local tierOk = ins.bestTier ~= nil and ins.bestTier >= song.tier
    if song.mine then
        -- Une composition se joue avec n'importe quel instrument et ne demande aucune compétence.
        d.requires:SetText(ns.T("Requis : ") .. "|cffffffff" .. ns.T("n'importe quel instrument") .. "|r")
    else
        d.requires:SetText(ns.T("Requis : ") .. (tierOk and ("|cffffffff" .. requires .. "|r") or ("|cffff2020" .. requires .. "|r"))
            .. ns.T(" · compétence ") .. (Skill() >= song.skill and ("|cffffffff" .. song.skill .. "|r") or ("|cffff2020" .. song.skill .. "|r")))
    end

    d.about:SetText(ns.Sub(song.subtitle) .. "\n" .. song.bpm .. " BPM · " .. song.count .. ns.T(" notes")
        .. (song.holds > 0 and (ns.T(" dont ") .. song.holds .. ns.T(" tenues")) or ""))

    local base = ns.Round(ns.Base(song, ins))
    d.buffIcon:SetTexture((ins.buffSpell and GetSpellTexture(ins.buffSpell)) or BUFF_ICON)
    d.buffCount:SetText("+" .. base)
    if song.mine then
        d.buffName:SetText(ns.T("Aucun chant"))
        d.buffCount:SetText("")
        d.buffLine:SetText(ns.T("Ni chant ni compétence : pour le plaisir et les concerts."))
        d.buffIcon:SetTexture(BUFF_ICON)
    else
    d.buffName:SetText(ins.buff ~= "" and ns.N(ins.buff) or ns.T("Chant"))
    d.buffLine:SetText(ns.T("jusqu'à +") .. ns.Value(ins, base) .. " " .. ns.N(ins.effect) .. "\n" .. (10 + 5 * song.tier) .. ns.T(" min"))
    end

    -- Maîtrise : le palier atteint, et ce qu'il reste pour le suivant.
    ns.Shown(d.topButton, not song.mine)
    ns.Shown(d.deleteButton, song.mine)
    ns.Shown(d.copyButton, song.kind == 1)
    local tier = ns.Mastery(song.wins)
    local nextStep = ns.MASTERY[tier + 1]
    local mastery = tier > 0 and (ns.MASTERY[tier].color .. ns.T("Maîtrise ") .. ns.T(ns.MASTERY[tier].name) .. " +" .. ns.MASTERY[tier].bonus .. " %|r")
        or ns.T("Aucune maîtrise")
    d.mastery:SetText(mastery .. " · " .. song.wins .. ns.T(" réussite") .. (song.wins > 1 and "s" or "")
        .. (nextStep and (" · " .. (nextStep.wins - song.wins) .. ns.T(" avant ") .. ns.T(nextStep.name)) or ""))
    if song.best > 0 then
        d.record:SetText(ns.T("Meilleur score : |cffffffff") .. ns.Int(song.best) .. "|r (" .. math.floor(song.bestAcc * 100 + 0.5)
            .. " %)")
    else
        d.record:SetText(ns.T("Jamais joué."))
    end
end

-- La fenêtre du classement : les cinq meilleurs scores de l'air choisi.
function UI.ShowTop()
    if not frame or not ns.top then return end
    if not frame.top then
        local t = CreateFrame("Frame", "BardeTopFrame", frame)
        ns.Size(t, 300, 150)
        t:SetPoint("TOPLEFT", frame, "TOPRIGHT", -8, -110)
        t:SetBackdrop({
            bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background",
            edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
            tile = true, tileSize = 32, edgeSize = 24,
            insets = { left = 8, right = 8, top = 8, bottom = 8 },
        })
        t.title = t:CreateFontString(nil, "OVERLAY", "GameFontNormal")
        t.title:SetPoint("TOP", 0, -14)
        t.lines = {}
        for i = 1, 5 do
            local line = t:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
            line:SetPoint("TOPLEFT", 20, -36 - (i - 1) * 18)
            line:SetJustifyH("LEFT")
            t.lines[i] = line
        end
        local close = CreateFrame("Button", nil, t, "UIPanelCloseButton")
        close:SetPoint("TOPRIGHT", -2, -2)
        frame.top = t
    end
    local song = FindSong(ns.top.song)
    frame.top.title:SetText(ns.T("Classement · ") .. (song and ns.N(song.name) or "?"))
    for i = 1, 5 do
        local entry = ns.top.list[i]
        frame.top.lines[i]:SetText(entry and (i .. ". |cffffffff" .. entry.name .. "|r  " .. ns.Int(entry.score) .. "  (" .. math.floor(entry.acc + 0.5) .. " %)")
            or (i .. ns.T(". |cff808080—|r")))
    end
    frame.top:Show()
end

function UI.RefreshSpec()
    if not frame or not frame.spec then return end
    local current = ns.spec and ns.spec.current or 0
    frame.spec:SetText(ns.T("Spécialité"))
end

function UI.Refresh()
    if not frame then return end
    if not selected and ns.songs[1] then selected = ns.songs[1].id end
    if selected and not FindSong(selected) and ns.songs[1] then selected = ns.songs[1].id end
    UpdateList()
    UpdateRank()
    UpdateDetail()
    if frame.instrumentMenu then
        local ins = ns.CurrentInstrument()
        UIDropDownMenu_SetText(frame.instrumentMenu, ins and ns.InstrumentName(ins) or ns.T("Instrument"))
    end
    ns.Shown(frame.empty, #ns.songs == 0)
end

-- ------------------------------------------------------------- habillage ElvUI

-- Si ElvUI est présent, le carnet reprend son style (fond plat, boutons, barre, ascenseur).
-- Tout est protégé : si une fonction d'ElvUI manque dans votre version, on garde le cadre du jeu.
local function ApplyElvUI(f, parts)
    if not ElvUI then return nil end
    local E = unpack(ElvUI)
    local S = E and E:GetModule("Skins")
    if not (E and S and E.private and E.private.skins and E.private.skins.blizzard
        and E.private.skins.blizzard.enable) then
        return nil
    end

    local ok = pcall(function()
        for _, tex in ipairs(parts.art) do tex:Hide() end
        parts.portrait:Hide()

        f:CreateBackdrop("Transparent")
        f.backdrop:ClearAllPoints()
        f.backdrop:SetPoint("TOPLEFT", 11, -12)
        f.backdrop:SetPoint("BOTTOMRIGHT", -32, 76)
        S:HandleCloseButton(parts.close, f.backdrop)

        parts.rank:CreateBackdrop()
        parts.rank:SetStatusBarTexture(E.media.normTex)
        parts.rank:SetStatusBarColor(0.22, 0.39, 0.84)
        parts.rankBorder:Hide()
        parts.rankBg:Hide()

        for _, tex in ipairs(parts.tab) do tex:Hide() end
        S:HandleEditBox(parts.search)
        S:HandleDropDownBox(parts.dropDown, 120)
        S:HandleDropDownBox(f.instrumentMenu, 130)
        S:HandleButton(parts.training)
        S:HandleButton(parts.play)
        S:HandleButton(parts.together)
        S:HandleButton(parts.workshop)
        S:HandleButton(parts.duel)
        S:HandleButton(parts.improv)
        S:HandleButton(parts.lang)
        S:HandleButton(parts.quit)
        S:HandleScrollBar(BardeListScrollScrollBar)

        for _, row in ipairs(f.rows) do
            if E.Media and E.Media.Textures and E.Media.Textures.Highlight then
                row:SetHighlightTexture(E.Media.Textures.Highlight)
                row:GetHighlightTexture():SetAlpha(0.35)
                row.sel:SetTexture(E.Media.Textures.Highlight)
                row.sel:SetVertexColor(1, 1, 1)
                row.sel:SetAlpha(0.35)
            end
        end
        if E.Media and E.Media.Textures and E.Media.Textures.Minus then
            parts.collapseIcon:SetTexture(E.Media.Textures.Minus)
        end

        for _, pair in ipairs({ { parts.iconFrame, parts.icon }, { parts.buffFrame, parts.buffIcon } }) do
            pair[1]:SetTemplate("Default")
            pair[2]:SetTexCoord(unpack(E.TexCoords))
            pair[2]:SetInside()
        end
        parts.iconBorder:Hide()
        parts.buffBorder:Hide()
        for _, tex in ipairs(parts.header) do tex:Hide() end
        parts.box:SetTemplate("Default")
        parts.boxBg:Hide()
    end)
    return ok and E or nil
end

-- ------------------------------------------------------------- création

local function ResetScroll()
    if FauxScrollFrame_SetOffset then
        FauxScrollFrame_SetOffset(BardeListScroll, 0)
    end
    if BardeListScrollScrollBar then BardeListScrollScrollBar:SetValue(0) end
end

local function Select(id)
    selected = id
    UI.Refresh()
end

local function Art(parent, file, w, h, point, relative, relPoint, x, y, layer)
    local tex = parent:CreateTexture(nil, layer or "BORDER")
    ns.Size(tex, w, h)
    tex:SetPoint(point, relative or parent, relPoint or point, x or 0, y or 0)
    tex:SetTexture(file)
    return tex
end

local function CreateFrameUI()
    local f = CreateFrame("Frame", "BardeFrame", UIParent)
    ns.Size(f, 384, 512)
    f:SetPoint("TOPLEFT", UIParent, "TOPLEFT", 60, -104)
    f:SetFrameStrata("DIALOG")
    f:SetToplevel(true)
    f:SetMovable(true)
    f:EnableMouse(true)
    f:SetClampedToScreen(true)
    f:SetHitRectInsets(0, 34, 0, 75)
    f:RegisterForDrag("LeftButton")
    f:SetScript("OnDragStart", f.StartMoving)
    f:SetScript("OnDragStop", f.StopMovingOrSizing)
    f:Hide()
    tinsert(UISpecialFrames, "BardeFrame")      -- Échap ferme le carnet

    -- Portrait, puis le cadre : mêmes images et mêmes positions que la fenêtre des métiers.
    local portrait = f:CreateTexture(nil, "BACKGROUND")
    ns.Size(portrait, 60, 60)
    portrait:SetPoint("TOPLEFT", 7, -6)
    portrait:SetTexture(PORTRAIT)

    local topLeft = Art(f, TRAINER .. "TopLeft", 256, 256, "TOPLEFT")
    local topRight = Art(f, TRAINER .. "TopRight", 128, 256, "TOPRIGHT")
    local botLeft = Art(f, "Interface\\TradeSkillFrame\\UI-TradeSkill-BotLeft", 256, 256, "BOTTOMLEFT")
    local botRight = Art(f, TRAINER .. "BotRight", 128, 256, "BOTTOMRIGHT")

    -- Barre horizontale entre la liste et le détail.
    local barLeft = Art(f, TRAINER .. "HorizontalBar", 256, 16, "TOPLEFT", f, "TOPLEFT", 15, -221, "ARTWORK")
    barLeft:SetTexCoord(0, 1, 0, 0.25)
    local barRight = Art(f, TRAINER .. "HorizontalBar", 75, 16, "LEFT", barLeft, "RIGHT", 0, 0, "ARTWORK")
    barRight:SetTexCoord(0, 0.29296875, 0.25, 0.5)

    local title = f:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    title:SetPoint("TOP", 0, -17)
    title:SetText(ns.T("Barde"))

    local close = CreateFrame("Button", nil, f, "UIPanelCloseButton")
    close:SetPoint("TOPRIGHT", -29, -8)

    local duel = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    local widthWorkshop = ns.IsEnglish() and 42 or 50
    ns.Size(duel, 36, 18)
    duel:SetPoint("TOPLEFT", 130, -14)
    duel:SetText(ns.T("Duel"))
    duel:SetScript("OnClick", function()
        if selected then ns.Duel(selected) end
    end)
    duel:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:SetText(ns.T("Duel de bardes"))
        GameTooltip:AddLine(ns.T("Sélectionnez un autre barde (clic sur son personnage), puis défiez-le à l'air choisi : ")
            .. ns.T("le meilleur score gagne les pourboires des deux bourses."), 1, 1, 1, 1)
        GameTooltip:Show()
    end)
    duel:SetScript("OnLeave", function() GameTooltip:Hide() end)

    local improv = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    ns.Size(improv, 42, 18)
    improv:SetPoint("TOPLEFT", 84, -14)
    improv:SetText(ns.T("Impro"))
    improv:SetScript("OnClick", function() ns.Improv.Show() end)
    improv:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:SetText(ns.T("Improviser"))
        GameTooltip:AddLine(ns.T("Jouer librement avec les touches A S D F G H J K, sans partition."), 1, 1, 1, 1)
        GameTooltip:Show()
    end)
    improv:SetScript("OnLeave", function() GameTooltip:Hide() end)

    local workshop = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    ns.Size(workshop, widthWorkshop, 18)
    workshop:SetPoint("TOPRIGHT", -62, -14)
    workshop:SetText(ns.IsEnglish() and "Shop" or ns.T("Atelier"))
    workshop:SetScript("OnClick", function() ns.Atelier.Toggle() end)

    -- Barre de progression, comme la barre de compétence d'un métier.
    local rank = CreateFrame("StatusBar", "BardeRankFrame", f)
    ns.Size(rank, 265, 14)
    rank:SetPoint("TOPLEFT", 75, -36)
    rank:SetStatusBarTexture("Interface\\PaperDollInfoFrame\\UI-Character-Skills-Bar")
    rank:SetStatusBarColor(0.25, 0.25, 0.75)
    rank:SetMinMaxValues(0, 1)
    rank:SetValue(0)
    local rankBg = rank:CreateTexture(nil, "BACKGROUND")
    rankBg:SetAllPoints()
    rankBg:SetTexture(1, 1, 1, 0.2)
    local rankBorder = rank:CreateTexture(nil, "ARTWORK")
    ns.Size(rankBorder, 274, 27)
    rankBorder:SetPoint("LEFT", -5, 0)
    rankBorder:SetTexture("Interface\\PaperDollInfoFrame\\UI-Character-Skills-BarBorder")
    f.rankText = rank:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    f.rankText:SetPoint("CENTER", rank, "CENTER", 0, 1)

    -- Recherche (sous la barre, à droite) et filtre (menu déroulant, comme « Toutes les sous-classes »).
    local search = CreateFrame("EditBox", "BardeSearchBox", f, "InputBoxTemplate")
    ns.Size(search, 128, 20)
    search:SetPoint("TOPRIGHT", rank, "BOTTOMRIGHT", 2, 2)
    search:SetAutoFocus(false)
    search:SetMaxLetters(40)
    search:SetTextInsets(16, 0, 0, 0)
    search:SetScript("OnEscapePressed", function(self) self:SetText(""); self:ClearFocus() end)
    search:SetScript("OnEnterPressed", function(self) self:ClearFocus() end)
    search:SetScript("OnTextChanged", function() if frame then UI.Refresh() end end)
    local lens = search:CreateTexture(nil, "OVERLAY")
    ns.Size(lens, 12, 12)
    lens:SetPoint("LEFT", 2, -1)
    lens:SetTexture("Interface\\Common\\UI-Searchbox-Icon")
    lens:SetAlpha(0.7)

    local dropDown = CreateFrame("Frame", "BardeFilterDropDown", f, "UIDropDownMenuTemplate")
    dropDown:SetPoint("TOPRIGHT", -25, -66)
    UIDropDownMenu_Initialize(dropDown, function()
        for _, key in ipairs({ "all", "avail", "new", "won" }) do
            local info = UIDropDownMenu_CreateInfo()
            info.text = FILTERS[key]
            info.value = key
            info.checked = (filter == key)
            info.func = function(self)
                filter = self.value
                UIDropDownMenu_SetText(dropDown, FILTERS[filter])
                ResetScroll()
                UI.Refresh()
            end
            UIDropDownMenu_AddButton(info)
        end
    end)
    UIDropDownMenu_SetWidth(dropDown, 120)
    UIDropDownMenu_SetText(dropDown, FILTERS.all)

    -- Choix de l'instrument : un menu à gauche de la recherche. Les instruments qu'on n'a pas sont en rouge.
    local instrumentMenu = CreateFrame("Frame", "BardeInstrumentDropDown", f, "UIDropDownMenuTemplate")
    instrumentMenu:SetPoint("TOPLEFT", 40, -43)
    UIDropDownMenu_Initialize(instrumentMenu, function()
        for _, ins in ipairs(ns.instruments) do
            local info = UIDropDownMenu_CreateInfo()
            info.text = ins.has and ns.InstrumentName(ins) or ("|cffff4040" .. ns.InstrumentName(ins) .. "|r")
            info.value = ins.id
            info.checked = (ns.instrumentId == ins.id)
            info.func = function(self)
                ns.instrumentId = self.value
                UI.Refresh()
            end
            UIDropDownMenu_AddButton(info)
        end
    end)
    UIDropDownMenu_SetWidth(instrumentMenu, 122)
    UIDropDownMenu_SetText(instrumentMenu, ns.T("Instrument"))
    f.instrumentMenu = instrumentMenu

    -- Onglet « Tous » : replie ou déplie tous les paliers.
    local tab = CreateFrame("Frame", nil, f)
    ns.Size(tab, 54, 32)
    tab:SetPoint("TOPLEFT", 15, -71)
    local tabLeft = tab:CreateTexture(nil, "BACKGROUND")
    ns.Size(tabLeft, 8, 32)
    tabLeft:SetPoint("TOPLEFT")
    tabLeft:SetTexture(TRAINER .. "ExpandTab-Left")
    local tabMiddle = tab:CreateTexture(nil, "BACKGROUND")
    ns.Size(tabMiddle, 38, 32)
    tabMiddle:SetPoint("LEFT", tabLeft, "RIGHT", 0, 6)
    tabMiddle:SetTexture("Interface\\QuestFrame\\UI-QuestLogSortTab-Middle")
    local tabRight = tab:CreateTexture(nil, "BACKGROUND")
    ns.Size(tabRight, 8, 32)
    tabRight:SetPoint("LEFT", tabMiddle, "RIGHT", 0, 0)
    tabRight:SetTexture("Interface\\QuestFrame\\UI-QuestLogSortTab-Right")

    local collapseAll = CreateFrame("Button", nil, tab)
    ns.Size(collapseAll, 40, 22)
    collapseAll:SetPoint("LEFT", tabLeft, "RIGHT", 0, 3)
    collapseAll:SetNormalTexture("Interface\\Buttons\\UI-MinusButton-Up")
    local collapseIcon = collapseAll:GetNormalTexture()
    collapseIcon:ClearAllPoints()
    collapseIcon:SetPoint("LEFT", 3, 0)
    ns.Size(collapseIcon, 16, 16)
    local collapseText = collapseAll:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    collapseText:SetPoint("LEFT", 21, 0)
    collapseText:SetText(ns.T("Tous"))
    collapseAll:SetScript("OnClick", function()
        local anyOpen = false
        for _, group in ipairs(ns.GROUPS) do if not collapsed[group] then anyOpen = true end end
        for _, group in ipairs(ns.GROUPS) do collapsed[group] = anyOpen end
        ResetScroll()
        UI.Refresh()
    end)

    -- La liste : 8 lignes de 16 pixels dans une zone de défilement de 296 x 130.
    local scroll = CreateFrame("ScrollFrame", "BardeListScroll", f, "FauxScrollFrameTemplate")
    ns.Size(scroll, 296, 130)
    scroll:SetPoint("TOPRIGHT", f, "TOPRIGHT", -67, -96)
    scroll:SetScript("OnVerticalScroll", function(self, offset)
        FauxScrollFrame_OnVerticalScroll(self, offset, ROW_H, UpdateList)
    end)

    f.rows = {}
    for i = 1, ROWS do
        local button = CreateFrame("Button", nil, f)
        ns.Size(button, ROW_W, ROW_H)
        button:SetPoint("TOPLEFT", f, "TOPLEFT", 22, -96 - (i - 1) * ROW_H)
        button:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestLogTitleHighlight", "ADD")
        button.sel = button:CreateTexture(nil, "BACKGROUND")
        button.sel:SetAllPoints()
        button.sel:SetTexture("Interface\\Buttons\\UI-Listbox-Highlight2")
        button.sel:Hide()
        button.name = button:CreateFontString(nil, "OVERLAY", "GameFontNormal")
        button.name:SetJustifyH("LEFT")
        button.name:SetWidth(250)
        button.toggle = CreateFrame("Button", nil, button)
        ns.Size(button.toggle, 16, 16)
        button.toggle:SetPoint("LEFT", 3, 0)
        button.toggle:Hide()
        button.toggle:EnableMouse(false)
        button:SetScript("OnClick", function(self)
            local entry = self.entry
            if not entry then return end
            if entry.header then
                collapsed[entry.tier] = not collapsed[entry.tier]
                UI.Refresh()
            else
                Select(entry.song.id)
            end
        end)
        button:SetScript("OnDoubleClick", function(self)
            local entry = self.entry
            if entry and entry.song then
                Select(entry.song.id)
                UI.PlaySelected(false)
            end
        end)
        f.rows[i] = button
    end

    f.empty = f:CreateFontString(nil, "OVERLAY", "GameFontDisable")
    f.empty:SetPoint("TOP", scroll, "TOP", 0, -50)
    f.empty:SetText(ns.T("Demande des airs au serveur..."))

    -- Détail de l'air sélectionné : 297 x 176 sous la barre horizontale, bandeau d'en-tête du formateur.
    local d = CreateFrame("Frame", nil, f)
    d:SetPoint("TOPLEFT", 20, -234)
    ns.Size(d, 297, 176)
    f.detail = d

    local headerLeft = Art(d, TRAINER .. "DetailHeaderLeft", 256, 64, "TOPLEFT", d, "TOPLEFT", 0, 3, "BACKGROUND")
    local headerRight = Art(d, TRAINER .. "DetailHeaderRight", 64, 64, "TOPLEFT", headerLeft, "TOPRIGHT", 0, 0, "BACKGROUND")

    local iconFrame = CreateFrame("Frame", nil, d)
    ns.Size(iconFrame, 37, 37)
    iconFrame:SetPoint("TOPLEFT", 8, -3)
    d.icon = iconFrame:CreateTexture(nil, "ARTWORK")
    d.icon:SetAllPoints()
    local iconBorder = iconFrame:CreateTexture(nil, "OVERLAY")
    ns.Size(iconBorder, 64, 64)
    iconBorder:SetPoint("CENTER", 0, 0)
    iconBorder:SetTexture("Interface\\Buttons\\UI-Quickslot2")

    d.name = d:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    d.name:SetPoint("TOPLEFT", 50, -5)
    d.name:SetJustifyH("LEFT")
    d.name:SetWidth(244)
    d.requires = d:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    d.requires:SetPoint("TOPLEFT", d.name, "BOTTOMLEFT", 0, 0)
    d.requires:SetJustifyH("LEFT")
    d.requires:SetWidth(244)

    d.about = d:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    d.about:SetPoint("TOPLEFT", 5, -50)
    d.about:SetJustifyH("LEFT")
    d.about:SetWidth(290)
    d.about:SetSpacing(2)

    local chantLabel = d:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    chantLabel:SetPoint("TOPLEFT", d.about, "BOTTOMLEFT", 0, -10)
    chantLabel:SetText(ns.T("Chant :"))

    local box = CreateFrame("Frame", nil, d)
    ns.Size(box, 270, 44)
    box:SetPoint("TOPLEFT", chantLabel, "BOTTOMLEFT", 1, -3)
    local boxBg = box:CreateTexture(nil, "BACKGROUND")
    boxBg:SetAllPoints()
    boxBg:SetTexture(0, 0, 0, 0.35)
    local buffFrame = CreateFrame("Frame", nil, box)
    ns.Size(buffFrame, 37, 37)
    buffFrame:SetPoint("LEFT", 3, 0)
    d.buffIcon = buffFrame:CreateTexture(nil, "ARTWORK")
    d.buffIcon:SetAllPoints()
    local buffBorder = buffFrame:CreateTexture(nil, "OVERLAY")
    ns.Size(buffBorder, 64, 64)
    buffBorder:SetPoint("CENTER", 0, 0)
    buffBorder:SetTexture("Interface\\Buttons\\UI-Quickslot2")
    d.buffCount = buffFrame:CreateFontString(nil, "OVERLAY", "NumberFontNormal")
    d.buffCount:SetPoint("BOTTOMRIGHT", -3, 2)
    d.buffName = box:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    d.buffName:SetPoint("TOPLEFT", buffFrame, "TOPRIGHT", 8, -2)
    d.buffName:SetJustifyH("LEFT")
    d.buffLine = box:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    d.buffLine:SetPoint("TOPLEFT", d.buffName, "BOTTOMLEFT", 0, -1)
    d.buffLine:SetJustifyH("LEFT")
    d.buffLine:SetWidth(210)
    d.buffLine:SetTextColor(0.7, 0.7, 0.75)

    d.record = d:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    d.record:SetPoint("TOPLEFT", 5, -148)
    d.record:SetJustifyH("LEFT")
    d.record:SetWidth(210)
    d.mastery = d:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    d.mastery:SetPoint("TOPLEFT", 5, -160)
    d.mastery:SetJustifyH("LEFT")
    d.mastery:SetWidth(290)

    -- Le classement de l'air : les cinq meilleurs scores du serveur.
    local topButton = CreateFrame("Button", nil, d, "UIPanelButtonTemplate")
    ns.Size(topButton, 74, 18)
    topButton:SetPoint("TOPRIGHT", d, "TOPRIGHT", -2, -145)
    topButton:SetText(ns.T("Classement"))
    topButton:SetScript("OnClick", function() if selected then ns.RequestTop(selected) end end)
    d.topButton = topButton

    -- Supprimer une de ses compositions (à la place du classement, que les compositions n'ont pas).
    local deleteButton = CreateFrame("Button", nil, d, "UIPanelButtonTemplate")
    ns.Size(deleteButton, 44, 18)
    deleteButton:SetPoint("TOPRIGHT", d, "TOPRIGHT", -2, -145)
    deleteButton:SetText(ns.T("Suppr."))
    deleteButton:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_TOP")
        GameTooltip:SetText(ns.T("Supprimer"))
        GameTooltip:AddLine(ns.T("Supprime cet air de votre carnet."), 1, 1, 1, 1)
        GameTooltip:Show()
    end)
    deleteButton:SetScript("OnLeave", function() GameTooltip:Hide() end)
    deleteButton:SetScript("OnClick", function()
        StaticPopupDialogs["BARDE_DELETE"].text = ns.T("Supprimer cette composition ? Ses scores sont effacés aussi.")
        StaticPopup_Show("BARDE_DELETE", nil, nil, selected)
    end)
    deleteButton:Hide()
    d.deleteButton = deleteButton

    -- Copier une de ses compositions sur un parchemin (1 Parchemin léger et 1 Encre de lune) : on peut le vendre ou l'échanger.
    local copyButton = CreateFrame("Button", nil, d, "UIPanelButtonTemplate")
    ns.Size(copyButton, 46, 18)
    copyButton:SetPoint("RIGHT", deleteButton, "LEFT", -2, 0)
    copyButton:SetText(ns.T("Copier"))
    copyButton:SetScript("OnClick", function() if selected then ns.Send("COPY", selected) end end)
    copyButton:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_TOP")
        GameTooltip:SetText(ns.T("Copier sur un parchemin"))
        GameTooltip:AddLine(ns.T("Il faut 1 Parchemin léger et 1 Encre de lune. Le parchemin se vend ou s'échange ; celui qui le lit apprend l'air."), 1, 1, 1, 1)
        GameTooltip:Show()
    end)
    copyButton:SetScript("OnLeave", function() GameTooltip:Hide() end)
    copyButton:Hide()
    d.copyButton = copyButton

    -- Boutons du bas : mêmes places que « Tout créer », « Créer » et « Quitter ».
    local training = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    ns.Size(training, 96, 22)
    training:SetPoint("CENTER", f, "TOPLEFT", 58, -422)
    training:SetText(ns.T("Entraînement"))
    training:SetScript("OnClick", function() UI.PlaySelected(true) end)

    local together = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    ns.Size(together, 76, 22)
    together:SetPoint("CENTER", f, "TOPLEFT", 145, -422)
    together:SetText(ns.T("Ensemble"))
    together:SetScript("OnClick", function()
        if selected then ns.PlayTogether(selected) end
    end)
    together:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:SetText(ns.T("Jouer ensemble"))
        GameTooltip:AddLine(ns.T("Propose cet air aux bardes de votre groupe : chacun joue avec son instrument, ")
            .. ns.T("et les chants vont à tout le groupe (duo ×1,6, trio ×2)."), 1, 1, 1, 1)
        GameTooltip:Show()
    end)
    together:SetScript("OnLeave", function() GameTooltip:Hide() end)

    local play = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    ns.Size(play, 80, 22)
    play:SetPoint("CENTER", f, "TOPLEFT", 224, -422)
    play:SetText(ns.T("Jouer"))
    play:SetScript("OnClick", function() UI.PlaySelected(false) end)

    local quit = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    ns.Size(quit, 80, 22)
    quit:SetPoint("CENTER", f, "TOPLEFT", 305, -422)
    quit:SetText(ns.T("Quitter"))
    quit:SetScript("OnClick", function() f:Hide() end)

    -- Le bouton de langue : français / anglais (l'interface se recharge).
    local lang = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    ns.Size(lang, 40, 18)
    lang:SetPoint("RIGHT", workshop, "LEFT", -7, 0)
    lang:SetText(ns.IsEnglish() and "US | FR" or "FR | US")
    lang:SetScript("OnClick", function() ns.ToggleLang() end)
    lang:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_TOP")
        GameTooltip:SetText(ns.T(ns.IsEnglish() and "Langue : anglais" or "Langue : français"))
        GameTooltip:AddLine(ns.T("Changer la langue de l'addon (français / anglais). L'interface se recharge."), 1, 1, 1, 1)
        GameTooltip:Show()
    end)
    lang:SetScript("OnLeave", function() GameTooltip:Hide() end)

    -- L'aide du métier, de A à Z.
    local help = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    ns.Size(help, 46, 18)
    help:SetPoint("TOPLEFT", f, "TOPLEFT", 300, -447)
    help:SetText(ns.T("Aide"))
    help:SetScript("OnClick", function() ns.Help.Toggle() end)
    f.help = help

    -- La voie (spécialisation) : un menu à trois choix, gratuit la première fois.
    local spec = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    ns.Size(spec, 84, 18)
    spec:SetPoint("TOPLEFT", f, "TOPLEFT", 20, -447)
    local specMenu = CreateFrame("Frame", "BardeSpecMenu", f, "UIDropDownMenuTemplate")
    UIDropDownMenu_Initialize(specMenu, function()
        local current = ns.spec and ns.spec.current or 0
        local minSkill = ns.spec and ns.spec.minSkill or 180
        local cost = ns.spec and ns.spec.cost or 100000
        for index, entry in ipairs(ns.SPECS) do
            local info = UIDropDownMenu_CreateInfo()
            local locked = (ns.info.skill or 0) < minSkill
            info.text = locked and ("|cff808080" .. ns.T(entry.name) .. "|r") or ns.T(entry.name)
            info.value = index
            info.checked = (current == index)
            info.tooltipTitle = ns.T(entry.name)
            info.tooltipText = ns.T(entry.text) .. "\n\n" .. (locked
                and ("|cffff4040" .. ns.T("Spécialité accessible à partir de la compétence de barde ") .. minSkill .. ns.T(" (vous : ") .. (ns.info.skill or 0) .. ").|r")
                or (current ~= 0 and ns.T("Changer coûte ") .. math.floor(cost / 10000) .. ns.T(" po.") or ns.T("Le premier choix est gratuit.")))
            info.tooltipOnButton = true
            info.func = function(self)
                if locked then
                    ns.Print(ns.T("Spécialité accessible à partir de la compétence de barde ") .. minSkill .. ".")
                elseif self.value ~= current then
                    ns.Send("SPECSET", self.value)
                end
            end
            UIDropDownMenu_AddButton(info)
        end
    end, "MENU")
    spec:SetScript("OnClick", function(self) ToggleDropDownMenu(1, nil, specMenu, self, 0, 0) end)
    f.spec = spec
    UI.RefreshSpec()

    -- Couper le son des autres bardes (ceux d'un ensemble comme ceux qu'on croise) ; ce qu'on joue soi-même reste audible.
    local mute = CreateFrame("CheckButton", "BardeMuteOthers", f, "UICheckButtonTemplate")
    ns.Size(mute, 24, 24)
    mute:SetPoint("TOPLEFT", f, "TOPLEFT", 108, -444)
    mute.text = mute:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    mute.text:SetPoint("LEFT", mute, "RIGHT", 0, 1)
    mute.text:SetText(ns.T("Couper le son des autres bardes"))
    mute:SetChecked(ns.Muted())
    mute:SetScript("OnClick", function(self)
        BardeDB = BardeDB or {}
        BardeDB.mute = self:GetChecked() and true or false
    end)
    mute:SetScript("OnShow", function(self) self:SetChecked(ns.Muted()) end)
    f.mute = mute

    UI.skin = ApplyElvUI(f, {
        art = { topLeft, topRight, botLeft, botRight, barLeft, barRight }, portrait = portrait, close = close,
        rank = rank, rankBorder = rankBorder, rankBg = rankBg, tab = { tabLeft, tabMiddle, tabRight },
        search = search, dropDown = dropDown, training = training, together = together, workshop = workshop, duel = duel, improv = improv, lang = lang, play = play, quit = quit,
        collapseIcon = collapseIcon, header = { headerLeft, headerRight },
        iconFrame = iconFrame, icon = d.icon, iconBorder = iconBorder,
        buffFrame = buffFrame, buffIcon = d.buffIcon, buffBorder = buffBorder, box = box, boxBg = boxBg,
    })

    f:SetScript("OnShow", function() PlaySound("igCharacterInfoOpen") end)
    f:SetScript("OnHide", function() PlaySound("igCharacterInfoClose") end)
    return f
end

-- ------------------------------------------------------------- actions

function UI.PlaySelected(training)
    if not selected then return end
    ns.Play(selected, training)
end

function UI.Show()
    frame = frame or CreateFrameUI()
    frame:Show()
    ns.RequestSongs()
    UI.Refresh()
end

function UI.Hide()
    if frame then frame:Hide() end
end

function UI.Toggle()
    if frame and frame:IsShown() then
        UI.Hide()
    else
        UI.Show()
    end
end

-- Les sacs changent (instrument reçu, fabriqué, jeté) : un instant plus tard, le carnet (s'il est ouvert) redemande au serveur
-- ce qu'on possède, pour que le menu des instruments et les couleurs soient justes.
local bagEvents = CreateFrame("Frame")
local refreshAt
bagEvents:RegisterEvent("BAG_UPDATE")
bagEvents:SetScript("OnEvent", function() refreshAt = GetTime() + 0.8 end)
bagEvents:SetScript("OnUpdate", function()
    if refreshAt and GetTime() >= refreshAt then
        refreshAt = nil
        if frame and frame:IsShown() then ns.RequestSongs() end
    end
end)

StaticPopupDialogs["BARDE_DELETE"] = {
    text = "Supprimer cette composition ?",
    button1 = ACCEPT,
    button2 = CANCEL,
    timeout = 0,
    whileDead = 0,
    hideOnEscape = 1,
    OnAccept = function(self, data) ns.Send("DELCOMP", data) end,
}
