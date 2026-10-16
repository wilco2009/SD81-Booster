#!/usr/bin/env python
# Utilidades minimas de PNG (sin librerias): leer (RGB/RGBA/paleta, sin entrelazar), recortar y ampliar.
#   python pngtool.py info foto.png
#   python pngtool.py crop foto.png salida.png x y w h escala
import struct, sys, zlib

def read_png(path):
    d = open(path, 'rb').read()
    assert d[:8] == b'\x89PNG\r\n\x1a\n'
    pos, idat, plte = 8, b'', None
    while pos < len(d):
        n, t = struct.unpack('>I4s', d[pos:pos + 8])
        c = d[pos + 8:pos + 8 + n]
        if t == b'IHDR':
            w, h, bd, ct, _, _, il = struct.unpack('>IIBBBBB', c)
        elif t == b'PLTE':
            plte = [tuple(c[i:i + 3]) for i in range(0, len(c), 3)]
        elif t == b'IDAT':
            idat += c
        pos += 12 + n
    assert il == 0 and bd in (8,), (bd, il)
    ch = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ct]
    raw = zlib.decompress(idat)
    stride = w * ch
    rows, prev = [], bytearray(stride)
    p = 0
    for y in range(h):
        f = raw[p]; line = bytearray(raw[p + 1:p + 1 + stride]); p += 1 + stride
        for i in range(stride):
            a = line[i - ch] if i >= ch else 0
            b = prev[i]
            c = prev[i - ch] if i >= ch else 0
            if f == 1: line[i] = (line[i] + a) & 255
            elif f == 2: line[i] = (line[i] + b) & 255
            elif f == 3: line[i] = (line[i] + ((a + b) >> 1)) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 255
        rows.append(line); prev = line
    px = []
    for line in rows:
        r = []
        for x in range(w):
            if ct == 3: r.append(plte[line[x]])
            elif ct == 0: r.append((line[x],) * 3)
            else: r.append(tuple(line[x * ch:x * ch + 3]))
        px.append(r)
    return w, h, px

def write_png(path, w, h, px):
    raw = b''.join(b'\x00' + bytes(v for p in px[y] for v in p) for y in range(h))
    def chunk(t, dd):
        c = struct.pack('>I', len(dd)) + t + dd
        return c + struct.pack('>I', zlib.crc32(t + dd) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) +
                           chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))

if __name__ == '__main__':
    cmd = sys.argv[1]
    w, h, px = read_png(sys.argv[2])
    if cmd == 'info':
        cols = {}
        for r in px:
            for p in r: cols[p] = cols.get(p, 0) + 1
        print(w, h, len(cols), 'colores')
        for c, n in sorted(cols.items(), key=lambda x: -x[1])[:20]: print(c, n)
    elif cmd == 'crop':
        x, y, cw, ch, sc = [int(v) for v in sys.argv[4:9]]
        out = []
        for yy in range(y, min(y + ch, h)):
            row = []
            for xx in range(x, min(x + cw, w)):
                row.extend([px[yy][xx]] * sc)
            out.extend([row] * sc)
        write_png(sys.argv[3], len(out[0]), len(out), out)
