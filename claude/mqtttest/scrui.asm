; Programa de prueba para el PC (test_z80scrabbleui.cpp): el motor (scrabble.inc), el diccionario (dict.inc), gfx.inc y la
; pantalla del Scrabble (scrabble_ui.inc) con una tabla de saltos. No es un juego. El texto a escribir en las filas
; de estado va en $9F00 (ASCII, 0) y el jugador de t_note en $9FF0.

DataPort    equ 0A7h
ClkPort     equ 0AFh
DC_BASE     equ 9400h
SB_BASE     equ 9600h
SU_BASE     equ 9900h
UI_BASE     equ 9A00h
SI_BASE     equ 9B00h
GFX_SQUARES equ 0
ROW_TITLE   equ 0               ; (los pone lobby.inc en el juego)
ROW_HELP    equ 22

            org  24576
            jp   t_draw             ; 24576
            jp   t_init             ; 24579
            jp   t_check            ; 24582
            jp   t_apply            ; 24585
            jp   t_note             ; 24588
            jp   t_board            ; 24591
            jp   t_rack             ; 24594
            jp   t_panel            ; 24597
            jp   t_msg              ; 24600
            jp   t_status           ; 24603
            jp   t_dopen            ; 24606
            jp   t_help             ; 24609
            jp   t_input            ; 24612
            jp   t_key              ; 24615
            jp   t_iclear           ; 24618
            jp   t_ibuild           ; 24621
            jp   t_idraw            ; 24624
            jp   t_errmsg           ; 24627
            jp   t_sprinit          ; 24630
            jp   t_sproff           ; 24633
            jp   t_blink            ; 24636

t_draw:     call sbu_draw_all
            ld   bc,0
            ret
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
t_note:     ld   a,(9FF0h)
            call sbu_note_last
            ld   bc,0
            ret
t_board:    call sbu_board
            ld   bc,0
            ret
t_rack:     call sbu_rack
            ld   bc,0
            ret
t_panel:    call sbu_panel
            ld   bc,0
            ret
t_msg:      ld   hl,9F00h
            call sbu_msg
            ld   bc,0
            ret
t_status:   ld   hl,9F00h
            call sbu_status
            ld   bc,0
            ret
t_help:     ld   hl,9F00h
            call sbu_help
            ld   bc,0
            ret
t_input:    ld   hl,9F00h
            call sbu_input
            ld   bc,0
            ret
t_key:      ld   a,(9FF1h)
            call sbi_key
            ld   c,a
            ld   b,0
            ret
t_iclear:   call sbi_clear
            ld   bc,0
            ret
t_ibuild:   call sbi_build
            ld   c,a
            ld   b,0
            ret
t_idraw:    call sbi_draw
            ld   bc,0
            ret
t_errmsg:   ld   a,(9FF1h)
            call sbi_errmsg
            ld   b,h
            ld   c,l
            ret
t_sprinit:  call sbi_spr_init
            ld   bc,0
            ret
t_sproff:   call sbi_spr_off
            ld   bc,0
            ret
t_blink:    call sbi_blink
            ld   bc,0
            ret
t_dopen:    call sb_open_dict
            ld   c,a
            ld   b,0
            ret

txt_on:     ret

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

; A = ASCII -> A = codigo de caracter del ZX81. Los codigos 1-31 pasan tal cual
; (graficos de bloque) y los de 128 en adelante tambien (inverso)
a2z:        cp   32
            ret  c
            cp   128
            ret  nc
            cp   ' '
            jr   nz,az1
            xor  a
            ret
az1:        cp   '0'
            jr   c,az_p
            cp   '9'+1
            jr   nc,az2
            sub  '0'-28
            ret
az2:        cp   'A'
            jr   c,az_p
            cp   'Z'+1
            jr   nc,az_p
            sub  'A'-38
            ret
az_p:       ld   c,a
            ld   hl,az_tab
az_l:       ld   a,(hl)
            or   a
            jr   z,az_none
            cp   c
            jr   z,az_f
            inc  hl
            inc  hl
            jr   az_l
az_f:       inc  hl
            ld   a,(hl)
            ret
az_none:    xor  a
            ret
az_tab:     defb '.',27, ',',26, ':',14, '-',22, '+',21, '=',20, '<',19, '>',18
            defb '(',16, ')',17, '?',15, '"',11, '*',23, '/',24, ';',25, '$',13
            defb 0

            include "../../EXAMPLES/NETGAME/dict.inc"
            include "../../EXAMPLES/NETGAME/scrabble.inc"
            include "../../EXAMPLES/NETGAME/gfx.inc"
            include "../../EXAMPLES/NETGAME/scrabble_ui.inc"
            include "../../EXAMPLES/NETGAME/scrabble_in.inc"
