#!/usr/bin/env python
# Genera pieces.inc (las piezas del ajedrez, 16x16 puntos cada una) a partir de la hoja piezas_16x16.png:
# seis casillas de 16x16 en una fila, sin lineas de separacion, en el orden peon, caballo, alfil, torre,
# dama, rey; negro = pieza, blanco = vacio. Esa hoja es la fuente: se retoca en cualquier editor de pixeles
# (guardada como PNG sin entrelazar) y se vuelve a ejecutar:
#
#   python gen_pieces.py            -> pieces.inc
#
# Cada pieza ocupa 2x2 celdas de 8x8: se guarda como cuatro trozos de 8 bytes en este orden:
#   arriba-izquierda, arriba-derecha, abajo-izquierda, abajo-derecha (un byte por linea, bit 7 a la izquierda).
# Las blancas y las negras usan el mismo dibujo; solo cambia la tinta (blanca o negra) de sus celdas.
import sys
from pngtool import read_png

ORDER = "PNBRQK"
SHEET = "piezas_16x16.png"

def load_tiles(path=SHEET):
    w, h, px = read_png(path)
    if (w, h) != (16 * len(ORDER), 16):
        sys.exit("%s: la hoja debe medir %dx16 puntos (6 casillas de 16x16), mide %dx%d" % (path, 16 * len(ORDER), w, h))
    tiles = {}
    for i, k in enumerate(ORDER):
        g = []
        for y in range(16):
            g.append([1 if sum(px[y][i * 16 + x][:3]) < 3 * 128 else 0 for x in range(16)])
        tiles[k] = g
    return tiles

def cells(g):
    """cuatro trozos de 8 bytes: TL, TR, BL, BR"""
    res = []
    for cy in (0, 8):
        for cx in (0, 8):
            for y in range(cy, cy + 8):
                b = 0
                for x in range(cx, cx + 8):
                    b = (b << 1) | g[y][x]
                res.append(b)
    return res

def emit(tiles):
    out = ["; Generado por gen_pieces.py a partir de piezas_16x16.png: no se edita a mano.",
           "; Cada pieza son 32 bytes: celdas arriba-izquierda, arriba-derecha, abajo-izquierda y abajo-derecha",
           "; (8 bytes cada una), en el orden P N B R Q K. Blancas y negras usan el mismo dibujo.",
           "pc_solid:"]
    for k in ORDER:
        d = cells(tiles[k])
        out.append("            ; " + k)
        for i in range(0, 32, 8):
            out.append("            defb " + ",".join("0%02Xh" % b for b in d[i:i + 8]))
    return "\n".join(out) + "\n"

if __name__ == '__main__':
    t = load_tiles()
    open('pieces.inc', 'w', newline='\r\n').write(emit(t))
    print("pieces.inc escrito")
