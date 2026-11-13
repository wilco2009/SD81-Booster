#!/usr/bin/env python
# Graficos del SOLITARIO (Klondike del SD81 Booster, HiRes 8x1 + Chroma modo 0). Necesita Pillow (PIL).
#
#   python gen_cards.py png [--force]  -> cartas.png desde cero (dibujado en este script). NO sobrescribe una
#                                         hoja ya existente (puede estar retocada a mano) salvo con --force.
#   python gen_cards.py inc            -> lee cartas.png y escribe cards.inc (+ refresca las muestras de la hoja)
#   python gen_cards.py preview        -> preview_mesa.png (mesa de ejemplo a 1:1, 256x192) desde cartas.png
#   python gen_cards.py                -> inc + preview   (lo normal tras retocar la hoja)
#
# ---------------------------------------------------------------------------------------------------------
# HOJA cartas.png (se edita en cualquier editor de pixeles, guardada como PNG sin entrelazar, a 1:1)
# ---------------------------------------------------------------------------------------------------------
# Las casillas EDITABLES (blanco = vacio, negro = punto) estan sobre fondo azul grisaceo; el script solo lee
# esas casillas, por las coordenadas fijas de abajo. La zona verde de abajo son MUESTRAS ya coloreadas (se
# regeneran con "inc"; no se leen, no vale retocarlas).
#
#   fila y=0   : 13 rangos A 2 3 4 5 6 7 8 9 10 J Q K, casillas 8x7 en x = 9*i (el glifo ya lleva su margen
#                izquierdo: bit 7 = columna 0 de la casilla = x 0 de la carta). Luego 4 palos pequenos 8x7
#                (picas, corazones, diamantes, treboles) en x = 124 + 9*i: glifo de 7x7 + 1 columna vacia.
#   fila y=8   : 4 palos grandes 24x16 en x = 25*i (el glifo de 16x16 va centrado: columnas 4-19) y las
#                letras grandes J Q K 24x16 en x = 108 + 25*i.
#   fila y=25  : hueco vacio 24x32 en x=0 (contorno punteado; las 32 lineas) y dorso 24x28 en x=25 (solo
#                las lineas 2-29 de la carta: las lineas 0, 1, 30 y 31 son estructura fija, ver abajo).
#   y=59 en adelante: muestras en color de las cartas completas (A picas, 10 corazones, J diamantes,
#                Q treboles, K picas, A diamantes, dorso, hueco).
#
# ---------------------------------------------------------------------------------------------------------
# REGLAS DE COLOR (el modo 8x1 da solo 2 colores por byte de linea)
# ---------------------------------------------------------------------------------------------------------
# Atributo = papel * 16 + tinta (bit 3 de cada nibble = brillo, bit 2 verde, bit 1 rojo, bit 0 azul);
# un bit a 1 del bitmap = tinta, a 0 = papel. Toda linea de una carta lleva el MISMO atributo en sus 3 bytes.
# Colores: tapete = verde (4); cara = blanco brillante (15); tinta roja = rojo brillante (10); negro = 0;
# dorso = papel azul (1) y tinta cian (5).
# Para el sprite de arrastre hay que girar los nibbles (color de sprite = tinta*16 + papel).
#
# ---------------------------------------------------------------------------------------------------------
# FORMATO de cards.inc  (lo que usa draw_card; todo en ensamblador de pasmo)
# ---------------------------------------------------------------------------------------------------------
# Rango r = 0..12 (A 2..9 10 J Q K), palo s = 0..3 (0 picas, 1 corazones, 2 diamantes, 3 treboles). Las
# 32 lineas de una carta (L = 0..31) y cada linea son 3 bytes de bitmap + 1 atributo:
#
#   Cara (boca arriba):
#     L = 0, 1, 30, 31  (bordes redondeados):  bitmap = cd_edge[3*j .. 3*j+2] con j = 0,1,2,3 para
#                       L = 0,1,30,31 (3 bytes por entrada); atributo = cd_edgeattr[j] ($40, $4F, $4F, $4F).
#     L = 2..8 (indice, 7 lineas):  byte0 = cd_rank[r*7 + L-2]; byte1 = 0; byte2 = cd_ssuit[s*7 + L-2].
#     L = 12..27 (centro, 16 lineas): se hace OR de 3 bytes: si r <= 9 (A..10) cd_bsuit[s*48 + (L-12)*3],
#                       si r >= 10 (J, Q, K) cd_bletter[(r-10)*48 + (L-12)*3].
#     L = 9..11 y 28..29: en blanco. Atributo de las lineas 2..29 = cd_suitattr[s] ($F0 negro, $FA rojo).
#   Dorso (boca abajo):
#     L = 0, 1, 30, 31 : igual que la cara (cd_edge) pero con cd_backattr[j] ($40, $41, $41, $41).
#     L = 2..29 : bitmap = cd_back[(L-2)*3 .. +2], atributo CD_ATTR_BACK ($15).
#   Hueco vacio: bitmap = cd_hole[L*3 .. +2] en las 32 lineas, atributo CD_ATTR_HOLE ($40) en todas.
#   Tapete liso: bitmap 0, atributo CD_ATTR_FELT ($40).
#
# draw_card puede parar en la linea N (banda superior de una carta tapada por otra: 4 o 10 lineas, y como
# minimo 9 para que el indice se vea entero). El destino tiene paso configurable (32 = pantalla,
# 3 = bufer de sprites): se escribe el bitmap en una zona y el atributo en la otra, igual de ordenado.
# Todos los bytes de glifos tienen el bit 7 a la izquierda.
# ---------------------------------------------------------------------------------------------------------
import os, sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
SHEET = os.path.join(HERE, "cartas.png")
INC = os.path.join(HERE, "cards.inc")
PREVIEW = os.path.join(HERE, "preview_mesa.png")
ROMFILE = os.path.join(HERE, "..", "..", "SD Content", "SYS", "zx81v2.rom")

RANKS = ["A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"]
SUITS = ["picas", "corazones", "diamantes", "treboles"]
SUIT_RED = [0, 1, 1, 0]

# atributos (papel*16 + tinta)
ATTR_FELT = 0x40          # tapete liso
ATTR_TOP = 0x40           # linea 0: tinta negra, papel tapete
ATTR_EDGE_FACE = 0x4F     # lineas 1/30/31 de la cara: tinta blanca, papel tapete (los 0 del bitmap son esquina)
ATTR_EDGE_BACK = 0x41     # lo mismo en el dorso: tinta azul
ATTR_BLACK = 0xF0         # cara con tinta negra
ATTR_RED = 0xFA           # cara con tinta roja brillante
ATTR_BACK = 0x15          # dorso: papel azul, tinta cian
ATTR_HOLE = 0x40          # hueco: contorno negro punteado sobre el tapete

EDGE_LINES = [0, 1, 30, 31]
EDGE_BYTES = [(0x3F, 0xFF, 0xFC), (0x7F, 0xFF, 0xFE), (0x7F, 0xFF, 0xFE), (0x3F, 0xFF, 0xFC)]
EDGE_ATTR_FACE = [ATTR_TOP, ATTR_EDGE_FACE, ATTR_EDGE_FACE, ATTR_EDGE_FACE]
EDGE_ATTR_BACK = [ATTR_TOP, ATTR_EDGE_BACK, ATTR_EDGE_BACK, ATTR_EDGE_BACK]
IDX_LINE = 2              # primera linea del indice (rango / palo pequeno)
BIG_LINE = 12             # primera linea del palo grande
BACK_LINE = 2

# ---------------------------------------------------------------------------------------------------------
# Dibujos iniciales (cada '#' es un punto; la hoja PNG es la fuente una vez generada)
# ---------------------------------------------------------------------------------------------------------
RANK_ART = {
    "A": [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    "2": [".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"],
    "3": ["#####", "...#.", "..#..", "...#.", "....#", "#...#", ".###."],
    "4": ["...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."],
    "5": ["#####", "#....", "####.", "....#", "....#", "#...#", ".###."],
    "6": ["..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###."],
    "7": ["#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."],
    "8": [".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."],
    "9": [".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.."],
    # el 10 es estrecho: "1" de 3 columnas, hueco de 1 y "0" de 3 columnas = 7 de ancho
    "10": [".#.", "##.", ".#.", ".#.", ".#.", ".#.", "###"],
    "J": ["..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."],
    "Q": [".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"],
    "K": ["#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"],
}
ZERO3 = ["###", "#.#", "#.#", "#.#", "#.#", "#.#", "###"]
RANK_ART["10"] = [a + "." + b for a, b in zip(RANK_ART["10"], ZERO3)]

SSUIT_ART = [
    # picas
    ["...#...", "..###..", ".#####.", "#######", "#######", "...#...", "..###.."],
    # corazones
    [".##.##.", "#######", "#######", "#######", ".#####.", "..###..", "...#..."],
    # diamantes
    ["...#...", "..###..", ".#####.", "#######", ".#####.", "..###..", "...#..."],
    # treboles
    ["..###..", "..###..", ".#####.", "#######", "#######", "...#...", "..###.."],
]

BSUIT_ART = [
    # picas
    [".......##.......", "......####......", ".....######.....", "....########....",
     "...##########...", "..############..", ".##############.", "################",
     "################", "################", "################", ".#####.##.#####.",
     "......####......", ".....######.....", "....########....", "................"],
    # corazones
    ["..####....####..", ".######..######.", "################", "################",
     "################", "################", "################", ".##############.",
     "..############..", "...##########...", "....########....", ".....######.....",
     "......####......", ".......##.......", "................", "................"],
    # diamantes
    [".......##.......", "......####......", ".....######.....", "....########....",
     "...##########...", "..############..", ".##############.", "################",
     "################", ".##############.", "..############..", "...##########...",
     "....########....", ".....######.....", "......####......", ".......##......."],
    # treboles
    [".......##.......", ".....######.....", ".....######.....", "....########....",
     "....########....", ".....######.....", "..############..", ".######..######.",
     ".##############.", ".##############.", ".##############.", ".##############.",
     "...###.##.###...", "......####......", "......####......", ".....######....."],
]

BLETTER_ART = {
    "J": [".......###", ".......###", ".......###", ".......###", ".......###", ".......###", ".......###",
          ".......###", ".......###", "###....###", "###....###", "##########", ".########.", "..######.."],
    "Q": ["....#####.....", "..#########...", ".###########..", ".####...####..", "####.....####.",
          "###.......###.", "###.......###.", "###.......###.", "###....#..###.", "####..###.###.",
          ".####..#####..", ".###########..", "..###########.", "....#####..###"],
    "K": ["###.....####", "###....####.", "###...####..", "###..####...", "###..###....", "###.###.....",
          "########....", "#########...", "####..###...", "###....###..", "###....###..", "###.....###.",
          "###......###", "###......###"],
}


def grid_from_art(art, w, h, x0, y0):
    """lista de h filas de w puntos con el dibujo (cadenas '#'/'.') colocado en (x0, y0)"""
    g = [[0] * w for _ in range(h)]
    for y, row in enumerate(art):
        for x, c in enumerate(row):
            if c == '#':
                g[y0 + y][x0 + x] = 1
    return g


def grid_back():
    """dorso 24x28 (lineas 2-29): marco de 1 punto (en la 2a linea, para que se vea en las cartas tapadas) y celosia dentro"""
    g = [[0] * 24 for _ in range(28)]
    for y in range(28):
        for x in range(24):
            inside = 2 <= x <= 21 and 1 <= y <= 26
            frame = inside and (x in (2, 21) or y in (1, 26))
            lat = 3 <= x <= 20 and 2 <= y <= 25 and ((x + y) % 4 == 0 or (x - y) % 4 == 0)
            g[y][x] = 1 if (frame or lat) else 0
    return g


def grid_hole():
    """hueco 24x32: contorno redondeado punteado (lineas 0..31)"""
    g = [[0] * 24 for _ in range(32)]
    for y in range(32):
        for x in range(24):
            if y in (0, 31):
                on = 2 <= x <= 21 and x % 2 == 0
            elif y in (1, 30):
                on = x in (1, 22) and y % 2 == 0 or (x in (1, 22) and y == 1)
            else:
                on = x in (0, 23) and y % 2 == 0
            g[y][x] = 1 if on else 0
    return g


# ---------------------------------------------------------------------------------------------------------
# Disposicion de la hoja
# ---------------------------------------------------------------------------------------------------------
W, H = 264, 94
GAPCOL = (190, 205, 230)
FELT_RGB = None  # se calcula mas abajo
CELLS = {}       # (nombre, i) -> (x, y, w, h)
for i in range(13):
    CELLS[("rank", i)] = (9 * i, 0, 8, 7)
for i in range(4):
    CELLS[("ssuit", i)] = (124 + 9 * i, 0, 8, 7)
    CELLS[("bsuit", i)] = (25 * i, 8, 24, 16)
for i in range(3):
    CELLS[("bletter", i)] = (108 + 25 * i, 8, 24, 16)
CELLS[("hole", 0)] = (0, 25, 24, 32)
CELLS[("back", 0)] = (25, 25, 24, 28)
SAMPLE_Y = 60
SAMPLES = [(0, 0), (9, 1), (10, 2), (11, 3), (12, 0), (0, 2), "back", "hole"]


def sample_xy(i):
    return 8 + 32 * i, SAMPLE_Y


# ---------------------------------------------------------------------------------------------------------
# Paleta y utilidades de bytes
# ---------------------------------------------------------------------------------------------------------
def rgb(idx):
    lv = 255 if idx & 8 else 205
    return ((lv if idx & 2 else 0), (lv if idx & 4 else 0), (lv if idx & 1 else 0))


FELT_RGB = rgb(4)


def row_bytes(row):
    """lista de 0/1 (largo multiplo de 8) -> lista de bytes, bit 7 a la izquierda"""
    out = []
    for i in range(0, len(row), 8):
        b = 0
        for p in row[i:i + 8]:
            b = (b << 1) | p
        out.append(b)
    return out


def grid_bytes(g):
    out = []
    for row in g:
        out.extend(row_bytes(row))
    return out


# ---------------------------------------------------------------------------------------------------------
# Hoja: crear, leer, muestras
# ---------------------------------------------------------------------------------------------------------
def initial_grids():
    G = {}
    for i, r in enumerate(RANKS):
        art = RANK_ART[r]
        G[("rank", i)] = grid_from_art(art, 8, 7, 1, 0)
    for i in range(4):
        G[("ssuit", i)] = grid_from_art(SSUIT_ART[i], 8, 7, 0, 0)
        G[("bsuit", i)] = grid_from_art(BSUIT_ART[i], 24, 16, 4, 0)
    for i, k in enumerate("JQK"):
        art = BLETTER_ART[k]
        w = max(len(r) for r in art)
        h = len(art)
        art = [r.ljust(w, '.') for r in art]
        G[("bletter", i)] = grid_from_art(art, 24, 16, (24 - w) // 2, (16 - h) // 2)
    G[("back", 0)] = grid_back()
    G[("hole", 0)] = grid_hole()
    return G


def write_cells(im, G):
    from PIL import ImageDraw
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W - 1, SAMPLE_Y - 3], fill=GAPCOL)
    for key, (x, y, w, h) in CELLS.items():
        g = G[key]
        for yy in range(h):
            for xx in range(w):
                im.putpixel((x + xx, y + yy), (0, 0, 0) if g[yy][xx] else (255, 255, 255))


def read_cells(path=SHEET):
    im = Image.open(path).convert("RGB")
    if im.size != (W, H):
        sys.exit("%s: la hoja debe medir %dx%d puntos, mide %dx%d" % (path, W, H, im.size[0], im.size[1]))
    G = {}
    for key, (x, y, w, h) in CELLS.items():
        G[key] = [[1 if sum(im.getpixel((x + xx, y + yy))) < 3 * 128 else 0 for xx in range(w)] for yy in range(h)]
    return G


class Data:
    """datos de las cartas ya convertidos a bytes"""
    def __init__(self, G):
        self.rank = [grid_bytes(G[("rank", i)]) for i in range(13)]            # 7 bytes
        self.ssuit = [grid_bytes(G[("ssuit", i)]) for i in range(4)]           # 7 bytes
        self.bsuit = [grid_bytes(G[("bsuit", i)]) for i in range(4)]           # 48 bytes
        self.bletter = [grid_bytes(G[("bletter", i)]) for i in range(3)]       # 48 bytes
        self.back = grid_bytes(G[("back", 0)])                                 # 84 bytes
        self.hole = grid_bytes(G[("hole", 0)])                                 # 96 bytes


def card_lines(d, rank, suit, up=True):
    """32 lineas (b0, b1, b2, atributo) de una carta, exactamente como las compone draw_card"""
    out = []
    if not up:
        for L in range(32):
            if L in EDGE_LINES:
                j = EDGE_LINES.index(L)
                out.append(EDGE_BYTES[j] + (EDGE_ATTR_BACK[j],))
            else:
                o = (L - BACK_LINE) * 3
                out.append(tuple(d.back[o:o + 3]) + (ATTR_BACK,))
        return out
    ink = ATTR_RED if SUIT_RED[suit] else ATTR_BLACK
    for L in range(32):
        if L in EDGE_LINES:
            j = EDGE_LINES.index(L)
            out.append(EDGE_BYTES[j] + (EDGE_ATTR_FACE[j],))
            continue
        b = [0, 0, 0]
        if IDX_LINE <= L < IDX_LINE + 7:
            b[0] = d.rank[rank][L - IDX_LINE]
            b[2] = d.ssuit[suit][L - IDX_LINE]
        if BIG_LINE <= L < BIG_LINE + 16:
            o = (L - BIG_LINE) * 3
            src = d.bsuit[suit] if rank <= 9 else d.bletter[rank - 10]
            for k in range(3):
                b[k] |= src[o + k]
        out.append(tuple(b) + (ink,))
    return out


def hole_lines(d):
    return [tuple(d.hole[L * 3:L * 3 + 3]) + (ATTR_HOLE,) for L in range(32)]


def lines_to_image(lines):
    """lista de (b0,b1,b2,attr) -> imagen RGB de 24 x len(lines)"""
    im = Image.new("RGB", (24, len(lines)))
    for y, (b0, b1, b2, a) in enumerate(lines):
        paper, ink = rgb(a >> 4), rgb(a & 15)
        for k, b in enumerate((b0, b1, b2)):
            for bit in range(8):
                im.putpixel((k * 8 + bit, y), ink if (b >> (7 - bit)) & 1 else paper)
    return im


def sample_lines(d, s):
    if s == "back":
        return card_lines(d, 0, 0, up=False)
    if s == "hole":
        return hole_lines(d)
    return card_lines(d, s[0], s[1])


def refresh_samples(d, path=SHEET):
    im = Image.open(path).convert("RGB")
    from PIL import ImageDraw
    ImageDraw.Draw(im).rectangle([0, SAMPLE_Y - 2, W - 1, H - 1], fill=FELT_RGB)
    for i, s in enumerate(SAMPLES):
        x, y = sample_xy(i)
        im.paste(lines_to_image(sample_lines(d, s)), (x, y))
    im.save(path)


def make_png(force=False):
    if os.path.exists(SHEET) and not force:
        sys.exit("%s ya existe (puede estar retocada a mano): usa --force para regenerarla desde cero" % SHEET)
    G = initial_grids()
    im = Image.new("RGB", (W, H), FELT_RGB)
    write_cells(im, G)
    im.save(SHEET)
    refresh_samples(Data(G))
    print("cartas.png escrito (%dx%d)" % (W, H))


# ---------------------------------------------------------------------------------------------------------
# cards.inc
# ---------------------------------------------------------------------------------------------------------
def defb(bs, per=8):
    out = []
    for i in range(0, len(bs), per):
        out.append("            defb " + ",".join("0%02Xh" % b for b in bs[i:i + per]))
    return out


def emit(d):
    o = ["; Generado por gen_cards.py a partir de cartas.png: no se edita a mano (retoca la hoja y vuelve a",
         "; ejecutar \"python gen_cards.py\"). El formato y como lo usa draw_card estan en la cabecera de",
         "; gen_cards.py. Rango 0..12 = A 2..9 10 J Q K; palo 0..3 = picas, corazones, diamantes, treboles.",
         "",
         "CD_W            equ 3           ; ancho de la carta en bytes (24 px)",
         "CD_H            equ 32          ; alto en lineas",
         "CD_IDX_LINE     equ %d           ; primera linea del indice (rango + palo pequeno), 7 lineas" % IDX_LINE,
         "CD_BIG_LINE     equ %d          ; primera linea del palo / letra grande, 16 lineas" % BIG_LINE,
         "CD_BACK_LINE    equ %d           ; primera linea del patron del dorso, 28 lineas" % BACK_LINE,
         "CD_ATTR_FELT    equ 0%02Xh        ; tapete liso (bitmap 0)" % ATTR_FELT,
         "CD_ATTR_BACK    equ 0%02Xh        ; dorso: papel azul, tinta cian" % ATTR_BACK,
         "CD_ATTR_HOLE    equ 0%02Xh        ; hueco vacio" % ATTR_HOLE,
         "",
         "; atributo de la cara por palo (papel blanco brillante; tinta negra o roja brillante)",
         "cd_suitattr:",
         "            defb " + ",".join("0%02Xh" % (ATTR_RED if SUIT_RED[s] else ATTR_BLACK) for s in range(4)),
         "",
         "; lineas 0, 1, 30 y 31 (bordes redondeados): bitmap (3 bytes por linea) y atributo",
         "cd_edge:"]
    for j in range(4):
        o.append("            defb " + ",".join("0%02Xh" % b for b in EDGE_BYTES[j]) + "   ; linea %d" % EDGE_LINES[j])
    o += ["cd_edgeattr:",
          "            defb " + ",".join("0%02Xh" % a for a in EDGE_ATTR_FACE) + "   ; cara",
          "cd_backattr:",
          "            defb " + ",".join("0%02Xh" % a for a in EDGE_ATTR_BACK) + "   ; dorso",
          "",
          "; indices de rango: 13 x 7 bytes (A 2 3 4 5 6 7 8 9 10 J Q K), alineados a la izquierda de la carta",
          "cd_rank:"]
    for i, r in enumerate(RANKS):
        o.append("            ; " + r)
        o += defb(d.rank[i], 7)
    o += ["", "; palos pequenos: 4 x 7 bytes, a la derecha de la carta (byte 2)", "cd_ssuit:"]
    for i, s in enumerate(SUITS):
        o.append("            ; " + s)
        o += defb(d.ssuit[i], 7)
    o += ["", "; palos grandes: 4 x 48 bytes (16 lineas x 3 bytes), a partir de la linea CD_BIG_LINE", "cd_bsuit:"]
    for i, s in enumerate(SUITS):
        o.append("            ; " + s)
        o += defb(d.bsuit[i], 3)
    o += ["", "; letras grandes J Q K: 3 x 48 bytes, mismo formato que cd_bsuit", "cd_bletter:"]
    for i, k in enumerate("JQK"):
        o.append("            ; " + k)
        o += defb(d.bletter[i], 3)
    o += ["", "; dorso: 28 lineas x 3 bytes (lineas 2-29 de la carta)", "cd_back:"]
    o += defb(d.back, 3)
    o += ["", "; hueco vacio: 32 lineas x 3 bytes", "cd_hole:"]
    o += defb(d.hole, 3)
    o.append("")
    return "\n".join(o)


# ---------------------------------------------------------------------------------------------------------
# Vista previa de una mesa (256x192, 32 bytes por linea, regla de 2 colores por byte)
# ---------------------------------------------------------------------------------------------------------
class Screen:
    def __init__(self):
        self.bm = [[0] * 32 for _ in range(192)]
        self.at = [[ATTR_FELT] * 32 for _ in range(192)]

    def put_lines(self, x, y, lines, n=None):
        assert x % 8 == 0
        for L, (b0, b1, b2, a) in enumerate(lines[:n]):
            yy = y + L
            if not 0 <= yy < 192:
                continue
            for k, b in enumerate((b0, b1, b2)):
                self.bm[yy][x // 8 + k] = b
                self.at[yy][x // 8 + k] = a

    def to_image(self):
        im = Image.new("RGB", (256, 192))
        for y in range(192):
            for bx in range(32):
                a, b = self.at[y][bx], self.bm[y][bx]
                paper, ink = rgb(a >> 4), rgb(a & 15)
                for bit in range(8):
                    im.putpixel((bx * 8 + bit, y), ink if (b >> (7 - bit)) & 1 else paper)
        return im

    def text(self, col, y, s, attr, rom):
        for i, ch in enumerate(s):
            code = zx_code(ch)
            for r in range(8):
                self.bm[y + r][col + i] = rom[0x1E00 + code * 8 + r]
                self.at[y + r][col + i] = attr


def zx_code(ch):
    if ch == " ":
        return 0
    if ch.isdigit():
        return 28 + int(ch)
    if ch.isalpha():
        return 38 + ord(ch.upper()) - 65
    return {".": 27, ":": 14, ",": 26, "-": 22, "+": 21, "/": 24, "*": 23}.get(ch, 15)


def preview(d):
    scr = Screen()
    xcol = lambda k: 16 + 32 * k
    TOP, BOARD = 2, 40
    # fila de arriba: mazo (dorso), descarte en abanico de 3 (paso 8), fundaciones
    scr.put_lines(xcol(0), TOP, card_lines(d, 0, 0, up=False))
    for i, (r, s) in enumerate([(8, 2), (1, 0), (12, 1)]):          # 9 diamantes, 2 picas, K corazones
        scr.put_lines(xcol(1) + 8 * i, TOP, card_lines(d, r, s))
    scr.put_lines(xcol(3), TOP, card_lines(d, 2, 0))                 # 3 de picas en la fundacion
    scr.put_lines(xcol(4), TOP, hole_lines(d))
    scr.put_lines(xcol(5), TOP, card_lines(d, 0, 1))                 # A de corazones
    scr.put_lines(xcol(6), TOP, hole_lines(d))
    # tablero: (tapadas, [destapadas])
    cols = [
        (0, []),
        (1, [(10, 1)]),                                    # J corazones
        (2, [(1, 3)]),                                     # 2 treboles
        (3, [(5, 2), (4, 3), (3, 1)]),                     # 6 diamantes, 5 treboles, 4 corazones
        (4, [(11, 2)]),                                    # Q diamantes
        (5, [(9, 0), (8, 1)]),                             # 10 picas, 9 corazones
        (6, [(12, 0), (11, 1), (10, 3), (9, 2), (8, 0), (7, 1), (6, 3)]),   # K..7 alternando
    ]
    for k, (down, ups) in enumerate(cols):
        y = BOARD
        if down == 0 and not ups:
            scr.put_lines(xcol(k), y, hole_lines(d))
            continue
        for _ in range(down):
            scr.put_lines(xcol(k), y, card_lines(d, 0, 0, up=False))
            y += 4
        for (r, s) in ups:
            scr.put_lines(xcol(k), y, card_lines(d, r, s))
            y += 10
    # barra de estado (la fuente de 8x8 de la ROM)
    if os.path.exists(ROMFILE):
        rom = open(ROMFILE, "rb").read()
        scr.text(0, 184, "SCORE 0000  00:00  NEW D3 QUIT".ljust(32), 0x0F, rom)
        for col, ln in ((19, 3), (23, 2), (26, 4)):
            for i in range(col, col + ln):
                for r in range(8):
                    scr.at[184 + r][i] = 0xF0
    return scr.to_image()


def main():
    args = sys.argv[1:]
    mode = args[0] if args else "all"
    if mode == "png":
        make_png("--force" in args)
        return
    if not os.path.exists(SHEET):
        sys.exit("falta cartas.png: ejecuta primero \"python gen_cards.py png\"")
    d = Data(read_cells())
    if mode in ("inc", "all"):
        open(INC, "w", newline="\r\n").write(emit(d))
        refresh_samples(d)
        print("cards.inc escrito, muestras de cartas.png refrescadas")
    if mode in ("preview", "all"):
        preview(d).save(PREVIEW)
        print("preview_mesa.png escrito (256x192)")


if __name__ == "__main__":
    main()
