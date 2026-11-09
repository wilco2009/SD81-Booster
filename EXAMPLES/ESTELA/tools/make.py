#!/usr/bin/env python3
"""make.py -- genera todos los datos de ESTELA a partir de assets/ y los deja en out/ (con sus comprobaciones).

    assets/sprites.png + sprites.def  --spr2cel-->  out/HOJA.BIN, HOJA.PAL, HOJA.INC
    assets/tileset.png                --png2i16-->  out/NIVEL1.I16
    assets/titulo.png                 --png2i16-->  out/TITULO.I16  (pantalla de titulo; ESTELA la enseña si existe)
    assets/NIVEL1.LVL (+ tiles.def, paths.def)  --map2bin-->  out/NIVEL1.MAP, LEVEL.INC
    assets/tilesetN.png (N = 2-5)     --png2i16-->  out/NIVELN.I16   (un tileset y una paleta por nivel; gentemas.py)
    assets/NIVELN.LVL (+ tilesN.def)  --map2bin-->  out/NIVELN.MAP   (niveles 2-5; LEVEL.INC solo sale del 1 y se comprueba que
                                                                      el de los demas es igual salvo los nombres de los tiles)

Con --regen se regeneran antes los graficos provisionales (gensprites.py, gentiles.py, gentemas.py y gentitulo.py, que SOBREESCRIBEN
sprites.png, tileset*.png y titulo.png: no usarlo cuando ya hay graficos definitivos). Los niveles (NIVEL1.LVL ... NIVEL5.LVL) nunca
se regeneran por omision: se editan a mano (--regen-niveles ejecuta genniveles.py y SOBREESCRIBE los .LVL de los niveles 2-5).

Uso:  python make.py [--regen] [--regen-niveles] [--out ../out]
"""
import argparse
import os
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import genniveles  # noqa: E402
import gensprites  # noqa: E402
import gentemas    # noqa: E402
import gentiles    # noqa: E402
import gentitulo   # noqa: E402
import map2bin     # noqa: E402
import pngio       # noqa: E402
import spr2cel     # noqa: E402

png2i16 = pngio.png2i16


def make_i16(png, out_path):
    """tileset.png (160x100 indexado) -> .I16, como png2i16.py."""
    pix, pal_rgb = png2i16.load(png, False)
    if len(pal_rgb) > 256 or max(max(r) for r in pix) >= max(len(pal_rgb), 1):
        raise png2i16.PngError('paleta incorrecta')
    pal, worst = png2i16.convert(pal_rgb, 1.0)
    body = png2i16.MAGIC + pal + bytes(v for row in pix for v in row)
    assert len(body) == 4 + 512 + png2i16.W * png2i16.H
    open(out_path, 'wb').write(body)
    return worst


MAX_LEVEL_BYTES = 7 * 1024


def inc_common(text):
    """LEVEL.INC sin la primera linea (nombre del nivel) ni los TILE_* (nombres de los tiles, distintos en cada tema)."""
    return [l for l in text.splitlines()[1:] if not l.startswith('TILE_')]


def make_levels(assets, out):
    """Niveles 2-5: NIVELn.I16 (tileset + paleta) y NIVELn.MAP, con sus comprobaciones."""
    rc = 0
    ref = inc_common(open(os.path.join(out, 'LEVEL.INC'), encoding='utf-8').read())
    for n in range(2, 6):
        png = os.path.join(assets, 'tileset%d.png' % n)
        i16 = os.path.join(out, 'NIVEL%d.I16' % n)
        worst = make_i16(png, i16)
        errs = gentiles.check_i16(i16, png)
        tmp = tempfile.mkdtemp(prefix='estela')
        try:
            sys.argv = ['map2bin', os.path.join(assets, 'NIVEL%d.LVL' % n), '-o', tmp, '--check']
            r = map2bin.main()
            if r:
                rc |= r
                continue
            mp = os.path.join(tmp, 'NIVEL%d.MAP' % n)
            size = os.path.getsize(mp)
            shutil.copyfile(mp, os.path.join(out, 'NIVEL%d.MAP' % n))
            if size > MAX_LEVEL_BYTES:
                errs.append('NIVEL%d.MAP ocupa %d bytes y el maximo es %d' % (n, size, MAX_LEVEL_BYTES))
            if inc_common(open(os.path.join(tmp, 'LEVEL.INC'), encoding='utf-8').read()) != ref:
                errs.append('NIVEL%d: rangos de paleta, trayectorias o TILEADR distintos de los del nivel 1 (LEVEL.INC)' % n)
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
        for e in errs:
            print('FALLO: ' + e, file=sys.stderr)
        if errs:
            rc |= 2
        else:
            print('NIVEL%d.I16 + NIVEL%d.MAP (%d bytes de %d): correcto; desviacion maxima de la paleta %.1f %%' % (n, n, size, MAX_LEVEL_BYTES, worst * 100))
    return rc


def main():
    ap = argparse.ArgumentParser(description='genera los datos de ESTELA')
    ap.add_argument('--regen', action='store_true', help='regenera antes los graficos provisionales (sobreescribe los PNG)')
    ap.add_argument('--regen-niveles', action='store_true', help='regenera NIVEL2.LVL ... NIVEL5.LVL con genniveles.py (sobreescribe lo editado a mano)')
    ap.add_argument('--out', default=os.path.join(HERE, '..', 'out'))
    a = ap.parse_args()
    assets = os.path.join(HERE, '..', 'assets')
    out = a.out
    os.makedirs(out, exist_ok=True)
    rc = 0
    if a.regen:
        for mod, name in ((gensprites, 'gensprites'), (gentiles, 'gentiles'), (gentemas, 'gentemas'), (gentitulo, 'gentitulo')):
            sys.argv = [name, '-o', assets, '--check']
            rc |= mod.main()
    if a.regen_niveles:
        sys.argv = ['genniveles', '-o', assets]
        rc |= genniveles.main()
    # hoja de sprites
    sys.argv = ['spr2cel', os.path.join(assets, 'sprites.png'), '-o', out, '--check']
    rc |= spr2cel.main()
    # tileset -> NIVEL1.I16 y su comprobacion
    png = os.path.join(assets, 'tileset.png')
    i16 = os.path.join(out, 'NIVEL1.I16')
    worst = make_i16(png, i16)
    errs = gentiles.check_i16(i16, png)
    for e in errs:
        print('FALLO: ' + e, file=sys.stderr)
    if errs:
        rc |= 2
    else:
        print('NIVEL1.I16: 16.516 bytes, desviacion maxima por la red analogica %.1f %%; ida y vuelta correcta' % (worst * 100))
    # pantalla de titulo -> TITULO.I16
    png = os.path.join(assets, 'titulo.png')
    i16 = os.path.join(out, 'TITULO.I16')
    worst = make_i16(png, i16)
    errs = gentiles.check_i16(i16, png) + gentitulo.check(png)
    for e in errs:
        print('FALLO: ' + e, file=sys.stderr)
    if errs:
        rc |= 2
    else:
        print('TITULO.I16: 16.516 bytes, desviacion maxima por la red analogica %.1f %%; ida y vuelta correcta' % (worst * 100))
    # mapa y oleadas
    sys.argv = ['map2bin', os.path.join(assets, 'NIVEL1.LVL'), '-o', out, '--check']
    rc |= map2bin.main()
    rc |= make_levels(assets, out)
    print('make: %s' % ('correcto' if rc == 0 else 'CON FALLOS'))
    return rc


if __name__ == '__main__':
    sys.exit(main())
