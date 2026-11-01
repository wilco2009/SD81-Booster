; =============================================================
; PHOTO.ASM -- visor de fotos en el modo de video 160x100 a 8 bpp (256 colores a la vez)
;
; FPGA de la rama fpga-mode160 (POKE 2045,175; ver claude/planning/pdm_color.md). Solo funciona en la placa con esa FPGA.
;
; Lee IMG001.I16, IMG002.I16... de la carpeta actual (los que genera png2i16.py o bmp2i16.py) y los pone en pantalla. Si falta el
; IMGnnn.I16, prueba IMGnnn.BMP: el MCU lo convierte al abrirlo (comando 76: escala a 160x100 y reduce a 256 colores, tarda unos segundos).
;   N = la imagen siguiente   B = la anterior   ESPACIO = salir a BASIC
;   F = cambia como se coloca un BMP y lo vuelve a cargar: estirado (llena la pantalla), con su proporcion (bandas negras) o recortado
; Si falta un numero, N / B saltan al siguiente que exista; si no hay otra imagen, se queda la actual.
;
; Formato .I16 (16.516 bytes): "I160", 512 bytes de paleta (byte bajo {G,B}, alto {BRIGHT,R}: tal cual para POKE 2064 / 2065)
; y 16.000 pixeles (160x100, un byte por pixel).
;
; Para no ver la imagen a medias ni la paleta de la anterior, cada carga pone la paleta en negro, lee la imagen sobre $8000
; con el modo activo y al final carga la paleta nueva: de una foto a la siguiente pasa por negro (menos de un segundo).
;
; Devuelve BC = 0 al salir con ESPACIO y BC = 1 si no hay ninguna imagen (el PHOTO.B81 lo cuenta).
; Memoria: $7530 (30000) codigo, paleta leida y variables; imagen en $8000-$BE7F (HFILE = $8000).
;
; Ensamblar: pasmo photo.asm photo.bin photo.sym   (compila.bat)
; Cargar desde BASIC: PHOTO.B81 (LOAD FAST "PHOTO.BIN" CODE 30000), con PHOTO.BIN, PHOTO.B81 y las IMGnnn.I16 en la misma carpeta
; =============================================================

IMG             equ $8000
IMGLEN          equ 160*100

                org 30000
                jp start

                include "mcu.asm"

start:          di
                xor a
                ld (2043),a
                ld a,IMG>>8
                ld (2044),a             ; HFILE = $8000
                ld bc,$7FEF
                ld a,$20
                out (c),a               ; color activado, borde negro
                call blackpal           ; negro antes de encender el modo: no se ve nada de lo que haya en memoria
                ld a,175
                ld (2045),a             ; modo 160x100 a 8 bpp
                xor a
                ld (down),a
                ld (fitmode),a
                inc a
                ld (cand),a
                ld (picno),a
                call try_load           ; la primera imagen (si no esta la 1, la siguiente que haya)
                jr nc,main_loop
                call next
                jr nc,main_loop
                ld a,85
                ld (2045),a             ; no hay ninguna imagen: modo ZX81 normal
                ld bc,1
                ret

main_loop:      call scan
                jr nz,ml_key
                ld (down),a             ; ninguna tecla: se puede volver a pulsar
                jr main_loop
ml_key:         ld b,a
                ld a,(down)
                or a
                jr nz,main_loop         ; la tecla sigue pulsada
                inc a
                ld (down),a
                bit 0,b
                jr nz,exit
                bit 2,b
                jr z,ml_n
                call refit
                jr main_loop
ml_n:           bit 3,b
                jr z,ml_b
                call next
                jr main_loop
ml_b:           bit 4,b
                call nz,prev
                jr main_loop

exit:           ld a,85
                ld (2045),a             ; modo ZX81 normal
                ld bc,0
                ret

; teclado: A = bits 0 ESPACIO, 3 N, 4 B, 2 F (1 = pulsada); Z si no hay ninguna
scan:           ld a,$7F
                in a,(254)
                cpl
                and $19
                ld b,a
                ld a,$FD
                in a,(254)
                cpl
                and 8                   ; F = bit 3 de la fila A-G
                rrca                    ; -> bit 2
                or b
                ret

; F: la siguiente colocacion de los BMP y se vuelve a cargar la imagen actual (a un .I16 no le cambia nada)
refit:          ld a,(fitmode)
                inc a
                cp 3
                jr c,rf_ok
                xor a
rf_ok:          ld (fitmode),a
                ld a,(picno)
                ld (cand),a
                jp try_load

; la imagen siguiente / anterior que exista (como mucho 255 intentos); Carry si no hay otra y entonces se queda la actual
next:           ld d,1
                jr step
prev:           ld d,255                ; -1
step:           ld b,255
st_l:           ld a,(cand)
                add a,d
                ld (cand),a
                push bc
                push de
                call try_load
                pop de
                pop bc
                jr nc,st_ok
                djnz st_l
                ld a,(picno)
                ld (cand),a
                scf
                ret
st_ok:          ld a,(cand)
                ld (picno),a
                or a
                ret

; pone en pantalla IMGnnn.I16 con nnn = (cand), o IMGnnn.BMP si no hay .I16. Carry si no se puede (no existe, no es un .I16 o el BMP no
; se convierte) y entonces no se toca la pantalla
try_load:       call mk_name
                ld hl,nm+7
                ld (hl),'I'
                inc hl
                ld (hl),'1'
                inc hl
                ld (hl),'6'
                ld hl,nm
                call f_open
                cp $FF
                jr nz,tl_open
                ld hl,nm+7              ; no hay .I16: IMGnnn.BMP
                ld (hl),'B'
                inc hl
                ld (hl),'M'
                inc hl
                ld (hl),'P'
                ld hl,nm
                ld a,(fitmode)
                ld e,a
                call f_open_img
                cp 4
                jr c,tl_open
                scf
                ret
tl_open:        ld (fh),a
                ld hl,hdr
                ld bc,4
                call f_read_n
                ld hl,hdr
                ld de,magic
                ld b,4
tl_m:           ld a,(de)
                cp (hl)
                jr nz,tl_bad
                inc hl
                inc de
                djnz tl_m
                call blackpal
                ld a,(fh)
                ld hl,palbuf
                ld bc,512
                call f_read_n
                ld a,(fh)
                ld hl,IMG
                ld bc,IMGLEN
                call f_read_n
                ld a,(fh)
                call f_close
                call setpal
                or a
                ret
tl_bad:         ld a,(fh)
                call f_close
                scf
                ret

; las cifras de (cand) en "IMGnnn.I16"
mk_name:        ld a,(cand)
                ld b,'0'-1
mn_h:           inc b
                sub 100
                jr nc,mn_h
                add a,100
                ld hl,nm+3
                ld (hl),b
                ld b,'0'-1
mn_t:           inc b
                sub 10
                jr nc,mn_t
                add a,10+'0'
                inc hl
                ld (hl),b
                inc hl
                ld (hl),a
                ret

; paleta: POKE 2063 (puntero), 2064 (byte bajo), 2065 (byte alto: escribe la entrada y el puntero avanza)
blackpal:       xor a
                ld (2063),a
                ld b,0                  ; 256 entradas
bp_l:           ld (2064),a
                ld (2065),a
                djnz bp_l
                ret

setpal:         xor a
                ld (2063),a
                ld hl,palbuf
                ld b,0
sp_l:           ld a,(hl)
                ld (2064),a
                inc hl
                ld a,(hl)
                ld (2065),a
                inc hl
                djnz sp_l
                ret

nm:             defb "IMG000.I16",0
magic:          defb "I160"
down:           defb 0
fh:             defb 0
fitmode:        defb 0
picno:          defb 0
cand:           defb 0
hdr:            defb 0,0,0,0
palbuf:         defs 512
