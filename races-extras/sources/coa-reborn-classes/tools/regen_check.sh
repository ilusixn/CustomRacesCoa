cd ${COA_ROOT:-.}
while read CT BASE CLS ESS GEN TABS; do
  rm -rf /tmp/rg-src /tmp/rg-out; mkdir -p /tmp/rg-out
  python3 prepara_regen.py $CT $BASE $CLS $ESS /tmp/rg-src > /dev/null
  python3 $GEN /tmp/rg-src /tmp/rg-out --apply < /dev/null > /tmp/rg.log 2>&1 || { echo "$GEN FALHOU"; tail -3 /tmp/rg.log; continue; }
  python3 ajusta_ligacoes.py /tmp/rg-out/CharacterAdvancement.dbc --classtype $CT --tabs $TABS --apply > /dev/null
  echo "== $GEN: $(grep -E "ids preservados" /tmp/rg.log | cut -c1-120)"
  python3 compara_ca.py env/dist/data/dbc/CharacterAdvancement.dbc /tmp/rg-out/CharacterAdvancement.dbc 1,47,64
  python3 compara_ca.py env/dist/data/dbc/ChrSpecs.dbc /tmp/rg-out/ChrSpecs.dbc 1,2,29,30,31,46
  python3 compara_ca.py env/dist/data/dbc/CharacterAdvancementEssence.dbc /tmp/rg-out/CharacterAdvancementEssence.dbc 0
done <<'EOF'
44 140000 PALADIN 2 gera_paladino.py 7,14,25
37 140200 WARRIOR 1 gera_warrior.py 87,6,5,7
40 140800 PRIEST 5 gera_priest.py 87,16,14,15
39 141400 MAGE 8 gera_mage.py 87,11,13,12
38 140600 ROGUE 4 gera_rogue.py 87,10,8,9
41 141600 WARLOCK 9 gera_warlock.py 87,17,18,19
36 140400 HUNTER 3 gera_hunter.py 87,4,3,2
43 141200 SHAMAN 7 gera_shaman.py 87,23,24,20
42 141800 DRUID 11 gera_druid.py 87,22,21,20
EOF
python3 ${COA_ROOT:-.}/audita_duplicadas.py ${1:-env/dist/data/dbc}/ --trava || exit 1  # [ESTABILIZA-1210]
