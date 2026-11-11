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
