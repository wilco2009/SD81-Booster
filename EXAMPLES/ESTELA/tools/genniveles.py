#!/usr/bin/env python3
"""genniveles.py -- escribe los niveles 2-5 de ESTELA (assets/NIVEL2.LVL ... NIVEL5.LVL) para map2bin.py.

Cada nivel tiene su tema (tilesetN.png / tilesN.def, de gentemas.py), un mapa de 330-410 filas y las oleadas:
    2 HIELO (332 filas)      tundra, lago helado con puente, glaciar, canon criogenico, base y antesala del jefe
    3 DESIERTO (360 filas)   estepa, dunas, oasis con puente, pozos de alquitran, ruinas y el templo del jefe
    4 CIUDAD (384 filas)     calles y tejados, parque, rio con puente, polvora industrial y la azotea del jefe
    5 NAVE ALIENIGENA (400)  hongos y huevos, rio toxico, cuevas de carne y hueso, plasma, colmena y el nucleo
Las oleadas se reparten con una semilla fija: cada nivel mas seguidas (la separacion media baja de 175 a 105 pixeles de
scroll), con mas enemigos por oleada, combinaciones por los dos lados y mas torretas (enemigos de suelo sobre sus
plataformas); las mejoras (arma, escudo, bomba) se reparten a trozos y el jefe (tipo 6, trayectoria ENTER) sale 230
pixeles antes del final. Solo se usan las 9 trayectorias de paths.def (el codigo las lleva compiladas). La regla de
"como mucho 3 enemigos de 16x16 en una misma linea" se cumple por construccion: cada oleada se retrasa de 10 en 10 pixeles
hasta que cabe (y map2bin --check lo vuelve a comprobar). Es determinista: siempre sale lo mismo; despues se puede
editar a mano el .LVL.

Uso:  python genniveles.py [-o ../assets]
"""
import argparse
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import map2bin  # noqa: E402

W = 8


# ---------------------------------------------------------------------------------------------------------
# Constructor de mapas
# ---------------------------------------------------------------------------------------------------------
class MapB:
    def __init__(self, seed):
        self.rnd = random.Random(seed)
        self.rows = []          # rows[0] = el principio del nivel
        self.ground = []        # (fila, columna, nombre del tile): sitios de enemigos de suelo

    def pickw(self, opts):
        r = self.rnd.random() * sum(w for _, w in opts)
        for t, w in opts:
            r -= w
            if r < 0:
                return t
        return opts[-1][0]

    def row(self, opts):
        return [self.pickw(opts) for _ in range(W)]

    def add(self, row):
        assert len(row) == W, row
        self.rows.append(row)
        return len(self.rows) - 1

    def mark(self, col, name=None):
        """Marca la ultima fila anadida: el tile de la columna es un sitio para un enemigo de suelo."""
        r = len(self.rows) - 1
        self.ground.append((r, col, name or self.rows[r][col]))

    def fill(self, n, opts, edit=None):
        for i in range(n):
            row = self.row(opts)
            if edit:
                row = edit(i, row) or row
            self.add(row)

    def river(self, n, land, t, bridge_at, island_at=None, c0=2):
        """Rio vertical que serpentea (columnas c0..c0+3) con orillas, puente en la fila bridge_at (relativa) y bordes arriba/abajo."""
        for i in range(n):
            row = self.row(land)
            if i == 0:
                for c in (c0 + 1, c0 + 2):
                    row[c] = t['shoret']
            elif i == n - 1:
                for c in (c0 + 1, c0 + 2):
                    row[c] = t['shoreb']
            else:
                if i % 7 == 0 and not bridge_at - 4 <= i <= bridge_at + 4:
                    c0 = max(1, min(3, c0 + self.rnd.choice((-1, 1))))
                row[c0] = t['bankl']
                row[c0 + 1] = self.rnd.choice(t['water'])
                row[c0 + 2] = self.rnd.choice(t['water'][:3])
                row[c0 + 3] = t['bankr']
                if i == bridge_at:
                    row[c0 - 1] = t['bridgee']
                    for c in range(c0, c0 + 4):
                        row[c] = t['bridge']
                    row[c0 + 4] = t['bridgee']
                if island_at is not None and i == island_at:
                    row[c0 + 1] = t['island']
            self.add(row)

    def canyon(self, n, side, t, bridge_at, start_land):
        """Canon con un rio de lava (u otro) en las columnas 2-5 y un puente de metal; bordes t['top'] y t['bottom']."""
        for i in range(n):
            row = self.row(side)
            if i == 0:
                row = self.row(start_land)
                for c in range(2, 6):
                    row[c] = t['top']
            elif i == n - 1:
                for c in range(2, 6):
                    row[c] = t['bottom']
            else:
                row[2] = t['left']
                row[3] = self.rnd.choice(t['liquid'])
                row[4] = self.rnd.choice(t['liquid'])
                row[5] = t['right']
                if i == bridge_at:
                    row[2:6] = [t['metalr'], t['metal'], t['metal'], t['metalr']]
                if i in (bridge_at - 1, bridge_at + 1):
                    row[2:6] = [t['haz']] * 4
            self.add(row)

    def base(self, n, N, pipe_every=12, lights_row=None, pads=True, first_haz=True):
        """Zona de metal: luces en los laterales, tuberias cada pipe_every filas, plataformas (sitios de suelo)."""
        opts = [(N['metal'], 50), (N['metalr'], 20), (N['grate'], 12), (N['dark'], 8), (N['lights'], 6), (N['hatch'], 4)]
        for i in range(n):
            row = self.row(opts)
            row[0] = N['mlight']
            row[7] = N['mlight']
            if first_haz and i in (0, 1):
                row = [N['haz']] * W
            if i > 1 and (i - 2) % pipe_every == 0:
                row[1:7] = [N['pipeh']] * 6
            if pads and i > 1 and (i - 8) % pipe_every == 0:
                c = self.rnd.choice((2, 5))
                row[c] = N['pad']
                self.add(row)
                self.mark(c)
                continue
            if lights_row is not None and i == lights_row:
                row = [N['lights2']] * W
            self.add(row)

    def arena(self, n, N, pad_every=None):
        """Antesala del jefe: oscuro, damero y metal, luces laterales, franjas de luces y espacio con estrellas al final."""
        for i in range(n):
            row = self.row([(N['dark'], 45), (N['check'], 35), (N['metal'], 10), (N['grate'], 10)])
            row[0] = N['mlight']
            row[7] = N['mlight']
            if i in (0, 1):
                row = [N['haz']] * W
            if i >= n - 6:
                row = self.row([(N['stars'], 6), (N['space'], 3)])
            elif i >= 14 and (i - 14) % 6 == 0:
                row = [N['lights2']] * W
            self.add(row)


# ---------------------------------------------------------------------------------------------------------
# Niveles
# ---------------------------------------------------------------------------------------------------------
ICE_N = dict(metal='METAL', metalr='METALR', grate='GRATE', dark='DARK', lights='LIGHTS', hatch='HATCH', mlight='MLIGHT',
             pipeh='PIPEH', pad='PAD', haz='HAZARD', lights2='LIGHTS2', check='ICETILE', stars='STARS', space='SPACE')


def level2():
    b = MapB(82)
    snow = [('SNOW1', 55), ('SNOW2', 14), ('PINEFOR', 8), ('PINE', 6), ('PINES2', 4), ('SHRUB', 5), ('CRYSTAL', 3), ('SNOWCRATER', 2),
            ('FROZEN', 3)]

    def tundra(i, row):
        if 8 <= i <= 40:
            row[5] = 'ROADV'
        if i == 22:
            row = ['ROADH'] * W
            row[5] = 'ROADX'
        if i in (14, 30):
            row[4] = 'SNOWTURRET'
        return row
    b.fill(46, snow, tundra)
    b.ground += [(14, 4, 'SNOWTURRET'), (30, 4, 'SNOWTURRET')]
    b.river(50, snow, dict(shoret='SNOWSHORET', shoreb='SNOWSHOREB', bankl='SNOWBANKL', bankr='SNOWBANKR',
                           water=['ICEWATER', 'ICEWATER', 'ICEWATERW', 'FLOES'], bridge='ICEBRIDGE', bridgee='ICEBRIDGEE',
                           island='ICEBERG'), bridge_at=26, island_at=40)
    glacier = [('ICEPLAIN', 32), ('ICETILE', 14), ('ICEROCK', 12), ('ICECRACK', 10), ('FROZEN', 8), ('CRYSTAL', 6),
               ('ICEPLAINST', 8), ('DEEPICE', 6), ('DEEPICEV', 4)]

    def gl(i, row):
        if i == 0:
            return ['SNOWFROZEN'] * W
        if i in (14, 30, 50):
            c = (2, 5, 3)[(i // 16) % 3]
            row[c] = 'PAD'
            b.ground.append((len(b.rows), c, 'PAD'))
        return row
    b.fill(66, glacier, gl)
    b.canyon(60, [('DEEPICE', 40), ('DEEPICEV', 14), ('SPARK', 16), ('ICEROCK', 12), ('ICEROCK2', 10), ('ICECRACK', 8)],
             dict(top='CRYOT', bottom='CRYOB', left='CRYOL', right='CRYOR', liquid=['CRYO', 'CRYO', 'CRYOCR', 'CRYOBU'],
                  metal='METAL', metalr='METALR', haz='HAZARD'), bridge_at=30, start_land=glacier)
    b.base(70, ICE_N, lights_row=40)
    b.arena(40, ICE_N)
    return b, 'HIELO: tundra, lago helado, glaciar, canon criogenico, base y antesala del jefe', dict(gap=175, ntur=7, items=4)


DES_N = dict(metal='STONE', metalr='STONER', grate='GRATE', dark='DARK', lights='LIGHTS', hatch='PYRAMID', mlight='MLIGHT',
             pipeh='BLOCKS', pad='PAD', haz='HAZARD', lights2='LIGHTS2', check='CHECK', stars='STARS', space='SPACE')


def level3():
    b = MapB(83)
    steppe = [('STEPPE1', 50), ('STEPPE2', 14), ('SCRUB', 8), ('ACACIA', 6), ('CACTUS', 8), ('CACTUS2', 3), ('CLAY', 4), ('BOULDER', 3),
              ('CRATER', 2)]

    def start(i, row):
        if 8 <= i <= 36:
            row[5] = 'ROADV'
        if i == 20:
            row = ['ROADH'] * W
            row[5] = 'ROADX'
        if i in (12, 28):
            row[4] = 'DUNETURRET'
        return row
    b.fill(40, steppe, start)
    b.ground += [(12, 4, 'DUNETURRET'), (28, 4, 'DUNETURRET')]
    dunes = [('SAND', 36), ('DUNES', 30), ('SANDST', 12), ('SANDSTONE', 8), ('ROCKCRACK', 4), ('CLAY', 4), ('CLAYST', 4), ('PALM', 2)]

    def dn(i, row):
        if i < 3:
            return ['STEPPECLAY'] * W if i == 0 else [b.pickw([('CLAY', 5), ('CLAYST', 2), ('SAND', 2)]) for _ in range(W)]
        if i in (22, 44):
            c = (2, 5)[(i // 22) % 2]
            row[c] = 'PAD'
            b.ground.append((len(b.rows), c, 'PAD'))
        return row
    b.fill(70, dunes, dn)
    b.river(60, steppe + [('PALM', 6)], dict(shoret='OASISSHORET', shoreb='OASISSHOREB', bankl='OASISBANKL', bankr='OASISBANKR',
                                          water=['OASIS', 'OASIS', 'OASISW', 'OASIS'], bridge='PLANKS', bridgee='PLANKSE',
                                          island='OASISISLE'), bridge_at=32, island_at=18)
    side = [('BASALT', 45), ('BASALTV', 12), ('EMBERS', 18), ('SANDSTONE', 10), ('REDROCK', 10), ('ROCKCRACK', 5)]
    b.canyon(70, side, dict(top='TART', bottom='TARB', left='TARL', right='TARR', liquid=['TAR', 'TAR', 'TARCR', 'TARBU'],
                            metal='STONE', metalr='STONER', haz='HAZARD'), bridge_at=35, start_land=dunes)

    def ruins(i, row):
        if i in (0, 1):
            return [b.pickw([('STONE', 3), ('CLAY', 1)]) for _ in range(W)]
        if i % 10 == 4:
            row = ['BLOCKS'] * W
            row[3] = 'STONE'
            row[4] = 'STONE'          # abertura en el muro
        elif i % 10 == 6:
            row[1], row[6] = 'COLUMN', 'COLUMN'
            row[3] = row[4] = 'OBELISK' if i % 20 == 6 else row[3]
        elif i % 10 == 8:
            c = b.rnd.choice((2, 5))
            row[c] = 'PAD'
            b.ground.append((len(b.rows), c, 'PAD'))
        return row
    b.fill(60, [('STONE', 40), ('STONER', 20), ('SAND', 15), ('SANDST', 10), ('CLAYST', 8), ('BOULDER', 7)], ruins)
    # templo: piramides y bloques hasta la antesala
    def temple(i, row):
        if i in (0, 1):
            return ['HAZARD'] * W
        if i % 8 == 3:
            row[3:5] = ['PYRAMID'] * 2
        if i >= 40:
            row = b.row([('STARS', 6), ('SPACE', 3)])
        elif i >= 12 and (i - 12) % 7 == 0:
            row = ['LIGHTS2'] * W
        return row
    b.fill(50, [('STONE', 35), ('BLOCKS', 20), ('CHECK', 25), ('DARK', 10), ('STONER', 10)], temple)
    return b, 'DESIERTO: estepa, dunas, oasis, pozos de alquitran, ruinas y templo', dict(gap=150, ntur=9, items=5)


def level4():
    b = MapB(84)
    park = [('PARK1', 50), ('PARK2', 14), ('PARKTREES', 10), ('BIGTREE', 8), ('HEDGE', 8), ('PARKTREES2', 6), ('CONCRETE', 4)]

    # --- ciudad: una avenida vertical en la columna 3 con calles cada 12 filas y manzanas de tejados
    def block(rows, c0, c1, r0, r1, style):
        """Dibuja un edificio de tejado en las filas r0..r1 y columnas c0..c1 (inclusive) de la lista rows."""
        for r in range(r0, r1 + 1):
            for c in range(c0, c1 + 1):
                top, bot = r == r1, r == r0
                lft, rgt = c == c0, c == c1
                # la lista de filas va de abajo (0) a arriba: "t" = parte alta del edificio en pantalla = r1
                name = {(True, False, True, False): 'ROOFTL', (True, False, False, False): 'ROOFT',
                        (True, False, False, True): 'ROOFTR', (False, False, True, False): 'ROOFL',
                        (False, False, False, False): 'ROOF', (False, False, False, True): 'ROOFR',
                        (False, True, True, False): 'ROOFBL', (False, True, False, False): 'ROOFB',
                        (False, True, False, True): 'ROOFBR'}.get((top, bot, lft, rgt))
                if name is None:                       # edificio de una sola fila o columna: tejado liso con pretil parcial
                    name = 'ROOF'
                rows[r][c] = name
        # decoraciones del tejado en el interior
        inner = [(r, c) for r in range(r0 + 1, r1) for c in range(c0 + 1, c1)]
        for r, c in inner:
            x = b.rnd.random()
            if style == 'neon' and x < 0.5:
                rows[r][c] = 'NEON'
            elif x < 0.28:
                rows[r][c] = 'ROOFAC'
        return rows

    n_city = 198
    rows = [[None] * W for _ in range(n_city)]
    for r in range(n_city):
        for c in range(W):
            rows[r][c] = b.pickw([('SIDEWALK', 60), ('CONCRETE', 20), ('CONCRETEST', 10), ('PARK1', 10)])
    # calles horizontales cada 12 filas (11, 23, ...), avenida en la columna 3
    streets = list(range(11, n_city - 1, 12))
    for r in range(n_city):
        rows[r][3] = 'ROADV'
    for r in streets:
        rows[r] = ['ROADH'] * W
        rows[r][3] = 'ROADX'
        rows[r][1] = 'CROSSH' if False else rows[r][1]
    for r in streets:
        rows[r - 1][3] = 'CROSSV' if False else rows[r - 1][3]
    prev = 0
    for si, r in enumerate(streets + [n_city]):
        r0, r1 = prev + (1 if prev else 0), r - 1
        prev = r
        if r1 - r0 < 4:
            continue
        mid = (r0 + r1) // 2
        style = 'neon' if si % 3 == 1 else 'plain'
        # manzana izquierda (columnas 0-2): dos edificios o un parque; manzana derecha (4-7)
        if si % 4 == 2:
            for rr in range(r0, r1 + 1):
                for c in (0, 1, 2):
                    rows[rr][c] = b.pickw(park)
        else:
            block(rows, 0, 2, r0, mid - 1 if mid - r0 >= 3 else r1, style)
            if mid - r0 >= 3 and r1 - mid >= 3:
                block(rows, 0, 2, mid + 1, r1, 'plain')
        if si % 4 == 1:
            for rr in range(r0, r1 + 1):
                for c in (4, 5, 6, 7):
                    rows[rr][c] = b.pickw(park)
        else:
            block(rows, 4, 7, r0, r1, style)
        # pasos de cebra junto a las calles
        if r0 > 0 and rows[r0 - 1][3] == 'ROADV' and False:
            pass
    for r in streets:
        for c in (2, 4):
            rows[r][c] = rows[r][c]
        rows[r][5] = 'CROSSH' if rows[r][5] == 'ROADH' and (r // 12) % 2 == 0 else rows[r][5]
    for r in range(n_city):
        b.add(rows[r])
        if r % 24 == 5:
            for c, t in ((0, 'ROOFAC'), (5, 'ROOFAC'), (6, 'ROOFAC')):
                if rows[r][c] == 'ROOFAC':
                    b.mark(c, 'ROOFAC')
    # el rio, con puente
    b.river(56, park, dict(shoret='QUAYT', shoreb='QUAYB', bankl='QUAYL', bankr='QUAYR',
                           water=['CANAL', 'CANAL', 'CANALW', 'CANAL'], bridge='BRIDGE', bridgee='BRIDGEEND', island='PIER'),
            bridge_at=28, island_at=44)
    # zona industrial: metal y tuberias, con cañones
    b.base(70, dict(metal='STEELROOF', metalr='STEELROOFR', grate='NEON', dark='DARK', lights='LIGHTS', hatch='HATCH',
                    mlight='MLIGHT', pipeh='PIPEH', pad='PAD', haz='HAZARD', lights2='LIGHTS2', check='CHECK', stars='STARS',
                    space='SPACE'), pipe_every=10, lights_row=36)
    # azotea del jefe: tejados de acero con luces y, al final, el espacio
    for r in range(60):
        if r < 8:
            row = [b.pickw([('SIDEWALK', 60), ('CONCRETE', 25), ('CONCRETEST', 15)]) for _ in range(W)]
            if r % 7 == 0:
                row = ['ROADH'] * W
        else:
            row = b.row([('ROOF', 45), ('ROOFAC', 15), ('STEELROOF', 25), ('NEON', 15)])
            row[0] = row[7] = 'MLIGHT'
            if r in (8, 9):
                row = ['HAZARD'] * W
        if r >= 54:
            row = b.row([('STARS', 6), ('SPACE', 3)])
        b.add(row)
    return b, 'CIUDAD: calles y tejados, parque, rio con puente, zona industrial y azotea del jefe', dict(gap=125, ntur=11, items=5)


ALIEN_N = dict(metal='BIOMECH', metalr='BIOMECHR', grate='RIBS', dark='DARK', lights='BIOLIGHT', hatch='MAW', mlight='BIOLIGHTV',
               pipeh='ARTERYH', pad='NEST', haz='HAZARD', lights2='BIOLIGHT2', check='HEX', stars='STARS', space='SPACE')


def level5():
    b = MapB(85)
    fungus = [('FUNGUS1', 45), ('FUNGUS2', 14), ('MOSS', 10), ('FUNGUSTREE', 8), ('SPORES', 8), ('SPORETREES', 5), ('EGGS', 6),
              ('FLESH', 4)]

    def fi(i, row):
        if 8 <= i <= 40:
            row[5] = 'SPINEV'
        if i == 22:
            row = ['SPINEH'] * W
            row[5] = 'SPINEX'
        if i in (14, 30):
            row[4] = 'STINGER'
        return row
    b.fill(46, fungus, fi)
    b.ground += [(14, 4, 'STINGER'), (30, 4, 'STINGER')]
    b.river(50, fungus, dict(shoret='TOXSHORET', shoreb='TOXSHOREB', bankl='TOXBANKL', bankr='TOXBANKR',
                             water=['TOXIC', 'TOXIC', 'TOXICW', 'LILY'], bridge='RIBBRIDGE', bridgee='RIBBRIDGEE',
                             island='TOXISLAND'), bridge_at=26, island_at=38)
    caves = [('FLESH', 28), ('FLESHST', 12), ('BONE', 14), ('CHITIN', 12), ('CARAPACE', 8), ('CHITINCR', 6), ('EGGS', 10), ('EYE', 4),
             ('RIDGES', 6)]

    def cv(i, row):
        if i == 0:
            return ['FLESHFUNGUS'] * W
        if i in (16, 34, 52):
            c = (2, 5, 3)[(i // 18) % 3]
            row[c] = 'STINGER'
            b.ground.append((len(b.rows), c, 'STINGER'))
        return row
    b.fill(64, caves, cv)
    b.canyon(60, [('DARKCHITIN', 40), ('VEINS', 14), ('SPORE', 16), ('CHITIN', 12), ('CARAPACE', 10), ('CHITINCR', 8)],
             dict(top='PLASMAT', bottom='PLASMAB', left='PLASMAL', right='PLASMAR',
                  liquid=['PLASMA', 'PLASMA', 'PLASMACR', 'PLASMABU'], metal='BIOMECH', metalr='BIOMECHR', haz='HAZARD'),
             bridge_at=30, start_land=caves)
    b.base(80, ALIEN_N, pipe_every=10, lights_row=44)
    def hive(i, row):
        if i in (0, 1):
            return ['HAZARD'] * W
        if i >= 10 and (i - 10) % 6 == 0:
            return ['BIOLIGHT2'] * W
        return row
    b.fill(40, [('HEX', 40), ('BIOMECH', 25), ('RIBS', 15), ('DARK', 10), ('EYE', 4)], hive)
    b.fill(60, [('HEX', 50), ('DARK', 20), ('BIOLIGHT2', 5), ('RIBS', 15), ('EYE', 3)],
           lambda i, row: (b.row([('STARS', 6), ('SPACE', 3)]) if i >= 54 else (['BIOLIGHT2'] * W if i % 9 == 8 else row)))
    return b, 'NAVE ALIENIGENA: hongos, rio toxico, cuevas, plasma, colmena y nucleo', dict(gap=105, ntur=12, items=6)


# ---------------------------------------------------------------------------------------------------------
# Oleadas
# ---------------------------------------------------------------------------------------------------------
# patrones aereos: (nombre, tipo, trayectoria, x, x espejo, cuenta (min, max), separacion, flags, comentario)
PATTERNS = [
    ('sine', 'E1', 'SINE', 48, 96, (4, 6), 14, 0, 'platillos serpenteando'),
    ('dive', 'E2', 'DIVE', 24, 120, (3, 4), 18, 0, 'dardos en picado'),
    ('zig', 'E3', 'ZIGZAG', 32, 96, (3, 4), 24, 0, 'cangrejos en zigzag'),
    ('sweep', 'E4', 'SWEEP', 20, 130, (3, 4), 22, 0, 'giratorios cruzando'),
    ('hover', 'E5', 'HOVER', 48, 48, (2, 3), 40, 2, 'bombarderos que se quedan disparando'),
    ('rowdown', 'E2', 'DOWN', 40, 40, (3, 3), 32, 2, 'fila de dardos'),
]
# pesos de los patrones por nivel (sine, dive, zig, sweep, hover, rowdown)
WEIGHTS = {2: (3, 3, 3, 2, 2, 1), 3: (2, 3, 3, 3, 3, 2), 4: (2, 3, 3, 3, 4, 3), 5: (2, 3, 3, 4, 4, 3)}


def build_wave(spec, paths):
    typ = map2bin.TYPES[spec['type']][0]
    pidx = [n for n, _ in paths].index(spec['path'])
    return dict(pos=spec['pos'], type=typ, x=spec['x'], path=pidx, count=spec['count'], spacing=spec['spacing'],
                flags=spec['flags'], line=0)


def coverage(w, paths, scroll=1):
    """Cobertura de una oleada: {tick: [(y0, y1, peso)]} (la misma cuenta que map2bin.simulate)."""
    cov = {}
    tname, th, weight = map2bin.TYPE_BY_NUM[w['type']]
    steps = paths[w['path']][1]
    sign = -1 if w['flags'] & map2bin.F_MIRROR else 1
    Z = map2bin.ZONE_ROWS
    for i in range(w['count']):
        row = w['flags'] & map2bin.F_ROW
        tick = w['pos'] * scroll + (0 if row else i * w['spacing'])
        x = w['x'] + (i * w['spacing'] if row else 0)
        y = -th
        age = 0
        while age < map2bin.LEAVE_TICKS:
            y0, y1 = max(0, y), min(Z - 1, y + th - 1)
            if y0 <= y1:
                cov.setdefault(tick, []).append((y0, y1, weight))
            if tname == 'BOSS' and age >= 400:
                break
            dx, dy = steps[min(age, len(steps) - 1)]
            x += sign * dx
            y += dy
            if w['flags'] & map2bin.F_GROUND and tick % scroll == 0:
                y += 1
            tick += 1
            age += 1
            if y >= Z or x < -40 or x > 200 or y < -th - 40:
                break
    return cov


def fits(total, cov, limit=3.0):
    for tick, lst in cov.items():
        allw = total.get(tick, []) + lst
        diff = [0.0] * (map2bin.ZONE_ROWS + 1)
        for y0, y1, wt in allw:
            diff[y0] += wt
            diff[y1 + 1] -= wt
        acc = 0.0
        for r in range(map2bin.ZONE_ROWS):
            acc += diff[r]
            if acc > limit + 1e-9:
                return False
    return True


def add_cov(total, cov):
    for t, lst in cov.items():
        total.setdefault(t, []).extend(lst)


def make_waves(n, b, info, paths):
    rnd = random.Random(900 + n)
    rows = len(b.rows)
    endpos = 16 * rows - 100
    waves = []          # (pos, texto, comentario, dict)
    total = {}

    def put(spec, comment):
        w = build_wave(spec, paths)
        c = coverage(w, paths)
        add_cov(total, c)
        waves.append((spec['pos'], spec, comment))
    # 1) el jefe y los enemigos de suelo (posiciones fijas)
    boss = dict(pos=endpos - 230, type='BOSS', x=64, path='ENTER', count=1, spacing=0, flags=0)
    put(boss, 'el jefe de 32x24 baja y se queda')
    sites = [g for g in b.ground if 12 <= g[0] < rows - 70]
    rnd.shuffle(sites)
    sites = sorted(sites[:info['ntur']])
    for r, c, name in sites:
        spec = dict(pos='r%d' % r, type='E6', x=16 + 16 * c, path='STILL', count=1, spacing=0, flags=4)
        w = build_wave(dict(spec, pos=16 * r - 100), paths)
        cov = coverage(w, paths)
        if not fits(total, cov):
            continue
        add_cov(total, cov)
        waves.append((16 * r - 100, spec, 'enemigo de suelo sobre %s (fila %d)' % (name, r)))
    # 2) las oleadas aereas, retrasadas de 10 en 10 hasta que caben
    weights = WEIGHTS[n]
    pos = 70
    last_item = pos
    ni = 0
    item_spots = [int(endpos * (k + 1) / (info['items'] + 1)) for k in range(info['items'])]
    nxt_item = 0
    while pos < endpos - 330:
        # una mejora cuando toca
        if nxt_item < len(item_spots) and pos >= item_spots[nxt_item]:
            kind = ('ITEM1', 'ITEM2', 'ITEM1', 'ITEM3', 'ITEM2', 'ITEM3')[nxt_item % 6]
            spec = dict(pos=pos, type=kind, x=rnd.choice((48, 64, 80, 96)), path='SLOW', count=1, spacing=0, flags=0)
            for _ in range(60):
                cov = coverage(build_wave(spec, paths), paths)
                if fits(total, cov):
                    break
                spec['pos'] += 10
            add_cov(total, cov)
            waves.append((spec['pos'], spec, {'ITEM1': 'mejora de arma', 'ITEM2': 'escudo', 'ITEM3': 'bomba'}[kind]))
            nxt_item += 1
            pos = max(pos, spec['pos']) + 60
            continue
        pat = rnd.choices(PATTERNS, weights=weights)[0]
        name, typ, path, x, xm, (c0, c1), sp, fl, com = pat
        count = rnd.randint(c0, min(c1, c0 + (n - 2) // 2 + 1))
        mirror = rnd.random() < 0.5
        spec = dict(pos=pos, type=typ, x=xm if mirror else x, path=path, count=count, spacing=sp, flags=fl | (1 if mirror else 0))
        if name == 'hover':
            spec['x'] = rnd.choice((32, 48, 64))
        # que quepa: retrasar de 10 en 10
        for _ in range(60):
            w = build_wave(spec, paths)
            cov = coverage(w, paths)
            if fits(total, cov):
                break
            spec['pos'] += 10
        add_cov(total, cov)
        put_pos = spec['pos']
        waves.append((put_pos, spec, com + (' (espejo)' if mirror else '')))
        # los niveles dificiles lanzan a veces una segunda oleada casi a la vez por el otro lado
        if n >= 4 and rnd.random() < 0.25 + 0.1 * (n - 4):
            pat2 = rnd.choices(PATTERNS[:4], weights=weights[:4])[0]
            n2, t2, p2, x2, xm2, (a2, b2), s2, f2, c2 = pat2
            mir2 = not mirror
            spec2 = dict(pos=put_pos + rnd.randint(20, 70), type=t2, x=xm2 if mir2 else x2, path=p2, count=a2, spacing=s2,
                         flags=f2 | (1 if mir2 else 0))
            for _ in range(60):
                w = build_wave(spec2, paths)
                cov = coverage(w, paths)
                if fits(total, cov):
                    add_cov(total, cov)
                    waves.append((spec2['pos'], spec2, c2 + (' (espejo)' if mir2 else '') + ', a la vez por el otro lado'))
                    break
                spec2['pos'] += 10
        pos = put_pos + int(info['gap'] * rnd.uniform(0.8, 1.25)) + count * 6
    waves.sort(key=lambda t: t[0])
    return waves, endpos


def spec_text(s):
    t = 'pos=%s    type=%s x=%d path=%s count=%d' % (s['pos'], s['type'], s['x'], s['path'], s['count'])
    if s['count'] > 1:
        t += ' spacing=%d' % s['spacing']
    fl = []
    if s['flags'] & 1:
        fl.append('mirror')
    if s['flags'] & 2:
        fl.append('row')
    if s['flags'] & 4:
        fl.append('ground')
    if fl:
        t += ' flags=' + ','.join(fl)
    return t


LEVELS = {2: level2, 3: level3, 4: level4, 5: level5}


def write_level(n, out_dir, paths):
    b, title, info = LEVELS[n]()
    rows = len(b.rows)
    waves, endpos = make_waves(n, b, info, paths)
    L = []
    L.append('; NIVEL%d.LVL -- %s (lo lee map2bin.py; formatos en claude/planning/juego160.md, seccion 8).' % (n, title))
    L.append('; Generado por genniveles.py; despues se puede editar a mano (genniveles.py lo sobrescribe si se vuelve a ejecutar).')
    L.append(';')
    L.append('; El mapa (al final) tiene 8 tiles de ancho; la linea de ARRIBA es el final del nivel y la de ABAJO el principio.')
    L.append('; %d filas de 16 pixeles = %d pixeles de scroll; el jefe sale en la posicion %d.' % (rows, 16 * rows, endpos - 230))
    L.append('; Regla de diseno (la comprueba map2bin --check): como mucho 3 enemigos de 16x16 cortan una misma linea de la zona.')
    L.append('')
    L.append('include tiles%d.def' % n)
    L.append('include paths.def')
    L.append('')
    L.append('name NIVEL%d' % n)
    L.append('scroll 1')
    L.append('')
    for pos, spec, com in waves:
        L.append('wave ' + spec_text(spec) + ('    ; ' + com if com else ''))
    L.append('')
    L.append('map')
    for i, row in enumerate(reversed(b.rows)):
        r = rows - 1 - i
        L.append('  ' + ' '.join('%-9s' % t for t in row).rstrip() + '    ; fila %d' % r)
    L.append('end')
    path = os.path.join(out_dir, 'NIVEL%d.LVL' % n)
    open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(L) + '\n')
    print('NIVEL%d.LVL (%s): %d filas, %d oleadas' % (n, title.split(':')[0], rows, len(waves)))


def main():
    ap = argparse.ArgumentParser(description='escribe assets/NIVEL2.LVL ... NIVEL5.LVL')
    ap.add_argument('-o', '--out', default=os.path.join(HERE, '..', 'assets'))
    ap.add_argument('--only', type=int, nargs='*')
    a = ap.parse_args()
    lv = map2bin.parse_level(os.path.join(a.out, 'paths.def'))
    for n in (a.only or sorted(LEVELS)):
        write_level(n, a.out, lv.paths)
    return 0


if __name__ == '__main__':
    sys.exit(main())
