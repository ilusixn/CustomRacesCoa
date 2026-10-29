# Annonce Discord : le métier de barde

Textes prêts à coller. Discord coupe les messages à 2000 caractères : la version longue est découpée en messages numérotés, à poster dans l'ordre. Dépôt : https://github.com/romaindnd89-cmd/Prof-bard

---

## Version courte (français)

**Nouveau métier : le Barde** 🎶
Jouez des airs dans un mini-jeu de rythme (touches D F J K) et gagnez des **chants** : mana, vie, armure, vitesse, hâte, critique, résistances... Fabriquez vos instruments, jouez à deux ou trois pour partager vos chants avec le groupe, donnez des concerts, recevez des pourboires et des ovations, composez vos propres airs, et gravissez le prestige jusqu'à un **instrument légendaire unique**.
Dans le jeu : `/barde`, et le bouton **Aide** pour le guide complet.
Code et manuel d'installation : https://github.com/romaindnd89-cmd/Prof-bard

## Short version (English)

**New profession: the Bard** 🎶
Play songs in a rhythm mini-game (keys D F J K) and earn **chants**: mana, health, armor, speed, haste, crit, resistances... Craft your instruments, play as a duo or trio to share your chants with the group, give concerts, earn tips and ovations, compose your own songs, and climb the prestige ladder to a **one-of-a-kind legendary instrument**.
In game: `/barde`, and the **Help** button for the full guide.
Code and installation guide: https://github.com/romaindnd89-cmd/Prof-bard

---

## Version longue (français)

### Message 1/9 : le principe

**🎶 Le métier de Barde : tout ce qu'il apporte**

Le barde joue des airs dans un **mini-jeu de rythme** intégré au jeu. Des notes descendent vers quatre cercles : on appuie sur **D F J K** (ou on clique) quand elles les touchent. Chaque note est jugée *parfaite*, *bonne* ou *ratée*, avec un **combo** et un **score**. Les notes longues se tiennent.

À la fin du morceau, on reçoit le **chant** de l'instrument : une amélioration de quelques minutes, qui s'ajoute aux buffs de classe. Plus on joue juste, plus il est fort.

Le jeu de rythme tourne sur le PC du joueur (le décalage réseau le rendrait injouable), mais le **serveur vérifie tout** : durée, nombre de notes, combo, score. Un résultat impossible est refusé, et la justesse est recalculée côté serveur. Impossible de tricher.

Pour l'apprendre : Livre des artisans → « Apprenti barde » (gratuit), puis `/barde` ou le sort « Barde » du grimoire.

### Message 2/9 : les airs, la compétence, la justesse

**📜 40 airs, 5 niveaux, compétence de 1 à 300**
- Apprenti, Compagnon, Ménestrel, Maître, **Grand barde** : 8 airs par niveau. Plus l'air est dur, plus son chant est fort.
- La **compétence de barde** monte en jouant : un air réussi (70 % de justesse ou plus) peut donner un point, selon sa difficulté par rapport à votre niveau, et la première réussite d'un air en donne un de plus.

**🎯 La force du chant**
- Elle dépend de l'air, de l'instrument, de votre **justesse** et d'un **crescendo** : +2 % par série de 25 notes d'affilée, jusqu'à +12 %. Plafond : x1,2 de la valeur de base.
- Un chant dure **10 minutes** pour un air Apprenti, jusqu'à **30 minutes** pour un Grand barde.
- Sous **50 % de justesse**, c'est une **fausse note** : le chant de cet instrument est retiré, et un **malus** (-25 % de la valeur, 2 minutes) frappe le barde.
- Le mode **Entraînement** rejoue un air sans risque : tempo ralenti, pas de malus, chant plus faible, mais ni compétence ni maîtrise.

**🥇 Maîtrise de chaque air**
Bronze (5 réussites), argent (15), or (40) : **+1 % par palier** à la force du chant de cet air. Votre meilleur score est gardé, et le **classement** montre les cinq meilleurs du serveur pour chaque air.

### Message 3/9 : les dix instruments et ce qu'ils donnent

**🎻 Chaque instrument a son propre chant**
Valeurs en solo, du premier air du carnet au plus dur :
- 🪕 **Luth** : *Ballade de clarté* : **6 → 33 mana toutes les 5 s**
- 🎼 **Harpe** : *Berceuse* : **10 → 45 PV toutes les 5 s**
- 🎶 **Flûte** : *Marche* : **+5 → +12 % de vitesse de déplacement**
- 📯 **Cromorne** : *Chant de courage* : **+2 → +5 % de dégâts infligés**
- 🎻 **Vièle** : *Hymne protecteur* : **+150 → +1 200 d'armure**
- 🎹 **Piano** : *Psaume de garde* : **+10 → +60 de résistance à toutes les magies**
- 🎛️ **Orgue portatif** : *Air entraînant* : **+2 → +5 % de hâte** (mêlée, distance et sorts)
- 🪈 **Ocarina** : *Chant du chasseur* : **+1 → +4 % de chances de coup critique**
- 🥁 **Tympanon** : *Gigue d'endurance* : **+10 → +54 d'endurance**
- 🪘 **Tambour** : *Roulement de guerre* : **+40 → +250 de puissance d'attaque**

Un barde peut porter **jusqu'à 3 chants différents** en même temps (un par instrument). Rejouer le même instrument ne remplace le chant que s'il est plus fort. Le **duo** multiplie les valeurs par **1,6**, le **trio** par **2**.

### Message 4/9 : l'atelier

**🛠️ L'atelier du barde** (bouton « Atelier » du carnet)
- **5 paliers** d'instruments : cuivre, argent, mithril, thorium, arcanite. On les fabrique avec des matériaux, selon sa compétence ; le formateur offre les instruments de cuivre.
- **Qualité** tirée à la fabrication : commun, inhabituel (x1,05), rare (x1,10), **épique (x1,15)**, légendaire (x1,20). Une **gemme** du palier améliore les chances ; le **cristal arcanique** donne une chance d'épique à l'arcanite.
- **Accordage** : un instrument perd **4 % d'accord par morceau** (3 % le tambour). Sous 50 %, le chant faiblit jusqu'à x0,85. Une **clé d'accordage** remet à 100 % (5 clés = 1 barre de cuivre + 1 fil grossier). Un instrument légendaire ne se désaccorde jamais.

**🏕️ Le camp de barde**
Fabriqué à l'atelier (4 tissus de laine, 2 cuirs moyens, 1 silex et amadou). Il pose un **feu de cuisine** et **deux bancs** pendant **30 minutes** (un par zone). Près du feu, les instruments **s'accordent seuls**, on peut **cuisiner**, et les auditeurs reçoivent la **moitié du chant** du barde.

### Message 5/9 : jouer ensemble

**👥 Duo et trio**
- Le chef de groupe propose un air (bouton **Ensemble**). Les bardes du groupe proches, vivants, hors combat, qui connaissent le métier et ont assez de compétence reçoivent une invitation et choisissent **leur instrument** (deux bardes ne jouent pas le même).
- Une **fenêtre de préparation** montre une carte par barde : instrument, partie, et si chacun a répondu, puis un **compte à rebours de 5 secondes** avant le départ.
- Tout le monde joue **en même temps**, chacun avec ses touches. La **justesse du groupe** est la **moyenne** des bardes ; sous 50 %, personne n'a de chant.
- **Le chant de chaque barde va à tous les membres du groupe proches** (jusqu'à 100 mètres), pas seulement au barde. Un duo donne donc à tout le groupe les chants de deux instruments, multipliés par 1,6 ; un trio, trois chants multipliés par 2.
- Le **tambour** joue le **rythme**, les autres la **mélodie** ou l'**harmonie**.
- On **entend ses partenaires en direct**, et les joueurs proches (jusqu'à 8, dans un rayon de 7,5 m) entendent les notes du barde. Une **aura bleue** entoure le barde qui joue, et il danse son instrument en main. Une case permet de **couper le son des autres bardes**.

### Message 6/9 : concerts, pourboires, ovations

**🎭 Le concert et la bourse**
Jouer dans un **camp de barde**, une **taverne** ou une **ville** pose une **bourse** devant le barde pendant son concert (et quelques minutes après). Les spectateurs cliquent dessus pour laisser un **pourboire** de 10 pièces d'argent, 1 po ou 5 po. **Le barde reçoit tout**, sans prélèvement ni bonus créé. Le barde clique sur la bourse pour la ramasser ; s'il l'oublie, elle lui est envoyée par la poste.

**👏 Les ovations : à quoi ça sert**
Un spectateur qui n'a pas d'argent à donner peut **applaudir** : `/applaud` (ou `/applaudir`) près de la bourse, ou le menu de la bourse. Une ovation :
- **prolonge de 60 secondes le chant de tous les auditeurs** du concert (jusqu'à +5 minutes par concert) ;
- **compte pour le barde** : 100 ovations lui donnent le titre **l'Idole**.
Chaque spectateur n'applaudit **qu'une fois par concert**, et jamais depuis le **compte du barde**. Une ovation ne crée pas d'argent : l'argent, ce sont les pourboires des joueurs.

**👂 Les auditeurs**
Les spectateurs proches du barde reçoivent une **part de son chant** : **la moitié au camp**, **le quart en taverne ou en ville**. Les spécialités peuvent augmenter cette part.

**⚔️ Les duels**
Un barde défie un autre barde sur le même air devant un public : le **meilleur score gagne** les pourboires des deux bourses.

### Message 7/9 : improviser et composer

**🎹 Improvisation**
- Le bouton **Impro** ouvre un clavier libre (A S D F G H J K, ou la souris). On choisit l'instrument, le mode et la note de départ. **Tous les joueurs proches vous entendent en direct.**
- Le **mode expert** est un **clavier de piano complet de Sol 3 à Do 6** (30 notes) : Z X C V B N M, A S D F G H J K L, Q W, et Maj pour les dièses. De quoi **reproduire de vraies musiques**, note par note, avec le son de l'instrument.

**✍️ Composer**
- On **enregistre** ce qu'on joue (au moins 16 notes et 20 secondes), puis on le **compose** : le serveur place les notes sur une grille et fixe le niveau de l'air. L'air apparaît dans **« Mes créations »**.
- Un air composé (le sien ou acheté) **ne donne ni chant, ni compétence, ni malus** : on le joue pour le plaisir, le score et les concerts.
- On peut **copier un air sur un parchemin d'air** (1 parchemin léger + 1 encre) pour le **partager** : à donner ou à vendre. Le parchemin est à **usage unique** : le lire apprend l'air au lecteur (il va dans « Custom ») et le consomme. Une musique reproduite ou composée peut ainsi circuler de joueur en joueur, et chacun la rejoue ou la fait écouter à tous.

### Message 8/9 : titres et spécialités

**🏅 7 titres** (à choisir dans la fenêtre du personnage)
- **le Ménestrel** : gagner 10 airs différents du carnet
- **le Compositeur** : composer un air
- **Voix d'or** : recevoir 5 po de pourboires
- **Roi de la taverne** : gagner 10 duels (une victoire par adversaire et par jour)
- **le Virtuose** : maîtrise or sur 3 airs
- **l'Idole** : 100 ovations
- **Barde légendaire** : forger l'instrument légendaire

**🧭 3 spécialités** (dès la compétence 180 : premier choix gratuit, changement 10 po, aucune ne donne d'argent)
- **Barde de guerre** : +10 % sur les chants de combat (cromorne, vièle, piano, orgue, ocarina, tympanon, tambour)
- **Conteur** : +10 % sur luth, harpe et flûte, et un public qui profite **deux fois plus** du chant (camp 100 %, taverne 50 %)
- **Ménestrel de cour** : portée 7,5 m → 11 m, 16 auditeurs au lieu de 8, part du public x1,5, et un **applaudissement compte double** (+2 min, 2 ovations)

### Message 9/9 : l'instrument légendaire et le confort

**🌟 La quête « des Mille Voix »**
Un **instrument légendaire** : chant **x1,2**, **jamais désaccordé**, **lié à l'âme** (impossible à échanger, vendre ou acheter), et **un seul par personnage, pour toujours**. On choisit lequel de ses instruments d'arcanite épiques le devient. Cinq épreuves, toutes difficiles :
1. **Maîtrise or et un passage à 100 %** sur les **40 airs** du carnet ;
2. remettre **10 barres d'arcanite, 5 essences d'eau et 1 braise vivante** ;
3. **battre un virtuose en duel** ;
4. jouer un air de Grand barde en **trio parfait** : trois bardes, **100 % chacun** ;
5. **forger** l'instrument d'arcanite épique choisi.

**🧰 Confort**
- Addon **bilingue** (FR / US) en un clic, serveur en français.
- **Guide du métier** intégré (bouton **Aide**), en français et en anglais.

Dans le jeu : `/barde`. Code et manuel d'installation : https://github.com/romaindnd89-cmd/Prof-bard

---

## Long version (English)

### Message 1/9: the idea

**🎶 The Bard profession: everything it brings**

The bard plays songs in a **rhythm mini-game** built into the game. Notes fall toward four circles: press **D F J K** (or click) when they reach them. Each note is judged *perfect*, *good* or *missed*, with a **combo** and a **score**. Long notes must be held.

When the song ends you receive the instrument's **chant**: a few-minute buff that stacks with class buffs. The more accurate you are, the stronger it is.

The rhythm game runs on the player's PC (network lag would make it unplayable), but the **server checks everything**: duration, note counts, combo, score. An impossible result is rejected, and accuracy is recomputed server-side. No cheating.

To learn it: Crafting book → "Apprenti barde" (free), then `/barde` or the "Barde" spell in the spellbook.

### Message 2/9: songs, skill, accuracy

**📜 40 songs, 5 levels, skill from 1 to 300**
- Apprentice, Journeyman, Minstrel, Master, **Grand Bard**: 8 songs per level. The harder the song, the stronger its chant.
- **Bard skill** rises as you play: a successful song (70 % accuracy or more) can give a point, depending on its difficulty compared to your level, and the first success of a song gives one more.

**🎯 Chant strength**
- It depends on the song, the instrument, your **accuracy** and a **crescendo**: +2 % per streak of 25 notes, up to +12 %. Cap: x1.2 of the base value.
- A chant lasts **10 minutes** for an Apprentice song, up to **30 minutes** for a Grand Bard one.
- Under **50 % accuracy** it is a **wrong note**: that instrument's chant is removed, and a **malus** (-25 % of the value, 2 minutes) hits the bard.
- **Training** mode replays a song risk-free: slower tempo, no malus, weaker chant, but no skill or mastery.

**🥇 Mastery of each song**
Bronze (5 successes), silver (15), gold (40): **+1 % per tier** to that song's chant. Your best score is kept, and the **ranking** shows the server's five best for each song.

### Message 3/9: the ten instruments and what they give

**🎻 Each instrument has its own chant**
Solo values, from the first song of the book to the hardest:
- 🪕 **Lute**: *Ballad of Clarity*: **6 → 33 mana every 5 s**
- 🎼 **Harp**: *Lullaby*: **10 → 45 health every 5 s**
- 🎶 **Flute**: *March*: **+5 → +12 % movement speed**
- 📯 **Crumhorn**: *Song of Courage*: **+2 → +5 % damage dealt**
- 🎻 **Fiddle**: *Protective Hymn*: **+150 → +1,200 armor**
- 🎹 **Piano**: *Psalm of Warding*: **+10 → +60 resistance to all magic**
- 🎛️ **Portable organ**: *Rousing Air*: **+2 → +5 % haste** (melee, ranged and spells)
- 🪈 **Ocarina**: *Hunter's Song*: **+1 → +4 % critical strike chance**
- 🥁 **Timpani**: *Jig of Endurance*: **+10 → +54 stamina**
- 🪘 **Drum**: *War Drumroll*: **+40 → +250 attack power**

A bard can carry **up to 3 different chants** at once (one per instrument). Replaying the same instrument only replaces the chant if it is stronger. A **duo** multiplies the values by **1.6**, a **trio** by **2**.

### Message 4/9: the workshop

**🛠️ The bard's workshop** (the "Shop" button of the songbook)
- **5 tiers** of instruments: copper, silver, mithril, thorium, arcanite. They are crafted from materials, depending on your skill; the trainer gives the copper ones.
- **Quality** drawn at crafting: common, uncommon (x1.05), rare (x1.10), **epic (x1.15)**, legendary (x1.20). A tier **gem** improves the odds; the **Arcane Crystal** gives arcanite a chance to be epic.
- **Tuning**: an instrument loses **4 % tuning per song** (3 % the drum). Under 50 %, the chant weakens down to x0.85. A **tuning key** restores 100 % (5 keys = 1 copper bar + 1 coarse thread). A legendary instrument never goes out of tune.

**🏕️ The bard camp**
Crafted at the workshop (4 wool cloth, 2 medium leather, 1 flint and tinder). It sets a **cooking fire** and **two benches** for **30 minutes** (one per zone). Near the fire instruments **tune themselves**, you can **cook**, and listeners receive **half of the bard's chant**.

### Message 5/9: playing together

**👥 Duo and trio**
- The group leader proposes a song (the **Together** button). Nearby, living, out-of-combat bards of the group who know the profession and have enough skill get an invitation and choose **their instrument** (two bards cannot play the same one).
- A **preparation window** shows one card per bard: instrument, part, and whether each has answered, then a **5-second countdown** before the start.
- Everybody plays **at the same time**, each with their own keys. The **group accuracy** is the **average** of the bards; under 50 %, nobody gets a chant.
- **Each bard's chant goes to every nearby group member** (up to 100 yards), not only to the bard. A duo gives the whole group two instruments' chants, multiplied by 1.6; a trio, three chants multiplied by 2.
- The **drum** plays the **rhythm**, the others the **melody** or the **harmony**.
- You **hear your partners live**, and nearby players (up to 8, within 7.5 m) hear the bard's notes. A **blue aura** surrounds a playing bard, who dances with the instrument in hand. A box lets you **mute other bards**.

### Message 6/9: concerts, tips, ovations

**🎭 The concert and the purse**
Playing at a **bard camp**, a **tavern** or a **city** sets a **purse** in front of the bard for the concert (and a few minutes after). Spectators click it to leave a **tip** of 10 silver, 1 gold or 5 gold. **The bard receives all of it**, with no cut and no created bonus. The bard clicks the purse to collect it; if forgotten, it is mailed to them.

**👏 Ovations: what they are for**
A spectator with no money to give can **applaud**: `/applaud` near the purse, or the purse menu. An ovation:
- **lengthens the chant of every listener** at the concert by 60 seconds (up to +5 minutes per concert);
- **counts for the bard**: 100 ovations earn the title **the Idol**.
Each spectator applauds **only once per concert**, and never from the **bard's own account**. An ovation creates no money: the money is the players' tips.

**👂 Listeners**
Spectators near the bard receive a **share of the chant**: **half at a camp**, **a quarter in a tavern or city**. Specialties can increase this share.

**⚔️ Duels**
A bard challenges another on the same song in front of an audience: the **best score wins** both purses' tips.

### Message 7/9: improvising and composing

**🎹 Improvisation**
- The **Improv** button opens a free keyboard (A S D F G H J K, or the mouse). Pick the instrument, the mode and the starting note. **Every nearby player hears you live.**
- **Expert mode** is a **full piano keyboard from G3 to C6** (30 notes): Z X C V B N M, A S D F G H J K L, Q W, and Shift for sharps. Enough to **reproduce real music**, note by note, with the instrument's sound.

**✍️ Composing**
- **Record** what you play (at least 16 notes and 20 seconds), then **compose** it: the server places the notes on a grid and sets the song's level. The song appears in **"My creations"**.
- A composed song (yours or bought) gives **no chant, no skill and no malus**: it is played for pleasure, score and concerts.
- You can **copy a song onto a song scroll** (1 light parchment + 1 ink) to **share** it: give it or sell it. The scroll is **single-use**: reading it teaches the song to the reader (it goes to "Custom") and consumes it. A reproduced or composed piece can thus travel from player to player, and each one replays it or lets everybody hear it.

### Message 8/9: titles and specialties

**🏅 7 titles** (chosen in the character window)
- **the Minstrel**: win 10 different songs of the songbook
- **the Composer**: compose a song
- **Golden Voice**: receive 5 gold in tips
- **Tavern King**: win 10 duels (one win per opponent per day)
- **the Virtuoso**: gold mastery on 3 songs
- **the Idol**: 100 ovations
- **Legendary Bard**: forge the legendary instrument

**🧭 3 specialties** (from skill 180: first choice free, changing costs 10 gold, none gives money)
- **War Bard**: +10 % on combat chants (crumhorn, fiddle, piano, organ, ocarina, timpani, drum)
- **Storyteller**: +10 % on lute, harp and flute, and an audience that gains **twice as much** of the chant (camp 100 %, tavern 50 %)
- **Court Minstrel**: range 7.5 m → 11 m, 16 listeners instead of 8, audience share x1.5, and an **applause counts double** (+2 min, 2 ovations)

### Message 9/9: the legendary instrument and comfort

**🌟 The "Quest of a Thousand Voices"**
A **legendary instrument**: chant **x1.2**, **never out of tune**, **soulbound** (cannot be traded, sold or bought), and **one per character, for ever**. You choose which of your epic arcanite instruments becomes it. Five trials, all hard:
1. **Gold mastery and a 100 % run** on all **40 songs** of the book;
2. hand in **10 arcanite bars, 5 essences of water and 1 living ember**;
3. **beat a virtuoso in a duel**;
4. play a Grand Bard song as a **perfect trio**: three bards, **100 % each**;
5. **forge** the chosen epic arcanite instrument.

**🧰 Comfort**
- **Bilingual** addon (FR / US) in one click, server in French.
- Built-in **profession guide** (**Help** button), in French and English.

In game: `/barde`. Code and installation guide: https://github.com/romaindnd89-cmd/Prof-bard
