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

DFILE       equ 8000h           ; pantalla: 1 + 24 filas de 33 (32 caracteres y el NEWLINE)
SET_FAST    equ 02E7h           ; ROM: apaga el video si estaba en SLOW
SLOW_FAST   equ 0207h           ; ROM: vuelve al modo que pide CDFLAG

KB_SHIFT    equ UI_BASE+0
KB_KEY      equ UI_BASE+1
KB_PORT     equ UI_BASE+2
POLLC       equ UI_BASE+3       ; vueltas desde el ultimo sondeo
CODEN       equ UI_BASE+4       ; letras del codigo tecleadas
MSG         equ UI_BASE+5       ; el mensaje de la fila ROW_MSG (0 = ninguno)
UI_Y        equ UI_BASE+6

; filas y columnas de la pantalla
ROW_TITLE   equ 0
ROW_ROOM    equ 2
ROW_YOU     equ 3
ROW_BOARD   equ 6               ; la fila de arriba del tablero
ROW_COLS    equ 13
ROW_STATUS  equ 15
ROW_MSG     equ 17
ROW_HELP    equ 22
BOARD_COL   equ 4               ; la barra izquierda del tablero

            org  24576

start:      call SET_FAST           ; sin NMI: el video lo da la FPGA
            ld   hl,NB_BASE         ; variables a cero
            ld   de,NB_BASE+1
            ld   bc,0500h
            ld   (hl),0
            ldir
            call cls
            ld   hl,DFILE
            ld   (2096),hl          ; pantalla alternativa
            ld   a,170
            ld   (2098),a
            xor  a
            ld   (2090),a           ; sin desplazamiento
            ld   a,170              ; Superfast texto de 32 columnas
            ld   (2045),a

            ld   hl,t_title
            ld   bc,ROW_TITLE*256+7
            call print_at
            ld   hl,t_modem
            ld   bc,5*256+4
            call print_at
            call net_init
            or   a
            jr   z,menu
            ld   hl,t_moderr        ; el modem no contesta
            ld   bc,7*256+4
            call print_at
            ld   hl,t_anykey
            ld   bc,9*256+4
            call print_at
            call key_wait
            jp   exit

; ---------------------------------------------------------------------
;  Menu: partida nueva, unirse, salir
; ---------------------------------------------------------------------
menu:       call cls
            ld   hl,t_title
            ld   bc,ROW_TITLE*256+7
            call print_at
            ld   hl,t_menu1
            ld   bc,5*256+6
            call print_at
            ld   hl,t_menu2
            ld   bc,7*256+6
            call print_at
            ld   hl,t_menu3
            ld   bc,9*256+6
            call print_at
mn_key:     call key_wait
            cp   '1'
            jp   z,new_game
            cp   '2'
            jp   z,join_game
            cp   'Q'
            jp   z,exit
            jr   mn_key

; ---------------------------------------------------------------------
;  Partida nueva: crea la sala, ensena el codigo y espera al rival
; ---------------------------------------------------------------------
new_game:   call cls
            ld   hl,t_creating
            ld   bc,5*256+7
            call print_at
            call room_new
            or   a
            jr   z,ng_ok
            call show_err
            jp   menu
ng_ok:      call game_start
            call cls
            ld   hl,t_title
            ld   bc,ROW_TITLE*256+7
            call print_at
            ld   hl,t_code
            ld   bc,4*256+4
            call print_at
            call print_code_big
            ld   hl,t_tell
            ld   bc,9*256+3
            call print_at
            ld   hl,t_waitr
            ld   bc,11*256+4
            call print_at
            ld   hl,t_quit
            ld   bc,ROW_HELP*256+4
            call print_at
            xor  a
            ld   (POLLC),a
ng_loop:    call poll_step
            cp   2                  ; el rival ha entrado
            jp   z,play
            call kbd_scan
            cp   'Q'
            jr   nz,ng_next
            call room_leave
            jp   menu
ng_next:    call delay20
            jr   ng_loop

; el codigo de sala (ya en codigos de caracter del ZX81), en la fila 6
print_code_big: ld hl,NB_CODE81
            ld   bc,6*256+11
            call scr_addr
            ex   de,hl
            ld   hl,NB_CODE81
            ld   bc,10
            ldir
            ret

; ---------------------------------------------------------------------
;  Unirse: teclear el codigo
; ---------------------------------------------------------------------
join_game:  call cls
            ld   hl,t_title
            ld   bc,ROW_TITLE*256+7
            call print_at
            ld   hl,t_enter
            ld   bc,4*256+4
            call print_at
            ld   hl,t_enter2
            ld   bc,ROW_HELP*256+1
            call print_at
            xor  a
            ld   (CODEN),a
jg_key:     call key_wait
            cp   13
            jr   z,jg_enter
            cp   8
            jr   z,jg_del
            ld   c,a
            ld   a,(CODEN)
            cp   10
            jr   nc,jg_key
            ld   a,c
            cp   '0'
            jr   c,jg_key
            cp   'Z'+1
            jr   nc,jg_key
            cp   '9'+1
            jr   c,jg_ok
            cp   'A'
            jr   c,jg_key
jg_ok:      call a2z                ; codigo de caracter del ZX81
            ld   c,a
            ld   a,(CODEN)
            ld   e,a
            ld   d,0
            ld   hl,NB_CODE81
            add  hl,de
            ld   (hl),c
            ld   a,e
            inc  a
            ld   (CODEN),a
            call show_typed
            jr   jg_key
jg_del:     ld   a,(CODEN)
            or   a
            jr   z,jg_key
            dec  a
            ld   (CODEN),a
            call show_typed
            jr   jg_key
jg_enter:   ld   a,(CODEN)
            or   a
            jp   z,menu             ; ENTER sin nada: vuelve al menu
            cp   10
            jr   nz,jg_key
            call cls
            ld   hl,t_joining
            ld   bc,5*256+7
            call print_at
            call room_join
            or   a
            jr   z,jg_ok2
            call show_err
            jp   menu
jg_ok2:     call game_start
            jp   play

; lo tecleado, en la fila 6 (y espacios detras)
show_typed: ld   bc,6*256+11
            call scr_addr
            ex   de,hl
            ld   hl,NB_CODE81
            ld   a,(CODEN)
            ld   c,a
            ld   b,10
st_lp:      ld   a,10
            sub  b                  ; indice 0-9
            cp   c
            ld   a,0
            jr   nc,st_put          ; todavia no tecleada: espacio
            ld   a,(hl)
st_put:     ld   (de),a
            inc  de
            inc  hl
            djnz st_lp
            ret

; error de la sala (A = codigo de net.inc): mensaje y una tecla
show_err:   push af
            call cls
            ld   hl,t_err
            ld   bc,5*256+4
            call print_at
            pop  af
            cp   4
            ld   hl,t_badcode
            jr   z,se_say
            cp   6
            ld   hl,t_full
            jr   z,se_say
            cp   2
            ld   hl,t_timeout
            jr   z,se_say
            ld   hl,t_neterr
se_say:     ld   bc,7*256+4
            call print_at
            ld   hl,t_anykey
            ld   bc,10*256+4
            call print_at
            jp   key_wait

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

; A = lo que da game_poll cuando toca recoger, 0 si no toca
poll_step:  ld   a,(POLLC)
            inc  a
            ld   (POLLC),a
            cp   20
            jr   nz,ps_not20
            call room_poll_send
            xor  a
            ret
ps_not20:   cp   22
            jr   nz,ps_none
            xor  a
            ld   (POLLC),a
            jp   game_poll
ps_none:    xor  a
            ret

; ---------------------------------------------------------------------
;  Salir al BASIC
; ---------------------------------------------------------------------
exit:       ld   a,85               ; video nativo (apaga tambien la pantalla alternativa)
            ld   (2045),a
            call SLOW_FAST
            ld   bc,0
            ret

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
;  Pantalla: cls, direcciones y texto
; ---------------------------------------------------------------------
cls:        ld   hl,DFILE
            ld   (hl),76h           ; el HALT del principio
            inc  hl
            ld   c,24
cl_r:       ld   b,32
cl_c:       ld   (hl),0
            inc  hl
            djnz cl_c
            ld   (hl),76h           ; NEWLINE
            inc  hl
            dec  c
            jr   nz,cl_r
            ret

; B = fila, C = columna -> HL = su direccion en la pantalla
scr_addr:   ld   l,b
            ld   h,0
            ld   d,h
            ld   e,l
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl              ; * 32
            add  hl,de              ; * 33
            ld   e,c
            ld   d,0
            add  hl,de
            ld   de,DFILE+1
            add  hl,de
            ret

; B = fila: la deja en blanco
clr_row:    ld   c,0
            call scr_addr
            ld   d,h
            ld   e,l
            inc  de
            ld   (hl),0
            ld   bc,31
            ldir
            ret

; como print_at, pero borra antes la fila
print_row:  push hl
            push bc
            call clr_row
            pop  bc
            pop  hl
            jp   print_at

; HL = texto ASCII terminado en 0, B = fila, C = columna
print_at:   push hl
            call scr_addr
            ex   de,hl
            pop  hl
pa_lp:      ld   a,(hl)
            or   a
            ret  z
            inc  hl
            push hl                 ; a2z usa HL
            call a2z
            pop  hl
            ld   (de),a
            inc  de
            jr   pa_lp

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

; ---------------------------------------------------------------------
;  Teclado: lectura directa de las ocho filas (como TELNET). A = ASCII, o 0
;  si no hay tecla. SHIFT+0 = 8 (borrar)
; ---------------------------------------------------------------------
kbd_scan:   xor  a
            ld   (KB_SHIFT),a
            ld   (KB_KEY),a
            ld   a,0FEh
            ld   (KB_PORT),a
            ld   hl,key_tab
            ld   d,8
ks_row:     ld   a,(KB_PORT)
            ld   b,a
            ld   c,0FEh
            in   a,(c)
            cpl
            and  1Fh
            ld   e,a
            ld   b,5
ks_bit:     srl  e
            jr   nc,ks_nb
            ld   a,(hl)
            or   a
            jr   nz,ks_kk
            ld   a,1
            ld   (KB_SHIFT),a       ; la entrada 0 es SHIFT
            jr   ks_nb
ks_kk:      ld   c,a
            ld   a,(KB_KEY)
            or   a
            jr   nz,ks_nb           ; ya habia una: gana la primera
            ld   a,c
            ld   (KB_KEY),a
ks_nb:      inc  hl
            djnz ks_bit
            ld   a,(KB_PORT)
            rlc  a
            ld   (KB_PORT),a
            dec  d
            jr   nz,ks_row
            ld   a,(KB_KEY)
            ld   c,a
            ld   a,(KB_SHIFT)
            or   a
            ld   a,c
            ret  z
            cp   '0'
            ret  nz
            ld   a,8                ; SHIFT+0
            ret

; espera a que no haya ninguna tecla pulsada
key_release: call kbd_scan
            or   a
            ret  z
            call delay20
            jr   key_release

; espera una tecla (soltando la anterior), la devuelve y espera a que se suelte
key_wait:   call key_release
kw_lp:      call kbd_scan
            or   a
            jr   nz,kw_got
            call delay20
            jr   kw_lp
kw_got:     push af
            call key_release
            pop  af
            ret

key_tab:    defb 0,'Z','X','C','V'
            defb 'A','S','D','F','G'
            defb 'Q','W','E','R','T'
            defb '1','2','3','4','5'
            defb '0','9','8','7','6'
            defb 'P','O','I','U','Y'
            defb 13,'L','K','J','H'
            defb ' ','.','M','N','B'

; ---------------------------------------------------------------------
;  Textos (ASCII; a2z los pasa a caracteres del ZX81)
; ---------------------------------------------------------------------
t_bar_code  equ 14              ; la barra del tablero: el ':' del ZX81 hace de barra vertical
t_title:    defm "NETWORK CONNECT 4"
            defb 0
t_modem:    defm "STARTING THE MODEM..."
            defb 0
t_moderr:   defm "THE MODEM DOES NOT ANSWER"
            defb 0
t_anykey:   defm "PRESS ANY KEY"
            defb 0
t_menu1:    defm "1 = NEW GAME"
            defb 0
t_menu2:    defm "2 = JOIN A GAME"
            defb 0
t_menu3:    defm "Q = QUIT"
            defb 0
t_creating: defm "CREATING THE ROOM..."
            defb 0
t_code:     defm "ROOM CODE:"
            defb 0
t_tell:     defm "TELL IT TO YOUR RIVAL"
            defb 0
t_waitr:    defm "WAITING FOR THE RIVAL..."
            defb 0
t_quit:     defm "Q = BACK"
            defb 0
t_enter:    defm "TYPE THE ROOM CODE:"
            defb 0
t_enter2:   defm "DEL = SHIFT+0  ENTER = GO"
            defb 0
t_joining:  defm "JOINING THE ROOM..."
            defb 0
t_err:      defm "COULD NOT ENTER THE ROOM"
            defb 0
t_badcode:  defm "THAT CODE IS NOT VALID"
            defb 0
t_full:     defm "THE ROOM IS FULL"
            defb 0
t_timeout:  defm "THE MODEM DID NOT ANSWER"
            defb 0
t_neterr:   defm "NETWORK ERROR"
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
