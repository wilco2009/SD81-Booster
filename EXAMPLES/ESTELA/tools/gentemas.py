#!/usr/bin/env python3
"""gentemas.py -- tilesets de los niveles 2-5 de ESTELA (hielo, desierto, ciudad, nave alienigena), provisionales como los del 1.

Cada tema es un tileset de 160x100 (10 x 6 tiles de 16x16, indexado, paleta de 256) con SU paleta y SUS 60 tiles:
usa el mismo reparto de indices de paleta que gentiles.py (hierba 16-31, bosque 32-47, tierra 48-63, ... agua 224-239,
lava 240-247, luces 248-255; el codigo cicla esos tres ultimos rangos, por eso estan SIEMPRE en el mismo sitio) pero con otros
colores, y reutiliza las funciones de dibujo de gentiles.py recoloreadas, mas unos cuantos tiles propios del tema.
Las filas 96-99 llevan, como en el nivel 1, los dos paneles laterales (fila 96) con los colores del tema.

Salida (en --out, por omision ../assets), para cada nivel n = 2..5:
    tilesetN.png   160x100 indexado           tilesN.def   nombres de los 60 tiles, rangos y fila de los paneles (para map2bin)
y con --preview una vista ampliada de cada uno (PNG, vistaN.png).

Uso:  python gentemas.py [-o ../assets] [--preview CARPETA] [--check]
"""
import argparse
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gentiles as gt  # noqa: E402
import pngio           # noqa: E402

W, H, T, COLS = gt.W, gt.H, gt.T, gt.COLS


def ramp3(stops, n):
    """Rampa de n colores que pasa por los colores de stops (repartidos por igual)."""
    out = []
    k = len(stops) - 1
    for i in range(n):
        t = i / (n - 1) * k
        j = min(int(t), k - 1)
        out.append(gt.lerp(stops[j], stops[j + 1], t - j))
    return out


def cycle(c0, c1, n):
    return [gt.lerp(c0, c1, 0.5 - 0.5 * math.cos(2 * math.pi * i / n)) for i in range(n)]


def tint(c, f):
    return tuple(max(0, min(255, int(round(v * m)))) for v, m in zip(c, f))


# ---------------------------------------------------------------------------------------------------------
# Tiles propios de los temas (funciones (x, y) -> indice de paleta; usan los indices y el ruido de gentiles)
# ---------------------------------------------------------------------------------------------------------
def _n(i, x, y):
    return gt.N[i][y][x]


def _nr(i, x, y):
    return gt.NR[i][y][x]


def snow(x, y):
    return gt.pick(gt.GRASS, 16, 0.5 + 0.4 * _n(0, x, y))


# --- hielo
def ice_floes(x, y):                      # agua helada con bloques de hielo flotando
    for cx, cy, rx, ry in ((4, 4, 3.6, 2.6), (11, 9, 3.1, 3.2), (5, 13, 2.6, 2.0)):
        d = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
        if d < 1:
            return gt.FOAM + 10 + (3 if d < 0.4 else 0) + (1 if (x + y) % 5 == 0 else 0)
        if d < 1.5:
            return gt.FOAM + 4
    return gt.water(x, y)


def ice_crystal(x, y):                    # cristal de hielo sobre la nieve
    dx, dy = x - 7.5, y - 8.0
    d = abs(dx) / 5.5 + abs(dy) / 7.0
    if d < 1.0:
        if d > 0.86:
            return gt.ROCK + 6
        if dx < -1.2:
            return gt.SAND + 13
        if dx > 1.2:
            return gt.SAND + 5
        return gt.SAND + 9 + int(_nr(2, x, y) * 3)
    return snow(x, y)


def ice_tile(x, y):                       # losetas de hielo (damero azul claro y claro)
    base = gt.SAND + (10 if ((x // 8) + (y // 8)) % 2 == 0 else 6)
    if x % 8 == 0 or y % 8 == 0:
        return gt.ROCK + 8
    return base + int(_n(1, x, y) * 3) - 1


# --- desierto
def d_blocks(x, y):                       # muro de bloques de piedra
    row = y // 4
    bx = (x + (row % 2) * 4) % 8
    if y % 4 == 0 or bx == 0:
        return gt.METAL + 2
    if y % 4 == 1:
        return gt.METAL + 12
    return gt.pick(gt.METAL, 16, 0.45 + 0.25 * _n(0, x, y))


def d_column(x, y):                       # capitel de columna sobre la arena, con sombra
    d = math.hypot(x - 6.5, y - 6.5)
    if d < 5.2:
        return gt.METAL + 14 - int(d * 1.2) - (1 if d > 3.6 else 0) + (1 if (x < 6 and y < 6 and d > 3) else 0)
    if math.hypot(x - 9.5, y - 9.5) < 5.4:
        return gt.DIRT + 3
    return gt.pick(gt.SAND, 16, 0.35 + 0.4 * _n(4, x, y))


def d_obelisk(x, y):
    if 5 <= x <= 10 and 1 <= y <= 12:
        return gt.METAL + (12 if x <= 7 else 6) + (1 if y < 3 else 0)
    if 11 <= x <= 14 and 4 <= y <= 14 and y - 4 >= x - 11:
        return gt.DIRT + 3
    return gt.pick(gt.SAND, 16, 0.35 + 0.4 * _n(4, x, y))


def d_pyramid(x, y):                      # piramide vista desde arriba: cuatro caras
    dx, dy = x - 7.5, y - 7.5
    if max(abs(dx), abs(dy)) > 7.0:
        return gt.pick(gt.SAND, 16, 0.35 + 0.4 * _n(4, x, y))
    if abs(abs(dx) - abs(dy)) < 0.6:
        return gt.ROCK + 14
    if abs(dy) > abs(dx):
        return gt.ROCK + (12 if dy < 0 else 3) + int(_nr(1, x, y) * 2)
    return gt.ROCK + (8 if dx < 0 else 5) + int(_nr(1, x, y) * 2)


def d_palm(x, y):                         # palmera sobre la arena
    dx, dy = x - 7.5, y - 7.5
    bg = gt.pick(gt.SAND, 16, 0.35 + 0.4 * _n(4, x, y))
    if math.hypot(x - 9.5, y - 9.5) < 2.4:
        bg = gt.DIRT + 3
    for k in range(8):
        a = k * math.pi / 4 + 0.3
        ux, uy = math.cos(a), math.sin(a)
        along = dx * ux + dy * uy
        perp = abs(-dx * uy + dy * ux)
        if 0 <= along <= 7.0 and perp < 1.1 - along / 12.0:
            return gt.FOREST + 5 + int(along) + (2 if k % 2 else 0)
    if math.hypot(dx, dy) < 1.8:
        return gt.DIRT + 7
    return bg


# --- ciudad
def roof(l=False, r=False, t=False, b=False, core=None):
    """Fragmento de tejado: borde de acera (2 px), pretil claro (2 px) en los lados que dan a la calle, y tejado liso."""
    def f(x, y):
        for flag, dist in ((l, x), (r, T - 1 - x), (t, y), (b, T - 1 - y)):
            if flag and dist < 2:
                return gt.SAND + 9 + (dist % 2)
        for flag, dist in ((l, x), (r, T - 1 - x), (t, y), (b, T - 1 - y)):
            if flag and dist < 4:
                return gt.ROCK + 14 - (dist - 2) * 3
        if core is not None:
            v = core(x, y)
            if v is not None:
                return v
        return gt.pick(gt.ROCK, 16, 0.3 + 0.2 * _n(0, x, y))
    return f


def c_ac(x, y):                           # ventilador de aire acondicionado en el tejado
    d = math.hypot(x - 7.5, y - 7.5)
    if d < 5.4:
        if d < 1.4:
            return gt.METAL + 4
        ang = math.atan2(y - 7.5, x - 7.5)
        return gt.METAL + (13 if int((ang + math.pi) / (math.pi / 3)) % 2 else 7) - (2 if d > 4.4 else 0)
    if math.hypot(x - 9.5, y - 9.5) < 5.8:
        return gt.ROCK + 2
    return None


def c_neon(x, y):                         # cartel de neon en el tejado: bandas diagonales que ciclan (rango de lava)
    if 3 <= x <= 12 and 3 <= y <= 12:
        return gt.LAVA + ((x + y) // 2) % 8
    return None


def c_cross_h(x, y):                      # paso de cebra en una calle horizontal
    if 3 <= x <= 12 and x % 2 == 0:
        return gt.ROAD + 14
    return gt.pick(gt.ROAD, 16, 0.18 + 0.15 * _n(1, x, y))


def c_cross_v(x, y):
    if 3 <= y <= 12 and y % 2 == 0:
        return gt.ROAD + 14
    return gt.pick(gt.ROAD, 16, 0.18 + 0.15 * _n(1, x, y))


def c_parking(x, y):                      # aparcamiento: lineas y dos coches vistos desde arriba
    for cx, col in ((3, 0), (11, 3)):
        if cx - 2 <= x <= cx + 2 and 3 <= y <= 12:
            if 5 <= y <= 6:
                return gt.METAL + 13           # parabrisas
            return gt.ACC + col + (0 if y < 5 else 1)
    if x in (0, 8) or (y == 0 and x % 8 < 8):
        return gt.ROAD + 12
    return gt.pick(gt.ROAD, 16, 0.18 + 0.15 * _n(1, x, y))


# --- nave alienigena
def a_eggs(x, y):
    for cx, cy in ((4, 4), (11, 5), (8, 12)):
        dx, dy = (x - cx) / 3.3, (y - cy) / 4.0
        d = dx * dx + dy * dy
        if d < 1.0:
            if d > 0.75:
                return gt.DIRT + 2
            return gt.SAND + 8 + int(max(0.0, 5 - 4 * (dx + 0.7) ** 2 - 4 * (dy + 0.5) ** 2)) + (2 if (x + y) % 7 == 0 else 0)
    return gt.pick(gt.DIRT, 16, 0.25 + 0.5 * _n(3, x, y))


def a_hex(x, y):                          # celdas hexagonales (panal de la colmena)
    pts = [(4, 4), (12, 4), (0, 12), (8, 12), (16, 12), (-4, 4), (20, 4)]
    ds = sorted(math.hypot(x - px, (y - py) * 1.15) for px, py in pts)
    for oy in (-16, 16):
        ds += [math.hypot(x - px, (y - py - oy) * 1.15) for px, py in pts]
    ds.sort()
    if ds[1] - ds[0] < 1.1:
        return gt.METAL + 2
    return gt.pick(gt.METAL, 16, 0.55 - ds[0] / 12.0 + 0.1 * _n(1, x, y))


def a_eye(x, y):                          # ojo: el iris usa el rango de lava, que cicla
    d = math.hypot(x - 7.5, y - 7.5)
    if d < 2.0:
        return gt.OBSID + 1
    if d < 4.2:
        return gt.LAVA + (int(math.atan2(y - 7.5, x - 7.5) * 1.3) + int(d)) % 8
    if d < 6.4:
        return gt.SAND + 13 - int((d - 4.2) * 2)
    return gt.pick(gt.DIRT, 16, 0.25 + 0.5 * _n(3, x, y))


# ---------------------------------------------------------------------------------------------------------
# Temas
# ---------------------------------------------------------------------------------------------------------
class Theme:
    def __init__(self, n, key, title, ramps, water, lava, lights, acc, panel, rename, replace):
        self.n, self.key, self.title = n, key, title
        self.ramps, self.water, self.lava, self.lights, self.acc, self.panel = ramps, water, lava, lights, acc, panel
        self.rename, self.replace = rename, replace      # rename: nombre base -> nombre; replace: nombre base -> (nombre, funcion)

    def palette(self):
        pal = []
        pm = self.panel
        pal += [gt.PANEL[0]] + [tint(c, pm) for c in gt.PANEL[1:8]] + list(gt.PANEL[8:])
        r = self.ramps
        for k in ('grass', 'forest', 'dirt', 'sand', 'rock', 'rockw', 'metal', 'obsid', 'road'):
            pal += ramp3(r[k], 16)
        pal += self.acc
        pal += ramp3(r['foam'], 16)
        pal += gt.ramp((20, 18, 6), (255, 220, 40), 8) + gt.ramp((90, 10, 10), (255, 110, 30), 8)       # peligro (como el 1)
        pal += ramp3(r['space'], 16)
        pal += cycle(self.water[0], self.water[1], 16)
        pal += cycle(self.lava[0], self.lava[1], 8)
        pal += cycle(self.lights[0], self.lights[1], 8)
        assert len(pal) == 256
        return pal

    def tiles(self):
        out = []
        for name, f in gt.TILES:
            if name in self.replace:
                out.append(self.replace[name])
            else:
                out.append((self.rename.get(name, name), f))
        names = [n for n, _ in out]
        assert len(set(names)) == 60, 'nombres repetidos en el tema %s: %s' % (self.key, sorted(n for n in names if names.count(n) > 1))
        return out


def mk_themes():
    th = []
    # ---- 2. HIELO
    th.append(Theme(
        2, 'ice', 'hielo',
        dict(grass=[(120, 150, 190), (215, 232, 250), (252, 254, 255)], forest=[(8, 34, 34), (30, 90, 80), (110, 170, 150)],
             dirt=[(44, 50, 66), (100, 110, 130), (170, 180, 196)], sand=[(130, 165, 205), (200, 225, 248), (240, 250, 255)],
             rock=[(30, 50, 90), (90, 130, 180), (190, 215, 240)], rockw=[(44, 40, 72), (100, 96, 150), (190, 185, 230)],
             metal=[(24, 36, 60), (90, 120, 160), (190, 210, 235)], obsid=[(2, 10, 34), (20, 50, 100), (80, 130, 190)],
             road=[(50, 60, 80), (110, 125, 150), (190, 200, 220)], foam=[(100, 170, 215), (210, 240, 255), (255, 255, 255)],
             space=[(0, 4, 26), (20, 40, 90), (70, 110, 170)]),
        ((10, 60, 130), (120, 220, 255)), ((0, 90, 110), (190, 255, 255)), ((10, 30, 50), (190, 240, 255)),
        [(150, 220, 255), (255, 255, 255), (200, 235, 255), (120, 180, 255), (250, 200, 255), (255, 240, 160), (80, 200, 230),
         (60, 90, 140), (255, 120, 120), (255, 210, 120), (230, 245, 255), (255, 190, 230), (110, 130, 200), (160, 255, 230),
         (40, 130, 170), (30, 50, 90)],
        (0.85, 0.95, 1.12),
        dict(GRASS1='SNOW1', GRASS2='SNOW2', GRASS3='PINEFOR', DIRT='FROZEN', DIRTST='FROZENST', SAND='ICEPLAIN', ROCK1='ICEROCK',
             ROCK2='ICEROCK2', ROCKCR='ICECRACK', WATER='ICEWATER', WATERW='ICEWATERW', BANKL='SNOWBANKL', BANKR='SNOWBANKR',
             BRIDGEH='ICEBRIDGE', BRIDGEE='ICEBRIDGEE', SHORET='SNOWSHORET', SHOREB='SNOWSHOREB', ISLAND='ICEBERG',
             LAVA='CRYO', LAVACR='CRYOCR', LAVAL='CRYOL', LAVAR='CRYOR', LAVAT='CRYOT', LAVAB='CRYOB', LAVABU='CRYOBU',
             OBSID='DEEPICE', OBSIDS='DEEPICEV', EMBER='SPARK', TURRET='SNOWTURRET', CRATER='SNOWCRATER', DUNES='DRIFTS',
             TREE='PINE', BUSH='SHRUB', TREES2='PINES2', GRDIRT='SNOWFROZEN', DIRTGR='FROZENSNOW', GRROCKL='SNOWICEL',
             GRROCKR='SNOWICER', SANDST='ICEPLAINST'),
        dict(LILY=('FLOES', ice_floes), BOULDER=('CRYSTAL', ice_crystal), CHECK=('ICETILE', ice_tile))))
    # ---- 3. DESIERTO
    th.append(Theme(
        3, 'desert', 'desierto',
        dict(grass=[(90, 84, 36), (160, 150, 70), (215, 205, 120)], forest=[(14, 50, 22), (50, 110, 50), (120, 170, 80)],
             dirt=[(70, 36, 20), (140, 80, 44), (200, 140, 90)], sand=[(170, 130, 70), (235, 200, 120), (255, 238, 176)],
             rock=[(80, 50, 30), (170, 110, 60), (235, 180, 110)], rockw=[(70, 24, 16), (150, 56, 34), (215, 120, 80)],
             metal=[(70, 60, 46), (160, 140, 106), (230, 215, 176)], obsid=[(14, 10, 10), (60, 44, 40), (130, 110, 100)],
             road=[(80, 60, 40), (130, 104, 72), (190, 164, 120)], foam=[(60, 150, 150), (180, 235, 225), (250, 255, 245)],
             space=[(10, 6, 30), (70, 30, 80), (190, 100, 90)]),
        ((0, 90, 110), (80, 230, 220)), ((40, 14, 0), (255, 140, 20)), ((60, 30, 0), (255, 230, 120)),
        [(220, 60, 40), (250, 210, 60), (250, 250, 230), (240, 120, 90), (160, 70, 150), (250, 150, 40), (60, 190, 200),
         (120, 70, 40), (200, 80, 60), (255, 235, 130), (220, 230, 255), (255, 170, 150), (110, 60, 100), (255, 190, 90),
         (40, 150, 150), (70, 40, 20)],
        (1.12, 1.0, 0.78),
        dict(GRASS1='STEPPE1', GRASS2='STEPPE2', GRASS3='SCRUB', DIRT='CLAY', DIRTST='CLAYST', ROCK1='SANDSTONE', ROCK2='REDROCK',
             ROCKCR='ROCKCRACK', WATER='OASIS', WATERW='OASISW', BANKL='OASISBANKL', BANKR='OASISBANKR', BRIDGEH='PLANKS',
             BRIDGEE='PLANKSE', SHORET='OASISSHORET', SHOREB='OASISSHOREB', ISLAND='OASISISLE', LAVA='TAR', LAVACR='TARCR',
             LAVAL='TARL', LAVAR='TARR', LAVAT='TART', LAVAB='TARB', LAVABU='TARBU', OBSID='BASALT', OBSIDS='BASALTV',
             EMBER='EMBERS', METAL='STONE', METALR='STONER', TURRET='DUNETURRET', TREE='ACACIA', BUSH='CACTUS',
             TREES2='CACTUS2', BOULDER='BOULDER', GRDIRT='STEPPECLAY', DIRTGR='CLAYSTEPPE', GRROCKL='STEPPEROCKL',
             GRROCKR='STEPPEROCKR'),
        dict(LILY=('PALM', d_palm), PIPEH=('BLOCKS', d_blocks), PIPEV=('COLUMN', d_column), PIPEC=('OBELISK', d_obelisk),
             HATCH=('PYRAMID', d_pyramid))))
    # ---- 4. CIUDAD
    ro = roof
    th.append(Theme(
        4, 'city', 'ciudad',
        dict(grass=[(14, 50, 24), (50, 120, 50), (110, 180, 80)], forest=[(6, 30, 16), (24, 80, 44), (70, 140, 80)],
             dirt=[(50, 52, 58), (110, 112, 120), (170, 172, 180)], sand=[(110, 108, 104), (170, 166, 158), (220, 216, 206)],
             rock=[(40, 42, 52), (100, 104, 120), (180, 186, 204)], rockw=[(60, 28, 24), (130, 64, 50), (200, 120, 96)],
             metal=[(26, 34, 50), (90, 110, 140), (190, 205, 225)], obsid=[(6, 8, 16), (30, 36, 54), (80, 92, 120)],
             road=[(16, 16, 20), (48, 50, 58), (214, 212, 196)], foam=[(40, 80, 110), (140, 190, 220), (240, 250, 255)],
             space=[(2, 2, 16), (16, 20, 50), (60, 60, 110)]),
        ((4, 16, 60), (40, 120, 200)), ((255, 30, 160), (30, 230, 255)), ((20, 16, 8), (255, 230, 130)),
        [(255, 40, 160), (40, 240, 255), (255, 255, 255), (255, 220, 40), (170, 80, 255), (255, 120, 30), (60, 255, 150),
         (90, 60, 120), (255, 80, 80), (255, 255, 140), (200, 230, 255), (255, 150, 220), (120, 60, 200), (255, 180, 60),
         (30, 160, 180), (50, 20, 70)],
        (0.9, 0.92, 1.0),
        dict(GRASS1='PARK1', GRASS2='PARK2', GRASS3='PARKTREES', DIRT='CONCRETE', DIRTST='CONCRETEST', SAND='SIDEWALK',
             ROCK1='GRAYROOF', ROCK2='BRICKROOF', ROCKCR='ROOFCRACK', WATER='CANAL', WATERW='CANALW', BANKL='QUAYL', BANKR='QUAYR',
             BRIDGEH='BRIDGE', BRIDGEE='BRIDGEEND', SHORET='QUAYT', SHOREB='QUAYB', ISLAND='PIER', OBSID='DARKROOF',
             METAL='STEELROOF', METALR='STEELROOFR', TURRET='FLAK', TREE='BIGTREE', BUSH='HEDGE', TREES2='PARKTREES2',
             GRDIRT='PARKCONC', DIRTGR='CONCPARK', GRROCKL='PARKROCKL', GRROCKR='PARKROCKR'),
        dict(LAVA=('ROOFTL', ro(l=True, t=True)), LAVACR=('ROOFT', ro(t=True)), LAVAL=('ROOFTR', ro(r=True, t=True)),
             LAVAR=('ROOFL', ro(l=True)), LAVAT=('ROOF', ro()), LAVAB=('ROOFR', ro(r=True)),
             OBSIDS=('ROOFBL', ro(l=True, b=True)), LAVABU=('ROOFB', ro(b=True)), EMBER=('ROOFBR', ro(r=True, b=True)),
             GRATE=('NEON', ro(core=c_neon)), LILY=('ROOFAC', ro(core=c_ac)), CRATER=('CROSSH', c_cross_h), BOULDER=('CROSSV', c_cross_v),
             SANDST=('PARKING', c_parking))))
    # ---- 5. NAVE ALIENIGENA
    th.append(Theme(
        5, 'alien', 'nave alienigena',
        dict(grass=[(30, 10, 50), (100, 40, 130), (200, 120, 220)], forest=[(6, 22, 26), (20, 80, 80), (80, 170, 150)],
             dirt=[(50, 12, 40), (120, 40, 90), (200, 100, 150)], sand=[(110, 100, 70), (200, 190, 140), (250, 240, 200)],
             rock=[(20, 14, 40), (70, 50, 110), (150, 130, 190)], rockw=[(14, 36, 30), (50, 100, 80), (140, 200, 160)],
             metal=[(14, 30, 34), (50, 100, 106), (150, 210, 200)], obsid=[(4, 2, 12), (30, 16, 56), (90, 60, 130)],
             road=[(30, 24, 44), (70, 60, 96), (150, 140, 176)], foam=[(60, 110, 80), (170, 240, 170), (240, 255, 230)],
             space=[(2, 0, 14), (30, 10, 70), (110, 60, 170)]),
        ((10, 70, 20), (150, 255, 100)), ((120, 0, 110), (255, 110, 255)), ((6, 30, 40), (120, 255, 255)),
        [(255, 60, 160), (150, 255, 60), (240, 255, 255), (255, 150, 255), (100, 60, 220), (255, 140, 40), (40, 230, 220),
         (100, 40, 80), (220, 40, 80), (210, 255, 120), (200, 220, 255), (255, 170, 220), (90, 40, 150), (255, 200, 60),
         (30, 150, 150), (50, 16, 60)],
        (0.9, 0.82, 1.1),
        dict(GRASS1='FUNGUS1', GRASS2='FUNGUS2', GRASS3='MOSS', DIRT='FLESH', DIRTST='FLESHST', SAND='BONE', ROCK1='CHITIN',
             ROCK2='CARAPACE', ROCKCR='CHITINCR', WATER='TOXIC', WATERW='TOXICW', BANKL='TOXBANKL', BANKR='TOXBANKR',
             BRIDGEH='RIBBRIDGE', BRIDGEE='RIBBRIDGEE', SHORET='TOXSHORET', SHOREB='TOXSHOREB', ISLAND='TOXISLAND',
             LAVA='PLASMA', LAVACR='PLASMACR', LAVAL='PLASMAL', LAVAR='PLASMAR', LAVAT='PLASMAT', LAVAB='PLASMAB',
             LAVABU='PLASMABU', OBSID='DARKCHITIN', OBSIDS='VEINS', EMBER='SPORE', METAL='BIOMECH', METALR='BIOMECHR',
             GRATE='RIBS', LIGHTS='BIOLIGHT', PIPEH='ARTERYH', PIPEV='ARTERYV', PIPEC='ARTERYC', HATCH='MAW', PAD='NEST',
             TURRET='STINGER', CRATER='SPAWNPIT', ROADH='SPINEH', ROADV='SPINEV', ROADX='SPINEX', DUNES='RIDGES',
             TREE='FUNGUSTREE', BUSH='SPORES', TREES2='SPORETREES', GRDIRT='FUNGUSFLESH', DIRTGR='FLESHFUNGUS',
             GRROCKL='FUNGUSCHITINL', GRROCKR='FUNGUSCHITINR', LIGHTS2='BIOLIGHT2', MLIGHT='BIOLIGHTV'),
        dict(BOULDER=('EGGS', a_eggs), CHECK=('HEX', a_hex), SANDST=('EYE', a_eye))))
    return th


THEMES = mk_themes()


# ---------------------------------------------------------------------------------------------------------
def with_noise(theme, fn):
    """Ejecuta fn con el ruido de gentiles resembrado para el tema (cada tema tiene su propio grano); lo restaura despues."""
    saveN, saveNR = [list(g) for g in gt.N], [list(g) for g in gt.NR]
    k = 1000 * theme.n
    gt.N[:] = [gt.tnoise(s + k) for s in range(1, 9)]
    gt.NR[:] = [gt.tnoise(100 + s + k, 0) for s in range(1, 5)]
    try:
        return fn()
    finally:
        gt.N[:] = saveN
        gt.NR[:] = saveNR


def render(theme):
    tiles = theme.tiles()
    pal = theme.palette()

    def go():
        img = [[0] * W for _ in range(H)]
        for t, (name, f) in enumerate(tiles):
            ox, oy = (t % COLS) * T, (t // COLS) * T
            for y in range(T):
                for x in range(T):
                    v = f(x, y)
                    assert 0 <= v < 256, '%s (%d,%d) = %d' % (name, x, y, v)
                    img[oy + y][ox + x] = v
        left = [1, 4, 5, 4, 3, 3, 2, 2, 2, 2, 3, 3, 4, 5, 7, 0]
        for x in range(16):
            img[gt.PANEL_ROW][x] = left[x]
            img[gt.PANEL_ROW][W - 1 - x] = left[x]
        return img
    return with_noise(theme, go), pal, tiles


def tiles_def(theme, tiles):
    L = ['# tiles%d.def -- generado por gentemas.py (tema %s). Lo lee map2bin.py (nombres de los tiles para escribir los mapas).' % (theme.n, theme.title),
         '# tile NOMBRE INDICE        indice 0-59 = fila*10 + columna en el tileset',
         '# range NOMBRE PRIMERO ULTIMO   rangos de la paleta de 256 (los mismos que el nivel 1: el codigo cicla agua, lava y luces)',
         '# panelrow N               fila del PNG que lleva el dibujo de los paneles (x 0-15 izquierdo, x 144-159 derecho)']
    for i, (name, _) in enumerate(tiles):
        L.append('tile %s %d' % (name, i))
    for name, a, b in gt.RANGES:
        L.append('range %s %d %d' % (name, a, b))
    L.append('panelrow %d' % gt.PANEL_ROW)
    return '\n'.join(L) + '\n'


def check_theme(theme, out_dir):
    """Ida y vuelta: se lee el PNG, se regenera cada tile y se compara; paleta; el .def; que los ciclos se usan."""
    img, pal, tiles = render(theme)
    w, h, rimg, rpal = pngio.read_indexed(os.path.join(out_dir, 'tileset%d.png' % theme.n))
    errs = []
    if (w, h) != (W, H):
        errs.append('tileset%d.png mide %dx%d' % (theme.n, w, h))
        return errs
    if rpal != pal:
        errs.append('tileset%d.png: la paleta no es la del tema' % theme.n)
    if rimg != img:
        errs.append('tileset%d.png: los pixeles no son los del tema' % theme.n)
    used = {v for r in rimg for v in r}
    for name, a, b in gt.RANGES[-3:]:
        if not any(a <= v <= b for v in used):
            errs.append('tema %s: el rango %s no se usa en ningun pixel' % (theme.key, name))
    names = {}
    for line in open(os.path.join(out_dir, 'tiles%d.def' % theme.n), encoding='utf-8'):
        f = line.split()
        if f and f[0] == 'tile':
            names[f[1]] = int(f[2])
    if sorted(names.values()) != list(range(60)) or names != {n: i for i, (n, _) in enumerate(tiles)}:
        errs.append('tiles%d.def no coincide con los tiles' % theme.n)
    return errs


def main():
    ap = argparse.ArgumentParser(description='tilesets de los niveles 2-5 de ESTELA')
    ap.add_argument('-o', '--out', default=os.path.join(HERE, '..', 'assets'))
    ap.add_argument('--preview', metavar='CARPETA', help='ademas escribe vistas previas ampliadas x4 (vistaN.png)')
    ap.add_argument('--check', action='store_true', help='comprobacion de ida y vuelta')
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    rc = 0
    for th in THEMES:
        img, pal, tiles = render(th)
        pngio.write_indexed(os.path.join(a.out, 'tileset%d.png' % th.n), W, H, img, pal)
        open(os.path.join(a.out, 'tiles%d.def' % th.n), 'w', encoding='utf-8', newline='\n').write(tiles_def(th, tiles))
        print('tileset%d.png (%s): %d colores usados, 60 tiles' % (th.n, th.title, len({v for r in img for v in r})))
        if a.preview:
            os.makedirs(a.preview, exist_ok=True)
            pngio.write_indexed(os.path.join(a.preview, 'vista%d.png' % th.n), W, H, img, pal, scale=4)
        if a.check:
            errs = check_theme(th, a.out)
            for e in errs:
                print('FALLO: ' + e, file=sys.stderr)
            if errs:
                rc = 2
    if a.check and rc == 0:
        print('gentemas: ida y vuelta correcta')
    return rc


if __name__ == '__main__':
    sys.exit(main())
