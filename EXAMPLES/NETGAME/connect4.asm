; =====================================================================
;  CONNECT4.ASM - 4 en raya en red para dos ZX81 con SD81 Booster y modulo
;  WiFi, sin servidor propio (MQTT): capa C del juego en red
;  (claude/planning/juego_en_red.md). Usa net.inc (la sala y los mensajes
;  firmados), game.inc (el controlador de partida) y connect4.inc (las reglas).
;
;  Pantalla: Superfast texto de 32 columnas con la pantalla alternativa en
;  $8000 (como TELNET): el video lo da la FPGA, sin parpadeo. Letras del
;  ZX81, sin color: los graficos buenos se ponen aparte.
;
;  Como se juega:
;      1 = partida nueva: sale un codigo de 10 caracteres; se lo dices al otro.
;      2 = unirse: tecleas el codigo (DEL = SHIFT+0) y ENTER.
;      1-7 = la columna donde cae la ficha. Q = salir. R = reenviar si fallo la red.
;      ENTER = otra partida, cuando ha acabado.
;
;  Memoria:   $6000-$77FF el programa;  $7800-$7CFF variables;  $8000-$8318 pantalla.
;  Cargar:    CONNECT4.B81 (CLEAR 24575, LOAD FAST ... CODE 24576, RAND USR 24576).
;  Ensamblar: pasmo connect4.asm connect4.bin connect4.sym   (compila.bat)
; =====================================================================

NB_BASE     equ 7800h
GM_BASE     equ 7C00h
C4_BASE     equ 7C40h
UI_BASE     equ 7C80h
G_STATE_LEN equ 42
G_MOVE_LEN  equ 1
G_CLOCK     equ 0               ; sin relojes
DFILE       equ 8000h           ; pantalla: 1 + 24 filas de 33


MSG         equ UI_BASE+5       ; el mensaje de la fila ROW_MSG (0 = ninguno)
UI_Y        equ UI_BASE+6

; filas y columnas de la pantalla
ROW_ROOM    equ 2
ROW_YOU     equ 3
ROW_BOARD   equ 6               ; la fila de arriba del tablero
ROW_COLS    equ 13
ROW_STATUS  equ 15
ROW_MSG     equ 17
BOARD_COL   equ 4               ; la barra izquierda del tablero

            org  24576

            include "lobby.inc"

; ---------------------------------------------------------------------
;  La partida
; ---------------------------------------------------------------------
play:       xor  a
            ld   (POLLC),a
            ld   (MSG),a
            call cls                ; lo fijo se pinta una vez; draw_all solo repinta lo que cambia
            ld   hl,t_title
            ld   bc,ROW_TITLE*256+7
            call print_at
            ld   hl,t_room
            ld   bc,ROW_ROOM*256+5
            call print_at
            ld   bc,ROW_ROOM*256+11
            call scr_addr
            ex   de,hl
            ld   hl,NB_CODE81
            ld   bc,10
            ldir
            ld   hl,t_you
            ld   bc,ROW_YOU*256+5
            call print_at
            ld   hl,t_cols
            ld   bc,ROW_COLS*256+BOARD_COL
            call print_at
            ld   hl,t_line
            ld   bc,256*(ROW_BOARD+6)+BOARD_COL
            call print_at
            call draw_all
pl_loop:    call kbd_scan
            or   a
            jr   z,pl_poll
            push af
            call key_release        ; una pulsacion = una jugada (kbd_scan destruye BC)
            pop  af
            cp   'Q'
            jp   z,pl_quit
            cp   13
            jr   z,pl_again
            cp   'R'
            jr   z,pl_resend
            cp   '1'
            jr   c,pl_poll
            cp   '7'+1
            jr   nc,pl_poll
            sub  '1'                ; columna 0-6
            call game_move
            ld   hl,MSG
            or   a
            jr   z,pl_done
            ld   b,3                ; mensaje 3: columna llena (1 = no legal)
            cp   1
            jr   z,pl_msg
            ld   b,4                ; 4: no te toca
            cp   2
            jr   z,pl_msg
            ld   b,5                ; 5: ha acabado
            cp   3
            jr   z,pl_msg
            ld   b,6                ; 6: error de red
pl_msg:     ld   (hl),b
            call draw_all
            jr   pl_poll
pl_done:    ld   (hl),0
            call draw_all
            jr   pl_poll
pl_again:   ld   a,(GM_OVER)
            or   a
            jr   z,pl_poll
            call game_rematch
            ld   a,0
            ld   (MSG),a
            call draw_all
            jr   pl_poll
pl_resend:  call game_resend
            or   a
            ld   b,0
            jr   z,pl_rs
            ld   b,6
pl_rs:      ld   a,b
            ld   (MSG),a
            call draw_all
pl_poll:    call poll_step_game
            call delay20
            jr   pl_loop
pl_quit:    call room_leave
            jp   menu

; un paso del sondeo en la partida: cada 20 vueltas pregunta y 2 vueltas despues recoge
poll_step_game: call poll_step
            or   a
            ret  z
            cp   1                  ; el rival ha jugado
            ld   b,0
            jr   z,psg_set
            cp   2
            ld   b,7                ; el rival ha entrado
            jr   z,psg_set
            cp   3
            ld   b,8                ; se ha ido
            jr   z,psg_set
            cp   4
            ld   b,9                ; sincronizado
            jr   z,psg_set
            cp   5
            ld   b,10               ; desincronizado: pidiendo el estado
            jr   z,psg_set
            cp   8
            ld   b,0                ; otra partida
            jr   z,psg_set
            ld   b,11               ; error del modem
psg_set:    ld   a,b
            ld   (MSG),a
            jp   draw_all

; ---------------------------------------------------------------------
;  Dibujo
; ---------------------------------------------------------------------
draw_all:   ld   bc,ROW_YOU*256+11     ; mi ficha
            call scr_addr
            ex   de,hl
            ld   a,(GM_ME)
            call glyph3
            ld   hl,t_rival
            ld   a,(NB_PEER)
            or   a
            jr   nz,da_r
            ld   hl,t_rival0
da_r:       ld   bc,ROW_YOU*256+16
            call print_row
            call draw_board
            ld   a,(GM_OVER)
            or   a
            jr   z,da_play
            cp   3
            ld   hl,t_draw
            jr   z,da_st
            ld   hl,GM_ME
            cp   (hl)
            ld   hl,t_win
            jr   z,da_st
            ld   hl,t_lose
            jr   da_st
da_play:    call game_turn_of
            ld   hl,GM_ME
            cp   (hl)
            ld   hl,t_mine
            jr   z,da_st
            ld   hl,t_theirs
da_st:      ld   bc,ROW_STATUS*256+4
            call print_row
            ld   a,(MSG)
            ld   hl,t_empty
            or   a
            jr   z,da_msg
            dec  a
            add  a,a
            ld   e,a
            ld   d,0
            ld   hl,msg_tab
            add  hl,de
            ld   e,(hl)
            inc  hl
            ld   d,(hl)
            ex   de,hl
da_msg:     ld   bc,ROW_MSG*256+4
            call print_row
            ld   a,(GM_OVER)
            or   a
            ld   hl,t_help
            jr   z,da_h2
            ld   hl,t_help_over
da_h2:      ld   bc,ROW_HELP*256+1
            jp   print_row

; el tablero: la fila de arriba es la fila 5 del juego
draw_board: xor  a
            ld   (UI_Y),a
db_row:     ld   a,(UI_Y)
            add  a,ROW_BOARD
            ld   b,a
            ld   c,BOARD_COL
            call scr_addr
            ex   de,hl              ; DE = la pantalla
            ld   a,t_bar_code
            ld   (de),a
            inc  de
            ld   a,5
            ld   hl,UI_Y
            sub  (hl)
            ld   c,a                ; C = fila del juego
            ld   b,0                ; B = columna
db_col:     push bc
            push de
            ld   a,c
            add  a,a
            add  a,a
            add  a,a
            sub  c
            add  a,b                ; casilla = columna + 7 * fila
            ld   e,a
            ld   d,0
            ld   hl,G_BOARD
            add  hl,de
            ld   a,(hl)
            pop  de
            call glyph3
            pop  bc
            inc  b
            ld   a,b
            cp   7
            jr   c,db_col
            ld   a,t_bar_code
            ld   (de),a
            ld   hl,UI_Y
            inc  (hl)
            ld   a,(hl)
            cp   6
            jr   c,db_row
            ret

; A = 0 vacia, 1 jugador 1, 2 jugador 2: tres caracteres en (DE); DE avanza
glyph3:     or   a
            jr   nz,g3_p
            ld   a,0
            ld   (de),a
            inc  de
            ld   a,27               ; '.'
            ld   (de),a
            inc  de
            xor  a
            ld   (de),a
            inc  de
            ret
g3_p:       cp   1
            ld   a,128              ; el uno: bloque entero (espacio en inverso)
            jr   z,g3_w
            ld   a,8                ; el dos: gris
g3_w:       ld   (de),a
            inc  de
            ld   (de),a
            inc  de
            ld   (de),a
            inc  de
            ret

; ---------------------------------------------------------------------
;  Textos (ASCII; a2z los pasa a caracteres del ZX81)
; ---------------------------------------------------------------------
t_bar_code  equ 14              ; la barra del tablero: el ':' del ZX81 hace de barra vertical
pick_tc:    or   a                  ; el 4 en raya no tiene reloj
            ret
t_sides:    defm "PLAYER 1 MOVES FIRST"
            defb 0
t_title:    defm "NETWORK CONNECT 4"
            defb 0
t_room:     defm "ROOM"
            defb 0
t_you:      defm "YOU"
            defb 0
t_rival:    defm "RIVAL: HERE"
            defb 0
t_rival0:   defm "RIVAL: AWAY"
            defb 0
t_cols:     defm "  1  2  3  4  5  6  7"
            defb 0
t_line:     defm "-----------------------"
            defb 0
t_empty:    defb 0
t_mine:     defm "YOUR TURN: PRESS 1 TO 7"
            defb 0
t_theirs:   defm "THE RIVAL IS THINKING..."
            defb 0
t_win:      defm "YOU WIN"
            defb 0
t_lose:     defm "THE RIVAL WINS"
            defb 0
t_draw:     defm "DRAW"
            defb 0
t_help:     defm "1-7 PLAY  R RESEND  Q QUIT"
            defb 0
t_help_over: defm "ENTER = AGAIN   Q = QUIT"
            defb 0
msg_tab:    defw m_1,m_2,m_3,m_4,m_5,m_6,m_7,m_8,m_9,m_10,m_11
m_1:        defb 0
m_2:        defb 0
m_3:        defm "THAT COLUMN IS FULL"
            defb 0
m_4:        defm "IT IS NOT YOUR TURN"
            defb 0
m_5:        defm "THE GAME IS OVER"
            defb 0
m_6:        defm "NETWORK ERROR: R RESENDS"
            defb 0
m_7:        defm "THE RIVAL HAS ENTERED"
            defb 0
m_8:        defm "THE RIVAL HAS LEFT"
            defb 0
m_9:        defm "BACK IN SYNC"
            defb 0
m_10:       defm "RESYNCING WITH THE RIVAL..."
            defb 0
m_11:       defm "MODEM ERROR"
            defb 0

            include "../NETCHAT/net.inc"
            include "game.inc"
            include "connect4.inc"
