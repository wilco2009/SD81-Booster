#!/usr/bin/env python3
"""Paleta de prueba del modo 160x100 a 8 bpp (POKE 2045,175) y modelo de la red analogica de la SCART.

Cada color de la paleta son cuatro densidades de pulsos de 0 a 15 (n/15), una por pin: R, G, B y BRIGHT.
La tension en la SCART de cada canal (red de resistencias del esquema: 270 ohm desde el pin del canal, 130 ohm a masa,
470 ohm desde BRIGHT, carga de 75 ohm del televisor) es

    V = CA * dc + CB * b        (dc = densidad del pin del canal, b = densidad de BRIGHT, comun a los tres canales)

asi que de un color (R,G,B) en voltios hay que elegir el b comun que deje a los tres canales dentro de 0..CA.
Uso:  python pal160.py   -> escribe pal160.bin (512 bytes: por entrada, byte bajo {G,B} y byte alto {BRIGHT,R}) y pal160.txt
"""
import colorsys, sys

VDD = 3.3
RS, RG, RB, RL = 270.0, 130.0, 470.0, 75.0
GS, GG, GB, GL = 1 / RS, 1 / RG, 1 / RB, 1 / RL
GT = GS + GG + GB + GL
CA = VDD * GS / GT      # voltios por unidad de densidad del pin del canal
CB = VDD * GB / GT      # voltios por unidad de densidad de BRIGHT
VMAX = CA + CB          # todo a 1 (blanco brillante)


def volts(nr, ng, nb, nw):
    b = nw / 15.0
    return tuple(CA * n / 15.0 + CB * b for n in (nr, ng, nb))


def solve(r, g, b):
    """r, g, b en 0..1 (fraccion de VMAX; la tension se toma directa, sin gamma). Devuelve (nr, ng, nb, nw)."""
    tgt = (r * VMAX, g * VMAX, b * VMAX)
    best = None
    for nw in range(16):
        lift = CB * nw / 15.0
        ns = []
        err = 0.0
        for t in tgt:
            n = min(15, max(0, int(round((t - lift) / CA * 15.0))))
            ns.append(n)
            err += (CA * n / 15.0 + lift - t) ** 2
        if best is None or err < best[0] - 1e-12:
            best = (err, nw, tuple(ns))
    return best[2][0], best[2][1], best[2][2], best[1]


def entry(nr, ng, nb, nw):
    return bytes([(ng << 4) | nb, (nw << 4) | nr])


def build():
    pal = []
    top = CA / VMAX                      # saturacion maxima de un primario sin brillo (0,635 de VMAX)
    for k in range(16):                  # 0: grises
        v = k / 15.0
        pal.append(solve(v, v, v))
    for ch in range(3):                  # 1-3: rojo, verde, azul (hasta 0,455 V)
        for k in range(16):
            v = top * k / 15.0
            pal.append(solve(*[v if c == ch else 0 for c in range(3)]))
    for ch in range(3):                  # 4-6: amarillo, cian, magenta
        for k in range(16):
            v = top * k / 15.0
            pal.append(solve(*[0 if c == ch else v for c in range(3)]))
    for c in range(16):                  # 7: los 16 colores de Chroma81 (normal = pin a 15, brillante = 15 + BRIGHT 15)
        bits = c & 7
        bri = 15 if c & 8 else 0
        gg, rr, bb = (bits >> 2) & 1, (bits >> 1) & 1, bits & 1      # formato GRB como el puerto $7FEF
        pal.append((15 * rr, 15 * gg, 15 * bb, bri))
    for row in range(8):                 # 8-15: rueda de 16 tonos con saturacion creciente (en el limite del gamut)
        s = (row + 1) / 8.0
        for h in range(16):
            r, g, b = colorsys.hsv_to_rgb(h / 16.0, s, 1.0)
            pal.append(solve(r, g, b))
    assert len(pal) == 256
    return pal


if __name__ == '__main__':
    pal = build()
    open('pal160.bin', 'wb').write(b''.join(entry(*p) for p in pal))
    with open('pal160.txt', 'w') as f:
        f.write('CA = %.3f V, CB = %.3f V, VMAX = %.3f V\n' % (CA, CB, VMAX))
        f.write('idx  R  G  B  W   tension R,G,B (V)\n')
        for i, p in enumerate(pal):
            v = volts(*p)
            f.write('%3d %2d %2d %2d %2d   %.3f %.3f %.3f\n' % (i, p[0], p[1], p[2], p[3], v[0], v[1], v[2]))
    print('pal160.bin escrito (512 bytes); CA = %.3f V, CB = %.3f V, VMAX = %.3f V' % (CA, CB, VMAX))
