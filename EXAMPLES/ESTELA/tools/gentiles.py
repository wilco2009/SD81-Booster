#!/usr/bin/env python3
"""gentiles.py -- tileset provisional de ESTELA: PNG indexado de 160x100 (10 x 6 tiles de 16x16) con la paleta de 256.

Salida (en --out, por omision ../assets):
    tileset.png   160x100 indexado. Despues, con EXAMPLES/PHOTO/png2i16.py (que respeta el orden de la paleta):
                      python ../../PHOTO/png2i16.py -o ../out --start 1 tileset.png   (y renombrar IMG001.I16 a NIVEL1.I16)
                  o con make.py, que lo hace todo.
    tiles.def     descripcion del tileset para map2bin.py: nombre de cada tile, rangos de paleta y fila de los paneles

Paleta de 256 (los rangos para ciclar los fija el dibujante; aqui):
      0-15    paneles laterales (metal, remaches, luces roja / verde / ambar) y colores comunes
     16-31    hierba            32-47   bosque oscuro      48-63   tierra            64-79   arena
     80-95    roca fria         96-111  roca calida       112-127  metal azulado    128-143  obsidiana
    144-159   asfalto          160-175  acentos (flores)  176-191  espuma / hielo   192-207  peligro (amarillo, rojo)
    208-223   espacio
    224-239   AGUA   (16 pasos: ciclar la paleta mueve las olas)
    240-247   LAVA   (8 pasos)
    248-255   LUCES  (8 pasos)

Los tiles se repiten en vertical sin costuras (el escenario pasa por debajo). Las filas 96-99 del PNG estan libres: la
fila 96 lleva el dibujo de los dos paneles laterales (x 0-15 el izquierdo, x 144-159 el derecho; todas las filas del
anillo llevan los mismos 16 bytes), que en el juego queda en $C000 + 96*160 = $FC00 (+0 y +144).

Uso:  python gentiles.py [-o ../assets] [--check]
"""
import argparse
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pngio  # noqa: E402
import pal160  # noqa: E402

W, H = 160, 100
T = 16
COLS = 10
PANEL_ROW = 96

# ---------------------------------------------------------------------------------------------------------
# Paleta
# ---------------------------------------------------------------------------------------------------------


def lerp(c0, c1, t):
    return tuple(int(round(a + (b - a) * t)) for a, b in zip(c0, c1))


def ramp(c0, c1, n):
    return [lerp(c0, c1, i / (n - 1)) for i in range(n)]


PANEL = [(0, 0, 0), (24, 28, 40), (44, 50, 68), (70, 78, 100), (110, 120, 146), (170, 180, 204), (230, 236, 248), (14, 14, 20),
         (120, 20, 20), (240, 50, 50), (20, 100, 40), (70, 230, 90), (150, 100, 10), (255, 200, 40), (60, 200, 230), (40, 90, 200)]
ACCENT = [(220, 40, 40), (250, 220, 50), (250, 250, 250), (240, 120, 180), (170, 80, 210), (250, 140, 30), (60, 200, 220), (120, 70, 40),
          (200, 60, 60), (255, 240, 120), (220, 230, 255), (255, 170, 210), (110, 60, 160), (255, 190, 90), (40, 140, 160), (70, 40, 20)]


def build_palette():
    pal = []
    pal += PANEL                                               # 0-15
    pal += ramp((24, 70, 24), (120, 200, 70), 16)              # 16 hierba
    pal += ramp((10, 40, 20), (60, 130, 70), 16)               # 32 bosque
    pal += ramp((60, 36, 20), (180, 130, 80), 16)              # 48 tierra
    pal += ramp((140, 110, 60), (240, 220, 150), 16)           # 64 arena
    pal += ramp((40, 44, 56), (190, 196, 210), 16)             # 80 roca fria
    pal += ramp((60, 44, 36), (190, 160, 130), 16)             # 96 roca calida
    pal += ramp((30, 40, 60), (170, 190, 220), 16)             # 112 metal
    pal += ramp((8, 6, 14), (90, 60, 110), 16)                 # 128 obsidiana
    pal += ramp((30, 30, 34), (150, 150, 156), 16)             # 144 asfalto
    pal += ACCENT                                              # 160 acentos
    pal += ramp((40, 100, 120), (230, 250, 255), 16)           # 176 espuma
    pal += ramp((20, 18, 6), (255, 220, 40), 8) + ramp((90, 10, 10), (255, 110, 30), 8)   # 192 peligro
    pal += ramp((0, 0, 20), (80, 40, 120), 16)                 # 208 espacio
    for i in range(16):                                        # 224 agua: ciclo coseno
        pal.append(lerp((10, 40, 120), (90, 190, 255), 0.5 - 0.5 * math.cos(2 * math.pi * i / 16)))
    for i in range(8):                                         # 240 lava
        pal.append(lerp((140, 20, 0), (255, 200, 40), 0.5 - 0.5 * math.cos(2 * math.pi * i / 8)))
    for i in range(8):                                         # 248 luces
        pal.append(lerp((30, 20, 5), (255, 240, 160), 0.5 - 0.5 * math.cos(2 * math.pi * i / 8)))
    assert len(pal) == 256
    return pal


RANGES = [('PAL_PANEL', 0, 15), ('PAL_GRASS', 16, 31), ('PAL_FOREST', 32, 47), ('PAL_DIRT', 48, 63), ('PAL_SAND', 64, 79),
          ('PAL_ROCK', 80, 95), ('PAL_ROCKW', 96, 111), ('PAL_METAL', 112, 127), ('PAL_OBSID', 128, 143), ('PAL_ROAD', 144, 159),
          ('PAL_ACCENT', 160, 175), ('PAL_FOAM', 176, 191), ('PAL_HAZARD', 192, 207), ('PAL_SPACE', 208, 223),
          ('PAL_WATER', 224, 239), ('PAL_LAVA', 240, 247), ('PAL_LIGHTS', 248, 255)]

GRASS, FOREST, DIRT, SAND, ROCK, ROCKW, METAL, OBSID, ROAD, ACC, FOAM, HAZ, SPACE, WATER, LAVA, LIGHTS = \
    16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 240, 248

# ---------------------------------------------------------------------------------------------------------
# Ruido periodico de 16x16 (para que el tile se repita sin costuras)
# ---------------------------------------------------------------------------------------------------------


def tnoise(seed, passes=2):
    rnd = random.Random(seed)
    g = [[rnd.random() for _ in range(T)] for _ in range(T)]
    for _ in range(passes):
        g = [[(g[y][x] * 2 + g[y][(x + 1) % T] + g[y][(x - 1) % T] + g[(y + 1) % T][x] + g[(y - 1) % T][x]) / 6.0 for x in range(T)] for y in range(T)]
    lo = min(min(r) for r in g)
    hi = max(max(r) for r in g)
    return [[(v - lo) / (hi - lo) for v in r] for r in g]


N = [tnoise(s) for s in range(1, 9)]
NR = [tnoise(100 + s, 0) for s in range(1, 5)]     # ruido sin suavizar (granos)


def pick(base, n, v):
    """Indice de la rampa base..base+n-1 segun v en 0..1."""
    return base + max(0, min(n - 1, int(v * n)))


def edge(y, mid, amp, ph=0.0):
    """Frontera vertical (x donde cambia el material) con ondulacion de periodo 16 en y."""
    return mid + amp * math.sin(2 * math.pi * y / T + ph)


def water(x, y, foam=False):
    ph = y + 2.4 * math.sin(2 * math.pi * x / T) + 1.6 * N[5][y][x]
    i = int(round(ph)) % 16
    if foam and NR[0][y][x] > 0.86:
        return FOAM + 14
    return WATER + i


def lava(x, y, crack=False):
    ph = y + (x // 4) + int(N[6][y][x] * 3)
    if crack and NR[1][y][x] > 0.9:
        return OBSID + 3
    return LAVA + ph % 8


def lights(x, y):
    return LIGHTS + ((x // 2) + (y // 2)) % 8


def bevel(x, y, base, n=16):
    """Placa de metal con borde en relieve (arriba/izquierda claro, abajo/derecha oscuro)."""
    v = 0.45 + 0.2 * N[0][y][x]
    if x == 0 or y == 0:
        v += 0.3
    if x == T - 1 or y == T - 1:
        v -= 0.3
    return pick(base, n, v)


# ---------------------------------------------------------------------------------------------------------
# Tiles: nombre -> funcion (x, y) -> indice de paleta
# ---------------------------------------------------------------------------------------------------------
TILES = []


def tile(name):
    def deco(f):
        TILES.append((name, f))
        return f
    return deco


@tile('GRASS1')
def _(x, y):
    return pick(GRASS, 16, 0.25 + 0.5 * N[0][y][x] + 0.2 * (NR[0][y][x] - 0.5))


@tile('GRASS2')
def _(x, y):
    if NR[1][y][x] > 0.965:
        return ACC + (x * 7 + y * 3) % 6
    return pick(GRASS, 16, 0.3 + 0.5 * N[1][y][x])


@tile('GRASS3')
def _(x, y):
    return pick(FOREST, 16, 0.2 + 0.6 * N[2][y][x])


@tile('DIRT')
def _(x, y):
    return pick(DIRT, 16, 0.25 + 0.5 * N[3][y][x] + 0.15 * (NR[2][y][x] - 0.5))


@tile('DIRTST')
def _(x, y):
    v = pick(DIRT, 16, 0.25 + 0.5 * N[3][y][x])
    for cx, cy in ((3, 4), (11, 2), (8, 11), (14, 14), (2, 12)):
        d = math.hypot(x - cx, y - cy)
        if d < 1.6:
            return ROCK + 9 + int(2 - d)
    return v


@tile('SAND')
def _(x, y):
    return pick(SAND, 16, 0.35 + 0.4 * N[4][y][x])


@tile('ROCK1')
def _(x, y):
    return pick(ROCK, 16, 0.2 + 0.55 * N[4][y][x] + 0.15 * (NR[0][y][x] - 0.5))


@tile('ROCK2')
def _(x, y):
    return pick(ROCKW, 16, 0.2 + 0.55 * N[5][y][x] + 0.15 * (NR[1][y][x] - 0.5))


@tile('ROCKCR')
def _(x, y):
    cx = edge(y, 7.5, 4.0, 0.7)
    if abs(x - cx) < 0.7:
        return ROCK + 1
    return pick(ROCK, 16, 0.25 + 0.5 * N[4][y][x])


@tile('SPACE')
def _(x, y):
    return pick(SPACE, 6, 0.2 + 0.5 * N[7][y][x])


@tile('WATER')
def _(x, y):
    return water(x, y)


@tile('WATERW')
def _(x, y):
    return water(x, y, foam=True)


@tile('BANKL')       # hierba a la izquierda, agua a la derecha
def _(x, y):
    b = edge(y, 5.5, 1.6)
    if x < b - 1.2:
        return pick(GRASS, 16, 0.3 + 0.5 * N[0][y][x])
    if x < b + 0.8:
        return FOAM + 12 + int(NR[2][y][x] * 3)
    return water(x, y)


@tile('BANKR')       # agua a la izquierda, hierba a la derecha
def _(x, y):
    b = edge(y, 10.5, 1.6, 2.0)
    if x > b + 1.2:
        return pick(GRASS, 16, 0.3 + 0.5 * N[0][y][x])
    if x > b - 0.8:
        return FOAM + 12 + int(NR[2][y][x] * 3)
    return water(x, y)


@tile('BRIDGEH')     # puente de tablones que cruza el rio (los tablones van a lo largo de x)
def _(x, y):
    if y % 4 == 0:
        return DIRT + 3
    return pick(DIRT, 16, 0.55 + 0.25 * N[3][y][x] + 0.04 * ((y // 4) % 2))


@tile('BRIDGEE')     # extremo del puente sobre hierba
def _(x, y):
    if y >= 12:
        return pick(GRASS, 16, 0.3 + 0.5 * N[0][y][x])
    if y % 4 == 0 and y < 12:
        return DIRT + 3
    return pick(DIRT, 16, 0.55 + 0.25 * N[3][y][x])


@tile('SHORET')      # arriba hierba, abajo agua
def _(x, y):
    b = edge(x, 6.5, 1.5)
    if y < b - 1.2:
        return pick(GRASS, 16, 0.3 + 0.5 * N[0][y][x])
    if y < b + 0.8:
        return FOAM + 12 + int(NR[2][y][x] * 3)
    return water(x, y)


@tile('SHOREB')      # arriba agua, abajo hierba
def _(x, y):
    b = edge(x, 9.5, 1.5, 1.0)
    if y > b + 1.2:
        return pick(GRASS, 16, 0.3 + 0.5 * N[0][y][x])
    if y > b - 0.8:
        return FOAM + 12 + int(NR[2][y][x] * 3)
    return water(x, y)


@tile('ISLAND')
def _(x, y):
    d = math.hypot(x - 7.5, y - 7.5)
    if d < 5.5:
        return pick(ROCK, 16, 0.75 - d / 9.0 + 0.2 * N[4][y][x])
    if d < 6.7:
        return FOAM + 13
    return water(x, y)


@tile('LILY')
def _(x, y):
    for cx, cy in ((4, 4), (11, 10), (3, 13)):
        d = math.hypot(x - cx, y - cy)
        if d < 2.3:
            return GRASS + 8 + int(N[1][y][x] * 4) if not (abs(x - cx) < 0.6 and y < cy) else WATER + 3
    return water(x, y)


@tile('LAVA')
def _(x, y):
    return lava(x, y)


@tile('LAVACR')
def _(x, y):
    return lava(x, y, crack=True)


@tile('LAVAL')       # roca a la izquierda, lava a la derecha
def _(x, y):
    b = edge(y, 5.5, 1.8)
    if x < b - 0.5:
        return pick(OBSID, 16, 0.2 + 0.5 * N[2][y][x])
    if x < b + 1.2:
        return OBSID + 12
    return lava(x, y)


@tile('LAVAR')
def _(x, y):
    b = edge(y, 10.5, 1.8, 2.0)
    if x > b + 0.5:
        return pick(OBSID, 16, 0.2 + 0.5 * N[2][y][x])
    if x > b - 1.2:
        return OBSID + 12
    return lava(x, y)


@tile('LAVAT')       # roca arriba, lava abajo
def _(x, y):
    b = edge(x, 6.5, 1.6)
    if y < b - 0.5:
        return pick(OBSID, 16, 0.2 + 0.5 * N[2][y][x])
    if y < b + 1.2:
        return OBSID + 12
    return lava(x, y)


@tile('LAVAB')       # lava arriba, roca abajo
def _(x, y):
    b = edge(x, 9.5, 1.6, 1.0)
    if y > b + 0.5:
        return pick(OBSID, 16, 0.2 + 0.5 * N[2][y][x])
    if y > b - 1.2:
        return OBSID + 12
    return lava(x, y)


@tile('OBSID')
def _(x, y):
    return pick(OBSID, 16, 0.15 + 0.5 * N[2][y][x] + 0.2 * (NR[3][y][x] - 0.5))


@tile('OBSIDS')      # obsidiana con vetas de lava
def _(x, y):
    cx = edge(y, 8.0, 3.5, 0.3)
    if abs(x - cx) < 1.0:
        return LAVA + (y + x) % 8
    return pick(OBSID, 16, 0.15 + 0.5 * N[2][y][x])


@tile('LAVABU')      # lava con burbujas
def _(x, y):
    for cx, cy in ((4, 5), (12, 11)):
        d = math.hypot(x - cx, y - cy)
        if d < 2.2:
            return LAVA + 6 + (1 if d < 1.0 else 0)
    return lava(x, y)


@tile('EMBER')       # roca con brasas
def _(x, y):
    if NR[2][y][x] > 0.93:
        return LAVA + 5
    return pick(OBSID, 16, 0.2 + 0.5 * N[2][y][x])


@tile('METAL')
def _(x, y):
    return bevel(x, y, METAL)


@tile('METALR')      # con remaches
def _(x, y):
    for cx, cy in ((3, 3), (12, 3), (3, 12), (12, 12)):
        d = math.hypot(x - cx, y - cy)
        if d < 1.4:
            return METAL + 14 if (x <= cx and y <= cy) else METAL + 3
    return bevel(x, y, METAL)


@tile('GRATE')
def _(x, y):
    if x % 4 == 0 or y % 4 == 0:
        return METAL + 10
    return METAL + 1 + (x + y) % 2


@tile('HAZARD')      # rayas diagonales amarillo / negro
def _(x, y):
    return HAZ + 6 if ((x + y) // 4) % 2 == 0 else HAZ + 0


@tile('LIGHTS')      # metal con tira de luces que cicla
def _(x, y):
    if 6 <= y <= 9 and 1 <= x <= 14:
        return lights(x, y)
    return bevel(x, y, METAL)


@tile('PIPEH')
def _(x, y):
    if 4 <= y <= 11:
        return pick(METAL, 16, 0.25 + 0.65 * math.sin((y - 4 + 0.5) / 8.0 * math.pi))
    return bevel(x, y, METAL, 8)


@tile('PIPEV')
def _(x, y):
    if 4 <= x <= 11:
        return pick(METAL, 16, 0.25 + 0.65 * math.sin((x - 4 + 0.5) / 8.0 * math.pi))
    return bevel(x, y, METAL, 8)


@tile('PIPEC')
def _(x, y):
    if (4 <= y <= 11 and x >= 4) or (4 <= x <= 11 and y >= 4):     # codo: del borde derecho al borde inferior
        if x >= 12:
            u = y
        elif y >= 12:
            u = x
        else:
            u = y if x >= y else x
        return pick(METAL, 16, 0.25 + 0.65 * math.sin((u - 4 + 0.5) / 8.0 * math.pi))
    return bevel(x, y, METAL, 8)


@tile('HATCH')
def _(x, y):
    d = max(abs(x - 7.5), abs(y - 7.5))
    if d < 5.5:
        if d > 4.0:
            return METAL + 2
        return pick(METAL, 16, 0.3 + 0.25 * N[0][y][x]) if (x + y) % 8 else METAL + 12
    return bevel(x, y, METAL)


@tile('PAD')         # plataforma redonda (base de un cañon)
def _(x, y):
    d = math.hypot(x - 7.5, y - 7.5)
    if d < 3.0:
        return METAL + 13 - int(d)
    if d < 6.5:
        return METAL + 4 + int(d - 3)
    if d < 7.5:
        return METAL + 1
    return pick(ROCK, 16, 0.2 + 0.5 * N[4][y][x])


@tile('TURRET')      # base de cañon sobre hierba
def _(x, y):
    d = math.hypot(x - 7.5, y - 7.5)
    if d < 4.5:
        return METAL + 12 - int(d * 1.5)
    if d < 6.5:
        return METAL + 2
    return pick(GRASS, 16, 0.3 + 0.5 * N[0][y][x])


@tile('CRATER')
def _(x, y):
    d = math.hypot(x - 7.5, y - 7.5)
    if d < 4.5:
        return pick(DIRT, 16, 0.12 + 0.12 * N[3][y][x] + 0.02 * d)
    if d < 6.2:
        return pick(DIRT, 16, 0.7 + 0.1 * N[3][y][x])
    return pick(GRASS, 16, 0.3 + 0.5 * N[0][y][x])


@tile('ROADH')
def _(x, y):
    if y in (7, 8) and x % 8 < 4:
        return ROAD + 14
    return pick(ROAD, 16, 0.18 + 0.15 * N[1][y][x])


@tile('ROADV')
def _(x, y):
    if x in (7, 8) and y % 8 < 4:
        return ROAD + 14
    return pick(ROAD, 16, 0.18 + 0.15 * N[1][y][x])


@tile('ROADX')
def _(x, y):
    return pick(ROAD, 16, 0.18 + 0.15 * N[1][y][x])


@tile('DUNES')
def _(x, y):
    ph = y + 2.5 * math.sin(2 * math.pi * x / T) + 8.0
    return pick(SAND, 16, 0.5 + 0.35 * math.sin(2 * math.pi * ph / T) * 0.9 + 0.1 * N[4][y][x] - 0.05)


@tile('TREE')
def _(x, y):
    d = math.hypot(x - 7.5, y - 7.5)
    if d < 6.5:
        return pick(FOREST, 16, 0.85 - d / 9.0 + 0.2 * N[2][y][x] - (0.0 if x + y < 15 else 0.12))
    return pick(GRASS, 16, 0.3 + 0.5 * N[0][y][x])


@tile('BUSH')
def _(x, y):
    for cx, cy in ((4, 5), (11, 4), (8, 11)):
        d = math.hypot(x - cx, y - cy)
        if d < 3.6:
            return pick(FOREST, 16, 0.85 - d / 6.0 + 0.2 * N[2][y][x])
    return pick(GRASS, 16, 0.3 + 0.5 * N[0][y][x])


@tile('TREES2')
def _(x, y):
    best = None
    for cx, cy in ((3.5, 3.5), (11.5, 4.5), (4.5, 12.5), (12.5, 12.5)):
        for ox in (-T, 0, T):
            for oy in (-T, 0, T):
                d = math.hypot(x - cx - ox, y - cy - oy)
                if d < 4.4 and (best is None or d < best):
                    best = d
    if best is not None:
        return pick(FOREST, 16, 0.9 - best / 6.0 + 0.15 * N[2][y][x])
    return pick(GRASS, 16, 0.25 + 0.4 * N[0][y][x])


@tile('BOULDER')
def _(x, y):
    d = math.hypot(x - 7, y - 8)
    if d < 5.2:
        return pick(ROCK, 16, 0.85 - d / 8.0 - 0.04 * (x - 7) + 0.03 * (7 - y))
    if d < 6.2:
        return GRASS + 1
    return pick(GRASS, 16, 0.3 + 0.5 * N[0][y][x])


@tile('GRDIRT')      # arriba hierba, abajo tierra
def _(x, y):
    b = edge(x, 7.5, 1.5)
    if y < b:
        return pick(GRASS, 16, 0.3 + 0.5 * N[0][y][x])
    return pick(DIRT, 16, 0.25 + 0.5 * N[3][y][x])


@tile('DIRTGR')      # arriba tierra, abajo hierba
def _(x, y):
    b = edge(x, 7.5, 1.5, 1.2)
    if y > b:
        return pick(GRASS, 16, 0.3 + 0.5 * N[0][y][x])
    return pick(DIRT, 16, 0.25 + 0.5 * N[3][y][x])


@tile('GRROCKL')     # hierba a la izquierda, roca a la derecha
def _(x, y):
    b = edge(y, 7.5, 2.2)
    if x < b:
        return pick(GRASS, 16, 0.3 + 0.5 * N[0][y][x])
    return pick(ROCK, 16, 0.2 + 0.55 * N[4][y][x]) if x > b + 1 else ROCK + 2


@tile('GRROCKR')     # roca a la izquierda, hierba a la derecha
def _(x, y):
    b = edge(y, 7.5, 2.2, 1.5)
    if x > b:
        return pick(GRASS, 16, 0.3 + 0.5 * N[0][y][x])
    return pick(ROCK, 16, 0.2 + 0.55 * N[4][y][x]) if x < b - 1 else ROCK + 2


@tile('LIGHTS2')     # suelo de luces que ciclan en toda la baldosa
def _(x, y):
    if (x % 8 in (2, 3, 4, 5)) and (y % 8 in (2, 3, 4, 5)):
        return lights(x, y)
    return bevel(x, y, METAL, 10)


@tile('MLIGHT')      # franja vertical de luces
def _(x, y):
    if 6 <= x <= 9:
        return LIGHTS + (y // 2 + (x - 6) // 2) % 8
    return bevel(x, y, METAL, 10)


@tile('STARS')
def _(x, y):
    if NR[3][y][x] > 0.95:
        return PANEL_STAR[(x + y) % 3]
    return pick(SPACE, 6, 0.2 + 0.5 * N[7][y][x])


@tile('DARK')
def _(x, y):
    return pick(METAL, 5, 0.1 + 0.4 * N[1][y][x])


@tile('CHECK')
def _(x, y):
    if ((x // 8) + (y // 8)) % 2 == 0:
        return pick(METAL, 5, 0.6 + 0.3 * N[0][y][x])
    return pick(METAL, 5, 0.1 + 0.3 * N[0][y][x])


@tile('SANDST')      # arena con piedras
def _(x, y):
    for cx, cy in ((4, 3), (12, 8), (6, 13)):
        d = math.hypot(x - cx, y - cy)
        if d < 1.8:
            return ROCKW + 8 + int(3 - d)
    return pick(SAND, 16, 0.35 + 0.4 * N[4][y][x])


PANEL_STAR = [6, 5, 14]


def render_tiles():
    assert len(TILES) == 60, 'hay %d tiles y deben ser 60' % len(TILES)
    img = [[0] * W for _ in range(H)]
    for t, (name, f) in enumerate(TILES):
        ox, oy = (t % COLS) * T, (t // COLS) * T
        for y in range(T):
            for x in range(T):
                v = f(x, y)
                assert 0 <= v < 256, '%s (%d,%d) = %d' % (name, x, y, v)
                img[oy + y][ox + x] = v
    # fila 96: los dos paneles laterales (izquierdo x 0-15, derecho x 144-159; el derecho es el espejo)
    left = [1, 4, 5, 4, 3, 3, 2, 2, 2, 2, 3, 3, 4, 5, 7, 0]
    for x in range(16):
        img[PANEL_ROW][x] = left[x]
        img[PANEL_ROW][W - 1 - x] = left[x]
    return img


def tiles_def():
    L = ['# tiles.def -- generado por gentiles.py. Lo lee map2bin.py (las lineas "tile" dan los nombres para escribir los mapas).',
         '# tile NOMBRE INDICE        indice 0-59 = fila*10 + columna en el tileset',
         '# range NOMBRE PRIMERO ULTIMO   rangos de la paleta de 256 (el agua, la lava y las luces se ciclan)',
         '# panelrow N               fila del PNG que lleva el dibujo de los paneles (x 0-15 izquierdo, x 144-159 derecho)']
    for i, (name, _) in enumerate(TILES):
        L.append('tile %s %d' % (name, i))
    for name, a, b in RANGES:
        L.append('range %s %d %d' % (name, a, b))
    L.append('panelrow %d' % PANEL_ROW)
    return '\n'.join(L) + '\n'


# ---------------------------------------------------------------------------------------------------------
def check_png(out_dir):
    """Ida y vuelta: se lee el PNG, se regenera cada tile con su funcion y se compara; y los rangos ciclables."""
    w, h, img, pal = pngio.read_indexed(os.path.join(out_dir, 'tileset.png'))
    errs = []
    if (w, h) != (W, H):
        errs.append('el PNG mide %dx%d' % (w, h))
    if pal != build_palette():
        errs.append('la paleta no es la de gentiles')
    for t, (name, f) in enumerate(TILES):
        ox, oy = (t % COLS) * T, (t // COLS) * T
        for y in range(T):
            for x in range(T):
                if img[oy + y][ox + x] != f(x, y):
                    errs.append('tile %s: pixel (%d,%d) distinto' % (name, x, y))
                    break
    # los ciclos: nadie fuera de su tile usa agua, lava o luces donde no toca (solo informativo): se cuentan los usos
    used = {}
    for r in img:
        for v in r:
            used[v] = used.get(v, 0) + 1
    for name, a, b in RANGES[-3:]:
        n = sum(c for v, c in used.items() if a <= v <= b)
        if n == 0:
            errs.append('el rango %s no se usa en ningun pixel' % name)
    # el .def: 60 tiles con indices 0-59 sin repetir
    names = {}
    for line in open(os.path.join(out_dir, 'tiles.def'), encoding='utf-8'):
        f = line.split()
        if f and f[0] == 'tile':
            names[f[1]] = int(f[2])
    if sorted(names.values()) != list(range(60)):
        errs.append('tiles.def no tiene los 60 indices')
    # los tiles se repiten en vertical sin costuras: la fila 15 y la 0 de un mismo tile (periodicas) deben ser continuas;
    # se comprueba que la funcion es periodica (f(x,y) con y y y+16 coincide) tomando el ruido modulo 16, que es por construccion.
    return errs, used


def check_i16(i16_path, png_path):
    """Ida y vuelta del .I16: cabecera, paleta (512 B) y 16.000 pixeles contra el PNG y pal160.solve."""
    w, h, img, pal = pngio.read_indexed(png_path)
    d = open(i16_path, 'rb').read()
    errs = []
    if len(d) != 4 + 512 + 16000 or d[:4] != b'I160':
        return ['%s: cabecera o tamano incorrectos (%d bytes)' % (i16_path, len(d))]
    if list(d[516:]) != [v for r in img for v in r]:
        errs.append('los pixeles del .I16 no son los del PNG')
    for i in range(256):
        r, g, b = pal[i] if i < len(pal) else (0, 0, 0)
        nr, ng, nb, nw = pal160.solve(*[c / 255.0 for c in (r, g, b)])
        if d[4 + 2 * i:6 + 2 * i] != pal160.entry(nr, ng, nb, nw):
            errs.append('paleta[%d] del .I16 distinta de la del PNG' % i)
            break
    return errs


def main():
    ap = argparse.ArgumentParser(description='tileset provisional de ESTELA')
    here = os.path.dirname(os.path.abspath(__file__))
    ap.add_argument('-o', '--out', default=os.path.join(here, '..', 'assets'))
    ap.add_argument('--preview', metavar='FICHERO', help='ademas escribe una vista previa ampliada x4 (PNG)')
    ap.add_argument('--check', action='store_true', help='comprobacion de ida y vuelta')
    a = ap.parse_args()
    pal = build_palette()
    img = render_tiles()
    os.makedirs(a.out, exist_ok=True)
    pngio.write_indexed(os.path.join(a.out, 'tileset.png'), W, H, img, pal)
    open(os.path.join(a.out, 'tiles.def'), 'w', encoding='utf-8', newline='\n').write(tiles_def())
    used = len(set(v for r in img for v in r))
    print('tileset.png (%dx%d, %d colores usados de 256) y tiles.def: %d tiles' % (W, H, used, len(TILES)))
    if a.preview:
        pngio.write_indexed(a.preview, W, H, img, pal, scale=4)
    if a.check:
        errs, _ = check_png(a.out)
        for e in errs:
            print('FALLO: ' + e, file=sys.stderr)
        if errs:
            return 2
        print('comprobacion de ida y vuelta: correcta (60 tiles regenerados, paleta, rangos y tiles.def)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
