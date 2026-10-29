// À exécuter dans la console du navigateur, avec le prototype ouvert (docs/carnet-du-barde.html) servi
// par tools/collect_server.py. Envoie les 40 airs du prototype au serveur de collecte.
//
// Chaque note : [pas, midi, durée, accord, colonne]
//   pas     position en doubles croches depuis le début
//   midi    hauteur
//   durée   en doubles croches pour une note tenue, 0 sinon (le serveur la garde 80 % de sa durée)
//   accord  1 pour une note d'accord (octave sous la mélodie), 0 sinon
//   colonne 1 à 4, de la gauche (graves) à la droite (aigus), telle que le prototype la calcule
(async () => {
  const airs = SONGS.map(s => ({
    id: s.id, name: s.name, subtitle: s.orig, tier: s.tier, bpm: s.bpm, req: s.req,
    notes: s.notes.map(n => [
      Math.round(n.t / s.spb * 4), n.p, n.end ? Math.round(n.dur / s.spb * 4) : 0, n.ch ? 1 : 0, n.lane + 1,
    ]),
  }));
  const response = await fetch("/save", { method: "POST", body: JSON.stringify(airs) });
  return `${airs.length} airs envoyés : ${await response.text()}`;
})();
