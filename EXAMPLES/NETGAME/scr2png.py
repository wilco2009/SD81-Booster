#!/usr/bin/env python
# Convierte un fichero .SCR del Spectrum (6912 bytes: bitmap entrelazado + atributos) en un PNG,
# con la paleta del Spectrum (el brillo solo afecta a los colores distintos del negro).
#   python scr2png.py pantalla.scr salida.png [escala] [ROM]
# Con una ROM (por ejemplo ../../z80rom/SDBOOST.ROM) sustituye la fuente sintetica de las pruebas del PC
# (claude/mqtttest) por la de verdad ($1E00), para ver los textos como saldran en la maquina.
import struct, sys, zlib

PAL = {0: (0, 0, 0), 1: (0, 0, 205), 2: (205, 0, 0), 3: (205, 0, 205),
       4: (0, 205, 0), 5: (0, 205, 205), 6: (205, 205, 0), 7: (205, 205, 205)}
PALB = {0: (0, 0, 0), 1: (0, 0, 255), 2: (255, 0, 0), 3: (255, 0, 255),
        4: (0, 255, 0), 5: (0, 255, 255), 6: (255, 255, 0), 7: (255, 255, 255)}

def write_png(path, w, h, px):
    raw = b''.join(b'\x00' + bytes(sum(([r, g, b] for (r, g, b) in px[y * w:(y + 1) * w]), [])) for y in range(h))
    def chunk(t, d):
        c = struct.pack('>I', len(d)) + t + d
        return c + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) +
                           chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))

def main():
    src, dst = sys.argv[1], sys.argv[2]
    scale = int(sys.argv[3]) if len(sys.argv) > 3 else 3
    d = bytearray(open(src, 'rb').read())
    if len(sys.argv) > 4:
        rom = open(sys.argv[4], 'rb').read()
        for r in range(24):
            for c in range(32):
                base = ((r & 0x18) << 8) | ((r & 7) << 5) | c
                cell = [d[base + (k << 8)] for k in range(8)]
                for code in range(1, 64):
                    if cell == [(code * 3 + 1 + k * 17) & 0xFF for k in range(8)]:
                        for k in range(8):
                            d[base + (k << 8)] = rom[0x1E00 + code * 8 + k]
                        break
        # las coordenadas del ajedrez van centradas: letras desplazadas 4 puntos en horizontal (fila 17) y cifras
        # desplazadas 4 puntos en vertical (columna 2), asi que no coinciden con una celda entera
        def cb(r, c, k):
            return d[((r & 0x18) << 8) | ((r & 7) << 5) | c | (k << 8)]
        def setb(r, c, k, v):
            d[((r & 0x18) << 8) | ((r & 7) << 5) | c | (k << 8)] = v
        def syn(code, k):
            return (code * 3 + 1 + k * 17) & 0xFF
        for f in range(8):
            r, c = 17, 3 + 2 * f
            for code in range(1, 64):
                if all(cb(r, c, k) == (syn(code, k) >> 4) and cb(r, c + 1, k) == ((syn(code, k) << 4) & 0xF0) for k in range(8)):
                    for k in range(8):
                        g = rom[0x1E00 + code * 8 + k]
                        setb(r, c, k, g >> 4)
                        setb(r, c + 1, k, (g << 4) & 0xF0)
                    break
        for y in range(8):
            r, c = 1 + 2 * y, 2
            for code in range(1, 64):
                if (all(cb(r, c, 4 + k) == syn(code, k) and cb(r + 1, c, k) == syn(code, 4 + k) for k in range(4))):
                    for k in range(4):
                        setb(r, c, 4 + k, rom[0x1E00 + code * 8 + k])
                        setb(r + 1, c, k, rom[0x1E00 + code * 8 + 4 + k])
                    break
    bitmap, attrs = d[:6144], d[6144:6912]
    img = []
    for y in range(192):
        row = []
        base = ((y & 0xC0) << 5) | ((y & 7) << 8) | ((y & 0x38) << 2)
        for cx in range(32):
            b = bitmap[base + cx]
            a = attrs[(y >> 3) * 32 + cx]
            br = bool(a & 0x40) and True
            ink, paper = a & 7, (a >> 3) & 7
            for bit in range(8):
                on = (b >> (7 - bit)) & 1
                c = ink if on else paper
                row.append((PALB if (br and c) else PAL)[c])
        img.append(row)
    flat = []
    for row in img:
        for _ in range(scale):
            for p in row:
                flat.extend([p] * scale)
    write_png(dst, 256 * scale, 192 * scale, flat)

if __name__ == '__main__':
    main()
