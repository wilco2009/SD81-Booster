#!/usr/bin/env python3
"""Mide el choque de color de las imagenes AGI al pasarlas al HiRes 8x1 (2 colores por cada 8 pixeles).

Para cada celda de 8 pixeles horizontales se eligen los dos colores mas frecuentes; el resto de pixeles
pasa al mas cercano de los dos (distancia RGB). Se cuentan los pixeles cuyo color cambia.

Geometrias:
  A  1 pixel HiRes por pixel AGI (160 de ancho, 20 celdas por linea)
  C  estirado 160 -> 256 (factor 1,6), 32 celdas por linea; se compara con la imagen estirada

Uso:  medir8x1.py JUEGO [SALIDA]   (SALIDA = carpeta donde dejar el PNG de la peor y la mediana)
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import agi_read as ag

W, H = ag.W, ag.H
DIST = [[sum((a - b) ** 2 for a, b in zip(ag.EGA[i], ag.EGA[j])) for j in range(16)] for i in range(16)]


def quantize(line):
    """line: colores de una linea (largo multiplo de 8) -> (nueva linea, pixeles cambiados)."""
    out = []
    bad = 0
    for c in range(0, len(line), 8):
        cell = line[c:c + 8]
        cnt = {}
        for v in cell:
            cnt[v] = cnt.get(v, 0) + 1
        top = sorted(cnt, key=lambda k: (-cnt[k], k))[:2]
        for v in cell:
            if v in top:
                out.append(v)
            else:
                out.append(min(top, key=lambda t: DIST[v][t]))
                bad += 1
    return out, bad


def geom_a(vis):
    """160 de ancho: 20 celdas por linea."""
    res = []
    bad = 0
    for y in range(H):
        q, b = quantize(vis[y * W:(y + 1) * W])
        res.append(q)
        bad += b
    return res, bad, W * H


def geom_c(vis):
    """Estirado a 256."""
    res = []
    bad = 0
    idx = [x * W // 256 for x in range(256)]
    for y in range(H):
        row = vis[y * W:(y + 1) * W]
        line = [row[i] for i in idx]
        q, b = quantize(line)
        res.append(q)
        bad += b
    return res, bad, 256 * H


def render(pic, a, c):
    """original estirado | A | C, a 1:1 en HiRes."""
    idx = [x * W // 256 for x in range(256)]
    rows = []
    for y in range(H):
        row = pic.vis[y * W:(y + 1) * W]
        r = [ag.EGA[row[i]] for i in idx] + [(40, 40, 40)] * 4
        r += [ag.EGA[v] for v in a[y]] + [(40, 40, 40)] * (4 + (256 - W))
        r += [ag.EGA[v] for v in c[y]]
        rows.append(r)
    return rows, 256 * 3 + 8 + 256 - W + 0


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    g = ag.Game(argv[1])
    out = argv[2] if len(argv) > 2 else None
    res = []
    for n in g.present('PIC'):
        d = g.resource('PIC', n)
        try:
            pic = ag.Picture(d)
        except (IndexError, ValueError):
            continue
        a, ba, na = geom_a(pic.vis)
        c, bc, nc = geom_c(pic.vis)
        res.append((n, 100.0 * ba / na, 100.0 * bc / nc, pic, a, c))
    if not res:
        return 1
    ma = sum(r[1] for r in res) / len(res)
    mc = sum(r[2] for r in res) / len(res)
    print('%s: %d imagenes; pixeles cambiados  A(160): media %.2f%%  C(256): media %.2f%%' % (os.path.basename(argv[1].rstrip('/\\')), len(res), ma, mc))
    byc = sorted(res, key=lambda r: r[2])
    print('   C: mediana %.2f%%, peor %.2f%% (imagen %d), mejor %.2f%%' % (byc[len(byc) // 2][2], byc[-1][2], byc[-1][0], byc[0][2]))
    byA = sorted(res, key=lambda r: r[1])
    print('   A: mediana %.2f%%, peor %.2f%% (imagen %d)' % (byA[len(byA) // 2][1], byA[-1][1], byA[-1][0]))
    if out:
        os.makedirs(out, exist_ok=True)
        for tag, r in (('peor', byc[-1]), ('mediana', byc[len(byc) // 2])):
            rows, w = render(r[3], r[4], r[5])
            ag.write_png(os.path.join(out, '%s_%03d.png' % (tag, r[0])), rows, len(rows[0]), H)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
