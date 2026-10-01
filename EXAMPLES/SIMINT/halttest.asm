; =====================================================================
;  HALTTEST.ASM - prueba de las interrupciones simuladas (paso 3: HALT)
;
;  Con las interrupciones simuladas activas, un HALT (o DD HALT) se sirve
;  como RST 38h y la FPGA espera en $0038 (JR $) hasta el VSYNC; despues
;  llama a la rutina y vuelve DETRAS del HALT (ver sim_int.v). La prueba:
;
;   1. 100 HALT (50 HALT y 50 DD HALT): cada uno tiene que esperar una
;      trama y volver una sola vez a la instruccion de detras. Tienen que
;      salir 100 vueltas, 100 interrupciones y 100 tramas (+-1). Si volviera al propio HALT saldrian 200 tramas; si no
;      esperara, casi ninguna.
;   2. CB 76 (BIT 6,(HL)), ED 76 y DD CB d 76 (BIT 6,(IX+d)) NO son HALT:
;      20000 vueltas con las interrupciones activas tienen que tardar unas
;      pocas tramas (si esperaran, seria una trama por instruccion), y las
;      interrupciones tienen que coincidir con las tramas (+-1).
;
;  USR devuelve 0 si todo cuadra, 1 si no y 2 si la FPGA no tiene
;  detector. Los resultados quedan en 24579 (tabla r_*), para el stub.
;
;  Mientras corre deja en la primera fila la prueba en curso (columna 0) y
;  un caracter que incrementa la rutina de interrupcion (columna 2).
;
;  Ensamblar: pasmo halttest.asm halttest.bin
; =====================================================================

SET_FAST    equ 02E7h
SLOW_FAST   equ 0207h
SV_DFILE    equ 16396
FRAMES      equ 16436
DBG         equ 3FEFh           ; puerto del detector (firma)
NHALT       equ 100             ; HALT de la prueba 1 (par)
NPFX        equ 20000           ; vueltas de la prueba 2

            org  24576

            jp   start
r_after:    defw 0              ; 24579: vueltas detras del HALT (NHALT)
r_hints:    defw 0              ; 24581: HALT: interrupciones
r_hframes:  defw 0              ; 24583: HALT: tramas
r_pints:    defw 0              ; 24585: prefijos: interrupciones
r_pframes:  defw 0              ; 24587: prefijos: tramas

start:      call SET_FAST
            di
            push ix             ; la prueba 2 cambia IX

            ld   bc,DBG         ; hay detector?
            ld   a,15
            out  (c),a
            in   a,(c)
            cp   51h            ; 51h: interrupciones simuladas;
            jr   z,sig_ok       ; 52h: tambien el depurador
            cp   52h
sig_ok:     ld   hl,2
            jp   nz,done

            ld   hl,(SV_DFILE)  ; primera fila de la pantalla, para las
            inc  hl             ; señales
            ld   (scr),hl
            ld   a,170          ; Superfast texto: las interrupciones solo
            ld   (2045),a       ; funcionan en Superfast
            ld   hl,isr         ; la rutina
            ld   (2038),hl

            ld   hl,(scr)       ; 1. HALT y DD HALT
            ld   (hl),29        ; '1'
            call m_begin
            ld   de,0
            ld   b,NHALT/2
h_loop:     halt
            inc  de             ; una vez por HALT
            defb 0DDh,076h      ; DD HALT
            inc  de
            djnz h_loop
            ld   (r_after),de
            ld   hl,r_hints
            call m_end

            ld   hl,(scr)       ; 2. CB 76, ED 76 y DD CB d 76
            ld   (hl),30        ; '2'
            call m_begin
            ld   hl,buf
            ld   ix,buf
            ld   bc,NPFX
p_loop:     bit  6,(hl)                 ; CB 76
            defb 0EDh,076h              ; ED 76 (IM 1)
            bit  6,(ix+0)               ; DD CB 00 76
            dec  bc
            ld   a,b
            or   c
            jr   nz,p_loop
            ld   hl,r_pints
            call m_end

            ld   de,NHALT       ; 3. comprobaciones
            ld   hl,r_after
            call eqw
            jr   nz,fail
            ld   hl,r_hints
            call eqw
            jr   nz,fail
            ld   hl,r_hframes   ; tramas = NHALT +-1
            call near1
            jr   nc,fail
            ld   de,(r_pints)   ; prefijos: las interrupciones coinciden
            ld   hl,r_pframes   ; con las tramas
            call near1
            jr   nc,fail
            ld   hl,(r_pframes) ; pocas tramas: ninguno ha esperado
            ld   de,60
            or   a
            sbc  hl,de
            jr   nc,fail
            ld   hl,0           ; todo cuadra
            jr   done
fail:       ld   hl,1

done:       xor  a
            ld   (2040),a       ; interrupciones fuera
            ld   a,85
            ld   (2045),a       ; video nativo
            pop  ix
            push hl
            call SLOW_FAST
            pop  bc             ; USR devuelve BC
            ret

; empieza una medida: interrupciones a 0, FRAMES de partida, y las
; interrupciones activas
m_begin:    ld   hl,0
            ld   (ints),hl
            ld   hl,(FRAMES)
            ld   (frames0),hl
            ld   a,1
            ld   (2040),a
            ret

; acaba una medida: desactiva y guarda en (HL) las interrupciones de la
; rutina y las tramas que han pasado
m_end:      xor  a
            ld   (2040),a
            push hl
            ld   hl,(FRAMES)    ; FRAMES cuenta hacia abajo (15 bits)
            ex   de,hl
            ld   hl,(frames0)
            or   a
            sbc  hl,de
            ld   a,h
            and  7Fh
            ld   h,a
            ld   (tmpf),hl
            pop  hl
            ld   de,(ints)
            ld   (hl),e
            inc  hl
            ld   (hl),d
            inc  hl
            ld   de,(tmpf)
            ld   (hl),e
            inc  hl
            ld   (hl),d
            ret

; Z si la palabra en (HL) es DE
eqw:        ld   a,(hl)
            cp   e
            ret  nz
            inc  hl
            ld   a,(hl)
            cp   d
            ret

; C si la palabra en (HL) esta entre DE-1 y DE+1
near1:      ld   a,(hl)
            inc  hl
            ld   h,(hl)
            ld   l,a
            or   a
            sbc  hl,de
            inc  hl             ; -1..+1 -> 0..2
            ld   a,h
            or   a
            ret  nz             ; (OR deja C a 0)
            ld   a,l
            cp   3
            ret

; la rutina de interrupcion: cuenta y vuelve con un RET normal
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

buf:        defb 40h
ints:       defw 0
frames0:    defw 0
tmpf:       defw 0
scr:        defw 0

            end
