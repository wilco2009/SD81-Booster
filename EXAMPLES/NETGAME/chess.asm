; =====================================================================
;  CHESS.ASM - ajedrez en red para dos ZX81 con SD81 Booster y modulo WiFi,
;  sin servidor propio (MQTT): capa C del juego en red
;  (claude/planning/juego_en_red.md). Usa lobby.inc (menu, sala, teclado y
;  pantalla), game.inc (el controlador de partida), chess.inc (las reglas) y
;  net.inc (la sala y los mensajes firmados).
;
;  Como se juega:
;      1 = partida nueva: sale un codigo de 10 caracteres; se lo dices al otro.
;      2 = unirse: tecleas el codigo (SHIFT+0 borra) y ENTER.
;      Una jugada se teclea con las casillas de origen y destino, por ejemplo E2 E4:
;      cuatro teclas (letra, cifra, letra, cifra) y ENTER. Al coronar, una quinta
;      letra elige la pieza (Q dama, R torre, B alfil, N caballo; sin ella, dama).
;      SHIFT+0 borra la ultima tecla. 9 = reenviar si fallo la red. 0 = salir.
;      ENTER = otra partida, cuando ha acabado y no hay nada tecleado.
;  Las blancas (quien crea la sala) empiezan y juegan abajo; las negras ven el
;  tablero girado.  Las piezas blancas se ven como letras, las negras en inverso:
;  P peon, N caballo, B alfil, R torre, Q dama, K rey.
;
;  Reglas: todo el ajedrez salvo la triple repeticion (enroque, al paso, coronacion,
;  jaque, mate, ahogado, material insuficiente y las 50 jugadas).
;
;  Memoria:   $6000-$87FF el programa;  $8800-$8B18 pantalla;  $9000-$9A00 variables
;  Cargar:    CHESS.B81 (CLEAR 24575, LOAD FAST ... CODE 24576, RAND USR 24576).
;  Ensamblar: pasmo chess.asm chess.bin chess.sym   (compila.bat)
; =====================================================================

NB_BASE     equ 9000h
GM_BASE     equ 9400h
UI_BASE     equ 9480h
CH_BASE     equ 9500h           ; alineado a 256
G_STATE_LEN equ 67
G_MOVE_LEN  equ 2
DFILE       equ 8800h           ; pantalla: 1 + 24 filas de 33

MSG         equ UI_BASE+5       ; el mensaje de la fila ROW_MSG (0 = ninguno)
UI_Y        equ UI_BASE+6
UI_X        equ UI_BASE+7
UI_R        equ UI_BASE+8       ; fila y columna de la casilla que se dibuja (0-7)
UI_F        equ UI_BASE+9
MV_N        equ UI_BASE+10      ; teclas de la jugada tecleadas (0-5)
MV_BUF      equ UI_BASE+11      ; (6) lo tecleado, ASCII
MV_OUT      equ UI_BASE+17      ; (2) la jugada en el formato de game.inc
LAST_BUF    equ UI_BASE+20      ; (20) texto de la ultima jugada

ROW_ROOM    equ 1
ROW_BOARD   equ 3               ; la fila de arriba del tablero
ROW_FILES   equ 11
ROW_STATUS  equ 13
ROW_INPUT   equ 15
ROW_MSG     equ 17
ROW_LAST    equ 19
BOARD_COL   equ 4               ; la primera casilla (cada una ocupa 3 columnas)

            org  24576

            include "lobby.inc"

; ---------------------------------------------------------------------
;  La partida
; ---------------------------------------------------------------------
play:       xor  a
            ld   (POLLC),a
            ld   (MSG),a
            ld   (MV_N),a
            call cls                ; lo fijo se pinta una vez; draw_all repinta lo que cambia
            ld   hl,t_title
            ld   bc,ROW_TITLE*256+7
            call print_at
            ld   hl,t_room
            ld   bc,ROW_ROOM*256+1
            call print_at
            ld   bc,ROW_ROOM*256+6
            call scr_addr
            ex   de,hl
            ld   hl,NB_CODE81
            ld   bc,10
            ldir
            ld   hl,t_white
            ld   a,(GM_ME)
            cp   2
            jr   nz,pl_col
            ld   hl,t_black
pl_col:     ld   bc,ROW_ROOM*256+18
            call print_at
            ld   hl,t_files
            ld   a,(GM_ME)
            cp   2
            jr   nz,pl_files
            ld   hl,t_files_b
pl_files:   ld   bc,ROW_FILES*256+BOARD_COL
            call print_at
            call draw_all
pl_loop:    call kbd_scan
            or   a
            jp   z,pl_poll
            push af
            call key_release        ; una pulsacion = una tecla (kbd_scan destruye BC)
            pop  af
            cp   '0'
            jp   z,pl_quit
            cp   '9'
            jp   z,pl_resend
            cp   8
            jr   z,pl_del
            cp   13
            jp   z,pl_enter
            call add_key            ; una tecla de la jugada, si cuadra en esa posicion
            call draw_input
            jp   pl_poll
pl_del:     ld   a,(MV_N)
            or   a
            jp   z,pl_poll
            dec  a
            ld   (MV_N),a
            call draw_input
            jp   pl_poll
pl_enter:   ld   a,(MV_N)
            or   a
            jr   nz,pl_send
            ld   a,(GM_OVER)        ; sin nada tecleado: otra partida si ha acabado
            or   a
            jp   z,pl_poll
            call game_rematch
            xor  a
            ld   (MSG),a
            call draw_all
            jp   pl_poll
pl_send:    cp   4
            jp   c,pl_poll          ; faltan teclas
            call build_move
            ld   hl,MV_OUT
            call game_move_p
            ld   b,0
            or   a
            jr   z,pl_msg
            ld   b,3                ; 3: jugada ilegal (1)
            cp   1
            jr   z,pl_msg
            ld   b,4                ; 4: no te toca
            cp   2
            jr   z,pl_msg
            ld   b,5                ; 5: ha acabado
            cp   3
            jr   z,pl_msg
            ld   b,6                ; 6: error de red
pl_msg:     ld   a,b
            ld   (MSG),a
            xor  a
            ld   (MV_N),a
            call draw_all
            jp   pl_poll
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
            jp   pl_loop
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
;  Teclear la jugada: letra, cifra, letra, cifra (y una letra de corona)
; ---------------------------------------------------------------------
; A = tecla: la guarda en MV_BUF si es la que toca en esa posicion
add_key:    ld   c,a
            ld   a,(MV_N)
            cp   5
            ret  nc                 ; ya esta completa
            ld   b,a
            cp   4
            jr   z,ak_promo
            and  1
            jr   z,ak_file          ; posiciones 0 y 2: letra A-H
            ld   a,c                ; posiciones 1 y 3: cifra 1-8
            cp   '1'
            ret  c
            cp   '9'
            ret  nc
            jr   ak_put
ak_file:    ld   a,c
            cp   'A'
            ret  c
            cp   'I'
            ret  nc
            jr   ak_put
ak_promo:   ld   a,c                ; la quinta: Q R B N
            cp   'Q'
            jr   z,ak_put
            cp   'R'
            jr   z,ak_put
            cp   'B'
            jr   z,ak_put
            cp   'N'
            ret  nz
ak_put:     ld   e,b
            ld   d,0
            ld   hl,MV_BUF
            add  hl,de
            ld   (hl),c
            ld   a,b
            inc  a
            ld   (MV_N),a
            ret

; MV_BUF (E2E4 o E7E8Q) -> MV_OUT: [origen | corona << 6] [destino], casillas 0-63
build_move: ld   a,(MV_BUF+1)
            sub  '1'
            add  a,a
            add  a,a
            add  a,a                ; fila * 8
            ld   b,a
            ld   a,(MV_BUF)
            sub  'A'
            add  a,b
            ld   e,a                ; origen
            ld   a,(MV_BUF+3)
            sub  '1'
            add  a,a
            add  a,a
            add  a,a
            ld   b,a
            ld   a,(MV_BUF+2)
            sub  'A'
            add  a,b
            ld   (MV_OUT+1),a       ; destino
            ld   a,(MV_N)
            ld   c,0                ; sin quinta letra: dama
            cp   5
            jr   nz,bm_set
            ld   a,(MV_BUF+4)
            ld   c,0
            cp   'Q'
            jr   z,bm_set
            ld   c,1
            cp   'R'
            jr   z,bm_set
            ld   c,2
            cp   'B'
            jr   z,bm_set
            ld   c,3
bm_set:     ld   a,c
            rrca
            rrca                    ; corona en los bits 6-7
            or   e
            ld   (MV_OUT),a
            ret

; ---------------------------------------------------------------------
;  Dibujo
; ---------------------------------------------------------------------
draw_all:   call draw_board
            call draw_input
            ; estado: quien mueve, jaque, y el final
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
            jr   z,da_chk
            ld   hl,t_theirs
da_chk:     push hl                 ; "en jaque" si al que le toca lo esta
            ld   a,(GM_PLY)
            and  1
            add  a,a
            add  a,a
            add  a,a
            call ch_in_check
            pop  hl
            jr   nc,da_st
            ld   hl,t_mine_chk
            push hl
            call game_turn_of
            ld   hl,GM_ME
            cp   (hl)
            pop  hl
            jr   z,da_st
            ld   hl,t_theirs_chk
da_st:      ld   bc,ROW_STATUS*256+4
            call print_row
            ; la ultima jugada
            call last_move
            ld   hl,LAST_BUF
            ld   bc,ROW_LAST*256+4
            call print_row
            ; el mensaje
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

; "MOVE: E2E4" en la fila de entrada
draw_input: ld   hl,LAST_BUF        ; se arma ahi: "MOVE " + lo tecleado + " _"
            ld   (hl),'M'
            inc  hl
            ld   (hl),'O'
            inc  hl
            ld   (hl),'V'
            inc  hl
            ld   (hl),'E'
            inc  hl
            ld   (hl),':'
            inc  hl
            ld   (hl),' '
            inc  hl
            ld   de,MV_BUF
            ld   a,(MV_N)
            ld   b,a
            or   a
            jr   z,di_done
di_lp:      ld   a,(de)
            ld   (hl),a
            inc  hl
            inc  de
            djnz di_lp
di_done:    ld   (hl),'-'
            inc  hl
            ld   (hl),0
            ld   hl,LAST_BUF
            ld   bc,ROW_INPUT*256+4
            jp   print_row

; LAST_BUF = "LAST: E2E4", o nada si no ha habido jugadas
last_move:  ld   hl,LAST_BUF
            ld   (hl),0
            ld   de,(GM_PLY)
            ld   a,d
            or   e
            ret  z
            ld   (hl),'L'
            inc  hl
            ld   (hl),'A'
            inc  hl
            ld   (hl),'S'
            inc  hl
            ld   (hl),'T'
            inc  hl
            ld   (hl),':'
            inc  hl
            ld   (hl),' '
            inc  hl
            ld   a,(GM_MOVE)
            and  3Fh
            call sq_text
            ld   a,(GM_MOVE+1)
            and  3Fh
            call sq_text
            ld   (hl),0
            ret

; A = casilla 0-63 -> dos caracteres ("E2") en (HL); HL avanza
sq_text:    ld   b,a
            and  7
            add  a,'A'
            ld   (hl),a
            inc  hl
            ld   a,b
            rrca
            rrca
            rrca
            and  7
            add  a,'1'
            ld   (hl),a
            inc  hl
            ret

; el tablero (con las blancas abajo, o las negras si soy el 2)
draw_board: xor  a
            ld   (UI_Y),a
db_row:     ld   a,(UI_Y)
            ld   e,a
            ld   a,(GM_ME)
            cp   2
            ld   a,e
            jr   z,db_b
            ld   a,7
            sub  e                  ; blancas abajo: rank = 7 - fila de pantalla
db_b:       ld   (UI_R),a
            ld   a,(UI_Y)           ; la cifra de la fila, en la columna 2
            add  a,ROW_BOARD
            ld   b,a
            ld   c,2
            call scr_addr
            ld   a,(UI_R)
            add  a,29               ; '1' del ZX81 = 29
            ld   (hl),a
            ld   a,(UI_Y)
            add  a,ROW_BOARD
            ld   b,a
            ld   c,BOARD_COL
            call scr_addr
            ex   de,hl              ; DE = la pantalla
            xor  a
            ld   (UI_X),a
db_col:     ld   a,(GM_ME)
            cp   2
            ld   a,(UI_X)
            jr   nz,db_f
            ld   b,a
            ld   a,7
            sub  b                  ; negras abajo: columna invertida
db_f:       ld   (UI_F),a
            ld   a,(UI_R)
            add  a,a
            add  a,a
            add  a,a
            add  a,a
            ld   hl,UI_F
            add  a,(hl)             ; casilla 0x88
            ld   l,a
            ld   h,CH_BOARD/256
            ld   a,(hl)
            call piece_glyph        ; tres caracteres en (DE)
            ld   hl,UI_X
            inc  (hl)
            ld   a,(hl)
            cp   8
            jr   c,db_col
            ld   hl,UI_Y
            inc  (hl)
            ld   a,(hl)
            cp   8
            jr   c,db_row
            ret

; A = pieza (0 vacia): tres caracteres en (DE); DE avanza. Las negras en inverso; las casillas
; oscuras vacias llevan un punto
piece_glyph: or  a
            jr   nz,pg_piece
            ld   a,(UI_R)
            ld   hl,UI_F
            add  a,(hl)
            and  1
            jr   nz,pg_light        ; (rank + columna) par = casilla oscura (a1)
            xor  a
            ld   (de),a
            inc  de
            ld   a,27               ; '.'
            ld   (de),a
            inc  de
            xor  a
            ld   (de),a
            inc  de
            ret
pg_light:   xor  a
            ld   (de),a
            inc  de
            ld   (de),a
            inc  de
            ld   (de),a
            inc  de
            ret
pg_piece:   ld   c,a
            and  7
            dec  a
            ld   l,a
            ld   h,0
            push de
            ld   de,pg_codes
            add  hl,de
            pop  de
            ld   a,(hl)             ; el codigo de la letra
            bit  3,c
            jr   nz,pg_black
            ld   b,a
            xor  a
            ld   (de),a
            inc  de
            ld   a,b
            ld   (de),a
            inc  de
            xor  a
            ld   (de),a
            inc  de
            ret
pg_black:   ld   b,a
            ld   a,128
            ld   (de),a
            inc  de
            ld   a,b
            add  a,128
            ld   (de),a
            inc  de
            ld   a,128
            ld   (de),a
            inc  de
            ret
pg_codes:   defb 53,51,39,55,54,48  ; P N B R Q K (codigos del ZX81)

; ---------------------------------------------------------------------
;  Textos
; ---------------------------------------------------------------------
t_title:    defm "NETWORK CHESS"
            defb 0
t_room:     defm "ROOM"
            defb 0
t_white:    defm "YOU ARE WHITE"
            defb 0
t_black:    defm "YOU ARE BLACK"
            defb 0
t_files:    defm " A  B  C  D  E  F  G  H"
            defb 0
t_files_b:  defm " H  G  F  E  D  C  B  A"
            defb 0
t_empty:    defb 0
t_mine:     defm "YOUR MOVE"
            defb 0
t_mine_chk: defm "YOUR MOVE - CHECK"
            defb 0
t_theirs:   defm "THE RIVAL IS THINKING..."
            defb 0
t_theirs_chk: defm "THE RIVAL IS IN CHECK"
            defb 0
t_win:      defm "CHECKMATE - YOU WIN"
            defb 0
t_lose:     defm "CHECKMATE - THE RIVAL WINS"
            defb 0
t_draw:     defm "DRAW"
            defb 0
t_help:     defm "E2 E4 ENTER  0 QUIT  9 RESEND"
            defb 0
t_help_over: defm "ENTER = AGAIN   0 = QUIT"
            defb 0
msg_tab:    defw m_1,m_2,m_3,m_4,m_5,m_6,m_7,m_8,m_9,m_10,m_11
m_1:        defb 0
m_2:        defb 0
m_3:        defm "THAT MOVE IS NOT LEGAL"
            defb 0
m_4:        defm "IT IS NOT YOUR TURN"
            defb 0
m_5:        defm "THE GAME IS OVER"
            defb 0
m_6:        defm "NETWORK ERROR: 9 RESENDS"
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
            include "chess.inc"
