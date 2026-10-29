"""Installe mod-barde v0.1 dans un serveur CoA du gestionnaire (de préférence une copie).

Usage : python install_barde.py <repack> <worldserver compilé> <mod-barde> <client>

1. arrête le repack ;
2. sauvegarde Core\\worldserver.exe (une seule fois) puis copie le nouveau ;
3. installe mod_barde.conf (sans écraser un fichier déjà modifié) ;
4. ajoute les sorts du barde au Spell.dbc du serveur et écrit le patch client patch-ZO.MPQ ;
5. démarre MySQL, vérifie que l'objet 1950001 est libre, applique le SQL du module ;
6. copie l'addon et le patch dans le client (le jeu doit être fermé) ;
7. démarre le repack.
"""

import filecmp
import shutil
import subprocess
import sys
from pathlib import Path

ITEM_ENTRY = 1950001


def run(*args, stdin=None):
    result = subprocess.run(args, stdin=stdin, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if result.returncode != 0:
        raise RuntimeError(f"échec de {Path(args[0]).name} : {result.stderr.strip() or result.stdout.strip()}")
    return result.stdout


def main() -> None:
    repack, build, module, client = (Path(a).resolve() for a in sys.argv[1:5])
    python = repack / "Runtime/python/python.exe"
    manage = repack / "Scripts/manage.py"
    core = repack / "Core"
    mysql = repack / "mysql/bin/mysql.exe"
    login = f"--defaults-extra-file={repack / 'mysql/admin-client.ini'}"

    for required in (python, manage, build / "worldserver.exe", module / "data/sql", client / "Interface"):
        if not required.exists():
            raise RuntimeError(f"introuvable : {required}")

    print("[1/7] Arrêt du repack...")
    run(str(python), "-B", str(manage), "stop-all")

    print("[2/7] Nouveau worldserver...")
    backup = core / "_sauvegarde_avant_barde"
    if not (backup / "worldserver.exe").exists():
        backup.mkdir(exist_ok=True)
        shutil.copy2(core / "worldserver.exe", backup / "worldserver.exe")
    # Le serveur du gestionnaire est lié autrement que le nôtre : on ajoute seulement les DLL qui lui
    # manquent (boost) et on ne touche jamais à celles qu'il a déjà (OpenSSL, MySQL, zlib, zstd).
    shutil.copy2(build / "worldserver.exe", core / "worldserver.exe")
    for dll in build.glob("*.dll"):
        if not (core / dll.name).exists():
            shutil.copy2(dll, core / dll.name)

    print("[3/7] Configuration...")
    modules = core / "configs/modules"
    modules.mkdir(parents=True, exist_ok=True)
    shutil.copy2(module / "conf/mod_barde.conf.dist", modules / "mod_barde.conf.dist")
    if not (modules / "mod_barde.conf").exists():
        shutil.copy2(module / "conf/mod_barde.conf.dist", modules / "mod_barde.conf")

    print("[4/7] Sorts du barde...")
    patch = Path(__file__).resolve().parent / "patch-ZO.MPQ"
    print("      " + run(str(python), "-I", str(module / "tools/barde_dbc.py"), str(repack / "Data/dbc"), str(patch)).strip())

    print("[5/7] Base de données...")
    run(str(python), "-B", str(manage), "start-mysql")
    taken = run(str(mysql), login, "-N", "-e",
                f"SELECT name FROM acore_world.item_template WHERE entry = {ITEM_ENTRY} AND name <> 'Luth de cuivre'")
    if taken.strip():
        raise RuntimeError(f"l'objet {ITEM_ENTRY} existe déjà ({taken.strip()}) : changez item_entry dans le SQL.")
    for database, folder in (("acore_world", "db-world"), ("acore_characters", "db-characters")):
        for sql in sorted((module / "data/sql" / folder).glob("*.sql")):
            with sql.open("rb") as source:
                run(str(mysql), login, "--default-character-set=utf8mb4", database, stdin=source)
            print(f"      {database} <- {sql.name}")

    print("[6/7] Addon et patch client...")
    target = client / "Interface/AddOns/Barde"
    if target.exists():
        shutil.rmtree(target)
    shutil.copytree(module / "addon/Barde", target)
    installed = client / "Data/patch-ZO.MPQ"
    if installed.exists() and filecmp.cmp(patch, installed, shallow=False):
        print("      patch-ZO.MPQ déjà à jour dans le client.")
    else:
        try:
            shutil.copy2(patch, installed)
        except PermissionError:
            raise RuntimeError("le jeu est ouvert et garde patch-ZO.MPQ : fermez-le puis relancez INSTALL_BARDE.bat.")

    print("[7/7] Démarrage du repack...")
    subprocess.run([str(python), "-B", str(manage), "start-all"])


if __name__ == "__main__":
    try:
        main()
    except Exception as error:  # message lisible dans la fenêtre du .bat
        print(f"ERREUR : {error}")
        sys.exit(1)
