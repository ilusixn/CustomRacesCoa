import struct
def load(p):
    b=open(p,"rb").read(); m,n,f,rs,ss=struct.unpack("<4sIIII",b[:20]); return [b[20+i*rs:20+(i+1)*rs] for i in range(n)], b[20+n*rs:]
u=lambda r,i: struct.unpack_from("<I",r,i*4)[0]
D="env/dist/data/dbc.antes-fase2"
ca,ss=load(f"{D}/CharacterAdvancement.dbc"); ct,cts=load("fase2/src/CharacterAdvancementClassTypes.dbc"); tt,tts=load("fase2/src/CharacterAdvancementTabTypes.dbc")
S=lambda strs,o: strs[o:strs.find(b"\x00",o)].decode("utf8","replace")
tab={u(r,0):S(tts,u(r,1)) for r in tt}
names={1:"Warrior",2:"Paladin",3:"Hunter",4:"Rogue",5:"Priest",6:"Death Knight",7:"Shaman",8:"Mage",9:"Warlock",11:"Druid"}
reborn={44:2,37:1,36:3,38:4,40:5,45:6,43:7,39:8,41:9,42:11}
print("# Habilidades e talentos acima do nivel 60 (classes classicas, dados originais do Reborn)\n")
print("Fonte: env/dist/data/dbc.antes-fase2/CharacterAdvancement.dbc (intocado). Gerado em 11/10/2026. Regra: nivel maximo do realm = 60; estas linhas ficam fora da arvore ativa e NAO sao apagadas nem rebaixadas.\n")
print("| Classe | Especializacao (aba) | Habilidade / talento | Tipo | Spell ID | Nivel original |")
print("|---|---|---|---|---|---|")
rows=sorted([r for r in ca if u(r,32) in reborn and u(r,26)>60], key=lambda r:(names[reborn[u(r,32)]],tab.get(u(r,33),""),u(r,26)))
for r in rows:
    tipo=S(ss,u(r,1)) or "Ability"
    print("| %s | %s | %s | %s | %d | %d |"%(names[reborn[u(r,32)]],tab.get(u(r,33),str(u(r,33))),S(ss,u(r,47)),tipo,u(r,5),u(r,26)))
print("\nTotal: %d linha(s)."%len(rows))
