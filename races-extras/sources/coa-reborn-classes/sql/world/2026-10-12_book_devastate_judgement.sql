-- [BLOCO1-LIVRO 12/10] Devastate (identidade da Protection, vem da árvore) fora do livro no nível 1; Judgement 10321 (duplicata da árvore) fora do livro e das cidades
START TRANSACTION;
DELETE FROM trainer_spell WHERE TrainerId=900100 AND SpellId=1120243;
DELETE FROM trainer_spell WHERE SpellId=10321 AND TrainerId IN (3,4,5,6,900100);
COMMIT;
