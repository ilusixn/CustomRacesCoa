import os as _os; COA_ROOT = _os.environ.get("COA_ROOT", ".")  # [release] server tree root
# Auditoria de magias duplicadas no Character Advancement (CoA 60) - somente leitura
import struct, collections, subprocess, json, sys
D=(sys.argv[1].rstrip("/")+"/") if len(sys.argv)>1 and not sys.argv[1].startswith("--") else COA_ROOT+"/env/dist/data/dbc/"
def load(f):
    d=open(D+f,"rb").read(); n,nf,rs,ss=struct.unpack("<4I",d[4:20]); sb=20+n*rs
    def S(o): return d[sb+o:d.index(b"\0",sb+o)].decode("utf8","replace") if 0<=o<ss else ""
    return d,n,nf,rs,sb,S
# ChrClasses tokens
d,n,nf,rs,sb,S=load("ChrClasses.dbc"); tok={}
for r in range(n):
    v=struct.unpack_from("<%dI"%nf,d,20+r*rs); tok[v[0]]=S(v[55])
d,n,nf,rs,sb,S=load("CharacterAdvancementClassTypes.dbc"); ctype={}
for r in range(n):
    v=struct.unpack_from("<%dI"%nf,d,20+r*rs); ctype[v[0]]=(v[2],v[5]!=0,v[4]!=0,S(v[1]))
d,n,nf,rs,sb,S=load("CharacterAdvancementTabTypes.dbc"); tabtok={}
for r in range(n):
    v=struct.unpack_from("<%dI"%nf,d,20+r*rs); tabtok[v[0]]=S(v[1])
d,n,nf,rs,sb,S=load("ChrSpecs.dbc"); spec={}; ident={}; signature={}
for r in range(n):
    v=struct.unpack_from("<%dI"%nf,d,20+r*rs)
    spec[(S(v[1]),S(v[2]))]=v[0]
    if v[28]: ident[v[28]]=v[0]
    signature[v[0]]=v[24]
specname={v:k[0]+"/"+k[1] for k,v in spec.items()}
# Spell names
d,n,nf,rs,sb,S=load("Spell.dbc"); sname={}; learns={}
for r in range(n):
    v=struct.unpack_from("<%dI"%nf,d,20+r*rs)
    sname[v[0]]=S(v[136])
    for e in range(3):
        if v[71+e]==36 and v[113+e]: learns.setdefault(v[0],[]).append(v[113+e])
def nm(s): return "%s(%d)"%(sname.get(s,"?"),s)
# CA entries, loader rules
d,n,nf,rs,sb,S=load("CharacterAdvancement.dbc")
E={}
for r in range(n):
    o=20+r*rs; v=struct.unpack_from("<74I",d,o)
    ct=ctype.get(v[32])
    if not ct: continue
    cid,stock,custom,ctname=ct
    if not ((custom and 12<=cid<=32) or (stock and 1<=cid<=11 and cid!=10)): continue
    fl=struct.unpack_from("<I",d,o+479)[0]
    if fl&9: continue
    tab=v[33]; sp=0
    if tab!=87:
        sp=spec.get((tok.get(cid,""),tabtok.get(tab,"").upper()))
        if not sp: continue
    spells=[x for x in v[5:10] if x]
    E[v[0]]=dict(id=v[0],cls=cid,spec=sp,tab=tab,ae=v[14],te=v[15],lvl=v[26],name=S(v[47]),spells=spells,flags=fl,
                 paid=bool(v[14] or v[15]),ident=ident.get(v[0])==sp and sp!=0, ctname=ctname)
def kind(e):
    if e["ident"]: return "IDENT"
    if e["paid"]: return ("PAGO-CLASSE" if e["tab"]==87 else "PAGO-SPEC")
    return ("AUTO-CLASSE" if e["tab"]==87 else "AUTO-SPEC")
def where(e): return "%d[%s %s %s]"%(e["id"],kind(e),"classe" if e["tab"]==87 else specname.get(e["spec"],e["spec"]),e["name"])
# trainer / create spells per class
def q(sql):
    out=subprocess.run(["docker","exec",_os.environ.get("DB_CONTAINER", "ac-database"),"mysql","-u"+_os.environ.get("DB_USER", "root"),"-p"+_os.environ.get("DB_PASS", ""),"-N","-e",sql],capture_output=True,text=True).stdout
    return [l.split("\t") for l in out.strip().split("\n") if l]
trainer=collections.defaultdict(set)
for cls,s in q("select t.Requirement, ts.SpellId from acore_world.trainer t join acore_world.trainer_spell ts on ts.TrainerId=t.Id where t.Type=0"):
    s=int(s); trainer[int(cls)].add(s)
    for x in learns.get(s,[]): trainer[int(cls)].add(x)
create=collections.defaultdict(set)
for tbl in ("playercreateinfo_spell_custom","playercreateinfo_skills"):
    pass
for cm,s in q("select classmask, spell from acore_world.playercreateinfo_spell_custom"):
    cm=int(cm)
    for c in range(1,33):
        if cm==0 or cm&(1<<(c-1)): create[c].add(int(s))
# 1) same spell in several entries of the same class
byspell=collections.defaultdict(list)
for e in E.values():
    for i,s in enumerate(e["spells"]): byspell[(e["cls"],s)].append((e,i+1))
report=collections.defaultdict(list)
for (c,s),lst in byspell.items():
    ids={e["id"] for e,_ in lst}
    if len(ids)<2: continue
    kinds={kind(e) for e,_ in lst}; specs={e["spec"] for e,_ in lst}
    paid=[e for e,_ in lst if e["paid"]]
    if len(paid)>=2 and len({e["spec"] for e in paid})>1: cat="P1 pago x pago em specs diferentes (mistura)"
    elif paid and any(not e["paid"] for e,_ in lst): cat="P2 pago x automatico/identidade (servidor ve o pago como investido)"
    elif len(paid)>=2: cat="P3 pago x pago na mesma spec"
    elif len(specs)>1: cat="L1 automatico em varias specs/classe (compartilhada)"
    else: cat="L2 automatico repetido na mesma spec"
    report[cat].append((c,s,lst))
# 2) paid spells obtainable outside the tree (trainer / creation)
outside=[]
for e in E.values():
    if not e["paid"]: continue
    for i,s in enumerate(e["spells"]):
        src=[]
        if s in trainer[e["cls"]]: src.append("treinador")
        if s in create[e["cls"]]: src.append("criacao")
        if src: outside.append((e,i+1,s,src))
# 3) identity spells that are class spells elsewhere
idclash=[]
for e in E.values():
    if not e["ident"]: continue
    for s in e["spells"]:
        others=[o for o,_ in byspell[(e["cls"],s)] if o["id"]!=e["id"]]
        src=(["treinador"] if s in trainer[e["cls"]] else [])+(["criacao"] if s in create[e["cls"]] else [])
        if others or src: idclash.append((e,s,others,src))
out=[]
P=out.append
P("# AUDITORIA DE MAGIAS DUPLICADAS - %d entradas ativas"%len(E))
stockc={1,2,3,4,5,7,8,9,11}
for cat in sorted(report):
    lst=report[cat]
    P("\n## %s : %d casos (classicas %d / CoA %d)"%(cat,len(lst),sum(1 for c,_,_ in lst if c in stockc),sum(1 for c,_,_ in lst if c not in stockc)))
    for c,s,l in sorted(lst,key=lambda x:(x[0] not in stockc,x[0],x[1])):
        P("- %s %s: %s"%(tok.get(c,c),nm(s),"; ".join("%s r%d"%(where(e),r) for e,r in l)))
P("\n## P4 magias de talento PAGO que tambem vem de treinador/criacao: %d"%len(outside))
for e,r,s,src in sorted(outside,key=lambda x:(x[0]["cls"] not in stockc,x[0]["cls"])):
    P("- %s %s r%d em %s <- %s"%(tok.get(e["cls"]),nm(s),r,where(e),"+".join(src)))
P("\n## P5 identidades cuja magia existe em outro lugar: %d"%len(idclash))
for e,s,o,src in idclash:
    P("- %s %s em %s; tambem: %s %s"%(tok.get(e["cls"]),nm(s),where(e),", ".join(where(x) for x in o),"+".join(src)))
open("/tmp/auditoria-duplicadas.md","w").write("\n".join(out))
print("\n".join(l for l in out if l.startswith("#")))

# [ESTABILIZA-1210] trava: regeracao so passa sem os conflitos que ja causaram mistura de specs.
# P1 nas classes do CoA (Witch Doctor, original) e tratado pelo servidor (SharedPaidEntries) e fica de fora.
if "--trava" in sys.argv:
    falhas = sum(1 for c,_,_ in report.get("P1 pago x pago em specs diferentes (mistura)",[]) if c in stockc)
    falhas += len(report.get("P2 pago x automatico/identidade (servidor ve o pago como investido)",[]))
    falhas += len(outside) + len(idclash)
    print("TRAVA DUPLICADAS:", "OK" if not falhas else "FALHOU (%d conflitos) - ver /tmp/auditoria-duplicadas.md" % falhas)
    sys.exit(1 if falhas else 0)
