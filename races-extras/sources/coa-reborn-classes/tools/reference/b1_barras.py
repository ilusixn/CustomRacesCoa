# [BLOCO1-BARRAS 12/10] playercreateinfo_action das 9 clássicas -> magias realmente conhecidas (gêmeas 11xxxxx).
# Só leitura dos dados da auditoria (res.pkl / action.tsv); gera SQL. Uso: python3 b1_barras.py > saida.sql
import pickle, collections, sys
sys.path.insert(0, "/tmp/ini-audit")
B = pickle.load(open("/tmp/ini-audit/base.pkl", "rb")); SP = B["SP"]
RES, TALENT = pickle.load(open("/tmp/ini-audit/res.pkl", "rb"))
DEVASTATE = {20243, 1120243}
ACT = collections.defaultdict(list)
for l in open("/tmp/ini-audit/action.tsv"):
    p = l.split("\t")
    if len(p) >= 5: ACT[(int(p[0]), int(p[1]))].append((int(p[2]), int(p[3]), int(p[4])))
CLASSIC = [1, 2, 3, 4, 5, 7, 8, 9, 11]
def known_of(k):
    return {s for s, _ in RES[k]["sp"]} - DEVASTATE
def resolve(a, known):
    if a in known: return a
    twin = a + 1100000 if a < 1100000 else a - 1100000
    if twin in known: return twin
    nm = SP.get(a, {}).get("n")
    if nm:
        same = sorted(s for s in known if SP.get(s, {}).get("n") == nm)
        if len(same) == 1: return same[0]
    return None
upd, ins, left, stats = [], [], collections.Counter(), collections.Counter()
for (race, c) in sorted(RES):
    if c not in CLASSIC: continue
    known = known_of((race, c)); acts = ACT.get((race, c))
    if acts:
        for btn, a, t in acts:
            if t != 0 or a in known: continue
            r = resolve(a, known)
            if r: upd.append((race, c, btn, a, r)); stats["trocado"] += 1
            else: left[(c, a)] += 1
    else:
        # sem barra: modelo = raça original da mesma classe com mais botões resolvidos
        best = None
        for (r2, c2), a2 in ACT.items():
            if c2 != c or r2 > 11: continue
            row = [(b, resolve(x, known) if t == 0 else None, t, x) for b, x, t in a2]
            ok = [(b, s) for b, s, t, x in row if t == 0 and s]
            if not best or len(ok) > len(best[1]): best = (r2, ok)
        for b, s in best[1]:
            ins.append((race, c, b, s)); stats["inserido"] += 1
        stats["pares-sem-barra"] += 1
print("-- [BLOCO1-BARRAS] gerado de /tmp/ini-audit; rollback = bloco1 backup de playercreateinfo_action")
print("START TRANSACTION;")
for race, c, b, a, r in upd:
    print(f"UPDATE playercreateinfo_action SET action={r} WHERE race={race} AND class={c} AND button={b} AND action={a} AND type=0;")
for race, c, b, s in ins:
    print(f"INSERT INTO playercreateinfo_action (race,class,button,action,type) VALUES ({race},{c},{b},{s},0);")
print("COMMIT;")
print("--", dict(stats), file=sys.stderr)
nm = lambda s: SP.get(s, {}).get("n", "?")
print("-- sem equivalente conhecido (ficam como estao; o core ignora):", file=sys.stderr)
for (c, a), n in sorted(left.items()):
    print(f"--   classe {c} magia {a} {nm(a)} em {n} pares", file=sys.stderr)
