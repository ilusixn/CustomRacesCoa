import struct
W,PAL,H,R,PR,SH,M,LK,D = 1,2,3,4,5,7,8,9,11
HUMAN=[W,PAL,R,PR,M,LK]; DWARF=[W,PAL,H,R,PR]; NELF=[W,H,R,PR,D]; GNOME=[W,R,M,LK]
DRAENEI=[W,PAL,H,PR,SH,M]; ORC=[W,H,R,SH,LK]; UNDEAD=[W,R,PR,M,LK]; TAUREN=[W,H,SH,D]
TROLL=[W,H,R,PR,SH,M]; BELF=[PAL,H,R,PR,M,LK]
MATRIZ={
 1:HUMAN, 32:HUMAN,                      # Human, Thin Human
 3:DWARF, 48:[W,PAL,H,R,PR,SH,M,LK],     # Dwarf; Dark Iron (retail)
 27:DWARF, 68:DWARF, 69:DWARF,           # Earthen x3
 4:NELF, 7:GNOME, 67:[W,H,R,PR,M,LK],    # Night Elf, Gnome, Mechagnome
 11:DRAENEI, 62:[W,PAL,H,PR,M],          # Draenei, Lightforged
 13:[W,H,R,PR,M,LK,D], 47:[W,H,R,PR,M,LK,D],  # Worgen, Alpha Worgen
 14:[W,PAL,H,R,PR,M,LK],                 # High Elf
 15:[W,H,R,SH,M], 38:None,               # Tuskarr (38 NPC)
 16:[W,H,R,PR,SH,M,D],                   # Kul Tiran
 18:[W,H,R,SH],                          # Vrykul
 22:[W,H,PR,SH],                         # Broken
 29:[W,H,R,PR,SH,M], 20:[W,H,R,PR,SH,M], # Pandaren A/H
 30:[W,H,SH,M], 31:[W,H,SH,M],           # Murloc A/H
 54:[W,PR,M,LK], 58:[W,PR,M,LK],         # Dracthyr A/H
 59:[W,H,R,PR,M,LK],                     # Void Elf
 61:[W,H,R,LK], 60:[W,H,R,LK],           # Illidari NE / BE
 65:[W],                                 # Test Race
 71:[W,H,R,PR,D], 70:[W,H,R,PR,D],       # Haranir A/H
 77:[W,H,PR,SH,M], 72:[W,H,PR,SH,M],     # Skyborne A/H
 78:[W,H,PR,SH,M],                       # Jinyu
 80:[W,R,PR,M,LK], 81:[W,R,PR,M,LK],     # Darkfallen A/H
 82:[W,H,PR,M,LK],                       # Sethrak
 97:[W,H,R],                             # Saberon
 2:ORC, 49:[W,H,R,PR,SH,M], 23:[W,H,R,LK], 52:None,  # Orc, Mag har, Fel Orc
 5:UNDEAD, 26:UNDEAD, 51:[W,R,PR,M,LK],  # Undead, Skeleton, Undead Gnome
 6:TAUREN, 17:TAUREN, 66:TAUREN,         # Tauren, Taunka, Highmountain
 8:TROLL, 12:[W,PAL,H,R,PR,SH,M,D], 24:TROLL, 25:TROLL, 28:TROLL,  # Troll, Zandalari, Forest, Ice, Drakkari
 9:[W,H,R,PR,SH,M,LK], 57:[W,H,R,PR,SH,M,LK],  # Goblin, Alpha Goblin
 10:BELF, 63:[W,H,R,PR,M,LK],            # Blood Elf, Nightborne
 19:[W,H,R,PR,SH,M,LK], 74:[W,H,R,PR,SH,M,LK], # Vulpera x2
 21:[W,H,PR,M,LK], 35:None,              # Naga
 53:[W,SH,M,LK],                         # Ogre
 55:[W,PR,M,LK],                         # Eredar
 56:[PR,M,LK],                           # Harpy
 50:[W,H,D],                             # Furbolg
 83:[W,H,R],                             # Gnoll
}
CLASSIC={1,2,3,4,5,7,8,9,11}
# cliente: CharBaseInfo novo
d=open("/tmp/audit-mpq/CharBaseInfo.T.dbc","rb").read()
n,nf,rs,ss=struct.unpack("<4I",d[4:20])
keep=[]
removidas=0
for r in range(n):
    race,cls=d[20+r*rs],d[20+r*rs+1]
    if cls in CLASSIC:
        allowed=MATRIZ.get(race)
        if allowed is None or cls not in allowed: removidas+=1; continue
    keep.append(bytes((race,cls)))
out=struct.pack('<4s4I',b'WDBC',len(keep),nf,rs,ss)+b''.join(keep)+d[20+n*rs:]
open("/tmp/audit-mpq/CharBaseInfo.novo.dbc","wb").write(out)
print(f"CharBaseInfo: {n} -> {len(keep)} pares ({removidas} combinacoes classicas removidas)")
# servidor: SQL
sql=[]
for race,allowed in MATRIZ.items():
    al=set(allowed or [])
    proibidas=sorted(CLASSIC-al)
    if proibidas:
        sql.append(f"DELETE FROM playercreateinfo WHERE race={race} AND class IN ({','.join(map(str,proibidas))});")
open("/tmp/matriz_combos.sql","w").write("\n".join(sql)+"\n")
print("SQL com",len(sql),"deletes por raca")
