"""Génère les notes sonores de chaque instrument de l'addon Barde (Sounds/<instrument>_<midi>.wav).

Synthèse en pur Python, sans bibliothèque : cordes pincées (Karplus-Strong) pour le luth, la harpe, le psaltérion et le
tympanon ; sons soufflés ou frottés (harmoniques, vibrato, souffle) pour la flûte, le cromorne, la vièle, l'orgue et
l'ocarina. Les notes sont courtes (environ une seconde) car le client ne permet pas d'arrêter un son avant sa fin.

Instruments : luth, harpe, psalt, tymp, flute, ocar, orgue, crom, viele, tamb.

Usage : python gen_instrument_sounds.py <dossier de sortie> [instrument ...] [--low 55 --high 84]
Sans nom d'instrument, tous sont générés (le luth est aussi fait par gen_luth_sounds.py, avec le même résultat).
"""

import math
import random
import struct
import sys
import wave
from pathlib import Path

RATE = 22050


def hz(midi):
    return 440.0 * 2 ** ((midi - 69) / 12)


def envelope(n, total, attack, release, sustain=1.0):
    t = n / RATE
    if t < attack:
        return t / attack
    if t > total - release:
        return max(0.0, (total - t) / release) * sustain
    return sustain


def plucked(midi, seconds, decay, bright=0.0, doubled=False):
    """Karplus-Strong : une corde pincée. `bright` > 0 rend l'attaque plus claire."""
    f = hz(midi)
    period = max(2, round(RATE / f))
    rng = random.Random(midi)
    noise = [rng.uniform(-1, 1) for _ in range(period)]
    ring = [(noise[i] * (1 - bright) + noise[i - 1] * (1 + bright)) / 2 for i in range(period)]
    total = int(RATE * seconds)
    out = []
    index = 0
    for n in range(total):
        current = ring[index]
        ring[index] = decay * 0.5 * (current + ring[(index + 1) % period])
        index = (index + 1) % period
        out.append(current)
    if doubled:                                  # deux cordes légèrement désaccordées
        second = plucked(midi, seconds, decay, bright)
        shift = int(RATE * 0.004)
        out = [a * 0.6 + (second[n - shift] * 0.4 if n >= shift else 0.0) for n, a in enumerate(out)]
    fade = int(RATE * 0.05)
    return [s * (1.0 if n < total - fade else (total - n) / fade) for n, s in enumerate(out)]


def blown(midi, seconds, partials, vibrato=(5.0, 0.004), breath=0.0, attack=0.05, release=0.25):
    f = hz(midi)
    total = int(RATE * seconds)
    rng = random.Random(midi * 7)
    out, phase = [], 0.0
    for n in range(total):
        t = n / RATE
        depth = vibrato[1] * min(1.0, t / 0.3)
        inst = f * (1 + depth * math.sin(2 * math.pi * vibrato[0] * t))
        phase += 2 * math.pi * inst / RATE
        value = sum(a * math.sin(phase * k) for k, a in partials)
        value += breath * rng.uniform(-1, 1)
        out.append(value * envelope(n, seconds, attack, release, 0.85))
    return out


def reed(midi, seconds):
    """Cromorne : onde carrée adoucie, son nasal et rond."""
    f = hz(midi)
    total = int(RATE * seconds)
    cutoff = min(f * 3.2, 2400.0)
    alpha = 1 - math.exp(-2 * math.pi * cutoff / RATE)
    out, state = [], 0.0
    for n in range(total):
        square = 1.0 if (n * f / RATE) % 1.0 < 0.5 else -1.0
        state += alpha * (square - state)
        out.append(state * envelope(n, seconds, 0.03, 0.2, 0.9))
    return out


def bowed(midi, seconds):
    """Vièle : deux ondes triangulaires à peine désaccordées, vibrato lent, attaque douce."""
    f = hz(midi)
    total = int(RATE * seconds)
    alpha = 1 - math.exp(-2 * math.pi * min(f * 3.0, 3000.0) / RATE)
    out, state, p1, p2 = [], 0.0, 0.0, 0.0
    for n in range(total):
        t = n / RATE
        vib = 1 + 0.004 * min(1.0, t / 0.35) * math.sin(2 * math.pi * 5.5 * t)
        p1 = (p1 + f * vib / RATE) % 1.0
        p2 = (p2 + f * 1.005 * vib / RATE) % 1.0
        tri = (abs(p1 * 4 - 2) - 1) + (abs(p2 * 4 - 2) - 1)
        state += alpha * (tri / 2 - state)
        out.append(state * envelope(n, seconds, 0.08, 0.3, 0.9))
    return out


def drum(midi, seconds):
    """Tambour : une peau tendue sur un fût. Les modes d'une membrane circulaire (rapports inharmoniques 1 ; 1,59 ; 2,14 ;
    2,30 ; 2,65), chacun s'éteignant à son rythme, une hauteur qui retombe juste après la frappe, le claquement de la
    baguette, le souffle du fût et une petite réverbération de salle. Les notes graves sont des toms, les aiguës des peaux
    plus sèches, à peine plus longues qu'un claquement."""
    f = hz(midi)
    total = int(RATE * seconds)
    rng = random.Random(midi * 13)
    modes = [(1.00, 1.00, 9.0), (1.59, 0.62, 12.0), (2.14, 0.40, 15.0), (2.30, 0.30, 16.0), (2.65, 0.22, 19.0)]
    dry = max(0.0, (midi - 60) / 14.0)          # 0 pour les graves, 1 pour la peau la plus aiguë
    phases = [0.0] * len(modes)
    noise_lp = 0.0
    body = []
    for n in range(total):
        t = n / RATE
        glide = 1.0 + 0.45 * math.exp(-t * 45)           # la peau retombe en hauteur après la frappe
        value = 0.0
        for k, (ratio, amp, decay) in enumerate(modes):
            phases[k] += 2 * math.pi * f * ratio * (glide if k == 0 else 1 + 0.15 * (glide - 1)) / RATE
            value += amp * math.sin(phases[k]) * math.exp(-t * decay * (1.0 + 1.4 * dry))
        # claquement : bruit très bref ; souffle du fût : bruit filtré qui s'éteint vite
        white = rng.uniform(-1, 1)
        noise_lp += 0.25 * (white - noise_lp)
        value += white * math.exp(-t * 320) * (0.55 + 0.5 * dry)
        value += noise_lp * math.exp(-t * 28) * 0.35
        # coup de grosse caisse en dessous : une sinusoïde grave qui tombe
        value += 0.7 * (1 - dry) * math.sin(2 * math.pi * f * 0.5 * (1 + 0.6 * math.exp(-t * 35)) * t) * math.exp(-t * 11)
        body.append(value)
    # salle : deux échos légers
    out = list(body)
    for delay, gain in ((0.021, 0.18), (0.047, 0.10)):
        shift = int(RATE * delay)
        for n in range(shift, total):
            out[n] += body[n - shift] * gain
    fade = int(RATE * 0.05)
    return [s * (1.0 if n < total - fade else (total - n) / fade) for n, s in enumerate(out)]


def piano(midi, seconds):
    """Piano : une corde (deux, légèrement désaccordées) avec ses harmoniques un peu étirées, un marteau qui claque au début et une
    extinction plus lente dans le grave. Les notes graves résonnent plus longtemps que les aiguës."""
    f = hz(midi)
    total = int(RATE * seconds)
    rng = random.Random(midi * 17)
    stretch = 0.0004 * (1 + (midi - 55) / 30.0)             # l'inharmonicité d'une corde raide
    length = 2.6 - 1.4 * (midi - 55) / 29.0                  # durée du son (graves plus longs)
    out = []
    phases = [0.0] * 14
    detune = [0.0] * 14
    for n in range(total):
        t = n / RATE
        value = 0.0
        for k in range(1, 8):
            freq = f * k * math.sqrt(1 + stretch * k * k)
            amp = 1.0 / (k ** 1.15) * (1.0 if k < 5 else 0.7)
            decay = (1.6 + 0.9 * k) / length
            for j, shift in enumerate((-0.0007, 0.0007)):
                idx = (k - 1) * 2 + j
                phases[idx] += 2 * math.pi * freq * (1 + shift) / RATE
                value += 0.5 * amp * math.sin(phases[idx]) * math.exp(-t * decay)
        value += rng.uniform(-1, 1) * math.exp(-t * 130) * 0.25 * (1 + (midi - 55) / 60.0)       # le marteau
        value *= min(1.0, t / 0.004)                                                              # attaque presque immédiate
        out.append(value)
    fade = int(RATE * 0.12)
    return [s * (1.0 if n < total - fade else (total - n) / fade) for n, s in enumerate(out)]


def zither(midi):
    """Psaltérion : cordes doublées, désaccordées, avec l'octave au-dessus qui scintille : un son clair et frotté, pas celui du luth."""
    low = plucked(midi, 1.5, 0.9975, bright=0.7, doubled=True)
    high = plucked(midi + 12, 1.2, 0.996, bright=0.8)
    return [low[n] * 0.7 + (high[n] * 0.45 if n < len(high) else 0.0) for n in range(len(low))]


VOICES = {
    "piano": lambda m: piano(m, 2.4),
    "tamb": lambda m: drum(m, 0.75),
    "luth": lambda m: plucked(m, 1.1, 0.996 if m < 72 else 0.993),
    "harpe": lambda m: plucked(m, 1.6, 0.9985 if m < 72 else 0.997, bright=0.2),
    "psalt": zither,
    "tymp": lambda m: plucked(m, 1.3, 0.995, bright=0.1, doubled=True),
    "flute": lambda m: blown(m, 1.0, [(1, 1.0), (2, 0.12), (3, 0.04)], vibrato=(5.0, 0.004), breath=0.02),
    "ocar": lambda m: blown(m, 1.0, [(1, 1.0), (2, 0.05)], vibrato=(4.5, 0.003), breath=0.01, attack=0.06),
    "orgue": lambda m: blown(m, 1.0, [(1, 0.5), (2, 0.35), (3, 0.14), (4, 0.07)], vibrato=(0.0, 0.0), attack=0.05, release=0.2),
    "crom": lambda m: reed(m, 0.9),
    "viele": lambda m: bowed(m, 1.1),
}


def write(path, samples):
    peak = max(abs(s) for s in samples) or 1.0
    gain = 0.8 / peak
    with wave.open(str(path), "wb") as file:
        file.setnchannels(1)
        file.setsampwidth(2)
        file.setframerate(RATE)
        file.writeframes(b"".join(struct.pack("<h", int(max(-1, min(1, s * gain)) * 32767)) for s in samples))


def main():
    args = sys.argv[1:]
    low, high = 55, 84
    for flag in ("--low", "--high"):
        if flag in args:
            i = args.index(flag)
            value = int(args[i + 1])
            low, high = (value, high) if flag == "--low" else (low, value)
            del args[i:i + 2]
    out = Path(args[0])
    names = args[1:] or list(VOICES)
    out.mkdir(parents=True, exist_ok=True)
    for name in names:
        voice = VOICES[name]
        for midi in range(low, high + 1):
            write(out / f"{name}_{midi}.wav", voice(midi))
        print(f"{name} : {high - low + 1} notes", flush=True)


if __name__ == "__main__":
    main()
