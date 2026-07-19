; =============================================================
; VTEST8.ASM -- paso 8 del diagnostico incremental en HW real
;
; Parte de vtest7 (movimiento de cursor 6/7 DENTRO de la ventana visible,
; CONFIRMADO en HW real) y añade el SCROLL: cruzar el borde de la
; ventana de 21 filas, con copy_row/clear_row/scroll_list_up/
; scroll_list_down (copia literal de explorer.asm, incluido el fix del
; bug de "scanline en blanco progresivo" encontrado antes en HW real).
;
; Objetivo: con esto, vt_down/vt_up quedan identicos a do_down/do_up del
; explorador real. Si funciona, el movimiento de cursor esta descartado
; del todo como causa de la pantalla negra de explorer.asm.
;
; Cargar igual que el explorador (mismo stub BASIC, USR 24576). ESPACIO
; para volver al BASIC.
; Ensamblar: pasmo vtest8.asm VTEST8.BIN
; =============================================================
        org 24576

VIDBASE         equ 8000h       ; bloque 4, mapeo por defecto (como bounce)
VIDBASE_HI      equ 80h
ATTRBASE_HI     equ VIDBASE_HI+18h
NORM_ATTR       equ 038h        ; papel blanco, tinta negra
SEL_ATTR        equ 00Fh        ; papel azul, tinta blanca (resaltado)
PATH_ATTR       equ 020h        ; papel verde, tinta negra
MAXVIS          equ 21
namebuf         equ VIDBASE+1B00h

        jp start
        defs 3                  ; hueco (mantiene el mapa parecido al explorador)

start:
        di

        ; --- HFILE = $8000 (bloque 4) ---
        xor a
        ld (2043),a             ; HFILE bajo
        ld a,VIDBASE_HI
        ld (2044),a             ; HFILE alto

        ; --- limpiar pantalla (bitmap a 0, atributos a NORM_ATTR) ---
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

        ; --- pegar el marco: fila 0 (banner) y fila 23 (iconos) ---
        ld ix,BG_ROW0
        xor a
        call blit_row
        ld ix,BG_ROW23
        ld a,23
        call blit_row

        ; --- abrir directorio raiz ---
        call do_opendir_root

        ; --- pedir y pintar la ruta actual (fila 22) ---
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
        jr c,vt_t1
        ld a,42
vt_t1:  ld b,a
        call p42_string
        ld a,22
        ld c,PATH_ATTR
        call fill_row_attr

        ; --- listado: filas 1..MAXVIS (identico a rs_loop/refresh_screen) ---
        ld hl,1
        ld (cur_index),hl
        ld (win_start),hl
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
        cp 42
        jr c,vlist_t2
        ld a,42
vlist_t2: ld b,a
        push bc
        call p42_string
        pop bc

        ld a,(cur_attr)
        cp SEL_ATTR
        jr nz,vlist_nofull
        push bc
        ld a,c
        inc a
        ld c,SEL_ATTR
        call fill_row_attr
        pop bc
vlist_nofull:

        ld hl,(row_ptr)
        inc hl
        ld (row_ptr),hl
        inc c
        jr vlist_loop
vlist_done:

; -------------------------------------------------------------
; vt_loop: bucle interactivo real -- read_key + 6/7 (movimiento SIN
; scroll, ver comentario de cabecera) + ESPACIO (salir al BASIC).
; Cualquier otra tecla se ignora y vuelve a esperar.
; -------------------------------------------------------------
vt_loop:
        call read_key
        cp 1
        jp z,vt_exit
        cp 3
        jp z,vt_down
        cp 4
        jp z,vt_up
        jr vt_loop

vt_exit:
        ld a,85
        ld (2045),a             ; Superfast OFF (vuelve a modo ZX81 normal)
        ld bc,7fefh
        xor a
        out (c),a               ; Chroma81 OFF
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
; explorer.asm, con VIDBASE_HI/ATTRBASE_HI recalculados para el bloque 4.
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

        ld b,8
cpr_loop:
        push bc
        ld hl,(cp_srcaddr)
        ld de,(cp_dstaddr)
        ld bc,32
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
        ld bc,32
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
        ld bc,31
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
        ld bc,31
        ldir
        ret

; -------------------------------------------------------------
; redraw_row_attr / fill_row_attr / blit_row / calc_bmp_addr /
; calc_attr_addr: copia literal de explorer.asm, con VIDBASE_HI/
; ATTRBASE_HI recalculados para el bloque 4.
; -------------------------------------------------------------
redraw_row_attr:
        push af
        push bc
        call get_row
        pop bc
        pop af
        ld d,a
        ld e,0
        call p42_setxy
        push af
        ld a,c
        ld (cur_attr),a
        ld hl,namebuf
        ld a,(namelen)
        cp 42
        jr c,rra_t1
        ld a,42
rra_t1: ld b,a
        call p42_string
        pop af
        call fill_row_attr
        ret

fill_row_attr:
        call calc_attr_addr
        ld (hl),c
        ld d,h
        ld e,l
        inc de
        ld bc,31
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
        jr z,rk_pgup
        bit 1,a
        jr z,rk_pgdn
        bit 4,a
        jr z,rk_k5
        ld a,0EFh
        in a,(0FEh)
        bit 4,a
        jr z,rk_k6
        bit 3,a
        jr z,rk_k7
        bit 2,a
        jr z,rk_k8
        ld a,0BFh
        in a,(0FEh)
        and 01h
        jr z,rk_enter
        ld a,7Fh
        in a,(0FEh)
        bit 0,a
        jr z,rk_space
        bit 3,a
        jr z,rk_n
        ld a,0FDh
        in a,(0FEh)
        bit 2,a
        jr z,rk_d
        ld a,0FBh
        in a,(0FEh)
        bit 3,a
        jr z,rk_r
        ld a,0FEh
        in a,(0FEh)
        bit 3,a
        jr z,rk_c
        bit 2,a
        jr z,rk_x
        bit 4,a
        jr z,rk_v
        jr rk_wait
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
        and 01h
        jr z,rk_stillp
        ld a,7Fh
        in a,(0FEh)
        and 09h
        cp 09h
        jr nz,rk_stillp
        ld a,0FDh
        in a,(0FEh)
        and 04h
        jr z,rk_stillp
        ld a,0FBh
        in a,(0FEh)
        and 08h
        jr z,rk_stillp
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
cur_attr:       defb NORM_ATTR
namelen:        defb 0
mcustatus:      defb 0
xycoords:       defb 0,0
p42_workspace:  defs 8

; -------------------------------------------------------------
; Fuente 6x8 y decoracion de pantalla (mismos recursos que el explorador)
; -------------------------------------------------------------
FONTBASE:
        incbin "specfont.bin"

BG_ROW0:
        incbin "bg_row0.bin"
BG_ROW23:
        incbin "bg_row23.bin"

        end
