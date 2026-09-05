; =============================================================
; EXPLORER.ASM -- Explorador de archivos SD81 Booster (42 columnas)
;
; Navega carpetas de la SD, carga programas .P (y en general cualquier
; archivo, devolviendo el control al BASIC con su nombre) y gestiona
; archivos (nueva carpeta, borrar, renombrar, copiar/mover), en modo
; Superfast HiRes Spectrum (42 columnas via fuente comprimida de 6px).
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
; y 7 sirven para esto (son los que hacen de espejo de las paginas 2 y 3
; en modo video normal, ver mcu_map en video_off); tambien es valido el
; bloque 4 (RAM libre de la ampliacion, sin remapear -- asi se probo en
; vtest1..9, ver esos ficheros si hace falta volver a esa opcion).
; -------------------------------------------------------------
VIDBLOCK        equ 7           ; bloque de pantalla (6 o 7)
VIDPAGE         equ 8           ; pagina fisica dedicada mientras esta activa
VIDMIRRORPAGE   equ VIDBLOCK-4  ; pagina que refleja VIDBLOCK en modo ZX81
                                ; normal (bloque 6->pagina 2, bloque 7->pagina 3)
VIDBASE         equ VIDBLOCK*2000h     ; direccion base del bloque (bitmap)
VIDBASE_HI      equ VIDBLOCK*32        ; byte alto de VIDBASE (VIDBASE/256)
ATTRBASE_HI     equ VIDBASE_HI+18h     ; byte alto del area de atributos
NORM_ATTR       equ 038h        ; papel blanco, tinta negra
SEL_ATTR        equ 00Fh        ; papel azul, tinta blanca (resaltado)
PATH_ATTR       equ 020h        ; papel verde, tinta negra
MAXVIS          equ 21
CLOCK_TICK_THRESHOLD equ 2000   ; pasadas de rk_wait por refresco del reloj
                                 ; en vivo -- sin calibrar en hardware real
PANEL_ATTRCOL0  equ 20          ; 1ª columna de atributo (0-31) del panel de config.
LIST_ATTRCOLS   equ PANEL_ATTRCOL0      ; columnas de atributo de la lista con panel activo
LIST_MAXCHARS   equ 26          ; caracteres (6px) que caben en LIST_ATTRCOLS sin invadir el panel
PANEL_TXTCOL    equ 27          ; columna de texto (6px) donde arranca el panel
PANEL_VALCOL    equ 37          ; columna donde arranca el valor (ON/OFF/128/64) de cada opcion
PANEL_ATTR      equ 028h        ; papel cian, tinta negra (filas de opciones)
PANEL_KEY_ON_ATTR equ 02Ah      ; papel cian, tinta roja (letra de tecla activa)
PANEL_BASE_ATTR equ PANEL_ATTR  ; fondo base del panel: tambien cian
PANEL_TITLE_ATTR equ 01Fh       ; papel magenta, tinta blanca (fila del titulo) -- prueba
; columnas de bytes (0-31) donde pegar con blit_cols cada icono -- salen
; de extract_icon.py (preposicionan el icono a nivel de pixel, alineado
; con columnas de texto de 6px; ver cabecera de ese script) y NO se
; recalculan aqui porque el desplazamiento sub-byte ya esta "horneado"
; en el recurso .bin correspondiente.
PANEL_ICONJOY_COL   equ 24   ; iconos de flechas (5 bytes, pixel visible 198)
PANEL_JOY_VALCOL    equ 33   ; teclas QAOP: mismo pixel (198) que el icono
PANEL_ICONSTOP_COL  equ 24   ; bajo la T (pixel 192)
PANEL_ICONPAUSE_COL equ 25   ; bajo la Y (pixel 204)
PANEL_ICONPLAY_COL  equ 27   ; bajo la U (pixel 216)
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
HEXROW_BYTES    equ 8            ; bytes por fila en pantalla
HEXR_ASCII_COL  equ 33           ; columna de texto (6px) donde arranca la
                                  ; columna ASCII/ZX81: 6 (offset) + 2 +
                                  ; 24 (8 pares hex) + 1 = 33
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

        ; --- HFILE en el bloque VIDBLOCK, remapeado a VIDPAGE ---
        ld a,VIDBLOCK
        ld e,VIDPAGE
        call mcu_map
        xor a
        ld (2043),a             ; HFILE bajo
        ld a,VIDBASE_HI
        ld (2044),a             ; HFILE alto

        ; (el primer video_clear lo hace vt_refresh mas abajo)

        ; --- Chroma81 ON (bit5) ---
        ld bc,7fefh
        ld a,20h
        out (c),a

        ; --- modo Superfast HiRes Spectrum ---
        ld a,172
        ld (2045),a

        ; --- borde azul ---
        ld a,1
        out (0fbh),a

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
        ld a,05h                 ; tinta cian, papel negro
        ld (cur_attr),a
        ld d,0
        ld e,0
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
        ld e,32
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

        ld ix,BG_ROW0
        xor a
        call blit_row
        ld ix,BG_ROW23
        ld a,23
        call blit_row
        call vt_update_clip_icons ; re-tenir C/X si hay algo marcado (el
                                  ; blit anterior los deja en negro)

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
        cp 42
        jr c,vtr_t1
        ld a,42
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
        ld a,42
        ld (list_maxchars),a
        ld a,32
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
; PANEL_ATTRCOL0..31) -- fondo azul base, la fila del titulo (1) en
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

        ; -- iconos de flechas del joystick --
        ld a,10
        ld d,PANEL_ICONJOY_COL
        ld b,5
        ld ix,ICON_JOY
        call blit_cols

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
        ld a,14
        ld d,PANEL_ICONSTOP_COL
        ld b,1
        ld ix,ICON_STOP
        call blit_cols
        ld a,14
        ld d,PANEL_ICONPAUSE_COL
        ld b,2
        ld ix,ICON_PAUSE
        call blit_cols
        ld a,14
        ld d,PANEL_ICONPLAY_COL
        ld b,1
        ld ix,ICON_PLAY
        call blit_cols

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
; de settings (columnas PANEL_ATTRCOL0..31) en esa fila, con el atributo
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
        ld d,PANEL_ATTRCOL0
        ld b,32-PANEL_ATTRCOL0
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
; centrado en el hueco de texto del panel (PANEL_TXTCOL..41, 15 columnas)
PANEL_TITLE_COL equ PANEL_TXTCOL+(15-panel_title_len)/2

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
; centrado igual que el titulo (PANEL_TXTCOL..41, 15 columnas)
PANEL_TYU_COL equ PANEL_TXTCOL+(15-panel_lbl_tyu_len)/2

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
        jp vt_loop

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
        defb "FILTER (WILDCARDS):"
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
        ld a,(cfg_chr128)
        xor 1
        ld (cfg_chr128),a
        or a
        jr z,vtchr_64
        ld a,27                   ; CMD_chars128
        jr vtchr_send
vtchr_64:
        ld a,28                   ; CMD_chars64
vtchr_send:
        call mcu_send
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
        defb "JOYSTICK KEYS (UP,DOWN,LEFT,RIGHT,FIRE):"
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
        ld a,85
        ld (2045),a             ; Superfast OFF (vuelve a modo ZX81 normal)
        ld bc,7fefh
        xor a
        out (c),a               ; Chroma81 OFF
        ld a,VIDBLOCK
        ld e,VIDMIRRORPAGE
        call mcu_map            ; restaura bloque VIDBLOCK = espejo de su pagina
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
        ld a,85
        ld (2045),a             ; Superfast OFF
        ld bc,7fefh
        xor a
        out (c),a               ; Chroma81 OFF
        ld a,VIDBLOCK
        ld e,VIDMIRRORPAGE
        call mcu_map            ; restaura bloque VIDBLOCK = espejo de su pagina
        ld a,(retlen)
        ld c,a
        ld b,0
        ret                     ; USR devuelve BC = longitud del nombre

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
; atributo, ver comentario de calc_bmp_addr y extract_bg.py). Se lee en
; bloques de 256 bytes (una scanline x los 8 bytes-fila de un tercio, o
; 8 filas de atributo) -- NO fila a fila, que serian cientos de
; peticiones MCU -- y cada bloque se reparte directamente a las filas
; de pantalla reales via calc_bmp_addr/calc_attr_addr. Espera cualquier
; tecla y vuelve al listado.
; -------------------------------------------------------------
vt_view_scr:
        call f_open_txt
        cp 0FFh
        jp z,vt_loop

        ld (viewer_handle),a
        xor a
        ld (clock_screen_active),a
        call video_clear

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
        call calc_bmp_addr       ; hl = direccion scanline0 de esa fila
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
        call calc_attr_addr
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
        jp vt_refresh_and_loop

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

        call video_clear
        ld a,NORM_ATTR
        ld (cur_attr),a
        ld d,10
        ld e,13
        call p42_setxy
        ld hl,noconn_msg
        ld b,noconn_msg_len
        call p42_string
        call read_key
        jp vt_refresh_and_loop

ip_path:        defb "/MAN/IP.TXT"
ip_path_len     equ $-ip_path
noconn_msg:     defb "NO CONNEXION"
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
        ld ix,BG_ROW0
        xor a
        call blit_row
        ld ix,BG_ROW23
        ld a,23
        call blit_row

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
; deja en namebuf/(namelen), recortada a 42 caracteres -- sigue
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
        ld c,a
        ld a,(namelen)
        cp 42
        jr nc,vnl_charloop        ; linea ya llena: seguir consumiendo sin guardar
        ld hl,namebuf
        ld e,a
        ld d,0
        add hl,de
        ld (hl),c
        ld a,(namelen)
        inc a
        ld (namelen),a
        jr vnl_charloop
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
        ld a,32
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
; los nombres de archivo). Los graficos de bloque (01-0A) se dibujan a
; nivel de bitmap (ver hexr_invloop/p42_draw_block/zxblock_tbl). Los
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
        call show_prompt
        call text_input
        ld a,(namelen)
        or a
        jr z,hexr_goto_redraw     ; cancelado: solo repintar (el dialogo tapo el visor)

        call hex_parse
        ld a,(hexg_val)
        and 0F0h
        ld (hexg_val),a
        ld b,3                     ; /8 (byte -> fila): 3 desplazamientos
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
        defb "GOTO ADDRESS (HEX):"
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
        ld ix,BG_ROW0
        xor a
        call blit_row
        ld ix,BG_ROW23
        ld a,23
        call blit_row

        ld a,06h                 ; tinta amarilla, papel negro
        ld (cur_attr),a
        ld d,23
        ld e,20                  ; ~columna 14 de 8px (14*8/6), +1
        call p42_setxy
        ld hl,hexr_hint
        ld b,hexr_hint_len
        call p42_string

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

hexr_hint:     defb "G-GOTO Z-ZX/ASC     "
hexr_hint_len  equ $-hexr_hint

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
; de pantalla (vwr_row). Formato: "OOOOOO  h1 h2 .. h8  a1a2..a8"
; (recorta a huecos en blanco si la fila es la ultima y es parcial).
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
        ld a,' '
        call nb_append
        ld a,' '
        call nb_append

        ld a,(hexr_count)
        ld b,a
        or a
        jr z,hexr_hex_blanks
        ld hl,viewer_buf
hexr_hex_real:
        ld a,(hl)
        inc hl
        call nb_hexbyte
        ld a,' '
        call nb_append
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
        ld a,' '
        call nb_append

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

        ; -- modo ZX81: dibuja los graficos de bloque (codigos 01-08, ver
        ; p42_draw_block/zxblock_tbl) e invierte (a nivel de bitmap, ver
        ; p42_invert_cell) los caracteres cuyo byte tenia el bit7 a 1 --
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
        call show_prompt

        ; --- mostrar el nombre del archivo/carpeta a borrar ---
        ld a,NORM_ATTR
        ld (cur_attr),a
        ld d,8
        ld e,0
        call p42_setxy
        ld hl,namebuf
        ld a,(namelen)
        cp 42
        jr c,vtd_nt1
        ld a,42
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
        defb "DELETE? ENTER=YES  SPACE=NO"
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
        defb "RENAME - NEW NAME:"
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
; vt_update_clip_icons: tiñe de azul la tinta del icono C o X (fila 23)
; segun (clip_mode) -- 1=copiar marcado (icono C), 2=mover marcado
; (icono X), 0=nada (los deja en negro, su color normal). Se llama al
; marcar/pegar y despues de cada blit de la fila 23 en vt_refresh (que
; la deja en negro por defecto).
; -------------------------------------------------------------
ICON_COL_C equ 14
ICON_COL_X equ 16
vt_update_clip_icons:
        ld c,ICON_COL_C
        xor a                     ; negro
        call vt_set_icon_ink
        ld c,ICON_COL_X
        xor a
        call vt_set_icon_ink
        ld a,(clip_mode)
        cp 1
        jr z,vtuci_c
        cp 2
        jr z,vtuci_x
        ret
vtuci_c:
        ld c,ICON_COL_C
        ld a,1                    ; azul
        jr vt_set_icon_ink
vtuci_x:
        ld c,ICON_COL_X
        ld a,1                    ; azul

; vt_set_icon_ink: cambia solo los 3 bits de tinta del atributo de la
; fila 23, dejando papel/brillo tal cual.
; IN: C=columna (0-31), A=nuevo color de tinta (0-7). Destruye AF,HL.
vt_set_icon_ink:
        push af
        ld a,23
        call calc_attr_addr       ; hl = direccion atributo fila23 columna0
        ld a,l
        add a,c
        ld l,a
        pop af
        ld b,a
        ld a,(hl)
        and 0F8h
        or b
        ld (hl),a
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
        xor a
        ld (clock_screen_active),a
        push hl
        push bc
        call video_clear
        pop bc
        pop hl
        ld a,NORM_ATTR
        ld (cur_attr),a
        ld d,10
        ld e,0
        call p42_setxy
        call p42_string
        ld a,12
        ld (ti_row),a
        ret

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
        ld a,NORM_ATTR
        ld (cur_attr),a
        ld a,(ti_row)
        ld d,a
        ld e,0
        call p42_setxy
        ld hl,ti_spaces
        ld b,ti_spaceslen
        call p42_string
        ld a,(ti_row)
        ld d,a
        ld e,0
        call p42_setxy
        ld hl,namebuf
        ld a,(namelen)
        ld b,a
        call p42_string

        call draw_cursor          ; raya de 6px en la columna del cursor

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

ti_spaces:
        defb "                                          "
ti_spaceslen equ $-ti_spaces

prompt_newfolder:
        defb "NEW FOLDER - NAME:"
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
; scroll_list_up/scroll_list_down/copy_row/clear_row: copia literal de
; explorer.asm, usando VIDBASE_HI/ATTRBASE_HI (derivados de VIDBLOCK).
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

cp_srcrow:      defb 0
cp_dstrow:      defb 0
cp_srcaddr:     defw 0
cp_dstaddr:     defw 0

copy_row:
        ld a,d
        ld (cp_dstrow),a
        ld a,e
        ld (cp_srcrow),a

        call calc_bmp_addr
        ld (cp_srcaddr),hl
        ld a,(cp_dstrow)
        call calc_bmp_addr
        ld (cp_dstaddr),hl

; copy_row/clear_row: limitadas a (list_attrw) columnas (32 con el panel
; cerrado, igual que siempre) para no desplazar/borrar el panel de
; configuracion cuando el listado hace scroll con el panel abierto.
        ld b,8
cpr_loop:
        push bc
        ld hl,(cp_srcaddr)
        ld de,(cp_dstaddr)
        ld a,(list_attrw)
        ld c,a
        ld b,0
        ldir
        ld hl,(cp_srcaddr)
        inc h
        ld (cp_srcaddr),hl
        ld hl,(cp_dstaddr)
        inc h
        ld (cp_dstaddr),hl
        pop bc
        djnz cpr_loop

        ld a,(cp_srcrow)
        call calc_attr_addr
        ex de,hl
        ld a,(cp_dstrow)
        call calc_attr_addr
        ex de,hl
        ld a,(list_attrw)
        ld c,a
        ld b,0
        ldir
        ret

clear_row:
        push af
        call calc_bmp_addr
        ld b,8
cr_loop:
        push bc
        push hl
        ld (hl),0
        ld d,h
        ld e,l
        inc de
        ld a,(list_attrw)
        dec a
        ld c,a
        ld b,0
        ldir
        pop hl
        inc h
        pop bc
        djnz cr_loop

        pop af
        call calc_attr_addr
        ld a,NORM_ATTR
        ld (hl),a
        ld d,h
        ld e,l
        inc de
        ld a,(list_attrw)
        dec a
        ld c,a
        ld b,0
        ldir
        ret

; -------------------------------------------------------------
; redraw_row_attr / fill_row_attr / blit_row / calc_bmp_addr /
; calc_attr_addr: usan VIDBASE_HI/ATTRBASE_HI (derivados de VIDBLOCK).
; -------------------------------------------------------------
; redraw_row_attr: repintado parcial de UNA fila del listado (usado por
; vt_down/vt_up sin cruzar pagina). Respeta (list_maxchars)/(list_attrw)
; -- igual que vlist_loop en vt_refresh -- para no invadir el panel de
; configuracion cuando esta abierto (si esta cerrado valen 42/32 y el
; comportamiento es identico al de siempre).
redraw_row_attr:
        ld (rra_row),a
        push bc
        call get_row
        pop bc
        ld a,c
        ld (rra_attr),a
        ld (cur_attr),a

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

fill_row_attr:
        ld b,32
        ld d,0
        jr fill_row_attr_col

; fill_row_attr_n: como fill_row_attr pero con anchura B (1-32) desde la
; columna 0. Usada por el listado cuando el panel de config esta activo,
; para no pintar el resalte de seleccion encima del panel.
fill_row_attr_n:
        ld d,0
        jr fill_row_attr_col

; fill_row_attr_col: A=fila, D=columna inicial de atributo(0-31),
; B=anchura(1-32 cols), C=attr -> destruye AF,BC,DE,HL
fill_row_attr_col:
        call calc_attr_addr      ; hl = direccion columna 0 de la fila
        ld a,d
        ld d,0
        ld e,a
        add hl,de                ; hl += columna inicial
        ld (hl),c
        ld d,h
        ld e,l
        inc de
        ld a,b
        dec a
        ld c,a
        ld b,0
        ldir
        ret

video_clear:
        ld hl,VIDBASE
        ld de,VIDBASE+1
        ld bc,17ffh
        ld (hl),0
        ldir
        ld hl,VIDBASE+1800h
        ld de,VIDBASE+1801h
        ld bc,2ffh
        ld a,NORM_ATTR
        ld (hl),a
        ldir
        ret

blit_row:
        push af
        call calc_bmp_addr
        pop af
        ld b,8
br_loop:
        push bc
        push hl
        ex de,hl
        push ix
        pop hl
        ld bc,32
        ldir
        push hl
        pop ix
        pop hl
        inc h
        pop bc
        djnz br_loop

        call calc_attr_addr
        ex de,hl
        push ix
        pop hl
        ld bc,32
        ldir
        ret

; -------------------------------------------------------------
; blit_cols: como blit_row pero con anchura y columna inicial variables
; (para los iconos del panel de configuracion, mas estrechos que una fila
; completa). Recurso generado por extract_bg.py con rango de columnas:
; anchura*8 bytes de bitmap (8 scanlines de "anchura" bytes) + anchura
; bytes de atributo. Entrada: A=fila, D=columna inicial(0-31),
; B=anchura(cols), IX=recurso. Destruye AF,BC,DE,HL,IX.
; -------------------------------------------------------------
blit_cols:
        ld (blit_row_reg),a
        ld a,d
        ld (blit_col_reg),a
        ld a,b
        ld (blit_w_reg),a

        ld a,(blit_row_reg)
        call calc_bmp_addr        ; hl = direccion linea0, columna 0 de la fila
        ld a,(blit_col_reg)
        ld e,a
        ld d,0
        add hl,de                  ; hl += columna inicial

        ld b,8
bcl_loop:
        push bc
        push hl
        ex de,hl
        push ix
        pop hl
        ld a,(blit_w_reg)
        ld c,a
        ld b,0
        ldir
        push hl
        pop ix
        pop hl
        inc h
        pop bc
        djnz bcl_loop

        ld a,(blit_row_reg)
        call calc_attr_addr
        ld a,(blit_col_reg)
        ld e,a
        ld d,0
        add hl,de
        ex de,hl
        push ix
        pop hl
        ld a,(blit_w_reg)
        ld c,a
        ld b,0
        ldir
        ret

; -------------------------------------------------------------
; draw_cursor: dibuja una raya de 6px de "tinta" en la ULTIMA scanline
; (linea 7 de 8) de la fila (ti_row), en la columna (ti_cursor). Trabaja
; a nivel de bitmap, no de atributos: como cada caracter de la fuente
; comprimida ocupa 6px pero las celdas de atributo son de 8px, resaltar
; por atributo tiñe parte del caracter vecino (celdas compartidas). Un
; trazo en el bitmap no tiene ese problema.
; Destruye AF,BC,DE,HL.
; -------------------------------------------------------------
draw_cursor:
        ld a,(ti_cursor)
        ld l,a
        ld h,0
        add hl,hl
        ld d,h
        ld e,l
        add hl,hl
        add hl,de                ; hl = ti_cursor*6 (posicion en pixeles)
        ld a,l                   ; h siempre 0 aqui (TI_MAXLEN=40 -> max 234)
        ld b,a
        and 7
        ld c,a                   ; c = desplazamiento de bit dentro del byte (0-7)
        ld a,b
        rrca
        rrca
        rrca
        and 1Fh
        ld b,a                   ; b = columna de byte (0-31) dentro de la fila

        push bc
        ld a,(ti_row)
        call calc_bmp_addr       ; hl = direccion linea0 de la fila
        pop bc
        ld a,h
        add a,7
        ld h,a                   ; ultima scanline (linea 7 de 8)
        ld a,l
        add a,b
        ld l,a                   ; + columna de byte del cursor

        ; mascara de 6 bits a la izquierda (11111100), desplazada c bits
        ; y repartida entre este byte y el siguiente
        ld d,0FCh
        ld e,0
        ld a,c
        or a
        jr z,dc_shifted
dc_shift:
        srl d
        rr e
        dec a
        jr nz,dc_shift
dc_shifted:
        ld a,(hl)
        or d
        ld (hl),a
        inc hl
        ld a,(hl)
        or e
        ld (hl),a
        ret

; -------------------------------------------------------------
; p42_cellpos: D=fila,E=columna (caracteres de 6px, 0-41) -> HL=direccion
; del primer byte (linea 0) de esa celda en el bitmap, C=desplazamiento
; de bit (0-7) dentro de ese byte. Comun a p42_invert_cell y
; p42_draw_block. Destruye AF,B.
; -------------------------------------------------------------
p42_cellpos:
        ld a,d
        push af                  ; guarda la fila (d se reutiliza como escratch)
        ld a,e
        ld l,a
        ld h,0
        add hl,hl
        ld d,h
        ld e,l
        add hl,hl
        add hl,de                ; hl = columna*6 (posicion en pixeles)
        ld a,l
        ld b,a
        and 7
        ld c,a                   ; c = desplazamiento de bit (0-7)
        ld a,b
        rrca
        rrca
        rrca
        and 1Fh
        ld b,a                   ; b = columna de byte (0-31)

        pop af                   ; recupera la fila
        push bc
        call calc_bmp_addr       ; hl = direccion linea0 de la fila
        pop bc
        ld a,l
        add a,b
        ld l,a                   ; hl += columna de byte
        ret

; -------------------------------------------------------------
; p42_invert_cell: D=fila,E=columna (caracteres de 6px, 0-41) -> invierte
; (XOR) el bloque de 6x8 pixeles de esa posicion, dejando el atributo
; (papel/tinta) intacto. Mismo truco que draw_cursor (evita el "clash"
; de la celda de atributo de 8px contra el caracter de 6px) pero
; aplicado a las 8 scanlines en vez de solo la ultima -- para el modo
; ZX81 inverso (bit7) del visor hexadecimal. Destruye AF,BC,DE,HL.
; -------------------------------------------------------------
p42_invert_cell:
        call p42_cellpos         ; hl = direccion linea0, c = desplazamiento
        ld d,0FCh                ; mascara de 6 bits (11111100) desplazada
        ld e,0                   ; c bits, repartida entre este byte y el
        ld a,c                   ; siguiente (igual que draw_cursor)
        or a
        jr z,pic_shifted
pic_shift:
        srl d
        rr e
        dec a
        jr nz,pic_shift
pic_shifted:
        ld b,8
pic_loop:
        push bc
        push hl
        ld a,(hl)
        xor d
        ld (hl),a
        inc hl
        ld a,(hl)
        xor e
        ld (hl),a
        pop hl
        inc h
        pop bc
        djnz pic_loop
        ret

; -------------------------------------------------------------
; p42_draw_block: A=codigo de bloque ZX81 (1-0Ah), D=fila, E=columna ->
; dibuja (OR, no XOR) el patron de graficos de bloque correspondiente
; sobre una celda de 6x8 px ya en blanco (ver zxblock_tbl). Destruye
; AF,BC,DE,HL.
; -------------------------------------------------------------
p42_draw_block:
        push af                  ; codigo de bloque (1-8)
        call p42_cellpos         ; hl = direccion linea0, c = desplazamiento
        pop af
        dec a
        add a,a
        add a,a
        add a,a                  ; a = (codigo-1)*8
        ld de,zxblock_tbl
        add a,e
        ld e,a
        jr nc,pdb_noc
        inc d
pdb_noc:                         ; de = puntero al patron de 8 bytes
        ld b,8
pdb_loop:
        push bc
        ld a,(de)
        inc de
        push de
        ld d,a
        ld e,0
        ld a,c
        or a
        jr z,pdb_shifted
pdb_shift:
        srl d
        rr e
        dec a
        jr nz,pdb_shift
pdb_shifted:
        push hl
        ld a,(hl)
        or d
        ld (hl),a
        inc hl
        ld a,(hl)
        or e
        ld (hl),a
        pop hl
        inc h
        pop de
        pop bc
        djnz pdb_loop
        ret

; zxblock_tbl: los datos estan al final del fichero (ver mas abajo, junto
; a specfont.bin/BG_ROW0/etc.) -- es una tabla pura sin saltos ni
; llamadas, puede vivir por encima de 32768 sin problema.

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

calc_bmp_addr:
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

calc_attr_addr:
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
        ld a,0FEh                ; SHIFT+1 = reset del filtro de listado
        in a,(0FEh)               ; (comprobacion aparte porque SHIFT no
        bit 0,a                   ; se rastrea como tecla propia en el
        jr nz,rk_normal1           ; resto de la matriz)
        ld a,0F7h
        in a,(0FEh)
        bit 0,a
        jp z,rk_resetfilter
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
rk_pgup:
        ld a,13
        jr rk_deb
rk_pgdn:
        ld a,14
rk_deb:
        push af
rk_rel:
        ld a,0F7h
        in a,(0FEh)
        and 13h
        cp 13h
        jr nz,rk_stillp
        ld a,0EFh
        in a,(0FEh)
        and 1Ch
        cp 1Ch
        jr nz,rk_stillp
        ld a,0BFh
        in a,(0FEh)
        and 19h
        cp 19h
        jr nz,rk_stillp
        ld a,7Fh
        in a,(0FEh)
        and 0Fh
        cp 0Fh
        jr nz,rk_stillp
        ld a,0FDh
        in a,(0FEh)
        and 1Fh
        cp 1Fh
        jr nz,rk_stillp
        ld a,0FBh
        in a,(0FEh)
        and 1Ah
        cp 1Ah
        jr nz,rk_stillp
        ld a,0DFh
        in a,(0FEh)
        and 1Ch
        cp 1Ch
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
; HEXROW_BYTES), DE=palabra alta, HL=palabra baja. Destruye AF.
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
; RENDERIZADO 42 COLUMNAS (fuente 6px comprimida) -- copia literal de
; explorer.asm.
; =============================================================
p42_setxy:
        ld (xycoords),de
        ret

p42_newline:
        ld de,(xycoords)
        call nxtline
        ld (xycoords),de
        ret

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

p42_putchar:
        exx
        push hl
        exx
        ld c,a
        ld h,0
        ld l,a
        ld de,whichcolumn-32
        add hl,de
        ld a,(hl)
        cp 32
        jr nc,p42_calcchar

        ld de,p42_characters
        ld l,a
        call p42_mult8
        ld b,h
        ld c,l
        jr p42_printdata

p42_calcchar:
        ld de,FONTBASE-256
        ld l,c
        call p42_mult8

        ld de,p42_workspace
        push de
        exx
        ld c,a
        cpl
        ld b,a
        exx
        ld b,8
p42_loop1:
        ld a,(hl)
        inc hl
        exx
        ld e,a
        and c
        ld d,a
        ld a,e
        rla
        and b
        or d
        exx
        ld (de),a
        inc de
        djnz p42_loop1
        pop bc

p42_printdata:
        call p42_testcoords
        inc e
        ld (xycoords),de
        dec e
        ld a,e
        sla a
        ld l,a
        sla a
        add a,l
        ld l,a
        srl a
        srl a
        srl a
        ld e,a
        ld a,l
        and 7
        push af
        ex af,af'
        ld a,d
        sra a
        sra a
        sra a
        add a,ATTRBASE_HI
        ld h,a
        ld a,d
        and 7
        rrca
        rrca
        rrca
        add a,e
        ld l,a
        ld a,(cur_attr)
        ld e,a
        ld (hl),e
        inc hl
        pop af
        cp 3
        jr c,p42_hop1
        ld (hl),e
p42_hop1:
        dec hl
        ld a,d
        and 248
        add a,VIDBASE_HI
        ld h,a
        push hl
        exx
        pop hl
        exx
        ld a,8
p42_hop4:
        push af
        ld a,(bc)
        exx
        push hl
        ld c,0
        ld de,1023
        ex af,af'
        and a
        jr z,p42_hop3
        ld b,a
        ex af,af'
p42_hop2:
        and a
        rra
        rr c
        scf
        rr d
        rr e
        djnz p42_hop2
        ex af,af'
p42_hop3:
        ex af,af'
        ld b,a
        ld a,(hl)
        and d
        or b
        ld (hl),a
        inc hl
        ld a,(hl)
        and e
        or c
        ld (hl),a
        pop hl
        inc h
        exx
        inc bc
        pop af
        dec a
        jr nz,p42_hop4
        exx
        pop hl
        exx
        ret

p42_mult8:
        ld h,0
        add hl,hl
        add hl,hl
        add hl,hl
        add hl,de
        ret

p42_testcoords:
        ld de,(xycoords)
nxtchar:
        ld a,e
        cp 42
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

whichcolumn:
        defb 254       ; SPACE
        defb 254       ; !
        defb 128       ; "
        defb 224       ; #
        defb 128       ; $
        defb 0         ; % (redefinido)
        defb 1         ; & (redefinido)
        defb 128       ; '
        defb 128       ; (
        defb 128       ; )
        defb 128       ; *
        defb 128       ; +
        defb 128       ; ,
        defb 128       ; -
        defb 128       ; .
        defb 128       ; /
        defb 2         ; 0 (redefinido)
        defb 128       ; 1
        defb 224       ; 2
        defb 224       ; 3
        defb 252       ; 4
        defb 224       ; 5
        defb 224       ; 6
        defb 192       ; 7
        defb 240       ; 8
        defb 240       ; 9
        defb 240       ; :
        defb 240       ; ;
        defb 192       ; <
        defb 240       ; =
        defb 192       ; >
        defb 192       ; ?
        defb 248       ; @
        defb 240       ; A
        defb 240       ; B
        defb 240       ; C
        defb 240       ; D
        defb 240       ; E
        defb 240       ; F
        defb 240       ; G
        defb 240       ; H
        defb 128       ; I
        defb 240       ; J
        defb 192       ; K
        defb 240       ; L
        defb 240       ; M
        defb 248       ; N
        defb 240       ; O
        defb 240       ; P
        defb 248       ; Q
        defb 240       ; R
        defb 240       ; S
        defb 3         ; T (redefinido)
        defb 240       ; U
        defb 240       ; V
        defb 240       ; W
        defb 240       ; X
        defb 4         ; Y (redefinido)
        defb 252       ; Z
        defb 224       ; [
        defb 252       ; \
        defb 240       ; ]
        defb 252       ; ^
        defb 6         ; _
        defb 240       ; Libra
        defb 255       ; a
        defb 128       ; b
        defb 255       ; c
        defb 255       ; d
        defb 255       ; e
        defb 255       ; f
        defb 255       ; g
        defb 255       ; h
        defb 255       ; i
        defb 255       ; j
        defb 255       ; k
        defb 255       ; l
        defb 255       ; m
        defb 255       ; n
        defb 255       ; o
        defb 255       ; p
        defb 255       ; q
        defb 255       ; r
        defb 255       ; s
        defb 255       ; t
        defb 255       ; u
        defb 255       ; v
        defb 255       ; w
        defb 255       ; x
        defb 255       ; y
        defb 255       ; z
        defb 128       ; {
        defb 128       ; |
        defb 255       ; }
        defb 128       ; ~
        defb 5         ; (c) (redefinido, fin de la tabla de columnas)

p42_characters:
        defb 0           ; %
        defb 0
        defb 100
        defb 104
        defb 16
        defb 44
        defb 76
        defb 0

        defb 0           ; &
        defb 32
        defb 80
        defb 32
        defb 84
        defb 72
        defb 52
        defb 0

        defb 0          ; digito 0
        defb 56
        defb 76
        defb 84
        defb 84
        defb 100
        defb 56
        defb 0

        defb 0           ; Letra T
        defb 124
        defb 16
        defb 16
        defb 16
        defb 16
        defb 16
        defb 0

        defb 0          ; Letra Y
        defb 68
        defb 68
        defb 40
        defb 16
        defb 16
        defb 16
        defb 0

        defb 0          ; simbolo (c)
        defb 48
        defb 72
        defb 180
        defb 164
        defb 180
        defb 72
        defb 48

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
cfg_joy_keys:   defb "67580"    ; teclas arriba/abajo/izda/dcha/fuego (por defecto)
cfg_vgm_loaded:   defb 0        ; 0=nada cargado, 1=hay algo cargado (VGM o PEB, sonando o en pausa)
cfg_vgm_playing:  defb 0        ; 0=parado/en pausa, 1=sonando
cfg_vgm_namelen:  defb 0
cfg_vgm_name:     defs VGM_NAME_MAXLEN
cfg_media_peb:    defb 0        ; 0=lo cargado es un VGM, 1=es un PEB (que comandos usan T/Y/U)
blit_row_reg:   defb 0          ; temporales de blit_cols (fila/columna/anchura)
blit_col_reg:   defb 0
blit_w_reg:     defb 0
cur_attr:       defb NORM_ATTR
namelen:        defb 0
retlen:         defb 0
mcustatus:      defb 0
xycoords:       defb 0,0
p42_workspace:  defs 8

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

; -------------------------------------------------------------
; Fuente 6x8 y decoracion de pantalla (mismos recursos que el explorador)
; -------------------------------------------------------------
FONTBASE:
        incbin "specfont.bin"

BG_ROW0:
        incbin "bg_row0.bin"
BG_ROW23:
        incbin "bg_row23.bin"
ICON_JOY:
        incbin "icon_joy.bin"
ICON_STOP:
        incbin "icon_stop.bin"
ICON_PAUSE:
        incbin "icon_pause.bin"
ICON_PLAY:
        incbin "icon_play.bin"

; zxblock_tbl: patrones de graficos de bloque ZX81 (01-0A), extraidos
; bit a bit de la ROM real del ZX81 en $1E00. Tabla pura (sin saltos ni
; llamadas), por eso vive aqui con el resto de recursos, por encima de
; 32768. Usada por p42_draw_block.
zxblock_tbl:
        defb 0e0h,0e0h,0e0h,0e0h,000h,000h,000h,000h   ; 01
        defb 01Ch,01Ch,01Ch,01Ch,000h,000h,000h,000h   ; 02
        defb 0FCh,0FCh,0FCh,0FCh,000h,000h,000h,000h   ; 03
        defb 000h,000h,000h,000h,0e0h,0e0h,0e0h,0e0h   ; 04
        defb 0e0h,0e0h,0e0h,0e0h,0e0h,0e0h,0e0h,0e0h   ; 05
        defb 01ch,01ch,01ch,01ch,0e0h,0e0h,0e0h,0e0h   ; 06
        defb 0FCh,0FCh,0FCh,0FCh,01ch,01Ch,01Ch,01Ch   ; 07
        defb 0A8h,054h,0A8h,054h,0A8h,054h,0A8h,054h   ; 08
        defb 000h,000h,000h,000h,0A8h,054h,0A8h,054h   ; 09
        defb 0A8h,054h,0A8h,054h,000h,000h,000h,000h   ; 0A

        end
