#!/usr/bin/env python3
"""spr2cel.py -- hoja de sprites (PNG indexado, 16 colores) -> celdas del formato B + pal16 + .inc para pasmo.

Entrada
    sprites.png   PNG indexado de hasta 16 colores, en una rejilla de celdas de 8x8. El indice 0 es el transparente.
    sprites.def   lista de graficos (uno por linea): "nombre  x y  w h  [fases]"
                    x y    celda de la esquina superior izquierda en el PNG (en celdas, no en pixeles)
                    w h    tamano del grafico en celdas (16x16 = 2 2; el jefe de 32x24 = 4 3)
                    fases  numero de fases de animacion (por omision 1); van una detras de otra en horizontal:
                           la fase f ocupa las columnas x + f*w ... x + f*w + w - 1
                  El orden de las lineas es el orden de las paginas. Dentro de un grafico, las celdas van por filas
                  (arriba-izquierda, arriba-derecha, abajo-izquierda, ...) y una fase detras de otra.
Salida (en --out, por omision la carpeta actual; --name HOJA)
    HOJA.BIN  celdas de 32 bytes (8 filas de 4 bytes, nibble alto = pixel de la izquierda); se carga en SHEET_ADDR
    HOJA.PAL  la pal16: 16 entradas de 2 bytes (byte bajo {G,B}, byte alto {BRIGHT,R}), listas para POKE 2064 / 2066
    HOJA.INC  etiquetas para pasmo: pagina de cada grafico y de cada fase, medidas y la pal16 como defb

Uso:  python spr2cel.py ../assets/sprites.png [-d ../assets/sprites.def] [-o ../out] [--check]
"""
import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pngio  # noqa: E402
import pal160  # noqa: E402

CELL = 8
CELL_BYTES = 32


class DefError(Exception):
    pass


def parse_def(text):
    """Devuelve [(nombre, x, y, w, h, fases)]."""
    gr = []
    seen = set()
    for n, line in enumerate(text.splitlines(), 1):
        line = re.split(r'[#;]', line, maxsplit=1)[0].strip()
        if not line:
            continue
        f = line.split()
        if len(f) not in (5, 6):
            raise DefError('linea %d: se esperaba "nombre x y w h [fases]"' % n)
        name = f[0].upper()
        if not re.fullmatch(r'[A-Z][A-Z0-9_]*', name):
            raise DefError('linea %d: nombre no valido para una etiqueta: %s' % (n, f[0]))
        if name in seen:
            raise DefError('linea %d: nombre repetido: %s' % (n, name))
        seen.add(name)
        try:
            x, y, w, h = (int(v) for v in f[1:5])
            fr = int(f[5]) if len(f) == 6 else 1
        except ValueError:
            raise DefError('linea %d: numeros no validos' % n)
        if w < 1 or h < 1 or fr < 1 or x < 0 or y < 0:
            raise DefError('linea %d: medidas no validas' % n)
        gr.append((name, x, y, w, h, fr))
    return gr


def cell_pixels(img, cx, cy):
    """Los 8x8 indices de la celda (cx, cy) del PNG."""
    return [[img[cy * CELL + r][cx * CELL + c] for c in range(CELL)] for r in range(CELL)]


def encode_cell(pix):
    """8x8 indices (0-15) -> 32 bytes del formato B."""
    out = bytearray()
    for row in pix:
        for k in range(0, CELL, 2):
            out.append((row[k] << 4) | row[k + 1])
    return bytes(out)


def decode_cell(b):
    """32 bytes -> 8x8 indices."""
    return [[(b[r * 4 + k // 2] >> 4) if k % 2 == 0 else (b[r * 4 + k // 2] & 15) for k in range(CELL)] for r in range(CELL)]


def layout(groups, first_page):
    """Asigna paginas. Devuelve [(nombre, pagina_inicial, w, h, fases, [(pagina, cx_png, cy_png), ...])]."""
    out = []
    page = first_page
    for name, x, y, w, h, fr in groups:
        cells = []
        for f in range(fr):
            for cy in range(h):
                for cx in range(w):
                    cells.append((page, x + f * w + cx, y + cy))
                    page += 1
        out.append((name, cells[0][0], w, h, fr, cells))
    return out, page - first_page


def build(img, w_png, h_png, pal_rgb, groups, first_page, gamma):
    """Devuelve (bin, pal16 de 32 bytes, layout, error maximo de la red analogica)."""
    if w_png % CELL or h_png % CELL:
        raise DefError('el PNG mide %dx%d: debe ser multiplo de 8' % (w_png, h_png))
    if max(max(r) for r in img) > 15:
        raise DefError('el PNG usa mas de 16 colores (indice %d)' % max(max(r) for r in img))
    lay, ncell = layout(groups, first_page)
    if first_page < 1 or first_page + ncell > 256:
        raise DefError('%d celdas desde la pagina %d no caben (la pagina 0 es el formato A y el maximo es la 255)' % (ncell, first_page))
    cw, ch = w_png // CELL, h_png // CELL
    blob = bytearray()
    for name, p0, w, h, fr, cells in lay:
        for page, cx, cy in cells:
            if cx >= cw or cy >= ch:
                raise DefError('%s: la celda (%d,%d) esta fuera del PNG (%dx%d celdas)' % (name, cx, cy, cw, ch))
            blob += encode_cell(cell_pixels(img, cx, cy))
    pal = bytearray()
    worst = 0.0
    for i in range(16):
        r, g, b = pal_rgb[i] if i < len(pal_rgb) else (0, 0, 0)
        tgt = [(c / 255.0) ** gamma for c in (r, g, b)]
        nr, ng, nb, nw = pal160.solve(*tgt)
        pal += pal160.entry(nr, ng, nb, nw)
        got = pal160.volts(nr, ng, nb, nw)
        for t, v in zip(tgt, got):
            worst = max(worst, abs(v - t * pal160.VMAX) / pal160.VMAX)
    return bytes(blob), bytes(pal), lay, worst


def make_inc(name, src, block, first_page, ncell, lay, pal, pal_rgb):
    addr = block * 8192 + first_page * CELL_BYTES
    L = []
    L.append('; %s.INC -- generado por spr2cel.py a partir de %s. No editar a mano.' % (name, src))
    L.append(';')
    L.append('; Hoja de sprites del formato B (celdas de 8x8 a 4 bpp, 32 bytes): %s.BIN se carga en SHEET_ADDR.' % name)
    L.append('; La celda de la pagina p esta en bloque*8192 + p*32 = SHEET_ADDR + (p - SHEET_PAGE0)*32.')
    L.append(';')
    L.append('SHEET_BLOCK\tequ %d\t\t; POKE 2129' % block)
    L.append('SHEET_ADDR\tequ $%04X\t; direccion Z80 de la primera celda' % addr)
    L.append('SHEET_PAGE0\tequ %d\t\t; pagina de la primera celda' % first_page)
    L.append('SHEET_CELLS\tequ %d\t\t; celdas usadas (%s.BIN mide %d bytes)' % (ncell, name, ncell * CELL_BYTES))
    L.append('SHEET_END\tequ $%04X\t; primera direccion libre despues de la hoja' % (addr + ncell * CELL_BYTES))
    L.append(';')
    L.append('; Cada grafico: NOMBRE = pagina de su primera celda; NOMBRE_W / _H = celdas de ancho y alto; NOMBRE_N = fases;')
    L.append('; NOMBRE_STEP = celdas de una fase (W*H): la pagina de la fase f es NOMBRE + f*NOMBRE_STEP.')
    L.append('; Las celdas de un grafico van por filas: TL, TR, BL, BR en uno de 16x16 (paginas p, p+1, p+2, p+3).')
    L.append('; Con mas de una fase, ademas NOMBRE_0, NOMBRE_1, ... = pagina de la primera celda de cada fase.')
    L.append('')
    for nm, p0, w, h, fr, cells in lay:
        L.append('; %s: %dx%d celdas (%dx%d px), %d fase%s, paginas %d-%d' % (
            nm, w, h, w * 8, h * 8, fr, '' if fr == 1 else 's', cells[0][0], cells[-1][0]))
        L.append('%s\tequ %d' % (nm, p0))
        L.append('%s_W\tequ %d' % (nm, w))
        L.append('%s_H\tequ %d' % (nm, h))
        L.append('%s_N\tequ %d' % (nm, fr))
        L.append('%s_STEP\tequ %d' % (nm, w * h))
        if fr > 1:
            for f in range(fr):
                L.append('%s_%d\tequ %d' % (nm, f, p0 + f * w * h))
        L.append('')
    L.append('; pal16 de los sprites (el indice 0 es el transparente: nibble alto de POKE 2103 a 0).')
    L.append('; Por entrada: byte bajo {G,B} -> POKE 2064; byte alto {BRIGHT,R} -> POKE 2066 (escribe y avanza el puntero POKE 2063).')
    L.append('PAL16:')
    for i in range(16):
        lo, hi = pal[2 * i], pal[2 * i + 1]
        L.append('\tdefb $%02X,$%02X\t; %2d  RGB %3d %3d %3d' % (lo, hi, i, *(pal_rgb[i] if i < len(pal_rgb) else (0, 0, 0))))
    L.append('PAL16_END:')
    L.append('')
    return '\n'.join(L)


# ---------------------------------------------------------------------------------------------------------
# Comprobacion de ida y vuelta: se leen de nuevo los ficheros que se han escrito y se comparan con el PNG
# ---------------------------------------------------------------------------------------------------------
def check(png_path, def_path, out_dir, name, first_page, block, gamma):
    w, h, img, pal_rgb = pngio.read_indexed(png_path)
    groups = parse_def(open(def_path, encoding='utf-8').read())
    blob = open(os.path.join(out_dir, name + '.BIN'), 'rb').read()
    pal = open(os.path.join(out_dir, name + '.PAL'), 'rb').read()
    inc = open(os.path.join(out_dir, name + '.INC'), encoding='utf-8').read()
    lay, ncell = layout(groups, first_page)
    errs = []
    if len(blob) != ncell * CELL_BYTES:
        errs.append('%s.BIN mide %d y deberia medir %d' % (name, len(blob), ncell * CELL_BYTES))
    # 1) cada celda del .BIN decodificada = la celda del PNG; la direccion sale de la formula del hardware
    for nm, p0, cw, ch, fr, cells in lay:
        for page, cx, cy in cells:
            off = (block * 8192 + page * CELL_BYTES) - (block * 8192 + first_page * CELL_BYTES)
            if decode_cell(blob[off:off + CELL_BYTES]) != cell_pixels(img, cx, cy):
                errs.append('%s: la celda de la pagina %d no coincide con el PNG (%d,%d)' % (nm, page, cx, cy))
    # 2) la pal16: lo que decodifica el hardware (densidades -> tension) es lo que sale de pal160.solve del color del PNG
    if len(pal) != 32:
        errs.append('%s.PAL mide %d y deberia medir 32' % (name, len(pal)))
    for i in range(16):
        lo, hi = pal[2 * i], pal[2 * i + 1]
        ng, nb, nw, nr = lo >> 4, lo & 15, hi >> 4, hi & 15
        r, g, b = pal_rgb[i] if i < len(pal_rgb) else (0, 0, 0)
        exp = pal160.solve(*[(c / 255.0) ** gamma for c in (r, g, b)])
        if (nr, ng, nb, nw) != exp:
            errs.append('pal16[%d]: %s en el fichero, %s esperado' % (i, (nr, ng, nb, nw), exp))
    # 3) las etiquetas del .inc valen lo que dice el reparto
    lab = {}
    for m in re.finditer(r'^([A-Z][A-Z0-9_]*)\tequ\s+(\$?[0-9A-F]+)', inc, re.M):
        v = m.group(2)
        lab[m.group(1)] = int(v[1:], 16) if v.startswith('$') else int(v)
    for nm, p0, cw, ch, fr, cells in lay:
        want = {nm: p0, nm + '_W': cw, nm + '_H': ch, nm + '_N': fr, nm + '_STEP': cw * ch}
        for f in range(fr if fr > 1 else 0):
            want['%s_%d' % (nm, f)] = p0 + f * cw * ch
        for k, v in want.items():
            if lab.get(k) != v:
                errs.append('etiqueta %s = %s en el .inc, %s esperado' % (k, lab.get(k), v))
    if lab.get('SHEET_ADDR') != block * 8192 + first_page * CELL_BYTES or lab.get('SHEET_CELLS') != ncell:
        errs.append('SHEET_ADDR / SHEET_CELLS no coinciden')
    pal_inc = re.findall(r'defb \$([0-9A-F]{2}),\$([0-9A-F]{2})', inc)
    if bytes(int(v, 16) for p in pal_inc for v in p) != pal:
        errs.append('la pal16 del .inc no es la del .PAL')
    return errs


def main():
    ap = argparse.ArgumentParser(description='PNG de sprites -> celdas del formato B, pal16 y .inc')
    ap.add_argument('png')
    ap.add_argument('-d', '--def', dest='deff', help='lista de graficos (por omision, el PNG con extension .def)')
    ap.add_argument('-o', '--out', default='.', help='carpeta de salida')
    ap.add_argument('--name', default='HOJA', help='nombre de los ficheros de salida (sin "_"; por omision HOJA)')
    ap.add_argument('--block', type=int, default=3, help='bloque de 8 KB de la hoja, POKE 2129 (2-7; por omision 3)')
    ap.add_argument('--first-page', type=int, default=128, help='pagina de la primera celda (por omision 128)')
    ap.add_argument('--gamma', type=float, default=1.0, help='v = (c/255) ** gamma antes de pasar a densidades')
    ap.add_argument('--check', action='store_true', help='comprobacion de ida y vuelta de lo que se ha escrito')
    a = ap.parse_args()
    if not 2 <= a.block <= 7:
        ap.error('el bloque va de 2 a 7')
    deff = a.deff or os.path.splitext(a.png)[0] + '.def'
    try:
        w, h, img, pal_rgb = pngio.read_indexed(a.png)
        groups = parse_def(open(deff, encoding='utf-8').read())
        blob, pal, lay, worst = build(img, w, h, pal_rgb, groups, a.first_page, a.gamma)
    except (pngio.PngError, DefError, OSError) as e:
        print('spr2cel: %s' % e, file=sys.stderr)
        return 1
    ncell = len(blob) // CELL_BYTES
    os.makedirs(a.out, exist_ok=True)
    open(os.path.join(a.out, a.name + '.BIN'), 'wb').write(blob)
    open(os.path.join(a.out, a.name + '.PAL'), 'wb').write(pal)
    open(os.path.join(a.out, a.name + '.INC'), 'w', encoding='utf-8', newline='\n').write(
        make_inc(a.name, os.path.basename(a.png), a.block, a.first_page, ncell, lay, pal, pal_rgb))
    empty = sum(1 for i in range(ncell) if not any(blob[i * 32:i * 32 + 32]))
    print('%s: %d graficos, %d celdas (paginas %d-%d, %d bytes en $%04X-$%04X), %d vacias; desviacion maxima de la pal16 %.1f %%' % (
        a.name, len(lay), ncell, a.first_page, a.first_page + ncell - 1, len(blob),
        a.block * 8192 + a.first_page * 32, a.block * 8192 + a.first_page * 32 + len(blob) - 1, empty, worst * 100))
    if a.check:
        errs = check(a.png, deff, a.out, a.name, a.first_page, a.block, a.gamma)
        for e in errs:
            print('FALLO: ' + e, file=sys.stderr)
        if errs:
            return 2
        print('comprobacion de ida y vuelta: correcta (%d celdas, pal16 y %d etiquetas)' % (ncell, sum(5 + (fr if fr > 1 else 0) for _, _, _, _, fr, _ in lay)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
