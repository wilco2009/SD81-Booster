#!/usr/bin/env python3
"""Datos de prueba de EXAMPLES/PHOTO (png2i16.py y el visor): PNG de 160x100 de cuatro tipos, escritos aqui con zlib y struct
(con los cinco filtros de PNG, uno por fila) y la comprobacion de lo que convierte png2i16.py contra el calculo independiente.

    python -I photo_testdata.py make  CARPETA    escribe CARPETA/t1.png .. t4.png (y sus indices esperados en CARPETA/t*.exp)
    python -I photo_testdata.py verify CARPETA   comprueba CARPETA/IMG001.I16, IMG002.I16, IMG004.I16 e IMG005.I16

Imagenes: t1 = indexada de 8 bits con 256 colores; t2 = RGB de 40 colores; t3 = indexada de 4 bits con 16 colores; t4 = gris de 8 bits.
"""
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'EXAMPLES', 'MODE160'))
import pal160

W, H = 160, 100


def chunk(t, d):
    return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xFFFFFFFF)


def paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    return a if pa <= pb and pa <= pc else (b if pb <= pc else c)


def write_png(path, ctype, depth, rows, plte=None):
    chans = {0: 1, 2: 3, 3: 1}[ctype]
    bpp = max(1, chans * depth // 8)
    raw = bytearray()
    prev = bytes(len(rows[0]))
    for y, row in enumerate(rows):
        ft = y % 5
        raw.append(ft)
        for i, x in enumerate(row):
            left = row[i - bpp] if i >= bpp else 0
            up = prev[i]
            ul = prev[i - bpp] if i >= bpp else 0
            pred = (0, left, up, (left + up) >> 1, paeth(left, up, ul))[ft]
            raw.append((x - pred) & 255)
        prev = row
    out = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, depth, ctype, 0, 0, 0))
    if plte:
        out += chunk(b'PLTE', bytes(v for c in plte for v in c))
    out += chunk(b'IDAT', zlib.compress(bytes(raw))) + chunk(b'IEND', b'')
    open(path, 'wb').write(out)


def images():
    """(nombre, tipo, profundidad, filas en bytes, paleta PNG, indices esperados, paleta RGB esperada)."""
    res = []
    # t1: indexada de 8 bits, 256 colores
    plte = [(k, 255 - k, (k * 37) & 255) for k in range(256)]
    idx = [[(x + 3 * y) & 255 for x in range(W)] for y in range(H)]
    res.append(('t1', 3, 8, [bytes(r) for r in idx], plte, idx, plte))
    # t2: RGB de 40 colores; los indices salen por orden de aparicion
    cols = [((j * 53) & 255, (j * 97) & 255, (j * 29 + 10) & 255) for j in range(40)]
    pix = [[cols[(x // 4 + y // 5) % 40] for x in range(W)] for y in range(H)]
    order = {}
    idx = []
    for row in pix:
        r = []
        for p in row:
            order.setdefault(p, len(order))
            r.append(order[p])
        idx.append(r)
    pal = [None] * len(order)
    for p, i in order.items():
        pal[i] = p
    res.append(('t2', 2, 8, [bytes(v for p in row for v in p) for row in pix], None, idx, pal))
    # t3: indexada de 4 bits, 16 colores
    plte = [(k * 17, (k * 51) & 255, 255 - k * 17) for k in range(16)]
    idx = [[(x // 10 + y // 7) % 16 for x in range(W)] for y in range(H)]
    rows = [bytes((r[2 * i] << 4) | r[2 * i + 1] for i in range(W // 2)) for r in idx]
    res.append(('t3', 3, 4, rows, plte, idx, plte))
    # t4: gris de 8 bits (rampa horizontal, con una franja de negro al final)
    g = [[(x * 255 // 159) if y < 90 else 0 for x in range(W)] for y in range(H)]
    order = {}
    idx = []
    for row in g:
        r = []
        for v in row:
            order.setdefault(v, len(order))
            r.append(order[v])
        idx.append(r)
    pal = [None] * len(order)
    for v, i in order.items():
        pal[i] = (v, v, v)
    res.append(('t4', 0, 8, [bytes(r) for r in g], None, idx, pal))
    return res


def expected_pal(rgb):
    out = bytearray()
    for i in range(256):
        r, g, b = rgb[i] if i < len(rgb) else (0, 0, 0)
        out += pal160.entry(*pal160.solve(r / 255.0, g / 255.0, b / 255.0))
    return bytes(out)


def main():
    mode, d = sys.argv[1], sys.argv[2]
    os.makedirs(d, exist_ok=True)
    if mode == 'make':
        for name, ctype, depth, rows, plte, idx, rgb in images():
            write_png(os.path.join(d, name + '.png'), ctype, depth, rows, plte)
            print('escrito', name + '.png')
        return 0
    bad = 0
    for (name, ctype, depth, rows, plte, idx, rgb), n in zip(images(), (1, 2, 4, 5)):
        data = open(os.path.join(d, 'IMG%03d.I16' % n), 'rb').read()
        ok = len(data) == 16516 and data[:4] == b'I160'
        ok = ok and data[4:516] == expected_pal(rgb) and data[516:] == bytes(v for r in idx for v in r)
        print('IMG%03d.I16 (%s, %d colores): %s' % (n, name, len(rgb), 'bien' if ok else 'MAL'))
        bad += 0 if ok else 1
    print('TODO OK' if not bad else 'FALLOS')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
