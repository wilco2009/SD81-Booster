#!/usr/bin/env python3
"""
Genera dual_ay_test.p — programa ZX81 que prueba los dos chips AY del SD81.

Estructura BASIC:
  1 REM [machine code]   ← código en REM, dirección = PROG+5 = 0x4081 = 16513
  2 RAND USR 16513       ← ejecuta el código

El código activa:
  Chip 0 (A3=1, puertos 0xCF/0x0F): Canal A a 440 Hz
  Chip 1 (A3=0, puertos 0xC6/0x06): Canal A a 880 Hz

Tokens ZX81 (de zx81BasicLister.cpp):
  REM  = 0xEA   RAND = 0xF9   USR = 0xD4
  NEWLINE = 0x76   número = 0x7E + 5-byte float

Uso: python make_dual_ay_p.py
"""

import struct, os

# ─────────────────────────────────────────────────────────────────────────────
# Código máquina Z80
# ─────────────────────────────────────────────────────────────────────────────
mc = bytes([
    # Chip 0 (A3=1, latch=0xCF, data=0x0F): 440 Hz en canal A
    0x3E, 0x00, 0xD3, 0xCF,  # LD A,0  / OUT (CFh),A  → latch reg 0
    0x3E, 0xFC, 0xD3, 0x0F,  # LD A,FCh/ OUT (0Fh),A  → tono fino = 252

    0x3E, 0x01, 0xD3, 0xCF,  # latch reg 1
    0xAF,       0xD3, 0x0F,  # XOR A   / OUT (0Fh),A  → tono grueso = 0

    0x3E, 0x07, 0xD3, 0xCF,  # latch reg 7 (mixer)
    0x3E, 0xF8, 0xD3, 0x0F,  # OUT F8h                → tono A activo, ruido off

    0x3E, 0x08, 0xD3, 0xCF,  # latch reg 8 (volumen A)
    0x3E, 0x0F, 0xD3, 0x0F,  # OUT 0Fh               → volumen máximo

    # Chip 1 (A3=0, latch=0xC6, data=0x06): 880 Hz en canal A
    0x3E, 0x00, 0xD3, 0xC6,
    0x3E, 0x7E, 0xD3, 0x06,  # tono fino = 126

    0x3E, 0x01, 0xD3, 0xC6,
    0xAF,       0xD3, 0x06,

    0x3E, 0x07, 0xD3, 0xC6,
    0x3E, 0xF8, 0xD3, 0x06,

    0x3E, 0x08, 0xD3, 0xC6,
    0x3E, 0x0F, 0xD3, 0x06,

    # Bucle infinito (chips siguen sonando autónomamente)
    0x76,        # HALT
    0x18, 0xFE,  # JR -2  (volver a HALT)
])
assert len(mc) == 65, f"MC size error: {len(mc)}"

# ─────────────────────────────────────────────────────────────────────────────
# Helpers
# ─────────────────────────────────────────────────────────────────────────────
DIGIT = [0x1C,0x1D,0x1E,0x1F,0x20,0x21,0x22,0x23,0x24,0x25]  # '0'..'9'

def zx81_float(n):
    """Entero positivo → 5 bytes float ZX81."""
    if n == 0:
        return bytes(5)
    p = n.bit_length() - 1          # posición del bit más significativo
    exp = (p + 1) + 128             # exponente biased
    mantissa = (n << (31 - p)) & 0x7FFFFFFF  # 31 bits, MSB implicit → sign=0
    return bytes([exp,
                  (mantissa >> 24) & 0xFF,
                  (mantissa >> 16) & 0xFF,
                  (mantissa >>  8) & 0xFF,
                   mantissa        & 0xFF])

def zx81_number(n):
    """Literal numérico en BASIC: dígitos ZX81 + marcador 0x7E + float."""
    digits = bytes(DIGIT[int(d)] for d in str(n))
    return digits + bytes([0x7E]) + zx81_float(n)

def basic_line(line_no, content):
    """Línea BASIC: line_no(2,be) + len(2,le) + content (incluye NEWLINE)."""
    body = content + bytes([0x76])  # NEWLINE al final
    return struct.pack('>H', line_no) + struct.pack('<H', len(body)) + body

# ─────────────────────────────────────────────────────────────────────────────
# Programa BASIC
# ─────────────────────────────────────────────────────────────────────────────
PROG = 0x407C   # dirección de inicio del programa en la RAM del ZX81

line1 = basic_line(1, bytes([0xEA]) + mc)         # 1 REM [mc]
line2 = basic_line(2, bytes([0xF9, 0xD4]) + zx81_number(PROG + 5))  # 2 RAND USR addr

# La dir del código = PROG + 2(line_no) + 2(len) + 1(REM) = PROG+5
assert PROG + 5 == 0x4081, f"addr check failed: {hex(PROG+5)}"
basic = line1 + line2

# ─────────────────────────────────────────────────────────────────────────────
# Mapa de memoria ZX81 desde 0x4009 (inicio del .p)
# ─────────────────────────────────────────────────────────────────────────────
VARS   = PROG  + len(basic)                     # vars justo después del BASIC
DFILE  = VARS  + 1                              # +1 por el marcador 0x80
ELINE  = DFILE + 25                             # 25 × 0x76 = display vacío
STKBOT = ELINE + 2                              # e_line = 0x7F 0x76

# Variables de sistema (0x4009 a 0x407B = 115 bytes en el fichero)
sysvar = bytearray(115)
def sv(addr, *vals):
    off = addr - 0x4009
    for i,v in enumerate(vals):
        sysvar[off+i] = v & 0xFF

sv(0x4009, 0x00)                    # VERSN
sv(0x400A, 0x00, 0x01)              # E_PPC = línea 1 (big-endian)
sv(0x400C, DFILE & 0xFF, DFILE >> 8)      # D_FILE
sv(0x400E, (DFILE+1) & 0xFF, (DFILE+1) >> 8)  # DF_CC = D_FILE+1
sv(0x4010, VARS & 0xFF, VARS >> 8)        # VARS
sv(0x4012, 0x00, 0x00)              # DEST
sv(0x4014, ELINE & 0xFF, ELINE >> 8)      # E_LINE
sv(0x4016, (ELINE+1) & 0xFF, (ELINE+1) >> 8)  # CH_ADD (apunta al NEWLINE en E_LINE)
sv(0x4018, 0x09, 0x40)              # X_PTR = 0x4009
sv(0x401A, STKBOT & 0xFF, STKBOT >> 8)   # STKBOT
sv(0x401C, STKBOT & 0xFF, STKBOT >> 8)   # STKEND = STKBOT (stack vacío)
sv(0x401E, 0x2E)                    # BREG
sv(0x401F, 0x00, 0x40)              # MEM = 0x4000
sv(0x4021, 0x00)
sv(0x4022, 0x02)                    # DF_SZ = 2
sv(0x4023, 0x01, 0x00)              # S_TOP = 1
sv(0x4025, 0xFF, 0xFF)              # LAST_K = ninguna tecla
sv(0x4027, 0x00)                    # DEBOUNCE
sv(0x4028, 0x37)                    # MARGIN = 55 (PAL)
sv(0x4029, PROG & 0xFF, PROG >> 8)  # NXTLIN = primera línea (sin autostart)
sv(0x402B, 0x00, 0x00)              # OLDPPC
sv(0x402D, 0x00)                    # FLAGX
sv(0x402E, 0x00, 0x00)              # STRLEN
sv(0x4030, 0x00, 0x00)              # T_ADDR
sv(0x4032, 0x00, 0x00)              # SEED
sv(0x4034, 0x00, 0x00)              # FRAMES
sv(0x4036, 0x00, 0x00)              # COORDS
sv(0x4038, 0x00)                    # PR_CC
sv(0x4039, 0x21, 0x18)              # S_POSN = col 33, fila 24
sv(0x403B, 0x00)                    # CDFLAG
# PRBUFF (0x403C, 33 bytes de 0x76)
for i in range(33): sysvar[0x403C - 0x4009 + i] = 0x76
# MEMBOT (0x405D, 30 bytes de 0x00) — ya 0 por defecto

# ─────────────────────────────────────────────────────────────────────────────
# Ensamblar el fichero .p
# ─────────────────────────────────────────────────────────────────────────────
# sysvar cubre exactamente 0x4009..0x407B (115 bytes), PROG empieza en 0x407C sin padding
p_data = (bytes(sysvar) +     # 0x4009..0x407B (115 bytes)
          basic +              # programa BASIC desde PROG=0x407C
          bytes([0x80]) +      # VARS end marker
          bytes([0x76] * 25) + # display file vacío
          bytes([0x7F, 0x76])) # E_LINE: cursor + newline

# Escribir fichero
out = os.path.join(os.path.dirname(__file__), 'dual_ay_test.p')
with open(out, 'wb') as f:
    f.write(p_data)

print(f"Generado: {out}  ({len(p_data)} bytes)")
print(f"  PROG    = 0x{PROG:04X} = {PROG}")
print(f"  MC addr = 0x{PROG+5:04X} = {PROG+5}  (RAND USR {PROG+5})")
print(f"  VARS    = 0x{VARS:04X}")
print(f"  D_FILE  = 0x{DFILE:04X}")
print(f"Cargar desde SDBOOST.ROM: LOAD \"dual_ay_test\"")
print(f"Ejecutar: RUN  (o RAND USR {PROG+5} directamente)")
