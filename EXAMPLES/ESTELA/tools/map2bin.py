#!/usr/bin/env python3
"""map2bin.py -- nivel de ESTELA (texto legible) -> NIVELn.MAP (mapa + oleadas) y LEVEL.INC (formatos y trayectorias para pasmo).

Entrada: un fichero de nivel (.LVL) que puede incluir (include) tiles.def (lo escribe gentiles.py) y paths.def.
Lineas (todo lo que sigue a ; o # es un comentario):

    include fichero              incluye otro fichero (ruta relativa al que lo incluye)
    name NIVEL1                  nombre del fichero de salida (NIVEL1.MAP; sin "_")
    scroll 1                     ticks de juego por pixel de scroll: 1 = un pixel por tick, 2 = un pixel cada dos ticks
    tile NOMBRE n                (tiles.def) nombre de un tile del tileset, 0-59
    range NOMBRE a b             (tiles.def) rango de la paleta de 256; panelrow N  (tiles.def) fila de los paneles
    path NOMBRE                  (paths.def) empieza una trayectoria; sus tramos son las lineas siguientes:
        line N VX VY               N ticks con velocidad (VX, VY) en pixeles por tick (admite decimales)
        sine N AMP CICLOS VY       N ticks: x oscila AMP*sin(2*pi*CICLOS*t/N) mientras y avanza VY por tick
    wave pos=.. type=.. x=.. path=.. count=.. spacing=.. [flags=a,b]
                                 una oleada (ver el README / juego160.md 8):
        pos      posicion del scroll (pixeles desde el inicio) en que sale; "r12" = cuando la fila de tiles 12 esta a punto de
                 entrar por arriba (16*12 - 100); "r12+8" suma pixeles
        type     E1-E6 (enemigos 16x16), BOSS (32x24), ITEM1-ITEM3 (mejoras 8x8) o un numero 0-15
        x        pixel X de pantalla del lado izquierdo del primer enemigo (la zona de juego va de 16 a 143)
        path     trayectoria (nombre o numero; 0 = STILL, quieto)
        count    cuantos enemigos (1-255);  spacing  separacion
        flags    mirror (dx de la trayectoria con el signo cambiado), row (los count salen a la vez, separados spacing pixeles
                 en X; sin row salen uno cada spacing ticks), ground (el enemigo va pegado al suelo: ademas baja con el scroll)
    map ... end                  el mapa: una fila por linea, 8 tiles (nombre o numero 0-59) separados por espacios.
                                 La linea de arriba es el FINAL del nivel y la de abajo el principio.

Salida (en --out): NIVELn.MAP y LEVEL.INC. Con --check, comprobacion de ida y vuelta (se decodifica lo escrito y se compara con
la entrada) y simulacion de la regla de diseno: como mucho 3 enemigos de 16x16 (peso 3) cortan una misma linea de la zona.

Uso:  python map2bin.py ../assets/NIVEL1.LVL [-o ../out] [--check]
"""
import argparse
import math
import os
import re
import struct
import sys

MAGIC = b'ELV1'
HDR = 16
WSIZE = 8
TILES_N = 60
MAP_W = 8
TILESET_BASE = 0xC000
TILESET_STRIDE = 160          # bytes de una fila de pixeles del tileset (160x100 a 8 bpp)
TILE_PX = 16

# nombre -> (numero, alto en filas, peso para la regla de 12 huecos por linea)
TYPES = {'E1': (0, 16, 1.0), 'E2': (1, 16, 1.0), 'E3': (2, 16, 1.0), 'E4': (3, 16, 1.0), 'E5': (4, 16, 1.0), 'E6': (5, 16, 1.0),
         'BOSS': (6, 24, 2.0), 'ITEM1': (8, 8, 0.5), 'ITEM2': (9, 8, 0.5), 'ITEM3': (10, 8, 0.5)}
TYPE_BY_NUM = {v[0]: (k, v[1], v[2]) for k, v in TYPES.items()}
FLAGS = {'MIRROR': 1, 'ROW': 2, 'GROUND': 4}
F_MIRROR, F_ROW, F_GROUND = 1, 2, 4
ZONE_ROWS = 100
LEAVE_TICKS = 3000


class LevelError(Exception):
    pass


# ---------------------------------------------------------------------------------------------------------
# Analisis del texto
# ---------------------------------------------------------------------------------------------------------
class Level:
    def __init__(self):
        self.name = None
        self.scroll = 1
        self.tiles = {}            # nombre -> indice
        self.ranges = []           # (nombre, a, b)
        self.panelrow = None
        self.paths = [('STILL', [(0, 0)])]     # la trayectoria 0 siempre es STILL
        self.waves = []            # dicts
        self.map = []              # filas en el orden del texto (la primera es el final del nivel)


def _strip(line):
    return re.split(r'[;#]', line, maxsplit=1)[0].strip()


def _num(tok, what, lo=None, hi=None, ln=0, kind=int):
    try:
        v = kind(tok)
    except ValueError:
        raise LevelError('linea %d: %s no es un numero: %s' % (ln, what, tok))
    if lo is not None and not lo <= v <= hi:
        raise LevelError('linea %d: %s = %s fuera de %s..%s' % (ln, what, tok, lo, hi))
    return v


def parse_level(path, lv=None, _stack=()):
    """Analiza un fichero (y sus include) y devuelve el Level."""
    if lv is None:
        lv = Level()
    path = os.path.abspath(path)
    if path in _stack:
        raise LevelError('include circular: %s' % path)
    try:
        lines = open(path, encoding='utf-8').read().splitlines()
    except OSError as e:
        raise LevelError(str(e))
    cur = None                 # trayectoria en curso: lista de (dx, dy) y estado de coma flotante
    in_map = False
    for ln, raw in enumerate(lines, 1):
        line = _strip(raw)
        if not line:
            continue
        f = line.split()
        key = f[0].lower()
        if in_map:
            if key == 'end':
                in_map = False
                continue
            row = []
            for tok in f:
                t = tok.upper()
                if t in lv.tiles:
                    row.append(lv.tiles[t])
                elif re.fullmatch(r'\d+', tok):
                    row.append(_num(tok, 'tile', 0, TILES_N - 1, ln))
                else:
                    raise LevelError('%s linea %d: tile desconocido: %s' % (os.path.basename(path), ln, tok))
            if len(row) != MAP_W:
                raise LevelError('%s linea %d: la fila tiene %d tiles y deben ser %d' % (os.path.basename(path), ln, len(row), MAP_W))
            lv.map.append(row)
            continue
        if key == 'include':
            parse_level(os.path.join(os.path.dirname(path), f[1]), lv, _stack + (path,))
        elif key == 'name':
            lv.name = f[1].upper()
            if '_' in lv.name:
                raise LevelError('linea %d: el nombre no puede llevar "_" (el ZX81 no lo tiene)' % ln)
        elif key == 'scroll':
            lv.scroll = _num(f[1], 'scroll', 1, 8, ln)
        elif key == 'tile':
            lv.tiles[f[1].upper()] = _num(f[2], 'indice de tile', 0, TILES_N - 1, ln)
        elif key == 'range':
            lv.ranges.append((f[1].upper(), _num(f[2], 'rango', 0, 255, ln), _num(f[3], 'rango', 0, 255, ln)))
        elif key == 'panelrow':
            lv.panelrow = _num(f[1], 'panelrow', 0, 99, ln)
        elif key == 'path':
            if not re.fullmatch(r'[A-Za-z0-9]+', f[1]) or f[1].upper() == 'COUNT':
                raise LevelError('linea %d: nombre de trayectoria no valido (solo letras y cifras; COUNT esta reservado): %s' % (ln, f[1]))
            cur = {'name': f[1].upper(), 'steps': [], 'x': 0.0, 'y': 0.0, 'ix': 0, 'iy': 0}
            if cur['name'] in [p[0] for p in lv.paths]:
                raise LevelError('linea %d: trayectoria repetida: %s' % (ln, f[1]))
            lv.paths.append((cur['name'], cur['steps']))
        elif key in ('line', 'sine'):
            if cur is None:
                raise LevelError('linea %d: "%s" fuera de una trayectoria' % (ln, key))
            n = _num(f[1], 'ticks', 1, 255, ln)
            if key == 'line':
                vx, vy = _num(f[2], 'VX', kind=float, ln=ln), _num(f[3], 'VY', kind=float, ln=ln)
                x0, y0 = cur['x'], cur['y']
                pos = lambda t: (x0 + vx * t, y0 + vy * t)    # noqa: E731
            else:
                amp, cyc, vy = _num(f[2], 'AMP', kind=float, ln=ln), _num(f[3], 'CICLOS', kind=float, ln=ln), _num(f[4], 'VY', kind=float, ln=ln)
                x0, y0 = cur['x'], cur['y']
                pos = lambda t: (x0 + amp * math.sin(2 * math.pi * cyc * t / n), y0 + vy * t)  # noqa: E731
            for t in range(1, n + 1):
                px, py = pos(t)
                nx, ny = int(round(px)), int(round(py))
                dx, dy = nx - cur['ix'], ny - cur['iy']
                if not (-128 <= dx <= 127 and -128 <= dy <= 127):
                    raise LevelError('linea %d: velocidad demasiado alta (paso %d,%d)' % (ln, dx, dy))
                cur['steps'].append((dx, dy))
                cur['ix'], cur['iy'] = nx, ny
            cur['x'], cur['y'] = pos(n)
            if len(cur['steps']) > 255:
                raise LevelError('linea %d: la trayectoria %s pasa de 255 pasos' % (ln, cur['name']))
        elif key == 'wave':
            lv.waves.append(parse_wave(f[1:], lv, ln))
        elif key == 'map':
            in_map = True
        else:
            raise LevelError('%s linea %d: no entiendo "%s"' % (os.path.basename(path), ln, f[0]))
    if in_map:
        raise LevelError('%s: falta "end" al final del mapa' % os.path.basename(path))
    return lv


def parse_wave(toks, lv, ln):
    kv = {}
    for t in toks:
        if '=' not in t:
            raise LevelError('linea %d: se esperaba clave=valor: %s' % (ln, t))
        k, v = t.split('=', 1)
        kv[k.lower()] = v
    for need in ('pos', 'type', 'x', 'path', 'count'):
        if need not in kv:
            raise LevelError('linea %d: falta %s=' % (ln, need))
    m = re.fullmatch(r'r(\d+)(?:\+(\d+))?', kv['pos'], re.I)
    if m:
        pos = 16 * int(m.group(1)) - 100 + int(m.group(2) or 0)
        if pos < 0:
            raise LevelError('linea %d: la fila %s ya esta en pantalla al empezar (usa pos en pixeles)' % (ln, m.group(1)))
    else:
        pos = _num(kv['pos'], 'pos', 0, 65534, ln)
    tv = kv['type'].upper()
    if tv in TYPES:
        typ = TYPES[tv][0]
    else:
        typ = _num(tv, 'type', 0, 15, ln)
    pv = kv['path'].upper()
    pnames = [p[0] for p in lv.paths]
    if pv in pnames:
        path = pnames.index(pv)
    elif re.fullmatch(r'\d+', pv) and int(pv) < len(lv.paths):
        path = int(pv)
    else:
        raise LevelError('linea %d: trayectoria desconocida: %s' % (ln, kv['path']))
    flags = 0
    for fl in filter(None, kv.get('flags', '').upper().split(',')):
        if fl not in FLAGS:
            raise LevelError('linea %d: flag desconocido: %s' % (ln, fl))
        flags |= FLAGS[fl]
    return {'pos': pos, 'type': typ, 'x': _num(kv['x'], 'x', 0, 255, ln), 'path': path,
            'count': _num(kv['count'], 'count', 1, 255, ln), 'spacing': _num(kv.get('spacing', '0'), 'spacing', 0, 255, ln),
            'flags': flags, 'line': ln}


def validate(lv):
    if not lv.name:
        raise LevelError('falta "name"')
    if len(lv.map) < 7:
        raise LevelError('el mapa tiene %d filas y debe tener al menos 7 (la pantalla enseña 6,25)' % len(lv.map))
    if len(lv.map) * 16 - 100 > 65534:
        raise LevelError('el mapa es demasiado largo')
    if len(lv.waves) > 255:
        raise LevelError('mas de 255 oleadas')
    if len(lv.paths) > 64:
        raise LevelError('mas de 64 trayectorias')
    for w in lv.waves:
        if w['flags'] & F_ROW and w['x'] + (w['count'] - 1) * w['spacing'] > 255:
            raise LevelError('linea %d: la fila de enemigos se sale de X = 255' % w['line'])


# ---------------------------------------------------------------------------------------------------------
# Binario
# ---------------------------------------------------------------------------------------------------------
def wave_key(w):
    return w['pos']


def build_binary(lv):
    """El fichero NIVELn.MAP: cabecera de 16 bytes, oleadas de 8 bytes (+ una final con POS = $FFFF) y filas de 8 tiles."""
    waves = sorted(lv.waves, key=wave_key)          # sorted es estable: a igual pos, el orden del texto
    rows = list(reversed(lv.map))                   # fila 0 = la primera en entrar (la de abajo del texto)
    woff = HDR
    moff = woff + WSIZE * (len(waves) + 1)
    length = moff + MAP_W * len(rows)
    if length > 0xFFFF:
        raise LevelError('el fichero pasa de 64 KB')
    out = bytearray(MAGIC)
    out += bytes([lv.scroll, len(waves)])
    out += struct.pack('<HHHHH', len(rows), woff, moff, length, 16 * len(rows) - 100)
    assert len(out) == HDR
    for w in waves:
        out += struct.pack('<HBBBBBB', w['pos'], w['type'], w['x'], w['path'], w['count'], w['spacing'], w['flags'])
    out += struct.pack('<HBBBBBB', 0xFFFF, 0, 0, 0, 0, 0, 0)
    for r in rows:
        out += bytes(r)
    assert len(out) == length
    return bytes(out)


def decode_binary(d):
    if d[:4] != MAGIC:
        raise LevelError('no es un fichero de nivel (cabecera %r)' % d[:4])
    scroll, nw = d[4], d[5]
    nrows, woff, moff, length, endpos = struct.unpack('<HHHHH', d[6:16])
    if length != len(d):
        raise LevelError('la longitud de la cabecera (%d) no es la del fichero (%d)' % (length, len(d)))
    waves = []
    for i in range(nw + 1):
        pos, typ, x, path, count, spacing, flags = struct.unpack('<HBBBBBB', d[woff + WSIZE * i:woff + WSIZE * i + WSIZE])
        if i == nw:
            if pos != 0xFFFF:
                raise LevelError('falta la oleada final')
            break
        waves.append({'pos': pos, 'type': typ, 'x': x, 'path': path, 'count': count, 'spacing': spacing, 'flags': flags})
    rows = [list(d[moff + MAP_W * i:moff + MAP_W * i + MAP_W]) for i in range(nrows)]
    return {'scroll': scroll, 'waves': waves, 'rows': rows, 'endpos': endpos, 'woff': woff, 'moff': moff}


# ---------------------------------------------------------------------------------------------------------
# LEVEL.INC
# ---------------------------------------------------------------------------------------------------------
def tile_addr(t):
    return TILESET_BASE + (t // 10) * TILE_PX * TILESET_STRIDE + (t % 10) * TILE_PX


def make_inc(lv):
    L = []
    A = L.append
    A('; LEVEL.INC -- generado por map2bin.py a partir de %s.LVL. No editar a mano.' % lv.name)
    A('; Formatos del fichero de nivel (NIVELn.MAP), de las trayectorias y del tileset. Ver claude/planning/juego160.md, seccion 8.')
    A('')
    A('; ---- cabecera del fichero de nivel (16 bytes, todo lo de 16 bits en little endian)')
    for nm, off, c in (('LVL_MAGIC', 0, '"ELV1" (4 bytes)'), ('LVL_SCROLL', 4, 'ticks de juego por pixel de scroll (1 o 2)'),
                       ('LVL_NWAVES', 5, 'numero de oleadas (0-255)'), ('LVL_ROWS', 6, 'filas del mapa (u16)'),
                       ('LVL_WAVES', 8, 'desplazamiento de la tabla de oleadas (u16)'), ('LVL_MAP', 10, 'desplazamiento del mapa (u16)'),
                       ('LVL_LEN', 12, 'longitud del fichero (u16)'),
                       ('LVL_END', 14, 'scroll final = 16*filas - 100 (u16): el nivel acaba al llegar ahi')):
        A('%s\tequ %d\t\t; %s' % (nm, off, c))
    A('LVL_HDR_SIZE\tequ %d' % HDR)
    A('')
    A('; ---- oleadas: registros de %d bytes ordenados por WAVE_POS; el ultimo lleva WAVE_POS = $FFFF' % WSIZE)
    for nm, off, c in (('WAVE_POS', 0, 'u16: posicion del scroll (pixeles) en que sale'), ('WAVE_TYPE', 2, 'tipo de enemigo (TYPE_*)'),
                       ('WAVE_X', 3, 'X de pantalla del lado izquierdo del primero (sprite X = WAVE_X + 32)'),
                       ('WAVE_PATH', 4, 'trayectoria (PATH_*)'), ('WAVE_COUNT', 5, 'cuantos enemigos'),
                       ('WAVE_SPACING', 6, 'separacion: ticks entre uno y el siguiente, o pixeles en X con WF_ROW'),
                       ('WAVE_FLAGS', 7, 'WF_*')):
        A('%s\tequ %d\t\t; %s' % (nm, off, c))
    A('WAVE_SIZE\tequ %d' % WSIZE)
    A('WF_MIRROR\tequ %d\t\t; dx de la trayectoria con el signo cambiado' % F_MIRROR)
    A('WF_ROW\tequ %d\t\t; los enemigos salen a la vez, en fila (X + i*SPACING)' % F_ROW)
    A('WF_GROUND\tequ %d\t\t; va pegado al suelo: ademas de la trayectoria baja con el scroll' % F_GROUND)
    A('')
    A('; ---- tipos de enemigo (WAVE_TYPE) y su sprite en HOJA.INC (ENEMY1... = tipo + 1)')
    for k, (n, h, wgt) in sorted(TYPES.items(), key=lambda kv: kv[1][0]):
        A('TYPE_%s\tequ %d' % (k, n))
    A('')
    A('; ---- trayectorias: PATH_* = numero; PATHTAB = tabla de punteros; cada una es [n pasos][dx dy]*n, con dx, dy de 8 bits')
    A('; con signo; el paso k se aplica en el tick k de la vida del enemigo; pasado el ultimo se repite el ultimo paso.')
    for i, (nm, steps) in enumerate(lv.paths):
        A('PATH_%s\tequ %d' % (nm, i))
    A('PATH_COUNT\tequ %d' % len(lv.paths))
    A('PATHTAB:')
    A('\tdefw ' + ','.join('PD_%s' % nm for nm, _ in lv.paths))
    for nm, steps in lv.paths:
        A('PD_%s:\tdefb %d\t\t; %d pasos' % (nm, len(steps), len(steps)))
        for k in range(0, len(steps), 8):
            A('\tdefb ' + ','.join('%d,%d' % s for s in steps[k:k + 8]))
    A('')
    A('; ---- tileset (los 16.000 pixeles de NIVELn.I16 en $C000)')
    A('TILESET_BASE\tequ $%04X' % TILESET_BASE)
    A('TILESET_STRIDE\tequ %d\t\t; bytes de una fila de pixeles' % TILESET_STRIDE)
    A('; direccion del tile t: TILESET_BASE + (t/10)*2560 + (t mod 10)*16; la fila y del tile esta y*160 bytes mas adelante')
    A('TILEADR:\t\t\t; 60 palabras, indexadas por el numero de tile')
    for t in range(TILES_N):
        A('\tdefw $%04X' % tile_addr(t))
    for nm, t in sorted(lv.tiles.items(), key=lambda kv: kv[1]):
        A('TILE_%s\tequ %d' % (nm, t))
    if lv.panelrow is not None:
        A('PANEL_ADDR\tequ $%04X\t\t; fila de los paneles: +0..+15 el izquierdo, +144..+159 el derecho' % (TILESET_BASE + lv.panelrow * TILESET_STRIDE))
    A('')
    A('; ---- rangos de la paleta de 256')
    for nm, a, b in lv.ranges:
        A('%s\tequ %d\t\t; %d-%d' % (nm, a, a, b))
        A('%s_N\tequ %d' % (nm, b - a + 1))
    A('')
    return '\n'.join(L)


# ---------------------------------------------------------------------------------------------------------
# Simulacion de la regla de diseno
# ---------------------------------------------------------------------------------------------------------
def parse_paths_from_inc(text):
    """Lee PATHTAB y los datos de las trayectorias de LEVEL.INC. Devuelve [(nombre, [(dx, dy)])] por orden de PATH_*."""
    ids = {m.group(1): int(m.group(2)) for m in re.finditer(r'^PATH_([A-Z0-9]+)\tequ (\d+)', text, re.M) if m.group(1) != 'COUNT'}
    out = {}
    for m in re.finditer(r'^PD_([A-Z0-9]+):\tdefb (\d+)[^\n]*\n((?:\tdefb [-\d,]+\n)*)', text + '\n', re.M):
        nums = [int(v) for line in m.group(3).splitlines() for v in line.split('defb')[1].split(',')]
        steps = [(nums[i], nums[i + 1]) for i in range(0, len(nums), 2)]
        if len(steps) != int(m.group(2)):
            raise LevelError('PD_%s: %d pasos y la cabecera dice %s' % (m.group(1), len(steps), m.group(2)))
        out[m.group(1)] = steps
    return [(nm, out[nm]) for nm, _ in sorted(ids.items(), key=lambda kv: kv[1])]


def simulate(waves, paths, scroll, max_weight=3.0):
    """Mueve cada enemigo tick a tick y mide, para cada linea de la zona (0-99), el peso de los enemigos que la cortan
    (16x16 = 1, jefe = 2, mejora = 0,5). Devuelve (peso maximo, [(tick, linea, peso)], [avisos], ticks simulados)."""
    cover = {}
    problems = []
    last = 0
    for wi, w in enumerate(waves):
        tname, th, weight = TYPE_BY_NUM.get(w['type'], ('T%d' % w['type'], 16, 1.0))
        steps = paths[w['path']][1]
        sign = -1 if w['flags'] & F_MIRROR else 1
        for i in range(w['count']):
            row = w['flags'] & F_ROW
            t0 = w['pos'] * scroll + (0 if row else i * w['spacing'])
            x = w['x'] + (i * w['spacing'] if row else 0)
            y = -th
            age = 0
            tick = t0
            gone = False
            while age < LEAVE_TICKS:
                y0, y1 = max(0, y), min(ZONE_ROWS - 1, y + th - 1)
                if y0 <= y1:
                    cover.setdefault(tick, []).append((y0, y1, weight))
                if tname == 'BOSS' and age >= 400:
                    gone = True
                    break
                dx, dy = steps[min(age, len(steps) - 1)]
                x += sign * dx
                y += dy
                if w['flags'] & F_GROUND and tick % scroll == 0:
                    y += 1
                tick += 1
                age += 1
                if y >= ZONE_ROWS or x < -40 or x > 200 or y < -th - 40:
                    gone = True
                    break
            last = max(last, tick)
            if not gone:
                problems.append('oleada %d (linea %s): un enemigo sigue en pantalla pasados %d ticks (la trayectoria no lo saca)' % (
                    wi, w.get('line', '?'), LEAVE_TICKS))
                break
    worst = 0.0
    bad = []
    for tick in sorted(cover):
        diff = [0.0] * (ZONE_ROWS + 1)
        for y0, y1, wt in cover[tick]:
            diff[y0] += wt
            diff[y1 + 1] -= wt
        acc = 0.0
        for r in range(ZONE_ROWS):
            acc += diff[r]
            if acc > worst:
                worst = acc
            if acc > max_weight + 1e-9:
                bad.append((tick, r, acc))
                break
    return worst, bad, problems, last


# ---------------------------------------------------------------------------------------------------------
def check(lv, data, inc, out_inc_text_paths, max_weight):
    """Ida y vuelta: decodifica los ficheros escritos, los compara con lo analizado y simula la regla con lo decodificado."""
    errs = []
    dec = decode_binary(data)
    waves = sorted(lv.waves, key=wave_key)
    if dec['scroll'] != lv.scroll:
        errs.append('scroll: %d en el binario y %d en el texto' % (dec['scroll'], lv.scroll))
    if len(dec['waves']) != len(waves):
        errs.append('%d oleadas en el binario y %d en el texto' % (len(dec['waves']), len(waves)))
    for i, (a, b) in enumerate(zip(dec['waves'], waves)):
        for k in ('pos', 'type', 'x', 'path', 'count', 'spacing', 'flags'):
            if a[k] != b[k]:
                errs.append('oleada %d: %s = %s en el binario y %s en el texto' % (i, k, a[k], b[k]))
    if dec['rows'] != list(reversed(lv.map)):
        errs.append('el mapa del binario no es el del texto invertido')
    if dec['endpos'] != 16 * len(lv.map) - 100:
        errs.append('scroll final incorrecto')
    ps = parse_paths_from_inc(inc)
    if [(n, s) for n, s in ps] != [(n, s) for n, s in lv.paths]:
        errs.append('las trayectorias del LEVEL.INC no son las del texto')
    for nm, steps in ps:        # la trayectoria decodificada sigue la funcion original dentro del redondeo de 1 pixel acumulado
        if not 1 <= len(steps) <= 255:
            errs.append('trayectoria %s con %d pasos' % (nm, len(steps)))
    adr = [int(v, 16) for v in re.findall(r'^\tdefw \$([0-9A-F]{4})$', inc, re.M)]
    if adr != [tile_addr(t) for t in range(TILES_N)]:
        errs.append('la tabla TILEADR no es la esperada')
    worst, bad, problems, last = simulate(dec['waves'], ps, dec['scroll'], max_weight)
    for p in problems:
        errs.append(p)
    for tick, r, wt in bad[:6]:
        errs.append('regla de diseno: peso %.1f > %.1f en la linea %d del tick %d (scroll %d px)' % (wt, max_weight, r, tick, tick // dec['scroll']))
    if len(bad) > 6:
        errs.append('... y %d instantes mas' % (len(bad) - 6))
    return errs, worst, last


def main():
    ap = argparse.ArgumentParser(description='nivel de ESTELA (.LVL) -> NIVELn.MAP + LEVEL.INC')
    ap.add_argument('lvl')
    ap.add_argument('-o', '--out', default='.')
    ap.add_argument('--max-weight', type=float, default=3.0, help='peso maximo por linea de la zona (por omision 3 = tres enemigos 16x16)')
    ap.add_argument('--check', action='store_true', help='ida y vuelta y simulacion de la regla de diseno (falla si se incumple)')
    a = ap.parse_args()
    try:
        lv = parse_level(a.lvl)
        validate(lv)
        data = build_binary(lv)
    except LevelError as e:
        print('map2bin: %s' % e, file=sys.stderr)
        return 1
    os.makedirs(a.out, exist_ok=True)
    inc = make_inc(lv)
    mapf = os.path.join(a.out, lv.name + '.MAP')
    open(mapf, 'wb').write(data)
    open(os.path.join(a.out, 'LEVEL.INC'), 'w', encoding='utf-8', newline='\n').write(inc)
    print('%s.MAP: %d bytes (%d filas de mapa = %d px, %d oleadas, %d trayectorias); LEVEL.INC' % (
        lv.name, len(data), len(lv.map), 16 * len(lv.map), len(lv.waves), len(lv.paths)))
    if a.check:
        try:
            errs, worst, last = check(lv, open(mapf, 'rb').read(), open(os.path.join(a.out, 'LEVEL.INC'), encoding='utf-8').read(), None, a.max_weight)
        except LevelError as e:
            errs, worst, last = [str(e)], 0, 0
        for e in errs:
            print('FALLO: ' + e, file=sys.stderr)
        if errs:
            return 2
        print('comprobacion de ida y vuelta: correcta; regla de diseno: peso maximo por linea %.1f (limite %.1f) en %d ticks simulados' % (
            worst, a.max_weight, last))
    return 0


if __name__ == '__main__':
    sys.exit(main())
