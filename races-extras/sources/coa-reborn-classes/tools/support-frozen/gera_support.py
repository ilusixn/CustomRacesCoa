#!/usr/bin/env python3
"""4a especializacao SUPPORT para Paladin/Priest/Druid/Shaman (pos-processo: NAO regenera as arvores existentes).
Uso: gera_support.py <dir_dbc_origem> <dir_saida> [--apply]"""
import struct, sys, os
SRC, OUT = sys.argv[1], sys.argv[2]; APPLY="--apply" in sys.argv
os.makedirs(OUT, exist_ok=True)
def load(name):
    b=open(os.path.join(SRC,name),"rb").read(); m,n,f,rs,ss=struct.unpack("<4sIIII",b[:20])
    return dict(n=n,f=f,rs=rs,recs=[bytearray(b[20+i*rs:20+(i+1)*rs]) for i in range(n)],strs=bytearray(b[20+n*rs:]))
def save(name,d):
    open(os.path.join(OUT,name),"wb").write(struct.pack("<4sIIII",b"WDBC",len(d["recs"]),d["f"],d["rs"],len(d["strs"]))+b"".join(d["recs"])+bytes(d["strs"]))
def u(r,i): return struct.unpack_from("<I",r,i*4)[0]
def su(r,i,v): struct.pack_into("<I",r,i*4,v)
def ub(r,o): return struct.unpack_from("<I",r,o)[0]
def sub_(r,o,v): struct.pack_into("<I",r,o,v)
def addstr(d,t):
    off=len(d["strs"]); d["strs"]+=t.encode("utf8")+b"\x00"; return off
def S(d,r,i):
    o=u(r,i); e=d["strs"].find(b"\x00",o); return bytes(d["strs"][o:e]).decode("utf8","replace") if 0<o<len(d["strs"]) else ""
F_ID,F_TYPE,F_REQ,F_SPELLS,F_AE,F_TE,F_LEVEL,F_GROUP,F_CLASSTYPE,F_TAB,F_NAME,F_DESC=0,1,2,5,14,15,26,29,32,33,47,64
B_POSX,B_POSY,B_STARTING,B_CONNECTED,B_FLAGS=398,402,414,415,479
ROW_DISABLED,ROW_DISPLAY,ROW_AUTOMATIC,ROW_IDENTITY=0x8,0x20000,0x100000,0x40000
KEY_X=10.0; TEMPLATE_TYPE=33; MOLDE_TAB_NAME="Fortitude"
CFG={
 "paladin": dict(ct=44,base=142000,tab=95,spec_id=102,cls="PALADIN",classname="Paladin",
   signature=("Beacon of Virtue",1277680),
   keys=[("Greater Blessing of Might",1125782),("Greater Blessing of Wisdom",1125894),("Greater Blessing of Kings",1125898),("Greater Blessing of Sanctuary",1125899)],
   web=[("Improved Flash of Light",1120249,15),("Blessing of Light",1171870,18),("Sense Undead",1105502,20),("Aegis of the Martyr",1181441,25),("Shadow Resistance Aura",1119876,28),("Frost Resistance Aura",1119888,32),("Fire Resistance Aura",1119891,36),("Guardian of Ancient Kings",1180101,45)],
   desc="A guardian of the group: blessings, auras and protective power that strengthen every ally.",
   examples=(1125898,1119876,1180101)),
 "priest": dict(ct=40,base=142100,tab=96,spec_id=103,cls="PRIEST",classname="Priest",
   signature=("Power Word: Barrier",1180523),
   keys=[("Prayer of Fortitude",1121562),("Leap of Faith",1186381),("Prayer of Shadow Protection",1127683),("Prayer of Spirit",1127681)],
   web=[("Glyph of Power Word: Shield",1156160,12),("Heal",1102054,16),("Fear Ward",1106346,20),("Holy Word: Chastise",1180051,25),("Focused Casting",1114743,30),("Glyph of Prayer of Healing",1156161,35),("Glyph of Dispel Magic",1156131,40)],
   desc="Empower and shield the group with prayers, wards and holy words.",
   examples=(1121562,1106346,1186381)),
 "druid": dict(ct=42,base=142200,tab=97,spec_id=104,cls="DRUID",classname="Druid",
   signature=("Efflorescence",1186384),
   keys=[("Gift of the Wild",1121849),("Touch of Elune",1180141),("Moonkin Aura",1124907),("Spark of Nature",1148435)],
   web=[("Improved Barkskin",1166530,10),("Blood Frenzy",1116952,15)],
   desc="Nature's caretaker: wild gifts, regenerating ground and lunar blessings for the group.",
   examples=(1121849,1180141,1148435)),
 "shaman": dict(ct=43,base=142300,tab=98,spec_id=105,cls="SHAMAN",classname="Shaman",
   signature=("Spirit Link Totem",1181421),
   keys=[("Ancestral Fortitude",1116177),("Sentry Totem",1106495),("Unleash Earth",1182010),("Elemental Shields",1130669)],
   web=[("Focused",1143339,10),("Primal Strike",1117365,12),("Freeze",1163685,15),("Astral Plane",1182050,35)],
   desc="The elements guard the group: linking totems, ancestral shields and the strength of the earth.",
   examples=(1116177,1106495,1181421)),
}
ca=load("CharacterAdvancement.dbc"); specs=load("ChrSpecs.dbc"); tt=load("CharacterAdvancementTabTypes.dbc")
tabname={u(r,0):S(tt,r,1) for r in tt["recs"]}
# conferencias de seguranca
ids_existentes={u(r,F_ID) for r in ca["recs"]}
for c in CFG.values():
    tomados=[i for i in range(c["base"],c["base"]+100) if i in ids_existentes]
    assert not tomados, f"faixa {c['base']} ocupada: {tomados[:3]}"
    assert c["tab"] not in tabname, f"tab {c['tab']} ja existe"
    assert not any(u(r,0)==c["spec_id"] for r in specs["recs"]), f"spec {c['spec_id']} ja existe"
# molde: Venomancer tab "Fortitude"
tmpl=[r for r in ca["recs"] if u(r,F_CLASSTYPE)==TEMPLATE_TYPE]
molde_tab=next(t for t,n in tabname.items() if n==MOLDE_TAB_NAME and any(u(r,F_TAB)==t for r in tmpl))
mrows=[r for r in tmpl if u(r,F_TAB)==molde_tab]
posx=lambda r: struct.unpack_from("<f",r,B_POSX)[0]
posy=lambda r: struct.unpack_from("<f",r,B_POSY)[0]
posof=lambda r:(posx(r),posy(r))
mkeys=sorted([r for r in mrows if posx(r)==KEY_X and (ub(r,B_FLAGS)&ROW_AUTOMATIC)],key=posy)
mident=next(r for r in mrows if ub(r,B_FLAGS)&ROW_IDENTITY)
mweb=sorted([r for r in mrows if posx(r)!=KEY_X and not(ub(r,B_FLAGS)&ROW_IDENTITY)],key=lambda r:(posy(r),posx(r)))
assert len(mkeys)==4, len(mkeys)
MAX_PAIRS=0
def poda_organica(tm, need):
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
    alive=set(nodes); par={q:set(v) for q,v in parents.items()}
    def ok(par_,alive_):
        ch={q:set() for q in alive_}
        for q in alive_:
            for x in par_[q]:
                if x in alive_: ch[x].add(q)
        seen=set(); st=[q for q in roots if q in alive_]
        while st:
            x=st.pop()
            if x in seen: continue
            seen.add(x); st+=ch[x]
        return seen==alive_ and all(q in roots or (par_[q]&alive_) for q in alive_)
    removed=[]
    while len(alive)>need:
        ch={q:sum(1 for x in alive if q in par[x]) for q in alive}
        cands=[q for q in alive if q not in roots]
        cands.sort(key=lambda q:(ch[q],-q[1],q[0]))
        done=False
        for q in cands:
            p2={x:set(v) for x,v in par.items()}
            for x in alive:
                if q in p2[x]: p2[x]=(p2[x]-{q})|(par[q]&alive)
            a2=alive-{q}
            if ok(p2,a2): par,alive=p2,a2; removed.append(q); done=True; break
        if not done: raise SystemExit("poda impossivel")
    kept=sorted(alive,key=lambda q:(q[1],q[0]))
    return kept,{q:sorted(par[q]&alive) for q in kept},nodes,roots
rel=[]
for cname,c in CFG.items():
    nid=[c["base"]]
    def take():
        v=nid[0]; nid[0]+=1; return v
    novo=[]
    # aba
    tr=bytearray(next(r for r in tt["recs"] if u(r,0)==molde_tab)); off=addstr(tt,"Support")
    su(tr,0,c["tab"]); su(tr,1,off); su(tr,2,off); tt["recs"].append(tr)
    # teia
    kept,par,nodes,roots=poda_organica(mweb,len(c["web"]))
    posid={}; fila=list(c["web"])
    made=[]
    for q in kept:
        nm,sp,lvl=fila.pop(0)
        r=bytearray(nodes[q][0]); i=take()
        su(r,F_ID,i); su(r,F_CLASSTYPE,c["ct"]); su(r,F_TAB,c["tab"]); su(r,F_AE,0); su(r,F_TE,1)
        for k in range(F_SPELLS,F_SPELLS+9): su(r,k,0)
        su(r,F_SPELLS,sp); su(r,F_LEVEL,lvl); su(r,27,lvl); su(r,28,lvl); su(r,F_GROUP,0)
        su(r,F_NAME,addstr(ca,nm)); su(r,F_DESC,0)
        fl=ub(r,B_FLAGS)&~(ROW_DISPLAY|ROW_AUTOMATIC|ROW_DISABLED|ROW_IDENTITY); sub_(r,B_FLAGS,fl)
        posid[q]=i; made.append((r,q))
    for r,q in made:
        links=[posid[p] for p in par[q]][:15]
        for k in range(F_REQ,F_REQ+3): su(r,k,0)
        for k in range(15): sub_(r,B_CONNECTED+4*k, links[k] if k<len(links) else 0)
        novo.append(r)
    # chaves (coluna direita, niveis do molde 20/30/40/50)
    for t,(nm,sp) in zip(mkeys,c["keys"]):
        r=bytearray(t); i=take()
        su(r,F_ID,i); su(r,F_CLASSTYPE,c["ct"]); su(r,F_TAB,c["tab"]); su(r,F_AE,0); su(r,F_TE,0)
        for k in range(F_SPELLS,F_SPELLS+9): su(r,k,0)
        su(r,F_SPELLS,sp)
        lvl=u(t,F_LEVEL); su(r,F_LEVEL,lvl); su(r,27,lvl); su(r,28,lvl); su(r,F_GROUP,0)
        su(r,F_NAME,addstr(ca,nm)); su(r,F_DESC,0)
        for k in range(F_REQ,F_REQ+3): su(r,k,0)
        for k in range(15): sub_(r,B_CONNECTED+4*k,0)
        fl=(ub(r,B_FLAGS)|ROW_AUTOMATIC)&~(ROW_DISABLED|ROW_IDENTITY); sub_(r,B_FLAGS,fl)
        novo.append(r)
    # identidade
    r=bytearray(mident); i=take()
    snm,ssp=c["signature"]
    su(r,F_ID,i); su(r,F_CLASSTYPE,c["ct"]); su(r,F_TAB,c["tab"])
    for k in range(F_SPELLS,F_SPELLS+9): su(r,k,0)
    su(r,F_SPELLS,ssp)
    for k in range(F_REQ,F_REQ+3): su(r,k,0)
    for k in range(15): sub_(r,B_CONNECTED+4*k,0)
    su(r,F_NAME,addstr(ca,f"Support {c['classname']}")); su(r,F_DESC,addstr(ca,c["desc"]))
    novo.append(r); ident_id=i
    # ChrSpecs
    sp_row=bytearray(next(x for x in specs["recs"] if S(specs,x,1)==c["cls"]))
    su(sp_row,0,c["spec_id"]); su(sp_row,2,addstr(specs,"SUPPORT")); su(sp_row,29,addstr(specs,"Support"))
    for k in range(11,17): su(sp_row,k,1 if k==16 else 0)
    su(sp_row,24,ssp)
    for k,v in enumerate(c["examples"]): su(sp_row,25+k,v)
    su(sp_row,28,ident_id); su(sp_row,46,addstr(specs,c["desc"]))
    specs["recs"].append(sp_row)
    ca["recs"]+=novo
    rel.append(f"{c['classname']} Support: tab {c['tab']}, spec {c['spec_id']}, ids {c['base']}-{nid[0]-1} ({len(c['web'])} teia + 4 chaves + identidade {ident_id}; assinatura {snm})")
for l in rel: print(l)
if APPLY:
    save("CharacterAdvancement.dbc",ca); save("ChrSpecs.dbc",specs); save("CharacterAdvancementTabTypes.dbc",tt)
    print("gravado em",OUT)
else:
    print("(sem --apply: so relatorio)")
