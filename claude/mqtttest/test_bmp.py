#!/usr/bin/env python3
"""Pruebas de la conversion de BMP a .I16 (Arduino/SD81BoosterV2_039_STM32/BMP.cpp) en el PC.

Genera BMP de todos los tipos que admite (1, 4, 8, 24 y 32 bits, de abajo arriba y de arriba abajo, cabeceras CORE / INFO / V5,
BI_BITFIELDS), los convierte con la referencia en Python (EXAMPLES/PHOTO/bmp2i16.py) y con el mismo codigo del STM32 compilado
para el PC (test_bmp.exe), y comprueba:
  - que las dos salidas son IGUALES byte a byte (o dan el mismo error);
  - propiedades que no dependen del algoritmo: el mismo dibujo de arriba abajo y de abajo arriba da lo mismo, igual en 24 y 32 bits y
    con paleta, y en una imagen 4:3 los tres modos de colocacion (estirar / conservar / recortar) coinciden;
  - la calidad: error medio entre la imagen escalada y su paleta, y las bandas negras del modo "conservar";
  - que el STM32 lee el fichero siempre hacia delante (salvo el principio de cada pasada).
Uso:  python test_bmp.py [carpeta_de_trabajo] [ruta de test_bmp.exe]     (lo llama build_bmp.sh)
"""
import math
import os
import random
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'EXAMPLES', 'PHOTO'))
import bmp2i16

WORK = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'tmp_bmp'))
EXE = os.path.abspath(sys.argv[2] if len(sys.argv) > 2 else os.path.join(WORK, 'test_bmp.exe'))


def clamp(v):
    return 0 if v < 0 else (255 if v > 255 else int(v))


def photo(x, y, w, h):
    """Una imagen sintetica con degradados, un disco, una barra oscura y algo de ruido determinista."""
    u, v = x / float(w), y / float(h)
    r = 128 + 100 * math.sin(6.0 * u + 2.0 * v)
    g = 128 + 90 * math.sin(4.0 * v - 3.0 * u + 1.0)
    b = 100 + 120 * u * (1 - v)
    if (u - 0.65) ** 2 + (v - 0.4) ** 2 < 0.03:
        r, g, b = 250, 220, 40
    if 0.1 < v < 0.18:
        r, g, b = 20, 20, 60
    n = ((x * 7919 + y * 104729) % 23) - 11
    return clamp(r + n), clamp(g + n), clamp(b - n)


def pixels(w, h, fn=photo):
    return [[fn(x, y, w, h) for x in range(w)] for y in range(h)]


def pack_rows(px, bpp, pal=None):
    """Filas de pixeles (r, g, b) -> bytes de cada fila, sin relleno, para bpp 24 / 32 o con indices de paleta."""
    rows = []
    for row in px:
        if bpp == 24:
            rows.append(b''.join(bytes((b, g, r)) for r, g, b in row))
        elif bpp == 32:
            rows.append(b''.join(bytes((b, g, r, 0)) for r, g, b in row))
        else:
            idx = [pal.index(p) for p in row]
            if bpp == 8:
                rows.append(bytes(idx))
            elif bpp == 4:
                if len(idx) % 2:
                    idx.append(0)
                rows.append(bytes((idx[i] << 4) | idx[i + 1] for i in range(0, len(idx), 2)))
            else:
                bits = ''.join('1' if i else '0' for i in idx)
                bits += '0' * (-len(bits) % 8)
                rows.append(bytes(int(bits[i:i + 8], 2) for i in range(0, len(bits), 8)))
    return rows


def write_bmp(path, w, h, bpp, rows, bottom_up=True, pal=None, dib=40, comp=0, clr_used=0, masks=False, drop_last_pad=False,
              magic=b'BM', truncate=0):
    stride = ((w * bpp + 31) // 32) * 4
    ordered = list(reversed(rows)) if bottom_up else list(rows)
    body = b''.join(r.ljust(stride, b'\0') for r in ordered)
    if drop_last_pad:
        body = body[:len(body) - (stride - len(rows[0]))]
    entsz = 3 if dib == 12 else 4
    palb = b''
    if bpp <= 8:
        n = clr_used if clr_used else (1 << bpp)
        pl = list(pal) + [(0, 0, 0)] * (n - len(pal))
        palb = b''.join(bytes((b, g, r, 0)[:entsz]) for r, g, b in pl[:n])
    if dib == 12:
        head = struct.pack('<IHHHH', 12, w, h, 1, bpp)
    else:
        head = struct.pack('<IiiHHIIiiII', 40, w, h if bottom_up else -h, 1, bpp, comp, len(body), 2835, 2835, clr_used, 0)
        extra = b''
        if masks or comp == 3:
            extra = struct.pack('<III', 0x00FF0000, 0x0000FF00, 0x000000FF)
        if dib > 40:
            head = struct.pack('<I', dib) + head[4:]
            head += extra.ljust(dib - 40, b'\0')
            extra = b''
        head += extra
    off = 14 + len(head) + len(palb)
    data = magic + struct.pack('<IHHI', off + len(body), 0, 0, off) + head + palb + body
    if truncate:
        data = data[:len(data) - truncate]
    open(path, 'wb').write(data)
    return data


def make_palette(n, seed):
    rnd = random.Random(seed)
    return [(rnd.randrange(256), rnd.randrange(256), rnd.randrange(256)) for _ in range(n)]


def blocks(pal):
    n = len(pal)

    def fn(x, y, w, h):
        return pal[((x // 7) + (y // 5) * 3 + (x * y) // 211) % n]
    return fn


# ---------------------------------------------------------------------------
results = []


def oracle_scale(px, g):
    """El escalado hecho directamente (pesos de solape de cada pixel de origen con cada pixel de destino), sin los acumuladores del
    codigo: la referencia independiente del escalado. px = filas de (r, g, b) de la imagen COMPLETA, de arriba abajo."""
    sw, sh, dw, dh = g['sw'], g['sh'], g['dw'], g['dh']

    def wgt(i, d, nd, ns):                 # solape de la casilla i de origen con la d de destino (unidades: casilla de origen = nd, de destino = ns)
        return max(0, min(nd * (i + 1), (d + 1) * ns) - max(nd * i, d * ns))
    total = sw * sh
    out = []
    for dy in range(dh):
        row = []
        for dx in range(dw):
            acc = [0, 0, 0]
            for yy in range(sh):
                wy = wgt(yy, dy, dh, sh)
                if not wy:
                    continue
                for xx in range(sw):
                    wx = wgt(xx, dx, dw, sw)
                    if not wx:
                        continue
                    p = px[g['sy0'] + yy][g['sx0'] + xx]
                    for c in range(3):
                        acc[c] += wx * wy * p[c]
            row.append(tuple((a + total // 2) // total for a in acc))
        out.append(row)
    return out


def scaler_oracle_tests(P):
    for (w, h, fit, bu) in ((37, 23, 0, True), (37, 23, 0, False), (23, 37, 1, True), (50, 20, 1, False), (50, 20, 2, True),
                            (20, 50, 2, False), (160, 100, 0, True), (13, 9, 0, True), (200, 130, 2, False)):
        px = pixels(w, h)
        path = P('o.bmp')
        write_bmp(path, w, h, 24, pack_rows(px, 24), bu)
        data = open(path, 'rb').read()
        bi = bmp2i16.parse(data)
        g = bmp2i16.geometry(bi['W'], bi['H'], fit)
        got = {}
        bmp2i16.scale_pass(data, bi, g, lambda dy, rgb: got.__setitem__(dy, [tuple(rgb[i * 3:i * 3 + 3]) for i in range(g['dw'])]))
        want = oracle_scale(px, g)
        check('escalado %dx%d fit=%d %s: igual que el calculo directo' % (w, h, fit, 'abajo-arriba' if bu else 'arriba-abajo'),
              [got[i] for i in range(g['dh'])] == want)


def run_c(path, fit, outp):
    r = subprocess.run([EXE, path, outp, str(fit)], capture_output=True, text=True)
    line = r.stdout.strip()
    if r.returncode not in (0, 1) or not line:
        return ('CRASH', line + r.stderr)
    if line.startswith('ERR'):
        return ('ERR', line[4:])
    f = line.split()
    return ('OK', open(outp, 'rb').read(), int(f[1]), int(f[2]), int(f[3]))


PYCACHE = os.environ.get('BMP_PYCACHE')       # para repetir las pruebas (mutaciones) sin volver a convertir en Python


def run_py(path, fit):
    data = open(path, 'rb').read()
    key = None
    if PYCACHE:
        import hashlib
        key = os.path.join(PYCACHE, hashlib.md5(data).hexdigest() + '_%d' % fit)
        if os.path.exists(key + '.i16'):
            bmp2i16.convert.info = eval(open(key + '.info').read())
            return ('OK', open(key + '.i16', 'rb').read())
        if os.path.exists(key + '.err'):
            return ('ERR', open(key + '.err').read())
    try:
        out = ('OK', bmp2i16.convert(data, fit))
    except bmp2i16.BmpError as e:
        out = ('ERR', str(e))
    if key:
        os.makedirs(PYCACHE, exist_ok=True)
        if out[0] == 'OK':
            open(key + '.i16', 'wb').write(out[1])
            open(key + '.info', 'w').write(repr(bmp2i16.convert.info))
        else:
            open(key + '.err', 'w').write(out[1])
    return out


def check(name, ok, detail=''):
    results.append((name, ok, detail))
    print(('  ok    ' if ok else '  FALLO ') + name + ((' -- ' + detail) if detail and not ok else ''))


outputs = {}       # (caso, fit) -> bytes del .I16


def case(name, path, fit, size_note=''):
    py = run_py(path, fit)
    c = run_c(path, fit, os.path.join(WORK, 'c.i16'))
    if py[0] == 'ERR':
        check('%s fit=%d: el mismo error' % (name, fit), c[0] == 'ERR' and c[1] == py[1], 'py=%s c=%s' % (py[1], c[1:]))
        return None
    if c[0] != 'OK':
        check('%s fit=%d: el STM32 convierte' % (name, fit), False, str(c))
        return None
    same = py[1] == c[1]
    detail = ''
    if not same:
        diff = [i for i in range(len(py[1])) if py[1][i] != c[1][i]]
        detail = '%d bytes distintos, el primero en %d' % (len(diff), diff[0])
    check('%s fit=%d: igual que Python%s' % (name, fit, size_note), same, detail)
    check('%s fit=%d: lectura hacia delante (%d seeks atras)' % (name, fit, c[3]), c[3] <= 4)
    outputs[(name, fit)] = c[1]
    info = dict(bmp2i16.convert.info)
    return c[1], info, c


def main():
    os.makedirs(WORK, exist_ok=True)
    P = lambda n: os.path.join(WORK, n)

    scaler_oracle_tests(P)

    # --- una imagen 4:3 de 24 bits, de abajo arriba y de arriba abajo, en 24 y 32 bits, y con paleta ---
    px = pixels(320, 240)
    rows24 = pack_rows(px, 24)
    write_bmp(P('a24bu.bmp'), 320, 240, 24, rows24, True)
    write_bmp(P('a24td.bmp'), 320, 240, 24, rows24, False)
    write_bmp(P('a32.bmp'), 320, 240, 32, pack_rows(px, 32), True)
    write_bmp(P('a32bf.bmp'), 320, 240, 32, pack_rows(px, 32), False, comp=3)
    write_bmp(P('a24v5.bmp'), 320, 240, 24, rows24, True, dib=124)
    write_bmp(P('a24core.bmp'), 320, 240, 24, rows24, True, dib=12)
    for fit in (0, 1, 2):
        r = case('24 bits abajo-arriba 320x240', P('a24bu.bmp'), fit)
        if fit == 0:
            base = r
            check('calidad: error medio de la paleta bajo (%.2f)' % r[1]['mae'], r[1]['mae'] < 8.0)
            check('paleta con 256 colores usados', r[1]['npal'] == 256, str(r[1]['npal']))
        for other, label in (('a24td.bmp', '24 bits arriba-abajo'), ('a32.bmp', '32 bits'), ('a32bf.bmp', '32 bits BITFIELDS arriba-abajo'),
                             ('a24v5.bmp', '24 bits cabecera V5'), ('a24core.bmp', '24 bits cabecera CORE')):
            r2 = case(label + ' 320x240', P(other), fit)
            if r2:
                check('%s da lo mismo que 24 bits abajo-arriba (fit=%d)' % (label, fit),
                      r2[0] == outputs.get(('24 bits abajo-arriba 320x240', fit)))
    # 320x240 es 4:3: los tres modos de colocacion coinciden
    o3 = [outputs.get(('24 bits abajo-arriba 320x240', f)) for f in (0, 1, 2)]
    check('en 4:3, estirar = conservar = recortar', None not in o3 and o3[0] == o3[1] == o3[2])

    # --- no 4:3, con relleno de fila (ancho no multiplo de 4) y tamanos raros ---
    for (w, h) in ((333, 251), (251, 333), (97, 61), (800, 100), (100, 800)):
        px = pixels(w, h)
        rows = pack_rows(px, 24)
        write_bmp(P('b%dx%d_bu.bmp' % (w, h)), w, h, 24, rows, True)
        write_bmp(P('b%dx%d_td.bmp' % (w, h)), w, h, 24, rows, False, drop_last_pad=True)
        for fit in (0, 1, 2):
            r1 = case('24 bits %dx%d' % (w, h), P('b%dx%d_bu.bmp' % (w, h)), fit)
            r2 = case('24 bits %dx%d arriba-abajo sin relleno final' % (w, h), P('b%dx%d_td.bmp' % (w, h)), fit)
            if r1 and r2:
                check('%dx%d fit=%d: arriba-abajo = abajo-arriba' % (w, h, fit), r1[0] == r2[0])
            if r1 and fit == 1:
                g = r1[1]['geo']
                black = r1[1]['black']
                out = r1[0]
                outside = [out[516 + y * 160 + x] for y in range(100) for x in range(160)
                           if not (g['dx0'] <= x < g['dx0'] + g['dw'] and g['dy0'] <= y < g['dy0'] + g['dh'])]
                check('%dx%d fit=1: bandas negras (%d pixeles)' % (w, h, len(outside)),
                      all(v == black for v in outside) and r1[1]['pal'][black] == (0, 0, 0))

    # --- imagenes con paleta: 8, 4 y 1 bit; iguales que su version de 24 bits ---
    for bpp, ncol, (w, h) in ((8, 200, (160, 100)), (8, 256, (400, 300)), (4, 16, (97, 61)), (1, 2, (321, 200))):
        pal = make_palette(ncol, bpp * 100 + ncol)
        if bpp == 1:
            pal = [(10, 20, 30), (240, 230, 40)]
        px = pixels(w, h, blocks(pal))
        write_bmp(P('p%d_%d.bmp' % (bpp, ncol)), w, h, bpp, pack_rows(px, bpp, pal), True, pal=pal)
        write_bmp(P('p%d_%d_td.bmp' % (bpp, ncol)), w, h, bpp, pack_rows(px, bpp, pal), False, pal=pal, clr_used=ncol if ncol < (1 << bpp) else 0)
        write_bmp(P('p%d_%d_24.bmp' % (bpp, ncol)), w, h, 24, pack_rows(px, 24), True)
        for fit in (0, 1, 2):
            r1 = case('%d bits %d colores %dx%d' % (bpp, ncol, w, h), P('p%d_%d.bmp' % (bpp, ncol)), fit)
            r2 = case('%d bits %d colores arriba-abajo' % (bpp, ncol), P('p%d_%d_td.bmp' % (bpp, ncol)), fit)
            r3 = case('%d bits %d colores en 24 bits' % (bpp, ncol), P('p%d_%d_24.bmp' % (bpp, ncol)), fit)
            if r1 and r2 and r3:
                check('%d bits %d colores fit=%d: paleta = arriba-abajo = 24 bits' % (bpp, ncol, fit), r1[0] == r2[0] == r3[0])
            if r1 and fit == 0 and (w, h) == (160, 100):
                check('160x100 con %d colores: error medio pequeno (%.2f)' % (ncol, r1[1]['mae']), r1[1]['mae'] < 9.0)
    # cabecera CORE con paleta de 3 bytes
    pal = make_palette(16, 77)
    px = pixels(60, 50, blocks(pal))
    write_bmp(P('core4.bmp'), 60, 50, 4, pack_rows(px, 4, pal), True, pal=pal, dib=12)
    write_bmp(P('core4_24.bmp'), 60, 50, 24, pack_rows(px, 24), True)
    r1 = case('4 bits cabecera CORE', P('core4.bmp'), 0)
    r2 = case('4 bits CORE en 24 bits', P('core4_24.bmp'), 0)
    if r1 and r2:
        check('CORE de 4 bits = 24 bits', r1[0] == r2[0])

    # --- tamanos extremos ---
    for (w, h) in ((1, 1), (3, 2), (160, 100), (161, 101), (40, 30), (2, 1000), (1000, 2), (160, 1)):
        px = pixels(w, h)
        write_bmp(P('e%dx%d.bmp' % (w, h)), w, h, 24, pack_rows(px, 24), True)
        for fit in (0, 1, 2):
            case('24 bits %dx%d' % (w, h), P('e%dx%d.bmp' % (w, h)), fit)
    # 1024x768 (el Python tarda unos segundos): imagen mas realista y mas grande que los buferes de lectura
    px = pixels(1024, 768)
    write_bmp(P('big.bmp'), 1024, 768, 24, pack_rows(px, 24), True)
    for fit in (0, 1):
        r = case('24 bits 1024x768', P('big.bmp'), fit)
        if r:
            check('1024x768 fit=%d: error medio bajo (%.2f)' % (fit, r[1]['mae']), r[1]['mae'] < 8.0)
            check('1024x768 fit=%d: lee el fichero dos veces (%d bytes de %d)' % (fit, r[2][2], os.path.getsize(P('big.bmp'))),
                  r[2][2] <= 2 * os.path.getsize(P('big.bmp')) + 1000)
    # recorte: solo se leen las filas necesarias
    px = pixels(200, 600)
    write_bmp(P('tall.bmp'), 200, 600, 24, pack_rows(px, 24), True)
    r = case('24 bits 200x600 recorte', P('tall.bmp'), 2)
    if r:
        check('recorte vertical: lee solo las filas que usa (%d bytes de %d)' % (r[2][2], os.path.getsize(P('tall.bmp'))),
              r[2][2] < os.path.getsize(P('tall.bmp')))

    # --- errores ---
    good = pixels(8, 8)
    rows = pack_rows(good, 24)
    write_bmp(P('x_magic.bmp'), 8, 8, 24, rows, magic=b'BX')
    write_bmp(P('x_rle8.bmp'), 8, 8, 8, pack_rows(pixels(8, 8, blocks(make_palette(4, 1))), 8, make_palette(4, 1)), True,
              pal=make_palette(4, 1), comp=1)
    write_bmp(P('x_trunc.bmp'), 8, 8, 24, rows, truncate=40)
    write_bmp(P('x_w0.bmp'), 8, 8, 24, rows)
    d = bytearray(open(P('x_w0.bmp'), 'rb').read())
    d[18:22] = struct.pack('<i', 0)
    open(P('x_w0.bmp'), 'wb').write(d)
    d = bytearray(open(P('x_w0.bmp'), 'rb').read())
    d[18:22] = struct.pack('<i', 20000)
    open(P('x_big.bmp'), 'wb').write(d)
    d = bytearray(open(P('x_w0.bmp'), 'rb').read())
    d[18:22] = struct.pack('<i', 8)
    d[28:30] = struct.pack('<H', 16)
    open(P('x_16bit.bmp'), 'wb').write(d)
    d[28:30] = struct.pack('<H', 24)
    d[30:34] = struct.pack('<I', 3)
    open(P('x_bitf24.bmp'), 'wb').write(d)
    open(P('x_tiny.bmp'), 'wb').write(b'BM\x00\x00')
    open(P('x_empty.bmp'), 'wb').write(b'')
    write_bmp(P('x_masks.bmp'), 8, 8, 32, pack_rows(good, 32), True, comp=3)
    d = bytearray(open(P('x_masks.bmp'), 'rb').read())
    d[54:58] = struct.pack('<I', 0x0000F800)          # mascaras de 565 en un BMP de 32 bits
    open(P('x_masks.bmp'), 'wb').write(d)
    for n in ('x_magic', 'x_rle8', 'x_trunc', 'x_w0', 'x_big', 'x_16bit', 'x_bitf24', 'x_tiny', 'x_empty', 'x_masks'):
        py = run_py(P(n + '.bmp'), 0)
        c = run_c(P(n + '.bmp'), 0, P('c.i16'))
        check('%s: error "%s" igual en el STM32' % (n, py[1] if py[0] == 'ERR' else '(no daba error)'),
              py[0] == 'ERR' and c[0] == 'ERR' and c[1] == py[1], 'py=%s c=%s' % (py, c[:2]))

    bad = [r for r in results if not r[1]]
    print('-----')
    print('%d comprobaciones, %d fallos' % (len(results), len(bad)))
    print('TODO OK' if not bad else 'FALLOS')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
