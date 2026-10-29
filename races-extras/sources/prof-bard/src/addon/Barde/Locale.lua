-- Barde : la langue de l'addon (français ou anglais).
-- Licence AGPL-3.0, voir LICENSE.
--
-- Les textes du code sont en français ; ns.T() les remplace par leur traduction (Locale_data.lua) quand la langue est l'anglais.
-- Les noms envoyés par le serveur (airs, instruments, chants) sont traduits par ns.N() quand on les connaît.
-- Le choix est gardé dans BardeDB.lang ; le français est la langue par défaut. Changer de langue recharge l'interface.

local ADDON, ns = ...

function ns.GetLang()
    return (BardeDB and BardeDB.lang) or "fr"
end

function ns.IsEnglish()
    return ns.GetLang() == "en"
end

function ns.T(text)
    if ns.GetLang() == "en" then return ns.EN[text] or text end
    return text
end

-- Un nom venu du serveur (air, instrument, chant, effet, palier) : traduit s'il est connu, tel quel sinon.
function ns.N(name)
    if name and ns.GetLang() == "en" then return ns.NAMES_EN[name] or name end
    return name
end

local NOTES = { ["do"] = "C", ["ré"] = "D", ["mi"] = "E", ["fa"] = "F", ["sol"] = "G", ["la"] = "A", ["si"] = "B" }

-- Le sous-titre d'un air : « lai, mode de ré » -> « lay, D mode ».
function ns.Sub(subtitle)
    if not subtitle or ns.GetLang() ~= "en" then return subtitle end
    local form, note, rest = string.match(subtitle, "^(.-), mode de (%S+)%s*(.*)$")
    if form then
        local letter = NOTES[string.lower(note)] or note
        return (ns.FORMS_EN[form] or form) .. ", " .. letter .. (rest == "majeur" and " major" or "") .. " mode"
    end
    local composer = string.match(subtitle, "^composition de (.+)$")
    if composer then return "composition by " .. composer end
    return ns.FORMS_EN[subtitle] or subtitle
end

-- Une liste de textes français lue au moment de l'affichage (la langue n'est connue qu'une fois BardeDB chargée).
function ns.Lazy(source)
    return setmetatable({}, { __index = function(_, key)
        local value = source[key]
        return value and ns.T(value) or nil
    end })
end

-- « Luth » + « d'argent » : « Luth d'argent » en français, « Silver Lute » en anglais.
function ns.TierInstrument(ins, suffix)
    local base = string.gsub(ins.name, " de cuivre$", "")
    if ns.GetLang() == "en" then
        local adjective = ns.NAMES_EN[suffix] or suffix
        return string.upper(string.sub(adjective, 1, 1)) .. string.sub(adjective, 2) .. " " .. (ns.NAMES_EN[base] or base)
    end
    return base .. " " .. suffix
end

function ns.SetLang(lang)
    BardeDB = BardeDB or {}
    BardeDB.lang = lang
    ns.Print(lang == "en" and ns.EN["La langue passe en anglais : l'interface se recharge."] or "La langue passe en français : l'interface se recharge.")
    ReloadUI()
end

function ns.ToggleLang()
    ns.SetLang(ns.IsEnglish() and "fr" or "en")
end
