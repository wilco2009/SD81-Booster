; Programa de prueba para el PC (test_z80nq.cpp): net.inc (la sala) y netq.inc (mensajes sin esperas) con una tabla
; de saltos. Buzon en $7F00:  MB+0 longitud de lo que se manda, MB+16 sus datos; MB+2 resultado (2 bytes).

NB_BASE     equ 7000h
NQ_BASE     equ 7500h
MB          equ 7F00h

            org  24576
            jp   e_init             ; 24576
            jp   e_new              ; 24579
            jp   e_join             ; 24582
            jp   e_open             ; 24585
            jp   e_send             ; 24588
            jp   e_pump             ; 24591
            jp   e_get              ; 24594
            jp   e_leave            ; 24597

e_init:     ld   hl,net_init
            jr   run
e_new:      ld   hl,room_new
            jr   run
e_join:     ld   hl,room_join
            jr   run
e_open:     ld   hl,nq_open
            jr   run
e_leave:    ld   hl,room_leave
run:        call jp_hl
            ld   c,a
            ld   b,0
            ret
jp_hl:      jp   (hl)

e_send:     ld   a,(MB)
            ld   hl,MB+16
            call nq_send
            ld   a,0
            adc  a,0                ; CF -> A
            ld   c,a
            ld   b,0
            ret
e_pump:     jp   nq_pump
e_get:      call nq_get
            ld   c,a
            ld   b,0
            ret

            include "../../EXAMPLES/NETCHAT/net.inc"
            include "../../EXAMPLES/NETCHAT/netq.inc"
