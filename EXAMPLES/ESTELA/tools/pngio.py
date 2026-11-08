#!/usr/bin/env python3
"""Lectura y escritura de PNG indexados con la biblioteca estandar (zlib), comun a las herramientas de ESTELA.

La lectura es la de EXAMPLES/PHOTO/png2i16.py (acepta PNG indexados de 1, 2, 4 y 8 bits, sin entrelazado).
La escritura saca siempre un PNG indexado de 8 bits con paleta (PLTE) y, si se pide, tRNS con el indice 0 transparente
(para que el editor lo enseñe como transparente; las herramientas lo ignoran al leer).
"""
import os
import struct
import sys
import zlib

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(_HERE, '..', '..', 'PHOTO'))
sys.path.insert(0, os.path.join(_HERE, '..', '..', 'MODE160'))
import png2i16  # noqa: E402  (read_png, convert) -- y con el pal160, el modelo de la red analogica
import pal160   # noqa: E402,F401

PngError = png2i16.PngError


def read_indexed(path):
    """Devuelve (ancho, alto, filas de indices, paleta de (r, g, b)). Error si el PNG no es indexado."""
    w, h, img, plte = png2i16.read_png(path)
    if plte is None:
        raise PngError('%s no es un PNG indexado (guardalo con paleta)' % path)
    return w, h, img, plte


def _chunk(typ, body):
    c = struct.pack('>I', len(body)) + typ + body
    return c + struct.pack('>I', zlib.crc32(typ + body) & 0xFFFFFFFF)


def write_indexed(path, w, h, rows, palette, transparent0=False, scale=1):
    """Escribe un PNG indexado de 8 bits. rows: h filas de w indices; palette: lista de (r, g, b) (hasta 256).
    scale > 1 repite cada pixel scale x scale veces (para las vistas previas)."""
    if len(palette) > 256:
        raise PngError('la paleta tiene %d entradas' % len(palette))
    raw = bytearray()
    for row in rows:
        if len(row) != w:
            raise PngError('fila de %d pixeles en una imagen de %d' % (len(row), w))
        if scale > 1:
            line = bytes(v for v in row for _ in range(scale))
        else:
            line = bytes(row)
        for _ in range(scale):
            raw.append(0)
            raw += line
    out = b'\x89PNG\r\n\x1a\n'
    out += _chunk(b'IHDR', struct.pack('>IIBBBBB', w * scale, h * scale, 8, 3, 0, 0, 0))
    out += _chunk(b'PLTE', b''.join(bytes(c) for c in palette))
    if transparent0:
        out += _chunk(b'tRNS', b'\x00')
    out += _chunk(b'IDAT', zlib.compress(bytes(raw), 9))
    out += _chunk(b'IEND', b'')
    d = os.path.dirname(path)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(path, 'wb') as f:
        f.write(out)
