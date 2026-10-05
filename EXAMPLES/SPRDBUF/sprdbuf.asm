; =============================================================
; SPRDBUF.ASM -- prueba de sprites por linea + doble buffer
;
; Comprueba juntas dos cosas que ningun programa usaba a la vez:
; los sprites (que la FPGA lee por linea de la RAM de sombra) y el
; doble buffer present-blit (que copia 8 KB por el mismo puerto de la
; BRAM durante el blanking). Modo Superfast HiRes Spectrum.
;
; Fondo: pelota de DBUF (bitmap) rebotando, con el doble buffer ON o OFF.
;
; Sprites (cuadrado 8x8 con aro de color y centro negro; el color del aro
; identifica al sprite):
;   A  sprites 0..11  en la MISMA linea (Y=16), separados 14 px.
;      Limite de 8 por linea: se ven los 8 de indice mas alto, los
;      sprites 0..3 (aros azul, rojo, magenta, verde) NO deben aparecer.
;   B  sprites 12..17 en otra linea (Y=56). Son 6: se ven los 6.
;   C  sprites 18..22 (izquierda, Y=96) y 23..27 (derecha, Y=100): en las
;      lineas 100-103 coinciden 10 sprites, asi que los dos de la izquierda
;      (18 y 19) solo muestran sus 4 filas de arriba; los otros 8 salen
;      enteros.
;   D  sprite 28 recorre la pantalla en horizontal y el 29 en vertical,
;      por encima de la pelota. Se mueven al empezar el VSYNC.
;
; Con el doble buffer ON (borde VERDE) y OFF (borde ROJO) los sprites tienen
; que verse exactamente igual: no parpadean, no se parten y no hay lineas
; que cambien de sitio. La pelota parpadea solo con el doble buffer OFF.
;
; Teclas:  SPACE = conmutar doble buffer   M = congelar/mover   Q = salir
;
; Ensamblar: pasmo sprdbuf.asm sprdbuf.bin sprdbuf.sym
; Cargar desde BASIC: SPRDBUF.B81 (LOAD FAST "SPRDBUF.BIN" CODE 30000)
; =============================================================

        org 30000

; Direccion de los POKEs de sprites (ver Apendice H del manual)
SPR_SEL  equ 2100
SPR_EN   equ 2101
SPR_XL   equ 2102
SPR_XH   equ 2103
SPR_Y    equ 2104
SPR_COL  equ 2105       ; 8 filas
SPR_PIX  equ 2113       ; 8 filas
SPR_MSK  equ 2121       ; 8 filas

start:  di
        ; --- HFILE = $8000 ---
        ld a,0
        ld (2043),a
        ld a,80h
        ld (2044),a

        ; --- limpiar bitmap (6144 bytes a 0) ---
        ld hl,8000h
        ld de,8001h
        ld bc,17ffh
        ld (hl),0
        ldir

        ; --- atributos: papel blanco, tinta negra ---
        ld hl,9800h
        ld de,9801h
        ld bc,2ffh
        ld (hl),38h
        ldir

        ; --- color Chroma81 activo, borde negro ---
        ld bc,7fefh
        ld a,20h
        out (c),a

        ; --- modo Superfast HiRes Spectrum ---
        ld a,172
        ld (2045),a

        ; --- sprites ---
        call defspr

        ; --- doble buffer ON, front = bloque 5 ---
        ld a,173
        ld (2057),a
        ld a,1
        ld (dbufst),a
        ld a,4                  ; borde verde = dbuf ON
        out (0fbh),a

; -------------------------------------------------------------
; Bucle principal: una vuelta por imagen, sincronizada al VSYNC.
; Los sprites se mueven nada mas empezar el VSYNC, antes de que el
; haz entre en el area visible, para que no haya cortes.
; -------------------------------------------------------------
main:   call waitvs
        call movspr
        call erase
        call delay
        call move
        call draw
        call keys
        jr nz,main

; --- salida: sprites fuera, modos normales y vuelta a BASIC ---
quit:   ld b,32
        ld c,0
qs:     ld a,c
        ld (SPR_SEL),a
        xor a
        ld (SPR_EN),a
        inc c
        djnz qs
        ld a,85
        ld (2057),a             ; doble buffer OFF
        ld a,85
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

; -------------------------------------------------------------
; defspr: define y activa los 28 sprites estaticos y los dos que se mueven
; Las posiciones llevan sumados los 32 pixeles de desplazamiento
; (X=32 / Y=32 es el pixel 0,0 de la pantalla).
; -------------------------------------------------------------
defspr: ; A: sprites 0..5, tinta 1..6, desde X=40
        ld a,0
        ld (g_n),a
        ld a,6
        ld (g_cnt),a
        ld hl,40
        ld (g_x),hl
        ld a,32+16
        ld (g_y),a
        ld a,1
        ld (g_ink),a
        call group
        ; A: sprites 6..11, tinta 9..14 (sin el 7: blanco sobre blanco)
        ld a,6
        ld (g_cnt),a
        ld a,9
        ld (g_ink),a
        call group
        ; B: sprites 12..17, otra linea
        ld a,6
        ld (g_cnt),a
        ld hl,40
        ld (g_x),hl
        ld a,32+56
        ld (g_y),a
        ld a,1
        ld (g_ink),a
        call group
        ; C izquierda: sprites 18..22, Y=96
        ld a,5
        ld (g_cnt),a
        ld hl,40
        ld (g_x),hl
        ld a,32+96
        ld (g_y),a
        ld a,1
        ld (g_ink),a
        call group
        ; C derecha: sprites 23..27, Y=100
        ld a,5
        ld (g_cnt),a
        ld hl,130
        ld (g_x),hl
        ld a,32+100
        ld (g_y),a
        ld a,9
        ld (g_ink),a
        call group
        ; D: sprite 28 (horizontal, Y=150) y 29 (vertical, X=230)
        ld b,28
        ld de,32
        ld c,32+150
        ld a,5
        call spr_def
        ld b,29
        ld de,32+230
        ld c,32
        ld a,2
        call spr_def
        ret

; group: define g_cnt sprites desde g_n, X desde g_x con paso 14, misma Y,
;        tinta desde g_ink (sube 1 por sprite)
group:  ld a,(g_cnt)
        or a
        ret z
        dec a
        ld (g_cnt),a
        ld a,(g_n)
        ld b,a
        inc a
        ld (g_n),a
        ld hl,(g_x)
        push hl
        ld de,14
        add hl,de
        ld (g_x),hl
        pop de
        ld a,(g_y)
        ld c,a
        ld a,(g_ink)
        push af
        inc a
        ld (g_ink),a
        pop af
        call spr_def
        jr group

; spr_def: B=sprite, DE=X (9 bits), C=Y, A=tinta (0-15). Papel negro.
spr_def:
        push af
        ld a,b
        ld (SPR_SEL),a
        ld a,e
        ld (SPR_XL),a
        ld a,d
        ld (SPR_XH),a
        ld a,c
        ld (SPR_Y),a
        pop af
        add a,a
        add a,a
        add a,a
        add a,a                 ; tinta*16 + papel 0
        ld hl,SPR_COL
        ld b,8
sd1:    ld (hl),a
        inc hl
        djnz sd1
        ld hl,ring
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

ring:   defb 0ffh,81h,81h,81h,81h,81h,81h,0ffh
full:   defb 0ffh,0ffh,0ffh,0ffh,0ffh,0ffh,0ffh,0ffh

; -------------------------------------------------------------
; movspr: el 28 se mueve 2 px en X (9 bits) y el 29 sube 3 px en Y
; -------------------------------------------------------------
movspr: ld a,28
        ld (SPR_SEL),a
        ld hl,(s28x)
        ld de,2
        add hl,de
        ld a,h
        cp 1
        jr c,m28ok
        ld a,l
        cp 45                   ; X > 300: vuelta al principio
        jr c,m28ok
        ld hl,32
m28ok:  ld (s28x),hl
        ld a,l
        ld (SPR_XL),a
        ld a,h
        ld (SPR_XH),a
        ld a,29
        ld (SPR_SEL),a
        ld a,(s29y)
        add a,3
        cp 224
        jr c,m29ok
        ld a,32
m29ok:  ld (s29y),a
        ld (SPR_Y),a
        ret

; -------------------------------------------------------------
; pixad: D=fila (0-191), E=columna en bytes (0-31) -> HL=direccion
; -------------------------------------------------------------
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
; erase / draw: bloque de 16 lineas x 2 bytes en (bally,ballx)
; -------------------------------------------------------------
erase:  ld c,0
        jr blkgo
draw:   ld c,0ffh
blkgo:  ld a,(bally)
        ld d,a
        ld a,(ballx)
        ld e,a
        ld b,16
blk1:   push bc
        push de
        call pixad
        ld (hl),c
        inc l
        ld (hl),c
        pop de
        pop bc
        inc d
        djnz blk1
        ret

; -------------------------------------------------------------
; move: la pelota rebota (y en 0..176, x en 0..30)
; -------------------------------------------------------------
move:   ld a,(moven)
        or a
        ret z
        ld a,(bally)
        ld b,a
        ld a,(dy)
        add a,b
        cp 177
        jr c,myok
        ld a,(dy)
        neg
        ld (dy),a
        ld a,b
myok:   ld (bally),a
        ld a,(ballx)
        ld b,a
        ld a,(dx)
        add a,b
        cp 31
        jr c,mxok
        ld a,(dx)
        neg
        ld (dx),a
        ld a,b
mxok:   ld (ballx),a
        ret

; -------------------------------------------------------------
; delay: ~6 ms de "trabajo" que invade el barrido
; -------------------------------------------------------------
delay:  ld bc,800
dloop:  dec bc
        ld a,b
        or c
        jr nz,dloop
        ret

; -------------------------------------------------------------
; keys: SPACE conmuta el doble buffer (borde verde/rojo), M congela.
; Devuelve Z activo si Q esta pulsada.
; -------------------------------------------------------------
keys:   ld a,7fh
        in a,(0feh)
        rrca
        jr c,knosp
kwait:  ld a,7fh
        in a,(0feh)
        rrca
        jr nc,kwait
        ld a,(dbufst)
        xor 1
        ld (dbufst),a
        or a
        jr z,koff
        ld a,173
        ld (2057),a
        ld a,4
        out (0fbh),a
        jr knosp
koff:   ld a,85
        ld (2057),a
        ld a,2
        out (0fbh),a
knosp:  ld a,7fh
        in a,(0feh)
        and 4
        jr nz,knom
kwm:    ld a,7fh
        in a,(0feh)
        and 4
        jr z,kwm
        ld a,(moven)
        xor 1
        ld (moven),a
knom:   ld a,0fbh
        in a,(0feh)
        and 1
        ret

; -------------------------------------------------------------
; variables
; -------------------------------------------------------------
bally:  defb 80
ballx:  defb 14
dy:     defb 2
dx:     defb 1
moven:  defb 1
dbufst: defb 1
s28x:   defw 32
s29y:   defb 32
g_n:    defb 0
g_cnt:  defb 0
g_x:    defw 0
g_y:    defb 0
g_ink:  defb 0

        end start
