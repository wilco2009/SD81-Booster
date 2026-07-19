#!/usr/bin/env python3
# extract_bg.py -- extrae filas sueltas de una captura de pantalla Spectrum
# (.scr, 6912 bytes: 6144 bitmap + 768 atributos) a recursos binarios que
# explorer.asm incluye con INCBIN (ver bg_row0.bin/bg_row1.bin/bg_row23.bin
# y la rutina blit_row).
#
# Formato de salida por fila (288 bytes, "lineal", NO el formato Spectrum
# nativo con los "tercios"): 256 bytes de bitmap (8 scanlines de 32 bytes
# SEGUIDOS, linea0..linea7) + 32 bytes de atributo. blit_row en explorer.asm
# ya sabe convertir esto a las direcciones reales de pantalla (que si usan
# el formato Spectrum con tercios) al pegarlo.
#
# Uso: python3 extract_bg.py explorer.scr 0 bg_row0.bin
#      python3 extract_bg.py explorer.scr 1 bg_row1.bin
#      python3 extract_bg.py explorer.scr 23 bg_row23.bin
#
# Si se rediseña la captura (p.ej. nuevos iconos en la fila 23), basta con
# regenerar el .scr y volver a correr este script para el mismo numero de
# fila; el .bin resultante sustituye directamente al anterior.

import sys

def bitmap_addr_native(x_byte, y):
    # direccion dentro del bitmap Spectrum nativo (6144 bytes), formato
    # de "tercios": ver comentario de calc_bmp_addr en explorer.asm.
    third = y // 64
    within = y % 64
    row_in_third = within // 8
    line = within % 8
    return third * 2048 + line * 256 + row_in_third * 32 + x_byte

def extract_row(data, row):
    assert len(data) == 6912, "se esperaba un .scr de 6912 bytes"
    out = bytearray()
    for line in range(8):
        y = row * 8 + line
        for xb in range(32):
            out.append(data[bitmap_addr_native(xb, y)])
    attr_off = 6144 + row * 32
    out += data[attr_off:attr_off + 32]
    assert len(out) == 288
    return bytes(out)

def main():
    if len(sys.argv) != 4:
        print("uso: extract_bg.py <entrada.scr> <fila 0-23> <salida.bin>")
        sys.exit(1)
    scr_path, row_str, out_path = sys.argv[1], sys.argv[2], sys.argv[3]
    row = int(row_str)
    with open(scr_path, "rb") as f:
        data = f.read()
    chunk = extract_row(data, row)
    with open(out_path, "wb") as f:
        f.write(chunk)
    print(f"fila {row} -> {out_path} ({len(chunk)} bytes)")

if __name__ == "__main__":
    main()
