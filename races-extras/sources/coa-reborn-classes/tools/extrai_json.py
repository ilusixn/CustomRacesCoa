# extrai a configuracao de um gera_<classe>.py antigo para JSON do mk-classe
import re, ast, json, sys
p,out,label,cls,ess=sys.argv[1],sys.argv[2],sys.argv[3],sys.argv[4],int(sys.argv[5])
s=open(p).read()
def lit(name):
    i=s.index("\n"+name+"=")+1 if ("\n"+name+"=") in s else s.index("\n"+name+" = ")+1
    j=s.index("=",i)+1; depth=0; k=j
    while True:
        c=s[k]
        if c in "{([": depth+=1
        if c in "})]": depth-=1
        if c=="\n" and depth==0 and s[j:k].strip(): break
        k+=1
    txt=s[j:k].split("  #")[0].strip()
    return ast.literal_eval(txt)
ks=lit("KEYSTONES"); tabs=lit("PAL_TABS"); tmpl=lit("TEMPLATE_SPEC_TABS"); sig=lit("SIGNATURE"); ex=lit("EXAMPLES"); desc=lit("DESC")
dup=list(lit("DUP_TALENT_NAMES")) if "DUP_TALENT_NAMES=" in s else []
ct=int(re.search(r"PAL_TYPE, TEMPLATE_TYPE, CLASS_TAB = (\d+)",s).group(1)); nb=int(re.search(r"NEW_BASE = (\d+)",s).group(1))
json.dump({"out":out,"label":label,"classtype":ct,"chrspecs_class":cls,"essence_class":ess,"new_base":nb,
  "tabs":[[int(k),v] for k,v in tabs.items()],"templates":tmpl,"keystones":ks,"dup":dup,"signature":sig,
  "examples":{k:list(v) for k,v in ex.items()},"desc":desc}, open(out.replace(".py",".json").replace("gera_",""),"w"), indent=1)
print("json de", p)
