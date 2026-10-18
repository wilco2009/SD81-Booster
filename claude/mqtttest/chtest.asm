; Programa de prueba para el PC (test_z80chess.cpp): el motor de ajedrez (chess.inc) con una tabla de
; saltos y un buzon en $8F00. No es un juego: el juego es EXAMPLES/NETGAME/chess.asm.
;   buzon: MB+0 jugada (2 bytes) o color, MB+2 jugador, MB+3 resultado, MB+4 cuantas jugadas, MB+16 estado (67 bytes)

NB_BASE     equ 8000h
GM_BASE     equ 8400h
CH_BASE     equ 8500h
G_STATE_LEN equ 67
G_MOVE_LEN  equ 2
G_CLOCK     equ 0
G_MOVE_VAR  equ 0               ; (game.inc) jugadas de longitud fija
G_BIG       equ 0               ; estado que cabe en un mensaje
MB          equ 8F00h

            org  24576
            jp   t_reset            ; 24576
            jp   t_valid            ; 24579
            jp   t_apply            ; 24582
            jp   t_winner           ; 24585
            jp   t_save             ; 24588
            jp   t_load             ; 24591
            jp   t_gen              ; 24594

t_reset:    jp   g_reset
t_valid:    ld   a,(MB+2)
            ld   b,a
            ld   hl,MB
            call g_valid
            ld   a,0
            adc  a,0                ; CF -> A
            ld   (MB+3),a
            ret
t_apply:    ld   a,(MB+2)
            ld   b,a
            ld   hl,MB
            jp   g_apply
t_winner:   call g_winner
            ld   (MB+3),a
            ret
t_save:     ld   hl,MB+16
            jp   g_save
t_load:     ld   hl,MB+16
            jp   g_load
t_gen:      ld   a,(MB)             ; color: 0 blancas / 8 negras
            call ch_gen_legal
            ld   a,(CH_N)
            ld   (MB+4),a
            ret

            include "../../EXAMPLES/NETCHAT/net.inc"
            include "../../EXAMPLES/NETGAME/game.inc"
            include "../../EXAMPLES/NETGAME/chess.inc"
