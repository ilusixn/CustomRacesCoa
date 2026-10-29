-- mod-barde : le camp de barde (v0.4d). Rejouable.
-- Un feu de camp qui sert de foyer de cuisine (type 8, point de focalisation 4 comme les « Campfire » du monde, rayon 10), deux bancs
-- pour s'asseoir (type 7), et l'objet « Camp de barde » qui les pose (le sort 1950400 de l'objet est ajouté au Spell.dbc par barde_dbc.py ;
-- le module intercepte l'utilisation, voir BardeScripts.cpp).

DELETE FROM `gameobject_template` WHERE `entry` IN (1950401, 1950402);
INSERT INTO `gameobject_template` (`entry`, `type`, `displayId`, `name`, `IconName`, `castBarCaption`, `unk1`, `size`, `Data0`, `Data1`, `Data2`, `Data3`,
  `AIName`, `ScriptName`, `VerifiedBuild`) VALUES
(1950401, 8, 192, 'Feu de camp de barde', '', '', '', 1, 4, 10, 0, 0, '', '', 12340),
(1950402, 7, 622, 'Banc du camp de barde', '', '', '', 1, 2, 1, 0, 0, '', '', 12340);

DELETE FROM `item_template` WHERE `entry` = 1950030;
INSERT INTO `item_template` (`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `BuyPrice`, `SellPrice`, `InventoryType`, `ItemLevel`, `RequiredLevel`,
  `stackable`, `maxcount`, `bonding`, `description`, `spellid_1`, `spelltrigger_1`, `spellcharges_1`, `ScriptName`, `VerifiedBuild`) VALUES
(1950030, 15, 0, 'Camp de barde', 173, 1, 0, 0, 0, 1, 1, 20, 0, 0,
 'Pose un feu de camp et deux bancs pendant une demi-heure : on y cuisine, les instruments s''y accordent, les auditeurs y profitent du chant. Un camp par zone.',
 1950400, 0, 0, 'item_barde_camp', 12340);

-- La bourse d'un concert (v0.5) : un objet cliquable (type 10) dont le menu laisse un pourboire, ou ramasse la bourse pour le barde
-- (script go_barde_purse, voir BardeConcert.cpp).
DELETE FROM `gameobject_template` WHERE `entry` = 1950403;
INSERT INTO `gameobject_template` (`entry`, `type`, `displayId`, `name`, `IconName`, `castBarCaption`, `unk1`, `size`, `Data0`, `Data1`, `Data2`, `Data3`,
  `AIName`, `ScriptName`, `VerifiedBuild`) VALUES
(1950403, 10, 323, 'Bourse du barde', 'Speak', '', '', 1, 0, 0, 0, 0, '', 'go_barde_purse', 12340);

-- Le parchemin d'air (v0.6) : on copie une de ses compositions (1 Light Parchment, 1 Moonglow Ink), on le lit pour apprendre l'air, et il se vend
-- ou s'échange comme un objet. L'air qu'il porte est inscrit comme amélioration permanente de l'objet (script item_barde_scroll).
DELETE FROM `item_template` WHERE `entry` = 1950040;
INSERT INTO `item_template` (`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `BuyPrice`, `SellPrice`, `InventoryType`, `ItemLevel`, `RequiredLevel`,
  `stackable`, `maxcount`, `bonding`, `description`, `spellid_1`, `spelltrigger_1`, `spellcharges_1`, `ScriptName`, `VerifiedBuild`) VALUES
(1950040, 15, 0, 'Parchemin d''air', 218, 1, 0, 500, 0, 1, 1, 1, 0, 0,
 'Contient l''air d''un barde. Utilisé, il apprend l''air au lecteur (groupe Custom du carnet), puis il disparaît.',
 1950410, 0, 0, 'item_barde_scroll', 12340);
