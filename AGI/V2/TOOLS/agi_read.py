#!/usr/bin/env python3
"""Lector de datos AGI v2 (Sierra): directorios, volumenes, logicas, imagenes y vistas.

Uso:
    agi_read.py JUEGO list                      # recursos de cada tipo (volumen, desplazamiento, tamano)
    agi_read.py JUEGO pics SALIDA [n ...]       # imagenes a PNG (visual | prioridad), todas si no se dan numeros
    agi_read.py JUEGO views SALIDA [n ...]      # vistas a PNG (una hoja por vista)
    agi_read.py JUEGO logic [n ...]             # mensajes de las logicas (descifrados)
    agi_read.py JUEGO words                     # vocabulario de WORDS.TOK
    agi_read.py JUEGO objects                   # objetos del inventario

JUEGO es la carpeta con LOGDIR, PICDIR, VIEWDIR, SNDDIR, VOL.n, WORDS.TOK y OBJECT.
Solo biblioteca estandar (el PNG se escribe a mano).
"""
import os
import struct
import sys
import zlib

W, H = 160, 168

# Paleta EGA estandar
EGA = [
    (0x00, 0x00, 0x00), (0x00, 0x00, 0xAA), (0x00, 0xAA, 0x00), (0x00, 0xAA, 0xAA),
    (0xAA, 0x00, 0x00), (0xAA, 0x00, 0xAA), (0xAA, 0x55, 0x00), (0xAA, 0xAA, 0xAA),
    (0x55, 0x55, 0x55), (0x55, 0x55, 0xFF), (0x55, 0xFF, 0x55), (0x55, 0xFF, 0xFF),
    (0xFF, 0x55, 0x55), (0xFF, 0x55, 0xFF), (0xFF, 0xFF, 0x55), (0xFF, 0xFF, 0xFF),
]

KEY = b'Avis Durgan'


# --------------------------------------------------------------------------- PNG

def write_png(path, rows, w, h):
    """rows: lista de h filas de w tuplas (r,g,b)."""
    raw = bytearray()
    for r in rows:
        raw.append(0)
        for px in r:
            raw += bytes(px)

    def chunk(t, d):
        c = struct.pack('>I', len(d)) + t + d
        return c + struct.pack('>I', zlib.crc32(t + d) & 0xFFFFFFFF)

    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)))
        f.write(chunk(b'IDAT', zlib.compress(bytes(raw), 9)))
        f.write(chunk(b'IEND', b''))


# --------------------------------------------------------------------------- juego

class Game:
    TYPES = ('LOG', 'PIC', 'VIEW', 'SND')

    def __init__(self, path):
        self.path = path
        self.dirs = {}
        for t in self.TYPES:
            with open(os.path.join(path, t + 'DIR'), 'rb') as f:
                d = f.read()
            ents = []
            for i in range(len(d) // 3):
                b0, b1, b2 = d[3 * i:3 * i + 3]
                if b1 == 0xFF and b2 == 0xFF:           # FFFFFF vacio; SQ2 trae tambien xFFFF
                    ents.append(None)
                else:
                    ents.append((b0 >> 4, ((b0 & 0x0F) << 16) | (b1 << 8) | b2))
            self.dirs[t] = ents
        self.vols = {}

    def vol(self, n):
        if n not in self.vols:
            with open(os.path.join(self.path, 'VOL.%d' % n), 'rb') as f:
                self.vols[n] = f.read()
        return self.vols[n]

    def resource(self, t, n):
        """Datos de un recurso (sin cabecera) o None."""
        e = self.dirs[t][n] if n < len(self.dirs[t]) else None
        if e is None:
            return None
        v, off = e
        d = self.vol(v)
        if d[off:off + 2] != b'\x12\x34':
            raise ValueError('%s %d: cabecera erronea en VOL.%d+%X' % (t, n, v, off))
        ln = d[off + 3] | (d[off + 4] << 8)
        return d[off + 5:off + 5 + ln]

    def present(self, t):
        return [i for i, e in enumerate(self.dirs[t]) if e is not None]


# --------------------------------------------------------------------------- logicas

def logic_messages(d):
    """Mensajes de una logica: lista (indice 1..n) de cadenas."""
    code = d[0] | (d[1] << 8)
    ms = code + 2
    n = d[ms]
    end = d[ms + 1] | (d[ms + 2] << 8)
    tstart = ms + 3 + 2 * n
    text = bytearray(d[ms + 3 + 2 * n:ms + 1 + end] if end else d[tstart:])
    # El bloque cifrado empieza en tstart y la clave se reinicia ahi.
    text = bytearray(d[tstart:])
    for i in range(len(text)):
        text[i] ^= KEY[i % len(KEY)]
    out = []
    for i in range(n):
        p = d[ms + 3 + 2 * i] | (d[ms + 4 + 2 * i] << 8)
        if p == 0:
            out.append(None)
            continue
        p = p + ms + 1 - tstart
        q = text.find(b'\x00', p)
        out.append(bytes(text[p:q if q >= 0 else len(text)]).decode('latin-1'))
    return out


# --------------------------------------------------------------------------- imagenes

def _rnd(v, direction):
    import math
    f = math.floor(v)
    if direction < 0:
        return f if v - f <= 0.501 else math.ceil(v)
    return f if v - f < 0.499 else math.ceil(v)


class Picture:
    def __init__(self, data):
        self.vis = [15] * (W * H)
        self.pri = [4] * (W * H)
        self.cc, self.con = 0, False
        self.pc, self.pon = 0, False
        self.pen = 0
        self.d = data
        self.p = 0
        self.run()

    def byte(self):
        if self.p >= len(self.d):
            return 0xFF
        b = self.d[self.p]
        self.p += 1
        return b

    def peek(self):
        return self.d[self.p] if self.p < len(self.d) else 0xFF

    def put(self, x, y):
        if 0 <= x < W and 0 <= y < H:
            i = y * W + x
            if self.con:
                self.vis[i] = self.cc
            if self.pon:
                self.pri[i] = self.pc

    def line(self, x1, y1, x2, y2):
        x1, y1 = max(0, min(W - 1, x1)), max(0, min(H - 1, y1))
        x2, y2 = max(0, min(W - 1, x2)), max(0, min(H - 1, y2))
        h, w = y2 - y1, x2 - x1
        if w == 0 and h == 0:
            self.put(x1, y1)
            return
        if abs(w) > abs(h):
            ax = 1 if w > 0 else -1
            ay = h / abs(w)
            x, y = x1, float(y1)
            while x != x2:
                self.put(x, _rnd(y, ay))
                x += ax
                y += ay
        else:
            ay = 1 if h > 0 else -1
            ax = w / abs(h)
            x, y = float(x1), y1
            while y != y2:
                self.put(_rnd(x, ax), y)
                y += ay
                x += ax
        self.put(x2, y2)

    def can_fill(self, x, y):
        if not self.con and not self.pon:
            return False
        if self.con and self.cc == 15:
            return False
        i = y * W + x
        if self.con:
            return self.vis[i] == 15
        return self.pri[i] == 4

    def fill(self, x, y):
        if not (0 <= x < W and 0 <= y < H) or not self.can_fill(x, y):
            return
        st = [(x, y)]
        while st:
            x, y = st.pop()
            if not self.can_fill(x, y):
                continue
            xl = x
            while xl > 0 and self.can_fill(xl - 1, y):
                xl -= 1
            xr = x
            while xr < W - 1 and self.can_fill(xr + 1, y):
                xr += 1
            for xx in range(xl, xr + 1):
                self.put(xx, y)
            for yy in (y - 1, y + 1):
                if 0 <= yy < H:
                    inside = False
                    for xx in range(xl, xr + 1):
                        ok = self.can_fill(xx, yy)
                        if ok and not inside:
                            st.append((xx, yy))
                        inside = ok

    def brush(self, x, y):
        """Pincel F9/FA: aproximado (circulo o cuadrado; el salpicado no se reproduce exacto)."""
        size = self.pen & 7
        rect = bool(self.pen & 0x10)
        for dy in range(-size, size + 1):
            for dx in range(-size, size + 1):
                if rect or dx * dx + dy * dy <= size * size + size // 2:
                    self.put(x + dx, y + dy)

    def run(self):
        while self.p < len(self.d):
            op = self.byte()
            if op == 0xFF:
                break
            if op == 0xF0:
                self.cc, self.con = self.byte() & 15, True
            elif op == 0xF1:
                self.con = False
            elif op == 0xF2:
                self.pc, self.pon = self.byte() & 15, True
            elif op == 0xF3:
                self.pon = False
            elif op in (0xF4, 0xF5):
                x, y = self.byte(), self.byte()
                self.put(x, y)
                horiz = (op == 0xF5)        # F5 empieza en horizontal, F4 en vertical
                while self.peek() < 0xF0:
                    v = self.byte()
                    if horiz:
                        self.line(x, y, v, y)
                        x = v
                    else:
                        self.line(x, y, x, v)
                        y = v
                    horiz = not horiz
            elif op == 0xF6:
                x, y = self.byte(), self.byte()
                self.put(x, y)
                while self.peek() < 0xF0:
                    x2, y2 = self.byte(), self.byte()
                    self.line(x, y, x2, y2)
                    x, y = x2, y2
            elif op == 0xF7:
                x, y = self.byte(), self.byte()
                self.put(x, y)
                while self.peek() < 0xF0:
                    b = self.byte()
                    dx, dy = (b >> 4) & 15, b & 15
                    if dx & 8:
                        dx = -(dx & 7)
                    if dy & 8:
                        dy = -(dy & 7)
                    self.line(x, y, x + dx, y + dy)
                    x, y = x + dx, y + dy
            elif op == 0xF8:
                while self.peek() < 0xF0:
                    x, y = self.byte(), self.byte()
                    self.fill(x, y)
            elif op == 0xF9:
                self.pen = self.byte()
            elif op == 0xFA:
                while self.peek() < 0xF0:
                    if self.pen & 0x20:
                        self.byte()             # patron de salpicado
                    x, y = self.byte(), self.byte()
                    self.brush(x, y)
            # cualquier otro byte < F0 suelto se ignora

    def rows(self, scale=2):
        """Visual y prioridad lado a lado; scale = ancho de cada pixel (AGI es 2:1)."""
        out = []
        for y in range(H):
            r = []
            for plane in (self.vis, self.pri):
                for x in range(W):
                    r.extend([EGA[plane[y * W + x]]] * scale)
                if plane is self.vis:
                    r.extend([(40, 40, 40)] * 4)
            out.append(r)
        return out, (2 * W * scale + 4)


# --------------------------------------------------------------------------- vistas

def view_cells(d):
    """Devuelve (descripcion, bucles); cada bucle es una lista de celdas (w, h, trans, filas)."""
    nloops = d[2]
    desc = None
    dofs = d[3] | (d[4] << 8)
    if dofs:
        q = d.find(b'\x00', dofs)
        desc = bytes(d[dofs:q]).decode('latin-1')
    loops = []
    for l in range(nloops):
        lo = d[5 + 2 * l] | (d[6 + 2 * l] << 8)
        nc = d[lo]
        cells = []
        for c in range(nc):
            co = lo + (d[lo + 1 + 2 * c] | (d[lo + 2 + 2 * c] << 8))
            w, h, tm = d[co], d[co + 1], d[co + 2]
            trans = tm & 15
            p = co + 3
            rows = []
            for _ in range(h):
                row = []
                while True:
                    b = d[p]
                    p += 1
                    if b == 0:
                        break
                    row.extend([b >> 4] * (b & 15))
                row = (row + [trans] * w)[:w]
                rows.append(row)
            cells.append((w, h, trans, rows))
        loops.append(cells)
    return desc, loops


def view_sheet(loops, scale=2):
    """Una fila por bucle, las celdas una detras de otra sobre fondo magenta oscuro."""
    bg = (70, 0, 70)
    maxh = max((c[1] for l in loops for c in l), default=1)
    rowsout = []
    totw = 0
    strips = []
    for cells in loops:
        strip = [[] for _ in range(maxh)]
        for (w, h, trans, rows) in cells:
            for y in range(maxh):
                if y < h:
                    for v in rows[y]:
                        strip[y].extend([bg if v == trans else EGA[v]] * scale)
                else:
                    strip[y].extend([bg] * (w * scale))
                strip[y].extend([bg, bg])
        strips.append(strip)
        totw = max(totw, len(strip[0]))
    for strip in strips:
        for r in strip:
            rowsout.append(r + [bg] * (totw - len(r)))
        rowsout.append([(40, 40, 40)] * totw)
    return rowsout, totw


# --------------------------------------------------------------------------- vocabulario y objetos

def words(path):
    with open(os.path.join(path, 'WORDS.TOK'), 'rb') as f:
        d = f.read()
    p = 52                                       # 26 desplazamientos de letra
    out = []
    prev = ''
    while p < len(d) - 2:
        pre = d[p]
        p += 1
        s = list(prev[:pre])
        while True:
            b = d[p]
            p += 1
            s.append(chr((b & 0x7F) ^ 0x7F))
            if b & 0x80:
                break
        w = ''.join(s)
        num = (d[p] << 8) | d[p + 1]
        p += 2
        out.append((w, num))
        prev = w
    return out


def objects(path):
    with open(os.path.join(path, 'OBJECT'), 'rb') as f:
        d = bytearray(f.read())
    if d[:1] and False:
        pass
    # el fichero puede venir cifrado con la misma clave que los mensajes
    def parse(buf):
        n = (buf[0] | (buf[1] << 8)) // 3
        names = []
        for i in range(n):
            o = buf[3 + 3 * i] | (buf[4 + 3 * i] << 8)
            room = buf[5 + 3 * i]
            o += 3
            q = buf.find(b'\x00', o)
            if o >= len(buf) or q < 0:
                return None
            names.append((i, room, bytes(buf[o:q]).decode('latin-1')))
        return names
    r = parse(d)
    if r is None or not all(c.isprintable() for _, _, c in r[:5]):
        dd = bytearray(d)
        for i in range(len(dd)):
            dd[i] ^= KEY[i % len(KEY)]
        r2 = parse(dd)
        if r2 is not None:
            return r2
    return r


# --------------------------------------------------------------------------- CLI

def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 1
    g = Game(argv[1])
    cmd = argv[2]
    args = argv[3:]
    if cmd == 'list':
        for t in Game.TYPES:
            pr = g.present(t)
            print('%s: %d de %d' % (t, len(pr), len(g.dirs[t])))
            for n in pr:
                v, off = g.dirs[t][n]
                try:
                    ln = len(g.resource(t, n))
                except ValueError as e:
                    ln = -1
                print('  %3d  VOL.%d+%05X  %5d' % (n, v, off, ln))
    elif cmd in ('pics', 'views'):
        out = args[0]
        os.makedirs(out, exist_ok=True)
        t = 'PIC' if cmd == 'pics' else 'VIEW'
        sel = [int(a) for a in args[1:]] or g.present(t)
        for n in sel:
            d = g.resource(t, n)
            if d is None:
                continue
            try:
                if t == 'PIC':
                    rows, w = Picture(d).rows()
                    h = H
                else:
                    desc, loops = view_cells(d)
                    if not loops:
                        continue
                    rows, w = view_sheet(loops)
                    h = len(rows)
                write_png(os.path.join(out, '%s%03d.png' % (t.lower(), n)), rows, w, h)
            except (IndexError, ValueError) as e:
                print('%s %d: error %s' % (t, n, e))
        print('hecho:', len(sel), 'recursos en', out)
    elif cmd == 'logic':
        sel = [int(a) for a in args] or g.present('LOG')
        for n in sel:
            d = g.resource('LOG', n)
            if d is None:
                continue
            print('--- logica %d (%d bytes)' % (n, len(d)))
            for i, m in enumerate(logic_messages(d), 1):
                if m is not None:
                    print('  %2d: %s' % (i, m.replace('\n', '\\n')))
    elif cmd == 'words':
        for w, num in words(argv[1]):
            print('%5d %s' % (num, w))
    elif cmd == 'objects':
        for i, room, name in objects(argv[1]):
            print('%3d sala %3d  %s' % (i, room, name))
    else:
        print(__doc__)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
