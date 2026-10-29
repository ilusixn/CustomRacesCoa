#!/usr/bin/env python3
import os as _os; COA_ROOT = _os.environ.get("COA_ROOT", ".")  # [release] server tree root
"""[SUPPORT-V2] Forja as magias novas do Paladin Support (2600006-2600043) clonando moldes stock do Spell.dbc.
So GERA o SQL (nao aplica). Familia 2 (livre) para as magias da Support; familia 10 so para os
modificadores das Blessings (mascara limpa Might 0x2 | Wisdom 0x10000, que inclui a Vanguard 2600000)."""
import struct, sys
COLS=[l.split("\t") for l in open("/tmp/spell_dbc_cols.txt").read().strip().splitlines()]
IDX={n:i for i,(n,t) in enumerate(COLS)}
d=open(COA_ROOT+"/env/dist/data/dbc/Spell.dbc","rb").read()
n,nf,rs,ss=struct.unpack("<4I",d[4:20]); assert nf==234
ROWS={}
for r in range(n):
    i=struct.unpack_from("<I",d,20+r*rs)[0]; ROWS[i]=r
def base(sid):
    r=ROWS[sid]; return [struct.unpack_from("<I",d,20+r*rs+i*4)[0] for i in range(nf)]
U=lambda v: v & 0xFFFFFFFF
BLESS=0x2|0x10000                      # Might | Wisdom (+ Vanguard, que tem o bit da Might)
SD,AOR,LB=0x2,0x4,0x8                  # bits da familia 2: Shared Devotion, Aura of Reprisal, Light's Bulwark
def clear_eff(row,e):
    for c in ("Effect","EffectDieSides","EffectRealPointsPerLevel","EffectBasePoints","EffectMechanic","ImplicitTargetA","ImplicitTargetB",
              "EffectRadiusIndex","EffectAura","EffectAuraPeriod","EffectMultipleValue","EffectChainTargets","EffectItemType","EffectMiscValue",
              "EffectMiscValueB","EffectTriggerSpell","EffectPointsPerCombo","EffectSpellClassMaskA","EffectSpellClassMaskB","EffectSpellClassMaskC",
              "EffectChainAmplitude","EffectBonusMultiplier"):
        row[IDX[f"{c}_{e}"]]=0
def eff(row,e,effect,aura=0,misc=0,value=0,tA=1,tB=0,radius=0,trig=0,maskA=0):
    clear_eff(row,e)
    S=lambda c,v: row.__setitem__(IDX[f"{c}_{e}"],U(v))
    S("Effect",effect); S("EffectAura",aura); S("EffectMiscValue",misc); S("EffectBasePoints",value-1); S("EffectDieSides",1)
    S("ImplicitTargetA",tA); S("ImplicitTargetB",tB); S("EffectRadiusIndex",radius); S("EffectTriggerSpell",trig); S("EffectSpellClassMaskA",maskA)
    row[IDX[f"EffectMultipleValue_{e}"]]=struct.unpack("<I",struct.pack("<f",1.0))[0]
    row[IDX[f"EffectBonusMultiplier_{e}"]]=struct.unpack("<I",struct.pack("<f",1.0))[0]
SPELLS=[]
def add(sid,molde,name,desc,level,family=2,mask=0,effects=(),**over):
    row=base(molde)
    row[IDX["ID"]]=sid; row[IDX["SpellClassSet"]]=family
    row[IDX["SpellClassMask_1"]]=mask; row[IDX["SpellClassMask_2"]]=0; row[IDX["SpellClassMask_3"]]=0
    row[IDX["BaseLevel"]]=level; row[IDX["SpellLevel"]]=level; row[IDX["MaxLevel"]]=0
    row[IDX["SpellDescriptionVariableID"]]=0; row[IDX["SpellDifficultyID"]]=0
    for e in (1,2,3): clear_eff(row,e)
    for k,args in enumerate(effects,1): eff(row,k,**args)
    for k,v in over.items(): row[IDX[k]]=U(v)
    SPELLS.append((sid,name,desc,row))
P=dict(effect=6)
# --- ATIVAS (livro, aba Support) ---
add(2600006,6940,"Shared Devotion","Redirects 30% of the damage taken by a friendly target to you for 12 sec.",25,mask=SD,
    effects=[dict(effect=6,aura=81,misc=127,value=30,tA=57)],Category=0,RecoveryTime=120000,CategoryRecoveryTime=0,ProcTypeMask=0,ProcChance=101)
add(2600007,467,"Aura of Reprisal","Party members within 30 yards deal 25 Holy damage to melee attackers that strike them. Lasts 10 sec.",35,mask=AOR,
    effects=[dict(effect=6,aura=15,misc=1,value=25,tA=20,radius=10)],DurationIndex=1,RecoveryTime=180000,RangeIndex=1,ManaCost=0,ManaCostPct=12,SchoolMask=2)
add(2600008,33206,"Light's Bulwark","Reduces all damage taken by a friendly target by 40% for 8 sec. Does not stack with Pain Suppression.",55,mask=LB,
    effects=[dict(effect=6,aura=87,misc=127,value=-40,tA=21),dict(effect=6,aura=235,value=65,tA=21)],Category=0,RecoveryTime=180000,CategoryRecoveryTime=0)
# --- AUTOMATICAS (livro, aba Support) ---
add(2600010,19506,"Vanguard's Presence","Increases the armor of party and raid members within 40 yards by 3%.",25,
    effects=[dict(effect=65,aura=142,misc=1,value=3,radius=23)])
add(2600011,12298,"Oath of Protection","When you block an attack, all party members within 30 yards are healed for 1% of their maximum health. Can only occur once every 6 sec.",45,
    effects=[dict(effect=6,aura=42,trig=2600042)],ProcChance=100)
add(2600042,1277681,"Oath of Protection","Heals party members for 1% of their maximum health.",45,
    effects=[dict(effect=136,value=1,tA=20,radius=10)],RangeIndex=1,ManaCost=0,ManaCostPct=0,RecoveryTime=0)
# --- PASSIVOS DE TALENTO (sem livro) ---
add(2600009,20117,"Beacon Mastery","Beacon of Virtue affects 1 additional ally and lasts 10 sec longer.",45,effects=[dict(effect=6,aura=4,value=1)])
for k,v in enumerate((1,2,3)):
    add(2600012+k,20117,"Vanguard Training","Increases all your attributes by %d%%."%v,10,effects=[dict(effect=6,aura=137,misc=-1,value=v)])
for k,v in enumerate((2,4,6)):
    add(2600015+k,20143,"Hardened Faith","Increases your armor value from items by %d%%."%v,10,effects=[dict(effect=6,aura=142,misc=1,value=v)])
for k,v in enumerate((25,50)):
    add(2600018+k,20101,"Benevolent Blessings","Reduces the mana cost of Blessing of Might, Blessing of Wisdom and Blessing of the Vanguard by %d%%."%v,14,family=10,
        effects=[dict(effect=6,aura=108,misc=14,value=-v,maskA=BLESS)])
for k,v in enumerate((1,2,3)):
    add(2600020+k,53709,"Blessed Resilience","Reduces all damage taken by %d%%."%v,16,effects=[dict(effect=6,aura=87,misc=127,value=-v)],ProcTypeMask=0,ProcChance=101)
for k,v in enumerate((4,8)):
    add(2600023+k,19506,"Sanctified Wellspring","Party and raid members within 40 yards regain %d mana every 5 sec."%v,20,effects=[dict(effect=65,aura=85,misc=0,value=v,radius=23)])
add(2600025,19506,"Shining Example","Increases all damage done by party and raid members within 40 yards by 1%.",22,effects=[dict(effect=65,aura=79,misc=127,value=1,radius=23)])
for k,v in enumerate((10,20,30)):
    add(2600026+k,14521,"Devout Meditation","Allows %d%% of your mana regeneration to continue while casting."%v,28,effects=[dict(effect=6,aura=134,value=v)])
for k,v in enumerate((10,20)):
    add(2600029+k,16757,"Guardian's Reach","Increases the range of Blessing of Might, Blessing of Wisdom and Blessing of the Vanguard by %d%%."%v,30,family=10,
        effects=[dict(effect=6,aura=108,misc=5,value=v,maskA=BLESS)])
for k,v in enumerate((10,20)):
    add(2600031+k,19506,"Warding Auras","Increases the resistance to all magic schools of party and raid members within 40 yards by %d."%v,32,effects=[dict(effect=65,aura=22,misc=126,value=v,radius=23)])
for k,v in enumerate((12,25)):
    add(2600033+k,20117,"Hallowed Blessings","Increases the effect of Blessing of Might, Blessing of Wisdom and Blessing of the Vanguard by %d%%."%v,34,family=10,
        effects=[dict(effect=6,aura=108,misc=8,value=v,maskA=BLESS)])
for k,v in enumerate((2,4,6)):
    add(2600035+k,19506,"Devoted Heart","Increases healing received by party and raid members within 40 yards by %d%%."%v,42,effects=[dict(effect=65,aura=118,misc=127,value=v,radius=23)])
for k,v in enumerate((2,4)):
    add(2600038+k,20060,"Protector's Instinct","Increases your chance to parry and block by %d%%."%v,40,effects=[dict(effect=6,aura=47,value=v),dict(effect=6,aura=51,value=v)],ProcTypeMask=0,ProcChance=101)
for k,v in enumerate((4,8)):
    add(2600040+k,20117,"Crusader's Benediction","Increases your Holy damage and your healing done by %d%%."%v,52,effects=[dict(effect=6,aura=79,misc=2,value=v),dict(effect=6,aura=136,misc=126,value=v)])
add(2600043,20117,"Avatar of the Vanguard","Reduces the cooldown of Shared Devotion and Light's Bulwark by 30 sec and increases the duration of Aura of Reprisal by 5 sec.",60,
    effects=[dict(effect=6,aura=107,misc=11,value=-30000,maskA=SD|LB),dict(effect=6,aura=107,misc=1,value=5000,maskA=AOR)])
# --- SQL ---
ids=[s[0] for s in SPELLS]; assert len(ids)==len(set(ids))==38, (len(ids),len(set(ids)))
assert sorted(ids)==list(range(2600006,2600044)), sorted(ids)
def q(s): return "'"+s.replace("\\","\\\\").replace("'","''")+"'"
out=["-- [SUPPORT-V2] magias do Paladin Support (gerado por forja_support_paladino.py)",
     "DELETE FROM spell_dbc WHERE ID BETWEEN 2600006 AND 2600043;","DELETE FROM coa_client_spell_description WHERE ID BETWEEN 2600006 AND 2600043;"]
for sid,name,desc,row in SPELLS:
    vals=[]
    for i,(c,t) in enumerate(COLS):
        if t=="float": vals.append(repr(struct.unpack("<f",struct.pack("<I",row[i]))[0]))
        elif "char" in t or "text" in t:
            vals.append(q(name) if c=="Name_Lang_enUS" else q(desc) if c=="Description_Lang_enUS" else "''")
        elif t.startswith("int") and not t.endswith("unsigned") and row[i]>=0x80000000: vals.append(str(row[i]-0x100000000))
        else: vals.append(str(row[i]))
    out.append("INSERT INTO spell_dbc ("+",".join("`%s`"%c for c,_ in COLS)+") VALUES ("+",".join(vals)+");")
    out.append("INSERT INTO coa_client_spell_description (ID,Description,ToolTip) VALUES (%d,%s,'');"%(sid,q(desc)))
open(sys.argv[1] if len(sys.argv)>1 else "/tmp/support_v2_spells.sql","w").write("\n".join(out)+"\n")
for sid,name,desc,row in SPELLS:
    e=[(row[IDX["Effect_%d"%k]],row[IDX["EffectAura_%d"%k]]) for k in (1,2,3) if row[IDX["Effect_%d"%k]]]
    print(sid,name.ljust(24),"fam",row[IDX["SpellClassSet"]],"mask %x"%row[IDX["SpellClassMask_1"]],"efeitos",e)
