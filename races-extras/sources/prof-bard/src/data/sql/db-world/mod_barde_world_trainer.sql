-- mod-barde : le métier s'apprend au Livre des artisans (formateurs 200001 et 200002, créatures 57500 et 57524),
-- comme les autres métiers. « Apprenti barde » (sort 1950200, ajouté au Spell.dbc par tools/barde_dbc.py)
-- enseigne le sort « Barde » (1950201). Gratuit, sans niveau ni compétence requis.

DELETE FROM `trainer_spell` WHERE `TrainerId` IN (200001, 200002) AND `SpellId` = 1950200;
INSERT INTO `trainer_spell` (`TrainerId`, `SpellId`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqAbility1`, `ReqAbility2`, `ReqAbility3`, `ReqLevel`, `VerifiedBuild`) VALUES
(200001, 1950200, 0, 0, 0, 0, 0, 0, 0, 0),
(200002, 1950200, 0, 0, 0, 0, 0, 0, 0, 0);
