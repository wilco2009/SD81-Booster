#!/usr/bin/env python3
"""Convierte imagenes JPG a ficheros IMGnnn.I16 para el visor PHOTO del SD81 Booster (modo 160x100 a 8 bpp).

Es la referencia en Python de Arduino/SD81BoosterV2_039_STM32/JPG.cpp, el codigo con el que el STM32 convierte un .JPG al abrirlo con el comando 76
(igual que bmp2i16.py lo es de BMP.cpp): el mismo resultado, byte a byte, pero escrito de otra manera. Aqui se decodifica la imagen reducida entera
y luego se orienta y se escala con los pesos de solape; el STM32 lo hace en streaming, una fila de MCU cada vez. Es un decodificador lento (Python
puro): sirve para comprobar el codigo del STM32 (claude/mqtttest/build_jpg.sh) y para convertir en el PC fotos no muy grandes.

Algoritmo (T.81, baseline secuencial de 8 bits con Huffman, y el DC de los progresivos):
  1. Cada bloque de 8x8 se reduce a la escala N/8 (N = 1, 2, 4 u 8; la croma de 4:2:0, al doble, como hace libjpeg): se toman los N x N coeficientes de menor frecuencia, ya cuantificados, y
     se les aplica una IDCT de N puntos en enteros (N = 1 es solo el DC: la media del bloque). Se elige la menor N que deja al menos tantas
     muestras como puntos de destino (160x100, o menos con --fit fit) y cuya fila de MCU cabe en el bufer del STM32 (JPG_ROWBUF).
  2. La croma que queda con menos muestras se repite (muestreo 4:4:4, 4:2:2, 4:4:0, 4:2:0, 4:1:1) y se pasa a RGB como en JFIF.
  3. Se aplica la orientacion EXIF (etiqueta 0x0112), y se escala y reduce a 256 colores como bmp2i16.py (media por area, median cut, red SCART).

Progresivos (SOF2): solo se usa el primer scan de DC con todos los componentes entrelazados (Ss = Se = 0, Ah = 0): escala 1/8 siempre, sin AC, y la croma
de 4:2:0 se repite. Si el DC trae transformada de punto (Al), se suma la mitad de lo que falta. Una imagen de menos de 1280x800 se amplia (borrosa).

Se rechazan (con el mismo texto que el STM32): aritmetico, 12 bits, CMYK, varios scans en un baseline, JPG sin tablas y progresivos sin ese scan de DC.

Uso:
    python jpg2i16.py foto1.jpg foto2.jpg ...            -> IMG001.I16, IMG002.I16 ... en la carpeta actual
    python jpg2i16.py -o SD/FOTOS --start 10 --fit crop *.jpg
"""
import argparse
import math
import os
import sys

import bmp2i16
from bmp2i16 import OUT_W, OUT_H, PIX_OFF, PIX_COUNT, FIT_STRETCH, FIT_FIT, FIT_CROP

JPG_ROWBUF = 4096            # bytes de una fila de MCU en el STM32 (JpgWork::rowbuf)
MAX_PIXELS = 128000000
MAX_SAMPLES = 16000000

ZZ = [0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5,
      12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
      35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
      58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63]


class JpgError(Exception):
    pass


def idct_matrix(n):
    """T[x][u] = 4096 * sqrt(2/N) * c(u) * cos((2x+1) u pi / 2N), c(0) = 1/sqrt(2)."""
    t = []
    for x in range(n):
        row = []
        for u in range(n):
            c = 1 / math.sqrt(2) if u == 0 else 1.0
            row.append(int(round(4096 * math.sqrt(2.0 / n) * c * math.cos((2 * x + 1) * u * math.pi / (2 * n)))))
        t.append(row)
    return t


TMAT = {n: idct_matrix(n) for n in (1, 2, 4, 8)}


def idct(coef, n):
    """coef[v][u] (ya cuantificados) -> n x n muestras 0..255 [y][x]: IDCT de n puntos, en enteros, con el factor n/8."""
    t = TMAT[n]
    shift_n = {1: 3, 2: 2, 4: 1, 8: 0}[n]
    tmp = [[(sum(t[x][u] * coef[v][u] for u in range(n)) + 32) >> 6 for x in range(n)] for v in range(n)]
    rnd = 1 << (17 + shift_n)
    out = []
    for y in range(n):
        row = []
        for x in range(n):
            s = sum(t[y][v] * tmp[v][x] for v in range(n))
            row.append(min(255, max(0, ((s + rnd) >> (18 + shift_n)) + 128)))
        out.append(row)
    return out


def sat16(v):
    return -32768 if v < -32768 else (32767 if v > 32767 else v)


# ---------------------------------------------------------------------------
# Cabecera
# ---------------------------------------------------------------------------
def build_huff(counts, vals):
    """Devuelve {(longitud, codigo): simbolo}."""
    code = 0
    k = 0
    table = {}
    for l in range(1, 17):
        for _ in range(counts[l - 1]):
            if code >= (1 << l):
                raise JpgError('BAD JPG HUFFMAN TABLE')
            table[(l, code)] = vals[k]
            k += 1
            code += 1
        code <<= 1
    return table


def parse(data):
    n = len(data)
    if n < 2 or data[0] != 0xFF or data[1] != 0xD8:
        raise JpgError('NOT A JPG FILE')
    pos = 2
    q = {}
    dc = {}
    ac = {}
    info = dict(orient=1, ri=0, sof=False, jfif=False, adobe=False, adobe_tr=0, prog=False, al=0, dcbias=0)

    def byte(i):
        if i >= n:
            raise JpgError('JPG TRUNCATED')
        return data[i]

    while True:
        # buscar el marcador
        while True:
            b = byte(pos)
            pos += 1
            if b == 0xFF:
                break
        while True:
            m = byte(pos)
            pos += 1
            if m != 0xFF:
                break
        if m == 0 or m == 1 or m == 0xD8 or 0xD0 <= m <= 0xD7:
            continue
        if m == 0xD9:
            raise JpgError('PROGRESSIVE JPG: NO DC SCAN' if info['prog'] else 'JPG HAS NO IMAGE DATA')
        ln = (byte(pos) << 8) | byte(pos + 1)
        if ln < 2:
            raise JpgError('BAD JPG HEADER')
        seg = data[pos + 2:pos + ln]            # el segmento sin la longitud (puede quedar corto si el fichero se acaba)
        seg_end = pos + ln
        if m in (0xC0, 0xC1, 0xC2):
            if info['sof']:
                raise JpgError('BAD JPG HEADER')
            s = list(seg) + [-1] * 16
            prec = s[0]
            h = (s[1] << 8) | s[2] if s[1] >= 0 and s[2] >= 0 else -1
            w = (s[3] << 8) | s[4] if s[3] >= 0 and s[4] >= 0 else -1
            nf = s[5]
            if prec != 8:
                raise JpgError('UNSUPPORTED JPG (NOT 8 BIT)')
            if h <= 0 or w <= 0:
                raise JpgError('BAD JPG SIZE')
            if nf == 4:
                raise JpgError('CMYK JPG NOT SUPPORTED')
            if nf not in (1, 3):
                raise JpgError('UNSUPPORTED JPG COMPONENTS')
            if len(seg) < 6 + 3 * nf:
                raise JpgError('BAD JPG HEADER')
            comps = []
            for i in range(nf):
                cid, hv, tq = seg[6 + 3 * i:9 + 3 * i]
                ch, cv = hv >> 4, hv & 15
                if nf == 1:
                    ch = cv = 1
                if not (1 <= ch <= 4 and 1 <= cv <= 4 and tq <= 3):
                    raise JpgError('BAD JPG HEADER')
                comps.append(dict(id=cid, h=ch, v=cv, tq=tq))
            hmax = max(c['h'] for c in comps)
            vmax = max(c['v'] for c in comps)
            if hmax * vmax > 4:
                raise JpgError('UNSUPPORTED JPG SAMPLING')
            for c in comps:
                if hmax % c['h'] or vmax % c['v']:
                    raise JpgError('UNSUPPORTED JPG SAMPLING')
            info.update(sof=True, W=w, H=h, comps=comps, hmax=hmax, vmax=vmax, prog=(m == 0xC2))
        elif 0xC9 <= m <= 0xCF:
            raise JpgError('ARITHMETIC JPG NOT SUPPORTED')
        elif m in (0xC3, 0xC5, 0xC6, 0xC7):
            raise JpgError('UNSUPPORTED JPG TYPE')
        elif m == 0xC4:
            i = 0
            while i < len(seg):
                tt = seg[i]
                tc, th = tt >> 4, tt & 15
                if tc > 1 or th > 1:
                    raise JpgError('UNSUPPORTED JPG HUFFMAN TABLE')
                counts = list(seg[i + 1:i + 17])
                if len(counts) < 16:
                    raise JpgError('BAD JPG HEADER')
                total = sum(counts)
                if total > (256 if tc else 16):
                    raise JpgError('BAD JPG HUFFMAN TABLE')
                vals = list(seg[i + 17:i + 17 + total])
                if len(vals) < total:
                    raise JpgError('BAD JPG HEADER')
                (ac if tc else dc)[th] = build_huff(counts, vals)
                i += 17 + total
        elif m == 0xDB:
            i = 0
            while i < len(seg):
                pt = seg[i]
                if (pt & 15) > 3:
                    raise JpgError('BAD JPG HEADER')
                if pt >> 4:
                    raise JpgError('UNSUPPORTED JPG (16 BIT TABLE)')
                tab = list(seg[i + 1:i + 65])
                if len(tab) < 64:
                    raise JpgError('BAD JPG HEADER')
                q[pt & 15] = tab
                i += 65
        elif m == 0xDD:
            info['ri'] = (seg[0] << 8) | seg[1] if len(seg) >= 2 else 0
        elif m == 0xE0:
            if seg[:5] == b'JFIF\0':
                info['jfif'] = True
        elif m == 0xE1:
            o = exif_orientation(seg)
            if o:
                info['orient'] = o
        elif m == 0xEE:
            if len(seg) >= 12 and seg[:5] == b'Adobe':
                info.update(adobe=True, adobe_tr=seg[11])
        elif m == 0xDA:
            if not info['sof']:
                raise JpgError('BAD JPG HEADER')
            comps = info['comps']
            ns = seg[0] if seg else -1
            if ns < 1 or ns > len(comps):
                raise JpgError('BAD JPG HEADER')
            if not info['prog'] and ns != len(comps):
                raise JpgError('MULTI-SCAN JPG NOT SUPPORTED')
            if len(seg) < 1 + 2 * ns + 3:
                raise JpgError('BAD JPG HEADER')
            order = []
            tds = []
            tas = []
            for i in range(ns):
                cs, t = seg[1 + 2 * i], seg[2 + 2 * i]
                ci = next((j for j, c in enumerate(comps) if c['id'] == cs and j not in order), None)
                if ci is None:
                    raise JpgError('BAD JPG HEADER')
                order.append(ci)
                tds.append(t >> 4)
                tas.append(t & 15)
            ss, se, a = seg[1 + 2 * ns:4 + 2 * ns]
            if info['prog']:
                # el unico scan que se usa: el primero de DC con todos los componentes entrelazados (Ss = Se = 0, Ah = 0)
                if ns != len(comps) or ss != 0 or se != 0 or (a >> 4) != 0:
                    if seg_end > n:
                        raise JpgError('JPG TRUNCATED')
                    pos = seg_end
                    continue
                if (a & 15) > 13:
                    raise JpgError('BAD JPG HEADER')
                for k, ci in enumerate(order):
                    if tds[k] > 1 or tds[k] not in dc:
                        raise JpgError('JPG HUFFMAN TABLE MISSING')
                    if comps[ci]['tq'] not in q:
                        raise JpgError('JPG QUANT TABLE MISSING')
                info['al'] = a & 15
                info['dcbias'] = (1 << (a & 15)) >> 1
            else:
                if ss != 0 or se != 63 or a != 0:
                    raise JpgError('UNSUPPORTED JPG (NOT BASELINE)')
                for k, ci in enumerate(order):
                    if tds[k] > 1 or tas[k] > 1 or tds[k] not in dc or tas[k] not in ac:
                        raise JpgError('JPG HUFFMAN TABLE MISSING')
                    if comps[ci]['tq'] not in q:
                        raise JpgError('JPG QUANT TABLE MISSING')
            for k, ci in enumerate(order):
                comps[ci]['td'] = tds[k]
                comps[ci]['ta'] = tas[k]
            info.update(order=order, scan_off=seg_end, q=q, dc=dc, ac=ac)
            if len(comps) == 3:
                if info['adobe']:
                    info['rgb'] = info['adobe_tr'] == 0
                elif info['jfif']:
                    info['rgb'] = False
                else:
                    info['rgb'] = [c['id'] for c in comps] == [ord('R'), ord('G'), ord('B')]
            else:
                info['rgb'] = False
            return info
        if seg_end > n:
            raise JpgError('JPG TRUNCATED')
        pos = seg_end


def exif_orientation(seg):
    """Etiqueta 0x0112 del IFD0 de un APP1 con EXIF, o 0."""
    if len(seg) < 14 or seg[:6] != b'Exif\0\0':
        return 0
    if seg[6:8] == b'II':
        le = True
    elif seg[6:8] == b'MM':
        le = False
    else:
        return 0

    def u16(i):
        return int.from_bytes(seg[i:i + 2], 'little' if le else 'big')

    def u32(i):
        return int.from_bytes(seg[i:i + 4], 'little' if le else 'big')
    ifd = u32(10)                              # relativo al "II"/"MM" (byte 6 del segmento)
    p = 6 + ifd
    if ifd < 8 or p + 2 > len(seg):
        return 0
    cnt = u16(p)
    p += 2
    for _ in range(cnt):
        if p + 12 > len(seg):
            return 0
        if u16(p) == 0x0112 and u16(p + 2) == 3:
            o = u16(p + 8)
            return o if 1 <= o <= 8 else 0
        p += 12
    return 0


# ---------------------------------------------------------------------------
# Decodificacion
# ---------------------------------------------------------------------------
def split_scan(data, off):
    """Los datos de la scan separados por los marcadores RSTn y sin los FF00: lista de bytes."""
    segs = []
    cur = bytearray()
    i = off
    n = len(data)
    while i < n:
        b = data[i]
        if b != 0xFF:
            cur.append(b)
            i += 1
            continue
        j = i + 1
        while j < n and data[j] == 0xFF:
            j += 1
        if j >= n:
            break
        m = data[j]
        if m == 0:
            cur.append(0xFF)
            i = j + 1
            continue
        if 0xD0 <= m <= 0xD7:                  # RSTn: otro intervalo
            segs.append(bytes(cur))
            cur = bytearray()
            i = j + 1
            continue
        break                                   # EOI u otro marcador: se acaba
    segs.append(bytes(cur))
    return segs


class Bits:
    def __init__(self, b):
        self.v = int.from_bytes(b, 'big') if b else 0
        self.n = len(b) * 8
        self.pos = 0

    def bit(self):
        if self.pos >= self.n:
            raise JpgError('JPG TRUNCATED')
        r = (self.v >> (self.n - 1 - self.pos)) & 1
        self.pos += 1
        return r

    def bits(self, k):
        r = 0
        for _ in range(k):
            r = (r << 1) | self.bit()
        return r


def huff(bits, table):
    code = 0
    for l in range(1, 17):
        code = (code << 1) | bits.bit()
        s = table.get((l, code))
        if s is not None:
            return s
    raise JpgError('BAD JPG DATA')


def extend(v, s):
    return v if v >= (1 << (s - 1)) else v - (1 << s) + 1


def comp_scale(info, c, n):
    """(lado de los bloques reducidos del componente, muestras de la rejilla que caen en una suya en x, en y). Como libjpeg: un componente
    submuestreado a la mitad en los dos ejes (la croma de 4:2:0) se decodifica al doble de escala, para no repetirla."""
    rh, rv = info['hmax'] // c['h'], info['vmax'] // c['v']
    k = n
    while not info['prog'] and k < 8 and rh % 2 == 0 and rv % 2 == 0:
        k *= 2
        rh //= 2
        rv //= 2
    return k, rh, rv


def choose_scale(info, fit):
    """La menor N (1, 2, 4, 8) que deja al menos tantas muestras como puntos de destino y cuya fila de MCU cabe en el STM32."""
    W, H = info['W'], info['H']
    swap = info['orient'] >= 5
    mcu_cols = -(-W // (8 * info['hmax']))
    best = 0
    for n in ((1,) if info['prog'] else (1, 2, 4, 8)):
        if mcu_cols * sum(c['h'] * c['v'] * comp_scale(info, c, n)[0] ** 2 for c in info['comps']) > JPG_ROWBUF:
            break
        best = n
        if info['prog']:
            break
        ws, hs = (W * n + 7) // 8, (H * n + 7) // 8
        g = bmp2i16.geometry(hs, ws, fit) if swap else bmp2i16.geometry(ws, hs, fit)
        if g['sw'] >= g['dw'] and g['sh'] >= g['dh']:
            break
    if not best:
        raise JpgError('JPG TOO WIDE')
    return best


def decode_scaled(data, info, n):
    """La imagen reducida a la escala n/8 sin orientar: filas de (r, g, b), de Hs filas de Ws pixeles."""
    W, H = info['W'], info['H']
    comps = info['comps']
    hmax, vmax = info['hmax'], info['vmax']
    mcu_cols = -(-W // (8 * hmax))
    mcu_rows = -(-H // (8 * vmax))
    for c in comps:
        c['n'], c['hr'], c['vr'] = comp_scale(info, c, n)
        c['plane'] = [[0] * (mcu_cols * c['h'] * c['n']) for _ in range(mcu_rows * c['v'] * c['n'])]
    segs = split_scan(data, info['scan_off'])
    ri = info['ri']
    seg_i = 0
    bits = Bits(segs[0])
    pred = [0] * len(comps)
    q = info['q']
    total = mcu_cols * mcu_rows
    for mcu in range(total):
        if ri and mcu and mcu % ri == 0:
            seg_i += 1
            if seg_i >= len(segs):
                raise JpgError('JPG TRUNCATED')
            bits = Bits(segs[seg_i])
            pred = [0] * len(comps)
        mx, my = mcu % mcu_cols, mcu // mcu_cols
        for ci in info['order']:
            c = comps[ci]
            qt = q[c['tq']]
            cn = c['n']
            for by in range(c['v']):
                for bx in range(c['h']):
                    s = huff(bits, info['dc'][c['td']])
                    if s > 15:
                        raise JpgError('BAD JPG DATA')
                    diff = extend(bits.bits(s), s) if s else 0
                    pred[ci] = max(-32768, min(32767, pred[ci] + diff))
                    coef = [[0] * cn for _ in range(cn)]
                    coef[0][0] = sat16((pred[ci] * (1 << info['al']) + info['dcbias']) * qt[0])
                    k = 1
                    while k < 64 and not info['prog']:
                        sym = huff(bits, info['ac'][c['ta']])
                        r, ssz = sym >> 4, sym & 15
                        if not ssz:
                            if r != 15:
                                break
                            k += 16
                            continue
                        k += r
                        v = extend(bits.bits(ssz), ssz)
                        if k < 64:
                            nat = ZZ[k]
                            row, col = nat >> 3, nat & 7
                            if row < cn and col < cn:
                                coef[row][col] = sat16(v * qt[k])
                        k += 1
                    px = idct(coef, cn)
                    x0 = (mx * c['h'] + bx) * cn
                    y0 = (my * c['v'] + by) * cn
                    for y in range(cn):
                        c['plane'][y0 + y][x0:x0 + cn] = px[y]
    ws, hs = (W * n + 7) // 8, (H * n + 7) // 8
    rows = []
    ncomp = len(comps)
    for y in range(hs):
        row = []
        for x in range(ws):
            smp = [c['plane'][y // c['vr']][x // c['hr']] for c in comps]
            if ncomp == 1:
                row.append((smp[0],) * 3)
            elif info['rgb']:
                row.append(tuple(smp))
            else:
                yy, cb, cr = smp[0], smp[1] - 128, smp[2] - 128
                row.append((min(255, max(0, yy + ((91881 * cr + 32768) >> 16))),
                            min(255, max(0, yy + ((-22554 * cb - 46802 * cr + 32768) >> 16))),
                            min(255, max(0, yy + ((116130 * cb + 32768) >> 16)))))
        rows.append(row)
    return rows


def orient_image(rows, o):
    """Aplica la orientacion EXIF: pixel (x, y) de la imagen orientada <- pixel (xs, ys) de la almacenada."""
    hj, wj = len(rows), len(rows[0])
    if o == 1:
        return rows
    ow, oh = (hj, wj) if o >= 5 else (wj, hj)
    out = []
    for y in range(oh):
        row = []
        for x in range(ow):
            if o == 2:
                xs, ys = wj - 1 - x, y
            elif o == 3:
                xs, ys = wj - 1 - x, hj - 1 - y
            elif o == 4:
                xs, ys = x, hj - 1 - y
            elif o == 5:
                xs, ys = y, x
            elif o == 6:
                xs, ys = y, hj - 1 - x
            elif o == 7:
                xs, ys = wj - 1 - y, hj - 1 - x
            else:
                xs, ys = wj - 1 - y, x
            row.append(rows[ys][xs])
        out.append(row)
    return out


def area_scale(rows, g):
    """Media por area (pesos enteros de solape) del recorte de rows al rectangulo de destino: dh filas de dw * 3 valores."""
    sx0, sy0, sw, sh, dw, dh = g['sx0'], g['sy0'], g['sw'], g['sh'], g['dw'], g['dh']

    def wgt(i, d, nd, ns):
        return max(0, min(nd * (i + 1), (d + 1) * ns) - max(nd * i, d * ns))
    area = sw * sh
    half = area // 2
    # horizontal: para cada fila del recorte, dw * 3 sumas
    hrows = []
    for yy in range(sh):
        src = rows[sy0 + yy][sx0:sx0 + sw]
        hr = [0] * (dw * 3)
        for dx in range(dw):
            lo = (dx * sw) // dw
            hi = ((dx + 1) * sw - 1) // dw
            for xx in range(lo, hi + 1):
                wx = wgt(xx, dx, dw, sw)
                if wx:
                    p = src[xx]
                    hr[dx * 3] += wx * p[0]
                    hr[dx * 3 + 1] += wx * p[1]
                    hr[dx * 3 + 2] += wx * p[2]
        hrows.append(hr)
    out = []
    for dy in range(dh):
        lo = (dy * sh) // dh
        hi = ((dy + 1) * sh - 1) // dh
        acc = [0] * (dw * 3)
        for yy in range(lo, hi + 1):
            wy = wgt(yy, dy, dh, sh)
            if wy:
                hr = hrows[yy]
                for i in range(dw * 3):
                    acc[i] += wy * hr[i]
        out.append([(a + half) // area for a in acc])
    return out


def finish(dest, g):
    """dh filas de dw * 3 valores (ya escaladas) -> .I16: paleta, color mas cercano y cabecera (como bmp2i16.convert)."""
    dw, dh = g['dw'], g['dh']
    hist = [0] * 4096
    for row in dest:
        for dx in range(dw):
            hist[((row[dx * 3] >> 4) << 8) | ((row[dx * 3 + 1] >> 4) << 4) | (row[dx * 3 + 2] >> 4)] += 1
    hist[0] += PIX_COUNT - dw * dh
    pal, npal = bmp2i16.build_palette(hist)
    out = bytearray(bmp2i16.BMP_I16_SIZE)
    out[0:4] = b'I160'
    for j, (r, gg, b) in enumerate(pal):
        nr, ng, nb, nw = bmp2i16.solve_int(r, gg, b)
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
    err_sum = 0
    for dy in range(dh):
        o = PIX_OFF + (g['dy0'] + dy) * OUT_W + g['dx0']
        row = dest[dy]
        for dx in range(dw):
            r, gg, b = row[dx * 3], row[dx * 3 + 1], row[dx * 3 + 2]
            j = nearest(r, gg, b)
            out[o + dx] = j
            err_sum += abs(r - pal[j][0]) + abs(gg - pal[j][1]) + abs(b - pal[j][2])
    convert.info = dict(pal=pal, npal=npal, geo=g, black=black, mae=err_sum / (3.0 * dw * dh))
    return bytes(out)


def convert(data, fit=FIT_STRETCH):
    """JPG (bytes) -> .I16 (16516 bytes)."""
    info = parse(data)
    if info['W'] * info['H'] > MAX_PIXELS:
        raise JpgError('JPG TOO LARGE')
    n = choose_scale(info, fit)
    scaled = decode_scaled(data, info, n)
    img = orient_image(scaled, info['orient'])
    g = bmp2i16.geometry(len(img[0]), len(img), fit)
    if g['sw'] * g['sh'] > MAX_SAMPLES:
        raise JpgError('JPG TOO LARGE')
    dest = area_scale(img, g)
    convert.n = n
    return finish(dest, g)


def main():
    ap = argparse.ArgumentParser(description='JPG -> IMGnnn.I16 (visor PHOTO)')
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
        except JpgError as e:
            print('%s: %s' % (f, e))
            return 1
        name = os.path.join(a.out, 'IMG%03d.I16' % n)
        open(name, 'wb').write(i16)
        print('%s -> %s' % (f, name))
        n += 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
