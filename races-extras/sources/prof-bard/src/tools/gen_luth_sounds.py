"""Génère les notes du luth de l'addon Barde (Sounds/luth_<midi>.wav).

Corde pincée synthétisée par l'algorithme de Karplus-Strong, sans bibliothèque
externe. Usage : python gen_luth_sounds.py <dossier de sortie> [midi bas] [midi haut]
"""

import math
import random
import struct
import sys
import wave
from pathlib import Path

RATE = 22050
SECONDS = 1.1


def pluck(midi: int) -> bytes:
    frequency = 440.0 * 2 ** ((midi - 69) / 12)
    period = max(2, round(RATE / frequency))
    rng = random.Random(midi)
    # Bruit filtré une fois : une attaque plus douce, moins métallique.
    noise = [rng.uniform(-1, 1) for _ in range(period)]
    ring = [(noise[i] + noise[i - 1]) / 2 for i in range(period)]
    decay = 0.996 if midi < 72 else 0.993
    samples = []
    index = 0
    total = int(RATE * SECONDS)
    for n in range(total):
        current = ring[index]
        following = ring[(index + 1) % period]
        ring[index] = decay * 0.5 * (current + following)
        index = (index + 1) % period
        fade = 1.0 if n < total - 2000 else (total - n) / 2000
        samples.append(current * fade)
    peak = max(abs(s) for s in samples) or 1.0
    gain = 0.8 / peak
    return b"".join(struct.pack("<h", int(max(-1, min(1, s * gain)) * 32767)) for s in samples)


def main() -> None:
    out = Path(sys.argv[1])
    low = int(sys.argv[2]) if len(sys.argv) > 2 else 55
    high = int(sys.argv[3]) if len(sys.argv) > 3 else 84
    out.mkdir(parents=True, exist_ok=True)
    for midi in range(low, high + 1):
        with wave.open(str(out / f"luth_{midi}.wav"), "wb") as file:
            file.setnchannels(1)
            file.setsampwidth(2)
            file.setframerate(RATE)
            file.writeframes(pluck(midi))
    print(f"{high - low + 1} notes écrites dans {out}")


if __name__ == "__main__":
    main()
