; =====================================================================
;  SCRABBLE_IN.INC - teclear la jugada del Scrabble. Se guardan las teclas pulsadas y despues de cada cambio se
;  REINTERPRETAN enteras (asi borrar, la fusion de CH, LL y RR y los limites salen solos); una tecla que no cuadra en
;  esa posicion no entra. La jugada se teclea como en el ajedrez, con la palabra ENTERA tal como quedara:
;
;      H8 R QUIZ  ENTER      columna A-O, fila 1-15, R (derecha) o D (abajo), y la palabra con las letras que ya estan
;      (o con el cursor: las flechas 5 (izquierda), 6 (abajo), 7 (arriba) y 8 (derecha) mueven un cursor por el tablero, 0 elige la
;       casilla de salida, y despues 8 (derecha) o 6 (abajo) elige el sentido: el cursor y una flecha son SPRITES de hardware)
;      P  ENTER              pasar
;      X letras ENTER        cambiar esas fichas (1 a 7)
;
;  En la palabra: las letras A-Z; un punto antes de una letra fuerza el comodin para esa letra (sin el punto, el comodin
;  solo se usa si falta la letra); en castellano CH, LL y RR se juntan solos en una ficha al teclear C H, L L y R R, y
;  la tecla 1 es la N con tilde. Los espacios no cuentan. SHIFT+0 (8) borra la ultima tecla.
;
;  El que lo incluye define antes SI_BASE (SI_SIZE bytes) y tiene scrabble.inc y scrabble_ui.inc (y por tanto gfx.inc).
;
;  Rutinas (destruyen todos los registros):
;      sbi_clear    vacia lo tecleado
;      sbi_key      A = tecla (ASCII en mayusculas, 8 = borrar, 13 = ENTER)  ->  A = 0 editado o rechazada,
;                   1 = ENTER y la jugada esta completa, 2 = ENTER pero falta algo
;      sbi_build    pone la jugada en WB como la quiere sb_check (WB: x, y, sentido, n, fichas) o, si es un cambio, WB+3 = n y
;                   WB+4.. las fichas  ->  A = 1 jugada, 2 pasar, 3 cambiar (0 si no hay nada)
;      sbi_draw     dibuja la fila de entrada y la ayuda
;      sbi_errmsg   A = error de sb_check -> HL = el texto del aviso (con la palabra que falla, en 8 y 9); sbu_msg lo escribe
;      sbi_spr_init / sbi_spr_off / sbi_blink   los sprites del cursor y de la flecha: definirlos (al empezar la partida), apagarlos (al
;                   salir de la pantalla) y hacer parpadear el cursor (una vez por vuelta del bucle principal)
;  Variables: SI_MODE (0 nada, 1 jugada, 2 pasar, 3 cambiar), SI_X, SI_Y (de 0), SI_DIR, SI_N, SI_T (las fichas), SI_CX y SI_CY (el cursor),
;  SI_HIDE (el juego la pone a 1 para que no haya cursor: partida acabada)
; =====================================================================

SI_KN       equ SI_BASE+0       ; teclas guardadas
SI_BUF      equ SI_BASE+1       ; (40)
SI_MODE     equ SI_BASE+41
SI_X        equ SI_BASE+42
SI_Y        equ SI_BASE+43
SI_DIR      equ SI_BASE+44
SI_N        equ SI_BASE+45      ; fichas de la palabra
SI_T        equ SI_BASE+46      ; (16) las fichas (con el bit 7 = comodin forzado)
SI_STAGE    equ SI_BASE+62      ; 0 columna, 1 primera cifra de la fila, 2 segunda cifra o sentido, 3 sentido, 5 la palabra, 6 la letra del comodin, 9 nada mas
SI_I        equ SI_BASE+63
SI_TL       equ SI_BASE+64      ; largo del texto
SI_TXT      equ SI_BASE+65      ; (24) lo que va antes de las fichas: "H8 R " o "PASS" o "EXCHANGE: "
SI_MSG      equ SI_BASE+89      ; (34) el aviso de error
SI_CX       equ SI_BASE+124     ; el cursor: columna y fila (0-14)
SI_CY       equ SI_BASE+125
SI_PH       equ SI_BASE+126     ; contador del parpadeo
SI_BL       equ SI_BASE+127     ; 0 cursor negro, 1 blanco
SI_HIDE     equ SI_BASE+128     ; 1 = sin cursor
SI_SIZE     equ 132

; los sprites por hardware (Apendice H del manual): POKEs 2100-2128. El cursor es el 0, la flecha a la derecha el 1 y la de abajo el 2
SPR_SEL     equ 2100
SPR_EN      equ 2101
SPR_XL      equ 2102
SPR_XH      equ 2103
SPR_Y       equ 2104
SPR_COL     equ 2105            ; luego 2113 (pixeles) y 2121 (mascara)
SPR_CUR     equ 0
SPR_RT      equ 1
SPR_DN      equ 2

; ---------------------------------------------------------------------
;  Teclas
; ---------------------------------------------------------------------
sbi_clear:  xor  a
            ld   (SI_KN),a
            jp   sbi_parse

sbi_key:    cp   8
            jr   z,sk_del
            cp   13
            jr   z,sk_enter
            cp   ' '
            jr   z,sk_ign
            ld   c,a
            ld   a,(SI_KN)
            or   a
            jr   nz,cu_k2
            ld   a,c                ; con nada tecleado: 5 6 7 8 mueven el cursor y 0 elige la casilla
            cp   '0'
            jp   z,cu_pick
            cp   '5'
            jr   c,sk_norm
            cp   '9'
            jr   nc,sk_norm
            sub  '5'                ; 0 izquierda, 1 abajo, 2 arriba, 3 derecha
            jp   cu_move
cu_k2:      ld   a,(SI_STAGE)       ; esperando el sentido (etapa 2 o 3): 8 = R y 6 = D
            cp   2
            jr   z,cu_dir
            cp   3
            jr   nz,sk_norm
cu_dir:     ld   a,c
            cp   '8'
            jr   nz,cu_d6
            ld   c,'R'
            jr   sk_norm
cu_d6:      cp   '6'
            jr   nz,sk_norm
            ld   c,'D'
sk_norm:    ld   a,(SI_KN)
            cp   40
            jr   nc,sk_ign
            ld   e,a
            ld   d,0
            ld   hl,SI_BUF
            add  hl,de
            ld   (hl),c
            inc  a
            ld   (SI_KN),a
            call sbi_parse
            or   a
            jr   z,sk_ign           ; valida: se queda
            ld   hl,SI_KN           ; no cuadra: fuera
            dec  (hl)
            call sbi_parse
sk_ign:     xor  a
            ret
sk_del:     ld   a,(SI_KN)
            or   a
            jr   z,sk_ign
            dec  a
            ld   (SI_KN),a
            call sbi_parse
            xor  a
            ret
sk_enter:   call sbi_ready
            or   a
            ld   a,1
            ret  nz
            ld   a,2
            ret

; --- el cursor: 5 izquierda, 6 abajo, 7 arriba, 8 derecha (A = 0 a 3); 0 elige la casilla (teclea la columna y la fila por ti) ---
cu_move:    or   a
            jr   nz,cu_m1
            ld   a,(SI_CX)
            or   a
            jr   z,cu_md
            dec  a
            ld   (SI_CX),a
            jr   cu_md
cu_m1:      cp   1
            jr   nz,cu_m2
            ld   a,(SI_CY)
            cp   14
            jr   nc,cu_md
            inc  a
            ld   (SI_CY),a
            jr   cu_md
cu_m2:      cp   2
            jr   nz,cu_m3
            ld   a,(SI_CY)
            or   a
            jr   z,cu_md
            dec  a
            ld   (SI_CY),a
            jr   cu_md
cu_m3:      ld   a,(SI_CX)
            cp   14
            jr   nc,cu_md
            inc  a
            ld   (SI_CX),a
cu_md:      xor  a
            ret

cu_pick:    ld   a,(SI_CX)          ; la columna y la fila del cursor, como si se hubieran tecleado
            add  a,'A'
            call sbi_key
            ld   a,(SI_CY)
            inc  a
            cp   10
            jr   c,cu_p1
            sub  10                 ; fila 10 a 15: un 1 y la cifra de las unidades
            push af
            ld   a,'1'
            call sbi_key
            pop  af
            add  a,'0'
            call sbi_key
            jr   cu_pd
cu_p1:      add  a,'0'
            call sbi_key
cu_pd:      xor  a
            ret

; --- los sprites ---
; los define (apagados): A = el sprite, HL = 24 bytes (color, pixel y mascara de cada fila)
cu_def:     ld   (SPR_SEL),a
            xor  a
            ld   (SPR_EN),a
            ld   de,SPR_COL
            ld   bc,24
            ldir
            ret

sbi_spr_init: ld a,SPR_CUR
            ld   hl,cu_spr_ring
            call cu_def
            ld   a,SPR_RT
            ld   hl,cu_spr_right
            call cu_def
            ld   a,SPR_DN
            ld   hl,cu_spr_down
            call cu_def
            xor  a
            ld   (SI_BL),a
            ld   (SI_PH),a
            ld   a,7                ; el cursor empieza en el centro (H8)
            ld   (SI_CX),a
            ld   (SI_CY),a
            ret

sbi_spr_off: ld  b,3
            ld   c,0
cu_o1:      ld   a,c
            ld   (SPR_SEL),a
            xor  a
            ld   (SPR_EN),a
            inc  c
            djnz cu_o1
            ret

; A = sprite, B = columna y C = fila del tablero (0-14): lo pone encima de esa casilla y lo enciende
cu_place:   ld   (SPR_SEL),a
            ld   a,b
            add  a,BOARD_COL
            add  a,a
            add  a,a
            add  a,a
            add  a,32               ; las coordenadas de sprite llevan 32 pixeles de mas
            ld   (SPR_XL),a
            xor  a
            ld   (SPR_XH),a
            ld   a,c
            add  a,BOARD_ROW
            add  a,a
            add  a,a
            add  a,a
            add  a,32
            ld   (SPR_Y),a
            ld   a,1
            ld   (SPR_EN),a
            ret

cu_hide:    ld   (SPR_SEL),a
            xor  a
            ld   (SPR_EN),a
            ret

; pone el cursor y la flecha como toque segun lo tecleado (la llama sbi_draw)
sbi_spr:    ld   a,(SI_HIDE)
            or   a
            jr   nz,cu_off
            ld   a,(SI_MODE)
            cp   2
            jr   z,cu_off           ; pasar y cambiar: sin cursor
            cp   3
            jr   z,cu_off
            ld   a,(SI_STAGE)
            cp   2
            jr   c,cu_cur
            ld   a,(SI_X)           ; con la casilla elegida, el cursor es ella (y se recuerda)
            ld   (SI_CX),a
            ld   a,(SI_Y)
            ld   (SI_CY),a
cu_cur:     ld   a,(SI_CX)
            ld   b,a
            ld   a,(SI_CY)
            ld   c,a
            ld   a,SPR_CUR
            call cu_place
            ld   a,(SI_MODE)        ; la flecha, con el sentido ya elegido
            cp   1
            jr   nz,cu_noa
            ld   a,(SI_STAGE)
            cp   5
            jr   c,cu_noa
            ld   a,(SI_DIR)
            or   a
            jr   nz,cu_down
            ld   a,(SI_CX)          ; a la derecha: la casilla de al lado
            inc  a
            cp   15
            jr   nc,cu_noa
            ld   b,a
            ld   a,(SI_CY)
            ld   c,a
            ld   a,SPR_RT
            call cu_place
            ld   a,SPR_DN
            jp   cu_hide
cu_down:    ld   a,(SI_CY)          ; hacia abajo: la de debajo
            inc  a
            cp   15
            jr   nc,cu_noa
            ld   c,a
            ld   a,(SI_CX)
            ld   b,a
            ld   a,SPR_DN
            call cu_place
            ld   a,SPR_RT
            jp   cu_hide
cu_noa:     ld   a,SPR_RT
            call cu_hide
            ld   a,SPR_DN
            jp   cu_hide
cu_off:     jp   sbi_spr_off

; una vez por vuelta del bucle principal: cada 16 vueltas el cursor aparece o desaparece. Para desaparecer, la MASCARA del sprite pasa a 0 (todo
; transparente): no borra ningun punto de las letras de debajo. El color y los pixeles no cambian nunca (negro)
sbi_blink:  ld   a,(SI_PH)
            inc  a
            ld   (SI_PH),a
            and  0Fh
            ret  nz
            ld   a,(SI_BL)
            xor  1
            ld   (SI_BL),a
            ld   a,SPR_CUR
            ld   (SPR_SEL),a
            ld   hl,SPR_COL+16      ; la mascara de las 8 filas
            ld   a,(SI_BL)
            or   a
            jr   nz,cu_bz
            ex   de,hl              ; visible: la mascara del marco
            ld   hl,cu_spr_ring+16
            ld   bc,8
            ldir
            ret
cu_bz:      ld   b,8                ; oculto: mascara a 0
            xor  a
cu_bl:      ld   (hl),a
            inc  hl
            djnz cu_bl
            ret

; los tres sprites: color de cada fila (negro; la flecha, negra con el borde blanco), pixeles y mascara
cu_spr_ring: defb 000h,000h,000h,000h,000h,000h,000h,000h
            defb 0FFh,081h,081h,081h,081h,081h,081h,0FFh
            defb 0FFh,081h,081h,081h,081h,081h,081h,0FFh
cu_spr_right: defb 00Fh,00Fh,00Fh,00Fh,00Fh,00Fh,00Fh,00Fh
            defb 010h,018h,01Ch,0FEh,0FEh,01Ch,018h,010h
            defb 03Ch,03Eh,0FFh,0FFh,0FFh,0FFh,03Eh,03Ch
cu_spr_down: defb 00Fh,00Fh,00Fh,00Fh,00Fh,00Fh,00Fh,00Fh
            defb 018h,018h,018h,018h,07Eh,03Ch,018h,000h
            defb 03Ch,03Ch,03Ch,0FFh,0FFh,0FFh,07Eh,03Ch

; A = 1 si lo tecleado se puede jugar
sbi_ready:  ld   a,(SI_MODE)
            cp   2
            jr   z,rd_yes
            cp   3
            jr   z,rd_x
            cp   1
            jr   nz,rd_no
            ld   a,(SI_STAGE)       ; jugada: la palabra, con 2 fichas o mas
            cp   5
            jr   nz,rd_no
            ld   a,(SI_N)
            cp   2
            jr   c,rd_no
            jr   rd_yes
rd_x:       ld   a,(SI_STAGE)       ; cambio: 1 ficha o mas
            cp   5
            jr   nz,rd_no
            ld   a,(SI_N)
            or   a
            jr   z,rd_no
rd_yes:     ld   a,1
            ret
rd_no:      xor  a
            ret

; ---------------------------------------------------------------------
;  Interpretar lo tecleado -> A = 0 si vale (aunque este a medias), distinto de 0 si no
; ---------------------------------------------------------------------
sbi_parse:  xor  a
            ld   (SI_MODE),a
            ld   (SI_N),a
            ld   (SI_STAGE),a
            ld   (SI_X),a
            ld   (SI_Y),a
            ld   (SI_DIR),a
            ld   (SI_TL),a
            ld   (SI_TXT),a
            ld   (SI_I),a
            ld   a,(SI_KN)
            or   a
            ret  z
            ld   b,a
            ld   hl,SI_BUF
sp_lp:      ld   a,(hl)
            push hl
            push bc
            call sbi_step
            pop  bc
            pop  hl
            ret  nz
            inc  hl
            djnz sp_lp
            xor  a
            ret

; una tecla (A) segun la etapa -> Z si vale, NZ si no
sbi_step:   ld   c,a
            ld   a,(SI_STAGE)
            or   a
            jr   z,st0
            cp   1
            jp   z,st1
            cp   2
            jp   z,st2
            cp   3
            jp   z,st3
            cp   5
            jp   z,st5
            cp   6
            jp   z,st6
st_bad:     ld   a,1
            or   a
            ret
st_ok:      xor  a
            ret

st0:        ld   a,c                ; la primera tecla: columna, P o X
            cp   'P'
            jr   nz,s0a
            ld   a,2
            ld   (SI_MODE),a
            ld   a,9
            ld   (SI_STAGE),a
            ld   hl,sbi_t_pass
            call sbi_txt
            jr   st_ok
s0a:        cp   'X'
            jr   nz,s0b
            ld   a,3
            ld   (SI_MODE),a
            ld   a,5
            ld   (SI_STAGE),a
            ld   hl,sbi_t_exch
            call sbi_txt
            jr   st_ok
s0b:        cp   'A'
            jp   c,st_bad
            cp   'P'
            jp   nc,st_bad
            sub  'A'
            ld   (SI_X),a
            ld   a,1
            ld   (SI_MODE),a
            ld   (SI_STAGE),a
            ld   a,c
            call sbi_putc
            jr   st_ok

st1:        ld   a,c                ; la primera cifra de la fila (1-9)
            cp   '1'
            jp   c,st_bad
            cp   ':'
            jp   nc,st_bad
            call sbi_putc
            ld   a,c
            sub  '1'
            ld   (SI_Y),a
            ld   b,3
            or   a
            jr   nz,s1s
            ld   b,2                ; un 1 puede ser 10-15
s1s:        ld   a,b
            ld   (SI_STAGE),a
            jr   st_ok

st2:        ld   a,c                ; despues de un 1: 0-5 (fila 10-15) o ya el sentido
            cp   '0'
            jr   c,st3
            cp   '6'
            jr   nc,st3
            call sbi_putc
            ld   a,c
            sub  '0'-9
            ld   (SI_Y),a
            ld   a,3
            ld   (SI_STAGE),a
            jp   st_ok

st3:        ld   a,c                ; el sentido: R o D
            cp   'R'
            jr   z,s3r
            cp   'D'
            jp   nz,st_bad
            ld   a,1
            jr   s3g
s3r:        xor  a
s3g:        ld   (SI_DIR),a
            ld   a,' '
            call sbi_putc
            ld   a,c
            call sbi_putc
            ld   a,' '
            call sbi_putc
            ld   a,5
            ld   (SI_STAGE),a
            jp   st_ok

st5:        ld   a,c                ; la palabra
            cp   '.'
            jr   nz,s5a
            ld   a,6
            ld   (SI_STAGE),a
            jp   st_ok
s5a:        cp   '1'
            jr   nz,s5b
            ld   a,(SB_LANG)        ; 1 = la N con tilde (solo en castellano)
            or   a
            jp   z,st_bad
            ld   c,'['
            jr   s5t
s5b:        cp   'A'
            jp   c,st_bad
            cp   '['
            jp   nc,st_bad
s5t:        ld   b,0
            jr   sbi_addtile

st6:        ld   a,c                ; la letra de un comodin
            cp   'A'
            jp   c,st_bad
            cp   '['
            jp   nc,st_bad
            ld   b,80h
            call sbi_addtile
            ret  nz
            ld   a,5
            ld   (SI_STAGE),a
            xor  a
            ret

; la ficha C (con B = 80h si es un comodin forzado) al final de la palabra -> Z si cabe
sbi_addtile: ld  a,b
            or   a
            jr   nz,at_new
            ld   a,(SB_LANG)        ; castellano: C H, L L y R R son una sola ficha
            or   a
            jr   z,at_new
            ld   a,(SI_N)
            or   a
            jr   z,at_new
            dec  a
            ld   e,a
            ld   d,0
            ld   hl,SI_T
            add  hl,de
            ld   a,(hl)             ; la ficha anterior
            cp   'C'
            jr   nz,at_l
            ld   a,c
            cp   'H'
            jr   nz,at_new
            ld   (hl),5Ch
            jp   st_ok
at_l:       cp   'L'
            jr   nz,at_r
            ld   a,c
            cp   'L'
            jr   nz,at_new
            ld   (hl),']'
            jp   st_ok
at_r:       cp   'R'
            jr   nz,at_new
            ld   a,c
            cp   'R'
            jr   nz,at_new
            ld   (hl),'^'
            jp   st_ok
at_new:     ld   a,(SI_MODE)
            cp   3
            jr   nz,at_mv
            ld   a,(SI_N)           ; cambio: hasta 7
            cp   7
            jp   nc,st_bad
            jr   at_put
at_mv:      ld   a,(SI_DIR)         ; jugada: la ficha tiene que caber en el tablero
            or   a
            ld   a,(SI_X)
            jr   z,at_lim
            ld   a,(SI_Y)
at_lim:     ld   d,a
            ld   a,(SI_N)
            add  a,d
            cp   15
            jp   nc,st_bad
at_put:     ld   a,(SI_N)
            ld   e,a
            ld   d,0
            ld   hl,SI_T
            add  hl,de
            ld   a,c
            or   b
            ld   (hl),a
            ld   a,(SI_N)
            inc  a
            ld   (SI_N),a
            jp   st_ok

; HL = texto (0): lo pone en SI_TXT
sbi_txt:    ld   de,SI_TXT
            ld   b,0
tx_l:       ld   a,(hl)
            ld   (de),a
            or   a
            jr   z,tx_d
            inc  hl
            inc  de
            inc  b
            jr   tx_l
tx_d:       ld   a,b
            ld   (SI_TL),a
            ret

; A = caracter: lo anade a SI_TXT
sbi_putc:   ld   b,a
            ld   a,(SI_TL)
            ld   e,a
            ld   d,0
            ld   hl,SI_TXT
            add  hl,de
            ld   (hl),b
            inc  hl
            ld   (hl),0
            inc  a
            ld   (SI_TL),a
            ret

; ---------------------------------------------------------------------
;  La jugada para el motor
; ---------------------------------------------------------------------
sbi_build:  ld   a,(SI_MODE)
            cp   1
            jr   z,bd_mv
            cp   3
            jr   z,bd_x
            ret
bd_mv:      ld   a,(SI_X)
            ld   (WB),a
            ld   a,(SI_Y)
            ld   (WB+1),a
            ld   a,(SI_DIR)
            ld   (WB+2),a
            ld   a,(SI_N)
            ld   (WB+3),a
            ld   c,a
            ld   b,0
            ld   hl,SI_T
            ld   de,WB+4
            or   a
            jr   z,bd_r
            ldir
bd_r:       ld   a,1
            ret
bd_x:       ld   a,(SI_N)
            ld   (WB+3),a
            ld   b,a
            ld   hl,SI_T
            ld   de,WB+4
bx_l:       ld   a,(hl)
            inc  hl
            bit  7,a
            jr   z,bx_p
            ld   a,'?'              ; un comodin en el atril es '?'
bx_p:       ld   (de),a
            inc  de
            djnz bx_l
            ld   a,3
            ret

; ---------------------------------------------------------------------
;  Dibujo: la fila de entrada (lo tecleado y las fichas) y la ayuda
; ---------------------------------------------------------------------
sbi_draw:   ld   b,ROW_INPUT
            ld   c,0
            ld   e,32
            call sbu_clr
            ld   a,ATTR_TEXT
            ld   b,ROW_INPUT
            call gattr_row
            ld   hl,SI_TXT
            ld   b,ROW_INPUT
            ld   c,0
            call gprint_at
            xor  a
            ld   (SI_I),a
dr_l:       ld   a,(SI_I)
            ld   hl,SI_N
            cp   (hl)
            jr   nc,dr_cur
            ld   hl,SI_TL
            add  a,(hl)
            cp   32
            jr   nc,dr_cur
            ld   (SU_X),a           ; la columna de esta ficha
            ld   a,(SI_I)
            ld   e,a
            ld   d,0
            ld   hl,SI_T
            add  hl,de
            ld   a,(hl)
            ld   (SU_C),a
            rlca
            ld   a,AT_TILE
            jr   nc,dr_a
            ld   a,AT_BLANK
dr_a:       ld   (SQ_ATTR),a
            ld   a,(SU_C)
            call sbu_glyph
            ld   a,(SU_X)
            ld   c,a
            ld   b,ROW_INPUT
            call ds_cell
            ld   hl,SI_I
            inc  (hl)
            jr   dr_l
dr_cur:     ld   a,(SI_STAGE)       ; el cursor, un bloque (un punto si espera la letra de un comodin)
            cp   9
            jr   z,dr_help
            ld   a,(SI_N)
            ld   hl,SI_TL
            add  a,(hl)
            cp   32
            jr   nc,dr_help
            ld   c,a
            ld   b,ROW_INPUT
            ld   a,ATTR_TEXT
            ld   (SQ_ATTR),a
            ld   hl,sbi_block
            ld   a,(SI_STAGE)
            cp   6
            jr   nz,dr_c
            ld   hl,ROMFONT+27*8
dr_c:       call ds_cell
dr_help:    ld   a,(SI_STAGE)       ; la ayuda segun la etapa
            ld   hl,sbi_h0
            or   a
            jr   z,dr_h
            ld   hl,sbi_h1
            cp   1
            jr   z,dr_h
            ld   hl,sbi_h2
            cp   2
            jr   z,dr_h
            ld   hl,sbi_h3
            cp   3
            jr   z,dr_h
            ld   hl,sbi_h6
            cp   6
            jr   z,dr_h
            ld   hl,sbi_h9
            cp   9
            jr   z,dr_h
            ld   hl,sbi_h5x         ; la palabra: un cambio, o una jugada (con la N con tilde en castellano)
            ld   a,(SI_MODE)
            cp   3
            jr   z,dr_h
            ld   hl,sbi_h5
            ld   a,(SB_LANG)
            or   a
            jr   z,dr_h
            ld   hl,sbi_h5s
dr_h:       call sbu_help
            jp   sbi_spr

; ---------------------------------------------------------------------
;  Los avisos de error de sb_check
; ---------------------------------------------------------------------
; A = error -> HL = el aviso (se corta a 32 letras)
sbi_errmsg: ld   c,a
            ld   de,SI_MSG
            cp   8
            jr   z,em_w
            cp   9
            jr   nz,em_t
em_w:       ld   hl,DC_W            ; la palabra que falla
em_wl:      ld   a,(hl)
            or   a
            jr   z,em_wd
            inc  hl
            call sbu_t2a
            jr   em_wl
em_wd:      ld   a,c
            cp   9
            ld   hl,sbi_e8
            jr   nz,em_cp
            ld   hl,sbi_e9
            jr   em_cp
em_t:       ld   a,(SI_MODE)        ; un cambio con menos de 7 fichas en la bolsa
            cp   3
            jr   nz,em_n
            ld   a,c
            cp   4
            ld   hl,sbi_e4x
            jr   z,em_cp
em_n:       ld   hl,sbi_etab        ; el aviso numero C de la tabla
            ld   b,c
            dec  b
            jr   z,em_cp
em_s:       ld   a,(hl)
            inc  hl
            or   a
            jr   nz,em_s
            djnz em_s
em_cp:      ld   a,(hl)
            ld   (de),a
            or   a
            jr   z,em_ok
            inc  hl
            inc  de
            jr   em_cp
em_ok:      xor  a
            ld   (SI_MSG+32),a      ; la fila tiene 32 celdas
            ld   hl,SI_MSG
            ret

sbi_t_pass: defm "PASS",0
sbi_t_exch: defm "SWAP: ",0
sbi_block:  defs 8,0FFh
sbi_h0:     defm "5-8=MOVE 0=OK P=PASS X=SWAP",0
sbi_h1:     defm "ROW NUMBER (1-15)",0
sbi_h2:     defm "8=RIGHT 6=DOWN  (0-5 = ROW 10+)",0
sbi_h3:     defm "8=RIGHT 6=DOWN  (OR R / D)",0
sbi_h5:     defm "LETTERS  . = BLANK  ENTER = PLAY",0
sbi_h5s:    defm "1=ENYE  .=BLANK  ENTER = PLAY",0
sbi_h5x:    defm "TILES TO SWAP  . = BLANK  ENTER",0
sbi_h6:     defm "WHICH LETTER IS THE BLANK?",0
sbi_h9:     defm "ENTER = PASS   DEL = CANCEL",0
sbi_etab:   defm "BAD POSITION OR LETTER",0
            defm "YOU DO NOT HAVE THOSE TILES",0
            defm "LETTER DOES NOT MATCH THE BOARD",0
            defm "PLACE AT LEAST ONE TILE",0
            defm "THE WORD WOULD BE LONGER",0
            defm "FIRST WORD MUST COVER THE CENTER",0
            defm "THE WORD MUST TOUCH THE TILES",0
            defm " IS NOT A WORD",0
            defm " (CROSSING) IS NOT A WORD",0
            defm "A WORD HAS 2 TO 15 TILES",0
            defm "THE GAME IS OVER",0
sbi_e8:     defm " IS NOT A WORD",0
sbi_e9:     defm " (CROSSING) IS NOT A WORD",0
sbi_e4x:    defm "THE BAG HAS LESS THAN 7 TILES",0
