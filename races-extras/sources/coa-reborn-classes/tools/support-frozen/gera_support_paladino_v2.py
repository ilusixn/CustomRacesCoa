#!/usr/bin/env python3
"""[SUPPORT-V2] Arvore do Paladin Support com 29 nos (pos-processo, idempotente; roda DEPOIS do fecha_regen).
Preserva os ids 142000-142012 (os 4 da opcao R mantem o id e trocam a magia), cria 142013/142015-142029,
arvore interna General2 (29). Teia de 22 nos com posicoes/ligacoes/portoes do molde CoA (poda organica),
como todas as outras specs; margem x9 com 2 automaticas-dependencia (REQ = mae).
Uso: gera_support_paladino_v2.py <dir_dbc_origem> <dir_saida> [--apply]"""
import struct, sys, os
SRC, OUT = sys.argv[1], sys.argv[2]; APPLY="--apply" in sys.argv
def load(name):
    b=open(os.path.join(SRC,name),"rb").read(); m,n,f,rs,ss=struct.unpack("<4sIIII",b[:20])
    return dict(n=n,f=f,rs=rs,recs=[bytearray(b[20+i*rs:20+(i+1)*rs]) for i in range(n)],strs=bytearray(b[20+n*rs:]))
def save(name,d):
    os.makedirs(OUT,exist_ok=True)
    open(os.path.join(OUT,name),"wb").write(struct.pack("<4sIIII",b"WDBC",len(d["recs"]),d["f"],d["rs"],len(d["strs"]))+b"".join(d["recs"])+bytes(d["strs"]))
def u(r,i): return struct.unpack_from("<I",r,i*4)[0]
def su(r,i,v): struct.pack_into("<I",r,i*4,v)
def ub(r,o): return struct.unpack_from("<I",r,o)[0]
def sub_(r,o,v): struct.pack_into("<I",r,o,v)
def addstr(d,t):
    i=d["strs"].find(b"\x00"+t.encode()+b"\x00")
    if i>=0: return i+1
    off=len(d["strs"]); d["strs"]+=t.encode()+b"\x00"; return off
def S(d,r,i):
    o=u(r,i); e=d["strs"].find(b"\x00",o); return bytes(d["strs"][o:e]).decode("utf8","replace") if 0<o<len(d["strs"]) else ""
F_ID,F_REQ,F_SPELLS,F_AE,F_TE,F_LEVEL,F_GROUP,F_CLASSTYPE,F_TAB,F_NAME,F_DESC=0,2,5,14,15,26,29,32,33,47,64
F_GATE=39
B_POSX,B_POSY,B_CONNECTED,B_FLAGS=398,402,415,479
ROW_DISABLED,ROW_DISPLAY,ROW_AUTOMATIC,ROW_IDENTITY=0x8,0x20000,0x100000,0x40000
PAL_CT, GENERAL2, TEMPLATE_TYPE, MOLDE_TAB_NAME, KEY_X = 44, 29, 33, "Fortitude", 10.0
GATE_MIN_LEVEL = 27   # Paladino (Essence): TE(27)=9 = portao 8 + 1 ponto do proprio no
# teia em ordem de nivel: (id, nome, [magias por rank], nivel)
WEB=[(142013,"Vanguard Training",[2600012,2600013,2600014],10),
     (142015,"Hardened Faith",[2600015,2600016,2600017],10),
     (142016,"Benevolent Blessings",[2600018,2600019],14),
     (142000,"Improved Flash of Light",[1120249],15),
     (142017,"Blessed Resilience",[2600020,2600021,2600022],16),
     (142001,"Blessing of Light",[1171870],18),
     (142002,"Sanctified Wellspring",[2600023,2600024],20),
     (142018,"Shining Example",[2600025],22),
     (142003,"Aegis of the Martyr",[1181441],25),
     (142019,"Shared Devotion",[2600006],25),
     (142004,"Devout Meditation",[2600026,2600027,2600028],28),
     (142020,"Guardian's Reach",[2600029,2600030],30),
     (142005,"Warding Auras",[2600031,2600032],32),
     (142006,"Hallowed Blessings",[2600033,2600034],34),
     (142021,"Aura of Reprisal",[2600007],35),
     (142022,"Protector's Instinct",[2600038,2600039],40),
     (142023,"Devoted Heart",[2600035,2600036,2600037],42),
     (142024,"Beacon Mastery",[2600009],45),
     (142007,"Guardian of Ancient Kings",[1180101],45),
     (142025,"Crusader's Benediction",[2600040,2600041],52),
     (142026,"Light's Bulwark",[2600008],55),
     (142027,"Avatar of the Vanguard",[2600043],60)]
MARGIN=[(142028,"Vanguard's Presence",2600010,25,142015),(142029,"Oath of Protection",2600011,45,142022)]
assert len(WEB)==22 and len({w[0] for w in WEB})==22
ca=load("CharacterAdvancement.dbc"); tt=load("CharacterAdvancementTabTypes.dbc")
tabname={u(r,0):S(tt,r,1) for r in tt["recs"]}
assert tabname.get(GENERAL2)=="General2", tabname.get(GENERAL2)
byid={u(r,F_ID):r for r in ca["recs"]}
for i in range(142000,142013):
    r=byid[i]; assert u(r,F_CLASSTYPE)==PAL_CT and u(r,F_TAB)==GENERAL2, i
posx=lambda r: struct.unpack_from("<f",r,B_POSX)[0]
posy=lambda r: struct.unpack_from("<f",r,B_POSY)[0]
posof=lambda r:(posx(r),posy(r))
tmpl=[r for r in ca["recs"] if u(r,F_CLASSTYPE)==TEMPLATE_TYPE]
molde_tab=next(t for t,n in tabname.items() if n==MOLDE_TAB_NAME and any(u(r,F_TAB)==t for r in tmpl))
mrows=[r for r in tmpl if u(r,F_TAB)==molde_tab]
mweb=sorted([r for r in mrows if posx(r)!=KEY_X and not(ub(r,B_FLAGS)&ROW_IDENTITY) and not(ub(r,B_FLAGS)&ROW_AUTOMATIC)],key=lambda r:(posy(r),posx(r)))
def poda_organica(tm, need):
    byid_t={u(r,F_ID):r for r in tm}; nodes={}
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
    while len(alive)>need:
        ch={q:sum(1 for x in alive if q in par[x]) for q in alive}
        cands=sorted([q for q in alive if q not in roots],key=lambda q:(ch[q],-q[1],q[0]))
        for q in cands:
            p2={x:set(v) for x,v in par.items()}
            for x in alive:
                if q in p2[x]: p2[x]=(p2[x]-{q})|(par[q]&alive)
            a2=alive-{q}
            if ok(p2,a2): par,alive=p2,a2; break
        else: raise SystemExit("poda impossivel")
    kept=sorted(alive,key=lambda q:(q[1],q[0]))
    return kept,{q:sorted(par[q]&alive) for q in kept},nodes,roots
kept,par,nodes,roots=poda_organica(mweb,len(WEB))
# nos mais rasos primeiro; dentro da linha, nivel menor a esquerda (WEB ja esta em ordem de nivel)
posid={}; out=[]
for q,(eid,nm,spells,lvl) in zip(kept,WEB):
    t=nodes[q][0]
    r=byid.get(eid)
    r=bytearray(t) if r is None else r
    keep=bytearray(r)
    r[:]=t                                  # posicao, portao (campo 39), flags e ligacoes do molde
    su(r,F_ID,eid); su(r,F_CLASSTYPE,PAL_CT); su(r,F_TAB,GENERAL2); su(r,F_AE,0); su(r,F_TE,1)
    for k in range(F_SPELLS,F_SPELLS+9): su(r,k,0)
    for k,s in enumerate(spells): su(r,F_SPELLS+k,s)
    # portao de 8 pontos (campo 39 do molde): o no so abre quando ha gate+1 TE -> nivel minimo coerente
    if u(r,F_GATE): lvl=max(lvl,GATE_MIN_LEVEL)
    su(r,F_LEVEL,lvl); su(r,27,lvl); su(r,28,lvl); su(r,F_GROUP,0)
    su(r,F_NAME,addstr(ca,nm)); su(r,F_DESC,0)
    for k in range(F_REQ,F_REQ+3): su(r,k,0)
    sub_(r,B_FLAGS,ub(r,B_FLAGS)&~(ROW_DISPLAY|ROW_AUTOMATIC|ROW_DISABLED|ROW_IDENTITY))
    posid[q]=eid; out.append((r,q,eid in byid))
for r,q,_ in out:
    links=[posid[p] for p in par[q]][:15]
    for k in range(15): sub_(r,B_CONNECTED+4*k, links[k] if k<len(links) else 0)
# margem x9: automaticas-dependencia (molde: Track Humanoids 140536 = flags 0x102004, custo 0, REQ mae, sem ligacao)
dep_t=byid[140536]
for eid,nm,sp,lvl,mother in MARGIN:
    r=byid.get(eid); new=r is None
    r=bytearray(dep_t) if new else r
    r[:]=dep_t
    mrow=next(x for x,_,_ in out if u(x,F_ID)==mother)
    su(r,F_ID,eid); su(r,F_CLASSTYPE,PAL_CT); su(r,F_TAB,GENERAL2); su(r,F_AE,0); su(r,F_TE,0)
    for k in range(F_SPELLS,F_SPELLS+9): su(r,k,0)
    su(r,F_SPELLS,sp); su(r,F_LEVEL,lvl); su(r,27,lvl); su(r,28,lvl); su(r,F_GROUP,0); su(r,F_GATE,0)
    su(r,F_NAME,addstr(ca,nm)); su(r,F_DESC,0)
    su(r,F_REQ,mother); su(r,F_REQ+1,0); su(r,F_REQ+2,0)
    for k in range(15): sub_(r,B_CONNECTED+4*k,0)
    struct.pack_into("<f",r,B_POSX,9.0); struct.pack_into("<f",r,B_POSY,posy(mrow))
    out.append((r,None,not new))
# grava: substitui os existentes no lugar, acrescenta os novos no fim
existing_ids={u(r,F_ID) for r in ca["recs"]}
for r,q,was in out:
    if u(r,F_ID) in existing_ids:
        idx=next(i for i,x in enumerate(ca["recs"]) if u(x,F_ID)==u(r,F_ID)); ca["recs"][idx]=r
    else:
        ca["recs"].append(r)
for r,q,was in out:
    print(u(r,F_ID), S(ca,r,F_NAME).ljust(26), "pos(%g,%g)"%(posx(r),posy(r)), "nv",u(r,F_LEVEL), "ranks",len([x for x in (u(r,F_SPELLS+k) for k in range(5)) if x]),
          "TE",u(r,F_TE), "portao",u(r,F_GATE), "req",u(r,F_REQ), "lig",[ub(r,B_CONNECTED+4*k) for k in range(15) if ub(r,B_CONNECTED+4*k)], "existente" if was else "NOVO")
if APPLY: save("CharacterAdvancement.dbc",ca); print("gravado em",OUT)
