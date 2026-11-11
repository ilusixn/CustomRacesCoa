import sqlite3,collections,csv,os
from common import *
os.makedirs('out',exist_ok=True)
pool=db.execute('select entry,name,subname,minlevel,maxlevel,rank,type,family,unit_class,n,ek,kal,outl,nr,inst from pool').fetchall()
sp=collections.defaultdict(set)
for e,s in db.execute('select distinct c.entry,c.spell from cspell c join pool p on p.entry=c.entry'): 
    if s in spells: sp[e].add(s)
RANK={0:'normal',1:'elite',2:'rare elite',4:'rare'}
arch=collections.Counter()
def archetype(ss):
    cats=[cat(spells[x]) for x in ss]
    heal='Heal' in cats
    caster=any(cat(spells[x])=='Damage' and spells[x][10]!=1 for x in ss)
    phys=any(cat(spells[x])=='Damage' and spells[x][10]==1 for x in ss)
    if not ss: return 'no spells'
    if heal: return 'healer'
    if caster and not phys: return 'caster'
    if caster and phys: return 'hybrid'
    if phys: return 'melee'
    return 'utility only'
with open('out/creature_pool.csv','w',newline='',encoding='utf8') as f:
    w=csv.writer(f);w.writerow(['entry','name','subname','type','family','minlevel','maxlevel','rank','unit_class','spawns','EK','Kalimdor','Outland','Northrend','Instances','archetype','spells (id:name:category)'])
    for r in sorted(pool,key=lambda r:(r[6],r[7],r[3])):
        ss=sorted(sp[r[0]]);a=archetype(ss);arch[(TYPES[r[6]],a)]+=1
        w.writerow([r[0],r[1],r[2] or '',TYPES[r[6]],fam.get(r[7],'') if r[6]==1 else '',r[3],r[4],RANK[r[5]],CLS.get(r[8],r[8]),*r[9:15],a,' | '.join(f'{x}:{spells[x][1]}:{cat(spells[x])}' for x in ss)])
cnt=collections.defaultdict(collections.Counter);lv=collections.defaultdict(list)
for r in pool:
    for x in sp[r[0]]: cnt[(r[6],x)][r[0]]+=1; lv[(r[6],x)].append(r[3])
with open('out/spells_by_type.csv','w',newline='',encoding='utf8') as f:
    w=csv.writer(f);w.writerow(['type','spell_id','name','rank','category','school_mask','creatures_using','min_creature_level','max_creature_level','example_creatures'])
    for (t,x),c in sorted(cnt.items(),key=lambda kv:(kv[0][0],-len(kv[1]))):
        names=[n for (e,n) in db.execute(f'select entry,name from pool where entry in ({",".join(map(str,list(c)[:4]))})')]
        w.writerow([TYPES[t],x,spells[x][1],spells[x][2],cat(spells[x]),spells[x][10],len(c),min(lv[(t,x)]),max(lv[(t,x)]),'; '.join(names)])
for t in TYPES:
    print(TYPES[t],{a:n for (tt,a),n in arch.items() if tt==TYPES[t]})
