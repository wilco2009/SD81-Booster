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
;  tablero girado.  Las piezas son dibujos de 16x16 (pieces.inc, que genera gen_pieces.py a partir de
;  piezas_16x16.png): las mismas para las dos bandos, con tinta blanca o negra, sobre casillas verde claro y
;  rojo oscuro, brillantes. La partida se dibuja en Superfast HiRes Spectrum (gfx.inc); el lobby y los
;  menus siguen en texto. La ultima jugada se resalta en magenta y el rey en jaque con el papel del color
;  contrario a su tinta (el blanco sobre negro, el negro sobre blanco).
;
;  Reglas: todo el ajedrez salvo la triple repeticion (enroque, al paso, coronacion,
;  jaque, mate, ahogado, material insuficiente y las 50 jugadas).
;
;  Memoria:   $6000-$87FF el programa;  $8800-$8B18 pantalla de texto del lobby;  $9000-$9A00 variables;
;             $A000-$BAFF pantalla de la partida (bitmap $A000, atributos $B800)
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

; La pantalla (gfx.inc: HiRes Spectrum, 32 x 24 celdas de 8x8): el titulo arriba, el tablero de 8 x 8
; casillas de 2 x 2 celdas, las coordenadas, un panel a la derecha y el estado abajo. Los colores
; (gfx.inc) son los de Cyrus II adaptados a la paleta del Spectrum: casillas verdes y rojas, brillantes,
; barra del titulo amarilla y el estado en una caja verde
BOARD_ROW   equ 1               ; la celda de arriba a la izquierda del tablero
BOARD_COL   equ 3
PANEL_COL   equ 21              ; el panel de la derecha (sala, color, ultima jugada)
ROW_FILES   equ 17              ; A-H bajo el tablero
ROW_STATUS  equ 18
ROW_MSG     equ 19
ROW_INPUT   equ 20
ATTR_TITLE  equ 070h            ; barra del titulo: papel amarillo brillante, tinta negra
ATTR_STATUS equ 060h            ; caja del estado: papel verde brillante, tinta negra
ATTR_MSG    equ 042h            ; rojo brillante
ATTR_HELP   equ 005h            ; cian

            org  24576

            include "lobby.inc"

; ---------------------------------------------------------------------
;  La partida
; ---------------------------------------------------------------------
play:       xor  a
            ld   (POLLC),a
            ld   (MSG),a
            ld   (MV_N),a
            call gfx_on             ; lo fijo se pinta una vez; draw_all repinta lo que cambia
            ld   hl,t_title
            ld   bc,ROW_TITLE*256+BOARD_COL
            call gprint_at
            ld   b,ROW_TITLE
            ld   a,ATTR_TITLE
            call gattr_row
            ld   hl,t_room
            ld   bc,1*256+PANEL_COL
            call gprint_at
            ld   e,0                ; el codigo de sala: ya son codigos de caracter del ZX81
pl_code:    push de
            ld   b,2
            ld   a,PANEL_COL
            add  a,e
            ld   c,a
            call gaddr              ; HL = la celda
            pop  de
            push de
            ld   d,0
            push hl
            ld   hl,NB_CODE81
            add  hl,de
            ld   a,(hl)
            pop  hl
            call gglyph
            pop  de
            inc  e
            ld   a,e
            cp   10
            jr   nz,pl_code
            ld   hl,t_white
            ld   a,(GM_ME)
            cp   2
            jr   nz,pl_col
            ld   hl,t_black
pl_col:     ld   bc,5*256+PANEL_COL
            call gprint_at
            ld   hl,t_youare
            ld   bc,4*256+PANEL_COL
            call gprint_at
            ld   hl,t_lastlbl
            ld   bc,8*256+PANEL_COL
            call gprint_at
            call draw_labels
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
pl_quit:    call gfx_off            ; el menu es de texto
            call room_leave
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
da_st:      ld   bc,ROW_STATUS*256+2
            call gprint_row
            ld   b,ROW_STATUS
            ld   a,ATTR_STATUS
            call gattr_row
            ; la ultima jugada, en el panel
            call last_move
            ld   hl,LAST_BUF
            ld   bc,9*256+PANEL_COL
            call gprint_at
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
da_msg:     ld   bc,ROW_MSG*256+2
            call gprint_row
            ld   a,(MSG)
            cp   3                  ; los avisos de error (jugada ilegal, no te toca...) en rojo
            ld   a,ATTR_TEXT
            jr   c,da_mc
            ld   a,ATTR_MSG
da_mc:      ld   b,ROW_MSG
            call gattr_row
            ld   a,(GM_OVER)
            or   a
            ld   hl,t_help
            jr   z,da_h2
            ld   hl,t_help_over
da_h2:      ld   bc,ROW_HELP*256+2
            call gprint_row
            ld   b,ROW_HELP
            ld   a,ATTR_HELP
            jp   gattr_row

; "MOVE: E2E4" en la fila de entrada
draw_input: ld   hl,LAST_BUF        ; se arma ahi: "MOVE " + lo tecleado + " -"
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
            ld   bc,ROW_INPUT*256+2
            jp   gprint_row

; LAST_BUF = la ultima jugada ("E2-E4") o "-----" si no ha habido ninguna (siempre cinco caracteres)
last_move:  ld   hl,LAST_BUF
            ld   de,(GM_PLY)
            ld   a,d
            or   e
            jr   nz,lm_some
            ld   b,5
lm_none:    ld   (hl),'-'
            inc  hl
            djnz lm_none
            ld   (hl),0
            ret
lm_some:    ld   a,(GM_MOVE)
            and  3Fh
            call sq_text
            ld   (hl),'-'
            inc  hl
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

; las letras A-H bajo el tablero y las cifras 1-8 a su izquierda (al reves si soy el 2)
draw_labels: ld  b,0
dl_f:       push bc
            ld   a,(GM_ME)
            cp   2
            ld   a,b
            jr   nz,dl_f1
            ld   a,7
            sub  b
dl_f1:      add  a,38               ; 'A' del ZX81
            push af
            ld   a,b
            add  a,a
            add  a,BOARD_COL
            ld   c,a
            ld   b,ROW_FILES
            pop  af
            call gglyph_h           ; centrada en las dos celdas de la casilla
            pop  bc
            inc  b
            ld   a,b
            cp   8
            jr   nz,dl_f
            ld   b,0
dl_r:       push bc
            ld   a,(GM_ME)
            cp   2
            ld   a,b
            jr   z,dl_r1
            ld   a,7
            sub  b
dl_r1:      add  a,29               ; '1' del ZX81 = 29
            push af
            ld   a,b
            add  a,a
            add  a,BOARD_ROW
            ld   b,a
            ld   c,BOARD_COL-1
            pop  af
            call gglyph_v           ; centrada en las dos celdas de la casilla
            pop  bc
            inc  b
            ld   a,b
            cp   8
            jr   nz,dl_r
            ret

; el tablero (con las blancas abajo, o las negras si soy el 2)
draw_board: call mk_marks
            xor  a
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
            call draw_square
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

; las casillas que se resaltan: la ultima jugada (de y a) y el rey en jaque (casilla 0-63, o 0FFh)
mk_marks:   ld   a,0FFh
            ld   (HL_FROM),a
            ld   (HL_TO),a
            ld   (CHK_SQ),a
            ld   hl,(GM_PLY)
            ld   a,h
            or   l
            jr   z,mm_chk
            ld   a,(GM_MOVE)
            and  3Fh
            ld   (HL_FROM),a
            ld   a,(GM_MOVE+1)
            and  3Fh
            ld   (HL_TO),a
mm_chk:     call mm_color           ; A = 0 o 8: el que mueve
            call ch_in_check
            ret  nc
            call mm_color
            or   6
            ld   c,a                ; C = su rey
            ld   b,0
mm_r:       ld   d,0
mm_f:       ld   a,b
            add  a,a
            add  a,a
            add  a,a
            add  a,a
            add  a,d
            ld   l,a
            ld   h,CH_BOARD/256
            ld   a,(hl)
            cp   c
            jr   z,mm_found
            inc  d
            ld   a,d
            cp   8
            jr   c,mm_f
            inc  b
            ld   a,b
            cp   8
            jr   c,mm_r
            ret
mm_found:   ld   a,b
            add  a,a
            add  a,a
            add  a,a
            add  a,d
            ld   (CHK_SQ),a
            ret
mm_color:   ld   a,(GM_PLY)
            and  1
            add  a,a
            add  a,a
            add  a,a
            ret

; ---------------------------------------------------------------------
;  Textos
; ---------------------------------------------------------------------
t_title:    defm "NETWORK CHESS"
            defb 0
t_room:     defm "ROOM"
            defb 0
t_youare:   defm "YOU ARE"
            defb 0
t_sides:    defm "PLAYER 1 = WHITE, 2 = BLACK"
            defb 0
t_white:    defm "WHITE"
            defb 0
t_black:    defm "BLACK"
            defb 0
t_lastlbl:  defm "LAST MOVE"
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
            include "gfx.inc"
            include "pieces.inc"
