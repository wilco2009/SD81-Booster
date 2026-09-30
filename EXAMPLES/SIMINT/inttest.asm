; =====================================================================
;  INTTEST.ASM - prueba de las interrupciones simuladas (paso 2)
;
;  En Superfast y con DI, la FPGA inyecta un RST 38h en un limite de
;  instruccion con cada VSYNC y en $0038 sirve CALL a la rutina de
;  POKE 2038/2039 (ver sim_int.v). La prueba:
;
;   1. Ejecuta una carga de trabajo determinista, con instrucciones de
;      todas las familias de prefijos (CB, ED, LDIR, DD, DD CB, prefijos
;      repetidos, DD ED), SIN interrupciones: checksum de referencia.
;   2. La repite CON interrupciones; la rutina solo cuenta.
;   3. El checksum tiene que salir igual (si un RST partiera una
;      instruccion o se saltara un byte, cambiaria o se colgaria), y las
;      interrupciones tienen que coincidir con las tramas que han pasado
;      (FRAMES, que en Superfast lleva la FPGA) y con las que cuenta la
;      propia FPGA (puerto $3FEF, indices 9/10).
;
;  USR devuelve 0 si todo cuadra, 1 si no y 2 si la FPGA no tiene
;  detector. Los resultados quedan en 24579 (tabla r_*), para el stub.
;
;  Mientras corre (en Superfast se ve el DFILE en directo) deja señales en
;  la primera fila, para saber donde se para si se cuelga:
;      columna 0  la pasada: 1 (sin interrupciones) o 2 (con ellas)
;      columna 2  la rutina de interrupcion lo incrementa: si cambia, entran
;      columna 4  el progreso de la carga (la vuelta exterior)
;  El stub hace CLS antes, para que el DFILE este completo.
;
;  Ensamblar: pasmo inttest.asm inttest.bin
; =====================================================================

SET_FAST    equ 02E7h
SV_DFILE    equ 16396
SLOW_FAST   equ 0207h
FRAMES      equ 16436
DBG         equ 3FEFh           ; puerto del detector
PASSES      equ 100             ; vueltas de 256 de la carga (~3 s por pasada)

            org  24576

            jp   start
r_ref:      defw 0              ; 24579: checksum sin interrupciones
r_chk:      defw 0              ; 24581: checksum con interrupciones
r_ints:     defw 0              ; 24583: interrupciones (las cuenta la rutina)
r_frames:   defw 0              ; 24585: tramas que han pasado (FRAMES)
r_fpga:     defw 0              ; 24587: interrupciones que cuenta la FPGA

start:      call SET_FAST
            di
            push ix             ; la carga cambia IX

            ld   bc,DBG         ; hay detector?
            ld   a,15
            out  (c),a
            in   a,(c)
            cp   51h
            ld   hl,2
            jp   nz,done

            ld   hl,(SV_DFILE)  ; primera fila de la pantalla, para las
            inc  hl             ; señales
            ld   (scr),hl
            ld   a,170          ; Superfast texto: las interrupciones solo
            ld   (2045),a       ; funcionan en Superfast
            ld   hl,isr         ; la rutina
            ld   (2038),hl

            ld   hl,(scr)
            ld   (hl),29        ; '1'
            ld   a,0            ; 1. sin interrupciones
            ld   (2040),a
            call work
            ld   hl,(chk)
            ld   (r_ref),hl

            ld   hl,(scr)
            ld   (hl),30        ; '2'
            ld   hl,0           ; 2. con interrupciones
            ld   (ints),hl
            ld   bc,DBG         ; contador de la FPGA al empezar
            call rd_fpga
            ld   (fpga0),hl
            ld   hl,(FRAMES)
            ld   (frames0),hl
            ld   a,1
            ld   (2040),a
            call work
            xor  a
            ld   (2040),a
            ld   hl,(FRAMES)    ; FRAMES cuenta hacia abajo (15 bits)
            ex   de,hl
            ld   hl,(frames0)
            or   a
            sbc  hl,de
            ld   a,h
            and  7Fh
            ld   h,a
            ld   (r_frames),hl
            ld   hl,(chk)
            ld   (r_chk),hl
            ld   hl,(ints)
            ld   (r_ints),hl
            ld   bc,DBG
            call rd_fpga
            ld   de,(fpga0)
            or   a
            sbc  hl,de
            ld   (r_fpga),hl

            ld   hl,1           ; 3. comprobaciones (HL = resultado)
            ld   de,(r_ref)     ; mismo checksum
            ld   a,(r_chk)
            cp   e
            jr   nz,done
            ld   a,(r_chk+1)
            cp   d
            jr   nz,done
            ld   de,(r_ints)    ; la FPGA cuenta las mismas que la rutina
            ld   a,(r_fpga)
            cp   e
            jr   nz,done
            ld   a,(r_fpga+1)
            cp   d
            jr   nz,done
            ld   a,d            ; tiene que haber habido interrupciones
            or   e
            jr   z,done
            push hl             ; tramas - interrupciones, entre -1 y +1
            ld   hl,(r_frames)
            or   a
            sbc  hl,de
            inc  hl             ; -1..+1 -> 0..2
            ld   a,h
            or   a
            jr   nz,chk_bad
            ld   a,l
            cp   3
            jr   nc,chk_bad
            pop  hl
            ld   hl,0           ; todo cuadra
            jr   done
chk_bad:    pop  hl

done:       xor  a
            ld   (2040),a       ; interrupciones fuera
            ld   a,85
            ld   (2045),a       ; video nativo
            pop  ix
            push hl
            call SLOW_FAST
            pop  bc             ; USR devuelve BC
            ret

; HL = interrupciones que lleva contadas la FPGA (puerto $3FEF, 9/10)
rd_fpga:    ld   a,9
            out  (c),a
            in   l,(c)
            ld   a,10
            out  (c),a
            in   h,(c)
            ret

; la rutina de interrupcion: cuenta y vuelve (RET normal: el epilogo que
; sirve la FPGA en $003B se encarga de la direccion de retorno)
isr:        push af
            push hl
            ld   hl,(ints)
            inc  hl
            ld   (ints),hl
            ld   hl,(scr)       ; señal en la columna 2
            inc  hl
            inc  hl
            inc  (hl)
            pop  hl
            pop  af
            ret

; ---------------------------------------------------------------------
;  Carga de trabajo: todo lo que hace entra en el checksum (chk), asi que
;  un byte perdido o una instruccion partida lo cambiarian
; ---------------------------------------------------------------------
work:       ld   hl,1234h
            ld   (chk),hl
            ld   hl,src         ; estado inicial de los buffers
            ld   de,buf
            ld   bc,8
            ldir
            ld   ix,buf
            ld   c,PASSES
w_outer:    ld   hl,(scr)       ; progreso en la columna 4 (no entra en
            inc  hl             ; el checksum)
            inc  hl
            inc  hl
            inc  hl
            ld   (hl),c
            ld   b,0
w_inner:    ld   a,(chk)
            rlc  a                      ; CB 07
            xor  b
            neg                         ; ED 44
            add  a,(ix+1)               ; DD 86 01
            rlc  (ix+2)                 ; DD CB 02 06
            ld   (ix+1),a               ; DD 77 01
            srl  a                      ; CB 3F
            bit  3,(ix+3)               ; DD CB 03 5E
            jr   z,w_nz
            inc  a
w_nz:       defb 0DDh,0DDh,086h,003h    ; DD DD ADD A,(IX+3)
            defb 0DDh,0EDh,044h         ; DD NEG
            push bc
            ld   hl,buf                 ; LDIR de 4 bytes
            ld   de,buf+4
            ld   bc,4
            ldir
            pop  bc
            ld   hl,(chk)
            ld   e,a
            ld   d,0
            add  hl,de
            add  hl,hl
            jr   nc,w_nc
            inc  hl
w_nc:       ld   (chk),hl
            djnz w_inner
            dec  c
            jr   nz,w_outer
            ret

src:        defb 11h,22h,33h,44h,55h,66h,77h,88h
buf:        defs 8
chk:        defw 0
ints:       defw 0
fpga0:      defw 0
scr:        defw 0
frames0:    defw 0

            end
