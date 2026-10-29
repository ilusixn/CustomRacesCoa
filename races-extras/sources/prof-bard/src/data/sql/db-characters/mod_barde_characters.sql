-- mod-barde : compétence, réussites et meilleurs scores par personnage et par air.

CREATE TABLE IF NOT EXISTS `mod_barde_char_song` (
  `guid`          INT UNSIGNED      NOT NULL,
  `song_id`       INT UNSIGNED      NOT NULL,
  `plays`         INT UNSIGNED      NOT NULL DEFAULT 0 COMMENT 'parties jouées hors entraînement',
  `wins`          INT UNSIGNED      NOT NULL DEFAULT 0 COMMENT 'parties à 50 % de justesse ou plus',
  `best_score`    INT UNSIGNED      NOT NULL DEFAULT 0,
  `best_accuracy` SMALLINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'pour mille',
  PRIMARY KEY (`guid`, `song_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `mod_barde_char_skill` (
  `guid`  INT UNSIGNED      NOT NULL,
  `skill` SMALLINT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'compétence de barde, 1 à Barde.SkillMax',
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Migration de la v0.4c (accordage) : les instruments fabriqués avant avaient une durabilité de 0, donc « complètement désaccordés ».
-- On les remet à 100, une seule fois (le marqueur évite de recommencer à chaque installation).
CREATE TABLE IF NOT EXISTS `mod_barde_migration` (
  `name` VARCHAR(40) NOT NULL,
  PRIMARY KEY (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
UPDATE `item_instance` SET `durability` = 100
 WHERE `itemEntry` BETWEEN 1950001 AND 1954010 AND `durability` = 0
   AND NOT EXISTS (SELECT 1 FROM `mod_barde_migration` WHERE `name` = 'tuning-0.4c');
INSERT IGNORE INTO `mod_barde_migration` (`name`) VALUES ('tuning-0.4c');

-- Bonus de concert déjà reçu par barde et par jour (plafond quotidien, voir BardeConcert.cpp). `day` : jours depuis le 1er janvier 1970.
CREATE TABLE IF NOT EXISTS `mod_barde_char_bonus` (
  `guid`   INT UNSIGNED NOT NULL,
  `day`    INT UNSIGNED NOT NULL,
  `copper` INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`guid`, `day`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Compositions des bardes (v0.6) : un air fait par un joueur à partir d'une improvisation. L'identifiant de l'air est 1000000 + id.
-- `notes` : « pas:note » séparés par des espaces (pas = double croche à 84 BPM, note = numéro MIDI).
CREATE TABLE IF NOT EXISTS `mod_barde_composition` (
  `id`      INT UNSIGNED  NOT NULL AUTO_INCREMENT,
  `guid`    INT UNSIGNED  NOT NULL COMMENT 'le compositeur',
  `title`   VARCHAR(40)   NOT NULL,
  `tier`    TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `notes`   TEXT          NOT NULL,
  `created` TIMESTAMP     NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  KEY `by_guid` (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Airs appris par parchemin (v0.6) : la composition d'un autre barde, lue sur un parchemin. Ils sont dans le groupe « Custom » du carnet.
CREATE TABLE IF NOT EXISTS `mod_barde_char_learned` (
  `guid`    INT UNSIGNED NOT NULL,
  `song_id` INT UNSIGNED NOT NULL,
  PRIMARY KEY (`guid`, `song_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Ce qu'un barde a fait (v1.0) : ovations reçues, pourboires reçus (cuivre), duels gagnés, spécialisation choisie. Sert aux titres.
CREATE TABLE IF NOT EXISTS `mod_barde_char_stats` (
  `guid`      INT UNSIGNED     NOT NULL,
  `ovations`  INT UNSIGNED     NOT NULL DEFAULT 0,
  `tips`      BIGINT UNSIGNED  NOT NULL DEFAULT 0 COMMENT 'pourboires reçus, en cuivre',
  `duels_won` INT UNSIGNED     NOT NULL DEFAULT 0,
  `spec`      TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 aucune, 1 barde de guerre, 2 ménestrel de cour, 3 conteur',
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Une victoire de duel ne compte qu'une fois par adversaire et par jour (jours depuis 1970).
CREATE TABLE IF NOT EXISTS `mod_barde_duel_log` (
  `guid`     INT UNSIGNED NOT NULL,
  `opponent` INT UNSIGNED NOT NULL,
  `day`      INT UNSIGNED NOT NULL,
  PRIMARY KEY (`guid`, `opponent`, `day`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- La quête des Mille Voix (v1.0) : les étapes franchies (matériaux remis, duel contre un virtuose, trio parfait) et l'instrument légendaire forgé (un seul par personnage).
CREATE TABLE IF NOT EXISTS `mod_barde_legend` (
  `guid`      INT UNSIGNED     NOT NULL,
  `delivered` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `duel`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `trio`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `forged`    TINYINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
