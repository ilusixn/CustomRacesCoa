import struct
MARCIAIS={12,18,21,15,19}
ARCANAS={20,22,24,26,32,29}
SAGRADAS={27}
PROFANAS={14,17,23,25,30}
PRIMAIS={13,16,31}
TINKER={28}
COA=MARCIAIS|ARCANAS|SAGRADAS|PROFANAS|PRIMAIS|TINKER
BRUTAS={83,97,50,30,31,18,15}        # sem tradicao arcana
PROFANAS_R={5,51,80,81,55,60,61,26}  # racas profanas
LUZ={62,11}                           # racas da Luz
TEC={7,67,9,57,3,48,1,32,5,19,74}    # tecnologicas (Tinker)
def permite(race,cls):
    if cls in MARCIAIS: return True
    if cls in ARCANAS: return race not in BRUTAS
    if cls in SAGRADAS: return race not in PROFANAS_R
    if cls in PROFANAS: return race not in LUZ
    if cls in PRIMAIS: return race!=67
    if cls in TINKER: return race in TEC
    return True
d=open("/tmp/audit-mpq/CharBaseInfo.novo.dbc","rb").read()
n,nf,rs,ss=struct.unpack("<4I",d[4:20])
keep=[];rem=0
for r in range(n):
    race,cls=d[20+r*rs],d[20+r*rs+1]
    if cls in COA and not permite(race,cls): rem+=1; continue
    keep.append(bytes((race,cls)))
out=struct.pack("<4s4I",b"WDBC",len(keep),nf,rs,ss)+b"".join(keep)+d[20+n*rs:]
open("/tmp/audit-mpq/CharBaseInfo.novo.dbc","wb").write(out)
print(f"CharBaseInfo: {n} -> {len(keep)} ({rem} combinacoes CoA removidas)")
sql=[]
racas=set(range(1,33))|{47,48,49,50,51,53,54,55,56,57,58,59,60,61,62,63,65,66,67,68,69,70,71,72,74,77,78,80,81,82,83,97}
for race in sorted(racas):
    proib=sorted(c for c in COA if not permite(race,c))
    if proib: sql.append(f"DELETE FROM playercreateinfo WHERE race={race} AND class IN ({','.join(map(str,proib))});")
open("/tmp/matriz_coa.sql","w").write("\n".join(sql)+"\n")
print("SQL:",len(sql),"deletes")
