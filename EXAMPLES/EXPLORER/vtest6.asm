; =============================================================
; VTEST6.ASM -- diagnostico de estados MCU en HW real (sin depurador)
;
; Parte de vtest5 (marco + ruta, confirmado) pero SIN el bucle de
; listado completo: en su lugar, captura los BYTES DE ESTADO que
; devuelve el MCU en cada paso y los pinta en pantalla como numeros
; decimales, para poder verlos sin depurador en la maquina real:
;
;   OD=nnn  -> status devuelto por OPENDIR2 (0=OK, !=0 fallo)
;   L1=nnn  -> namelen tras GETROW(indice=1) (0 si no encontro nada)
;   S1=nnn  -> status devuelto por ese GETROW(1)
;
; Hipotesis a confirmar: cmd_getrow (firmware, COMMANDS.cpp) usa para
; indice>=1 el array file_array[], que SOLO se rellena en cmd_opendir2;
; el indice 0 (la ruta) usa current_dir directamente y por eso funciona
; siempre. Si OPENDIR2 fallo silenciosamente en HW real (OD!=0), el
; listado (indice>=1) nunca puede encontrar nada -- exactamente el
; sintoma visto en vtest5 (ruta bien, listado vacio, sin resaltado).
;
; Se congela en un bucle infinito despues de pintar los 3 valores.
;
; Cargar igual que el explorador (mismo stub BASIC, USR 24576). NO vuelve
; nunca al BASIC (hay que apagar/resetear la maquina).
; Ensamblar: pasmo vtest6.asm VTEST6.BIN
; =============================================================
        org 24576

VIDBASE         equ 8000h       ; bloque 4, mapeo por defecto (como bounce)
VIDBASE_HI      equ 80h
ATTRBASE_HI     equ VIDBASE_HI+18h
NORM_ATTR       equ 038h        ; papel blanco, tinta negra
PATH_ATTR       equ 020h        ; papel verde, tinta negra
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

        ; --- abrir directorio raiz: CAPTURAR el status devuelto ---
        call do_opendir_root
        ld (od_status),a

        ; --- pedir y pintar la ruta actual (fila 22), igual que antes ---
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

        ; --- pedir la fila 1 (primer archivo): CAPTURAR namelen y status ---
        ld de,1
        call get_row
        ld a,(namelen)
        ld (l1_namelen),a
        ld a,(mcustatus)
        ld (s1_status),a

        ; --- pintar los 3 diagnosticos como texto+numero en pantalla ---
        ld a,NORM_ATTR
        ld (cur_attr),a

        ld d,5
        ld e,0
        call p42_setxy
        ld hl,msg_od
        ld b,msg_od_len
        call p42_string
        ld a,(od_status)
        ld de,digbuf
        call byte_to_dec3
        ld hl,digbuf
        ld b,3
        call p42_string

        ld d,6
        ld e,0
        call p42_setxy
        ld hl,msg_l1
        ld b,msg_l1_len
        call p42_string
        ld a,(l1_namelen)
        ld de,digbuf
        call byte_to_dec3
        ld hl,digbuf
        ld b,3
        call p42_string

        ld d,7
        ld e,0
        call p42_setxy
        ld hl,msg_s1
        ld b,msg_s1_len
        call p42_string
        ld a,(s1_status)
        ld de,digbuf
        call byte_to_dec3
        ld hl,digbuf
        ld b,3
        call p42_string

forever:
        jr forever              ; congelado para siempre, nada mas se ejecuta

; -------------------------------------------------------------
; byte_to_dec3: IN A=valor(0-255), DE=puntero destino (3 bytes).
; Escribe 3 digitos ASCII (con ceros a la izquierda) en (DE),(DE+1),(DE+2).
; Destruye AF,BC,DE,HL.
; -------------------------------------------------------------
byte_to_dec3:
        ld l,a
        ld b,0
b2d_c:
        ld a,l
        cp 100
        jr c,b2d_cdone
        sub 100
        ld l,a
        inc b
        jr b2d_c
b2d_cdone:
        ld a,b
        add a,'0'
        ld (de),a
        inc de

        ld b,0
b2d_d:
        ld a,l
        cp 10
        jr c,b2d_ddone
        sub 10
        ld l,a
        inc b
        jr b2d_d
b2d_ddone:
        ld a,b
        add a,'0'
        ld (de),a
        inc de

        ld a,l
        add a,'0'
        ld (de),a
        ret

msg_od:  defb "OD="
msg_od_len equ $-msg_od
msg_l1:  defb "L1="
msg_l1_len equ $-msg_l1
msg_s1:  defb "S1="
msg_s1_len equ $-msg_s1
digbuf:  defs 3
od_status:   defb 0
l1_namelen:  defb 0
s1_status:   defb 0

; -------------------------------------------------------------
; fill_row_attr / blit_row / calc_bmp_addr / calc_attr_addr: copia
; literal de explorer.asm, con VIDBASE_HI/ATTRBASE_HI recalculados para
; el bloque 4.
; -------------------------------------------------------------
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
