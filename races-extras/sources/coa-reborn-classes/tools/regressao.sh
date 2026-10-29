cd ${COA_ROOT:-.}
for a in "44 87,7,14,25" "37 87,6,5,7" "40 87,16,14,15" "39 87,11,13,12" "38 87,10,8,9" "41 87,17,18,19"; do
  set -- $a
  python3 ajusta_ligacoes.py env/dist/data/dbc/CharacterAdvancement.dbc --classtype $1 --tabs $2 | grep "^==" | awk -v c=$1 '{split($4,x,"->"); if (x[1]!=x[2]) print "MUDARIA", c, $0}'
  python3 valida_classe.py env/dist/data/dbc/CharacterAdvancement.dbc --classtype $1 --tabs $2 | grep -E "isolados [1-9]|invalida [1-9]|lado [1-9]"
done
echo "regressao concluida"
