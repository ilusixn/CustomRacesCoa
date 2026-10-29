-- Barde : l'improvisation, jouer librement sans partition (jeu de rôle : ni score, ni chant).
-- Licence AGPL-3.0, voir LICENSE.
--
-- Huit touches (A S D F G H J K) jouent la gamme du mode choisi, à partir de la note de départ choisie, avec l'instrument en main.
-- Les notes sont jouées ici, et envoyées au serveur (HITS) qui les fait entendre aux joueurs proches. Échap ferme la fenêtre.

local ADDON, ns = ...

local Improv = {}
ns.Improv = Improv

local SOUND_PATH = "Interface\\AddOns\\Barde\\Sounds\\%s_%d.wav"
local LOWEST, HIGHEST = 55, 84
local KEYS = { "A", "S", "D", "F", "G", "H", "J", "K" }

-- Les modes : les huit degrés de la gamme, en demi-tons à partir de la note de départ.
local MODES = {
    { name = "Majeur (ionien)", steps = { 0, 2, 4, 5, 7, 9, 11, 12 } },
    { name = "Mineur (éolien)", steps = { 0, 2, 3, 5, 7, 8, 10, 12 } },
    { name = "Dorien", steps = { 0, 2, 3, 5, 7, 9, 10, 12 } },
    { name = "Phrygien", steps = { 0, 1, 3, 5, 7, 8, 10, 12 } },
    { name = "Mixolydien", steps = { 0, 2, 4, 5, 7, 9, 10, 12 } },
    { name = "Pentatonique mineure", steps = { 0, 3, 5, 7, 10, 12, 15, 17 } },
}
local ROOTS = { { name = "Do", midi = 60 }, { name = "Ré", midi = 62 }, { name = "Mi", midi = 64 }, { name = "Fa", midi = 65 },
    { name = "Sol", midi = 67 }, { name = "La", midi = 69 } }

local frame
local mode, root = 1, 1
local active = false            -- le serveur a accepté : l'instrument est en main
local recording = false         -- on enregistre les notes pour en faire une composition
local recorded = {}             -- { temps, note }
local outbox = {}               -- les morceaux du message d'une composition, envoyés un à un
local nextSend = 0
local queue, nextFlush = {}, 0

local function Midi(index)
    local midi = ROOTS[root].midi + MODES[mode].steps[index]
    while midi < LOWEST do midi = midi + 12 end
    while midi > HIGHEST do midi = midi - 12 end
    return midi
end

-- Joue une note (numéro MIDI) : le son, l'envoi aux voisins, l'enregistrement, et l'éclat de la touche.
local function PlayMidi(midi, key)
    local ins = ns.CurrentInstrument()
    if not ins or not active then return end
    PlaySoundFile(string.format(SOUND_PATH, pianoVoice and "piano" or ins.sound or "luth", midi))
    if #queue < 12 then table.insert(queue, midi) end
    if recording and #recorded < 400 then table.insert(recorded, { GetTime(), midi }) end
    if key then
        key.lit = GetTime() + 0.15
        if key.flash then key.flash:Show() else key:SetBackdropColor(0.85, 0.64, 0.26, 0.9) end
    end
end

local function Press(index)
    PlayMidi(Midi(index), frame.keys[index])
end

-- Le mode expert : un clavier de piano complet, de Sol 3 (55) à Do 6 (84), les notes dont l'addon a le son.
-- Touches blanches au clavier : Z X C V B N M, A S D F G H J K L, Q W (dans l'ordre, de la plus grave à la plus aiguë) ;
-- avec Maj, la touche noire qui suit (dièse). Tout se joue aussi à la souris.
local WHITE_KEYS = { "Z", "X", "C", "V", "B", "N", "M", "A", "S", "D", "F", "G", "H", "J", "K", "L", "Q", "W" }
local expert = false
local pianoVoice = false            -- le son du piano à la place de celui de l'instrument
local pianoByLetter = {}            -- touche du clavier -> { blanche, noire (ou nil) }

local function Create()
    local f = CreateFrame("Frame", "BardeImprovFrame", UIParent)
    ns.Size(f, 520, 296)
    f:SetPoint("CENTER", UIParent, "CENTER", 0, -120)
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
    tinsert(UISpecialFrames, "BardeImprovFrame")

    local header = f:CreateTexture(nil, "ARTWORK")
    ns.Size(header, 300, 64)
    header:SetPoint("TOP", f, "TOP", 0, 12)
    header:SetTexture("Interface\\DialogFrame\\UI-DialogBox-Header")
    local title = f:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    title:SetPoint("TOP", header, "TOP", 0, -14)
    title:SetText(ns.T("Improvisation"))
    local close = CreateFrame("Button", nil, f, "UIPanelCloseButton")
    close:SetPoint("TOPRIGHT", -8, -8)

    -- Le mode et la note de départ.
    local modeMenu = CreateFrame("Frame", "BardeImprovMode", f, "UIDropDownMenuTemplate")
    modeMenu:SetPoint("TOPLEFT", 10, -40)
    UIDropDownMenu_Initialize(modeMenu, function()
        for index, entry in ipairs(MODES) do
            local info = UIDropDownMenu_CreateInfo()
            info.text = ns.T(entry.name)
            info.value = index
            info.checked = (mode == index)
            info.func = function(self)
                mode = self.value
                UIDropDownMenu_SetText(modeMenu, ns.T(MODES[mode].name))
                Improv.Refresh()
            end
            UIDropDownMenu_AddButton(info)
        end
    end)
    UIDropDownMenu_SetWidth(modeMenu, 150)
    UIDropDownMenu_SetText(modeMenu, ns.T(MODES[mode].name))

    local rootMenu = CreateFrame("Frame", "BardeImprovRoot", f, "UIDropDownMenuTemplate")
    rootMenu:SetPoint("LEFT", modeMenu, "RIGHT", -10, 0)
    UIDropDownMenu_Initialize(rootMenu, function()
        for index, entry in ipairs(ROOTS) do
            local info = UIDropDownMenu_CreateInfo()
            info.text = ns.T("Départ : ") .. ns.T(entry.name)
            info.value = index
            info.checked = (root == index)
            info.func = function(self)
                root = self.value
                UIDropDownMenu_SetText(rootMenu, ns.T("Départ : ") .. ns.T(ROOTS[root].name))
                Improv.Refresh()
            end
            UIDropDownMenu_AddButton(info)
        end
    end)
    UIDropDownMenu_SetWidth(rootMenu, 100)
    UIDropDownMenu_SetText(rootMenu, ns.T("Départ : ") .. ns.T(ROOTS[root].name))

    f.info = f:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    f.info:SetPoint("TOPLEFT", 26, -78)

    -- Les huit touches.
    f.keys = {}
    for index = 1, 8 do
        local key = CreateFrame("Frame", nil, f)
        ns.Size(key, 52, 90)
        key:SetPoint("TOPLEFT", 28 + (index - 1) * 58, -104)
        key:SetBackdrop({ bgFile = "Interface\\ChatFrame\\ChatFrameBackground", edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
            tile = true, tileSize = 16, edgeSize = 12, insets = { left = 3, right = 3, top = 3, bottom = 3 } })
        key:SetBackdropColor(0.12, 0.14, 0.21, 0.95)
        key:SetBackdropBorderColor(0.85, 0.64, 0.26, 0.8)
        key.letter = key:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
        key.letter:SetPoint("BOTTOM", 0, 10)
        key.letter:SetText(KEYS[index])
        key.note = key:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
        key.note:SetPoint("TOP", 0, -10)
        key:EnableMouse(true)
        key:SetScript("OnMouseDown", function() Press(index) end)
        f.keys[index] = key
    end

    -- Le clavier de piano du mode expert (caché tant qu'on ne l'a pas demandé).
    f.piano = CreateFrame("Frame", nil, f)
    ns.Size(f.piano, 480, 96)
    f.piano:SetPoint("TOPLEFT", 20, -100)
    f.piano:Hide()
    local whiteWidth, whiteHeight, blackWidth, blackHeight = 26, 92, 16, 56
    local whiteIndexOf = {}
    local function Light(key)
        key.flash = key:CreateTexture(nil, "OVERLAY")
        key.flash:SetAllPoints()
        key.flash:SetTexture(0.85, 0.64, 0.26, 0.55)
        key.flash:Hide()
        key:EnableMouse(true)
    end
    local function Backdrop(key, r, g, b)
        key:SetBackdrop({ bgFile = "Interface\\ChatFrame\\ChatFrameBackground", edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
            tile = true, tileSize = 16, edgeSize = 8, insets = { left = 2, right = 2, top = 2, bottom = 2 } })
        key:SetBackdropColor(r, g, b, 1)
        key:SetBackdropBorderColor(0.3, 0.3, 0.35, 1)
    end
    local function IsBlack(midi)
        local pitchClass = midi % 12
        return pitchClass == 1 or pitchClass == 3 or pitchClass == 6 or pitchClass == 8 or pitchClass == 10
    end
    f.pianoKeys = {}
    local white = 0
    for midi = 55, 84 do
        if not IsBlack(midi) then
            white = white + 1
            local key = CreateFrame("Frame", nil, f.piano)
            ns.Size(key, whiteWidth - 1, whiteHeight)
            key:SetPoint("TOPLEFT", (white - 1) * whiteWidth, 0)
            Backdrop(key, 0.92, 0.9, 0.84)
            Light(key)
            key.letter = key:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
            key.letter:SetPoint("BOTTOM", 0, 6)
            key.letter:SetTextColor(0.2, 0.2, 0.28)
            key.letter:SetText(WHITE_KEYS[white] or "")
            key.midi = midi
            key:SetScript("OnMouseDown", function() PlayMidi(midi, key) end)
            whiteIndexOf[midi] = white
            pianoByLetter[WHITE_KEYS[white] or ""] = { key }
            table.insert(f.pianoKeys, key)
        end
    end
    -- Les touches noires, au-dessus et entre deux touches blanches.
    for midi = 55, 84 do
        if IsBlack(midi) then
            local left = whiteIndexOf[midi - 1]
            local key = CreateFrame("Frame", nil, f.piano)
            key:SetFrameLevel(f.piano:GetFrameLevel() + 3)
            ns.Size(key, blackWidth, blackHeight)
            key:SetPoint("TOPLEFT", left * whiteWidth - blackWidth / 2 - 1, 0)
            Backdrop(key, 0.08, 0.08, 0.12)
            Light(key)
            key.midi = midi
            key:SetScript("OnMouseDown", function() PlayMidi(midi, key) end)
            local owner = pianoByLetter[WHITE_KEYS[left] or ""]
            if owner then owner[2] = key end
            table.insert(f.pianoKeys, key)
        end
    end

    f.expertButton = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    ns.Size(f.expertButton, 84, 20)
    f.expertButton:SetPoint("TOPRIGHT", -44, -76)
    f.expertButton:SetText(ns.T("Mode expert"))
    f.expertButton:SetScript("OnClick", function() Improv.ToggleExpert() end)

    f.hint = f:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    f.hint:SetPoint("BOTTOM", 0, 20)
    f.hint:SetText(ns.T("Jouez avec les touches A S D F G H J K (ou la souris). Échap pour arrêter. Les joueurs proches vous entendent."))

    -- L'enregistrement : jouer, arrêter, puis donner un titre (le serveur place les notes sur sa grille et fixe le palier).
    f.status = f:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    f.status:SetPoint("TOPLEFT", 28, -204)
    f.status:SetJustifyH("LEFT")
    f.record = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    ns.Size(f.record, 110, 22)
    f.record:SetPoint("TOPLEFT", 28, -226)
    f.record:SetText(ns.T("Enregistrer"))
    f.record:SetScript("OnClick", function() Improv.ToggleRecording() end)
    f.compose = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    ns.Size(f.compose, 110, 22)
    f.compose:SetPoint("LEFT", f.record, "RIGHT", 8, 0)
    f.compose:SetText(ns.T("Composer"))
    f.compose:SetScript("OnClick", function() Improv.AskTitle() end)
    f.clear = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
    ns.Size(f.clear, 90, 22)
    f.clear:SetPoint("LEFT", f.compose, "RIGHT", 8, 0)
    f.clear:SetText(ns.T("Effacer"))
    f.clear:SetScript("OnClick", function() wipe(recorded); recording = false; Improv.Refresh() end)

    -- L'instrument : celui qu'on veut parmi ceux qu'on possède (son, et instrument vu par ceux qui écoutent).
    local insMenu = CreateFrame("Frame", "BardeImprovInstrument", f, "UIDropDownMenuTemplate")
    insMenu:SetPoint("LEFT", f.clear, "RIGHT", -6, -2)
    UIDropDownMenu_Initialize(insMenu, function()
        for _, ins in ipairs(ns.instruments) do
            if ins.has then
                local info = UIDropDownMenu_CreateInfo()
                info.text = ns.InstrumentName(ins)
                info.value = ins.id
                info.checked = (ns.instrumentId == ins.id)
                info.func = function(self)
                    ns.instrumentId = self.value
                    UIDropDownMenu_SetText(insMenu, ns.InstrumentName(ns.CurrentInstrument()))
                    active = false
                    Improv.Refresh()
                    ns.Send("IMPROV", self.value, pianoVoice and "piano" or "")
                end
                UIDropDownMenu_AddButton(info)
            end
        end
    end)
    UIDropDownMenu_SetWidth(insMenu, 110)
    f.insMenu = insMenu

    f:EnableKeyboard(true)
    f:SetScript("OnKeyDown", function(_, key)
        if expert then
            local entry = pianoByLetter[key]
            if entry then
                local pressed = (IsShiftKeyDown() and entry[2]) or entry[1]      -- avec Maj : le dièse (la touche noire qui suit)
                PlayMidi(pressed.midi, pressed)
            end
            return
        end
        for index, letter in ipairs(KEYS) do
            if key == letter then Press(index) return end
        end
    end)
    f:SetScript("OnUpdate", function()
        local now = GetTime()
        for _, key in ipairs(f.keys) do
            if key.lit and now >= key.lit then
                key:SetBackdropColor(0.12, 0.14, 0.21, 0.95)
                key.lit = nil
            end
        end
        for _, key in ipairs(f.pianoKeys) do
            if key.lit and now >= key.lit then
                key.flash:Hide()
                key.lit = nil
            end
        end
        if #outbox > 0 and now >= nextSend then
            local message = table.remove(outbox, 1)
            ns.Send(unpack(message))
            nextSend = now + 0.15
        end
        if #queue > 0 and now >= nextFlush then
            ns.Send("HITS", table.concat(queue, ","))
            wipe(queue)
            nextFlush = now + 0.1
        end
    end)
    f:SetScript("OnHide", function()
        if active then ns.Send("IMPROVEND") end
        active = false
        recording = false
        wipe(queue)
        wipe(recorded)
    end)
    return f
end

local NOTE_NAMES = ns.Lazy({ [0] = "Do", "Do#", "Ré", "Ré#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" })

function Improv.Refresh()
    if not frame then return end
    local ins = ns.CurrentInstrument()
    frame.info:SetText(ns.T("Instrument : |cffffffff") .. (ins and ns.InstrumentName(ins) or "?") .. ns.T("|r  ·  ") .. (active and ns.T("|cff9fef9fprêt|r") or ns.T("en attente du serveur...")))
    for index = 1, 8 do
        frame.keys[index].note:SetText(NOTE_NAMES[Midi(index) % 12])
    end
    local seconds = 0
    if #recorded > 1 then seconds = math.floor(recorded[#recorded][1] - recorded[1][1] + 0.5) end
    local text = (recording and ns.T("Enregistrement : ") or ns.T("Prêt à composer : ")) .. #recorded .. ns.T(" notes, ") .. seconds .. ns.T(" s (au moins ")
        .. "16" .. ns.T(" notes et ") .. "20" .. ns.T(" s)")
    frame.status:SetText(#recorded > 0 and text or "")
    frame.record:SetText(recording and ns.T("Arrêter") or ns.T("Enregistrer"))
    if #recorded > 0 and not recording then frame.compose:Enable() else frame.compose:Disable() end
    if #recorded > 0 then frame.clear:Enable() else frame.clear:Disable() end
end

function Improv.Show()
    frame = frame or Create()
    ns.UI.Hide()
    frame:Show()
    local ins = ns.CurrentInstrument()
    active = false
    if ins then UIDropDownMenu_SetText(frame.insMenu, ns.InstrumentName(ins)) end
    Improv.Refresh()
    ns.Send("IMPROV", ins and ins.id or 1, pianoVoice and "piano" or "")
end

-- Le mode expert : le clavier de piano complet à la place des huit touches (les deux menus du mode et de la note de départ ne servent plus).
function Improv.ToggleExpert()
    expert = not expert
    if frame then
        ns.Shown(frame.piano, expert)
        for _, key in ipairs(frame.keys) do ns.Shown(key, not expert) end
        ns.Shown(BardeImprovMode, not expert)
        ns.Shown(BardeImprovRoot, not expert)
        frame.expertButton:SetText(expert and ns.T("Mode simple") or ns.T("Mode expert"))
        frame.hint:SetText(expert
            and ns.T("Clavier complet (Sol 3 à Do 6) : Z X C V B N M, A S D F G H J K L, Q W ; Maj pour les dièses. Ou la souris.")
            or ns.T("Jouez avec les touches A S D F G H J K (ou la souris). Échap pour arrêter. Les joueurs proches vous entendent."))
    end
    Improv.Refresh()
end

function Improv.ToggleRecording()
    if not active then
        ns.Print(ns.T("Aucun instrument en main : attendez que le serveur accepte."))
        return
    end
    if not recording then wipe(recorded) end
    recording = not recording
    Improv.Refresh()
end

-- Envoie l'enregistrement au serveur : le titre, les notes (temps depuis la première, en millisecondes) par morceaux, puis la fin.
function Improv.Send(title)
    if #recorded == 0 then
        ns.Print(ns.T("Aucune note enregistrée : jouez d'abord."))
        return
    end
    title = string.gsub(title or "", "[%^|]", "")
    wipe(outbox)
    table.insert(outbox, { "COMPBEGIN", title })
    local origin = recorded[1][1]
    local piece = {}
    for _, note in ipairs(recorded) do
        table.insert(piece, math.floor((note[1] - origin) * 1000 + 0.5) .. ":" .. note[2])
        if #piece == 18 then
            table.insert(outbox, { "COMPN", table.concat(piece, ";") })
            piece = {}
        end
    end
    if #piece > 0 then table.insert(outbox, { "COMPN", table.concat(piece, ";") }) end
    table.insert(outbox, { "COMPEND" })
    wipe(recorded)
    recording = false
    Improv.Refresh()
end

StaticPopupDialogs["BARDE_COMPOSE"] = {
    text = "Titre de votre composition :",
    button1 = ACCEPT,
    button2 = CANCEL,
    hasEditBox = 1,
    maxLetters = 40,
    timeout = 0,
    whileDead = 0,
    hideOnEscape = 1,
    OnShow = function(self) self.editBox:SetText(""); self.editBox:SetFocus() end,
    OnAccept = function(self) Improv.Send(self.editBox:GetText()) end,
    EditBoxOnEnterPressed = function(self)
        local parent = self:GetParent()
        Improv.Send(parent.editBox:GetText())
        parent:Hide()
    end,
    EditBoxOnEscapePressed = function(self) self:GetParent():Hide() end,
}

function Improv.AskTitle()
    StaticPopupDialogs["BARDE_COMPOSE"].text = ns.T("Titre de votre composition :")
    StaticPopup_Show("BARDE_COMPOSE")
end

function Improv.Accepted()
    active = true
    Improv.Refresh()
end
