#!/usr/bin/env python3
"""Genera galaxian_gfx.inc (sprites 8x8, posiciones, seno) y preview.png.

Los aliens salen de GALAXIANS.png (64x16; 8x8 por sprite):
    x  0.. 7 / 8..15   tipo 1 (rojo)     frame 0 / frame 1
    x 16..23 / 24..31  tipo 2 (magenta)  frame 0 / frame 1
    x 32..39 / 40..47  tipo 3 (cian)     frame 0 / frame 1
    x 48..55           tipo 0 (comandante, un solo frame)
Cada fila de un sprite admite DOS colores (tinta = pixel a 1, papel = pixel a
0 con mascara a 1). Si una fila de la imagen tiene mas de dos, se deja la mas
frecuente mas la primera que aparezca y se avisa.

La nave y el disparo se dibujan aqui, en ASCII, un color por fila.
Ejecutar:  python gen_gfx.py
"""
import struct
import sys
import zlib
from math import sin, pi
from pathlib import Path

HERE = Path(__file__).parent

# letra -> (tinta Chroma81 0-15, RGB para la vista previa).  8-15 = brillantes
COL = {
    # colores de GALAXIANS.png (RGB exacto de la imagen -> letra minuscula)
    'a': (2,  (224, 11, 0)),
    'b': (11, (255, 0, 255)),
    'c': (5,  (0, 197, 214)),
    'd': (9,  (28, 2, 251)),
    'e': (2,  (195, 62, 0)),
    'f': (6,  (224, 213, 0)),
    # nave y disparo
    'W': (7,  (230, 230, 230)),
    'C': (13, (60, 255, 255)),
    'R': (10, (255, 60, 60)),
}
PNGMAP = {v[1]: k for k, v in COL.items() if k.islower()}

SHIP = [('W', "...XX..."), ('W', "...XX..."), ('W', "..XXXX.."), ('C', ".XXXXXX."),
        ('C', "XXXXXXXX"), ('R', "XX.XX.XX"), ('R', "X..XX..X"), ('R', "...XX...")]
SHOT = [('W', "...XX...")] * 4 + [('W', "........")] * 4

# formacion: (cantidad, columna inicial, tipo) por fila; 10 columnas de 16 px
ROWS = [(2, 3, 0), (6, 2, 1), (8, 1, 2), (10, 0, 3), (10, 0, 3), (10, 0, 3)]
X0 = 32 + 52          # X de la columna 0 (32 de desplazamiento + margen)
XS = 16               # separacion horizontal
Y0 = 32 + 16          # Y de la fila 0
YS = 12               # separacion vertical
NFORM = sum(r[0] for r in ROWS)           # 46
DIVERS = [0, 1, 1]                        # tipos de los sprites 46..48


def load_png(path):
    d = path.read_bytes()
    p, idat = 8, b''
    while p < len(d):
        n, = struct.unpack('>I', d[p:p + 4])
        t = d[p + 4:p + 8]
        if t == b'IHDR':
            w, h, bd, ct = struct.unpack('>IIBB', d[p + 8:p + 18])
        if t == b'IDAT':
            idat += d[p + 8:p + 8 + n]
        p += 12 + n
    assert bd == 8 and ct == 6, "se espera PNG RGBA de 8 bits"
    raw, bpp, st = zlib.decompress(idat), 4, w * 4
    rows, prev, o = [], bytearray(st), 0
    for _ in range(h):
        f, line = raw[o], bytearray(raw[o + 1:o + 1 + st])
        o += 1 + st
        for i in range(st):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if f == 1:
                line[i] = (line[i] + a) & 255
            elif f == 2:
                line[i] = (line[i] + b) & 255
            elif f == 3:
                line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pr = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 255
        rows.append(line)
        prev = line
    return w, h, [[tuple(r[x * 4:x * 4 + 4]) for x in range(w)] for r in rows]


def cut(px, x0):
    """8x8 de la imagen -> 8 filas de 8 letras ('.' = transparente)."""
    out = []
    for y in range(8):
        s = ''
        for x in range(8):
            p = px[y][x0 + x]
            if p[3] < 128:
                s += '.'
            else:
                s += PNGMAP[p[:3]]   # KeyError = color que falta en COL
        out.append(s)
    return out


def fit(rows, name):
    """Deja como mucho 2 colores por fila."""
    out = []
    for y, s in enumerate(rows):
        letters = [c for c in s if c != '.']
        keep = []
        for c in letters:
            if c not in keep:
                keep.append(c)
        if len(keep) > 2:
            freq = sorted(keep, key=lambda c: -letters.count(c))
            main = freq[0]
            first = next(c for c in keep if c != main)
            ok = {main, first}
            print("AVISO %s fila %d: %s tiene %d colores; se queda con %s" %
                  (name, y, s, len(keep), ''.join(sorted(ok))))
            s = ''.join(c if c in ok or c == '.' else main for c in s)
        out.append(s)
    return out


def encode(rows):
    """8 filas de letras -> (pix, mask, col) de 8 bytes cada uno."""
    pix, msk, col = [], [], []
    for s in rows:
        seen = []
        for c in s:
            if c != '.' and c not in seen:
                seen.append(c)
        ink = seen[0] if seen else 'W'
        paper = seen[1] if len(seen) > 1 else None
        p = m = 0
        for c in s:
            p, m = p << 1, m << 1
            if c != '.':
                m |= 1
                if c == ink:
                    p |= 1
        pix.append(p)
        msk.append(m)
        col.append((COL[ink][0] << 4) | (COL[paper][0] if paper else 0))
    return pix, msk, col


def ascii_rows(rows):
    return [''.join(c if x == 'X' else '.' for x in a) for c, a in rows]


def build_shapes():
    w, h, px = load_png(HERE / "GALAXIANS.png")
    sh = {}
    for t, x0 in ((1, 0), (2, 16), (3, 32)):
        sh[t] = [fit(cut(px, x0 + 8 * f), "tipo%d f%d" % (t, f)) for f in (0, 1)]
    flag = fit(cut(px, 48), "comandante")
    sh[0] = [flag, flag]
    sh[4] = [ascii_rows(SHIP)] * 2
    sh[5] = [ascii_rows(SHOT)] * 2
    return sh


def main():
    shapes = build_shapes()
    enc = {(t, f): encode(shapes[t][f]) for t in range(6) for f in range(2)}
    out = ["; Generado por gen_gfx.py -- no editar a mano"]
    for name, idx in (("pixtab", 0), ("masktab", 1), ("coltab", 2)):
        out.append("%s:  ; (tipo*2+frame)*8" % name)
        for t in range(6):
            for f in range(2):
                out.append("        defb " + ",".join("0%02Xh" % v for v in enc[(t, f)][idx]))

    stype, fx, fy = [], [], []
    for r, (n, c0, t) in enumerate(ROWS):
        for i in range(n):
            stype.append(t)
            fx.append(X0 + (c0 + i) * XS)
            fy.append(Y0 + r * YS)
    assert len(stype) == NFORM
    stype += DIVERS
    fy += [124] * len(DIVERS)
    out.append("NFORM   equ %d" % NFORM)
    out.append("NSPR    equ %d" % len(stype))
    out.append("stype:  defb " + ",".join(map(str, stype)))
    out.append("formy:  defb " + ",".join(map(str, fy)))
    out.append("formx:  defw " + ",".join(map(str, fx)))
    sins = [round(40 * sin(2 * pi * i / 32)) for i in range(32)]
    out.append("sintab: defb " + ",".join(str(v & 255) for v in sins))
    (HERE / "galaxian_gfx.inc").write_text("\n".join(out) + "\n")

    # ---- preview.png: 256x192 x4, formacion + picado + nave, frames 0 y 1
    W, H, S = 256, 192, 4

    def frame(fr):
        img = [[(0, 0, 0)] * W for _ in range(H)]

        def put(t, x, y):          # x,y en pantalla (sin los 32 de desplazamiento)
            for ry, s in enumerate(shapes[t][fr]):
                for rx, ch in enumerate(s):
                    if ch != '.' and 0 <= y + ry < H and 0 <= x + rx < W:
                        img[y + ry][x + rx] = COL[ch][1]
        for t, x, y in zip(stype[:NFORM], fx, fy):
            put(t, x - 32, y - 32)
        put(0, 128 + 25, 124 - 32)
        put(1, 128 + 11, 116 - 32)
        put(1, 128 + 39, 116 - 32)
        put(4, 120, 176)
        put(5, 120, 150)
        return img
    f0, f1 = frame(0), frame(1)
    img = [a + [(60, 60, 60)] * 4 + b for a, b in zip(f0, f1)]
    w = len(img[0])
    raw = b"".join(
        b"\x00" + b"".join(bytes(p) * S for p in row) for row in img for _ in range(S))

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", w * S, H * S, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))
    (HERE / "preview.png").write_bytes(png)
    print("OK: galaxian_gfx.inc y preview.png")


if __name__ == "__main__":
    sys.exit(main())
