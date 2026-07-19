; =============================================================
; VTEST4.ASM -- paso 4 del diagnostico incremental en HW real
;
; Parte de vtest2 (marco estatico confirmado) y prueba la fuente
; comprimida de 6px (p42_string/p42_putchar) con texto FIJO (embebido en
; el programa, SIN tocar el MCU -- aisla la fuente de cualquier duda
; sobre el protocolo). Dibuja dos filas con el alfabeto, digitos y
; simbolos, casi llenando las 42 columnas, para poder ver si:
;   a) los 40 caracteres caben en las 42 columnas (compresion real de
;      6px, no serian 40 caracteres si fuera fuente normal de 8px: a
;      256px de ancho solo caben 32 caracteres de 8px).
;   b) los glifos (incluidos los redefinidos: %, &, 0, T, Y) se ven bien
;      formados, sin basura ni deformaciones.
;
; Se congela en un bucle infinito justo despues.
;
; Cargar igual que el explorador (mismo stub BASIC, USR 24576). NO vuelve
; nunca al BASIC (hay que apagar/resetear la maquina).
; Ensamblar: pasmo vtest4.asm VTEST4.BIN
; =============================================================
        org 24576

VIDBASE         equ 8000h       ; bloque 4, mapeo por defecto (como bounce)
VIDBASE_HI      equ 80h
ATTRBASE_HI     equ VIDBASE_HI+18h
NORM_ATTR       equ 038h        ; papel blanco, tinta negra

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

        ; --- texto FIJO (sin MCU) con la fuente comprimida de 6px ---
        ld a,NORM_ATTR
        ld (cur_attr),a

        ld d,1
        ld e,0
        call p42_setxy
        ld hl,testline1
        ld b,testline1_len
        call p42_string

        ld d,2
        ld e,0
        call p42_setxy
        ld hl,testline2
        ld b,testline2_len
        call p42_string

        ld d,4
        ld e,0
        call p42_setxy
        ld hl,testline3
        ld b,testline3_len
        call p42_string

forever:
        jr forever              ; congelado para siempre, nada mas se ejecuta

; -------------------------------------------------------------
; Cadenas de prueba: alfabeto (fila 1), digitos+simbolos (fila 2),
; glifos redefinidos %,&,0,T,Y otra vez juntos para verlos claros (fila 4).
; -------------------------------------------------------------
testline1:      defb "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
testline1_len   equ $-testline1
testline2:      defb "0123456789.:/<>=+-*"
testline2_len   equ $-testline2
testline3:      defb "TEST TY0% & 42 COLS OK?"
testline3_len   equ $-testline3

; -------------------------------------------------------------
; blit_row / calc_bmp_addr / calc_attr_addr: copia literal de
; explorer.asm, con VIDBASE_HI/ATTRBASE_HI recalculados para el bloque 4.
; -------------------------------------------------------------
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
