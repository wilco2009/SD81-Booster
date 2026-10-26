; =====================================================================
;  agiview.asm - visor de las imagenes PICnnn.GFX (AGI v2 convertidas por TOOLS/agi2hr.py) en el HiRes 8x1 del
;  ZX81 con SD81 Booster. Sirve para probar el formato y medir el tiempo de carga antes de hacer el interprete.
;
;  Teclas:  N = la imagen siguiente   B = la anterior   ESPACIO = salir a BASIC
;  Los ficheros PICnnn.GFX estan en la carpeta actual (la del AGIVIEW.B81).
;
;  Cargar: AGIVIEW.BIN en $4200 (LOAD FAST "../AGIVIEW.BIN" CODE 16896) y RAND USR 16914.
;  Ensamblar: pasmo agiview.asm agiview.bin agiview.sym   (compila.bat)
;  La entrada ($4212), el bucle `main_loop` y el paginado son los del arnes de PC de SCUMM (SCUMM/V2/ENGINE/test),
;  que lo ejecuta tal cual: ver test.sh
;
;  Memoria: $4200-  codigo   $5F00 cabecera   $6000-$6FFF prioridad (RLE)   $7F00 pila
;           $A000 bitmap (HFILE)   $C000 atributos de HiRes modo 0
; =====================================================================

                org 16896
                jp start
                defs 16914-$
                jp start                ; RAND USR 16914 (y el arnes de PC entran aqui)

SET_FAST        equ $02E7
SLOW_FAST       equ $0207
MAP_PORT        equ $E7
CMD_64C         equ $1C
HR_BM           equ $A000
HR_ATT          equ $C000
PLANE           equ 168*32              ; bitmap y atributos de la imagen: 168 lineas de 32 bytes
HDR             equ $5F00
PRI             equ $6000
PRIMAX          equ 4096
STACK           equ $7F00

                include "mcu.asm"

start:          di
                ld (sv_sp),sp
                ld sp,STACK
                ld a,i
                ld (sv_i),a
                call SET_FAST
                ld a,CMD_FULLPAGING
                call mcu_send
                ld b,6                  ; bloques 6 y 7 a sus paginas ($C000 son los atributos de video)
                ld a,6
                call sc_map
                ld b,7
                ld a,7
                call sc_map
                ld a,85
                ld (2058),a             ; WRX apagado
                xor a
                ld (2043),a
                ld a,HR_BM>>8
                ld (2044),a             ; HFILE = bitmap
                ld a,171
                ld (2045),a             ; Superfast HiRes nativo
                call clear
                ld bc,$7FEF
                ld a,$20                ; Chroma, modo 0
                out (c),a
                ld a,255
                ld (2091),a
                ld (2092),a
                ld (2093),a
                xor a
                ld (2090),a
                ld (down),a
                inc a
                ld (picno),a
                ld (cand),a
                call try_load           ; la primera imagen (si no esta la 1, la siguiente que haya)
                jr nc,main_loop
                call next

main_loop:      call scan               ; cada pasada es un "fotograma" para el arnes
                jr nz,ml_key
                ld (down),a
                jr main_loop
ml_key:         ld b,a
                ld a,(down)
                or a
                jr nz,main_loop         ; la tecla sigue pulsada
                inc a
                ld (down),a
                bit 0,b
                jr nz,exit
                bit 3,b
                jr z,ml_b
                call next
                jr main_loop
ml_b:           bit 4,b
                call nz,prev
                jr main_loop

exit:           ld bc,$7FEF
                ld a,$0C
                out (c),a
                ld a,CMD_64C
                call mcu_send
                ld a,(sv_i)
                ld i,a
                call SLOW_FAST
                ld sp,(sv_sp)
                ld bc,0
                ret

sc_map:         ld c,MAP_PORT
                out (c),a
                ret

; teclado: A = bits 0 ESPACIO, 3 N, 4 B (1 = pulsada); Z si no hay ninguna
scan:           ld a,$7F
                in a,(254)
                cpl
                and $19
                ret

; la imagen siguiente / anterior que exista (como mucho 255 intentos); la actual se queda si no hay otra
next:           ld a,(picno)
                ld (orig),a
                ld d,1
                jr step
prev:           ld a,(picno)
                ld (orig),a
                ld d,255                ; -1
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
                ld a,(orig)
                ld (cand),a
                ret
st_ok:          ld a,(cand)
                ld (picno),a
                ret

; pone en pantalla PICnnn.GFX con nnn = (cand). Carry si no se puede
try_load:       call mk_name
                ld hl,nm
                call f_open
                cp $FF
                jr nz,tl_open
                scf
                ret
tl_open:        ld (fh),a
                ld hl,HDR
                ld bc,256
                call f_read_n
                ld a,(HDR)
                cp 'A'
                jr nz,tl_bad
                ld a,(HDR+3)
                cp 'P'
                jr nz,tl_bad
                ld bc,(HDR+10)          ; bytes de prioridad, con tope
                ld hl,PRIMAX
                or a
                sbc hl,bc
                jr nc,tl_p
                ld bc,PRIMAX
tl_p:           ld (prilen),bc
                ld a,(fh)
                ld hl,HR_BM
                ld bc,PLANE
                call f_read_n
                ld a,(fh)
                ld hl,HR_ATT
                ld bc,PLANE
                call f_read_n
                ld a,(fh)
                ld hl,PRI
                ld bc,(prilen)
                call f_read_n
                ld a,(fh)
                call f_close
                or a
                ret
tl_bad:         ld a,(fh)
                call f_close
                scf
                ret

; las cifras de (cand) en "PICnnn.GFX"
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

clear:          ld hl,HR_BM
                ld de,HR_BM+1
                ld bc,$1FFF
                ld (hl),0
                ldir
                ld hl,HR_ATT
                ld de,HR_ATT+1
                ld bc,$1FFF
                ld (hl),0
                ldir
                ret

nm:             defb "PIC000.GFX",0
sv_sp:          defw 0
sv_i:           defb 0
picno:          defb 0
cand:           defb 0
orig:           defb 0
down:           defb 0
fh:             defb 0
prilen:         defw 0
