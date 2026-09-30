; =====================================================================
;  M1TEST.ASM - prueba del detector de limites de instruccion de la FPGA
;  (paso 1 de las interrupciones simuladas; m1_tracker en sim_int.v)
;
;  El detector cuenta las M1, las que empiezan instruccion y las
;  DD CB / FD CB, y se lee por el puerto $3FEF:
;      OUT 80h  borra los contadores
;      OUT 40h  los congela (lo que se lee es esa copia)
;      OUT n    elige que devuelve IN: 0/1 M1, 2/3 instrucciones,
;               4/5 DD/FD CB, 6 ultimo opcode, 7 estado, 15 firma 51h
;
;  Se mide dos veces con el mismo camino de codigo: con un cuerpo vacio
;  y con un cuerpo con todas las familias de instrucciones (CB, ED,
;  LDIR, DD/FD, DD CB, FD CB, prefijos repetidos, DD ED). La diferencia
;  tiene que ser exactamente lo que ejecuta el cuerpo: 35 M1, 18
;  instrucciones y 2 DD/FD CB.
;
;  Todo en FAST y con DI: en SLOW la rutina de video (NMI) se colaria
;  en la cuenta.
;
;  USR devuelve 0 si cuadra, 1 si no y 2 si la FPGA no tiene detector.
;  Los resultados quedan en 24579 (tabla res_*), para el stub BASIC.
;
;  Ensamblar: pasmo m1test.asm m1test.bin
; =====================================================================

SET_FAST    equ 02E7h
SLOW_FAST   equ 0207h
DBG         equ 3FEFh           ; puerto del detector

EXP_M1      equ 35              ; lo que tiene que dar el cuerpo
EXP_INSN    equ 18
EXP_IDXCB   equ 2

            org  24576

            jp   start
res_m1:     defw 0              ; 24579: M1 del cuerpo
res_insn:   defw 0              ; 24581: instrucciones del cuerpo
res_idxcb:  defw 0              ; 24583: DD/FD CB del cuerpo
res_last:   defb 0              ; 24585: ultimo opcode visto
res_state:  defb 0              ; 24586: estado del detector al final

start:      call SET_FAST
            di
            push ix             ; el cuerpo cambia IX (la ROM lo usa en SLOW)

            ld   bc,DBG         ; hay detector?
            ld   a,15
            out  (c),a
            in   a,(c)
            cp   51h
            ld   hl,2
            jr   nz,done

            ld   hl,empty       ; cuerpo vacio
            ld   de,cnt0
            call measure
            ld   hl,body        ; cuerpo de prueba
            ld   de,cnt1
            call measure

            ld   hl,(cnt1)      ; diferencias
            ld   de,(cnt0)
            or   a
            sbc  hl,de
            ld   (res_m1),hl
            ld   hl,(cnt1+2)
            ld   de,(cnt0+2)
            or   a
            sbc  hl,de
            ld   (res_insn),hl
            ld   hl,(cnt1+4)
            ld   de,(cnt0+4)
            or   a
            sbc  hl,de
            ld   (res_idxcb),hl
            ld   a,(cnt1+6)
            ld   (res_last),a
            ld   a,(cnt1+7)
            ld   (res_state),a

            ld   hl,1           ; se compara con lo esperado
            ld   de,(res_m1)
            ld   a,e
            cp   EXP_M1
            jr   nz,done
            ld   a,d
            or   a
            jr   nz,done
            ld   de,(res_insn)
            ld   a,e
            cp   EXP_INSN
            jr   nz,done
            ld   a,d
            or   a
            jr   nz,done
            ld   de,(res_idxcb)
            ld   a,e
            cp   EXP_IDXCB
            jr   nz,done
            ld   a,d
            or   a
            jr   nz,done
            ld   hl,0

done:       pop  ix
            push hl
            call SLOW_FAST
            pop  bc             ; USR devuelve BC
            ret

; HL = cuerpo, DE = donde dejar los 8 bytes leidos (M1, instrucciones,
; DD/FD CB, ultimo opcode, estado). Mismo camino para los dos cuerpos,
; asi que lo que no es el cuerpo se anula al restar.
measure:    ld   (m_call+1),hl
            push de
            ld   bc,DBG
            ld   a,80h
            out  (c),a          ; borrar
m_call:     call 0              ; el cuerpo
            ld   bc,DBG         ; (el cuerpo cambia BC)
            ld   a,40h
            out  (c),a          ; congelar
            pop  hl
            ld   d,0            ; indices 0-7
m_rd:       ld   a,d
            out  (c),a
            in   a,(c)
            ld   (hl),a
            inc  hl
            inc  d
            ld   a,d
            cp   8
            jr   nz,m_rd
            ret

empty:      ret

; 35 M1, 18 instrucciones, 2 DD/FD CB (sin contar el RET, que tambien
; tiene el cuerpo vacio)
body:       nop                         ;  1 M1  1 instr
            ld   a,5                    ;  1     1
            rlc  b                      ;  2     1   CB 00
            neg                         ;  2     1   ED 44
            ld   hl,src                 ;  1     1
            ld   de,dst                 ;  1     1
            ld   bc,4                   ;  1     1
            ldir                        ;  8     4   ED B0, una vez por byte
            ld   ix,buf                 ;  2     1   DD 21
            ld   a,(ix+1)               ;  2     1   DD 7E d
            rlc  (ix+2)                 ;  2     1   DD CB d 06: d y 06 no son M1
            bit  0,(iy+1)               ;  2     1   FD CB d 46
            defb 0DDh,0DDh,0DDh,07Eh,000h ; 4    1   DD DD LD A,(IX+0)
            defb 0FDh,0DDh,021h         ;  3     1   FD DD LD IX,buf
            defw buf
            defb 0DDh,0EDh,044h         ;  3     1   DD NEG
            ret

src:        defb 1,2,3,4
dst:        defs 4
buf:        defs 4
cnt0:       defs 8
cnt1:       defs 8

            end
