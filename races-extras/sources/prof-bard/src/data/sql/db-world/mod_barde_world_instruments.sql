-- mod-barde : les dix instruments, leurs cinq paliers (cuivre à arcanite) et leurs objets.
-- Fichier généré par tools/build_instruments_sql.py à partir de tools/instruments.json et tools/tiers.json : ne pas modifier à la main.
-- Les sorts de chant et de malus sont ajoutés au Spell.dbc du serveur et du client par tools/barde_dbc.py.
-- Valeurs de buff et repères de comparaison : docs/valeurs-des-buffs.md.

DELETE FROM `mod_barde_instrument` WHERE `id` IN (1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
INSERT INTO `mod_barde_instrument` (`id`, `name`, `sound`, `tier`, `item_entry`, `buff_name`, `effect`, `unit`, `buff_spell`, `buff_aura`, `buff_aura2`, `min_solo`, `max_solo`, `malus_name`, `malus_spell`) VALUES
(1, 'Luth de cuivre', 'luth', 0, 1950001, 'Ballade de clarté', 'mana toutes les 5 s', '', 1950101, 85, 0, 6, 33, 'Esprit troublé', 1950102),
(2, 'Harpe de cuivre', 'harpe', 0, 1950002, 'Berceuse', 'PV toutes les 5 s', '', 1950111, 84, 0, 10, 45, 'Nuit agitée', 1950112),
(3, 'Flûte de cuivre', 'flute', 0, 1950003, 'Marche', 'de vitesse de déplacement', '%', 1950121, 31, 0, 5, 12, 'Pas de plomb', 1950122),
(4, 'Cromorne de cuivre', 'crom', 0, 1950004, 'Chant de courage', 'de dégâts infligés', '%', 1950131, 79, 0, 2, 5, 'Courage brisé', 1950132),
(5, 'Vièle de cuivre', 'viele', 0, 1950005, 'Hymne protecteur', 'd''armure', '', 1950141, 22, 0, 150, 1200, 'Garde baissée', 1950142),
(6, 'Piano de cuivre', 'piano', 0, 1950006, 'Psaume de garde', 'de résistance aux magies', '', 1950151, 22, 0, 10, 60, 'Psaume maudit', 1950152),
(7, 'Orgue portatif de cuivre', 'orgue', 0, 1950007, 'Air entraînant', 'de hâte (mêlée, distance et sorts)', '%', 1950161, 192, 216, 2, 5, 'Cadence brisée', 1950162),
(8, 'Ocarina de cuivre', 'ocar', 0, 1950008, 'Chant du chasseur', 'de chances de coup critique', '%', 1950171, 52, 0, 1, 4, 'Main tremblante', 1950172),
(9, 'Tympanon de cuivre', 'tymp', 0, 1950009, 'Gigue d''endurance', 'd''endurance', '', 1950181, 29, 0, 10, 54, 'Souffle court', 1950182),
(10, 'Tambour de cuivre', 'tamb', 0, 1950010, 'Roulement de guerre', 'de puissance d''attaque', '', 1950191, 99, 124, 40, 250, 'Tempo perdu', 1950192);

-- Atelier : un palier donne un objet par instrument ; fabriquer demande la compétence indiquée et les matériaux
-- (aucun matériau : le formateur l'offre).
CREATE TABLE IF NOT EXISTS `mod_barde_tier` (
  `tier`      TINYINT UNSIGNED  NOT NULL,
  `suffix`    VARCHAR(24)       NOT NULL COMMENT 'de cuivre, d''argent...',
  `skill_req` SMALLINT UNSIGNED NOT NULL COMMENT 'compétence de barde pour le fabriquer',
  PRIMARY KEY (`tier`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
CREATE TABLE IF NOT EXISTS `mod_barde_tier_material` (
  `tier`       TINYINT UNSIGNED NOT NULL,
  `item_entry` INT UNSIGNED     NOT NULL,
  `count`      TINYINT UNSIGNED NOT NULL,
  PRIMARY KEY (`tier`, `item_entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
CREATE TABLE IF NOT EXISTS `mod_barde_tier_gem` (
  `tier`      TINYINT UNSIGNED NOT NULL,
  `gem_entry` INT UNSIGNED     NOT NULL COMMENT 'la gemme de qualité de ce palier (facultative à la fabrication)',
  PRIMARY KEY (`tier`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
CREATE TABLE IF NOT EXISTS `mod_barde_instrument_item` (
  `item_entry`    INT UNSIGNED     NOT NULL,
  `instrument_id` INT UNSIGNED     NOT NULL,
  `tier`          TINYINT UNSIGNED NOT NULL,
  PRIMARY KEY (`item_entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DELETE FROM `mod_barde_tier`;
INSERT INTO `mod_barde_tier` (`tier`, `suffix`, `skill_req`) VALUES
(0, 'de cuivre', 1),
(1, 'd''argent', 60),
(2, 'de mithril', 120),
(3, 'de thorium', 180),
(4, 'd''arcanite', 240);
DELETE FROM `mod_barde_tier_material`;
INSERT INTO `mod_barde_tier_material` (`tier`, `item_entry`, `count`) VALUES
(1, 2842, 2),
(1, 2592, 2),
(1, 2319, 2),
(2, 3860, 3),
(2, 4338, 2),
(2, 4234, 2),
(3, 12359, 3),
(3, 14047, 3),
(3, 8170, 2),
(4, 12360, 2),
(4, 14047, 4),
(4, 8170, 4);
DELETE FROM `mod_barde_tier_gem`;
INSERT INTO `mod_barde_tier_gem` (`tier`, `gem_entry`) VALUES
(0, 774),
(1, 1529),
(2, 3864),
(3, 7910),
(4, 12800);
DELETE FROM `mod_barde_instrument_item`;
INSERT INTO `mod_barde_instrument_item` (`item_entry`, `instrument_id`, `tier`) VALUES
(1950001, 1, 0),
(1951001, 1, 1),
(1952001, 1, 2),
(1953001, 1, 3),
(1954001, 1, 4),
(1950002, 2, 0),
(1951002, 2, 1),
(1952002, 2, 2),
(1953002, 2, 3),
(1954002, 2, 4),
(1950003, 3, 0),
(1951003, 3, 1),
(1952003, 3, 2),
(1953003, 3, 3),
(1954003, 3, 4),
(1950004, 4, 0),
(1951004, 4, 1),
(1952004, 4, 2),
(1953004, 4, 3),
(1954004, 4, 4),
(1950005, 5, 0),
(1951005, 5, 1),
(1952005, 5, 2),
(1953005, 5, 3),
(1954005, 5, 4),
(1950006, 6, 0),
(1951006, 6, 1),
(1952006, 6, 2),
(1953006, 6, 3),
(1954006, 6, 4),
(1950007, 7, 0),
(1951007, 7, 1),
(1952007, 7, 2),
(1953007, 7, 3),
(1954007, 7, 4),
(1950008, 8, 0),
(1951008, 8, 1),
(1952008, 8, 2),
(1953008, 8, 3),
(1954008, 8, 4),
(1950009, 9, 0),
(1951009, 9, 1),
(1952009, 9, 2),
(1953009, 9, 3),
(1954009, 9, 4),
(1950010, 10, 0),
(1951010, 10, 1),
(1952010, 10, 2),
(1953010, 10, 3),
(1954010, 10, 4);

-- Apparences prises dans le client (modèle et icône déjà présents) : luth 142067, guitare 142068, flûte 142021,
-- cor de chasse 142026, banjo 142069, guitare des morts-vivants 67653, cor du Chasseur de dragons 142028, flûte de pan 142020,
-- tambour 142035, grand tambour 142036. Un seul exemplaire par personnage et par palier.
DELETE FROM `item_template` WHERE `entry` IN (1950001, 1951001, 1952001, 1953001, 1954001, 1950002, 1951002, 1952002, 1953002, 1954002, 1950003, 1951003, 1952003, 1953003, 1954003, 1950004, 1951004, 1952004, 1953004, 1954004, 1950005, 1951005, 1952005, 1953005, 1954005, 1950006, 1951006, 1952006, 1953006, 1954006, 1950007, 1951007, 1952007, 1953007, 1954007, 1950008, 1951008, 1952008, 1953008, 1954008, 1950009, 1951009, 1952009, 1953009, 1954009, 1950010, 1951010, 1952010, 1953010, 1954010, 1950020);
INSERT INTO `item_template` (`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `BuyPrice`, `SellPrice`, `InventoryType`, `ItemLevel`, `RequiredLevel`, `stackable`, `maxcount`, `bonding`, `description`, `VerifiedBuild`, `MaxDurability`) VALUES
(1950001, 15, 4, 'Luth de cuivre', 142067, 1, 0, 0, 0, 1, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1951001, 15, 4, 'Luth d''argent', 142067, 2, 0, 0, 0, 20, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1952001, 15, 4, 'Luth de mithril', 142067, 3, 0, 0, 0, 40, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1953001, 15, 4, 'Luth de thorium', 142067, 3, 0, 0, 0, 60, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1954001, 15, 4, 'Luth d''arcanite', 142067, 4, 0, 0, 0, 75, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1950002, 15, 4, 'Harpe de cuivre', 142068, 1, 0, 0, 0, 1, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1951002, 15, 4, 'Harpe d''argent', 142068, 2, 0, 0, 0, 20, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1952002, 15, 4, 'Harpe de mithril', 142068, 3, 0, 0, 0, 40, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1953002, 15, 4, 'Harpe de thorium', 142068, 3, 0, 0, 0, 60, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1954002, 15, 4, 'Harpe d''arcanite', 142068, 4, 0, 0, 0, 75, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1950003, 15, 4, 'Flûte de cuivre', 142021, 1, 0, 0, 0, 1, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1951003, 15, 4, 'Flûte d''argent', 142021, 2, 0, 0, 0, 20, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1952003, 15, 4, 'Flûte de mithril', 142021, 3, 0, 0, 0, 40, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1953003, 15, 4, 'Flûte de thorium', 142021, 3, 0, 0, 0, 60, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1954003, 15, 4, 'Flûte d''arcanite', 142021, 4, 0, 0, 0, 75, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1950004, 15, 4, 'Cromorne de cuivre', 142026, 1, 0, 0, 0, 1, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1951004, 15, 4, 'Cromorne d''argent', 142026, 2, 0, 0, 0, 20, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1952004, 15, 4, 'Cromorne de mithril', 142026, 3, 0, 0, 0, 40, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1953004, 15, 4, 'Cromorne de thorium', 142026, 3, 0, 0, 0, 60, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1954004, 15, 4, 'Cromorne d''arcanite', 142026, 4, 0, 0, 0, 75, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1950005, 15, 4, 'Vièle de cuivre', 142069, 1, 0, 0, 0, 1, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1951005, 15, 4, 'Vièle d''argent', 142069, 2, 0, 0, 0, 20, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1952005, 15, 4, 'Vièle de mithril', 142069, 3, 0, 0, 0, 40, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1953005, 15, 4, 'Vièle de thorium', 142069, 3, 0, 0, 0, 60, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1954005, 15, 4, 'Vièle d''arcanite', 142069, 4, 0, 0, 0, 75, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1950006, 15, 4, 'Piano de cuivre', 57733, 1, 0, 0, 0, 1, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1951006, 15, 4, 'Piano d''argent', 57733, 2, 0, 0, 0, 20, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1952006, 15, 4, 'Piano de mithril', 57733, 3, 0, 0, 0, 40, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1953006, 15, 4, 'Piano de thorium', 57733, 3, 0, 0, 0, 60, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1954006, 15, 4, 'Piano d''arcanite', 57733, 4, 0, 0, 0, 75, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1950007, 15, 4, 'Orgue portatif de cuivre', 142028, 1, 0, 0, 0, 1, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1951007, 15, 4, 'Orgue portatif d''argent', 142028, 2, 0, 0, 0, 20, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1952007, 15, 4, 'Orgue portatif de mithril', 142028, 3, 0, 0, 0, 40, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1953007, 15, 4, 'Orgue portatif de thorium', 142028, 3, 0, 0, 0, 60, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1954007, 15, 4, 'Orgue portatif d''arcanite', 142028, 4, 0, 0, 0, 75, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1950008, 15, 4, 'Ocarina de cuivre', 142020, 1, 0, 0, 0, 1, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1951008, 15, 4, 'Ocarina d''argent', 142020, 2, 0, 0, 0, 20, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1952008, 15, 4, 'Ocarina de mithril', 142020, 3, 0, 0, 0, 40, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1953008, 15, 4, 'Ocarina de thorium', 142020, 3, 0, 0, 0, 60, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1954008, 15, 4, 'Ocarina d''arcanite', 142020, 4, 0, 0, 0, 75, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1950009, 15, 4, 'Tympanon de cuivre', 142035, 1, 0, 0, 0, 1, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1951009, 15, 4, 'Tympanon d''argent', 142035, 2, 0, 0, 0, 20, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1952009, 15, 4, 'Tympanon de mithril', 142035, 3, 0, 0, 0, 40, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1953009, 15, 4, 'Tympanon de thorium', 142035, 3, 0, 0, 0, 60, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1954009, 15, 4, 'Tympanon d''arcanite', 142035, 4, 0, 0, 0, 75, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1950010, 15, 4, 'Tambour de cuivre', 142036, 1, 0, 0, 0, 1, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1951010, 15, 4, 'Tambour d''argent', 142036, 2, 0, 0, 0, 20, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1952010, 15, 4, 'Tambour de mithril', 142036, 3, 0, 0, 0, 40, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1953010, 15, 4, 'Tambour de thorium', 142036, 3, 0, 0, 0, 60, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1954010, 15, 4, 'Tambour d''arcanite', 142036, 4, 0, 0, 0, 75, 1, 1, 1, 0, 'Instrument de barde. Tapez /barde pour jouer.', 12340, 100),
(1950020, 15, 0, 'Clé d''accordage', 10249, 1, 0, 0, 0, 1, 1, 20, 0, 0, 'Remet un instrument d''accord à 100 %. À utiliser depuis l''onglet Accordage de l''atelier du carnet.', 12340, 0);
