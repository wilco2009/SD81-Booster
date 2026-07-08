; =============================================================
; BOUNCE.ASM — demo del doble buffer del SD81 Booster (POKE 2057)
;
; Pelota (bloque 16x16) rebotando en modo Superfast HiRes Spectrum.
; En cada frame se BORRA la pelota, se pierde ~6 ms a proposito
; (simulando un dibujado pesado que cruza el barrido) y se REDIBUJA.
;
;   - Con doble buffer ON  (borde VERDE): imagen solida, sin parpadeo.
;   - Con doble buffer OFF (borde ROJO):  parpadeo intenso, porque el
;     haz pasa por la zona borrada antes del redibujado.
;
; Controles:  SPACE = conmutar doble buffer   M = congelar/mover   Q = salir
;
; Ensamblar con zmac (ver README.md). Carga desde BASIC:
;   10 FAST
;   20 LOAD FAST 'BOUNCE.BIN' CODE 30000
;   30 RAND USR 30000
;   40 SLOW
;
; Pantalla (HFILE) en $8000 (bloque 4). Front buffer en bloque 5:
;   POKE 2057,168+5 = 173  -> ON      POKE 2057,85 -> OFF
; =============================================================

        org 30000

start:  di
        ; --- HFILE = $8000 ---
        ld a,0
        ld (2043),a         ; HFILE bajo
        ld a,80h
        ld (2044),a         ; HFILE alto

        ; --- limpiar bitmap (6144 bytes a 0) ---
        ld hl,8000h
        ld de,8001h
        ld bc,17ffh
        ld (hl),0
        ldir

        ; --- atributos: papel blanco, tinta negra (768 bytes a $38) ---
        ld hl,9800h
        ld de,9801h
        ld bc,2ffh
        ld (hl),38h
        ldir

        ; --- activar color Chroma81 (bit5), borde negro ---
        ld bc,7fefh
        ld a,20h
        out (c),a

        ; --- modo Superfast HiRes Spectrum ---
        ld a,172
        ld (2045),a

        ; --- doble buffer ON, front = bloque 5 ---
        ld a,173
        ld (2057),a
        ld a,1
        ld (dbufst),a
        ld a,4              ; borde verde = dbuf ON (puerto ULA Spectrum)
        out (0fbh),a

; -------------------------------------------------------------
; Bucle principal: 1 iteracion por frame, sincronizada al VSYNC.
; Con dbuf ON el hardware toma la instantanea al final del area
; visible, mucho despues de que draw haya terminado -> sin cortes.
; -------------------------------------------------------------
main:   call waitvs         ; flanco de subida de VSYNC (blit ya terminado)
        call erase          ; borrar pelota (posicion actual)
        call delay          ; ~6 ms de "trabajo": el haz entra en pantalla
        call move           ; nueva posicion con rebote
        call draw           ; dibujar pelota
        call keys           ; SPACE conmuta dbuf; Z activo si Q pulsada
        jr nz,main

; --- salida: restaurar modos y volver a BASIC ---
quit:   ld a,85
        ld (2057),a         ; doble buffer OFF
        ld a,85
        ld (2045),a         ; modo ZX81 normal
        ret

; -------------------------------------------------------------
; waitvs: espera el flanco de subida del VSYNC (bit 0, puerto $AF)
; -------------------------------------------------------------
waitvs: in a,(0afh)
        rrca
        jr c,waitvs         ; si estamos dentro del vsync, esperar a salir
wvs2:   in a,(0afh)
        rrca
        jr nc,wvs2          ; esperar a entrar en vsync
        ret

; -------------------------------------------------------------
; pixad: D=fila (0-191), E=columna en bytes (0-31) -> HL=direccion
; Formato Spectrum: H = $80 | (y7y6>>3) | y2y1y0 ; L = (y5y4y3<<2) | x
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
; block: rellena 16 lineas x 2 bytes con C en (bally,ballx)
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
; move: y +/-2 con rebote en 0..176; x +/-1 (CADA frame, 50 Hz) en 0..30
; Con la tecla M se congela el movimiento (test de fantasma estatico).
; -------------------------------------------------------------
move:   ld a,(moven)
        or a
        ret z               ; movimiento congelado
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
; delay: ~6 ms (simula un dibujado pesado que invade el barrido)
; -------------------------------------------------------------
delay:  ld bc,800
dloop:  dec bc
        ld a,b
        or c
        jr nz,dloop
        ret

; -------------------------------------------------------------
; keys: SPACE conmuta el doble buffer (borde verde/rojo).
; Devuelve Z activo si Q esta pulsada (salir).
; -------------------------------------------------------------
keys:   ld a,7fh
        in a,(0feh)
        rrca
        jr c,knosp          ; SPACE no pulsada
kwait:  ld a,7fh            ; esperar a soltar (anti-rebote)
        in a,(0feh)
        rrca
        jr nc,kwait
        ld a,(dbufst)
        xor 1
        ld (dbufst),a
        or a
        jr z,koff
        ld a,173
        ld (2057),a         ; dbuf ON, front = bloque 5
        ld a,4              ; borde verde
        out (0fbh),a
        jr knosp
koff:   ld a,85
        ld (2057),a         ; dbuf OFF
        ld a,2              ; borde rojo
        out (0fbh),a
knosp:  ld a,7fh            ; M (misma fila que SPACE, bit 2)
        in a,(0feh)
        and 4
        jr nz,knom          ; M no pulsada
kwm:    ld a,7fh            ; esperar a soltar (anti-rebote)
        in a,(0feh)
        and 4
        jr z,kwm
        ld a,(moven)
        xor 1
        ld (moven),a        ; congelar/descongelar movimiento
knom:   ld a,0fbh
        in a,(0feh)
        and 1               ; Z activo si Q pulsada
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

        end
