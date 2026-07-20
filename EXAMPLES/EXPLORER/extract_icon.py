#!/usr/bin/env python3
# extract_icon.py -- extrae un icono pequeño de una captura .scr de Spectrum
# y lo REPOSICIONA a nivel de pixel (no de columna de atributo de 8px),
# para poder alinearlo con texto dibujado con la fuente comprimida de 6px
# (ver p42_putchar en explorer.asm). El desplazamiento sub-byte se hace
# aqui en Python -- blit_cols en explorer.asm solo necesita pegar el
# recurso resultante en la columna de bytes que se indique, sin desplazar
# nada en tiempo de ejecucion.
#
# El icono debe ser de un solo color (ink/paper uniforme) -- se lee el
# atributo del primer pixel origen y se replica en todos los bytes de
# salida.
#
# Formato de salida (igual familia que extract_bg.py): anchura*8 bytes de
# bitmap (8 scanlines de "anchura" bytes) + anchura bytes de atributo.
#
# Uso: extract_icon.py <in.scr> <fila 0-23> <px_origen_inicio> <px_origen_fin> <px_destino_inicio> <out.bin>
#   fila: fila de caracter (0-23) donde esta el icono en el .scr
#   px_origen_inicio/fin: rango de pixeles (0-255, fin exclusivo) que ocupa
#     el icono tal cual esta dibujado
#   px_destino_inicio: pixel de pantalla (0-255) donde debe empezar al
#     pegarlo con blit_cols; el script calcula el desplazamiento sub-byte
#     necesario y la columna de bytes (px_destino_inicio // 8) que hay que
#     pasarle a blit_cols como D.

import sys

def bitmap_addr_native(x_byte, y):
    third = y // 64
    within = y % 64
    row_in_third = within // 8
    line = within % 8
    return third * 2048 + line * 256 + row_in_third * 32 + x_byte

def get_pixel(data, x, y):
    byte = data[bitmap_addr_native(x // 8, y)]
    return (byte >> (7 - (x % 8))) & 1

def get_attr(data, x, y):
    # BRIGHT (bit6) fuera: los iconos se pintan en cian normal, no brillante
    # (el cian brillante daba problemas -- ver historial de la sesion).
    return data[6144 + (y // 8) * 32 + (x // 8)] & ~0x40

def extract_icon(data, row, src_x0, src_x1, dst_x0):
    assert len(data) == 6912, "se esperaba un .scr de 6912 bytes"
    width_px = src_x1 - src_x0
    shift = dst_x0 % 8
    start_byte_col = dst_x0 // 8
    width_bytes = (shift + width_px + 7) // 8
    bmp = bytearray(width_bytes * 8)
    for line in range(8):
        y = row * 8 + line
        for i in range(width_px):
            if get_pixel(data, src_x0 + i, y):
                bitpos = shift + i
                byte_idx, bit_idx = bitpos // 8, 7 - (bitpos % 8)
                bmp[line * width_bytes + byte_idx] |= (1 << bit_idx)
    attr_val = get_attr(data, src_x0, row * 8)
    return bytes(bmp) + bytes([attr_val] * width_bytes), start_byte_col, width_bytes

def main():
    if len(sys.argv) != 7:
        print("uso: extract_icon.py <in.scr> <fila 0-23> <px_ini> <px_fin> <px_destino> <out.bin>")
        sys.exit(1)
    scr_path, row_s, x0_s, x1_s, dst_s, out_path = sys.argv[1:7]
    with open(scr_path, "rb") as f:
        data = f.read()
    chunk, col, width_bytes = extract_icon(data, int(row_s), int(x0_s), int(x1_s), int(dst_s))
    with open(out_path, "wb") as f:
        f.write(chunk)
    print(f"{out_path}: {len(chunk)} bytes -- blit_cols con D={col}, B={width_bytes}")

if __name__ == "__main__":
    main()
