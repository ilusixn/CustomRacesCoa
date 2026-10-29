#!/usr/bin/env python3
import os as _os; COA_ROOT = _os.environ.get("COA_ROOT", ".")  # [release] server tree root
"""SOMENTE LEITURA. Quests de classe que ensinam magias -> relatorio (classe, magia, spell id, quest, nivel, treinador, CA, conflitos)."""
import struct, subprocess, collections
def q(db,sql): return [l.split("\t") for l in subprocess.run(["docker","exec",_os.environ.get("DB_CONTAINER", "ac-database"),"mysql","-u"+_os.environ.get("DB_USER", "root"),"-p"+_os.environ.get("DB_PASS", ""),db,"-N","-e",sql],capture_output=True,text=True).stdout.splitlines()]
def load(p):
    b=open(p,"rb").read(); m,n,f,rs,ss=struct.unpack("<4sIIII",b[:20]); return [b[20+i*rs:20+(i+1)*rs] for i in range(n)], b[20+n*rs:]
u=lambda r,i: struct.unpack_from("<I",r,i*4)[0]; FL=lambda r: struct.unpack_from("<I",r,479)[0]
sp,sps=load("env/dist/data/dbc/Spell.dbc"); sidx={u(r,0):r for r in sp}
sname=lambda i: (lambda r: sps[u(r,136):sps.find(b"\x00",u(r,136))].decode("utf8","replace") if r else "?")(sidx.get(i))
def learned(sid, depth=0):
    r=sidx.get(sid); out=[]
    if not r or depth>2: return out
    for k in range(3):
        eff=u(r,71+k); trig=u(r,116+k)
        if eff in (36,57) and trig: out.append(trig)          # LEARN_SPELL / LEARN_PET_SPELL
        elif eff in (64,) and trig: out+=learned(trig,depth+1) # TRIGGER_SPELL
    return out
CLS={1:"Warrior",2:"Paladin",3:"Hunter",4:"Rogue",5:"Priest",7:"Shaman",8:"Mage",9:"Warlock",11:"Druid"}
CT={1:37,2:44,3:36,4:38,5:40,7:43,8:39,9:41,11:42}
ca,ss=load("env/dist/data/dbc/CharacterAdvancement.dbc")
caS=lambda r,i: ss[u(r,i):ss.find(b"\x00",u(r,i))].decode("utf8","replace")
inCA=collections.defaultdict(dict)
for r in ca:
    if FL(r)&0x8: continue
    for cl,ct in CT.items():
        if u(r,32)==ct:
            kind="automatica" if FL(r)&0x100000 else ("talento" if u(r,15) else "pick")
            if u(r,33)!=87 and kind=="pick": kind="talento"
            for i in range(5,8):
                if u(r,i): inCA[cl][u(r,i)]=(u(r,0),caS(r,47),kind,u(r,33))
trainers=collections.defaultdict(set)
for tid,req,typ in q("acore_world","select Id,Requirement,Type from trainer"):
    if typ=="0" and int(req) in CLS: trainers[int(req)].add(int(tid))
tspell=collections.defaultdict(set)
for tid,sid,lvl in q("acore_world","select TrainerId,SpellId,ReqLevel from trainer_spell"):
    tspell[int(tid)].add(int(sid))
rows=q("acore_world","select t.Id,t.LogTitle,t.MinLevel,t.QuestLevel,t.RewardSpell,t.RewardDisplaySpell,coalesce(a.AllowableClasses,0),t.AllowableRaces from quest_template t left join quest_template_addon a on a.ID=t.Id where (t.RewardSpell<>0 or t.RewardDisplaySpell<>0)")
out=[]
for qid,title,minl,ql,rs,rds,ac,races in rows:
    ac=int(ac)
    cls=[c for c in CLS if ac & (1<<(c-1))] if ac else []
    if not cls or len(cls)>2: continue
    for c in cls:
        for wrap in {int(rs),int(rds)}-{0}:
            taught=learned(wrap) or [wrap]
            for s in taught:
                twin=s+1100000 if s<1100000 else s
                ca_hit=inCA[c].get(twin) or inCA[c].get(s)
                tr=[t for t in trainers[c] if s in tspell[t] or twin in tspell[t]]
                out.append((CLS[c],sname(s),s,twin,qid,title,int(minl),ca_hit,len(tr)))
seen=set()
print("| Classe | Habilidade | Spell ID (orig/gemea) | Quest | Nivel min. | No Character Advancement? | Em treinador? |")
print("|---|---|---|---|---|---|---|")
for c,n,s,tw,qid,title,ml,cah,ntr in sorted(out, key=lambda x:(x[0],x[6],x[1])):
    k=(c,s,qid)
    if k in seen: continue
    seen.add(k)
    cas=f"sim: {cah[2]} '{cah[1]}' (id {cah[0]}, aba {cah[3]})" if cah else "NAO"
    print(f"| {c} | {n} | {s} / {tw} | {qid} {title} | {ml} | {cas} | {'sim ('+str(ntr)+')' if ntr else 'nao'} |")
print(f"\nlinhas: {len(seen)}; treinadores de classe por classe:", {CLS[c]:len(t) for c,t in trainers.items()})
