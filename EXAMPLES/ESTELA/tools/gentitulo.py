#!/usr/bin/env python3
"""gentitulo.py -- pantalla de titulo PROVISIONAL de ESTELA: assets/titulo.png (160x100, indexado, paleta de 256).

    ESTELA        el nombre grande arriba (letras 5x7 ampliadas 4x3, degradado amarillo -> rojo, contorno y sombra)
    fondo         espacio con nebulosa suave y estrellas (semilla fija: siempre sale lo mismo)
    la nave       la fase recta de NAVE de sprites.png (x2) con su llama, y dos enemigos a los lados
    RECORD        encima de la franja de las cifras (y 75-81)
    franja        x 56-103, y 84-91: indice 0 (negro) liso, con un marco fino fuera, para que se lean las 6 cifras
    ESPACIO: JUGAR   X: SALIR     abajo (y 94-98, letras 3x5)

Los colores de la nave y los enemigos se copian de la paleta de sprites.png (indices 240-254), asi que hay que
regenerarlo despues de cambiar la hoja de sprites (make.py --regen lo hace, en este orden: sprites, tiles, titulo).
make.py convierte titulo.png en out/TITULO.I16 (formato .I16 normal: "I160" + 512 B de paleta + 16.000 pixeles).

Uso:  python gentitulo.py [-o ../assets] [--preview vista.png] [--check]
"""
import argparse
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import pngio    # noqa: E402
import spr2cel  # noqa: E402

W, H = 160, 100
STRIP = (56, 84, 103, 91)          # x0 y0 x1 y1 (incluidos): donde ESTELA pone las cifras del record
FRAME = (55, 83, 104, 92)

# indices de la paleta
NEB0 = 1          # 1-15: nebulosa (de casi negro a morado oscuro)
STAR0 = 16        # 16-31: estrellas grises (de tenue a blanca)
STARC = 32        # 32-39: estrellas de color
TIT0 = 40         # 40-60: degradado del titulo (21 filas)
OUTLINE, SHADOW, FRAMEC = 61, 62, 63
RECORDC, YELLOW, WHITE, CYAN = 64, 65, 66, 67
SHIP0 = 239       # el 1 de la hoja de sprites -> 240 ... el 15 -> 254

FONT7 = {
    'E': ['11111', '10000', '10000', '11110', '10000', '10000', '11111'],
    'S': ['01111', '10000', '10000', '01110', '00001', '00001', '11110'],
    'T': ['11111', '00100', '00100', '00100', '00100', '00100', '00100'],
    'L': ['10000', '10000', '10000', '10000', '10000', '10000', '11111'],
    'A': ['01110', '10001', '10001', '11111', '10001', '10001', '10001'],
    'R': ['11110', '10001', '10001', '11110', '10100', '10010', '10001'],
    'C': ['01111', '10000', '10000', '10000', '10000', '10000', '01111'],
    'O': ['01110', '10001', '10001', '10001', '10001', '10001', '01110'],
    'D': ['11110', '10001', '10001', '10001', '10001', '10001', '11110'],
}
FONT5 = {
    'A': ['010', '101', '111', '101', '101'],
    'C': ['011', '100', '100', '100', '011'],
    'E': ['111', '100', '110', '100', '111'],
    'G': ['011', '100', '101', '101', '011'],
    'I': ['111', '010', '010', '010', '111'],
    'J': ['001', '001', '001', '101', '010'],
    'L': ['100', '100', '100', '100', '111'],
    'O': ['010', '101', '101', '101', '010'],
    'P': ['110', '101', '110', '100', '100'],
    'R': ['110', '101', '110', '101', '101'],
    'S': ['011', '100', '010', '001', '110'],
    'U': ['101', '101', '101', '101', '011'],
    'X': ['101', '101', '010', '101', '101'],
    ':': ['0', '1', '0', '1', '0'],
    ' ': ['00', '00', '00', '00', '00'],
}


def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def build_palette(sprite_pal):
    pal = [(0, 0, 0)] * 256
    pal = list(pal)
    for i in range(15):                                     # nebulosa
        t = (i + 1) / 15.0
        pal[NEB0 + i] = (int(40 * t ** 1.3), int(12 * t), int(78 * t ** 0.9))
    for i in range(16):                                     # estrellas grises
        v = 56 + i * 13
        pal[STAR0 + i] = (v, v, min(255, v + 10))
    pal[STARC:STARC + 8] = [(255, 228, 120), (255, 190, 90), (255, 150, 130), (150, 190, 255),
                            (120, 230, 255), (200, 160, 255), (255, 255, 200), (170, 255, 190)]
    stops = [(0.0, (255, 252, 190)), (0.18, (255, 226, 60)), (0.5, (255, 148, 24)), (0.8, (236, 64, 28)),
             (1.0, (150, 20, 52))]
    for i in range(21):
        t = i / 20.0
        for k in range(len(stops) - 1):
            if stops[k][0] <= t <= stops[k + 1][0]:
                u = (t - stops[k][0]) / (stops[k + 1][0] - stops[k][0])
                pal[TIT0 + i] = lerp(stops[k][1], stops[k + 1][1], u)
                break
    pal[OUTLINE] = (8, 14, 64)
    pal[SHADOW] = (26, 8, 54)
    pal[FRAMEC] = (70, 100, 190)
    pal[RECORDC] = (110, 232, 255)
    pal[YELLOW] = (255, 232, 80)
    pal[WHITE] = (222, 228, 244)
    pal[CYAN] = (110, 232, 255)
    for i in range(1, 16):
        pal[SHIP0 + i] = tuple(sprite_pal[i]) if i < len(sprite_pal) else (0, 0, 0)
    return pal


def nebula(img, rnd):
    """Nebulosa: ruido de valores suave (dos octavas); por debajo de y=73 se limita para que los textos se lean."""
    def grid(nx, ny):
        return [[rnd.random() for _ in range(nx + 2)] for _ in range(ny + 2)]
    g1, g2 = grid(8, 6), grid(16, 12)

    def sample(g, nx, ny, x, y):
        fx, fy = x * nx / W, y * ny / H
        ix, iy = int(fx), int(fy)
        tx, ty = fx - ix, fy - iy
        tx, ty = tx * tx * (3 - 2 * tx), ty * ty * (3 - 2 * ty)
        a = g[iy][ix] * (1 - tx) + g[iy][ix + 1] * tx
        b = g[iy + 1][ix] * (1 - tx) + g[iy + 1][ix + 1] * tx
        return a * (1 - ty) + b * ty
    for y in range(H):
        for x in range(W):
            v = 0.7 * sample(g1, 8, 6, x, y) + 0.3 * sample(g2, 16, 12, x, y)
            v = max(0.0, (v - 0.42) / 0.5)
            lvl = int(min(1.0, v) * 11)
            if y >= 73:
                lvl = min(lvl, 3)
            img[y][x] = NEB0 + lvl - 1 if lvl > 0 else 0


def stars(img, rnd):
    def ok(x, y):
        return 0 <= x < W and 0 <= y < H and not (40 <= x <= 119 and y >= 73)
    for _ in range(80):
        x, y = rnd.randrange(W), rnd.randrange(H)
        if not ok(x, y):
            continue
        r = rnd.random()
        if r < 0.62:
            img[y][x] = STAR0 + rnd.randrange(0, 5)
        elif r < 0.9:
            img[y][x] = STAR0 + rnd.randrange(5, 11)
        else:
            img[y][x] = STARC + rnd.randrange(8)
    for x, y in ((9, 36), (150, 33), (23, 68), (139, 64), (150, 8), (6, 8)):       # estrellas grandes con cruz
        img[y][x] = STAR0 + 15
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            if ok(x + dx, y + dy):
                img[y + dy][x + dx] = STAR0 + 8
        for dx, dy in ((2, 0), (-2, 0)):
            if ok(x + dx, y + dy):
                img[y + dy][x + dx] = STAR0 + 3


def glyph_mask(text, font, sx, sy, gap):
    """Mascara (conjunto de (x, y)) del texto con la fuente dada, ampliado sx x sy, y su ancho."""
    pts = set()
    x = 0
    for ch in text:
        g = font[ch]
        gw = len(g[0])
        for r, row in enumerate(g):
            for c, bit in enumerate(row):
                if bit == '1':
                    for dy in range(sy):
                        for dx in range(sx):
                            pts.add((x + c * sx + dx, r * sy + dy))
        x += gw * sx + gap
    return pts, x - gap


def draw_title(img):
    pts, w = glyph_mask('ESTELA', FONT7, 4, 3, 4)
    x0, y0 = (W - w) // 2, 6
    letters = {(x0 + x, y0 + y) for x, y in pts}
    shadow = {(x + 2, y + 2) for x, y in letters}
    outline = set()
    for x, y in letters:
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                outline.add((x + dx, y + dy))
    for x, y in {(x + 2, y + 2) for x, y in outline}:
        if (x, y) not in outline:
            img[y][x] = SHADOW
    for x, y in outline:
        if (x, y) not in letters:
            img[y][x] = OUTLINE
    for x, y in letters:
        img[y][x] = TIT0 + (y - y0)
    del shadow
    return x0, y0, w


def draw_text(img, text, x0, y0, font, colors, gap=1):
    x = x0
    for i, ch in enumerate(text):
        g = font[ch]
        for r, row in enumerate(g):
            for c, bit in enumerate(row):
                if bit == '1':
                    img[y0 + r][x + c] = colors[i]
        x += len(g[0]) + gap
    return x - gap


def text_width(text, font, gap=1):
    return sum(len(font[ch][0]) for ch in text) + gap * (len(text) - 1)


def blit(img, cells, cx, cy, x0, y0, scale, rows):
    """Copia un grafico de la hoja (esquina en celdas cx, cy; tamano en celdas) con su paleta en SHIP0+n y ampliado."""
    cw, ch = cells
    for r in range(ch * 8):
        for c in range(cw * 8):
            v = rows[cy * 8 + r][cx * 8 + c]
            if v == 0:
                continue
            for dy in range(scale):
                for dx in range(scale):
                    img[y0 + r * scale + dy][x0 + c * scale + dx] = SHIP0 + v


def draw_ships(img, sheet, groups):
    def pos(name, phase):
        for n, gx, gy, gw, gh, fr in groups:
            if n == name:
                return gx + phase * gw, gy, gw, gh
        raise KeyError(name)
    cx, cy, gw, gh = pos('NAVE', 1)
    blit(img, (gw, gh), cx, cy, 64, 31, 2, sheet)
    fx, fy, fw, fh = pos('LLAMA', 0)
    blit(img, (fw, fh), fx, fy, 72, 58, 2, sheet)
    ex, ey, ew, eh = pos('ENEMY1', 0)
    blit(img, (ew, eh), ex, ey, 20, 38, 1, sheet)
    ex, ey, ew, eh = pos('ENEMY4', 0)
    blit(img, (ew, eh), ex, ey, 124, 44, 1, sheet)
    ex, ey, ew, eh = pos('ENEMY2', 0)
    blit(img, (ew, eh), ex, ey, 44, 52, 1, sheet)
    ex, ey, ew, eh = pos('ENEMY5', 0)
    blit(img, (ew, eh), ex, ey, 100, 56, 1, sheet)


def render(assets):
    sw, sh_, sheet, sprite_pal = pngio.read_indexed(os.path.join(assets, 'sprites.png'))
    groups = spr2cel.parse_def(open(os.path.join(assets, 'sprites.def'), encoding='utf-8').read())
    pal = build_palette(sprite_pal)
    rnd = random.Random(81)
    img = [[0] * W for _ in range(H)]
    nebula(img, rnd)
    stars(img, rnd)
    draw_ships(img, sheet, groups)
    draw_title(img)
    # RECORD y marco de la franja de las cifras
    rw = text_width('RECORD', FONT7, 1)
    draw_text(img, 'RECORD', (W - rw) // 2, 75, FONT7, [RECORDC] * 6, 1)
    fx0, fy0, fx1, fy1 = FRAME
    for x in range(fx0, fx1 + 1):
        img[fy0][x] = FRAMEC
        img[fy1][x] = FRAMEC
    for y in range(fy0, fy1 + 1):
        img[y][fx0] = FRAMEC
        img[y][fx1] = FRAMEC
    sx0, sy0, sx1, sy1 = STRIP
    for y in range(sy0, sy1 + 1):
        for x in range(sx0, sx1 + 1):
            img[y][x] = 0
    # las instrucciones
    parts = [('ESPACIO', YELLOW), (': ', WHITE), ('JUGAR', WHITE), ('   ', WHITE), ('X', YELLOW), (': ', WHITE),
             ('SALIR', WHITE)]
    text = ''.join(p for p, _ in parts)
    colors = [c for p, c in parts for _ in p]
    tw = text_width(text, FONT5, 1)
    draw_text(img, text, (W - tw) // 2, 94, FONT5, colors, 1)
    return img, pal


def check(png_path):
    """Ida y vuelta del PNG escrito y las reglas del hueco de las cifras."""
    w, h, img, pal = pngio.read_indexed(png_path)
    errs = []
    if (w, h) != (W, H):
        errs.append('titulo.png mide %dx%d y deberia medir %dx%d' % (w, h, W, H))
        return errs
    if len(pal) > 256 or max(max(r) for r in img) >= len(pal):
        errs.append('paleta incorrecta')
    x0, y0, x1, y1 = STRIP
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            if img[y][x] != 0 or pal[0] != (0, 0, 0):
                errs.append('la franja de las cifras (x %d-%d, y %d-%d) no es negra y lisa' % (x0, x1, y0, y1))
                return errs
    for y in range(74, 83):                              # en el centro, entre RECORD y la franja, solo el texto
        for x in range(40, 120):
            if 62 <= x <= 96 and 75 <= y <= 81:
                continue
            if img[y][x] > NEB0 + 3:
                errs.append('algo ensucia la zona de RECORD en (%d,%d)' % (x, y))
                return errs
    return errs


def main():
    ap = argparse.ArgumentParser(description='pantalla de titulo provisional de ESTELA')
    ap.add_argument('-o', '--out', default=os.path.join(HERE, '..', 'assets'))
    ap.add_argument('--preview', metavar='FICHERO', help='ademas escribe una vista previa ampliada x4 (PNG)')
    ap.add_argument('--check', action='store_true', help='comprobacion de ida y vuelta')
    a = ap.parse_args()
    # los graficos se leen de assets/, aunque -o apunte a otra carpeta
    src = os.path.join(HERE, '..', 'assets')
    img, pal = render(src)
    os.makedirs(a.out, exist_ok=True)
    path = os.path.join(a.out, 'titulo.png')
    pngio.write_indexed(path, W, H, img, pal)
    if a.preview:
        pngio.write_indexed(a.preview, W, H, img, pal, scale=4)
    print('titulo.png: %dx%d, %d colores usados' % (W, H, len(set(v for r in img for v in r))))
    if a.check:
        errs = check(path)
        for e in errs:
            print('FALLO: ' + e, file=sys.stderr)
        if errs:
            return 2
        print('titulo.png: comprobacion correcta')
    return 0


if __name__ == '__main__':
    sys.exit(main())
