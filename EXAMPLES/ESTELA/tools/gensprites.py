#!/usr/bin/env python3
"""gensprites.py -- sprites provisionales de ESTELA como PNG indexado de 16 colores para spr2cel.py.

Genera (en --out, por omision ../assets):
    sprites.png   la hoja: rejilla de celdas de 8x8, 128 px de ancho; indice 0 = transparente
    sprites.def   la lista de graficos (nombre x y w h fases) en el orden de las paginas, para spr2cel.py

Los graficos son los de la tabla 4.2 de claude/planning/juego160.md, en este orden (= orden de las paginas):

    NAVE 2x2 x3   0 = inclinada a la izquierda, 1 = recta, 2 = a la derecha
    LLAMA 1x1 x2  llama del motor (8x8, debajo de la nave)
    ENEMY1..ENEMY6 2x2 x2   seis tipos, dos fases de animacion
    EXPL 2x2 x4   explosion de cuatro fases
    BALAJ 1x1 x2  balas del jugador (normal, fuerte);  BALAE 1x1 x2  balas enemigas (bola, rayo)
    CIFRA 1x1 x10 cifras 0-9
    VIDA, BOMBA   iconos del marcador;  ITEM1 (arma), ITEM2 (escudo), ITEM3 (bomba)  mejoras
    JEFE 4x3 x2   jefe de 32x24 en dos fases

Son 121 celdas de las 128 de la hoja. Los dibujos son provisionales (arte ASCII en este fichero): se sustituyen
dibujando otro PNG con la misma rejilla y la misma lista (sprites.def) y pasandolo por spr2cel.py.

Paleta de 16 (indice 0 transparente):
    1 gris oscuro  2 gris  3 gris claro  4 blanco  5 azul oscuro  6 azul  7 celeste  8 rojo oscuro
    9 rojo  a verde oscuro  b verde  c amarillo  d naranja  e morado  f rosa
En el arte, cada caracter es un digito hexadecimal de esa paleta y "." es el 0.

Uso:  python gensprites.py [-o ../assets] [--check]
"""
import argparse
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pngio  # noqa: E402

PALETTE = [
    (0, 0, 0),          # 0 transparente
    (48, 52, 66),       # 1 gris oscuro
    (110, 116, 132),    # 2 gris
    (176, 183, 198),    # 3 gris claro
    (252, 252, 252),    # 4 blanco
    (24, 44, 150),      # 5 azul oscuro
    (60, 110, 232),     # 6 azul
    (140, 210, 255),    # 7 celeste
    (140, 24, 30),      # 8 rojo oscuro
    (236, 52, 52),      # 9 rojo
    (24, 112, 48),      # a verde oscuro
    (84, 212, 84),      # b verde
    (255, 236, 72),     # c amarillo
    (255, 142, 28),     # d naranja
    (150, 60, 172),     # e morado
    (244, 116, 196),    # f rosa
]

SHEET_W = 16   # celdas de ancho de la hoja


class Canvas:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.p = [[0] * w for _ in range(h)]

    @staticmethod
    def from_art(rows, mirror=True, w=None):
        """rows: cadenas; con mirror, cada cadena es la mitad izquierda y se refleja para formar el ancho completo."""
        h = len(rows)
        half = len(rows[0])
        cv = Canvas(half * 2 if mirror else half, h)
        for y, r in enumerate(rows):
            assert len(r) == half, 'fila de %d caracteres, se esperaban %d: %r' % (len(r), half, r)
            for x, ch in enumerate(r):
                v = 0 if ch == '.' else int(ch, 16)
                cv.p[y][x] = v
                if mirror:
                    cv.p[y][cv.w - 1 - x] = v
        return cv

    def copy(self):
        c = Canvas(self.w, self.h)
        c.p = [r[:] for r in self.p]
        return c

    def put(self, x, y, v):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.p[y][x] = v

    def rect(self, x0, y0, x1, y1, v):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.put(x, y, v)

    def ellipse(self, cx, cy, rx, ry, v):
        for y in range(self.h):
            for x in range(self.w):
                if ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0:
                    self.p[y][x] = v

    def recolor(self, a, b):
        for r in self.p:
            for x in range(self.w):
                if r[x] == a:
                    r[x] = b
        return self

    def shift(self, x0, x1, dy):
        """Desplaza en vertical las columnas x0..x1 (incluidas) dy filas."""
        src = [r[:] for r in self.p]
        for x in range(x0, x1 + 1):
            for y in range(self.h):
                self.p[y][x] = 0
        for x in range(x0, x1 + 1):
            for y in range(self.h):
                if 0 <= y - dy < self.h:
                    self.p[y][x] = src[y - dy][x]
        return self

    def squash_x(self, f):
        """Comprime en horizontal con factor f (<1) respecto al centro (giro)."""
        src = [r[:] for r in self.p]
        c = (self.w - 1) / 2.0
        for y in range(self.h):
            for x in range(self.w):
                sx = int(round(c + (x - c) / f))
                self.p[y][x] = src[y][sx] if 0 <= sx < self.w else 0
        return self

    def shift_x(self, y0, y1, dx):
        for y in range(y0, y1 + 1):
            row = self.p[y]
            new = [0] * self.w
            for x in range(self.w):
                if 0 <= x - dx < self.w:
                    new[x] = row[x - dx]
            self.p[y] = new
        return self

    def outline(self, v):
        """Rodea con el color v cada pixel visible por los vecinos vacios (4-vecindad)."""
        src = [r[:] for r in self.p]
        for y in range(self.h):
            for x in range(self.w):
                if src[y][x] == 0:
                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        nx, ny = x + dx, y + dy
                        if 0 <= nx < self.w and 0 <= ny < self.h and src[ny][nx] not in (0, v):
                            self.p[y][x] = v
                            break
        return self


# ---------------------------------------------------------------------------------------------------------
# Dibujos
# ---------------------------------------------------------------------------------------------------------
SHIP = [
    ".......4",
    ".......3",
    "......33",
    "......37",
    ".....337",
    ".....367",
    "....3367",
    "...33323",
    "..333223",
    ".1332223",
    "13322223",
    "93322223",
    "91132223",
    "..1.1221",
    ".....1.1",
    "........",
]


def ship_frames():
    c = Canvas.from_art(SHIP)
    left = c.copy()      # inclinada a la izquierda: la nariz y el ala de ese lado se desplazan, el ala de arriba se acorta
    left.shift_x(0, 7, -1)
    for y in (9, 10, 11):
        left.p[y][0] = 0
    left.put(15, 11, 8)
    right = c.copy()
    right.shift_x(0, 7, 1)
    for y in (9, 10, 11):
        right.p[y][15] = 0
    right.put(0, 11, 8)
    return [left, c, right]


def flame_frames():
    a = Canvas.from_art(["...dd...", "..dccd..", "..dccd..", "...dd...", "....d...", "........", "........", "........"], mirror=False)
    b = Canvas.from_art(["...dd...", "..dccd..", "..dccd..", "..dccd..", "...cc...", "...dd...", "...d....", "........"], mirror=False)
    return [a, b]


ENEMY_ART = {
    # 1: platillo verde con cupula
    1: ["........", "........", "......66", ".....677", "....6777", "..333333", ".3bbbbbb", "3bbabbab",
        "3baaaaaa", ".1aaaaaa", "..1c1c1c", "....1..1", "........", "........", "........", "........"],
    # 2: dardo rojo que apunta hacia abajo
    2: ["........", "99999999", ".9999999", ".9889999", "..988999", "..9cc999", "...9cc99", "...98899",
        "....9889", ".....988", ".......9", "........", "........", "........", "........", "........"],
    # 3: cangrejo azul con ojos
    3: ["........", ".....4..", ".....5..", "...66666", "..677777", ".6777777", ".6747777", ".6677777",
        "..566666", "..5.5555", ".5..5..5", ".5.....5", "........", "........", "........", "........"],
    # 4: giratorio morado
    4: ["........", ".......e", "......ee", ".....eef", "....eeff", "...eefff", "..eeeeff", "..eeecff",
        "..eeeeff", "...eefff", "....eeff", ".....eef", "......ee", ".......e", "........", "........"],
    # 5: bombardero naranja, ancho
    5: ["........", "......dd", ".....ddd", "....dccd", "dd..dccd", "dddddddd", "d8dd8ddd", "d8dddddd",
        ".d8888dd", "..dddddd", "...88888", "....8..8", ".....8..", "........", "........", "........"],
    # 6: dron blindado gris con ojo amarillo
    6: ["........", "....1111", "...12222", "..123333", "..123333", ".1233cc2", ".1233cc2", ".1233333",
        "..123333", "..122222", "...11111", "..1.1.1.", "..1.1.1.", "........", "........", "........"],
}


def enemy_frames(n):
    c = Canvas.from_art(ENEMY_ART[n])
    c.outline(1)
    f = c.copy()
    if n == 1:
        f.recolor(12, 9)          # luces
        f.put(7, 10, 12)
        f.put(8, 10, 12)
    elif n == 2:
        f.shift(0, 3, -1)         # las alas se pliegan
        f.shift(12, 15, -1)
    elif n == 3:
        f.shift(0, 2, 1)          # patas
        f.shift(13, 15, 1)
    elif n == 4:
        f.squash_x(0.55)          # giro
    elif n == 5:
        f.shift(0, 3, 1)          # aletea
        f.shift(12, 15, 1)
    elif n == 6:
        f.recolor(12, 9)          # el ojo parpadea
        f.put(3, 12, 0)
        f.put(12, 12, 0)
    return [c, f]


def explosion_frames():
    fr = []
    # fase 0: chispa; 1: bola; 2: bola grande hueca; 3: restos
    c = Canvas(16, 16)
    c.ellipse(7.5, 7.5, 3, 3, 12)
    c.ellipse(7.5, 7.5, 1.5, 1.5, 4)
    fr.append(c)
    c = Canvas(16, 16)
    c.ellipse(7.5, 7.5, 6, 6, 13)
    c.ellipse(7.5, 7.5, 4.2, 4.2, 12)
    c.ellipse(7.5, 7.5, 2, 2, 4)
    fr.append(c)
    c = Canvas(16, 16)
    c.ellipse(7.5, 7.5, 7.6, 7.6, 9)
    c.ellipse(7.5, 7.5, 6, 6, 13)
    c.ellipse(7.5, 7.5, 3.2, 3.2, 8)
    fr.append(c)
    c = Canvas(16, 16)
    for (x, y, v) in ((2, 3, 13), (5, 1, 12), (12, 2, 13), (14, 6, 9), (1, 9, 9), (4, 13, 13), (9, 14, 12), (13, 12, 13),
                      (7, 7, 8), (8, 7, 8), (7, 8, 8), (3, 6, 8), (11, 9, 8), (10, 4, 8), (6, 11, 8)):
        c.put(x, y, v)
    fr.append(c)
    return fr


def bullets():
    pj = Canvas.from_art(["...cc...", "...cc...", "...44...", "...44...", "...cc...", "...dd...", "........", "........"], mirror=False)
    pj2 = Canvas.from_art(["..cddc..", "..c44c..", "..c44c..", "..cddc..", "...cc...", "...dd...", "...dd...", "........"], mirror=False)
    be = Canvas.from_art(["........", "..9999..", ".994499.", ".9c44c9.", ".9c44c9.", ".994499.", "..9999..", "........"], mirror=False)
    be2 = Canvas.from_art(["...ff...", "...ff...", "...44...", "...44...", "...ff...", "...ff...", "...ee...", "........"], mirror=False)
    return [pj, pj2], [be, be2]


FONT = {   # 5x7 punteado; se centra en la celda de 8x8
    '0': ["01110", "10001", "10011", "10101", "11001", "10001", "01110"],
    '1': ["00100", "01100", "00100", "00100", "00100", "00100", "01110"],
    '2': ["01110", "10001", "00001", "00110", "01000", "10000", "11111"],
    '3': ["11110", "00001", "00001", "01110", "00001", "00001", "11110"],
    '4': ["00010", "00110", "01010", "10010", "11111", "00010", "00010"],
    '5': ["11111", "10000", "11110", "00001", "00001", "10001", "01110"],
    '6': ["00110", "01000", "10000", "11110", "10001", "10001", "01110"],
    '7': ["11111", "00001", "00010", "00100", "01000", "01000", "01000"],
    '8': ["01110", "10001", "10001", "01110", "10001", "10001", "01110"],
    '9': ["01110", "10001", "10001", "01111", "00001", "00010", "01100"],
}


def digit_frames():
    fr = []
    for d in '0123456789':
        c = Canvas(8, 8)
        for y, row in enumerate(FONT[d]):
            for x, ch in enumerate(row):
                if ch == '1':
                    c.put(1 + x + (0 if x < 5 else 0), y, 4 if y < 3 else 3)
        fr.append(c)
    return fr


def icon(rows):
    return Canvas.from_art(rows, mirror=False)


VIDA = ["...33...", "...33...", "..3733..", "..3333..", ".332233.", "3322223.", "93.11.3.", "........"]
BOMBA = ["....c...", "...d....", "..1111..", ".122221.", ".123221.", ".122221.", "..1111..", "........"]
ITEM1 = ["..cccc..", ".cddddc.", "cdd44ddc", "cd4dd4dc", "cd444ddc", "cd4ddddc", ".cddddc.", "..cccc.."]   # P: arma
ITEM2 = ["..7777..", ".755556.", "75bbbb56", "75b44b56", "75bbbb56", ".755556.", "..6666..", "........"]  # escudo
ITEM3 = ["..9999..", ".988889.", "98444489", "98488489", "98444489", "98488489", ".988889.", "..9999.."]   # B: bomba


def boss_frames():
    """Jefe de 32x24: casco gris con alas, dos cañones y un nucleo; la fase 1 abre las alas y enciende el nucleo."""
    def make(open_):
        c = Canvas(32, 24)
        # casco central
        c.ellipse(15.5, 11.5, 11, 9, 2)
        c.ellipse(15.5, 11.5, 9, 7, 3)
        c.rect(6, 15, 25, 20, 2)
        c.rect(8, 16, 23, 19, 1)
        # alas
        wy = 2 if open_ else 5
        for sgn in (0, 1):
            for i in range(9):
                x = 2 + i if sgn == 0 else 29 - i
                for y in range(wy + i // 2, wy + 8 + i // 2):
                    c.put(x, y, 1 if y == wy + i // 2 else 2)
        # canones
        for x in (7, 8, 23, 24):
            c.rect(x, 18, x, 23, 1)
        for x in (7, 24):
            c.put(x, 23, 9)
        # nucleo
        c.ellipse(15.5, 11.5, 4.5, 4.5, 8)
        c.ellipse(15.5, 11.5, 3, 3, 9 if open_ else 13)
        c.ellipse(15.5, 11.5, 1.2, 1.2, 12 if open_ else 4)
        # cabina y luces
        for x in (11, 20):
            c.put(x, 6, 7)
            c.put(x, 7, 6)
        c.outline(1)
        return c
    return [make(False), make(True)]


# ---------------------------------------------------------------------------------------------------------
def graphics():
    """Lista [(nombre, w_celdas, h_celdas, [canvas de cada fase])] en el orden de las paginas."""
    g = []
    g.append(('NAVE', 2, 2, ship_frames()))
    g.append(('LLAMA', 1, 1, flame_frames()))
    for n in range(1, 7):
        g.append(('ENEMY%d' % n, 2, 2, enemy_frames(n)))
    g.append(('EXPL', 2, 2, explosion_frames()))
    bj, be = bullets()
    g.append(('BALAJ', 1, 1, bj))
    g.append(('BALAE', 1, 1, be))
    g.append(('CIFRA', 1, 1, digit_frames()))
    g.append(('VIDA', 1, 1, [icon(VIDA)]))
    g.append(('BOMBA', 1, 1, [icon(BOMBA)]))
    g.append(('ITEM1', 1, 1, [icon(ITEM1)]))
    g.append(('ITEM2', 1, 1, [icon(ITEM2)]))
    g.append(('ITEM3', 1, 1, [icon(ITEM3)]))
    g.append(('JEFE', 4, 3, boss_frames()))
    for name, w, h, frs in g:
        for f in frs:
            assert (f.w, f.h) == (w * 8, h * 8), '%s: %dx%d en vez de %dx%d' % (name, f.w, f.h, w * 8, h * 8)
    return g


def pack(g):
    """Coloca los graficos en la hoja: de izquierda a derecha y, al llenarse la fila de SHEET_W celdas, la fila siguiente.
    Devuelve (w_celdas, h_celdas, lista [(nombre, x, y, w, h, fases)])."""
    pos = []
    x = y = rowh = 0
    for name, w, h, frs in g:
        ww = w * len(frs)
        if ww > SHEET_W:
            raise SystemExit('%s no cabe en una fila de la hoja' % name)
        if x + ww > SHEET_W:
            x, y, rowh = 0, y + rowh, 0
        pos.append((name, x, y, w, h, len(frs)))
        x += ww
        rowh = max(rowh, h)
    return SHEET_W, y + rowh, pos


def render(g):
    cw, ch, pos = pack(g)
    img = [[0] * (cw * 8) for _ in range(ch * 8)]
    for (name, x, y, w, h, fr), (_, _, _, frs) in zip(pos, g):
        for f, cv in enumerate(frs):
            for py in range(cv.h):
                for px in range(cv.w):
                    img[y * 8 + py][(x + f * w) * 8 + px] = cv.p[py][px]
    return cw * 8, ch * 8, img, pos


def def_text(pos):
    lines = ['# sprites.def -- generado por gensprites.py (el orden de las lineas es el orden de las paginas)',
             '# nombre   x y   w h   fases      (x y, w h en celdas de 8x8)']
    for name, x, y, w, h, fr in pos:
        lines.append('%-8s %2d %2d  %d %d  %d' % (name, x, y, w, h, fr))
    return '\n'.join(lines) + '\n'


def check(out_dir, g):
    """Ida y vuelta: se lee el PNG escrito y se compara cada fase con el dibujo, y el .def con la colocacion."""
    import spr2cel
    w, h, img, pal = pngio.read_indexed(os.path.join(out_dir, 'sprites.png'))
    errs = []
    if pal[:16] != PALETTE:
        errs.append('la paleta del PNG no es la de gensprites')
    groups = spr2cel.parse_def(open(os.path.join(out_dir, 'sprites.def'), encoding='utf-8').read())
    if len(groups) != len(g):
        errs.append('el .def tiene %d graficos y hay %d' % (len(groups), len(g)))
    for (name, x, y, cw, chh, fr), (n2, w2, h2, frs) in zip(groups, g):
        if (name, cw, chh, fr) != (n2, w2, h2, len(frs)):
            errs.append('%s: el .def no coincide' % name)
            continue
        for f, cv in enumerate(frs):
            for py in range(cv.h):
                for px in range(cv.w):
                    if img[y * 8 + py][(x + f * cw) * 8 + px] != cv.p[py][px]:
                        errs.append('%s fase %d: pixel (%d,%d) distinto' % (name, f, px, py))
                        break
    ncell = sum(cw * chh * fr for _, _, _, cw, chh, fr in groups)
    if ncell > 128:
        errs.append('%d celdas: no caben en las 128 de la hoja' % ncell)
    return errs, ncell


def main():
    ap = argparse.ArgumentParser(description='sprites provisionales de ESTELA')
    here = os.path.dirname(os.path.abspath(__file__))
    ap.add_argument('-o', '--out', default=os.path.join(here, '..', 'assets'))
    ap.add_argument('--preview', metavar='FICHERO', help='ademas escribe una vista previa ampliada x4 (PNG)')
    ap.add_argument('--check', action='store_true', help='comprobacion de ida y vuelta')
    a = ap.parse_args()
    g = graphics()
    w, h, img, pos = render(g)
    os.makedirs(a.out, exist_ok=True)
    pngio.write_indexed(os.path.join(a.out, 'sprites.png'), w, h, img, PALETTE, transparent0=True)
    open(os.path.join(a.out, 'sprites.def'), 'w', encoding='utf-8', newline='\n').write(def_text(pos))
    ncell = sum(w_ * h_ * len(f) for _, w_, h_, f in g)
    print('sprites.png (%dx%d) y sprites.def: %d graficos, %d celdas de 128' % (w, h, len(g), ncell))
    if a.preview:
        pngio.write_indexed(a.preview, w, h, img, [(40, 40, 56)] + PALETTE[1:], scale=4)
    if a.check:
        errs, n = check(a.out, g)
        for e in errs:
            print('FALLO: ' + e, file=sys.stderr)
        if errs:
            return 2
        print('comprobacion de ida y vuelta: correcta (%d celdas)' % n)
    return 0


if __name__ == '__main__':
    sys.exit(main())
