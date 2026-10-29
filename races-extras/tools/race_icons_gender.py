"""The race button icons drawn by the user (per race / faction / sex) -> BLPs in Interface\\Icons and the Lua table the creation
screen reads (CHAR_CREATE_COA_GENDER_ICONS, keyed by the upper-cased race file name + sex)."""
import os, sys
sys.path[:0] = [r'C:\CoA-Build\logo', r'C:\CoA-Build\kv']
from PIL import Image
import blp, effread

BS = chr(92)
ICON_DIR = r'C:\Users\ilusi\OneDrive\Pictures\character ICON'
FILES = {
    'CENTAURALLIANCE_FEMALE': 'Charactercreate-races_centaur-female_alliance.png',
    'CENTAURHORDE_FEMALE': 'Charactercreate-races_centaur-female_horde.png',
    'CENTAURALLIANCE_MALE': 'Charactercreate-races_centaur-male_alliance.png',
    'CENTAURHORDE_MALE': 'Charactercreate-races_centaur-male_horde.png',
    'DRAKONIDALLIANCE_MALE': 'Charactercreate-races_drakonid-male_alliance.png',
    'DRAKONIDHORDE_MALE': 'Charactercreate-races_drakonid-male_horde.png',
    'FURBOLG_MALE': 'Charactercreate-races_furbolg-male_alliance.png',
    'SLIMEGIANTHORDE_MALE': 'Charactercreate-races_slime-giant_horde.png',
    'SLIMEGIANTHORDE_FEMALE': 'Charactercreate-races_slime-giant_horde.png',
    'KYRIANALLIANCE_MALE': 'kyrian_male.png',
    'KYRIANALLIANCE_FEMALE': 'Kyrian_female_2.png',
    'VENTHYRHORDE_MALE': 'venthyr_male_2.png',
    'VENTHYRHORDE_FEMALE': 'venthyr_female_2.png',
    'VORKAIALLIANCE_MALE': r'C:\CoA-Build\vorkai\icon_m.png',
    'VORKAIALLIANCE_FEMALE': r'C:\CoA-Build\vorkai\icon_f.png',
    'MANTIDHORDE_MALE': r'C:\CoA-Build\mantid\icon.png',
    'MANTIDHORDE_FEMALE': r'C:\CoA-Build\mantid\icon.png',
    'ETHEREALALLIANCE_MALE': r'C:\CoA-Build\ethereal\icon.png',
    'ETHEREALALLIANCE_FEMALE': r'C:\CoA-Build\ethereal\icon.png',
    'SYLVARIALLIANCE_MALE': r'C:\CoA-Build\vorkai\icon_m.png',
    'SYLVARIALLIANCE_FEMALE': r'C:\CoA-Build\vorkai\icon_f.png',
    'MANTIDHORDE_MALE': r'C:\CoA-Build\mantid\icon.png',
    'MANTIDHORDE_FEMALE': r'C:\CoA-Build\mantid\icon.png',
    'ETHEREALALLIANCE_MALE': r'C:\CoA-Build\ethereal\icon.png',
    'ETHEREALALLIANCE_FEMALE': r'C:\CoA-Build\ethereal\icon.png',
}


def build(put):
    like = blp.read(effread.read('Interface/Icons/CoA_Race_Saberon.blp')[0])
    entries = []
    for key, name in FILES.items():
        image = Image.open(os.path.join(ICON_DIR, name)).convert('RGBA').resize((64, 64), Image.LANCZOS)
        put('Interface' + BS + 'Icons' + BS + 'CoA_RaceG_%s.blp' % key, blp.encode(image, like))
        entries.append('["%s"] = "Interface%sIcons%sCoA_RaceG_%s"' % (key, BS * 2, BS * 2, key))
    return '\n-- Local server: race icons per race / faction / sex\nCHAR_CREATE_COA_GENDER_ICONS = {' + ', '.join(entries) + '}\n'
