# uso: mk-classe.py <config.json>  -> cria gera_<classe>.py a partir do gera_rogue.py (modelo com todas as regras)
import json, re, sys
c=json.load(open(sys.argv[1])); s=open("modelo_classe.py").read()
a=s.index("KEYSTONES="); b=s.index("DUP_TALENT_NAMES="); e=s.index("\n",b)
ks="KEYSTONES={"+",\n           ".join('"%s":%s'%(k,json.dumps(v)) for k,v in c["keystones"].items())+"}"
ks=ks.replace("null","None")
s=s[:a]+ks+"\nAUTO_DEPS=%s\nDUP_TALENT_NAMES=%s"%(json.dumps(c.get("auto_deps",{})), "set(%s)"%json.dumps(c.get("dup",[])))+s[e:]
s=s.replace("DISPLAY={}","DISPLAY=%s"%json.dumps(c.get("display",{})),1)
s=s.replace("TRAINER_ONLY=set()","TRAINER_ONLY=set(%s)"%json.dumps(c.get("trainer_only",[])),1)
s=s.replace("BASELINE=set()","BASELINE=set(%s)"%json.dumps(c.get("baseline",[])),1)
s=s.replace("AUTO_DEPS_SPELL={}","AUTO_DEPS_SPELL=%s"%json.dumps(c.get("auto_deps_spell",{})),1)
s=re.sub(r"PAL_TYPE, TEMPLATE_TYPE, CLASS_TAB = \d+, 33, 87  # \w+","PAL_TYPE, TEMPLATE_TYPE, CLASS_TAB = %d, 33, 87  # %s"%(c["classtype"],c["label"]),s)
s=re.sub(r"PAL_TABS = \{.*\}","PAL_TABS = {%s}"%", ".join('%d:"%s"'%(int(t),n) for t,n in c["tabs"]),s)
s=re.sub(r"TEMPLATE_SPEC_TABS = \{.*?\}","TEMPLATE_SPEC_TABS = %s"%json.dumps(c["templates"]),s,count=1)
s=re.sub(r"NEW_BASE = \d+  # faixa do \w+ \(receita §8\)","NEW_BASE = %d  # faixa do %s (receita §8)"%(c["new_base"],c["label"]),s)
s=re.sub(r'SIGNATURE=\{.*',"SIGNATURE=%s"%json.dumps(c["signature"]),s)
s=re.sub(r'EXAMPLES=\{.*',"EXAMPLES=%s"%json.dumps({k:tuple(v) for k,v in c["examples"].items()}).replace("[","(").replace("]",")"),s)
a=s.index("DESC={"); e=s.index("}\n",a)+2
s=s[:a]+"DESC=%s\n"%json.dumps(c["desc"])+s[e:]
s=s.replace('S(specs,r,1)=="ROGUE"','S(specs,r,1)=="%s"'%c["chrspecs_class"]).replace('f"{DISPLAY.get(name,name)} Rogue"','f"{DISPLAY.get(name,name)} %s"'%c["label"])
s=s.replace("if u(r,2)==4 and not any","if u(r,2)==%d and not any"%c["essence_class"]).replace('report["Essence classe 4"]','report["Essence classe %d"]'%c["essence_class"])
if c["chrspecs_class"]!="ROGUE":
    for k in ['"ROGUE"',' Rogue"','u(r,2)==4 ']: assert k not in s, k
open(c["out"],"w").write(s); print(c["out"],"criado")
