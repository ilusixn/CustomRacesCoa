#!/bin/bash
# [ESTABILIZA-1210] Ordem obrigatoria depois de regenerar as classes classicas:
#   1) ./aplica_todas.sh   2) python3 gera_support.py <dbc> <out> --apply
#   3) este script: estabiliza (Support->General2, duplicadas desativadas) + trava de auditoria.
# Uso: ./fecha_regen.sh <dir_dbc_entrada> <dir_saida>
set -e
cd ${COA_ROOT:-.}
python3 estabiliza_arvores.py "$1" "$2" --apply
for f in Spell.dbc ChrClasses.dbc CharacterAdvancementClassTypes.dbc CharacterAdvancementTabTypes.dbc CharacterAdvancementEssence.dbc; do
  [ -e "$2/$f" ] || cp -p "$1/$f" "$2/$f"
done
# [CONGELA-SUPPORT 12/10] Support congelada: enquanto existir o arquivo SUPPORT_CONGELADA, desativa a 4a spec
if [ -e SUPPORT_CONGELADA ]; then python3 congela_support.py "$2" "$2" --apply; fi
python3 audita_duplicadas.py "$2/" --trava
