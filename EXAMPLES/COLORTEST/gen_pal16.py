#!/usr/bin/env python3
"""Genera pal16.inc: paletas de 16 colores de prueba para la FPGA con paleta de 16 (POKE 2063/2064/2066/2067).

Siete rampas de 16 niveles, en este orden (el de colortest.asm: NPAL y la tabla de nombres): gris, rojo, verde, azul, amarillo, cian y magenta.
El color de Chroma de indice k ({BRIGHT,G,R,B}: 0 negro, 1 azul, 2 rojo, 3 magenta, 4 verde, 5 cian, 6 amarillo, 7 blanco, +8 brillante)
sale con el nivel k/15 de la rampa, pasado por el modelo de la red analogica de la SCART de ../MODE160/pal160.py. El gris llega hasta el blanco
brillante; los colores, hasta lo que da la red sin brillo (0,455 V por canal, el 63 % del maximo: un primario puro no puede ser mas claro sin
desaturarse). Cada paleta son 16 entradas de dos bytes, bajo {G,B} (POKE 2064) y alto {BRIGHT,R} (POKE 2066): 32 bytes.
"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'MODE160'))
import pal160

TOP = pal160.CA / pal160.VMAX                      # el primario mas alto sin brillo, como fraccion de VMAX
PALETTES = [                                       # nombre, (peso de R, de G, de B), tope de la rampa
    ('GREY',    (1, 1, 1), 1.0),
    ('RED',     (1, 0, 0), TOP),
    ('GREEN',   (0, 1, 0), TOP),
    ('BLUE',    (0, 0, 1), TOP),
    ('YELLOW',  (1, 1, 0), TOP),
    ('CYAN',    (0, 1, 1), TOP),
    ('MAGENTA', (1, 0, 1), TOP),
]

lines = ['; paletas de 16 colores de prueba (gen_pal16.py), 32 bytes cada una: bajo {G,B}, alto {BRIGHT,R}']
for name, w, top in PALETTES:
    lines.append('; %s' % name)
    for c in range(16):
        v = top * c / 15.0
        nr, ng, nb, nw = pal160.solve(w[0] * v, w[1] * v, w[2] * v)
        lo, hi = (ng << 4) | nb, (nw << 4) | nr
        lines.append('            defb 0%02Xh,0%02Xh           ; %2d  R%2d G%2d B%2d W%2d' % (lo, hi, c, nr, ng, nb, nw))
open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'pal16.inc'), 'w').write('\n'.join(lines) + '\n')
print('\n'.join(lines))
