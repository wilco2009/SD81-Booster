#!/usr/bin/env python3
"""Convierte PNG de 160x100 (hasta 256 colores) a ficheros IMGnnn.I16 para el visor PHOTO del SD81 Booster (modo 160x100 a 8 bpp).

Formato .I16 (16.516 bytes):
    0      4 bytes   "I160"
    4    512 bytes   paleta: por entrada, byte bajo {G[3:0],B[3:0]} y byte alto {BRIGHT[3:0],R[3:0]} (listo para POKE 2064 / 2065);
                     cada campo es la densidad de pulsos 0-15 de su pin (ver FPGA/SD81V2.1000/mode160.v)
    516 16000 bytes  pixeles, fila a fila (100 filas de 160), un byte por pixel = indice de la paleta

Los colores del PNG (RGB de 8 bits) pasan a las cuatro densidades con el modelo de la red analogica de la SCART de
../MODE160/pal160.py: los colores fuera de lo que da la red (el canal mas alto menos el mas bajo no pasa de 0,455 V sobre 0,717 V)
se acercan al mas parecido y el programa dice cuanto se han movido.

Uso:
    python png2i16.py foto1.png foto2.png ...            -> IMG001.I16, IMG002.I16 ... en la carpeta actual
    python png2i16.py -o SD/FOTOS --start 10 *.png       -> IMG010.I16 ... en SD/FOTOS
    python png2i16.py --gamma 1.4 foto.png               -> aclara / oscurece los medios tonos (v = (c/255) ** gamma)

El PNG puede ser indexado (se respeta el orden de su paleta) o RGB / gris / con alfa si no pasa de 256 colores distintos. Solo con la
biblioteca estandar: es lo que se necesita para un PNG de 160x100 con paleta. Si esta instalado Pillow (pip install Pillow) tambien
se aceptan PNG entrelazados, con mas de 256 colores (los reduce a 256 sin tramado) y de otro tamano con --resize.
"""
import argparse
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'MODE160'))
import pal160  # el modelo de la red analogica y la conversion de RGB a las cuatro densidades

W, H = 160, 100
MAGIC = b'I160'


class PngError(Exception):
    pass


def paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    return a if pa <= pb and pa <= pc else (b if pb <= pc else c)


def read_png(path):
    """Devuelve (ancho, alto, filas de (r, g, b) o de indices, paleta o None). Sin entrelazado."""
    data = open(path, 'rb').read()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise PngError('no es un PNG')
    pos = 8
    ihdr = None
    plte = None
    idat = []
    while pos + 8 <= len(data):
        n, typ = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        pos += 12 + n
        if typ == b'IHDR':
            ihdr = struct.unpack('>IIBBBBB', body)
        elif typ == b'PLTE':
            plte = [tuple(body[i:i + 3]) for i in range(0, len(body) - 2, 3)]
        elif typ == b'IDAT':
            idat.append(body)
        elif typ == b'IEND':
            break
    if ihdr is None or not idat:
        raise PngError('PNG incompleto')
    w, h, depth, ctype, _comp, _filt, inter = ihdr
    if inter:
        raise PngError('PNG entrelazado (Adam7): guardalo sin entrelazado o instala Pillow')
    chans = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}.get(ctype)
    if chans is None or depth not in (1, 2, 4, 8, 16) or (ctype != 0 and ctype != 3 and depth < 8) or (ctype == 3 and depth == 16):
        raise PngError('tipo de PNG no soportado (color %d, %d bits)' % (ctype, depth))
    raw = zlib.decompress(b''.join(idat))
    bpp = max(1, chans * depth // 8)
    stride = (w * chans * depth + 7) // 8
    if len(raw) < h * (stride + 1):
        raise PngError('datos de imagen cortos')
    rows = []
    prev = bytearray(stride)
    for y in range(h):
        base = y * (stride + 1)
        ft = raw[base]
        line = bytearray(raw[base + 1:base + 1 + stride])
        if ft == 1:
            for i in range(bpp, stride):
                line[i] = (line[i] + line[i - bpp]) & 255
        elif ft == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 255
        elif ft == 3:
            for i in range(stride):
                left = line[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 255
        elif ft == 4:
            for i in range(stride):
                left = line[i - bpp] if i >= bpp else 0
                ul = prev[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + paeth(left, prev[i], ul)) & 255
        elif ft != 0:
            raise PngError('filtro %d desconocido' % ft)
        prev = line
        rows.append(bytes(line))
    # muestras por pixel
    def samples(line):
        if depth == 8:
            return list(line)
        if depth == 16:
            return [line[i] for i in range(0, len(line), 2)]          # el byte alto
        out = []
        mask = (1 << depth) - 1
        for byte in line:
            for sh in range(8 - depth, -1, -depth):
                out.append((byte >> sh) & mask)
        return out
    img = []
    for line in rows:
        s = samples(line)
        row = []
        for x in range(w):
            if ctype == 3:
                row.append(s[x])
            elif ctype == 0:
                g = s[x] * 255 // ((1 << depth) - 1) if depth < 8 else s[x]
                row.append((g, g, g))
            elif ctype == 4:
                g = s[2 * x]
                row.append((g, g, g))
            else:
                row.append((s[chans * x], s[chans * x + 1], s[chans * x + 2]))
        img.append(row)
    if ctype == 3:
        if plte is None:
            raise PngError('PNG indexado sin paleta')
        return w, h, img, plte
    return w, h, img, None


def load(path, resize):
    """Devuelve (filas de indices 160x100, paleta de hasta 256 (r, g, b))."""
    try:
        from PIL import Image
    except ImportError:
        Image = None
    if Image is not None:
        im = Image.open(path)
        if im.size != (W, H):
            if not resize:
                raise PngError('mide %dx%d y debe medir %dx%d (con --resize se ajusta)' % (im.size[0], im.size[1], W, H))
            im = im.convert('RGB').resize((W, H), Image.LANCZOS)
        if im.mode == 'P' and len(im.getpalette() or []) // 3 <= 256:
            idx = list(im.getdata())
            pal = [tuple(im.getpalette()[3 * i:3 * i + 3]) for i in range(len(im.getpalette()) // 3)]
            return [idx[y * W:(y + 1) * W] for y in range(H)], pal
        im = im.convert('RGB')
        cols = im.getcolors(maxcolors=W * H)
        if len(cols) > 256:
            im = im.quantize(256, dither=Image.Dither.NONE)
            idx = list(im.getdata())
            pal = [tuple(im.getpalette()[3 * i:3 * i + 3]) for i in range(len(im.getpalette()) // 3)]
            return [idx[y * W:(y + 1) * W] for y in range(H)], pal
        pix = list(im.getdata())
        return index_rgb([pix[y * W:(y + 1) * W] for y in range(H)])
    w, h, img, plte = read_png(path)
    if (w, h) != (W, H):
        raise PngError('mide %dx%d y debe medir %dx%d (para ajustar el tamano instala Pillow y usa --resize)' % (w, h, W, H))
    if plte is not None:
        return img, plte
    return index_rgb(img)


def index_rgb(img):
    """Imagen RGB con hasta 256 colores distintos -> indices por orden de aparicion."""
    table = {}
    out = []
    for row in img:
        r = []
        for p in row:
            if p not in table:
                if len(table) == 256:
                    raise PngError('tiene mas de 256 colores distintos (instala Pillow para reducirlos, o reduce la paleta con tu editor)')
                table[p] = len(table)
            r.append(table[p])
        out.append(r)
    pal = [None] * len(table)
    for p, i in table.items():
        pal[i] = p
    return out, pal


def convert(pal_rgb, gamma):
    """Paleta RGB -> 512 bytes y el error maximo (fraccion de VMAX) de pasar por las cuatro densidades."""
    out = bytearray()
    worst = 0.0
    for i in range(256):
        r, g, b = pal_rgb[i] if i < len(pal_rgb) else (0, 0, 0)
        tgt = [(c / 255.0) ** gamma for c in (r, g, b)]
        nr, ng, nb, nw = pal160.solve(*tgt)
        out += pal160.entry(nr, ng, nb, nw)
        got = pal160.volts(nr, ng, nb, nw)
        for t, v in zip(tgt, got):
            worst = max(worst, abs(v - t * pal160.VMAX) / pal160.VMAX)
    return bytes(out), worst


def main():
    ap = argparse.ArgumentParser(description='PNG de 160x100 -> IMGnnn.I16 para el visor PHOTO')
    ap.add_argument('png', nargs='+')
    ap.add_argument('-o', '--out', default='.', help='carpeta de salida (por defecto, la actual)')
    ap.add_argument('--start', type=int, default=1, help='numero del primer fichero (IMGnnn.I16, 1-255)')
    ap.add_argument('--gamma', type=float, default=1.0, help='v = (c/255) ** gamma antes de pasar a densidades (1 = directo)')
    ap.add_argument('--resize', action='store_true', help='ajustar a 160x100 (necesita Pillow)')
    a = ap.parse_args()
    if a.start < 1 or a.start + len(a.png) - 1 > 255:
        ap.error('los numeros de fichero van de 1 a 255')
    os.makedirs(a.out, exist_ok=True)
    bad = 0
    for k, path in enumerate(a.png):
        name = 'IMG%03d.I16' % (a.start + k)
        try:
            pix, pal_rgb = load(path, a.resize)
            if len(pal_rgb) > 256:
                raise PngError('la paleta tiene %d entradas' % len(pal_rgb))
            if max(max(r) for r in pix) >= max(len(pal_rgb), 1):
                raise PngError('un pixel usa un color que no esta en la paleta')
            pal, worst = convert(pal_rgb, a.gamma)
            body = MAGIC + pal + bytes(v for row in pix for v in row)
            assert len(body) == 4 + 512 + W * H
            open(os.path.join(a.out, name), 'wb').write(body)
            print('%s <- %s: %d colores, desviacion maxima por la red analogica %.1f %%' % (name, path, len(pal_rgb), worst * 100))
        except (PngError, OSError, zlib.error) as e:
            bad += 1
            print('%s: %s' % (path, e), file=sys.stderr)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
