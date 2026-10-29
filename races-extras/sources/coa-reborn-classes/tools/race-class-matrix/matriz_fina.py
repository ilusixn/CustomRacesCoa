import struct,re,sys
# classes CoA: 12 Barbarian 13 WitchDoctor 14 Felsworn 15 WitchHunter 16 Stormbringer 17 KnightXoroth
# 18 Guardian 19 Templar 20 Bloodmage 21 Ranger 22 Chronomancer 23 Necromancer 24 Pyromancer 25 Cultist
# 26 Starcaller 27 SunCleric 28 Tinker 29 Venomancer 30 Reaper 31 Primalist 32 Runemaster
S=lambda *a:set(a)
HUM=S(19,18,15,21,22,27,28); DWF=S(18,19,21,28,32,15); GNO=S(28,22,24,26,15)
NEL=S(21,18,26,31,19,15); DRA=S(19,27,26,18,22,32); WOR=S(15,21,30,18,31)
ELF=S(20,22,26,21,15); PAN=S(19,18,31,16,22); ORC=S(12,16,13,18,21)
TAU=S(12,31,16,13,18,27); TRO=S(13,16,12,29,31,21); UND=S(23,25,30,20,29,22)
COA={
 1:HUM, 32:HUM, 16:HUM|S(16),
 3:DWF, 48:DWF|S(24,20), 27:DWF|S(31), 68:DWF|S(31), 69:DWF|S(31),
 7:GNO, 67:GNO,
 4:NEL, 71:NEL, 70:NEL,
 11:DRA, 62:DRA,
 13:WOR, 47:WOR,
 14:ELF|S(27), 59:ELF|S(23), 63:ELF, 10:ELF|S(14,27), 60:ELF|S(14,30), 61:S(15,21,14,30),
 29:PAN, 20:PAN,
 19:S(28,21,13,29,24), 74:S(28,21,13,29,24),
 2:ORC, 49:ORC, 23:ORC|S(14,17,30), 52:ORC,
 6:TAU, 17:TAU, 66:TAU,
 8:TRO, 12:TRO|S(20), 24:TRO, 25:TRO, 28:TRO,
 5:UND, 51:UND|S(28), 26:UND, 80:UND, 81:UND,
 9:S(28,24,29,22,21), 57:S(28,24,29,22,21),
 55:S(14,17,25,23,20,24,22),
 21:S(16,20,29,18,25), 35:S(16,20,29,18,25),
 53:S(12,20,24,16,18),
 56:S(16,29,25,26),
 50:S(31,12,18,13),
 83:S(12,21,29,13), 97:S(12,21,29,13),
 30:S(13,16,29), 31:S(13,16,29),
 18:S(12,18,16,32,30), 38:S(12,18,16,32,30),
 15:S(31,16,18,13),
 22:S(13,31,16,29), 36:S(13,31,16,29),
 54:S(20,22,26,24,18), 58:S(20,22,26,24,18),
 72:S(16,26,27,19,18), 77:S(16,26,27,19,18),
 78:S(13,16,31,29),
 82:S(29,25,27,16,21),
 65:S(),
}
ALLCOA=set(range(12,33))
# 1) restaurar do backup todas as linhas CoA e aplicar os deletes novos
bk=sys.argv[1]
txt=open(bk).read()
m=re.search(r"INSERT INTO `playercreateinfo` VALUES (.+?);",txt,re.S)
rows=re.findall(r"\(([^)]+)\)",m.group(1))
ins=[]; dels={}
for row in rows:
    f=row.split(","); race,cls=int(f[0]),int(f[1])
    if cls in ALLCOA and race not in (26,57,65,74):
        if cls in COA.get(race,set()): ins.append("("+row+")")
with open("/tmp/matriz_fina.sql","w") as f:
    f.write("DELETE FROM playercreateinfo WHERE class BETWEEN 12 AND 32;\n")
    f.write("INSERT INTO playercreateinfo VALUES "+",".join(ins)+";\n")
print("playercreateinfo CoA: ",len(ins),"linhas permitidas")
# 2) CharBaseInfo: refaz a partir do atual (classicas ja certas), trocando so a parte CoA
d=open("/tmp/audit-mpq/CharBaseInfo.novo.dbc","rb").read()
n,nf,rs,ss=struct.unpack("<4I",d[4:20])
pares=set()
for r in range(n):
    race,cls=d[20+r*rs],d[20+r*rs+1]
    if cls in ALLCOA:
        if cls in COA.get(race,set()): pares.add((race,cls))
    else: pares.add((race,cls))
keep=sorted(pares)
out=struct.pack("<4s4I",b"WDBC",len(keep),nf,rs,ss)+b"".join(bytes(p) for p in keep)+d[20+n*rs:]
open("/tmp/audit-mpq/CharBaseInfo.novo.dbc","wb").write(out)
print("CharBaseInfo:",len(keep),"pares")
