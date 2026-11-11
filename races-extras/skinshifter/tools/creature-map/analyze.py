import sqlite3,collections,json
db=sqlite3.connect('world.db')
TYPES={1:'Beast',2:'Dragonkin',3:'Demon',4:'Elemental',5:'Giant',6:'Undead',7:'Humanoid',9:'Mechanical',10:'Not specified'}
CLS={1:'Warrior',2:'Paladin',4:'Rogue',8:'Mage',0:'?'}
fam={}
import struct
f=open('/mnt/user-data/uploads/CoA Server-Ogre-edits/Data/dbc/CreatureFamily.dbc','rb').read()
n,fc,rs,ss=struct.unpack('<4I',f[4:20]);sb=20+n*rs
for r in range(n):
    v=struct.unpack_from(f'<{fc}I',f,20+r*rs)
    # find name: field 10 is enUS name in 3.3.5
    o=v[10];fam[v[0]]=f[sb+o:f.index(b'\0',sb+o)].decode()
def cat(s):
    id,name,rank,d,e1,e2,e3,a1,a2,a3,school,m1,m2,m3,t1,t2,t3=s[:17]
    E={e1,e2,e3};A={a1,a2,a3};M={m1,m2,m3};T={t1,t2,t3}
    if M&{1,5,7,9,10,12,13,14,17,18,24,30} or A&{5,6,7,12,26,27,95}: return 'CC'
    if E&{28,41,42,43,44,45,46,56,57,87,97,112}: return 'Summon'
    if E&{96,149,145,138,29,144}: return 'Mobility'
    if A&{16}: return 'Stealth'
    if E&{10,67,75,30} or A&{8,20}: return 'Heal'
    if A&{69,77,24}: return 'Shield/Immune'
    if E&{2,9,17,31,58,121,7} or A&{3,53,89}: return 'Damage'
    if A&{11,33,1,31,22,123,79}: 
        return 'Debuff' if T&{6,16,15,22,30,24,53,25} - {1} and 1 not in T else 'Buff'
    if A: return 'Buff' if T<= {1,0,21,30,31,20,22} else 'Debuff'
    return 'Other'
spells={r[0]:r for r in db.execute('select id,name,rank,descr,eff1,eff2,eff3,aura1,aura2,aura3,school,mech1,mech2,mech3,tgt1,tgt2,tgt3 from spell')}
rows=db.execute('select distinct p.entry,p.type,p.family,p.unit_class,c.spell from pool p join cspell c on c.entry=p.entry').fetchall()
out={}
for t in TYPES:
    sub=[r for r in rows if r[1]==t]
    cnt=collections.Counter();catc=collections.Counter();per=collections.defaultdict(set)
    for e,ty,fm,uc,sp in sub:
        s=spells.get(sp)
        if not s: continue
        cnt[(s[1])]+=1; c=cat(s); per[c].add(s[1])
    catc={k:len(v) for k,v in per.items()}
    out[TYPES[t]]={'categories':catc,'top':cnt.most_common(30)}
    print('==',TYPES[t],catc); print('  ',', '.join(f'{a}({b})' for a,b in cnt.most_common(30)))
print('== Beast families')
for r in db.execute('select family,count(*),min(minlevel),max(maxlevel) from pool where type=1 group by family order by 2 desc'):
    print(fam.get(r[0],r[0]),r[1:])
print('== unit_class per type')
for r in db.execute('select type,unit_class,count(*) from pool group by 1,2'):
    print(TYPES[r[0]],CLS.get(r[1],r[1]),r[2])
json.dump(out,open('spells_by_type.json','w'),indent=1)
