; =============================================================
; MODE1604.ASM -- prueba del modo de video 160x200 a 4 bpp (POKE 2045,176)
;
; FPGA de la rama fpga-mode160-4bpp (FPGA/SD81V2.1000/mode160.v, entrada m4). SOLO funciona en la placa con esa FPGA.
;
;   imagen   160x200 pixeles, 80 bytes por fila, 16.000 bytes en $8000-$BE7F (HFILE = $8000); el nibble ALTO es el pixel
;            de la izquierda. Cada nibble es el indice (0-15) de la paleta de 16 (pal16), la misma de los demas modos:
;            POKE 2063 (puntero), 2064 (bajo {G,B}) y 2066 (alto {BRIGHT,R}; escribe y avanza). Por defecto, los 16
;            colores digitales de Chroma81 (indice = {BRIGHT,G,R,B}), asi que aqui no se carga nada.
;   Una fila por linea de TV (lineas 58 a 257), pixel de 3 ciclos de 13 MHz como en el modo de 8 bpp.
;
; Dibujo: arriba (100 filas) 16 barras verticales de 10 pixeles, una por color (0-15, de izquierda a derecha);
; abajo (100 filas) diagonales: pixel (x,y) = (x+y) mod 16 -> comprueba el orden de los nibbles y las filas.
;
; Teclas:  X = salir a BASIC
; Ensamblar: pasmo mode1604.asm mode1604.bin mode1604.sym   (compila.bat)   Cargar: MODE1604.B81
; Para una imagen propia: png2i4.py (PNG de 160x200 con hasta 16 colores -> pic4.bin + pal4.bin)
; =============================================================

        org 30000

start:  di
        xor a
        ld (2043),a
        ld a,80h
        ld (2044),a             ; HFILE = $8000
        ld bc,7fefh
        ld a,20h
        out (c),a               ; color activado, borde negro

        ld hl,8000h
        ld d,100
r1:     ld e,0                  ; E = color de la barra
c1:     ld a,e
        add a,a
        add a,a
        add a,a
        add a,a
        or e                    ; dos pixeles del mismo color
        ld b,5                  ; 5 bytes = 10 pixeles
f1:     ld (hl),a
        inc hl
        djnz f1
        inc e
        ld a,e
        cp 16
        jr nz,c1
        dec d
        jr nz,r1

        ld d,0                  ; D = fila (0-99)
r2:     ld e,0                  ; E = byte de la fila (0-79)
c2:     ld a,e
        add a,a
        add a,d
        and 0fh                 ; pixel izquierdo = (2*E + fila) mod 16
        ld c,a
        add a,a
        add a,a
        add a,a
        add a,a
        ld b,a                  ; nibble alto
        ld a,c
        inc a
        and 0fh                 ; pixel derecho = el siguiente
        or b
        ld (hl),a
        inc hl
        inc e
        ld a,e
        cp 80
        jr nz,c2
        inc d
        ld a,d
        cp 100
        jr nz,r2

        ld a,176
        ld (2045),a             ; modo 160x200 a 4 bpp

loop:   call waitvs
        ld a,0feh
        in a,(0feh)
        bit 2,a                 ; X
        jr nz,loop
        ld a,85
        ld (2045),a             ; modo ZX81 normal
        ret

waitvs: in a,(0afh)
        rrca
        jr c,waitvs
wvs2:   in a,(0afh)
        rrca
        jr nc,wvs2
        ret
