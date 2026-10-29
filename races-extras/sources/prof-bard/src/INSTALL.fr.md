# Installer mod-barde

Ce manuel installe le module sur un serveur AzerothCore (3.3.5a) et son addon sur le client. Prévoyez une demi-heure, plus la compilation. *English: [INSTALL.md](INSTALL.md).*

Il faut intervenir à trois endroits : le **serveur** (module, base de données, DBC, configuration), le **patch client** et l'**addon**. L'ordre ci-dessous évite les pièges.

## 0. Avant de commencer

- Faites une **sauvegarde** de vos bases (`acore_world`, `acore_characters`) et du dossier `Data/dbc` du serveur. Le script des DBC garde de lui-même une copie `*.pre-barde` de chaque fichier qu'il modifie, mais une sauvegarde complète reste prudente.
- Installez **Python 3** (aucune bibliothèque à ajouter).
- Vérifiez que les identifiants du barde sont libres chez vous : sorts et objets 1950001 à 1950999, améliorations d'objet 1950501 à 1950504, titres 226, 227 et 231 à 235 (lignes « Reuse » de `CharTitles.dbc`), objets de jeu 1950401 à 1950403.

```sql
SELECT entry, name FROM item_template WHERE entry BETWEEN 1950001 AND 1950999 AND name NOT LIKE '%cuivre%';
```

## 1. Le module

1. Placez le module dans `modules/` des sources AzerothCore, dans un dossier nommé **`mod-barde`** (le dépôt s'appelle `Prof-bard`, mais le module doit porter ce nom) :

```
git clone https://github.com/romaindnd89-cmd/Prof-bard.git modules/mod-barde
```
2. Compilez le serveur avec les modules statiques (`-DMODULES=static`), comme pour n'importe quel module AzerothCore. Sous Windows, `scripts/windows/BUILD_BARDE.bat` montre une compilation complète (Visual Studio, CMake, Ninja, vcpkg) : **adaptez les chemins**, il a été écrit pour un poste précis.

## 2. La base de données

Appliquez, dans cet ordre, les fichiers SQL (ou laissez l'updater d'AzerothCore le faire si `Updates.EnableDatabases` est actif) :

- base **world** : `data/sql/db-world/*.sql` (les cinq fichiers : monde, camp, instruments, airs, formateur) ;
- base **characters** : `data/sql/db-characters/mod_barde_characters.sql`.

Ces fichiers se rejouent sans danger.

**Le formateur.** Le métier s'apprend au Livre des artisans par le sort « Apprenti barde » (1950200). `mod_barde_world_trainer.sql` l'ajoute aux formateurs 200001 et 200002 du serveur d'origine : si ces numéros n'existent pas chez vous, remplacez-les par les vôtres, ou enseignez le métier à la main en jeu avec `.barde apprendre`.

## 3. La configuration

Copiez `conf/mod_barde.conf.dist` en `mod_barde.conf` dans le dossier des configurations de modules du serveur. Chaque réglage est commenté. Les plus utiles :

| Réglage | Rôle |
|---|---|
| `Barde.Enable` | active le module |
| `Barde.RequireProfession` | exige d'avoir appris le métier |
| `Barde.SkillMax` | compétence maximale (300) |
| `Barde.Buff.*` | durées, plafonds et crescendo des chants |
| `Barde.Camp.*`, `Barde.Concert.*` | camp, bourse, pourboires |
| `Barde.Spec.*`, `Barde.Ovation.*` | spécialités, ovations |

## 4. Les DBC et le patch client

Le client doit connaître les sorts, objets, améliorations, animations et titres du barde. L'outil les ajoute aux DBC du serveur et fabrique le patch client :

```
python tools/barde_dbc.py <dossier Data/dbc du serveur> <chemin de sortie>/patch-ZO.MPQ
```

- Il copie chaque DBC qu'il modifie en `*.pre-barde` (une seule fois) et se rejoue sans doublon.
- Si un numéro est déjà pris, il s'arrête et le dit : changez-le dans `tools/barde_dbc.py` (et dans les fichiers SQL).
- Le patch s'appelle `patch-ZO.MPQ` pour passer **après** les autres patchs du client. Si vous avez des patchs plus tard dans l'ordre alphabétique, renommez-le plus loin (`patch-ZP.MPQ`...).

**Redémarrez le serveur** après avoir modifié ses DBC.

## 5. Le client

**Fermez le jeu d'abord** : il garde le patch ouvert.

1. Copiez `patch-ZO.MPQ` dans le dossier `Data` du client.
2. Copiez `addon/Barde` dans `Interface\AddOns` du client.
3. Lancez le jeu. Un fichier d'addon ajouté ou un `.toc` modifié demande de **quitter complètement le jeu** puis de le relancer : un `/reload` ne relit pas la liste des fichiers.

Les sons (`addon/Barde/Sounds`) et les textures sont déjà fournis. Pour les régénérer : `python tools/gen_instrument_sounds.py addon/Barde/Sounds` et `python tools/gen_textures.py addon/Barde/Textures`.

## 6. Vérifier que tout marche

En jeu, avec un compte MJ :

1. `.barde luth` : un Luth de cuivre arrive dans le sac, avec son icône.
2. Apprenez le métier : par le formateur (Livre des artisans, « Apprenti barde ») ou avec `.barde apprendre`, puis cliquez sur « Barde » dans le grimoire (ou tapez `/barde`).
3. Choisissez un air, « Jouer » : les notes descendent, jouez avec D F J K, le chant arrive en fin de morceau.
4. `.barde competence 300` puis `.barde instruments 4 3` donnent la compétence maximale et les instruments d'arcanite épiques pour essayer l'atelier, les spécialités et la quête légendaire.

## Commandes MJ

| Commande | Accès | Effet |
|---|---|---|
| `.barde airs` | joueur | liste les airs |
| `.barde jouer <n> [entrainement]` | joueur | lance un air |
| `.barde luth` | MJ | donne le Luth de cuivre |
| `.barde instruments [palier] [qualité]` | MJ | donne les instruments (palier 0 à 4, qualité 0 à 4) |
| `.barde competence <valeur>` | MJ | fixe la compétence de barde |
| `.barde apprendre` | MJ | enseigne le métier de barde au joueur ciblé (ou à soi) |
| `.barde duo <air> [justesse %]` | MJ | duo de test avec un partenaire virtuel |
| `.barde legende tout` / `reset` | MJ | valide ou efface les étapes de la quête légendaire (essais) |
| `.barde aura <1 à 7>`, `.barde pose <1 à 3>` | MJ | essais de l'aura et des danses |
| `.barde recharger` | admin | recharge les airs et instruments depuis la base |

## En cas de problème

- **Une fenêtre d'erreur « Interface\AddOns\Barde\... »** : quittez complètement le jeu et relancez-le (fichier d'addon nouveau), puis envoyez le texte de l'erreur.
- **Des objets ou des icônes manquants (« ? »)** : le patch client n'est pas chargé. Vérifiez qu'il est dans `Data`, que son nom passe après les autres patchs, et que le jeu a été relancé.
- **Le serveur refuse de démarrer sur un numéro déjà pris** : changez le numéro, dans `tools/barde_dbc.py` et le SQL, et rejouez.
- **Un objet reçu n'apparaît pas dans le sac** : repassez par l'écran des personnages et reconnectez-vous.
- **« patch-ZO.MPQ » ne peut pas être copié** : le jeu est ouvert.
- **Une erreur de version MySQL à la compilation** : voir `scripts/windows/mysql-version.patch`, un contournement propre à un serveur précis.
