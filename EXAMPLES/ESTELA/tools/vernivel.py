#!/usr/bin/env python3
"""vernivel.py -- dibuja el mapa de un nivel (.LVL) con su tileset en un PNG, para revisarlo a ojo.

El mapa sale en columnas (de abajo arriba, el principio del nivel es la primera columna por abajo); una columna mide
--filas filas (por omision 60) y las filas del principio quedan abajo de la primera columna. Los enemigos de suelo (E6) se marcan
con una cruz roja y las oleadas se ignoran.

Uso:  python vernivel.py ../assets/NIVEL2.LVL tileset2.png salida.png [--filas 60] [--escala 1]
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import map2bin  # noqa: E402
import pngio    # noqa: E402


def main():
    ap = argparse.ArgumentParser(description='dibuja el mapa de un nivel')
    ap.add_argument('lvl')
    ap.add_argument('tileset', help='PNG del tileset (en assets/ si no lleva carpeta)')
    ap.add_argument('out')
    ap.add_argument('--filas', type=int, default=60)
    ap.add_argument('--escala', type=int, default=1)
    a = ap.parse_args()
    lv = map2bin.parse_level(a.lvl)
    ts = a.tileset if os.path.dirname(a.tileset) else os.path.join(os.path.dirname(os.path.abspath(a.lvl)), a.tileset)
    w, h, img, pal = pngio.read_indexed(ts)
    rows = list(reversed(lv.map))              # rows[0] = el principio
    n = len(rows)
    ncol = (n + a.filas - 1) // a.filas
    gap = 6
    out = [[0] * (ncol * (128 + gap)) for _ in range(a.filas * 16)]
    for r in range(n):
        col, rr = divmod(r, a.filas)
        y0 = (a.filas - 1 - rr) * 16
        for c in range(8):
            t = rows[r][c]
            ox, oy = (t % 10) * 16, (t // 10) * 16
            for y in range(16):
                line = out[y0 + y]
                base = col * (128 + gap) + c * 16
                line[base:base + 16] = img[oy + y][ox:ox + 16]
    # cruces de los enemigos de suelo
    red = len(pal) - 1
    pal = list(pal) + [(255, 0, 0)] * (256 - len(pal))
    pal[255] = (255, 0, 255)
    for wv in lv.waves:
        if wv['type'] == 5 and wv['flags'] & 4:
            row = (wv['pos'] + 100) // 16
            col, rr = divmod(row, a.filas)
            x0 = col * (128 + gap) + (wv['x'] - 16)
            y0 = (a.filas - 1 - rr) * 16
            for k in range(16):
                if 0 <= y0 + k < len(out):
                    out[y0 + k][x0 + k] = 255
                    out[y0 + k][x0 + 15 - k] = 255
    pngio.write_indexed(a.out, len(out[0]), len(out), out, pal, scale=a.escala)
    print('%s: %d filas en %d columnas' % (a.out, n, ncol))
    return 0


if __name__ == '__main__':
    sys.exit(main())
