; =====================================================================
;  GFX.INC - la pantalla grafica del ajedrez: Superfast HiRes Spectrum (256x192, un
;  atributo por celda de 8x8) con el bitmap en HFILE ($A000, bloque 5) y los atributos
;  tras el: HFILE+$1800. Las piezas son de 16x16 (2x2 celdas); el color de cada celda
;  es el de la casilla (papel) y el de la pieza (tinta: blanca o negra, el mismo dibujo).
;  Todo el tablero va en brillante (el bit de brillo es de la celda entera, papel y tinta, y la
;  tinta blanca brillante lo exige): casillas claras verdes y oscuras rojas. El texto sale de
;  la fuente de la ROM ($1E00, codigos del ZX81) a una celda por caracter.
;
;  El juego que lo incluye define:  txt_on (lobby.inc) y las variables HL_FROM, HL_TO,
;  CHK_SQ, SQ_PTR (2) y SQ_ATTR (una cada una, contiguas desde HL_FROM), y las filas y
;  columnas del tablero: BOARD_ROW, BOARD_COL.
;
;  Rutinas (destruyen los registros que digan):
;      gfx_on            pasa a HiRes Spectrum con la pantalla en blanco (texto blanco sobre negro)
;      gfx_off           vuelve al texto de 32 columnas del lobby
;      gaddr             B = fila 0-23, C = columna 0-31 -> HL = direccion de la celda (linea 0)
;      gattr             B = fila, C = columna -> HL = direccion de su atributo (destruye DE)
;      gglyph            A = codigo ZX81, HL = celda -> dibuja el caracter (destruye A, B, DE, HL)
;      gglyph_h          A = codigo, B/C = fila/columna de la celda izquierda: lo centra en las dos celdas
;      gglyph_v          A = codigo, B/C = fila/columna de la celda de arriba: lo centra en las dos celdas
;      gprint_at         HL = texto ASCII (0), B = fila, C = columna (destruye todo)
;      gprint_row        igual, borrando antes la fila
;      gclr_row          B = fila: la deja en blanco, con los atributos del texto
;      gattr_row         B = fila, A = atributo
;      draw_square       A = pieza (0 vacia), UI_Y/UI_X = casilla en pantalla, UI_R/UI_F = rank y
;                        columna del tablero: la dibuja con su color (blancas: tinta blanca)
; =====================================================================

HFILE       equ 0A000h
ATTRB       equ HFILE+1800h
ROMFONT     equ 1E00h

ATTR_TEXT   equ 047h            ; tinta blanca brillante sobre negro
SQ_DARK     equ 050h            ; brillante, papel rojo
SQ_LIGHT    equ 060h            ; brillante, papel verde
SQ_LAST     equ 058h            ; brillante, papel magenta: la ultima jugada
SQ_CHECK_W  equ 040h            ; el rey BLANCO en jaque: papel negro (su tinta es blanca)
SQ_CHECK_B  equ 078h            ; el rey NEGRO en jaque: papel blanco (su tinta es negra)
INK_WHITE   equ 007h            ; tinta de las piezas blancas (las negras, 0)
CHROMA_P    equ 07FEFh
CHROMA_ON   equ 020h            ; color, modo 0, borde negro
CHROMA_OFF  equ 00Ch

HL_FROM     equ UI_BASE+40
HL_TO       equ UI_BASE+41
CHK_SQ      equ UI_BASE+42
SQ_PTR      equ UI_BASE+43      ; (2)
SQ_ATTR     equ UI_BASE+45

; --- el modo --------------------------------------------------------
gfx_on:     xor  a
            ld   (2043),a           ; HFILE
            ld   a,HFILE/256
            ld   (2044),a
            ld   hl,HFILE           ; bitmap en blanco
            ld   de,HFILE+1
            ld   bc,17FFh
            ld   (hl),0
            ldir
            ld   hl,ATTRB           ; atributos del texto
            ld   de,ATTRB+1
            ld   bc,2FFh
            ld   (hl),ATTR_TEXT
            ldir
            ld   bc,CHROMA_P        ; color activado, modo 0, borde negro
            ld   a,CHROMA_ON
            out  (c),a
            ld   a,172              ; Superfast HiRes Spectrum
            ld   (2045),a
            xor  a
            out  (0FBh),a           ; borde negro y sin beeper
            ret

gfx_off:    ld   a,85
            ld   (2045),a
            ld   bc,CHROMA_P
            ld   a,CHROMA_OFF
            out  (c),a
            jp   txt_on

; --- direcciones ----------------------------------------------------
; fila B, columna C -> HL = HFILE + (B and 18h)*256 + (B and 7)*32 + C
gaddr:      ld   a,b
            and  18h
            or   HFILE/256
            ld   h,a
            ld   a,b
            and  7
            rrca
            rrca
            rrca                    ; * 32
            add  a,c
            ld   l,a
            ret

; fila B, columna C -> HL = ATTRB + B*32 + C
gattr:      ld   l,b
            ld   h,0
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl
            ld   e,c
            ld   d,0
            add  hl,de
            ld   de,ATTRB
            add  hl,de
            ret

; --- texto ----------------------------------------------------------
; A = codigo de caracter del ZX81, HL = celda: sus 8 lineas
gglyph:     push hl
            ld   l,a
            ld   h,0
            add  hl,hl
            add  hl,hl
            add  hl,hl
            ld   de,ROMFONT
            add  hl,de
            ex   de,hl              ; DE = el caracter en la ROM
            pop  hl
            ld   b,8
gg_l:       ld   a,(de)
            ld   (hl),a
            inc  de
            inc  h
            djnz gg_l
            ret

; A = codigo, B = fila, C = columna de la celda izquierda: centrado en horizontal en las dos celdas
; (cada linea del caracter, desplazada 4 puntos a medias entre las dos)
gglyph_h:   ld   l,a
            ld   h,0
            add  hl,hl
            add  hl,hl
            add  hl,hl
            ld   de,ROMFONT
            add  hl,de
            ex   de,hl              ; DE = el caracter en la ROM
            call gaddr              ; HL = la celda izquierda (no toca DE)
            ld   b,8
gh_l:       ld   a,(de)
            ld   c,a
            rrca
            rrca
            rrca
            rrca
            and  0Fh
            ld   (hl),a             ; izquierda: la mitad alta, 4 puntos a la derecha
            inc  l
            ld   a,c
            rlca
            rlca
            rlca
            rlca
            and  0F0h
            ld   (hl),a             ; derecha: la mitad baja, 4 puntos a la izquierda
            dec  l
            inc  de
            inc  h
            djnz gh_l
            ret

; A = codigo, B = fila, C = columna de la celda de arriba: centrado en vertical en las dos celdas
; (4 lineas abajo en la de arriba y las otras 4 al principio de la de abajo)
gglyph_v:   push bc
            ld   l,a
            ld   h,0
            add  hl,hl
            add  hl,hl
            add  hl,hl
            ld   de,ROMFONT
            add  hl,de
            ex   de,hl              ; DE = el caracter en la ROM
            call gaddr              ; HL = la celda de arriba
            ld   a,h
            add  a,4
            ld   h,a                ; lineas 4-7
            ld   b,4
gv_u:       ld   a,(de)
            ld   (hl),a
            inc  de
            inc  h
            djnz gv_u
            pop  bc
            inc  b                  ; la celda de abajo, lineas 0-3
            call gaddr
            ld   b,4
gv_d:       ld   a,(de)
            ld   (hl),a
            inc  de
            inc  h
            djnz gv_d
            ret

; HL = texto ASCII terminado en 0, B = fila, C = columna
gprint_at:  ld   a,(hl)
            or   a
            ret  z
            inc  hl
            push hl
            push bc
            call a2z                ; destruye C y HL
            pop  bc
            push bc
            push af
            call gaddr
            pop  af
            call gglyph
            pop  bc
            inc  c
            pop  hl
            jr   gprint_at

; B = fila: bitmap en blanco y atributos del texto
gclr_row:   push bc
            ld   c,0
            call gaddr
            ld   a,8
gc_l:       push hl
            ld   d,h
            ld   e,l
            inc  de
            ld   (hl),0
            ld   bc,31
            ldir
            pop  hl
            inc  h
            dec  a
            jr   nz,gc_l
            pop  bc
            ld   a,ATTR_TEXT
; B = fila, A = atributo para sus 32 celdas
gattr_row:  push bc
            ld   c,0
            push af
            call gattr
            pop  af
            ld   d,h
            ld   e,l
            inc  de
            ld   (hl),a
            ld   bc,31
            ldir
            pop  bc
            ret

gprint_row: push hl
            push bc
            call gclr_row
            pop  bc
            pop  hl
            jp   gprint_at

; --- una casilla ----------------------------------------------------
; A = pieza (0 vacia): la dibuja en la casilla de pantalla (UI_Y, UI_X), rank UI_R, columna UI_F
draw_square: ld  c,a                ; C = la pieza
            ld   a,(UI_R)
            ld   hl,UI_F
            add  a,(hl)
            and  1
            ld   b,SQ_DARK          ; (rank + columna) par = casilla oscura (a1)
            jr   z,ds_pap
            ld   b,SQ_LIGHT
ds_pap:     ld   a,(UI_R)           ; resaltados, por numero de casilla (rank*8 + columna)
            add  a,a
            add  a,a
            add  a,a
            ld   hl,UI_F
            add  a,(hl)
            ld   hl,CHK_SQ
            cp   (hl)
            jr   nz,ds_nochk
            ld   b,SQ_CHECK_W       ; el rey en jaque, con el papel del color contrario a su tinta
            bit  3,c
            jr   z,ds_np
            ld   b,SQ_CHECK_B
            jr   ds_np
ds_nochk:   ld   hl,HL_FROM
            cp   (hl)
            jr   z,ds_last
            inc  hl                 ; HL_TO
            cp   (hl)
            jr   nz,ds_np
ds_last:    ld   b,SQ_LAST          ; la ultima jugada, en amarillo
ds_np:      ld   a,c
            or   a
            ld   hl,pc_none
            jr   z,ds_att
            bit  3,c
            jr   nz,ds_blk
            ld   a,b
            or   INK_WHITE          ; las blancas, con tinta blanca (las negras, con la negra)
            ld   b,a
ds_blk:     ld   a,c
            and  7                  ; tipo 1-6
            dec  a
            rlca
            rlca
            rlca
            rlca
            rlca                    ; * 32
            ld   e,a
            ld   d,0
            ld   hl,pc_solid
            add  hl,de
ds_att:     ld   a,b
            ld   (SQ_ATTR),a
ds_src:     ld   a,(UI_Y)           ; la celda de arriba a la izquierda de la casilla
            add  a,a
            add  a,BOARD_ROW
            ld   b,a
            ld   a,(UI_X)
            add  a,a
            add  a,BOARD_COL
            ld   c,a
            call ds_cell
            inc  c
            call ds_cell
            inc  b
            dec  c
            call ds_cell
            inc  c
; una celda: B = fila, C = columna, HL = sus 8 bytes (avanza 8); el color es SQ_ATTR. B y C se conservan
ds_cell:    push bc
            push hl
            call gaddr
            ex   de,hl              ; DE = destino
            pop  hl
            ld   b,8
dc_l:       ld   a,(hl)
            ld   (de),a
            inc  hl
            inc  d
            djnz dc_l
            pop  bc
            push bc
            push hl
            call gattr
            ld   a,(SQ_ATTR)
            ld   (hl),a
            pop  hl
            pop  bc
            ret

pc_none:    defs 32,0
