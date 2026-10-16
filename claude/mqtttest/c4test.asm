; Programa de prueba para el PC (test_z80game.cpp): la libreria de red (net.inc), el controlador
; (game.inc) y las reglas del 4 en raya (connect4.inc), con una tabla de saltos y un buzon en
; $7F00 para darles los parametros y recoger los resultados. No es un juego: el juego es
; EXAMPLES/NETGAME/connect4.asm.
;   buzon: MB+0 A de entrada, MB+1 B de entrada, MB+2 resultado (2 bytes), MB+16 estado (48 bytes)

NB_BASE     equ 7000h
GM_BASE     equ 7400h
C4_BASE     equ 7440h
G_STATE_LEN equ 42
G_MOVE_LEN  equ 1
G_CLOCK     equ 0
MB          equ 7F00h

            org  24576
            jp   e_init             ; 24576
            jp   e_new              ; 24579
            jp   e_join             ; 24582
            jp   e_send             ; 24585
            jp   e_poll             ; 24588
            jp   e_leave            ; 24591
            jp   e_psend            ; 24594
            jp   e_precv            ; 24597
            jp   t_reset            ; 24600
            jp   t_valid            ; 24603
            jp   t_apply            ; 24606
            jp   t_winner           ; 24609
            jp   t_save             ; 24612
            jp   t_load             ; 24615
            jp   t_hash             ; 24618
            jp   t_gstart           ; 24621
            jp   t_gmove            ; 24624
            jp   t_gpoll            ; 24627
            jp   t_gnew             ; 24630
            jp   t_gresend          ; 24633
            jp   t_grematch         ; 24636

e_init:     ld   hl,net_init
            jr   run
e_new:      ld   hl,room_new
            jr   run
e_join:     ld   hl,room_join
            jr   run
e_send:     ld   hl,room_send
            jr   run
e_poll:     ld   hl,room_poll
            jr   run
e_leave:    ld   hl,room_leave
            jr   run
e_psend:    ld   hl,room_poll_send
            jr   run
e_precv:    ld   hl,room_poll_recv
run:        call jp_hl
            ld   c,a
            ld   b,0
            ret
jp_hl:      jp   (hl)

t_reset:    jp   g_reset
t_valid:    ld   a,(MB+1)
            ld   b,a
            ld   hl,MB
            call g_valid
            ld   a,0
            adc  a,0                ; CF -> A
            ld   (MB+2),a
            ret
t_apply:    ld   a,(MB+1)
            ld   b,a
            ld   hl,MB
            jp   g_apply
t_winner:   call g_winner
            ld   (MB+2),a
            ret
t_save:     ld   hl,MB+16
            jp   g_save
t_load:     ld   hl,MB+16
            jp   g_load
t_hash:     call game_hash
            ld   (MB+2),hl
            ret
t_gstart:   jp   game_start
t_gmove:    ld   a,(MB)
            call game_move
            ld   (MB+2),a
            ret
t_gpoll:    call game_poll
            ld   (MB+2),a
            ret
t_gnew:     jp   game_new
t_gresend:  call game_resend
            ld   (MB+2),a
            ret
t_grematch: call game_rematch
            ld   (MB+2),a
            ret

            include "../../EXAMPLES/NETCHAT/net.inc"
            include "../../EXAMPLES/NETGAME/game.inc"
            include "../../EXAMPLES/NETGAME/connect4.inc"
