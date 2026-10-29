import os as _os; COA_ROOT = _os.environ.get("COA_ROOT", ".")  # [release] server tree root
# [BLOCO1-TREINADORES 12/10] Opção B: treinadores de classe das 9 clássicas vendem as MESMAS gêmeas 11xxxxx do livro 900100.
# Troca SpellId original -> gêmea (só quando o livro vende a gêmea), ReqAbility original -> gêmea; mantém custo/nível da cidade.
# Se o treinador já tiver a gêmea, a linha original sai (sem cobrança dupla). Uso: python3 b1_treinadores.py > saida.sql
import subprocess, sys, collections
def q(sql):
    out = subprocess.run(["docker", "exec", _os.environ.get("DB_CONTAINER", "ac-database"), "mysql", "-u"+_os.environ.get("DB_USER", "root"), "-p"+_os.environ.get("DB_PASS", ""), "-N", "-e", sql],
                         capture_output=True, text=True).stdout
    return [l.split("\t") for l in out.splitlines() if l]
CLASSIC = {1, 2, 3, 4, 5, 7, 8, 9, 11}
TR = {int(i): int(r) for i, t, r in q("select Id,Type,Requirement from acore_world.trainer where Type=0")}
TR = {i: r for i, r in TR.items() if r in CLASSIC}
rows = q("select TrainerId,SpellId,MoneyCost,ReqSkillLine,ReqSkillRank,ReqAbility1,ReqAbility2,ReqAbility3,ReqLevel from acore_world.trainer_spell")
BOOK = {int(r[1]) for r in rows if r[0] == "900100"}
TWIN = lambda s: s + 1100000 if 0 < s < 1100000 else s
por_tr = collections.defaultdict(set)
for r in rows: por_tr[int(r[0])].add(int(r[1]))
upd, dele, keep, stats = [], [], collections.Counter(), collections.Counter()
for r in rows:
    t, s = int(r[0]), int(r[1])
    if t not in TR: continue
    ab = [int(x) for x in r[5:8]]
    nab = [TWIN(a) if a and TWIN(a) in BOOK else a for a in ab]
    if s < 1100000 and TWIN(s) in BOOK:
        if TWIN(s) in por_tr[t]:
            dele.append((t, s)); stats["original removida (gemea ja existe)"] += 1
        else:
            upd.append((t, s, TWIN(s), nab)); stats["trocada pela gemea"] += 1
    elif nab != ab:
        upd.append((t, s, s, nab)); stats["so pre-requisito"] += 1
    else:
        keep[s < 1100000] += 1
print("-- [BLOCO1-TREINADORES] treinadores:", sorted(TR))
print("START TRANSACTION;")
for t, s in dele:
    print(f"DELETE FROM trainer_spell WHERE TrainerId={t} AND SpellId={s};")
for t, s, ns, ab in upd:
    print(f"UPDATE trainer_spell SET SpellId={ns}, ReqAbility1={ab[0]}, ReqAbility2={ab[1]}, ReqAbility3={ab[2]} WHERE TrainerId={t} AND SpellId={s};")
print("COMMIT;")
print("--", dict(stats), "| sem gemea no livro (ficam):", keep[True], "originais,", keep[False], "ja 11xxxxx", file=sys.stderr)
