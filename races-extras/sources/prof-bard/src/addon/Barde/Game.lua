-- Barde : le mini-jeu, avec le visuel du prototype « Carnet du Barde ».
-- Licence AGPL-3.0, voir LICENSE.
--
-- Les notes tombent dans 4 colonnes (D F J K, graves à gauche). Chaque note est jugée ici, sur
-- le PC du joueur ; le serveur ne reçoit que les totaux et les vérifie avant d'accorder quoi que ce soit.
--
--   Parfait   écart <= 60 ms     100 points
--   Bon       écart <= 120 ms     50 points
--   Raté      au-delà, ou note non jouée
--   Tenue     jugée deux fois : à la frappe, puis au relâchement (+50 points, si tenue jusqu'au bout)
--   Combo     +10 % de points par tranche de 25 notes d'affilée

local ADDON, ns = ...

local Game = {}
ns.Game = Game

local TEX = ns.TEXTURES
local DISPLAY = "Fonts\\MORPHEUS.TTF"
local BODY = "Fonts\\FRIZQT__.TTF"

local PERFECT, GOOD = 0.060, 0.120
local RELEASE_SLACK = 0.15      -- une note tenue peut être lâchée un peu avant sa fin
local HOLD_FRACTION = 0.8       -- une note tenue se garde 80 % de sa durée
local LEAD_IN = 3.0             -- décompte avant la première note
local COMBO_STEP = 25
local TRAVEL = { [0] = 1.8, 1.7, 1.6, 1.55, 1.5 }   -- temps de chute d'une note, par palier

-- Disposition (en pixels, origine en bas à gauche).
local Z = 1.0                   -- grossissement des éléments du jeu (notes, médaillons) pour remplir le cadre
local W, H = 424, 640
local HUD = 64
local LANE_W = 100
local WOOD = 96                -- hauteur du parquet sous la ligne de frappe
local FIELD_H = H - HUD
local AREA = FIELD_H - WOOD     -- hauteur de chute, de la ligne de frappe au haut des colonnes
local R = 30 * Z                   -- rayon d'un médaillon
local NOTE_R = 26 * Z              -- rayon d'une tête de note
local FIELD_X = (W - 4 * LANE_W) / 2

local KEYS = { D = 1, F = 2, J = 3, K = 4 }
local KEY_NAMES = { "D", "F", "J", "K" }
local LANE_COLORS = {
    { 0.851, 0.643, 0.255 },
    { 0.788, 0.478, 0.353 },
    { 0.498, 0.690, 0.839 },
    { 0.663, 0.561, 0.839 },
}
local COLORS = {
    perfect = { 0.624, 0.941, 0.753 },
    good = { 0.663, 0.776, 1.000 },
    miss = { 1.000, 0.561, 0.596 },
}
local INK, MUTED, BRASS = { 0.925, 0.902, 0.839 }, { 0.604, 0.624, 0.722 }, { 0.851, 0.643, 0.255 }

local LOWEST_SOUND, HIGHEST_SOUND = 55, 84
local SOUND_PATH = "Interface\\AddOns\\Barde\\Sounds\\%s_%d.wav"

local state             -- la partie en cours, nil sinon
local frame             -- la fenêtre de jeu, créée au premier morceau
local pool = { head = {}, stem = {}, bar = {} }

-- ------------------------------------------------------------- fenêtre

local function Font(fs, path, size, color, flags)
    fs:SetFont(path, size, flags)
    if color then fs:SetTextColor(color[1], color[2], color[3]) end
end

local function MakeBox(parent, bg, border)
    local box = CreateFrame("Frame", nil, parent)
    box:SetBackdrop({
        bgFile = "Interface\\ChatFrame\\ChatFrameBackground",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true, tileSize = 16, edgeSize = 12,
        insets = { left = 3, right = 3, top = 3, bottom = 3 },
    })
    box:SetBackdropColor(bg[1], bg[2], bg[3], bg[4] or 1)
    box:SetBackdropBorderColor(border[1], border[2], border[3], border[4] or 1)
    return box
end

local function CreateResultCard(stage)
    local card = MakeBox(stage, { 0.12, 0.14, 0.21, 0.98 }, { 0.30, 0.33, 0.50, 1 })
    ns.Size(card, 380, 410)
    card:SetPoint("CENTER", stage, "CENTER", 0, 10)
    card:SetFrameLevel(stage:GetFrameLevel() + 20)
    card:EnableMouse(true)
    card:Hide()

    card.title = card:CreateFontString(nil, "OVERLAY")
    Font(card.title, DISPLAY, 30, BRASS)
    card.title:SetPoint("TOP", 0, -20)

    card.info = card:CreateFontString(nil, "OVERLAY")
    Font(card.info, BODY, 12, INK)
    card.info:SetPoint("TOP", 0, -62)
    card.info:SetWidth(340)

    card.boxes = {}
    local labels = { { ns.T("parfait"), COLORS.perfect }, { ns.T("bon"), COLORS.good }, { ns.T("raté"), COLORS.miss } }
    for i, entry in ipairs(labels) do
        local box = MakeBox(card, { 0.09, 0.10, 0.17, 1 }, { 0.20, 0.22, 0.35, 1 })
        ns.Size(box, 106, 54)
        box:SetPoint("TOPLEFT", 14 + (i - 1) * 113, -92)
        box.num = box:CreateFontString(nil, "OVERLAY")
        Font(box.num, BODY, 22, entry[2], "OUTLINE")
        box.num:SetPoint("TOP", 0, -8)
        box.lab = box:CreateFontString(nil, "OVERLAY")
        Font(box.lab, BODY, 11, entry[2])
        box.lab:SetPoint("BOTTOM", 0, 7)
        box.lab:SetText(entry[1])
        card.boxes[i] = box
    end

    local rule = card:CreateTexture(nil, "ARTWORK")
    rule:SetTexture(0.30, 0.33, 0.50, 1)
    rule:SetPoint("TOPLEFT", 16, -162)
    ns.Size(rule, 348, 1)

    card.head = card:CreateFontString(nil, "OVERLAY")
    Font(card.head, BODY, 12, INK)
    card.head:SetPoint("TOP", 0, -176)
    card.head:SetWidth(340)

    card.value = card:CreateFontString(nil, "OVERLAY")
    Font(card.value, BODY, 20, INK, "OUTLINE")
    card.value:SetPoint("TOP", 0, -198)
    card.value:SetWidth(350)

    card.calc = card:CreateFontString(nil, "OVERLAY")
    Font(card.calc, BODY, 11, MUTED)
    card.calc:SetPoint("TOP", 0, -232)
    card.calc:SetWidth(340)
    card.calc:SetSpacing(2)

    card.duration = card:CreateFontString(nil, "OVERLAY")
    Font(card.duration, BODY, 12, INK)
    card.duration:SetPoint("TOP", 0, -292)
    card.duration:SetWidth(340)

    card.record = card:CreateFontString(nil, "OVERLAY")
    Font(card.record, BODY, 12, BRASS)
    card.record:SetPoint("TOP", 0, -318)
    card.record:SetWidth(340)

    card.again = CreateFrame("Button", nil, card, "UIPanelButtonTemplate")
    ns.Size(card.again, 150, 26)
    card.again:SetPoint("BOTTOMLEFT", 20, 18)
    card.again:SetText(ns.T("Rejouer"))
    card.again:SetScript("OnClick", function()
        local song, training = state and state.song, state and state.training
        Game.Close()
        if song then ns.Play(song.id, training) end
    end)

    card.book = CreateFrame("Button", nil, card, "UIPanelButtonTemplate")
    ns.Size(card.book, 150, 26)
    card.book:SetPoint("BOTTOMRIGHT", -20, 18)
    card.book:SetText(ns.T("Carnet"))
    card.book:SetScript("OnClick", function()
        Game.Close()
        ns.UI.Show()
    end)
    return card
end

-- Habillage de la scène : le bord et la barre de titre des fenêtres du jeu (boîte de dialogue), comme le carnet.
local function CreateBorder(stage)
    local border = CreateFrame("Frame", nil, stage)
    border:SetPoint("TOPLEFT", stage, "TOPLEFT", -7, 7)
    border:SetPoint("BOTTOMRIGHT", stage, "BOTTOMRIGHT", 7, -7)
    border:SetFrameLevel(stage:GetFrameLevel() + 30)
    border:SetBackdrop({
        edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border", edgeSize = 32,
        insets = { left = 11, right = 12, top = 12, bottom = 11 },
    })

    local header = border:CreateTexture(nil, "OVERLAY")
    ns.Size(header, 256, 64)
    header:SetPoint("TOP", border, "TOP", 0, 12)
    header:SetTexture("Interface\\DialogFrame\\UI-DialogBox-Header")
    local title = border:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    title:SetPoint("TOP", header, "TOP", 0, -14)
    title:SetText(ns.T("Barde"))

    -- La scène tient dans l'écran : réduite si besoin, centrée à chaque ouverture.
    stage:HookScript("OnShow", function()
        stage:SetScale(math.min(1, UIParent:GetHeight() * 0.9 / (H + 40)))
        stage:ClearAllPoints()
        stage:SetPoint("CENTER", UIParent, "CENTER", 0, -10)
    end)
    return border
end

local function CreateStage()
    local f = CreateFrame("Frame", "BardeStage", UIParent)
    ns.Size(f, W, H)
    f:SetPoint("CENTER", 0, 20)
    f:SetFrameStrata("HIGH")
    f:SetMovable(true)
    f:EnableMouse(true)
    f:RegisterForDrag("LeftButton")
    f:SetScript("OnDragStart", f.StartMoving)
    f:SetScript("OnDragStop", f.StopMovingOrSizing)
    f:SetClampedToScreen(true)
    f:Hide()

    local bg = f:CreateTexture(nil, "BACKGROUND")
    bg:SetPoint("TOPLEFT", 3, -3)
    bg:SetPoint("BOTTOMRIGHT", -3, 3)
    bg:SetTexture(TEX .. "fond.tga")

    -- Parquet sous la ligne de frappe, puis la ligne elle-même.
    local wood = f:CreateTexture(nil, "ARTWORK")
    wood:SetPoint("BOTTOMLEFT", 3, 3)
    wood:SetPoint("BOTTOMRIGHT", -3, 3)
    wood:SetHeight(WOOD - 3)
    wood:SetTexture(TEX .. "bois.tga")

    local lineGlow = f:CreateTexture(nil, "ARTWORK")
    lineGlow:SetPoint("BOTTOMLEFT", 3, WOOD - 9)
    lineGlow:SetPoint("BOTTOMRIGHT", -3, WOOD - 9)
    lineGlow:SetHeight(18)
    lineGlow:SetTexture(TEX .. "ligne.tga")
    lineGlow:SetVertexColor(0.88, 0.69, 0.31)
    lineGlow:SetBlendMode("ADD")

    local line = f:CreateTexture(nil, "ARTWORK")
    line:SetPoint("BOTTOMLEFT", 3, WOOD - 1)
    line:SetPoint("BOTTOMRIGHT", -3, WOOD - 1)
    line:SetHeight(3)
    line:SetTexture(0.91, 0.72, 0.35, 1)

    -- Terrain : les quatre colonnes.
    local field = CreateFrame("Frame", nil, f)
    field:SetPoint("BOTTOMLEFT", FIELD_X, 0)
    ns.Size(field, 4 * LANE_W, FIELD_H)
    f.field = field

    f.watermark = field:CreateFontString(nil, "ARTWORK")
    Font(f.watermark, DISPLAY, 140, { 0.953, 0.812, 0.490 })
    f.watermark:SetPoint("CENTER", field, "BOTTOMLEFT", 2 * LANE_W, WOOD + AREA * 0.5)
    f.watermark:SetAlpha(0.09)

    f.lanes = {}
    for lane = 1, 4 do
        local c = LANE_COLORS[lane]
        local entry = {}

        local glow = field:CreateTexture(nil, "BORDER")
        glow:SetPoint("BOTTOMLEFT", (lane - 1) * LANE_W, WOOD)
        ns.Size(glow, LANE_W, AREA)
        glow:SetTexture(TEX .. "voie.tga")
        glow:SetVertexColor(c[1], c[2], c[3])
        glow:SetAlpha(0.07)
        entry.glow = glow

        if lane > 1 then
            local sep = field:CreateTexture(nil, "BORDER")
            sep:SetPoint("BOTTOMLEFT", (lane - 1) * LANE_W, 3)
            ns.Size(sep, 1, FIELD_H - 3)
            sep:SetTexture(0.851, 0.643, 0.255, 0.2)
        end

        -- Médaillon : le conteneur reste fixe, seule sa partie intérieure grossit à la frappe.
        local holder = CreateFrame("Frame", nil, field)
        holder:SetFrameLevel(field:GetFrameLevel() + 2)
        ns.Size(holder, 72 * Z, 72 * Z)
        holder:SetPoint("CENTER", field, "BOTTOMLEFT", (lane - 0.5) * LANE_W, WOOD)
        local medal = CreateFrame("Frame", nil, holder)
        ns.Size(medal, 72 * Z, 72 * Z)
        medal:SetPoint("CENTER", holder, "CENTER", 0, 0)    -- un seul point d'ancrage : la mise à l'échelle part du centre
        entry.medal = medal

        local back = medal:CreateTexture(nil, "BACKGROUND")
        back:SetAllPoints()
        back:SetTexture(TEX .. "medaillon_fond.tga")

        local lamp = medal:CreateTexture(nil, "BORDER")
        lamp:SetPoint("CENTER")
        ns.Size(lamp, 54 * Z, 54 * Z)
        lamp:SetTexture(TEX .. "lampe.tga")
        lamp:SetVertexColor(c[1], c[2], c[3])
        entry.lamp = lamp

        local ring = medal:CreateTexture(nil, "ARTWORK")
        ring:SetAllPoints()
        ring:SetTexture(TEX .. "medaillon_cercle.tga")

        local halo = medal:CreateTexture(nil, "OVERLAY")
        halo:SetPoint("CENTER")
        ns.Size(halo, 150 * Z, 150 * Z)
        halo:SetTexture(TEX .. "glow.tga")
        halo:SetBlendMode("ADD")
        halo:SetVertexColor(c[1], c[2], c[3])
        halo:SetAlpha(0)
        entry.halo = halo

        local label = medal:CreateFontString(nil, "OVERLAY")
        Font(label, BODY, 16, { 0.94, 0.90, 0.82 }, "OUTLINE")
        label:SetPoint("CENTER", 0, 1)
        label:SetText(KEY_NAMES[lane])
        entry.label = label

        f.lanes[lane] = entry
    end

    -- Notes : au-dessus des médaillons ; le rideau cache leur apparition en haut des colonnes.
    local notes = CreateFrame("Frame", nil, field)
    notes:SetAllPoints(field)
    notes:SetFrameLevel(field:GetFrameLevel() + 3)
    f.notes = notes

    local curtainFrame = CreateFrame("Frame", nil, field)
    curtainFrame:SetAllPoints(field)
    curtainFrame:SetFrameLevel(field:GetFrameLevel() + 4)
    local curtain = curtainFrame:CreateTexture(nil, "OVERLAY")
    curtain:SetPoint("TOPLEFT", 0, 0)
    ns.Size(curtain, 4 * LANE_W, 56)
    curtain:SetTexture(TEX .. "rideau.tga")

    -- Jugements et éclats de frappe.
    local fx = CreateFrame("Frame", nil, field)
    fx:SetAllPoints(field)
    fx:SetFrameLevel(field:GetFrameLevel() + 5)
    f.fx = {}
    for lane = 1, 4 do
        local entry = { lane = lane }
        entry.burst = fx:CreateTexture(nil, "OVERLAY")
        entry.burst:SetTexture(TEX .. "anneau.tga")
        entry.burst:SetBlendMode("ADD")
        entry.burst:Hide()
        entry.text = fx:CreateFontString(nil, "OVERLAY")
        Font(entry.text, BODY, 19, INK, "OUTLINE")
        entry.text:Hide()
        f.fx[lane] = entry
    end

    f.countdown = fx:CreateFontString(nil, "OVERLAY")
    Font(f.countdown, DISPLAY, 64, BRASS)
    f.countdown:SetPoint("CENTER", field, "BOTTOMLEFT", 2 * LANE_W, WOOD + AREA * 0.5)

    f.status1 = fx:CreateFontString(nil, "OVERLAY")
    Font(f.status1, BODY, 12, MUTED, "OUTLINE")
    f.status1:SetPoint("TOPLEFT", field, "TOPLEFT", 10, -8)
    f.status2 = fx:CreateFontString(nil, "OVERLAY")
    Font(f.status2, BODY, 12, BRASS, "OUTLINE")
    f.status2:SetPoint("TOPLEFT", field, "TOPLEFT", 10, -24)

    -- Bandeau du haut : retour, titre, justesse et combo.
    local hud = CreateFrame("Frame", nil, f)
    hud:SetAllPoints(f)
    hud:SetFrameLevel(f:GetFrameLevel() + 6)

    local track = hud:CreateTexture(nil, "BACKGROUND")
    track:SetPoint("TOPLEFT", 3, -3)
    track:SetPoint("TOPRIGHT", -3, -3)
    track:SetHeight(4)
    track:SetTexture(1, 1, 1, 0.08)
    f.progress = hud:CreateTexture(nil, "ARTWORK")
    f.progress:SetPoint("TOPLEFT", 3, -3)
    f.progress:SetHeight(4)
    f.progress:SetTexture(BRASS[1], BRASS[2], BRASS[3], 1)
    f.progress:SetWidth(1)

    local back = CreateFrame("Button", nil, hud, "UIPanelButtonTemplate")
    ns.Size(back, 34, 26)
    back:SetPoint("TOPLEFT", 14, -18)
    back:SetText("<")
    back:SetScript("OnClick", function() Game.Abort() end)

    f.title = hud:CreateFontString(nil, "OVERLAY")
    Font(f.title, DISPLAY, 22, INK)
    f.title:SetPoint("TOPLEFT", 58, -14)
    f.title:SetJustifyH("LEFT")
    f.title:SetWidth(240)
    f.subtitle = hud:CreateFontString(nil, "OVERLAY")
    Font(f.subtitle, BODY, 11, MUTED)
    f.subtitle:SetPoint("TOPLEFT", 58, -40)
    f.subtitle:SetJustifyH("LEFT")
    f.subtitle:SetWidth(240)

    f.acc = hud:CreateFontString(nil, "OVERLAY")
    Font(f.acc, BODY, 16, INK, "OUTLINE")
    f.acc:SetPoint("TOPRIGHT", -16, -14)
    f.combo = hud:CreateFontString(nil, "OVERLAY")
    Font(f.combo, BODY, 12, MUTED)
    f.combo:SetPoint("TOPRIGHT", -16, -36)

    f.result = CreateResultCard(f)
    f.border = CreateBorder(f)

    f:EnableKeyboard(false)
    f:SetScript("OnKeyDown", function(_, key) Game.OnKeyDown(key) end)
    f:SetScript("OnKeyUp", function(_, key) Game.OnKeyUp(key) end)
    f:SetScript("OnUpdate", function() Game.OnUpdate() end)
    return f
end

-- ------------------------------------------------------------- notes à l'écran

local function Acquire(kind)
    local tex = table.remove(pool[kind])
    if tex then return tex end
    tex = frame.notes:CreateTexture(nil, kind == "bar" and "ARTWORK" or "OVERLAY")
    return tex
end

local function ReleaseParts(note)
    for _, kind in ipairs({ "head", "stem", "bar" }) do
        local tex = note[kind]
        if tex then
            tex:Hide()
            table.insert(pool[kind], tex)
            note[kind] = nil
        end
    end
end

local function EnsureParts(note)
    if note.head then return end
    local c = LANE_COLORS[note.lane]
    note.head = Acquire("head")
    ns.Size(note.head, NOTE_R * 2, NOTE_R * 2)
    note.head:SetTexture(TEX .. "note.tga")
    note.head:SetVertexColor(c[1], c[2], c[3])
    note.stem = Acquire("stem")
    ns.Size(note.stem, 3, NOTE_R * 2)
    note.stem:SetTexture(0.078, 0.094, 0.149, 1)
    if note.hold > 0 then
        note.bar = Acquire("bar")
        note.bar:SetTexture(c[1], c[2], c[3], 1)
    end
end

local function Draw(now)
    local ceiling = FIELD_H - 8
    for _, note in ipairs(state.notes) do
        local visible = note.headRes == nil or note.holding
        if visible then
            local y = WOOD + (note.time - now) / state.travel * AREA
            if note.holding then y = WOOD end
            if y > ceiling or y < 20 then
                ReleaseParts(note)
            else
                EnsureParts(note)
                local x = (note.lane - 0.5) * LANE_W
                note.head:ClearAllPoints()
                note.head:SetPoint("CENTER", frame.notes, "BOTTOMLEFT", x, y)
                note.head:Show()
                note.stem:ClearAllPoints()
                note.stem:SetPoint("BOTTOMLEFT", frame.notes, "BOTTOMLEFT", x + NOTE_R * 0.55, y)
                note.stem:Show()
                if note.bar then
                    local top = math.min(WOOD + (note.endTime - now) / state.travel * AREA, FIELD_H - 8)
                    note.bar:ClearAllPoints()
                    note.bar:SetPoint("BOTTOMLEFT", frame.notes, "BOTTOMLEFT", x - NOTE_R * 0.35, y)
                    ns.Size(note.bar, NOTE_R * 0.7, math.max(1, top - y))
                    note.bar:SetAlpha(note.holding and 0.75 or 0.45)
                    note.bar:Show()
                end
            end
        else
            ReleaseParts(note)
        end
    end
end

local function UpdateFx(now)
    for lane, entry in ipairs(frame.fx) do
        local cx = (lane - 0.5) * LANE_W
        if entry.textT0 then
            local age = now - entry.textT0
            if age > 0.5 then
                entry.text:Hide()
                entry.textT0 = nil
            else
                entry.text:ClearAllPoints()
                entry.text:SetPoint("CENTER", frame.fx_parent or frame.field, "BOTTOMLEFT", cx, WOOD + R + 22 + age * 40)
                entry.text:SetAlpha(1 - age / 0.5)
            end
        end
        if entry.burstT0 then
            local age = now - entry.burstT0
            if age > 0.5 then
                entry.burst:Hide()
                entry.burstT0 = nil
            else
                local d = 2 * R + 220 * age
                ns.Size(entry.burst, d, d)
                entry.burst:SetAlpha(1 - age / 0.5)
            end
        end
    end
end

local function Flash(lane, kind)
    local entry = frame.fx[lane]
    local now = GetTime()
    local color = COLORS[kind] or COLORS.miss
    local labels = { perfect = ns.T("Parfait"), good = ns.T("Bon"), miss = ns.T("Raté"), released = ns.T("Lâché") }
    entry.text:SetText(labels[kind] or ns.T("Raté"))
    entry.text:SetTextColor(color[1], color[2], color[3])
    entry.text:SetAlpha(1)
    entry.text:Show()
    entry.textT0 = now
    if kind == "perfect" or kind == "good" then
        local c = kind == "perfect" and { 1, 0.95, 0.77 } or LANE_COLORS[lane]
        entry.burst:ClearAllPoints()
        entry.burst:SetPoint("CENTER", frame.field, "BOTTOMLEFT", (lane - 0.5) * LANE_W, WOOD)
        entry.burst:SetVertexColor(c[1], c[2], c[3])
        entry.burst:Show()
        entry.burstT0 = now
    end
end

local function SetPressed(lane, on)
    local entry = frame.lanes[lane]
    local c = LANE_COLORS[lane]
    entry.glow:SetAlpha(on and 0.28 or 0.07)
    entry.halo:SetAlpha(on and 0.8 or 0)
    entry.medal:SetScale(on and 1.08 or 1)
    if on then
        entry.lamp:SetVertexColor(1, 0.97, 0.88)
        entry.label:SetTextColor(0.1, 0.08, 0.03)
    else
        entry.lamp:SetVertexColor(c[1], c[2], c[3])
        entry.label:SetTextColor(0.94, 0.90, 0.82)
    end
end

-- ------------------------------------------------------------- jugement

local function SongTime()
    return GetTime() - state.startTime
end

local function PlayNote(note)
    local midi = note.midi
    while midi < LOWEST_SOUND do midi = midi + 12 end
    while midi > HIGHEST_SOUND do midi = midi - 12 end
    PlaySoundFile(string.format(SOUND_PATH, state.sound, midi))
end

local function Accuracy()
    local judged = state.perfect + state.good + state.miss
    if judged == 0 then return 1 end
    return (state.perfect + 0.6 * state.good) / judged
end

local function UpdateHud()
    frame.acc:SetText(math.floor(Accuracy() * 100 + 0.5) .. " %")
    frame.combo:SetText(ns.T("combo ") .. state.combo)
    frame.watermark:SetText(state.combo >= 10 and tostring(state.combo) or "")
    local crescendo = math.min(math.floor(state.maxCombo / COMBO_STEP) * 2, 12)
    frame.status2:SetText(crescendo > 0 and (ns.T("Crescendo +") .. crescendo .. " %") or "")
end

local function Credit(points)
    state.score = state.score + points * (1 + math.floor(state.combo / COMBO_STEP) * 0.1)
    state.combo = state.combo + 1
    if state.combo > state.maxCombo then state.maxCombo = state.combo end
end

-- Première note de la colonne qui n'a pas encore été jugée.
local function NextNote(lane)
    local list = state.lanes[lane]
    local index = state.nextIndex[lane]
    while list[index] and list[index].headRes ~= nil do
        index = index + 1
    end
    state.nextIndex[lane] = index
    return list[index]
end

local function MissNote(note)
    note.headRes = "miss"
    state.miss = state.miss + 1
    if note.hold > 0 then
        note.tailRes = "miss"
        state.miss = state.miss + 1
    end
    state.combo = 0
    Flash(note.lane, "miss")
    ReleaseParts(note)
    UpdateHud()
end

local function FinishHold(note, kept)
    state.holding[note.lane] = nil
    note.holding = false
    if kept then
        note.tailRes = "perfect"
        state.perfect = state.perfect + 1
        state.holdsKept = state.holdsKept + 1
        Credit(50)
        Flash(note.lane, "perfect")
    else
        note.tailRes = "miss"
        state.miss = state.miss + 1
        state.combo = 0
        Flash(note.lane, "released")
    end
    ReleaseParts(note)
    UpdateHud()
end

function Game.OnKeyDown(key)
    if not state then return end
    if key == "ESCAPE" then
        if state.finished then
            Game.Close()
            ns.UI.Show()
        else
            Game.Abort()
        end
        return
    end
    local lane = KEYS[key]
    if not lane or state.finished or state.pressed[lane] then return end
    state.pressed[lane] = true
    SetPressed(lane, true)

    local note = NextNote(lane)
    if not note then return end
    local gap = math.abs(note.time - SongTime())
    if gap > GOOD then return end            -- touche dans le vide : sans effet

    local grade = gap <= PERFECT and "perfect" or "good"
    note.headRes = grade
    PlayNote(note)
    if #state.hitQueue < 12 then table.insert(state.hitQueue, note.midi) end
    if grade == "perfect" then
        state.perfect = state.perfect + 1
        Credit(100)
    else
        state.good = state.good + 1
        Credit(50)
    end
    Flash(lane, grade)
    if note.hold > 0 then
        note.holding = true
        state.holding[lane] = note
    else
        ReleaseParts(note)
    end
    UpdateHud()
end

function Game.OnKeyUp(key)
    if not state then return end
    local lane = KEYS[key]
    if not lane then return end
    state.pressed[lane] = false
    SetPressed(lane, false)
    local note = state.holding[lane]
    if note and not state.finished then
        FinishHold(note, SongTime() >= note.endTime - RELEASE_SLACK)
    end
end

-- Mode automatique (maîtres de jeu, /barde auto) : appuie sur les touches au bon moment, avec la justesse demandée.
function Game.AutoPlay(now)
    for lane = 1, 4 do
        local key = KEY_NAMES[lane]
        local held = state.holding[lane]
        if state.pressed[lane] then
            if not held or now >= held.endTime - 0.02 then Game.OnKeyUp(key) end
        else
            local note = NextNote(lane)
            if note and now >= note.time and not note.autoTried then
                note.autoTried = true
                if math.random() < ns.autoplay then Game.OnKeyDown(key) end
            end
        end
    end
end

-- Les notes qu'un autre barde de l'ensemble vient de jouer : on les entend avec son instrument.
function Game.Hear(instrumentId, midis, partner, voice)
    if not midis or ns.Muted() then return end
    if state and not state.finished then
        if not partner then return end          -- en jouant, on n'entend que ses partenaires
    elseif state or partner or not ns.Listening() then
        return                                   -- hors d'un morceau, on écoute les bardes proches si on le veut
    end
    local ins = ns.GetInstrument(instrumentId)
    local sound = (voice and voice ~= "" and voice) or (ins and ins.sound) or "luth"
    local count = 0
    for text in string.gmatch(midis, "[^,]+") do
        count = count + 1
        if count > 12 then return end
        local midi = tonumber(text)
        if midi then
            while midi < LOWEST_SOUND do midi = midi + 12 end
            while midi > HIGHEST_SOUND do midi = midi - 12 end
            PlaySoundFile(string.format(SOUND_PATH, sound, midi))
        end
    end
end

function Game.OnUpdate()
    if not state then return end
    local now = SongTime()

    if not state.finished then
        frame.countdown:SetText(now < 0 and tostring(math.ceil(-now)) or "")
        if ns.autoplay then Game.AutoPlay(now) end
        if #state.hitQueue > 0 and GetTime() >= state.nextFlush then
            ns.Send("HITS", table.concat(state.hitQueue, ","))
            wipe(state.hitQueue)
            state.nextFlush = GetTime() + 0.1
        end
        for lane = 1, 4 do
            local note = NextNote(lane)
            while note and now - note.time > GOOD do
                MissNote(note)
                note = NextNote(lane)
            end
            local held = state.holding[lane]
            if held and now >= held.endTime then
                FinishHold(held, true)
            end
        end
        frame.progress:SetWidth(math.max(1, (W - 6) * math.min(1, math.max(0, now / state.length))))
        Draw(now)
        if now > state.length + 0.6 then Game.Finish() end
    end
    UpdateFx(GetTime())
end

-- ------------------------------------------------------------- une partie

function Game.Start(song)
    frame = frame or CreateStage()
    frame.fx_parent = frame.field
    if state then
        for _, note in ipairs(state.notes) do ReleaseParts(note) end
    end
    for lane = 1, 4 do
        SetPressed(lane, false)
        frame.fx[lane].text:Hide()
        frame.fx[lane].burst:Hide()
        frame.fx[lane].textT0, frame.fx[lane].burstT0 = nil, nil
    end

    local step = 15 / (song.bpm * song.tempo)       -- durée d'une double croche
    state = {
        token = song.token, song = song.song or { id = song.id, name = song.name }, training = song.training,
        name = song.name, travel = TRAVEL[song.tier or 0] or 1.8,
        notes = {}, lanes = { {}, {}, {}, {} }, nextIndex = { 1, 1, 1, 1 },
        pressed = {}, holding = {},
        perfect = 0, good = 0, miss = 0, holdsKept = 0,
        combo = 0, maxCombo = 0, score = 0, length = 0,
        startTime = GetTime() + LEAD_IN,
        instrument = ns.GetInstrument(song.instrumentId) or ns.CurrentInstrument(),
        group = song.group or 1, part = song.part or 0, hitQueue = {}, nextFlush = 0,
    }
    state.sound = state.instrument and state.instrument.sound or "luth"
    for _, raw in ipairs(song.notes) do
        local note = {
            time = raw.step * step, lane = raw.lane, midi = raw.midi,
            hold = raw.len > 0 and raw.len * step * HOLD_FRACTION or 0,
        }
        note.endTime = note.time + note.hold
        table.insert(state.notes, note)
        table.insert(state.lanes[note.lane], note)
        state.length = math.max(state.length, note.endTime)
    end
    for lane = 1, 4 do
        table.sort(state.lanes[lane], function(a, b) return a.time < b.time end)
    end

    frame.title:SetText(ns.N(song.name))
    -- Un titre long se réduit pour tenir sur une ligne, sans déborder sur le sous-titre.
    local length = string.len(ns.N(song.name))
    Font(frame.title, DISPLAY, length > 26 and 15 or length > 21 and 17 or 22, INK)
    frame.subtitle:SetText(ns.Sub(song.song and song.song.subtitle or ""))
    frame.status1:SetText((state.instrument and ns.InstrumentName(state.instrument) or ns.T("Luth"))
        .. (song.training and ns.T(" · entraînement : tempo -25 %, buff ÷ 2")
            or (song.group and song.group > 1) and (ns.T(" · ensemble de ") .. song.group) or ns.T(" · solo"))
        .. ((song.part == 1) and ns.T(" · rythme") or (song.part == 2) and ns.T(" · harmonie") or ""))
    frame.result:Hide()
    frame.progress:SetWidth(1)
    UpdateHud()
    frame:Show()
    frame:EnableKeyboard(true)
end

function Game.Close()
    if frame then
        frame:EnableKeyboard(false)
        frame:Hide()
        frame.result:Hide()
    end
    if state then
        for _, note in ipairs(state.notes) do ReleaseParts(note) end
        state = nil
    end
end

function Game.Abort()
    if not state then return end
    if not state.finished then ns.Send("ABORT", state.token) end
    Game.Close()
    ns.UI.Show()
    ns.Print(ns.T("morceau abandonné."))
end

function Game.Finish()
    state.finished = true
    state.waiting = true
    frame.countdown:SetText("")
    state.finalScore = math.floor(state.score + 0.5)
    ns.Send("RES", state.token, state.perfect, state.good, state.miss, state.holdsKept,
        state.maxCombo, state.finalScore)

    local card = frame.result
    card.title:SetText("...")
    card.info:SetText(ns.T("Envoi du résultat au serveur..."))
    for _, box in ipairs(card.boxes) do box.num:SetText("") end
    card.head:SetText("")
    card.value:SetText("")
    card.calc:SetText("")
    card.duration:SetText("")
    card.record:SetText("")
    card.again:Disable()
    card.book:Enable()
    card:Show()
end

function Game.IsWaiting(token)
    return state and state.waiting and state.token == token
end

-- ------------------------------------------------------------- fin de morceau

local function Grade(accuracy)
    if accuracy >= 0.95 then return ns.T("Virtuose") end
    if accuracy >= 0.85 then return ns.T("Superbe") end
    if accuracy >= 0.70 then return ns.T("Belle prestation") end
    if accuracy >= 0.50 then return ns.T("Correct") end
    return ns.T("Fausse note !")
end

function Game.ShowResult(result)
    if not Game.IsWaiting(result.token) then return end
    state.waiting = false

    local card = frame.result
    local accuracy = result.accuracy
    local percent = math.floor(accuracy * 100 + 0.5)
    card.title:SetText(Grade(accuracy))
    local groupNote = (result.group and result.group > 1)
        and (ns.T(" · groupe |cffffffff") .. math.floor(result.groupAccuracy * 100 + 0.5) .. " %|r") or ""
    local partName = (state.part == 1) and "rythme" or (state.part == 2) and "harmonie" or ns.T("mélodie")
    card.info:SetText(ns.T("Toi, ") .. partName .. ns.T(" : |cffffffff") .. percent .. " %|r" .. groupNote .. ns.T(" · combo max ") .. state.maxCombo
        .. ns.T(" · score ") .. ns.Int(result.score) .. (state.training and ns.T(" · entraînement") or ""))
    card.boxes[1].num:SetText(state.perfect)
    card.boxes[2].num:SetText(state.good)
    card.boxes[3].num:SetText(state.miss)

    local ins = state.instrument
    local effect = ns.N(ins and ins.effect or "")
    local amount = ns.Value(ins, result.amount)
    local minutes = math.floor(result.seconds / 60 + 0.5)
    if result.malus then
        card.head:SetText("|cffe0707a" .. ns.N(result.text) .. ns.T("|r frappe le barde :"))
        card.value:SetText("|cffe0707a" .. amount .. " " .. effect .. "|r")
        card.calc:SetText(ns.T("Sous 50 % de justesse, la fausse note se retourne contre le musicien."))
        card.duration:SetText(ns.T("Durée ") .. minutes .. ns.T(" min"))
    elseif result.amount > 0 then
        card.head:SetText(ns.N(result.text) .. ns.T(" accordé :"))
        card.value:SetText("+" .. amount .. " " .. effect)
        local together = result.group and result.group > 1
        local shown = together and math.floor(result.groupAccuracy * 100 + 0.5) or percent
        local parts = { ns.T("base ") .. ns.Dec(result.base, 2) .. (together and (ns.T(" (ensemble ×") .. ns.Dec(result.multiplier, 2) .. ")") or "")
            .. ((result.quality or 1) > 1.001 and (ns.T(" (qualité ×") .. ns.Dec(result.quality, 2) .. ")") or "")
            .. ((result.tuneMult or 1) < 0.999 and (ns.T(" (désaccordé ×") .. ns.Dec(result.tuneMult, 2) .. ")") or "")
            .. ((result.mastery or 0) > 0 and (ns.T(" (maîtrise ") .. ns.T(ns.MASTERY[result.mastery].name) .. " ×" .. ns.Dec(1 + 0.01 * result.mastery, 2) .. ")") or "")
            .. ns.T(" × justesse ") .. (together and ns.T("du groupe ") or "") .. shown .. " % ^1,5" }
        if result.crescendo > 0 then
            table.insert(parts, ns.T("× crescendo +") .. math.floor(result.crescendo * 100 + 0.5) .. " %")
        end
        if state.training then table.insert(parts, ns.T("÷ 2 (entraînement)")) end
        local calc = table.concat(parts, " ")
        if result.capped then calc = calc .. ns.T("\nPlafonné à ×1,2 la valeur de base.") end
        card.calc:SetText(calc)
        card.duration:SetText(ns.T("Durée ") .. minutes .. ns.T(" min"))
    else
        card.head:SetText("")
        card.value:SetText("|cffe0707a" .. ns.N(result.text) .. "|r")
        card.calc:SetText("")
        card.duration:SetText("")
    end

    ns.info.skill = result.skill
    local skillNote = ""
    if result.gained > 0 then
        skillNote = ns.T("|cffffd100+") .. result.gained .. ns.T(" compétence (") .. result.skill .. ns.T(")|r · ")
    elseif not state.training and accuracy >= 0.5 and accuracy < 0.7 then
        skillNote = ns.T("Pas de point : 70 % de justesse requis · ")
    end
    if state.training then
        card.record:SetText(ns.T("Entraînement : score, buff et compétence non comptés."))
    elseif result.newBest then
        card.record:SetText(skillNote .. ns.T("Nouveau record !"))
    else
        card.record:SetText(skillNote .. ns.T("Meilleur score : ") .. ns.Int(result.best))
    end
    card.again:Enable()
    card.book:Enable()
    ns.RequestSongs()
end

function Game.ShowRefused(text)
    state.waiting = false
    local card = frame.result
    card.title:SetText(ns.T("Refusé"))
    card.info:SetText("|cffe0707a" .. text .. "|r")
    card.again:Enable()
end
