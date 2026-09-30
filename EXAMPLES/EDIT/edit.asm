; =====================================================================
;  EDIT.ASM - editor de textos ASCII para el ZX81 con SD81 Booster.
;  80x24 en Superfast (caracteres de 7 pixeles), fuente CP437 de 256
;  caracteres, teclas de WordStar.
;
;  El fichero a editar se le pasa en la variable F$ (la que monta el
;  stub BASIC del explorador; desde el prompt, LET F$="NOTAS.TXT"):
;    - si F$ existe y el fichero tambien, lo abre;
;    - si F$ existe pero el fichero no, empieza uno nuevo con ese nombre;
;    - si no hay F$ (o esta vacia), empieza uno sin nombre, y lo pide al
;      guardar.
;  Como el editor es codigo maquina (LOAD ... CODE y RAND USR), no se
;  carga ningun programa BASIC y las variables siguen ahi.
;
;  Teclas (SHIFT+ENTER y luego la letra = CTRL+letra):
;      flechas SHIFT+5/6/7/8, o ^S ^E ^X ^D
;      ^A / ^F          palabra anterior / siguiente
;      ^R / ^C          pagina arriba / abajo
;      ^Q S / ^Q D      principio / final de linea
;      ^Q R / ^Q C      principio / final del texto
;      SHIFT+0, SHIFT+9 borrar a la izquierda; ^G borrar a la derecha
;      ^Y               borrar la linea
;      SHIFT+.          tabulador
;      ^K S             guardar
;      ^K X, ^K D       guardar y salir
;      ^K Q, ENTER+0    salir (pregunta si hay cambios sin guardar)
;      SHIFT+1          ESC: cancela ^K / ^Q y las preguntas
;      ENTER+9, ^J      pantalla de ayuda: el teclado y las ordenes
;  El teclado es el de TELNET (y el de CP/M): minusculas sin SHIFT,
;  ENTER+tecla da los simbolos.
;
;  Texto: gap buffer. Todo el acceso al texto pasa por las rutinas tb_*
;  e it_* (y la carga y el guardado), de forma que para paginar
;  (ficheros de mas de 26 KB) baste con cambiar esas rutinas. Se guarda
;  con LF; si el fichero traia CRLF, se vuelve a guardar con CRLF.
;
;  Ficheros: comandos del MCU 53 (fopen), 59 (fstat), 55 (fread),
;  71 (fcreate), 56 (fwrite) y 57 (fclose), en trozos de 512 bytes.
;  fcreate (71) es del firmware 2.6.
;
;  Memoria:
;      $6000-          este programa
;      $8000-$8798     pantalla (1 + 24 filas de 81 bytes)
;      $8800-$8F98     atributos, misma geometria
;      $9000-$97FF     fuente (I = $90)
;      $9800-$FFFE     texto (26623 bytes). Los bloques 6 y 7 ($C000-
;                      $FFFF) se mapean a las paginas 8 y 9 mientras se
;                      edita (por defecto son un espejo de la RAM del
;                      sistema), y al salir se dejan en su pagina (6 y
;                      7), como hace el explorador con el bloque 7.
;
;  Ensamblar: pasmo edit.asm edit.bin (compila.bat)
;  Cargar: desde el explorador (tecla E) o con el stub EDIT.B81.
; =====================================================================

; --- ROM y sistema ---------------------------------------------------
SET_FAST    equ 02E7h           ; ROM: apaga el video si estaba en SLOW
SLOW_FAST   equ 0207h           ; ROM: vuelve al modo que pide CDFLAG
SV_VARS     equ 16400           ; variable del sistema VARS
FVAR        equ 04Bh            ; F$ en la zona de variables: 010 + letra F

; --- MCU -------------------------------------------------------------
DataPort    equ 0A7h
ClkPort     equ 0AFh
CMD_64C     equ 1Ch             ; SEL_64CHARS
CMD_128C    equ 1Bh             ; SEL_128CHARS
CMD_256C    equ 41h             ; SEL_256CHARS
CMD_FOPEN   equ 53
CMD_FREAD   equ 55
CMD_FWRITE  equ 56
CMD_FCLOSE  equ 57
CMD_FSTAT   equ 59
CMD_FCREATE equ 71
CHUNK       equ 512             ; lo que cabe en un fread/fwrite

; --- memoria ---------------------------------------------------------
MAP_PORT    equ 0E7h
PAGE_B6     equ 8               ; paginas del texto en los bloques 6 y 7
PAGE_B7     equ 9
BUF_START   equ 9800h
BUF_END     equ 0FFFFh          ; fin (exclusivo) del texto
BUF_SIZE    equ BUF_END-BUF_START

; --- video -----------------------------------------------------------
DFILE       equ 8000h           ; pantalla (DFILE_OVR)
ATTR        equ 8800h           ; atributos (ATTR_OVR)
ATTR_OFF    equ ATTR-DFILE
FONT_ADDR   equ 9000h
FONT_I      equ FONT_ADDR/256
SCRW        equ 80
SCRH        equ 24
TEXTH       equ 23              ; filas de texto; la 24 es la de estado
ROWSTRIDE   equ 81              ; 80 caracteres + 1 byte de relleno
STATUS_ROW  equ DFILE+1+ROWSTRIDE*TEXTH
; Atributo PPPP IIII (papel arriba, tinta abajo; en cada nibble brillo,
; verde, rojo, azul)
TEXT_ATTR   equ 00Fh            ; blanco brillante sobre negro
STAT_ATTR   equ 070h            ; negro sobre blanco
CHROMA_PORT equ 07FEFh
CHROMA_ON   equ 030h            ; color + modo 1 (atributos), borde negro
CHROMA_OFF  equ 00Ch
CURSOR_BLOCK equ 0DBh           ; bloque solido CP437 (cursor normal)
CURSOR_UNDER equ 05Fh           ; guion bajo (cursor en modo CTRL)
CTRL_GLYPH  equ 0FEh            ; como se ve un caracter de control
; pantalla de ayuda: cada tecla en azul, con la tecla en blanco, lo que
; hace con SHIFT en amarillo y lo que da con ENTER en cian
KEY_ATTR    equ 010h            ; fondo de la tecla: papel azul
KEYCAP_ATTR equ 01Fh
KEYSH_ATTR  equ 01Eh
KEYEN_ATTR  equ 01Dh
GL_LEFT     equ 1               ; flechas: caracteres propios (los codigos
GL_RIGHT    equ 2               ; 0-31 del texto se ven como CTRL_GLYPH,
GL_UP       equ 3               ; asi que no chocan)
GL_DOWN     equ 4
BLINK_RATE  equ 25              ; VSYNC por semiciclo de parpadeo
REP_DELAY   equ 25              ; VSYNC antes de empezar a repetir
REP_RATE    equ 3               ; VSYNC entre repeticiones

; --- teclas ----------------------------------------------------------
KEY_UP      equ 081h
KEY_DOWN    equ 082h
KEY_RIGHT   equ 083h
KEY_LEFT    equ 084h
K_ESC       equ 01Bh
K_QUIT      equ 01Dh            ; ^]  (ENTER+0)
K_HELP      equ 01Ch            ; ^\  (ENTER+9)

            org  24576

; =====================================================================
;  Arranque
; =====================================================================
start:      ld   (sv_sp),sp         ; para salir desde cualquier sitio
            ld   a,i
            ld   (sv_i),a
            call SET_FAST           ; sin NMI: el video lo da la FPGA

            ld   hl,vars            ; variables a cero
            ld   de,vars+1
            ld   bc,VARS_LEN-1
            ld   (hl),0
            ldir

            ld   a,6                ; el texto: paginas propias en los
            ld   e,PAGE_B6          ; bloques 6 y 7
            call mcu_map
            ld   a,7
            ld   e,PAGE_B7
            call mcu_map
            call tb_clear

            call video_on
            call find_fvar          ; F$ -> nombre
            call load_file

; ---------------------------------------------------------------------
;  Bucle principal: ajustar la ventana, pintar y atender una tecla
; ---------------------------------------------------------------------
main:       call ensure_visible
            call redraw
            call draw_status
            call place_cursor
            call kbget
            ld   hl,0               ; el mensaje dura hasta la tecla siguiente
            ld   (msg_ptr),hl
            call dispatch
            jr   main

; =====================================================================
;  Ordenes
; =====================================================================
dispatch:   ld   c,a
            ld   a,(prefix)
            or   a
            jp   nz,do_prefix
            ld   a,c
            cp   KEY_UP
            jp   z,k_up
            cp   KEY_DOWN
            jp   z,k_down
            cp   KEY_LEFT
            jp   z,k_left
            cp   KEY_RIGHT
            jp   z,k_right
            cp   07Fh               ; SHIFT+0: borrar a la izquierda
            jp   z,k_bs
            cp   ' '
            jr   nc,k_char          ; imprimible
            ld   l,a                ; control: tabla
            ld   h,0
            add  hl,hl
            ld   de,ctrl_tab
            add  hl,de
            ld   a,(hl)
            inc  hl
            ld   h,(hl)
            ld   l,a
            jp   (hl)

ctrl_tab:   defw k_none,  k_wleft, k_none,  k_pgdn     ; 00 ^A ^B ^C
            defw k_right, k_up,    k_wright,k_del      ; ^D ^E ^F ^G
            defw k_bs,    k_char,  k_help,  k_kpre     ; ^H TAB ^J ^K
            defw k_none,  k_enter, k_none,  k_none     ; ^L ENTER ^N ^O
            defw k_none,  k_qpre,  k_pgup,  k_left     ; ^P ^Q ^R ^S
            defw k_none,  k_none,  k_none,  k_none     ; ^T ^U ^V ^W
            defw k_down,  k_dline, k_none,  k_none     ; ^X ^Y ^Z ESC
            defw k_help,  k_quit,  k_none,  k_none     ; ^\ ^] ^^ ^_

k_none:     ret

k_char:     ld   a,c                ; insertar el caracter
            call tb_insert
            jp   nc,set_want
            ld   hl,msg_full
            ld   (msg_ptr),hl
            ret

k_enter:    ld   c,0Ah
            jr   k_char

k_bs:       call tb_del_back
            jp   set_want

k_del:      jp   tb_del_fwd

k_left:     call tb_left
            jp   set_want

k_right:    call tb_right
            jp   set_want

; arriba: al principio de la linea anterior y avanzar hasta want_col
k_up:       call cur_line_start
            ld   a,h
            or   l
            ret  z                  ; ya en la primera linea
            dec  hl                 ; el LF que cierra la anterior
            call tb_goto
            call cur_line_start
            call tb_goto
            jp   goto_want

; abajo: detras del siguiente LF (si lo hay) y avanzar hasta want_col
k_down:     call next_lf
            ret  nc                 ; ultima linea
            call tb_goto
            jp   goto_want

k_pgup:     ld   b,TEXTH-1
kpu1:       push bc
            call k_up
            pop  bc
            djnz kpu1
            ret

k_pgdn:     ld   b,TEXTH-1
kpd1:       push bc
            call k_down
            pop  bc
            djnz kpd1
            ret

k_home:     call cur_line_start
            call tb_goto
            jp   set_want

k_end:      call next_lf            ; HL = detras del LF (o el final)
            jr   nc,ke1
            dec  hl                 ; delante del LF
ke1:        call tb_goto
            jp   set_want

k_top:      ld   hl,0
            call tb_goto
            jp   set_want

k_bottom:   call tb_len
            call tb_goto
            jp   set_want

; palabra siguiente: saltar la palabra y luego los blancos
k_wright:   call tb_char_fwd
            jr   c,kwr3
            call is_blank
            jr   z,kwr2
            call tb_right
            jr   k_wright
kwr2:       call tb_char_fwd
            jr   c,kwr3
            call is_blank
            jr   nz,kwr3
            call tb_right
            jr   kwr2
kwr3:       jp   set_want

; palabra anterior: saltar los blancos y luego la palabra
k_wleft:    call tb_char_back
            jr   c,kwl3
            call is_blank
            jr   nz,kwl2
            call tb_left
            jr   k_wleft
kwl2:       call tb_char_back
            jr   c,kwl3
            call is_blank
            jr   z,kwl3
            call tb_left
            jr   kwl2
kwl3:       jp   set_want

is_blank:   cp   ' '                ; Z si A es espacio, TAB o LF
            ret  z
            cp   09h
            ret  z
            cp   0Ah
            ret

; ^Y: borrar la linea entera, con su LF
k_dline:    call cur_line_start
            call tb_goto
kdl1:       call tb_char_fwd
            jr   c,kdl2
            push af
            call tb_del_fwd
            pop  af
            cp   0Ah
            jr   nz,kdl1
kdl2:       jp   set_want

; --- ^K y ^Q: la tecla siguiente decide -------------------------------
k_kpre:     ld   a,'K'
            ld   hl,msg_kpre
            jr   kp1
k_qpre:     ld   a,'Q'
            ld   hl,msg_qpre
kp1:        ld   (prefix),a
            ld   (msg_ptr),hl
            ret

do_prefix:  ld   b,a                ; B = 'K' o 'Q'
            xor  a
            ld   (prefix),a
            ld   a,c
            cp   ' '                ; ^K^S = ^K S, como en WordStar
            jr   nc,dp1
            add  a,40h
dp1:        and  0DFh               ; a mayusculas
            ld   c,a
            ld   a,b
            cp   'Q'
            ld   a,c
            jr   z,dp_q
            cp   'S'
            jp   z,do_save
            cp   'X'
            jr   z,k_saveexit
            cp   'D'
            jr   z,k_saveexit
            cp   'Q'
            jr   z,k_quit
            ret
dp_q:       cp   'S'
            jp   z,k_home
            cp   'D'
            jp   z,k_end
            cp   'R'
            jp   z,k_top
            cp   'C'
            jp   z,k_bottom
            ret

k_saveexit: call do_save
            ret  c                  ; no se ha guardado: seguir editando
            jp   quit

k_quit:     ld   a,(modified)
            or   a
            jp   z,quit
            ld   hl,msg_askq        ; SAVE CHANGES? (Y/N)
            call ask_yn
            ret  c                  ; ESC: seguir editando
            jp   nz,quit            ; N: salir sin guardar
            call do_save            ; Y
            ret  c
            jp   quit

; want_col = columna del cursor (tras moverse en horizontal o editar)
set_want:   call cur_vcol
            ld   (want_col),de
            ret

; avanzar desde el principio de una linea hasta want_col
goto_want:  ld   de,0
gw1:        call tb_char_fwd
            ret  c
            cp   0Ah
            ret  z
            push de
            call vc_adv             ; DE = columna detras de este caracter
            ld   hl,(want_col)
            or   a
            sbc  hl,de
            pop  hl                 ; HL = columna de este caracter
            ret  c                  ; se pasaria de want_col: quedarse
            push de
            call tb_right
            pop  de
            jr   gw1

; =====================================================================
;  Texto (gap buffer): [BUF_START,gap_s) antes del cursor y
;  [gap_e,BUF_END) detras. Las posiciones son desplazamientos desde el
;  principio del texto (0..tb_len).
; =====================================================================
tb_clear:   ld   hl,BUF_START
            ld   (gap_s),hl
            ld   hl,BUF_END
            ld   (gap_e),hl
            ld   hl,0
            ld   (cur_line),hl
            ret

tb_pos:     ld   hl,(gap_s)         ; HL = posicion del cursor
            ld   de,BUF_START
            or   a
            sbc  hl,de
            ret

tb_len:     call tb_pos             ; HL = longitud del texto
            push hl
            ld   hl,BUF_END
            ld   de,(gap_e)
            or   a
            sbc  hl,de
            pop  de
            add  hl,de
            ret

; A = caracter a insertar en el cursor. CF si no cabe.
tb_insert:  ld   c,a
            ld   hl,(gap_s)
            ld   de,(gap_e)
            or   a
            sbc  hl,de
            scf
            ret  z
            ld   hl,(gap_s)
            ld   (hl),c
            inc  hl
            ld   (gap_s),hl
            ld   a,c
            cp   0Ah
            jr   nz,tbi1
            ld   hl,(cur_line)
            inc  hl
            ld   (cur_line),hl
tbi1:       ld   a,1
            ld   (modified),a
            or   a
            ret

; borrar delante del cursor (a la izquierda). CF si no hay nada.
tb_del_back: ld  hl,(gap_s)
            ld   de,BUF_START
            or   a
            sbc  hl,de
            scf
            ret  z
            ld   hl,(gap_s)
            dec  hl
            ld   (gap_s),hl
            ld   a,(hl)
            cp   0Ah
            jr   nz,tbi1
            ld   hl,(cur_line)
            dec  hl
            ld   (cur_line),hl
            jr   tbi1

; borrar el caracter del cursor (a la derecha). CF si no hay nada.
tb_del_fwd: ld   hl,(gap_e)
            ld   de,BUF_END
            or   a
            sbc  hl,de
            scf
            ret  z
            ld   hl,(gap_e)
            inc  hl
            ld   (gap_e),hl
            jr   tbi1

; A = caracter del cursor (CF al final) / el de delante (CF al principio)
tb_char_fwd: ld  hl,(gap_e)
            ld   de,BUF_END
            or   a
            sbc  hl,de
            scf
            ret  z
            ld   hl,(gap_e)
            ld   a,(hl)
            or   a
            ret
tb_char_back: ld hl,(gap_s)
            ld   de,BUF_START
            or   a
            sbc  hl,de
            scf
            ret  z
            ld   hl,(gap_s)
            dec  hl
            ld   a,(hl)
            or   a
            ret

; cursor una posicion a la izquierda / derecha. CF si no se puede.
tb_left:    call tb_char_back
            ret  c
            ld   hl,(gap_s)
            dec  hl
            ld   (gap_s),hl
            ld   hl,(gap_e)
            dec  hl
            ld   (gap_e),hl
            ld   (hl),a
            cp   0Ah
            jr   nz,tbl1
            ld   hl,(cur_line)
            dec  hl
            ld   (cur_line),hl
tbl1:       or   a
            ret

tb_right:   call tb_char_fwd
            ret  c
            ld   hl,(gap_e)
            inc  hl
            ld   (gap_e),hl
            ld   hl,(gap_s)
            ld   (hl),a
            inc  hl
            ld   (gap_s),hl
            cp   0Ah
            jr   nz,tbl1
            ld   hl,(cur_line)
            inc  hl
            ld   (cur_line),hl
            or   a
            ret

; HL = posicion destino (<= tb_len): mueve el hueco en bloque y lleva la
; cuenta de lineas (los LF que cruza).
tb_goto:    push hl
            call tb_pos
            pop  de                 ; HL = posicion actual, DE = destino
            or   a
            sbc  hl,de              ; HL = actual - destino
            ret  z
            jr   c,tg_right
            ld   b,h                ; a la izquierda: BC bytes de delante
            ld   c,l                ; del cursor pasan detras del hueco
            push bc
            ld   hl,(gap_s)
            or   a
            sbc  hl,bc
            call count_lf
            ld   hl,(cur_line)
            or   a
            sbc  hl,de
            ld   (cur_line),hl
            pop  bc
            ld   hl,(gap_s)
            dec  hl
            ld   de,(gap_e)
            dec  de
            lddr
            inc  hl
            ld   (gap_s),hl
            inc  de
            ld   (gap_e),de
            ret
tg_right:   xor  a                  ; a la derecha: BC = destino - actual
            sub  l
            ld   c,a
            sbc  a,a
            sub  h
            ld   b,a
            push bc
            ld   hl,(gap_e)
            call count_lf
            ld   hl,(cur_line)
            add  hl,de
            ld   (cur_line),hl
            pop  bc
            ld   hl,(gap_e)
            ld   de,(gap_s)
            ldir
            ld   (gap_e),hl
            ld   (gap_s),de
            ret

; DE = LF que hay en los BC bytes desde HL
count_lf:   ld   de,0
            ld   a,b
            or   c
            ret  z
cl1:        ld   a,0Ah
            cpir
            ret  nz                 ; no hay mas
            inc  de
            ld   a,b
            or   c
            jr   nz,cl1
            ret

; HL = posicion del principio de la linea del cursor
cur_line_start:
            call tb_pos
            ld   a,h
            or   l
            ret  z
            ld   b,h
            ld   c,l
            ld   hl,(gap_s)
            dec  hl
            ld   a,0Ah
            cpdr
            jr   nz,cls0            ; no hay LF delante: linea 0
            inc  hl                 ; HL queda delante del LF: saltarlo
            inc  hl
            ld   de,BUF_START
            or   a
            sbc  hl,de
            ret
cls0:       ld   hl,0
            ret

; HL = posicion detras del siguiente LF desde el cursor, con CF; si no
; hay mas LF, NC y HL = tb_len
next_lf:    ld   hl,BUF_END
            ld   de,(gap_e)
            or   a
            sbc  hl,de
            ld   a,h
            or   l
            jr   z,nl_none
            ld   b,h
            ld   c,l
            ld   hl,(gap_e)
            ld   a,0Ah
            cpir
            jr   nz,nl_none
            ld   de,(gap_e)         ; HL - gap_e = distancia desde el cursor
            or   a
            sbc  hl,de
            push hl
            call tb_pos
            pop  de
            add  hl,de
            scf
            ret
nl_none:    call tb_len
            or   a
            ret

; DE = columna en pantalla del cursor (con los TAB expandidos)
cur_vcol:   call cur_line_start
            ld   bc,BUF_START
            add  hl,bc
            ld   de,0
cv1:        push hl
            ld   bc,(gap_s)
            or   a
            sbc  hl,bc
            pop  hl
            ret  z
            ld   a,(hl)
            inc  hl
            call vc_adv
            jr   cv1

; DE = columna detras del caracter A escrito en la columna DE
vc_adv:     cp   09h
            jr   z,va_tab
            inc  de
            ret
va_tab:     ld   a,e                ; siguiente multiplo de 8
            or   7
            ld   e,a
            inc  de
            ret

; --- recorrido secuencial (pintar y guardar) -------------------------
; it_set: HL = posicion; it_next: A = caracter y avanza (CF al final)
it_set:     ex   de,hl              ; DE = posicion
            ld   hl,(gap_s)
            ld   bc,BUF_START
            or   a
            sbc  hl,bc              ; HL = lo que hay delante del hueco
            ex   de,hl
            or   a
            sbc  hl,de
            jr   nc,is1
            add  hl,de              ; delante del hueco
            ld   bc,BUF_START
            add  hl,bc
            ld   (it_addr),hl
            ret
is1:        ld   de,(gap_e)         ; detras del hueco
            add  hl,de
            ld   (it_addr),hl
            ret

it_next:    ld   hl,(it_addr)
            ld   de,(gap_s)
            or   a
            sbc  hl,de
            jr   nz,in1
            ld   hl,(gap_e)         ; al llegar al hueco, se salta
            jr   in2
in1:        add  hl,de
in2:        ld   de,BUF_END
            or   a
            sbc  hl,de
            scf
            ret  z
            add  hl,de
            ld   a,(hl)
            inc  hl
            ld   (it_addr),hl
            or   a
            ret

; =====================================================================
;  Pantalla
; =====================================================================
; ventana: la linea del cursor entre top_line y top_line+TEXTH-1, y su
; columna entre left_col y left_col+SCRW-1
ensure_visible:
            ld   hl,(cur_line)
            ld   de,(top_line)
            or   a
            sbc  hl,de
            jr   c,ev_up
            ld   de,TEXTH
            or   a
            sbc  hl,de
            jr   c,ev_h
            inc  hl                 ; lineas que hay que bajar
            ld   b,h
            ld   c,l
ev_adv:     push bc
            call top_next_line
            pop  bc
            dec  bc
            ld   a,b
            or   c
            jr   nz,ev_adv
            jr   ev_h
ev_up:      call cur_line_start
            ld   (top_off),hl
            ld   hl,(cur_line)
            ld   (top_line),hl
ev_h:       call cur_vcol
            ld   hl,(left_col)
            ex   de,hl              ; HL = columna, DE = left_col
            or   a
            sbc  hl,de
            jr   c,ev_left
            ld   de,SCRW
            or   a
            sbc  hl,de
            ret  c
            call cur_vcol           ; se sale por la derecha
            ld   hl,-60
            add  hl,de
            ld   (left_col),hl
            ret
ev_left:    call cur_vcol           ; se sale por la izquierda
            ld   hl,-20
            add  hl,de
            jr   c,evl1
            ld   hl,0
evl1:       ld   (left_col),hl
            ret

top_next_line:
            ld   hl,(top_off)
            call it_set
tnl1:       call it_next
            ret  c
            ld   hl,(top_off)
            inc  hl
            ld   (top_off),hl
            cp   0Ah
            jr   nz,tnl1
            ld   hl,(top_line)
            inc  hl
            ld   (top_line),hl
            ret

; las TEXTH filas de texto desde top_off
redraw:     ld   hl,(top_off)
            call it_set
            xor  a
            ld   (rd_eof),a
            ld   hl,DFILE+1
            ld   b,TEXTH
rd_row:     push bc
            push hl
            ld   (rd_addr),hl
            ld   d,h
            ld   e,l
            inc  de
            ld   (hl),' '
            ld   bc,SCRW-1
            ldir
            ld   a,(rd_eof)
            or   a
            jr   nz,rd_next
            ld   hl,0
            ld   (rd_vcol),hl
rd_ch:      call it_next
            jr   c,rd_end
            cp   0Ah
            jr   z,rd_next
            cp   09h
            jr   z,rd_tab
            cp   ' '
            jr   nc,rd_put
            ld   a,CTRL_GLYPH
rd_put:     call rd_glyph
            jr   rd_ch
rd_tab:     ld   a,' '
            call rd_glyph
            ld   a,(rd_vcol)
            and  7
            jr   nz,rd_tab
            jr   rd_ch
rd_end:     ld   a,1
            ld   (rd_eof),a
rd_next:    pop  hl
            ld   de,ROWSTRIDE
            add  hl,de
            pop  bc
            djnz rd_row
            ret

; A en la columna rd_vcol de la fila, si cae dentro de la ventana
rd_glyph:   ld   c,a
            ld   hl,(rd_vcol)
            ld   de,(left_col)
            or   a
            sbc  hl,de
            jr   c,rg1
            ld   a,h
            or   a
            jr   nz,rg1
            ld   a,l
            cp   SCRW
            jr   nc,rg1
            ld   de,(rd_addr)
            add  hl,de
            ld   (hl),c
rg1:        ld   hl,(rd_vcol)
            inc  hl
            ld   (rd_vcol),hl
            ret

; linea de estado: nombre, * si hay cambios, linea y columna, ayuda; o
; el mensaje pendiente
draw_status:
            ld   hl,STATUS_ROW
            ld   d,h
            ld   e,l
            inc  de
            ld   (hl),' '
            ld   bc,SCRW-1
            ldir
            ld   hl,(msg_ptr)
            ld   a,h
            or   l
            jr   z,ds_norm
            ld   de,STATUS_ROW+1
            jp   put_str
ds_norm:    ld   de,STATUS_ROW+1
            ld   a,(name_len)
            or   a
            jr   z,ds_noname
            ld   b,a
            ld   hl,name_buf
ds_n1:      ld   a,(hl)
            ld   (de),a
            inc  hl
            inc  de
            djnz ds_n1
            jr   ds_mod
ds_noname:  ld   hl,msg_noname
            call put_str
ds_mod:     ld   a,(modified)
            or   a
            jr   z,ds_pos
            ld   a,'*'
            ld   (de),a
ds_pos:     ld   de,STATUS_ROW+32
            ld   hl,msg_line
            call put_str
            ld   hl,(cur_line)
            inc  hl
            call put_num
            ld   hl,msg_col
            call put_str
            push de
            call cur_vcol
            ex   de,hl
            inc  hl
            pop  de
            call put_num
            ld   de,STATUS_ROW+SCRW-25
            ld   hl,msg_help
            jp   put_str

; HL = texto terminado en 0 -> (DE); DE queda detras
put_str:    ld   a,(hl)
            or   a
            ret  z
            ld   (de),a
            inc  hl
            inc  de
            jr   put_str

; HL = numero -> (DE) en decimal, sin ceros delante; DE queda detras.
; Sin IX: la ROM lo necesita para el video en modo SLOW.
put_num:    ld   (pn_dst),de
            ld   de,pow10
            ld   (pn_ptr),de
            ld   c,0                ; C = ya ha salido una cifra
pn1:        ld   de,(pn_ptr)
            ld   a,(de)
            inc  de
            ld   b,a
            ld   a,(de)
            inc  de
            ld   (pn_ptr),de
            ld   d,a
            ld   e,b                ; DE = potencia de 10
            or   e
            jr   z,pn_end
            ld   a,'0'-1
pn2:        inc  a
            or   a
            sbc  hl,de
            jr   nc,pn2
            add  hl,de
            ld   b,a                ; B = cifra
            cp   '0'
            jr   nz,pn3
            ld   a,c
            or   a
            jr   nz,pn3
            ld   a,e                ; la de las unidades sale siempre
            dec  a
            or   d
            jr   nz,pn1
pn3:        push hl
            ld   hl,(pn_dst)
            ld   (hl),b
            inc  hl
            ld   (pn_dst),hl
            pop  hl
            ld   c,1
            jr   pn1
pn_end:     ld   de,(pn_dst)
            ret
pow10:      defw 10000,1000,100,10,1,0

; el cursor de la pantalla donde esta el del texto
place_cursor:
            ld   hl,(cur_line)
            ld   de,(top_line)
            or   a
            sbc  hl,de
            ld   a,l
            ld   (cur_row),a
            call cur_vcol
            ld   hl,(left_col)
            ex   de,hl
            or   a
            sbc  hl,de
            ld   a,l
            ld   (cur_col),a
            ret

; video en 80 columnas con la fuente CP437, como TELNET
video_on:   ld   hl,FONT_ADDR       ; fuente: codigos 0-31 a cero (no se
            ld   de,FONT_ADDR+1     ; imprimen) y 32-255 de font.inc
            ld   bc,255
            ld   (hl),0
            ldir
            ld   hl,font_src
            ld   de,FONT_ADDR+100h
            ld   bc,FONT_LEN
            ldir
            ld   hl,arrow_glyphs    ; 1-4: flechas de la pantalla de ayuda
            ld   de,FONT_ADDR+8
            ld   bc,4*8
            ldir
            ld   hl,caret_glyph     ; '^': la fuente lo trae como flecha
            ld   de,FONT_ADDR+'^'*8 ; hacia arriba, y se confundiria con
            ld   bc,8               ; la de SHIFT+7
            ldir
            ld   a,FONT_I
            ld   i,a
            ld   hl,DFILE           ; pantalla en blanco
            ld   (hl),' '
            ld   de,DFILE+1
            ld   bc,ROWSTRIDE*SCRH
            ldir
            call attr_init
            ld   hl,DFILE           ; pantalla y atributos alternativos
            ld   (2096),hl
            ld   a,170
            ld   (2098),a
            ld   hl,ATTR
            ld   (2059),hl
            ld   a,170
            ld   (2061),a
            ld   bc,CHROMA_PORT
            ld   a,CHROMA_ON
            out  (c),a
            xor  a
            ld   (2094),a           ; sin desplazar ni recortar
            ld   (2095),a
            ld   a,174              ; Superfast texto, 80 columnas
            ld   (2045),a
            ld   a,CMD_256C
            jp   mcu_send

; atributos del editor: el texto y la fila de estado
attr_init:  ld   hl,ATTR
            ld   (hl),TEXT_ATTR
            ld   de,ATTR+1
            ld   bc,ROWSTRIDE*TEXTH
            ldir
            ld   hl,ATTR+1+ROWSTRIDE*TEXTH
            ld   (hl),STAT_ATTR
            ld   de,ATTR+2+ROWSTRIDE*TEXTH
            ld   bc,ROWSTRIDE-1
            ldir
            ret

; =====================================================================
;  Pantalla de ayuda (ENTER+9, ^J): el teclado del ZX81 con lo que hace
;  cada tecla sola, con SHIFT y con ENTER, y las ordenes de WordStar.
;  Cualquier tecla vuelve; el bucle principal repinta el texto.
; =====================================================================
k_help:     ld   hl,DFILE           ; todo en blanco
            ld   (hl),' '
            ld   de,DFILE+1
            ld   bc,ROWSTRIDE*SCRH
            ldir
            ld   hl,ATTR
            ld   (hl),TEXT_ATTR
            ld   de,ATTR+1
            ld   bc,ROWSTRIDE*SCRH
            ldir
            ld   de,0               ; titulo, como la fila de estado
            ld   b,SCRW
            ld   c,STAT_ATTR
            call fill_attr

            ld   hl,key_tab         ; las 40 teclas: 4 filas de 10
            ld   d,2
kh_row:     ld   e,0
kh_key:     ld   b,7                ; fondo de la tecla, 7x3
            ld   c,KEY_ATTR
            call fill_attr
            inc  d
            call fill_attr
            inc  d
            call fill_attr
            dec  d
            dec  d
            inc  e                  ; y sus tres textos
            ld   c,KEYCAP_ATTR
            call put5
            inc  d
            ld   c,KEYSH_ATTR
            call put5
            inc  d
            ld   c,KEYEN_ATTR
            call put5
            dec  d
            dec  d
            ld   a,e
            add  a,7
            ld   e,a
            cp   SCRW
            jr   c,kh_key
            ld   a,d
            add  a,4
            ld   d,a
            cp   2+4*4
            jr   c,kh_row

            ld   hl,kh_lines        ; titulo, leyenda y ordenes
kh_l1:      ld   a,(hl)
            cp   0FFh
            jr   z,kh_wait
            ld   d,a
            inc  hl
            ld   e,(hl)
            inc  hl
            ld   c,(hl)
            inc  hl
kh_l2:      ld   a,(hl)
            inc  hl
            or   a
            jr   z,kh_l1
            call put_ch
            inc  e
            jr   kh_l2

kh_wait:    call scan_keys          ; que se suelte la que la ha abierto,
            cp   0FFh               ; esperar otra y que se suelte
            jr   nz,kh_wait
            ld   a,e
            and  3
            jr   nz,kh_wait
kh_w2:      call scan_keys
            cp   0FFh
            jr   nz,kh_w3
            ld   a,e
            and  3
            jr   z,kh_w2
kh_w3:      call scan_keys
            cp   0FFh
            jr   nz,kh_w3
            ld   a,e
            and  3
            jr   nz,kh_w3
            jp   attr_init

; fill_attr: D=fila, E=columna, B=ancho, C=atributo (conserva BC, DE y
; HL: el que llama recorre la tabla de teclas con HL)
fill_attr:  push hl
            push bc
            push de
            call rc_addr
            ld   de,ATTR_OFF
            add  hl,de
fa1:        ld   (hl),c
            inc  hl
            djnz fa1
            pop  de
            pop  bc
            pop  hl
            ret

; put5: 5 caracteres de (HL) en D,E con el atributo C; HL avanza 5
put5:       push de
            ld   b,5
p5a:        ld   a,(hl)
            inc  hl
            call put_ch
            inc  e
            djnz p5a
            pop  de
            ret

; put_ch: A en D,E con el atributo C (conserva BC, DE y HL)
put_ch:     push hl
            push af
            call rc_addr
            pop  af
            ld   (hl),a
            push de
            ld   de,ATTR_OFF
            add  hl,de
            ld   (hl),c
            pop  de
            pop  hl
            ret

; rc_addr: D=fila, E=columna -> HL (conserva BC y DE)
rc_addr:    ld   a,d
            ld   (cur_row),a
            ld   a,e
            ld   (cur_col),a
            jp   char_addr

; =====================================================================
;  Salida: video y memoria como estaban, y al BASIC
; =====================================================================
quit:       ld   sp,(sv_sp)
qt_rel:     call scan_keys          ; esperar a que se suelte todo: con
            cp   0FFh               ; ENTER+0 el 0 se suelta antes que el
            jr   nz,qt_rel          ; ENTER, y el explorador tomaria ese
            ld   a,e                ; ENTER como "abrir el archivo"
            and  3
            jr   nz,qt_rel
            ld   a,85               ; video nativo (apaga tambien la pantalla
            ld   (2045),a           ; y los atributos alternativos)
            ld   bc,CHROMA_PORT
            ld   a,CHROMA_OFF
            out  (c),a
            ld   a,(sv_i)           ; el modo de caracteres de antes
            cp   3Ch
            ld   b,CMD_128C
            jr   z,qt1
            cp   38h
            ld   b,CMD_256C
            jr   z,qt1
            ld   b,CMD_64C
qt1:        ld   a,b
            call mcu_send
            ld   a,(sv_i)
            ld   i,a
            ld   a,6                ; bloques 6 y 7 a su pagina
            ld   e,6
            call mcu_map
            ld   a,7
            ld   e,7
            call mcu_map
            call SLOW_FAST
            ld   bc,0
            ret

; =====================================================================
;  Ficheros
; =====================================================================
; F$ -> name_buf (en ASCII). Sin F$, o vacia, name_len = 0.
find_fvar:  ld   hl,(SV_VARS)
fv1:        ld   a,(hl)
            cp   80h                ; fin de las variables
            ret  z
            ld   c,a
            and  0E0h
            cp   040h               ; 010: cadena
            jr   z,fv_str
            cp   060h               ; 011: numero de una letra
            ld   de,6
            jr   z,fv_skip
            cp   0E0h               ; 111: variable de FOR
            ld   de,18
            jr   z,fv_skip
            cp   0A0h               ; 101: numero de nombre largo
            jr   z,fv_long
fv_len:     inc  hl                 ; matrices y cadenas: longitud y datos
            ld   e,(hl)
            inc  hl
            ld   d,(hl)
            inc  hl
fv_skip:    add  hl,de
            jr   fv1
fv_long:    inc  hl                 ; letras hasta la que lleva el bit 7
fvl1:       ld   a,(hl)
            inc  hl
            bit  7,a
            jr   z,fvl1
            ld   de,5
            jr   fv_skip
fv_str:     ld   a,c
            cp   FVAR
            jr   nz,fv_len
            inc  hl                 ; F$: longitud y texto
            ld   c,(hl)
            inc  hl
            ld   b,(hl)
            inc  hl
            ld   a,b                ; mas de 63 caracteres: se corta
            or   a
            jr   nz,fvs1
            ld   a,c
            cp   64
            jr   c,fvs2
fvs1:       ld   c,63
fvs2:       ld   a,c
            ld   (name_len),a
            or   a
            ret  z
            ld   b,c
            ld   de,name_buf
fvs3:       ld   a,(hl)             ; codigo ZX81 -> ASCII
            and  03Fh
            push hl
            ld   hl,zx2ascii
            add  a,l
            ld   l,a
            jr   nc,fvs4
            inc  h
fvs4:       ld   a,(hl)
            pop  hl
            ld   (de),a
            inc  hl
            inc  de
            djnz fvs3
            ret

zx2ascii:   defb " ??????????",022h,"#$:?()><=+-*/;,.0123456789"
            defb "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

; carga el fichero de name_buf; si no existe, texto vacio
load_file:  ld   a,(name_len)
            or   a
            ret  z
            ld   a,CMD_FOPEN
            call f_name_cmd
            cp   0FFh
            jr   nz,lf1
            ld   hl,msg_new
            ld   (msg_ptr),hl
            ret
lf1:        ld   (handle),a
            call f_stat             ; fsize (4 bytes)
            ld   hl,(fsize+2)       ; 64 KB o mas: no cabe seguro
            ld   a,h
            or   l
            jr   nz,lf_big
            ld   hl,(fsize)
            ld   de,BUF_SIZE+1
            or   a
            sbc  hl,de
            jr   c,lf_read
lf_big:     ld   a,(handle)
            call f_close
            ld   hl,msg_big
            call put_msg_wait
            jp   quit
lf_read:    xor  a
            ld   (pend_cr),a
lf_chunk:   ld   hl,(fsize)         ; quedan fsize bytes
            ld   a,h
            or   l
            jr   z,lf_done
            ld   de,CHUNK
            or   a
            sbc  hl,de
            jr   nc,lf2
            add  hl,de              ; menos de CHUNK: todo
            ex   de,hl
            ld   hl,0
lf2:        ld   (fsize),hl         ; DE = los de este trozo
            push de
            ld   a,(handle)
            call f_read             ; -> stage
            pop  bc
            ld   hl,stage
lf3:        ld   a,(hl)
            inc  hl
            push hl
            push bc
            call lf_byte
            pop  bc
            pop  hl
            dec  bc
            ld   a,b
            or   c
            jr   nz,lf3
            jr   lf_chunk
lf_done:    ld   a,(pend_cr)
            or   a
            ld   a,0Dh
            call nz,tb_insert
            ld   a,(handle)
            call f_close
            ld   hl,0               ; al principio, sin cambios
            call tb_goto
            xor  a
            ld   (modified),a
            ret

; un byte del fichero al texto: CRLF -> LF (y se recuerda para guardar)
lf_byte:    cp   0Dh
            jr   z,lb_cr
            cp   0Ah
            jr   z,lb_lf
            push af
            call lb_flush
            pop  af
            jp   tb_insert
lb_cr:      call lb_flush           ; un CR suelto se queda como esta
            ld   a,1
            ld   (pend_cr),a
            ret
lb_lf:      ld   a,(pend_cr)
            or   a
            jr   z,lb_lf1
            xor  a
            ld   (pend_cr),a
            inc  a
            ld   (crlf),a
lb_lf1:     ld   a,0Ah
            jp   tb_insert
lb_flush:   ld   a,(pend_cr)
            or   a
            ret  z
            xor  a
            ld   (pend_cr),a
            ld   a,0Dh
            jp   tb_insert

; ^K S. CF si no se ha guardado.
do_save:    ld   a,(name_len)
            or   a
            jr   nz,sv1
            call ask_name
            ret  c
sv1:        ld   a,CMD_FCREATE
            call f_name_cmd
            cp   0FFh
            jr   nz,sv2
            ld   hl,msg_nocreate
            ld   (msg_ptr),hl
            scf
            ret
sv2:        ld   (handle),a
            xor  a
            ld   (save_err),a
            ld   hl,0
            ld   (st_cnt),hl
            call it_set
sv3:        call it_next
            jr   c,sv5
            cp   0Ah
            jr   nz,sv4
            ld   a,(crlf)
            or   a
            jr   z,sv_lf
            ld   a,0Dh
            call st_put
sv_lf:      ld   a,0Ah
sv4:        call st_put
            jr   sv3
sv5:        call st_flush
            ld   a,(handle)
            call f_close
            ld   a,(save_err)
            or   a
            jr   z,sv6
            ld   hl,msg_wrerr
            ld   (msg_ptr),hl
            scf
            ret
sv6:        ld   (modified),a
            ld   hl,msg_saved
            ld   (msg_ptr),hl
            or   a
            ret

; A -> stage; se escribe al llenarse
st_put:     ld   hl,(st_cnt)
            ld   de,stage
            add  hl,de
            ld   (hl),a
            ld   hl,(st_cnt)
            inc  hl
            ld   (st_cnt),hl
            ld   de,CHUNK
            or   a
            sbc  hl,de
            ret  c
st_flush:   ld   hl,(st_cnt)
            ld   a,h
            or   l
            ret  z
            ld   a,(handle)
            call f_write
            or   a
            jr   z,stf1
            ld   (save_err),a
stf1:       ld   hl,0
            ld   (st_cnt),hl
            ret

; --- comandos de ficheros del MCU ------------------------------------
; A = CMD_FOPEN o CMD_FCREATE con el nombre de name_buf -> A = handle
; (0FFh si no se puede)
f_name_cmd: call mcu_send
            ld   a,(name_len)
            ld   b,a
            call mcu_send
            ld   hl,name_buf
fnc1:       ld   a,(hl)
            inc  hl
            call mcu_send
            djnz fnc1
            jp   mcu_recv

; A = handle -> fsize (4 bytes); fecha y hora se descartan
f_stat:     ld   c,a
            ld   a,CMD_FSTAT
            call mcu_send
            ld   a,c
            call mcu_send
            ld   hl,fsize
            ld   b,4
fst1:       call mcu_recv
            ld   (hl),a
            inc  hl
            djnz fst1
            ld   b,4
fst2:       call mcu_recv
            djnz fst2
            jp   mcu_recv           ; estado

; A = handle, DE = cuantos (<= CHUNK) -> stage; A = estado
f_read:     ld   c,a
            ld   a,CMD_FREAD
            call mcu_send
            ld   a,c
            call mcu_send
            ld   a,e
            call mcu_send
            ld   a,d
            call mcu_send
            ld   hl,stage
fr1:        call mcu_recv
            ld   (hl),a
            inc  hl
            dec  de
            ld   a,d
            or   e
            jr   nz,fr1
            jp   mcu_recv

; A = handle, HL = cuantos de stage (<= CHUNK) -> A = estado
f_write:    ld   c,a
            ld   a,CMD_FWRITE
            call mcu_send
            ld   a,c
            call mcu_send
            ld   a,l
            call mcu_send
            ld   a,h
            call mcu_send
            ex   de,hl              ; DE = cuantos
            ld   hl,stage
fw1:        ld   a,(hl)
            inc  hl
            call mcu_send
            dec  de
            ld   a,d
            or   e
            jr   nz,fw1
            jp   mcu_recv

; A = handle -> A = estado
f_close:    ld   c,a
            ld   a,CMD_FCLOSE
            call mcu_send
            ld   a,c
            call mcu_send
            jp   mcu_recv

; handshake como el del explorador: cada llamada mira el reloj, y BC,
; DE y HL se conservan (A no sobrevive a mcu_send)
mcu_send:   push bc
            ld   b,a
            in   a,(ClkPort)
            ld   c,a
            ld   a,b
            out  (DataPort),a
msw:        in   a,(ClkPort)
            xor  c
            jp   p,msw
            pop  bc
            ret

mcu_recv:   push bc
            in   a,(ClkPort)
            ld   c,a
            in   a,(DataPort)
            ld   b,a
mrw:        in   a,(ClkPort)
            xor  c
            jp   p,mrw
            ld   a,b
            pop  bc
            ret

; A = bloque (6 o 7), E = pagina. Como el mcu_map del explorador: en
; paginacion simple la pagina va en los bits 7-3 del dato, y en la
; completa en B (la parte alta del puerto)
mcu_map:    push bc
            and  7
            ld   c,a
            ld   a,e
            and  31
            rlca
            rlca
            rlca
            or   c
            ld   b,e
            ld   c,MAP_PORT
            out  (c),a
            pop  bc
            ret

; =====================================================================
;  Preguntas en la linea de estado
; =====================================================================
; HL = mensaje; espera una tecla
put_msg_wait:
            ld   (msg_ptr),hl
            call draw_status
            ld   a,TEXTH
            ld   (cur_row),a
            ld   a,SCRW-1
            ld   (cur_col),a
            jp   kbget

; HL = pregunta (Y/N). Z = Y, NZ = N, CF = ESC
ask_yn:     ld   (msg_ptr),hl
            call draw_status
            ld   hl,0
            ld   (msg_ptr),hl
            ld   a,TEXTH
            ld   (cur_row),a
            ld   a,SCRW-1
            ld   (cur_col),a
ayn1:       call kbget
            cp   K_ESC
            scf
            ret  z
            and  0DFh
            cp   'Y'
            ret  z
            cp   'N'
            jr   nz,ayn1
            or   a                  ; NZ, NC
            ret

; nombre para guardar, en la linea de estado. CF si se cancela.
ask_name:   xor  a
            ld   (name_len),a
an1:        ld   hl,STATUS_ROW      ; "SAVE AS: " y lo que se lleva escrito
            ld   d,h
            ld   e,l
            inc  de
            ld   (hl),' '
            ld   bc,SCRW-1
            ldir
            ld   de,STATUS_ROW+1
            ld   hl,msg_saveas
            call put_str
            ld   a,(name_len)
            or   a
            jr   z,an2
            ld   b,a
            ld   hl,name_buf
an3:        ld   a,(hl)
            ld   (de),a
            inc  hl
            inc  de
            djnz an3
an2:        ld   a,TEXTH
            ld   (cur_row),a
            ld   a,(name_len)
            add  a,10               ; detras de " SAVE AS: "
            ld   (cur_col),a
            call kbget
            cp   K_ESC
            jr   z,an_esc
            cp   0Dh
            jr   z,an_ok
            cp   08h
            jr   z,an_bs
            cp   07Fh
            jr   z,an_bs
            cp   ' '+1              ; ni espacios ni controles
            jr   c,an1
            cp   07Fh
            jr   nc,an1
            ld   c,a
            ld   a,(name_len)
            cp   63
            jr   nc,an1
            ld   e,a
            ld   d,0
            ld   hl,name_buf
            add  hl,de
            ld   (hl),c
            inc  a
            ld   (name_len),a
            jr   an1
an_bs:      ld   a,(name_len)
            or   a
            jr   z,an1
            dec  a
            ld   (name_len),a
            jr   an1
an_ok:      ld   a,(name_len)
            or   a
            jr   z,an1              ; sin nombre no se puede
            ret                     ; NC
an_esc:     xor  a
            ld   (name_len),a
            scf
            ret

; =====================================================================
;  Teclado y cursor: los de TELNET (chario.z80 de CPM3_SD81), con
;  repeticion automatica al mantener una tecla
; =====================================================================
kbget:
ck_top:     ld   a,(ctrl_prefix)
            or   a
            jr   nz,ck_pwait        ; modo CTRL armado -> esperar la tecla
            xor  a
            ld   (mods_seen),a      ; modo normal: reiniciar mods vistos
ck_nwait:   call wait_scan          ; A=idx/FF ; E: bit0=SHIFT bit1=ENTER
            cp   0FFh
            jr   nz,ck_key          ; hay tecla -> procesar
            xor  a                  ; nada pulsado: se acabo la repeticion
            ld   (rep_idx),a
            ld   a,(mods_seen)      ; sin tecla: acumular modificadores vistos
            or   e
            ld   (mods_seen),a
            ld   a,e
            and  3
            jr   nz,ck_nwait        ; algun modificador aun pulsado -> seguir
            ld   a,(mods_seen)      ; todo suelto: decidir segun lo visto
            and  3
            jr   z,ck_clr           ; sin modificadores
            cp   3
            jr   z,ck_arm           ; SHIFT+ENTER -> armar modo CTRL
            bit  1,a
            jr   z,ck_clr           ; solo SHIFT -> nada
            ld   a,(enter_used)     ; solo ENTER
            or   a
            jr   nz,ck_clr          ; ENTER ya genero simbolo -> no CR
            call cursor_clear
            ld   a,0Dh              ; ENTER solo -> CR
            ret

ck_clr:     xor  a
            ld   (enter_used),a
            jr   ck_top
ck_arm:     call cursor_clear
            ld   a,1
            ld   (ctrl_prefix),a    ; modo CTRL (cursor pasa a subrayado)
            jr   ck_top
; --- modo CTRL armado: la siguiente tecla -> CTRL+letra ---
ck_pwait:   call wait_scan
            cp   0FFh
            jr   z,ck_pwait
            push af
            call cursor_clear
            pop  af
            ld   (km_idx),a
ck_prel:    call scan_keys          ; esperar a soltar la tecla
            cp   0FFh
            jr   nz,ck_prel
            xor  a
            ld   (ctrl_prefix),a    ; desarmar modo CTRL
            ld   hl,keymap
            ld   a,(km_idx)
            ld   c,a
            ld   b,0
            add  hl,bc
            ld   a,(hl)
            and  01Fh               ; CTRL = letra AND $1F
            ret
; --- tecla con modificadores (SHIFT / ENTER / ambos) ---
ck_key:     push af
            call cursor_clear
            pop  af
            ld   (km_idx),a
            ld   a,e
            ld   (km_mods),a
            ld   a,(rep_idx)        ; la misma que se estaba repitiendo?
            ld   b,a
            ld   a,(km_idx)
            cp   b
            ld   a,REP_DELAY
            jr   nz,ck_r0
            ld   a,REP_RATE
ck_r0:      ld   (rep_lim),a
            xor  a
            ld   (rep_cnt),a
            in   a,(ClkPort)        ; empezar a contar VSYNC desde ahora
ck_rel:     call scan_keys          ; esperar a soltar la tecla; ACUMULAR mods
            push af
            ld   a,(km_mods)
            or   e
            ld   (km_mods),a
            pop  af
            cp   0FFh
            jr   z,ck_up
            in   a,(ClkPort)        ; sigue pulsada: contar VSYNC
            and  07Eh
            rrca
            ld   hl,rep_cnt
            add  a,(hl)
            ld   (hl),a
            ld   hl,rep_lim
            cp   (hl)
            jr   c,ck_rel
            ld   a,(km_idx)         ; tiempo cumplido: repetir
            ld   (rep_idx),a
            jr   ck_emit
ck_up:      xor  a
            ld   (rep_idx),a
ck_emit:    ld   a,(km_mods)
            cp   3                  ; SHIFT+ENTER+tecla directo -> CTRL
            jr   z,ck_ctrl
            ld   hl,keymap
            bit  1,a                ; ENTER -> simbolos
            jr   z,ck_nse
            ld   hl,keymap_sym
            jr   ck_tbl
ck_nse:     bit  0,a                ; SHIFT -> mayusculas/funciones
            jr   z,ck_tbl
            ld   hl,keymap_shift
ck_tbl:     ld   a,(km_idx)
            ld   c,a
            ld   b,0
            add  hl,bc
            ld   a,(hl)
            or   a
            jp   z,ck_top           ; sin ASCII -> reesperar
            jr   ck_mods
ck_ctrl:    ld   hl,keymap
            ld   a,(km_idx)
            ld   c,a
            ld   b,0
            add  hl,bc
            ld   a,(hl)
            and  01Fh
ck_mods:    ld   c,a                ; codigo a devolver
            ld   a,(km_mods)        ; si ENTER fue modificador, marcar consumido
            bit  1,a
            jr   z,ck_ret
            ld   a,1
            ld   (enter_used),a
ck_ret:     ld   a,c
            ret

scan_keys:  ; A=indice (1..39) o 0FFh ; E: bit0=SHIFT, bit1=ENTER
            ld   bc,0FEFEh          ; SHIFT (A8 D0)
            in   a,(c)
            ld   e,0
            rra
            jr   c,sk_r0
            inc  e                  ; E bit0 = SHIFT
sk_r0:      or   0F0h               ; A8: D0=Z D1=X D2=C D3=V
            ld   d,1
            ld   c,4
sk_r0b:     rra
            jr   nc,sk_done
            inc  d
            dec  c
            jr   nz,sk_r0b
            ld   b,0BFh             ; ENTER (A14 D0)
            ld   c,0FEh
            in   a,(c)
            bit  0,a
            jr   nz,sk_e0
            set  1,e                ; E bit1 = ENTER
sk_e0:      ld   hl,row_lines+1     ; filas A9..A15
            ld   d,5
sk_row:     ld   a,(hl)
            or   a
            jr   z,sk_no
            ld   b,a
            ld   c,0FEh
            in   a,(c)
            or   0E0h
            bit  6,b                ; A14 (0BFh): bit6=0 -> ignorar D0 (ENTER)
            jr   nz,sk_nm
            or   1
sk_nm:      ld   c,5
sk_rb:      rra
            jr   nc,sk_done
            inc  d
            dec  c
            jr   nz,sk_rb
            inc  hl
            jr   sk_row
sk_no:      ld   a,0FFh
            ret
sk_done:    ld   a,d
            ret

wait_scan:  call scan_keys          ; hace avanzar el parpadeo si no hay tecla
            cp   0FFh
            ret  nz
            push de
            call cursor_blink_poll
            pop  de
            ld   a,0FFh
            ret

cursor_clear:                       ; si el cursor esta dibujado, borrarlo
            ld   a,(cursor_on)
            or   a
            ret  z
            jp   cursor_toggle

cursor_toggle:                      ; alterna entre el caracter real y el cursor
            call char_addr
            ld   a,(cursor_on)
            or   a
            jr   nz,curt_hide
            ld   a,(hl)
            ld   (cursor_save),a
            ld   a,(ctrl_prefix)
            or   a
            ld   a,CURSOR_BLOCK
            jr   z,curt_put
            ld   a,CURSOR_UNDER     ; modo CTRL: cursor de subrayado
curt_put:   ld   (hl),a
            ld   a,1
            ld   (cursor_on),a
            ret
curt_hide:  ld   a,(cursor_save)
            ld   (hl),a
            xor  a
            ld   (cursor_on),a
            ret

cursor_blink_poll:
            in   a,(ClkPort)
            and  07Eh               ; bits 6..1 = VSYNC desde la ultima lectura
            rrca
            or   a
            ret  z
            ld   c,a
            ld   hl,blink
            ld   a,(hl)
            add  a,c
            cp   BLINK_RATE
            jr   c,cbp_store
            sub  BLINK_RATE
            ld   (hl),a
            jp   cursor_toggle
cbp_store:  ld   (hl),a
            ret

; HL = DFILE+1+cur_row*ROWSTRIDE+cur_col (no toca BC ni DE)
char_addr:  push bc
            push de
            ld   a,(cur_row)
            ld   l,a
            ld   h,0
            ld   d,h
            ld   e,l
            add  hl,hl              ; *81 = *64 + *16 + *1
            add  hl,hl
            add  hl,hl
            add  hl,hl
            push hl                 ; *16
            add  hl,hl
            add  hl,hl              ; *64
            pop  bc
            add  hl,bc
            add  hl,de
            ld   de,DFILE+1
            add  hl,de
            ld   a,(cur_col)
            ld   e,a
            ld   d,0
            add  hl,de
            pop  de
            pop  bc
            ret

row_lines:  defb 0FEh,0FDh,0FBh,0F7h,0EFh,0DFh,0BFh,07Fh,000h
keymap:     defb 000h,'z','x','c','v'      ; A8 : SHIFT Z X C V (base)
            defb 'a','s','d','f','g'       ; A9
            defb 'q','w','e','r','t'       ; A10
            defb '1','2','3','4','5'       ; A11
            defb '0','9','8','7','6'       ; A12
            defb 'p','o','i','u','y'       ; A13
            defb 00Dh,'l','k','j','h'      ; A14
            defb ' ','.','m','n','b'       ; A15

keymap_shift: ; SHIFT: mayusculas + funciones (0=DEL, 9=BS, 5/6/7/8=flechas)
            defb 000h,'Z','X','C','V'      ; A8
            defb 'A','S','D','F','G'       ; A9
            defb 'Q','W','E','R','T'       ; A10
            defb 01Bh,000h,000h,000h,KEY_LEFT       ; A11: 1=ESC, 5=izquierda
            defb 07Fh,008h,KEY_RIGHT,KEY_UP,KEY_DOWN ; A12: 0=DEL,9=BS,8=der,7=arr,6=abj
            defb 'P','O','I','U','Y'       ; A13
            defb 00Dh,'L','K','J','H'      ; A14
            defb ' ',009h,'M','N','B'      ; A15: .=TAB

keymap_sym:  ; ENTER+tecla: simbolos serigrafiados y los que faltan
            defb 000h,':',';','?','/'      ; A8
            defb 000h,05Ch,000h,07Eh,060h  ; A9 : S=\ F=~ G=`
            defb 027h,07Bh,07Dh,05Bh,05Fh  ; A10: Q=' W={ E=} R=[ T=_
            defb '!',040h,'#',07Ch,'%'     ; A11: 1=! 2=@ 3=# 4=| 5=%
            defb 01Dh,01Ch,000h,000h,'&'   ; A12: 0=^] (salir) 9=^\ (ayuda) 6=&
            defb 022h,')','(','$',05Dh     ; A13: P=" O=) I=( U=$ Y=]
            defb 00Dh,'=','+','-',05Eh     ; A14: L== K=+ J=- H=^
            defb ' ',',','>','<','*'       ; A15: .=, M=> N=< B=*

; ---------------------------------------------------------------------
;  Textos
; ---------------------------------------------------------------------
msg_noname: defb "(new file)",0
msg_line:   defb "Line ",0
msg_col:    defb "  Col ",0
msg_help:   defb "ENTER+9: keys & commands",0
msg_kpre:   defb "^K  S=save  X=save and exit  Q=quit",0
msg_qpre:   defb "^Q  S=line start  D=line end  R=top  C=bottom",0
msg_new:    defb "New file",0
msg_saved:  defb "Saved",0
msg_full:   defb "Text full (26623 bytes max)",0
msg_big:    defb "File too big: this version edits up to 26623 bytes. Press a key",0
msg_nocreate: defb "Can't create the file",0
msg_wrerr:  defb "Write error: the file may be incomplete",0
msg_askq:   defb "The text has changes. Save them? (Y/N, ESC = go on editing)",0
msg_saveas: defb "Save as: ",0

; pantalla de ayuda: flechas (codigos 1-4 de la fuente)
arrow_glyphs:
            defb 000h,010h,030h,07Eh,030h,010h,000h,000h   ; GL_LEFT
            defb 000h,008h,00Ch,07Eh,00Ch,008h,000h,000h   ; GL_RIGHT
            defb 000h,010h,038h,07Ch,010h,010h,010h,000h   ; GL_UP
            defb 000h,010h,010h,010h,07Ch,038h,010h,000h   ; GL_DOWN
caret_glyph: defb 000h,010h,028h,044h,000h,000h,000h,000h  ; '^'

; las 40 teclas, por filas: la tecla, con SHIFT y con ENTER (5 bytes cada uno)
key_tab:
            defb '1',' ',' ',' ',' ','E','S','C',' ',' ','!',' ',' ',' ',' '
            defb '2',' ',' ',' ',' ',' ',' ',' ',' ',' ','@',' ',' ',' ',' '
            defb '3',' ',' ',' ',' ',' ',' ',' ',' ',' ','#',' ',' ',' ',' '
            defb '4',' ',' ',' ',' ',' ',' ',' ',' ',' ','|',' ',' ',' ',' '
            defb '5',' ',' ',' ',' ',GL_LEFT,' ',' ',' ',' ','%',' ',' ',' ',' '
            defb '6',' ',' ',' ',' ',GL_DOWN,' ',' ',' ',' ','&',' ',' ',' ',' '
            defb '7',' ',' ',' ',' ',GL_UP,' ',' ',' ',' ',' ',' ',' ',' ',' '
            defb '8',' ',' ',' ',' ',GL_RIGHT,' ',' ',' ',' ',' ',' ',' ',' ',' '
            defb '9',' ',' ',' ',' ','D','E','L',' ',' ','K','E','Y','S',' '
            defb '0',' ',' ',' ',' ','D','E','L',' ',' ','Q','U','I','T',' '
            defb 'Q',' ',' ',' ',' ',' ',' ',' ',' ',' ',027h,' ',' ',' ',' '
            defb 'W',' ',' ',' ',' ',' ',' ',' ',' ',' ','{',' ',' ',' ',' '
            defb 'E',' ',' ',' ',' ',' ',' ',' ',' ',' ','}',' ',' ',' ',' '
            defb 'R',' ',' ',' ',' ',' ',' ',' ',' ',' ','[',' ',' ',' ',' '
            defb 'T',' ',' ',' ',' ',' ',' ',' ',' ',' ','_',' ',' ',' ',' '
            defb 'Y',' ',' ',' ',' ',' ',' ',' ',' ',' ',']',' ',' ',' ',' '
            defb 'U',' ',' ',' ',' ',' ',' ',' ',' ',' ','$',' ',' ',' ',' '
            defb 'I',' ',' ',' ',' ',' ',' ',' ',' ',' ','(',' ',' ',' ',' '
            defb 'O',' ',' ',' ',' ',' ',' ',' ',' ',' ',')',' ',' ',' ',' '
            defb 'P',' ',' ',' ',' ',' ',' ',' ',' ',' ',022h,' ',' ',' ',' '
            defb 'A',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '
            defb 'S',' ',' ',' ',' ',' ',' ',' ',' ',' ',05Ch,' ',' ',' ',' '
            defb 'D',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '
            defb 'F',' ',' ',' ',' ',' ',' ',' ',' ',' ','~',' ',' ',' ',' '
            defb 'G',' ',' ',' ',' ',' ',' ',' ',' ',' ','`',' ',' ',' ',' '
            defb 'H',' ',' ',' ',' ',' ',' ',' ',' ',' ','^',' ',' ',' ',' '
            defb 'J',' ',' ',' ',' ',' ',' ',' ',' ',' ','-',' ',' ',' ',' '
            defb 'K',' ',' ',' ',' ',' ',' ',' ',' ',' ','+',' ',' ',' ',' '
            defb 'L',' ',' ',' ',' ',' ',' ',' ',' ',' ','=',' ',' ',' ',' '
            defb 'E','N','T','E','R','C','T','R','L',' ',' ',' ',' ',' ',' '
            defb 'S','H','I','F','T',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '
            defb 'Z',' ',' ',' ',' ',' ',' ',' ',' ',' ',':',' ',' ',' ',' '
            defb 'X',' ',' ',' ',' ',' ',' ',' ',' ',' ',';',' ',' ',' ',' '
            defb 'C',' ',' ',' ',' ',' ',' ',' ',' ',' ','?',' ',' ',' ',' '
            defb 'V',' ',' ',' ',' ',' ',' ',' ',' ',' ','/',' ',' ',' ',' '
            defb 'B',' ',' ',' ',' ',' ',' ',' ',' ',' ','*',' ',' ',' ',' '
            defb 'N',' ',' ',' ',' ',' ',' ',' ',' ',' ','<',' ',' ',' ',' '
            defb 'M',' ',' ',' ',' ',' ',' ',' ',' ',' ','>',' ',' ',' ',' '
            defb '.',' ',' ',' ',' ','T','A','B',' ',' ',',',' ',' ',' ',' '
            defb 'S','P','A','C','E',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '

; titulo, leyenda y ordenes: fila, columna, atributo, texto, 0; 0FFh al final
kh_lines:
            defb 0,1,STAT_ATTR,"EDIT - keys and commands (ENTER+9 or ^J)                        any key: back",0
            defb 18,1,00Fh,"key alone = lowercase",0
            defb 18,26,00Eh,"yellow = with SHIFT (and capitals)",0
            defb 18,63,00Dh,"cyan = with ENTER",0
            defb 19,1,00Fh,"^S ^D ^E ^X  cursor (or SHIFT+5 8 7 6)   ^A ^F  word left / right",0
            defb 20,1,00Fh,"^R ^C        page up / down              ^Q S  ^Q D  start / end of the line",0
            defb 21,1,00Fh,"^G  delete right   ^Y  delete the line   ^Q R  ^Q C  start / end of the text",0
            defb 22,1,00Fh,"^K S  save   ^K X  save and exit   ^K Q  quit (asks if there are changes)",0
            defb 23,1,00Fh,"CTRL: SHIFT+ENTER, then the key      ESC (SHIFT+1): cancels ^K, ^Q, questions",0
            defb 0FFh

; ---------------------------------------------------------------------
;  Variables (se ponen a cero al arrancar)
; ---------------------------------------------------------------------
vars:
cur_col:    defs 1              ; cursor en pantalla
cur_row:    defs 1
km_idx:     defs 1              ; teclado
km_mods:    defs 1
enter_used: defs 1
blink:      defs 1
cursor_on:  defs 1
mods_seen:  defs 1
ctrl_prefix: defs 1
cursor_save: defs 1
rep_idx:    defs 1              ; tecla que se esta repitiendo (0: ninguna)
rep_cnt:    defs 1
rep_lim:    defs 1
gap_s:      defs 2              ; texto
gap_e:      defs 2
cur_line:   defs 2              ; linea del cursor (desde 0)
it_addr:    defs 2
modified:   defs 1
crlf:       defs 1              ; el fichero venia con CRLF
pend_cr:    defs 1
top_off:    defs 2              ; ventana
top_line:   defs 2
left_col:   defs 2
want_col:   defs 2
rd_addr:    defs 2
rd_vcol:    defs 2
rd_eof:     defs 1
prefix:     defs 1              ; 'K' o 'Q' tras ^K / ^Q
msg_ptr:    defs 2              ; mensaje para la linea de estado (0: no)
name_len:   defs 1
name_buf:   defs 64
handle:     defs 1
fsize:      defs 4
st_cnt:     defs 2
save_err:   defs 1
pn_dst:     defs 2
pn_ptr:     defs 2
VARS_LEN    equ $-vars
sv_sp:      defs 2              ; fuera de vars: se guardan antes de
sv_i:       defs 1              ; ponerlas a cero
stage:      defs CHUNK          ; trozo que va o viene del fichero

            include "../TELNET/font.inc"

            end
