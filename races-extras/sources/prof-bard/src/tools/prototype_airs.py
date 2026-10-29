"""Extrait les airs du prototype « Carnet du Barde » au format de mod_barde_song.

Reprend à l'identique la notation, le tri et le compositeur à graine du prototype, pour que les
airs du module soient exactement ceux du prototype. Usage : python prototype_airs.py [nombre d'airs]

Sortie : les lignes SQL INSERT des premiers airs, dans l'ordre du carnet (palier, puis ordre d'origine).
Format des notes : pas:midi[:durée], pas = double croche ; la durée n'est donnée que pour les notes
tenues (au moins 1,5 temps et 0,8 s), à maintenir pendant 80 % de leur durée.
"""

import math
import sys

P = {
    "pavA": "A4:2 C5 B4 A4:2 E4:2 F4 G4 A4 B4 C5:2 B4:2 A4:2 G4 F4 E4:3 F4 G4:2 A4 G4 F4:2 E4:2 D4 E4 F4 D4 E4:4",
    "pavB": "C5:2 D5 E5 D5:2 C5 B4 C5 B4 A4 G4 A4:4 G4 A4 B4 G4 A4:4",
    "laiA": "D4:2 F4 A4:2 G4 F4 E4 D4 E4:3 D4:2 F4 A4:2 C5 B4 A4 G4 A4:3",
    "laiB": "D5:2 C5 A4:2 G4 F4:1.5 G4:.5 A4 E4:3 F4 G4 A4 G4:2 F4 E4 C4 E4 D4:3",
    "ronA": "A4:1 C5:.5 B4:1 A4:.5 G4:1 A4:.5 B4:1.5 C5:1 D5:.5 E5:1 C5:.5 D5:1.5 B4:1.5 A4:1 C5:.5 B4:1 A4:.5 G4:1 E4:.5 G4:1.5 A4:1 B4:.5 G4:1 E4:.5 A4:3",
    "ronB": "E5:1 E5:.5 D5:1 C5:.5 D5:1 C5:.5 B4:1.5 C5:1 B4:.5 A4:1 G4:.5 A4:1.5 E4:1.5 E5:1 E5:.5 D5:1 C5:.5 D5:1 E5:.5 D5:1.5 C5:1 B4:.5 G4:1 B4:.5 A4:3",
    "grV1": "A4 C5:2 D5 E5:1.5 F5:.5 E5 D5:2 B4 G4:1.5 A4:.5 B4 C5:2 A4 A4:1.5 G#4:.5 A4 B4:2 G#4 E4:2",
    "grV2": "A4 C5:2 D5 E5:1.5 F5:.5 E5 D5:2 B4 G4:1.5 A4:.5 B4 C5:1.5 B4:.5 A4 G#4:1.5 F#4:.5 G#4 A4:3",
}

# Airs Apprenti écrits à la main, dans l'ordre du prototype (palier 0).
TUNES = [
    {"n": "Lai du Chevalier perdu", "o": "lai, mode de ré", "bpm": 66, "u": 1, "seq": ["laiA", "laiB"]},
    {"n": "Rondeau de la Fontaine", "o": "rondeau, mode de la", "bpm": 70, "u": 1, "seq": ["ronA", "ronB", "ronA"]},
    {"n": "Greensleeves", "o": "lente, Angleterre XVIe s.", "bpm": 66, "u": 1, "seq": ["grV1", "grV2"]},
    {"n": "Pavane des Brumes", "o": "pavane, mode de la", "bpm": 64, "u": 1, "seq": ["pavA", "pavB"]},
]
# Airs Apprenti composés par le prototype : nom, mesure, mode, bpm, index dans sa liste GEN.
GEN_APPRENTI = [
    ("Ballade du Gué", "3", "re", 68, 0),
    ("Complainte du Moine errant", "4", "la", 64, 1),
    ("Ronde des Lanternes", "6", "fa", 72, 2),
    ("Chanson de la Fileuse", "3", "sol", 70, 3),
]
TOTAL_SONGS = 40        # taille du carnet du prototype : base du buff = lo + (hi - lo) * (id - 1) / 39

MODES = {
    "re": {"n": "mode de ré", "t": 62, "s": [0, 2, 3, 5, 7, 9, 10], "lo": -1, "hi": 9},
    "la": {"n": "mode de la", "t": 57, "s": [0, 2, 3, 5, 7, 8, 10], "lo": 0, "hi": 10},
    "mi": {"n": "mode de mi", "t": 64, "s": [0, 1, 3, 5, 7, 8, 10], "lo": -1, "hi": 8},
    "sol": {"n": "mode de sol", "t": 67, "s": [0, 2, 4, 5, 7, 9, 10], "lo": -3, "hi": 7},
    "fa": {"n": "mode de fa", "t": 65, "s": [0, 2, 4, 5, 7, 9, 11], "lo": -2, "hi": 8},
}
CELLS = {
    "3": [[[2, 1], 0], [[1, 1, 1], 0], [[3], 0], [[1.5, .5, 1], 1], [[1, .5, .5, 1], 2], [[.5, .5, 1, 1], 2], [[.5, .5, .5, .5, 1], 3]],
    "6": [[[1, .5, 1, .5], 0], [[1.5, 1.5], 0], [[1, .5, 1.5], 0], [[.5, .5, .5, 1.5], 1], [[1.5, .5, .5, .5], 1], [[.5, .5, .5, .5, .5, .5], 2], [[1, .5, .5, .5, .5], 2]],
    "4": [[[1, 1, 1, 1], 0], [[2, 1, 1], 0], [[1, 1, 2], 0], [[2, 2], 0], [[1.5, .5, 1, 1], 1], [[.5, .5, 1, 1, 1], 1], [[1, .5, .5, 1, 1], 2], [[.5, .5, .5, .5, 1, 1], 2], [[.5, .5, .5, .5, .5, .5, .5, .5], 3]],
}
CAD = {"3": [[1, 2], [3]], "6": [[1, .5, 1.5], [1.5, 1.5]], "4": [[1, 1, 2], [2, 2], [1.5, .5, 2]]}
NOTE = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def parse(text, default):
    out = []
    for tok in text.split():
        name, _, dur = tok.partition(":")
        midi = 12 * (int(name[-1]) + 1) + NOTE[name[0]] + (1 if "#" in name else -1 if name[1:2] == "b" else 0)
        out.append((midi, float(dur) if dur else default))
    return out


def rng(seed):
    state = [seed]

    def r():
        state[0] = (state[0] * 1664525 + 1013904223) % 4294967296
        return state[0] / 4294967296
    return r


def js_round(x):
    return math.floor(x + 0.5)


def compose(name, meter, mode, tier, seed):
    """Port exact du compositeur du prototype (même graine, mêmes tirages)."""
    h = seed
    for ch in name:
        h = (h * 31 + ord(ch)) % 4294967296
    r, M = rng(h), MODES[mode]

    def pick(a):
        return a[math.floor(r() * len(a))]

    ok = [c[0] for c in CELLS[meter] if c[1] <= tier]
    top = min(tier, max(c[1] for c in CELLS[meter]))
    dense = [c[0] for c in CELLS[meter] if top - 1 <= c[1] <= top]

    def cell():
        return pick(dense) if tier >= 2 and r() < .6 else pick(ok)

    def cl(d):
        return max(M["lo"], min(M["hi"], d))

    def midi(d):
        o, i = math.floor(d / 7), d % 7
        return M["t"] + 12 * o + M["s"][i]

    def walk(start, c, direction):
        o, d = [], start
        for i, u in enumerate(c):
            if i > 0:
                st = 1 if r() < .72 else 2 if r() < .8 else 3
                st *= direction if direction and r() < .65 else (1 if r() < .5 else -1)
                if d + st > M["hi"] or d + st < M["lo"]:
                    st = -st
                d = cl(d + st)
            o.append({"d": d, "u": u})
        return o

    def cad(prev, tg):
        c = pick(CAD[meter])
        if len(c) == 1:
            return [{"d": tg, "u": c[0]}]
        nb = tg + (1 if prev >= tg else -1)
        if len(c) == 2:
            return [{"d": nb, "u": c[0]}, {"d": tg, "u": c[1]}]
        return [{"d": cl(js_round((prev + nb) / 2)), "u": c[0]}, {"d": nb, "u": c[1]}, {"d": tg, "u": c[2]}]

    def phrase(start, peak):
        b1 = walk(start, cell(), 0)
        sh = 1 if r() < .5 else -1
        b2 = [{"d": cl(n["d"] + sh), "u": n["u"]} for n in b1] if r() < .65 else walk(b1[-1]["d"], cell(), 0)
        b3 = walk(b2[-1]["d"], cell(), 1 if peak > b2[-1]["d"] else -1)
        return [b1, b2, b3]

    A = phrase(0 if r() < .5 else 2, 4)
    B = phrase(4 if r() < .5 else 5, 7)
    out = []

    def emit(bars, tg):
        for b in bars:
            out.extend(b)
        out.extend(cad(bars[2][-1]["d"], tg))

    def form():
        emit(A, 1 if r() < .5 else 4)
        emit(A, 0)
        emit(B, 4)
        emit(B, 0)

    form()
    if tier >= 4:
        form()
    return [(midi(n["d"]), n["u"]) for n in out]


def songs():
    result = []
    for tune in TUNES:
        seq = []
        for key in tune["seq"]:
            seq += parse(P[key], tune["u"])
        result.append((tune["n"], tune["o"], tune["bpm"], seq))
    for name, meter, mode, bpm, k in GEN_APPRENTI:
        subtitle = name.split(" ")[0].lower() + ", " + MODES[mode]["n"]
        result.append((name, subtitle, bpm, compose(name, meter, mode, 0, k + 7)))
    return result


def to_notes(bpm, seq):
    spb, beat, tokens = 60 / bpm, 0.0, []
    for midi, d in seq:
        step = round(beat * 4)
        held = d >= 1.5 and d * spb >= .8
        tokens.append(f"{step}:{midi}:{round(d * 4)}" if held else f"{step}:{midi}")
        beat += d
    return " ".join(tokens)


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 5
    rows = []
    for index, (name, subtitle, bpm, seq) in enumerate(songs()[:count]):
        song_id = index + 1
        skill = 1 + index * 60 // 8           # TSTART + floor(lv * 60 / 8), comme le prototype
        power = (song_id - 1) / (TOTAL_SONGS - 1)
        name_sql, subtitle_sql = name.replace("'", "''"), subtitle.replace("'", "''")
        rows.append(f"({song_id}, '{name_sql}', '{subtitle_sql}', 0, {skill}, {bpm}, {power:.6f},\n '{to_notes(bpm, seq)}')")
    print(",\n".join(rows) + ";")


if __name__ == "__main__":
    main()
