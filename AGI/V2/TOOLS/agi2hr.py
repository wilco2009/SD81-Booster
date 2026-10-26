#!/usr/bin/env python3
"""agi2hr.py: imagenes de AGI v2 -> PICnnn.GFX para el HiRes 8x1 de 16 colores (256x168).

El plano visual de 160x168 se estira a 256 de ancho (x1,6: el pixel HiRes x toma el pixel AGI x*160//256) y se parte en celdas de
8x1 pixeles con dos colores cada una (los dos que menos error de color dan entre los presentes). Los colores EGA pasan a los
codigos de Chroma (bit 3 brillo, 2 verde, 1 rojo, 0 azul). El plano de prioridad se guarda a la resolucion original (160x168) con RLE,
porque los sprites se colocan en coordenadas AGI.

Formato (little endian):
  +0    'AGIP'
  +4    u8 version (1), u8 0
  +6    u16 ancho HiRes (256)
  +8    u8 lineas (168), u8 0
  +10   u16 tamano en bytes de la prioridad (RLE)
  +12   hasta +255 a 0
  +256  bitmap: 168 lineas x 32 bytes (bit 7 = pixel de la izquierda, 1 = tinta)   -> va a $A000 tal cual
  +5632 atributos: 168 x 32 bytes (papel en el nibble alto, tinta en el bajo)      -> va a $C000 tal cual
  +11008 prioridad: por cada una de las 168 filas, runs de (prioridad << 4 | (longitud - 1)); cada fila suma 160 pixeles

Uso:  agi2hr.py JUEGO SALIDA [--png] [n ...]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import agi_read as ag

W, H = ag.W, ag.H
HW = 256
MAGIC = b'AGIP'
HDR = 256
PLANE = H * 32
IDX = [x * W // HW for x in range(HW)]


def to_chroma(e):
    """Indice EGA -> codigo de Chroma (intercambia los bits de rojo y verde; el marron 6 pasa a amarillo 6)."""
    return (e & 8) | ((e & 4) >> 1) | ((e & 2) << 1) | (e & 1)


def chroma_rgb(c):
    br = c >> 3
    return tuple((255 if br else 170) if (c >> bit) & 1 else (85 if br else 0) for bit in (1, 2, 0))


CHROMA = [chroma_rgb(c) for c in range(16)]
DIST = [[sum((a - b) ** 2 for a, b in zip(CHROMA[i], CHROMA[j])) for j in range(16)] for i in range(16)]
_cache = {}


def encode_cell(cell):
    """cell: 8 codigos de Chroma -> (bitmap, atributo, 8 codigos reconstruidos)."""
    key = tuple(cell)
    r = _cache.get(key)
    if r:
        return r
    cnt = {}
    for v in cell:
        cnt[v] = cnt.get(v, 0) + 1
    cols = sorted(cnt)
    if len(cols) == 1:
        paper = ink = cols[0]
    else:
        best = None
        for i, a in enumerate(cols):
            for b in cols[i + 1:]:
                cost = sum(min(DIST[v][a], DIST[v][b]) for v in cell)
                k = (cost, -(cnt[a] + cnt[b]), a, b)
                if best is None or k < best:
                    best = k
        a, b = best[2], best[3]
        paper, ink = (a, b) if (cnt[a], -a) >= (cnt[b], -b) else (b, a)
    bm = 0
    rec = []
    for v in cell:
        if paper == ink or DIST[v][paper] <= DIST[v][ink]:
            rec.append(paper)
            bm <<= 1
        else:
            rec.append(ink)
            bm = (bm << 1) | 1
    r = (bm, (paper << 4) | ink, rec)
    _cache[key] = r
    return r


def convert(pic):
    """Imagen -> (bitmap, atributos, reconstruccion por lineas, pixeles cambiados)."""
    bm = bytearray()
    at = bytearray()
    rec = []
    bad = 0
    for y in range(H):
        row = [to_chroma(pic.vis[y * W + i]) for i in IDX]
        line = []
        for c in range(0, HW, 8):
            b, a, rc = encode_cell(row[c:c + 8])
            bm.append(b)
            at.append(a)
            line.extend(rc)
            bad += sum(1 for p, q in zip(rc, row[c:c + 8]) if p != q)
        rec.append(line)
    return bm, at, rec, bad


def rle_priority(pri):
    out = bytearray()
    for y in range(H):
        row = pri[y * W:(y + 1) * W]
        x = 0
        while x < W:
            v = row[x]
            n = 1
            while x + n < W and row[x + n] == v and n < 16:
                n += 1
            out.append((v << 4) | (n - 1))
            x += n
    return bytes(out)


def unrle_priority(d):
    pri = []
    row = 0
    p = 0
    for _ in range(H):
        cur = []
        while len(cur) < W:
            b = d[p]
            p += 1
            cur.extend([b >> 4] * ((b & 15) + 1))
        pri.extend(cur)
    return pri


def build(pic):
    bm, at, rec, bad = convert(pic)
    pr = rle_priority(pic.pri)
    hdr = bytearray(HDR)
    hdr[0:4] = MAGIC
    hdr[4] = 1
    struct.pack_into('<H', hdr, 6, HW)
    hdr[8] = H
    struct.pack_into('<H', hdr, 10, len(pr))
    return bytes(hdr) + bytes(bm) + bytes(at) + pr, rec, bad


def read_back(data):
    """Verificador: reconstruye (codigos de Chroma por lineas, prioridad) solo desde el fichero."""
    assert data[:4] == MAGIC
    bm = data[HDR:HDR + PLANE]
    at = data[HDR + PLANE:HDR + 2 * PLANE]
    pl = struct.unpack_from('<H', data, 10)[0]
    pr = data[HDR + 2 * PLANE:HDR + 2 * PLANE + pl]
    rows = []
    for y in range(H):
        line = []
        for c in range(32):
            b, a = bm[y * 32 + c], at[y * 32 + c]
            for k in range(8):
                line.append(a & 15 if (b >> (7 - k)) & 1 else a >> 4)
        rows.append(line)
    return rows, unrle_priority(pr)


def main(argv):
    args = [a for a in argv[1:] if not a.startswith('--')]
    png = '--png' in argv
    if len(args) < 2:
        print(__doc__)
        return 1
    g = ag.Game(args[0])
    out = args[1]
    os.makedirs(out, exist_ok=True)
    sel = [int(a) for a in args[2:]] or g.present('PIC')
    tot = bad = n = 0
    sizes = []
    for k in sel:
        d = g.resource('PIC', k)
        if d is None:
            continue
        try:
            pic = ag.Picture(d)
        except (IndexError, ValueError) as e:
            print('PIC %d: error %s' % (k, e))
            continue
        data, rec, b = build(pic)
        rows, pri = read_back(data)
        assert rows == rec and pri == pic.pri, 'PIC %d: el fichero no reproduce la conversion' % k
        with open(os.path.join(out, 'PIC%03d.GFX' % k), 'wb') as f:
            f.write(data)
        if png:
            img = [[CHROMA[v] for v in r] for r in rec]
            ag.write_png(os.path.join(out, 'pic%03d.png' % k), img, HW, H)
        tot += HW * H
        bad += b
        n += 1
        sizes.append(len(data))
    print('%s: %d imagenes, %d bytes en total (media %d, maximo %d); pixeles cambiados %.2f%%; verificadas contra el fichero' % (
        os.path.basename(args[0].rstrip('/\\')), n, sum(sizes), sum(sizes) // max(n, 1), max(sizes), 100.0 * bad / max(tot, 1)))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
