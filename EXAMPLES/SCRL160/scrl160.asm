; =============================================================
; SCRL160.ASM -- scroll en anillo de los modos 160x100 y 160x200
;
; Prueba POKE 2072-2075 (ver FPGA/SD81V2.1000/mode160.v):
;   2072 fila de la imagen que sale arriba      2073 filas del anillo
;   2074 pixel que sale a la izquierda          2075 bytes por fila del anillo
;
; Un mundo infinito de baldosas de 16x16 (rejilla blanca y una diagonal
; negra en cada baldosa) que se mueve pixel a pixel. El anillo tiene una
; fila y una columna mas que la pantalla (dos columnas en 4 bpp): antes de
; mover, el programa pinta ahi lo que va a entrar, sin que se vea. La FPGA
; hace el resto: no se copia la pantalla.
;
;   160x100 a 8 bpp (POKE 2045,175): anillo de 101 filas x 161 bytes
;   160x200 a 4 bpp (POKE 2045,176): anillo de 201 filas x 81 bytes
;     (162 pixeles; con un desplazamiento impar sale medio byte)
;
; Teclas: Q A O P = arriba, abajo, izquierda, derecha (mantener)
;         S = movimiento automatico en diagonal (si / no)
;         M = cambiar de modo (8 bpp / 4 bpp)      X = salir
;
; Ensamblar: pasmo scrl160.asm scrl160.bin scrl160.sym
; Cargar desde BASIC: SCRL160.B81 (LOAD FAST "SCRL160.BIN" CODE 30000)
; =============================================================

        org 30000

IMG       equ 8000h
VOFS      equ 2072
VRING     equ 2073
HOFS      equ 2074
HRING     equ 2075

start:  di
        xor a
        ld (mode4),a
        ld (auto),a
        call setmode

main:   call waitvs
        ld a,(rv)               ; lo que se preparo en la vuelta anterior
        ld (VOFS),a
        ld a,(rh)
        ld (HOFS),a
        call keys               ; D = dx, E = dy (-1, 0, 1); Z = salir
        jr z,quit
        ld (dxy),de             ; los pasos tocan todos los registros
        ld a,d
        dec a
        call z,stepright
        ld a,(dxy+1)
        inc a
        call z,stepleft
        ld a,(dxy)
        dec a
        call z,stepdown
        ld a,(dxy)
        inc a
        call z,stepup
        jr main

; --- salida: sin scroll, modo normal, ventana del 4 bpp por defecto ---
quit:   xor a
        ld (VOFS),a
        ld (VRING),a
        ld (HOFS),a
        ld (HRING),a
        ld (2070),a
        ld a,25
        ld (2071),a
        ld a,85
        ld (2045),a
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
; setmode: tamanos, tabla de filas, mundo en (0,0), paleta y modo
; -------------------------------------------------------------
setmode:
        ld hl,par8
        ld a,(mode4)
        or a
        jr z,sm1
        ld hl,par4
sm1:    ld de,hc
        ld bc,4
        ldir                    ; hc, wpx, wcb, nvis
        ld hl,IMG               ; rowtab[f] = IMG + f * wcb
        ld ix,rowtab
        ld a,(wcb)
        ld e,a
        ld d,0
        ld a,(hc)
        ld b,a
sm2:    ld (ix+0),l
        ld (ix+1),h
        inc ix
        inc ix
        add hl,de
        djnz sm2
        xor a
        ld (VOFS),a             ; el origen del mundo en (0,0) antes de encender el modo
        ld (HOFS),a
        xor a                   ; HFILE = $8000
        ld (2043),a
        ld a,IMG/256
        ld (2044),a
        ld bc,7fefh             ; color Chroma81
        ld a,20h
        out (c),a
        ld a,(hc)
        ld (VRING),a
        ld a,(wcb)
        ld (HRING),a
        ld a,(mode4)
        or a
        jr nz,sm4
        call pal8
        ld a,175                ; 160x100 a 8 bpp
        ld (2045),a
        ld a,4                  ; borde verde
        out (0fbh),a
        jr smfill
sm4:    call pal4
        xor a
        ld (2069),a             ; sin texto
        ld (2070),a             ; ventana de 200 lineas
        ld a,25
        ld (2071),a
        ld a,176                ; 160x200 a 4 bpp
        ld (2045),a
        ld a,1                  ; borde azul
        out (0fbh),a
smfill: ld a,1
        ld (busy),a             ; el anillo se esta pintando (lo mira el arnes del PC)

        xor a
        ld (oxl),a
        ld (oyl),a
        ld (rv),a
        ld (rh),a
        ld (VOFS),a
        ld (HOFS),a
        ld c,0                  ; todo el anillo: la fila f es la fila f del mundo
sm3:    ld a,c
        ld e,c
        push bc
        call fillrow
        pop bc
        inc c
        ld a,(hc)
        cp c
        jr nz,sm3
        xor a
        ld (busy),a
        ret

par8:   defb 101,161,161,100    ; filas del anillo, pixeles por fila, bytes por fila, filas visibles
par4:   defb 201,162,81,200

; paleta de 256: RGB 3-3-2 (la de fabrica). 2063 puntero, 2064 {G,B}, 2065 {BRIGHT,R}
pal8:   xor a
        ld (2063),a
        ld c,0
p8l:    ld a,c                  ; G = bits 4-2
        rrca
        rrca
        and 7
        call lev3
        add a,a
        add a,a
        add a,a
        add a,a
        ld b,a
        ld a,c                  ; B = bits 1-0: 0, 5, 10, 15
        and 3
        ld d,a
        add a,a
        add a,a
        add a,d
        or b
        ld (2064),a
        ld a,c                  ; R = bits 7-5
        rlca
        rlca
        rlca
        and 7
        call lev3
        ld (2065),a
        inc c
        jr nz,p8l
        ret
lev3:   push hl
        ld hl,l3tab
        add a,l
        ld l,a
        jr nc,lv1
        inc h
lv1:    ld a,(hl)
        pop hl
        ret
l3tab:  defb 0,2,4,6,9,11,13,15

; paleta de 16: los colores de Chroma (cada canal a 0 o 15). 2066 = byte alto de la de 16
pal4:   xor a
        ld (2063),a
        ld c,0
p4l:    xor a                   ; {G,B}: G = bit 2, B = bit 0
        bit 2,c
        jr z,p41
        ld a,0f0h
p41:    bit 0,c
        jr z,p42
        or 0fh
p42:    ld (2064),a
        xor a                   ; {BRIGHT,R}: BRIGHT = bit 3, R = bit 1
        bit 3,c
        jr z,p43
        ld a,0f0h
p43:    bit 1,c
        jr z,p44
        or 0fh
p44:    ld (2066),a
        inc c
        ld a,c
        cp 16
        jr nz,p4l
        ret

; -------------------------------------------------------------
; Pasos. Cada uno pinta primero lo que va a entrar en la fila o columna
; oculta del anillo y luego mueve el origen.
; -------------------------------------------------------------
stepright:                      ; entra la columna del mundo ox+160
        ld a,(oxl)
        add a,160
        ld d,a
        ld a,(rh)
        ld b,160
        call modadd
        call fillcol
        ld hl,oxl
        inc (hl)
        ld a,(rh)
        ld b,1
        call modadd
        ld (rh),a
        ret
stepleft:                       ; entra la columna del mundo ox-1
        ld a,(oxl)
        dec a
        ld d,a
        ld a,(wpx)
        dec a
        ld b,a
        ld a,(rh)
        call modadd
        push af
        call fillcol
        pop af
        ld (rh),a
        ld hl,oxl
        dec (hl)
        ret
stepdown:                       ; entra la fila del mundo oy+filas visibles
        ld a,(nvis)
        ld b,a
        ld a,(oyl)
        add a,b
        ld e,a
        ld a,(rv)
        call modaddh
        call fillrow
        ld hl,oyl
        inc (hl)
        ld a,(rv)
        ld b,1
        call modaddh
        ld (rv),a
        ret
stepup:                         ; entra la fila del mundo oy-1
        ld a,(oyl)
        dec a
        ld e,a
        ld a,(hc)
        dec a
        ld b,a
        ld a,(rv)
        call modaddh
        push af
        call fillrow
        pop af
        ld (rv),a
        ld hl,oyl
        dec (hl)
        ret

; A = (A + B) mod wpx / mod hc (A y B menores que el modulo; la suma puede pasar de 255)
modadd: push hl
        ld hl,wpx
        jr mda
modaddh:
        push hl
        ld hl,hc
mda:    add a,b
        jr c,mds
        cp (hl)
        jr c,mdx
mds:    sub (hl)
mdx:    pop hl
        ret

; -------------------------------------------------------------
; fillrow: A = fila del anillo, E = fila del mundo (byte bajo).
; La columna c del anillo es la columna ox + ((c - rh) mod wpx) del mundo.
; -------------------------------------------------------------
fillrow:
        call rowaddr            ; HL = principio de la fila
        ld a,(rh)               ; C = desplazamiento de la columna 0
        or a
        jr z,fr1
        ld b,a
        ld a,(wpx)
        sub b
fr1:    ld c,a
        ld a,(wpx)
        ld b,a                  ; B = pixeles que quedan (wpx par en 4 bpp: B par = nibble alto)
fr2:    ld a,(oxl)
        add a,c
        ld d,a                  ; D = columna del mundo
        call world              ; A = color
        call putpix
        inc c
        ld a,(wpx)
        cp c
        jr nz,fr3
        ld c,0
fr3:    djnz fr2
        ret

; putpix: A = color en (HL); en 4 bpp, nibble alto si B es par, y HL avanza tras el bajo
putpix: push af
        ld a,(mode4)
        or a
        jr nz,pp4
        pop af
        ld (hl),a
        inc hl
        ret
pp4:    pop af
        bit 0,b
        jr nz,pplo
        add a,a
        add a,a
        add a,a
        add a,a
        push bc
        ld c,a
        ld a,(hl)
        and 0fh
        or c
        ld (hl),a
        pop bc
        ret
pplo:   push bc
        ld c,a
        ld a,(hl)
        and 0f0h
        or c
        ld (hl),a
        pop bc
        inc hl
        ret

; -------------------------------------------------------------
; fillcol: A = columna del anillo (pixel), D = columna del mundo.
; La fila f del anillo es la fila oy + ((f - rv) mod hc) del mundo.
; -------------------------------------------------------------
fillcol:
        ld c,a                  ; columna: desplazamiento en bytes y nibble
        ld a,(mode4)
        or a
        ld a,c
        ld b,0                  ; B par = nibble alto (y en 8 bpp no cuenta)
        jr z,fc1
        srl a
        jr nc,fc1
        inc b                   ; columna impar: nibble bajo
fc1:    ld (fcoff),a
        ld a,b
        ld (fcnib),a
        ld a,(oyl)
        ld e,a                  ; E = fila del mundo
        ld a,(rv)
        ld c,a                  ; C = fila del anillo
        ld a,(hc)
        ld b,a
fc2:    push bc
        ld a,c
        call rowaddr
        ld a,(fcoff)
        add a,l
        ld l,a
        jr nc,fc3
        inc h
fc3:    call world
        ld bc,(fcnib)           ; B = basura, C = nibble
        ld b,c
        call putpix
        pop bc
        inc e
        inc c
        ld a,(hc)
        cp c
        jr nz,fc4
        ld c,0
fc4:    djnz fc2
        ret

; rowaddr: A = fila del anillo -> HL = rowtab[A]. Conserva BC y DE.
rowaddr:
        ld l,a
        ld h,0
        add hl,hl
        push de
        ld de,rowtab
        add hl,de
        pop de
        ld a,(hl)
        inc hl
        ld h,(hl)
        ld l,a
        ret

; -------------------------------------------------------------
; world: D = x, E = y del mundo (bytes bajos) -> A = color.
; Rejilla cada 16 pixeles (blanco), diagonal negra en cada baldosa y el
; color de la baldosa segun su X y su Y. Conserva BC, DE y HL.
; -------------------------------------------------------------
world:  ld a,d
        and 15
        jr z,wgrid
        ld a,e
        and 15
        jr z,wgrid
        ld a,d
        sub e
        and 15
        cp 8
        jr z,wdiag
        ld a,(mode4)
        or a
        jr nz,w4
        ld a,e                  ; 8 bpp: R y G alto de la X, G bajo y B de la Y
        rrca
        rrca
        rrca
        rrca
        and 0fh
        push bc
        ld c,a
        ld a,d
        and 0f0h
        or c
        pop bc
        ret
w4:     ld a,e                  ; 4 bpp: (X/16 + 3*Y/16) mod 8, sin el 0
        rrca
        rrca
        rrca
        rrca
        and 0fh
        push bc
        ld c,a
        add a,a
        add a,c
        ld c,a
        ld a,d
        rrca
        rrca
        rrca
        rrca
        and 0fh
        add a,c
        pop bc
        and 7
        ret nz
        ld a,10                 ; rojo brillante
        ret
wgrid:  ld a,(mode4)
        or a
        ld a,255                ; blanco (8 bpp)
        ret z
        ld a,15                 ; blanco brillante (4 bpp)
        ret
wdiag:  xor a                   ; negro
        ret

; -------------------------------------------------------------
; keys: D = dx, E = dy. S, M y X con espera a soltar. Z si X pulsada.
; -------------------------------------------------------------
keys:   ld de,0
        ld a,(auto)
        or a
        jr z,k0
        ld de,0101h             ; automatico: derecha y abajo
k0:     ld a,0fbh               ; Q (arriba)
        in a,(0feh)
        rrca
        jr c,k1
        ld e,0ffh
k1:     ld a,0fdh               ; A (abajo), S (automatico)
        in a,(0feh)
        rrca
        jr c,k2
        ld e,1
k2:     rrca
        jr c,k3
        call kup
        ld a,(auto)
        xor 1
        ld (auto),a
k3:     ld a,0dfh               ; P (derecha), O (izquierda)
        in a,(0feh)
        rrca
        jr c,k4
        ld d,1
k4:     rrca
        jr c,k5
        ld d,0ffh
k5:     ld a,7fh                ; M (cambiar de modo)
        in a,(0feh)
        and 4
        jr nz,k6
        call kup
        ld a,(mode4)
        xor 1
        ld (mode4),a
        push de
        call setmode
        pop de
        ld de,0                 ; esta vuelta no se mueve
k6:     ld a,0feh               ; X (salir)
        in a,(0feh)
        and 4
        ret                     ; Z si pulsada

; kup: espera a que no haya ninguna tecla pulsada
kup:    xor a
        in a,(0feh)
        cpl
        and 1fh
        jr nz,kup
        ret

; -------------------------------------------------------------
; variables
; -------------------------------------------------------------
mode4:  defb 0                  ; 0 = 160x100 a 8 bpp, 1 = 160x200 a 4 bpp
auto:   defb 0
busy:   defb 0                  ; 1 mientras setmode pinta el anillo
hc:     defb 0                  ; filas del anillo
wpx:    defb 0                  ; pixeles por fila del anillo
wcb:    defb 0                  ; bytes por fila del anillo
nvis:   defb 0                  ; filas visibles
oxl:    defb 0                  ; origen del mundo (bytes bajos)
oyl:    defb 0
rv:     defb 0                  ; fila del anillo que sale arriba (POKE 2072)
rh:     defb 0                  ; pixel del anillo que sale a la izquierda (POKE 2074)
dxy:    defw 0                  ; E = dy (byte bajo), D = dx
fcoff:  defb 0
fcnib:  defb 0,0
rowtab: defs 2*201

        end start
