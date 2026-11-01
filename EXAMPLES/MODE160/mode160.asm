; =============================================================
; MODE160.ASM -- prueba del modo de video 160x100 a 8 bpp con paleta y PDM
;
; FPGA de la rama fpga-mode160 (POKE 2045,175; ver claude/planning/pdm_color.md y FPGA/SD81V2.1000/mode160.v).
;
;   imagen   160x100 bytes en $8000-$BE7F (HFILE = $8000), un byte por pixel = indice de la paleta
;   paleta   256 entradas de 16 bits, POKE 2063 (puntero), 2064 (byte bajo {G,B}) y 2065 (alto {BRIGHT,R}; escribe y avanza);
;            cada campo es la densidad de pulsos 0-15 de su pin; la tabla sale de pal160.py (INCBIN)
;   posicion POKE 2099,n (0-15) mueve la imagen en horizontal, un paso = 1/3 de pixel
;
; Dibujo: 16 filas de 16 bloques de 10x4 pixeles (indices 0-255: toda la paleta, por filas), y debajo cuatro barras de
; 16 pasos de 9 lineas (grises, rojo, verde, azul: las cuatro primeras filas de la paleta). Filas de la paleta:
;   0 grises   1-3 rojo, verde, azul   4-6 amarillo, cian, magenta   7 los 16 colores de Chroma81
;   8-15 rueda de 16 tonos con la saturacion creciente
;
; Teclas:  P / O = imagen a la derecha / izquierda (POKE 2099)      X = salir a BASIC
;
; Ensamblar: python pal160.py && pasmo mode160.asm mode160.bin mode160.sym   (compila.bat)
; Cargar desde BASIC: MODE160.B81 (LOAD FAST "MODE160.BIN" CODE 30000)
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

        ; --- paleta ---
        xor a
        ld (2063),a
        ld hl,paltab
        ld b,0                  ; 256 entradas
pl:     ld a,(hl)
        ld (2064),a
        inc hl
        ld a,(hl)
        ld (2065),a
        inc hl
        djnz pl

        ; --- imagen ---
        ld hl,8000h
        ld d,0                  ; fila 0-99
row:    ld a,d
        cp 64
        jr nc,bars
        rrca
        rrca
        and 0fh                 ; fila de la paleta = fila / 4
        jr haverow
bars:   sub 64                  ; las barras: 9 lineas cada una
        ld e,0
b1:     cp 9
        jr c,b2
        sub 9
        inc e
        jr b1
b2:     ld a,e                  ; 0-3
haverow:
        add a,a
        add a,a
        add a,a
        add a,a
        ld c,a                  ; C = fila de la paleta * 16
        ld e,0                  ; E = columna de la paleta (0-15)
seg:    ld a,c
        add a,e
        ld b,10
fill:   ld (hl),a
        inc hl
        djnz fill
        inc e
        ld a,e
        cp 16
        jr nz,seg
        inc d
        ld a,d
        cp 100
        jr nz,row

        ; --- modo 160x100 ---
        ld a,175
        ld (2045),a

; -------------------------------------------------------------
; Bucle: P / O mueven la imagen, X sale
; -------------------------------------------------------------
loop:   call waitvs
        ld a,0feh
        in a,(0feh)
        bit 2,a                 ; X
        jr z,quit
        ld a,0dfh
        in a,(0feh)
        bit 0,a                 ; P: a la derecha
        jr nz,k1
        ld a,(xadj)
        cp 15
        jr z,k1
        inc a
        jr setx
k1:     ld a,0dfh
        in a,(0feh)
        bit 1,a                 ; O: a la izquierda
        jr nz,loop
        ld a,(xadj)
        or a
        jr z,loop
        dec a
setx:   ld (xadj),a
        ld (2099),a
        ld b,6                  ; repeticion: cada 6 cuadros
sx1:    call waitvs
        djnz sx1
        jr loop

quit:   ld a,85
        ld (2045),a             ; modo ZX81 normal
        ret

; -------------------------------------------------------------
; waitvs: espera el flanco de subida del VSYNC (bit 0, puerto $AF)
; -------------------------------------------------------------
waitvs: in a,(0afh)
        rrca
        jr c,waitvs
wvs2:   in a,(0afh)
        rrca
        jr nc,wvs2
        ret

xadj:   defb 8                  ; el valor por defecto de la FPGA

paltab: incbin "pal160.bin"
