-- Barde : échanges avec le module serveur mod-barde.
-- Licence AGPL-3.0, voir LICENSE.
--
-- Le serveur et l'addon se parlent par messages d'addon chuchotés au joueur
-- lui-même, préfixe BARDE, champs séparés par « ^ ». Le détail du protocole est
-- dans src/BardeProtocol.cpp. Un message BARDE ne compte que s'il vient du
-- joueur lui-même : c'est ainsi qu'arrivent les réponses du serveur.

local ADDON, ns = ...

ns.PREFIX = "BARDE"
ns.SEP = "^"
ns.songs = {}           -- airs reçus : { id, tier, bpm, count, holds, skill, best, bestAcc, wins, base, subtitle, name }
ns.info = { skill = 1, skillMax = 300 }          -- compétence du joueur
ns.tiers = {}           -- paliers d'instruments reçus : { tier, suffix, skill, mats = { { item, count, name } } }
ns.instruments = {}     -- instruments reçus : { id, name, sound, buff, effect, unit, min, max, buffSpell, has }
ns.instrumentId = nil   -- instrument choisi pour jouer
ns.pending = nil        -- partition en cours de réception
ns.TEXTURES = "Interface\\AddOns\\Barde\\Textures\\"

-- Maîtrise d'un air : bronze après 5 réussites (+1 %), argent après 15 (+2 %), or après 40 (+3 %).
ns.MASTERY = { { wins = 5, name = "bronze", color = "|cffcd7f32", bonus = 1 }, { wins = 15, name = "argent", color = "|cffc0c0c0", bonus = 2 },
    { wins = 40, name = "or", color = "|cffffd100", bonus = 3 } }
function ns.Mastery(wins)
    local tier = 0
    for index, step in ipairs(ns.MASTERY) do
        if wins >= step.wins then tier = index end
    end
    return tier
end

ns.TIERS = ns.Lazy({ [0] = "Apprenti", "Compagnon", "Ménestrel", "Maître", "Grand barde", [100] = "Mes créations", [101] = "Custom" })
-- Les groupes du carnet : les cinq paliers du livre, puis les airs que l'on a composés (100) et ceux que l'on a achetés (101).
ns.GROUPS = { 0, 1, 2, 3, 4, 100, 101 }
function ns.GroupOf(song)
    return song.kind == 1 and 100 or song.kind == 2 and 101 or song.tier
end

-- ------------------------------------------------------------- utilitaires

function ns.Print(text)
    DEFAULT_CHAT_FRAME:AddMessage(ns.T("|cffd9a441Barde|r : ") .. text)
end

function ns.Send(...)
    SendAddonMessage(ns.PREFIX, table.concat({ ... }, ns.SEP), "WHISPER", UnitName("player"))
end

function ns.Size(region, width, height)
    region:SetWidth(width)
    region:SetHeight(height)
end

function ns.Shown(region, shown)
    if shown then region:Show() else region:Hide() end
end

-- Nombre entier avec espaces entre les milliers : 4 200 -> "4 200".
function ns.Int(n)
    local text = tostring(math.floor(n + 0.5))
    local separator = ns.IsEnglish() and "," or " "
    local out = text
    repeat
        text = out
        out = string.gsub(text, "^(-?%d+)(%d%d%d)", "%1" .. separator .. "%2")
    until out == text
    return out
end

-- Décimale à la française : 1.41 -> "1,41" (jusqu'à `digits` chiffres, sans zéros inutiles).
function ns.Dec(x, digits)
    if ns.IsEnglish() then
        local text = string.format("%." .. (digits or 2) .. "f", x)
        if string.find(text, "%.") then text = string.gsub(string.gsub(text, "0+$", ""), "%.$", "") end
        return text
    end
    local text = string.format("%." .. (digits or 2) .. "f", x)
    if string.find(text, "%.") then
        text = string.gsub(string.gsub(text, "0+$", ""), "%.$", "")
    end
    return (string.gsub(text, "%.", ","))
end

-- Valeur du buff pour l'affichage : entier comme le serveur l'applique.
function ns.Round(x)
    return math.max(1, math.floor(x + 0.5))
end

-- ------------------------------------------------------------- messages du serveur

local handlers = {}

-- Deux demandes de liste qui se croisent donneraient chaque ligne deux fois : on remplace la ligne qui a le même numéro.
local function Put(list, entry)
    for index, existing in ipairs(list) do
        if existing.id == entry.id then
            list[index] = entry
            return entry
        end
    end
    table.insert(list, entry)
    return entry
end

function handlers.AIR(f)
    Put(ns.songs, {
        id = tonumber(f[2]), tier = tonumber(f[3]), bpm = tonumber(f[4]), count = tonumber(f[5]),
        holds = tonumber(f[6]), skill = tonumber(f[7]), best = tonumber(f[8]),
        bestAcc = tonumber(f[9]) / 1000, wins = tonumber(f[10]), power = tonumber(f[11]) / 1000,
        subtitle = f[12] or "", name = f[13] or "?", kind = tonumber(f[14]) or 0, mine = (tonumber(f[14]) or 0) > 0, factor = (tonumber(f[15]) or 1000) / 1000,
    })
end

function handlers.INS(f)
    local ins = Put(ns.instruments, {
        id = tonumber(f[2]), name = f[3] or "?", sound = f[4] or "luth", buff = f[5] or "", effect = f[6] or "",
        unit = f[7] or "", min = tonumber(f[8]) / 100, max = tonumber(f[9]) / 100,
        buffSpell = tonumber(f[10]), has = f[11] == "1", bestTier = tonumber(f[12]) or -1, owned = {},
    })
    local mask = tonumber(f[13]) or 0
    ins.tuning = (tonumber(f[14]) or 1000) / 10          -- accord de l'instrument en main, en pourcentage
    for t = 0, 4 do
        ins.owned[t] = math.floor(mask / 2 ^ t) % 2 == 1
    end
end

function handlers.TIER(f)
    local mats = {}
    for item, count, name in string.gmatch(f[5] or "", "(%d+):(%d+):([^,]*)") do
        table.insert(mats, { item = tonumber(item), count = tonumber(count), name = name })
    end
    local tier = tonumber(f[2]) or 0
    local gemItem, gemName = string.match(f[6] or "", "^(%d+):(.*)$")
    local crystalItem, crystalName = string.match(f[7] or "", "^(%d+):(.*)$")
    ns.tiers[tier + 1] = {
        tier = tier, suffix = f[3] or "", skill = tonumber(f[4]) or 1, mats = mats,
        gem = gemItem and { item = tonumber(gemItem), name = gemName } or nil,
        crystal = crystalItem and { item = tonumber(crystalItem), name = crystalName } or nil,
    }
end

-- Les chances de qualité à la fabrication, sans gemme, avec la gemme, avec la gemme et le cristal (en pourcentage :
-- commun, inhabituel, rare, épique), puis le bonus maximal de compétence et la compétence qu'il faut pour chaque point.
function handlers.QUAL(f)
    local q = {}
    for mode = 1, 3 do
        q[mode] = {}
        for value in string.gmatch(f[mode + 1] or "", "%d+") do table.insert(q[mode], tonumber(value)) end
    end
    q.bonusMax = tonumber(f[5]) or 10
    q.bonusPer = tonumber(f[6]) or 15
    ns.qualityChances = q
end

-- Accordé, ou clés fabriquées : on le dit et on redemande l'état.
-- Le classement d'un air : les cinq meilleurs scores du serveur.
function handlers.TOP(f)
    local list = {}
    for name, score, acc in string.gmatch(f[3] or "", "([^,;]+),(%d+),(%d+)") do
        table.insert(list, { name = name, score = tonumber(score), acc = tonumber(acc) / 10 })
    end
    ns.top = { song = tonumber(f[2]), list = list }
    if ns.UI.ShowTop then ns.UI.ShowTop() end
end

-- Ce que contient un parchemin d'air : demandé au serveur la première fois qu'on passe la souris dessus.
ns.scrolls = {}
function handlers.SCROLLINFO(f)
    ns.scrolls[tonumber(f[2])] = { title = f[3] or "?", tier = tonumber(f[4]) or 0, composer = f[5] or "?" }
end

GameTooltip:HookScript("OnTooltipSetItem", function(tip)
    local _, link = tip:GetItem()
    if not link then return end
    local item, enchant = string.match(link, "item:(%d+):(%d+)")
    if tonumber(item) ~= 1950040 then return end
    local song = tonumber(enchant) or 0
    if song == 0 then return end
    local info = ns.scrolls[song]
    if info then
        tip:AddLine(ns.T("Air : ") .. "|cffffffff" .. ns.N(info.title) .. "|r", 1, 0.82, 0)
        tip:AddLine(ns.T("par ") .. info.composer .. " · " .. ns.T("palier ") .. (ns.TIERS[info.tier] or "?"), 0.7, 0.7, 0.8)
        tip:Show()
    else
        ns.Send("SCROLL", song)
    end
end)

-- La voie du barde : SPEC^courante^compétence requise^prix de changement (cuivre)
ns.SPECS = ({
    { name = "Barde de guerre", short = "Guerre", text = "|cffffd100Chants de combat : +10 %|r\nCromorne, vièle, piano, orgue, ocarina, tympanon et tambour.\nExemple : un chant de +100 d'armure passe à +110.\nRien ne change pour le luth, la harpe et la flûte." },
    { name = "Ménestrel de cour", short = "Cour", text = "|cffffd100Portée : 7,5 m → 11 m|r, auditeurs : 8 → 16.\n|cffffd100Part du public x1,5|r : au camp 50 % → 75 % du chant, en taverne 25 % → 37 %.\n|cffffd100Applaudissement double|r : +2 min au lieu de +1 min de chant pour les auditeurs (max 5 min), et 2 ovations pour vos titres.\nAucun bonus sur la force de vos chants." },
    { name = "Conteur", short = "Conteur", text = "|cffffd100Chants du luth, de la harpe et de la flûte : +10 %|r\n|cffffd100Part du public x2|r : au camp 50 % → 100 % du chant (la même force que la vôtre), en taverne 25 % → 50 %.\nRien ne change pour les autres instruments." },
})

function handlers.SPEC(f)
    ns.spec = { current = tonumber(f[2]) or 0, minSkill = tonumber(f[3]) or 180, cost = tonumber(f[4]) or 100000 }
    if ns.UI.RefreshSpec then ns.UI.RefreshSpec() end
end

-- La quête légendaire : LEGEND^forgé^remis^duel^trio^or^parfaits^total^barres^essences^braises^masque des instruments épiques
function handlers.LEGEND(f)
    ns.legend = {
        forged = f[2] == "1", delivered = f[3] == "1", duel = f[4] == "1", trio = f[5] == "1",
        gold = tonumber(f[6]) or 0, perfect = tonumber(f[7]) or 0, total = tonumber(f[8]) or 40,
        bars = tonumber(f[9]) or 0, essences = tonumber(f[10]) or 0, embers = tonumber(f[11]) or 0, epic = tonumber(f[12]) or 0,
    }
    if ns.Legend then ns.Legend.Refresh() end
end

function handlers.DONE(f)
    ns.Print(f[2] or "fait.")
    PlaySound("igMainMenuOptionCheckBoxOn")
    ns.RequestSongs()
end

function handlers.CRAFTED(f)
    ns.Print(ns.T("instrument fabriqué (qualité : ") .. ns.T(f[2] or "commun") .. ").")
    PlaySound("igMainMenuOptionCheckBoxOn")
    ns.RequestSongs()
end

function handlers.AIRS_END(f)
    ns.info.skill = tonumber(f[3]) or 1
    ns.info.skillMax = tonumber(f[4]) or 300
    ns.isGM = f[5] == "1"
    -- Le choix est gardé s'il existe encore ; sinon le premier instrument possédé, à défaut le premier.
    if not ns.GetInstrument(ns.instrumentId) then
        ns.instrumentId = nil
        for _, ins in ipairs(ns.instruments) do
            if ins.has and not ns.instrumentId then ns.instrumentId = ins.id end
        end
        if not ns.instrumentId and ns.instruments[1] then ns.instrumentId = ns.instruments[1].id end
    end
    ns.UI.Refresh()
    if ns.Atelier then ns.Atelier.Refresh() end
end

function handlers.BEGIN(f)
    ns.pending = {
        token = f[2], id = tonumber(f[3]), bpm = tonumber(f[4]), tempo = tonumber(f[5]) / 1000,
        count = tonumber(f[6]), chunks = tonumber(f[7]), training = f[8] == "1", tier = tonumber(f[9]),
        name = f[10] or "?", group = tonumber(f[11]) or 1, part = tonumber(f[12]) or 0, instrumentId = tonumber(f[13]),
        received = 0, notes = {},
    }
    for _, song in ipairs(ns.songs) do
        if song.id == ns.pending.id then ns.pending.song = song end
    end
end

function handlers.N(f)
    local pending = ns.pending
    if not pending or f[2] ~= pending.token then return end
    for item in string.gmatch(f[4] or "", "[^;]+") do
        local step, lane, midi, len = strsplit(",", item)
        table.insert(pending.notes, {
            step = tonumber(step), lane = tonumber(lane), midi = tonumber(midi), len = tonumber(len),
        })
    end
    pending.received = pending.received + 1
    if pending.received == pending.chunks then
        ns.pending = nil
        if #pending.notes ~= pending.count then
            ns.Print(ns.T("partition incomplète (") .. #pending.notes .. ns.T(" notes sur ") .. pending.count .. ").")
            ns.Send("ABORT", pending.token)
            return
        end
        ns.UI.Hide()
        ns.Game.Start(pending)
    end
end

function handlers.OK(f)
    ns.Game.ShowResult({
        token = f[2], accuracy = tonumber(f[3]) / 1000, amount = tonumber(f[4]), seconds = tonumber(f[5]),
        score = tonumber(f[6]), best = tonumber(f[7]), newBest = f[8] == "1", malus = f[9] == "1",
        base = tonumber(f[10]) / 100, crescendo = tonumber(f[11]) / 1000, capped = f[12] == "1",
        text = f[13] or "", gained = tonumber(f[14]) or 0, skill = tonumber(f[15]) or 1,
        group = tonumber(f[16]) or 1, groupAccuracy = (tonumber(f[17]) or 0) / 1000,
        multiplier = (tonumber(f[18]) or 1000) / 1000, quality = (tonumber(f[19]) or 1000) / 1000,
        tuning = (tonumber(f[20]) or 1000) / 1000, tuneMult = (tonumber(f[21]) or 1000) / 1000, mastery = tonumber(f[22]) or 0,
    })
end

-- Jouer ensemble : une invitation à accepter ou refuser (le serveur laisse 20 secondes).
StaticPopupDialogs["BARDE_ENSEMBLE"] = {
    text = ns.T("%s propose de jouer « %s » ensemble."),
    button1 = ACCEPT,
    button2 = DECLINE,
    timeout = 20,
    whileDead = 0,
    hideOnEscape = 1,
    OnAccept = function(self, data)
        ns.Send("ENSJOIN", data, ns.instrumentId or 0)
    end,
    OnCancel = function(self, data)
        ns.Send("ENSNO", data)
    end,
}

-- Un autre barde de l'ensemble a joué ces notes : on les entend avec son instrument.
function handlers.HEAR(f)
    ns.Game.Hear(tonumber(f[2]), f[3], f[4] == "1", f[5])
end

-- Un défi : accepter (avec l'instrument choisi) ou refuser.
StaticPopupDialogs["BARDE_DUEL"] = {
    text = ns.T("%s vous défie en duel à « %s ». Le meilleur score gagne les pourboires."),
    button1 = ACCEPT,
    button2 = DECLINE,
    timeout = 20,
    whileDead = 0,
    hideOnEscape = 1,
    OnAccept = function(self, data) ns.Send("DUELJOIN", data, ns.instrumentId or 0) end,
    OnCancel = function(self, data) ns.Send("DUELNO", data) end,
}

function handlers.IMPROV_OK()
    if ns.Improv then ns.Improv.Accepted() end
end

function handlers.DUELINV(f)
    StaticPopupDialogs["BARDE_DUEL"].text = ns.T("%s vous défie en duel à « %s ». Le meilleur score gagne les pourboires.")
    PlaySound("igQuestLogOpen")
    StaticPopup_Show("BARDE_DUEL", f[3] or "?", f[5] or "?", tonumber(f[2]))
end

function handlers.INVITE(f)
    StaticPopupDialogs["BARDE_ENSEMBLE"].text = ns.T("%s propose de jouer « %s » ensemble.")
    PlaySound("igQuestLogOpen")
    StaticPopup_Show("BARDE_ENSEMBLE", f[3] or "?", f[5] or "?", tonumber(f[2]))
end

-- L'état d'un ensemble : ENSTATE^id^phase^air^bardes^moyenne‰^chant^nom|état|instrument|partie|justesse‰;...
function handlers.ENSTATE(f)
    if ns.Ensemble then ns.Ensemble.Update(f) end
end

function handlers.KO(f)
    local text = f[3] or "erreur"
    if ns.Game.IsWaiting(f[2]) then
        ns.Game.ShowRefused(text)
    else
        ns.Print(text)
        UIErrorsFrame:AddMessage(text, 1, 0.2, 0.2)
    end
end

function handlers.OPEN()
    ns.UI.Show()
end

function handlers.MSG(f)
    ns.Print(f[2] or "")
end

-- L'écoute des bardes proches est un réglage du joueur, gardé d'une session à l'autre.
-- Le son des autres bardes coupé (case du carnet).
function ns.Muted()
    return BardeDB and BardeDB.mute == true
end

function ns.Listening()
    return not BardeDB or BardeDB.listen ~= false
end

local events = CreateFrame("Frame")
events:RegisterEvent("CHAT_MSG_ADDON")
events:RegisterEvent("ADDON_LOADED")
events:SetScript("OnEvent", function(_, event, prefix, message, channel, sender)
    if event == "ADDON_LOADED" then
        if prefix == ADDON then BardeDB = BardeDB or {} end
        return
    end
    if prefix ~= ns.PREFIX or channel ~= "WHISPER" or sender ~= UnitName("player") then return end
    local fields = { strsplit(ns.SEP, message) }
    local handler = handlers[fields[1]]
    if handler then handler(fields) end
end)

-- ------------------------------------------------------------- demandes

-- Le nom de l'instrument tel que le joueur le possède : le meilleur palier qu'il a (« Luth d'argent »), à défaut le nom de base.
function ns.InstrumentName(ins)
    if not ins then return "?" end
    local info = ins.bestTier and ins.bestTier >= 0 and ns.tiers[ins.bestTier + 1]
    if not info then return ns.TierInstrument(ins, "de cuivre") end
    return ns.TierInstrument(ins, info.suffix)
end

function ns.GetInstrument(id)
    for _, ins in ipairs(ns.instruments) do
        if ins.id == id then return ins end
    end
end

function ns.CurrentInstrument()
    return ns.GetInstrument(ns.instrumentId) or ns.instruments[1]
end

-- Valeur de base du chant d'un air avec un instrument : de min à max selon la puissance de l'air.
function ns.Base(song, ins)
    if not ins then return 0 end
    return (ins.min + (ins.max - ins.min) * song.power) * (song.factor or 1)
end

-- « 21 » ou « 4 % » selon l'unité de l'instrument.
function ns.Value(ins, amount)
    return amount .. ((ins and ins.unit == "%") and " %" or "")
end

local lastRequest = 0
function ns.RequestSongs()
    local now = GetTime()
    if now - lastRequest < 0.4 then return end        -- une demande vient de partir : sa réponse arrive
    lastRequest = now
    wipe(ns.songs)
    wipe(ns.instruments)
    wipe(ns.tiers)
    ns.Send("LIST")
end

-- Fabrique l'instrument de ce palier à l'atelier (le serveur vérifie compétence, matériaux et place dans les sacs).
function ns.Tune(instrumentId)
    ns.Send("TUNE", instrumentId)
end

function ns.RequestTop(songId)
    ns.Send("TOP", songId)
end

function ns.MakeCamp()
    ns.Send("CAMP")
end

function ns.MakeKeys()
    ns.Send("KEYS")
end

-- Un curseur de volume : celui des effets sonores du jeu, dont font partie les notes des instruments.
function ns.CreateVolumeSlider(parent, name, width)
    local slider = CreateFrame("Slider", name, parent, "OptionsSliderTemplate")
    slider:SetWidth(width)
    slider:SetHeight(14)
    slider:SetMinMaxValues(0, 1)
    slider:SetValueStep(0.05)
    _G[name .. "Low"]:SetText("")
    _G[name .. "High"]:SetText("")
    _G[name .. "Text"]:SetText(ns.T("Volume des effets sonores"))
    slider:SetValue(tonumber(GetCVar("Sound_SFXVolume")) or 1)
    slider:SetScript("OnValueChanged", function(_, value)
        SetCVar("Sound_SFXVolume", string.format("%.2f", value))
    end)
    slider:SetScript("OnShow", function(self)
        self:SetValue(tonumber(GetCVar("Sound_SFXVolume")) or 1)
    end)
    slider:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_TOP")
        GameTooltip:SetText(ns.T("Volume des effets sonores"))
        GameTooltip:AddLine(ns.T("C'est le réglage du jeu : il change aussi les autres effets (sorts, combat)."), 1, 1, 1, 1)
        GameTooltip:Show()
    end)
    slider:SetScript("OnLeave", function() GameTooltip:Hide() end)
    return slider
end

function ns.Craft(instrumentId, tier, gem)
    ns.Send("CRAFT", instrumentId, tier, gem or 0)
end

function ns.Play(songId, training)
    local ins = ns.CurrentInstrument()
    ns.Send("PLAY", songId, training and "1" or "0", ins and ins.id or 1)
end

-- Propose l'air choisi au groupe : les bardes proches répondent, puis tout le monde joue en même temps.
-- Défie le barde sélectionné (clic sur son personnage) à l'air choisi.
function ns.Duel(songId)
    local ins = ns.CurrentInstrument()
    ns.Send("DUEL", songId, ins and ins.id or 1)
end

function ns.PlayTogether(songId)
    local ins = ns.CurrentInstrument()
    ns.Send("ENS", songId, ins and ins.id or 1)
end

-- /barde auto [justesse] : pour les maîtres de jeu, le morceau se joue tout seul (test d'un ensemble avec un seul clavier).
-- Le serveur vérifie le résultat comme pour n'importe qui ; seul le droit d'activer le mode vient du serveur.
SLASH_BARDE1 = "/barde"
SlashCmdList.BARDE = function(msg)
    local command, argument = strsplit(" ", msg or "")
    if command == "improviser" or command == "improv" then
        ns.Improv.Show()
        return
    end
    if command == "lang" or command == "langue" then
        if argument == "en" or argument == "fr" then ns.SetLang(argument) else ns.ToggleLang() end
        return
    end
    if command == "atelier" then
        ns.Atelier.Show()
        return
    end
    if command == "ecoute" then
        BardeDB = BardeDB or {}
        BardeDB.listen = not ns.Listening()
        ns.Print(BardeDB.listen and ns.T("vous entendez les bardes proches.") or ns.T("vous n'entendez plus les bardes proches."))
        return
    end
    if command == "auto" then
        if not ns.isGM then
            ns.Print(ns.T("le mode automatique est réservé aux maîtres de jeu (ouvrez d'abord le carnet avec /barde)."))
        elseif ns.autoplay and not tonumber(argument) then
            ns.autoplay = nil
            ns.Print(ns.T("mode automatique désactivé."))
        else
            local percent = math.min(100, math.max(0, tonumber(argument) or 90))
            ns.autoplay = percent / 100
            ns.Print(ns.T("mode automatique activé : les morceaux se jouent seuls à environ ") .. percent .. ns.T(" % de justesse."))
        end
        return
    end
    ns.UI.Toggle()
end
