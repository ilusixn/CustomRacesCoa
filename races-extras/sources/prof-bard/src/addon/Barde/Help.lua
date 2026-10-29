-- Barde : l'aide du métier, de A à Z (un bouton « Aide » du carnet). Les textes sont ici en français et en anglais.
local _, ns = ...

local Help = {}
ns.Help = Help

local frame, current = nil, 1

-- Chaque rubrique : un titre et un texte, dans les deux langues. |cffffd100...|r met en valeur ; \n saute une ligne.
local TOPICS = {
    {
        fr = "Bien débuter",
        en = "Getting started",
        frText = "|cffffd100Devenir barde|r\nOuvrez le Livre des artisans (les métiers), apprenez « Apprenti barde » : un nouveau sort « Barde » apparaît. Cliquez dessus, ou tapez /barde, pour ouvrir le carnet.\n\n"
            .. "|cffffd100Jouer un air|r\nChoisissez un instrument (menu en haut à gauche) et un air dans la liste, puis « Jouer ». Des notes descendent vers quatre cercles : appuyez sur D, F, J, K (ou cliquez) au moment où elles les touchent. Plus vous êtes juste, plus le chant est fort.\n\n"
            .. "|cffffd100Le chant|r\nQuand le morceau est fini, vous recevez le chant de l'instrument (une amélioration de quelques minutes : mana, armure, vitesse, hâte...). Sous 50 % de justesse, la fausse note se retourne contre vous : un malus à la place.\n\n"
            .. "|cffffd100Les chants se cumulent|r\nUn chant par instrument : vous pouvez en porter jusqu'à trois différents en même temps. Rejouer le même instrument remplace le chant seulement s'il est plus fort.\n\n"
            .. "|cffffd100Entraînement|r\nLe bouton « Entraînement » rejoue un air sans risque : pas de malus, un chant plus faible, mais aucune compétence ni maîtrise gagnée.",
        enText = "|cffffd100Becoming a bard|r\nOpen the Crafting book (professions) and learn \"Bard Apprentice\": a new \"Bard\" spell appears. Click it, or type /barde, to open the songbook.\n\n"
            .. "|cffffd100Playing a song|r\nPick an instrument (menu at the top left) and a song in the list, then \"Play\". Notes fall toward four circles: press D, F, J, K (or click) when they reach them. The more accurate you are, the stronger the chant.\n\n"
            .. "|cffffd100The chant|r\nWhen the song is over you receive the instrument's chant (a few-minute buff: mana, armor, speed, haste...). Under 50 % accuracy the wrong note turns against you: a malus instead.\n\n"
            .. "|cffffd100Chants stack|r\nOne chant per instrument: you can carry up to three different ones at once. Replaying the same instrument only replaces the chant if it is stronger.\n\n"
            .. "|cffffd100Training|r\nThe \"Training\" button replays a song with no risk: no malus, a weaker chant, but no skill or mastery gained.",
    },
    {
        fr = "Airs, compétence, maîtrise",
        en = "Songs, skill, mastery",
        frText = "|cffffd100Les airs|r\nLe carnet contient 40 airs, classés en cinq niveaux : Apprenti, Compagnon, Ménestrel, Maître, Grand barde. Plus l'air est dur, plus son chant est fort. Les airs en rouge demandent plus de compétence ou un meilleur instrument.\n\n"
            .. "|cffffd100La compétence de barde|r\nElle va de 1 à 300. Chaque air réussi (assez juste) la fait monter ; les airs difficiles font progresser plus vite que ceux que vous maîtrisez déjà.\n\n"
            .. "|cffffd100La maîtrise d'un air|r\nChaque air réussi compte pour sa maîtrise : bronze à 5 réussites, argent à 15, or à 40. Chaque palier ajoute +1 % à la force de son chant. Votre meilleur score est gardé, et le « Classement » montre les cinq meilleurs du serveur.",
        enText = "|cffffd100The songs|r\nThe songbook holds 40 songs in five levels: Apprentice, Journeyman, Minstrel, Master, Grand Bard. The harder the song, the stronger its chant. Red songs need more skill or a better instrument.\n\n"
            .. "|cffffd100Bard skill|r\nIt goes from 1 to 300. Each successful song (accurate enough) raises it; hard songs raise it faster than ones you already master.\n\n"
            .. "|cffffd100Mastery of a song|r\nEach successful song counts toward its mastery: bronze at 5 successes, silver at 15, gold at 40. Each tier adds +1 % to the strength of its chant. Your best score is kept, and the \"Ranking\" shows the server's five best.",
    },
    {
        fr = "Instruments et atelier",
        en = "Instruments and workshop",
        frText = "|cffffd100Les instruments|r\nLuth, harpe, flûte, cromorne, vièle, piano, orgue portatif, ocarina, tympanon et tambour. Chacun donne un chant différent (mana, armure, vitesse, hâte...). Le formateur de barde vous en offre en cuivre.\n\n"
            .. "|cffffd100Les cinq paliers|r\nCuivre, argent, mithril, thorium, arcanite : chaque palier est plus puissant. L'Atelier (bouton en haut à droite du carnet) les fabrique avec des matériaux, selon votre compétence.\n\n"
            .. "|cffffd100La qualité|r\nCommun, inhabituel (x1,05), rare (x1,10), épique (x1,15), légendaire (x1,20). Elle est tirée à la fabrication ; une gemme du palier (et le cristal pour l'arcanite) donne de meilleures chances.\n\n"
            .. "|cffffd100L'accordage|r\nUn instrument perd 4 % d'accord par morceau (3 % le tambour). Sous 50 %, le chant faiblit jusqu'à x0,85. Une clé d'accordage (5 clés = 1 barre de cuivre + 1 fil grossier) le remet à 100 % : onglet Accordage de l'Atelier. Un instrument légendaire ne se désaccorde jamais.\n\n"
            .. "|cffffd100Le camp de barde|r\nFabriqué à l'Atelier (4 tissus de laine, 2 cuirs moyens, 1 silex et amadou). Il pose un feu de cuisine et deux bancs pendant 30 minutes (un par zone). Près du feu, vos instruments s'accordent seuls, et les auditeurs profitent d'une part de votre chant.",
        enText = "|cffffd100Instruments|r\nLute, harp, flute, crumhorn, fiddle, piano, portable organ, ocarina, timpani and drum. Each gives a different chant (mana, armor, speed, haste...). The bard trainer gives you copper ones.\n\n"
            .. "|cffffd100The five tiers|r\nCopper, silver, mithril, thorium, arcanite: each tier is stronger. The Workshop (button at the top right of the songbook) makes them from materials, depending on your skill.\n\n"
            .. "|cffffd100Quality|r\nCommon, uncommon (x1.05), rare (x1.10), epic (x1.15), legendary (x1.20). It is drawn at crafting; a tier gem (and the crystal for arcanite) improves the odds.\n\n"
            .. "|cffffd100Tuning|r\nAn instrument loses 4 % tuning per song (3 % the drum). Under 50 %, the chant weakens down to x0.85. A tuning key (5 keys = 1 copper bar + 1 coarse thread) restores 100 %: Tuning tab of the Workshop. A legendary instrument never goes out of tune.\n\n"
            .. "|cffffd100The bard camp|r\nMade at the Workshop (4 wool cloth, 2 medium leather, 1 flint and tinder). It sets a cooking fire and two benches for 30 minutes (one per zone). Near the fire your instruments tune themselves, and listeners share part of your chant.",
    },
    {
        fr = "Jouer ensemble",
        en = "Playing together",
        frText = "|cffffd100Duo et trio|r\nLe bouton « Ensemble » invite un autre barde (groupe) : à deux, les chants sont multipliés par 1,6 ; à trois, par 2. Chacun joue son instrument en même temps ; la justesse du groupe est la moyenne, et le chant de chaque barde va à tout le groupe proche.\n\n"
            .. "|cffffd100Le tambour|r\nLe tambour joue le rythme, les autres instruments la mélodie ou l'harmonie : un ensemble réunit des parties différentes.\n\n"
            .. "|cffffd100S'entendre|r\nVos partenaires vous entendent en direct, et les joueurs proches (jusqu'à 8, à 7,5 m) entendent aussi vos notes. Une aura bleue entoure un barde qui joue ; il danse l'instrument en main.",
        enText = "|cffffd100Duo and trio|r\nThe \"Together\" button invites another bard (group): as a duo the chants are multiplied by 1.6; as a trio by 2. Everyone plays their instrument at the same time; the group's accuracy is the average, and each bard's chant goes to the whole nearby group.\n\n"
            .. "|cffffd100The drum|r\nThe drum plays the rhythm, other instruments the melody or the harmony: an ensemble joins different parts.\n\n"
            .. "|cffffd100Hearing each other|r\nYour partners hear you live, and nearby players (up to 8, within 7.5 m) also hear your notes. A blue aura surrounds a playing bard; they dance with the instrument in hand.",
    },
    {
        fr = "Concerts et ovations",
        en = "Concerts and ovations",
        frText = "|cffffd100La bourse|r\nQuand vous jouez dans un camp de barde, une taverne ou une ville, une bourse se pose devant vous pour la durée du concert (et quelques minutes après). Les spectateurs cliquent dessus pour vous laisser un pourboire : 10 pièces d'argent, 1 po ou 5 po. Les pourboires vont entièrement au barde ; il ramasse la bourse en cliquant dessus. Si vous l'oubliez, elle vous est envoyée par la poste.\n\n"
            .. "|cffffd100Les ovations|r\nUn spectateur peut aussi applaudir : /applaud (/applaudir en français) près de la bourse, ou le menu de la bourse. Chaque spectateur n'applaudit qu'une fois par concert, et jamais depuis le compte du barde. Un applaudissement :\n"
            .. "  - prolonge de 1 minute le chant des auditeurs du concert (jusqu'à 5 minutes de plus par concert) ;\n"
            .. "  - compte comme une ovation pour le barde : 100 ovations donnent le titre « l'Idole ».\nIl ne donne pas d'argent : l'argent, ce sont les pourboires des joueurs.\n\n"
            .. "|cffffd100Les auditeurs|r\nAu camp, les spectateurs proches reçoivent la moitié de votre chant ; en taverne, le quart. Les spécialités de barde (bouton Spécialité) peuvent augmenter cette part.",
        enText = "|cffffd100The purse|r\nWhen you play at a bard camp, a tavern or a city, a purse is set in front of you for the concert (and a few minutes after). Spectators click it to leave a tip: 10 silver, 1 gold or 5 gold. Tips go entirely to the bard; they collect the purse by clicking it. If you forget it, it is mailed to you.\n\n"
            .. "|cffffd100Ovations|r\nA spectator can also applaud: /applaud near the purse, or the purse menu. Each spectator applauds once per concert, and never from the bard's own account. An applause:\n"
            .. "  - lengthens the listeners' chant by 1 minute (up to 5 more minutes per concert);\n"
            .. "  - counts as an ovation for the bard: 100 ovations earn the title \"the Idol\".\nIt gives no money: the money is the players' tips.\n\n"
            .. "|cffffd100Listeners|r\nAt a camp, nearby spectators receive half of your chant; in a tavern, a quarter. Bard specialties (Specialty button) can increase this share.",
    },
    {
        fr = "Duels et classement",
        en = "Duels and ranking",
        frText = "|cffffd100Le duel|r\nLe bouton « Duel » défie un joueur proche, devant un public : vous jouez le même air, et le meilleur score l'emporte. Les musiciens font parler la salle : la bourse d'un duel reste aux gagnants.\n\n"
            .. "|cffffd100Le classement|r\nChaque air a son « Classement » : les cinq meilleurs scores du serveur.\n\n"
            .. "|cffffd100Roi de la taverne|r\nGagner 10 duels (une victoire par adversaire et par jour) donne le titre « Roi de la taverne ».",
        enText = "|cffffd100The duel|r\nThe \"Duel\" button challenges a nearby player in front of an audience: you play the same song and the best score wins. The duel's purse goes to the winners.\n\n"
            .. "|cffffd100The ranking|r\nEach song has its \"Ranking\": the server's five best scores.\n\n"
            .. "|cffffd100Tavern King|r\nWinning 10 duels (one win per opponent per day) earns the title \"Tavern King\".",
    },
    {
        fr = "Improviser et composer",
        en = "Improvising and composing",
        frText = "|cffffd100L'improvisation|r\nLe bouton « Impro » ouvre un clavier : jouez librement avec A S D F G H J K (ou la souris). Le menu de l'instrument change le son. Les joueurs proches vous entendent.\n\n"
            .. "|cffffd100Le mode expert|r\nUn clavier de piano complet, de Sol 3 à Do 6 (30 notes) : Z X C V B N M, A S D F G H J K L, Q W, et Maj pour les dièses.\n\n"
            .. "|cffffd100Composer|r\n« Enregistrer » garde vos notes (au moins 16 notes et 20 secondes), « Composer » en fait un air que vous nommez : il apparaît dans « Mes créations ». Un air composé (le vôtre ou acheté) ne donne ni chant, ni compétence, ni malus : on le joue pour le plaisir, le score et les concerts.\n\n"
            .. "|cffffd100Les parchemins d'air|r\nCopiez un de vos airs sur un parchemin d'air (1 parchemin léger + 1 encre). On le lit pour apprendre l'air, le parchemin est alors consommé. On peut le donner ou le vendre. Les airs appris vont dans « Custom ». Le titre « le Compositeur » récompense votre première composition.",
        enText = "|cffffd100Improvisation|r\nThe \"Impro\" button opens a keyboard: play freely with A S D F G H J K (or the mouse). The instrument menu changes the sound. Nearby players hear you.\n\n"
            .. "|cffffd100Expert mode|r\nA full piano keyboard, from G3 to C6 (30 notes): Z X C V B N M, A S D F G H J K L, Q W, and Shift for sharps.\n\n"
            .. "|cffffd100Composing|r\n\"Record\" keeps your notes (at least 16 notes and 20 seconds), \"Compose\" turns them into a song you name: it appears in \"My creations\". A composed song (yours or bought) gives no chant, no skill and no malus: it is played for pleasure, score and concerts.\n\n"
            .. "|cffffd100Song scrolls|r\nCopy one of your songs onto a song scroll (1 light parchment + 1 ink). Reading it teaches the song and consumes the scroll. It can be given or sold. Learned songs go to \"Custom\". The title \"the Composer\" rewards your first composition.",
    },
    {
        fr = "Titres",
        en = "Titles",
        frText = "Les titres du barde se gagnent en jouant, et se choisissent dans la fenêtre du personnage.\n\n"
            .. "|cffffd100le Ménestrel|r : gagner 10 airs différents du carnet.\n"
            .. "|cffffd100le Compositeur|r : composer un air.\n"
            .. "|cffffd100Voix d'or|r : recevoir 5 po de pourboires.\n"
            .. "|cffffd100Roi de la taverne|r : gagner 10 duels.\n"
            .. "|cffffd100le Virtuose|r : maîtrise or (40 réussites) sur 3 airs.\n"
            .. "|cffffd100l'Idole|r : recevoir 100 ovations.\n"
            .. "|cffffd100Barde légendaire|r : forger l'instrument légendaire (Atelier, bouton « Quête légendaire »; un seul par personnage, lié à l'âme).",
        enText = "Bard titles are earned by playing, and chosen in the character window.\n\n"
            .. "|cffffd100the Minstrel|r: win 10 different songs of the songbook.\n"
            .. "|cffffd100the Composer|r: compose a song.\n"
            .. "|cffffd100Golden Voice|r: receive 5 gold in tips.\n"
            .. "|cffffd100Tavern King|r: win 10 duels.\n"
            .. "|cffffd100the Virtuoso|r: gold mastery (40 successes) on 3 songs.\n"
            .. "|cffffd100the Idol|r: receive 100 ovations.\n"
            .. "|cffffd100Legendary Bard|r: forge the legendary instrument (Shop button, then Legendary quest; one per character, soulbound).",
    },
    {
        fr = "Spécialités",
        en = "Specialties",
        frText = "À partir de 180 de compétence de barde, le bouton « Spécialité » (en bas à gauche du carnet) permet de choisir une spécialité. Le premier choix est gratuit ; en changer coûte 10 po. Aucune voie ne donne d'argent.\n\n"
            .. "|cffffd100Barde de guerre|r\n+10 % sur les chants de combat : cromorne, vièle, piano, orgue, ocarina, tympanon et tambour.\n\n"
            .. "|cffffd100Ménestrel de cour|r\nPortée 7,5 m -> 11 m, 16 auditeurs au lieu de 8 ; part du public x1,5 (camp 75 %, taverne 37 %) ; un applaudissement compte double (+2 min, 2 ovations).\n\n"
            .. "|cffffd100Conteur|r\n+10 % sur les chants du luth, de la harpe et de la flûte ; part du public x2 (camp 100 %, taverne 50 %).",
        enText = "From 180 bard skill, the \"Specialty\" button (bottom left of the songbook) lets you choose a specialty. The first choice is free; changing it costs 10 gold. No path gives money.\n\n"
            .. "|cffffd100War Bard|r\n+10 % on the combat chants: crumhorn, fiddle, piano, organ, ocarina, timpani and drum.\n\n"
            .. "|cffffd100Court Minstrel|r\nRange 7.5 m -> 11 m, 16 listeners instead of 8; audience share x1.5 (camp 75 %, tavern 37 %); an applause counts double (+2 min, 2 ovations).\n\n"
            .. "|cffffd100Storyteller|r\n+10 % on the lute, harp and flute chants; audience share x2 (camp 100 %, tavern 50 %).",
    },
    {
        fr = "Astuces",
        en = "Tips",
        frText = "- /barde ouvre le carnet ; /applaud (ou /applaudir) applaudit près d'une bourse.\n"
            .. "- Le bouton FR | US (en haut, à gauche d'Atelier) change la langue de l'addon (le serveur reste en français).\n"
            .. "- Chaque instrument a son chant : jouez-en plusieurs pour cumuler jusqu'à trois chants.\n"
            .. "- Gardez des clés d'accordage, ou accordez au camp : un instrument désaccordé affaiblit le chant.\n"
            .. "- Les airs composés ne donnent pas de chant : pour votre compétence et vos chants, jouez les airs du carnet.\n"
            .. "- La case « Couper le son des autres bardes » (en bas du carnet) coupe les notes de vos partenaires et des bardes proches ; vos propres notes restent audibles.\n"
            .. "- Si un objet que vous venez de recevoir n'apparaît pas dans le sac, revenez à l'écran des personnages et reconnectez-vous.",
        enText = "- /barde opens the songbook; /applaud applauds near a purse.\n"
            .. "- The FR | US button (top, left of Workshop) changes the addon's language (the server stays in French).\n"
            .. "- Each instrument has its chant: play several to stack up to three chants.\n"
            .. "- Keep tuning keys, or tune at a camp: an out-of-tune instrument weakens the chant.\n"
            .. "- Composed songs give no chant: for your skill and chants, play the songbook's songs.\n"
            .. "- The \"Mute other bards\" box (bottom of the songbook) silences your partners' and nearby bards' notes; your own notes stay audible.\n"
            .. "- If an item you just received does not show in your bag, go back to the character screen and log in again.",
    },
}

local function Show(index)
    current = index
    local topic = TOPICS[index]
    local english = ns.IsEnglish()
    frame.text:SetText(english and topic.enText or topic.frText)
    frame.scroll:SetVerticalScroll(0)
    local height = frame.text:GetStringHeight() + 8
    frame.child:SetHeight(math.max(height, 10))
    for i, button in ipairs(frame.topics) do
        if i == index then button:LockHighlight() else button:UnlockHighlight() end
    end
end

local function Create()
    local f = CreateFrame("Frame", "BardeHelpFrame", UIParent)
    ns.Size(f, 640, 440)
    f:SetPoint("CENTER", UIParent, "CENTER", 0, 20)
    f:SetFrameStrata("DIALOG")
    f:SetToplevel(true)
    f:SetMovable(true)
    f:EnableMouse(true)
    f:SetClampedToScreen(true)
    f:RegisterForDrag("LeftButton")
    f:SetScript("OnDragStart", f.StartMoving)
    f:SetScript("OnDragStop", f.StopMovingOrSizing)
    f:SetBackdrop({
        bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background",
        edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
        tile = true, tileSize = 32, edgeSize = 32,
        insets = { left = 11, right = 12, top = 12, bottom = 11 },
    })
    f:Hide()
    tinsert(UISpecialFrames, "BardeHelpFrame")

    local header = f:CreateTexture(nil, "ARTWORK")
    ns.Size(header, 300, 64)
    header:SetPoint("TOP", f, "TOP", 0, 12)
    header:SetTexture("Interface\\DialogFrame\\UI-DialogBox-Header")
    local title = f:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    title:SetPoint("TOP", header, "TOP", 0, -14)
    title:SetText(ns.IsEnglish() and "Bard guide" or "Guide du barde")
    local close = CreateFrame("Button", nil, f, "UIPanelCloseButton")
    close:SetPoint("TOPRIGHT", -8, -8)

    -- La liste des rubriques, à gauche.
    f.topics = {}
    for i, topic in ipairs(TOPICS) do
        local button = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
        ns.Size(button, 172, 24)
        button:SetPoint("TOPLEFT", 22, -34 - (i - 1) * 28)
        button:SetText(ns.IsEnglish() and topic.en or topic.fr)
        button:SetScript("OnClick", function() Show(i) end)
        f.topics[i] = button
    end

    -- Le texte, à droite, avec sa barre de défilement.
    local scroll = CreateFrame("ScrollFrame", "BardeHelpScroll", f, "UIPanelScrollFrameTemplate")
    scroll:SetPoint("TOPLEFT", 204, -36)
    scroll:SetPoint("BOTTOMRIGHT", -34, 22)
    local child = CreateFrame("Frame", nil, scroll)
    ns.Size(child, 380, 10)
    scroll:SetScrollChild(child)
    local text = child:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    text:SetPoint("TOPLEFT", 0, 0)
    text:SetWidth(376)
    text:SetJustifyH("LEFT")
    text:SetJustifyV("TOP")
    f.scroll, f.child, f.text = scroll, child, text
    return f
end

function Help.Toggle()
    if frame and frame:IsShown() then
        frame:Hide()
        return
    end
    frame = frame or Create()
    frame:Show()
    Show(current)
end
