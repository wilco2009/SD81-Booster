; =====================================================================
;  DBGTEST.ASM - prueba del depurador por hardware (fase 1)
;
;  Necesita la FPGA con el depurador (firma 52h) y el monitor de juguete
;  (dbgmon.asm) en la SD como /SYS/DEBUG.BIN: el MCU lo carga al encender.
;  El monitor apunta cada ruptura (direccion y estado) en LOG; la prueba lo
;  compara con lo esperado.
;
;   1. Trampa (OUT $10): rompe en la instruccion siguiente.
;   2. Paso a paso: trampa y 4 pasos por instrucciones con prefijos CB, ED
;      y DD: 5 rupturas, una por instruccion.
;   3. Comparador de ejecucion: dos llamadas a la misma rutina, dos
;      rupturas, y la rutina se ejecuta las dos veces (al continuar no
;      vuelve a romper en la misma instruccion).
;   4. Punto de vigilancia de escritura: rompe detras del LD (nn),A.
;   5. Breakpoint por software: FF en una instruccion; el monitor repone el
;      byte y la instruccion se ejecuta.
;   6. Anidado: breakpoint por software dentro de la rutina de interrupcion
;      simulada (Superfast + POKE 2040), despertando de un HALT. Rompe con
;      nivel = rutina, y las interrupciones siguen.
;   7. El paso se queda en su nivel: 250 pasos por un bucle con las
;      interrupciones simuladas activas. Ninguna ruptura dentro de la
;      rutina, y alguna interrupcion por medio.
;   8. La pagina 63 esta protegida: con la paginacion completa (LOAD
;      *FULLPAG antes) se mapea en el bloque 5, se lee el JP del monitor
;      ($C3), se intenta escribir encima y tiene que seguir ahi. Sin la
;      paginacion completa no se puede mapear y la prueba se salta.
;
;  Mientras corre (en Superfast se ve el DFILE en directo), la primera
;  columna de la primera fila dice la prueba en curso: si algo se cuelga, se
;  ve en cual. El stub hace CLS antes, para que el DFILE este completo.
;
;  USR devuelve 0 si todo cuadra, el numero de la prueba que falla, 254 si
;  la FPGA no tiene el depurador o 253 si no hay monitor cargado.
;
;  Ensamblar: pasmo dbgtest.asm dbgtest.bin
; =====================================================================

SET_FAST    equ 02E7h
SV_DFILE    equ 16396
SLOW_FAST   equ 0207h
DBG         equ 3FEFh           ; puerto del depurador
MB          equ 7F00h           ; buzon del monitor (ver dbgmon.asm)
LOG         equ 7800h           ; registro de rupturas: X (2) y estado (1)
NSTEP7      equ 250

            org  24576

            jp   start
r_test:     defb 0              ; 24579: prueba que falla (0 = ninguna)
r_count:    defb 0              ; 24580: rupturas apuntadas en esa prueba
r_ints:     defw 0              ; 24581: interrupciones de la prueba 7
r_p63:      defb 0              ; 24583: prueba 8: 0 saltada, 1 hecha

start:      call SET_FAST
            di
            push ix             ; la prueba 2 cambia IX
            ld   bc,DBG         ; FPGA con depurador?
            ld   a,15
            out  (c),a
            in   a,(c)
            cp   52h
            ld   a,254
            jp   nz,result
            xor  a              ; armado (monitor cargado)?
            out  (c),a
            in   a,(c)
            bit  7,a
            ld   a,253
            jp   z,result
            ld   hl,(SV_DFILE)  ; primera fila de la pantalla: la prueba
            inc  hl             ; en curso
            ld   (scr),hl
            ld   a,170          ; Superfast texto (la prueba 6 y la 7 usan
            ld   (2045),a       ; las interrupciones simuladas)

; --- 1. trampa ---
            ld   a,1
            call show
            call init_log
            ld   bc,DBG
            ld   a,10h
            out  (c),a
t1_x:       nop
            ld   hl,exp1
            call check
            ld   a,1
            jp   nz,result

; --- 2. paso a paso ---
            ld   a,2
            call show
            call init_log
            ld   a,4
            ld   (MB+2),a
            ld   bc,DBG
            ld   a,10h
            out  (c),a
t2_a:       nop
t2_b:       ld   a,5
t2_c:       rlc  b
t2_d:       neg
t2_e:       ld   ix,0
            nop
            ld   hl,exp2
            call check
            ld   a,2
            jp   nz,result

; --- 3. comparador de ejecucion ---
            ld   a,3
            call show
            call init_log
            xor  a
            ld   (t3_cnt),a
            ld   hl,t3_target
            ld   a,1                    ; ejecucion
            call set_cmp
            call t3_target
            call t3_target
            xor  a
            call set_mode
            ld   hl,exp3
            call check
            ld   a,3
            jp   nz,result
            ld   a,(t3_cnt)
            cp   2
            ld   a,3
            jp   nz,result

; --- 4. vigilancia de escritura ---
            ld   a,4
            call show
            call init_log
            ld   hl,t4_var
            ld   a,3                    ; escritura
            call set_cmp
            ld   a,55h
            ld   (t4_var),a
t4_n:       nop
            xor  a
            call set_mode
            ld   hl,exp4
            call check
            ld   a,4
            jp   nz,result

; --- 5. breakpoint por software ---
            ld   a,5
            call show
            call init_log
            ld   hl,t5_bp
            ld   (MB+3),hl
            ld   a,(hl)
            ld   (MB+5),a
            ld   (hl),0FFh
            xor  a
            call t5_rt
            cp   42h                    ; se ha ejecutado la instruccion de verdad
            ld   a,5
            jp   nz,result
            ld   hl,exp5
            call check
            ld   a,5
            jp   nz,result

; --- 6. anidado: breakpoint dentro de la rutina de interrupcion ---
            ld   a,6
            call show
            call init_log
            ld   hl,isr
            ld   (2038),hl
            ld   hl,0
            ld   (ints),hl
            ld   hl,isr_bp
            ld   (MB+3),hl
            ld   a,(hl)
            ld   (MB+5),a
            ld   (hl),0FFh
            ld   a,1
            ld   (2040),a
            halt
            halt
            xor  a
            ld   (2040),a
            ld   hl,exp6
            call check
            ld   a,6
            jp   nz,result
            ld   hl,(ints)              ; dos interrupciones completas
            ld   a,h
            or   a
            jr   nz,t6_bad
            ld   a,l
            cp   2
            jr   z,t7
t6_bad:     ld   a,6
            jp   result

; --- 7. el paso se queda en su nivel ---
t7:         ld   a,7
            call show
            call init_log
            ld   hl,0
            ld   (ints),hl
            ld   a,1
            ld   (2040),a
            ld   a,NSTEP7
            ld   (MB+2),a
            ld   bc,DBG
            ld   a,10h
            out  (c),a
t7_a:       ld   b,0
t7_l:       djnz t7_l
            xor  a
            ld   (2040),a
            ld   hl,(ints)
            ld   (r_ints),hl
            ld   a,(MB+6)
            ld   (r_count),a
            cp   NSTEP7+1
            jr   nz,t7_bad
            ld   a,h                    ; alguna interrupcion por medio
            or   l
            jr   z,t7_bad
            ld   hl,LOG                 ; todas en t7_a o t7_l, en el nivel
            ld   b,NSTEP7+1             ; del programa
t7_1:       ld   e,(hl)
            inc  hl
            ld   d,(hl)
            inc  hl
            ld   a,(hl)
            inc  hl
            and  20h
            jr   nz,t7_bad
            push hl
            ld   hl,t7_a
            or   a
            sbc  hl,de
            jr   z,t7_2
            ld   hl,t7_l
            or   a
            sbc  hl,de
t7_2:       pop  hl
            jr   nz,t7_bad
            djnz t7_1
            jr   t8
t7_bad:     ld   a,7
            jr   result

; --- 8. proteccion de la pagina 63 ---
t8:         ld   a,8
            call show
            ld   bc,05E7h               ; pagina del bloque 5
            in   a,(c)
            and  3Fh
            ld   (t8_save),a
            ld   a,33                   ; paginacion completa? (en la simple
            call map5                   ; el mapper solo ve D7-D3: pagina 1)
            ld   bc,05E7h
            in   a,(c)
            and  3Fh
            cp   33
            jr   nz,t8_end              ; no: se salta
            ld   a,1
            ld   (r_p63),a
            ld   a,63
            call map5
            ld   a,(0A000h)             ; el JP del monitor
            cp   0C3h
            jr   nz,t8_bad
            xor  a
            ld   (0A000h),a             ; tiene que no llegar a la SRAM
            ld   a,(0A000h)
            cp   0C3h
            jr   nz,t8_bad
t8_end:     ld   a,(t8_save)
            call map5
            xor  a                      ; todo cuadra
            jr   result
t8_bad:     ld   a,(t8_save)
            call map5
            ld   a,8

result:     ld   (r_test),a
            ld   hl,0                   ; buzon ya no valido
            ld   (MB+7),hl
            xor  a
            ld   (2040),a               ; interrupciones fuera
            ld   a,85
            ld   (2045),a               ; video nativo
            pop  ix
            call SLOW_FAST
            ld   a,(r_test)
            ld   c,a
            ld   b,0
            ret

; ---------------------------------------------------------------------

; A = numero de la prueba en curso -> primera columna de la pantalla
show:       add  a,1Ch                  ; '0' + A
            ld   hl,(scr)
            ld   (hl),a
            ret

; A = pagina -> bloque 5. Vale para los dos modos del mapper: en el simple
; la pagina va en D7-D3 del dato, en el completo en B (A13-A8)
map5:       ld   b,a
            and  1Fh
            rlca
            rlca
            rlca
            or   5
            ld   c,0E7h
            out  (c),a
            ret

; vacia el registro y el buzon, y lo marca como valido
init_log:   ld   hl,LOG
            ld   (MB),hl
            xor  a
            ld   (MB+2),a
            ld   (MB+6),a
            ld   hl,0
            ld   (MB+3),hl
            ld   hl,'T'*256+'D'
            ld   (MB+7),hl
            ret

; HL = direccion, A = modo del comparador
set_cmp:    push af
            ld   bc,DBG
            ld   a,80h
            out  (c),a
            out  (c),l
            ld   a,81h
            out  (c),a
            out  (c),h
            pop  af
set_mode:   ld   bc,DBG
            ld   d,a
            ld   a,82h
            out  (c),a
            out  (c),d
            ret

; HL = tabla esperada: numero de rupturas y, por cada una, direccion,
; mascara y valor del estado. Z si el registro cuadra.
check:      ld   a,(MB+6)
            ld   (r_count),a
            cp   (hl)
            ret  nz
            or   a
            ret  z
            ld   b,a
            inc  hl
            ld   de,LOG
chk1:       ld   a,(de)
            cp   (hl)
            ret  nz
            inc  de
            inc  hl
            ld   a,(de)
            cp   (hl)
            ret  nz
            inc  de
            inc  hl
            ld   a,(de)
            and  (hl)
            inc  hl
            cp   (hl)
            ret  nz
            inc  hl
            inc  de
            djnz chk1
            xor  a
            ret

t3_target:  ld   hl,t3_cnt
            inc  (hl)
            ret

t5_rt:      nop
t5_bp:      ld   a,42h
            ret

; rutina de interrupcion: cuenta
isr:        push af
            push hl
isr_bp:     ld   hl,(ints)
            inc  hl
            ld   (ints),hl
            pop  hl
            pop  af
            ret

; Rupturas esperadas: estado AND 27h (nivel y motivo) = valor
; motivos: 3 trampa, 4 paso, 5 comparador, 6 vigilancia, 7 FF de memoria
exp1:       defb 1
            defw t1_x
            defb 27h,03h
exp2:       defb 5
            defw t2_a
            defb 27h,03h
            defw t2_b
            defb 27h,04h
            defw t2_c
            defb 27h,04h
            defw t2_d
            defb 27h,04h
            defw t2_e
            defb 27h,04h
exp3:       defb 2
            defw t3_target
            defb 27h,05h
            defw t3_target
            defb 27h,05h
exp4:       defb 1
            defw t4_n
            defb 27h,06h
exp5:       defb 1
            defw t5_bp
            defb 27h,07h
exp6:       defb 1
            defw isr_bp
            defb 27h,27h                ; nivel = rutina de interrupcion

t3_cnt:     defb 0
t4_var:     defb 0
ints:       defw 0
scr:        defw 0
t8_save:    defb 0

            end
