#!/usr/bin/env python3
import os as _os; COA_ROOT = _os.environ.get("COA_ROOT", ".")  # [release] server tree root
"""Fase 2 (dados): Paladino (classe 2, ClassType 44 'RebornPaladin') no sistema de talentos do CoA.
Clona o layout (posições/conexões) das árvores da Venomancer (ClassType 33) e troca as magias pelas do Paladino Reborn.
Uso: gera_paladino.py <dir_dbc_origem> <dir_saida> [--apply]  (sem --apply só relata)."""
import struct, sys, os, collections
SRC, OUT = sys.argv[1], sys.argv[2]; APPLY = "--apply" in sys.argv
os.makedirs(OUT, exist_ok=True)
def load(name):
    b=open(os.path.join(SRC,name),"rb").read(); m,n,f,rs,ss=struct.unpack("<4sIIII",b[:20])
    recs=[bytearray(b[20+i*rs:20+(i+1)*rs]) for i in range(n)]; strs=bytearray(b[20+n*rs:])
    return dict(n=n,f=f,rs=rs,recs=recs,strs=strs)
def save(name,d):
    hdr=struct.pack("<4sIIII",b"WDBC",len(d["recs"]),d["f"],d["rs"],len(d["strs"]))
    open(os.path.join(OUT,name),"wb").write(hdr+b"".join(d["recs"])+bytes(d["strs"]))
def u(r,i): return struct.unpack_from("<I",r,i*4)[0]
def su(r,i,v): struct.pack_into("<I",r,i*4,v)
def ub(r,o): return struct.unpack_from("<I",r,o)[0]
def sub_(r,o,v): struct.pack_into("<I",r,o,v)
def addstr(d,text):
    off=len(d["strs"]); d["strs"]+=text.encode("utf8")+b"\x00"; return off
def S(d,r,i):
    o=u(r,i); e=d["strs"].find(b"\x00",o); return bytes(d["strs"][o:e]).decode("utf8","replace") if 0<o<len(d["strs"]) else ""
# --- constantes do layout (conferidas no loader do servidor e nos dumps) ---
F_ID,F_TYPE,F_REQ,F_SPELLS,F_AE,F_TE,F_LEVEL,F_GROUP,F_CLASSTYPE,F_TAB,F_NAME,F_DESC = 0,1,2,5,14,15,26,29,32,33,47,64
B_STARTING, B_CONNECTED, B_FLAGS = 414, 415, 479
ROW_DISABLED, ROW_DISPLAY, ROW_AUTOMATIC, ROW_IDENTITY = 0x8, 0x20000, 0x100000, 0x40000
KEY_X = 10.0  # coluna vertical da direita: identidade (y=0) + 4 habilidades automaticas da spec (y=2/4/6/8)
KEYSTONES={"Assassination":["Rupture", "Deadly Poison", "Envenom", "Deadly Throw"],
           "Combat":["Dismantle", "Fan of Knives", "Saber Slash", "Shiv"],
           "Subtlety":["Distract", "Blind", "Cloak of Shadows", "Tricks of the Trade"]}
AUTO_DEPS={}
DUP_TALENT_NAMES=set([])
BASELINE=set(["Sinister Strike", "Eviscerate"])  # habilidades-base da classe: automaticas sem custo, no lugar da escolha
TRAINER_ONLY=set([])  # habilidades que ficam SO no treinador (fora da arvore, para todas as specs)
AUTO_DEPS_SPELL={}  # automatica por id de magia quando nao ha linha de habilidade no Reborn: nome -> [spell, nivel, mae]
DISPLAY={}  # nome de exibicao quando o nome interno da spec difere (ex.: BeastMastery -> Beast Mastery)
MAX_LEVEL = 60  # nivel maximo do realm: habilidades acima ficam fora da arvore ativa e vao para o relatorio (dados originais intactos)
PAL_TYPE, TEMPLATE_TYPE, CLASS_TAB = 38, 33, 87  # Rogue
PAL_TABS = {10:"Assassination", 8:"Combat", 9:"Subtlety"}
TEMPLATE_SPEC_TABS = {"Assassination": "Stalking", "Combat": "Venom", "Subtlety": "Fortitude"}  # curandeiro/tank/melee
NEW_BASE = 140600  # faixa do Rogue (receita §8)
# ---------------------------------------------------------------
ca=load("CharacterAdvancement.dbc"); specs=load("ChrSpecs.dbc"); ess=load("CharacterAdvancementEssence.dbc")
tt=load("CharacterAdvancementTabTypes.dbc"); tabname={u(r,0):S(tt,r,1) for r in tt["recs"]}
taken=[u(r,F_ID) for r in ca["recs"] if NEW_BASE<=u(r,F_ID)<NEW_BASE+200]; assert not taken, f"faixa {NEW_BASE}-{NEW_BASE+199} ocupada: {taken[:5]}"  # cada classe tem sua faixa de 200 ids
byid={u(r,F_ID):r for r in ca["recs"]}
pal=[r for r in ca["recs"] if u(r,F_CLASSTYPE)==PAL_TYPE]
tmpl=[r for r in ca["recs"] if u(r,F_CLASSTYPE)==TEMPLATE_TYPE]
B_POSX, B_POSY = 398, 402
posxy=lambda r: (struct.unpack_from('<f',r,B_POSY)[0], struct.unpack_from('<f',r,B_POSX)[0])
tmpl_class=sorted([r for r in tmpl if u(r,F_TAB)==CLASS_TAB], key=posxy)
tmpl_spec={tabname[u(r,F_TAB)]:[] for r in tmpl if u(r,F_TAB)!=CLASS_TAB}
for r in tmpl:
    if u(r,F_TAB)!=CLASS_TAB: tmpl_spec[tabname[u(r,F_TAB)]].append(r)
posx=lambda r: struct.unpack_from('<f',r,B_POSX)[0]
tmpl_keys={k:sorted([r for r in v if posx(r)==KEY_X and (ub(r,B_FLAGS)&ROW_AUTOMATIC)], key=posxy) for k,v in tmpl_spec.items()}
SPEC_GATE=8  # campo 39: portao de pontos da linha 4 nas specs do CoA (classe usa 4 por outro campo)
_lat={}; _byid={u(r,F_ID):r for r in tmpl_class}
for r in tmpl_class:
    p=(struct.unpack_from('<f',r,B_POSX)[0],struct.unpack_from('<f',r,B_POSY)[0]); _lat.setdefault(p,r)
LATTICE=sorted(_lat.items(), key=lambda kv:(kv[0][1],kv[0][0]))
LATTICE_PARENTS={p:[(struct.unpack_from('<f',_byid[v],B_POSX)[0],struct.unpack_from('<f',_byid[v],B_POSY)[0]) for k in range(15) for v in [ub(r,B_CONNECTED+4*k)] if v in _byid] for p,r in LATTICE}
for k in tmpl_spec: tmpl_spec[k]=sorted([r for r in tmpl_spec[k] if posx(r)!=KEY_X and not (ub(r,B_FLAGS)&ROW_IDENTITY)], key=posxy)
ranks=lambda r: sum(1 for i in range(F_SPELLS,F_SPELLS+5) if u(r,i))
typ=lambda r: S(ca,r,F_TYPE)
print(f"Classe Reborn (ClassType {PAL_TYPE}): {len(pal)} nos | molde Venomancer: classe {len(tmpl_class)} nos, specs { {k:len(v) for k,v in tmpl_spec.items()} }")
newrecs=[]; nextid=[NEW_BASE]; RANK_COMPRESS=[]
def clone(template, src, *, tab, ae, te, keep_group=False, keep_flags=False, level=None):
    r=bytearray(template); nid=nextid[0]; nextid[0]+=1
    su(r,F_ID,nid); su(r,F_CLASSTYPE,PAL_TYPE); su(r,F_TAB,tab); su(r,F_AE,ae); su(r,F_TE,te)
    for i in range(F_SPELLS,F_SPELLS+9): su(r,i,0)
    sp=[u(src,i) for i in range(F_SPELLS,F_SPELLS+9) if u(src,i)]
    if len(sp)>3:  # CoA guarda no maximo 3 ranks: usa ranks 1, meio e ultimo (5 ranks -> 1,3,5) para manter o efeito maximo original
        sp=[sp[0], sp[round((len(sp)-1)/2)], sp[-1]]; RANK_COMPRESS.append((S(ca,src,F_NAME),len(sp)))
    for k,v in enumerate(sp): su(r,F_SPELLS+k,v)
    lv=u(src,F_LEVEL) if level is None else level
    su(r,F_LEVEL,lv); su(r,27,lv); su(r,28,lv)
    su(r,F_NAME,u(src,F_NAME)); su(r,F_DESC,u(src,F_DESC))
    if not keep_group: su(r,F_GROUP,0)
    fl=ub(r,B_FLAGS); fl&=~(ROW_DISABLED|ROW_IDENTITY) if keep_flags else ~(ROW_DISPLAY|ROW_AUTOMATIC|ROW_DISABLED|ROW_IDENTITY); sub_(r,B_FLAGS,fl)
    return r, nid
def remap_links(records, idmap):
    for r in records:
        for i in range(F_REQ,F_REQ+3):
            v=u(r,i); su(r,i, idmap.get(v,0) if v else 0)
        for k in range(15):
            o=B_CONNECTED+4*k; v=ub(r,o); sub_(r,o, idmap.get(v,0) if v else 0)
report=collections.OrderedDict()
MAX_PAIRS=2   # nos de escolha (dois talentos na mesma posicao) mantidos por spec
posof=lambda r:(struct.unpack_from('<f',r,B_POSX)[0],struct.unpack_from('<f',r,B_POSY)[0])
def poda_organica(tm, need):
    """Poda o layout da spec molde mantendo a teia: so folhas/nos sem juncao saem, filhos religam ao pai do removido.
    Devolve (posicoes mantidas em ordem (y,x), pares, pais contraidos, removidas)."""
    byid_t={u(r,F_ID):r for r in tm}
    nodes={}
    for r in tm: nodes.setdefault(posof(r),[]).append(r)
    parents={q:set() for q in nodes}
    for q,rows in nodes.items():
        for r in rows:
            for k in range(15):
                v=ub(r,B_CONNECTED+4*k)
                if v in byid_t: parents[q].add(posof(byid_t[v]))
    roots={q for q in nodes if not parents[q]}
    pairpos=sorted([q for q,rows in nodes.items() if len(rows)>1], key=lambda q:(q[1],q[0]))
    pairs=[]
    for q in pairpos:
        if len(pairs)<MAX_PAIRS and all(q[1]!=x[1] for x in pairs): pairs.append(q)
    alive=set(nodes); par={q:set(v) for q,v in parents.items()}
    def ok(par_, alive_):
        ch={q:set() for q in alive_}
        for q in alive_:
            for x in par_[q]:
                if x in alive_: ch[x].add(q)
        seen=set(); st=[q for q in roots if q in alive_]
        while st:
            x=st.pop()
            if x in seen: continue
            seen.add(x); st+=ch[x]
        return seen==alive_ and all(q in roots or (par_[q] & alive_) for q in alive_)
    target=need-len(pairs); removed=[]
    while len(alive)>target:
        rowcount={}
        for q in alive: rowcount[q[1]]=rowcount.get(q[1],0)+1
        ch={q:sum(1 for x in alive if q in par[x]) for q in alive}
        cands=[q for q in alive if q not in pairs and q not in roots and q[1]>=2]
        cands.sort(key=lambda q:(-rowcount[q[1]], ch[q], -q[1], q[0]))
        done=False
        for q in cands:
            p2={x:set(v) for x,v in par.items()}
            for x in alive:
                if q in p2[x]: p2[x]=(p2[x]-{q})|(par[q]&alive)
            a2=alive-{q}
            if ok(p2,a2): par,alive=p2,a2; removed.append(q); done=True; break
        if not done: raise SystemExit(f"poda impossivel: faltam {len(alive)-target} remocoes")
    kept=sorted(alive, key=lambda q:(q[1],q[0]))
    return kept, pairs, {q:sorted(par[q]&alive) for q in kept}, removed, nodes, roots
# 1) árvore de classe (aba 87): habilidades do Paladino (nós DISPLAY sem talento) -> picks de AE
abilities=sorted([r for r in pal if typ(r) in ("","Ability") and (ub(r,B_FLAGS)&ROW_DISPLAY)], key=lambda r:(u(r,F_LEVEL),u(r,F_ID)))
seen=set(); abil=[]
for r in abilities:
    sp=u(r,F_SPELLS)
    if sp and sp not in seen: seen.add(sp); abil.append(r)
byname={S(ca,r,F_NAME):r for r in abil}
keyset=set(n for v in KEYSTONES.values() for n in v if n)  # None = vaga vazia na coluna; missing=[n for n in keyset|set(AUTO_DEPS)|set(AUTO_DEPS.values()) if n not in byname]; assert not missing, missing
general=[r for r in abil if S(ca,r,F_NAME) not in keyset and S(ca,r,F_NAME) not in AUTO_DEPS and S(ca,r,F_NAME) not in DUP_TALENT_NAMES and u(r,F_LEVEL)<=MAX_LEVEL and S(ca,r,F_NAME) not in TRAINER_ONLY]
report["so no treinador"]=", ".join(sorted(TRAINER_ONLY)) or "nenhuma"
above_cap=[r for r in abil if u(r,F_LEVEL)>MAX_LEVEL]
assert not [n for n in keyset if u(byname[n],F_LEVEL)>MAX_LEVEL], "chave acima do nivel maximo"
report["acima do nivel maximo"]=f"{len(above_cap)} habilidade(s) acima do nivel {MAX_LEVEL} fora da arvore ativa: "+", ".join(f"{S(ca,r,F_NAME)} (nivel {u(r,F_LEVEL)})" for r in above_cap)
use=general[:len(tmpl_class)]; idmap={}; made=[]; pickid={}
if len(general) < len(tmpl_class):
    # poda organica tambem na aba de classe: menos habilidades que posicoes -> remove folhas, religa filhos ao pai
    kept,pairs,par,removed,nodes,roots=poda_organica(tmpl_class,len(general))
    posid={}; queue=list(general)
    for q in kept:
        rows=nodes[q]; n=2 if q in pairs else 1; ids=[]
        for i in range(n):
            if not queue: break
            sr=queue.pop(0)
            r,nid=clone(rows[min(i,len(rows)-1)],sr,tab=CLASS_TAB,ae=1,te=0,keep_group=(n==2))
            if n==1: su(r,F_GROUP,0)
            made.append(r); ids.append(nid); pickid[S(ca,sr,F_NAME)]=nid
        posid[q]=ids
    assert not queue, "classe: sobraram habilidades sem posicao"
    for r,q in zip(made,[q for q in kept for _ in posid[q]]):
        links=[i for pq in par[q] for i in posid[pq]][:15]
        for i in range(F_REQ,F_REQ+3): su(r,i,0)
        for k in range(15): sub_(r,B_CONNECTED+4*k, links[k] if k<len(links) else 0)
    report["poda classe"]=(f"{len(kept)} posicoes ({len(pairs)} nos de escolha em {[(int(x),int(y)) for x,y in pairs]}), {len(removed)} removidas {[(int(x),int(y)) for x,y in removed]}; "
        f"ligacoes {sum(len(par[q]) for q in kept)}")
else:
    for t,sr in zip(tmpl_class,use):
        r,nid=clone(t,sr,tab=CLASS_TAB,ae=1,te=0,keep_group=True); idmap[u(t,F_ID)]=nid; made.append(r); pickid[S(ca,sr,F_NAME)]=nid
    remap_links(made,idmap)
newrecs+=made
# dependencias automaticas: molde = uma linha da Venomancer sem custo e com requisito (mesmas flags/posicao que o CoA usa)
# habilidades-base (nao custam ponto): a escolha vira automatica no mesmo lugar (custo 0, flag 0x102004, sem pre-requisito).
# O servidor concede a todos da classe no nivel dela; as filhas continuam ligadas a ela.
_miss=[n for n in BASELINE if n not in pickid]; assert not _miss, f"base fora da aba de classe: {_miss}"
for r in made:
    if S(ca,r,F_NAME) in BASELINE:
        su(r,F_AE,0); sub_(r,B_FLAGS,(ub(r,B_FLAGS)|0x102004)&~ROW_DISABLED); su(r,F_GROUP,0)
        for i in range(F_REQ,F_REQ+3): su(r,i,0)
        for k in range(15): sub_(r,B_CONNECTED+4*k,0)
report["habilidades-base"]=", ".join(sorted(BASELINE)) or "nenhuma"
# dependencias automaticas (formato das 23 automaticas do CoA: custo 0, flag 0x102004, REQ = habilidade-mae, sem ligacao).
# Usadas quando a classe tem mais habilidades que posicoes: a filha e aprendida sozinha quando a mae e conhecida.
# Posicao: casa livre mais proxima da mae na aba de classe (mesma linha primeiro, depois abaixo/acima).
dep_tmpl=next(r for r in ca["recs"] if u(r,F_ID)==12201)   # Widowmaker (Venomancer): automatica do CoA
occupied=set(posof(r) for r in made); deps=[]; dep_rep=[]
_spell_deps=[]
for name,(spell,level,parent) in AUTO_DEPS_SPELL.items():
    fake=bytearray(dep_tmpl)
    for i in range(F_SPELLS,F_SPELLS+9): su(fake,i,0)
    su(fake,F_SPELLS,spell); su(fake,F_LEVEL,level); su(fake,F_NAME,addstr(ca,name)); su(fake,F_DESC,0)
    byname[name]=fake; _spell_deps.append((name,parent))
for name,parent in list(AUTO_DEPS.items())+_spell_deps:
    assert parent in pickid, f"mae de {name} ({parent}) nao e uma escolha da aba de classe"
    r,nid=clone(dep_tmpl,byname[name],tab=CLASS_TAB,ae=0,te=0,keep_flags=True)
    for i in range(F_REQ,F_REQ+3): su(r,i,0)
    su(r,F_REQ,pickid[parent])
    for k in range(15): sub_(r,B_CONNECTED+4*k,0)
    su(r,F_GROUP,0)
    pr=next(m for m in made if u(m,F_ID)==pickid[parent]); px,py=posof(pr)
    # margem direita primeiro (colunas 9-10, como a coluna de automaticas do CoA), na altura da mae;
    # so se a margem lotar cai na busca antiga de casa livre dentro da teia.
    cells=[(x,y) for x in (9,10) for y in range(0,10) if (float(x),float(y)) not in occupied]
    cells.sort(key=lambda c:(abs(c[1]-py), c[0], c[1]))
    if not cells:
        cells=[(x,y) for y in range(0,10) for x in range(0,9) if (float(x),float(y)) not in occupied]
        cells.sort(key=lambda c:(abs(c[1]-py)*2+abs(c[0]-px)+(0 if c[1]==py else 0.5), c[1], c[0]))
    assert cells, "sem casa livre para "+name
    cx,cy=cells[0]; struct.pack_into('<f',r,B_POSX,float(cx)); struct.pack_into('<f',r,B_POSY,float(cy)); occupied.add((float(cx),float(cy)))
    deps.append(r); dep_rep.append(f"{name}<-{parent} em ({cx},{cy})")
newrecs+=deps
report["dependencias automaticas"]=f"{len(deps)} automaticas (aprendidas com a mae): "+"; ".join(dep_rep)
report["LACUNA classe"]=f"{0 if len(general)<len(tmpl_class) else max(0,len(tmpl_class)-len(use))} posicoes sem habilidade (poda aplicada se havia menos habilidades); {max(0,len(general)-len(tmpl_class))} habilidades gerais ficaram fora: " + ", ".join(S(ca,r,F_NAME) for r in general[len(tmpl_class):])
report["classe aba 87"]=f"{len(use)} de {len(general)} habilidades gerais viraram picks de AE (molde {len(tmpl_class)} posicoes); 12 viraram chaves de spec; {len(AUTO_DEPS)} dependencias"
# 2) árvores de spec: talentos do Paladino por aba -> layout da spec molde
identity={}
for tab,name in PAL_TABS.items():
    tal=sorted([r for r in pal if u(r,F_TAB)==tab and typ(r) in ("Talent","TalentAbility")], key=lambda r:(u(r,39),u(r,F_LEVEL),u(r,F_ID)))
    tm=tmpl_spec[TEMPLATE_SPEC_TABS[name]]
    kept,pairs,par,removed,nodes,roots=poda_organica(tm,len(tal))
    made=[]; posid={}; queue=list(tal)
    for q in kept:
        rows=nodes[q]; n=2 if q in pairs else 1
        ids=[]
        for i in range(n):
            if not queue: break
            sr=queue.pop(0)
            r,nid=clone(rows[min(i,len(rows)-1)],sr,tab=tab,ae=0,te=1,keep_group=(n==2))
            if n==1: su(r,F_GROUP,0)
            made.append(r); ids.append(nid)
        posid[q]=ids
    assert not queue, f"{name}: sobraram talentos sem posicao"
    for r,q in zip(made,[q for q in kept for _ in posid[q]]):
        links=[i for pq in par[q] for i in posid[pq]][:15]
        for i in range(F_REQ,F_REQ+3): su(r,i,0)
        for k in range(15): sub_(r,B_CONNECTED+4*k, links[k] if k<len(links) else 0)
    newrecs+=made
    # validacao: alcance a partir da raiz e nenhum isolado
    reach=set(); st=[q for q in kept if q in roots]
    chl={q:[x for x in kept if q in par[x]] for q in kept}
    while st:
        x=st.pop()
        if x in reach: continue
        reach.add(x); st+=chl[x]
    rows_={}
    for q in kept: rows_[int(q[1])]=rows_.get(int(q[1]),0)+len(posid[q])
    report[f"poda {name}"]=(f"{len(kept)} posicoes ({len(pairs)} nos de escolha em {[(int(x),int(y)) for x,y in pairs]}), {len(removed)} removidas {[(int(x),int(y)) for x,y in removed]}; "
        f"talentos por linha {dict(sorted(rows_.items()))}; alcancaveis {len(reach)}/{len(kept)}; isolados {sum(1 for q in kept if q not in roots and not par[q])}; ligacoes {sum(len(par[q]) for q in kept)}")
    report[f"spec {name} (tab {tab})"]=f"{len(tal)} talentos no layout organico de {TEMPLATE_SPEC_TABS[name]} ({len(nodes)} posicoes originais)"
    keys=[]
    for t,kname in zip(tmpl_keys[TEMPLATE_SPEC_TABS[name]],KEYSTONES[name]):
        if not kname: continue  # vaga vazia (mantem as outras chaves no lugar)
        r,nid=clone(t,byname[kname],tab=tab,ae=0,te=0,keep_flags=True,level=u(t,F_LEVEL))
        for i in range(F_REQ,F_REQ+3): su(r,i,0)
        for k in range(15): sub_(r,B_CONNECTED+4*k,0)
        keys.append(r)
    newrecs+=keys
    report[f"chaves {name}"]=", ".join(f"{S(ca,r,F_NAME)} (nivel {u(r,F_LEVEL)}, pos {posxy(r)[1]:.0f},{posxy(r)[0]:.0f})" for r in keys)
    # desliga os talentos originais (ficam só p/ Reborn trainers as habilidades DISPLAY)
    for r in tal: sub_(r,B_FLAGS, ub(r,B_FLAGS)|ROW_DISABLED)
    report[f"spec {name} (tab {tab})"]=f"{min(len(tm),len(tal))} talentos clonados no layout de {TEMPLATE_SPEC_TABS[name]} ({len(tm)} posicoes); {len(tal)} originais marcados DISABLED"
    # 3) entrada de identidade da spec (clone da 4053 da Stalking): magia-assinatura da spec
    identity[name]=(tab, made[0] if made else None)
SIGNATURE={"Assassination": 1101329, "Combat": 1151690, "Subtlety": 1151713}
EXAMPLES={"Assassination": (1132645, 1151662, 1101943), "Combat": (1101752, 1151723, 1587522), "Subtlety": (1116511, 1136554, 1108676)}
DESC={"Assassination": "A deadly master of poisons who strikes from the shadows, finishing weakened foes with lethal precision.", "Combat": "A swashbuckler who relies on agility and relentless blows, outlasting any opponent in a straight fight.", "Subtlety": "A master of stealth and deception who dances in the shadows, striking before the enemy can react."}
ident_tmpl=byid[4053]
specrows={S(specs,r,2):r for r in specs["recs"] if S(specs,r,1)=="ROGUE"}
for name,(tab,first) in identity.items():
    r=bytearray(ident_tmpl); nid=nextid[0]; nextid[0]+=1
    su(r,F_ID,nid); su(r,F_CLASSTYPE,PAL_TYPE); su(r,F_TAB,tab)
    for i in range(F_SPELLS,F_SPELLS+9): su(r,i,0)
    su(r,F_SPELLS,SIGNATURE[name]); 
    for i in range(F_REQ,F_REQ+3): su(r,i,0)
    for k in range(15): sub_(r,B_CONNECTED+4*k,0)
    su(r,F_NAME,addstr(ca,f"{DISPLAY.get(name,name)} Rogue")); su(r,F_DESC,addstr(ca,DESC[name]))
    newrecs.append(r)
    sp=specrows[name.upper()]
    su(sp,16,0)  # papel secundario Support (campo 16) desligado: Paladino = Tank / Healer / Melee, como as classes do CoA
    su(sp,24,SIGNATURE[name]); su(sp,25,EXAMPLES[name][0]); su(sp,26,EXAMPLES[name][1]); su(sp,27,EXAMPLES[name][2]); su(sp,28,nid)
    if not S(specs,sp,46): su(sp,46,addstr(specs,DESC[name]))
    report[f"ChrSpecs {name}"]=f"identidade={nid} assinatura={SIGNATURE[name]} exemplos={EXAMPLES[name]} descricao ok"
nd=0
for r in pal:
    if not ub(r,B_FLAGS)&ROW_DISABLED: sub_(r,B_FLAGS, ub(r,B_FLAGS)|ROW_DISABLED); nd+=1
report["originais desligados"]=f"{nd} linhas originais do Reborn desta classe marcadas DISABLED (a classe agora vive so nos ids novos)"
# 3b) especializacoes extras (4a spec). minimal=True: so aba nova + linha de ChrSpecs + identidade (sem talentos, sem chaves)
EXTRA_SPECS_SUPPORT_FUTURO={"Support":dict(tab=95, clone_tab=16, spec_id=102, clone_spec=76, minimal=True,
    signature="Power Word: Fortitude", examples=("Power Word: Shield","Fear Ward","Divine Spirit"),
    desc="Empower and protect your allies with wards, blessings of fortitude and the restoration of mana.",
    roles={16:1})}   # campos de papel 11-16: 16 = Support
EXTRA_SPECS={}   # Support adiada (11/10): para ligar, EXTRA_SPECS=EXTRA_SPECS_SUPPORT_FUTURO
for name,cfg in EXTRA_SPECS.items():
    if not any(u(r,0)==cfg["tab"] for r in tt["recs"]):
        r=bytearray(next(r for r in tt["recs"] if u(r,0)==cfg["clone_tab"])); off=addstr(tt,name)
        su(r,0,cfg["tab"]); su(r,1,off); su(r,2,off); tt["recs"].append(r)
    spell_of=lambda n: u(byname[n],F_SPELLS)
    r=bytearray(ident_tmpl); nid=nextid[0]; nextid[0]+=1
    su(r,F_ID,nid); su(r,F_CLASSTYPE,PAL_TYPE); su(r,F_TAB,cfg["tab"])
    for i in range(F_SPELLS,F_SPELLS+9): su(r,i,0)
    su(r,F_SPELLS,spell_of(cfg["signature"]))
    for i in range(F_REQ,F_REQ+3): su(r,i,0)
    for k in range(15): sub_(r,B_CONNECTED+4*k,0)
    su(r,F_NAME,addstr(ca,f"{DISPLAY.get(name,name)} Rogue")); su(r,F_DESC,addstr(ca,cfg["desc"]))
    newrecs.append(r)
    sp=bytearray(next(x for x in specs["recs"] if u(x,0)==cfg["clone_spec"]))
    su(sp,0,cfg["spec_id"]); su(sp,2,addstr(specs,name.upper())); su(sp,29,addstr(specs,name))
    for i in range(11,17): su(sp,i,cfg["roles"].get(i,0))
    su(sp,24,spell_of(cfg["signature"]))
    for k,n in enumerate(cfg["examples"]): su(sp,25+k,spell_of(n))
    su(sp,28,nid); su(sp,46,addstr(specs,cfg["desc"]))
    specs["recs"]=[x for x in specs["recs"] if u(x,0)!=cfg["spec_id"]]+[sp]
    report[f"spec extra {name}"]=f"aba {cfg['tab']} (clone da {cfg['clone_tab']}), ChrSpecs {cfg['spec_id']}, identidade {nid} (assinatura {cfg['signature']}), minimal={cfg['minimal']}: sem talentos e sem chaves"
# 4) Essence: classe 2 recebe a curva da classe 29 (Venomancer)
curve={}
for r in ess["recs"]:
    if u(r,2)==29 and not any(u(r,i) for i in range(3,7)): curve[u(r,1)]=(u(r,7),u(r,8))
n=0
for r in ess["recs"]:
    if u(r,2)==4 and not any(u(r,i) for i in range(3,7)) and u(r,1) in curve:
        su(r,7,curve[u(r,1)][0]); su(r,8,curve[u(r,1)][1]); n+=1
report["Essence classe 4"]=f"{n} linhas com a curva da classe 29 (nivel 10: AE {curve[10][0]} TE {curve[10][1]}; 60: AE {curve[60][0]} TE {curve[60][1]})"
# ids estaveis: renumera os registros novos por chave (aba, flag identidade, 1a magia) para que regenerar nao mude os ids
B_FL=B_FLAGS
def key(r): return (u(r,F_TAB), 1 if ub(r,B_FL)&ROW_IDENTITY else 0, u(r,F_SPELLS), S(ca,r,F_NAME))
keys=[key(r) for r in newrecs]; assert len(set(keys))==len(keys), "chave repetida: "+str([k for k in keys if keys.count(k)>1][:3])
order=sorted(newrecs, key=key)
# preserva os ids ja instalados (mesma chave aba/identidade/1a magia/nome) e so da id novo, no fim da faixa, ao que for novo
PREV_DBC=COA_ROOT+"/env/dist/data/dbc/CharacterAdvancement.dbc"
prev={}
if os.path.exists(PREV_DBC):
    _b=open(PREV_DBC,"rb").read(); _m,_n,_f,_rs,_ss=struct.unpack("<4sIIII",_b[:20]); _st=_b[20+_n*_rs:]
    for _i in range(_n):
        _r=_b[20+_i*_rs:20+(_i+1)*_rs]; _id=u(_r,F_ID)
        if u(_r,F_CLASSTYPE)==PAL_TYPE and NEW_BASE<=_id<NEW_BASE+200 and not ub(_r,B_FLAGS)&ROW_DISABLED:
            _o=u(_r,F_NAME); _nm=_st[_o:_st.find(b"\x00",_o)].decode("utf8","replace")
            prev[(u(_r,F_TAB), 1 if ub(_r,B_FLAGS)&ROW_IDENTITY else 0, u(_r,F_SPELLS), _nm)]=_id
renum={}; nxt=max(list(prev.values())+[NEW_BASE-1])+1; kept_ids=0
for r in order:
    k=key(r)
    if k in prev: renum[u(r,F_ID)]=prev.pop(k); kept_ids+=1
    else: renum[u(r,F_ID)]=nxt; nxt+=1
assert len(set(renum.values()))==len(renum) and max(renum.values())<NEW_BASE+200, "colisao ou faixa estourada"
report["ids preservados"]=f"{kept_ids} reaproveitados do instalado, {len(renum)-kept_ids} novos; {len(prev)} instalados sem correspondente (saem)"
for r in newrecs:
    su(r,F_ID, renum[u(r,F_ID)])
    for i in range(F_REQ,F_REQ+3):
        v=u(r,i); su(r,i, renum.get(v,v))
    for k in range(15):
        o=B_CONNECTED+4*k; v=ub(r,o); sub_(r,o, renum.get(v,v))
for sp in specs["recs"]:
    v=u(sp,28); su(sp,28, renum.get(v,v))
report["ids"]=f"{len(newrecs)} registros novos renumerados de forma estavel ({NEW_BASE}..{NEW_BASE+len(newrecs)-1})"
report["ranks"]=f"{len(RANK_COMPRESS)} talentos de 4-5 ranks comprimidos em 3 (ranks 1, meio, ultimo): "+", ".join(n for n,_ in RANK_COMPRESS)
ca["recs"]+= newrecs
for k,v in report.items(): print(f"- {k}: {v}")
print(f"novos registros: {len(newrecs)} (ids {NEW_BASE}..{nextid[0]-1}); total CA {len(ca['recs'])}")
if APPLY:
    save("CharacterAdvancement.dbc",ca); save("ChrSpecs.dbc",specs); save("CharacterAdvancementTabTypes.dbc",tt); save("CharacterAdvancementEssence.dbc",ess); print("gravado em", OUT)
