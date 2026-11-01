; =============================================================
; MODE160W.ASM -- prueba del modo 160x200 a 4 bpp con ventana de imagen, sprites y texto 40x25 (POKE 2045,176)
;
; FPGA de la rama fpga-mode160-4bpp (FPGA/SD81V2.1000/mode160.v). SOLO funciona en la placa con esa FPGA.
;
;   ventana  POKE 2070,primera y POKE 2071,ultima+1 (filas de texto de 8 lineas): la imagen solo ocupa esas filas; fuera sale el borde
;   imagen   4 bpp, 80 bytes por fila, desde HFILE = $8000; barras de 16 colores en la mitad de arriba de la ventana y diagonales abajo
;   texto    codigos 40x25 justo detras de la imagen ($8000 + 640 * filas de la ventana); glifos de la tabla de 256 caracteres de
;            ROMTABLE (registro I = $78: tabla de 2 KB en $7800): se copia el juego de caracteres de la ROM (ZX81, $1E00) en los
;            codigos 64-127 (normales) y 192-255 (el bit 7 los invierte); un codigo < 32 es una celda transparente
;   sprites  0: anillo que se mueve con Q A O P; 1 y 2: cuadrados en las esquinas de la ventana (X/Y de sprite = pixel + 32,
;            la fila 0 es la primera linea de la ventana)
;
; Teclas:  1 = ventana 1-22 (160x168, como AGI)    2 = ventana 3-17 (160x112)    T = texto si / no
;          Q A O P = mueven el anillo      X = salir a BASIC
;
; Ensamblar: pasmo mode160w.asm mode160w.bin mode160w.sym   (compila.bat; el binario debe acabar antes de $7A00)
; Cargar desde BASIC: MODE160W.B81 (LOAD FAST "MODE160W.BIN" CODE 30000)
; =============================================================

SPR_SEL equ 2100
SPR_EN  equ 2101
SPR_XL  equ 2102
SPR_XH  equ 2103
SPR_Y   equ 2104
SPR_COL equ 2105                ; 8 filas
SPR_PIX equ 2113                ; 8 filas
SPR_MSK equ 2121                ; 8 filas

        org 30000

start:  di
        xor a
        ld (2043),a
        ld a,80h
        ld (2044),a             ; HFILE = $8000
        ld bc,7fefh
        ld a,20h
        out (c),a               ; color activado, borde negro

        ld hl,1e00h             ; juego de caracteres de la ROM (64 x 8 bytes)
        ld de,7a00h             ; codigos 64-127
        ld bc,200h
        ldir
        ld hl,1e00h
        ld de,7e00h             ; codigos 192-255 (el bit 7 los invierte)
        ld bc,200h
        ldir
        ld a,78h
        ld i,a                  ; ROMTABLE[15:11] = tabla en $7800

        ld a,0fh
        ld (2068),a             ; texto: papel 0, tinta 15
        ld a,1
        ld (txton),a
        ld (2069),a             ; texto activado
        xor a
        call setwin             ; ventana 1-22
        ld a,176
        ld (2045),a

main:   call waitvs
        call movspr
        call keys
        jr main

quit:   ld a,85
        ld (2045),a             ; modo ZX81 normal
        xor a
        ld (2069),a             ; sin texto
        ld (2070),a             ; ventana por omision: 0 y 25
        ld a,25
        ld (2071),a
        ld c,0
qs:     ld a,c
        ld (SPR_SEL),a
        xor a
        ld (SPR_EN),a
        inc c
        ld a,c
        cp 3
        jr nz,qs
        ld a,1eh
        ld i,a                  ; la tabla de caracteres de la ROM otra vez
        ret

; -------------------------------------------------------------
waitvs: in a,(0afh)
        rrca
        jr c,waitvs
wvs2:   in a,(0afh)
        rrca
        jr nc,wvs2
        ret

; -------------------------------------------------------------
; setwin: A = preset (0 = ventana 1-22, 1 = ventana 3-17); programa la ventana y redibuja imagen, texto y sprites
; -------------------------------------------------------------
setwin: ld hl,presets
        ld de,7
        or a
        jr z,sw0
sw1:    add hl,de
        dec a
        jr nz,sw1
sw0:    ld a,(hl)
        ld (2070),a             ; primera fila con imagen
        inc hl
        ld a,(hl)
        ld (2071),a             ; ultima + 1
        inc hl
        ld a,(hl)
        ld (nrows),a
        inc hl
        ld e,(hl)
        inc hl
        ld d,(hl)
        inc hl
        ld (cb),de              ; base de los codigos del texto
        ld e,(hl)
        inc hl
        ld d,(hl)
        ld (wstr),de
        ld a,72
        ld (sx),a
        ld a,40
        ld (sy),a
        call genimg
        call gentxt
        jp defspr

; -------------------------------------------------------------
; genimg: nrows*8 lineas de imagen; la mitad de arriba, 16 barras de 10 pixeles; la de abajo, diagonales
; -------------------------------------------------------------
genimg: ld a,(nrows)
        add a,a
        add a,a
        add a,a
        ld (lines),a
        srl a
        ld (half),a
        ld hl,8000h
        ld d,0                  ; D = linea
gl:     ld a,(lines)
        cp d
        ret z
        ld a,(half)
        ld c,a
        ld a,d
        cp c
        jr nc,gdiag
        ld e,0                  ; barras: E = color
gb:     ld a,e
        add a,a
        add a,a
        add a,a
        add a,a
        or e
        ld b,5
gb2:    ld (hl),a
        inc hl
        djnz gb2
        inc e
        ld a,e
        cp 16
        jr nz,gb
        jr gnext
gdiag:  ld e,0                  ; diagonal: pixel = (x + linea) mod 16
gd:     ld a,e
        add a,a
        add a,d
        and 0fh
        ld c,a
        add a,a
        add a,a
        add a,a
        add a,a
        ld b,a
        ld a,c
        inc a
        and 0fh
        or b
        ld (hl),a
        inc hl
        inc e
        ld a,e
        cp 80
        jr nz,gd
gnext:  inc d
        jr gl

; -------------------------------------------------------------
; gentxt: codigos del texto (1.000 bytes): todo transparente, barra de estado en inverso, tres lineas abajo y una caja
; -------------------------------------------------------------
gentxt: ld hl,(cb)
        ld d,h
        ld e,l
        inc de
        ld (hl),0
        ld bc,999
        ldir
        ld b,0
        ld d,192
        call rowfill            ; fila 0 en inverso
        ld hl,s_title
        ld b,0
        ld c,1
        ld d,192
        call puts
        ld hl,(wstr)
        ld b,0
        ld c,14
        ld d,192
        call puts
        ld b,22
        ld d,64
        call rowfill
        ld b,23
        ld d,64
        call rowfill
        ld b,24
        ld d,64
        call rowfill
        ld hl,s_l1
        ld b,22
        ld c,0
        ld d,64
        call puts
        ld hl,s_l2
        ld b,23
        ld c,0
        ld d,64
        call puts
        ld b,8                  ; caja opaca sobre la imagen: filas 8-10, columnas 6-33
gtb:    push bc
        ld c,6
        ld d,64
        ld e,28
        call fillrun
        pop bc
        inc b
        ld a,b
        cp 11
        jr nz,gtb
        ld hl,s_box
        ld b,9
        ld c,8
        ld d,64
        jp puts

; dest: B = fila, C = columna -> HL = (cb) + fila*40 + columna
dest:   ld hl,(cb)
        ld de,40
        ld a,b
        or a
        jr z,d0
d1:     add hl,de
        dec a
        jr nz,d1
d0:     ld e,c
        ld d,0
        add hl,de
        ret

; rowfill: B = fila, D = codigo -> toda la fila
rowfill: ld c,0
        ld e,40
; fillrun: B = fila, C = columna, D = codigo, E = cantidad
fillrun: push de
        call dest
        pop de
        ld a,d
fr:     ld (hl),a
        inc hl
        dec e
        jr nz,fr
        ret

; puts: HL = cadena ASCII acabada en 0, B = fila, C = columna, D = 64 (normal) o 192 (inverso)
puts:   ld a,d
        ld (padd),a
        push hl
        call dest
        ex de,hl
        pop hl
pl:     ld a,(hl)
        or a
        ret z
        inc hl
        call a2z
        ld b,a
        ld a,(padd)
        add a,b
        ld (de),a
        inc de
        jr pl

; a2z: ASCII -> codigo de caracter del ZX81 (espacio 0, ':' 14, '>' 18, '-' 22, '/' 24, '.' 27, digitos 28-37, letras 38-63)
a2z:    cp ' '
        jr nz,a1
        xor a
        ret
a1:     cp 'A'
        jr c,a2
        cp 'Z'+1
        jr nc,a2
        sub 27
        ret
a2:     cp '0'
        jr c,a3
        cp '9'+1
        jr nc,a3
        sub 20
        ret
a3:     cp '.'
        jr nz,a4
        ld a,27
        ret
a4:     cp ':'
        jr nz,a5
        ld a,14
        ret
a5:     cp '>'
        jr nz,a6
        ld a,18
        ret
a6:     cp '/'
        jr nz,a7
        ld a,24
        ret
a7:     cp '-'
        jr nz,a8
        ld a,22
        ret
a8:     ld a,15
        ret

; -------------------------------------------------------------
; Sprites
; -------------------------------------------------------------
defspr: ld b,0                  ; anillo
        ld a,(sx)
        add a,32
        ld e,a
        ld d,0
        ld a,(sy)
        add a,32
        ld c,a
        ld a,2
        ld hl,ring
        call spr_def
        ld b,1                  ; cuadrado arriba a la izquierda (pixel 0,0 de la ventana)
        ld de,32
        ld c,32
        ld a,1
        ld hl,full
        call spr_def
        ld b,2                  ; cuadrado abajo a la derecha
        ld de,32+152
        ld a,(nrows)
        add a,a
        add a,a
        add a,a
        add a,24                ; 32 + lineas - 8
        ld c,a
        ld a,4
        ld hl,full
        jp spr_def

; spr_def: B = sprite, DE = X (9 bits), C = Y, A = tinta (0-15), HL = dibujo (8 bytes). Papel negro, mascara llena.
spr_def: push af
        push hl
        ld a,b
        ld (SPR_SEL),a
        ld a,e
        ld (SPR_XL),a
        ld a,d
        ld (SPR_XH),a
        ld a,c
        ld (SPR_Y),a
        pop hl
        pop af
        add a,a
        add a,a
        add a,a
        add a,a
        push hl
        ld hl,SPR_COL
        ld b,8
sd1:    ld (hl),a
        inc hl
        djnz sd1
        pop hl
        ld de,SPR_PIX
        ld bc,8
        ldir
        ld hl,full
        ld de,SPR_MSK
        ld bc,8
        ldir
        ld a,1
        ld (SPR_EN),a
        ret

movspr: xor a
        ld (SPR_SEL),a
        ld a,(sx)
        add a,32
        ld (SPR_XL),a
        ld a,(sy)
        add a,32
        ld (SPR_Y),a
        ret

; -------------------------------------------------------------
; Teclado
; -------------------------------------------------------------
keys:   ld a,0feh
        in a,(0feh)
        bit 2,a                 ; X
        jp z,quit
        ld a,0fbh
        in a,(0feh)
        bit 0,a                 ; Q: arriba
        jr nz,k1
        ld a,(sy)
        cp 2
        jr c,k1
        sub 2
        ld (sy),a
k1:     ld a,0fdh
        in a,(0feh)
        bit 0,a                 ; A: abajo
        jr nz,k2
        ld a,(nrows)
        add a,a
        add a,a
        add a,a
        sub 10
        ld c,a
        ld a,(sy)
        cp c
        jr nc,k2
        add a,2
        ld (sy),a
k2:     ld a,0dfh
        in a,(0feh)
        bit 1,a                 ; O: izquierda
        jr nz,k3
        ld a,(sx)
        cp 2
        jr c,k3
        sub 2
        ld (sx),a
k3:     ld a,0dfh
        in a,(0feh)
        bit 0,a                 ; P: derecha
        jr nz,k4
        ld a,(sx)
        cp 150
        jr nc,k4
        add a,2
        ld (sx),a
k4:     ld a,0f7h
        in a,(0feh)
        bit 0,a                 ; 1: ventana 1-22
        jr nz,k5
        xor a
        call setwin
k4w:    ld a,0f7h
        in a,(0feh)
        bit 0,a
        jr z,k4w
k5:     ld a,0f7h
        in a,(0feh)
        bit 1,a                 ; 2: ventana 3-17
        jr nz,k6
        ld a,1
        call setwin
k5w:    ld a,0f7h
        in a,(0feh)
        bit 1,a
        jr z,k5w
k6:     ld a,0fbh
        in a,(0feh)
        bit 4,a                 ; T: texto si / no
        ret nz
        ld a,(txton)
        xor 1
        ld (txton),a
        ld (2069),a
k6w:    ld a,0fbh
        in a,(0feh)
        bit 4,a
        jr z,k6w
        ret

; -------------------------------------------------------------
ring:   defb 0ffh,81h,81h,81h,81h,81h,81h,0ffh
full:   defb 0ffh,0ffh,0ffh,0ffh,0ffh,0ffh,0ffh,0ffh

; presets: primera, ultima+1, filas, base de los codigos ($8000 + 640*filas), texto de la barra de estado
presets: defb 1,22,21
        defw 8000h+13440
        defw p1str
        defb 3,17,14
        defw 8000h+8960
        defw p2str

p1str:  defm "WIN 1-22 168 LINES"
        defb 0
p2str:  defm "WIN 3-17 112 LINES"
        defb 0
s_title: defm "SD81 4BPP"
        defb 0
s_l1:   defm "> MOVE: Q A O P"
        defb 0
s_l2:   defm "1 2 WINDOW  T TEXT  X EXIT"
        defb 0
s_box:  defm "TEXT OVER THE IMAGE"
        defb 0

nrows:  defb 0
lines:  defb 0
half:   defb 0
txton:  defb 0
padd:   defb 0
sx:     defb 0
sy:     defb 0
cb:     defw 0
wstr:   defw 0
