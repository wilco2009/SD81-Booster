; =====================================================================
;  SCRABBLE.ASM - Scrabble en red para dos ZX81 con SD81 Booster y modulo WiFi, sin servidor propio (MQTT): capa C
;  del juego en red (claude/planning/juego_en_red.md y claude/planning/scrabble.md). Todo lo hace el Z80: las reglas
;  (scrabble.inc), el diccionario de la SD con una busqueda binaria (dict.inc), la pantalla (scrabble_ui.inc), la entrada
;  de la jugada (scrabble_in.inc) y el controlador de partida (game.inc, con jugadas de longitud variable y el estado por
;  trozos). Usa lobby.inc (menu, sala, teclado y pantalla) y net.inc (la sala y los mensajes firmados).
;
;  Como se juega:
;      1 = partida nueva: eliges el idioma (ingles o castellano) y sale un codigo de 10 caracteres; se lo dices al otro.
;      2 = unirse: tecleas el codigo (SHIFT+0 borra) y ENTER.      3 = seguir la partida guardada.
;      Una jugada: con las flechas 5 (izquierda), 6 (abajo), 7 (arriba) y 8 (derecha) mueves un CURSOR por el tablero, 0 elige
;      la casilla de salida y 8 (derecha) o 6 (abajo) el sentido; despues la palabra ENTERA tal como quedara (con las letras que ya
;      estan en el tablero) y ENTER. Tambien se puede teclear H8 R QUIZ (columna A-O, fila 1-15, R o D y la palabra). Un punto antes de una letra fuerza el comodin
;      (.A); en castellano C H, L L y R R son una sola ficha y la tecla 1 es la N con tilde.
;      P ENTER = pasar.   X letras ENTER = cambiar fichas (1 a 7; hace falta que queden 7 o mas en la bolsa).
;      SHIFT+0 borra la ultima tecla.   R = reenviar la ultima jugada si fallo la red.   Q = salir.
;      ENTER = otra partida, cuando ha acabado y no hay nada tecleado.
;  Quien crea la sala es el jugador 1 y empieza. El idioma y la semilla de la bolsa salen del codigo de sala (la
;  primera letra: A ingles, B castellano), asi que los dos barajan igual sin decirse nada. Hace falta el diccionario en la
;  carpeta del juego: DICTEN.DIC (ingles) y/o DICTES.DIC (castellano), que genera TOOLS/make_dict.py.
;
;  Memoria:   $4400-$7FFF el programa;  $8800-$8B18 pantalla de texto del lobby;  $9000-$9970 variables;
;             $A000-$BAFF pantalla de la partida (bitmap $A000, atributos $B800);  $BC00 la partida guardada
;  Cargar:    SCRABBLE.B81 (CLEAR 17407, LOAD FAST ... CODE 17408, RAND USR 17408).
;  OJO: todo el codigo tiene que quedar por DEBAJO de $8000 (el ZX81 no ejecuta codigo en los bloques 4-7).
;  Ensamblar: pasmo scrabble.asm scrabble.bin scrabble.sym   (compila.bat)
; =====================================================================

NB_BASE     equ 9000h
GM_BASE     equ 9400h           ; unos 420 bytes (el estado por trozos)
DC_BASE     equ 95C0h           ; dict.inc
SB_BASE     equ 9640h           ; scrabble.inc (SB_LEN = 440)
SU_BASE     equ 9800h           ; scrabble_ui.inc
SI_BASE     equ 9870h           ; scrabble_in.inc (SI_SIZE = 132)
UI_BASE     equ 9900h
G_STATE_LEN equ 355             ; = SB_STATE_LEN (scrabble.inc)
G_MOVE_LEN  equ 21              ; [L][tipo][x y sentido n + 15 fichas]
G_CLOCK     equ 0               ; sin relojes
G_MOVE_VAR  equ 1               ; jugadas de longitud variable (game.inc)
G_BIG       equ 1               ; el estado va por trozos (game.inc)
DFILE       equ 8800h           ; pantalla de texto del lobby: 1 + 24 filas de 33
GFX_SQUARES equ 0               ; gfx.inc sin las casillas del ajedrez
NETMOUSE    equ 1               ; el raton (netmouse.inc)
NM_BASE     equ 9B00h           ; su RAM (NM_LEN bytes), por encima de las variables del juego
SC_BTN      equ 028h            ; un boton del panel: papel cian, tinta negra
SV_BASE     equ 0BC00h          ; savegame.inc: la partida guardada en la SD (SCRABBLE.SAV)
SV_ID       equ 3
SV_NAMELEN  equ 12

MSG         equ UI_BASE+8       ; el mensaje de la fila ROW_MSG (0 = ninguno, 20 = el texto de SI_MSG)
SC_DICT     equ UI_BASE+9       ; 0 si el diccionario del idioma se ha abierto
SC_PRE      equ UI_BASE+10      ; 1 si la jugada ya esta comprobada (g_valid no la repite)
SC_I        equ UI_BASE+11
SC_M        equ UI_BASE+12      ; lo que se ha tecleado: 1 jugar, 2 pasar, 3 cambiar
MV_OUT      equ UI_BASE+16      ; (21) la jugada en el formato de game.inc
SC_RC       equ UI_BASE+48      ; (10) la sala de la ultima partida

            org  17408

; La RAM de variables ($9500-$99FF: el estado por trozos de game.inc, el diccionario, el motor, la pantalla y la entrada) a cero: el lobby solo
; limpia desde NB_BASE hasta $94FF, y en un ZX81 recien encendido (o en el emulador, a $FF) lo demas trae basura. (Sigue en lobby.inc: start)
            ld   hl,9500h
            ld   de,9501h
            ld   bc,04FFh
            ld   (hl),0
            ldir

            include "lobby.inc"

; ---------------------------------------------------------------------
;  La partida
; ---------------------------------------------------------------------
play:       xor  a
            ld   (POLLC),a
            ld   (MSG),a
            ld   (SC_PRE),a
            call sc_me
            call sbi_clear
            call sbu_draw_all       ; lo fijo se pinta una vez; draw_all repinta lo que cambia
            call sbi_spr_init       ; los sprites del cursor y de la flecha
            ld   hl,t_qquit         ; Q sale (con nada tecleado)
            ld   bc,16*256+PANEL_COL
            call gprint_at
            ld   hl,t_room          ; el codigo de sala, bajo la leyenda
            ld   bc,14*256+PANEL_COL
            call gprint_at
            ld   e,0                ; ya son codigos de caracter del ZX81
pl_code:    push de
            ld   b,15
            ld   a,PANEL_COL
            add  a,e
            ld   c,a
            call gaddr
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
            call sc_btns            ; los botones del panel (ENTER, DEL, PASS, SWAP, RESEND)
            ld   a,(SC_DICT)        ; sin diccionario no se puede jugar
            or   a
            jr   z,pl_dok
            ld   a,13
            ld   (MSG),a
pl_dok:     call draw_all
pl_loop:    call kbd_scan
            or   a
            jp   z,pl_nokey
            push af
            call key_release        ; una pulsacion = una tecla (kbd_scan destruye BC)
            pop  af
pl_key:     ld   c,a
            ld   a,(SI_KN)
            or   a
            ld   a,c
            jr   nz,pl_edit
            cp   'Q'                ; con nada tecleado, estas son ordenes
            jp   z,pl_quit
            cp   'R'
            jp   z,pl_resend
            cp   13
            jr   nz,pl_edit
            ld   a,(GM_OVER)        ; ENTER sin nada tecleado: otra partida si ha acabado
            or   a
            jp   z,pl_poll
            call game_rematch
            xor  a
            ld   (MSG),a
            call draw_all
            jp   pl_poll
pl_edit:    call sbi_key            ; 0 editado, 1 jugada completa, 2 ENTER pero falta algo
            cp   1
            jr   z,pl_submit
            cp   2
            ld   b,12
            jp   z,pl_msg
            ld   a,(MSG)            ; una tecla nueva quita el aviso anterior
            or   a
            jr   z,pl_ed2
            xor  a
            ld   (MSG),a
            call draw_msg
pl_ed2:     call sbi_draw
            jp   pl_poll

; ENTER con la jugada completa: se comprueba con el motor (con el aviso concreto) y se manda
pl_submit:  ld   a,(SC_DICT)
            or   a
            ld   b,13
            jr   nz,pl_msg
            ld   a,(GM_OVER)
            or   a
            ld   b,5
            jr   nz,pl_msg
            call game_turn_of
            ld   hl,GM_ME
            cp   (hl)
            ld   b,4
            jr   nz,pl_msg
            call sbi_build          ; A = 1 jugar, 2 pasar, 3 cambiar
            ld   (SC_M),a
            cp   2
            jr   z,pl_ok
            cp   1
            jr   nz,pl_xc
            call sb_check
            jr   pl_chk
pl_xc:      call sb_xcheck
pl_chk:     or   a
            jr   z,pl_ok
            call sbi_errmsg         ; el aviso concreto, en SI_MSG
            ld   b,20
            jr   pl_msg
pl_ok:      ld   a,1                ; g_valid no vuelve a comprobarla
            ld   (SC_PRE),a
            ld   a,(SC_M)
            call sc_mkmove
            call game_move_p
            ld   b,a
            xor  a
            ld   (SC_PRE),a
            ld   a,b
            or   a
            jr   nz,pl_err
            call sbi_clear          ; hecha y enviada
            xor  a
            ld   (MSG),a
            call draw_all
            jp   pl_poll
pl_err:     ld   b,3                ; 3: no es legal (1)
            cp   1
            jr   z,pl_msg
            ld   b,4                ; 4: no te toca
            cp   2
            jr   z,pl_msg
            ld   b,5                ; 5: ha acabado
            cp   3
            jr   z,pl_msg
            call sbi_clear          ; 6: error de red (la jugada esta hecha en local: R la repite)
            ld   b,6
pl_msg:     ld   a,b
            ld   (MSG),a
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
            call sbi_blink
            call delay20
            jp   pl_loop
pl_quit:    call sbi_spr_off        ; los sprites no valen en los menus de texto
            call dc_close
            call gfx_off            ; el menu es de texto
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
draw_all:   call sbu_board
            call sbu_rack
            call sbu_panel
            call draw_status
            call draw_msg
            ld   a,(GM_OVER)        ; con la partida acabada, sin cursor
            ld   (SI_HIDE),a
            call sbi_draw
            ld   a,(GM_OVER)
            or   a
            jr   z,da_sv
            ld   hl,t_help_over
            call sbu_help
da_sv:      jp   sv_maybe_save      ; ya pintado: si ha cambiado la partida, a la SD

; el estado: de quien es el turno, o como ha acabado
draw_status: ld  a,(GM_OVER)
            or   a
            jr   z,ds_play
            ld   hl,t_draw
            cp   3
            jr   z,ds_st
            ld   hl,GM_ME
            cp   (hl)
            ld   hl,t_win
            jr   z,ds_st
            ld   hl,t_lose
            jr   ds_st
ds_play:    call game_turn_of
            ld   hl,GM_ME
            cp   (hl)
            ld   hl,t_mine
            jr   z,ds_st
            ld   hl,t_theirs
ds_st:      jp   sbu_status

; el aviso de la fila ROW_MSG: de la tabla, o el texto de SI_MSG (20)
draw_msg:   ld   a,(MSG)
            ld   hl,t_empty
            or   a
            jr   z,dm_st
            ld   hl,SI_MSG
            cp   20
            jr   z,dm_st
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
dm_st:      jp   sbu_msg

; ---------------------------------------------------------------------
;  El idioma, al crear la sala (lobby.inc): lo deja en NB_TC como la letra del codigo (A ingles, B castellano)
; ---------------------------------------------------------------------
pick_tc:    call cls
            ld   hl,t_title
            ld   bc,ROW_TITLE*256+7
            call print_at
            ld   hl,t_langq
            ld   bc,4*256+4
            call print_at
            ld   hl,t_lang1
            ld   bc,6*256+4
            call print_at
            ld   hl,t_lang2
            ld   bc,7*256+4
            call print_at
            ld   hl,t_pickq
            ld   bc,ROW_HELP*256+4
            call print_at
pt_key:     call key_wait
            cp   'Q'
            jr   nz,pt_k1
            scf
            ret
pt_k1:      sub  '1'
            jr   c,pt_key
            cp   2
            jr   nc,pt_key
            or   80h
            ld   (NB_TC),a
            or   a                  ; CF = 0
            ret

; ---------------------------------------------------------------------
;  Textos (ASCII; a2z los pasa a caracteres del ZX81)
; ---------------------------------------------------------------------
t_title:    defm "NETWORK SCRABBLE"
            defb 0
sv_name:    defm "SCRABBLE.SAV"     ; sin "/": en la carpeta actual, que es la del juego
t_sides:    defm "PLAYER 1 MOVES FIRST"
            defb 0
t_room:     defm "ROOM"
            defb 0
t_qquit:    defm "Q = QUIT"
            defb 0
t_langq:    defm "LANGUAGE:"
            defb 0
t_lang1:    defm "1 = ENGLISH"
            defb 0
t_lang2:    defm "2 = SPANISH (ESPANOL)"
            defb 0
t_empty:    defb 0
t_mine:     defm "YOUR MOVE"
            defb 0
t_theirs:   defm "THE RIVAL IS THINKING..."
            defb 0
t_win:      defm "GAME OVER - YOU WIN"
            defb 0
t_lose:     defm "GAME OVER - THE RIVAL WINS"
            defb 0
t_draw:     defm "GAME OVER - DRAW"
            defb 0
t_help_over: defm "ENTER = AGAIN   Q = QUIT"
            defb 0
msg_tab:    defw m_1,m_2,m_3,m_4,m_5,m_6,m_7,m_8,m_9,m_10,m_11,m_12,m_13
m_1:        defb 0
m_2:        defb 0
m_3:        defm "THAT MOVE IS NOT LEGAL"
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
m_12:       defm "FINISH THE MOVE FIRST"
            defb 0
m_13:       defm "NO DICTIONARY FILE IN THE SD"
            defb 0

; ---------------------------------------------------------------------
;  El raton (netmouse.inc): los clics se convierten en las teclas de siempre y entran por la cola de nm_push; el bucle las toma en pl_key
;  como si vinieran del teclado:
;      tablero, sin nada tecleado   elige la casilla de salida (la tecla 0 con el cursor ahi)
;      tablero, con la salida       una casilla a su derecha en su fila = sentido R (tecla 8); debajo en su columna = D (tecla 6); otra = empezar de nuevo
;      atril, con la palabra        la ficha (y antes las letras del tablero que ya estan en el camino)
;      panel                        ENTER, DEL, PASS, SWAP, RESEND y Q = QUIT
;      boton derecho                borra lo tecleado
; ---------------------------------------------------------------------
; una vuelta sin tecla del teclado: la cola del raton (si esta vacia, se lee el raton y se interpreta el clic)
pl_nokey:   call nm_pop
            or   a
            jp   nz,pl_key
            call nm_poll
            ld   a,(NM_EVT)
            or   a
            jp   z,pl_poll
            call sc_click
            call nm_pop
            or   a
            jp   nz,pl_key
            jp   pl_poll

sc_click:   ld   a,(NM_EVT)
            bit  1,a
            jr   nz,scc_right
            bit  0,a
            ret  z
            call nm_cell            ; B = fila, C = columna de la celda
            ld   a,b
            sub  BOARD_ROW
            jr   c,scc_oth
            cp   15
            jr   nc,scc_oth
            ld   d,a                ; D = fila del tablero
            ld   a,c
            sub  BOARD_COL
            jr   c,scc_oth
            cp   15
            jr   nc,scc_oth
            ld   e,a                ; E = columna del tablero
            jr   scc_board
scc_oth:    ld   a,b                ; el atril: dos filas de celdas
            sub  RACK_ROW
            cp   2
            jr   c,scc_rack
            ld   hl,sc_zones        ; los botones del panel
            call nm_zone
            or   a
            ret  z
            cp   14
            jr   c,scc_push         ; DEL (8) y ENTER (13) valen siempre
            ld   c,a                ; las letras (P, X, R, Q) son ordenes solo con nada tecleado
            ld   a,(SI_KN)
            or   a
            ret  nz
            ld   a,c
scc_push:   jp   nm_push
scc_right:  ld   a,(SI_KN)
            or   a
            ret  z
            call sbi_clear          ; lo tecleado, fuera; la tecla 8 hace que el bucle repinte la entrada
            ld   a,8
            jp   nm_push

scc_board:  ld   a,(SI_KN)
            or   a
            jr   z,scb_pick
            ld   a,(SI_MODE)
            cp   1
            ret  nz                 ; pasar y cambiar: sin tablero
            ld   a,(SI_STAGE)
            cp   2
            jr   z,scb_dir
            cp   3
            ret  nz                 ; escribiendo la palabra: el tablero no hace nada
scb_dir:    ld   a,d                ; ya hay casilla de salida (SI_X, SI_Y)
            ld   hl,SI_Y
            cp   (hl)
            jr   nz,scb_d2
            ld   a,e
            ld   hl,SI_X
            cp   (hl)
            ret  z                  ; la misma casilla
            jr   c,scb_new          ; a su izquierda: empezar de nuevo
            ld   a,'8'              ; a su derecha: el sentido R
            jp   nm_push
scb_d2:     ld   a,e
            ld   hl,SI_X
            cp   (hl)
            jr   nz,scb_new
            ld   a,d
            ld   hl,SI_Y
            cp   (hl)
            jr   c,scb_new          ; encima: empezar de nuevo
            ld   a,'6'              ; debajo: el sentido D
            jp   nm_push
scb_new:    call sbi_clear
scb_pick:   ld   a,e                ; el cursor a la casilla pulsada y la tecla 0 la elige
            ld   (SI_CX),a
            ld   a,d
            ld   (SI_CY),a
            ld   a,'0'
            jp   nm_push

; una ficha del atril (solo escribiendo la palabra: etapa 5): la tecla de la ficha, y antes las letras del tablero que ya estan en el camino
scc_rack:   ld   a,(SI_STAGE)
            cp   5
            ret  nz
            ld   a,(NM_QN)
            or   a
            ret  nz                 ; todavia hay teclas por entregar: el estado de SI_N no esta al dia
            ld   a,c
            sub  RACK_COL
            ret  c
            ld   d,0                ; D = la ficha del atril (3 celdas por ficha: 2 de ficha y 1 de separacion)
scr_d:      cp   3
            jr   c,scr_r
            sub  3
            inc  d
            jr   scr_d
scr_r:      cp   2
            ret  nc                 ; la celda de separacion
            ld   a,d
            cp   7
            ret  nc
            ld   e,a
            ld   a,(SU_ME)          ; SB_RACK + 7 * yo + ficha
            ld   d,a
            add  a,a
            add  a,a
            add  a,a
            sub  d
            add  a,e
            ld   e,a
            ld   d,0
            ld   hl,SB_RACK
            add  hl,de
            ld   a,(hl)
            or   a
            ret  z                  ; hueco
            ld   d,a                ; D = la ficha
            ld   a,(SI_MODE)
            cp   1
            jr   nz,scr_tile        ; cambiar: solo las fichas
            ld   e,0                ; E = letras del tablero ya metidas en la cola
scr_s:      call scr_cell           ; A = lo que hay en la casilla siguiente del camino (0 vacia, 0FFh fuera)
            or   a
            jr   z,scr_tile
            cp   0FFh
            jr   z,scr_tile
            and  7Fh                ; un comodin puesto: su letra
            call scr_push
            inc  e
            jr   scr_s
scr_tile:   ld   a,d
            cp   '?'
            jr   nz,scr_push
            ld   a,'.'              ; el comodin: el punto (la letra la teclea el jugador)
            jp   nm_push

; A = una ficha -> las teclas que la escriben (la N con tilde es la 1; CH, LL y RR, dos letras)
scr_push:   cp   '['
            jr   nz,sp_c
            ld   a,'1'
            jp   nm_push
sp_c:       cp   '\'
            jr   nz,sp_l
            ld   a,'C'
            call nm_push
            ld   a,'H'
            jp   nm_push
sp_l:       cp   ']'
            jr   nz,sp_r
            ld   a,'L'
            call nm_push
            ld   a,'L'
            jp   nm_push
sp_r:       cp   '^'
            jr   nz,sp_x
            ld   a,'R'
            call nm_push
            ld   a,'R'
            jp   nm_push
sp_x:       jp   nm_push

; A = lo que hay en la casilla del camino que toca (SI_N fichas ya escritas + E): 0 vacia, 0FFh si se sale del tablero. Conserva D y E
scr_cell:   push de
            ld   a,(SI_N)
            add  a,e
            ld   e,a                ; E = cuantas casillas se han avanzado
            ld   a,(SI_DIR)
            or   a
            jr   nz,sce_d
            ld   a,(SI_X)           ; hacia la derecha: x = SI_X + E
            add  a,e
            ld   b,a
            ld   a,(SI_Y)
            ld   c,a
            jr   sce_g
sce_d:      ld   a,(SI_X)           ; hacia abajo: y = SI_Y + E
            ld   b,a
            ld   a,(SI_Y)
            add  a,e
            ld   c,a
sce_g:      ld   a,b
            cp   15
            jr   nc,sce_out
            ld   a,c
            cp   15
            jr   nc,sce_out
            call sb_addr            ; B = x, C = y -> HL
            ld   a,(hl)
            pop  de
            ret
sce_out:    ld   a,0FFh
            pop  de
            ret

; los botones del panel (se pintan una vez al empezar la partida): fila, columna, longitud y texto (ASCII, acaba en 0)
sc_btns:    ld   hl,sc_blabel
scb_l:      ld   a,(hl)
            or   a
            ret  z
            ld   b,a
            inc  hl
            ld   c,(hl)
            inc  hl
            ld   e,(hl)
            inc  hl                 ; HL = el texto
            push bc
            push de
            push hl
            call gprint_at
            pop  hl
            pop  de
            pop  bc
            ld   a,SC_BTN
            push hl
            push de
            call gattr_span         ; B = fila, C = columna, E = longitud
            pop  de
            pop  hl
            ld   d,0
            add  hl,de
            inc  hl                 ; el texto y su 0
            jr   scb_l

sc_blabel:  defb 12,18,5
            defm "ENTER"
            defb 0
            defb 12,24,3
            defm "DEL"
            defb 0
            defb 12,28,4
            defm "PASS"
            defb 0
            defb 13,18,4
            defm "SWAP"
            defb 0
            defb 13,23,6
            defm "RESEND"
            defb 0
            defb 0

; zonas de los botones (nm_zone): fila, columna inicial, columna final + 1, tecla. Q = QUIT es la fila 16 entera del panel
sc_zones:   defb 12,18,23,13            ; ENTER
            defb 12,24,27,8             ; DEL (SHIFT+0)
            defb 12,28,32,'P'           ; PASS
            defb 13,18,22,'X'           ; SWAP
            defb 13,23,29,'R'           ; RESEND
            defb 16,18,32,'Q'           ; Q = QUIT
            defb 0

            include "../NETCHAT/net.inc"
            include "game.inc"
            include "savegame.inc"
            include "dict.inc"
            include "scrabble.inc"
            include "gfx.inc"
            include "netmouse.inc"
            include "scrabble_ui.inc"
            include "scrabble_in.inc"
            include "scrabble_game.inc"
