#!/usr/bin/env python3
# gen_font6.py -- genera la fuente comprimida de 6 px (96 caracteres, 32..127,
# 8 bytes cada uno = 768 bytes, mismo formato que specfont.bin) aplicando el
# mismo criterio que p42_putchar/p42_calcchar del explorador de 42 columnas
# (explorer.asm anterior al commit 34c8019):
#   - whichcolumn[c-32] >= 32: mascara m; por cada fila del glifo de 8x8:
#         out = (row & m) | ((row << 1) & ~m & 0xFF)
#     (la parte izquierda se queda tal cual, la derecha se desplaza una
#     columna a la izquierda: se elimina una columna)
#   - whichcolumn[c-32] < 32: indice en p42_characters (glifo de 8 bytes
#     ya comprimido, se usa tal cual)
import re, sys

asm_path, font_path, out_path = sys.argv[1], sys.argv[2], sys.argv[3]

def num(tok):
    tok = tok.strip()
    if tok.lower().endswith('h'):
        return int(tok[:-1], 16)
    if tok.startswith('$'):
        return int(tok[1:], 16)
    return int(tok, 0)

lines = open(asm_path, encoding='utf-8', errors='replace').read().splitlines()

def table(label):
    out, on = [], False
    for ln in lines:
        s = ln.split(';')[0].strip()
        if re.match(r'^%s:' % label, ln):
            on = True
            continue
        if not on:
            continue
        if re.match(r'^[A-Za-z_][A-Za-z0-9_]*:', ln) and not s.startswith('defb'):
            break
        m = re.match(r'^\s*defb\s+(.*)$', s)
        if m:
            out += [num(t) for t in m.group(1).split(',')]
        elif s and not s.startswith('defb'):
            if out:
                break
    return out

which = table('whichcolumn')
chars = table('p42_characters')
font = open(font_path, 'rb').read()
assert len(font) == 768, len(font)
assert len(which) == 96, "whichcolumn tiene %d entradas" % len(which)
assert len(chars) % 8 == 0, "p42_characters: %d bytes" % len(chars)
print("whichcolumn: %d entradas, p42_characters: %d glifos" % (len(which), len(chars) // 8))

out = bytearray()
for i in range(96):
    m = which[i]
    if m >= 32:
        g = font[i * 8:i * 8 + 8]
        out += bytes(((b & m) | ((b << 1) & (~m & 0xFF))) & 0xFF for b in g)
    elif m * 8 + 8 <= len(chars):
        out += bytes(chars[m * 8:m * 8 + 8])
    else:
        # whichcolumn apunta a un glifo que NO existe en p42_characters (el '_'
        # usa el indice 6 y la tabla solo tiene 6 glifos, 0-5: en el explorador
        # original se pintaban 8 bytes de basura de detras de la tabla). Aqui
        # se usa el glifo de specfont recortado a los 6 pixeles de la izquierda.
        print("AVISO: %r (indice %d inexistente) -> glifo de specfont & 0xFC" % (chr(32 + i), m))
        out += bytes(b & 0xFC for b in font[i * 8:i * 8 + 8])
assert len(out) == 768
open(out_path, 'wb').write(out)

low = [chr(32 + i) for i in range(96) if any(b & 3 for b in out[i * 8:i * 8 + 8])]
print("generado", out_path, len(out), "bytes")
print("caracteres con bits 1-0 (fuera de los 6 pixeles) a 1:", ''.join(low) or "ninguno")
