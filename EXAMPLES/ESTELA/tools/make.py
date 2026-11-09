#!/usr/bin/env python3
"""make.py -- genera todos los datos de ESTELA a partir de assets/ y los deja en out/ (con sus comprobaciones).

    assets/sprites.png + sprites.def  --spr2cel-->  out/HOJA.BIN, HOJA.PAL, HOJA.INC
    assets/tileset.png                --png2i16-->  out/NIVEL1.I16
    assets/titulo.png                 --png2i16-->  out/TITULO.I16  (pantalla de titulo; ESTELA la enseña si existe)
    assets/NIVEL1.LVL (+ tiles.def, paths.def)  --map2bin-->  out/NIVEL1.MAP, LEVEL.INC

Con --regen se regeneran antes los graficos provisionales (gensprites.py, gentiles.py y gentitulo.py, que SOBREESCRIBEN
sprites.png, tileset.png y titulo.png: no usarlo cuando ya hay graficos definitivos). El nivel (NIVEL1.LVL) nunca se regenera: se edita a mano.

Uso:  python make.py [--regen] [--out ../out]
"""
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gensprites  # noqa: E402
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


def main():
    ap = argparse.ArgumentParser(description='genera los datos de ESTELA')
    ap.add_argument('--regen', action='store_true', help='regenera antes los graficos provisionales (sobreescribe los PNG)')
    ap.add_argument('--out', default=os.path.join(HERE, '..', 'out'))
    a = ap.parse_args()
    assets = os.path.join(HERE, '..', 'assets')
    out = a.out
    os.makedirs(out, exist_ok=True)
    rc = 0
    if a.regen:
        for mod, name in ((gensprites, 'gensprites'), (gentiles, 'gentiles'), (gentitulo, 'gentitulo')):
            sys.argv = [name, '-o', assets, '--check']
            rc |= mod.main()
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
    print('make: %s' % ('correcto' if rc == 0 else 'CON FALLOS'))
    return rc


if __name__ == '__main__':
    sys.exit(main())
