; =====================================================================
;  LOBBY.INC - la parte comun de los juegos en red de NETGAME: arranque del
;  video (Superfast texto de 32 columnas con la pantalla alternativa en
;  $8000, como TELNET), menu (partida nueva / unirse / salir), crear la sala y
;  esperar al rival, teclear el codigo para unirse, errores de la sala, el
;  teclado leido a mano, el texto en pantalla y el sondeo. Cada juego (por ej.
;  connect4.asm) lo incluye justo despues del ORG y define:
;      DFILE        donde esta la pantalla alternativa: 1 + 24 filas de 33 (32 caracteres y el
;                   NEWLINE) = 793 bytes (connect4 la pone en $8000)
;      t_title      el titulo (ASCII, acaba en 0)
;      t_sides      una linea que dice que es ser el jugador 1 y el 2 (se ve al reentrar en una sala vacia)
;      play         lo que se ejecuta cuando ya hay sala y el rival esta (o, al unirse, al entrar)
;      NB_BASE, GM_BASE, UI_BASE (RAM) y G_STATE_LEN / G_MOVE_LEN, ademas de net.inc y game.inc
;  El lobby llama a game_start, room_new, room_join, room_leave, room_poll_send y game_poll.
;  Variables propias (UI_BASE+0..4): KB_SHIFT, KB_KEY, KB_PORT, POLLC, CODEN.
; =====================================================================

SET_FAST    equ 02E7h           ; ROM: apaga el video si estaba en SLOW
SLOW_FAST   equ 0207h           ; ROM: vuelve al modo que pide CDFLAG

KB_SHIFT    equ UI_BASE+0
KB_KEY      equ UI_BASE+1
KB_PORT     equ UI_BASE+2
POLLC       equ UI_BASE+3       ; vueltas desde el ultimo sondeo
CODEN       equ UI_BASE+4       ; letras del codigo tecleadas
ROW_TITLE   equ 0
ROW_HELP    equ 22

start:      call SET_FAST           ; sin NMI: el video lo da la FPGA
            ld   hl,NB_BASE         ; variables a cero
            ld   de,NB_BASE+1
            ld   bc,0500h
            ld   (hl),0
            ldir
            call txt_on

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

; Superfast texto de 32 columnas con la pantalla alternativa en DFILE (el juego puede haber pasado un
; rato a otro modo de video, p. ej. el ajedrez a HiRes Spectrum, y vuelve aqui para los menus)
txt_on:     call cls
            ld   hl,DFILE
            ld   (2096),hl          ; pantalla alternativa
            ld   a,170
            ld   (2098),a
            xor  a
            ld   (2090),a           ; sin desplazamiento
            ld   a,170              ; Superfast texto de 32 columnas
            ld   (2045),a
            ret

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
            cp   8
            jr   z,jg_empty
            call show_err
            jp   menu
jg_empty:   call cls                ; la sala existio pero esta vacia (se cayeron los dos): elegir con que ranura
            ld   hl,t_title
            ld   bc,ROW_TITLE*256+7
            call print_at
            ld   hl,t_roomempty
            ld   bc,4*256+4
            call print_at
            ld   hl,t_pick
            ld   bc,6*256+4
            call print_at
            ld   hl,t_pick1
            ld   bc,8*256+4
            call print_at
            ld   hl,t_pick2
            ld   bc,10*256+4
            call print_at
            ld   hl,t_sides         ; lo define el juego: que es ser el 1 y el 2
            ld   bc,13*256+4
            call print_at
            ld   hl,t_pickq
            ld   bc,ROW_HELP*256+4
            call print_at
jg_pick:    call key_wait
            cp   'Q'
            jp   z,menu
            ld   b,0
            cp   '1'
            jr   z,jg_as
            inc  b
            cp   '2'
            jr   nz,jg_pick
jg_as:      push bc
            call cls
            ld   hl,t_joining
            ld   bc,5*256+7
            call print_at
            pop  bc
            ld   a,b
            call room_join_as
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
            cp   7
            ld   hl,t_nofound
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
;  Textos del lobby (ASCII; a2z los pasa a caracteres del ZX81)
; ---------------------------------------------------------------------
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
t_roomempty: defm "THE ROOM IS EMPTY"
            defb 0
t_pick:     defm "ENTER AS:"
            defb 0
t_pick1:    defm "1 = PLAYER 1 (THE CREATOR)"
            defb 0
t_pick2:    defm "2 = PLAYER 2"
            defb 0
t_pickq:    defm "Q = BACK"
            defb 0
t_full:     defm "THE ROOM IS FULL"
            defb 0
t_nofound:  defm "THERE IS NO SUCH ROOM"
            defb 0
t_timeout:  defm "THE MODEM DID NOT ANSWER"
            defb 0
t_neterr:   defm "NETWORK ERROR"
            defb 0
