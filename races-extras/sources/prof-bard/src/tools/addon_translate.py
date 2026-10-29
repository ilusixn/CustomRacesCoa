"""Génère addon/Barde/Locale_data.lua (les traductions français -> anglais de l'addon) à partir de tools/addon_translations.py.

Les clés de EN sont les textes français du code de l'addon, tels qu'ils sont écrits entre guillemets dans les fichiers .lua ; ns.T() les remplace
quand la langue est l'anglais. NAMES et FORMS traduisent les noms envoyés par le serveur (airs, instruments, chants) et les formes musicales.

Usage : python addon_translate.py
"""

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from addon_translations import EN, FORMS, NAMES  # noqa: E402

BS = chr(92)


def entry(key, value):
    return '    ["' + key.replace('"', BS + '"') + '"] = "' + value.replace('"', BS + '"') + '",'


lines = ["-- Barde : traductions de l'addon (français -> anglais). Fichier généré par tools/addon_translate.py, ne pas modifier à la main.",
         "-- Les clés sont les textes français du code ; un texte absent de la table s'affiche tel quel.",
         "local ADDON, ns = ...", "", "ns.EN = {"]
lines += [entry(k, v) for k, v in EN.items()]
lines += ["}", "", "ns.NAMES_EN = {"]
lines += [entry(k, v) for k, v in NAMES.items()]
lines += ["}", "", "ns.FORMS_EN = {"]
lines += [entry(k, v) for k, v in FORMS.items()]
lines += ["}", ""]
target = HERE.parent / "addon/Barde/Locale_data.lua"
target.write_text("\n".join(lines), encoding="utf-8", newline="\n")
print(f"{len(EN)} textes, {len(NAMES)} noms, {len(FORMS)} formes écrits dans {target}")
