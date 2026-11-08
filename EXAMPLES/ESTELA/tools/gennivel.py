#!/usr/bin/env python3
"""gennivel.py -- escribe el nivel de ejemplo assets/NIVEL1.LVL (mapa de 280 filas y oleadas) para map2bin.py.

El mapa son cinco escenarios, de abajo (el principio) hacia arriba: pradera con camino, rio con puente, desierto,
canon de lava con puente y base metalica con jefe. Es un ejemplo: el texto resultante (NIVEL1.LVL) es lo que se
edita a mano despues; este script solo lo genera una vez (y es determinista: siempre sale el mismo).

Uso:  python gennivel.py [-o ../assets/NIVEL1.LVL]
"""
import argparse
import os
import random

W = 8
ROWS = 280


def build_map():
    rnd = random.Random(81)
    rows = []      # rows[0] = el principio del nivel

    def pickw(opts):
        r = rnd.random() * sum(w for _, w in opts)
        for t, w in opts:
            r -= w
            if r < 0:
                return t
        return opts[-1][0]

    meadow = [('GRASS1', 55), ('GRASS2', 14), ('GRASS3', 10), ('BUSH', 6), ('TREE', 4), ('TREES2', 3), ('BOULDER', 3), ('CRATER', 2), ('DIRT', 3)]
    desert = [('SAND', 40), ('DUNES', 28), ('SANDST', 12), ('ROCK1', 8), ('ROCKCR', 4), ('DIRT', 4), ('DIRTST', 4)]
    canyon_side = [('OBSID', 45), ('OBSIDS', 12), ('EMBER', 18), ('ROCK1', 10), ('ROCK2', 10), ('ROCKCR', 5)]

    # --- 1. pradera con un camino vertical en la columna 5 y una carretera que lo cruza (filas 0-47)
    for r in range(48):
        row = [pickw(meadow) for _ in range(W)]
        if 8 <= r <= 40:
            row[5] = 'ROADV'
        if r == 22:
            row = ['ROADH'] * W
            row[5] = 'ROADX'
        if r in (14, 30):
            row[4] = 'TURRET'
        rows.append(row)

    # --- 2. rio (filas 48-95): bancos BANKL / BANKR con agua en medio, que serpentea; puente en la fila 70
    c0 = 2
    for r in range(48, 96):
        if r == 48:
            row = [pickw(meadow) for _ in range(W)]
            for c in (c0 + 1, c0 + 2):
                row[c] = 'SHORET'
        elif r == 95:
            row = [pickw(meadow) for _ in range(W)]
            for c in (c0 + 1, c0 + 2):
                row[c] = 'SHOREB'
        else:
            if r % 7 == 0 and r not in range(66, 75):
                c0 = max(1, min(3, c0 + rnd.choice((-1, 1))))
            row = [pickw(meadow) for _ in range(W)]
            row[c0] = 'BANKL'
            row[c0 + 1] = rnd.choice(['WATER', 'WATER', 'WATERW', 'LILY'])
            row[c0 + 2] = rnd.choice(['WATER', 'WATER', 'WATERW'])
            row[c0 + 3] = 'BANKR'
            if r == 70:
                row[c0 - 1] = 'BRIDGEE'
                for c in range(c0, c0 + 4):
                    row[c] = 'BRIDGEH'
                row[c0 + 4] = 'BRIDGEE'
            if r == 82:
                row[c0 + 1] = 'ISLAND'
        rows.append(row)

    # --- 3. desierto (filas 96-135), con la entrada desde la pradera
    for r in range(96, 136):
        if r in (96, 97):
            row = ['GRDIRT'] * W if r == 96 else ['DIRT'] * W
        elif r < 102:
            row = [pickw([('DIRT', 5), ('DIRTST', 2), ('SAND', 2)]) for _ in range(W)]
        else:
            row = [pickw(desert) for _ in range(W)]
        rows.append(row)

    # --- 4. canon de lava (filas 136-187): lava en las columnas 2-5, con un puente metalico en la fila 160
    for r in range(136, 188):
        row = [pickw(canyon_side) for _ in range(W)]
        if r == 136:
            row = [pickw(desert) for _ in range(W)]
            for c in range(2, 6):
                row[c] = 'LAVAT'
        elif r == 187:
            for c in range(2, 6):
                row[c] = 'LAVAB'
        else:
            row[2] = 'LAVAL'
            row[3] = rnd.choice(['LAVA', 'LAVA', 'LAVACR', 'LAVABU'])
            row[4] = rnd.choice(['LAVA', 'LAVA', 'LAVACR', 'LAVABU'])
            row[5] = 'LAVAR'
            if r == 160:
                row[2:6] = ['METALR', 'METAL', 'METAL', 'METALR']
            if r in (159, 161):
                row[2:6] = ['HAZARD'] * 4
        rows.append(row)

    # --- 5. base metalica (filas 188-241): luces en los laterales, tuberias y plataformas de cañon
    for r in range(188, 242):
        row = [pickw([('METAL', 50), ('METALR', 20), ('GRATE', 12), ('DARK', 8), ('LIGHTS', 6), ('HATCH', 4)]) for _ in range(W)]
        row[0] = 'MLIGHT'
        row[7] = 'MLIGHT'
        if r in (188, 189):
            row = ['HAZARD'] * W
        if (r - 190) % 12 == 0:
            row[1:7] = ['PIPEH'] * 6
        if (r - 196) % 12 == 0:
            row[rnd.choice((2, 5))] = 'PAD'
        if r == 220:
            row = ['LIGHTS2'] * W
        rows.append(row)

    # --- 6. antesala del jefe (filas 242-279)
    for r in range(242, ROWS):
        row = [pickw([('DARK', 45), ('CHECK', 35), ('METAL', 10), ('GRATE', 10)]) for _ in range(W)]
        row[0] = 'MLIGHT'
        row[7] = 'MLIGHT'
        if r in (242, 243):
            row = ['HAZARD'] * W
        if r >= 274:
            row = [pickw([('STARS', 6), ('SPACE', 3)]) for _ in range(W)]
        elif r >= 256 and (r - 256) % 6 == 0:
            row = ['LIGHTS2'] * W
        rows.append(row)
    assert len(rows) == ROWS
    return rows


# (pos, type, x, path, count, spacing, flags, comentario)
WAVES = [
    ('# --- pradera', None),
    ('pos=60    type=E1 x=48  path=SINE   count=4 spacing=16', 'cuatro platillos serpenteando'),
    ('pos=r17   type=E6 x=40  path=STILL  count=1 flags=ground', 'tanque junto a la plataforma de la fila 14'),
    ('pos=230   type=E2 x=24  path=DIVE   count=3 spacing=18', 'tres dardos en picado desde la izquierda'),
    ('pos=r22   type=E6 x=88  path=STILL  count=1 flags=ground', 'tanque en la carretera'),
    ('pos=420   type=E1 x=96  path=SINE   count=4 spacing=16 flags=mirror', 'platillos desde la derecha'),
    ('pos=600   type=E3 x=32  path=ZIGZAG count=3 spacing=24', 'cangrejos en zigzag'),
    ('pos=700   type=ITEM1 x=64 path=SLOW count=1', 'mejora de arma'),
    ('pos=780   type=E2 x=40  path=DOWN   count=3 spacing=32 flags=row', 'fila de tres dardos'),
    ('# --- rio', None),
    ('pos=960   type=E4 x=20  path=SWEEP  count=3 spacing=22', 'giratorios cruzando el rio'),
    ('pos=1160  type=E4 x=130 path=SWEEP  count=3 spacing=22 flags=mirror', 'y desde el otro lado'),
    ('pos=1340  type=E1 x=64  path=SINE   count=4 spacing=14', ''),
    ('pos=1500  type=E5 x=48  path=HOVER  count=2 spacing=40 flags=row', 'dos bombarderos que se quedan disparando'),
    ('# --- desierto', None),
    ('pos=1800  type=E3 x=96  path=ZIGZAG count=3 spacing=24 flags=mirror', ''),
    ('pos=1980  type=E2 x=24  path=DIVE   count=3 spacing=18', ''),
    ('pos=2150  type=E2 x=120 path=DIVE   count=3 spacing=18 flags=mirror', ''),
    ('pos=2310  type=ITEM2 x=100 path=SLOW count=1', 'escudo'),
    ('pos=2400  type=E5 x=56  path=HOVER  count=2 spacing=36 flags=row', ''),
    ('# --- canon de lava', None),
    ('pos=2700  type=E4 x=20  path=SWEEP  count=3 spacing=22', ''),
    ('pos=r172  type=E6 x=100 path=STILL  count=1 flags=ground', 'torreta en el borde del canon'),
    ('pos=2880  type=E1 x=64  path=SINE   count=4 spacing=14', ''),
    ('pos=3060  type=E3 x=40  path=ZIGZAG count=3 spacing=24', ''),
    ('# --- base', None),
    ('pos=r198  type=E6 x=32  path=STILL  count=1 flags=ground', 'cañon en la plataforma'),
    ('pos=3330  type=E5 x=40  path=HOVER  count=2 spacing=48 flags=row', ''),
    ('pos=r209  type=E6 x=96  path=STILL  count=1 flags=ground', ''),
    ('pos=3640  type=E2 x=24  path=DIVE   count=3 spacing=18', ''),
    ('pos=3800  type=E2 x=120 path=DIVE   count=3 spacing=18 flags=mirror', ''),
    ('pos=3980  type=ITEM3 x=80 path=SLOW count=1', 'bomba'),
    ('# --- jefe', None),
    ('pos=4150  type=BOSS x=64 path=ENTER count=1', 'el jefe de 32x24 baja hasta y = 26 y se queda'),
]


def main():
    ap = argparse.ArgumentParser(description='escribe assets/NIVEL1.LVL')
    here = os.path.dirname(os.path.abspath(__file__))
    ap.add_argument('-o', '--out', default=os.path.join(here, '..', 'assets', 'NIVEL1.LVL'))
    a = ap.parse_args()
    rows = build_map()
    L = []
    L.append('; NIVEL1.LVL -- nivel de ejemplo de ESTELA (lo lee map2bin.py; formatos en claude/planning/juego160.md, seccion 8).')
    L.append('; Generado una vez por gennivel.py; a partir de aqui se edita a mano.')
    L.append(';')
    L.append('; El mapa (al final) tiene 8 tiles de ancho; la linea de ARRIBA es el final del nivel y la de ABAJO el principio.')
    L.append('; Reparto: filas 0-47 pradera, 48-95 rio (puente en la 70), 96-135 desierto, 136-187 canon de lava (puente en la 160),')
    L.append(';          188-241 base, 242-279 antesala del jefe y espacio. Cada fila de tiles son 16 pixeles de scroll.')
    L.append(';')
    L.append('; Regla de diseno (la comprueba map2bin --check): como mucho 3 enemigos de 16x16 cortan una misma linea de la zona.')
    L.append('')
    L.append('include tiles.def')
    L.append('include paths.def')
    L.append('')
    L.append('name NIVEL1')
    L.append('scroll 1')
    L.append('')
    for text, comment in WAVES:
        if comment is None:
            L.append('')
            L.append(text)
        else:
            L.append(('wave ' + text) + (('    ; ' + comment) if comment else ''))
    L.append('')
    L.append('map')
    for i, row in enumerate(reversed(rows)):
        r = ROWS - 1 - i
        L.append('  ' + ' '.join('%-7s' % t for t in row).rstrip() + '    ; fila %d' % r)
    L.append('end')
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    open(a.out, 'w', encoding='utf-8', newline='\n').write('\n'.join(L) + '\n')
    print('%s: %d filas de mapa, %d oleadas' % (a.out, ROWS, sum(1 for _, c in WAVES if c is not None)))


if __name__ == '__main__':
    main()
