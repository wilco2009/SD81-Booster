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

def extract_row(data, row, col0=0, col1=32):
    # col0/col1: rango de columnas de atributo (0-31, col1 exclusivo) a
    # extraer -- por defecto la fila completa (formato usado por
    # bg_row0.bin/bg_row23.bin y blit_row). Un rango mas estrecho produce
    # un recurso mas pequeño (formato usado por los iconos del panel de
    # configuracion y blit_cols): width*8 bytes de bitmap (8 scanlines de
    # width bytes) + width bytes de atributo.
    assert len(data) == 6912, "se esperaba un .scr de 6912 bytes"
    width = col1 - col0
    out = bytearray()
    for line in range(8):
        y = row * 8 + line
        for xb in range(col0, col1):
            out.append(data[bitmap_addr_native(xb, y)])
    attr_off = 6144 + row * 32 + col0
    out += data[attr_off:attr_off + width]
    assert len(out) == width * 9
    return bytes(out)

def main():
    if len(sys.argv) not in (4, 6):
        print("uso: extract_bg.py <entrada.scr> <fila 0-23> <salida.bin> [col_inicial col_final]")
        print("     (columnas de atributo 0-31, col_final exclusivo; por defecto 0 32 = fila completa)")
        sys.exit(1)
    scr_path, row_str, out_path = sys.argv[1], sys.argv[2], sys.argv[3]
    row = int(row_str)
    col0, col1 = (int(sys.argv[4]), int(sys.argv[5])) if len(sys.argv) == 6 else (0, 32)
    with open(scr_path, "rb") as f:
        data = f.read()
    chunk = extract_row(data, row, col0, col1)
    with open(out_path, "wb") as f:
        f.write(chunk)
    print(f"fila {row} cols {col0}-{col1} -> {out_path} ({len(chunk)} bytes)")

if __name__ == "__main__":
    main()
