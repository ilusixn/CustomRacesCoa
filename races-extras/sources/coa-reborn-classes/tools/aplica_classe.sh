#!/bin/bash
# uso: aplica_classe.sh <gerador> <dir_src> <dir_out> <classtype> <abas(87,a,b,c)> <classe-minuscula> <spec1,spec2,spec3> "<classtypes anteriores>"
# gera, confere ids estaveis, limpa ligacoes, valida, compara classes anteriores, faz backup, instala servidor+patches e reinicia.
set -e
G=$1; SRC=$2; OUT=$3; CT=$4; TABS=$5; CL=$6; SPECS=$7; PREV=$8
cd ${COA_ROOT:-.}
rm -rf $OUT /tmp/out2 && mkdir -p $OUT
python3 $G $SRC $OUT --apply < /dev/null > /tmp/gera.log 2>&1 || { cat /tmp/gera.log; exit 1; }
grep -E "acima do nivel|poda classe|classe aba|ranks|ids:" /tmp/gera.log | cut -c1-200
python3 $G $SRC /tmp/out2 --apply < /dev/null > /dev/null 2>&1
A=$(md5sum $OUT/CharacterAdvancement.dbc | cut -c1-32); B=$(md5sum /tmp/out2/CharacterAdvancement.dbc | cut -c1-32)
[ "$A" = "$B" ] && echo "ids estaveis: OK" || { echo "ids estaveis: FALHOU"; exit 1; }
python3 ajusta_ligacoes.py $OUT/CharacterAdvancement.dbc --classtype $CT --tabs $TABS --apply | grep -E "^==" | cut -c1-140
V=$(python3 valida_classe.py $OUT/CharacterAdvancement.dbc --classtype $CT --tabs $TABS); echo "$V" | cut -c1-200
echo "$V" | grep -qE "isolados [1-9]|ligacao invalida [1-9]|para cima/lado [1-9]|REQ pendurado [1-9]|duplicados no DBC: [1-9]" && { echo "VALIDACAO FALHOU"; exit 1; }
echo "$V" | grep -E "alcancaveis" | awk -F"alcancaveis " '{split($2,a," "); split(a[1],b,"/"); if (b[1]!=b[2]) {print "ALCANCE FALHOU"; exit 1}}'
python3 - "$OUT" "$CT" "$PREV" <<'PY'
import struct, collections, sys
OUT,CT,PREV=sys.argv[1],int(sys.argv[2]),[int(x) for x in sys.argv[3].split()]
def load(p):
    b=open(p,"rb").read(); m,n,f,rs,ss=struct.unpack("<4sIIII",b[:20]); return [b[20+i*rs:20+(i+1)*rs] for i in range(n)], b[20+n*rs:]
u=lambda r,i: struct.unpack_from("<I",r,i*4)[0]; FL=lambda r: struct.unpack_from("<I",r,479)[0]
new,ns=load(OUT+"/CharacterAdvancement.dbc"); old,os_=load("env/dist/data/dbc/CharacterAdvancement.dbc")
S=lambda strs,r,i: strs[u(r,i):strs.find(b"\x00",u(r,i))].decode("utf8","replace")
sig=lambda rows,strs,ct: sorted((bytes(r[:47*4])+bytes(r[48*4:64*4])+bytes(r[65*4:]), S(strs,r,47)) for r in rows if u(r,32)==ct)
bad=[ct for ct in PREV if sig(new,ns,ct)!=sig(old,os_,ct)]
print("classes anteriores identicas:", "OK" if not bad else "FALHOU %s"%bad)
act=[r for r in new if u(r,32)==CT and not FL(r)&0x8]
sp=collections.Counter(u(r,i) for r in act for i in range(5,8) if u(r,i))
rep=[s for s,c in sp.items() if c>1 and not all(FL(r)&0x40000 or S(ns,r,1) in ("Talent","TalentAbility") for r in act if s in [u(r,i) for i in range(5,8)])]
names=[n for n,c in collections.Counter(S(ns,r,47) for r in act).items() if c>1]
spl,_=load("env/dist/data/dbc/Spell.dbc"); ids={u(r,0) for r in spl}; miss=[s for s in sp if s not in ids]
print("magias repetidas fora da assinatura:", rep or "nenhuma", "| nomes repetidos:", names or "nenhum", "| magias inexistentes:", miss or "nenhuma")
import os
if bad or miss or ((rep or names) and not os.environ.get("DUP_SO_AVISO")): sys.exit(1)
if rep or names: print("AVISO: duplicatas pre-existentes mantidas (DUP_SO_AVISO)")
PY
T=$(date +%Y%m%d-%H%M); mkdir -p env/dist/data/dbc.antes-$CL
for f in CharacterAdvancement.dbc ChrSpecs.dbc CharacterAdvancementEssence.dbc; do cp -a env/dist/data/dbc/$f env/dist/data/dbc.antes-$CL/$f; done
cp -a /tmp/AtlasInfo.lua backup/AtlasInfo.lua.antes-$CL-$T; echo "backups ok ($T)"
python3 - "$CL" "$SPECS" <<'PY'
import sys, re
cl,specs=sys.argv[1],sys.argv[2].split(","); p="/tmp/AtlasInfo.lua"; s=open(p).read()
last=[l for l in s.splitlines()[:200] if l.startswith('\t["talents-background-')][-1]
i=s.index(last); e=s.index("\n",i)+1; bs="\\\\"; add=""
for item in specs:
    sp,fn=(item.split(":")+[item])[:2]   # chave da spec : nome do arquivo bg-<classe>-<arquivo>
    if f'"talents-background-{cl}-{sp}"' not in s:
        add+='\t["talents-background-%s-%s"] = { "Interface%sAddOns%sAwAddons%sTextures%sCAOverhaul%s2%sBG%sbg-%s-%s", 512, 246, 0, 1, 0.26, 0.74, false, false },\n'%(cl,sp,bs,bs,bs,bs,bs,bs,bs,cl,fn)
s=s[:e]+add+s[e:]; open(p,"w").write(s)
print("fundos %s:"%cl, sum(1 for it in specs if f'"talents-background-{cl}-{it.split(":")[0]}"' in s), "| thumbnails:", sum(1 for it in specs if f'"spec-thumbnail-{cl}-{it.split(":")[0]}"' in s))
PY
for f in CharacterAdvancement.dbc ChrSpecs.dbc CharacterAdvancementEssence.dbc; do cp $OUT/$f env/dist/data/dbc/$f; done
(cd /tmp && ./mk.sh patch-ZZV.MPQ AtlasInfo.lua "Interface\SharedXML\AtlasInfo.lua" | tail -1)
(cd $OUT && /tmp/mk.sh patch-ZZW.MPQ CharacterAdvancement.dbc "DBFilesClient\CharacterAdvancement.dbc" ChrSpecs.dbc "DBFilesClient\ChrSpecs.dbc" CharacterAdvancementEssence.dbc "DBFilesClient\CharacterAdvancementEssence.dbc" | tail -1)
docker compose restart ac-worldserver 2>&1 | tail -1
ST=$(date -u +%Y-%m-%dT%H:%M:%SZ); for j in $(seq 1 40); do sleep 10; L=$(docker logs --since $ST ${WORLD_CONTAINER:-ac-worldserver} 2>&1 | sed "s/\x1b\[[0-9;]*m//g"); echo "$L" | grep -q "worldserver-daemon) ready" && { echo "=== READY ($((j*10)) s) ==="; break; }; done
echo "$L" | grep -E "Loaded [0-9]+ CoA talent entries|ABORT" | tail -1 | cut -c1-130
