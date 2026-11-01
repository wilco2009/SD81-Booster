; =============================================================
; IMGVIEW.ASM -- visor de una imagen (.BMP o .I16) para el explorador
;
; Lo lanza el stub BASIC del explorador (EXPLORER.B81 / AUTOEXEC.P) al abrir con 8 / ENTER un archivo .BMP o .I16, como hace con .SCR, .VGM o el editor:
; carga /EXPLORER/IMGVIEW.BIN en 24576, lo arranca con USR y el nombre del archivo va en F$ (en ASCII tras convertirlo del ZX81, como EDIT).
; FPGA de la rama fpga-mode160: modo 160x100 a 256 colores (POKE 2045,175; ver claude/planning/pdm_color.md).
;
;   .I16  se lee con fopen (53): "I160", 512 bytes de paleta y 16000 pixeles (EXAMPLES/PHOTO).
;   .BMP  se abre con el comando 76 del MCU, que lo convierte (escala a 160x100 y 256 colores, tarda unos segundos) y lo ofrece como un .I16.
;
; Teclas:  F = (solo BMP) cambia como se coloca la imagen y la vuelve a cargar: con su proporcion (bandas negras; por omision), recortada o estirada
;          cualquier otra tecla = salir al explorador
;
; Devuelve BC = 0 si todo fue bien, 1 si no se puede abrir (no hay F$, no es .BMP / .I16, no existe o no es un .I16) y 2 si el MCU no puede
; convertir el BMP (el motivo queda en /MAN/BMPERR.TXT: LOAD THEN PRINT "*BMPERR"). En los errores no llega a tocar la pantalla.
;
; Memoria: el programa y sus variables desde 24576; la imagen en $8000-$BE7F (HFILE = $8000).
; Ensamblar: pasmo imgview.asm imgview.bin imgview.sym   (compila.bat)
; =============================================================

SET_FAST        equ 02E7h               ; ROM: apaga el video si estaba en SLOW
SLOW_FAST       equ 0207h               ; ROM: vuelve al modo que pide CDFLAG
SV_VARS         equ 16400               ; variable del sistema VARS
FVAR            equ 04Bh                ; F$ en la zona de variables: 010 + letra F
IMG             equ $8000
IMGLEN          equ 160*100
CHROMA_P        equ $7FEF
CHROMA_ON       equ $20                 ; color activado, borde negro
CHROMA_OFF      equ $0C

                org 24576
                jp start

                include "../PHOTO/mcu.asm"

start:          call SET_FAST
                xor a
                ld (modeon),a
                inc a
                ld (fitmode),a          ; con su proporcion
                call find_fvar
                ld a,(name_len)
                or a
                jr z,st_bad             ; sin F$
                call ext_type
                or a
                jr z,st_bad             ; ni .I16 ni .BMP
                ld (kind),a
                call load_image
                jr c,fail
                call key_up             ; suelta la tecla con la que se abrio
                call debounce
                call key_up

view:           ld a,$FD                ; F: fila A-G, bit 3
                in a,(254)
                bit 3,a
                jr z,vw_fit
                call key_other
                jr z,view               ; ninguna tecla
                jr exit
vw_fit:         ld a,(kind)
                cp 2
                jr nz,vw_rel            ; la F no hace nada con un .I16
                ld a,(fitmode)
                inc a
                cp 3
                jr c,vw_ok
                xor a
vw_ok:          ld (fitmode),a
                call load_image         ; si no se puede, se queda la imagen que habia
vw_rel:         call key_up
                call debounce
                call key_up
                jr view

exit:           ld a,85
                ld (2045),a             ; modo ZX81 normal
                ld bc,CHROMA_P
                ld a,CHROMA_OFF
                out (c),a
                call SLOW_FAST
                ld bc,0
                ret

st_bad:         ld a,1
fail:           ld c,a                  ; error: BC = 1 o 2
                ld b,0
                push bc
                call SLOW_FAST
                pop bc
                ret

; teclado: Z si ninguna tecla; NZ si hay alguna que no sea la F. Se leen las 8 filas una a una y en la de la F se ignora su bit: con un solo
; barrido de todas, el rebote de la F al pulsarla (abierta en la lectura de arriba y cerrada en esta) parecia otra tecla y se salia
key_other:      ld bc,$FEFE
ko_l:           in a,(c)
                ld d,a
                ld a,b
                cp $FD
                ld a,d
                jr nz,ko_n
                or 8                    ; fila A-G: el bit 3 es la F
ko_n:           cpl
                and $1F
                ret nz
                rlc b                   ; FE, FD, FB, F7, EF, DF, BF, 7F y de nuevo FE (acarreo 0)
                jr c,ko_l
                xor a
                ret

key_up:         xor a
                in a,(254)
                cpl
                and $1F
                jr nz,key_up
                ret

; unos 20 ms (a 3,25 MHz: 20 vueltas de 256 DJNZ de 13 T-states): el rebote de la tecla al pulsar y al soltar
debounce:       ld c,20
db_o:           ld b,0
db_i:           djnz db_i
                dec c
                jr nz,db_o
                ret

; A = 1 si el nombre (name_buf, ASCII en mayusculas) acaba en .I16, 2 si en .BMP, 0 si en otra cosa
ext_type:       ld a,(name_len)
                cp 5
                jr c,et_none
                ld c,a
                ld b,0
                ld hl,name_buf
                add hl,bc
                ld de,-4
                add hl,de               ; HL -> el punto
                ld de,ext_i16
                call cmp4
                jr z,et_i16
                ld de,ext_bmp
                call cmp4
                jr z,et_bmp
et_none:        xor a
                ret
et_i16:         ld a,1
                ret
et_bmp:         ld a,2
                ret

; Z si los 4 bytes de (HL) y (DE) son iguales
cmp4:           push hl
                push de
                ld b,4
c4_l:           ld a,(de)
                cp (hl)
                jr nz,c4_x
                inc hl
                inc de
                djnz c4_l
c4_x:           pop de
                pop hl
                ret

; abre name_buf (.I16 con fopen; .BMP con el comando 76 y la colocacion (fitmode)) y la pone en pantalla.
; Carry si no se puede: A = 1 (no existe, o no es un .I16) o 2 (el MCU no convierte el BMP); en ese caso no se toca la pantalla
load_image:     ld hl,name_buf
                ld a,(kind)
                cp 2
                jr z,li_bmp
                call f_open
                cp $FF
                jr nz,li_open
li_err1:        ld a,1
                scf
                ret
li_bmp:         ld a,(fitmode)
                ld e,a
                call f_open_img
                cp 4
                jr c,li_open
                cp $FF
                jr z,li_err1
                ld a,2
                scf
                ret
li_open:        ld (fh),a
                ld hl,hdr
                ld bc,4
                call f_read_n
                ld hl,hdr
                ld de,magic
                ld b,4
li_m:           ld a,(de)
                cp (hl)
                jr nz,li_bad
                inc hl
                inc de
                djnz li_m
                call blackpal           ; no se ve nada a medias ni con la paleta de la imagen anterior
                ld a,(modeon)
                or a
                call z,mode_on
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
li_bad:         ld a,(fh)
                call f_close
                jr li_err1

; la primera vez: HFILE, color y el modo 160x100 (la paleta ya esta en negro)
mode_on:        xor a
                ld (2043),a
                ld a,IMG>>8
                ld (2044),a             ; HFILE = $8000
                ld bc,CHROMA_P
                ld a,CHROMA_ON
                out (c),a
                ld a,175
                ld (2045),a
                ld a,1
                ld (modeon),a
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

; F$ -> name_buf (en ASCII). Sin F$, o vacia, name_len = 0. (De EXAMPLES/EDIT/edit.asm.)
find_fvar:      xor a
                ld (name_len),a
                ld hl,(SV_VARS)
fv1:            ld a,(hl)
                cp 80h                  ; fin de las variables
                ret z
                ld c,a
                and 0E0h
                cp 040h                 ; 010: cadena
                jr z,fv_str
                cp 060h                 ; 011: numero de una letra
                ld de,6
                jr z,fv_skip
                cp 0E0h                 ; 111: variable de FOR
                ld de,18
                jr z,fv_skip
                cp 0A0h                 ; 101: numero de nombre largo
                jr z,fv_long
fv_len:         inc hl                  ; matrices y cadenas: longitud y datos
                ld e,(hl)
                inc hl
                ld d,(hl)
                inc hl
fv_skip:        add hl,de
                jr fv1
fv_long:        inc hl                  ; letras hasta la que lleva el bit 7
fvl1:           ld a,(hl)
                inc hl
                bit 7,a
                jr z,fvl1
                ld de,5
                jr fv_skip
fv_str:         ld a,c
                cp FVAR
                jr nz,fv_len
                inc hl                  ; F$: longitud y texto
                ld c,(hl)
                inc hl
                ld b,(hl)
                inc hl
                ld a,b                  ; mas de 63 caracteres: se corta
                or a
                jr nz,fvs1
                ld a,c
                cp 64
                jr c,fvs2
fvs1:           ld c,63
fvs2:           ld a,c
                ld (name_len),a
                or a
                ret z
                ld b,c
                ld de,name_buf
fvs3:           ld a,(hl)               ; codigo ZX81 -> ASCII
                and 03Fh
                push hl
                ld hl,zx2ascii
                add a,l
                ld l,a
                jr nc,fvs4
                inc h
fvs4:           ld a,(hl)
                pop hl
                ld (de),a
                inc hl
                inc de
                djnz fvs3
                ret

zx2ascii:       defb " ??????????",022h,"#$:?()><=+-*/;,.0123456789"
                defb "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

ext_i16:        defb ".I16"
ext_bmp:        defb ".BMP"
magic:          defb "I160"
modeon:         defb 0                  ; 1 = el modo 160x100 ya esta encendido
fitmode:        defb 1                  ; 0 estira, 1 conserva la proporcion, 2 recorta (solo BMP)
kind:           defb 0                  ; 1 = .I16, 2 = .BMP
fh:             defb 0
name_len:       defb 0
hdr:            defb 0,0,0,0
name_buf:       defs 64
palbuf:         defs 512
