import struct,sqlite3
f=open('/mnt/user-data/uploads/CoA Server-Ogre-edits/Data/dbc/Spell.dbc','rb').read()
n,fc,rs,ss=struct.unpack('<4I',f[4:20]);base=20;sb=base+n*rs
def st(o):
    e=f.index(b'\0',sb+o);return f[sb+o:e].decode('utf8','replace')
db=sqlite3.connect('world.db');db.execute('DROP TABLE IF EXISTS spell')
db.execute('CREATE TABLE spell(id INTEGER PRIMARY KEY,name,rank,descr,eff1,eff2,eff3,aura1,aura2,aura3,school,mech1,mech2,mech3,tgt1,tgt2,tgt3,bp1,bp2,bp3,cast_idx,recovery,dur_idx,range_idx,mana,spellvisual,icon,attr0)')
rows=[]
for r in range(n):
    v=struct.unpack_from('<234I',f,base+r*rs); i=lambda k: v[k]; s=lambda k: struct.unpack('<i',struct.pack('<I',v[k]))[0]
    rows.append((v[0],st(v[136]),st(v[153]),st(v[170]),v[71],v[72],v[73],v[95],v[96],v[97],v[225],v[83],v[84],v[85],v[86],v[87],v[88],s(80),s(81),s(82),v[28],v[29],v[40],v[46],v[41+1],v[131],v[133],v[4]))
db.executemany('INSERT INTO spell VALUES('+','.join('?'*28)+')',rows);db.commit()
for q in [133,116,6788,2054,17]:
    print(db.execute('select id,name,eff1,aura1,school,bp1,cast_idx,range_idx,mana,icon from spell where id=?',(q,)).fetchone())
