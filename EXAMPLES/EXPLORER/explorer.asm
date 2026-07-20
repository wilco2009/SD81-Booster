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
PANEL_ATTRCOL0  equ 20          ; 1ª columna de atributo (0-31) del panel de config.
LIST_ATTRCOLS   equ PANEL_ATTRCOL0      ; columnas de atributo de la lista con panel activo
LIST_MAXCHARS   equ 26          ; caracteres (6px) que caben en LIST_ATTRCOLS sin invadir el panel
PANEL_TXTCOL    equ 27          ; columna de texto (6px) donde arranca el panel
PANEL_VALCOL    equ 37          ; columna donde arranca el valor (ON/OFF/128/64) de cada opcion
PANEL_ATTR      equ 028h        ; papel cian, tinta negra (filas de opciones)
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

        ; --- abrir directorio raiz ---
        call do_opendir_root

        ; --- posicion inicial y primer repintado completo ---
        ld hl,1
        ld (cur_index),hl
        ld (win_start),hl
        call vt_refresh
        jp vt_loop

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
        ld a,3
        call vdp_fillrow_opt
        ld a,5
        call vdp_fillrow_opt
        ld a,7
        call vdp_fillrow_opt
        ld a,9
        call vdp_fillrow_opt
        ld a,12
        call vdp_fillrow_opt
        ld a,14
        call vdp_fillrow_opt
        ld a,16
        call vdp_fillrow_opt

        ld d,1
        ld e,PANEL_TITLE_COL
        ld hl,panel_title
        ld b,panel_title_len
        ld c,PANEL_TITLE_ATTR
        call panel_print

        ld d,3
        ld e,PANEL_TXTCOL
        ld hl,panel_lbl_wrx
        ld b,panel_lbl_wrx_len
        ld c,PANEL_ATTR
        call panel_print
        ld d,3
        ld e,PANEL_VALCOL
        ld a,(cfg_wrx)
        call panel_onoff
        ld b,3
        ld c,PANEL_ATTR
        call panel_print

        ld d,5
        ld e,PANEL_TXTCOL
        ld hl,panel_lbl_fullpag
        ld b,panel_lbl_fullpag_len
        ld c,PANEL_ATTR
        call panel_print
        ld d,5
        ld e,PANEL_VALCOL
        ld a,(cfg_fullpag)
        call panel_onoff
        ld b,3
        ld c,PANEL_ATTR
        call panel_print

        ld d,7
        ld e,PANEL_TXTCOL
        ld hl,panel_lbl_mc45
        ld b,panel_lbl_mc45_len
        ld c,PANEL_ATTR
        call panel_print
        ld d,7
        ld e,PANEL_VALCOL
        ld a,(cfg_mc45)
        call panel_onoff
        ld b,3
        ld c,PANEL_ATTR
        call panel_print

        ld d,9
        ld e,PANEL_TXTCOL
        ld hl,panel_lbl_chr
        ld b,panel_lbl_chr_len
        ld c,PANEL_ATTR
        call panel_print
        ld d,9
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
        ld a,11
        ld d,PANEL_ICONJOY_COL
        ld b,5
        ld ix,ICON_JOY
        call blit_cols

        ld d,12
        ld e,PANEL_TXTCOL
        ld hl,panel_lbl_joy
        ld b,panel_lbl_joy_len
        ld c,PANEL_ATTR
        call panel_print
        ld d,12
        ld e,PANEL_JOY_VALCOL
        ld hl,cfg_joy_keys
        ld b,5
        ld c,PANEL_ATTR
        call panel_print

        ld d,14
        ld e,PANEL_TYU_COL
        ld hl,panel_lbl_tyu
        ld b,panel_lbl_tyu_len
        ld c,PANEL_ATTR
        call panel_print

        ; -- iconos STOP/PAUSA/PLAY, cada uno bajo su letra (T/Y/U) --
        ld a,15
        ld d,PANEL_ICONSTOP_COL
        ld b,1
        ld ix,ICON_STOP
        call blit_cols
        ld a,15
        ld d,PANEL_ICONPAUSE_COL
        ld b,2
        ld ix,ICON_PAUSE
        call blit_cols
        ld a,15
        ld d,PANEL_ICONPLAY_COL
        ld b,1
        ld ix,ICON_PLAY
        call blit_cols

        ; -- nombre del VGM cargado (vacio si no hay ninguno) --
        ld d,16
        ld e,PANEL_TXTCOL
        ld hl,cfg_vgm_name
        ld a,(cfg_vgm_namelen)
        ld b,a
        ld c,PANEL_ATTR
        call panel_print
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
panel_lbl_fullpag:     defb "F FULLPAG"
panel_lbl_fullpag_len  equ $-panel_lbl_fullpag
panel_lbl_mc45:        defb "M MC45"
panel_lbl_mc45_len     equ $-panel_lbl_mc45
panel_lbl_chr:         defb "H CHR"
panel_lbl_chr_len      equ $-panel_lbl_chr
panel_lbl_joy:         defb "J JOY"
panel_lbl_joy_len      equ $-panel_lbl_joy
panel_lbl_tyu:         defb "T Y U"
panel_lbl_tyu_len      equ $-panel_lbl_tyu
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
        jp z,vt_toggle_chr
        cp 20
        jp z,vt_edit_joy
        cp 21
        jp z,vt_vgm_stop
        cp 22
        jp z,vt_vgm_pause
        cp 23
        jp z,vt_vgm_cont
        jr vt_loop

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
; izda, dcha, fuego) partiendo del valor actual (cfg_joy_keys), con el
; mismo dialogo de texto que usan renombrar/nueva carpeta (show_prompt +
; text_input), pero con (ti_allow_space)=1: el ESPACIO es una tecla de
; joystick valida (p.ej. fuego), asi que aqui NO cancela -- se inserta
; como caracter normal. Para cancelar (recuperar las teclas de antes de
; editar) hay que usar SHIFT+1 (ti_restore), como en renombrar; por eso
; se rellena rn_oldname/rn_oldlen con el valor actual antes de editar.
; Solo si se sale con exactamente 5 caracteres se manda con CMD_joy (21)
; y se actualiza cfg_joy_keys; en cualquier otro caso no se toca nada.
; Solo actua si el panel esta visible.
; -------------------------------------------------------------
vt_edit_joy:
        ld a,(cfg_panel)
        or a
        jp z,vt_loop

        ld hl,cfg_joy_keys
        ld de,namebuf
        ld bc,5
        ldir
        ld a,5
        ld (namelen),a
        ld (rn_oldlen),a
        ld hl,namebuf
        ld de,rn_oldname
        ld bc,5
        ldir

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
        ld a,35                   ; CMD_stopVGM
        call mcu_send
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
        ld a,36                   ; CMD_pauseVGM
        call mcu_send
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
        ld a,37                   ; CMD_contVGM
        call mcu_send
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
        jp vt_refresh_and_loop

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
        ld a,0FDh
        in a,(0FEh)
        bit 2,a
        jp z,rk_d
        bit 1,a
        jp z,rk_s
        bit 3,a
        jp z,rk_f
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
        ld a,0FEh
        in a,(0FEh)
        bit 3,a
        jp z,rk_c
        bit 2,a
        jp z,rk_x
        bit 4,a
        jp z,rk_v
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
        and 0Dh
        cp 0Dh
        jr nz,rk_stillp
        ld a,0FDh
        in a,(0FEh)
        and 0Eh
        cp 0Eh
        jr nz,rk_stillp
        ld a,0FBh
        in a,(0FEh)
        and 1Ah
        cp 1Ah
        jr nz,rk_stillp
        ld a,0DFh
        in a,(0FEh)
        and 18h
        cp 18h
        jr nz,rk_stillp
        ld a,0FEh
        in a,(0FEh)
        and 1Ch
        cp 1Ch
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

do_opendir:
        ld a,16
        jp cmd_str_zx

do_opendir_root:
        ld hl,starmask
        ld b,1
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
        jr c,zta_unknown
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

; -- panel de configuracion: estado local de cada opcion (no hay forma de
; preguntarselo al firmware, asi que el explorador fuerza un estado inicial
; conocido en start y lo va llevando al alternar cada tecla) --
cfg_wrx:        defb 0          ; 0=OFF,1=ON
cfg_fullpag:    defb 0          ; 0=OFF,1=ON
cfg_mc45:       defb 0          ; 0=OFF,1=ON
cfg_chr128:     defb 0          ; 0=CHR64,1=CHR128
cfg_joy_keys:   defb "QAOP "    ; teclas arriba/abajo/izda/dcha/fuego
cfg_vgm_loaded:   defb 0        ; 0=nada cargado, 1=hay un VGM cargado (sonando o en pausa)
cfg_vgm_playing:  defb 0        ; 0=parado/en pausa, 1=sonando
cfg_vgm_namelen:  defb 0
cfg_vgm_name:     defs VGM_NAME_MAXLEN
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

        end
