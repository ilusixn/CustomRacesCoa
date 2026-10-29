"""Génère data/sql/db-world/mod_barde_world_songs.sql à partir de docs/prototype-airs.json.

Le JSON vient du prototype « Carnet du Barde » (tools/export_airs.js) : mêmes notes, mêmes tempos, mêmes
colonnes. La puissance d'un air est son rang dans le carnet : (id - 1) / (nombre d'airs - 1), de 0 pour
le premier à 1 pour le dernier, comme dans le prototype.

Usage : python build_songs_sql.py [airs.json] [sortie.sql]
"""

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def sql_text(text):
    return "'" + text.replace("\\", "\\\\").replace("'", "''") + "'"


def note_token(step, midi, length, _chord, lane):
    return f"{step}:{midi}:{length}:{lane}"


def main():
    source = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "docs/prototype-airs.json"
    target = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "data/sql/db-world/mod_barde_world_songs.sql"
    airs = json.loads(source.read_text(encoding="utf-8"))
    total = len(airs)

    rows = []
    for air in airs:
        power = (air["id"] - 1) / (total - 1)
        notes = " ".join(note_token(*note) for note in air["notes"])
        rows.append(
            f"({air['id']}, {sql_text(air['name'])}, {sql_text(air['subtitle'])}, {air['tier']}, {air['req']}, "
            f"{air['bpm']}, {power:.6f},\n {sql_text(notes)})"
        )

    header = (
        "-- mod-barde v0.2 : les 40 airs du prototype « Carnet du Barde » (docs/carnet-du-barde.html).\n"
        "-- Fichier généré par tools/build_songs_sql.py à partir de docs/prototype-airs.json : ne pas modifier à la main.\n"
        "-- Notes : pas:midi:durée:colonne, pas en doubles croches, durée > 0 pour une note tenue, colonne 1 à 4.\n"
        "-- Greensleeves et les gigues irlandaises sont traditionnelles ; les autres airs sont des compositions originales.\n\n"
        f"DELETE FROM `mod_barde_song` WHERE `id` BETWEEN 1 AND {total};\n"
        "INSERT INTO `mod_barde_song` (`id`, `name`, `subtitle`, `tier`, `skill_req`, `bpm`, `power`, `notes`) VALUES\n"
    )
    target.write_text(header + ",\n".join(rows) + ";\n", encoding="utf-8", newline="\n")
    print(f"{total} airs écrits dans {target} ({target.stat().st_size // 1000} Ko)")


if __name__ == "__main__":
    main()
