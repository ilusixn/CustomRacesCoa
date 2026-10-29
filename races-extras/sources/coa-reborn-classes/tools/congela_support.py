#!/usr/bin/env python3
"""[CONGELA-SUPPORT 12/10] Congela a 4a spec Support das classicas sem apagar nada:
 - ChrSpecs: tira as linhas 102-105 (Support Paladin/Priest/Druid/Shaman);
 - CharacterAdvancement: marca DISABLED (0x8) as linhas da aba General2 (29) dos classtypes 40/42/43/44;
 - SkillLineAbility: tira as linhas da pericia Support 11901.
Idempotente. Reativar = restaurar os DBC do backup (backups/support-congelamento-*/).
Uso: congela_support.py <dir_entrada> <dir_saida> [--apply] [--sla <SkillLineAbility.dbc do cliente> <saida>]"""
import struct, sys, os
SPECS={102,103,104,105}; CTYPES={40,42,43,44}; TAB=29; SKILL=11901
F_ID,F_CLASSTYPE,F_TAB=0,32,33; B_FLAGS,B_CONNECTED=479,415; DIS=0x8
def load(p):
    b=open(p,"rb").read(); m,n,f,rs,ss=struct.unpack("<4sIIII",b[:20])
    return dict(f=f,rs=rs,recs=[bytearray(b[20+i*rs:20+(i+1)*rs]) for i in range(n)],strs=b[20+n*rs:])
def save(p,d):
    open(p,"wb").write(struct.pack("<4sIIII",b"WDBC",len(d["recs"]),d["f"],d["rs"],len(d["strs"]))+b"".join(d["recs"])+d["strs"])
u=lambda r,i: struct.unpack_from("<I",r,i*4)[0]
def sla(src,dst,apply):
    d=load(src); antes=len(d["recs"]); d["recs"]=[r for r in d["recs"] if u(r,1)!=SKILL]
    print(f"SLA {src}: {antes-len(d['recs'])} linhas da pericia {SKILL} retiradas")
    if apply: save(dst,d)
args=[a for a in sys.argv[1:] if a!="--apply"]; APPLY="--apply" in sys.argv
if "--sla" in args:
    i=args.index("--sla"); sla(args[i+1],args[i+2],APPLY); args=args[:i]
if len(args)>=2:
    SRC,OUT=args[0],args[1]; os.makedirs(OUT,exist_ok=True)
    cs=load(f"{SRC}/ChrSpecs.dbc"); antes=len(cs["recs"]); cs["recs"]=[r for r in cs["recs"] if u(r,0) not in SPECS]
    print(f"ChrSpecs: {antes-len(cs['recs'])} specs Support retiradas ({antes}->{len(cs['recs'])})")
    # [ROLE-SHAMAN 12/10] pedido dele: Enhancement (spec 83) e so DPS - tira a marca Tank (campo 14 = Tank)
    for r in cs["recs"]:
        if u(r,0)==83 and u(r,14):
            struct.pack_into("<I",r,14*4,0); print("ChrSpecs 83 Enhancement: Tank retirado")  # ROLE_FIX
    ca=load(f"{SRC}/CharacterAdvancement.dbc"); alvo=set(); novas=0
    for r in ca["recs"]:
        if u(r,F_CLASSTYPE) in CTYPES and u(r,F_TAB)==TAB:
            alvo.add(u(r,F_ID)); fl=struct.unpack_from("<I",r,B_FLAGS)[0]
            if not fl&DIS: struct.pack_into("<I",r,B_FLAGS,fl|DIS); novas+=1
    fora=[u(r,F_ID) for r in ca["recs"] if u(r,F_ID) not in alvo and not struct.unpack_from("<I",r,B_FLAGS)[0]&DIS
          and any(struct.unpack_from("<I",r,B_CONNECTED+4*k)[0] in alvo for k in range(15))]
    print(f"CharacterAdvancement: {len(alvo)} linhas Support, {novas} desativadas agora; ligacoes de fora para a Support: {fora}")
    assert not fora, "linha ativa fora da Support aponta para a Support - parar"
    if os.path.exists(f"{SRC}/SkillLineAbility.dbc"): sla(f"{SRC}/SkillLineAbility.dbc", f"{OUT}/SkillLineAbility.dbc", APPLY)
    if APPLY:
        save(f"{OUT}/ChrSpecs.dbc",cs); save(f"{OUT}/CharacterAdvancement.dbc",ca); print("GRAVADO em",OUT)
