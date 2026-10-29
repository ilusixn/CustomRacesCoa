"""Génère data/sql/db-world/mod_barde_world_instruments.sql à partir de tools/instruments.json et tools/tiers.json.

Une ligne par instrument dans mod_barde_instrument ; pour chaque instrument et chaque palier (cuivre, argent, mithril,
thorium, arcanite), un objet dans item_template et une ligne dans mod_barde_instrument_item ; les paliers et leurs
matériaux dans mod_barde_tier et mod_barde_tier_material.
Les sorts (Spell.dbc) et les objets du client (Item.dbc) sortent des mêmes fichiers par tools/barde_dbc.py.

Numéros d'objet : celui de l'instrument de cuivre (1950001 à 1950010), plus 1000 par palier (1951001 pour l'argent, etc.).

Usage : python build_instruments_sql.py [instruments.json] [sortie.sql]
"""

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
COPPER_SUFFIX = " de cuivre"


def sql_text(text):
    return "'" + text.replace("\\", "\\\\").replace("'", "''") + "'"


def main():
    source = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "tools/instruments.json"
    target = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "data/sql/db-world/mod_barde_world_instruments.sql"
    instruments = json.loads(source.read_text(encoding="utf-8"))
    tiers = json.loads((ROOT / "tools/tiers.json").read_text(encoding="utf-8"))

    rows, items, links = [], [], []
    for ins in instruments:
        auras = ins["auras"]
        second = auras[1][0] if len(auras) > 1 else 0
        rows.append(
            f"({ins['id']}, {sql_text(ins['name'])}, {sql_text(ins['key'])}, 0, {ins['item']}, {sql_text(ins['chant'])}, "
            f"{sql_text(ins['effect'])}, {sql_text(ins['unit'])}, {ins['buff_spell']}, {auras[0][0]}, {second}, "
            f"{ins['min']}, {ins['max']}, {sql_text(ins['malus'])}, {ins['malus_spell']})"
        )
        base = ins["name"][:-len(COPPER_SUFFIX)] if ins["name"].endswith(COPPER_SUFFIX) else ins["name"]
        for tier in tiers:
            entry = ins["item"] + 1000 * tier["tier"]
            name = f"{base} {tier['suffix']}"
            items.append(
                f"({entry}, 15, 4, {sql_text(name)}, {ins['display']}, {tier['quality']}, 0, 0, 0, {tier['item_level']}, 1, 1, 1, 0, "
                f"'Instrument de barde. Tapez /barde pour jouer.', 12340, 100)"
            )
            links.append(f"({entry}, {ins['id']}, {tier['tier']})")

    items.append(
        "(1950020, 15, 0, 'Clé d''accordage', 10249, 1, 0, 0, 0, 1, 1, 20, 0, 0, "
        "'Remet un instrument d''accord à 100 %. À utiliser depuis l''onglet Accordage de l''atelier du carnet.', 12340, 0)"
    )

    tier_rows = [f"({t['tier']}, {sql_text(t['suffix'])}, {t['skill']})" for t in tiers]
    material_rows = [f"({t['tier']}, {item}, {count})" for t in tiers for item, count in t["mats"]]

    gem_rows = [f"({t['tier']}, {t['gem']})" for t in tiers]

    ids = ", ".join(str(i["id"]) for i in instruments)
    entries = ", ".join(str(i["item"] + 1000 * t["tier"]) for i in instruments for t in tiers) + ", 1950020"
    text = (
        "-- mod-barde : les dix instruments, leurs cinq paliers (cuivre à arcanite) et leurs objets.\n"
        "-- Fichier généré par tools/build_instruments_sql.py à partir de tools/instruments.json et tools/tiers.json : ne pas modifier à la main.\n"
        "-- Les sorts de chant et de malus sont ajoutés au Spell.dbc du serveur et du client par tools/barde_dbc.py.\n"
        "-- Valeurs de buff et repères de comparaison : docs/valeurs-des-buffs.md.\n\n"
        f"DELETE FROM `mod_barde_instrument` WHERE `id` IN ({ids});\n"
        "INSERT INTO `mod_barde_instrument` (`id`, `name`, `sound`, `tier`, `item_entry`, `buff_name`, `effect`, `unit`, "
        "`buff_spell`, `buff_aura`, `buff_aura2`, `min_solo`, `max_solo`, `malus_name`, `malus_spell`) VALUES\n"
        + ",\n".join(rows) + ";\n\n"
        "-- Atelier : un palier donne un objet par instrument ; fabriquer demande la compétence indiquée et les matériaux\n"
        "-- (aucun matériau : le formateur l'offre).\n"
        "CREATE TABLE IF NOT EXISTS `mod_barde_tier` (\n"
        "  `tier`      TINYINT UNSIGNED  NOT NULL,\n"
        "  `suffix`    VARCHAR(24)       NOT NULL COMMENT 'de cuivre, d''argent...',\n"
        "  `skill_req` SMALLINT UNSIGNED NOT NULL COMMENT 'compétence de barde pour le fabriquer',\n"
        "  PRIMARY KEY (`tier`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;\n"
        "CREATE TABLE IF NOT EXISTS `mod_barde_tier_material` (\n"
        "  `tier`       TINYINT UNSIGNED NOT NULL,\n"
        "  `item_entry` INT UNSIGNED     NOT NULL,\n"
        "  `count`      TINYINT UNSIGNED NOT NULL,\n"
        "  PRIMARY KEY (`tier`, `item_entry`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;\n"
        "CREATE TABLE IF NOT EXISTS `mod_barde_tier_gem` (\n"
        "  `tier`      TINYINT UNSIGNED NOT NULL,\n"
        "  `gem_entry` INT UNSIGNED     NOT NULL COMMENT 'la gemme de qualité de ce palier (facultative à la fabrication)',\n"
        "  PRIMARY KEY (`tier`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;\n"
        "CREATE TABLE IF NOT EXISTS `mod_barde_instrument_item` (\n"
        "  `item_entry`    INT UNSIGNED     NOT NULL,\n"
        "  `instrument_id` INT UNSIGNED     NOT NULL,\n"
        "  `tier`          TINYINT UNSIGNED NOT NULL,\n"
        "  PRIMARY KEY (`item_entry`)\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;\n\n"
        "DELETE FROM `mod_barde_tier`;\n"
        "INSERT INTO `mod_barde_tier` (`tier`, `suffix`, `skill_req`) VALUES\n" + ",\n".join(tier_rows) + ";\n"
        "DELETE FROM `mod_barde_tier_material`;\n"
        "INSERT INTO `mod_barde_tier_material` (`tier`, `item_entry`, `count`) VALUES\n" + ",\n".join(material_rows) + ";\n"
        "DELETE FROM `mod_barde_tier_gem`;\n"
        "INSERT INTO `mod_barde_tier_gem` (`tier`, `gem_entry`) VALUES\n" + ",\n".join(gem_rows) + ";\n"
        "DELETE FROM `mod_barde_instrument_item`;\n"
        "INSERT INTO `mod_barde_instrument_item` (`item_entry`, `instrument_id`, `tier`) VALUES\n" + ",\n".join(links) + ";\n\n"
        "-- Apparences prises dans le client (modèle et icône déjà présents) : luth 142067, guitare 142068, flûte 142021,\n"
        "-- cor de chasse 142026, banjo 142069, guitare des morts-vivants 67653, cor du Chasseur de dragons 142028, flûte de pan 142020,\n"
        "-- tambour 142035, grand tambour 142036. Un seul exemplaire par personnage et par palier.\n"
        f"DELETE FROM `item_template` WHERE `entry` IN ({entries});\n"
        "INSERT INTO `item_template` (`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `BuyPrice`, `SellPrice`, "
        "`InventoryType`, `ItemLevel`, `RequiredLevel`, `stackable`, `maxcount`, `bonding`, `description`, `VerifiedBuild`, `MaxDurability`) VALUES\n"
        + ",\n".join(items) + ";\n"
    )
    target.write_text(text, encoding="utf-8", newline="\n")
    print(f"{len(instruments)} instruments, {len(tiers)} paliers, {len(items)} objets écrits dans {target}")


if __name__ == "__main__":
    main()
