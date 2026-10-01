; =====================================================================
;  DBGMON.ASM - monitor de juguete del depurador por hardware (fase 1)
;
;  Se copia a la SD como /SYS/DEBUG.BIN. El MCU lo escribe al encender en la
;  pagina 63 y arma el depurador (orden 9). El Z80 lo ve en $2000-$3FFF
;  mientras esta parado: la FPGA sirve CALL $2000 al romper y pone la pagina
;  63 en el bloque 1 (ver FPGA/SD81V2.1000/sim_int.v).
;
;  No habla con el MCU (eso es la fase 2). En cada ruptura:
;   1. Apunta en el registro del programa de prueba (ver dbgtest.asm) la
;      direccion X de la instruccion donde ha parado y el estado del
;      depurador (puerto $3FEF, indice 0: armado, NMI, nivel, motivo).
;   2. Si el motivo es un FF de la memoria (breakpoint por software) en la
;      direccion que dice el buzon, repone el byte original.
;   3. Si el buzon pide mas pasos, programa N = 1 (romper en la siguiente).
;   4. Vuelve con JP $003B, con SP como estaba antes del CALL (sin el
;      $003B): la FPGA sirve el epilogo, que corrige X+1 -> X y vuelve.
;
;  Buzon (en la RAM del programa, MB):
;    MB+0  puntero del registro (3 bytes por ruptura: X, estado)
;    MB+2  pasos que quedan
;    MB+3  direccion del breakpoint por software
;    MB+5  byte original de esa direccion
;    MB+6  rupturas apuntadas
;    MB+7  marca 'D','T': sin ella el buzon no es valido y el monitor vuelve
;          sin tocar nada (una pausa desde la consola fuera de la prueba)
;
;  Ensamblar: pasmo dbgmon.asm debug.bin
; =====================================================================

DBG     equ 3FEFh       ; puerto del depurador
MB      equ 7F00h       ; buzon del programa de prueba

        org  2000h

        jp   entry
        defb "SD81DBG",0        ; $2003: firma del monitor

entry:  ld   (save_sp),sp       ; la pila del programa: [SP] = $003B,
        ld   sp,mon_stack       ; [SP+2] = X+1
        push af
        push bc
        push de
        push hl

        ld   hl,(MB+7)          ; buzon valido?
        ld   de,'T'*256+'D'
        or   a
        sbc  hl,de
        jr   nz,go

        ld   hl,(save_sp)       ; DE = X
        inc  hl
        inc  hl
        ld   e,(hl)
        inc  hl
        ld   d,(hl)
        dec  de

        ld   bc,DBG             ; estado del depurador (indice 0)
        xor  a
        out  (c),a
        in   a,(c)
        ld   (status),a

        ld   hl,(MB)            ; apuntar X y el estado
        ld   (hl),e
        inc  hl
        ld   (hl),d
        inc  hl
        ld   (hl),a
        inc  hl
        ld   (MB),hl
        ld   hl,MB+6
        inc  (hl)

        and  7                  ; breakpoint por software en el buzon:
        cp   7                  ; reponer el byte original
        jr   nz,no_sw
        ld   hl,(MB+3)
        or   a
        sbc  hl,de
        jr   nz,no_sw
        ld   a,(MB+5)
        ld   (de),a

no_sw:  ld   hl,MB+2            ; mas pasos: N = 1
        ld   a,(hl)
        or   a
        jr   z,go
        dec  (hl)
        ld   bc,DBG
        ld   a,83h
        out  (c),a
        ld   a,1
        out  (c),a
        ld   a,84h
        out  (c),a
        xor  a
        out  (c),a

go:     pop  hl
        pop  de
        pop  bc
        pop  af
        ld   sp,(save_sp)       ; la pila del programa, sin el $003B que
        inc  sp                 ; dejo el CALL: el epilogo tiene que
        inc  sp                 ; encontrar X+1 arriba (en las interrupciones
        jp   003Bh              ; lo quita el RET de la rutina)

save_sp:    defw 0
status:     defb 0
            defs 64
mon_stack:

            end
