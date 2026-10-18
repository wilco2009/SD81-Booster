; Programa de prueba para el PC (test_z80scrabble.cpp): el motor del Scrabble (scrabble.inc) y el diccionario
; (dict.inc) con una tabla de saltos. No es un juego. Las variables estan en $9600 (SB_BASE) y el diccionario en $9400.

DataPort    equ 0A7h
ClkPort     equ 0AFh
DC_BASE     equ 9400h
SB_BASE     equ 9600h

            org  24576
            jp   t_init             ; 24576
            jp   t_check            ; 24579
            jp   t_apply            ; 24582
            jp   t_pass             ; 24585
            jp   t_exch             ; 24588
            jp   t_dopen            ; 24591
            jp   t_rackval          ; 24594

t_init:     call sb_init
            ld   bc,0
            ret
t_check:    call sb_check
            ld   c,a
            ld   b,0
            ret
t_apply:    call sb_apply
            ld   bc,0
            ret
t_pass:     call sb_pass
            ld   bc,0
            ret
t_exch:     call sb_exchange
            ld   c,a
            ld   b,0
            ret
t_dopen:    call sb_open_dict
            ld   c,a
            ld   b,0
            ret
t_rackval:  call sb_rackval         ; HL = el atril
            ld   b,h
            ld   c,l
            ret

sd_clk:     in   a,(ClkPort)
            ld   c,a
            ret

mcu_send:   out  (DataPort),a
            ld   a,c
            cpl
            ld   c,a
ms_w:       in   a,(ClkPort)
            xor  c
            jp   m,ms_w
            ret

mcu_recv:   in   a,(DataPort)
            push af
            ld   a,c
            cpl
            ld   c,a
mr_w:       in   a,(ClkPort)
            xor  c
            jp   m,mr_w
            pop  af
            ret

            include "../../EXAMPLES/NETGAME/dict.inc"
            include "../../EXAMPLES/NETGAME/scrabble.inc"
