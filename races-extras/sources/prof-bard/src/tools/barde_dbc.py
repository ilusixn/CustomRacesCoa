"""Ajoute aux bases du jeu les sorts et les objets propres du barde, et construit le patch client qui les montre.

Les instruments sont définis dans tools/instruments.json (un seul endroit : sorts, objets et SQL en sortent).
Le serveur lit son Spell.dbc, le client lit le sien : pour que l'aura s'appelle « Ballade de clarté »,
avec sa propre icône et son propre texte, les deux doivent contenir le sort. De même pour Item.dbc : le
client n'affiche l'icône d'un objet (au lieu d'un « ? ») que s'il figure dans son Item.dbc. Ce script
  1. copie le Spell.dbc et le Item.dbc du serveur en .pre-barde (une seule fois),
  2. y ajoute (ou remplace) les sorts et les objets du barde,
  3. écrit un MPQ client, qui passe après patch-ZN.MPQ et les patchs du client, avec ces deux fichiers.

Usage : python barde_dbc.py <dossier Data/dbc du serveur> <patch-ZO.MPQ à écrire>
"""

import json
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import mpq_build

HERE = Path(__file__).resolve().parent
INSTRUMENTS = json.loads((HERE / "instruments.json").read_text(encoding="utf-8"))

TEMPLATE = 52051                    # une seule aura MOD_POWER_REGEN, sans enfant, déjà essayée
DEBUFF = 0x04000000                 # SPELL_ATTR0_AURA_IS_DEBUFF
HINT = "La puissance dépend de l'air joué et de votre justesse : voir le carnet du barde (/barde)."

F_ATTRIBUTES, F_ICON, F_AREA_GROUP = 4, 133, 224
F_NAME, F_DESCRIPTION, F_AURA_DESCRIPTION = 136, 170, 187
# Tableaux de trois valeurs, un par effet du sort : décalage de l'effet 0 dans la ligne.
F_EFFECT_ARRAYS = [71, 74, 77, 80, 83, 86, 89, 92, 95, 98, 101, 104, 107, 110, 113, 116, 119]
F_EFFECT_CLASS_MASK = 122           # trois mots par effet
F_BASEPOINTS, F_AURA, F_MISC = 80, 95, 110


def spell_specs():
    specs = []
    for ins in INSTRUMENTS:
        specs.append({"id": ins["buff_spell"], "name": ins["chant"], "text": f"{ins['text']} {HINT}",
                      "base": 0, "icon": ins["icon"], "auras": ins["auras"]})
        specs.append({"id": ins["malus_spell"], "name": ins["malus"], "text": ins["malus_text"],
                      "base": -2, "icon": ins["icon"], "auras": ins["auras"], "debuff": True})   # -2 stocké = -1 réel
    return specs


# Le métier, appris au Livre des artisans comme les autres : « Apprenti barde » (1950200) enseigne le sort
# « Barde » (1950201), sans effet, dont la simple présence prouve que le métier est appris.
# Les lignes sont copiées de « Apprentice Alchemist » (2275, enseigne 2259) et de « Blacksmithing » (2018).
PROFESSION_TEACH_TEMPLATE, PROFESSION_TEMPLATE = 2275, 2018
F_RANK = 153
F_EFFECT, F_TRIGGER = 71, 116
PROFESSION = [
    {"id": 1950200, "template": PROFESSION_TEACH_TEMPLATE, "name": "Apprenti barde", "rank": "",
     "text": "Apprend le métier de barde : jouez des airs au luth et aux autres instruments (/barde).", "trigger": 1950201},
    {"id": 1950410, "template": PROFESSION_TEACH_TEMPLATE, "name": "Apprendre un air", "rank": "",
     "text": "Apprend l'air du parchemin.", "trigger": 0},
    {"id": 1950400, "template": PROFESSION_TEACH_TEMPLATE, "name": "Camp de barde", "rank": "",
     "text": "Pose un camp de barde : un feu de camp et deux bancs pendant une demi-heure.", "trigger": 0},
    {"id": 1950201, "template": PROFESSION_TEMPLATE, "name": "Barde", "rank": "Apprenti",
     "text": "Vous connaissez le métier de barde. Ouvrez le carnet avec /barde.", "trigger": 0},
]


def profession_row(dbc, spec, icon):
    row = dbc.row(spec["template"])
    row[0] = spec["id"]
    row[F_ICON] = icon
    # Seul l'effet « apprendre » est gardé ; le second effet (compétence d'un autre métier) et le sort appris
    # d'origine disparaissent, sinon apprendre le barde donnerait de la forge ou de l'alchimie.
    for base in F_EFFECT_ARRAYS:
        for slot in range(1 if spec["trigger"] else 0, 3):
            row[base + slot] = 0
    row[F_TRIGGER] = spec["trigger"]
    if not spec["trigger"]:
        for slot in range(3):
            row[F_EFFECT + slot] = 0
    name, text, rank = dbc.intern(spec["name"]), dbc.intern(spec["text"]), dbc.intern(spec["rank"])
    for slot in range(9):
        row[F_NAME + slot] = name
        row[F_RANK + slot] = rank
        row[F_DESCRIPTION + slot] = text
        row[F_AURA_DESCRIPTION + slot] = text
    return row


# Auras sans effet, cachées de la barre d'effets, qui ne portent qu'un effet visuel : elles entourent le barde pendant
# qu'il joue. Sept apparences à essayer (`.barde aura <1 à 7>`), prises sur des sorts qui existent déjà :
# 1 Hymn Visual Dummy / Music Bash (995501), 2 Dance Trance (7665), 3 Dance Vibe (7666), 4 Hymn of Hope (12858),
# 5 Music Box (353), 6 Dance! (10437), 7 Ballad of the Conqueror (1478).
F_SPELL_VISUAL = 131
HIDDEN = 0x80                       # SPELL_ATTR0_DO_NOT_DISPLAY
PLAY_VISUALS = [995501, 7665, 7666, 12858, 353, 10437, 1478]
PLAY_AURAS = [
    {"id": 1950301 + i, "name": "Joue de la musique", "text": "Un barde joue.", "base": 0,
     "icon": INSTRUMENTS[0]["icon"], "auras": [[4, 0]], "visual": visual}
    for i, visual in enumerate(PLAY_VISUALS)
]


def play_aura_row(dbc, template, spec):
    row = make_row(dbc, template, spec)
    row[F_SPELL_VISUAL] = spec["visual"]
    row[F_SPELL_VISUAL + 1] = 0
    row[F_ATTRIBUTES] |= HIDDEN
    return row


# Animations : le client range l'arme dès qu'une animation porte l'indicateur « range l'arme » (16) dans AnimationData.dbc,
# ce qui retire l'instrument de la main du barde. Les animations de danse (69 : danse, 211 : danse spéciale) l'ont ;
# on les met à 0, comme les animations « sans ranger l'arme » du jeu. Seul le client lit ce fichier.
F_ANIM_WEAPON_FLAGS = 2
DANCE_ANIMS = [69, 211]


def keep_weapon_dances(dbc_dir):
    anims = SpellDbc(dbc_dir / "AnimationData.dbc")         # même structure : une ligne par numéro
    for anim_id in DANCE_ANIMS:
        row = anims.row(anim_id)
        row[F_ANIM_WEAPON_FLAGS] = 0
        anims.put(row)
    return anims.to_bytes()


# Qualité d'un instrument : une « amélioration permanente » sans effet, posée sur l'objet à la fabrication. Le jeu en affiche le
# nom dans l'infobulle ; le module en déduit le multiplicateur du chant (voir BardeAtelier.cpp). Numéros : 1950501 (inhabituel)
# à 1950504 (légendaire) ; un instrument commun n'en a pas. Le serveur et le client lisent SpellItemEnchantment.dbc.
ENCHANT_TEMPLATE = 1503             # « +100 Health » : la ligne sert de modèle, ses effets sont retirés
ENCHANTS = [
    {"id": 1950501, "name": "Instrument inhabituel (+5 % au chant)"},
    {"id": 1950502, "name": "Instrument rare (+10 % au chant)"},
    {"id": 1950503, "name": "Instrument épique (+15 % au chant)"},
    {"id": 1950504, "name": "Instrument légendaire (+20 % au chant, ne se désaccorde jamais)"},
]
F_ENCH_EFFECT, F_ENCH_NAME = 2, 14      # trois effets, puis trois valeurs mini, maxi et arguments, puis seize noms (un par langue)


def enchant_row(dbc, template, spec):
    row = list(template)
    row[0] = spec["id"]
    row[1] = 0                                  # pas de charges
    for index in range(2, 14):                  # effets, valeurs et arguments : aucun
        row[index] = 0
    name = dbc.intern(spec["name"])
    for slot in range(16):
        row[F_ENCH_NAME + slot] = name
    for index in range(31, 38):                 # visuel, drapeaux, objet source, condition, compétence, niveau
        row[index] = 0
    return row


# Objets du barde (mêmes numéros que item_template). Classe 15 « divers », sous-classe 4 « autre ».
ITEM_TEMPLATE = 262631              # « Mandolin » du client : sert de modèle de ligne
F_ITEM_CLASS, F_ITEM_SUBCLASS, F_ITEM_DISPLAY, F_ITEM_INVENTORY, F_ITEM_SHEATH = 1, 2, 5, 6, 7
TIERS = json.loads((HERE / "tiers.json").read_text(encoding="utf-8"))
# Un objet par instrument et par palier : le numéro de l'objet de cuivre, plus 1000 par palier.
ITEMS = [{"id": ins["item"] + 1000 * tier["tier"], "display": ins["display"]} for ins in INSTRUMENTS for tier in TIERS]
ITEMS.append({"id": 1950020, "display": 10249})
ITEMS.append({"id": 1950040, "display": 218})           # le parchemin d'air
ITEMS.append({"id": 1950030, "display": 173})            # le camp de barde (modèle du « Bundle of Wood »)         # la clé d'accordage (modèle de la « Workshop Key »)
SPELLS = spell_specs()

# Les titres : des lignes « ReuseNN » de CharTitles.dbc que le jeu laisse libres (BardeTitles.cpp donne les mêmes numéros).
TITLES = [(226, "%s le Ménestrel"), (227, "%s le Compositeur"), (231, "%s, Voix d'or"), (232, "%s, Roi de la taverne"),
          (233, "%s le Virtuose"), (234, "%s l'Idole"), (235, "%s, Barde légendaire")]


class SpellDbc:
    def __init__(self, path):
        data = Path(path).read_bytes()
        magic, self.count, self.fields, self.size, strings = struct.unpack("<4s4I", data[:20])
        assert magic == b"WDBC" and self.size == self.fields * 4
        end = 20 + self.count * self.size
        self.body = bytearray(data[20:end])
        self.strings = bytearray(data[end:end + strings])
        self.index = {}
        for i in range(self.count):
            self.index[struct.unpack_from("<I", self.body, i * self.size)[0]] = i
        self.added = {}

    def intern(self, text):
        if text not in self.added:
            self.added[text] = len(self.strings)
            self.strings += text.encode("utf-8") + b"\0"
        return self.added[text]

    def row(self, spell_id):
        return list(struct.unpack_from(f"<{self.fields}I", self.body, self.index[spell_id] * self.size))

    def put(self, row):
        packed = struct.pack(f"<{self.fields}I", *row)
        spell_id = row[0]
        if spell_id in self.index:
            i = self.index[spell_id]
            self.body[i * self.size:(i + 1) * self.size] = packed
        else:
            self.index[spell_id] = self.count
            self.count += 1
            self.body += packed

    def to_bytes(self):
        header = struct.pack("<4s4I", b"WDBC", self.count, self.fields, self.size, len(self.strings))
        return header + bytes(self.body) + bytes(self.strings)


def make_row(dbc, template, spec):
    row = list(template)
    row[0] = spec["id"]
    row[F_ICON] = spec["icon"]
    row[F_AREA_GROUP] = 0                       # le modèle est limité à Zul'Drak : le barde joue partout
    if spec.get("debuff"):
        row[F_ATTRIBUTES] |= DEBUFF
    auras = spec["auras"]
    if len(auras) > 1:                          # un second effet : copie du premier, puis on change son aura
        for base in F_EFFECT_ARRAYS:
            row[base + 1] = row[base]
        row[F_EFFECT_CLASS_MASK + 3:F_EFFECT_CLASS_MASK + 6] = row[F_EFFECT_CLASS_MASK:F_EFFECT_CLASS_MASK + 3]
    for index, (aura, misc) in enumerate(auras):
        row[F_BASEPOINTS + index] = spec["base"] & 0xFFFFFFFF
        row[F_AURA + index] = aura
        row[F_MISC + index] = misc & 0xFFFFFFFF
    name, text = dbc.intern(spec["name"]), dbc.intern(spec["text"])
    for slot in range(9):                       # les neuf langues du client pointent vers le même texte
        row[F_NAME + slot] = name
        row[F_DESCRIPTION + slot] = text
        row[F_AURA_DESCRIPTION + slot] = text
    return row


def load_base(dbc_dir, name, ids):
    """Charge dbc_dir/name, ou sa sauvegarde .pre-barde si nos lignes y sont déjà (pour rejouer sans doublon)."""
    path = dbc_dir / name
    backup = dbc_dir / (name + ".pre-barde")
    dbc = SpellDbc(path)
    taken = [i for i in ids if i in dbc.index]
    if taken:
        if not backup.exists():
            raise SystemExit(f"Le numéro {taken[0]} existe déjà dans {name} : changez-le dans barde_dbc.py.")
        dbc = SpellDbc(backup)
    else:
        backup.write_bytes(path.read_bytes())           # état actuel, avant nos lignes
    return path, dbc


def main(dbc_dir, client_mpq):
    dbc_dir, client_mpq = Path(dbc_dir), Path(client_mpq)

    spell_path, spells = load_base(dbc_dir, "Spell.dbc", [spec["id"] for spec in SPELLS + PROFESSION + PLAY_AURAS])
    template = spells.row(TEMPLATE)
    for spec in SPELLS:
        spells.put(make_row(spells, template, spec))
    for spec in PROFESSION:
        spells.put(profession_row(spells, spec, INSTRUMENTS[0]["icon"]))
    for spec in PLAY_AURAS:
        spells.put(play_aura_row(spells, template, spec))

    item_path, items = load_base(dbc_dir, "Item.dbc", [item["id"] for item in ITEMS])
    item_template = items.row(ITEM_TEMPLATE)
    for item in ITEMS:
        row = list(item_template)
        row[0] = item["id"]
        row[F_ITEM_CLASS], row[F_ITEM_SUBCLASS] = 15, 4
        row[F_ITEM_DISPLAY], row[F_ITEM_INVENTORY], row[F_ITEM_SHEATH] = item["display"], 0, 0
        items.put(row)

    spell_bytes, item_bytes = spells.to_bytes(), items.to_bytes()
    spell_path.write_bytes(spell_bytes)
    item_path.write_bytes(item_bytes)
    ench_path, enchants = load_base(dbc_dir, "SpellItemEnchantment.dbc", [spec["id"] for spec in ENCHANTS])
    ench_template = enchants.row(ENCHANT_TEMPLATE)
    for spec in ENCHANTS:
        enchants.put(enchant_row(enchants, ench_template, spec))
    ench_bytes = enchants.to_bytes()
    ench_path.write_bytes(ench_bytes)
    # Les lignes existent déjà (« ReuseNN ») : on les réécrit, toujours à partir de la sauvegarde.
    title_path, title_backup = dbc_dir / "CharTitles.dbc", dbc_dir / "CharTitles.dbc.pre-barde"
    if not title_backup.exists():
        title_backup.write_bytes(title_path.read_bytes())
    titles = SpellDbc(title_backup)
    for number, text in TITLES:
        row = titles.row(number)
        offset = titles.intern(text)
        for slot in range(16):                  # les deux noms (homme, femme), dans toutes les langues
            row[2 + slot] = offset
            row[19 + slot] = offset
        titles.put(row)
    title_bytes = titles.to_bytes()
    title_path.write_bytes(title_bytes)
    anim_bytes = keep_weapon_dances(dbc_dir)
    client_mpq.write_bytes(mpq_build.build([("DBFilesClient" + chr(92) + "Spell.dbc", spell_bytes),
                                            ("DBFilesClient" + chr(92) + "Item.dbc", item_bytes),
                                            ("DBFilesClient" + chr(92) + "SpellItemEnchantment.dbc", ench_bytes),
                                            ("DBFilesClient" + chr(92) + "AnimationData.dbc", anim_bytes),
                                            ("DBFilesClient" + chr(92) + "CharTitles.dbc", title_bytes)]))
    print(f"{len(SPELLS) + len(PROFESSION) + len(PLAY_AURAS)} sorts ({', '.join(str(s['id']) for s in SPELLS + PROFESSION + PLAY_AURAS)}) et {len(ITEMS)} objets "
          f"({', '.join(str(i['id']) for i in ITEMS)}) ajoutés : Spell.dbc et Item.dbc du serveur, "
          f"{client_mpq.name} ({client_mpq.stat().st_size // 1_000_000} Mo)")


if __name__ == "__main__":
    main(*sys.argv[1:3])
