"""Génère les textures de la scène de jeu de l'addon Barde (addon/Barde/Textures/*.tga).

Images TGA 32 bits non compressées aux dimensions en puissance de deux, que le client 3.3.5 lit
directement. Tout est dessiné ici, en pur Python, pour reprendre le visuel du prototype
« Carnet du Barde » : médaillons de laiton, têtes de notes, halos, dégradés du décor.

Usage : python gen_textures.py <dossier de sortie>
"""

import math
import struct
import sys
from pathlib import Path


def clamp(x, lo=0.0, hi=1.0):
    return max(lo, min(hi, x))


def lerp(a, b, t):
    return a + (b - a) * t


def mix(c1, c2, t):
    return tuple(lerp(a, b, t) for a, b in zip(c1, c2))


def hexc(text):
    return tuple(int(text[i:i + 2], 16) / 255 for i in (1, 3, 5))


def write_tga(path, width, height, pixel):
    """pixel(x, y) -> (r, g, b, a) en 0..1 ; y = 0 en haut."""
    rows = []
    for y in range(height):
        row = bytearray()
        for x in range(width):
            r, g, b, a = pixel(x + 0.5, y + 0.5)
            row += bytes((round(clamp(b) * 255), round(clamp(g) * 255), round(clamp(r) * 255), round(clamp(a) * 255)))
        rows.append(bytes(row))
    header = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, width, height, 32, 0x28)  # origine en haut à gauche, 8 bits d'alpha
    path.write_bytes(header + b"".join(rows))


def coverage(distance, radius):
    """Opacité d'un bord net lissé sur un pixel."""
    return clamp(radius - distance + 0.5)


def disc(size, falloff=None):
    c, r = size / 2, size / 2 - 1

    def pixel(x, y):
        d = math.hypot(x - c, y - c)
        a = coverage(d, r)
        if falloff:
            a *= lerp(falloff[0], falloff[1], clamp(d / r))
        return (1, 1, 1, a)
    return pixel


def ellipse(size, ratio):
    c, rx = size / 2, size / 2 - 1
    ry = rx * ratio

    def pixel(x, y):
        d = math.hypot((x - c) / rx, (y - c) / ry)
        return (1, 1, 1, clamp((1 - d) * rx / 1.0 + 0.5))
    return pixel


def glow(size):
    c, r = size / 2, size / 2

    def pixel(x, y):
        d = clamp(math.hypot(x - c, y - c) / r)
        return (1, 1, 1, (1 - d) ** 2)
    return pixel


def thin_ring(size, width):
    c, r = size / 2, size / 2 - width - 1

    def pixel(x, y):
        d = abs(math.hypot(x - c, y - c) - r)
        return (1, 1, 1, clamp(width / 2 - d + 0.5))
    return pixel


def medal_back(size):
    dark, c = hexc("#1e1309"), size / 2
    R = size / 2 - 1

    def pixel(x, y):
        return (*dark, coverage(math.hypot(x - c, y - c), R))
    return pixel


def medal_ring(size):
    c = size / 2
    light, mid, shade, dark = hexc("#f6dc96"), hexc("#c99441"), hexc("#6e4b20"), hexc("#1e1309")
    ring_r, half = size * 0.406, size * 0.034          # R = 30 px pour un médaillon de 72 px, trait de 5 px
    rivet_r, rivet_pos = size * 0.016, ring_r

    def pixel(x, y):
        d = math.hypot(x - c, y - c)
        a = coverage(abs(d - ring_r), half)
        t = clamp((y - (c - ring_r)) / (2 * ring_r))
        color = mix(light, mid, t * 2) if t < .5 else mix(mid, shade, (t - .5) * 2)
        alpha = a
        for k in range(4):
            ang = k * math.pi / 2 + .785
            rd = math.hypot(x - (c + math.cos(ang) * rivet_pos), y - (c + math.sin(ang) * rivet_pos))
            rv = coverage(rd, rivet_r)
            if rv > 0:
                color, alpha = mix(color, light, rv), max(alpha, rv)
        return (*color, alpha)
    return pixel


def vertical(stops, width=16):
    """Dégradé vertical : stops = [(position 0..1 depuis le haut, (r, g, b, a)), ...]."""
    def pixel(x, y, height=None):
        return None
    return pixel


def gradient_image(path, width, height, stops, lines=None):
    def pixel(x, y):
        t = y / height
        for (t0, c0), (t1, c1) in zip(stops, stops[1:]):
            if t <= t1:
                col = mix(c0, c1, clamp((t - t0) / (t1 - t0)))
                break
        else:
            col = stops[-1][1]
        if lines and int(y) % lines == lines - 1:
            col = (*[v * 0.75 for v in col[:3]], col[3])
        return col
    write_tga(path, width, height, pixel)


def main():
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    write_tga(out / "disc.tga", 64, 64, disc(64))
    write_tga(out / "lampe.tga", 64, 64, disc(64, (0.55, 0.12)))                 # centre d'un médaillon, dégradé radial
    write_tga(out / "note.tga", 64, 64, ellipse(64, 0.62))
    write_tga(out / "glow.tga", 64, 64, glow(64))
    write_tga(out / "anneau.tga", 128, 128, thin_ring(128, 4))
    write_tga(out / "medaillon_fond.tga", 128, 128, medal_back(128))
    write_tga(out / "medaillon_cercle.tga", 128, 128, medal_ring(128))
    white = (1, 1, 1)
    gradient_image(out / "voie.tga", 16, 256, [(0, (*white, 0)), (1, (*white, 1))])          # transparent en haut, plein sur la ligne
    gradient_image(out / "rideau.tga", 16, 64, [(0, (*hexc("#0d1020"), 1)), (1, (*hexc("#0d1020"), 0))])
    gradient_image(out / "ligne.tga", 16, 16, [(0, (*white, 0)), (0.5, (*white, 1)), (1, (*white, 0))])
    gradient_image(out / "fond.tga", 16, 256, [(0, (*hexc("#0d1020"), 1)), (0.75, (*hexc("#1b1830"), 1)), (1, (*hexc("#2a1d16"), 1))])
    gradient_image(out / "bois.tga", 16, 128, [(0, (*hexc("#3c2815"), 1)), (1, (*hexc("#1c1209"), 1))], lines=16)
    print(f"{len(list(out.glob('*.tga')))} textures écrites dans {out}")


if __name__ == "__main__":
    main()
