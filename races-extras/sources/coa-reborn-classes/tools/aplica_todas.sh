cd ${COA_ROOT:-.}
ALL="44 37 40 39 38 41 36 43 42"
while read CT BASE CLS ESS GEN TABS CL SPECS; do
  PREV=$(echo $ALL | tr " " "\n" | grep -vx $CT | tr "\n" " ")
  rm -rf fase2/regen-$CL; python3 prepara_regen.py $CT $BASE $CLS $ESS fase2/regen-$CL > /dev/null
  echo "######## $CL"
  DUP=""; [ $CL = paladin ] && DUP=1; DUP_SO_AVISO=$DUP ./aplica_classe.sh $GEN fase2/regen-$CL fase2/out-$CL $CT $TABS $CL $SPECS "$PREV" > /tmp/ap-$CL.log 2>&1; RC=$?
  grep -E "ids estaveis|identicas|repetidas|VALIDACAO|ALCANCE|FALHOU|Loaded [0-9]+ CoA" /tmp/ap-$CL.log | cut -c1-150
  grep -E "habilidades-base|ids preservados|dependencias automaticas" /tmp/gera.log | cut -c1-170
  [ $RC -ne 0 ] && { echo "PAROU em $CL (rc $RC)"; tail -5 /tmp/ap-$CL.log; exit 1; }
done <<'EOF'
44 140000 PALADIN 2 gera_paladino.py 7,14,25 paladin holy,protection,retribution
37 140200 WARRIOR 1 gera_warrior.py 87,6,5,7 warrior arms,fury,protection
40 140800 PRIEST 5 gera_priest.py 87,16,14,15 priest discipline,holy,shadow
39 141400 MAGE 8 gera_mage.py 87,11,13,12 mage arcane,fire,frost
38 140600 ROGUE 4 gera_rogue.py 87,10,8,9 rogue assassination,combat,subtlety
41 141600 WARLOCK 9 gera_warlock.py 87,17,18,19 warlock affliction,demonology,destruction
36 140400 HUNTER 3 gera_hunter.py 87,4,3,2 hunter beastmastery:beastmaster,marksmanship:marksman,survival
43 141200 SHAMAN 7 gera_shaman.py 87,23,24,20 shaman elemental,enhancement,restoration
42 141800 DRUID 11 gera_druid.py 87,22,21,20 druid balance,feral:cat,restoration
EOF
echo "TODAS APLICADAS"
