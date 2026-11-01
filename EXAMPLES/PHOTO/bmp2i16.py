#!/usr/bin/env python3
"""Convierte imagenes BMP a ficheros IMGnnn.I16 para el visor PHOTO del SD81 Booster (modo 160x100 a 8 bpp).

Es la referencia en Python de Arduino/SD81BoosterV2_039_STM32/BMP.cpp, el codigo con el que el STM32 convierte un .BMP al abrirlo con el comando 76
(el visor y el explorador lo leen como si fuera un .I16): el mismo algoritmo, en enteros, para que el PC y el STM32 den los mismos bytes.
Sirve para comprobar el codigo del STM32 (claude/mqtttest/build_bmp.sh) y tambien para convertir en el PC.

Algoritmo:
  1. Escalado con la media por area (pesos enteros exactos) a 160x100:
        --fit stretch  estira para llenar todo           (por omision)
        --fit fit      conserva la proporcion (pantalla 4:3) y rellena de negro
        --fit crop     conserva la proporcion y recorta lo que sobra
  2. Histograma de 4+4+4 bits de la imagen escalada y paleta de 256 colores por cortes de mediana; cada pixel va al color mas cercano.
  3. Cada color de la paleta pasa a las cuatro densidades (R, G, B, BRIGHT) de la red de la SCART (como pal160.py, pero en enteros).

BMP admitidos: sin compresion (o BI_BITFIELDS de 32 bits con las mascaras normales) de 1, 4, 8, 24 y 32 bits, de abajo arriba o de arriba abajo.

Uso:
    python bmp2i16.py foto1.bmp foto2.bmp ...            -> IMG001.I16, IMG002.I16 ... en la carpeta actual
    python bmp2i16.py -o SD/FOTOS --start 10 --fit fit *.bmp
"""
import argparse
import os
import struct
import sys

OUT_W, OUT_H = 160, 100
PIX_OFF = 516
PIX_COUNT = OUT_W * OUT_H
MAX_DIM = 16384
MAX_PIXELS = 16000000
STEP = 707612       # un paso de la densidad de un canal: 255/15 de la tension relativa (CA/VMAX) por 65536
LIFTU = 406500      # un paso de la densidad de BRIGHT (CB/VMAX)

FIT_STRETCH, FIT_FIT, FIT_CROP = 0, 1, 2


class BmpError(Exception):
    pass


def parse(data):
    """Cabecera: devuelve un dict con W, H, bottom_up, bpp, stride, data_off y la paleta (lista de (r, g, b))."""
    n = len(data)
    if n < 26 or data[0:2] != b'BM':
        raise BmpError('NOT A BMP FILE')
    head = data[:70]
    nh = len(head)
    data_off, dib = struct.unpack('<II', head[10:18])
    comp = clr_used = 0
    if dib == 12:
        w, h, planes, bpp = struct.unpack('<HHHH', head[18:26])
        entsz = 3
    elif 40 <= dib <= 124 and nh >= 54:
        w, h, planes, bpp = struct.unpack('<iiHH', head[18:30])
        comp = struct.unpack('<I', head[30:34])[0]
        clr_used = struct.unpack('<I', head[46:50])[0]
        entsz = 4
    else:
        raise BmpError('BAD BMP HEADER')
    if planes != 1 or w <= 0 or h == 0:
        raise BmpError('BAD BMP HEADER')
    bottom_up = h > 0
    H = abs(h)
    if w > MAX_DIM or H > MAX_DIM:
        raise BmpError('BMP TOO LARGE')
    if bpp not in (1, 4, 8, 24, 32):
        raise BmpError('UNSUPPORTED BMP DEPTH')
    if comp != 0:
        ok = comp == 3 and bpp == 32 and nh >= 66 and struct.unpack('<III', head[54:66]) == (0x00FF0000, 0x0000FF00, 0x000000FF)
        if not ok:
            raise BmpError('UNSUPPORTED BMP (COMPRESSED)')
    stride = ((w * bpp + 31) // 32) * 4
    lastrow = (w * bpp + 7) // 8
    if data_off + stride * (H - 1) + lastrow > n:
        raise BmpError('BMP TRUNCATED')
    pal = [(0, 0, 0)] * 256
    if bpp <= 8:
        ncol = clr_used if clr_used else (1 << bpp)
        ncol = min(ncol, 1 << bpp)
        pal = list(pal)
        pos = 14 + dib
        for i in range(ncol):
            e = data[pos:pos + entsz]
            if len(e) != entsz:
                raise BmpError('BMP TRUNCATED')
            pal[i] = (e[2], e[1], e[0])
            pos += entsz
    return dict(W=w, H=H, bottom_up=bottom_up, bpp=bpp, stride=stride, data_off=data_off, spal=pal)


def geometry(W, H, fit):
    sx0 = sy0 = 0
    sw, sh = W, H
    dx0 = dy0 = 0
    dw, dh = OUT_W, OUT_H
    wide = W * 3 >= H * 4
    if fit == FIT_FIT:
        if wide:
            dh = (800 * H + 3 * W) // (6 * W)
            dh = max(1, min(OUT_H, dh))
        else:
            dw = (240 * W + H) // (2 * H)
            dw = max(1, min(OUT_W, dw))
        dx0 = (OUT_W - dw) // 2
        dy0 = (OUT_H - dh) // 2
    elif fit == FIT_CROP:
        if wide:
            sw = min(W, (8 * H + 3) // 6)
            sw = max(1, sw)
            sx0 = (W - sw) // 2
        else:
            sh = min(H, (3 * W + 2) // 4)
            sh = max(1, sh)
            sy0 = (H - sh) // 2
    return dict(sx0=sx0, sy0=sy0, sw=sw, sh=sh, dx0=dx0, dy0=dy0, dw=dw, dh=dh)


def read_row(data, bi, g, t):
    """Suma horizontal de la fila t del fichero: lista de dw * 3 enteros."""
    dw, sw = g['dw'], g['sw']
    bpp = bi['bpp']
    spal = bi['spal']
    row = bi['data_off'] + t * bi['stride']
    H = [0] * (dw * 3)
    dx = 0
    rem_d = sw
    for i in range(g['sx0'], g['sx0'] + sw):
        pb = row + ((i * bpp) >> 3)
        if bpp >= 24:
            b, gg, r = data[pb], data[pb + 1], data[pb + 2]
        else:
            byte = data[pb]
            if bpp == 8:
                idx = byte
            elif bpp == 4:
                idx = (byte & 15) if (i & 1) else (byte >> 4)
            else:
                idx = (byte >> (7 - (i & 7))) & 1
            r, gg, b = spal[idx]
        rem_s = dw
        while rem_s:
            m = rem_s if rem_s < rem_d else rem_d
            k = dx * 3
            H[k] += m * r
            H[k + 1] += m * gg
            H[k + 2] += m * b
            rem_s -= m
            rem_d -= m
            if not rem_d:
                dx += 1
                rem_d = sw
    return H


def scale_pass(data, bi, g, sink):
    """Llama a sink(dy, filaRGB) con cada fila del destino terminada (dy de 0 a dh - 1)."""
    sh, sw, dw, dh = g['sh'], g['sw'], g['dw'], g['dh']
    area = sw * sh
    half = area // 2
    n3 = dw * 3
    A = [0] * n3
    bu = bi['bottom_up']
    t0 = (bi['H'] - g['sy0'] - sh) if bu else g['sy0']
    done = 0
    for k in range(sh):
        ys = (sh - 1 - k) if bu else k
        H = read_row(data, bi, g, t0 + k)
        s0, s1 = dh * ys, dh * (ys + 1)
        first, last = s0 // sh, (s1 - 1) // sh
        for q in range(last - first + 1):
            dy = (last - q) if bu else (first + q)
            d0, d1 = dy * sh, (dy + 1) * sh
            m = min(s1, d1) - max(s0, d0)
            for i in range(n3):
                A[i] += m * H[i]
            complete = (s0 <= d0) if bu else (s1 >= d1)
            if complete:
                sink(dy, [(a + half) // area for a in A])
                A = [0] * n3
                done += 1
    if done != dh:
        raise BmpError('INTERNAL ERROR')


def box_scan(hist, box):
    mn = [15, 15, 15]
    mx = [0, 0, 0]
    cnt = nb = 0
    for r in range(box['lo'][0], box['hi'][0] + 1):
        for g in range(box['lo'][1], box['hi'][1] + 1):
            for b in range(box['lo'][2], box['hi'][2] + 1):
                n = hist[(r << 8) | (g << 4) | b]
                if not n:
                    continue
                cnt += n
                nb += 1
                for a, v in enumerate((r, g, b)):
                    if v < mn[a]:
                        mn[a] = v
                    if v > mx[a]:
                        mx[a] = v
    if nb:
        box['lo'] = mn
        box['hi'] = mx
    box['cnt'] = cnt
    box['nbins'] = nb


def build_palette(hist):
    boxes = [dict(lo=[0, 0, 0], hi=[15, 15, 15], cnt=0, nbins=0)]
    box_scan(hist, boxes[0])
    order = (1, 0, 2)
    wt = (3, 4, 2)
    while len(boxes) < 256:
        pick = -1
        for i, b in enumerate(boxes):
            if b['nbins'] >= 2 and (pick < 0 or b['cnt'] > boxes[pick]['cnt']):
                pick = i
        if pick < 0:
            break
        b = boxes[pick]
        axis, bests = 0, -1
        for a in order:
            s = (b['hi'][a] - b['lo'][a]) * wt[a]
            if s > bests:
                bests, axis = s, a
        m = [0] * 16
        for r in range(b['lo'][0], b['hi'][0] + 1):
            for g in range(b['lo'][1], b['hi'][1] + 1):
                for bl in range(b['lo'][2], b['hi'][2] + 1):
                    m[(r, g, bl)[axis]] += hist[(r << 8) | (g << 4) | bl]
        half = (b['cnt'] + 1) // 2
        cum = 0
        cut = b['hi'][axis]
        for v in range(b['lo'][axis], b['hi'][axis] + 1):
            cum += m[v]
            if cum >= half:
                cut = v
                break
        if cut == b['hi'][axis]:
            cut -= 1
        nbx = dict(lo=list(b['lo']), hi=list(b['hi']), cnt=0, nbins=0)
        b['hi'] = list(b['hi'])
        b['hi'][axis] = cut
        nbx['lo'][axis] = cut + 1
        box_scan(hist, b)
        box_scan(hist, nbx)
        boxes.append(nbx)
    pal = []
    for b in boxes:
        s = [0, 0, 0]
        for r in range(b['lo'][0], b['hi'][0] + 1):
            for g in range(b['lo'][1], b['hi'][1] + 1):
                for bl in range(b['lo'][2], b['hi'][2] + 1):
                    n = hist[(r << 8) | (g << 4) | bl]
                    s[0] += n * (r * 17)
                    s[1] += n * (g * 17)
                    s[2] += n * (bl * 17)
        cnt = b['cnt'] if b['cnt'] else 1
        pal.append(tuple((x + cnt // 2) // cnt for x in s))
    n = len(pal)
    pal += [(0, 0, 0)] * (256 - n)
    return pal, n


def solve_int(r, g, b):
    """Un color RGB de 0 a 255 -> (nr, ng, nb, nw) de 0 a 15, como pal160.solve pero en enteros."""
    best = None
    for w in range(16):
        lift = LIFTU * w
        e = 0
        ns = []
        for ch in (r, g, b):
            t = ch * 65536
            num = t - lift
            n = 0 if num <= 0 else (num + STEP // 2) // STEP
            if n > 15:
                n = 15
            err = n * STEP + lift - t
            e += err * err
            ns.append(n)
        if best is None or e < best[0]:
            best = (e, ns[0], ns[1], ns[2], w)
    return best[1:]


def convert(data, fit=FIT_STRETCH):
    """BMP (bytes) -> .I16 (16516 bytes)."""
    bi = parse(data)
    if bi['W'] * bi['H'] > MAX_PIXELS:
        raise BmpError('BMP TOO LARGE')
    g = geometry(bi['W'], bi['H'], fit)
    if g['sw'] * g['sh'] > MAX_PIXELS:
        raise BmpError('BMP TOO LARGE')
    hist = [0] * 4096

    def sink_hist(dy, rgb):
        for dx in range(g['dw']):
            p = rgb[dx * 3:dx * 3 + 3]
            hist[((p[0] >> 4) << 8) | ((p[1] >> 4) << 4) | (p[2] >> 4)] += 1

    scale_pass(data, bi, g, sink_hist)
    hist[0] += PIX_COUNT - g['dw'] * g['dh']
    pal, npal = build_palette(hist)

    out = bytearray(BMP_I16_SIZE)
    out[0:4] = b'I160'
    for j, (r, gg, b) in enumerate(pal):
        nr, ng, nb, nw = solve_int(r, gg, b)
        out[4 + 2 * j] = (ng << 4) | nb
        out[5 + 2 * j] = (nw << 4) | nr

    cache = {}

    def nearest(r, gg, b):
        k = (r, gg, b)
        v = cache.get(k)
        if v is not None:
            return v
        best, bd = 0, 0x7FFFFFFF
        for j in range(npal):
            pr, pg, pb = pal[j]
            dr, dg, db = r - pr, gg - pg, b - pb
            d = 3 * dr * dr + 4 * dg * dg + 2 * db * db
            if d < bd:
                bd, best = d, j
        cache[k] = best
        return best

    black = nearest(0, 0, 0)
    out[PIX_OFF:PIX_OFF + PIX_COUNT] = bytes([black]) * PIX_COUNT

    err_sum = [0]

    def sink_map(dy, rgb):
        o = PIX_OFF + (g['dy0'] + dy) * OUT_W + g['dx0']
        for dx in range(g['dw']):
            r, gg, b = rgb[dx * 3], rgb[dx * 3 + 1], rgb[dx * 3 + 2]
            j = nearest(r, gg, b)
            out[o + dx] = j
            err_sum[0] += abs(r - pal[j][0]) + abs(gg - pal[j][1]) + abs(b - pal[j][2])

    scale_pass(data, bi, g, sink_map)
    # datos para las pruebas (no forman parte del resultado)
    convert.info = dict(pal=pal, npal=npal, geo=g, black=black, mae=err_sum[0] / (3.0 * g['dw'] * g['dh']))
    return bytes(out)


BMP_I16_SIZE = 16516


def main():
    ap = argparse.ArgumentParser(description='BMP -> IMGnnn.I16 (visor PHOTO)')
    ap.add_argument('files', nargs='+')
    ap.add_argument('-o', '--out', default='.', help='carpeta de salida')
    ap.add_argument('--start', type=int, default=1, help='numero del primer IMGnnn.I16 (1-999)')
    ap.add_argument('--fit', choices=('stretch', 'fit', 'crop'), default='stretch')
    a = ap.parse_args()
    fit = {'stretch': FIT_STRETCH, 'fit': FIT_FIT, 'crop': FIT_CROP}[a.fit]
    os.makedirs(a.out, exist_ok=True)
    n = a.start
    for f in a.files:
        if n > 999:
            print('demasiadas imagenes (maximo IMG999)')
            return 1
        try:
            i16 = convert(open(f, 'rb').read(), fit)
        except BmpError as e:
            print('%s: %s' % (f, e))
            return 1
        name = os.path.join(a.out, 'IMG%03d.I16' % n)
        open(name, 'wb').write(i16)
        print('%s -> %s' % (f, name))
        n += 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
