-- mod-barde : tables des airs et des instruments, instrument de départ et objet Luth.
-- Rejouable. Les deux tables ne contiennent que les données du module : elles sont recréées à chaque fois.

DROP TABLE IF EXISTS `mod_barde_song`;
CREATE TABLE `mod_barde_song` (
  `id`        INT UNSIGNED      NOT NULL,
  `name`      VARCHAR(40)       NOT NULL,
  `subtitle`  VARCHAR(64)       NOT NULL DEFAULT ''   COMMENT 'forme et mode : « lai, mode de ré »',
  `tier`      TINYINT UNSIGNED  NOT NULL DEFAULT 0    COMMENT '0 Apprenti, 1 Compagnon, 2 Ménestrel, 3 Maître, 4 Grand barde',
  `skill_req` SMALLINT UNSIGNED NOT NULL DEFAULT 1,
  `bpm`       SMALLINT UNSIGNED NOT NULL              COMMENT 'noires par minute, 130 maximum',
  `power`     FLOAT             NOT NULL              COMMENT '(rang dans le carnet - 1) / 39 : 0 = premier air, 1 = le plus dur',
  `notes`     TEXT              NOT NULL              COMMENT 'pas:midi[:durée] ; pas = double croche ; durée seulement pour les notes tenues',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `mod_barde_instrument`;
CREATE TABLE `mod_barde_instrument` (
  `id`          INT UNSIGNED     NOT NULL,
  `name`        VARCHAR(64)      NOT NULL,
  `sound`       VARCHAR(16)      NOT NULL COMMENT 'préfixe des notes sonores de l''addon (Sounds/<sound>_<midi>.wav)',
  `tier`        TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `item_entry`  INT UNSIGNED     NOT NULL,
  `buff_name`   VARCHAR(64)      NOT NULL,
  `effect`      VARCHAR(64)      NOT NULL COMMENT 'ce que donne le chant, après sa valeur : « mana toutes les 5 s »',
  `unit`        VARCHAR(2)       NOT NULL DEFAULT '' COMMENT '% quand la valeur est un pourcentage',
  `buff_spell`  INT UNSIGNED     NOT NULL COMMENT 'sort du barde (voir tools/barde_dbc.py) ; montant et durée fixés par le module',
  `buff_aura`   INT UNSIGNED     NOT NULL COMMENT 'type d''aura à régler dans ce sort (85 = MOD_POWER_REGEN)',
  `buff_aura2`  INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT 'second effet réglé à la même valeur (0 = aucun)',
  `min_solo`    FLOAT            NOT NULL COMMENT 'buff solo sur le premier air du carnet',
  `max_solo`    FLOAT            NOT NULL COMMENT 'buff solo sur l''air le plus dur',
  `malus_name`  VARCHAR(64)      NOT NULL,
  `malus_spell` INT UNSIGNED     NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Les airs sont dans mod_barde_world_songs.sql (généré à partir du prototype, tools/build_songs_sql.py).

-- Les instruments et leurs objets sont dans mod_barde_world_instruments.sql (généré, tools/build_instruments_sql.py).
