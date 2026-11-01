#!/usr/bin/env python3
"""PNG de 160x200 con hasta 16 colores -> pic4.bin (16.000 bytes, 80 por fila, nibble alto = pixel de la izquierda) y
pal4.bin (32 bytes: por entrada, byte bajo {G,B} y byte alto {BRIGHT,R}, listo para POKE 2064 / 2066 de la paleta de 16)
para el modo 160x200 a 4 bpp (POKE 2045,176). Solo biblioteca estandar (el PNG no puede ser entrelazado).

Uso:  python png2i4.py imagen.png [-o carpeta] [--gamma 1.0]
Los colores pasan a las cuatro densidades con el modelo de la red de la SCART de pal160.py (como png2i16.py).
"""
import argparse, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'PHOTO'))
import png2i16, pal160

W, H = 160, 200


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('png')
    ap.add_argument('-o', '--out', default='.')
    ap.add_argument('--gamma', type=float, default=1.0)
    a = ap.parse_args()
    w, h, img, plte = png2i16.read_png(a.png)
    if (w, h) != (W, H):
        sys.exit('mide %dx%d y debe medir %dx%d' % (w, h, W, H))
    pix, pal_rgb = (img, plte) if plte is not None else png2i16.index_rgb(img)
    if len(pal_rgb) > 16 or max(max(r) for r in pix) > 15:
        sys.exit('tiene mas de 16 colores: reduce la paleta con tu editor')
    pal = bytearray()
    for i in range(16):
        r, g, b = pal_rgb[i] if i < len(pal_rgb) else (0, 0, 0)
        nr, ng, nb, nw = pal160.solve(*[(c / 255.0) ** a.gamma for c in (r, g, b)])
        pal += pal160.entry(nr, ng, nb, nw)
    body = bytearray()
    for row in pix:
        for x in range(0, W, 2):
            body.append((row[x] << 4) | row[x + 1])
    assert len(body) == 16000
    os.makedirs(a.out, exist_ok=True)
    open(os.path.join(a.out, 'pic4.bin'), 'wb').write(body)
    open(os.path.join(a.out, 'pal4.bin'), 'wb').write(pal)
    print('pic4.bin (16000 bytes) y pal4.bin (32 bytes), %d colores' % len(pal_rgb))


if __name__ == '__main__':
    main()
