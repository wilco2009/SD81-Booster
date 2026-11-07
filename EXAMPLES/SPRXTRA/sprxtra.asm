; =============================================================
; SPRXTRA.ASM -- sprites de 4 bits por pixel y mas de 64 sprites
;
; Prueba las dos funciones nuevas del motor de sprites:
;  - formato B (POKE 2130 = pagina): 8x8 a 4 bits por pixel, hoja de
;    graficos en el bloque 3 (POKE 2129,3), animacion cambiando la pagina
;  - sprites extra (POKE 2131 = M): hasta 64 sprites mas, que no usan
;    POKE 2100: sus entradas de 32 bytes son las paginas 0..M-1 de la hoja
;    ($6000), con los mismos campos que la tabla, y se escriben como RAM.
;
; Modo 160x100 a 256 colores (POKE 2045,175). Lo primero que hace es poner
; el modo y cargar de fondo FONDO.JPG (del directorio actual) con el
; comando 76 del MCU, recortada para llenar la pantalla. Si no esta, o no se
; puede convertir, sigue con el fondo negro.
;
; En este modo los sprites van en pixeles de la imagen (8x8 de 160x100) y
; sus coordenadas son pixel + 32: X 32-191, Y 32-131. Los colores de los
; sprites salen de la paleta de 16 (pal16), no de la del fondo.
;
; Todo son bolas (mascara redonda, la misma en los dos formatos):
;   - formato B: arco iris en diagonal con 15 colores, animado (4 paginas)
;   - formato A: rayas horizontales, un color por fila
;
; Franjas de 8 filas desde la fila 2 (12 franjas):
;   0-4    los normales 0-59, 12 por franja, B, hacia la derecha
;   5      los normales 60-63 y los extra 0-7
;   6-10   los extra 8-48: 11, 9, 8, 7 y 6 por franja (cuantos mas atras
;          esta el extra, menos tiempo queda en la linea para cargarlo)
;   11     los extra 49-63, 15 B quietos: con 64 extra solo da tiempo a
;          cargar los primeros (los de numero mas bajo)
; Los extra pares son B y los impares A (los quietos, todos B); los que se
; mueven van hacia la izquierda.
;
; Teclas: E = numero de extra (0, 32, 48, 64; borde rojo, azul, magenta y
;         verde)   P = parar/mover   Q = salir
;
; Ensamblar: pasmo sprxtra.asm sprxtra.bin sprxtra.sym
; Cargar desde BASIC: SPRXTRA.B81 (LOAD FAST "SPRXTRA.BIN" CODE 30000)
; =============================================================

        org 30000

SPR_SEL   equ 2100
SPR_EN    equ 2101
SPR_XL    equ 2102
SPR_XH    equ 2103
SPR_Y     equ 2104
SPR_MASK  equ 2121
SPR_BASE  equ 2129
SPR_PAGE  equ 2130
SPR_EXTRA equ 2131
SHEET_BLK equ 3
XTBL      equ 6000h         ; pagina 0 del bloque 3: la entrada del extra 0
PALBUF    equ 6800h         ; paleta del fondo (512 bytes), detras de los extra
sheet     equ 7C00h         ; las 4 celdas del arco iris (128 bytes)
PG0       equ (sheet - 6000h) / 32
XSTATIC   equ 49            ; los extra desde este no se mueven
IMG       equ 8000h         ; imagen de 160x100, un byte por pixel
IMGLEN    equ 160*100
XMIN      equ 24            ; X de los que se mueven: 24..191, y vuelta
XRANGE    equ 168

        jp start

        include "../PHOTO/mcu.asm"

start:  di
        call bgload

        ld a,SHEET_BLK
        ld (SPR_BASE),a
        call mksheet
        call defall
        ld a,3                  ; empieza con 64 extra
        ld (mstep),a
        call setm

main:   call waitvs
        call anim
        call keys
        jr nz,main

; --- salida: sprites apagados y en formato A, sin extra, modo normal ---
quit:   ld b,64
        ld c,0
qs:     ld a,c
        ld (SPR_SEL),a
        xor a
        ld (SPR_EN),a
        ld (SPR_PAGE),a
        inc c
        djnz qs
        xor a
        ld (SPR_EXTRA),a
        ld a,2
        ld (SPR_BASE),a
        ld a,85
        ld (2045),a
        ret

; -------------------------------------------------------------
; bgload: el modo 160x100 con la imagen a negro y, si se puede, FONDO.JPG
; -------------------------------------------------------------
bgload: xor a                   ; HFILE = $8000
        ld (2043),a
        ld a,IMG/256
        ld (2044),a
        ld bc,7fefh             ; color Chroma81
        ld a,20h
        out (c),a
        ld hl,IMG               ; imagen a 0
        ld de,IMG+1
        ld bc,IMGLEN-1
        ld (hl),0
        ldir
        xor a                   ; color 0 de la paleta = negro
        ld (2063),a
        ld (2064),a
        ld (2065),a
        ld a,175                ; modo 160x100 a 8 bpp
        ld (2045),a
        ld hl,fname
        ld e,2                  ; recortada: llena la pantalla sin deformar
        call f_open_img
        cp 4
        ret nc                  ; no esta o no se puede: sin fondo
        ld (fh),a
        ld hl,hdr
        ld bc,4
        call f_read_n
        ld hl,hdr
        ld de,magic
        ld b,4
bgm:    ld a,(de)
        cp (hl)
        jr nz,bgbad
        inc hl
        inc de
        djnz bgm
        call blackpal           ; que no se vea a medias
        ld a,(fh)
        ld hl,PALBUF
        ld bc,512
        call f_read_n
        ld a,(fh)
        ld hl,IMG
        ld bc,IMGLEN
        call f_read_n
        ld a,(fh)
        call f_close
        jr setpal
bgbad:  ld a,(fh)
        jp f_close

; paleta: POKE 2063 (puntero), 2064 (byte bajo), 2065 (byte alto: escribe y avanza)
blackpal:
        xor a
        ld (2063),a
        ld b,0                  ; 256 entradas
bp1:    ld (2064),a
        ld (2065),a
        djnz bp1
        ret
setpal: xor a
        ld (2063),a
        ld hl,PALBUF
        ld b,0
sp1:    ld a,(hl)
        ld (2064),a
        inc hl
        ld a,(hl)
        ld (2065),a
        inc hl
        djnz sp1
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

; -------------------------------------------------------------
; mksheet: 4 celdas (fotogramas). El pixel (fila r, columna c) del
; fotograma f es el color ((r + c + 2f) mod 15) + 1: todos menos el 0.
; -------------------------------------------------------------
mksheet:
        ld hl,sheet
        ld e,0                  ; E = 2f
msf:    ld d,0                  ; D = fila
msr:    ld b,0                  ; B = columna
msb:    ld a,b
        call colr
        add a,a
        add a,a
        add a,a
        add a,a
        ld c,a                  ; pixel de la izquierda, nibble alto
        ld a,b
        inc a
        call colr
        or c
        ld (hl),a
        inc hl
        inc b
        inc b
        ld a,b
        cp 8
        jr c,msb
        inc d
        ld a,d
        cp 8
        jr c,msr
        inc e
        inc e
        ld a,e
        cp 8
        jr c,msf
        ret
colr:   add a,d
        add a,e
        cp 15
        jr c,cr1
        sub 15
cr1:    inc a
        ret

; -------------------------------------------------------------
; defall: los 128 sprites, franja a franja segun bandtab. El sprite g es
; el normal g (0-63) o el extra g-64 (64-127).
; -------------------------------------------------------------
defall: ld hl,XTBL
        ld de,XTBL+1
        ld bc,64*32-1
        ld (hl),0
        ldir                    ; entradas a 0 (Y = 0: no se ven)
        ld iy,bandtab
        xor a
        ld (band),a
        ld c,0                  ; C = g
df1:    ld a,(band)             ; Y = 32 + 2 + 8 * franja
        add a,a
        add a,a
        add a,a
        add a,32+2
        ld (cury),a
        ld a,(iy+2)
        ld (curx),a
        ld b,(iy+0)
df2:    push bc
        call setspr
        pop bc
        ld a,(curx)
        add a,(iy+1)
        cp XMIN+XRANGE
        jr c,df3
        sub XRANGE
df3:    ld (curx),a
        inc c
        djnz df2
        ld de,3
        add iy,de
        ld a,(band)
        inc a
        ld (band),a
        cp 12
        jr c,df1
        ret

; franjas: cuantos, separacion en X y X del primero
bandtab:
        defb 12,14,24, 12,14,29, 12,14,34, 12,14,39, 12,14,44
        defb 12,14,49           ; normales 60-63 y extra 0-7
        defb 11,15,54           ; extra 8-18
        defb 9,18,59            ; extra 19-27
        defb 8,21,64            ; extra 28-35
        defb 7,24,69            ; extra 36-42
        defb 6,28,74            ; extra 43-48
        defb 15,10,37           ; extra 49-63, quietos

; setspr: el sprite C en (curx, cury). Conserva C.
setspr: ld a,c
        cp 64
        jr nc,ssx
        ld (SPR_SEL),a          ; normal: por la ventana de POKEs
        ld e,c
        ld d,0
        ld hl,nx
        add hl,de
        ld a,(curx)
        ld (hl),a
        ld (SPR_XL),a
        xor a                   ; X < 256 y color transparente 0
        ld (SPR_XH),a
        ld a,(cury)
        ld (SPR_Y),a
        ld a,c
        and 3
        add a,PG0
        ld (SPR_PAGE),a
        ld hl,SPR_MASK
        call ballmsk
        ld a,1
        ld (SPR_EN),a
        ret
ssx:    sub 64                  ; extra: su entrada en XTBL
        ld l,a
        ld h,0
        add hl,hl
        add hl,hl
        add hl,hl
        add hl,hl
        add hl,hl
        ld de,XTBL
        add hl,de
        push hl
        pop ix
        ld a,(curx)
        ld (ix+1),a
        ld a,(cury)
        ld (ix+3),a
        ld a,c
        sub 64
        cp XSTATIC
        jr nc,ssb               ; los quietos, B
        bit 0,a
        jr nz,ssa               ; impares: formato A
ssb:    ld a,c
        and 3
        add a,PG0
        ld (ix+29),a
        jr ssm
ssa:    push ix                 ; formato A: color por fila y pixel lleno
        pop hl
        ld de,4
        add hl,de
        ex de,hl                ; DE = campo 4
        ld hl,acol
        push bc
        ld bc,8
        ldir                    ; campos 4-11: color
        ex de,hl                ; HL = campo 12
        ld b,8
ssa1:   ld (hl),0ffh            ; campos 12-19: pixel
        inc hl
        djnz ssa1
        pop bc
ssm:    push ix                 ; campos 20-27: mascara
        pop hl
        ld de,20
        add hl,de
                                ; sigue en ballmsk

; ballmsk: la mascara redonda en 8 bytes desde HL. Toca A, B y DE.
ballmsk:
        ld de,ballm
        ld b,8
bm1:    ld a,(de)
        ld (hl),a
        inc de
        inc hl
        djnz bm1
        ret

ballm:  defb 3ch,7eh,0ffh,0ffh,0ffh,0ffh,7eh,3ch
acol:   defb 20h,60h,40h,50h,10h,30h,0e0h,0a0h ; formato A: tinta de cada fila

; -------------------------------------------------------------
; anim: mueve y anima. Los normales por la ventana de POKEs, los extra
; escribiendo en su entrada. La pagina cambia cada 4 imagenes.
; -------------------------------------------------------------
anim:   ld a,(fc)
        inc a
        ld (fc),a
        rrca
        rrca
        and 3
        ld (fpg),a              ; fotograma comun
        ld a,(movon)
        ld (mvnow),a
        ld hl,nx
        ld c,0
an1:    ld a,c
        ld (SPR_SEL),a
        ld a,(mvnow)
        or a
        ld a,(hl)
        jr z,an2
        inc a                   ; un pixel a la derecha
        cp XMIN+XRANGE
        jr c,an2
        ld a,XMIN               ; vuelta por la izquierda
an2:    ld (hl),a
        inc hl
        ld (SPR_XL),a
        ld a,(fpg)
        add a,c
        and 3
        add a,PG0
        ld (SPR_PAGE),a
        inc c
        ld a,c
        cp 64
        jr c,an1
        ld ix,XTBL              ; los extra que se mueven
        ld c,0
an4:    ld a,(mvnow)
        or a
        jr z,an6
        ld a,(ix+1)
        dec a                   ; un pixel a la izquierda
        cp XMIN
        jr nc,an5
        ld a,XMIN+XRANGE-1
an5:    ld (ix+1),a
an6:    bit 0,c
        jr nz,an7               ; los A no se animan
        ld a,(fpg)
        add a,c
        and 3
        add a,PG0
        ld (ix+29),a
an7:    ld de,32
        add ix,de
        inc c
        ld a,c
        cp XSTATIC
        jr c,an4
        ret

; -------------------------------------------------------------
; keys: E cambia el numero de extra, P para y mueve. Z si Q pulsada.
; -------------------------------------------------------------
keys:   ld a,0fbh               ; Q W E R T
        in a,(0feh)
        and 4
        jr nz,kn1
kw1:    ld a,0fbh
        in a,(0feh)
        and 4
        jr z,kw1
        ld a,(mstep)
        inc a
        and 3
        ld (mstep),a
        call setm
kn1:    ld a,0dfh               ; P O I U Y
        in a,(0feh)
        and 1
        jr nz,kn2
kw2:    ld a,0dfh
        in a,(0feh)
        and 1
        jr z,kw2
        ld a,(movon)
        xor 1
        ld (movon),a
kn2:    ld a,0fbh
        in a,(0feh)
        and 1
        ret

; setm: POKE 2131 y el borde segun mstep (0, 32, 48, 64 extra)
setm:   ld a,(mstep)
        ld e,a
        ld d,0
        ld hl,mtab
        add hl,de
        ld a,(hl)
        ld (SPR_EXTRA),a
        ld hl,btab
        add hl,de
        ld a,(hl)
        out (0fbh),a
        ret
mtab:   defb 0,32,48,64
btab:   defb 2,1,3,4            ; rojo, azul, magenta, verde

; -------------------------------------------------------------
; variables
; -------------------------------------------------------------
fname:  defb "FONDO.JPG",0
magic:  defb "I160"
hdr:    defb 0,0,0,0
fh:     defb 0
fc:     defb 0
fpg:    defb 0
movon:  defb 1
mvnow:  defb 1
mstep:  defb 3
band:   defb 0
curx:   defb 0
cury:   defb 0
nx:     defs 64                 ; X de los 64 normales

        end start
