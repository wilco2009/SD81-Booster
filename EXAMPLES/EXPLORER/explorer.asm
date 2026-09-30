; =============================================================
; EXPLORER.ASM -- Explorador de archivos SD81 Booster (70 columnas)
;
; Navega carpetas de la SD, carga programas .P (y en general cualquier
; archivo, devolviendo el control al BASIC con su nombre) y gestiona
; archivos (nueva carpeta, borrar, renombrar, copiar/mover), en modo
; Superfast texto de 70 columnas (caracteres de 8x8): fuente de 256
; caracteres (Spectrum en 32-127, iconos y graficos del ZX81 en 0-31 y
; el logo de la cabecera en 128-155) y un atributo de color por caracter
; (Chroma81 modo 1). Solo el visor de .SCR pasa un momento a HiRes
; Spectrum.
;
; Construido y depurado de forma incremental en HW real (ver vtest1..9
; en esta misma carpeta -- cada paso confirmado antes de añadir el
; siguiente). Protocolo MCU: puertos $A7 (datos) / $AF (reloj), igual
; que sdhandler.inc.asm / mcu.bas. Comandos usados: CD(3), DEL(4),
; MKDIR(5), RMDIR(6), MOVE(7), COPY(8), OPENDIR2(16), GETROW(18).
;
; Teclas: 5=carpeta padre 6=abajo 7=arriba 8/ENTER=abrir/activar
;         1/2=pagina arriba/abajo  N=nueva carpeta D=borrar R=renombrar
;         C=marcar copiar X=marcar mover V=pegar  ESPACIO=salir
;         S=panel de configuracion (lateral)
;         E=editar el archivo (sale al BASIC con USR = longitud + 256;
;           el stub carga el editor, EDIT.BIN, con el nombre en F$)
;         SHIFT+E=fichero de texto nuevo: pide el nombre y abre el editor
;         K=pantalla de ayuda con todas las teclas
;         L=ver cualquier archivo en el visor de texto (no solo los .TXT)
;
; Ensamblar con pasmo: pasmo explorer.asm EXPLORER.BIN
; Cargar/usar: ver README.md (incluye el stub BASIC necesario).
; =============================================================
        org 24576

; -------------------------------------------------------------
; Bloque de 8K (0-7, de $2000 bytes cada uno) usado para la pantalla
; Superfast HiRes. Cambiar VIDBLOCK aqui mueve toda la pantalla (bitmap,
; atributos, namebuf, HFILE) a otro bloque -- el resto de constantes de
; esta seccion se recalculan solas. En el ZX81 de 16K solo los bloques 6
; y 7 sirven para esto (son los que en modo video normal pueden reflejar
; las paginas 2 y 3); tambien es valido el
; bloque 4 (RAM libre de la ampliacion, sin remapear -- asi se probo en
; vtest1..9, ver esos ficheros si hace falta volver a esa opcion).
; -------------------------------------------------------------
VIDBLOCK        equ 7           ; bloque de pantalla (6 o 7)
VIDPAGE         equ 8           ; pagina fisica dedicada mientras esta activa
; Al salir se restaura el mapeo IDENTIDAD (bloque N -> pagina N), que es el
; estado por defecto del mapper tras un reset, NO el espejo de la pagina
; VIDBLOCK-4 que se restauraba antes (bloque 7 -> pagina 3).
; Con el espejo, cualquier programa lanzado despues desde el explorador se
; encontraba las paginas 3 y 7 siendo la MISMA memoria: escribir en $E000
; machacaba $6000. En hardware real no se notaba porque el video lee de la
; shadow RAM de la FPGA, que es independiente y esta indexada por direccion
; -- pero la RAM si se corrompia, y en el emulador (que lee la RAM paginada)
; salia a la luz.
VIDRESTOREPAGE  equ VIDBLOCK    ; identidad: bloque 7 -> pagina 7
VIDBASE         equ VIDBLOCK*2000h     ; direccion base del bloque (bitmap)
VIDBASE_HI      equ VIDBLOCK*32        ; byte alto de VIDBASE (VIDBASE/256)
ATTRBASE_HI     equ VIDBASE_HI+18h     ; byte alto del area de atributos
; Pantalla de texto de 70 columnas en el mismo bloque: DFILE, atributos
; (con la misma geometria) y fuente de 256 caracteres (2 KB alineados,
; I = TXT_FONT/256). El visor de .SCR usa el bloque entero como bitmap
; Spectrum y al volver se reconstruye todo (video_text_on).
TXT_DFILE       equ VIDBASE             ; 1 + 24 filas de 71 bytes
TXT_ATTR        equ VIDBASE+0800h       ; misma geometria (el +0800h es solo H)
TXT_FONT        equ VIDBASE+1000h
SCRW            equ 70
ROWSTRIDE       equ 71              ; 70 caracteres + 1 byte de relleno
; caracteres propios (0-31 de la fuente)
G_PLAY          equ 1
G_PAUSE         equ 2
G_STOP          equ 3
G_UP            equ 4
G_DOWN          equ 5
G_LEFT          equ 6
G_RIGHT         equ 7
G_FIRE          equ 8
ZXG             equ 16              ; graficos de bloque del ZX81 (1-10) en 16-25,
                                    ; copiados de la ROM al arrancar
LOGO            equ 128             ; el logo de la cabecera: 28 caracteres
LOGO_COL        equ (SCRW-28)/2     ; centrado
; Atributos (Chroma modo 1): PPPP IIII, papel arriba y tinta abajo; en
; cada nibble brillo, verde, rojo y azul.
NORM_ATTR       equ 070h        ; papel blanco, tinta negra
SEL_ATTR        equ 017h        ; papel azul, tinta blanca (resaltado)
PATH_ATTR       equ 040h        ; papel verde, tinta negra
HDR_ATTR        equ 000h        ; cabecera: franja negra
LOGO_ATTR       equ 0A0h        ; el logo: papel rojo brillante, tinta negra
CLOCK_ATTR      equ 005h        ; reloj: papel negro, tinta cian
HELP_ATTR       equ 017h        ; barra de ayuda: papel azul, tinta blanca
HELPKEY_ATTR    equ 01Eh        ; y sus teclas: tinta amarilla
CLIP_ATTR       equ 02Fh        ; C o X con algo marcado: papel rojo
CURSOR_ATTR     equ 007h        ; cursor de la entrada de texto (invertido)
CHROMA_TEXT     equ 031h        ; color + modo 1 (atributos), borde azul
MAXVIS          equ 21
CLOCK_TICK_THRESHOLD equ 2000   ; pasadas de rk_wait por refresco del reloj
                                 ; en vivo -- sin calibrar en hardware real
PANEL_COL0      equ 46          ; 1ª columna del panel de configuracion
LIST_ATTRCOLS   equ PANEL_COL0  ; columnas de la lista con el panel activo
LIST_MAXCHARS   equ PANEL_COL0-1 ; caracteres del nombre con el panel activo
PANEL_TXTCOL    equ PANEL_COL0+2 ; columna donde arranca el texto del panel
PANEL_TXTW      equ SCRW-PANEL_TXTCOL ; ancho del texto del panel
PANEL_VALCOL    equ PANEL_TXTCOL+10 ; columna del valor (128/64) de cada opcion
PANEL_ATTR      equ 050h        ; papel cian, tinta negra (filas de opciones)
PANEL_KEY_ON_ATTR equ 052h      ; papel cian, tinta roja (letra de tecla activa)
PANEL_BASE_ATTR equ PANEL_ATTR  ; fondo base del panel: tambien cian
PANEL_TITLE_ATTR equ 037h       ; papel magenta, tinta blanca (fila del titulo)
PANEL_JOY_VALCOL equ PANEL_TXTCOL+8 ; teclas del joystick (y sus flechas encima)
; dialogos: recuadro encima de la pantalla, con el titulo, el campo de
; texto (o el nombre, al borrar) y una linea con las teclas
DLG_TOP         equ 8           ; filas DLG_TOP..DLG_BOT
DLG_BOT         equ 14
DLG_L           equ 7           ; columnas DLG_L..DLG_L+DLG_W-1
DLG_W           equ 56
TI_COL          equ DLG_L+2     ; columna del texto y del campo
TI_W            equ DLG_W-4     ; ancho del campo
DLG_ATTR        equ 017h        ; recuadro: papel azul, tinta blanca
DLG_TITLE_ATTR  equ 01Fh        ; titulo: tinta blanca brillante
DLG_HINT_ATTR   equ 015h        ; teclas: tinta cian
namebuf         equ VIDBASE+1B00h

; -------------------------------------------------------------
; Visor de texto (*.TXT): estado y buffer de lectura, alojados en el
; mismo hueco libre de VIDBLOCK que namebuf (justo despues), para no
; gastar presupuesto del bloque de 8K del programa.
; -------------------------------------------------------------
VWR_ROWS        equ 22           ; filas de texto en pantalla (1..22)
VWR_BUFSIZE     equ 512          ; trozo de fichero leido de una vez (maximo del firmware)
viewer_handle    equ namebuf+110
viewer_topline   equ viewer_handle+1     ; 2 bytes: nº de linea (0-based) en la fila 1
viewer_bufpos    equ viewer_topline+2    ; 2 bytes: posicion de lectura dentro de viewer_buf
viewer_buflen    equ viewer_bufpos+2     ; 2 bytes: bytes validos en viewer_buf
viewer_size      equ viewer_buflen+2     ; 4 bytes: tamaño total del fichero (CMD_f_stat, al abrir)
viewer_remaining equ viewer_size+4       ; 4 bytes: bytes que quedan por leer desde el ultimo rewind
viewer_buf       equ viewer_remaining+4  ; VWR_BUFSIZE bytes

; -------------------------------------------------------------
; Visor hexadecimal: comparte handle/topline/tamaño con el visor de
; texto (nunca estan activos a la vez), añade su propio estado a
; continuacion del buffer.
; -------------------------------------------------------------
HEXROW_BYTES    equ 16           ; bytes por fila en pantalla
HEXR_ASCII_COL  equ 54           ; columna donde arranca la columna
                                  ; ASCII/ZX81: 6 (offset) + 16 x " XX"
                                  ; = 54; va pegada, separada por color
HEX_OFS_ATTR    equ 071h         ; offset: tinta azul
HEX_ASC_ATTR    equ 050h         ; columna ASCII/ZX81: papel cian
hexr_row        equ viewer_buf+VWR_BUFSIZE  ; 2 bytes: fila (offset/HEXROW_BYTES) en curso durante el render
hexr_count      equ hexr_row+2               ; 1 byte: bytes reales de la fila en curso (1-8)
hexr_nrows      equ hexr_count+1             ; 1 byte: filas realmente pintadas (para PgDn)
hexr_tmplo      equ hexr_nrows+1             ; 2 bytes: escratch de hex_remaining
hexr_tmphi      equ hexr_tmplo+2             ; 2 bytes: escratch de hex_remaining
hexg_val        equ hexr_tmphi+2             ; 4 bytes: acumulador de hex_parse/hexr_goto
hexr_zxmode     equ hexg_val+4               ; 1 byte: 0=columna ASCII, 1=columna ZX81 (tecla Z)

        jp start
retname:        ; buffer ESTABLE del nombre devuelto a BASIC (24579=ORG+3),
        defs 104 ; en RAM bloque 3, sobrevive a video_off (que remapea VIDBLOCK)

start:
        di

        ; --- pantalla en el bloque VIDBLOCK, remapeado a VIDPAGE ---
        ld a,VIDBLOCK
        ld e,VIDPAGE
        call mcu_map

        ; --- estado inicial conocido del panel de config: WRX OFF,
        ; FULLPAG OFF, MC45 OFF, CHR 64 (no hay forma de leer el estado
        ; real del firmware, asi que lo forzamos al arrancar) ---
        xor a
        ld (cfg_wrx),a
        ld (cfg_fullpag),a
        ld (cfg_mc45),a
        ld (cfg_chr128),a
        ld a,85
        ld (2058),a               ; WRX OFF (POKE directo, sin protocolo MCU)
        ld a,30                   ; CMD_pages32 (FULLPAG OFF)
        call mcu_send
        ld a,20                   ; CMD_mc45_off
        call mcu_send
        ld a,28                   ; CMD_chars64
        call mcu_send
        call video_text_on        ; 70 columnas (y 256 caracteres: el modo
                                  ; CHR elegido se aplica al salir)

        ; --- joystick: configuracion por defecto, enviada nada mas
        ; arrancar (antes no se mandaba ninguna hasta tocarlo en el panel) ---
        ld hl,cfg_joy_keys
        ld b,5
        ld a,21                   ; CMD_joy
        call cmd_str_zx

        ; --- abrir directorio raiz ---
        call do_opendir_root

        ; --- posicion inicial y primer repintado completo ---
        ld hl,1
        ld (cur_index),hl
        ld (win_start),hl
        call vt_refresh
        jp vt_loop

; -------------------------------------------------------------
; vt_update_clock: pide la hora (CMD_rtc=50, modo lectura) y repinta,
; en cian, la fecha (DD/MM/AA) a la izquierda y la hora (HH:MM:SS) a la
; derecha de la fila 0. Se llama desde vt_refresh (repintado completo) y
; tambien, si (clock_screen_active)=1, desde read_key mientras espera
; tecla (ver rk_wait) para que se actualice sola sin tocar nada.
; -------------------------------------------------------------
vt_update_clock:
        ld hl,rtc_buf
        call rtc_fetch
        ld a,CLOCK_ATTR
        ld (cur_attr),a
        ld d,0
        ld e,1
        call p42_setxy
        ld a,(rtc_buf+8)         ; DD/MM/AA (rtc_buf = "yyyy-mm-dd ...")
        call p42_putchar
        ld a,(rtc_buf+9)
        call p42_putchar
        ld a,'/'
        call p42_putchar
        ld a,(rtc_buf+5)
        call p42_putchar
        ld a,(rtc_buf+6)
        call p42_putchar
        ld a,'/'
        call p42_putchar
        ld a,(rtc_buf+2)
        call p42_putchar
        ld a,(rtc_buf+3)
        call p42_putchar
        ld e,SCRW-9
        call p42_setxy
        ld a,(rtc_buf+11)
        call p42_putchar
        ld a,(rtc_buf+12)
        call p42_putchar
        ld a,':'
        call p42_putchar
        ld a,(rtc_buf+14)
        call p42_putchar
        ld a,(rtc_buf+15)
        call p42_putchar
        ld a,':'
        call p42_putchar
        ld a,(rtc_buf+17)
        call p42_putchar
        ld a,(rtc_buf+18)
        call p42_putchar
        ret

; -------------------------------------------------------------
; vt_refresh: repintado COMPLETO (video_clear + marco + ruta + listado),
; con (win_start)/(cur_index) actuales. Identico a refresh_screen del
; explorador real. Usado en el arranque y por PgUp/PgDn/activar carpeta,
; que no admiten el repintado parcial de vt_down/vt_up.
; -------------------------------------------------------------
vt_refresh:
        call video_clear
        call draw_header
        ld hl,help_main
        call draw_help
        call vt_update_clip_icons ; C/X en rojo si hay algo marcado

        call vt_update_clock
        ld a,1
        ld (clock_screen_active),a

        ld de,0
        call get_row
        ld a,PATH_ATTR
        ld (cur_attr),a
        ld d,22
        ld e,0
        call p42_setxy
        ld hl,namebuf
        ld a,(namelen)
        cp SCRW
        jr c,vtr_t1
        ld a,SCRW
vtr_t1: ld b,a
        call p42_string

        ld a,(list_filter_len)
        or a
        jr z,vtr_nofilter
        ld a,'['
        call p42_putchar          ; ojo: p42_putchar destruye B, por eso
        ld hl,list_filter          ; se carga la longitud DESPUES, justo
        ld a,(list_filter_len)     ; antes de p42_string (no antes, como
        ld b,a                     ; en el primer intento -- eso causaba
        call p42_string            ; el texto corrupto)
        ld a,']'
        call p42_putchar
vtr_nofilter:
        ld a,22
        ld c,PATH_ATTR
        call fill_row_attr

        ld a,(cfg_panel)
        or a
        jr z,vtr_widthfull
        ld a,LIST_MAXCHARS
        ld (list_maxchars),a
        ld a,LIST_ATTRCOLS
        ld (list_attrw),a
        jr vtr_widthdone
vtr_widthfull:
        ld a,SCRW
        ld (list_maxchars),a
        ld (list_attrw),a
vtr_widthdone:

        ld hl,(win_start)
        ld (row_ptr),hl
        ld c,0
vlist_loop:
        ld a,c
        cp MAXVIS
        jr nc,vlist_done
        ld de,(row_ptr)
        push bc
        call get_row
        pop bc
        ld a,(namelen)
        or a
        jr z,vlist_done

        ld hl,(row_ptr)
        ld de,(cur_index)
        or a
        sbc hl,de
        ld a,NORM_ATTR
        jr nz,vlist_setattr
        ld a,SEL_ATTR
vlist_setattr:
        ld (cur_attr),a
        ld a,c
        inc a
        ld d,a
        ld e,0
        push bc
        call p42_setxy
        pop bc
        ld hl,namebuf
        ld a,(namelen)
        ld b,a
        ld a,(list_maxchars)
        cp b
        jr nc,vlist_t2
        ld b,a
vlist_t2:
        push bc
        call p42_string
        pop bc

        ld a,(cur_attr)
        cp SEL_ATTR
        jr nz,vlist_nofull
        push bc
        ld a,c
        inc a
        push af                  ; guarda fila (1-based)
        ld a,(list_attrw)
        ld b,a
        pop af
        ld c,SEL_ATTR
        call fill_row_attr_n
        pop bc
vlist_nofull:

        ld hl,(row_ptr)
        inc hl
        ld (row_ptr),hl
        inc c
        jr vlist_loop
vlist_done:
        ld a,(cfg_panel)
        or a
        ret z
        jp vt_draw_panel

; -------------------------------------------------------------
; vt_draw_panel: pinta toda la zona de settings (columnas de atributo
; PANEL_COL0..SCRW-1) -- fondo cian base, la fila del titulo (1) en
; blanco/tinta negra a toda su anchura, y cada fila de opcion (3/5/7/9)
; en cian/tinta negra -- y luego el texto encima.
; Se llama desde vt_refresh, despues del listado, solo si (cfg_panel)=1.
; -------------------------------------------------------------
vt_draw_panel:
        ld b,1
vdp_base:
        push bc
        ld a,b
        call vdp_fillrow_base
        pop bc
        inc b
        ld a,b
        cp MAXVIS+1
        jr c,vdp_base

        ld a,1
        call vdp_fillrow_title
        ld a,2
        call vdp_fillrow_opt
        ld a,4
        call vdp_fillrow_opt
        ld a,6
        call vdp_fillrow_opt
        ld a,11
        call vdp_fillrow_opt
        ld a,13
        call vdp_fillrow_opt
        ld a,15
        call vdp_fillrow_opt
        ld a,16
        call vdp_fillrow_opt
        ld a,18
        call vdp_fillrow_opt
        ld a,19
        call vdp_fillrow_opt
        ld a,21
        call vdp_fillrow_opt

        ld d,1
        ld e,PANEL_TITLE_COL
        ld hl,panel_title
        ld b,panel_title_len
        ld c,PANEL_TITLE_ATTR
        call panel_print

        ; -- fila 2: W WRX + M MC45 (letra de la tecla en rojo si activo,
        ; negro si no -- en vez del ON/OFF a la derecha) --
        ld d,2
        ld e,PANEL_TXTCOL
        ld hl,panel_lbl_wrx
        ld b,panel_lbl_wrx_len
        ld a,(cfg_wrx)
        call panel_print_key
        ld d,2
        ld e,PANEL_TXTCOL+8
        ld hl,panel_lbl_mc45
        ld b,panel_lbl_mc45_len
        ld a,(cfg_mc45)
        call panel_print_key

        ; -- fila 4: F FULLP --
        ld d,4
        ld e,PANEL_TXTCOL
        ld hl,panel_lbl_fullpag
        ld b,panel_lbl_fullpag_len
        ld a,(cfg_fullpag)
        call panel_print_key

        ; -- fila 6: A CHR (subida desde la 8) --
        ld d,6
        ld e,PANEL_TXTCOL
        ld hl,panel_lbl_chr
        ld b,panel_lbl_chr_len
        ld c,PANEL_ATTR
        call panel_print
        ld d,6
        ld e,PANEL_VALCOL
        ld a,(cfg_chr128)
        or a
        ld hl,str_chr64
        jr z,vdp_chrval
        ld hl,str_chr128
vdp_chrval:
        ld b,3
        ld c,PANEL_ATTR
        call panel_print

        ; -- flechas del joystick, encima de sus teclas --
        ld a,PANEL_ATTR
        ld (cur_attr),a
        ld d,10
        ld e,PANEL_JOY_VALCOL
        call p42_setxy
        ld a,G_UP
        call p42_putchar
        ld a,G_DOWN
        call p42_putchar
        ld a,G_LEFT
        call p42_putchar
        ld a,G_RIGHT
        call p42_putchar
        ld a,G_FIRE
        call p42_putchar

        ld d,11
        ld e,PANEL_TXTCOL
        ld hl,panel_lbl_joy
        ld b,panel_lbl_joy_len
        ld c,PANEL_ATTR
        call panel_print
        ld d,11
        ld e,PANEL_JOY_VALCOL
        ld hl,cfg_joy_keys
        ld b,5
        ld c,PANEL_ATTR
        call panel_print

        ld d,13
        ld e,PANEL_TYU_COL
        ld hl,panel_lbl_tyu
        ld b,panel_lbl_tyu_len
        ld c,PANEL_ATTR
        call panel_print

        ; -- iconos STOP/PAUSA/PLAY, cada uno bajo su letra (T/Y/U) --
        ld a,PANEL_ATTR
        ld (cur_attr),a
        ld d,14
        ld e,PANEL_TYU_COL
        call p42_setxy
        ld a,G_STOP
        call p42_putchar
        ld a,' '
        call p42_putchar
        ld a,G_PAUSE
        call p42_putchar
        ld a,' '
        call p42_putchar
        ld a,G_PLAY
        call p42_putchar

        ; -- nombre del VGM/PEB cargado (vacio si no hay ninguno) --
        ld d,15
        ld e,PANEL_TXTCOL
        ld hl,cfg_vgm_name
        ld a,(cfg_vgm_namelen)
        ld b,a
        ld c,PANEL_ATTR
        call panel_print

        ; -- atajo para ver /MAN/IP.TXT (o "NO CONNEXION") --
        ld d,16
        ld e,PANEL_TXTCOL
        ld hl,panel_lbl_ip
        ld b,panel_lbl_ip_len
        ld c,PANEL_ATTR
        call panel_print

        ; -- filtro de listado (tecla "."), en dos lineas: el ancho del
        ; panel (PANEL_TXTCOL..41, 15 columnas) no da para las 24 --
        ld d,18
        ld e,PANEL_TXTCOL
        ld hl,panel_lbl_filter1
        ld b,panel_lbl_filter1_len
        ld c,PANEL_ATTR
        call panel_print
        ld d,19
        ld e,PANEL_TXTCOL
        ld hl,panel_lbl_filter2
        ld b,panel_lbl_filter2_len
        ld c,PANEL_ATTR
        call panel_print

        ; -- version MCU/ROM/FPGA, ultima linea: "Mx.y,Rx.y,Fx.y" --
        ld d,21
        ld e,PANEL_TXTCOL
        call p42_setxy
        ld a,PANEL_ATTR
        ld (cur_attr),a
        ld a,'M'
        call p42_putchar
        ld a,1                    ; CMD_ver (version MCU)
        call mcu_send
        call mcu_recv
        call panel_print_ver
        ld a,','
        call p42_putchar
        ld a,'R'
        call p42_putchar
        ld a,(2004h)              ; version ROM (mismo empaquetado, ver VER.txt)
        call panel_print_ver
        ld a,','
        call p42_putchar
        ld a,'F'
        call p42_putchar
        ld a,31                   ; CMD_getFPGAVer
        call mcu_send
        call mcu_recv
        call panel_print_ver
        ret

; panel_print_ver: A=byte de version empaquetado (nibble alto=mayor,
; nibble bajo=menor, igual formato que VERSION en el ROM) -> imprime
; "X.Y" en la posicion actual (xycoords). Destruye AF.
panel_print_ver:
        push af
        rrca
        rrca
        rrca
        rrca
        and 0Fh
        add a,'0'
        call p42_putchar
        ld a,'.'
        call p42_putchar
        pop af
        and 0Fh
        add a,'0'
        call p42_putchar
        ret

; vdp_fillrow_base/_title/_opt: A=fila -> tiñe toda la anchura de la zona
; de settings (columnas PANEL_COL0..SCRW-1) en esa fila, con el atributo
; base/titulo/opcion respectivamente.
vdp_fillrow_base:
        ld c,PANEL_BASE_ATTR
        jr vdp_fillrow_common
vdp_fillrow_title:
        ld c,PANEL_TITLE_ATTR
        jr vdp_fillrow_common
vdp_fillrow_opt:
        ld c,PANEL_ATTR
vdp_fillrow_common:
        ld d,PANEL_COL0
        ld b,SCRW-PANEL_COL0
        jp fill_row_attr_col

; panel_print: D=fila,E=columna,HL=puntero texto,B=longitud,C=atributo.
; Fija (cur_attr)=C antes de imprimir: p42_printdata tiñe con cur_attr la
; celda de cada caracter que dibuja (igual que hace el listado para
; resaltar la fila seleccionada); sin esto el texto del panel heredaba el
; ultimo cur_attr que dejo el listado (NORM_ATTR) y salia con fondo
; blanco encima del cian/blanco del panel.
panel_print:
        ld a,c
        ld (cur_attr),a
        call p42_setxy
        jp p42_string

; panel_print_key: D=fila,E=columna,HL=puntero etiqueta,B=longitud,
; A=1 si la opcion esta activa (letra de tecla en verde) o 0 (en negro,
; igual que el resto del texto -- no hay ON/OFF a la derecha, el estado
; lo indica el color de la letra). Destruye AF,BC,DE,HL.
panel_print_key:
        or a
        ld a,PANEL_ATTR
        jr z,ppk_setxy
        ld a,PANEL_KEY_ON_ATTR
ppk_setxy:
        push af
        call p42_setxy
        pop af
        ld (cur_attr),a
        dec b                     ; longitud restante
        ld a,(hl)
        inc hl                    ; hl ya apunta al resto de la cadena
        push bc                   ; p42_putchar destruye B, C **y HL**
        push hl                   ; (usa el juego alterno con exx, pero
        call p42_putchar          ; antes toca H/L del principal)
        pop hl
        pop bc
        ld a,PANEL_ATTR
        ld (cur_attr),a
        jp p42_string

; panel_onoff: A=0/1 -> HL=puntero a cadena de 3 caracteres "OFF"/"ON "
panel_onoff:
        or a
        ld hl,str_off
        ret z
        ld hl,str_on
        ret

panel_title:    defb "SETTINGS"
panel_title_len equ $-panel_title
; centrado en el hueco de texto del panel (PANEL_TXTW columnas)
PANEL_TITLE_COL equ PANEL_TXTCOL+(PANEL_TXTW-panel_title_len)/2

panel_lbl_wrx:         defb "W WRX"
panel_lbl_wrx_len      equ $-panel_lbl_wrx
panel_lbl_fullpag:     defb "F FULLP"
panel_lbl_fullpag_len  equ $-panel_lbl_fullpag
panel_lbl_mc45:        defb "M MC45"
panel_lbl_mc45_len     equ $-panel_lbl_mc45
panel_lbl_chr:         defb "A CHR"
panel_lbl_chr_len      equ $-panel_lbl_chr
panel_lbl_joy:         defb "J JOY"
panel_lbl_joy_len      equ $-panel_lbl_joy
panel_lbl_tyu:         defb "T Y U"
panel_lbl_tyu_len      equ $-panel_lbl_tyu
panel_lbl_ip:          defb "I IP"
panel_lbl_ip_len       equ $-panel_lbl_ip
panel_lbl_filter1:     defb ". FILTER"
panel_lbl_filter1_len  equ $-panel_lbl_filter1
panel_lbl_filter2:     defb "(SHIFT+1)=RESET"
panel_lbl_filter2_len  equ $-panel_lbl_filter2
; centrado igual que el titulo (PANEL_TXTW columnas)
PANEL_TYU_COL equ PANEL_TXTCOL+(PANEL_TXTW-panel_lbl_tyu_len)/2

str_off:        defb "OFF"
str_on:         defb "ON "
str_chr128:     defb "128"
str_chr64:      defb "64 "

; -------------------------------------------------------------
; vt_loop: bucle interactivo real -- read_key + 6/7 (movimiento con
; scroll) + 1/2 (PgUp/PgDn) + 8/ENTER (activar: entra en carpeta) +
; ESPACIO (salir al BASIC). Seleccionar un archivo (no carpeta) no hace
; nada en este test.
; -------------------------------------------------------------
vt_loop:
        call read_key
        cp 1
        jp z,vt_exit
        cp 2
        jp z,vt_updir
        cp 3
        jp z,vt_down
        cp 4
        jp z,vt_up
        cp 5
        jp z,vt_activate
        cp 6
        jp z,vt_activate
        cp 7
        jp z,vt_newfolder
        cp 8
        jp z,vt_delete
        cp 9
        jp z,vt_rename
        cp 10
        jp z,vt_mark_copy
        cp 11
        jp z,vt_mark_move
        cp 12
        jp z,vt_paste
        cp 13
        jp z,vt_pgup
        cp 14
        jp z,vt_pgdn
        cp 15
        jp z,vt_toggle_panel
        cp 16
        jp z,vt_toggle_wrx
        cp 17
        jp z,vt_toggle_fullpag
        cp 18
        jp z,vt_toggle_mc45
        cp 19
        jp z,vt_view_hex
        cp 20
        jp z,vt_edit_joy
        cp 21
        jp z,vt_vgm_stop
        cp 22
        jp z,vt_vgm_pause
        cp 23
        jp z,vt_vgm_cont
        cp 24
        jp z,vt_toggle_chr
        cp 27
        jp z,vt_view_ip
        cp 28
        jp z,vt_reset_filter
        cp 29
        jp z,vt_edit_filter
        cp 30
        jp z,vt_edit
        cp 31
        jp z,vt_help
        cp 32
        jp z,vt_view_any
        cp 33
        jp z,vt_newfile
        jp vt_loop

; -------------------------------------------------------------
; vt_help: tecla K -- pantalla con todas las teclas; cualquier tecla
; vuelve al listado.
; -------------------------------------------------------------
vt_help:
        xor a
        ld (clock_screen_active),a
        call video_clear
        call draw_header
        ld hl,help_back
        call draw_help
        ld hl,help_text
vh1:    ld a,(hl)                 ; fila (0FFh = fin), columna, texto
        cp 0FFh
        jr z,vh_done
        ld d,a
        inc hl
        ld e,(hl)
        inc hl
        call p42_setxy
        ld a,NORM_ATTR
        ld (cur_attr),a
vh2:    ld a,(hl)                 ; texto hasta 0; {tecla} en otro color
        inc hl
        or a
        jr z,vh1
        ld c,HELPTXT_KEY_ATTR
        cp '{'
        jr z,vh3
        ld c,NORM_ATTR
        cp '}'
        jr z,vh3
        call p42_putchar
        jr vh2
vh3:    ld a,c
        ld (cur_attr),a
        jr vh2
vh_done:
        call read_key
        jp vt_refresh_and_loop

HELPTXT_KEY_ATTR equ 071h        ; las teclas: papel blanco, tinta azul


; -------------------------------------------------------------
; vt_updir: tecla 5 -- copia literal de do_updir en explorer.asm (CD ..
; + recargar listado), terminando en vt_refresh en vez de mainloop.
; -------------------------------------------------------------
vt_updir:
        ld hl,updir_dotdot
        ld b,2
        call do_cd
        call do_opendir_root
        ld hl,1
        ld (cur_index),hl
        ld (win_start),hl
        call vt_refresh
        jp vt_loop
updir_dotdot: defb ".."

; -------------------------------------------------------------
; vt_toggle_panel: tecla S -- activa/desactiva el panel de configuracion
; lateral. No toca (cur_index)/(win_start): al desactivarlo, vt_refresh
; repinta la lista a ancho completo en la misma posicion en la que estaba.
; -------------------------------------------------------------
vt_toggle_panel:
        ld a,(cfg_panel)
        xor 1
        ld (cfg_panel),a
        call vt_refresh
        jp vt_loop

; -------------------------------------------------------------
; vt_toggle_wrx/fullpag/mc45/chr: teclas W/F/M/H del panel -- invierten
; su variable cfg_* y envian el comando correspondiente (WRX es un POKE
; directo a 2058, igual que CmdWRX en el ROM; los demas son comandos MCU
; de un solo byte, sin respuesta, igual que CMD_ONOFF_BC en el ROM).
; Repintan el panel entero (vt_refresh) para reflejar el nuevo estado.
; Si el panel no esta visible, no hacen nada (evita cambios de estado
; invisibles mientras se navega el listado).
; -------------------------------------------------------------
vt_toggle_wrx:
        ld a,(cfg_panel)
        or a
        jp z,vt_loop
        ld a,(cfg_wrx)
        xor 1
        ld (cfg_wrx),a
        or a
        jr z,vtw_off
        ld a,170
        jr vtw_poke
vtw_off:
        ld a,85
vtw_poke:
        ld (2058),a
        call vt_refresh
        jp vt_loop

; vt_edit_filter: tecla "." -- pide una cadena con wildcards (precarga el
; filtro actual, si hay) y la aplica a la carpeta actual via OPENDIR2
; (ver do_opendir_root/cmd_opendir2, que ya soporta wildcards separando
; el nombre tras la ultima "/"). Vacio = sin filtro. El filtro persiste
; al navegar; solo se borra con SHIFT+1 (vt_reset_filter).
vt_edit_filter:
        ld a,(list_filter_len)
        ld (namelen),a
        or a
        jr z,vef_noprefill
        ld hl,list_filter
        ld de,namebuf
        ld b,0
        ld c,a
        ldir
vef_noprefill:
        ld hl,prompt_filter
        ld b,prompt_filter_len
        ld de,hint_edit
        call show_prompt
        call text_input
        ld a,(namelen)
        ld (list_filter_len),a
        or a
        jr z,vef_nocopy
        ld hl,namebuf
        ld de,list_filter
        ld b,0
        ld c,a
        ldir
vef_nocopy:
        call do_opendir_root
        ld hl,1
        ld (cur_index),hl
        ld (win_start),hl
        call vt_refresh
        jp vt_loop

prompt_filter:
        defb "FILTER (WILDCARDS, EMPTY = SHOW ALL)"
prompt_filter_len equ $-prompt_filter

; vt_reset_filter: SHIFT+1 -- quita el filtro de listado (si habia) y
; recarga la carpeta actual sin el.
vt_reset_filter:
        xor a
        ld (list_filter_len),a
        call do_opendir_root
        ld hl,1
        ld (cur_index),hl
        ld (win_start),hl
        call vt_refresh
        jp vt_loop

vt_toggle_fullpag:
        ld a,(cfg_panel)
        or a
        jp z,vt_loop
        ld a,(cfg_fullpag)
        xor 1
        ld (cfg_fullpag),a
        or a
        jr z,vtf_off
        ld a,29                   ; CMD_pages64
        jr vtf_send
vtf_off:
        ld a,30                   ; CMD_pages32
vtf_send:
        call mcu_send
        call vt_refresh
        jp vt_loop

vt_toggle_mc45:
        ld a,(cfg_panel)
        or a
        jp z,vt_loop
        ld a,(cfg_mc45)
        xor 1
        ld (cfg_mc45),a
        or a
        jr z,vtm45_off
        ld a,19                   ; CMD_mc45_on
        jr vtm45_send
vtm45_off:
        ld a,20                   ; CMD_mc45_off
vtm45_send:
        call mcu_send
        call vt_refresh
        jp vt_loop

vt_toggle_chr:
        ld a,(cfg_panel)
        or a
        jp z,vt_loop
        ld a,(cfg_chr128)         ; se aplica al salir (video_off): el
        xor 1                     ; explorador se ve en modo de 256
        ld (cfg_chr128),a         ; caracteres, y mandarlo ahora lo apagaria
        call vt_refresh
        jp vt_loop

; -------------------------------------------------------------
; vt_edit_joy: tecla J -- edita las 5 teclas del joystick (arriba, abajo,
; izda, dcha, fuego), con el mismo dialogo de texto que usan renombrar/
; nueva carpeta (show_prompt + text_input), pero con (ti_allow_space)=1:
; el ESPACIO es una tecla de joystick valida (p.ej. fuego), asi que aqui
; NO cancela -- se inserta como caracter normal. El campo empieza VACIO
; (a diferencia de renombrar, que precarga el nombre): si se precargaran
; las 5 teclas actuales con el cursor al final, escribir encima sin
; borrar antes daba mas de 5 caracteres y la edicion se descartaba en
; silencio (namelen<>5). Para cancelar y recuperar las teclas de antes
; de editar sigue funcionando SHIFT+1 (ti_restore), por eso se rellena
; rn_oldname/rn_oldlen con el valor actual antes de editar. Solo si se
; sale con exactamente 5 caracteres se manda con CMD_joy (21) y se
; actualiza cfg_joy_keys; en cualquier otro caso no se toca nada. Solo
; actua si el panel esta visible.
; -------------------------------------------------------------
vt_edit_joy:
        ld a,(cfg_panel)
        or a
        jp z,vt_loop

        ld a,5
        ld (rn_oldlen),a
        ld hl,cfg_joy_keys
        ld de,rn_oldname
        ld bc,5
        ldir
        xor a
        ld (namelen),a            ; campo vacio: escribir sustituye, no
                                   ; hace falta borrar las 5 de antes
                                   ; (SHIFT+1 sigue recuperando rn_oldname)

        ld a,1
        ld (ti_allow_space),a
        ld hl,prompt_joy
        ld b,prompt_joy_len
        ld de,hint_joy
        call show_prompt
        call text_input
        xor a
        ld (ti_allow_space),a

        ld a,(namelen)
        cp 5
        jp nz,vt_refresh_and_loop  ; longitud distinta de 5: no se manda nada

        ld hl,namebuf
        ld de,cfg_joy_keys
        ld bc,5
        ldir
        ld hl,cfg_joy_keys
        ld b,5
        ld a,21                   ; CMD_joy
        call cmd_str_zx
        jp vt_refresh_and_loop

prompt_joy:
        defb "JOYSTICK KEYS: UP, DOWN, LEFT, RIGHT, FIRE"
prompt_joy_len equ $-prompt_joy

; -------------------------------------------------------------
; vt_vgm_stop/pause/cont: teclas T/Y/U -- control del VGM cargado con
; vt_act_loadvgm. T (parar) y Y (pausar) solo actuan si esta sonando;
; U (continuar) solo si hay algo cargado (sonando o en pausa). Solo
; actuan si el panel esta visible.
; -------------------------------------------------------------
vt_vgm_stop:
        ld a,(cfg_panel)
        or a
        jp z,vt_loop
        ld a,(cfg_vgm_playing)
        or a
        jp z,vt_loop
        xor a
        ld (cfg_vgm_playing),a
        ld (cfg_vgm_loaded),a
        ld (cfg_vgm_namelen),a
        ld a,(cfg_media_peb)
        or a
        jr nz,vvs_peb
        ld a,35                   ; CMD_stopVGM
        call mcu_send
        jr vvs_done
vvs_peb:
        ld a,42                   ; CMD_STOP_PEG
        call mcu_send
        xor a
        call mcu_send             ; hilo 0
vvs_done:
        call vt_refresh
        jp vt_loop

vt_vgm_pause:
        ld a,(cfg_panel)
        or a
        jp z,vt_loop
        ld a,(cfg_vgm_playing)
        or a
        jp z,vt_loop
        xor a
        ld (cfg_vgm_playing),a
        ld a,(cfg_media_peb)
        or a
        jr nz,vvp_peb
        ld a,36                   ; CMD_pauseVGM
        call mcu_send
        jr vvp_done
vvp_peb:
        ld a,43                   ; CMD_PAUSE_PEG
        call mcu_send
        xor a
        call mcu_send             ; hilo 0
vvp_done:
        call vt_refresh
        jp vt_loop

vt_vgm_cont:
        ld a,(cfg_panel)
        or a
        jp z,vt_loop
        ld a,(cfg_vgm_loaded)
        or a
        jp z,vt_loop
        ld a,1
        ld (cfg_vgm_playing),a
        ld a,(cfg_media_peb)
        or a
        jr nz,vvc_peb
        ld a,37                   ; CMD_contVGM
        call mcu_send
        jr vvc_done
vvc_peb:
        ld a,44                   ; CMD_CONT_PEG
        call mcu_send
        xor a
        call mcu_send             ; hilo 0
vvc_done:
        call vt_refresh
        jp vt_loop

vt_exit:
        call video_off
        ld bc,0
        ret                     ; USR devuelve BC=0

vt_down:
        ld hl,(cur_index)
        inc hl
        ld (row_ptr),hl
        ex de,hl
        call get_row
        ld a,(namelen)
        or a
        jp z,vt_loop             ; no hay entrada siguiente
        ld hl,(row_ptr)
        ld (cur_index),hl

        ld hl,(win_start)
        ld bc,MAXVIS-1
        add hl,bc
        ex de,hl
        ld hl,(row_ptr)
        or a
        sbc hl,de
        jp c,vtd_noscroll
        jp z,vtd_noscroll
        jp vtd_scroll

vtd_scroll:
        call scroll_list_up      ; desplaza filas 2..MAXVIS a 1..MAXVIS-1

        ld hl,(cur_index)
        dec hl                   ; fila que era la seleccionada, ahora en MAXVIS-1
        ex de,hl
        ld a,MAXVIS-1
        ld c,NORM_ATTR
        call redraw_row_attr

        ld hl,(win_start)
        inc hl
        ld (win_start),hl

        ld a,MAXVIS               ; limpiar la fila expuesta antes de pintarla
        call clear_row

        ld de,(cur_index)
        ld a,MAXVIS
        ld c,SEL_ATTR
        call redraw_row_attr
        jp vt_loop

vtd_noscroll:
        ld hl,(cur_index)
        dec hl                   ; fila que pierde la seleccion
        push hl
        call calc_screen_row
        pop de
        ld c,NORM_ATTR
        call redraw_row_attr

        ld hl,(cur_index)        ; fila que gana la seleccion
        push hl
        call calc_screen_row
        pop de
        ld c,SEL_ATTR
        call redraw_row_attr
        jp vt_loop

vt_up:
        ld hl,(cur_index)
        ld a,h
        or l
        jp z,vt_loop
        dec hl
        ld a,h
        or l
        jp z,vt_loop             ; cur_index ya era 1
        ld (cur_index),hl
        ld de,(win_start)
        or a
        sbc hl,de
        jp nc,vtu_noscroll        ; cur_index >= win_start, ok
        jp vtu_scroll

vtu_scroll:
        call scroll_list_down    ; desplaza filas 1..MAXVIS-1 a 2..MAXVIS

        ld hl,(cur_index)
        inc hl                   ; fila que era la seleccionada, ahora en fila 2
        ex de,hl
        ld a,2
        ld c,NORM_ATTR
        call redraw_row_attr

        ld hl,(win_start)
        dec hl
        ld (win_start),hl

        ld a,1                    ; limpiar la fila expuesta antes de pintarla
        call clear_row

        ld de,(cur_index)
        ld a,1
        ld c,SEL_ATTR
        call redraw_row_attr
        jp vt_loop

vtu_noscroll:
        ld hl,(cur_index)
        inc hl                   ; fila que pierde la seleccion
        push hl
        call calc_screen_row
        pop de
        ld c,NORM_ATTR
        call redraw_row_attr

        ld hl,(cur_index)        ; fila que gana la seleccion
        push hl
        call calc_screen_row
        pop de
        ld c,SEL_ATTR
        call redraw_row_attr
        jp vt_loop

; -------------------------------------------------------------
; vt_pgdn/vt_pgup: pagina completa (MAXVIS filas) -- copia literal de
; do_pgdn/do_pgup en explorer.asm, terminando en vt_refresh en vez de
; refresh_screen/mainloop.
; -------------------------------------------------------------
vt_pgdn:
        ld hl,(win_start)
        ld bc,MAXVIS
        add hl,bc
        ex de,hl
        call get_row
        ld a,(namelen)
        or a
        jr nz,pgd_full

        ld hl,(win_start)
pgd_find:
        ld d,h
        ld e,l
        push hl
        call get_row
        pop hl
        ld a,(namelen)
        or a
        jr z,pgd_last
        inc hl
        jr pgd_find
pgd_last:
        dec hl
        ld (cur_index),hl
        ld de,MAXVIS-1
        or a
        sbc hl,de
        jr c,pgd_clamp1
        ld a,h
        or l
        jr nz,pgd_usewin
pgd_clamp1:
        ld hl,1
pgd_usewin:
        ld (win_start),hl
        call vt_refresh
        jp vt_loop

pgd_full:
        ld hl,(win_start)
        ld bc,MAXVIS
        add hl,bc
        ld (win_start),hl
        ld (cur_index),hl
        call vt_refresh
        jp vt_loop

vt_pgup:
        ld hl,(win_start)
        ld de,MAXVIS+1
        or a
        sbc hl,de
        jp c,pgu_first
        inc hl
        ld (win_start),hl
        ld (cur_index),hl
        call vt_refresh
        jp vt_loop
pgu_first:
        ld hl,1
        ld (win_start),hl
        ld (cur_index),hl
        call vt_refresh
        jp vt_loop

; -------------------------------------------------------------
; vt_activate: 8/ENTER -- copia literal de do_activate/act_dir/
; act_loadp en explorer.asm. Si es carpeta (nombre entre '<' '>'),
; entra (CD) y recarga el listado. Si es archivo, devuelve el control
; al BASIC con su nombre (igual que act_loadp real: cualquier archivo,
; el filtrado por extension se hace en el stub BASIC).
; -------------------------------------------------------------
vt_activate:
        ld hl,(cur_index)
        ex de,hl
        call get_row
        ld a,(namelen)
        or a
        jp z,vt_loop

        ld a,(namebuf)
        cp '<'
        jp z,vt_act_dir
        call is_vgm_ext
        jp z,vt_act_loadvgm
        call is_peb_ext
        jp z,vt_act_loadpeb
        call is_scr_ext
        jp z,vt_view_scr
        call is_txt_ext
        jp z,vt_view_txt
        jp vt_act_loadp

vt_act_dir:
        call strip_brackets
        ld a,(namelen)
        ld b,a
        ld hl,namebuf
        call do_cd
        call do_opendir_root     ; recargar el listado (file_array) de la
                                 ; carpeta nueva -- faltaba esta llamada,
                                 ; por eso la ruta se actualizaba pero el
                                 ; listado seguia siendo el de la raiz
        ld hl,1
        ld (cur_index),hl
        ld (win_start),hl
        call vt_refresh
        jp vt_loop

; vt_act_loadp: copia literal de act_loadp en explorer.asm. Convierte
; namebuf (ASCII) a codigos ZX81 en retname (CHR$/PEEK desde BASIC
; esperan codigos ZX81), desactiva Superfast y sale al BASIC con
; BC=longitud del nombre (USR lo devuelve).
vt_act_loadp:
        xor a
vt_act_ret:                     ; entrada con A=1 desde vt_edit
        ld (retedit),a
        ld a,(namelen)
        ld (retlen),a
        or a
        jr z,vtl_done
        ld b,a
        ld hl,namebuf
        ld de,retname
vtl_loop:
        ld a,(hl)
        inc hl
        push bc
        push hl
        call ascii_to_zx
        ld (de),a
        inc de
        pop hl
        pop bc
        djnz vtl_loop
vtl_done:
        call video_off
        ld a,(retlen)
        ld c,a
        ld a,(retedit)
        ld b,a
        ret                     ; USR devuelve BC = longitud del nombre
                                ; (+256 si es para editarlo)

; -------------------------------------------------------------
; vt_edit: tecla E -- como vt_act_loadp (sale al BASIC con el nombre en
; retname), pero USR devuelve longitud + 256: el stub lo reconoce y
; carga el editor (EDIT.BIN) con el nombre en F$. Sobre una carpeta no
; hace nada.
; -------------------------------------------------------------
vt_edit:
        ld hl,(cur_index)
        ex de,hl
        call get_row
        ld a,(namelen)
        or a
        jp z,vt_loop
        ld a,(namebuf)
        cp '<'
        jp z,vt_loop
vte_go:
        ld a,1
        jp vt_act_ret

; -------------------------------------------------------------
; vt_newfile: SHIFT+E -- pide el nombre de un fichero NUEVO y sale igual
; que vt_edit: el editor, al no encontrarlo, empieza uno con ese nombre y
; lo crea al guardar.
; -------------------------------------------------------------
vt_newfile:
        xor a
        ld (namelen),a
        ld (rn_oldlen),a          ; SHIFT+1 no tiene nada que recuperar
        ld hl,prompt_newfile
        ld b,prompt_newfile_len
        ld de,hint_edit
        call show_prompt
        call text_input
        ld a,(namelen)
        or a
        jr nz,vte_go
        jp vt_refresh_and_loop    ; cancelado

prompt_newfile:
        defb "NEW TEXT FILE"
prompt_newfile_len equ $-prompt_newfile

VGM_NAME_MAXLEN equ 15   ; ancho de texto util del panel (PANEL_TXTCOL..41)

; is_vgm_ext: Z si namebuf/(namelen) termina en ".VGM" (mayusculas, mismo
; criterio que el resto de nombres que devuelve la SD). Destruye AF,DE,HL.
is_vgm_ext:
        ld a,(namelen)
        cp 4
        jr c,ive_no
        ld hl,namebuf
        ld e,a
        ld d,0
        add hl,de
        dec hl
        dec hl
        dec hl
        dec hl                  ; hl = namebuf + namelen - 4
        ld a,(hl)
        cp '.'
        jr nz,ive_no
        inc hl
        ld a,(hl)
        cp 'V'
        jr nz,ive_no
        inc hl
        ld a,(hl)
        cp 'G'
        jr nz,ive_no
        inc hl
        ld a,(hl)
        cp 'M'
        ret
ive_no:
        or 1                    ; asegura NZ
        ret

; is_peb_ext: Z si namebuf/(namelen) termina en ".PEB" (mismo criterio
; que is_vgm_ext). Destruye AF,DE,HL.
is_peb_ext:
        ld a,(namelen)
        cp 4
        jr c,ipe_no
        ld hl,namebuf
        ld e,a
        ld d,0
        add hl,de
        dec hl
        dec hl
        dec hl
        dec hl
        ld a,(hl)
        cp '.'
        jr nz,ipe_no
        inc hl
        ld a,(hl)
        cp 'P'
        jr nz,ipe_no
        inc hl
        ld a,(hl)
        cp 'E'
        jr nz,ipe_no
        inc hl
        ld a,(hl)
        cp 'B'
        ret
ipe_no:
        or 1
        ret

; is_scr_ext: Z si namebuf/(namelen) termina en ".SCR" (mismo criterio
; que is_vgm_ext). Destruye AF,DE,HL.
is_scr_ext:
        ld a,(namelen)
        cp 4
        jr c,isc_no
        ld hl,namebuf
        ld e,a
        ld d,0
        add hl,de
        dec hl
        dec hl
        dec hl
        dec hl
        ld a,(hl)
        cp '.'
        jr nz,isc_no
        inc hl
        ld a,(hl)
        cp 'S'
        jr nz,isc_no
        inc hl
        ld a,(hl)
        cp 'C'
        jr nz,isc_no
        inc hl
        ld a,(hl)
        cp 'R'
        ret
isc_no:
        or 1
        ret

; is_txt_ext: Z si namebuf/(namelen) termina en ".TXT" (mismo criterio
; que is_vgm_ext). Destruye AF,DE,HL.
is_txt_ext:
        ld a,(namelen)
        cp 4
        jr c,ite_no
        ld hl,namebuf
        ld e,a
        ld d,0
        add hl,de
        dec hl
        dec hl
        dec hl
        dec hl
        ld a,(hl)
        cp '.'
        jr nz,ite_no
        inc hl
        ld a,(hl)
        cp 'T'
        jr nz,ite_no
        inc hl
        ld a,(hl)
        cp 'X'
        jr nz,ite_no
        inc hl
        ld a,(hl)
        cp 'T'
        ret
ite_no:
        or 1
        ret

; vt_act_loadvgm: ENTER sobre un archivo .VGM -- lo carga con CMD_loadVGM
; (34) y se queda en el explorador (a diferencia de vt_act_loadp, que
; sale al BASIC). Guarda el nombre, recortado al ancho del panel, para
; mostrarlo en la fila de musica, y marca "cargado y sonando" (el reproductor
; arranca solo al cargar).
vt_act_loadvgm:
        ld a,(namelen)
        ld b,a
        ld hl,namebuf
        ld a,34                   ; CMD_loadVGM
        call cmd_str_zx

        ld a,(namelen)
        cp VGM_NAME_MAXLEN
        jr c,vtlv_short
        ld a,VGM_NAME_MAXLEN
vtlv_short:
        ld (cfg_vgm_namelen),a
        ld c,a
        ld b,0
        ld hl,namebuf
        ld de,cfg_vgm_name
        ldir

        ld a,1
        ld (cfg_vgm_loaded),a
        ld (cfg_vgm_playing),a
        xor a
        ld (cfg_media_peb),a
        jp vt_refresh_and_loop

; vt_act_loadpeb: ENTER sobre un archivo .PEB -- lo carga con SDLOAD_PEG
; (45, nombre + direccion 0) y arranca el hilo 0 con PLAY_PEG (41,
; hilo 0 + direccion 0). Mismo hueco de nombre/estado que un VGM (solo
; puede haber una cosa cargada a la vez); cfg_media_peb indica cual de
; los dos es, para que T/Y/U (stop/pause/cont) usen los comandos PEG en
; vez de los de VGM.
vt_act_loadpeb:
        ld a,(namelen)
        ld b,a
        ld hl,namebuf
        ld a,45                   ; CMD_SDLOAD_PEG
        call mcu_send
        call send_pascal_zx
        xor a
        call mcu_send             ; direccion 0
        call mcu_recv             ; status (sin uso)

        ld a,41                   ; CMD_PLAY_PEG
        call mcu_send
        xor a
        call mcu_send             ; hilo 0
        xor a
        call mcu_send             ; direccion 0

        ld a,(namelen)
        cp VGM_NAME_MAXLEN
        jr c,vtlp_short
        ld a,VGM_NAME_MAXLEN
vtlp_short:
        ld (cfg_vgm_namelen),a
        ld c,a
        ld b,0
        ld hl,namebuf
        ld de,cfg_vgm_name
        ldir

        ld a,1
        ld (cfg_vgm_loaded),a
        ld (cfg_vgm_playing),a
        ld (cfg_media_peb),a
        jp vt_refresh_and_loop

; -------------------------------------------------------------
; vt_view_scr: ENTER sobre un archivo .SCR -- captura de pantalla
; Spectrum nativa (6912 bytes: 6144 de bitmap "de tercios" + 768 de
; atributo, ver spec_bmp_addr). Pasa a HiRes Spectrum (video_spectrum_on) y
; al volver reconstruye la pantalla de texto. Se lee en
; bloques de 256 bytes (una scanline x los 8 bytes-fila de un tercio, o
; 8 filas de atributo) -- NO fila a fila, que serian cientos de
; peticiones MCU -- y cada bloque se reparte directamente a las filas
; de pantalla reales via spec_bmp_addr/spec_attr_addr. Espera cualquier
; tecla y vuelve al listado.
; -------------------------------------------------------------
vt_view_scr:
        call f_open_txt
        cp 0FFh
        jp z,vt_loop

        ld (viewer_handle),a
        xor a
        ld (clock_screen_active),a
        call video_spectrum_on

        ; -- bitmap: 3 tercios x 8 scanlines x 256 bytes (8 filas x 32) --
        xor a
        ld (vscr_third),a
vscr_third_loop:
        xor a
        ld (vscr_line),a
vscr_line_loop:
        ld a,(viewer_handle)
        ld de,256
        call f_read
        xor a
        ld (vscr_rit),a
        ld hl,viewer_buf
        ld (vscr_srcptr),hl
vscr_rit_loop:
        ld a,(vscr_third)
        add a,a
        add a,a
        add a,a                  ; a = tercio*8
        ld b,a
        ld a,(vscr_rit)
        add a,b                  ; a = fila global (0-23)
        call spec_bmp_addr       ; hl = direccion scanline0 de esa fila
        ld a,(vscr_line)
        add a,h
        ld h,a                   ; += scanline (cada una sale +100h)
        ex de,hl                 ; de = direccion destino en pantalla
        ld hl,(vscr_srcptr)
        ld bc,32
        ldir
        ld (vscr_srcptr),hl
        ld a,(vscr_rit)
        inc a
        ld (vscr_rit),a
        cp 8
        jr c,vscr_rit_loop

        ld a,(vscr_line)
        inc a
        ld (vscr_line),a
        cp 8
        jr c,vscr_line_loop

        ld a,(vscr_third)
        inc a
        ld (vscr_third),a
        cp 3
        jr c,vscr_third_loop

        ; -- atributos: 3 bloques de 256 bytes (8 filas x 32), sin
        ; "tercios" -- lineales, fila 0..23 seguidas --
        xor a
        ld (vscr_third),a
vscr_attr_third_loop:
        ld a,(viewer_handle)
        ld de,256
        call f_read
        xor a
        ld (vscr_rit),a
        ld hl,viewer_buf
        ld (vscr_srcptr),hl
vscr_attr_rit_loop:
        ld a,(vscr_third)
        add a,a
        add a,a
        add a,a
        ld b,a
        ld a,(vscr_rit)
        add a,b
        call spec_attr_addr
        ex de,hl
        ld hl,(vscr_srcptr)
        ld bc,32
        ldir
        ld (vscr_srcptr),hl
        ld a,(vscr_rit)
        inc a
        ld (vscr_rit),a
        cp 8
        jr c,vscr_attr_rit_loop

        ld a,(vscr_third)
        inc a
        ld (vscr_third),a
        cp 3
        jr c,vscr_attr_third_loop

        ld a,(viewer_handle)
        call f_close
        call read_key
        call video_text_on        ; el bitmap ha machacado pantalla y fuente
        jp vt_refresh_and_loop

; -------------------------------------------------------------
; vt_view_any: tecla L -- abre la entrada seleccionada en el visor de
; texto aunque no sea .TXT (listados .B81, .BAS, .CFG...). Sobre una
; carpeta no hace nada.
; -------------------------------------------------------------
vt_view_any:
        ld hl,(cur_index)
        ex de,hl
        call get_row
        ld a,(namelen)
        or a
        jp z,vt_loop
        ld a,(namebuf)
        cp '<'
        jp z,vt_loop
        jp vt_view_txt

; -------------------------------------------------------------
; vt_view_txt: ENTER sobre un archivo .TXT -- lo abre en modo ASCII
; (CMD_f_open=53) y muestra su contenido a pantalla completa, con 6/7
; para desplazarse linea a linea (ESPACIO para salir). Si no se puede
; abrir, no hace nada.
; -------------------------------------------------------------
vt_view_txt:
        call f_open_txt
        cp 0FFh
        jp z,vt_loop

vt_view_txt_opened:              ; entrada compartida con vt_view_ip (con
                                  ; (namebuf,namelen) ya listos y f_open_txt
                                  ; ya hecho con exito, A=handle)
        ld (viewer_handle),a
        call f_stat              ; -> (viewer_size) = tamaño exacto del
                                  ; fichero (ignoramos fecha/hora y status:
                                  ; el tamaño ya es valido con solo tener
                                  ; el handle abierto)
        ld hl,0
        ld (viewer_topline),hl
        call vwr_render

vwr_loop:
        call read_key
        cp 1
        jp z,vwr_exit
        cp 3
        jp z,vwr_down
        cp 4
        jp z,vwr_up
        cp 13
        jp z,vwr_pgup
        cp 14
        jp z,vwr_pgdn
        jr vwr_loop

vwr_down:
        ld hl,(viewer_topline)
        inc hl
        ld (viewer_topline),hl
        call vwr_render
        jr vwr_loop

vwr_up:
        ld hl,(viewer_topline)
        ld a,h
        or l
        jr z,vwr_loop            ; ya esta en la primera linea
        dec hl
        ld (viewer_topline),hl
        call vwr_render
        jr vwr_loop

; vwr_pgup/vwr_pgdn: teclas 1/2 -- avanzan/retroceden una pantalla
; completa (VWR_ROWS lineas). vwr_pgup recorta a 0 en vez de dejar que
; (viewer_topline) se vaya a negativo (envolveria a un numero enorme,
; de 16 bits sin signo). vwr_pgdn no comprueba el final del fichero por
; la misma razon que vwr_down no lo hace: si te pasas, la pantalla sale
; en blanco y con 7/PgUp se vuelve atras.
;
; vwr_pgdn SI comprueba si la pagina nueva queda en blanco (ninguna linea
; impresa): en ese caso deshace el avance y se queda en la ultima pagina
; con texto, en vez de dejar la pantalla vacia.
vwr_pgup:
        ld hl,(viewer_topline)
        ld de,VWR_ROWS
        or a
        sbc hl,de
        jr nc,vwr_pgup_ok
        ld hl,0
vwr_pgup_ok:
        ld (viewer_topline),hl
        call vwr_render
        jr vwr_loop

vwr_pgdn:
        ld hl,(viewer_topline)
        push hl                    ; guarda la fila actual por si hay que deshacer
        ld de,VWR_ROWS
        add hl,de
        ld (viewer_topline),hl
        call vwr_render
        ld a,(vwr_row)
        cp 1
        jr nz,vwr_pgdn_ok          ; se imprimio al menos una linea: aceptar
        pop hl
        ld (viewer_topline),hl
        call vwr_render            ; en blanco: repinta la ultima pagina con texto
        jr vwr_loop
vwr_pgdn_ok:
        pop hl
        jr vwr_loop

vwr_exit:
        ld a,(viewer_handle)
        call f_close
        jp vt_refresh_and_loop

; vt_view_ip: tecla I -- equivalente a abrir /MAN/IP.TXT en el visor de
; texto (vt_view_txt), sea cual sea la carpeta actual; si no existe,
; muestra "NO CONNEXION" en vez de quedarse callado.
vt_view_ip:
        ld hl,ip_path
        ld de,namebuf
        ld bc,ip_path_len
        ldir
        ld a,ip_path_len
        ld (namelen),a
        call f_open_txt
        cp 0FFh
        jp nz,vt_view_txt_opened

        ld hl,noconn_msg
        ld b,noconn_msg_len
        ld de,hint_anykey
        call show_prompt
        call read_key
        jp vt_refresh_and_loop

ip_path:        defb "/MAN/IP.TXT"
ip_path_len     equ $-ip_path
noconn_msg:     defb "NO CONNECTION (/MAN/IP.TXT NOT FOUND)"
noconn_msg_len  equ $-noconn_msg

; vwr_render: repinta la pantalla completa del visor. Vuelve siempre al
; principio del fichero y relee desde ahi, saltando (viewer_topline)
; lineas sin imprimir y luego imprimiendo hasta VWR_ROWS o EOF -- mas
; sencillo y robusto que llevar un historial de offsets, a costa de
; releer el fichero en cada desplazamiento (aceptable para el tamaño
; tipico de estos archivos).
vwr_render:
        xor a
        ld (clock_screen_active),a
        ld a,(viewer_handle)
        call f_rewind
        ld hl,0
        ld (viewer_bufpos),hl
        ld (viewer_buflen),hl
        ld hl,(viewer_size)
        ld (viewer_remaining),hl
        ld hl,(viewer_size+2)
        ld (viewer_remaining+2),hl

        call video_clear
        call draw_header
        ld hl,help_viewer
        call draw_help

        ld hl,(viewer_topline)
        ld a,h
        or l
        jr z,vwr_skipdone
vwr_skiploop:
        push hl
        call vwr_nextline
        pop hl
        jr nz,vwr_skipdone        ; EOF antes de llegar a la linea pedida
        dec hl
        ld a,h
        or l
        jr nz,vwr_skiploop
vwr_skipdone:

        ld a,1
        ld (vwr_row),a            ; fila de pantalla actual (1..VWR_ROWS) --
                                  ; en memoria, NO en C: vwr_nextline usa C
                                  ; como escratch para el caracter leido y
                                  ; lo destruye (era el bug: la fila se
                                  ; corrompia justo antes de p42_setxy)
vwr_printloop:
        ld a,(vwr_row)
        cp VWR_ROWS+1
        jr nc,vwr_printdone
        call vwr_nextline
        jr nz,vwr_printdone       ; EOF: no hay mas lineas que mostrar
        ld a,NORM_ATTR
        ld (cur_attr),a
        ld a,(vwr_row)
        ld d,a
        ld e,0
        call p42_setxy
        ld hl,namebuf
        ld a,(namelen)
        ld b,a
        call p42_string
        ld a,(vwr_row)
        inc a
        ld (vwr_row),a
        jr vwr_printloop
vwr_printdone:
        ret
vwr_row: defb 0

; vwr_nextline: lee la siguiente linea del stream (via vwr_getchar) y la
; deja en namebuf/(namelen), recortada a SCRW caracteres -- sigue
; consumiendo caracteres hasta el '\n' aunque no quepan mas, para no
; perder la cuenta de bytes del fichero. Ignora '\r'. Devuelve Z si se
; leyo una linea (aunque este vacia); NZ si no quedaba nada que leer.
vwr_nextline:
        xor a
        ld (namelen),a
vnl_charloop:
        call vwr_getchar
        jr c,vnl_eof
        cp 13
        jr z,vnl_charloop         ; ignora CR (por si el fichero es CRLF)
        cp 10
        jr z,vnl_done             ; LF: fin de linea
        cp 9
        jr z,vnl_tab
        cp 32
        jr c,vnl_dot
        cp 127
        jr c,vnl_put
vnl_dot:
        ld a,'.'                  ; controles y 127-255: no se pueden ver
vnl_put:
        call vnl_append
        jr vnl_charloop
vnl_tab:                          ; TAB: espacios hasta la columna multiplo de 8
        ld a,' '
        call vnl_append
        ld a,(namelen)
        cp SCRW
        jr nc,vnl_charloop
        and 7
        jr nz,vnl_tab
        jr vnl_charloop

; vnl_append: A=caracter -> al final de namebuf si cabe (SCRW); si no,
; se pierde (la linea sigue consumiendose hasta el LF). Destruye AF,DE,HL.
vnl_append:
        ld c,a
        ld a,(namelen)
        cp SCRW
        ret nc
        ld hl,namebuf
        ld e,a
        ld d,0
        add hl,de
        ld (hl),c
        inc a
        ld (namelen),a
        ret
vnl_done:
        xor a                     ; Z=1
        ret
vnl_eof:
        ld a,(namelen)
        or a
        jr nz,vnl_done            ; ultima linea sin '\n' final: se da por buena
        or 1                      ; NZ: no hay mas lineas
        ret

; vwr_getchar: A=siguiente byte del stream, recargando (viewer_buf) con
; f_read cuando se agota. CY=1 si no quedan mas datos (viewer_remaining
; llega a 0). (viewer_remaining) -- puesto a (viewer_size) por vwr_render,
; obtenido de CMD_f_stat al abrir -- dice exactamente cuantos bytes
; quedan, asi que cada recarga pide como mucho eso: nunca se le pide a
; CMD_f_read mas de lo que hay, y por tanto nunca rellena con ceros de
; relleno (no hace falta ninguna heuristica sobre bytes 0x00).
vwr_getchar:
        ld hl,(viewer_buflen)
        ld de,(viewer_bufpos)
        or a
        sbc hl,de
        jr nz,vgc_have             ; bufpos < buflen: quedan datos en el buffer

        ld hl,(viewer_remaining)
        ld a,h
        or l
        ld b,a
        ld hl,(viewer_remaining+2)
        ld a,h
        or l
        or b
        jr nz,vgc_refill
        scf
        ret                        ; (viewer_remaining)=0: fin de fichero real

vgc_refill:
        call vwr_reqlen            ; de = min(VWR_BUFSIZE,(viewer_remaining))
        push de
        ld a,(viewer_handle)
        call f_read
        pop de
        call vwr_sub_remaining
        ld hl,0
        ld (viewer_bufpos),hl
        ld (viewer_buflen),de
vgc_have:
        ld hl,(viewer_bufpos)
        ld de,viewer_buf
        add hl,de
        ld a,(hl)
        push af
        ld hl,(viewer_bufpos)
        inc hl
        ld (viewer_bufpos),hl
        pop af
        or a                       ; CY=0
        ret

; vwr_reqlen: DE = min(VWR_BUFSIZE,(viewer_remaining)). Asume que ya se
; comprobo que (viewer_remaining) > 0. Destruye AF,HL.
vwr_reqlen:
        ld hl,(viewer_remaining+2)
        ld a,h
        or l
        jr nz,vrl_full             ; parte alta <> 0: remaining > 65535
        ld de,(viewer_remaining)
        ld hl,VWR_BUFSIZE
        or a
        sbc hl,de
        ret nc                     ; VWR_BUFSIZE >= remaining_lo: de=remaining_lo
vrl_full:
        ld de,VWR_BUFSIZE
        ret

; vwr_sub_remaining: (viewer_remaining) -= DE (resta de 16 bits sobre un
; contador de 32). Destruye AF,HL.
vwr_sub_remaining:
        ld hl,(viewer_remaining)
        or a
        sbc hl,de
        ld (viewer_remaining),hl
        ld hl,(viewer_remaining+2)
        jr nc,vsr_done
        dec hl
vsr_done:
        ld (viewer_remaining+2),hl
        ret

; -------------------------------------------------------------
; vt_view_hex: tecla H -- visor hexadecimal+ASCII de la entrada
; seleccionada (cualquier archivo, no una carpeta), 8 bytes por fila.
; A diferencia del visor de texto, cada fila tiene tamaño fijo, asi que
; salta directamente con f_seek en vez de releer secuencialmente desde
; el principio. 6/7 desplazan fila a fila, 1/2 pantalla completa,
; ESPACIO cierra (reutiliza vwr_exit). No depende del panel.
; -------------------------------------------------------------
vt_view_hex:
        ld hl,(cur_index)
        ex de,hl
        call get_row
        ld a,(namelen)
        or a
        jp z,vt_loop
        ld a,(namebuf)
        cp '<'
        jp z,vt_loop              ; no se puede ver una carpeta en hex

        call f_open_txt
        cp 0FFh
        jp z,vt_loop

        ld (viewer_handle),a
        call f_stat
        ld a,SCRW
        ld (list_attrw),a         ; hexr_down/up reutilizan copy_row/clear_row
                                  ; (respetan este ancho); el visor es
                                  ; siempre a pantalla completa, sin panel
        xor a
        ld (hexr_zxmode),a       ; cada archivo se abre en modo ASCII
        ld hl,0
        ld (viewer_topline),hl
        call hexr_render

hexr_loop:
        call read_key
        cp 1
        jp z,vwr_exit
        cp 3
        jp z,hexr_down
        cp 4
        jp z,hexr_up
        cp 13
        jp z,hexr_pgup
        cp 14
        jp z,hexr_pgdn
        cp 25
        jp z,hexr_goto
        cp 26
        jp z,hexr_toggle_zx
        jp hexr_loop

; hexr_toggle_zx: tecla Z -- alterna la columna de la derecha entre
; interpretar cada byte como ASCII (por defecto) o como codigo de
; caracter ZX81 (via zx_to_ascii, la misma tabla que usa el listado para
; los nombres de archivo). Los graficos de bloque (01-0A) se dibujan con
; caracteres propios de la fuente (ver hexr_invloop/p42_draw_block). Los
; tokens de palabra clave de BASIC no tienen representacion, asi que
; salen como '?' (igual que hace zx_to_ascii con cualquier codigo que
; no sepa convertir).
hexr_toggle_zx:
        ld a,(hexr_zxmode)
        xor 1
        ld (hexr_zxmode),a
        call hexr_render
        jp hexr_loop

; hexr_down/hexr_up: como vt_down/vt_up en el listado principal --
; desplazan el contenido de pantalla ya pintado (copy_row) y solo
; calculan/pintan la UNA fila que queda al descubierto, en vez de
; repintar las VWR_ROWS filas enteras.
hexr_down:
        ld hl,(viewer_topline)     ; comprueba primero si existe la fila
        ld de,VWR_ROWS             ; candidata (la que entraria nueva al
        add hl,de                  ; final) antes de tocar la pantalla
        ld (hexr_row),hl
        call hex_offset
        call hex_remaining
        jr c,hexr_loop            ; no hay datos ahi: no se mueve nada

        call hexr_scroll_up

        ld hl,(viewer_topline)
        inc hl
        ld (viewer_topline),hl
        ld de,VWR_ROWS-1
        add hl,de
        ld (hexr_row),hl
        ld a,VWR_ROWS
        ld (vwr_row),a
        call clear_row             ; limpia la fila expuesta antes de pintarla
        call hexr_row_process
        jp hexr_loop

hexr_up:
        ld hl,(viewer_topline)
        ld a,h
        or l
        jr z,hexr_loop            ; ya esta en la primera fila

        call hexr_scroll_down

        ld hl,(viewer_topline)
        dec hl
        ld (viewer_topline),hl
        ld (hexr_row),hl
        ld a,1
        ld (vwr_row),a
        call clear_row
        call hexr_row_process
        jp hexr_loop

; hexr_scroll_up/hexr_scroll_down: desplazan las VWR_ROWS filas de
; pantalla ya pintadas (copia literal de scroll_list_up/scroll_list_down,
; pero con VWR_ROWS en vez de MAXVIS: el visor no tiene panel lateral).
hexr_scroll_up:
        ld b,1
hxsu_loop:
        ld d,b
        ld a,b
        inc a
        ld e,a
        push bc
        call copy_row
        pop bc
        inc b
        ld a,b
        cp VWR_ROWS
        jr c,hxsu_loop
        ret

hexr_scroll_down:
        ld b,VWR_ROWS
hxsd_loop:
        ld d,b
        ld a,b
        dec a
        ld e,a
        push bc
        call copy_row
        pop bc
        dec b
        ld a,b
        cp 2
        jr nc,hxsd_loop
        ret

; hexr_goto: tecla G -- pide una direccion hexadecimal (mismo dialogo que
; renombrar) y salta ahi. El nibble menos significativo se fuerza a 0
; (ej.: 12345668 -> 12345660), y el resultado se convierte de "byte" a
; "fila" (/HEXROW_BYTES) para (viewer_topline).
hexr_goto:
        xor a
        ld (namelen),a
        ld hl,prompt_goto
        ld b,prompt_goto_len
        ld de,hint_edit
        call show_prompt
        call text_input
        ld a,(namelen)
        or a
        jr z,hexr_goto_redraw     ; cancelado: solo repintar (el dialogo tapo el visor)

        call hex_parse
        ld a,(hexg_val)
        and 0F0h
        ld (hexg_val),a
        ld b,4                     ; /16 (byte -> fila): 4 desplazamientos
                                   ; a la derecha de los 32 bits
hxg_rshift:
        ld hl,hexg_val+3
        srl (hl)
        dec hl
        rr (hl)
        dec hl
        rr (hl)
        dec hl
        rr (hl)
        djnz hxg_rshift
        ld hl,(hexg_val)
        ld (viewer_topline),hl
hexr_goto_redraw:
        call hexr_render
        jp hexr_loop

prompt_goto:
        defb "GO TO ADDRESS (HEX)"
prompt_goto_len equ $-prompt_goto

; vwr_pgup/vwr_pgdn (VWR_ROWS) valen igual aqui: misma pantalla, mismo
; nº de filas visibles.
hexr_pgup:
        ld hl,(viewer_topline)
        ld de,VWR_ROWS
        or a
        sbc hl,de
        jr nc,hexr_pgup_ok
        ld hl,0
hexr_pgup_ok:
        ld (viewer_topline),hl
        call hexr_render
        jp hexr_loop

hexr_pgdn:
        ld hl,(viewer_topline)
        push hl                   ; guarda la fila actual por si hay que deshacer
        ld de,VWR_ROWS
        add hl,de
        ld (viewer_topline),hl
        call hexr_render
        ld a,(hexr_nrows)
        or a
        jr nz,hexr_pgdn_ok        ; se pinto al menos una fila: aceptar
        pop hl
        ld (viewer_topline),hl
        call hexr_render          ; en blanco: repinta la ultima pantalla con datos
        jp hexr_loop
hexr_pgdn_ok:
        pop hl
        jp hexr_loop

; hexr_render: repinta la pantalla completa del visor hexadecimal desde
; (viewer_topline) (fila = offset/HEXROW_BYTES). Cada fila: calcula su
; offset, comprueba con hex_remaining cuantos bytes hay realmente ahi
; (para no pedirle a CMD_f_read mas de los que quedan -- a diferencia
; del visor de texto, un binario SI puede contener bytes 0x00 reales, no
; se puede usar esa heuristica), salta con f_seek y lee con f_read.
; Para en el primer offset que ya no exista (fin real del fichero).
hexr_render:
        xor a
        ld (clock_screen_active),a
        call video_clear
        call draw_header
        ld hl,help_hex
        call draw_help

        ld hl,(viewer_topline)
        ld (hexr_row),hl
        ld a,1
        ld (vwr_row),a
        xor a
        ld (hexr_nrows),a

hexr_rowloop:
        ld a,(vwr_row)
        cp VWR_ROWS+1
        jr nc,hexr_done

        call hexr_row_process
        jr c,hexr_done            ; no queda nada desde este offset: parar

        ld hl,hexr_nrows
        inc (hl)
        ld hl,(hexr_row)
        inc hl
        ld (hexr_row),hl
        ld a,(vwr_row)
        inc a
        ld (vwr_row),a
        jr hexr_rowloop
hexr_done:
        ret


; hexr_row_process: procesa UNA fila -- (hexr_row)=numero de fila,
; (vwr_row)=fila de pantalla donde pintarla. Calcula el offset, mira con
; hex_remaining cuantos bytes hay realmente ahi (para no pedirle a
; CMD_f_read mas de los que quedan -- a diferencia del visor de texto,
; un binario SI puede contener bytes 0x00 reales, no vale la heuristica
; del byte nulo), salta con f_seek, lee con f_read y pinta con
; hexr_printrow. CY=1 si no habia ningun byte en ese offset (fin real
; del fichero): no pinta nada. Usada tanto por hexr_render (pantalla
; completa) como por hexr_down/hexr_up (una sola fila).
hexr_row_process:
        ld hl,(hexr_row)
        call hex_offset
        call hex_remaining
        ret c                     ; no queda nada desde este offset

        ld a,d
        or e
        jr nz,hrp_full            ; parte alta (16 bits) de "remaining" <> 0: fila completa
        ld a,h
        or a
        jr nz,hrp_full            ; byte alto de la parte baja <> 0: remaining >= 256, fila completa
        ld a,l
        cp HEXROW_BYTES
        jr nc,hrp_full
        jr hrp_gotcount
hrp_full:
        ld a,HEXROW_BYTES
hrp_gotcount:
        ld (hexr_count),a

        ld hl,(hexr_row)
        call hex_offset
        ld a,(viewer_handle)
        call f_seek
        cp 0FFh
        scf
        ret z                     ; fallo de seek: tratar como "sin datos"

        ld a,(hexr_count)
        ld e,a
        ld d,0
        ld a,(viewer_handle)
        call f_read

        call hexr_printrow
        or a                      ; CY=0: fila con datos, pintada
        ret

; hexr_printrow: construye en namebuf y pinta la fila actual (offset en
; (hexr_row), datos en viewer_buf, (hexr_count) bytes reales) en la fila
; de pantalla (vwr_row). Formato: "OOOOOO h1 h2 .. h16a1a2..a16" (70
; columnas justas: el offset y la columna ASCII se distinguen por color;
; huecos en blanco si la fila es la ultima y es parcial).
hexr_printrow:
        xor a
        ld (namelen),a

        ld hl,(hexr_row)
        call hex_offset           ; de:hl = offset de 32 bits; solo se
                                  ; muestran los 24 bits bajos (6 hex)
        ld a,e
        call nb_hexbyte
        ld a,h
        call nb_hexbyte
        ld a,l
        call nb_hexbyte

        ld a,(hexr_count)
        ld b,a
        or a
        jr z,hexr_hex_blanks
        ld hl,viewer_buf
hexr_hex_real:
        ld a,' '
        call nb_append
        ld a,(hl)
        inc hl
        call nb_hexbyte
        djnz hexr_hex_real
hexr_hex_blanks:
        ld a,HEXROW_BYTES
        ld hl,hexr_count
        sub (hl)
        or a
        jr z,hexr_asciicol
        ld b,a
hexr_hex_blankloop:
        ld a,' '
        call nb_append
        ld a,' '
        call nb_append
        ld a,' '
        call nb_append
        djnz hexr_hex_blankloop
hexr_asciicol:
        ld a,(hexr_count)
        ld b,a
        or a
        jr z,hexr_prdone
        ld hl,viewer_buf
hexr_asciiloop:
        ld a,(hl)
        inc hl
        ld c,a
        ld a,(hexr_zxmode)
        or a
        jr nz,hexr_ascii_zx
        ld a,c
        cp 32
        jr c,hexr_ascii_dot
        cp 127
        jr nc,hexr_ascii_dot
        jr hexr_ascii_ok
hexr_ascii_zx:
        ld a,c
        push hl                   ; zx_to_ascii usa HL para su tabla y no
        call zx_to_ascii          ; lo conserva (igual que get_row, que
        pop hl                    ; ya hace este mismo push/pop); misma
                                  ; tabla que usa el listado con los
                                  ; nombres de archivo, '?' para tokens/
                                  ; graficos sin representacion
        jr hexr_ascii_ok
hexr_ascii_dot:
        ld a,'.'
hexr_ascii_ok:
        call nb_append
        djnz hexr_asciiloop
hexr_prdone:
        ld a,NORM_ATTR
        ld (cur_attr),a
        ld a,(vwr_row)
        ld d,a
        ld e,0
        call p42_setxy
        ld hl,namebuf
        ld a,(namelen)
        ld b,a
        call p42_string
        ld a,(vwr_row)            ; el offset y la columna ASCII en su color
        ld d,0
        ld b,6
        ld c,HEX_OFS_ATTR
        call fill_row_attr_col
        ld a,(vwr_row)
        ld d,HEXR_ASCII_COL
        ld b,HEXROW_BYTES
        ld c,HEX_ASC_ATTR
        call fill_row_attr_col

        ; -- modo ZX81: pone los graficos de bloque (codigos 01-0A, los
        ; caracteres ZXG.. de la fuente, ver p42_draw_block) e invierte (por
        ; atributo, ver p42_invert_cell) los caracteres cuyo byte tenia el
        ; bit7 a 1 --
        ld a,(hexr_zxmode)
        or a
        ret z
        ld a,(hexr_count)
        or a
        ret z
        ld b,a
        ld hl,viewer_buf
        ld c,0                     ; c = indice dentro de la fila (0..hexr_count-1)
hexr_invloop:
        ld a,(hl)
        inc hl
        push bc
        push hl
        ld b,a                     ; b = byte original (con bit7)
        ld a,(vwr_row)
        ld d,a
        ld a,c
        add a,HEXR_ASCII_COL
        ld e,a                     ; d,e = fila,columna de esta celda
        ld a,b
        and 07Fh
        cp 1
        jr c,hexr_inv_noblk        ; codigo 0: no es bloque
        cp 11
        jr nc,hexr_inv_noblk       ; codigo >=11: no es bloque
        call p42_draw_block
hexr_inv_noblk:
        bit 7,b
        jr z,hexr_inv_skip
        call p42_invert_cell
hexr_inv_skip:
        pop hl
        pop bc
        inc c
        djnz hexr_invloop
        ret

; nb_append: A=caracter -> lo añade a namebuf en la posicion (namelen) y
; suma 1 a (namelen). Destruye HL,DE.
nb_append:
        push hl                  ; preserva HL/DE del que llama: se usa
        push de                  ; dentro de bucles que recorren su propio
        push af                  ; puntero (offset, viewer_buf...) en HL
        ld hl,namebuf
        ld a,(namelen)
        ld e,a
        ld d,0
        add hl,de
        pop af
        ld (hl),a
        ld a,(namelen)
        inc a
        ld (namelen),a
        pop de
        pop hl
        ret

; nb_hexbyte: A=byte -> añade sus 2 digitos hex a namebuf.
nb_hexbyte:
        push af
        rrca
        rrca
        rrca
        rrca
        call hex_nibble
        call nb_append
        pop af
        call hex_nibble
        call nb_append
        ret

; hex_nibble: A=byte (solo importan los 4 bits bajos) -> A=digito ASCII
; ('0'-'9'/'A'-'F').
hex_nibble:
        and 0Fh
        add a,'0'
        cp '9'+1
        ret c
        add a,7
        ret

; strip_brackets: quita '<' inicial y '>' final de namebuf, ajusta
; (namelen). Copia literal de explorer.asm.
strip_brackets:
        ld a,(namelen)
        sub 2
        ld (namelen),a
        or a
        ret z
        ld c,a
        ld b,0
        ld hl,namebuf+1
        ld de,namebuf
        ldir
        ret

; do_cd: HL=puntero ascii, B=longitud -> A=status
do_cd:
        ld a,3
        jp cmd_str_zx

; -------------------------------------------------------------
; vt_newfolder: tecla N -- copia literal de do_newfolder en explorer.asm.
; -------------------------------------------------------------
vt_newfolder:
        xor a
        ld (rn_oldlen),a
        ld (namelen),a            ; empezar en blanco (text_input ya no
                                 ; borra namelen solo, para poder precargar
                                 ; un nombre editable en otros casos)
        ld hl,prompt_newfolder
        ld b,prompt_newfolder_len
        ld de,hint_edit
        call show_prompt
        call text_input
        ld a,(namelen)
        or a
        jp z,vt_refresh_and_loop  ; cancelado (ESPACIO): repintar y seguir
        ld hl,namebuf
        ld b,a
        ld a,5                    ; CMD_mkdir
        call cmd_str_zx
        call do_opendir_root
vt_refresh_and_loop:
        call vt_refresh
        jp vt_loop

; -------------------------------------------------------------
; vt_delete: tecla D -- copia literal de do_delete/dd_file/dd_done en
; explorer.asm. Confirma y hace RMDIR (solo si esta vacia) o DEL segun
; sea carpeta o archivo.
; -------------------------------------------------------------
vt_delete:
        ld hl,(cur_index)
        ex de,hl
        call get_row
        ld a,(namelen)
        or a
        jp z,vt_loop
        ld hl,prompt_delete
        ld b,prompt_delete_len
        ld de,hint_delete
        call show_prompt

        ; --- el nombre del archivo/carpeta a borrar, en el campo ---
        ld a,(ti_row)
        call dlg_field
        ld hl,namebuf
        ld a,(namelen)
        cp TI_W
        jr c,vtd_nt1
        ld a,TI_W
vtd_nt1: ld b,a
        call p42_string

        call confirm_yesno
        or a
        jp z,vt_refresh_and_loop  ; no confirmado
        ld a,(namebuf)
        cp '<'
        jr nz,vtd_file
        call strip_brackets
        ld a,(namelen)
        ld b,a
        ld hl,namebuf
        ld a,6                    ; CMD_rmdir
        call cmd_str_zx
        jr vtd_done
vtd_file:
        ld a,(namelen)
        ld b,a
        ld hl,namebuf
        ld a,4                    ; CMD_del
        call cmd_str_zx
vtd_done:
        call do_opendir_root
        jp vt_refresh_and_loop

prompt_delete:
        defb "DELETE"
prompt_delete_len equ $-prompt_delete

; -------------------------------------------------------------
; vt_rename: tecla R -- copia literal de do_rename en explorer.asm.
; CMD_move(7) con origen=nombre actual y destino=nombre nuevo tecleado
; (renombrar es mover dentro del mismo directorio).
; -------------------------------------------------------------
vt_rename:
        ld hl,(cur_index)
        ex de,hl
        call get_row
        ld a,(namelen)
        or a
        jp z,vt_loop
        ld a,(namebuf)
        cp '<'
        call z,strip_brackets     ; si es carpeta, nombre "pelado" como origen
        ld a,(namelen)
        ld (rn_oldlen),a
        or a
        jr z,vtr_skipcopy
        ld c,a
        ld b,0
        ld hl,namebuf
        ld de,rn_oldname
        ldir
vtr_skipcopy:
        ld hl,prompt_rename
        ld b,prompt_rename_len
        ld de,hint_edit
        call show_prompt
        call text_input
        ld a,(namelen)
        or a
        jp z,vt_refresh_and_loop  ; cancelado
        ld hl,rn_oldname
        ld a,(rn_oldlen)
        ld b,a
        ld de,namebuf
        ld a,(namelen)
        ld c,a
        ld a,7                    ; CMD_move
        call cmd_2str_zx
        call do_opendir_root
        jp vt_refresh_and_loop

prompt_rename:
        defb "RENAME"
prompt_rename_len equ $-prompt_rename

; -------------------------------------------------------------
; vt_mark_copy (C) / vt_mark_move (X): copia literal de do_mark_copy/
; do_mark_move/do_mark/build_srcpath en explorer.asm. Guarda el
; elemento seleccionado como "portapapeles" (nombre + ruta absoluta de
; origen) para pegarlo luego en otra carpeta con vt_paste.
; -------------------------------------------------------------
vt_mark_copy:
        ld a,1
        jr vt_mark
vt_mark_move:
        ld a,2
vt_mark:
        push af
        ld hl,(cur_index)
        ex de,hl
        call get_row
        ld a,(namelen)
        or a
        jr nz,vtm_have
        pop af                   ; nada seleccionado, no marcar nada
        jp vt_loop
vtm_have:
        pop af
        ld (clip_mode),a
        ld a,(namebuf)
        cp '<'
        call z,strip_brackets
        ld a,(namelen)
        ld (clip_namelen),a
        or a
        jr z,vtm_noname
        ld c,a
        ld b,0
        ld hl,namebuf
        ld de,clip_name
        ldir
vtm_noname:
        call build_srcpath
        call vt_update_clip_icons
        jp vt_loop

; -------------------------------------------------------------
; vt_update_clip_icons: pone en rojo la C o la X de la barra de ayuda
; (fila 23) segun (clip_mode) -- 1=copiar marcado (C), 2=mover marcado
; (X), 0=nada. Se llama al marcar/pegar y despues de pintar la barra en
; vt_refresh (que las deja con el color de las demas teclas).
; -------------------------------------------------------------
vt_update_clip_icons:
        ld a,'C'
        ld c,HELPKEY_ATTR
        call vt_set_key_attr
        ld a,'X'
        ld c,HELPKEY_ATTR
        call vt_set_key_attr
        ld a,(clip_mode)
        cp 1
        ld a,'C'
        jr z,vtuci_set
        ld a,(clip_mode)
        cp 2
        ret nz
        ld a,'X'
vtuci_set:
        ld c,CLIP_ATTR

; vt_set_key_attr: A=letra de tecla, C=atributo -> cambia el atributo de
; la PRIMERA tecla resaltada de la fila 23 que sea esa letra (la de
; "Ccopy" y no la de "SPC", que va despues). Destruye AF,B,DE,HL.
vt_set_key_attr:
        ld e,a
        ld a,23
        call text_row_addr        ; hl = caracter 0 de la fila 23
        ld b,SCRW
vska1:  ld a,(hl)
        cp e
        jr nz,vska2
        ld a,h
        add a,08h                 ; su atributo
        ld h,a
        ld a,(hl)
        cp HELPKEY_ATTR
        jr z,vska3
        cp CLIP_ATTR
        jr z,vska3
        ld a,h
        sub 08h
        ld h,a
vska2:  inc hl
        djnz vska1
        ret
vska3:  ld (hl),c
        ret

; build_srcpath: clip_srcpath = directorio actual + '/' (si hace falta) +
; clip_name. Se usa como origen absoluto para copiar/mover entre
; carpetas, porque al pegar ya se habra navegado a otro directorio.
build_srcpath:
        ld de,0
        call get_row             ; namebuf/(namelen) = ruta del directorio actual
        ld hl,namebuf
        ld a,(namelen)
        ld de,clip_srcpath
        ld c,0                   ; c = bytes copiados
        or a
        jr z,bp_slash            ; longitud 0 (no deberia ocurrir), forzar barra
        ld b,a
bp_loop:
        ld a,(hl)
        ld (de),a
        inc hl
        inc de
        inc c
        djnz bp_loop
        dec de
        ld a,(de)
        inc de
        cp '/'
        jr z,bp_appendname
bp_slash:
        ld a,'/'
        ld (de),a
        inc de
        inc c
bp_appendname:
        ld hl,clip_name
        ld a,(clip_namelen)
        or a
        jr z,bp_done
        ld b,a
bp_nameloop:
        ld a,(hl)
        ld (de),a
        inc hl
        inc de
        inc c
        djnz bp_nameloop
bp_done:
        ld a,c
        ld (clip_srcpathlen),a
        ret

; -------------------------------------------------------------
; vt_paste (V): copia literal de do_paste en explorer.asm. CMD_move(7)
; o CMD_copy(8), origen=clip_srcpath (absoluto), destino=clip_name
; (relativo a la carpeta actual, ya navegada).
; -------------------------------------------------------------
vt_paste:
        ld a,(clip_mode)
        or a
        jp z,vt_loop              ; nada marcado
        push af
        ld hl,clip_srcpath
        ld a,(clip_srcpathlen)
        ld b,a
        ld de,clip_name
        ld a,(clip_namelen)
        ld c,a
        pop af
        cp 1
        jr z,vtp_copy
        ld a,7                    ; CMD_move
        jr vtp_send
vtp_copy:
        ld a,8                    ; CMD_copy
vtp_send:
        call cmd_2str_zx
        xor a
        ld (clip_mode),a
        call do_opendir_root
        jp vt_refresh_and_loop

; =============================================================
; DIALOGOS: mensaje de una linea + entrada/confirmacion de texto --
; copia literal de explorer.asm.
; =============================================================
show_prompt:
        ; HL=titulo, B=longitud, DE=linea de teclas (terminada en 0, o 0
        ; si no hay). Pinta el recuadro ENCIMA de lo que haya en pantalla
        ; (el que llama repinta despues) y deja (ti_row) en la fila del
        ; campo.
        xor a
        ld (clock_screen_active),a
        push de
        push hl
        push bc
        ld a,DLG_TOP
        ld b,DLG_BOT-DLG_TOP+1
sp1:    push af
        push bc
        ld d,DLG_L
        ld b,DLG_W
        ld c,DLG_ATTR
        call fill_row_col
        pop bc
        pop af
        inc a
        djnz sp1
        pop bc
        pop hl
        ld a,DLG_TITLE_ATTR
        ld (cur_attr),a
        ld d,DLG_TOP+1
        ld e,TI_COL
        call p42_setxy
        call p42_string
        pop hl
        ld a,h
        or l
        jr z,sp2
        ld a,DLG_HINT_ATTR
        ld (cur_attr),a
        ld d,DLG_BOT-1
        ld e,TI_COL
        call p42_setxy
sp3:    ld a,(hl)
        inc hl
        or a
        jr z,sp2
        call p42_putchar
        jr sp3
sp2:    ld a,DLG_TOP+3
        ld (ti_row),a
        ret

; dlg_field: A=fila -> el campo del dialogo en blanco (NORM_ATTR) y el
; cursor de texto al principio. Destruye AF,BC,DE,HL.
dlg_field:
        push af
        ld d,TI_COL
        ld b,TI_W
        ld c,NORM_ATTR
        call fill_row_col
        ld a,NORM_ATTR
        ld (cur_attr),a
        pop af
        ld d,a
        ld e,TI_COL
        jp p42_setxy

hint_edit:      defb "ENTER ok  SPACE cancel  SHIFT+0 del  SHIFT+1 undo",0
hint_joy:       defb "ENTER ok  SHIFT+0 del  SHIFT+1 undo  (5 keys)",0
hint_delete:    defb "ENTER delete  SPACE cancel",0
hint_anykey:    defb "Press a key",0

confirm_yesno:
        call read_key
        cp 6
        jr z,cy_yes
        cp 1
        jr z,cy_no
        jr confirm_yesno
cy_yes:
        ld a,1
        ret
cy_no:
        xor a
        ret

; text_input: parte de lo que YA haya en namebuf/(namelen) (para precargar
; un nombre editable, p.ej. al renombrar); el llamante debe poner
; (namelen)=0 si quiere empezar en blanco (p.ej. nueva carpeta). El
; cursor arranca al final del texto precargado.
TI_MAXLEN equ 40
text_input:
        ld a,(namelen)
        ld (ti_cursor),a
ti_loop:
        ld a,(ti_row)
        call dlg_field            ; campo en blanco (y sin el cursor anterior)
        ld hl,namebuf
        ld a,(namelen)
        ld b,a
        call p42_string

        call draw_cursor          ; celda del cursor en video inverso

        call read_char
        cp 13
        jp z,ti_done
        cp 32
        jr nz,ti_chkother
        push af
        ld a,(ti_allow_space)
        or a
        jr nz,ti_spaceok
        pop af
        jp ti_cancel
ti_spaceok:
        pop af
        jr ti_ischar
ti_chkother:
        cp 8
        jp z,ti_back
        cp 3
        jp z,ti_restore
        cp 1
        jp z,ti_left
        cp 2
        jp z,ti_right
ti_ischar:
        ld c,a
        ld a,(namelen)
        cp TI_MAXLEN
        jp nc,ti_loop
        call ti_insert
        jp ti_loop
ti_left:
        ld a,(ti_cursor)
        or a
        jp z,ti_loop
        dec a
        ld (ti_cursor),a
        jp ti_loop
ti_right:
        ld a,(ti_cursor)
        ld hl,namelen
        cp (hl)
        jp nc,ti_loop
        inc a
        ld (ti_cursor),a
        jp ti_loop
ti_back:
        ld a,(ti_cursor)
        or a
        jp z,ti_loop
        call ti_delete_before
        jp ti_loop
ti_restore:
        ld a,(rn_oldlen)
        ld (namelen),a
        ld (ti_cursor),a
        or a
        jp z,ti_loop
        ld c,a
        ld b,0
        ld hl,rn_oldname
        ld de,namebuf
        ldir
        jp ti_loop
ti_done:
        ret
ti_cancel:
        xor a
        ld (namelen),a
        ret

ti_insert:
        ld a,c
        ld (ti_char),a
        ld a,(namelen)
        ld hl,ti_cursor
        cp (hl)
        jr z,ti_ins_place

        ld hl,namebuf
        ld a,(namelen)
        ld e,a
        ld d,0
        add hl,de
        ld (ti_dst),hl
        dec hl
        ld (ti_src),hl

        ld a,(namelen)
        ld hl,ti_cursor
        sub (hl)
        ld c,a
        ld b,0
        ld hl,(ti_src)
        ld de,(ti_dst)
        lddr

ti_ins_place:
        ld hl,namebuf
        ld a,(ti_cursor)
        ld e,a
        ld d,0
        add hl,de
        ld a,(ti_char)
        ld (hl),a
        ld a,(namelen)
        inc a
        ld (namelen),a
        ld a,(ti_cursor)
        inc a
        ld (ti_cursor),a
        ret

ti_delete_before:
        ld a,(ti_cursor)
        dec a
        ld (ti_cursor),a
        ld (ti_delpos),a
        ld a,(namelen)
        dec a
        ld (namelen),a

        ld a,(ti_delpos)
        ld hl,namelen
        cp (hl)
        ret z

        ld hl,namebuf
        ld a,(ti_delpos)
        ld e,a
        ld d,0
        add hl,de
        ld (ti_dst),hl
        inc hl
        ld (ti_src),hl

        ld a,(namelen)
        ld hl,ti_delpos
        sub (hl)
        ld c,a
        ld b,0
        ld hl,(ti_src)
        ld de,(ti_dst)
        ldir
        ret

prompt_newfolder:
        defb "NEW FOLDER"
prompt_newfolder_len equ $-prompt_newfolder

; =============================================================
; TECLADO COMPLETO (para entrada de texto) -- copia literal de
; explorer.asm.
; =============================================================
read_char:
rc_wait:
        ld b,8
        ld hl,rc_rows
rc_scan:
        ld a,(hl)
        inc hl
        in a,(0FEh)
        ld c,a                   ; c = valor crudo leido de esta fila
        ld a,b
        cp 8
        jr nz,rc_chkrow
        ld a,c
        or 1                     ; fila 0: el bit0 (SHIFT) no cuenta como
        ld c,a                   ; tecla el solo, para no parar aqui el
                                 ; escaneo cuando SHIFT se combina con una
                                 ; tecla de OTRA fila (bug: sin esto, SHIFT
                                 ; solo ya deja la fila 0 "activa" y el
                                 ; escaneo nunca llega a mirar la fila real)
rc_chkrow:
        ld a,c
        and 1Fh
        cp 1Fh
        jr nz,rc_found
        djnz rc_scan
        jr rc_wait
rc_found:
        ld a,8
        sub b
        ld b,a
        ld a,c                   ; valor de la fila (con el bit0 ya forzado
                                 ; a 1 si era la fila 0), para que aqui se
                                 ; encuentre la tecla real y no el propio
                                 ; SHIFT cuando estan en la misma fila
        ld c,0
rc_bit:
        rrca
        jr nc,rc_gotbit
        inc c
        jr rc_bit
rc_gotbit:
        ld a,b
        add a,a
        add a,a
        add a,b
        add a,c
        ld (rc_offset),a
        ld a,0FEh
        in a,(0FEh)
        bit 0,a
        ld hl,rc_chars
        jr nz,rc_gettbl
        ld hl,rc_shiftchars
rc_gettbl:
        ld a,(rc_offset)
        ld d,0
        ld e,a
        add hl,de
        ld a,(hl)
        or a
        jr z,rc_wait
        ld (rc_pending),a
rc_relwait:
        ld b,8
        ld hl,rc_rows
rc_relscan:
        ld a,(hl)
        inc hl
        in a,(0FEh)
        ld c,a
        ld a,b
        cp 8
        jr nz,rc_relchk
        ld a,c
        or 1                     ; ignorar SHIFT (bit0 fila0): no hace falta
        ld c,a                   ; soltarlo para que se registre la tecla,
                                 ; solo la tecla real (Z, V, 5...)
rc_relchk:
        ld a,c
        and 1Fh
        cp 1Fh
        jr nz,rc_relstillp
        djnz rc_relscan
        ld a,(rc_pending)
        ret
rc_relstillp:
        jr rc_relwait

rc_rows:
        defb 0FEh,0FDh,0FBh,0F7h,0EFh,0DFh,0BFh,7Fh

rc_chars:
        defb 0,'Z','X','C','V'
        defb 'A','S','D','F','G'
        defb 'Q','W','E','R','T'
        defb '1','2','3','4','5'
        defb '0','9','8','7','6'
        defb 'P','O','I','U','Y'
        defb 13,'L','K','J','H'
        defb 32,'.','M','N','B'

rc_shiftchars:
        defb 0,':',';','?','/'
        defb 0,0,0,0,0
        defb 0,0,0,0,0
        defb 3,0,0,0,1
        defb 8,0,2,0,0
        defb 0,0,0,0,0
        defb 0,'=','+','-',0
        defb 96,',','>','<','*'

; -------------------------------------------------------------
; calc_screen_row: indice de listado -> fila de pantalla (usa win_start)
; IN: HL=indice. OUT: A=fila (0-23). Destruye HL,DE.
; -------------------------------------------------------------
calc_screen_row:
        ld de,(win_start)
        or a
        sbc hl,de
        inc hl
        ld a,l
        ret

; -------------------------------------------------------------
; scroll_list_up/scroll_list_down: desplazan las filas del listado (sin
; tocar el panel: copy_row respeta (list_attrw)).
; -------------------------------------------------------------
scroll_list_up:
        ld b,1
sl_up_loop:
        ld d,b
        ld a,b
        inc a
        ld e,a
        push bc
        call copy_row
        pop bc
        inc b
        ld a,b
        cp MAXVIS
        jr c,sl_up_loop
        ret

scroll_list_down:
        ld b,MAXVIS
sl_down_loop:
        ld d,b
        ld a,b
        dec a
        ld e,a
        push bc
        call copy_row
        pop bc
        dec b
        ld a,b
        cp 2
        jr nc,sl_down_loop
        ret

; =============================================================
; PANTALLA DE TEXTO (70 columnas): un byte por caracter en TXT_DFILE y
; su atributo en la misma posicion de TXT_ATTR (0800h mas arriba, asi
; que basta con sumarle 08h a H).
; =============================================================

; text_row_addr: A=fila -> HL=caracter 0 de esa fila. Destruye AF.
text_row_addr:
        push de
        add a,a
        ld e,a
        ld d,0
        ld hl,row_tab
        add hl,de
        ld a,(hl)
        inc hl
        ld h,(hl)
        ld l,a
        pop de
        ret

; text_addr: D=fila, E=columna -> HL=caracter. Destruye AF.
text_addr:
        ld a,d
        call text_row_addr
        ld a,e
        add a,l
        ld l,a
        ret nc
        inc h
        ret

row_tab:
        defw TXT_DFILE+1+ROWSTRIDE*0,  TXT_DFILE+1+ROWSTRIDE*1
        defw TXT_DFILE+1+ROWSTRIDE*2,  TXT_DFILE+1+ROWSTRIDE*3
        defw TXT_DFILE+1+ROWSTRIDE*4,  TXT_DFILE+1+ROWSTRIDE*5
        defw TXT_DFILE+1+ROWSTRIDE*6,  TXT_DFILE+1+ROWSTRIDE*7
        defw TXT_DFILE+1+ROWSTRIDE*8,  TXT_DFILE+1+ROWSTRIDE*9
        defw TXT_DFILE+1+ROWSTRIDE*10, TXT_DFILE+1+ROWSTRIDE*11
        defw TXT_DFILE+1+ROWSTRIDE*12, TXT_DFILE+1+ROWSTRIDE*13
        defw TXT_DFILE+1+ROWSTRIDE*14, TXT_DFILE+1+ROWSTRIDE*15
        defw TXT_DFILE+1+ROWSTRIDE*16, TXT_DFILE+1+ROWSTRIDE*17
        defw TXT_DFILE+1+ROWSTRIDE*18, TXT_DFILE+1+ROWSTRIDE*19
        defw TXT_DFILE+1+ROWSTRIDE*20, TXT_DFILE+1+ROWSTRIDE*21
        defw TXT_DFILE+1+ROWSTRIDE*22, TXT_DFILE+1+ROWSTRIDE*23

; copy_row: D=fila destino, E=fila origen -> copia (list_attrw)
; caracteres con sus atributos. Destruye AF,BC,DE,HL.
copy_row:
        ld a,e
        call text_row_addr
        push hl
        ld a,d
        call text_row_addr
        ex de,hl                  ; de = destino
        pop hl                    ; hl = origen
        push hl
        push de
        ld a,(list_attrw)
        ld c,a
        ld b,0
        ldir
        pop de
        pop hl
        ld a,h
        add a,08h
        ld h,a
        ld a,d
        add a,08h
        ld d,a
        ld a,(list_attrw)
        ld c,a
        ld b,0
        ldir
        ret

; clear_row: A=fila -> (list_attrw) espacios con NORM_ATTR.
clear_row:
        ld c,NORM_ATTR
        push af
        ld a,(list_attrw)
        ld b,a
        pop af
        ld d,0
        jr fill_row_col

; blank_row: A=fila, C=atributo -> fila entera en blanco.
blank_row:
        ld b,SCRW
        ld d,0
; fill_row_col: A=fila, D=columna, B=anchura, C=atributo -> espacios con
; ese atributo. Destruye AF,BC,DE,HL.
fill_row_col:
        push bc
        push de
        call text_row_addr
        pop de
        ld e,d
        ld d,0
        add hl,de
        pop bc
frc1:   ld (hl),' '
        ld a,h
        add a,08h
        ld h,a
        ld (hl),c
        sub 08h
        ld h,a
        inc hl
        djnz frc1
        ret

; -------------------------------------------------------------
; redraw_row_attr: repintado parcial de UNA fila del listado (usado por
; vt_down/vt_up sin cruzar pagina). Respeta (list_maxchars)/(list_attrw)
; -- igual que vlist_loop en vt_refresh -- para no invadir el panel de
; configuracion cuando esta abierto. A=fila, DE=indice, C=atributo.
; -------------------------------------------------------------
redraw_row_attr:
        ld (rra_row),a
        push bc
        call get_row
        pop bc
        ld a,c
        ld (rra_attr),a
        ld (cur_attr),a

        ld a,(rra_row)
        call clear_row            ; lo que quedara del nombre anterior

        ld a,(rra_row)
        ld d,a
        ld e,0
        call p42_setxy

        ld hl,namebuf
        ld a,(namelen)
        ld b,a
        ld a,(list_maxchars)
        cp b
        jr nc,rra_t1
        ld b,a
rra_t1: call p42_string

        ld a,(rra_row)
        push af
        ld a,(list_attrw)
        ld b,a
        ld a,(rra_attr)
        ld c,a
        pop af
        call fill_row_attr_n
        ret
rra_row:  defb 0
rra_attr: defb 0

; fill_row_attr: A=fila, C=attr -> la fila entera (solo atributos)
fill_row_attr:
        ld b,SCRW
        ld d,0
        jr fill_row_attr_col

; fill_row_attr_n: como fill_row_attr pero con anchura B desde la
; columna 0. Usada por el listado cuando el panel de config esta activo,
; para no pintar el resalte de seleccion encima del panel.
fill_row_attr_n:
        ld d,0

; fill_row_attr_col: A=fila, D=columna inicial, B=anchura, C=attr ->
; destruye AF,BC,DE,HL
fill_row_attr_col:
        push de
        call text_row_addr
        pop de
        ld a,h
        add a,08h
        ld h,a                    ; hl = atributo de la columna 0
        ld e,d
        ld d,0
        add hl,de
fra1:   ld (hl),c
        inc hl
        djnz fra1
        ret

; video_clear: pantalla en blanco con NORM_ATTR
video_clear:
        ld hl,TXT_DFILE
        ld de,TXT_DFILE+1
        ld bc,ROWSTRIDE*24
        ld (hl),' '
        ldir
        ld hl,TXT_ATTR
        ld de,TXT_ATTR+1
        ld bc,ROWSTRIDE*24
        ld (hl),NORM_ATTR
        ldir
        ret

; draw_header: fila 0, franja negra con el logo centrado (el reloj lo
; pone vt_update_clock)
draw_header:
        xor a
        ld c,HDR_ATTR
        call blank_row
        ld a,LOGO_ATTR
        ld (cur_attr),a
        ld d,0
        ld e,LOGO_COL
        call p42_setxy
        ld b,28                   ; contador en B: p42_putchar no
        ld a,LOGO                 ; conserva A, pero si BC
dhd1:   push af
        call p42_putchar
        pop af
        inc a
        djnz dhd1
        ret

; draw_help: HL=texto terminado en 0 -> fila 23. Un '|' no se pinta y
; marca el caracter siguiente como tecla (HELPKEY_ATTR).
draw_help:
        push hl
        ld a,23
        ld c,HELP_ATTR
        call blank_row
        ld d,23
        ld e,0
        call p42_setxy
        pop hl
dh1:    ld a,(hl)
        inc hl
        or a
        ret z
        ld c,HELP_ATTR
        cp '|'
        jr nz,dh2
        ld a,(hl)
        inc hl
        ld c,HELPKEY_ATTR
dh2:    push hl
        push af
        ld a,c
        ld (cur_attr),a
        pop af
        call p42_putchar
        pop hl
        jr dh1

help_main:      defb " |8open |5up |Nnew |Ddel |Rren |Ccopy |Xcut |Vpaste |Eedit |Ssetup |Kkeys |S|P|Cexit",0
help_back:      defb " any key: back to the list",0
help_viewer:    defb " |6|7 line   |1|2 page   |S|P|A|C|E exit",0
help_hex:       defb " |6|7 line   |1|2 page   |G goto   |Z ZX81/ASCII   |S|P|A|C|E exit",0

; -------------------------------------------------------------
; draw_cursor: la celda (ti_row, ti_cursor) en video inverso, por
; atributo. text_input vuelve a pintar la fila en cada tecla, asi que el
; cursor anterior desaparece solo.
; -------------------------------------------------------------
draw_cursor:
        ld a,(ti_row)
        ld d,a
        ld a,(ti_cursor)
        add a,TI_COL
        ld e,a
        call text_addr
        ld a,h
        add a,08h
        ld h,a
        ld (hl),CURSOR_ATTR
        ret

; -------------------------------------------------------------
; p42_invert_cell: D=fila, E=columna -> la celda en video inverso
; (intercambia papel y tinta) -- para el modo ZX81 inverso (bit 7) del
; visor hexadecimal. Destruye AF,HL.
; -------------------------------------------------------------
p42_invert_cell:
        call text_addr
        ld a,h
        add a,08h
        ld h,a
        ld a,(hl)
        rrca
        rrca
        rrca
        rrca
        ld (hl),a
        ret

; -------------------------------------------------------------
; p42_draw_block: A=codigo de bloque ZX81 (1-0Ah), D=fila, E=columna ->
; pone en la celda el grafico de bloque (los caracteres ZXG.. de la
; fuente, copiados de la ROM). Destruye AF,HL.
; -------------------------------------------------------------
p42_draw_block:
        push af
        call text_addr
        pop af
        add a,ZXG-1
        ld (hl),a
        ret

; -------------------------------------------------------------
; video_text_on: fuente, pantalla en blanco y modo de 70 columnas con
; atributos por caracter y 256 caracteres. Al arrancar y al volver del
; visor de .SCR (que usa el bloque entero para el bitmap).
; -------------------------------------------------------------
video_text_on:
        ld hl,TXT_FONT            ; 0-31: propios (el resto a cero)
        ld de,TXT_FONT+1
        ld bc,32*8-1
        ld (hl),0
        ldir
        ld hl,glyphs
        ld de,TXT_FONT
        ld bc,GLYPHS_LEN
        ldir
        ld hl,01E00h+8            ; 16-25: graficos 1-10 de la ROM
        ld de,TXT_FONT+ZXG*8
        ld bc,10*8
        ldir
        ld hl,FONTBASE            ; 32-127: la de Spectrum
        ld de,TXT_FONT+32*8
        ld bc,96*8
        ldir
        ld hl,logo_glyphs         ; 128-155: el logo
        ld de,TXT_FONT+LOGO*8
        ld bc,LOGO_LEN
        ldir
        ld a,TXT_FONT/256
        ld i,a
        call video_clear
        ld hl,TXT_DFILE           ; pantalla y atributos alternativos
        ld (2096),hl
        ld a,170
        ld (2098),a
        ld hl,TXT_ATTR
        ld (2059),hl
        ld a,170
        ld (2061),a
        ld bc,7fefh
        ld a,CHROMA_TEXT
        out (c),a
        xor a
        ld (2094),a               ; sin desplazar ni recortar
        ld (2095),a
        ld a,173                  ; Superfast texto, 70 columnas
        ld (2045),a
        ld a,41h                  ; CMD_256C
        jp mcu_send

; video_spectrum_on: para el visor de .SCR -- HiRes Spectrum con el
; bitmap en VIDBASE (y los atributos detras, en VIDBASE+1800h).
video_spectrum_on:
        ld hl,VIDBASE
        ld de,VIDBASE+1
        ld bc,17ffh
        ld (hl),0
        ldir
        ld hl,VIDBASE+1800h
        ld de,VIDBASE+1801h
        ld bc,2ffh
        ld (hl),038h              ; papel blanco, tinta negra (Spectrum)
        ldir
        ld a,85
        ld (2098),a               ; sin pantalla ni atributos alternativos
        ld (2061),a
        xor a
        ld (2043),a               ; HFILE
        ld a,VIDBASE_HI
        ld (2044),a
        ld bc,7fefh
        ld a,20h                  ; Chroma81 (atributos Spectrum)
        out (c),a
        ld a,172                  ; Superfast HiRes Spectrum
        ld (2045),a
        ret

; video_off: video nativo, I de la ROM, el modo CHR elegido en el panel y
; el bloque VIDBLOCK a su pagina. Antes de volver al BASIC.
video_off:
        ld a,85
        ld (2045),a               ; Superfast OFF (apaga tambien la pantalla
                                  ; y los atributos alternativos)
        ld bc,7fefh
        xor a
        out (c),a                 ; Chroma81 OFF
        ld a,1Eh                  ; I de la ROM ANTES de volver al video
        ld i,a                    ; nativo (con I >= $40 el WRX va forzado)
        ld a,(cfg_chr128)
        or a
        ld a,28                   ; CMD_chars64 (apaga tambien los 256)
        jr z,vo1
        ld a,27                   ; CMD_chars128
vo1:    call mcu_send
        ld a,VIDBLOCK
        ld e,VIDRESTOREPAGE
        jp mcu_map                ; bloque VIDBLOCK a su pagina identidad

; -------------------------------------------------------------
; mcu_map: asigna una pagina fisica a un bloque de 8K (puerto $E7)
; Entrada: A = bloque (0-7), E = pagina (0-63)
; -------------------------------------------------------------
mcu_map:
        push bc
        and 7
        ld c,a
        ld a,e
        ld b,a                  ; B = pagina completa (paginacion completa)
        ld a,e
        and 31
        rlca
        rlca
        rlca
        or c
        ld c,0E7h
        out (c),a
        pop bc
        ret

; spec_bmp_addr / spec_attr_addr: A=fila (0-23) de la pantalla Spectrum
; del visor de .SCR -> HL=linea 0 de su bitmap / su fila de atributos
spec_bmp_addr:
        ld l,a
        and 0F8h
        add a,VIDBASE_HI
        ld h,a
        ld a,l
        and 7
        rrca
        rrca
        rrca
        ld l,a
        ret

spec_attr_addr:
        ld h,0
        ld l,a
        add hl,hl
        add hl,hl
        add hl,hl
        add hl,hl
        add hl,hl
        ld a,h
        add a,ATTRBASE_HI
        ld h,a
        ret

; =============================================================
; TECLADO (matriz estilo Spectrum, puerto $FE) -- copia literal de
; explorer.asm (read_key completo, aunque aqui solo actuemos sobre
; 1=ESPACIO, 3=6, 4=7; el resto de codigos se generan igual pero se
; ignoran en vt_loop).
; =============================================================
read_key:
rk_wait:
        ; -- reloj en vivo: si estamos en el listado principal (no en un
        ; visor/dialogo), cuenta pasadas de este bucle y cada
        ; CLOCK_TICK_THRESHOLD lo repinta -- valor a calibrar en hardware
        ; real, no hay temporizador de verdad, es solo un contador --
        ld a,(clock_screen_active)
        or a
        jr z,rk_noclock
        ld hl,(clock_counter)
        inc hl
        ld (clock_counter),hl
        ld de,CLOCK_TICK_THRESHOLD
        or a
        sbc hl,de
        jr c,rk_noclock
        ld hl,0
        ld (clock_counter),hl
        call vt_update_clock
rk_noclock:
        ld a,0FEh                ; SHIFT+1 = reset del filtro de listado,
        in a,(0FEh)               ; SHIFT+E = fichero nuevo (comprobacion
        bit 0,a                   ; aparte porque SHIFT no se rastrea como
        jr nz,rk_normal1           ; tecla propia en el resto de la matriz)
        ld a,0F7h
        in a,(0FEh)
        bit 0,a
        jp z,rk_resetfilter
        ld a,0FBh
        in a,(0FEh)
        bit 2,a
        jp z,rk_newfile
rk_normal1:
        ld a,0F7h
        in a,(0FEh)
        bit 0,a
        jp z,rk_pgup
        bit 1,a
        jp z,rk_pgdn
        bit 4,a
        jp z,rk_k5
        ld a,0EFh
        in a,(0FEh)
        bit 4,a
        jp z,rk_k6
        bit 3,a
        jp z,rk_k7
        bit 2,a
        jp z,rk_k8
        ld a,0BFh
        in a,(0FEh)
        bit 0,a
        jp z,rk_enter
        bit 4,a
        jp z,rk_h
        bit 3,a
        jp z,rk_j
        bit 2,a
        jp z,rk_k
        bit 1,a
        jp z,rk_l
        ld a,7Fh
        in a,(0FEh)
        bit 0,a
        jp z,rk_space
        bit 3,a
        jp z,rk_n
        bit 2,a
        jp z,rk_m
        bit 1,a
        jp z,rk_dot
        ld a,0FDh
        in a,(0FEh)
        bit 2,a
        jp z,rk_d
        bit 1,a
        jp z,rk_s
        bit 3,a
        jp z,rk_f
        bit 0,a
        jp z,rk_a
        bit 4,a
        jp z,rk_g
        ld a,0FBh
        in a,(0FEh)
        bit 3,a
        jp z,rk_r
        bit 1,a
        jp z,rk_w
        bit 4,a
        jp z,rk_t
        bit 2,a
        jp z,rk_e
        ld a,0DFh
        in a,(0FEh)
        bit 4,a
        jp z,rk_y
        bit 3,a
        jp z,rk_u
        bit 2,a
        jp z,rk_i
        ld a,0FEh
        in a,(0FEh)
        bit 3,a
        jp z,rk_c
        bit 2,a
        jp z,rk_x
        bit 4,a
        jp z,rk_v
        bit 1,a
        jp z,rk_z
        jp rk_wait
rk_k5:
        ld a,2
        jr rk_deb
rk_k6:
        ld a,3
        jr rk_deb
rk_k7:
        ld a,4
        jr rk_deb
rk_k8:
        ld a,5
        jr rk_deb
rk_enter:
        ld a,6
        jr rk_deb
rk_space:
        ld a,1
        jr rk_deb
rk_n:
        ld a,7
        jr rk_deb
rk_d:
        ld a,8
        jr rk_deb
rk_s:
        ld a,15
        jr rk_deb
rk_w:
        ld a,16
        jr rk_deb
rk_f:
        ld a,17
        jr rk_deb
rk_m:
        ld a,18
        jr rk_deb
rk_h:
        ld a,19
        jr rk_deb
rk_j:
        ld a,20
        jr rk_deb
rk_t:
        ld a,21
        jr rk_deb
rk_y:
        ld a,22
        jr rk_deb
rk_u:
        ld a,23
        jr rk_deb
rk_i:
        ld a,27
        jr rk_deb
rk_resetfilter:
        ld a,28
        jr rk_deb
rk_newfile:
        ld a,33
        jr rk_deb
rk_dot:
        ld a,29
        jr rk_deb
rk_a:
        ld a,24
        jr rk_deb
rk_g:
        ld a,25
        jr rk_deb
rk_r:
        ld a,9
        jr rk_deb
rk_c:
        ld a,10
        jr rk_deb
rk_x:
        ld a,11
        jr rk_deb
rk_v:
        ld a,12
        jr rk_deb
rk_z:
        ld a,26
        jr rk_deb
rk_e:
        ld a,30
        jr rk_deb
rk_k:
        ld a,31
        jr rk_deb
rk_l:
        ld a,32
        jr rk_deb
rk_pgup:
        ld a,13
        jr rk_deb
rk_pgdn:
        ld a,14
rk_deb:
        push af
; espera a que se suelten TODAS las teclas (las 5 de cada fila): antes
; solo miraba las que habia al escribirlo, y con las nuevas (E, K, L)
; volvia con la tecla aun pulsada -- la ayuda se cerraba sola y el
; editor arrancaba con la E pulsada
rk_rel:
        ld a,0F7h
        in a,(0FEh)
        and 1Fh
        cp 1Fh
        jr nz,rk_stillp
        ld a,0EFh
        in a,(0FEh)
        and 1Fh
        cp 1Fh
        jr nz,rk_stillp
        ld a,0BFh
        in a,(0FEh)
        and 1Fh
        cp 1Fh
        jr nz,rk_stillp
        ld a,7Fh
        in a,(0FEh)
        and 1Fh
        cp 1Fh
        jr nz,rk_stillp
        ld a,0FDh
        in a,(0FEh)
        and 1Fh
        cp 1Fh
        jr nz,rk_stillp
        ld a,0FBh
        in a,(0FEh)
        and 1Fh
        cp 1Fh
        jr nz,rk_stillp
        ld a,0DFh
        in a,(0FEh)
        and 1Fh
        cp 1Fh
        jr nz,rk_stillp
        ld a,0FEh
        in a,(0FEh)
        and 1Fh
        cp 1Fh
        jr nz,rk_stillp
        jr rk_relok
rk_stillp:
        jr rk_rel
rk_relok:
        pop af
        ret

; =============================================================
; PROTOCOLO MCU (puertos $A7 datos / $AF reloj) -- copia literal de
; explorer.asm.
; =============================================================
mcu_send:
        push bc
        ld b,a
        in a,(0AFh)
        ld c,a
        ld a,b
        out (0A7h),a
msw:    in a,(0AFh)
        xor c
        jp p,msw
        pop bc
        ret

mcu_recv:
        push bc
        in a,(0AFh)
        ld c,a
        in a,(0A7h)
        ld b,a
mrw:    in a,(0AFh)
        xor c
        jp p,mrw
        ld a,b
        pop bc
        ret

send_pascal_zx:
        ld a,b
        call mcu_send
        ld a,b
        or a
        ret z
spz_loop:
        ld a,(hl)
        inc hl
        push bc
        push hl
        call ascii_to_zx
        call mcu_send
        pop hl
        pop bc
        djnz spz_loop
        ret

cmd_str_zx:
        push hl
        push bc
        call mcu_send
        pop bc
        pop hl
        call send_pascal_zx
        jp mcu_recv

; cmd_2str_zx: A=comando; HL=ptr1,B=longitud1; DE=ptr2,C=longitud2 -> A=status
; (para comandos de dos cadenas: CMD_move=7, CMD_copy=8). Copia literal
; de explorer.asm.
cmd_2str_zx:
        push hl
        push bc
        push de
        call mcu_send
        pop de
        pop bc
        pop hl
        call send_pascal_zx
        ex de,hl
        ld b,c
        call send_pascal_zx
        jp mcu_recv

; =============================================================
; PROTOCOLO DE FICHEROS (CMD_f_open/seek/read/close, 53/54/55/57) -- para
; el visor de texto. A diferencia de MOVE/COPY/etc, cmd_f_open (53)
; espera el nombre en ASCII "puro" (ver do_f_open en COMMANDS.cpp: con
; convert=false NO pasa los bytes por asc81_to_ascii), asi que aqui NO se
; convierte a charset ZX81 -- send_pascal_raw/cmd_str_raw mandan los
; bytes tal cual, a diferencia de send_pascal_zx/cmd_str_zx.
; =============================================================
send_pascal_raw:
        ld a,b
        call mcu_send
        ld a,b
        or a
        ret z
spr_loop:
        ld a,(hl)
        inc hl
        push bc
        push hl
        call mcu_send
        pop hl
        pop bc
        djnz spr_loop
        ret

cmd_str_raw:
        push hl
        push bc
        call mcu_send
        pop bc
        pop hl
        call send_pascal_raw
        jp mcu_recv

; f_open_txt: abre (namebuf,namelen) en modo ASCII -> A=handle (0-3) o
; 0xFF si no existe/error.
f_open_txt:
        ld hl,namebuf
        ld a,(namelen)
        ld b,a
        ld a,53                   ; CMD_f_open
        jp cmd_str_raw

; f_rewind: A=handle -> A=status. Vuelve al principio del fichero
; (offset 0); es el unico uso que necesita el visor de texto, asi que no
; hay una f_seek general con offset variable.
f_rewind:
        ld c,a
        ld a,54                   ; CMD_f_seek
        call mcu_send
        ld a,c
        call mcu_send
        xor a
        call mcu_send             ; offset byte 0
        xor a
        call mcu_send             ; offset byte 1 -- mcu_send NO conserva A
        xor a                     ; (al salir deja el resultado del XOR de
        call mcu_send             ; espera, no el byte enviado), asi que hay
        xor a                     ; que recargarlo antes de CADA llamada
        call mcu_send             ; offset byte 3 (MSB)
        jp mcu_recv

; f_read: A=handle, DE=cuenta -> llena (viewer_buf) con DE bytes
; (con relleno de ceros si el fichero se acaba antes) y devuelve A=status
; (0=completa, 1=corta/EOF, 0xFF=error).
f_read:
        ld c,a
        ld a,55                   ; CMD_f_read
        call mcu_send
        ld a,c
        call mcu_send
        ld a,e
        call mcu_send
        ld a,d
        call mcu_send
        ld hl,viewer_buf
fr_loop:
        push hl
        push de
        call mcu_recv
        pop de
        pop hl
        ld (hl),a
        inc hl
        dec de
        ld a,d
        or e
        jr nz,fr_loop
        jp mcu_recv

; f_close: A=handle -> A=status.
f_close:
        ld c,a
        ld a,57                   ; CMD_f_close
        call mcu_send
        ld a,c
        call mcu_send
        jp mcu_recv

; f_stat: A=handle (ya abierto) -> deja en (viewer_size) el tamaño (4
; bytes LE) y descarta fecha/hora (2+2 bytes, aun sin uso); A=status.
f_stat:
        ld c,a
        ld a,59                   ; CMD_f_stat
        call mcu_send
        ld a,c
        call mcu_send
        ld hl,viewer_size
        ld b,4
fst_loop1:
        push hl
        push bc
        call mcu_recv
        pop bc
        pop hl
        ld (hl),a
        inc hl
        djnz fst_loop1
        ld b,4                    ; fecha(2)+hora(2): se leen y se descartan
fst_loop2:
        push bc
        call mcu_recv
        pop bc
        djnz fst_loop2
        jp mcu_recv

; rtc_fetch: HL=destino (22 bytes) -- CMD_rtc (50) en modo lectura
; (parametro de longitud 0): rellena HL con "yyyy-mm-dd hh:mm:ss.cc" ya
; convertido de ZX81 a ASCII (zx_to_ascii, igual que los nombres de
; archivo que devuelve get_row), y descarta el byte de estado final.
; Destruye AF,BC,HL.
rtc_fetch:
        ld a,50                   ; CMD_rtc
        call mcu_send
        xor a
        call mcu_send             ; longitud de parametro = 0 (leer)
        ld b,22
rtcf_loop:
        push bc
        push hl
        call mcu_recv
        call zx_to_ascii
        pop hl
        ld (hl),a
        inc hl
        pop bc
        djnz rtcf_loop
        jp mcu_recv               ; status final (descartado)

; f_seek: A=handle, DE:HL=offset de 32 bits (DE=palabra alta, HL=palabra
; baja) -> A=status. Usado por el visor hexadecimal para saltar
; directamente a la fila pedida (a diferencia del visor de texto, que
; siempre relee desde el principio porque las lineas son de longitud
; variable). Recarga A antes de cada mcu_send (ver el bug de f_rewind:
; mcu_send no conserva A entre llamadas).
f_seek:
        ld c,a
        push hl
        push de
        ld a,54                   ; CMD_f_seek
        call mcu_send
        ld a,c
        call mcu_send
        pop de
        pop hl
        ld a,l
        call mcu_send             ; offset byte0 (LSB)
        ld a,h
        call mcu_send             ; offset byte1
        ld a,e
        call mcu_send             ; offset byte2
        ld a,d
        call mcu_send             ; offset byte3 (MSB)
        jp mcu_recv

; hex_offset: HL=fila (16 bits) -> DE:HL=offset de 32 bits (fila*
; HEXROW_BYTES = fila*16), DE=palabra alta, HL=palabra baja. Destruye AF.
hex_offset:
        ld de,0
        add hl,hl
        rl e
        rl d
        add hl,hl
        rl e
        rl d
        add hl,hl
        rl e
        rl d
        add hl,hl
        rl e
        rl d
        ret

; hex_remaining: DE:HL=offset (32 bits) -> si offset>=(viewer_size), CY=1
; (no quedan bytes: fin del fichero). Si no, DE:HL=(viewer_size)-offset
; (bytes reales desde ese offset) y CY=0. Destruye AF.
hex_remaining:
        ld (hexr_tmplo),hl
        ld (hexr_tmphi),de
        ld hl,(viewer_size)
        ld de,(hexr_tmplo)
        or a
        sbc hl,de
        ld (hexr_tmplo),hl
        ld hl,(viewer_size+2)
        ld de,(hexr_tmphi)
        sbc hl,de
        ld (hexr_tmphi),hl
        ret c                     ; offset > tamaño: no quedan bytes

        ld a,h
        or l
        ld b,a
        ld hl,(hexr_tmplo)
        ld a,h
        or l
        or b
        jr z,hxr_none             ; remaining == 0 exacto: tampoco quedan
        ld hl,(hexr_tmplo)
        ld de,(hexr_tmphi)
        or a
        ret
hxr_none:
        scf
        ret

; hex_parse: namebuf/(namelen) (digitos hex ASCII, mayuscula o minuscula;
; los caracteres que no sean 0-9/A-F/a-f cuentan como 0) -> (hexg_val) =
; valor de 32 bits (4 bytes, LSB primero). Usado por hexr_goto. Destruye
; AF,BC,DE,HL.
hex_parse:
        xor a
        ld (hexg_val),a
        ld (hexg_val+1),a
        ld (hexg_val+2),a
        ld (hexg_val+3),a
        ld a,(namelen)
        or a
        ret z
        ld c,a                    ; c = nº de caracteres que quedan
        ld b,0                    ; b = indice actual en namebuf
hxp_loop:
        push bc                   ; (hexg_val) <<= 4 (multiplicar por 16,
        ld b,4                    ; 4 desplazamientos de 1 bit con acarreo
hxp_shift:                        ; entre los 4 bytes)
        ld hl,hexg_val
        sla (hl)
        inc hl
        rl (hl)
        inc hl
        rl (hl)
        inc hl
        rl (hl)
        djnz hxp_shift
        pop bc

        push bc
        ld hl,namebuf
        ld a,b
        ld e,a
        ld d,0
        add hl,de
        ld a,(hl)
        pop bc
        call hex_digit_val         ; a = valor del digito (0-15), 0 si no es hex

        push bc                    ; (hexg_val) += a (suma de 32 bits con acarreo)
        ld hl,hexg_val
        add a,(hl)
        ld (hl),a
        ld a,0
        adc a,0
        inc hl
        add a,(hl)
        ld (hl),a
        ld a,0
        adc a,0
        inc hl
        add a,(hl)
        ld (hl),a
        ld a,0
        adc a,0
        inc hl
        add a,(hl)
        ld (hl),a
        pop bc

        inc b
        dec c
        ld a,c
        or a
        jr nz,hxp_loop
        ret

; hex_digit_val: A=caracter ASCII -> A=valor 0-15 si es '0'-'9'/'A'-'F'/
; 'a'-'f'; 0 en cualquier otro caso.
hex_digit_val:
        cp '0'
        jr c,hdv_zero
        cp '9'+1
        jr nc,hdv_notdigit
        sub '0'
        ret
hdv_notdigit:
        cp 'A'
        jr c,hdv_zero
        cp 'F'+1
        jr nc,hdv_lower
        sub 'A'-10
        ret
hdv_lower:
        cp 'a'
        jr c,hdv_zero
        cp 'f'+1
        jr nc,hdv_zero
        sub 'a'-10
        ret
hdv_zero:
        xor a
        ret

do_opendir:
        ld a,16
        jp cmd_str_zx

do_opendir_root:
        ld a,(list_filter_len)
        or a
        jr nz,dor_filtered
        ld hl,starmask
        ld b,1
        jp do_opendir
dor_filtered:
        ld hl,list_filter
        ld b,a
        jp do_opendir
starmask: defb "*"

get_row:
        ld a,18
        call mcu_send
        ld a,e
        call mcu_send
        ld a,d
        call mcu_send
        call mcu_recv
        or a
        ld (namelen),a
        jr z,gr_status
        ld b,a
        ld hl,namebuf
gr_loop:
        push bc
        call mcu_recv
        push hl
        call zx_to_ascii
        pop hl
        pop bc
        ld (hl),a
        inc hl
        djnz gr_loop
gr_status:
        call mcu_recv
        ld (mcustatus),a
        ret

; -------------------------------------------------------------
; Conversion ASCII <-> codigos de caracter ZX81 (copia literal)
; -------------------------------------------------------------
ascii_to_zx:
        cp 97
        jr c,atz_letdig
        cp 123
        jr nc,atz_letdig
        sub 32
atz_letdig:
        cp 65
        jr c,atz_digcheck
        cp 91
        jr nc,atz_digcheck
        sub 65
        add a,38
        ret
atz_digcheck:
        cp 48
        jr c,atz_sym
        cp 58
        jr nc,atz_sym
        sub 48
        add a,28
        ret
atz_sym:
        ld b,a
        ld hl,atz_symtable
atz_symloop:
        ld a,(hl)
        or a
        jr z,atz_notfound
        cp b
        jr nz,atz_symskip
        inc hl
        ld a,(hl)
        ret
atz_symskip:
        inc hl
        inc hl
        jr atz_symloop
atz_notfound:
        ld a,15                 ; '?'
        ret

atz_symtable:
        defb 32,0
        defb 34,11
        defb 38,12
        defb 36,13
        defb 58,14
        defb 63,15
        defb 40,16
        defb 41,17
        defb 62,18
        defb 60,19
        defb 61,20
        defb 43,21
        defb 45,22
        defb 42,23
        defb 47,24
        defb 59,25
        defb 44,26
        defb 46,27
        defb 0,0

zx_to_ascii:
        and 07Fh
        cp 38
        jr c,zta_chk28
        cp 64
        jr nc,zta_unknown
        sub 38
        add a,65
        ret
zta_chk28:
        cp 28
        jr c,zta_chk0
        sub 28
        add a,48
        ret
zta_chk0:
        or a
        jr nz,zta_chk11
        ld a,32
        ret
zta_chk11:
        cp 11
        jr nc,zta_c11old           ; 11 en adelante sigue la logica de antes
        ld a,32                    ; 01-0A: bloque grafico, se dibuja aparte
        ret                        ; (ver hexr_invloop/p42_draw_block); aqui,
                                    ; celda en blanco para no mezclar bitmaps
zta_c11old:
        cp 28
        jr nc,zta_unknown
        sub 11
        ld hl,zta_tbl
        ld d,0
        ld e,a
        add hl,de
        ld a,(hl)
        ret
zta_unknown:
        ld a,63                 ; '?'
        ret

zta_tbl:
        defb 34,38,36,58,63,40,41,62,60,61,43,45,42,47,59,44,46

; =============================================================
; ESCRITURA DE TEXTO (70 columnas). Los nombres p42_* vienen de la
; version de 42 columnas; ahora cada caracter es un byte en TXT_DFILE y
; su atributo (cur_attr) en TXT_ATTR.
; =============================================================
p42_setxy:
        ld (xycoords),de
        ret

p42_newline:
        ld de,(xycoords)
        call nxtline
        ld (xycoords),de
        ret

; p42_string: HL=texto, B=longitud. Se salta los controles y los
; caracteres de 128 en adelante; 13 = salto de linea.
p42_string:
        ld a,b
        or a
        ret z
p42s_loop:
        ld a,(hl)
        inc hl
        push hl
        push bc
        cp 13
        jr z,p42s_nl
        cp 32
        jr c,p42s_skip
        cp 128
        jr nc,p42s_skip
        call p42_putchar
        jr p42s_skip
p42s_nl:
        call p42_newline
p42s_skip:
        pop bc
        pop hl
        djnz p42s_loop
        ret

; p42_putchar: A=caracter (cualquier codigo 0-255) en (xycoords), con
; (cur_attr), y avanza. Conserva BC, DE y HL.
p42_putchar:
        push hl
        push de
        push bc
        ld c,a
        call p42_testcoords       ; de = posicion (ajustada si se salia)
        call text_addr
        ld (hl),c
        ld a,h
        add a,08h
        ld h,a
        ld a,(cur_attr)
        ld (hl),a
        inc e
        ld (xycoords),de
        pop bc
        pop de
        pop hl
        ret

p42_testcoords:
        ld de,(xycoords)
nxtchar:
        ld a,e
        cp SCRW
        jr c,ycoord
nxtline:
        inc d
        ld e,0
ycoord:
        ld a,d
        cp 24
        ret c
        ld d,0
        ret

; =============================================================
; DATOS / VARIABLES DE TRABAJO
; =============================================================
cur_index:      defw 1
win_start:      defw 1
row_ptr:        defw 0
cfg_panel:      defb 0          ; 0=panel oculto, 1=panel de config visible
list_maxchars:  defb 42         ; ancho de texto vigente del listado (recalc. en vt_refresh)
list_attrw:     defb 32         ; ancho de atributo vigente del listado (idem)

clock_screen_active: defb 0     ; 1 = el listado principal esta activo (row0
                                 ; muestra el reloj); 0 en visores/dialogos
clock_counter:       defw 0     ; contador de pasadas de rk_wait (ver read_key)

; -- filtro de listado (tecla W, solo con el panel oculto): wildcards que
; se añaden a la carpeta actual al pedir OPENDIR2 (ver do_opendir_root).
; Se resetea al navegar de verdad (do_cd), no al recargar la misma
; carpeta (mkdir/borrar/renombrar/pegar) --
list_filter_len: defb 0          ; 0 = sin filtro (usa "*")
list_filter:      defs TI_MAXLEN

rtc_buf: defs 22        ; "yyyy-mm-dd hh:mm:ss.cc" (ver rtc_fetch)

; -- contadores de vt_view_scr --
vscr_third:  defb 0
vscr_line:   defb 0
vscr_rit:    defb 0
vscr_srcptr: defw 0

; -- panel de configuracion: estado local de cada opcion (no hay forma de
; preguntarselo al firmware, asi que el explorador fuerza un estado inicial
; conocido en start y lo va llevando al alternar cada tecla) --
cfg_wrx:        defb 0          ; 0=OFF,1=ON
cfg_fullpag:    defb 0          ; 0=OFF,1=ON
cfg_mc45:       defb 0          ; 0=OFF,1=ON
cfg_chr128:     defb 0          ; 0=CHR64,1=CHR128
cfg_joy_keys:   defb "76580"    ; teclas arriba/abajo/izda/dcha/fuego (por defecto);
                                ; en el ZX81 7=arriba y 6=abajo, asi que el orden
                                ; de las dos primeras es 7,6 y no 6,7
cfg_vgm_loaded:   defb 0        ; 0=nada cargado, 1=hay algo cargado (VGM o PEB, sonando o en pausa)
cfg_vgm_playing:  defb 0        ; 0=parado/en pausa, 1=sonando
cfg_vgm_namelen:  defb 0
cfg_vgm_name:     defs VGM_NAME_MAXLEN
cfg_media_peb:    defb 0        ; 0=lo cargado es un VGM, 1=es un PEB (que comandos usan T/Y/U)
cur_attr:       defb NORM_ATTR
namelen:        defb 0
retlen:         defb 0
retedit:        defb 0          ; 1 = USR devuelve longitud + 256 (tecla E)
mcustatus:      defb 0
xycoords:       defb 0,0

; -- dialogos / entrada de texto --
ti_row:         defb 0
rc_pending:     defb 0
rc_offset:      defb 0
ti_cursor:      defb 0
ti_char:        defb 0
ti_allow_space: defb 0          ; 1 = ESPACIO se inserta como caracter
                                 ; normal en vez de cancelar (teclas joystick)
ti_src:         defw 0
ti_dst:         defw 0
ti_delpos:      defb 0

; -- renombrar: nombre original (do_newfolder deja rn_oldlen=0, asi que
; ti_restore no tiene nada que restaurar en ese contexto; se necesita el
; buffer igualmente porque ti_restore es codigo compartido) --
rn_oldlen:      defb 0
rn_oldname:     defs 48

; -- gestor de archivos: portapapeles (copiar/mover) --
clip_mode:       defb 0          ; 0=nada, 1=copiar marcado, 2=mover marcado
clip_namelen:    defb 0
clip_name:       defs 48
clip_srcpathlen: defb 0
clip_srcpath:    defs 100

; textos de la pantalla de ayuda (vt_help): fila, columna, texto con
; {tecla} en otro color, 0; 0FFh al final
help_text:
        defb 2,2,"LIST",0
        defb 3,4,"{6} {7}     move down / up",0
        defb 3,38,"{1} {2}      page up / down",0
        defb 4,4,"{8} {ENTER} open the folder or file",0
        defb 4,38,"{5}        parent folder",0
        defb 5,4,"{.}       filter (wildcards)",0
        defb 5,38,"{SHIFT+1}  remove the filter",0
        defb 6,4,"{H}       hex viewer",0
        defb 6,38,"{E}  edit    {SHIFT+E}  new file",0
        defb 7,4,"{I}       network (IP address)",0
        defb 7,38,"{SPACE}    exit to BASIC",0
        defb 8,4,"{L}       view any file as text",0
        defb 9,2,"FILES",0
        defb 10,4,"{N}       new folder",0
        defb 10,38,"{D}        delete",0
        defb 11,4,"{R}       rename",0
        defb 11,38,"{V}        paste here",0
        defb 12,4,"{C}       mark to copy",0
        defb 12,38,"{X}        mark to move",0
        defb 14,2,"SETUP PANEL ({S} opens / closes it; its keys only work with it open)",0
        defb 15,4,"{W}       WRX on / off",0
        defb 15,38,"{M}        MC45 on / off",0
        defb 16,4,"{F}       full paging on / off",0
        defb 16,38,"{A}        64 / 128 characters",0
        defb 17,4,"{J}       joystick keys",0
        defb 17,38,"{T} {Y} {U}  stop / pause / play",0
        defb 19,2,"VIEWERS",0
        defb 20,4,"{6} {7}     line            {1} {2}      page         {SPACE} back",0
        defb 21,4,"{G}       go to address (hex viewer)",0
        defb 21,38,"{Z}        ZX81 / ASCII (hex)",0
        defb 0FFh

; -------------------------------------------------------------
; Fuente: la de Spectrum (32-127), el logo (128-155) y los caracteres
; propios (0-15; los graficos del ZX81 se copian de la ROM a 16-25)
; -------------------------------------------------------------
FONTBASE:
        incbin "specfont.bin"

        include "logo.inc"

glyphs:
        defs 8                                          ; 0
        defb 000h,020h,030h,038h,03Ch,038h,030h,020h    ; 1 G_PLAY
        defb 000h,000h,066h,066h,066h,066h,000h,000h    ; 2 G_PAUSE
        defb 000h,000h,03Ch,03Ch,03Ch,03Ch,000h,000h    ; 3 G_STOP
        defb 000h,010h,038h,07Ch,010h,010h,010h,000h    ; 4 G_UP
        defb 000h,010h,010h,010h,07Ch,038h,010h,000h    ; 5 G_DOWN
        defb 000h,010h,030h,07Eh,030h,010h,000h,000h    ; 6 G_LEFT
        defb 000h,008h,00Ch,07Eh,00Ch,008h,000h,000h    ; 7 G_RIGHT
        defb 000h,000h,03Ch,07Eh,07Eh,07Eh,03Ch,000h    ; 8 G_FIRE
GLYPHS_LEN      equ $-glyphs

        end
