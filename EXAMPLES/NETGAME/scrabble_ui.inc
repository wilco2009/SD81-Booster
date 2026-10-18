; =====================================================================
;  SCRABBLE_UI.INC - la pantalla del Scrabble: Superfast HiRes Spectrum (256x192, un atributo por celda de 8x8),
;  con gfx.inc. El tablero son 15 x 15 casillas de UNA celda cada una (una letra de la fuente de la ROM por casilla,
;  y el color de la casilla en su atributo: no hay conflicto de color); el atril, 7 fichas de 16 x 16 (la letra de la
;  fuente al doble de tamano) con su valor debajo; a la derecha, el panel. Las fichas N con tilde, CH, LL y RR y la
;  estrella del centro no estan en la ROM: se dibujan de sbu_cg y sbu_star.
;
;  La pantalla (32 x 24 celdas):
;       0         titulo
;       1         las letras A-O sobre las columnas
;       2-16      el tablero (columnas 2-16) con los numeros 1-15 a su izquierda; el panel (columnas 18-31)
;       17-18     el atril (columnas 2-22), 19 su valor
;       20        el estado (caja verde)   21 el aviso (rojo)   22 la ayuda (cian)   23 la jugada que se teclea
;
;  El que lo incluye define antes SU_BASE (SU_LEN bytes), y tiene scrabble.inc, gfx.inc (GFX_SQUARES = 0, UI_BASE) y
;  a2z (ASCII a codigo del ZX81, de lobby.inc).
;
;  Rutinas (destruyen todos los registros):
;      sbu_draw_all     gfx_on y todo: titulo, ejes, leyenda, tablero, atril, panel
;      sbu_board        el tablero (todas las casillas)           sbu_rack  el atril de SU_ME
;      sbu_panel        los puntos, la bolsa y el turno           sbu_note_last  guarda la jugada de WB/NF/SB_TOT para
;                                                                  resaltarla y escribirla en el panel (tras sb_apply)
;      sbu_status / sbu_msg / sbu_help / sbu_input     HL = texto ASCII: lo escribe en su fila
;      sbu_reset        partida nueva: quita la ultima jugada       sbu_note_text  A = quien, HL = texto: una ultima jugada sin fichas
;  Variables: SU_ME (mi jugador, 0 o 1)
; =====================================================================

BOARD_ROW   equ 2
BOARD_COL   equ 2
PANEL_COL   equ 18
RACK_ROW    equ 17
RACK_COL    equ 2
ROW_HDR     equ 1                ; (ROW_TITLE = 0 y ROW_HELP = 22 los pone lobby.inc)
ROW_VAL     equ 19
ROW_STATUS  equ 20
ROW_MSG     equ 21
ROW_INPUT   equ 23

AT_NORM     equ 020h            ; casilla normal: papel verde, tinta negra
AT_DL       equ 068h            ; letra doble: papel cian brillante
AT_TL       equ 04Fh            ; letra triple: papel azul brillante, tinta blanca
AT_DW       equ 058h            ; palabra doble: papel magenta brillante
AT_TW       equ 057h            ; palabra triple: papel rojo brillante, tinta blanca
AT_TILE     equ 078h            ; ficha: papel blanco (negro de tinta)
AT_BLANK    equ 079h            ; comodin (vale 0): como una ficha pero con la tinta azul, en el tablero y en el atril
AT_LAST     equ 070h            ; ficha de la ultima jugada: papel amarillo (se distingue del verde del tablero y del blanco de las demas, haya o no brillo)
AT_LASTB    equ 071h            ; comodin de la ultima jugada
AT_TITLE    equ 070h
AT_STATUS   equ 060h
AT_MSG      equ 042h
AT_HELP     equ 005h
AT_HAND     equ 078h            ; las fichas del atril

SU_ME       equ SU_BASE+0
SU_LASTV    equ SU_BASE+1       ; 1 si hay una ultima jugada
SU_LX       equ SU_BASE+2
SU_LY       equ SU_BASE+3
SU_LDIR     equ SU_BASE+4
SU_LN       equ SU_BASE+5
SU_LPTS     equ SU_BASE+6       ; (2)
SU_LWHO     equ SU_BASE+8       ; quien la hizo (0 o 1)
SU_LNF      equ SU_BASE+9       ; (16) las fichas que puso (como NF)
SU_LTXT     equ SU_BASE+25      ; (32) la palabra en ASCII, terminada en 0 (se corta a 13 letras al escribirla)
SU_BUF      equ SU_BASE+57      ; (32) la ficha grande
SU_NUM      equ SU_BASE+89      ; (8) un numero en ASCII
SU_X        equ SU_BASE+97
SU_Y        equ SU_BASE+98
SU_C        equ SU_BASE+99
SU_T        equ SU_BASE+100
SU_R        equ SU_BASE+101     ; sbu_putnum: fila, ultima columna, longitud y signo
SU_K        equ SU_BASE+102
SU_L        equ SU_BASE+103
SU_S        equ SU_BASE+104
SU_LEN      equ 110

; ---------------------------------------------------------------------
;  Todo
; ---------------------------------------------------------------------
sbu_draw_all:
            call gfx_on
            ld   a,AT_TITLE
            ld   b,ROW_TITLE
            call gattr_row
            ld   hl,sbu_t_title
            ld   bc,ROW_TITLE*256+12
            call gprint_at
            call sbu_axes
            call sbu_legend
            call sbu_board
            call sbu_rack
            jp   sbu_panel

; las letras A-O sobre las columnas y los numeros 1-15 a la izquierda de las filas
sbu_axes:   ld   b,0
sax_h:      push bc
            ld   a,b
            add  a,BOARD_COL
            ld   c,a
            ld   b,ROW_HDR
            call gaddr
            pop  bc
            push bc
            ld   a,b
            add  a,38               ; 'A' en codigos del ZX81
            call gglyph
            pop  bc
            inc  b
            ld   a,b
            cp   15
            jr   nz,sax_h
            ld   b,0
sax_n:      push bc
            ld   a,b
            inc  a
            ld   d,a                ; D = el numero
            ld   a,b
            add  a,BOARD_ROW
            ld   b,a                ; B = la fila
            call sbu_num2
            pop  bc
            inc  b
            ld   a,b
            cp   15
            jr   nz,sax_n
            ret

; D = numero (1-99), B = fila: lo escribe en las columnas 0 y 1, alineado a la derecha
sbu_num2:   ld   a,d
            ld   e,0
n2_l:       cp   10
            jr   c,n2_d
            sub  10
            inc  e
            jr   n2_l
n2_d:       add  a,28
            ld   d,a                ; D = codigo de las unidades
            ld   a,e
            or   a
            jr   z,n2_u
            add  a,28               ; las decenas
            push de
            push bc
            ld   c,0
            push af
            call gaddr
            pop  af
            call gglyph
            pop  bc
            pop  de
n2_u:       ld   c,1
            push af
            call gaddr
            pop  af
            ld   a,d
            jp   gglyph

; los premios: cuatro casillas de color con su nombre
sbu_legend: ld   hl,sbu_t_dl
            ld   bc,13*256+PANEL_COL
            call gprint_at
            ld   a,AT_DL
            ld   b,13
            ld   c,PANEL_COL
            ld   e,2
            call gattr_span
            ld   hl,sbu_t_tl
            ld   bc,13*256+(PANEL_COL+3)
            call gprint_at
            ld   a,AT_TL
            ld   b,13
            ld   c,PANEL_COL+3
            ld   e,2
            call gattr_span
            ld   hl,sbu_t_dw
            ld   bc,13*256+(PANEL_COL+6)
            call gprint_at
            ld   a,AT_DW
            ld   b,13
            ld   c,PANEL_COL+6
            ld   e,2
            call gattr_span
            ld   hl,sbu_t_tw
            ld   bc,13*256+(PANEL_COL+9)
            call gprint_at
            ld   a,AT_TW
            ld   b,13
            ld   c,PANEL_COL+9
            ld   e,2
            jp   gattr_span

; ---------------------------------------------------------------------
;  Las fichas: la imagen de 8 x 8
; ---------------------------------------------------------------------
; A = ficha (el bit 7 se ignora) -> HL = sus 8 bytes
sbu_glyph:  and  7Fh
            cp   '?'
            jr   z,sg_q
            cp   '['
            jr   nc,sg_c
            sub  'A'-38             ; A-Z: la fuente de la ROM
            ld   l,a
            ld   h,0
            add  hl,hl
            add  hl,hl
            add  hl,hl
            ld   de,ROMFONT
            add  hl,de
            ret
sg_c:       sub  '['                ; N con tilde, CH, LL, RR
            ld   l,a
            ld   h,0
            add  hl,hl
            add  hl,hl
            add  hl,hl
            ld   de,sbu_cg
            add  hl,de
            ret
sg_q:       ld   hl,ROMFONT+15*8    ; el comodin en el atril: un "?"
            ret

; ---------------------------------------------------------------------
;  El tablero
; ---------------------------------------------------------------------
sbu_board:  xor  a
            ld   (SU_Y),a
sbb_y:      xor  a
            ld   (SU_X),a
sbb_x:      ld   a,(SU_X)
            ld   b,a
            ld   a,(SU_Y)
            ld   c,a
            call sb_addr
            ld   a,(hl)
            ld   (SU_C),a
            or   a
            jr   nz,sbb_tile
            ld   a,(SU_X)           ; una casilla vacia: el color de su premio
            ld   b,a
            ld   a,(SU_Y)
            ld   c,a
            call sb_prem
            ld   e,a
            ld   d,0
            ld   hl,sbu_pattr
            add  hl,de
            ld   a,(hl)
            ld   (SQ_ATTR),a
            ld   hl,sbu_zero
            ld   a,(SU_X)           ; el centro lleva una estrella
            cp   7
            jr   nz,sbb_draw
            ld   a,(SU_Y)
            cp   7
            jr   nz,sbb_draw
            ld   hl,sbu_star
            jr   sbb_draw
sbb_tile:   call sbu_islast         ; A = 1 si es de la ultima jugada
            add  a,a
            ld   b,a                ; 0 o 2
            ld   a,(SU_C)
            rlca                    ; CF = el bit 7: un comodin
            ld   a,0
            adc  a,b
            ld   e,a
            ld   d,0
            ld   hl,sbu_tattr
            add  hl,de
            ld   a,(hl)
            ld   (SQ_ATTR),a
            ld   a,(SU_C)
            call sbu_glyph
sbb_draw:   ld   a,(SU_Y)
            add  a,BOARD_ROW
            ld   b,a
            ld   a,(SU_X)
            add  a,BOARD_COL
            ld   c,a
            call ds_cell
            ld   hl,SU_X
            inc  (hl)
            ld   a,(hl)
            cp   15
            jr   nz,sbb_x
            ld   hl,SU_Y
            inc  (hl)
            ld   a,(hl)
            cp   15
            jr   nz,sbb_y
            ret

; A = 1 si la casilla (SU_X, SU_Y) es una ficha puesta en la ultima jugada
sbu_islast: ld   a,(SU_LASTV)
            or   a
            ret  z
            ld   a,(SU_LDIR)
            or   a
            jr   nz,sil_v
            ld   a,(SU_Y)           ; horizontal: la misma fila y x en [x0, x0 + n)
            ld   b,a
            ld   a,(SU_LY)
            cp   b
            jr   nz,sil_no
            ld   a,(SU_X)
            ld   b,a
            ld   a,(SU_LX)
            ld   c,a
            ld   a,b
            sub  c
            jr   c,sil_no
            jr   sil_i
sil_v:      ld   a,(SU_X)
            ld   b,a
            ld   a,(SU_LX)
            cp   b
            jr   nz,sil_no
            ld   a,(SU_Y)
            ld   b,a
            ld   a,(SU_LY)
            ld   c,a
            ld   a,b
            sub  c
            jr   c,sil_no
sil_i:      ld   b,a
            ld   a,(SU_LN)
            ld   c,a
            ld   a,b
            cp   c
            jr   nc,sil_no
            ld   e,b
            ld   d,0
            ld   hl,SU_LNF
            add  hl,de
            ld   a,(hl)
            or   a
            jr   z,sil_no
            ld   a,1
            ret
sil_no:     xor  a
            ret

; A = quien (0 o 1), HL = texto ASCII (0): una ultima jugada sin fichas (pasar, cambiar)
sbu_note_text: ld (SU_LWHO),a
            ld   a,1
            ld   (SU_LASTV),a
            xor  a
            ld   (SU_LN),a
            ld   (SU_LPTS),a
            ld   (SU_LPTS+1),a
            ld   de,SU_LTXT
nt_l:       ld   a,(hl)
            ld   (de),a
            inc  hl
            inc  de
            or   a
            jr   nz,nt_l
            ret

; partida nueva: ninguna ultima jugada que resaltar
sbu_reset:  xor  a
            ld   (SU_LASTV),a
            ret

; la jugada que acaba de hacer el jugador SB_TURN (antes de que sb_apply le pase el turno: llamar justo
; DESPUES de sb_apply con A = el jugador que la hizo): WB, NF y SB_TOT, a SU_L*
sbu_note_last: ld (SU_LWHO),a
            ld   a,1
            ld   (SU_LASTV),a
            ld   a,(WB)
            ld   (SU_LX),a
            ld   a,(WB+1)
            ld   (SU_LY),a
            ld   a,(WB+2)
            ld   (SU_LDIR),a
            ld   a,(WB+3)
            cp   16                 ; (una longitud imposible no puede desbordar SU_LTXT)
            jr   c,nl_n1
            ld   a,15
nl_n1:      ld   (SU_LN),a
            ld   hl,(SB_TOT)
            ld   (SU_LPTS),hl
            ld   hl,NF
            ld   de,SU_LNF
            ld   bc,16
            ldir
            ld   hl,WB+4            ; el texto: cada ficha como letras (N con tilde = N, CH, LL y RR, dos)
            ld   de,SU_LTXT
            ld   a,(SU_LN)
            ld   b,a
            or   a
            jr   z,nl_d0
nl_l:       ld   a,(hl)
            inc  hl
            call sbu_t2a
            djnz nl_l
nl_d0:      xor  a
            ld   (de),a
            ld   (SU_LTXT+14),a     ; el panel solo tiene 14 columnas
            ret

; A = ficha (el bit 7 se ignora), DE = destino: escribe sus letras en ASCII (N con tilde = N, CH, LL y RR, dos letras) y avanza DE
sbu_t2a:    and  7Fh
            cp   '['
            jr   nc,t2_s
            ld   (de),a
            inc  de
            ret
t2_s:       jr   nz,t2_ch
            ld   a,'N'
            ld   (de),a
            inc  de
            ret
t2_ch:      cp   5Ch
            jr   nz,t2_ll
            ld   a,'C'
            ld   (de),a
            inc  de
            ld   a,'H'
            ld   (de),a
            inc  de
            ret
t2_ll:      cp   ']'
            ld   a,'L'
            jr   z,t2_2
            ld   a,'R'
t2_2:       ld   (de),a
            inc  de
            ld   (de),a
            inc  de
            ret

; ---------------------------------------------------------------------
;  El atril: 7 fichas de 16 x 16 (2 x 2 celdas, con una celda de separacion) con su valor debajo
; ---------------------------------------------------------------------
sbu_rack:   xor  a
            ld   (SU_T),a
sbr_l:      ld   a,(SU_ME)          ; la ficha: SB_RACK + 7 * yo + i
            ld   e,a
            add  a,a
            add  a,a
            add  a,a
            sub  e
            ld   hl,SU_T
            add  a,(hl)
            ld   e,a
            ld   d,0
            ld   hl,SB_RACK
            add  hl,de
            ld   a,(hl)
            ld   (SU_C),a
            or   a
            jr   z,sbr_empty
            call sbu_glyph          ; HL = la letra; se agranda a SU_BUF
            call sbu_big
            ld   a,(SU_C)           ; el comodin, con la tinta azul
            cp   '?'
            ld   a,AT_HAND
            jr   nz,sbr_at
            ld   a,AT_BLANK
sbr_at:     ld   (SQ_ATTR),a
            call sbr_pos
            ld   hl,SU_BUF
            call ds_cell            ; arriba a la izquierda
            inc  c
            call ds_cell            ; arriba a la derecha
            inc  b
            dec  c
            call ds_cell            ; abajo a la izquierda
            inc  c
            call ds_cell            ; abajo a la derecha
            ld   a,(SU_C)
            call sb_tileval         ; el valor, debajo
            ld   d,a
            call sbr_pos
            ld   a,d
            call sbu_value
            jr   sbr_next
sbr_empty:  ld   a,AT_NORM          ; sin ficha: un hueco verde
            ld   (SQ_ATTR),a
            call sbr_pos
            ld   hl,sbu_zero
            call ds_cell
            inc  c
            ld   hl,sbu_zero
            call ds_cell
            inc  b
            dec  c
            ld   hl,sbu_zero
            call ds_cell
            inc  c
            ld   hl,sbu_zero
            call ds_cell
            call sbr_pos
            ld   a,255
            call sbu_value          ; borra el valor
sbr_next:   ld   hl,SU_T
            inc  (hl)
            ld   a,(hl)
            cp   7
            jp   nz,sbr_l
            ret

; B = fila y C = columna de la ficha SU_T del atril
sbr_pos:    ld   a,(SU_T)
            ld   c,a
            add  a,a
            add  a,c                ; 3 celdas por ficha: 2 de ficha y 1 de separacion
            add  a,RACK_COL
            ld   c,a
            ld   b,RACK_ROW
            ret

; A = valor (255 = nada), C = columna de la ficha: lo escribe debajo (fila ROW_VAL), en dos celdas
sbu_value:  ld   d,a
            ld   b,ROW_VAL
            push bc
            call gaddr              ; borra las dos celdas
            ld   e,8
sv_z:       ld   (hl),0
            inc  l
            ld   (hl),0
            dec  l
            inc  h
            dec  e
            jr   nz,sv_z
            pop  bc
            ld   a,d
            cp   255
            ret  z
            cp   10
            jr   c,sv_one
            push bc                 ; 10: "1" y "0"
            call gaddr
            ld   a,29
            call gglyph
            pop  bc
            inc  c
            ld   a,28
            jr   sv_dg
sv_one:     add  a,28
sv_dg:      push af
            call gaddr
            pop  af
            jp   gglyph

; HL = la letra de 8 x 8 -> SU_BUF = la ficha de 16 x 16 en 4 celdas de 8 bytes: arriba-izquierda, arriba-derecha,
; abajo-izquierda, abajo-derecha (cada punto es 2 x 2)
sbu_big:    ld   b,0                ; B = la linea del dibujo (0-7)
sbg_l:      ld   a,(hl)
            inc  hl
            push hl
            push bc
            ld   c,a
            rrca
            rrca
            rrca
            rrca
            and  0Fh
            call sbg_dbl
            ld   (SU_X),a           ; la mitad izquierda, doblada
            ld   a,c
            and  0Fh
            call sbg_dbl
            ld   (SU_Y),a           ; la derecha
            pop  bc
            ld   a,b                ; destino = 2 * linea + 8 * (linea / 4)
            add  a,a
            ld   e,a
            ld   a,b
            and  4
            add  a,a
            add  a,e
            ld   e,a
            ld   d,0
            ld   hl,SU_BUF
            add  hl,de
            ld   a,(SU_X)
            ld   (hl),a
            inc  hl
            ld   (hl),a
            ld   de,7               ; la celda de la derecha esta 8 bytes mas alla
            add  hl,de
            ld   a,(SU_Y)
            ld   (hl),a
            inc  hl
            ld   (hl),a
            pop  hl
            inc  b
            ld   a,b
            cp   8
            jr   nz,sbg_l
            ret
; un nibble (0-15) a un byte con cada bit doble
sbg_dbl:    push hl
            push de
            ld   l,a
            ld   h,0
            ld   de,sbu_dbl
            add  hl,de
            ld   a,(hl)
            pop  de
            pop  hl
            ret

; ---------------------------------------------------------------------
;  El panel
; ---------------------------------------------------------------------
sbu_panel:  ld   hl,sbu_t_you
            ld   bc,2*256+PANEL_COL
            call sbp_label
            ld   hl,sbu_t_rival
            ld   bc,3*256+PANEL_COL
            call sbp_label
            ld   a,(SU_ME)          ; mis puntos, y los del otro
            call sbp_score
            ld   bc,2*256+31
            call sbu_putnum
            ld   a,(SU_ME)
            xor  1
            call sbp_score
            ld   bc,3*256+31
            call sbu_putnum
            ld   hl,sbu_t_bag
            ld   bc,5*256+PANEL_COL
            call sbp_label
            ld   a,(SB_BAGN)
            ld   l,a
            ld   h,0
            ld   bc,5*256+31
            call sbu_putnum
            ld   a,(SB_OVER)        ; fin de la partida, o de quien es el turno
            or   a
            ld   hl,sbu_t_over
            jr   nz,sbp_t
            ld   a,(SB_TURN)
            ld   hl,sbu_t_yturn
            ld   b,a
            ld   a,(SU_ME)
            cp   b
            jr   z,sbp_t
            ld   hl,sbu_t_rturn
sbp_t:      ld   bc,7*256+PANEL_COL
            call sbp_label
            ld   a,(SU_LASTV)       ; la ultima jugada
            or   a
            ret  z
            ld   hl,sbu_t_last
            ld   bc,9*256+PANEL_COL
            call sbp_label
            ld   hl,SU_LTXT
            ld   bc,10*256+PANEL_COL
            call sbp_label
            ld   a,(SU_LWHO)
            ld   hl,sbu_t_you
            ld   b,a
            ld   a,(SU_ME)
            cp   b
            jr   z,sbp_w
            ld   hl,sbu_t_rival
sbp_w:      ld   bc,11*256+PANEL_COL
            call sbp_label
            ld   hl,(SU_LPTS)
            ld   bc,11*256+31
            jp   sbu_putnum

; A = jugador -> HL = sus puntos
sbp_score:  add  a,a
            ld   e,a
            ld   d,0
            ld   hl,SB_SCORE
            add  hl,de
            ld   e,(hl)
            inc  hl
            ld   d,(hl)
            ex   de,hl
            ret

; HL = texto, B = fila, C = columna: borra de C a 31 (el ancho del panel) y escribe (como mucho 14 letras)
sbp_label:  push hl
            push bc
            ld   a,32
            sub  c
            ld   e,a
            call sbu_clr
            pop  bc
            pop  hl
            jp   gprint_at

; B = fila, C = columna, E = cuantas celdas: las deja en blanco (sin tocar los atributos)
sbu_clr:    call gaddr
            ld   d,e
sc_c:       push hl
            ld   b,8
sc_l:       ld   (hl),0
            inc  h
            djnz sc_l
            pop  hl
            inc  hl
            dec  d
            jr   nz,sc_c
            ret

; HL = numero (16 bits, con signo), B = fila, C = la ultima columna: lo escribe alineado a la derecha (borra 8 celdas)
sbu_putnum: ld   a,b
            ld   (SU_R),a
            ld   a,c
            ld   (SU_K),a
            ld   a,h
            rlca
            and  1
            ld   (SU_S),a           ; 1 si es negativo
            jr   z,spn_p
            ld   a,h                ; HL = -HL
            cpl
            ld   h,a
            ld   a,l
            cpl
            ld   l,a
            inc  hl
spn_p:      ld   de,SU_NUM+7
            xor  a
            ld   (de),a             ; fin del texto
            ld   b,0                ; B = cuantos caracteres
spn_d:      call sbu_div10          ; HL = HL / 10, A = el resto
            add  a,'0'
            dec  de
            ld   (de),a
            inc  b
            ld   a,h
            or   l
            jr   nz,spn_d
            ld   a,(SU_S)
            or   a
            jr   z,spn_s
            dec  de
            ld   a,'-'
            ld   (de),a
            inc  b
spn_s:      ld   a,b
            ld   (SU_L),a
            ld   a,(SU_K)           ; borra 8 celdas que acaban en la ultima columna
            sub  7
            ld   c,a
            ld   a,(SU_R)
            ld   b,a
            ld   e,8
            call sbu_clr
            ld   a,(SU_L)           ; y escribe el numero
            ld   e,a
            ld   d,0
            ld   hl,SU_NUM+7
            or   a
            sbc  hl,de              ; HL = el principio del texto
            ld   a,(SU_K)
            sub  e
            inc  a
            ld   c,a
            ld   a,(SU_R)
            ld   b,a
            jp   gprint_at

; HL = HL / 10, A = el resto (conserva BC y DE)
sbu_div10:  push bc
            ld   b,16
            xor  a
sd_l:       add  hl,hl
            rla
            cp   10
            jr   c,sd_n
            sub  10
            inc  l
sd_n:       djnz sd_l
            pop  bc
            ret

; ---------------------------------------------------------------------
;  Las filas de texto
; ---------------------------------------------------------------------
sbu_status: ld   b,ROW_STATUS
            ld   a,AT_STATUS
            jr   sbu_row
sbu_msg:    ld   b,ROW_MSG
            ld   a,AT_MSG
            jr   sbu_row
sbu_help:   ld   b,ROW_HELP
            ld   a,AT_HELP
            jr   sbu_row
sbu_input:  ld   b,ROW_INPUT
            ld   a,ATTR_TEXT
sbu_row:    push hl
            push bc
            push af
            ld   c,0                ; borra la fila (el bitmap) y le pone su color
            ld   e,32
            call sbu_clr
            pop  af
            pop  bc
            push bc
            call gattr_row
            pop  bc
            pop  hl
            ld   c,0
            jp   gprint_at

; ---------------------------------------------------------------------
;  Datos
; ---------------------------------------------------------------------
sbu_t_title: defm "SCRABBLE",0
sbu_t_you:   defm "YOU",0
sbu_t_rival: defm "RIVAL",0
sbu_t_bag:   defm "BAG",0
sbu_t_yturn: defm "YOUR TURN",0
sbu_t_rturn: defm "RIVAL TURN",0
sbu_t_last:  defm "LAST MOVE",0
sbu_t_over:  defm "GAME OVER",0
sbu_t_dl:    defm "DL",0
sbu_t_tl:    defm "TL",0
sbu_t_dw:    defm "DW",0
sbu_t_tw:    defm "TW",0
sbu_pattr:   defb AT_NORM,AT_DL,AT_TL,AT_DW,AT_TW
sbu_tattr:   defb AT_TILE,AT_BLANK,AT_LAST,AT_LASTB
sbu_dbl:     defb 000h,003h,00Ch,00Fh,030h,033h,03Ch,03Fh,0C0h,0C3h,0CCh,0CFh,0F0h,0F3h,0FCh,0FFh
sbu_zero:    defs 8,0
sbu_star:    defb 010h,092h,054h,038h,0FEh,038h,054h,092h
; N con tilde, CH, LL, RR (en este orden: los codigos '[', '\', ']', '^')
sbu_cg:      defb 034h,058h,042h,062h,052h,04Ah,046h,042h
             defb 000h,06Ah,08Ah,08Ah,08Eh,08Ah,08Ah,06Ah
             defb 000h,088h,088h,088h,088h,088h,088h,0EEh
             defb 000h,0CCh,0AAh,0AAh,0CCh,0AAh,0AAh,0AAh
