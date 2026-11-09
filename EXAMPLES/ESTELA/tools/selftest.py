#!/usr/bin/env python3
"""selftest.py -- pruebas de las herramientas de ESTELA, en una carpeta temporal (no toca assets/ ni out/).

Comprueba la ida y vuelta de las cuatro herramientas y que las comprobaciones saben fallar:
  - spr2cel: una celda de 8x8 conocida da los 32 bytes esperados (nibble alto = pixel izquierdo) y vuelve igual;
    un PNG con un color 16 o un .def con celdas fuera de la hoja se rechazan; un .BIN alterado hace fallar check().
  - gentiles / png2i16: el .I16 coincide con el PNG, y uno alterado falla.
  - map2bin: formatos de cabecera y oleadas con valores conocidos; una oleada con cuatro enemigos 16x16 a la vez rompe la regla;
    un enemigo que nunca sale se detecta; un binario alterado falla.

Uso:  python selftest.py        (sale con 0 si todo va bien)
"""
import os
import shutil
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gensprites  # noqa: E402
import gentemas    # noqa: E402
import genniveles  # noqa: E402
import gentiles    # noqa: E402
import gentitulo   # noqa: E402
import make        # noqa: E402
import map2bin     # noqa: E402
import pngio       # noqa: E402
import spr2cel     # noqa: E402

fails = []


def ok(cond, msg):
    print(('  ok   ' if cond else '  FALLO ') + msg)
    if not cond:
        fails.append(msg)


def test_spr2cel(tmp):
    print('spr2cel')
    # una celda con un patron conocido: fila r, pixel x = (r + x) % 16
    img = [[(r + x) % 16 for x in range(16)] for r in range(16)]
    pal = [(i * 16, 255 - i * 16, (i * 37) % 256) for i in range(16)]
    png = os.path.join(tmp, 't.png')
    pngio.write_indexed(png, 16, 16, img, pal)
    open(os.path.join(tmp, 't.def'), 'w').write('UNO 0 0 2 2 1\nDOS 0 0 1 1 2\n')
    sys.argv = ['spr2cel', png, '-o', tmp, '--name', 'T', '--check']
    ok(spr2cel.main() == 0, 'PNG de 16x16 con dos graficos: genera y la ida y vuelta es correcta')
    blob = open(os.path.join(tmp, 'T.BIN'), 'rb').read()
    ok(len(blob) == (4 + 2) * 32, '6 celdas = 192 bytes (%d)' % len(blob))
    ok(blob[0:4] == bytes([0x01, 0x23, 0x45, 0x67]), 'fila 0 de la primera celda: 0,1,2,... en nibbles (%s)' % blob[0:4].hex())
    ok(blob[4:8] == bytes([0x12, 0x34, 0x56, 0x78]), 'fila 1: 1,2,3,... (%s)' % blob[4:8].hex())
    ok(blob[32:36] == bytes([0x89, 0xAB, 0xCD, 0xEF]), 'segunda celda (arriba a la derecha): 8,9,A,... (%s)' % blob[32:36].hex())
    inc = open(os.path.join(tmp, 'T.INC')).read()
    ok('UNO\tequ 128' in inc and 'DOS\tequ 132' in inc and 'DOS_1\tequ 133' in inc and 'SHEET_ADDR\tequ $7000' in inc,
       'etiquetas: UNO = 128, DOS = 132, DOS_1 = 133, SHEET_ADDR = $7000')
    # alterar el .BIN debe hacer fallar la comprobacion
    bad = bytearray(blob)
    bad[5] ^= 0x10
    open(os.path.join(tmp, 'T.BIN'), 'wb').write(bytes(bad))
    errs = spr2cel.check(png, os.path.join(tmp, 't.def'), tmp, 'T', 128, 3, 1.0)
    ok(len(errs) > 0, 'un .BIN alterado hace fallar la comprobacion')
    # errores de entrada
    img17 = [[16] * 8 for _ in range(8)]
    pngio.write_indexed(os.path.join(tmp, 'c17.png'), 8, 8, img17, [(0, 0, 0)] * 17)
    open(os.path.join(tmp, 'c17.def'), 'w').write('X 0 0 1 1\n')
    sys.argv = ['spr2cel', os.path.join(tmp, 'c17.png'), '-o', tmp]
    ok(spr2cel.main() == 1, 'un PNG con el color 16 se rechaza')
    open(os.path.join(tmp, 't.def'), 'w').write('FUERA 1 1 2 2\n')
    sys.argv = ['spr2cel', png, '-o', tmp, '--name', 'T']
    ok(spr2cel.main() == 1, 'un grafico fuera de la hoja se rechaza')


def test_sprites_and_tiles(tmp):
    print('gensprites + spr2cel')
    d = os.path.join(tmp, 'a')
    sys.argv = ['gensprites', '-o', d, '--check']
    ok(gensprites.main() == 0, 'gensprites: ida y vuelta correcta')
    sys.argv = ['spr2cel', os.path.join(d, 'sprites.png'), '-o', d, '--check']
    ok(spr2cel.main() == 0, 'spr2cel sobre los sprites provisionales: correcto')
    inc = open(os.path.join(d, 'HOJA.INC')).read()
    ok('SHEET_CELLS\tequ 121' in inc and 'NAVE\tequ 128' in inc and 'CIFRA_9\tequ ' in inc, '121 celdas, NAVE = 128, CIFRA_9 definida')
    print('gentiles + png2i16')
    sys.argv = ['gentiles', '-o', d, '--check']
    ok(gentiles.main() == 0, 'gentiles: ida y vuelta correcta')
    i16 = os.path.join(d, 'NIVEL1.I16')
    make.make_i16(os.path.join(d, 'tileset.png'), i16)
    ok(gentiles.check_i16(i16, os.path.join(d, 'tileset.png')) == [], '.I16 = PNG (pixeles y paleta por pal160.solve)')
    raw = bytearray(open(i16, 'rb').read())
    raw[600] ^= 1
    open(i16, 'wb').write(bytes(raw))
    ok(len(gentiles.check_i16(i16, os.path.join(d, 'tileset.png'))) > 0, 'un .I16 alterado hace fallar la comprobacion')
    print('gentemas + genniveles (niveles 2-5)')
    sys.argv = ['gentemas', '-o', d, '--check']
    ok(gentemas.main() == 0, 'gentemas: 4 tilesets y su ida y vuelta')
    shutil.copyfile(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'assets', 'paths.def'), os.path.join(d, 'paths.def'))
    sys.argv = ['genniveles', '-o', d]
    ok(genniveles.main() == 0, 'genniveles: NIVEL2..5.LVL')
    ref = None
    for n in range(2, 6):
        o = os.path.join(d, 'o%d' % n)
        sys.argv = ['map2bin', os.path.join(d, 'NIVEL%d.LVL' % n), '-o', o, '--check']
        ok(map2bin.main() == 0, 'NIVEL%d.LVL: ida y vuelta y regla de los 3 por linea' % n)
        ok(os.path.getsize(os.path.join(o, 'NIVEL%d.MAP' % n)) <= 7168, 'NIVEL%d.MAP cabe en 7 KB' % n)
        cur = make.inc_common(open(os.path.join(o, 'LEVEL.INC'), encoding='utf-8').read())
        ref = ref or cur
        ok(cur == ref, 'NIVEL%d: LEVEL.INC igual salvo los nombres de los tiles' % n)
    print('gentitulo')
    sys.argv = ['gentitulo', '-o', d, '--check']
    ok(gentitulo.main() == 0, 'gentitulo: 160x100, franja de las cifras negra y lisa')
    tpng = os.path.join(d, 'titulo.png')
    ti16 = os.path.join(d, 'TITULO.I16')
    make.make_i16(tpng, ti16)
    ok(gentiles.check_i16(ti16, tpng) == [], 'TITULO.I16 = PNG')
    w, h, img, pal = gentitulo.pngio.read_indexed(tpng)
    img[88][60] = 5
    gentitulo.pngio.write_indexed(tpng, w, h, img, pal)
    ok(len(gentitulo.check(tpng)) > 0, 'un pixel en la franja de las cifras hace fallar la comprobacion')


LVL = """name TEST
scroll 1
tile A 0
tile B 1
path DOWN
  line 100 0 2
path UP
  line 10 0 0
%s
map
%s
end
"""


def level(tmp, waves, rows=8):
    p = os.path.join(tmp, 'test.lvl')
    maprows = '\n'.join('  ' + ' '.join(['A' if (r + c) % 2 else 'B' for c in range(8)]) for r in range(rows))
    open(p, 'w').write(LVL % (waves, maprows))
    return p


def test_map2bin(tmp):
    print('map2bin')
    p = level(tmp, 'wave pos=50 type=E2 x=48 path=DOWN count=2 spacing=10\nwave pos=20 type=E1 x=10 path=DOWN count=1 flags=mirror,ground')
    lv = map2bin.parse_level(p)
    map2bin.validate(lv)
    data = map2bin.build_binary(lv)
    ok(data[:4] == b'ELV1' and data[4] == 1 and data[5] == 2, 'cabecera: magic, scroll 1, 2 oleadas')
    ok(struct.unpack('<HHHHH', data[6:16]) == (8, 16, 16 + 8 * 3, 16 + 24 + 64, 28), 'cabecera: filas, desplazamientos, longitud y scroll final = 16*8-100')
    ok(data[16:24] == bytes([20, 0, 0, 10, 1, 1, 0, 5]),'primera oleada (la de pos 20, ordenada): %s' % data[16:24].hex())
    ok(data[24:32] == bytes([50, 0, 1, 48, 1, 2, 10, 0]), 'segunda oleada: %s' % data[24:32].hex())
    ok(data[32:34] == b'\xff\xff', 'oleada final con POS = $FFFF')
    ok(list(data[40:48]) == lv.map[-1], 'la fila 0 del binario es la ultima linea del texto (el principio del nivel)')
    dec = map2bin.decode_binary(data)
    ok(dec['rows'] == list(reversed(lv.map)), 'decodificar el mapa reproduce el texto')
    inc = map2bin.make_inc(lv)
    errs, worst, _ = map2bin.check(lv, data, inc, None, 3.0)
    ok(errs == [], 'ida y vuelta y regla correctas (peso maximo %.1f)' % worst)
    # trayectoria con decimales: 1,5 px por tick -> pasos 2,1,2,1... y la suma exacta
    p2 = level(tmp, 'wave pos=0 type=E1 x=0 path=DOWN count=1')
    lv2 = map2bin.parse_level(os.path.join(tmp, 'test.lvl'))
    # posicion r en filas
    lv3 = map2bin.parse_wave(['pos=r10+4', 'type=E1', 'x=0', 'path=DOWN', 'count=1'], lv2, 1)
    ok(lv3['pos'] == 16 * 10 - 100 + 4, 'pos=r10+4 = %d' % lv3['pos'])
    # regla: cuatro 16x16 a la vez rompen el limite de 3
    p4 = level(tmp, 'wave pos=0 type=E1 x=16 path=DOWN count=4 spacing=32 flags=row')
    lv4 = map2bin.parse_level(p4)
    d4 = map2bin.build_binary(lv4)
    errs, worst, _ = map2bin.check(lv4, d4, map2bin.make_inc(lv4), None, 3.0)
    ok(any('regla de diseno' in e for e in errs) and worst >= 4.0, 'cuatro enemigos en fila incumplen la regla (peso %.1f)' % worst)
    # un enemigo que no sale nunca
    p5 = level(tmp, 'wave pos=0 type=E1 x=16 path=STILL count=1')
    lv5 = map2bin.parse_level(p5)
    errs, _, _ = map2bin.check(lv5, map2bin.build_binary(lv5), map2bin.make_inc(lv5), None, 3.0)
    ok(any('sigue en pantalla' in e for e in errs), 'un enemigo aereo con trayectoria STILL se detecta')
    # binario alterado
    bad = bytearray(data)
    bad[44] ^= 1
    errs, _, _ = map2bin.check(lv, bytes(bad), inc, None, 3.0)
    ok(len(errs) > 0, 'un binario alterado hace fallar la comprobacion')
    # errores de sintaxis
    for txt, what in (('wave pos=0 type=E1 x=0 path=NOEXISTE count=1', 'trayectoria desconocida'), ('wave pos=r2 type=E1 x=0 path=DOWN count=1', 'fila ya en pantalla')):
        try:
            map2bin.parse_level(level(tmp, txt))
            ok(False, 'se rechaza: ' + what)
        except map2bin.LevelError:
            ok(True, 'se rechaza: ' + what)
    # el nivel de ejemplo
    assets = os.path.join(HERE, '..', 'assets')
    lvl = map2bin.parse_level(os.path.join(assets, 'NIVEL1.LVL'))
    map2bin.validate(lvl)
    dd = map2bin.build_binary(lvl)
    errs, worst, _ = map2bin.check(lvl, dd, map2bin.make_inc(lvl), None, 3.0)
    ok(errs == [], 'NIVEL1.LVL: ida y vuelta y regla correctas (peso maximo %.1f)' % worst)


def test_pasmo(tmp):
    """Si hay pasmo en el PATH, comprueba que HOJA.INC y LEVEL.INC (los de out/) ensamblan y que las etiquetas valen lo previsto."""
    print('pasmo')
    exe = shutil.which('pasmo')
    out = os.path.join(HERE, '..', 'out')
    if not exe or not os.path.exists(os.path.join(out, 'HOJA.INC')):
        print('  (sin pasmo en el PATH o sin out/: se omite)')
        return
    import subprocess
    for f in ('HOJA.INC', 'LEVEL.INC'):
        shutil.copy(os.path.join(out, f), os.path.join(tmp, f))
    open(os.path.join(tmp, 't.asm'), 'w').write(
        '\torg $4800\n\tdefb NAVE,ENEMY6_1,CIFRA_9,SHEET_CELLS,TYPE_BOSS,WF_GROUND,PATH_DIVE\n\tdefw PATHTAB,TILEADR,SHEET_ADDR\n'
        '\tinclude "HOJA.INC"\n\tinclude "LEVEL.INC"\n')
    r = subprocess.run([exe, '--bin', 't.asm', 't.bin'], cwd=tmp, capture_output=True, text=True)
    ok(r.returncode == 0, 'pasmo ensambla HOJA.INC + LEVEL.INC (%s)' % (r.stderr.strip()[:200] or 'sin errores'))
    if r.returncode == 0:
        b = open(os.path.join(tmp, 't.bin'), 'rb').read()
        ok(b[1] == 186 and b[2] == 219, 'ENEMY6_1 = 186 y CIFRA_9 = 219 (%d, %d)' % (b[1], b[2]))
        ok(b[0] == 128 and b[3] == 121 and b[4] == 6 and b[5] == 4, 'NAVE = 128, SHEET_CELLS = 121, TYPE_BOSS = 6, WF_GROUND = 4 (%s)' % b[:7].hex())


def main():
    tmp = tempfile.mkdtemp(prefix='estela')
    try:
        test_spr2cel(tmp)
        test_sprites_and_tiles(tmp)
        test_map2bin(tmp)
        test_pasmo(tmp)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print('\n%s' % ('TODO CORRECTO' if not fails else '%d FALLOS' % len(fails)))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
