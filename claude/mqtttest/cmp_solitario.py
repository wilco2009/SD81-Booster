#!/usr/bin/env python
# Compara lo que ha volcado test_z80solitario con un modelo independiente escrito en Python (usa gen_cards.py para las cartas):
#   - screen.bin: la mesa del arranque (reparto con la semilla de entropia) contra el dibujo de referencia
#   - buf.bin: draw_card con paso 3 (con y sin giro de nibbles) contra card_lines()/hole_lines()
#   - sc_*.bin: pantalla y modelo tras cada escenario (repartos con semilla fija, clics en el mazo en robo de 3 y de 1,
#     y un tablero a tope); la pantalla se compara punto a punto con el dibujo de referencia (lineas 0-183)
#   - hits_*.bin: hit_test sobre una rejilla de puntos (paso 2) contra la respuesta esperada
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "EXAMPLES", "SOLITARIO"))
import gen_cards as g

errors = 0
def check(c, msg):
    global errors
    print(("  ok    " if c else "  ERROR: ") + msg)
    if not c:
        errors += 1

# --- direcciones del .sym ---
syms = {}
for line in open("solitario.sym"):
    p = line.split()
    if len(p) == 3 and p[1].upper() == "EQU":
        v = p[2].upper().rstrip("H")
        syms[p[0].lower()] = int(v, 16)
BASE = 0x8200

class Model:
    def __init__(self, raw):
        self.raw = raw
        def b(name, i=0): return raw[syms[name] - BASE + i]
        self.tab_n = [b("tab_n", k) for k in range(7)]
        self.tab = [[b("tab", 20 * k + i) for i in range(self.tab_n[k])] for k in range(7)]
        self.stock_n, self.waste_n, self.vis, self.mode = b("stock_n"), b("waste_n"), b("waste_vis"), b("draw_mode")
        self.stock = [b("stock", i) for i in range(self.stock_n)]
        self.waste = [b("waste", i) for i in range(self.waste_n)]
        self.found_n = [b("found_n", i) for i in range(4)]
        self.found_s = [b("found_s", i) for i in range(4)]
        self.col_d = [b("col_d", k) for k in range(7)]
        self.col_u = [b("col_u", k) for k in range(7)]

def steps(col):
    """(paso de las tapadas, paso de las destapadas) de una columna segun el plan, calculado de forma independiente"""
    nd = sum(1 for c in col if not c & 0x80)
    nu = len(col) - nd
    bd, bu = (nd - 1, 0) if nu == 0 and nd > 0 else (nd, max(nu - 1, 0))
    d, u = 4, 10
    while d * bd + u * bu > 110:
        if u > 8: u -= 1
        elif d > 2: d -= 1
        else: break
    return d, u

def col_layout(col):
    """lista de (y, lineas) por carta"""
    d, u = steps(col)
    out, y = [], 40
    for i, c in enumerate(col):
        if i == len(col) - 1:
            out.append((y, 32))
        else:
            st = u if c & 0x80 else d
            out.append((y, st)); y += st
    return out

d_data = g.Data(g.read_cells())

def render(m):
    scr = g.Screen()
    xc = [2, 6, 10, 14, 18, 22, 26]
    def code_lines(c):
        if c == 0xFF: return g.hole_lines(d_data)
        up = bool(c & 0x80)
        return g.card_lines(d_data, c & 15, (c >> 4) & 7, up)
    scr.put_lines(8 * 2, 2, code_lines(0 if m.stock_n else 0xFF))
    if m.waste_n:
        first = m.waste_n - m.vis
        for i in range(m.vis):
            scr.put_lines(8 * (6 + i), 2, code_lines(m.waste[first + i]))
    for s in range(4):
        c = 0xFF if m.found_n[s] == 0 else 0x80 | (m.found_s[s] << 4) | (m.found_n[s] - 1)
        scr.put_lines(8 * (14 + 4 * s), 2, code_lines(c))
    for k in range(7):
        col = m.tab[k]
        if not col:
            scr.put_lines(8 * xc[k], 40, code_lines(0xFF))
            continue
        for (y, n), c in zip(col_layout(col), col):
            scr.put_lines(8 * xc[k], y, code_lines(c), n)
    return scr

def compare_screen(raw, m, name):
    bm, at = raw[:6144], raw[6144:12288]
    ref = render(m)
    bad = 0
    for y in range(184):
        for bx in range(32):
            if bm[y * 32 + bx] != ref.bm[y][bx] or at[y * 32 + bx] != ref.at[y][bx]:
                bad += 1
    check(bad == 0, "%s: la pantalla coincide con el dibujo de referencia (%d bytes distintos)" % (name, bad))

def colx(x):
    if x < 16: return None
    r = x - 16
    if r % 32 >= 24: return None
    k = r // 32
    return k if k < 7 else None

def expect(x, y, m):
    none = (0, 255, 255)
    if 2 <= y < 34:
        if x < 16: return none
        if x < 40: return (1, 0, 255)
        if m.waste_n > 0 and 48 <= x < 72 + 8 * (m.vis - 1): return (2, 0, 255)
        k = colx(x)
        return (3, k - 3, 255) if k is not None and k >= 3 else none
    if y < 40 or y >= 184: return none
    k = colx(x)
    if k is None: return none
    col = m.tab[k]
    if not col:
        return (4, k, 255) if y < 72 else none
    for i, (cy, n) in enumerate(col_layout(col)):
        if cy <= y < cy + n:
            return (4, k, i)
    return none

def compare_hits(path, m, name):
    h = open(path, "rb").read()
    bad, first = 0, None
    i = 0
    for y in range(0, 192, 2):
        for x in range(0, 256, 2):
            got = tuple(h[i:i + 3]); i += 3
            exp = expect(x, y, m)
            if got != exp:
                bad += 1
                if first is None: first = (x, y, got, exp)
    check(bad == 0, "%s: hit_test coincide en toda la rejilla (%d distintos)%s" % (name, bad, "" if first is None else " primero %s" % (first,)))

def load(name):
    raw = open("sc_%s.bin" % name, "rb").read()
    return raw, Model(raw[12288:12288 + 512])

# ---------------------------------------------------------------- arranque
raw = open("screen.bin", "rb").read()
m = Model(raw[12288:12288 + 512])
compare_screen(raw, m, "arranque")
cards = [c for col in m.tab for c in col] + m.stock
check(sorted(c & 0x7F for c in cards) == sorted(s * 16 + r for s in range(4) for r in range(13)), "arranque: las 52 cartas, una vez cada una")

# ---------------------------------------------------------------- draw_card con paso 3
buf = open("buf.bin", "rb").read()
bbm, battr = buf[:0x400], buf[0x400:]
tests = [((0, 0), 32, 0), ((9, 1), 32, 0), ((10, 2), 32, 1), ((11, 3), 32, 0), ((12, 0), 32, 1),
         ("back", 32, 0), ("back", 4, 0), ((4, 1), 10, 0), ("hole", 32, 0)]
for i, (s, n, swap) in enumerate(tests):
    if s == "back": lines = g.card_lines(d_data, 0, 0, up=False)
    elif s == "hole": lines = g.hole_lines(d_data)
    else: lines = g.card_lines(d_data, s[0], s[1])
    ok = True
    for L in range(32):
        o = i * 96 + L * 3
        got_b, got_a = tuple(bbm[o:o + 3]), tuple(battr[o:o + 3])
        if L < n:
            a = lines[L][3]
            if swap: a = ((a << 4) | (a >> 4)) & 255
            ok &= got_b == tuple(lines[L][:3]) and got_a == (a, a, a)
        else:
            ok &= got_b == (0xEE,) * 3 and got_a == (0xEE,) * 3
    check(ok, "draw_card paso 3: %s, %d lineas, giro de atributo %d" % (s, n, swap))

# ---------------------------------------------------------------- repartos y clics en el mazo
decks = {}
for name, mode in (("a", 3), ("b", 1)):
    raw, m = load(name + "0")
    compare_screen(raw, m, name + " reparto")
    compare_hits("hits_%s0.bin" % name, m, name + " reparto")
    check(m.tab_n == [1, 2, 3, 4, 5, 6, 7] and m.stock_n == 24 and m.waste_n == 0 and m.found_n == [0] * 4, name + ": 7 columnas de 1 a 7, 24 en el mazo")
    check(all(col[-1] & 0x80 and not any(c & 0x80 for c in col[:-1]) for col in m.tab) and not any(c & 0x80 for c in m.stock),
          name + ": la de arriba de cada columna boca arriba y el resto boca abajo")
    cards = [c for col in m.tab for c in col] + m.stock
    check(sorted(c & 0x7F for c in cards) == sorted(s * 16 + r for s in range(4) for r in range(13)), name + ": las 52 cartas, una vez cada una")
    check(m.col_d == [4] * 7 and m.col_u == [10] * 7, name + ": pasos 4 y 10 en un reparto normal")
    decks[name] = cards
    stock, waste, vis = list(m.stock), [], 0
    for i in range(1, 11):
        # regla independiente del clic
        if stock:
            n = min(mode, len(stock))
            for _ in range(n): waste.append(stock.pop() | 0x80)
            vis = n
        elif waste:
            stock = [c & 0x7F for c in reversed(waste)]; waste = []; vis = 0
        raw, mi = load("%s%d" % (name, i))
        ok = mi.stock == stock and mi.waste == waste and mi.vis == vis and mi.tab == m.tab
        check(ok, "%s clic %d: mazo %d, descarte %d, abanico %d" % (name, i, len(stock), len(waste), vis))
        compare_screen(raw, mi, "%s clic %d" % (name, i))
        if i in (3, 10):
            compare_hits("hits_%s%d.bin" % (name, i), mi, "%s clic %d" % (name, i))
check(decks["a"] != decks["b"], "dos semillas dan dos repartos distintos")
check(decks["a"] != sorted(decks["a"]), "el reparto esta barajado")

# ---------------------------------------------------------------- tablero a tope
raw, m = load("full")
compare_screen(raw, m, "tablero a tope")
compare_hits("hits_full.bin", m, "tablero a tope")
want = [steps(col) for col in m.tab]
check(m.col_d == [w[0] for w in want] and m.col_u == [w[1] for w in want], "pasos por columna: %s" % list(zip(m.col_d, m.col_u)))
lay = col_layout(m.tab[3])
check(lay[-1][0] + 32 <= 182, "la columna 3 (6 tapadas + 13 destapadas) acaba en y=%d, dentro del tablero" % (lay[-1][0] + 32))

print("TODO OK" if not errors else "%d ERRORES" % errors)
sys.exit(1 if errors else 0)
