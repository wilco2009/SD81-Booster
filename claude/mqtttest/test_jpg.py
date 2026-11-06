#!/usr/bin/env python3
"""Pruebas de la conversion de JPG a .I16 (Arduino/SD81BoosterV2_039_STM32/JPG.cpp) en el PC.

Genera JPG de todos los tipos que admite (con Pillow y con un codificador propio para los muestreos que Pillow no hace), los convierte con la
referencia en Python (EXAMPLES/PHOTO/jpg2i16.py, que decodifica la imagen reducida entera) y con el mismo codigo del STM32 compilado para el PC
(test_jpg.exe, que la acumula en streaming), y comprueba:
  - que las dos salidas son IGUALES byte a byte (o dan el mismo error): muestreo 4:4:4 / 4:2:2 / 4:4:0 / 4:2:0 / 4:1:1 y gris, con y sin
    marcadores de reinicio, tablas Huffman optimizadas, todas las escalas (1/8, 1/4, 1/2 y 1/1), las ocho orientaciones EXIF, tamanos raros,
    los tres modos de colocacion y ficheros estropeados;
  - contra Pillow, que no comparte codigo con ninguno de los dos: la imagen reducida (la media de cada bloque) y la orientacion EXIF;
  - que se rechazan con el motivo correcto el progresivo, el CMYK, el truncado, etc.;
  - la calidad frente al camino del BMP (la foto decodificada entera por Pillow y convertida por bmp2i16.py);
  - que el STM32 lee el fichero siempre hacia delante (salvo el principio de cada pasada) y cuanta memoria usa.
Necesita Pillow y numpy.
Uso:  python test_jpg.py [carpeta_de_trabajo] [ruta de test_jpg.exe]     (lo llama build_jpg.sh)
"""
import io
import math
import os
import random
import struct
import subprocess
import sys

import numpy as np
from PIL import Image, ImageOps

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'EXAMPLES', 'PHOTO'))
import bmp2i16
import jpg2i16

WORK = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'tmp_jpg'))
EXE = os.path.abspath(sys.argv[2] if len(sys.argv) > 2 else os.path.join(WORK, 'test_jpg.exe'))

results = []
FAST = bool(os.environ.get('JPG_FAST'))      # JPG_FAST=1: salta las comparaciones largas con la referencia (solo errores, Pillow y calidad)


def check(name, ok, detail=''):
    results.append(ok)
    print('  %s %s%s' % ('ok  ' if ok else 'FAIL', name, (': ' + detail) if detail else ''))
    sys.stdout.flush()


def clamp(v):
    return 0 if v < 0 else (255 if v > 255 else int(v))


def photo_array(w, h):
    """Una imagen sintetica con degradados, un disco, una barra oscura, un borde duro y algo de ruido determinista."""
    ys, xs = np.mgrid[0:h, 0:w]
    u, v = xs / float(w), ys / float(h)
    r = 128 + 100 * np.sin(6.0 * u + 2.0 * v)
    g = 128 + 90 * np.sin(4.0 * v - 3.0 * u + 1.0)
    b = 100 + 120 * u * (1 - v)
    disc = (u - 0.65) ** 2 + (v - 0.4) ** 2 < 0.03
    r[disc], g[disc], b[disc] = 250, 220, 40
    bar = (v > 0.1) & (v < 0.18)
    r[bar], g[bar], b[bar] = 20, 20, 60
    edge = (u > 0.2) & (u < 0.25) & (v > 0.5)
    r[edge], g[edge], b[edge] = 255, 255, 255
    n = ((xs * 7919 + ys * 104729) % 23) - 11
    a = np.stack([r + n, g + n, b - n], axis=2)
    return np.clip(a, 0, 255).astype(np.uint8)


def exif_bytes(o):
    ex = Image.Exif()
    ex[0x0112] = o
    return ex.tobytes()


def pil_jpeg(arr, gray=False, **kw):
    im = Image.fromarray(arr)
    if gray:
        im = im.convert('L')
    b = io.BytesIO()
    im.save(b, 'JPEG', **kw)
    return b.getvalue()


# ---------------------------------------------------------------------------
# Un codificador baseline propio, para los muestreos que Pillow no hace (4:4:0, 4:1:1, h = 3...), RGB sin YCbCr y gris con h, v > 1
# ---------------------------------------------------------------------------
def segments(data):
    """Los segmentos antes de SOS: lista de (marcador, contenido)."""
    out = []
    pos = 2
    while True:
        assert data[pos] == 0xFF
        m = data[pos + 1]
        ln = struct.unpack('>H', data[pos + 2:pos + 4])[0]
        out.append((m, data[pos + 4:pos + 2 + ln]))
        pos += 2 + ln
        if m == 0xDA:
            return out


COS = np.array([[(math.sqrt(1 / 8) if u == 0 else math.sqrt(2 / 8)) * math.cos((2 * x + 1) * u * math.pi / 16) for x in range(8)] for u in range(8)])


def enc_tables(quality):
    """Tablas de cuantificacion (zigzag) y Huffman estandar (como las saca libjpeg) de un JPG de Pillow."""
    d = pil_jpeg(photo_array(16, 16), quality=quality, subsampling=0)
    q = []
    huff = {}
    for m, s in segments(d):
        if m == 0xDB:
            i = 0
            while i < len(s):
                q.append(list(s[i + 1:i + 65]))
                i += 65
        elif m == 0xC4:
            i = 0
            while i < len(s):
                tt = s[i]
                counts = list(s[i + 1:i + 17])
                tot = sum(counts)
                vals = list(s[i + 17:i + 17 + tot])
                code = 0
                k = 0
                enc = {}
                for l in range(1, 17):
                    for _ in range(counts[l - 1]):
                        enc[vals[k]] = (code, l)
                        k += 1
                        code += 1
                    code <<= 1
                huff[(tt >> 4, tt & 15)] = (enc, s[i:i + 17 + tot])
                i += 17 + tot
    return q, huff


class BitWriter:
    def __init__(self):
        self.out = bytearray()
        self.acc = 0
        self.n = 0

    def put(self, code, l):
        self.acc = (self.acc << l) | code
        self.n += l
        while self.n >= 8:
            b = (self.acc >> (self.n - 8)) & 0xFF
            self.out.append(b)
            if b == 0xFF:
                self.out.append(0)
            self.n -= 8
        self.acc &= (1 << self.n) - 1

    def flush(self):
        if self.n:
            self.put((1 << (8 - self.n)) - 1, 8 - self.n)


def encode(arr, samp, quality=85, ri=0, gray=False, rgb=False, adobe=False, jfif=True, ids=(1, 2, 3), exif=None, crop_dims=None, prog=None):
    """arr: imagen RGB (alto, ancho, 3). samp: ((h, v) de cada componente). Devuelve un JPG baseline.
    prog: lista de scans para un JPG progresivo (SOF2): ('dc', al) = DC entrelazado de todos los componentes con transformada de punto al (el unico
    que lleva datos de verdad), y scans de relleno con bytes al azar: ('ac', ci, ss, se), ('dcref', al), ('dcsep', ci)."""
    qz, huff = enc_tables(quality)
    ncomp = 1 if gray else 3
    h_img, w_img = arr.shape[:2]
    hmax = max(h for h, v in samp[:ncomp])
    vmax = max(v for h, v in samp[:ncomp])
    if ncomp == 1:
        hmax = vmax = 1
    if gray:
        planes = [np.asarray(Image.fromarray(arr).convert('L'), dtype=np.float64)]
    elif rgb:
        planes = [arr[:, :, i].astype(np.float64) for i in range(3)]
    else:
        ycc = np.asarray(Image.fromarray(arr).convert('YCbCr'), dtype=np.float64)
        planes = [ycc[:, :, i] for i in range(3)]
    mcu_w, mcu_h = 8 * hmax, 8 * vmax
    pw, ph = -(-w_img // mcu_w) * mcu_w, -(-h_img // mcu_h) * mcu_h
    blocks = []
    for ci in range(ncomp):
        hh, vv = (1, 1) if ncomp == 1 else samp[ci]
        p = np.pad(planes[ci], ((0, ph - h_img), (0, pw - w_img)), mode='edge')
        fx, fy = hmax // hh, vmax // vv
        if fx > 1 or fy > 1:
            p = p.reshape(ph // fy, fy, pw // fx, fx).mean(axis=(1, 3))
        blocks.append(p)
    out = bytearray(b'\xff\xd8')
    if jfif:
        out += b'\xff\xe0' + struct.pack('>H', 16) + b'JFIF\0\x01\x01\0\0\x01\0\x01\0\0'
    if adobe:
        out += b'\xff\xee' + struct.pack('>H', 14) + b'Adobe' + struct.pack('>HHHB', 100, 0, 0, 0 if rgb else 1)
    if exif:
        body = b'Exif\0\0' + exif[6:] if exif[:6] != b'Exif\0\0' else exif
        out += b'\xff\xe1' + struct.pack('>H', len(body) + 2) + body
    qs = [0, 1, 1] if ncomp == 3 else [0]
    for t in sorted(set(qs)):
        out += b'\xff\xdb' + struct.pack('>H', 67) + bytes([t]) + bytes(qz[t])
    sof = struct.pack('>BHHB', 8, h_img, w_img, ncomp)
    for ci in range(ncomp):
        hh, vv = (1, 1) if ncomp == 1 else samp[ci]
        sof += bytes([ids[ci], (hh << 4) | vv, qs[ci]])
    out += (b'\xff\xc2' if prog is not None else b'\xff\xc0') + struct.pack('>H', len(sof) + 2) + sof
    for key in sorted(huff):
        if (key[1] == 0) or ncomp == 3:
            out += b'\xff\xc4' + struct.pack('>H', len(huff[key][1]) + 2) + huff[key][1]
    if ri:
        out += b'\xff\xdd' + struct.pack('>HH', 4, ri)
    def sos_header(cis, ss, se, ah_al):
        sos = bytes([len(cis)])
        for ci in cis:
            sos += bytes([ids[ci], (qs[ci] << 4) | qs[ci]])
        sos += bytes([ss, se, ah_al])
        return b'\xff\xda' + struct.pack('>H', len(sos) + 2) + sos

    zz = jpg2i16.ZZ
    mcu_cols, mcu_rows = pw // mcu_w, ph // mcu_h

    def scan_data(dc_only, al):
        bw = BitWriter()
        pred = [0] * ncomp
        count = 0
        rst = 0
        for my in range(mcu_rows):
            for mx in range(mcu_cols):
                if ri and count and count % ri == 0:
                    bw.flush()
                    bw.out += bytes([0xFF, 0xD0 + rst])
                    rst = (rst + 1) & 7
                    pred = [0] * ncomp
                count += 1
                for ci in range(ncomp):
                    hh, vv = (1, 1) if ncomp == 1 else samp[ci]
                    tq = qs[ci]
                    dce, ace = huff[(0, tq)][0], huff[(1, tq)][0]
                    for by in range(vv):
                        for bx in range(hh):
                            y0, x0 = (my * vv + by) * 8, (mx * hh + bx) * 8
                            blk = blocks[ci][y0:y0 + 8, x0:x0 + 8] - 128
                            F = COS @ blk @ COS.T
                            qq = np.array(qz[tq])
                            natq = np.zeros(64)
                            for k in range(64):
                                natq[zz[k]] = qq[k]
                            Q = np.round(F.reshape(64) / natq).astype(int)
                            zq = [int(Q[zz[k]]) for k in range(64)]
                            dcv = zq[0] >> al
                            diff = dcv - pred[ci]
                            pred[ci] = dcv
                            s = abs(diff).bit_length()
                            bw.put(*dce[s])
                            if s:
                                bw.put(diff if diff > 0 else diff + (1 << s) - 1, s)
                            if dc_only:
                                continue
                            run = 0
                            for k in range(1, 64):
                                v = zq[k]
                                if v == 0:
                                    run += 1
                                    continue
                                while run > 15:
                                    bw.put(*ace[0xF0])
                                    run -= 16
                                s = abs(v).bit_length()
                                bw.put(*ace[(run << 4) | s])
                                bw.put(v if v > 0 else v + (1 << s) - 1, s)
                                run = 0
                            if run:
                                bw.put(*ace[0x00])
        bw.flush()
        return bytes(bw.out)

    def filler(n=24):
        bw = BitWriter()
        r = random.Random(n)
        for _ in range(n):
            bw.put(r.randrange(256), 8)
        bw.flush()
        return bytes(bw.out)

    if prog is None:
        out += sos_header(list(range(ncomp)), 0, 63, 0) + scan_data(False, 0)
    else:
        for sc in prog:
            if sc[0] == 'dc':
                out += sos_header(list(range(ncomp)), 0, 0, sc[1]) + scan_data(True, sc[1])
            elif sc[0] == 'ac':
                out += sos_header([sc[1]], sc[2], sc[3], 0x01) + filler()
            elif sc[0] == 'dcref':
                out += sos_header(list(range(ncomp)), 0, 0, 0x10 | (sc[1] - 1 if sc[1] else 0)) + filler()
            elif sc[0] == 'dcsep':
                out += sos_header([sc[1]], 0, 0, 0x01) + filler()
    out += b'\xff\xd9'
    return bytes(out)


# ---------------------------------------------------------------------------
def run_cpp(path, fit):
    r = subprocess.run([EXE, path, path + '.cpp.i16', str(fit)], capture_output=True, text=True)
    line = r.stdout.strip().splitlines()[-1] if r.stdout.strip() else 'ERR sin salida'
    if line.startswith('OK'):
        _, total, back, size = line.split()
        return ('OK', open(path + '.cpp.i16', 'rb').read(), int(total), int(back), int(size))
    return ('ERR', line[4:])


_orig_decode = jpg2i16.decode_scaled
_decoded = {}


def _cached_decode(data, info, n):
    """La referencia es lenta: la imagen reducida de un mismo fichero se decodifica una sola vez para los tres modos de colocacion."""
    key = (hash(data), n)
    if key not in _decoded:
        if len(_decoded) > 4:
            _decoded.clear()
        _decoded[key] = _orig_decode(data, info, n)
    return _decoded[key]


jpg2i16.decode_scaled = _cached_decode


def run_ref(data, fit):
    try:
        return ('OK', jpg2i16.convert(data, fit))
    except jpg2i16.JpgError as e:
        return ('ERR', str(e))


def jpg2i16_pal(i16):
    """La paleta (RGB aproximado) de las densidades de la cabecera de un .I16: tension = 0,455 V por canal + 0,261 V por BRIGHT, sobre 0,717 V."""
    pal = []
    for j in range(256):
        lo, hi = i16[4 + 2 * j], i16[5 + 2 * j]
        g, b, w, r = lo >> 4, lo & 15, hi >> 4, hi & 15
        pal.append(tuple(min(255, (c * 707612 + w * 406500 + 32768) >> 16) for c in (r, g, b)))
    return pal


def compare(name, data, fits=(0, 1, 2), expect_err=None):
    """El STM32 y la referencia dan lo mismo (o el mismo error) en los tres modos."""
    if FAST and not (name.startswith('x_') or name == 'mem'):
        return None
    path = os.path.join(WORK, name + '.jpg')
    open(path, 'wb').write(data)
    last = None
    for fit in fits:
        c = run_cpp(path, fit)
        r = run_ref(data, fit)
        if c[0] != r[0]:
            check('%s fit=%d' % (name, fit), False, 'STM32 %s %s, referencia %s %s' % (c[0], c[1] if c[0] == 'ERR' else '', r[0], r[1] if r[0] == 'ERR' else ''))
            continue
        if c[0] == 'ERR':
            ok = c[1] == r[1] and (expect_err is None or c[1] == expect_err)
            check('%s fit=%d' % (name, fit), ok, 'error "%s" %s' % (c[1], 'igual en el STM32' if ok else '(referencia "%s", se esperaba "%s")' % (r[1], expect_err)))
        else:
            same = c[1] == r[1]
            if same:
                n = getattr(jpg2i16.convert, 'n', 0)
                check('%s fit=%d' % (name, fit), True, 'iguales (escala %d/8, leidos %d bytes, %d seek atras)' % (n, c[2], c[3]))
            else:
                diff = [i for i in range(len(r[1])) if r[1][i] != c[1][i]]
                check('%s fit=%d' % (name, fit), False, '%d bytes distintos, el primero en %d' % (len(diff), diff[0]))
            last = c
    return last


# ---------------------------------------------------------------------------
def main():
    os.makedirs(WORK, exist_ok=True)
    print('== Pillow: muestreos, escalas, tamanos ==')
    sizes = [(64, 48), (160, 100), (200, 150), (320, 240), (640, 480), (1280, 960), (333, 251), (97, 131), (1000, 37), (37, 1000),
             (8, 8), (17, 9), (1, 1), (9, 1), (1, 9), (480, 640), (4000, 300)]
    for (w, h) in sizes:
        arr = photo_array(w, h)
        for ss in (2,) if (w * h > 400000) else (0, 1, 2):
            compare('p%dx%d_s%d' % (w, h, ss), pil_jpeg(arr, quality=88, subsampling=ss))
        if w * h <= 100000:
            compare('p%dx%d_gray' % (w, h), pil_jpeg(arr, gray=True, quality=88))

    print('== Pillow: opciones del codificador ==')
    arr = photo_array(200, 150)
    compare('opt', pil_jpeg(arr, quality=80, optimize=True, subsampling=2))
    compare('q5', pil_jpeg(arr, quality=5, subsampling=2))
    compare('q100', pil_jpeg(arr, quality=100, subsampling=0))
    compare('rst_blocks', pil_jpeg(arr, quality=85, subsampling=2, restart_marker_blocks=3))
    compare('rst_rows', pil_jpeg(arr, quality=85, subsampling=0, restart_marker_rows=1))
    compare('rst_gray', pil_jpeg(arr, gray=True, quality=85, restart_marker_blocks=5))
    compare('noise', pil_jpeg(np.random.RandomState(1).randint(0, 256, (150, 200, 3)).astype(np.uint8), quality=95, subsampling=0))
    compare('flat', pil_jpeg(np.full((150, 200, 3), 77, np.uint8), quality=85, subsampling=2))
    compare('dpi_icc', pil_jpeg(arr, quality=85, subsampling=2, dpi=(300, 300), icc_profile=b'\0' * 3000))

    print('== Codificador propio: muestreos raros ==')
    arr = photo_array(150, 100)
    for name, samp in (('444', ((1, 1),) * 3), ('422', ((2, 1), (1, 1), (1, 1))), ('440', ((1, 2), (1, 1), (1, 1))), ('420', ((2, 2), (1, 1), (1, 1))),
                       ('411', ((4, 1), (1, 1), (1, 1))), ('41x', ((1, 4), (1, 1), (1, 1))), ('mix', ((2, 2), (2, 1), (1, 2))),
                       ('chroma_big', ((1, 1), (2, 2), (2, 2))), ('h3', ((3, 1), (1, 1), (1, 1))), ('y_small', ((1, 1), (1, 1), (2, 2)))):
        compare('enc_' + name, encode(arr, samp))
    compare('enc_rst7', encode(arr, ((2, 2), (1, 1), (1, 1)), ri=7))
    compare('enc_rst1', encode(arr, ((2, 1), (1, 1), (1, 1)), ri=1))
    compare('enc_gray_h2', encode(arr, ((2, 2),), gray=True))
    compare('enc_rgb_adobe', encode(arr, ((1, 1),) * 3, rgb=True, adobe=True, jfif=False, ids=(1, 2, 3)))
    compare('enc_rgb_ids', encode(arr, ((1, 1),) * 3, rgb=True, jfif=False, ids=(ord('R'), ord('G'), ord('B'))))
    compare('enc_ycc_adobe', encode(arr, ((2, 2), (1, 1), (1, 1)), adobe=True, jfif=False))
    compare('enc_ids_odd', encode(arr, ((2, 2), (1, 1), (1, 1)), ids=(7, 3, 200)))
    compare('enc_nojfif', encode(arr, ((2, 2), (1, 1), (1, 1)), jfif=False))

    print('== Orientacion EXIF ==')
    arr = photo_array(200, 120)
    for o in range(1, 9):
        compare('exif%d_wide' % o, pil_jpeg(arr, quality=88, subsampling=2, exif=exif_bytes(o)))
        compare('exif%d_tall' % o, pil_jpeg(photo_array(90, 210), quality=88, subsampling=0, exif=exif_bytes(o)))
    for o in (3, 6, 8):
        compare('exif%d_big' % o, pil_jpeg(photo_array(800, 600), quality=85, subsampling=2, exif=exif_bytes(o)))
        compare('exif%d_rst' % o, pil_jpeg(arr, quality=85, subsampling=2, restart_marker_blocks=4, exif=exif_bytes(o)))
    compare('exif6_gray', pil_jpeg(arr, gray=True, exif=exif_bytes(6)))
    compare('exif6_enc', encode(arr, ((1, 2), (1, 1), (1, 1)), exif=exif_bytes(6)))
    # EXIF mal formado o sin la etiqueta: se ignora, igual en las dos
    ex = Image.Exif()
    ex[0x010F] = 'Marca'
    ex[0x0110] = 'Modelo'
    compare('exif_none', pil_jpeg(arr, quality=88, exif=ex.tobytes()))
    bad = exif_bytes(6)
    compare('exif_cut', pil_jpeg(arr, quality=88, exif=bad[:14]))
    compare('exif_range', pil_jpeg(arr, quality=88, exif=exif_bytes(9)))

    # la orientacion hecha por jpg2i16.orient_image coincide con la de Pillow (exif_transpose)
    ok = True
    base = np.arange(7 * 5 * 3, dtype=np.uint8).reshape(5, 7, 3)
    for o in range(1, 9):
        im = Image.fromarray(base)
        ex = im.getexif()
        ex[0x0112] = o
        t = np.asarray(ImageOps.exif_transpose(im))
        rows = [[tuple(int(c) for c in base[y, x]) for x in range(7)] for y in range(5)]
        mine = np.array(jpg2i16.orient_image(rows, o), dtype=np.uint8)
        if mine.shape != t.shape or not (mine == t).all():
            ok = False
            print('    orientacion %d distinta de la de Pillow' % o)
    check('orient_image == exif_transpose de Pillow (las 8)', ok)

    print('== Contra libjpeg (Pillow): la imagen reducida ==')
    # 1/8: libjpeg da EXACTAMENTE lo mismo en gris, 4:4:4 y 4:2:2 (la reduccion a 1/8 es el DC de cada bloque y la misma conversion YCbCr -> RGB).
    #      En 4:2:0 no sale identica: la croma se decodifica a 2x2 por bloque como hace libjpeg, pero su IDCT reducida es otra (diezma la IDCT de 8
    #      puntos; aqui se trunca en frecuencia), y se comprueba que no se separan (mucho menos que repitiendo el DC de la croma: 5-15 de media).
    # 1/4 y 1/2: la IDCT reducida de libjpeg toma muestras de la IDCT de 8 puntos y la de aqui trunca en frecuencia (mas parecida a la media por area),
    #      asi que se compara con la media de cajas de la imagen entera de Pillow. 1/1: igual que la IDCT entera de libjpeg.
    for (w, h, ss, gray) in ((256, 192, 0, False), (256, 192, 1, False), (333, 251, 1, False), (97, 131, 0, False), (200, 77, 1, False),
                             (123, 45, 0, True), (64, 64, 0, True), (256, 192, 2, False), (333, 251, 2, False), (64, 64, 2, False)):
        data = pil_jpeg(photo_array(w, h), gray=gray, quality=88, subsampling=ss)
        im = Image.open(io.BytesIO(data))
        im.draft('RGB', (w // 8, h // 8))
        im.load()
        lib = np.asarray(im.convert('RGB'))
        mine = np.array(jpg2i16.decode_scaled(data, jpg2i16.parse(data), 1), dtype=np.uint8)
        label = 'reducida 1/8 de %dx%d %s' % (w, h, 'gris' if gray else '4:%s' % ('4:4', '2:2', '2:0')[ss])
        if lib.shape != mine.shape:
            check(label, False, 'tamano %s vs %s' % (lib.shape, mine.shape))
        elif ss == 2:
            d = np.abs(lib.astype(float) - mine)
            check(label + ' vs libjpeg (croma a 2x2)', d.mean() < 3.5, 'error medio %.2f, maximo %.0f' % (d.mean(), d.max()))
        else:
            same = (lib == mine).all()
            check(label + ' == libjpeg', bool(same), '' if same else '%d pixeles distintos' % int((lib != mine).any(axis=2).sum()))
    arr = photo_array(256, 192)
    for name, data, tol in (('gris', pil_jpeg(arr, gray=True, quality=95), 2.0), ('4:4:4', pil_jpeg(arr, quality=95, subsampling=0), 2.2),
                            ('4:2:0', pil_jpeg(arr, quality=95, subsampling=2), 6.0)):
        info = jpg2i16.parse(data)
        full = np.asarray(Image.open(io.BytesIO(data)).convert('RGB'), dtype=np.float64)
        for n in (2, 4, 8):
            f = 8 // n
            ref = full.reshape(192 // f, f, 256 // f, f, 3).mean(axis=(1, 3))
            mine = np.array(jpg2i16.decode_scaled(data, info, n), dtype=np.float64)
            d = np.abs(mine - ref)
            t = (0.1 if name != '4:2:0' else 2.0) if n == 8 else tol
            check('reducida %d/8 %s vs media de cajas de la imagen entera' % (n, name), d.mean() < t, 'error medio %.2f, maximo %.0f' % (d.mean(), d.max()))

    print('== Una foto de 12 megapixeles (4000x3000): STM32 contra libjpeg + el resto de la referencia ==')
    import time
    for ss, label in ((1, '4:2:2'), (2, '4:2:0')):
        big = pil_jpeg(photo_array(4000, 3000), quality=90, subsampling=ss)
        bpath = os.path.join(WORK, 'big12mp_%d.jpg' % ss)
        open(bpath, 'wb').write(big)
        im = Image.open(io.BytesIO(big))
        im.draft('RGB', (500, 375))
        im.load()
        lib_rows = [[tuple(int(v) for v in px) for px in row] for row in np.asarray(im.convert('RGB'))]
        for fit in (0, 1, 2):
            g = bmp2i16.geometry(len(lib_rows[0]), len(lib_rows), fit)
            expect = jpg2i16.finish(jpg2i16.area_scale(lib_rows, g), g)
            t0 = time.time()
            c = run_cpp(bpath, fit)
            dt = time.time() - t0
            note = '%.2f s en el PC, %d bytes leidos de %d, %d saltos atras' % (dt, c[2], len(big), c[3]) if c[0] == 'OK' else c[1]
            if c[0] != 'OK':
                check('12 MP %s fit=%d' % (label, fit), False, note)
            elif ss == 1:
                check('12 MP %s fit=%d: STM32 == libjpeg 1/8 + area + paleta' % (label, fit), c[1] == expect, note)
            else:
                def render(i16, pal):
                    return np.array([pal[i16[516 + k]] for k in range(16000)], dtype=np.float64)
                # se compara lo que se ve (los colores que dan las densidades de la cabecera de cada .I16), no los indices
                d = np.abs(render(c[1], jpg2i16_pal(c[1])) - render(expect, jpg2i16_pal(expect))).mean()
                check('12 MP %s fit=%d: STM32 parecido a libjpeg 1/8 + area + paleta' % (label, fit), d < 3.0, 'diferencia media %.2f; %s' % (d, note))

    print('== Calidad frente al camino del BMP ==')
    for (w, h, ss) in ((640, 480, 2), (1280, 960, 2), (333, 251, 0)):
        data = pil_jpeg(photo_array(w, h), quality=90, subsampling=ss)
        im = Image.open(io.BytesIO(data)).convert('RGB')
        # el BMP de la foto decodificada entera, en 24 bits
        a = np.asarray(im)
        rows = [bytes(a[y, x, c] for x in range(w) for c in (2, 1, 0)) for y in range(h)]
        bmp = os.path.join(WORK, 'q%dx%d.bmp' % (w, h))
        stride = ((w * 24 + 31) // 32) * 4
        body = b''.join(r.ljust(stride, b'\0') for r in reversed(rows))
        hdr = b'BM' + struct.pack('<IHHI', 54 + len(body), 0, 0, 54) + struct.pack('<IiiHHIIiiII', 40, w, h, 1, 24, 0, len(body), 2835, 2835, 0, 0)
        i16_bmp = bmp2i16.convert(hdr + body, 0)
        pal_b = bmp2i16.convert.info['pal']
        i16_jpg = jpg2i16.convert(data, 0)
        pal_j = jpg2i16.convert.info['pal']

        def render(i16, pal):
            return np.array([pal[i16[516 + k]] for k in range(16000)], dtype=np.float64)
        d = np.abs(render(i16_bmp, pal_b) - render(i16_jpg, pal_j)).mean()
        check('JPG %dx%d 4:%s vs BMP de la foto entera' % (w, h, '4:4' if ss == 0 else '2:0'), d < 6.0, 'diferencia media %.2f (de 255)' % d)

    print('== JPG progresivos: solo el primer scan de DC (escala 1/8, sin AC) ==')
    arr = photo_array(333, 251)
    # los de libjpeg (Pillow): DC con Al = 1, despues los scans de AC y los de refinamiento (el de DC, el ultimo del fichero, no se lee)
    for ss in (0, 1, 2):
        compare('x_prog_s%d' % ss, pil_jpeg(arr, quality=88, subsampling=ss, progressive=True))
    compare('x_prog_gray', pil_jpeg(arr, gray=True, quality=88, progressive=True))
    compare('x_prog_opt', pil_jpeg(arr, quality=80, subsampling=2, progressive=True, optimize=True))
    compare('x_prog_rst', pil_jpeg(arr, quality=85, subsampling=2, progressive=True, restart_marker_blocks=3))
    compare('x_prog_exif6', pil_jpeg(arr, quality=88, subsampling=2, progressive=True, exif=exif_bytes(6)))
    compare('x_prog_exif3', pil_jpeg(arr, quality=88, subsampling=0, progressive=True, exif=exif_bytes(3)))
    for (w, h) in ((64, 48), (160, 100), (1280, 960), (97, 131), (8, 8), (1, 1), (9, 1), (4000, 300)):
        compare('x_prog%dx%d' % (w, h), pil_jpeg(photo_array(w, h), quality=88, subsampling=2, progressive=True))
    # los nuestros: Al = 0, 1, 3 y 13, con scans de relleno antes y despues del de DC
    arr = photo_array(150, 100)
    for al in (0, 1, 3):
        compare('x_prog_enc_al%d' % al, encode(arr, ((2, 2), (1, 1), (1, 1)), prog=[('dc', al)]))
    compare('x_prog_enc_fill', encode(arr, ((2, 2), (1, 1), (1, 1)), prog=[('ac', 0, 1, 5), ('dcsep', 1), ('dc', 1), ('ac', 1, 1, 63), ('dcref', 1), ('ac', 0, 6, 63)]))
    compare('x_prog_enc_444', encode(arr, ((1, 1),) * 3, prog=[('dc', 2), ('dcref', 2)]))
    compare('x_prog_enc_422', encode(arr, ((2, 1), (1, 1), (1, 1)), prog=[('dc', 1)], ri=5))
    compare('x_prog_enc_440', encode(arr, ((1, 2), (1, 1), (1, 1)), prog=[('dc', 1)], exif=exif_bytes(8)))
    compare('x_prog_enc_411', encode(arr, ((4, 1), (1, 1), (1, 1)), prog=[('dc', 0)]))
    compare('x_prog_enc_gray', encode(arr, ((1, 1),), gray=True, prog=[('dc', 1), ('ac', 0, 1, 63)]))
    compare('x_prog_enc_rgb', encode(arr, ((1, 1),) * 3, rgb=True, adobe=True, jfif=False, prog=[('dc', 1)]))
    compare('x_prog_enc_al13', encode(arr, ((1, 1),) * 3, prog=[('dc', 13)]))
    # sin scan de DC util: solo AC, o DC componente a componente
    compare('x_prog_noscan', encode(arr, ((2, 2), (1, 1), (1, 1)), prog=[('ac', 0, 1, 5), ('ac', 1, 1, 63)]), expect_err='PROGRESSIVE JPG: NO DC SCAN')
    compare('x_prog_dcsep', encode(arr, ((2, 2), (1, 1), (1, 1)), prog=[('dcsep', 0), ('dcsep', 1), ('dcsep', 2)]), expect_err='PROGRESSIVE JPG: NO DC SCAN')
    pg = encode(arr, ((2, 2), (1, 1), (1, 1)), prog=[('dc', 1), ('ac', 0, 1, 5)])
    compare('x_prog_cut_half', pg[:len(pg) // 2], fits=(0,))
    compare('x_prog_cut_dc', pg[:pg.index(b'\xff\xda') + 40], fits=(0,))
    # contra libjpeg: la imagen reducida a 1/8 (libjpeg usa el DC completo, aqui falta el bit bajo si Al = 1)
    for (w, h, ss, gray) in ((256, 192, 0, False), (256, 192, 1, False), (333, 251, 2, False), (123, 45, 0, True), (400, 300, 2, False)):
        data = pil_jpeg(photo_array(w, h), gray=gray, quality=88, subsampling=ss, progressive=True)
        im = Image.open(io.BytesIO(data))
        im.draft('RGB', (w // 8, h // 8))
        im.load()
        lib = np.asarray(im.convert('RGB'), dtype=np.float64)
        mine = np.array(jpg2i16.decode_scaled(data, jpg2i16.parse(data), 1), dtype=np.float64)
        label = 'progresivo %dx%d %s a 1/8 vs libjpeg' % (w, h, 'gris' if gray else '4:%s' % ('4:4', '2:2', '2:0')[ss])
        if lib.shape != mine.shape:
            check(label, False, 'tamano %s vs %s' % (lib.shape, mine.shape))
        else:
            # sin el bit bajo del DC (Al = 1) el error es de 1 a 3 niveles; en 4:2:0 libjpeg decodifica la croma a 2x2 con sus AC y aqui se repite su DC,
            # asi que en los bordes de color duros de la imagen sintetica sale mucho mas (en una foto, tras reducirla a 160x100, apenas se nota: ver los 12 MP)
            d = np.abs(lib - mine)
            check(label, d.mean() < (7.0 if ss == 2 else 1.5) and (ss == 2 or d.max() <= 4), 'error medio %.2f, maximo %.0f' % (d.mean(), d.max()))
    # con Al = 0 el DC es completo: el progresivo da EXACTAMENTE lo mismo que el DC de un baseline de la misma imagen (4:4:4 y 4:2:2: sin croma doble)
    arr = photo_array(150, 100)
    for name, samp in (('444', ((1, 1),) * 3), ('422', ((2, 1), (1, 1), (1, 1))), ('gris', ((1, 1),))):
        gray = name == 'gris'
        pg = encode(arr, samp, gray=gray, prog=[('dc', 0)])
        bs = encode(arr, samp, gray=gray)
        a = jpg2i16.decode_scaled(pg, jpg2i16.parse(pg), 1)
        b = jpg2i16.decode_scaled(bs, jpg2i16.parse(bs), 1)
        check('progresivo Al=0 %s == el DC del baseline de la misma imagen' % name, a == b)
    # una foto grande: solo se lee el principio del fichero
    import time
    for ss, label in ((1, '4:2:2'), (2, '4:2:0')):
        big = pil_jpeg(photo_array(4000, 3000), quality=90, subsampling=ss, progressive=True)
        bpath = os.path.join(WORK, 'big12mp_prog%d.jpg' % ss)
        open(bpath, 'wb').write(big)
        im = Image.open(io.BytesIO(big))
        im.draft('RGB', (500, 375))
        im.load()
        lib_rows = [[tuple(int(v) for v in px) for px in row] for row in np.asarray(im.convert('RGB'))]
        for fit in (0, 1, 2):
            g = bmp2i16.geometry(len(lib_rows[0]), len(lib_rows), fit)
            expect = jpg2i16.finish(jpg2i16.area_scale(lib_rows, g), g)
            t0 = time.time()
            c = run_cpp(bpath, fit)
            dt = time.time() - t0
            if c[0] != 'OK':
                check('12 MP progresivo %s fit=%d' % (label, fit), False, c[1])
                continue

            def render(i16, pal):
                return np.array([pal[i16[516 + k]] for k in range(16000)], dtype=np.float64)
            d = np.abs(render(c[1], jpg2i16_pal(c[1])) - render(expect, jpg2i16_pal(expect))).mean()
            check('12 MP progresivo %s fit=%d: parecido a libjpeg 1/8 + area + paleta, y solo se lee el principio' % (label, fit),
                  d < 3.0 and c[2] < len(big) // 4, 'diferencia media %.2f; %.2f s en el PC, %d bytes leidos de %d' % (d, dt, c[2], len(big)))

    print('== Errores ==')
    arr = photo_array(160, 120)
    good = pil_jpeg(arr, quality=85, subsampling=2)
    cm = io.BytesIO()
    Image.new('CMYK', (64, 48), (10, 20, 30, 40)).save(cm, 'JPEG')
    compare('x_cmyk', cm.getvalue(), expect_err='CMYK JPG NOT SUPPORTED')
    compare('x_empty', b'', expect_err='NOT A JPG FILE')
    compare('x_tiny', b'\xff\xd8', expect_err='JPG TRUNCATED')
    compare('x_bmp', b'BM' + b'\0' * 100, expect_err='NOT A JPG FILE')
    compare('x_garbage', bytes(random.Random(5).randrange(256) for _ in range(500)), expect_err='NOT A JPG FILE')
    compare('x_soi_only_junk', b'\xff\xd8' + bytes(random.Random(6).randrange(256) for _ in range(500)))
    compare('x_cut_half', good[:len(good) // 2], fits=(0,), expect_err='JPG TRUNCATED')
    compare('x_cut_tail', good[:-300], fits=(0,))
    compare('x_cut_header', good[:100])
    compare('x_no_eoi', good[:-2])
    compare('x_rst_cut', pil_jpeg(arr, quality=85, restart_marker_blocks=4)[:-200], fits=(0,))
    # tablas que faltan
    segs = segments(good)
    raw = good
    nodht = bytearray()
    pos = 2
    for m, s in segs:
        ln = len(s) + 2
        if m != 0xC4:
            nodht += raw[pos:pos + 2 + ln]
        pos += 2 + ln
    compare('x_nodht', b'\xff\xd8' + bytes(nodht) + raw[pos:], expect_err='JPG HUFFMAN TABLE MISSING')
    noqt = bytearray()
    pos = 2
    for m, s in segs:
        ln = len(s) + 2
        if m != 0xDB:
            noqt += raw[pos:pos + 2 + ln]
        pos += 2 + ln
    compare('x_noqt', b'\xff\xd8' + bytes(noqt) + raw[pos:], expect_err='JPG QUANT TABLE MISSING')
    # 12 bits y aritmetico: se cambia el marcador / la precision
    p = good.index(b'\xff\xc0')
    compare('x_12bit', good[:p + 4] + b'\x0c' + good[p + 5:], expect_err='UNSUPPORTED JPG (NOT 8 BIT)')
    compare('x_arith', good[:p + 1] + b'\xc9' + good[p + 2:], expect_err='ARITHMETIC JPG NOT SUPPORTED')
    compare('x_lossless', good[:p + 1] + b'\xc3' + good[p + 2:], expect_err='UNSUPPORTED JPG TYPE')
    compare('x_zero_h', good[:p + 5] + b'\0\0' + good[p + 7:], expect_err='BAD JPG SIZE')
    compare('x_4comp', good[:p + 9] + b'\x04' + good[p + 10:])
    compare('x_sampling', good[:p + 11] + b'\x33' + good[p + 12:])
    sos = good.index(b'\xff\xda')
    compare('x_not_baseline', good[:sos + 4 + 7 + 1] + b'\x20' + good[sos + 4 + 7 + 2:])
    compare('x_one_comp_of_three', good[:sos + 4] + b'\x01' + good[sos + 5:])

    print('== Estropeados al azar (las dos tienen que dar lo mismo) ==')
    rnd = random.Random(2026)
    base = {
        'a': pil_jpeg(photo_array(120, 90), quality=85, subsampling=2),
        'b': pil_jpeg(photo_array(120, 90), quality=85, subsampling=0, restart_marker_blocks=5),
        'c': pil_jpeg(photo_array(90, 120), gray=True, quality=70, exif=exif_bytes(6)),
    }
    for it in range(60):
        k = rnd.choice(sorted(base))
        d = bytearray(base[k])
        for _ in range(rnd.choice((1, 1, 2, 5))):
            if rnd.random() < 0.5:
                i = rnd.randrange(len(d))                       # en cualquier sitio
            else:
                i = rnd.randrange(d.index(b'\xff\xda') if rnd.random() < 0.4 else len(d) // 2, len(d))   # cabecera o mitad de datos
            d[i] = rnd.randrange(256)
        compare('mut%02d_%s' % (it, k), bytes(d), fits=(0,))

    print('== Memoria y lecturas ==')
    data = pil_jpeg(photo_array(1280, 960), quality=85, subsampling=2)
    c = compare('mem', data, fits=(0,))
    if c:
        print('    JpgWork = %d bytes (BmpWork = 6360)' % c[4])
        check('JpgWork cabe en los ~31 KB libres con la imagen', c[4] + 16516 <= 29500, '%d + 16516 = %d' % (c[4], c[4] + 16516))
        check('lectura hacia delante (solo el salto de vuelta al principio de los datos en cada pasada)', c[3] <= 2, '%d saltos atras' % c[3])
        check('lee el fichero dos veces como mucho', c[2] <= 2 * len(data), '%d bytes leidos de %d' % (c[2], len(data)))

    print('-----')
    fails = results.count(False)
    print('%d comprobaciones, %d fallos' % (len(results), fails))
    print('TODO OK' if not fails else 'HAY FALLOS')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
