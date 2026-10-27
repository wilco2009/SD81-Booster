; =============================================================
; GALAXIAN.ASM -- prueba visual: sprites 8x8 estilo Galaxian
;
; Todo son sprites por hardware (64 disponibles):
;   0..45   formacion de 46 aliens (2 comandantes, 6 rojos, 8 purpuras,
;           3 filas de 10 azules). Se balancea y bate las alas.
;   46..48  un comandante con dos escoltas en picado (curva senoidal)
;   61      disparo del jugador     62  nave del jugador
; Fondo: estrellas fijas en el bitmap (modo Superfast HiRes Spectrum).
; No hay colisiones ni puntuacion: es solo para ver el aspecto y medir.
;
; Teclas:  O = izquierda   P = derecha   SPACE = disparo   Q = salir
;
; Los dibujos y las tablas salen de gen_gfx.py (galaxian_gfx.inc).
; Ensamblar: pasmo galaxian.asm galaxian.bin galaxian.sym   (o compila.bat)
; Cargar:    GALAXIAN.B81  (LOAD FAST "GALAXIAN.BIN" CODE 30000)
; =============================================================

        org 30000

SPR_SEL  equ 2100
SPR_EN   equ 2101
SPR_XL   equ 2102
SPR_XH   equ 2103
SPR_Y    equ 2104
SPR_COL  equ 2105       ; 8 filas
SPR_PIX  equ 2113       ; 8 filas
SPR_MSK  equ 2121       ; 8 filas
S_SHOT   equ 61
S_SHIP   equ 62

start:  di
        ; --- HFILE = $8000 ---
        xor a
        ld (2043),a
        ld a,80h
        ld (2044),a

        ; --- bitmap a 0 y atributos: tinta blanca, papel negro ---
        ld hl,8000h
        ld de,8001h
        ld bc,17ffh
        ld (hl),0
        ldir
        ld hl,9800h
        ld de,9801h
        ld bc,2ffh
        ld (hl),07h
        ldir
        call stars

        ; --- color Chroma81 activo, borde negro ---
        ld bc,7fefh
        ld a,20h
        out (c),a
        xor a
        out (0fbh),a

        ; --- modo Superfast HiRes Spectrum ---
        ld a,172
        ld (2045),a

        ; --- sprites ---
        call reshape            ; dibujos de los 49 sprites de aliens
        call movform
        call movdive
        call enable
        ld b,S_SHIP
        ld c,4
        ld d,0
        call setshape
        ld a,1
        ld (SPR_EN),a
        ld b,S_SHOT
        ld c,5
        ld d,0
        call setshape           ; el disparo queda apagado hasta SPACE

main:   call waitvs
        call movform
        call movdive
        call movplay
        call tick
        call keys
        jr nz,main

; --- salida: sprites fuera y modo normal ---
        ld b,64
        ld c,0
qs:     ld a,c
        ld (SPR_SEL),a
        xor a
        ld (SPR_EN),a
        inc c
        djnz qs
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
; setshape: B=sprite, C=tipo, D=frame -> dibujo, mascara y colores.
;           No toca activo, X ni Y. Destruye A, BC, DE, HL.
; -------------------------------------------------------------
setshape:
        ld a,b
        ld (SPR_SEL),a
        ld a,c
        add a,a
        add a,d
        add a,a
        add a,a
        add a,a                 ; (tipo*2+frame)*8
        ld e,a
        ld d,0
        push de
        ld hl,pixtab
        add hl,de
        ld de,SPR_PIX
        ld bc,8
        ldir
        pop de
        push de
        ld hl,masktab
        add hl,de
        ld de,SPR_MSK
        ld bc,8
        ldir
        pop de
        ld hl,coltab
        add hl,de
        ld de,SPR_COL
        ld bc,8
        ldir
        ret

; reshape: redibuja los NSPR sprites de aliens con el frame actual
reshape:
        ld b,0
rs1:    push bc
        ld hl,stype
        ld e,b
        ld d,0
        add hl,de
        ld c,(hl)
        ld a,(frame)
        ld d,a
        call setshape
        pop bc
        inc b
        ld a,b
        cp NSPR
        jr nz,rs1
        ret

; enable: pone la Y y activa los NSPR sprites de aliens
enable: ld b,0
en1:    ld a,b
        ld (SPR_SEL),a
        ld hl,formy
        ld e,b
        ld d,0
        add hl,de
        ld a,(hl)
        ld (SPR_Y),a
        ld a,1
        ld (SPR_EN),a
        inc b
        ld a,b
        cp NSPR
        jr nz,en1
        ret

; -------------------------------------------------------------
; movform: X de los 46 aliens = X base + balanceo (con signo)
; -------------------------------------------------------------
movform:
        ld a,(sway)
        ld d,a                  ; D = parte baja
        rla
        sbc a,a
        ld e,a                  ; E = parte alta (extension de signo)
        ld hl,formx
        ld bc,NFORM*256
mf1:    ld a,c
        ld (SPR_SEL),a
        ld a,(hl)
        inc hl
        add a,d
        ld (SPR_XL),a
        ld a,(hl)
        inc hl
        adc a,e
        ld (SPR_XH),a
        inc c
        djnz mf1
        ret

; -------------------------------------------------------------
; movdive: comandante (46) y escoltas (47, 48) bajan en curva
; -------------------------------------------------------------
movdive:
        ld a,(dvy)
        inc a
        cp 228
        jr c,md1
        ld a,124
md1:    ld (dvy),a
        srl a
        and 31
        ld e,a
        ld d,0
        ld hl,sintab
        add hl,de
        ld a,(hl)
        add a,160
        ld c,a                  ; C = X del comandante
        ld a,46
        ld (SPR_SEL),a
        ld a,c
        ld (SPR_XL),a
        xor a
        ld (SPR_XH),a
        ld a,(dvy)
        ld (SPR_Y),a
        ld a,47
        ld (SPR_SEL),a
        ld a,c
        sub 14
        ld (SPR_XL),a
        xor a
        ld (SPR_XH),a
        ld a,(dvy)
        sub 8
        ld (SPR_Y),a
        ld a,48
        ld (SPR_SEL),a
        ld a,c
        add a,14
        ld (SPR_XL),a
        xor a
        ld (SPR_XH),a
        ld a,(dvy)
        sub 8
        ld (SPR_Y),a
        ret

; -------------------------------------------------------------
; movplay: nave (O/P) y disparo (SPACE)
; -------------------------------------------------------------
movplay:
        ld a,0dfh
        in a,(0feh)
        ld b,a
        ld a,(plx)
        bit 1,b                 ; O
        jr nz,mp1
        sub 2
        jr nc,mp1
        xor a
mp1:    bit 0,b                 ; P
        jr nz,mp2
        add a,2
        cp 249
        jr c,mp2
        ld a,248
mp2:    ld (plx),a
        ld l,a
        ld h,0
        ld de,32
        add hl,de
        ld a,S_SHIP
        ld (SPR_SEL),a
        ld a,l
        ld (SPR_XL),a
        ld a,h
        ld (SPR_XH),a
        ld a,32+176
        ld (SPR_Y),a

        ld a,(shy)
        or a
        jr nz,mpshot
        ld a,7fh
        in a,(0feh)
        rrca
        ret c                   ; SPACE sin pulsar
        ld a,(plx)
        ld (shx),a
        ld a,S_SHOT
        ld (SPR_SEL),a
        ld a,1
        ld (SPR_EN),a
        ld a,32+168
        jr mpdraw
mpshot: sub 4
        cp 36
        jr nc,mpdraw
        xor a                   ; salio por arriba
        ld (shy),a
        ld a,S_SHOT
        ld (SPR_SEL),a
        xor a
        ld (SPR_EN),a
        ret
mpdraw: ld (shy),a
        ld a,S_SHOT
        ld (SPR_SEL),a
        ld a,(shy)
        ld (SPR_Y),a
        ld a,(shx)
        ld l,a
        ld h,0
        ld de,32
        add hl,de
        ld a,l
        ld (SPR_XL),a
        ld a,h
        ld (SPR_XH),a
        ret

; -------------------------------------------------------------
; tick: balanceo (1 px cada 2 imagenes, +-24) y aleteo cada 16 imagenes
; -------------------------------------------------------------
tick:   ld a,(fcnt)
        inc a
        ld (fcnt),a
        and 1
        jr nz,tk1
        ld a,(sway)
        ld b,a
        ld a,(sdir)
        add a,b
        ld (sway),a
        cp 24
        jr z,tkrev
        cp -24
        jr nz,tk1
tkrev:  ld a,(sdir)
        neg
        ld (sdir),a
tk1:    ld a,(fcnt)
        and 15
        ret nz
        ld a,(frame)
        xor 1
        ld (frame),a
        jp reshape

; keys: Z activo si Q esta pulsada
keys:   ld a,0fbh
        in a,(0feh)
        and 1
        ret

; -------------------------------------------------------------
; stars: 48 estrellas fijas en el bitmap
; -------------------------------------------------------------
stars:  ld b,48
st1:    push bc
st2:    call rnd
        cp 192
        jr nc,st2
        ld d,a
        call rnd
        ld c,a
        rrca
        rrca
        rrca
        and 1fh
        ld e,a
        call pixad
        ld a,c
        and 7
        ld b,a
        ld a,80h
        jr z,st4
st3:    srl a
        djnz st3
st4:    or (hl)
        ld (hl),a
        pop bc
        djnz st1
        ret

rnd:    ld a,(seed)
        ld b,a
        rrca
        rrca
        rrca
        xor 1fh
        add a,b
        sbc a,255
        ld (seed),a
        ret

; pixad: D=fila (0-191), E=columna en bytes (0-31) -> HL=direccion
pixad:  ld a,d
        and 0c0h
        rrca
        rrca
        rrca
        ld h,a
        ld a,d
        and 7
        or h
        or 80h
        ld h,a
        ld a,d
        and 38h
        add a,a
        add a,a
        or e
        ld l,a
        ret

; -------------------------------------------------------------
; variables
; -------------------------------------------------------------
sway:   defb 0
sdir:   defb 1
fcnt:   defb 0
frame:  defb 0
dvy:    defb 124
plx:    defb 120
shx:    defb 0
shy:    defb 0
seed:   defb 77

        include "galaxian_gfx.inc"

        end start
