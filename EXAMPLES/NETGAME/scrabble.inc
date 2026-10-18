; =====================================================================
;  SCRABBLE.INC - las reglas del Scrabble para dos jugadores: la bolsa de fichas barajada con una semilla, los
;  atriles, la validacion de una jugada (con el diccionario de la SD, dict.inc), la puntuacion y el fin de la
;  partida. No sabe nada de la pantalla ni de la red. Diseno: claude/planning/scrabble.md.
;
;  TODO el estado de la partida es una funcion de la semilla y de las jugadas: los dos ordenadores barajan la
;  misma bolsa con la misma semilla y roban en el mismo orden, asi que cada uno sabe tambien el atril del otro
;  (el programa no lo muestra). Una jugada son pocos bytes.
;
;  El que lo incluye define antes:
;      SB_BASE   equ ...   ; RAM de trabajo (SB_LEN bytes)
;      DC_BASE   equ ...   ; RAM de dict.inc, y dict.inc incluido antes que este fichero
;
;  Las fichas son BYTES: A-Z son ellas mismas; en castellano, N con tilde = '[', CH = '\', LL = ']', RR = '^' (son
;  los bytes que usa el diccionario). El comodin en el atril es '?'; en el tablero, el comodin ya jugado es la letra
;  con el bit 7 puesto. 0 = vacio.
;
;  Una jugada se pone en WB: [x][y][sentido][n][n codigos]: la casilla de salida (0-14), el sentido (0 derecha,
;  1 abajo) y la palabra ENTERA como quedara en el tablero, con las letras que ya estaban. Un codigo con el bit 7
;  puesto fuerza el comodin; si la letra no esta en el atril y hay un comodin, se usa el comodin.
;
;  Rutinas (destruyen todos los registros):
;      sb_init       SB_SEED (4 bytes, no todo a cero) y SB_LANG (0 ingles, 1 castellano): partida nueva
;      sb_open_dict  abre el diccionario del idioma (DICTEN.DIC / DICTES.DIC): A = 0 bien o el error de dc_open
;      sb_check      comprueba la jugada de WB para el jugador SB_TURN -> A = 0 es legal (SB_TOT = sus puntos,
;                    el tablero no cambia), o el error: 1 fuera del tablero o mal formada, 2 una ficha que no
;                    esta en el atril, 3 no coincide con una letra del tablero, 4 no pone ninguna ficha,
;                    5 la palabra seria mas larga (hay letras pegadas), 6 la primera jugada ha de cubrir la
;                    casilla central, 7 no toca ninguna ficha, 8 la palabra no esta en el diccionario,
;                    9 una palabra cruzada no esta, 10 longitud mala (de 2 a 15 fichas), 11 la partida ha
;                    acabado. Con 8 o 9, DC_W tiene la palabra que falla.
;      sb_apply      hace la jugada comprobada (sb_check devolvio 0): pone las fichas, puntua, roba y pasa el turno
;      sb_pass       el jugador SB_TURN pasa
;      sb_xcheck     WB+3 = n y WB+4.. = las fichas que cambia (1 a 7): comprueba el cambio sin hacerlo -> A = 0 es legal, 2 no las
;                    tiene, 4 menos de 7 fichas en la bolsa, 10 n mal, 11 la partida ha acabado
;      sb_exchange   lo mismo y, si es legal, lo hace (se roba antes de devolver)
;      sb_rackval    HL = un atril -> HL = su valor
;  Variables: SB_BOARD (225), SB_RACK (2 x 7), SB_SCORE (2 x 16 bits con signo), SB_TURN (0 o 1), SB_OVER (0 sigue,
;  1 acabada), SB_BAGN (fichas en la bolsa)
; =====================================================================

SB_BOARD    equ SB_BASE+0       ; (225) 15 x 15, fila a fila
SB_BAG      equ SB_BASE+225     ; (100) la bolsa, circular
SB_RACK     equ SB_BASE+325     ; (14) el atril del jugador 0 y el del 1, de 7
SB_SCORE    equ SB_BASE+339     ; (4) los puntos de cada uno (16 bits, little-endian, con signo)
SB_BAGH     equ SB_BASE+343     ; donde esta la siguiente ficha de la bolsa
SB_BAGN     equ SB_BASE+344     ; cuantas hay
SB_TURN     equ SB_BASE+345     ; a quien le toca (0 o 1)
SB_PASS     equ SB_BASE+346     ; pases seguidos
SB_OVER     equ SB_BASE+347     ; 1 si la partida ha acabado
SB_LANG     equ SB_BASE+348     ; 0 ingles, 1 castellano
SB_SEED     equ SB_BASE+349     ; (4) la semilla (el generador la va cambiando)
SB_BCNT     equ SB_BASE+353     ; fichas en el tablero (0 = falta la primera jugada)
SB_GNUM     equ SB_BASE+354     ; numero de partida (el idioma y la semilla de cada una salen del codigo de sala y de este numero)
SB_STATE_LEN equ 355            ; lo anterior es el estado de la partida, seguido en memoria
; --- trabajo (no es estado)
SB_VALP     equ SB_BASE+360     ; (2) la tabla de valores del idioma
SB_CNTP     equ SB_BASE+362     ; (2) la tabla de reparto del idioma
WB          equ SB_BASE+364     ; (19) la jugada: x, y, sentido, n, los codigos
NF          equ SB_BASE+384     ; (16) por cada posicion de la palabra: la ficha nueva que se pone (0 = ya estaba)
TR          equ SB_BASE+400     ; (8) copia del atril para comprobar
SB_NNEW     equ SB_BASE+408     ; fichas nuevas
SB_TOT      equ SB_BASE+409     ; (2) los puntos de la jugada
SB_CONN     equ SB_BASE+411     ; 1 si toca algo
SB_I        equ SB_BASE+412
SB_CX       equ SB_BASE+413
SB_CY       equ SB_BASE+414
SB_DX       equ SB_BASE+415     ; el paso de la palabra
SB_DY       equ SB_BASE+416
SB_CODE     equ SB_BASE+417
SB_CELL     equ SB_BASE+418
SB_RX       equ SB_BASE+419     ; wr_run: una casilla de la palabra
SB_RY       equ SB_BASE+420
SB_RDX      equ SB_BASE+421     ; y su paso
SB_RDY      equ SB_BASE+422
SB_WLEN     equ SB_BASE+423     ; longitud de la palabra que da wr_run
SB_WSC      equ SB_BASE+424     ; (2) sus puntos
SB_SUM      equ SB_BASE+426     ; (2) suma de valores
SB_WM       equ SB_BASE+428     ; multiplicador de palabra
SB_V        equ SB_BASE+429     ; valor de la ficha
SB_RT       equ SB_BASE+430     ; (4) copia de la semilla para el generador
SB_SC0      equ SB_BASE+434     ; (2) valor del atril 0 al acabar
SB_SC1      equ SB_BASE+436     ; (2) valor del atril 1
SB_LEN      equ 440

; ---------------------------------------------------------------------
;  Partida nueva
; ---------------------------------------------------------------------
sb_init:    ld   hl,SB_BOARD        ; tablero vacio
            ld   de,SB_BOARD+1
            ld   bc,224
            ld   (hl),0
            ldir
            ld   hl,SB_RACK
            ld   de,SB_RACK+1
            ld   bc,13
            ld   (hl),0
            ldir
            ld   hl,0
            ld   (SB_SCORE),hl
            ld   (SB_SCORE+2),hl
            xor  a
            ld   (SB_TURN),a
            ld   (SB_PASS),a
            ld   (SB_OVER),a
            ld   (SB_BCNT),a
            ld   (SB_BAGH),a
            ld   hl,SB_SEED         ; la semilla no puede ser 0
            ld   a,(hl)
            inc  hl
            or   (hl)
            inc  hl
            or   (hl)
            inc  hl
            or   (hl)
            jr   nz,si_s
            ld   a,1
            ld   (SB_SEED),a
si_s:       call sb_setlang
            ; la bolsa en orden: para cada ficha, tantas como diga la tabla de reparto, y los 2 comodines
            ld   hl,SB_BAG
            ld   de,(SB_CNTP)
            ld   c,0                ; indice de la ficha (0 = A)
si_t:       ld   a,(de)
            or   a
            jr   z,si_nx
            ld   b,a
            ld   a,c
            add  a,'A'
si_f:       ld   (hl),a
            inc  hl
            djnz si_f
si_nx:      inc  de
            inc  c
            ld   a,c
            cp   30
            jr   nz,si_t
            ld   (hl),'?'
            inc  hl
            ld   (hl),'?'
            ld   a,100
            ld   (SB_BAGN),a
            ; Fisher-Yates: para i = 99 a 1, j = (rnd * (i + 1)) >> 8, y se intercambian
            ld   b,99
si_sh:      push bc
            call sb_rnd
            pop  bc
            push bc
            ld   a,(SB_SEED+1)      ; un byte de la semilla
            ld   c,a
            ld   a,b
            inc  a
            ld   b,a                ; B = i + 1
            ld   a,c
            call sb_mulhi           ; A = j
            pop  bc                 ; B = i
            ld   e,a
            ld   d,0
            ld   hl,SB_BAG
            add  hl,de
            push hl                 ; &bag[j]
            ld   e,b
            ld   d,0
            ld   hl,SB_BAG
            add  hl,de              ; &bag[i]
            pop  de
            ld   a,(de)
            ld   c,(hl)
            ld   (hl),a
            ld   a,c
            ld   (de),a
            djnz si_sh
            xor  a
            ld   (SB_TURN),a        ; reparte: primero el 0 y luego el 1
            call sb_refill
            ld   a,1
            ld   (SB_TURN),a
            call sb_refill
            xor  a
            ld   (SB_TURN),a
            ret

; abre el diccionario del idioma -> A = 0 bien o el error de dc_open (1 no se abre, 2 no es un diccionario, 3 vacio)
sb_open_dict: ld a,(SB_LANG)
            or   a
            ld   hl,sb_dn_en
            jr   z,sod_1
            ld   hl,sb_dn_es
sod_1:      ld   b,10
            jp   dc_open
sb_dn_en:   defm "DICTEN.DIC"
sb_dn_es:   defm "DICTES.DIC"

; apunta las tablas de valores y de reparto al idioma
sb_setlang: ld   a,(SB_LANG)
            or   a
            ld   hl,sb_val_en
            ld   de,sb_cnt_en
            jr   z,sl_s
            ld   hl,sb_val_es
            ld   de,sb_cnt_es
sl_s:       ld   (SB_VALP),hl
            ld   (SB_CNTP),de
            ret

; el generador (xorshift de 32 bits: x ^= x << 13; x ^= x >> 17; x ^= x << 5)
sb_rnd:     ld   b,13
            call sr_shl
            call sr_xor
            ld   b,17
            call sr_shr
            call sr_xor
            ld   b,5
            call sr_shl
            jp   sr_xor
sr_shl:     ld   a,b
            ld   hl,SB_SEED
            ld   de,SB_RT
            ld   bc,4
            ldir
            ld   b,a
sr_shl1:    ld   hl,SB_RT
            sla  (hl)
            inc  hl
            rl   (hl)
            inc  hl
            rl   (hl)
            inc  hl
            rl   (hl)
            djnz sr_shl1
            ret
sr_shr:     ld   a,b
            ld   hl,SB_SEED
            ld   de,SB_RT
            ld   bc,4
            ldir
            ld   b,a
sr_shr1:    ld   hl,SB_RT+3
            srl  (hl)
            dec  hl
            rr   (hl)
            dec  hl
            rr   (hl)
            dec  hl
            rr   (hl)
            djnz sr_shr1
            ret
sr_xor:     ld   hl,SB_RT
            ld   de,SB_SEED
            ld   b,4
sr_xl:      ld   a,(de)
            xor  (hl)
            ld   (de),a
            inc  hl
            inc  de
            djnz sr_xl
            ret

; A * B >> 8 -> A
sb_mulhi:   ld   c,a
            ld   hl,0
            ld   e,b
            ld   d,0
            ld   b,8
smh1:       add  hl,hl
            rlc  c
            jr   nc,smh2
            add  hl,de
smh2:       djnz smh1
            ld   a,h
            ret

; ---------------------------------------------------------------------
;  La bolsa y los atriles
; ---------------------------------------------------------------------
; saca la siguiente ficha de la bolsa -> A (la bolsa no esta vacia)
sb_draw:    ld   a,(SB_BAGH)
            ld   e,a
            ld   d,0
            ld   hl,SB_BAG
            add  hl,de
            ld   b,(hl)
            inc  e
            ld   a,e
            cp   100
            jr   nz,sd_h
            xor  a
sd_h:       ld   (SB_BAGH),a
            ld   hl,SB_BAGN
            dec  (hl)
            ld   a,b
            ret

; mete la ficha A al final de la bolsa
sb_bagput:  ld   b,a
            ld   a,(SB_BAGH)
            ld   hl,SB_BAGN
            add  a,(hl)
            cp   100
            jr   c,sbp_1
            sub  100
sbp_1:      ld   e,a
            ld   d,0
            ld   hl,SB_BAG
            add  hl,de
            ld   (hl),b
            ld   hl,SB_BAGN
            inc  (hl)
            ret

; HL = el atril del jugador SB_TURN
sb_rackbase: ld  a,(SB_TURN)
            ld   e,a
            add  a,a
            add  a,a
            add  a,a
            sub  e                  ; 7 * jugador
            ld   e,a
            ld   d,0
            ld   hl,SB_RACK
            add  hl,de
            ret

; llena los huecos del atril de SB_TURN con fichas de la bolsa
sb_refill:  call sb_rackbase
            ld   b,7
srf_l:      ld   a,(hl)
            or   a
            jr   nz,srf_n
            ld   a,(SB_BAGN)
            or   a
            ret  z
            push hl
            push bc
            call sb_draw
            pop  bc
            pop  hl
            ld   (hl),a
srf_n:      inc  hl
            djnz srf_l
            ret

; quita la ficha A del atril HL (7 casillas): Z si la ha quitado, NZ si no esta
sb_take:    ld   b,7
stk_l:      cp   (hl)
            jr   z,stk_f
            inc  hl
            djnz stk_l
            or   1
            ret
stk_f:      ld   (hl),0
            xor  a
            ret

; el valor de la ficha A (con o sin bit 7) -> A
sb_tileval: bit  7,a
            jr   z,stv_1
            xor  a
            ret
stv_1:      cp   '?'
            jr   nz,stv_2
            xor  a
            ret
stv_2:      sub  'A'
            ld   e,a
            ld   d,0
            ld   hl,(SB_VALP)
            add  hl,de
            ld   a,(hl)
            ret

; HL = un atril (7 casillas) -> HL = su valor
sb_rackval: ld   b,7
            ld   de,0
srv_l:      ld   a,(hl)
            or   a
            jr   z,srv_n
            push hl
            push de
            push bc
            call sb_tileval
            pop  bc
            pop  de
            pop  hl
            add  a,e
            ld   e,a
            jr   nc,srv_n
            inc  d
srv_n:      inc  hl
            djnz srv_l
            ex   de,hl
            ret

; B = x, C = y -> HL = la casilla del tablero (destruye DE)
sb_addr:    ld   l,c
            ld   h,0
            ld   d,h
            ld   e,l
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl
            or   a
            sbc  hl,de              ; 15 * y
            ld   e,b
            ld   d,0
            add  hl,de
            ld   de,SB_BOARD
            add  hl,de
            ret

; B = x, C = y -> A = casilla especial: 0 ninguna, 1 letra doble, 2 letra triple, 3 palabra doble, 4 palabra triple
sb_prem:    ld   a,b
            cp   8
            jr   c,spm_1
            ld   a,14
            sub  b
spm_1:      ld   e,a
            ld   a,c
            cp   8
            jr   c,spm_2
            ld   a,14
            sub  c
spm_2:      add  a,a
            add  a,a
            add  a,a
            add  a,e
            ld   e,a
            ld   d,0
            ld   hl,sb_premt
            add  hl,de
            ld   a,(hl)
            ret

; un cuadrante del tablero (8 x 8; el resto es simetrico)
sb_premt:   defb 4,0,0,1,0,0,0,4
            defb 0,3,0,0,0,2,0,0
            defb 0,0,3,0,0,0,1,0
            defb 1,0,0,3,0,0,0,1
            defb 0,0,0,0,3,0,0,0
            defb 0,2,0,0,0,2,0,0
            defb 0,0,1,0,0,0,1,0
            defb 4,0,0,1,0,0,0,3

; ---------------------------------------------------------------------
;  Comprobar una jugada
; ---------------------------------------------------------------------
sb_check:   ld   a,(SB_OVER)
            or   a
            jr   z,sc_go
            ld   a,11
            ret
sc_go:      xor  a
            ld   (SB_NNEW),a
            ld   (SB_CONN),a
            ld   a,(WB+3)
            cp   2
            jp   c,sc_len
            cp   16
            jp   nc,sc_len
            ld   a,(WB+2)
            cp   2
            jp   nc,sc_bad
            ld   a,(WB)
            cp   15
            jp   nc,sc_bad
            ld   a,(WB+1)
            cp   15
            jp   nc,sc_bad
            ld   a,(WB+2)           ; la ultima casilla tiene que estar dentro
            or   a
            ld   a,(WB)
            jr   z,sc_e
            ld   a,(WB+1)
sc_e:       ld   b,a
            ld   a,(WB+3)
            add  a,b
            cp   16
            jp   nc,sc_bad
            ; el paso
            ld   a,(WB+2)
            ld   (SB_DY),a
            xor  1
            ld   (SB_DX),a
            ; NF a cero, y una copia del atril para ir quitando fichas
            ld   hl,NF
            ld   de,NF+1
            ld   bc,15
            ld   (hl),0
            ldir
            call sb_rackbase
            ld   de,TR
            ld   bc,7
            ldir
            ld   a,(WB)
            ld   (SB_CX),a
            ld   a,(WB+1)
            ld   (SB_CY),a
            xor  a
            ld   (SB_I),a
sc_lp_s:      ld   a,(SB_CX)
            ld   b,a
            ld   a,(SB_CY)
            ld   c,a
            call sb_addr
            ld   a,(hl)
            ld   (SB_CELL),a
            ld   a,(SB_I)
            ld   e,a
            ld   d,0
            ld   hl,WB+4
            add  hl,de
            ld   a,(hl)
            ld   (SB_CODE),a
            and  7Fh                ; tiene que ser una ficha de A a ^
            cp   'A'
            jp   c,sc_bad
            cp   5Fh
            jp   nc,sc_bad
            ld   a,(SB_CELL)
            or   a
            jr   z,sc_new
            and  7Fh                ; la casilla ya tiene una ficha: tiene que ser la misma letra
            ld   b,a
            ld   a,(SB_CODE)
            and  7Fh
            cp   b
            jp   nz,sc_e3
            jr   sc_next            ; (NF ya vale 0)
sc_new:     ld   a,(SB_CODE)
            bit  7,a
            jr   z,sc_nb
            ld   hl,TR              ; comodin forzado
            ld   a,'?'
            call sb_take
            jp   nz,sc_e2
            ld   a,(SB_CODE)
            and  7Fh
            or   80h
            jr   sc_set
sc_nb:      ld   hl,TR
            call sb_take
            jr   nz,sc_nb2
            ld   a,(SB_CODE)
            jr   sc_set
sc_nb2:     ld   hl,TR              ; la letra no esta: un comodin
            ld   a,'?'
            call sb_take
            jp   nz,sc_e2
            ld   a,(SB_CODE)
            or   80h
sc_set:     ld   c,a
            ld   a,(SB_I)
            ld   e,a
            ld   d,0
            ld   hl,NF
            add  hl,de
            ld   (hl),c
            ld   hl,SB_NNEW
            inc  (hl)
sc_next:    ld   a,(SB_CX)
            ld   hl,SB_DX
            add  a,(hl)
            ld   (SB_CX),a
            ld   a,(SB_CY)
            ld   hl,SB_DY
            add  a,(hl)
            ld   (SB_CY),a
            ld   hl,SB_I
            inc  (hl)
            ld   a,(WB+3)
            cp   (hl)
            jp   nz,sc_lp_s
            ld   a,(SB_NNEW)
            or   a
            jr   z,sc_e4
            ; la palabra no puede seguir por los extremos: la casilla de antes y la de despues, vacias
            ld   a,(WB)
            ld   b,a
            ld   a,(SB_DX)
            ld   c,a
            ld   a,b
            sub  c
            jr   c,sc_aft
            ld   b,a
            ld   a,(WB+1)
            ld   c,a
            ld   a,(SB_DY)
            ld   d,a
            ld   a,c
            sub  d
            jr   c,sc_aft
            ld   c,a
            call sb_addr
            ld   a,(hl)
            or   a
            jr   nz,sc_e5
sc_aft:     ld   a,(SB_CX)
            cp   15
            jr   nc,sc_fm
            ld   b,a
            ld   a,(SB_CY)
            cp   15
            jr   nc,sc_fm
            ld   c,a
            call sb_addr
            ld   a,(hl)
            or   a
            jr   nz,sc_e5
sc_fm:      ld   a,(SB_BCNT)        ; la primera jugada tiene que cubrir la casilla central
            or   a
            jr   nz,sc_pl
            ld   a,(WB+2)
            or   a
            jr   nz,sc_fv
            ld   a,(WB+1)           ; en horizontal: y = 7 y x <= 7 <= x + n - 1
            cp   7
            jr   nz,sc_e6
            ld   a,(WB)
            jr   sc_fc
sc_fv:      ld   a,(WB)             ; en vertical: x = 7 y y <= 7 <= y + n - 1
            cp   7
            jr   nz,sc_e6
            ld   a,(WB+1)
sc_fc:      cp   8
            jr   nc,sc_e6
            ld   b,a
            ld   a,(WB+3)
            add  a,b
            cp   8
            jr   c,sc_e6
sc_pl:      call sb_place
            call sb_words
            push af
            call sb_unplace
            pop  af
            ret
sc_len:     ld   a,10
            ret
sc_bad:     ld   a,1
            ret
sc_e2:      ld   a,2
            ret
sc_e3:      ld   a,3
            ret
sc_e4:      ld   a,4
            ret
sc_e5:      ld   a,5
            ret
sc_e6:      ld   a,6
            ret

; pone las fichas nuevas (NF) en el tablero, o las quita
sb_place:   ld   c,1
            jr   spl_go
sb_unplace: ld   c,0
spl_go:     ld   a,(WB)
            ld   (SB_CX),a
            ld   a,(WB+1)
            ld   (SB_CY),a
            xor  a
            ld   (SB_I),a
spl_lp:     ld   a,(SB_I)
            ld   e,a
            ld   d,0
            ld   hl,NF
            add  hl,de
            ld   a,(hl)
            or   a
            jr   z,spl_n
            push bc
            ld   e,a
            ld   a,(SB_CX)
            ld   b,a
            ld   a,(SB_CY)
            ld   c,a
            push de
            call sb_addr
            pop  de
            pop  bc
            ld   a,c
            or   a
            jr   nz,spl_p
            ld   e,0
spl_p:      ld   (hl),e
spl_n:      ld   a,(SB_CX)
            ld   hl,SB_DX
            add  a,(hl)
            ld   (SB_CX),a
            ld   a,(SB_CY)
            ld   hl,SB_DY
            add  a,(hl)
            ld   (SB_CY),a
            ld   hl,SB_I
            inc  (hl)
            ld   a,(WB+3)
            cp   (hl)
            jr   nz,spl_lp
            ret

; las palabras que forma la jugada (las fichas ya estan puestas): la principal y las cruzadas
sb_words:   ld   a,(WB)
            ld   (SB_RX),a
            ld   a,(WB+1)
            ld   (SB_RY),a
            ld   a,(SB_DX)
            ld   (SB_RDX),a
            ld   a,(SB_DY)
            ld   (SB_RDY),a
            call sb_run
            call dc_lookup
            or   a
            ld   a,8
            ret  z
            ld   hl,(SB_WSC)
            ld   (SB_TOT),hl
            ld   a,(SB_NNEW)
            ld   b,a
            ld   a,(WB+3)
            cp   b
            jr   z,sw_nc
            ld   a,1                ; la palabra incluye una ficha que ya estaba: toca
            ld   (SB_CONN),a
sw_nc:      ld   a,(WB)             ; las cruzadas, una por cada ficha nueva
            ld   (SB_CX),a
            ld   a,(WB+1)
            ld   (SB_CY),a
            xor  a
            ld   (SB_I),a
sw_lp_s:      ld   a,(SB_I)
            ld   e,a
            ld   d,0
            ld   hl,NF
            add  hl,de
            ld   a,(hl)
            or   a
            jr   z,sw_n
            ld   a,(SB_CX)
            ld   (SB_RX),a
            ld   a,(SB_CY)
            ld   (SB_RY),a
            ld   a,(SB_DY)          ; el paso cruzado: (dy, dx)
            ld   (SB_RDX),a
            ld   a,(SB_DX)
            ld   (SB_RDY),a
            call sb_run
            ld   a,(SB_WLEN)
            cp   2
            jr   c,sw_n
            ld   a,1
            ld   (SB_CONN),a
            call dc_lookup
            or   a
            ld   a,9
            ret  z
            ld   hl,(SB_WSC)
            ld   de,(SB_TOT)
            add  hl,de
            ld   (SB_TOT),hl
sw_n:       ld   a,(SB_CX)
            ld   hl,SB_DX
            add  a,(hl)
            ld   (SB_CX),a
            ld   a,(SB_CY)
            ld   hl,SB_DY
            add  a,(hl)
            ld   (SB_CY),a
            ld   hl,SB_I
            inc  (hl)
            ld   a,(WB+3)
            cp   (hl)
            jr   nz,sw_lp_s
            ld   a,(SB_NNEW)        ; las 7 fichas: 50 puntos mas
            cp   7
            jr   nz,sw_b
            ld   hl,(SB_TOT)
            ld   de,50
            add  hl,de
            ld   (SB_TOT),hl
sw_b:       ld   a,(SB_BCNT)        ; tocar: solo hace falta despues de la primera jugada
            or   a
            jr   z,sw_ok
            ld   a,(SB_CONN)
            or   a
            ld   a,7
            ret  z
sw_ok:      xor  a
            ret

; La palabra que pasa por (SB_RX, SB_RY) en el sentido (SB_RDX, SB_RDY): sus letras en DC_W, su longitud en
; SB_WLEN y sus puntos en SB_WSC. Las casillas especiales solo cuentan con las fichas nuevas de la jugada (WB, NF).
sb_run:
sr_back:    ld   a,(SB_RX)          ; retrocede mientras haya ficha detras
            ld   hl,SB_RDX
            sub  (hl)
            jr   c,sr_fwd
            ld   d,a
            ld   a,(SB_RY)
            ld   hl,SB_RDY
            sub  (hl)
            jr   c,sr_fwd
            ld   e,a
            push de
            ld   b,d
            ld   c,e
            call sb_addr
            ld   a,(hl)
            pop  de
            or   a
            jr   z,sr_fwd
            ld   a,d
            ld   (SB_RX),a
            ld   a,e
            ld   (SB_RY),a
            jr   sr_back
sr_fwd:     xor  a
            ld   (SB_WLEN),a
            ld   hl,0
            ld   (SB_SUM),hl
            ld   a,1
            ld   (SB_WM),a
            ld   hl,DC_W
            ld   de,DC_W+1
            ld   bc,15
            ld   (hl),0
            ldir
sr_lp_s:      ld   a,(SB_RX)
            ld   b,a
            ld   a,(SB_RY)
            ld   c,a
            call sb_addr
            ld   a,(hl)
            or   a
            jp   z,sr_end
            ld   (SB_CODE),a
            and  7Fh                ; la letra a DC_W
            ld   c,a
            ld   a,(SB_WLEN)
            ld   e,a
            ld   d,0
            ld   hl,DC_W
            add  hl,de
            ld   (hl),c
            inc  a
            ld   (SB_WLEN),a
            ld   a,(SB_CODE)
            call sb_tileval
            ld   (SB_V),a
            call sb_isnew
            or   a
            jr   z,sr_add
            ld   a,(SB_RX)
            ld   b,a
            ld   a,(SB_RY)
            ld   c,a
            call sb_prem
            cp   1
            jr   nz,sr_p2
            ld   a,(SB_V)           ; letra doble
            add  a,a
            ld   (SB_V),a
            jr   sr_add
sr_p2:      cp   2
            jr   nz,sr_p3
            ld   a,(SB_V)           ; letra triple
            ld   b,a
            add  a,a
            add  a,b
            ld   (SB_V),a
            jr   sr_add
sr_p3:      cp   3
            jr   nz,sr_p4
            ld   a,(SB_WM)          ; palabra doble
            add  a,a
            ld   (SB_WM),a
            jr   sr_add
sr_p4:      cp   4
            jr   nz,sr_add
            ld   a,(SB_WM)          ; palabra triple
            ld   b,a
            add  a,a
            add  a,b
            ld   (SB_WM),a
sr_add:     ld   hl,(SB_SUM)
            ld   a,(SB_V)
            ld   e,a
            ld   d,0
            add  hl,de
            ld   (SB_SUM),hl
            ld   a,(SB_RX)
            ld   hl,SB_RDX
            add  a,(hl)
            ld   (SB_RX),a
            cp   15
            jr   nc,sr_end
            ld   a,(SB_RY)
            ld   hl,SB_RDY
            add  a,(hl)
            ld   (SB_RY),a
            cp   15
            jp   c,sr_lp_s
sr_end:     ld   a,(SB_WM)          ; puntos = suma * multiplicador
            ld   b,a
            ld   hl,0
            ld   de,(SB_SUM)
sr_m:       add  hl,de
            djnz sr_m
            ld   (SB_WSC),hl
            ret

; A = 1 si la casilla (SB_RX, SB_RY) es una ficha nueva de la jugada (WB, NF)
sb_isnew:   ld   a,(WB+2)
            or   a
            jr   nz,sin_v
            ld   a,(SB_RY)          ; en horizontal: la misma fila y x en [x0, x0 + n)
            ld   b,a
            ld   a,(WB+1)
            cp   b
            jr   nz,sin_no
            ld   a,(SB_RX)
            ld   b,a
            ld   a,(WB)
            ld   c,a
            ld   a,b
            sub  c
            jr   c,sin_no
            jr   sin_i
sin_v:      ld   a,(SB_RX)          ; en vertical: la misma columna
            ld   b,a
            ld   a,(WB)
            cp   b
            jr   nz,sin_no
            ld   a,(SB_RY)
            ld   b,a
            ld   a,(WB+1)
            ld   c,a
            ld   a,b
            sub  c
            jr   c,sin_no
sin_i:      ld   b,a
            ld   a,(WB+3)
            ld   c,a
            ld   a,b
            cp   c
            jr   nc,sin_no
            ld   e,b
            ld   d,0
            ld   hl,NF
            add  hl,de
            ld   a,(hl)
            or   a
            jr   z,sin_no
            ld   a,1
            ret
sin_no:     xor  a
            ret

; ---------------------------------------------------------------------
;  Hacer la jugada comprobada, pasar, cambiar fichas, acabar
; ---------------------------------------------------------------------
sb_apply:   call sb_place
            ld   a,(SB_NNEW)
            ld   b,a
            ld   a,(SB_BCNT)
            add  a,b
            ld   (SB_BCNT),a
            xor  a                  ; las fichas, fuera del atril
            ld   (SB_I),a
sa_lp:      ld   a,(SB_I)
            ld   e,a
            ld   d,0
            ld   hl,NF
            add  hl,de
            ld   a,(hl)
            or   a
            jr   z,sa_n
            bit  7,a
            jr   z,sa_t
            ld   a,'?'
sa_t:       push af
            call sb_rackbase
            pop  af
            call sb_take
sa_n:       ld   hl,SB_I
            inc  (hl)
            ld   a,(WB+3)
            cp   (hl)
            jr   nz,sa_lp
            call sb_refill
            ld   hl,(SB_TOT)        ; los puntos
            call sb_addscore
            xor  a
            ld   (SB_PASS),a
            ; se acaba si se queda sin fichas con la bolsa vacia
            ld   a,(SB_BAGN)
            or   a
            jr   nz,sa_flip
            call sb_rackbase
            ld   b,7
sa_e:       ld   a,(hl)
            or   a
            jr   nz,sa_flip
            inc  hl
            djnz sa_e
            call sb_finish_out
sa_flip:    ld   a,(SB_TURN)
            xor  1
            ld   (SB_TURN),a
            ret

; HL = puntos que se suman al jugador SB_TURN
sb_addscore: ld  a,(SB_TURN)
            add  a,a
            ld   e,a
            ld   d,0
            ex   de,hl
            ld   bc,SB_SCORE
            add  hl,bc              ; HL = &puntos del jugador, DE = lo que se suma
            ld   a,(hl)
            add  a,e
            ld   (hl),a
            inc  hl
            ld   a,(hl)
            adc  a,d
            ld   (hl),a
            ret

; puntos a restar del jugador A (0 o 1): HL = cuanto
sb_subscore: add a,a
            ld   c,a
            ld   b,0
            ex   de,hl
            ld   hl,SB_SCORE
            add  hl,bc
            ld   a,(hl)
            sub  e
            ld   (hl),a
            inc  hl
            ld   a,(hl)
            sbc  a,d
            ld   (hl),a
            ret

; el jugador SB_TURN se ha quedado sin fichas con la bolsa vacia: se queda lo que le quedaba al otro
sb_finish_out: ld a,1
            ld   (SB_OVER),a
            ld   a,(SB_TURN)
            xor  1
            ld   e,a                ; el otro
            add  a,a
            add  a,a
            add  a,a
            sub  e
            ld   e,a
            ld   d,0
            ld   hl,SB_RACK
            add  hl,de
            call sb_rackval         ; HL = su valor
            push hl
            call sb_addscore        ; el que acaba lo suma
            pop  hl
            ld   a,(SB_TURN)
            xor  1
            jp   sb_subscore        ; y el otro lo pierde

; el jugador SB_TURN pasa
sb_pass:    ld   a,(SB_OVER)
            or   a
            ret  nz
            ld   hl,SB_PASS
            inc  (hl)
            ld   a,(hl)
            cp   6
            jr   c,sp_flip
            ld   a,1                ; seis pases seguidos: se acaba y cada uno resta su atril
            ld   (SB_OVER),a
            ld   hl,SB_RACK
            call sb_rackval
            xor  a
            call sb_subscore
            ld   hl,SB_RACK+7
            call sb_rackval
            ld   a,1
            call sb_subscore
sp_flip:    ld   a,(SB_TURN)
            xor  1
            ld   (SB_TURN),a
            ret

; cambia las fichas WB+4.. (n = WB+3): se roban otras y las cambiadas van al final de la bolsa
sb_xcheck:  ld   a,(SB_OVER)
            or   a
            ld   a,11
            ret  nz
            ld   a,(WB+3)
            or   a
            jr   z,sx_len
            cp   8
            jr   nc,sx_len
            ld   a,(SB_BAGN)
            cp   7
            jr   c,sx_bag
            call sb_rackbase
            ld   de,TR
            ld   bc,7
            ldir
            ld   a,(WB+3)           ; las tiene que tener todas
            ld   b,a
            ld   hl,WB+4
sx_c:       ld   a,(hl)
            push hl
            push bc
            ld   hl,TR
            call sb_take
            pop  bc
            pop  hl
            jr   nz,sx_no
            inc  hl
            djnz sx_c
            xor  a
            ret

sb_exchange: call sb_xcheck
            or   a
            ret  nz
            ld   a,(WB+3)           ; fuera del atril
            ld   b,a
            ld   hl,WB+4
sx_r:       ld   a,(hl)
            push hl
            push bc
            push af
            call sb_rackbase
            pop  af
            call sb_take
            pop  bc
            pop  hl
            inc  hl
            djnz sx_r
            call sb_refill          ; se roba primero...
            ld   a,(WB+3)           ; ...y despues se devuelve
            ld   b,a
            ld   hl,WB+4
sx_p:       ld   a,(hl)
            push hl
            push bc
            call sb_bagput
            pop  bc
            pop  hl
            inc  hl
            djnz sx_p
            ld   hl,SB_PASS
            inc  (hl)
            ld   a,(SB_TURN)
            xor  1
            ld   (SB_TURN),a
            xor  a
            ret
sx_len:     ld   a,10
            ret
sx_bag:     ld   a,4
            ret
sx_no:      ld   a,2
            ret

; ---------------------------------------------------------------------
;  Tablas de cada idioma, por ficha: A..Z y despues (castellano) N con tilde, CH, LL, RR. El comodin no entra (2, valor 0)
; ---------------------------------------------------------------------
;                 A B C D E  F G H I J  K L M N O  P Q R S T  U V W X Y  Z [ \ ] ^
sb_cnt_en:  defb  9,2,2,4,12, 2,3,2,9,1, 1,4,2,6,8, 2,1,6,4,6, 4,2,2,1,2, 1,0,0,0,0
sb_val_en:  defb  1,3,3,2,1,  4,2,4,1,8, 5,1,3,1,1, 3,10,1,1,1, 1,4,4,8,4, 10,0,0,0,0
sb_cnt_es:  defb 12,2,4,5,12, 1,2,2,6,1, 0,4,2,5,9, 2,1,5,6,4, 5,1,0,1,1, 1,1,1,1,1
sb_val_es:  defb  1,3,3,2,1,  4,2,4,1,8, 0,1,3,1,1, 3,5,1,1,1, 1,4,0,8,4, 10,8,5,8,8
