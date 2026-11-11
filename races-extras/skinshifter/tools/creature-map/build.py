import sqlite3
db=sqlite3.connect('world.db')
db.executescript('''
DROP TABLE IF EXISTS cspell;
CREATE TABLE cspell(entry,spell,src);
INSERT INTO cspell SELECT CreatureID,Spell,'tpl' FROM creature_template_spell WHERE Spell>0;
INSERT INTO cspell SELECT entryorguid,action_param1,'sai' FROM smart_scripts WHERE source_type=0 AND entryorguid>0 AND action_type IN (11,85,86) AND action_param1>0;
INSERT INTO cspell SELECT c.id1,s.action_param1,'sai' FROM smart_scripts s JOIN creature c ON c.guid=-s.entryorguid WHERE s.source_type=0 AND s.entryorguid<0 AND s.action_type IN (11,85,86) AND s.action_param1>0;
DROP TABLE IF EXISTS spawn;
CREATE TABLE spawn AS SELECT id1 entry,count(*) n,
  sum(map=0) ek,sum(map=1) kal,sum(map=530) outl,sum(map=571) nr,sum(map not in (0,1,530,571)) inst FROM creature GROUP BY id1;
DROP TABLE IF EXISTS pool;
CREATE TABLE pool AS SELECT t.entry,t.name,t.subname,t.minlevel,t.maxlevel,t.rank,t.type,t.family,t.unit_class,t.faction,t.npcflag,t.unit_flags,t.flags_extra,s.n,s.ek,s.kal,s.outl,s.nr,s.inst
 FROM creature_template t JOIN spawn s ON s.entry=t.entry
 WHERE t.rank IN (0,1,2,4) AND (t.flags_extra & 128)=0 AND (t.unit_flags & 33554432)=0 AND (t.unit_flags & 2)=0 AND t.npcflag=0
   AND t.type IN (1,2,3,4,5,6,7,9,10) AND t.name NOT LIKE '%Trigger%' AND t.name NOT LIKE '%Bunny%' AND t.name NOT LIKE '%(%' ;
''')
db.commit()
TYPES={1:'Beast',2:'Dragonkin',3:'Demon',4:'Elemental',5:'Giant',6:'Undead',7:'Humanoid',9:'Mechanical',10:'Not specified'}
for r in db.execute('select type,count(*),sum(ek>0),sum(kal>0),sum(outl>0),sum(nr>0),sum(inst>0 and ek+kal+outl+nr=0),min(minlevel),max(maxlevel),sum(rank in (1,2)),sum(rank in (2,4)),(select count(distinct p2.entry) from pool p2 join cspell c on c.entry=p2.entry where p2.type=p.type) from pool p group by type'):
    print(TYPES[r[0]],r[1:])
