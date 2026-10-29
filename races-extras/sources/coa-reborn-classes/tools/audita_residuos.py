#!/usr/bin/env python3
import os as _os; COA_ROOT = _os.environ.get("COA_ROOT", ".")  # [release] server tree root
"""SOMENTE LEITURA. Relata, para cada personagem de classe classica (1-11) do CoA 60, magias do livro que pertencem a
entradas de talento (custo TE/AE) de uma spec diferente da ativa — residuos que fazem o Save Changes ser recusado
("entry of another specialization"). Nao altera nada. Uso: audita_residuos.py [dir_dbc]"""
import struct, subprocess, sys, collections
DBC=sys.argv[1] if len(sys.argv)>1 else COA_ROOT+"/env/dist/data/dbc"
def load(p):
    b=open(p,"rb").read(); m,n,f,rs,ss=struct.unpack("<4sIIII",b[:20]); return [b[20+i*rs:20+(i+1)*rs] for i in range(n)], b[20+n*rs:]
u=lambda r,i: struct.unpack_from("<I",r,i*4)[0]; FL=lambda r: struct.unpack_from("<I",r,479)[0]
def q(db,sql): return [l.split("\t") for l in subprocess.run(["docker","exec",_os.environ.get("DB_CONTAINER", "ac-database"),"mysql","-u"+_os.environ.get("DB_USER", "root"),"-p"+_os.environ.get("DB_PASS", ""),db,"-N","-e",sql],capture_output=True,text=True).stdout.splitlines()]
ca,ss=load(f"{DBC}/CharacterAdvancement.dbc"); cs,css=load(f"{DBC}/ChrSpecs.dbc"); ct,cts=load(f"{DBC}/CharacterAdvancementClassTypes.dbc")
def S(strs,r,i):
    o=u(r,i); return strs[o:strs.find(b"\x00",o)].decode("utf8","replace")
classtype_of={u(r,0):u(r,2) for r in ct}                      # ClassType -> classe (1-11 para stock)
specs={u(r,0):(S(css,r,1),S(css,r,29)) for r in cs}           # spec id -> (CLASSE, nome)
tabspec={}                                                    # (classe, aba) -> spec id (pelo nome da aba = nome da spec)
tt,tts=load(f"{DBC}/CharacterAdvancementTabTypes.dbc"); tabname={u(r,0):S(tts,r,1) for r in tt}
classname={1:"WARRIOR",2:"PALADIN",3:"HUNTER",4:"ROGUE",5:"PRIEST",6:"DEATHKNIGHT",7:"SHAMAN",8:"MAGE",9:"WARLOCK",11:"DRUID"}
for sid,(cl,nm) in specs.items():
    for tab,tn in tabname.items():
        if tn==nm: tabspec[(cl,tab)]=sid
spell_entries=collections.defaultdict(list)                    # spell -> [(classe, spec, entrada, nome)]
for r in ca:
    if FL(r)&0x9: continue
    cl=classtype_of.get(u(r,32))
    if cl not in classname: continue
    paid=bool(u(r,14) or u(r,15)); kind="talento" if paid else ("identidade" if FL(r)&0x40000 else "automatica")
    sid=tabspec.get((classname[cl],u(r,33)),0)
    if not sid: continue
    for i in range(5,8):
        if u(r,i): spell_entries[u(r,i)].append((cl,sid,u(r,0),S(ss,r,47)+" ["+kind+"]"))
chars=q("acore_characters","select guid,name,class,level,online from characters where class in (1,2,3,4,5,6,7,8,9,11) and deleteDate is null")
print(f"personagens de classe classica: {len(chars)} | magias de talento de spec mapeadas: {len(spell_entries)}")
total=0
for guid,name,cl,level,online in chars:
    cl=int(cl); act=q("acore_characters",f"select data from character_settings where guid={guid} and source='core.ascension_active_spec'")
    active=int(act[0][0].split()[0]) if act and act[0][0].strip() else 0
    rows=q("acore_characters",f"select spell from character_spell where guid={guid} and specMask<>0")
    res=[]
    for (sp,) in rows:
        for c,sid,eid,nm in spell_entries.get(int(sp),[]):
            if c==cl and sid!=active: res.append((int(sp),eid,nm,specs[sid][1]))
    if res:
        total+=1; print(f"- {name} (guid {guid}, classe {cl}, nivel {level}, spec ativa {active} {specs.get(active,('','-'))[1]}, online {online}): {len(res)} residuo(s)")
        for sp,eid,nm,sn in res: print(f"     magia {sp} = entrada {eid} '{nm}' da spec {sn}")
print(f"RESULTADO: {total} personagem(ns) com residuos" if total else "RESULTADO: nenhum residuo")
