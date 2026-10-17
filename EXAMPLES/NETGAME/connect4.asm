; =====================================================================
;  CONNECT4.ASM - 4 en raya en red para dos ZX81 con SD81 Booster y modulo
;  WiFi, sin servidor propio (MQTT): capa C del juego en red
;  (claude/planning/juego_en_red.md). Usa lobby.inc (menu, sala, teclado y
;  pantalla), net.inc (la sala y los mensajes firmados), game.inc (el
;  controlador de partida) y connect4.inc (las reglas).
;
;  Pantalla: la partida se dibuja en Superfast HiRes Spectrum (gfx.inc: 256x192, un atributo
;  por celda de 8x8); el lobby y los menus siguen en texto. El tablero son 7 x 6 casillas de
;  24x24 puntos (3 x 3 celdas), de color azul brillante, con discos de 20x20: rojos (jugador 1),
;  amarillos (jugador 2) y los huecos vacios en negro. La ultima ficha y los cuatro ganadores
;  se resaltan con el papel blanco. A la derecha, un panel (sala, tu color, el rival, la ultima
;  jugada) y debajo el estado, los avisos y la ayuda.
;
;  Como se juega:
;      1 = partida nueva: sale un codigo de 10 caracteres; se lo dices al otro.
;      2 = unirse: tecleas el codigo (DEL = SHIFT+0) y ENTER.
;      1-7 = la columna donde cae la ficha. Q = salir. R = reenviar si fallo la red.
;      ENTER = otra partida, cuando ha acabado.
;
;  Memoria:   $6000-$7FFF el programa;  $8800-$8B18 pantalla de texto del lobby;  $9000-$9560 variables;
;             $A000-$BAFF pantalla de la partida (bitmap $A000, atributos $B800)
;  OJO: todo el codigo tiene que quedar por DEBAJO de $8000 (el ZX81 no ejecuta codigo en los bloques 4-7).
;  Cargar:    CONNECT4.B81 (CLEAR 24575, LOAD FAST ... CODE 24576, RAND USR 24576).
;  Ensamblar: pasmo connect4.asm connect4.bin connect4.sym   (compila.bat)
; =====================================================================

NB_BASE     equ 9000h
GM_BASE     equ 9400h
UI_BASE     equ 9480h
C4_BASE     equ 9500h
G_STATE_LEN equ 42
G_MOVE_LEN  equ 1
G_CLOCK     equ 0               ; sin relojes
DFILE       equ 8800h           ; pantalla de texto del lobby: 1 + 24 filas de 33
GFX_SQUARES equ 0               ; gfx.inc sin las casillas del ajedrez
SV_BASE     equ 0BC00h          ; savegame.inc: la partida guardada en la SD (CONNECT4.SAV)
SV_ID       equ 1
SV_NAMELEN  equ 12

MSG         equ UI_BASE+5       ; el mensaje de la fila ROW_MSG (0 = ninguno)
UI_Y        equ UI_BASE+6       ; fila de pantalla del tablero que se dibuja (0 = arriba)
UI_X        equ UI_BASE+7       ; su columna (0-6)
LAST_X      equ UI_BASE+8       ; la ultima ficha: columna y fila de juego (255 = no hay)
LAST_Y      equ UI_BASE+9
LAST_BUF    equ UI_BASE+10      ; (10) "COLUMN 4" o "--------"
DISC_Y      equ UI_BASE+20      ; fila de juego de la casilla que se dibuja
DISC_P      equ UI_BASE+21      ; su ficha (0-2)

; La pantalla (gfx.inc, 32 x 24 celdas de 8x8): titulo arriba, los numeros de las columnas, el
; tablero de 7 x 6 casillas de 3 x 3 celdas, un panel a la derecha y el estado abajo
BOARD_ROW   equ 2               ; la celda de arriba a la izquierda del tablero
BOARD_COL   equ 1
PANEL_COL   equ 22              ; el panel de la derecha: 10 columnas (el codigo de la sala)
ROW_NUMS    equ 1               ; 1-7 sobre cada columna
ROW_STATUS  equ 20
ROW_MSG     equ 21              ; (ROW_HELP, la 22, esta en lobby.inc)
ATTR_TITLE  equ 070h            ; barra del titulo: papel amarillo brillante, tinta negra
ATTR_STATUS equ 060h            ; caja del estado: papel verde brillante, tinta negra
ATTR_MSG    equ 042h            ; rojo brillante
ATTR_HELP   equ 005h            ; cian
ATTR_BOARD  equ 048h            ; el tablero: papel azul brillante (tinta negra = hueco)
ATTR_HI     equ 078h            ; resaltado: papel blanco brillante
INK_P1      equ 002h            ; jugador 1: rojo
INK_P2      equ 006h            ; jugador 2: amarillo

            org  24576

            include "lobby.inc"

; ---------------------------------------------------------------------
;  La partida
; ---------------------------------------------------------------------
play:       xor  a
            ld   (POLLC),a
            ld   (MSG),a
            call gfx_on             ; lo fijo se pinta una vez; draw_all repinta lo que cambia
            ld   hl,t_title
            ld   bc,ROW_TITLE*256+7
            call gprint_at
            ld   b,ROW_TITLE
            ld   a,ATTR_TITLE
            call gattr_row
            ld   hl,t_room
            ld   bc,2*256+PANEL_COL
            call gprint_at
            ld   e,0                ; el codigo de sala: ya son codigos de caracter del ZX81
pl_code:    push de
            ld   b,3
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
            ld   hl,t_you
            ld   bc,5*256+PANEL_COL
            call gprint_at
            ld   hl,t_red           ; mi color, con su tinta
            ld   a,(GM_ME)
            cp   2
            ld   a,INK_P1+040h
            jr   nz,pl_me
            ld   hl,t_yellow
            ld   a,INK_P2+040h
pl_me:      push af
            ld   bc,6*256+PANEL_COL
            call gprint_at
            pop  af
            ld   b,6
            ld   c,PANEL_COL
            ld   e,6
            call gattr_span
            ld   hl,t_rivalw
            ld   bc,8*256+PANEL_COL
            call gprint_at
            ld   hl,t_lastlbl
            ld   bc,11*256+PANEL_COL
            call gprint_at
            ld   e,0                ; 1-7 sobre las columnas, centrados en la celda del medio
pl_nums:    push de
            ld   a,e
            add  a,a
            add  a,e
            add  a,BOARD_COL+1
            ld   c,a
            ld   b,ROW_NUMS
            call gaddr
            pop  de
            push de
            ld   a,e
            add  a,29               ; '1' en el ZX81
            call gglyph
            pop  de
            inc  e
            ld   a,e
            cp   7
            jr   nz,pl_nums
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
;  Dibujo
; ---------------------------------------------------------------------
draw_all:   call draw_board
            ld   hl,t_here
            ld   a,(NB_PEER)
            or   a
            jr   nz,da_r
            ld   hl,t_away
da_r:       ld   bc,9*256+PANEL_COL
            call gprint_at
            ld   hl,LAST_BUF
            ld   bc,12*256+PANEL_COL
            call gprint_at
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
da_st:      ld   bc,ROW_STATUS*256+1
            call gprint_row
            ld   b,ROW_STATUS
            ld   a,ATTR_STATUS
            call gattr_row
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
da_msg:     ld   bc,ROW_MSG*256+1
            call gprint_row
            ld   b,ROW_MSG
            ld   a,ATTR_MSG
            call gattr_row
            ld   a,(GM_OVER)
            or   a
            ld   hl,t_help
            jr   z,da_h2
            ld   hl,t_help_over
da_h2:      ld   bc,ROW_HELP*256+1
            call gprint_row
            ld   b,ROW_HELP
            ld   a,ATTR_HELP
            call gattr_row
            jp   sv_maybe_save      ; ya pintado: si ha cambiado la partida, a la SD

; la ultima ficha (LAST_X/LAST_Y) y su texto del panel (LAST_BUF): "COLUMN n", o "--------" si no hay
last_text:  ld   a,0FFh
            ld   (LAST_X),a
            ld   hl,t_none
            ld   de,LAST_BUF
            ld   bc,9
            ldir
            ld   a,(GM_PLY)
            ld   hl,GM_PLY+1
            or   (hl)
            ret  z                  ; sin jugadas
            ld   a,(GM_MOVE)
            cp   7
            ret  nc
            ld   e,a
            ld   d,0
            ld   hl,G_HEIGHT
            add  hl,de
            ld   a,(hl)
            or   a
            ret  z                  ; (no deberia pasar: la columna esta vacia)
            dec  a
            ld   (LAST_Y),a
            ld   a,e
            ld   (LAST_X),a
            add  a,'1'
            push af
            ld   hl,t_colw
            ld   de,LAST_BUF
            ld   bc,7
            ldir
            pop  af
            ld   (LAST_BUF+7),a     ; "COLUMN n"
            ret

; el tablero: la fila de arriba es la fila 5 del juego; cada casilla es un disco de 24x24
draw_board: call last_text
            xor  a
            ld   (UI_Y),a
db_row:     xor  a
            ld   (UI_X),a
db_col:     ld   a,(UI_Y)
            ld   b,a
            ld   a,5
            sub  b
            ld   c,a                ; C = fila de juego
            ld   a,(UI_X)
            ld   b,a                ; B = columna
            call c4_cell            ; A = 0 vacia, 1 jugador 1, 2 jugador 2 (conserva B y C)
            ld   (DISC_P),a
            ld   d,ATTR_BOARD       ; papel azul...
            or   a
            jr   z,db_ink           ; (un hueco nunca se resalta)
            ld   a,(LAST_X)         ; ...o blanco si es la ultima ficha o esta en los cuatro ganadores
            cp   b
            jr   nz,db_nolast
            ld   a,(LAST_Y)
            cp   c
            jr   z,db_hi
db_nolast:  ld   a,(GM_OVER)
            cp   1
            jr   c,db_ink
            cp   3
            jr   nc,db_ink
            call c4_inwin
            jr   nc,db_ink
db_hi:      ld   d,ATTR_HI
db_ink:     ld   a,(DISC_P)
            ld   e,0                ; hueco: tinta negra
            or   a
            jr   z,db_draw
            ld   e,INK_P1
            cp   1
            jr   z,db_draw
            ld   e,INK_P2
db_draw:    ld   a,d
            or   e
            ld   (SQ_ATTR),a
            ld   a,(UI_Y)           ; la celda de arriba a la izquierda del disco
            ld   c,a
            add  a,a
            add  a,c
            add  a,BOARD_ROW
            ld   b,a
            ld   a,(UI_X)
            ld   c,a
            add  a,a
            add  a,c
            add  a,BOARD_COL
            ld   c,a
            ld   hl,disc_cells
            call ds_cell
            inc  c
            call ds_cell
            inc  c
            call ds_cell
            inc  b
            dec  c
            dec  c
            call ds_cell
            inc  c
            call ds_cell
            inc  c
            call ds_cell
            inc  b
            dec  c
            dec  c
            call ds_cell
            inc  c
            call ds_cell
            inc  c
            call ds_cell
            ld   hl,UI_X
            inc  (hl)
            ld   a,(hl)
            cp   7
            jp   c,db_col
            ld   hl,UI_Y
            inc  (hl)
            ld   a,(hl)
            cp   6
            jp   c,db_row
            ret

; el disco de 24x24 (20 de diametro): 9 celdas de 8x8, de izquierda a derecha y de arriba abajo
disc_cells: defb 000h,000h,000h,003h,007h,00Fh,01Fh,01Fh
            defb 000h,000h,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh
            defb 000h,000h,000h,0C0h,0E0h,0F0h,0F8h,0F8h
            defb 03Fh,03Fh,03Fh,03Fh,03Fh,03Fh,03Fh,03Fh
            defb 0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh
            defb 0FCh,0FCh,0FCh,0FCh,0FCh,0FCh,0FCh,0FCh
            defb 01Fh,01Fh,00Fh,007h,003h,000h,000h,000h
            defb 0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,000h,000h
            defb 0F8h,0F8h,0F0h,0E0h,0C0h,000h,000h,000h

; ---------------------------------------------------------------------
;  Textos (ASCII; a2z los pasa a caracteres del ZX81)
; ---------------------------------------------------------------------
pick_tc:    or   a                  ; el 4 en raya no tiene reloj
            ret
t_sides:    defm "PLAYER 1 MOVES FIRST"
            defb 0
t_title:    defm "NETWORK CONNECT 4"
            defb 0
sv_name:    defm "CONNECT4.SAV"     ; sin "/": en la carpeta actual, que es la del juego
t_room:     defm "ROOM"
            defb 0
t_you:      defm "YOU ARE"
            defb 0
t_red:      defm "RED   "
            defb 0
t_yellow:   defm "YELLOW"
            defb 0
t_rivalw:   defm "RIVAL"
            defb 0
t_here:     defm "HERE"
            defb 0
t_away:     defm "AWAY"
            defb 0
t_lastlbl:  defm "LAST MOVE"
            defb 0
t_colw:     defm "COLUMN "
t_none:     defm "--------"
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
            include "savegame.inc"
            include "connect4.inc"
            include "gfx.inc"
