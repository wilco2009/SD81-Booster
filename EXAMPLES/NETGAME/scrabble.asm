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
            ld   a,(SC_DICT)        ; sin diccionario no se puede jugar
            or   a
            jr   z,pl_dok
            ld   a,13
            ld   (MSG),a
pl_dok:     call draw_all
pl_loop:    call kbd_scan
            or   a
            jp   z,pl_poll
            push af
            call key_release        ; una pulsacion = una tecla (kbd_scan destruye BC)
            pop  af
            ld   c,a
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

            include "../NETCHAT/net.inc"
            include "game.inc"
            include "savegame.inc"
            include "dict.inc"
            include "scrabble.inc"
            include "gfx.inc"
            include "scrabble_ui.inc"
            include "scrabble_in.inc"
            include "scrabble_game.inc"
