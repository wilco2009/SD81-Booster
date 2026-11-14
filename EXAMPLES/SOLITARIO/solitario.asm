; =====================================================================
;  SOLITARIO.ASM - Klondike con el raton Bluetooth del SD81 Booster, TODO en codigo maquina.
;
;  FASE 1 (video y dibujo): arranque del modo Superfast HiRes NATIVO con atributos Chroma modo 0 (uno por cada byte de linea, 8x1),
;  tapete, draw_card (carta entera o solo su banda superior, con destino y paso configurables), huecos y dorsos, barra de estado
;  con la fuente de la ROM y el cursor por hardware de mouselib.inc.
;  FASE 2 (modelo y mesa): baraja (Fisher-Yates), reparto, mesa entera con el solape dinamico por columna, prueba de impacto
;  (hit_test) y clic en el mazo (robo de 1 o de 3, reciclar el descarte).  Todavia no se pueden mover cartas (fase 3).
;  FASE 3 (arrastre): se agarran cartas con el boton izquierdo y se mueven con sprites; reglas de Klondike; al soltar, si cabe se
;  mueve y si no vuelve deslizandose; la carta que queda arriba de una columna se destapa sola.
;  FASE 4: clic derecho o doble clic a la fundacion, puntos (estilo Windows) y tiempo, botones NEW / D3 / QUIT de la barra.
;  Teclas: N = partida nueva, D = robo de 1 / de 3 (y partida nueva), Q = salir al BASIC.
;  Plan y decisiones: claude/planning/solitario.md
;
;  Video:   POKE 2043,0 / 2044,$A0 (HFILE = $A000: bitmap lineal, 32 bytes por linea, 6 KB), atributos de modo 0 en $C000
;           (bloque 6, mismo orden que el bitmap; papel en el nibble alto, tinta en el bajo, bit 3 de cada nibble = brillo),
;           POKE 2045,171 (HiRes nativo) y puerto $7FEF = $20 (Chroma activo, modo 0). Igual que EXAMPLES/SCROLLH.
;  Memoria: $5000- codigo, cards.inc y mouselib (maximo 12288 bytes: nada de codigo por encima de $8000);
;           $8000- buzon del raton (28 bytes); $8100- variables de dibujo; $8200- modelo (baraja, columnas, mazo, descarte,
;           fundaciones) y variables del juego.   Bitmap $A000-$B7FF, atributos $C000-$D7FF.
;  Requisitos: modo FAST (en SLOW el NMI molesta); la ROM sin LOAD *ROMLOCK (el cursor usa los registros de sprite).
;  No toca IX, IY, I ni los registros alternativos (la ROM los usa al volver a SLOW).
;  Entrada: RAND USR 20480 (SOLITARIO.B81).
;  Ensamblar: pasmo -I ..\MOUSE solitario.asm solitario.bin solitario.sym   (compila.bat)
; =====================================================================

MS_BASE     equ 8000h               ; buzon de mouselib (28 bytes)
ML_CLOCK    equ 4034h               ; FRAMES: reloj del doble clic (fase 4)

BM          equ 0A000h              ; bitmap: linea*32 + columna
A0          equ 0C000h              ; atributos de modo 0, mismo orden
ATTOFS      equ A0-BM
FONT        equ 1E00h               ; juego de caracteres de la ROM del ZX81 (64 x 8 bytes)
BAR_Y       equ 184                 ; barra de estado: lineas 184-191
TEST_KEYS   equ 0                   ; 1 = tecla de prueba W (parte de una partida ganada para ver la animacion); 0 en la version final
CD_HOLE     equ 0FFh                ; "codigo de carta" del hueco vacio
TOP_Y       equ 2                   ; fila de arriba (mazo, descarte, fundaciones)
BOARD_Y     equ 40                  ; primera linea del tablero
BOARD_H     equ 142                 ; alto del tablero (hasta y = 182)
DOWN_STEP   equ 4                   ; solape de las tapadas
UP_STEP     equ 10                  ; solape de las destapadas
DOWN_MIN    equ 2                   ; minimo de las tapadas
UP_MIN      equ 8                   ; minimo de las destapadas (con 7 el indice, de 7 lineas, queda cortado; con 8 el peor caso
                                    ; 6 tapadas + 12 destapadas + carta entera mide 140 de 142)

; variables (RAM sobre $8000, no van en el binario)
VARS        equ 8100h
dc_dst      equ VARS+0              ; w  destino del bitmap de la linea actual
dc_att      equ VARS+2              ; w  destino del atributo de la linea actual
dc_n        equ VARS+4              ; lineas pedidas
dc_line     equ VARS+5              ; linea actual
dc_kind     equ VARS+6              ; 0 cara, 1 dorso, 2 hueco
dc_pitch    equ VARS+7              ; paso entre lineas (32 = pantalla, 3 = bufer de sprites)
dc_swap     equ VARS+8              ; 1 = girar los nibbles del atributo (color de sprite = tinta*16 + papel)
dc_ink      equ VARS+9              ; atributo de la cara segun el palo
dc_pr       equ VARS+10             ; w  indice de rango de la carta (7 bytes)
dc_ps       equ VARS+12             ; w  palo pequeno (7 bytes)
dc_pb       equ VARS+14             ; w  palo / letra grande (48 bytes)
ln          equ VARS+16             ; 4 bytes: la linea compuesta (b0, b1, b2, atributo)
pc_attr     equ VARS+20             ; atributo con que putch pinta el caracter

; modelo del juego. Carta = palo*16 + rango (palo 0-3, rango 0-12), bit 7 = boca arriba (igual que el codigo de draw_card)
MODEL       equ 8200h
deck        equ MODEL+0             ; 52: la baraja
tab         equ MODEL+64            ; 7 columnas de 20 cartas (de la mas tapada a la de arriba)
tab_n       equ MODEL+204           ; 7: cuantas cartas tiene cada columna
stock       equ MODEL+224           ; 24: el mazo (la de arriba es la ultima), boca abajo
waste       equ MODEL+256           ; 24: el descarte (la de arriba es la ultima), boca arriba
stock_n     equ MODEL+288
waste_n     equ MODEL+289
waste_vis   equ MODEL+290           ; cartas del abanico del descarte (1-3)
draw_mode   equ MODEL+291           ; 1 o 3
found_n     equ MODEL+292           ; 4: cuantas lleva cada fundacion
found_s     equ MODEL+296           ; 4: su palo
col_d       equ MODEL+300           ; 7: paso de las tapadas de cada columna (lo deja col_steps)
col_u       equ MODEL+308           ; 7: paso de las destapadas
seed        equ MODEL+316           ; w
ht_area     equ MODEL+318           ; resultado de hit_test
ht_idx      equ MODEL+319
ht_card     equ MODEL+320
pk_n        equ MODEL+321           ; teclas N y D en el cuadro anterior
pk_d        equ MODEL+322
; temporales (rutinas de dibujo, reparto y prueba de impacto)
cs_k        equ MODEL+324
cs_n        equ MODEL+325
cs_nd       equ MODEL+326
cs_nu       equ MODEL+327
cs_d        equ MODEL+328
cs_u        equ MODEL+329
dcl_k       equ MODEL+330
dcl_x       equ MODEL+331
dcl_n       equ MODEL+332
dcl_i       equ MODEL+333
dcl_y       equ MODEL+334
dcl_step    equ MODEL+335
dcl_p       equ MODEL+336           ; w
dw_cnt      equ MODEL+338
dw_i        equ MODEL+339
dw_col      equ MODEL+340
df_s        equ MODEL+341
db_k        equ MODEL+342
dl_k        equ MODEL+343
dl_p        equ MODEL+344           ; w
ht_k        equ MODEL+346
ht_n        equ MODEL+347
ht_i        equ MODEL+348
ht_y        equ MODEL+349
ht_p        equ MODEL+350           ; w

; arrastre (fase 3)
DRV         equ 8400h
dr_src      equ DRV+0               ; origen: 1 columna, 2 descarte, 3 fundacion
dr_idx      equ DRV+1               ; columna o fundacion de origen
dr_i        equ DRV+2               ; indice de la primera carta arrastrada en su columna
dr_m        equ DRV+3               ; cuantas cartas se arrastran
dr_n0       equ DRV+4               ; cartas que tenia la columna de origen
dr_vis0     equ DRV+5               ; abanico del descarte al empezar
dr_x0       equ DRV+6               ; esquina del monton al empezar (origen)
dr_y0       equ DRV+7
dr_ox       equ DRV+8               ; desplazamiento del agarre respecto a la esquina
dr_oy       equ DRV+9
dr_px       equ DRV+10              ; esquina actual
dr_py       equ DRV+11
dr_step     equ DRV+12              ; solape del monton
dr_rows     equ DRV+13              ; filas de sprites (3 sprites por fila)
dr_moved    equ DRV+14
dr_tk       equ DRV+15              ; destino: columna o fundacion
dr_tarea    equ DRV+16              ; 1 columna, 2 fundacion
dr_h        equ DRV+17              ; alto del monton en lineas
dt_rank     equ DRV+18
dt_suit     equ DRV+19
dt_red      equ DRV+20
dt_n        equ DRV+21
pk_dd       equ DRV+22
sc_j        equ DRV+23
sc_y        equ DRV+24
pm_n        equ DRV+25
pm_d        equ DRV+26
pm_e        equ DRV+27
sl_r        equ DRV+28
sl_c        equ DRV+29
sm_r        equ DRV+30
sm_c        equ DRV+31
sm_y        equ DRV+32
; fase 4: puntos y tiempo
P4          equ 8500h
score       equ P4+0                ; w
tm_sec      equ P4+2                ; w  segundos de partida
tm_last     equ P4+4                ; w  ultimo FRAMES visto
tm_run      equ P4+6                ; 1 = el reloj corre (desde el primer movimiento)
tm_fr       equ P4+7                ; imagenes acumuladas (50 por segundo)
tm_s10      equ P4+8                ; segundos desde la ultima penalizacion de 2 puntos
bar_dirty   equ P4+9                ; 1 = hay que repintar la barra
tm_tmp      equ P4+10
pm_p        equ DRV+34              ; w
sl_p        equ DRV+36              ; w
dr_cards    equ DRV+48              ; hasta 13 cartas arrastradas (boca arriba)

; fase 5: autocompletar y animacion de victoria
P5          equ 8600h
auto_on     equ P5+0                ; 1 = autocompletando a las fundaciones
win_on      equ P5+1                ; 1 = animacion de victoria en marcha
at_col      equ P5+2                ; columna que se esta probando (auto_check/auto_step)
at_tx       equ P5+3                ; destino x del deslizamiento a la fundacion
at_ty       equ P5+4
wn_suit     equ P5+5                ; palo de la fundacion que se esta vaciando
wn_vy       equ P5+6                ; velocidad vertical de la caida (con signo)
wn_dir      equ P5+7                ; deriva horizontal por fotograma (con signo, multiplo de 8)
wn_t        equ P5+8                ; fotogramas que le quedan a la carta actual
wn_tr       equ P5+9                ; atributo de la estela de la carta actual
FLOOR_Y     equ 152                 ; BAR_Y(184) - CD_H(32): donde rebota el canto de abajo de la carta

            org 20480

; ---------------------------------------------------------------------
;  Arranque
; ---------------------------------------------------------------------
start:      xor  a                  ; espera a que se suelte la tecla con la que se lanzo (A = 0: se leen las 8 filas a la vez)
            in   a,(0FEh)
            cpl
            and  1Fh
            jr   nz,start

            di
            xor  a
            ld   (2043),a           ; HFILE = $A000
            ld   a,0A0h
            ld   (2044),a
            ld   bc,7FEFh
            ld   a,20h              ; Chroma activo, modo 0
            out  (c),a
            xor  a
            out  (0FBh),a           ; borde negro (como SCROLLH)

            ld   a,3
            ld   (draw_mode),a
            ld   hl,0ACE1h
            ld   (seed),hl
            xor  a
            ld   (pk_n),a
            ld   (pk_d),a
            call clear_screen       ; la pantalla se prepara con el modo normal puesto: no se ve el parpadeo
            call new_deal           ; reparto, puntos y tiempo a cero, y la barra
            ld   a,171
            ld   (2045),a           ; Superfast HiRes nativo

            ld   bc,191*256+255     ; Y maximo 191, X maximo 255
            call ml_init
            call ml_cur_init

main:       ld   a,(win_on)
            or   a
            jr   nz,m_win
            ld   a,(auto_on)
            or   a
            jr   nz,m_auto
            call waitvs
            call ml_poll
            call ml_cur_show
            call tick_clock
            ld   a,(bar_dirty)
            or   a
            call nz,bar_update
            call auto_check
            ld   a,(ML_PRESS)
            and  3                  ; clic izquierdo o derecho
            jr   z,m_keys
            ld   a,(MP_Y)
            cp   BAR_Y
            jr   c,m_game
            ld   a,(ML_PRESS)       ; en la barra: botones NEW / D3 / QUIT (solo el izquierdo)
            rra
            jr   nc,m_keys
            call bar_click
            jr   m_keys
m_win:      call win_step           ; una carta por vuelta; 0 = termina (sola o porque el usuario pulsa algo)
            or   a
            jp   nz,main
            call new_deal
            jp   main
m_auto:     call auto_step          ; mueve una carta a la fundacion; cuando no puede, pasa a la animacion de victoria
            jp   main
m_game:     call hit_test
            ld   a,(ML_PRESS)
            and  2
            jr   nz,m_quick         ; clic derecho: a la fundacion
            ld   a,(ML_DBL)
            and  1
            jr   nz,m_quick         ; doble clic: a la fundacion
            ld   a,(ht_area)
            cp   AREA_STOCK
            jr   nz,m_pick
            call stock_click
            jr   m_keys
m_pick:     call pickup             ; carta destapada del tablero, descarte o fundacion: arrastre
            call c,drag_loop
            jr   m_keys
m_quick:    call quick_found
m_keys:     ld   a,7Fh              ; N: partida nueva (una vez por pulsacion)
            in   a,(0FEh)
            cpl
            and  8
            ld   c,a
            ld   a,(pk_n)
            ld   b,a
            ld   a,c
            ld   (pk_n),a
            or   a
            jr   z,m_d
            ld   a,b
            or   a
            jr   nz,m_d
            call new_deal
m_d:        ld   a,0FDh             ; D: robo de 1 / de 3
            in   a,(0FEh)
            cpl
            and  4
            ld   c,a
            ld   a,(pk_d)
            ld   b,a
            ld   a,c
            ld   (pk_d),a
            or   a
            jr   z,m_w
            ld   a,b
            or   a
            jr   nz,m_w
            call toggle_mode
m_w:
            if TEST_KEYS
            ld   a,0FBh             ; W: (prueba) como si se hubiera ganado la partida
            in   a,(0FEh)
            bit  1,a
            call z,test_win
            endif
m_q:        ld   a,0FBh             ; Q: salir
            in   a,(0FEh)
            bit  0,a
            jp   nz,main

quit:       call ml_cur_hide
            ld   bc,191*256+255     ; devuelve al MCU el rango por defecto
            call ml_init
            ld   a,85
            ld   (2045),a           ; modo ZX81 normal
            ld   bc,7FEFh
            ld   a,0Ch
            out  (c),a              ; color de Chroma81 apagado ($0C, como el explorador): si no, el BASIC sale con color
            ld   bc,0
            ret

; waitvs: espera el flanco de subida del VSYNC (bit 0 del puerto $AF)
waitvs:     in   a,(0AFh)
            rrca
            jr   c,waitvs
wvs2:       in   a,(0AFh)
            rrca
            jr   nc,wvs2
            ret

; ---------------------------------------------------------------------
;  clear_screen: bitmap a 0 y atributos a papel verde / tinta negra (el tapete)
; ---------------------------------------------------------------------
clear_screen:
            ld   hl,BM
            ld   (hl),0
            ld   de,BM+1
            ld   bc,6143
            ldir
            ld   hl,A0
            ld   (hl),CD_ATTR_FELT
            ld   de,A0+1
            ld   bc,6143
            ldir
            ret

; ---------------------------------------------------------------------
;  draw_card: dibuja una carta, o solo sus primeras B lineas (la banda de una carta tapada por otra).
;     A  = codigo: bit 7 = boca arriba, bits 4-6 = palo (0 picas, 1 corazones, 2 diamantes, 3 treboles),
;          bits 0-3 = rango (0 = A ... 9 = 10, 10 = J, 11 = Q, 12 = K);  CD_HOLE (255) = hueco vacio
;     HL = destino del bitmap de la linea 0, byte 0;  DE = destino de los atributos (mismo orden)
;     B  = lineas a dibujar (1-32)
;     (dc_pitch) = paso entre lineas: 32 = pantalla (atributos en $C000+), 3 = bufer lineal de 3 bytes por linea
;     (dc_swap)  = 1 gira los nibbles del atributo al escribirlo (el color del sprite es tinta*16 + papel)
;  Las 3 bytes de una linea comparten atributo. El formato de los datos esta en la cabecera de gen_cards.py.
;  Destruye AF BC DE HL.
; ---------------------------------------------------------------------
draw_card:  ld   (dc_dst),hl
            ld   (dc_att),de
            ld   c,a
            ld   a,b
            ld   (dc_n),a
            xor  a
            ld   (dc_line),a
            ld   a,c
            cp   CD_HOLE
            jr   nz,dcc_nh
            ld   a,2
            jr   dcc_kind
dcc_nh:     bit  7,c
            jr   nz,dcc_face
            ld   a,1                ; dorso
dcc_kind:   ld   (dc_kind),a
            jr   dc_loop

dcc_face:   xor  a
            ld   (dc_kind),a
            ld   a,c                ; B = rango, C = palo
            and  0Fh
            ld   b,a
            ld   a,c
            rra
            rra
            rra
            rra
            and  7
            ld   c,a
            ld   e,a                ; atributo de la tinta
            ld   d,0
            ld   hl,cd_suitattr
            add  hl,de
            ld   a,(hl)
            ld   (dc_ink),a
            ld   a,b                ; indice de rango: cd_rank + 7*rango
            call mul7
            ld   e,a
            ld   d,0
            ld   hl,cd_rank
            add  hl,de
            ld   (dc_pr),hl
            ld   a,c                ; palo pequeno: cd_ssuit + 7*palo
            call mul7
            ld   e,a
            ld   d,0
            ld   hl,cd_ssuit
            add  hl,de
            ld   (dc_ps),hl
            ld   a,b                ; centro: palo grande (A..10) o letra grande (J, Q, K)
            cp   10
            jr   nc,dcc_let
            ld   a,c
            call mul48
            ld   e,a
            ld   d,0
            ld   hl,cd_bsuit
            jr   dcc_big
dcc_let:    sub  10
            call mul48
            ld   e,a
            ld   d,0
            ld   hl,cd_bletter
dcc_big:    add  hl,de
            ld   (dc_pb),hl

dc_loop:    call dc_getline
            call dc_store
            ld   hl,dc_line
            inc  (hl)
            ld   a,(dc_n)
            cp   (hl)
            jr   nz,dc_loop
            ret

mul7:       ld   d,a                ; A = 7*A (A <= 12)
            add  a,a
            add  a,a
            add  a,a
            sub  d
            ret

mul48:      ld   d,a                ; A = 48*A (A <= 3)
            add  a,a
            add  a,d
            add  a,a
            add  a,a
            add  a,a
            add  a,a
            ret

; dc_getline: compone la linea (dc_line) en ln[0..3]
dc_getline: ld   a,(dc_line)
            ld   c,a                ; C = linea
            ld   a,(dc_kind)
            cp   2
            jp   z,dg_hole
            ld   a,c
            cp   2
            jr   c,dg_edge          ; lineas 0 y 1: j = linea
            cp   30
            jr   c,dg_inner
            sub  28                 ; lineas 30 y 31: j = 2, 3
dg_edge:    ld   c,a                ; C = j
            ld   b,a
            add  a,a
            add  a,b                ; 3*j
            ld   e,a
            ld   d,0
            ld   hl,cd_edge
            add  hl,de
            call dg_copy3
            ld   hl,cd_edgeattr
            ld   a,(dc_kind)
            or   a
            jr   z,dg_ea
            ld   hl,cd_backattr
dg_ea:      ld   e,c
            ld   d,0
            add  hl,de
            ld   a,(hl)
            ld   (ln+3),a
            ret

dg_inner:   ld   a,(dc_kind)
            or   a
            jr   nz,dg_back
            xor  a                  ; cara: en blanco, con la tinta del palo
            ld   (ln),a
            ld   (ln+1),a
            ld   (ln+2),a
            ld   a,(dc_ink)
            ld   (ln+3),a
            ld   a,c
            sub  CD_IDX_LINE
            cp   7
            jr   nc,dg_big
            ld   e,a                ; indice: rango en el byte 0, palo pequeno en el 2
            ld   d,0
            ld   hl,(dc_pr)
            add  hl,de
            ld   a,(hl)
            ld   (ln),a
            ld   hl,(dc_ps)
            add  hl,de
            ld   a,(hl)
            ld   (ln+2),a
dg_big:     ld   a,c
            sub  CD_BIG_LINE
            cp   16
            ret  nc
            ld   b,a
            add  a,a
            add  a,b                ; 3 * (linea - CD_BIG_LINE)
            ld   e,a
            ld   d,0
            ld   hl,(dc_pb)
            add  hl,de
            ld   a,(ln)
            or   (hl)
            ld   (ln),a
            inc  hl
            ld   a,(ln+1)
            or   (hl)
            ld   (ln+1),a
            inc  hl
            ld   a,(ln+2)
            or   (hl)
            ld   (ln+2),a
            ret

dg_back:    ld   a,c
            sub  CD_BACK_LINE
            ld   b,a
            add  a,a
            add  a,b
            ld   e,a
            ld   d,0
            ld   hl,cd_back
            add  hl,de
            call dg_copy3
            ld   a,CD_ATTR_BACK
            ld   (ln+3),a
            ret

dg_hole:    ld   a,c
            ld   b,a
            add  a,a
            add  a,b
            ld   e,a
            ld   d,0
            ld   hl,cd_hole
            add  hl,de
            call dg_copy3
            ld   a,CD_ATTR_HOLE
            ld   (ln+3),a
            ret

dg_copy3:   ld   de,ln              ; HL -> ln[0..2]
            ld   a,(hl)
            ld   (de),a
            inc  hl
            inc  de
            ld   a,(hl)
            ld   (de),a
            inc  hl
            inc  de
            ld   a,(hl)
            ld   (de),a
            ret

; dc_store: escribe la linea compuesta en el destino y avanza (dc_pitch) bytes
dc_store:   ld   hl,(dc_dst)
            ld   a,(ln)
            ld   (hl),a
            inc  hl
            ld   a,(ln+1)
            ld   (hl),a
            inc  hl
            ld   a,(ln+2)
            ld   (hl),a
            ld   hl,(dc_dst)
            ld   a,(dc_pitch)
            ld   e,a
            ld   d,0
            add  hl,de
            ld   (dc_dst),hl
            ld   a,(ln+3)
            ld   b,a
            ld   a,(dc_swap)
            or   a
            ld   a,b
            jr   z,ds_ns
            rlca                    ; girar los nibbles
            rlca
            rlca
            rlca
ds_ns:      ld   hl,(dc_att)
            ld   (hl),a
            inc  hl
            ld   (hl),a
            inc  hl
            ld   (hl),a
            ld   hl,(dc_att)
            add  hl,de              ; DE sigue siendo el paso
            ld   (dc_att),hl
            ret

; ---------------------------------------------------------------------
;  place_card: draw_card sobre la pantalla.
;     A = codigo, B = lineas, D = columna (x/8, 0-31), E = y (0-191)
; ---------------------------------------------------------------------
place_card: ld   c,a
            ld   a,32
            ld   (dc_pitch),a
            xor  a
            ld   (dc_swap),a
            ld   l,e
            ld   h,0
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl              ; y*32
            ld   e,d
            ld   d,0
            add  hl,de              ; + columna
            ld   de,BM
            add  hl,de              ; HL = bitmap
            push hl
            ld   de,ATTOFS
            add  hl,de
            ex   de,hl              ; DE = atributos
            pop  hl
            ld   a,c
            jp   draw_card

; ---------------------------------------------------------------------
;  Generador pseudoaleatorio (xorshift de 16 bits, semilla en (seed), nunca 0)
; ---------------------------------------------------------------------
rnd16:      ld   hl,(seed)
            ld   a,h
            rra
            ld   a,l
            rra
            xor  h
            ld   h,a
            ld   a,l
            rra
            ld   a,h
            rra
            xor  l
            ld   l,a
            xor  h
            ld   h,a
            ld   (seed),hl
            ret

; rnd_mod: A = n (1-255) -> A = numero al azar en 0..n-1.  Destruye AF B HL.
rnd_mod:    ld   b,a
            call rnd16
            ld   a,h
rm1:        cp   b
            ret  c
            sub  b
            jr   rm1

; ---------------------------------------------------------------------
;  col_addr: A = columna del tablero (0-6) -> HL = tab + 20*A (las cartas de la columna, de la mas tapada a la de arriba).
;  Destruye AF BC DE HL.
; ---------------------------------------------------------------------
col_addr:   ld   c,a
            add  a,a
            add  a,a                ; 4k
            ld   b,a
            add  a,a
            add  a,a                ; 16k
            add  a,b                ; 20k
            ld   e,a
            ld   d,0
            ld   hl,tab
            add  hl,de
            ret

; ---------------------------------------------------------------------
;  new_game: reparto nuevo.  La semilla se mezcla con R, FRAMES y la posicion del raton.
;  shuffle_deal: baraja (Fisher-Yates) y reparte con la semilla tal cual (para las pruebas).
; ---------------------------------------------------------------------
new_game:   ld   hl,(seed)
            ld   a,r
            add  a,l
            ld   l,a
            ld   a,(ML_CLOCK)
            xor  h
            ld   h,a
            ld   a,(MP_X)
            add  a,l
            ld   l,a
            ld   a,(MP_Y)
            xor  h
            ld   h,a
            or   l
            jr   nz,ng1
            ld   hl,0ACE1h
ng1:        ld   (seed),hl

shuffle_deal:
            ld   hl,deck            ; baraja ordenada: palo*16 + rango
            ld   d,0
ng_s:       ld   e,0
ng_r:       ld   a,d
            rlca
            rlca
            rlca
            rlca
            or   e
            ld   (hl),a
            inc  hl
            inc  e
            ld   a,e
            cp   13
            jr   nz,ng_r
            inc  d
            ld   a,d
            cp   4
            jr   nz,ng_s

            ld   c,51               ; Fisher-Yates: i = 51 .. 1, j al azar en 0..i
sh_lp:      ld   a,c
            inc  a
            call rnd_mod
            ld   e,a
            ld   d,0
            ld   hl,deck
            add  hl,de
            push hl                 ; &deck[j]
            ld   hl,deck
            ld   e,c
            ld   d,0
            add  hl,de              ; &deck[i]
            pop  de
            ld   a,(de)
            ld   b,(hl)
            ld   (hl),a
            ld   a,b
            ld   (de),a
            dec  c
            jr   nz,sh_lp

            ld   hl,deck            ; reparto: la columna k recibe k+1 cartas, la ultima destapada
            ld   (dl_p),hl
            xor  a
            ld   (dl_k),a
dl_lp:      ld   a,(dl_k)
            call col_addr
            ex   de,hl              ; DE = destino
            ld   a,(dl_k)
            inc  a
            ld   c,a                ; C = k+1
            ld   b,0
            ld   hl,(dl_p)
            ldir
            ld   (dl_p),hl
            dec  de
            ld   a,(de)
            or   80h
            ld   (de),a             ; la de arriba, boca arriba
            ld   a,(dl_k)
            ld   e,a
            ld   d,0
            ld   hl,tab_n
            add  hl,de
            inc  a
            ld   (hl),a             ; tab_n[k] = k+1
            ld   (dl_k),a
            cp   7
            jr   nz,dl_lp

            ld   hl,(dl_p)          ; las 24 restantes, al mazo (boca abajo)
            ld   de,stock
            ld   bc,24
            ldir
            ld   a,24
            ld   (stock_n),a
            xor  a
            ld   (waste_n),a
            ld   (waste_vis),a
            ld   hl,found_n
            ld   (hl),a
            inc  hl
            ld   (hl),a
            inc  hl
            ld   (hl),a
            inc  hl
            ld   (hl),a
            ret

; ---------------------------------------------------------------------
;  clr_area: D = columna (x/8), E = y, B = lineas, C = ancho en bytes: bitmap a 0 y atributo de tapete.
; ---------------------------------------------------------------------
clr_area:   ld   l,e
            ld   h,0
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl
            ld   e,d
            ld   d,0
            add  hl,de
            ld   de,BM
            add  hl,de
ca_row:     push bc
            push hl
            ld   b,c
            xor  a
ca_b:       ld   (hl),a
            inc  hl
            djnz ca_b
            pop  hl
            push hl
            ld   de,ATTOFS
            add  hl,de
            ld   b,c
            ld   a,CD_ATTR_FELT
ca_a:       ld   (hl),a
            inc  hl
            djnz ca_a
            pop  hl
            ld   de,32
            add  hl,de
            pop  bc
            djnz ca_row
            ret

; ---------------------------------------------------------------------
;  Dibujo de la mesa.  Columna k del tablero / fila de arriba: x = 16 + 32*k, es decir byte 2 + 4*k.
;  Fila de arriba en y = TOP_Y (mazo k=0, descarte k=1-2, fundaciones k=3-6); tablero desde y = BOARD_Y.
; ---------------------------------------------------------------------
col_x:      defb 2,6,10,14,18,22,26

draw_board: ld   de,0               ; toda la mesa (sin la barra) a tapete
            ld   bc,BAR_Y*256+32
            call clr_area
            call draw_stock
            call draw_waste
            call draw_found
            xor  a
            ld   (db_k),a
dbd_lp:      ld   a,(db_k)
            call draw_col
            ld   hl,db_k
            inc  (hl)
            ld   a,(hl)
            cp   7
            jr   nz,dbd_lp
            ret

draw_stock: ld   a,(stock_n)
            or   a
            ld   a,0                ; dorso
            jr   nz,ds_c
            ld   a,CD_HOLE
ds_c:       ld   b,32
            ld   de,2*256+TOP_Y
            jp   place_card

; draw_waste: el descarte; con robo de 3 las ultimas (waste_vis) cartas en abanico de 8 px, todas boca arriba
draw_waste: ld   de,6*256+TOP_Y
            ld   bc,32*256+5
            call clr_area
            ld   a,(waste_n)
            or   a
            ret  z
            ld   b,a
            ld   a,(waste_vis)
            ld   (dw_cnt),a
            ld   c,a
            ld   a,b
            sub  c
            ld   (dw_i),a           ; indice de la primera carta del abanico
            ld   a,6
            ld   (dw_col),a
dw_lp:      ld   a,(dw_i)
            ld   e,a
            ld   d,0
            ld   hl,waste
            add  hl,de
            ld   c,(hl)
            ld   a,(dw_col)
            ld   d,a
            ld   e,TOP_Y
            ld   b,32
            ld   a,c
            call place_card
            ld   hl,dw_i
            inc  (hl)
            ld   hl,dw_col
            inc  (hl)
            ld   hl,dw_cnt
            dec  (hl)
            jr   nz,dw_lp
            ret

; draw_found: las 4 fundaciones (cada una guarda su palo y cuantas lleva), columnas 3-6
draw_found: xor  a
            ld   (df_s),a
df_lp:      ld   a,(df_s)
            ld   e,a
            ld   d,0
            ld   hl,found_n
            add  hl,de
            ld   a,(hl)
            or   a
            ld   c,CD_HOLE
            jr   z,df_go
            dec  a                  ; rango de la de arriba = cuantas lleva - 1
            ld   c,a
            ld   hl,found_s
            add  hl,de
            ld   a,(hl)
            rlca
            rlca
            rlca
            rlca
            or   c
            or   80h
            ld   c,a
df_go:      ld   a,(df_s)
            add  a,a
            add  a,a
            add  a,14               ; columna de byte 14 + 4*s
            ld   d,a
            ld   e,TOP_Y
            ld   b,32
            ld   a,c
            call place_card
            ld   hl,df_s
            inc  (hl)
            ld   a,(hl)
            cp   4
            jr   nz,df_lp
            ret

; ---------------------------------------------------------------------
;  col_steps: A = columna. Solape vertical dinamico: las tapadas 4 px y las destapadas 10 px; si la columna no cabe
;  (BOARD_H - 32 = 110 px de franjas), primero se baja el paso de las destapadas hasta UP_MIN y despues el de las tapadas
;  hasta DOWN_MIN. Deja los pasos en col_d[k], col_u[k] (y en cs_d, cs_u).
; ---------------------------------------------------------------------
col_steps:  ld   (cs_k),a
            call col_addr           ; HL = cartas de la columna
            ld   a,(cs_k)
            ld   e,a
            ld   d,0
            push hl
            ld   hl,tab_n
            add  hl,de
            ld   a,(hl)
            pop  hl
            ld   (cs_n),a
            ld   b,a
            ld   c,0                ; C = tapadas
            or   a
            jr   z,cs_cnt_e
cs_cnt:     bit  7,(hl)
            jr   nz,cs_cnt_n
            inc  c
cs_cnt_n:   inc  hl
            djnz cs_cnt
cs_cnt_e:   ld   a,c
            ld   (cs_nd),a
            ld   a,(cs_n)
            sub  c                  ; destapadas
            jr   nz,cs_hasup
            ld   a,(cs_nd)          ; ninguna destapada: la ultima (tapada) va entera
            or   a
            jr   z,cs_cn2
            dec  a
            ld   (cs_nd),a
            xor  a
            jr   cs_cn2
cs_hasup:   dec  a                  ; la ultima destapada va entera: solo cuentan las franjas
cs_cn2:     ld   (cs_nu),a
            ld   a,DOWN_STEP
            ld   (cs_d),a
            ld   a,UP_STEP
            ld   (cs_u),a
cs_fit:     call cs_h
            cp   BOARD_H-32+1
            jr   c,cs_done
            ld   a,(cs_u)
            cp   UP_MIN
            jr   z,cs_dd
            dec  a
            ld   (cs_u),a
            jr   cs_fit
cs_dd:      ld   a,(cs_d)
            cp   DOWN_MIN
            jr   z,cs_done
            dec  a
            ld   (cs_d),a
            jr   cs_fit
cs_done:    ld   a,(cs_k)
            ld   e,a
            ld   d,0
            ld   hl,col_d
            add  hl,de
            ld   a,(cs_d)
            ld   (hl),a
            ld   hl,col_u
            add  hl,de
            ld   a,(cs_u)
            ld   (hl),a
            ret

; cs_h: A = alto de las franjas = tapadas*cs_d + destapadas*cs_u
cs_h:       ld   a,(cs_nd)
            ld   b,a
            ld   a,(cs_d)
            ld   c,a
            xor  a
            inc  b
            jr   csh2
csh1:       add  a,c
csh2:       djnz csh1
            ld   e,a
            ld   a,(cs_nu)
            ld   b,a
            ld   a,(cs_u)
            ld   c,a
            ld   a,e
            inc  b
            jr   csh4
csh3:       add  a,c
csh4:       djnz csh3
            ret

; ---------------------------------------------------------------------
;  draw_col: A = columna del tablero (0-6): la borra y la dibuja entera (las tapadas y destapadas solo en franja)
; ---------------------------------------------------------------------
draw_col:   ld   (dcl_k),a
            call col_steps
            ld   a,(dcl_k)
            ld   e,a
            ld   d,0
            ld   hl,col_x
            add  hl,de
            ld   a,(hl)
            ld   (dcl_x),a
            ld   d,a
            ld   e,BOARD_Y
            ld   bc,256*(BAR_Y-BOARD_Y)+3
            call clr_area
            ld   a,(dcl_k)
            ld   e,a
            ld   d,0
            ld   hl,tab_n
            add  hl,de
            ld   a,(hl)
            ld   (dcl_n),a
            or   a
            jr   nz,dcl_cards
            ld   a,(dcl_x)          ; columna vacia: hueco
            ld   d,a
            ld   e,BOARD_Y
            ld   b,32
            ld   a,CD_HOLE
            jp   place_card
dcl_cards:  ld   a,(dcl_k)
            call col_addr
            ld   (dcl_p),hl
            ld   a,BOARD_Y
            ld   (dcl_y),a
            xor  a
            ld   (dcl_i),a
dcl_lp:     ld   hl,(dcl_p)
            ld   c,(hl)             ; C = carta
            inc  hl
            ld   (dcl_p),hl
            ld   a,(dcl_n)
            ld   b,a
            ld   a,(dcl_i)
            inc  a
            cp   b
            jr   z,dcl_last
            ld   a,(dcl_k)
            ld   e,a
            ld   d,0
            bit  7,c
            jr   nz,dcl_up
            ld   hl,col_d
            jr   dcl_gs
dcl_up:     ld   hl,col_u
dcl_gs:     add  hl,de
            ld   b,(hl)             ; B = paso = lineas que se dibujan
            ld   a,b
            jr   dcl_draw
dcl_last:   ld   b,32               ; la ultima, entera (y no hay mas)
            xor  a
dcl_draw:   ld   (dcl_step),a
            ld   a,(dcl_x)
            ld   d,a
            ld   a,(dcl_y)
            ld   e,a
            ld   a,c
            call place_card
            ld   a,(dcl_step)
            ld   hl,dcl_y
            add  a,(hl)
            ld   (hl),a
            ld   hl,dcl_i
            inc  (hl)
            ld   a,(dcl_n)
            cp   (hl)
            jr   nz,dcl_lp
            ret

; ---------------------------------------------------------------------
;  Prueba de impacto: que hay bajo el puntero (MP_X, MP_Y).
;     ht_area: 0 nada, 1 mazo, 2 descarte (su carta de arriba), 3 fundacion, 4 columna del tablero
;     ht_idx : fundacion (0-3) o columna (0-6); 255 si no hay
;     ht_card: en el tablero, indice de la carta de arriba que cubre el punto (0 = la mas tapada); 255 = hueco vacio / otra area
;  Usa los pasos col_d / col_u que dejo la ultima draw_col.
; ---------------------------------------------------------------------
AREA_STOCK  equ 1
AREA_WASTE  equ 2
AREA_FOUND  equ 3
AREA_TAB    equ 4

; ht_colx: A = columna (k) bajo MP_X; NC si MP_X esta sobre una carta (24 px de cada 32), C si cae fuera
ht_colx:    ld   a,(MP_X)
            sub  16
            ret  c
            ld   b,a
            and  31
            cp   24
            jr   nc,htc_no
            ld   a,b
            rrca
            rrca
            rrca
            rrca
            rrca
            and  7
            cp   7
            jr   nc,htc_no
            or   a
            ret
htc_no:     scf
            ret

hit_test:   xor  a
            ld   (ht_area),a
            dec  a
            ld   (ht_idx),a
            ld   (ht_card),a
            ld   a,(MP_Y)
            cp   TOP_Y
            ret  c
            cp   TOP_Y+32
            jp   c,ht_top
            cp   BOARD_Y
            ret  c
            cp   BAR_Y
            ret  nc
            call ht_colx
            ret  c
            ld   (ht_k),a
            ld   e,a
            ld   d,0
            ld   hl,tab_n
            add  hl,de
            ld   a,(hl)
            ld   (ht_n),a
            or   a
            jr   nz,ht_ne
            ld   a,(MP_Y)           ; columna vacia: solo el hueco (32 lineas)
            cp   BOARD_Y+32
            ret  nc
            jr   ht_set
ht_ne:      ld   a,(ht_k)
            call col_addr
            ld   (ht_p),hl
            ld   a,BOARD_Y
            ld   (ht_y),a
            xor  a
            ld   (ht_i),a
ht_w:       ld   a,(ht_n)
            ld   b,a
            ld   a,(ht_i)
            inc  a
            cp   b
            jr   z,ht_last
            ld   hl,(ht_p)
            ld   c,(hl)
            ld   a,(ht_k)
            ld   e,a
            ld   d,0
            bit  7,c
            jr   nz,htw_u
            ld   hl,col_d
            jr   htw_s
htw_u:      ld   hl,col_u
htw_s:      add  hl,de
            ld   a,(ht_y)
            add  a,(hl)
            ld   c,a                ; C = fin de la franja
            ld   a,(MP_Y)
            cp   c
            jr   c,ht_hit
            ld   a,c
            ld   (ht_y),a
            ld   hl,ht_i
            inc  (hl)
            ld   hl,(ht_p)
            inc  hl
            ld   (ht_p),hl
            jr   ht_w
ht_last:    ld   a,(ht_y)
            add  a,32
            ld   c,a
            ld   a,(MP_Y)
            cp   c
            ret  nc                 ; por debajo de la ultima carta: nada
ht_hit:     ld   a,(ht_i)
            ld   (ht_card),a
ht_set:     ld   a,(ht_k)
            ld   (ht_idx),a
            ld   a,AREA_TAB
            ld   (ht_area),a
            ret

ht_top:     ld   a,(MP_X)           ; fila de arriba
            cp   16
            ret  c
            cp   40
            jr   nc,htt_w
            ld   a,AREA_STOCK
            ld   (ht_area),a
            xor  a
            ld   (ht_idx),a
            ret
htt_w:      ld   a,(waste_n)
            or   a
            jr   z,htt_f
            ld   a,(waste_vis)
            dec  a
            add  a,a
            add  a,a
            add  a,a
            add  a,72
            ld   c,a                ; fin del abanico (excluido)
            ld   a,(MP_X)
            cp   48
            jr   c,htt_f
            cp   c
            jr   nc,htt_f
            ld   a,AREA_WASTE
            ld   (ht_area),a
            xor  a
            ld   (ht_idx),a
            ret
htt_f:      call ht_colx
            ret  c
            cp   3
            ret  c
            sub  3
            ld   (ht_idx),a
            ld   a,AREA_FOUND
            ld   (ht_area),a
            ret

; ---------------------------------------------------------------------
;  stock_click: clic en el mazo. Roba (draw_mode) cartas al descarte, o con el mazo vacio recicla el descarte
;  (sin limite de vueltas: el descarte se da la vuelta y su primera carta vuelve a quedar arriba).
; ---------------------------------------------------------------------
stock_click:
            call start_clock
            ld   a,(stock_n)
            or   a
            jr   z,sc_rec
            ld   b,a
            ld   a,(draw_mode)
            cp   b
            jr   c,sc_m
            ld   a,b                ; quedan menos que las pedidas
sc_m:       ld   c,a
            ld   (waste_vis),a
sc_lp:      ld   a,(stock_n)
            dec  a
            ld   (stock_n),a
            ld   e,a
            ld   d,0
            ld   hl,stock
            add  hl,de
            ld   a,(hl)
            or   80h
            ld   b,a
            ld   a,(waste_n)
            ld   e,a
            ld   d,0
            ld   hl,waste
            add  hl,de
            ld   (hl),b
            ld   hl,waste_n
            inc  (hl)
            dec  c
            jr   nz,sc_lp
            call draw_stock
            jp   draw_waste
sc_rec:     ld   a,(waste_n)
            or   a
            ret  z
            ld   c,a
            ld   (stock_n),a
            ld   hl,waste           ; stock[i] = waste[n-1-i], boca abajo
            ld   e,c
            ld   d,0
            add  hl,de
            dec  hl
            ld   de,stock
sc_rl:      ld   a,(hl)
            and  7Fh
            ld   (de),a
            dec  hl
            inc  de
            dec  c
            jr   nz,sc_rl
            xor  a
            ld   (waste_n),a
            ld   (waste_vis),a
            ld   hl,100             ; reciclar el descarte: -100 (robo de 1) / -20 (robo de 3)
            ld   a,(draw_mode)
            cp   3
            jr   nz,sc_pen
            ld   hl,20
sc_pen:     call sub_score
            call draw_stock
            jp   draw_waste

; toggle_mode: robo de 1 <-> robo de 3 (empieza partida nueva) y actualiza el boton de la barra
toggle_mode:
            ld   a,(draw_mode)
            cp   3
            ld   a,1
            jr   z,tm1
            ld   a,3
tm1:        ld   (draw_mode),a
            add  a,'0'
            ld   (bar_dn),a
            jp   new_deal

; ---------------------------------------------------------------------
;  FASE 3: arrastre con sprites.
;  Al pulsar sobre una carta destapada del tablero (se lleva ella y las de debajo), la de arriba del descarte o la de una
;  fundacion, se compone el monton en un bufer lineal (3 bytes por linea: bitmap, atributo y mascara), se carga en sprites
;  de formato A (8x8, 3 por fila, hasta 19 filas = 57 sprites; el cursor es el 63) y se redibuja el origen sin esas cartas.
;  Mientras se mantiene el boton los sprites siguen al cursor; al soltar, si el monton cabe en el sitio (regla de Klondike)
;  se mueve en el modelo y se redibujan origen y destino; si no, vuelve a su sitio deslizandose.
;  Un movimiento de menos de 3 pixeles es un clic: no mueve nada.
; ---------------------------------------------------------------------
SBM         equ 8800h               ; bufer del monton: bitmap, 20 filas * 8 lineas * 3 bytes = 480
SAT         equ 89E0h               ; atributo (primero el de pantalla, despues el de sprite: tinta*16 + papel)
SMK         equ 8BC0h               ; mascara
ATOFF       equ SAT-SBM
MASKOFF     equ SMK-SBM

; pickup: HL..: tras un clic izquierdo con hit_test ya hecho. CARRY = se ha empezado un arrastre
pickup:     ld   a,(ht_area)
            cp   AREA_TAB
            jr   z,pk_tab
            cp   AREA_WASTE
            jp   z,pk_waste
            cp   AREA_FOUND
            jp   z,pk_found
pk_fail:    or   a
            ret

pk_tab:     ld   a,(ht_card)
            cp   0FFh
            jr   z,pk_fail
            ld   (dr_i),a
            ld   a,(ht_idx)
            ld   (dr_idx),a
            call col_addr           ; HL = cartas de la columna
            ld   a,(dr_i)
            ld   e,a
            ld   d,0
            add  hl,de
            bit  7,(hl)             ; solo se arrastran las destapadas
            jr   z,pk_fail
            ld   a,(dr_idx)
            ld   e,a
            ld   d,0
            ld   hl,tab_n
            add  hl,de
            ld   a,(hl)
            ld   (dr_n0),a
            ld   hl,dr_i
            sub  (hl)
            ld   (dr_m),a           ; m = n - i
            ld   a,(dr_idx)
            call col_addr
            ld   a,(dr_i)
            ld   e,a
            ld   d,0
            add  hl,de
            ld   de,dr_cards
            ld   a,(dr_m)
            ld   c,a
            ld   b,0
            ldir
            ld   a,(dr_idx)         ; x0 = 8 * col_x, paso de las destapadas
            ld   e,a
            ld   d,0
            ld   hl,col_x
            add  hl,de
            ld   a,(hl)
            add  a,a
            add  a,a
            add  a,a
            ld   (dr_x0),a
            ld   hl,col_u
            add  hl,de
            ld   a,(hl)
            ld   (dr_step),a
            ld   hl,col_d
            add  hl,de
            ld   a,(hl)
            ld   (pk_dd),a
            ld   a,BOARD_Y          ; y0 = BOARD_Y + los pasos de las cartas de antes
            ld   (dr_y0),a
            ld   a,(dr_idx)
            call col_addr
            ld   a,(dr_i)
            or   a
            jr   z,pk_y0e
            ld   b,a
pk_yl:      bit  7,(hl)
            ld   a,(pk_dd)
            jr   z,pk_ys
            ld   a,(dr_step)
pk_ys:      ld   c,a
            ld   a,(dr_y0)
            add  a,c
            ld   (dr_y0),a
            inc  hl
            djnz pk_yl
pk_y0e:     ld   a,1
            ld   (dr_src),a
            jp   pk_common

pk_waste:   ld   a,(waste_n)
            or   a
            jr   nz,pkf_skip1375
            jp   pk_fail
pkf_skip1375:
            ld   e,a
            dec  e
            ld   d,0
            ld   hl,waste
            add  hl,de
            ld   a,(hl)
            ld   (dr_cards),a
            ld   a,1
            ld   (dr_m),a
            ld   a,(waste_vis)
            ld   (dr_vis0),a
            dec  a
            add  a,a
            add  a,a
            add  a,a
            add  a,48
            ld   (dr_x0),a          ; la de arriba del abanico
            ld   a,TOP_Y
            ld   (dr_y0),a
            ld   a,2
            ld   (dr_src),a
            xor  a
            ld   (dr_idx),a
            ld   a,UP_STEP
            ld   (dr_step),a
            jr   pk_common

pk_found:   ld   a,(ht_idx)
            ld   (dr_idx),a
            ld   e,a
            ld   d,0
            ld   hl,found_n
            add  hl,de
            ld   a,(hl)
            or   a
            jr   nz,pkf_skip1411
            jp   pk_fail
pkf_skip1411:
            dec  a                  ; rango de la de arriba
            ld   c,a
            ld   hl,found_s
            add  hl,de
            ld   a,(hl)
            rlca
            rlca
            rlca
            rlca
            or   c
            or   80h
            ld   (dr_cards),a
            ld   a,1
            ld   (dr_m),a
            ld   a,(dr_idx)         ; x0 = 112 + 32*s
            add  a,a
            add  a,a
            add  a,a
            add  a,a
            add  a,a
            add  a,112
            ld   (dr_x0),a
            ld   a,TOP_Y
            ld   (dr_y0),a
            ld   a,3
            ld   (dr_src),a
            ld   a,UP_STEP
            ld   (dr_step),a

pk_common:  ld   a,(MP_X)           ; donde se agarro la carta: desplazamiento del cursor respecto a su esquina
            ld   hl,dr_x0
            sub  (hl)
            jr   nc,pk_o1
            xor  a
pk_o1:      ld   (dr_ox),a
            ld   a,(MP_Y)
            ld   hl,dr_y0
            sub  (hl)
            jr   nc,pk_o2
            xor  a
pk_o2:      ld   (dr_oy),a
            ld   a,(dr_x0)
            ld   (dr_px),a
            ld   a,(dr_y0)
            ld   (dr_py),a
            xor  a
            ld   (dr_moved),a
            call spr_compose
            call spr_load_all
            call spr_move           ; los sprites quedan justo encima de las cartas
            ld   a,(dr_src)         ; y ahora se quitan del modelo y se redibuja el origen
            cp   1
            jr   nz,pk_s2
            ld   a,(dr_idx)
            ld   e,a
            ld   d,0
            ld   hl,tab_n
            add  hl,de
            ld   a,(dr_i)
            ld   (hl),a
            ld   a,(dr_idx)
            call draw_col
            scf
            ret
pk_s2:      cp   2
            jr   nz,pk_s3
            ld   hl,waste_n
            dec  (hl)
            ld   a,(waste_vis)
            dec  a
            jr   nz,pk_w1
            ld   a,(waste_n)
            or   a
            ld   a,0
            jr   z,pk_w1
            inc  a                  ; quedan cartas debajo: se ve una
pk_w1:      ld   (waste_vis),a
            call draw_waste
            scf
            ret
pk_s3:      ld   a,(dr_idx)
            ld   e,a
            ld   d,0
            ld   hl,found_n
            add  hl,de
            dec  (hl)
            call draw_found
            scf
            ret

; ---------------------------------------------------------------------
;  spr_compose: el monton dr_cards (dr_m cartas, solape dr_step) en SBM/SAT/SMK; deja dr_h (alto) y dr_rows (filas de 8)
;  La mascara es 0 donde el punto es color de tapete (las esquinas redondeadas); el atributo pasa a color de sprite.
; ---------------------------------------------------------------------
spr_compose:
            ld   hl,SBM
            ld   (hl),0
            ld   de,SBM+1
            ld   bc,3*480-1
            ldir
            ld   a,3
            ld   (dc_pitch),a
            xor  a
            ld   (dc_swap),a
            ld   (sc_j),a
            ld   (sc_y),a
            ld   a,(dr_m)           ; alto = (m-1)*paso + 32
            dec  a
            ld   b,a
            ld   a,(dr_step)
            ld   c,a
            xor  a
            inc  b
            jr   scc2
scc1:       add  a,c
scc2:       djnz scc1
            add  a,32
            ld   (dr_h),a
            add  a,7
            rrca
            rrca
            rrca
            and  1Fh
            ld   (dr_rows),a
scc_lp:     ld   a,(sc_y)           ; BC = 3*y
            ld   e,a
            ld   d,0
            ld   h,d
            ld   l,e
            add  hl,hl
            add  hl,de
            ld   b,h
            ld   c,l
            ld   hl,SAT
            add  hl,bc
            ex   de,hl              ; DE = atributos
            ld   hl,SBM
            add  hl,bc              ; HL = bitmap
            ld   a,(dr_m)
            ld   b,a
            ld   a,(sc_j)
            inc  a
            cp   b
            ld   b,32               ; la ultima, entera
            jr   z,scc_d
            ld   a,(dr_step)
            ld   b,a
scc_d:      push hl
            push de
            ld   a,(sc_j)
            ld   e,a
            ld   d,0
            ld   hl,dr_cards
            add  hl,de
            ld   a,(hl)
            pop  de
            pop  hl
            call draw_card
            ld   a,(dr_step)
            ld   hl,sc_y
            add  a,(hl)
            ld   (hl),a
            ld   hl,sc_j
            inc  (hl)
            ld   a,(dr_m)
            cp   (hl)
            jr   nz,scc_lp
            ; mascara y color de sprite, linea a linea
            ld   hl,SBM
            ld   (pm_p),hl
            ld   a,(dr_h)
            ld   (pm_n),a
pm_lp:      ld   hl,(pm_p)
            ld   de,ATOFF
            add  hl,de
            ld   a,(hl)             ; atributo de pantalla de la linea: papel*16 + tinta
            ld   c,a
            and  0Fh
            sub  4
            ld   d,0                ; D = 255 si la tinta se ve (no es el verde del tapete)
            jr   z,pm1
            dec  d
pm1:        ld   a,c
            rrca
            rrca
            rrca
            rrca
            and  0Fh
            sub  4
            ld   e,0                ; E = 255 si el papel se ve
            jr   z,pm2
            dec  e
pm2:        ld   a,d
            ld   (pm_d),a
            ld   a,e
            ld   (pm_e),a
            ld   a,c
            rlca
            rlca
            rlca
            rlca                    ; color de sprite = tinta*16 + papel
            ld   (hl),a
            inc  hl
            ld   (hl),a
            inc  hl
            ld   (hl),a
            ld   hl,(pm_p)
            call mk_byte
            call mk_byte
            call mk_byte
            ld   (pm_p),hl
            ld   hl,pm_n
            dec  (hl)
            jr   nz,pm_lp
            ret

; mk_byte: HL = byte de bitmap; escribe su mascara (visible donde el bit es 1 y la tinta se ve, o es 0 y el papel se ve); HL avanza
mk_byte:    ld   c,(hl)
            ld   a,(pm_e)
            ld   b,a
            ld   a,c
            cpl
            and  b
            ld   b,a
            ld   a,(pm_d)
            and  c
            or   b
            push hl
            ld   de,MASKOFF
            add  hl,de
            ld   (hl),a
            pop  hl
            inc  hl
            ret

; ---------------------------------------------------------------------
;  spr_load_all: carga los dr_rows*3 sprites (formato A) desde el bufer. spr_hide_all los apaga.
; ---------------------------------------------------------------------
spr_load_all:
            xor  a
            ld   (sl_r),a
sl_row:     xor  a
            ld   (sl_c),a
sl_col:     ld   a,(sl_r)           ; indice = 3*fila + columna
            ld   b,a
            add  a,a
            add  a,b
            ld   b,a
            ld   a,(sl_c)
            add  a,b
            ld   (SPR_SEL),a
            xor  a
            ld   (SPR_PAGE),a
            ld   a,(sl_r)           ; desplazamiento = 24*fila + columna (16 bits)
            ld   l,a
            ld   h,0
            add  hl,hl
            add  hl,hl
            add  hl,hl              ; 8*fila
            ld   d,h
            ld   e,l
            add  hl,hl
            add  hl,de              ; 24*fila
            ld   a,(sl_c)
            ld   e,a
            ld   d,0
            add  hl,de
            ld   de,SBM
            add  hl,de
            ld   (sl_p),hl
            ld   de,SPR_PIX
            call cp8s
            ld   hl,(sl_p)
            ld   de,ATOFF
            add  hl,de
            ld   de,SPR_COL
            call cp8s
            ld   hl,(sl_p)
            ld   de,MASKOFF
            add  hl,de
            ld   de,SPR_MASK
            call cp8s
            ld   a,(sl_c)
            inc  a
            ld   (sl_c),a
            cp   3
            jr   nz,sl_col
            ld   a,(sl_r)
            inc  a
            ld   (sl_r),a
            ld   hl,dr_rows
            cp   (hl)
            jr   nz,sl_row
            ret

; cp8s: 8 bytes de HL (de 3 en 3) a DE (consecutivos)
cp8s:       ld   b,8
c8:         ld   a,(hl)
            ld   (de),a
            inc  de
            inc  hl
            inc  hl
            inc  hl
            djnz c8
            ret

spr_hide_all:
            ld   a,(dr_rows)
            ld   b,a
            add  a,a
            add  a,b
            ld   b,a                ; B = sprites usados
            or   a
            ret  z
            ld   c,0
sph_lp:      ld   a,c
            ld   (SPR_SEL),a
            xor  a
            ld   (SPR_EN),a
            inc  c
            djnz sph_lp
            ret

; ---------------------------------------------------------------------
;  spr_move: coloca los sprites segun (dr_px, dr_py). Las filas que quedan por debajo de la pantalla se apagan.
; ---------------------------------------------------------------------
spr_move:   xor  a
            ld   (sm_r),a
sm_row:     ld   a,(sm_r)
            add  a,a
            add  a,a
            add  a,a
            ld   b,a
            ld   a,(dr_py)
            add  a,b                ; y de pantalla de la fila (dr_py <= 180, 8*fila <= 144: puede pasar de 255)
            jr   nc,sm_yok
            ld   a,255
sm_yok:     ld   (sm_y),a
            xor  a
            ld   (sm_c),a
sm_col:     ld   a,(sm_r)
            ld   b,a
            add  a,a
            add  a,b
            ld   b,a
            ld   a,(sm_c)
            add  a,b
            ld   (SPR_SEL),a
            ld   a,(sm_y)
            cp   192
            jr   nc,sm_off          ; fuera de la pantalla
            add  a,32
            ld   (SPR_Y),a
            ld   a,(sm_c)
            add  a,a
            add  a,a
            add  a,a
            ld   l,a
            ld   h,0
            ld   a,(dr_px)
            ld   e,a
            ld   d,0
            add  hl,de
            ld   de,32
            add  hl,de
            ld   a,l
            ld   (SPR_XL),a
            ld   a,h
            ld   (SPR_XH),a
            ld   a,1
            ld   (SPR_EN),a
            jr   sm_n
sm_off:     xor  a
            ld   (SPR_EN),a
sm_n:       ld   a,(sm_c)
            inc  a
            ld   (sm_c),a
            cp   3
            jr   nz,sm_col
            ld   a,(sm_r)
            inc  a
            ld   (sm_r),a
            ld   hl,dr_rows
            cp   (hl)
            jr   nz,sm_row
            ret

; ---------------------------------------------------------------------
;  drag_follow: la esquina del monton sigue al cursor (con el desplazamiento del agarre), dentro de la pantalla.
;  Marca dr_moved cuando se ha alejado 3 pixeles o mas del origen.
; ---------------------------------------------------------------------
drag_follow:
            ld   a,(MP_X)
            ld   hl,dr_ox
            sub  (hl)
            jr   nc,df1
            xor  a
df1:        cp   233
            jr   c,df2
            ld   a,232
df2:        ld   (dr_px),a
            ld   a,(MP_Y)
            ld   hl,dr_oy
            sub  (hl)
            jr   nc,df3
            xor  a
df3:        cp   181
            jr   c,df4
            ld   a,180
df4:        ld   (dr_py),a
            ld   a,(dr_px)
            ld   hl,dr_x0
            sub  (hl)
            jr   nc,df5
            neg
df5:        cp   3
            jr   nc,df_mv
            ld   a,(dr_py)
            ld   hl,dr_y0
            sub  (hl)
            jr   nc,df6
            neg
df6:        cp   3
            jr   c,df_nm
df_mv:      ld   a,1
            ld   (dr_moved),a
df_nm:      jp   spr_move

; drag_loop: mientras se mantiene el boton izquierdo; al soltarlo, drop
drag_loop:  call waitvs
            call ml_poll
            call ml_cur_show
            call tick_clock
            ld   a,(ML_BTN)
            bit  0,a
            jr   z,dl_rel
            call drag_follow
            jr   drag_loop
dl_rel:     call drag_follow
            ; sigue en drop

; ---------------------------------------------------------------------
;  drop: se ha soltado el monton en (dr_px, dr_py). Sin movimiento = clic: vuelve sin deslizarse.
; ---------------------------------------------------------------------
drop:       ld   a,(dr_moved)
            or   a
            jp   z,cancel_now
            ld   a,(dr_cards)       ; la de arriba del monton: rango, palo y color
            ld   b,a
            and  0Fh
            ld   (dt_rank),a
            ld   a,b
            rrca
            rrca
            rrca
            rrca
            and  7
            ld   (dt_suit),a
            inc  a
            and  2
            ld   (dt_red),a         ; 2 = roja (corazones, diamantes), 0 = negra
            call find_target
            jr   c,jrfix1
            jp   cancel_slide
jrfix1:
            jp   commit

; find_target: CARRY = hay sitio legal; dr_tarea (1 columna, 2 fundacion) y dr_tk (columna o fundacion)
find_target:
            ld   a,(dr_py)
            add  a,16               ; centro de la carta de arriba
            cp   36
            jr   nc,ft_tab
            ld   a,(dr_m)           ; arriba: solo cartas sueltas, a las fundaciones (x >= 100)
            cp   1
            jr   nz,ft_no
            ld   a,(dr_px)
            add  a,12
            cp   100
            jr   c,ft_no
            call found_slot
            cp   0FFh
            jr   z,ft_no
            ld   (dr_tk),a
            ld   a,2
            ld   (dr_tarea),a
            scf
            ret
ft_tab:     ld   a,(dr_px)          ; columna mas cercana al centro: px/32
            rrca
            rrca
            rrca
            rrca
            rrca
            and  7
            cp   7
            jr   c,ft1
            ld   a,6
ft1:        ld   (dr_tk),a
            ld   b,a
            ld   a,(dr_src)
            cp   1
            jr   nz,ft_chk
            ld   a,(dr_idx)
            cp   b
            jr   z,ft_no            ; sobre su propia columna: no se mueve
ft_chk:     call can_place
            jr   nc,ft_no
            ld   a,1
            ld   (dr_tarea),a
            scf
            ret
ft_no:      or   a
            ret

; can_place: CARRY si dr_cards[0] cabe sobre la columna dr_tk (vacia: solo un Rey; si no, rango+1 y color contrario)
can_place:  ld   a,(dr_tk)
            ld   e,a
            ld   d,0
            ld   hl,tab_n
            add  hl,de
            ld   a,(hl)
            or   a
            jr   nz,cp_ne
            ld   a,(dt_rank)
            cp   12
            jr   nz,cp_no
            scf
            ret
cp_ne:      dec  a
            ld   (dt_n),a           ; indice de la de arriba
            ld   a,(dr_tk)
            call col_addr
            ld   a,(dt_n)
            ld   e,a
            ld   d,0
            add  hl,de
            ld   a,(hl)             ; la de arriba de la columna
            bit  7,a
            jr   z,cp_no
            ld   b,a
            and  0Fh
            ld   c,a
            ld   a,(dt_rank)
            inc  a
            cp   c
            jr   nz,cp_no
            ld   a,b
            rrca
            rrca
            rrca
            rrca
            and  7
            inc  a
            and  2
            ld   c,a
            ld   a,(dt_red)
            cp   c
            jr   z,cp_no            ; mismo color
            scf
            ret
cp_no:      or   a
            ret

; found_slot: A = fundacion donde cabe dr_cards[0] (un As, en la primera libre; el resto, sobre su palo y rango anterior) o 255
found_slot: ld   a,(dt_rank)
            or   a
            jr   nz,fs_nz
            ld   hl,found_n
            ld   b,4
            ld   c,0
fs_a:       ld   a,(hl)
            or   a
            jr   z,fs_ok
            inc  hl
            inc  c
            djnz fs_a
            ld   a,0FFh
            ret
fs_ok:      ld   a,c
            ret
fs_nz:      ld   c,0
fs_b:       ld   e,c
            ld   d,0
            ld   hl,found_n
            add  hl,de
            ld   a,(hl)
            ld   hl,dt_rank
            cp   (hl)
            jr   nz,fs_n
            ld   hl,found_s
            add  hl,de
            ld   a,(hl)
            ld   hl,dt_suit
            cp   (hl)
            jr   z,fs_ok
fs_n:       inc  c
            ld   a,c
            cp   4
            jr   nz,fs_b
            ld   a,0FFh
            ret

; ---------------------------------------------------------------------
;  commit: mueve el monton a su sitio en el modelo, destapa la nueva de arriba del origen y redibuja origen y destino
; ---------------------------------------------------------------------
commit:     ld   a,(dr_src)
            cp   1
            jr   nz,cm_tgt
            ld   a,(dr_idx)         ; origen = columna: la que queda arriba se destapa sola
            ld   e,a
            ld   d,0
            ld   hl,tab_n
            add  hl,de
            ld   a,(hl)
            or   a
            jr   z,cm_tgt
            dec  a
            ld   (dt_n),a           ; en memoria: col_addr usa C como variable de trabajo (lo pisa)
            ld   a,(dr_idx)
            call col_addr
            ld   a,(dt_n)
            ld   e,a
            ld   d,0
            add  hl,de
            bit  7,(hl)
            jr   nz,cm_tgt
            set  7,(hl)             ; destapar una carta: +5
            ld   hl,5
            call add_score
cm_tgt:     ld   a,(dr_tarea)
            cp   2
            jr   z,cm_found
            ld   a,(dr_tk)          ; destino = columna: se anaden las m cartas
            ld   e,a
            ld   d,0
            ld   hl,tab_n
            add  hl,de
            ld   a,(hl)
            ld   (dt_n),a
            ld   a,(dr_tk)
            call col_addr
            ld   a,(dt_n)
            ld   e,a
            ld   d,0
            add  hl,de
            ex   de,hl
            ld   hl,dr_cards
            ld   a,(dr_m)
            ld   c,a
            ld   b,0
            ldir
            ld   a,(dt_n)
            ld   hl,dr_m
            add  a,(hl)
            ld   c,a
            ld   a,(dr_tk)
            ld   e,a
            ld   d,0
            ld   hl,tab_n
            add  hl,de
            ld   (hl),c
            jr   cm_redraw
cm_found:   ld   a,(dr_tk)
            ld   e,a
            ld   d,0
            ld   hl,found_s
            add  hl,de
            ld   a,(dt_suit)
            ld   (hl),a
            ld   hl,found_n
            add  hl,de
            inc  (hl)
cm_redraw:  call score_move
            call redraw_src
            ld   a,(dr_tarea)
            cp   2
            jr   z,cm_rf
            ld   a,(dr_tk)
            call draw_col
            jr   cm_end
cm_rf:      call draw_found
cm_end:     jp   spr_hide_all

; redraw_src: redibuja el origen del monton (columna, descarte o fundaciones)
redraw_src: ld   a,(dr_src)
            cp   1
            jr   nz,rs2
            ld   a,(dr_idx)
            jp   draw_col
rs2:        cp   2
            jp   z,draw_waste
            jp   draw_found

; cancel_slide: el monton vuelve a su sitio deslizandose (4 imagenes, cada una la mitad de lo que falta) y se deshace el arrastre
cancel_slide:
            ld   b,4
cs_sl:      push bc
            call waitvs
            ld   a,(dr_px)
            ld   hl,dr_x0
            call half_to
            ld   (dr_px),a
            ld   a,(dr_py)
            ld   hl,dr_y0
            call half_to
            ld   (dr_py),a
            call spr_move
            pop  bc
            djnz cs_sl

; cancel_now: el modelo vuelve a como estaba y se redibuja el origen
cancel_now: ld   a,(dr_src)
            cp   1
            jr   nz,cn2
            ld   a,(dr_idx)
            ld   e,a
            ld   d,0
            ld   hl,tab_n
            add  hl,de
            ld   a,(dr_n0)
            ld   (hl),a
            jr   cn_draw
cn2:        cp   2
            jr   nz,cn3
            ld   hl,waste_n
            inc  (hl)
            ld   a,(dr_vis0)
            ld   (waste_vis),a
            jr   cn_draw
cn3:        ld   a,(dr_idx)
            ld   e,a
            ld   d,0
            ld   hl,found_n
            add  hl,de
            inc  (hl)
cn_draw:    call redraw_src
            jp   spr_hide_all

; half_to: A = valor, (HL) = destino -> A + (destino - A)/2 (con signo)
half_to:    ld   e,a
            ld   d,0
            ld   l,(hl)
            ld   h,0
            or   a
            sbc  hl,de
            sra  h
            rr   l
            add  hl,de
            ld   a,l
            ret


; ---------------------------------------------------------------------
;  FASE 4: doble clic / clic derecho a la fundacion, puntos y tiempo, botones de la barra
; ---------------------------------------------------------------------
; new_deal: partida nueva (reparto, puntos y tiempo a cero) y repinta mesa y barra
new_deal:   call new_game
            ld   hl,0
            ld   (score),hl
            ld   (tm_sec),hl
            xor  a
            ld   (tm_run),a
            ld   (tm_fr),a
            ld   (tm_s10),a
            ld   (auto_on),a
            ld   (win_on),a
            ld   hl,(ML_CLOCK)
            ld   (tm_last),hl
            call draw_board
            jp   bar_update

start_clock:
            ld   a,1
            ld   (tm_run),a
            ret

; add_score / sub_score: HL = puntos (sin pasar de 9999 ni bajar de 0)
add_score:  ld   de,(score)
            add  hl,de
            ld   de,10000
            or   a
            sbc  hl,de
            jr   c,as1
            ld   hl,9999
            jr   as2
as1:        add  hl,de
as2:        ld   (score),hl
            ld   a,1
            ld   (bar_dirty),a
            ret
sub_score:  ex   de,hl
            ld   hl,(score)
            or   a
            sbc  hl,de
            jr   nc,ss1
            ld   hl,0
ss1:        ld   (score),hl
            ld   a,1
            ld   (bar_dirty),a
            ret

; score_move: puntos de un movimiento ya hecho en el modelo (dr_src -> dr_tarea)
score_move: call start_clock
            ld   a,(dr_tarea)
            cp   2
            jr   z,sv_f
            ld   a,(dr_src)
            cp   2
            jr   z,sv_p5
            cp   3
            ret  nz                 ; columna a columna: 0
            ld   hl,15              ; fundacion a columna: -15
            jp   sub_score
sv_p5:      ld   hl,5               ; descarte a columna: +5
            jp   add_score
sv_f:       ld   a,(dr_src)
            cp   3
            ret  z                  ; fundacion a fundacion: 0
            ld   hl,10              ; a la fundacion: +10
            jp   add_score

; tick_clock: una vez por vuelta; acumula las imagenes (FRAMES, suba o baje) mientras corre el reloj; 50 = 1 s;
; cada 10 s, -2 puntos
tick_clock: ld   hl,(ML_CLOCK)
            ld   de,(tm_last)
            ld   (tm_last),hl
            ld   a,(tm_run)
            or   a
            ret  z
            or   a
            sbc  hl,de
            jp   p,tc1
            ld   a,l
            cpl
            ld   l,a
            ld   a,h
            cpl
            ld   h,a
            inc  hl
tc1:        ld   a,h
            or   a
            ret  nz
            ld   a,l
            cp   51
            jr   c,tc2
            ld   a,50
tc2:        ld   hl,tm_fr
            add  a,(hl)
tc3:        cp   50
            jr   c,tc4
            sub  50
            push af
            call tick_sec
            pop  af
            jr   tc3
tc4:        ld   (tm_fr),a
            ret
tick_sec:   ld   hl,(tm_sec)
            inc  hl
            ld   (tm_sec),hl
            ld   a,1
            ld   (bar_dirty),a
            ld   hl,tm_s10
            inc  (hl)
            ld   a,(hl)
            cp   10
            ret  c
            ld   (hl),0
            ld   hl,2
            jp   sub_score

; bar_update: pone la puntuacion y el tiempo (mm:ss) en el texto de la barra y la repinta
bar_update: xor  a
            ld   (bar_dirty),a
            ld   hl,(score)
            ld   de,bar_txt+6
            call num4
            ld   hl,(tm_sec)
            ld   c,0
bu_m:       ld   a,h
            or   a
            jr   nz,bu_sub
            ld   a,l
            cp   60
            jr   c,bu_done
bu_sub:     ld   de,60
            or   a
            sbc  hl,de
            inc  c
            jr   bu_m
bu_done:    ld   a,l
            ld   (tm_tmp),a
            ld   hl,bar_txt+12
            ld   a,c
            cp   100
            jr   c,bu_mm
            ld   a,99
bu_mm:      call two
            inc  hl
            ld   a,(tm_tmp)
            call two
            jp   draw_bar

; num4: HL (0-9999) -> 4 cifras ASCII en (DE)
num4:       ld   bc,-1000
            call n4d
            ld   bc,-100
            call n4d
            ld   bc,-10
            call n4d
            ld   a,l
            add  a,'0'
            ld   (de),a
            ret
n4d:        ld   a,'0'-1
n4l:        inc  a
            add  hl,bc
            jr   c,n4l
            sbc  hl,bc
            ld   (de),a
            inc  de
            ret

; two: A (0-99) -> 2 cifras ASCII en (HL), HL avanza
two:        ld   b,'0'
t2l:        cp   10
            jr   c,t2d
            sub  10
            inc  b
            jr   t2l
t2d:        add  a,'0'
            ld   (hl),b
            inc  hl
            ld   (hl),a
            inc  hl
            ret

; bar_click: clic izquierdo en la barra segun MP_X: NEW (152-175), D3 (184-199), QUIT (208-239)
bar_click:  ld   a,(MP_X)
            cp   152
            ret  c
            cp   176
            jp   c,new_deal
            cp   184
            ret  c
            cp   200
            jp   c,toggle_mode
            cp   208
            ret  c
            cp   240
            ret  nc
            pop  hl                 ; QUIT: descarta el retorno y sale
            jp   quit

; quick_found: clic derecho o doble clic sobre la carta de arriba de una columna o del descarte: a la fundacion si cabe
quick_found:
            ld   a,(ht_area)
            cp   AREA_TAB
            jr   z,qf_tab
            cp   AREA_WASTE
            jr   z,qf_waste
            ret
qf_tab:     ld   a,(ht_card)
            cp   0FFh
            ret  z
            ld   (dr_i),a
            ld   a,(ht_idx)
            ld   (dr_idx),a
            ld   e,a
            ld   d,0
            ld   hl,tab_n
            add  hl,de
            ld   a,(hl)
            dec  a
            ld   hl,dr_i
            cp   (hl)
            ret  nz                 ; solo la de arriba
            ld   a,(dr_idx)
            call col_addr
            ld   a,(dr_i)
            ld   e,a
            ld   d,0
            add  hl,de
            ld   a,(hl)
            bit  7,a
            ret  z
            ld   (dr_cards),a
            ld   a,1
            ld   (dr_src),a
            jr   qf_go
qf_waste:   ld   a,(waste_n)
            or   a
            ret  z
            ld   e,a
            dec  e
            ld   d,0
            ld   hl,waste
            add  hl,de
            ld   a,(hl)
            ld   (dr_cards),a
            ld   a,2
            ld   (dr_src),a
            xor  a
            ld   (dr_idx),a
qf_go:      ld   a,(dr_cards)
            ld   b,a
            and  0Fh
            ld   (dt_rank),a
            ld   a,b
            rrca
            rrca
            rrca
            rrca
            and  7
            ld   (dt_suit),a
            call found_slot
            cp   0FFh
            ret  z
            ld   (dr_tk),a
            ld   a,(dr_src)         ; se quita del modelo, como en pickup
            cp   1
            jr   nz,qf_w
            ld   a,(dr_idx)
            ld   e,a
            ld   d,0
            ld   hl,tab_n
            add  hl,de
            dec  (hl)
            jr   qf_c
qf_w:       ld   hl,waste_n
            dec  (hl)
            ld   a,(waste_vis)
            dec  a
            jr   nz,qf_w1
            ld   a,(waste_n)
            or   a
            ld   a,0
            jr   z,qf_w1
            inc  a
qf_w1:      ld   (waste_vis),a
qf_c:       ld   a,2
            ld   (dr_tarea),a
            ld   a,1
            ld   (dr_m),a
            jp   commit

; ---------------------------------------------------------------------
;  FASE 5: autocompletar (mazo y descarte vacios, todo el tablero boca arriba) y animacion de victoria
; ---------------------------------------------------------------------
; auto_check: si mazo, descarte y las 7 columnas (de arriba) estan todos boca arriba o vacias, activa auto_on
auto_check: ld   a,(stock_n)
            or   a
            ret  nz
            ld   a,(waste_n)
            or   a
            ret  nz
            xor  a
            ld   (at_col),a
ac_lp:      ld   a,(at_col)
            ld   e,a
            ld   d,0
            ld   hl,tab_n
            add  hl,de
            ld   a,(hl)
            or   a
            jr   z,ac_next          ; columna vacia: cuenta como lista
            dec  a
            ld   (dt_n),a           ; en memoria: col_addr usa C como variable de trabajo
            ld   a,(at_col)
            call col_addr
            ld   a,(dt_n)
            ld   e,a
            ld   d,0
            add  hl,de
            bit  7,(hl)
            ret  z                  ; hay una tapada: aun no
ac_next:    ld   a,(at_col)
            inc  a
            ld   (at_col),a
            cp   7
            jr   c,ac_lp
            ld   a,1
            ld   (auto_on),a
            ret

; auto_step: mueve a su fundacion la primera carta de arriba (columnas 0..6) que encuentre que le quepa, deslizandola
; con sprites; si ninguna columna tiene ya una que mover, el tablero esta vacio: empieza la animacion de victoria.
auto_step:  xor  a
            ld   (at_col),a
as_lp:      ld   a,(at_col)
            ld   e,a
            ld   d,0
            ld   hl,tab_n
            add  hl,de
            ld   a,(hl)
            or   a
            jr   nz,as_body
            jp   as_next
as_body:    dec  a                  ; indice de la de arriba: se la "pide" a pickup como si fuera un clic
            ld   (ht_card),a
            ld   a,(at_col)
            ld   (ht_idx),a
            ld   a,AREA_TAB
            ld   (ht_area),a
            call pickup
            jr   c,as_body2
            jp   as_next
as_body2:   ld   a,(dr_cards)
            ld   b,a
            and  0Fh
            ld   (dt_rank),a
            ld   a,b
            rrca
            rrca
            rrca
            rrca
            and  7
            ld   (dt_suit),a
            call found_slot
            cp   0FFh
            jr   z,as_cancel
            ld   (dr_tk),a
            ld   a,(dr_tk)          ; destino: x0 = 112 + 32*fundacion, y = TOP_Y
            add  a,a
            add  a,a
            add  a,a
            add  a,a
            add  a,a
            add  a,112
            ld   (at_tx),a
            ld   a,TOP_Y
            ld   (at_ty),a
            ld   b,8
as_sl:      push bc
            call waitvs
            ld   a,(dr_px)
            ld   hl,at_tx
            call half_to
            ld   (dr_px),a
            ld   a,(dr_py)
            ld   hl,at_ty
            call half_to
            ld   (dr_py),a
            call spr_move
            pop  bc
            djnz as_sl
            ld   a,(at_tx)
            ld   (dr_px),a
            ld   a,(at_ty)
            ld   (dr_py),a
            call spr_move
            ld   a,(dr_tk)
            ld   e,a
            ld   d,0
            ld   hl,found_s
            add  hl,de
            ld   a,(dt_suit)
            ld   (hl),a
            ld   a,(dr_tk)
            ld   e,a
            ld   d,0
            ld   hl,found_n
            add  hl,de
            inc  (hl)
            ld   a,2
            ld   (dr_tarea),a
            call score_move
            call draw_found
            call spr_hide_all
            ret
as_cancel:  call cancel_now
as_next:    ld   a,(at_col)
            inc  a
            ld   (at_col),a
            cp   7
            jr   nc,as_done
            jp   as_lp
as_done:    xor  a                  ; ninguna columna tenia una carta que mover: tablero vacio, a celebrar
            ld   (auto_on),a
            ld   a,1
            ld   (win_on),a
            xor  a
            ld   (wn_suit),a
            ret

; win_step: saca la carta de arriba de la siguiente fundacion no vacia (de la K al As) y la deja caer con rebote,
; dejando estela; A=1 si ha animado una carta (seguir llamando), A=0 si no quedaba ninguna o el usuario corto con
; un clic o una tecla (entonces toca repartir de nuevo).
win_step:   ld   a,(wn_suit)
ws_loop:    cp   4
            jr   nc,ws_none
            ld   e,a
            ld   d,0
            ld   hl,found_n
            add  hl,de
            ld   a,(hl)
            or   a
            jr   nz,ws_pop
            ld   a,(wn_suit)
            inc  a
            ld   (wn_suit),a
            jr   ws_loop
ws_none:    xor  a
            ret
ws_pop:     dec  a
            ld   c,a                ; rango de la carta de arriba
            ld   a,(wn_suit)
            ld   e,a
            ld   d,0
            ld   hl,found_s
            add  hl,de
            ld   a,(hl)
            rlca
            rlca
            rlca
            rlca
            or   c
            or   80h
            ld   (dr_cards),a
            ld   a,1
            ld   (dr_m),a
            ld   a,UP_STEP
            ld   (dr_step),a
            ld   a,(wn_suit)
            add  a,a
            add  a,a
            add  a,a
            add  a,a
            add  a,112
            ld   (dr_px),a
            ld   a,TOP_Y
            ld   (dr_py),a
            call spr_compose
            call spr_load_all
            call spr_move
            ld   a,(wn_suit)
            ld   e,a
            ld   d,0
            ld   hl,found_n
            add  hl,de
            dec  (hl)
            call draw_found
            ld   a,(dr_cards)      ; color de la estela segun el palo
            rrca
            rrca
            rrca
            rrca
            and  7
            ld   e,a
            ld   d,0
            ld   hl,win_trail_col
            add  hl,de
            ld   a,(hl)
            ld   (wn_tr),a
            xor  a
            ld   (wn_vy),a
            call rnd16              ; deriva horizontal de la caida: 8 px a la izquierda o a la derecha, al azar
            ld   a,h
            and  1
            ld   a,8
            jr   nz,wn_d1
            ld   a,0F8h
wn_d1:      ld   (wn_dir),a
            ld   a,200              ; tope de seguridad: normalmente sale antes por un lado
            ld   (wn_t),a
wn_fall:    ld   a,(wn_t)
            or   a
            jr   z,wn_settled
            dec  a
            ld   (wn_t),a
            call waitvs
            call win_check_abort
            jr   c,wn_abort
            call win_mark
            ld   a,(wn_vy)
            inc  a
            ld   (wn_vy),a
            ld   b,a
            ld   a,(dr_py)
            add  a,b
            cp   FLOOR_Y+1
            jr   nc,wn_bounce
            ld   (dr_py),a
            jr   wn_mvx
wn_bounce:  ld   a,FLOOR_Y
            ld   (dr_py),a
            ld   a,(wn_vy)
            neg
            sra  a
            ld   (wn_vy),a
wn_mvx:     ld   a,(wn_t)
            and  1                  ; 8 px cada dos imagenes
            jr   nz,wn_sk
            ld   a,(dr_px)
            ld   b,a
            ld   a,(wn_dir)
            add  a,b
            cp   233
            jr   nc,wn_settled      ; salio por un lado (tambien si da la vuelta por debajo de 0): la siguiente
            ld   (dr_px),a
wn_sk:      call spr_move
            jr   wn_fall
wn_settled: call spr_hide_all
            ld   a,1
            ret
wn_abort:   call spr_hide_all
            xor  a
            ret

; win_check_abort: CARRY si hay un clic o se ha pulsado cualquier tecla
win_check_abort:
            ld   a,(ML_PRESS)
            and  3
            jr   nz,wca_yes
            xor  a
            in   a,(0FEh)
            cpl
            and  1Fh
            jr   z,wca_no
wca_yes:    scf
            ret
wca_no:     or   a
            ret

; win_mark: dibuja en el bitmap, en (dr_px, dr_py), la carta entera (dr_cards): al repetirlo en cada fotograma queda la estela en
; cascada de Windows. x es multiplo de 8 (un byte); y <= FLOOR_Y para no pisar la barra.
win_mark:   ld   a,(dr_px)
            rrca
            rrca
            rrca
            and  1Fh
            cp   30
            ret  nc                 ; no cabe una carta de 3 bytes
            ld   d,a
            ld   a,(dr_py)
            ld   e,a
            ld   b,32
            ld   a,(dr_cards)
            jp   place_card

; win_trail_col: atributo de la estela por palo (picas, corazones, diamantes, treboles): color en el papel (bitmap a 0)
win_trail_col:
            defb 0F0h,0A0h,0A0h,0F0h

; ---------------------------------------------------------------------
;  Barra de estado (lineas 184-191): texto de 8x8 con la fuente de la ROM, atributo uniforme por celda.
;  bar_data: columna, atributo, texto ASCII, 0 ... y 0FFh al final.
; ---------------------------------------------------------------------
draw_bar:   ld   hl,BM+BAR_Y*32     ; fondo: bitmap 0 y papel negro / tinta blanca
            ld   (hl),0
            ld   de,BM+BAR_Y*32+1
            ld   bc,255
            ldir
            ld   hl,A0+BAR_Y*32
            ld   (hl),0Fh
            ld   de,A0+BAR_Y*32+1
            ld   bc,255
            ldir
            ld   hl,bar_data
db_lp:      ld   a,(hl)
            cp   0FFh
            ret  z
            ld   e,a                ; columna
            ld   d,0
            inc  hl
            ld   a,(hl)
            ld   (pc_attr),a
            inc  hl
            push hl
            ld   hl,BM+BAR_Y*32
            add  hl,de
            ex   de,hl              ; DE = bitmap del primer caracter
            pop  hl
db_ch:      ld   a,(hl)
            inc  hl
            or   a
            jr   z,db_lp
            push hl
            push de
            call zxcode
            call putch
            pop  de
            pop  hl
            inc  de
            jr   db_ch

; putch: A = codigo ZX81, DE = bitmap de la celda (linea 0); pinta las 8 lineas y el atributo (pc_attr)
putch:      ld   l,a
            ld   h,0
            add  hl,hl
            add  hl,hl
            add  hl,hl
            ld   bc,FONT
            add  hl,bc              ; HL = dibujo en la ROM
            push de
            ld   b,8
pc1:        ld   a,(hl)
            ld   (de),a
            inc  hl
            ld   a,e
            add  a,32
            ld   e,a
            jr   nc,pc2
            inc  d
pc2:        djnz pc1
            pop  de
            ld   a,d
            add  a,ATTOFS/256
            ld   d,a
            ld   a,(pc_attr)
            ld   c,a
            ld   b,8
pc3:        ld   a,c
            ld   (de),a
            ld   a,e
            add  a,32
            ld   e,a
            jr   nc,pc4
            inc  d
pc4:        djnz pc3
            ret

; zxcode: ASCII (mayusculas, cifras, espacio, '.' y ':') -> codigo de caracter del ZX81
zxcode:     cp   'A'
            jr   c,zx1
            sub  'A'-38
            ret
zx1:        cp   ':'
            jr   z,zx_colon
            cp   '9'+1
            jr   nc,zx_q
            cp   '0'
            jr   c,zx2
            sub  '0'-28
            ret
zx2:        cp   '.'
            jr   z,zx_dot
            cp   ' '
            jr   z,zx_sp
zx_q:       ld   a,15               ; '?'
            ret
zx_colon:   ld   a,14
            ret
zx_dot:     ld   a,27
            ret
zx_sp:      xor  a
            ret

bar_data:   defb 0,0Fh
bar_txt:    defm "SCORE 0000  00:00"
            defb 0
            defb 19,0F0h            ; botones: tinta negra sobre blanco brillante
            defm "NEW"
            defb 0
            defb 23,0F0h
            defm "D"
bar_dn:     defm "3"
            defb 0
            defb 26,0F0h
            defm "QUIT"
            defb 0
            defb 0FFh


            if TEST_KEYS
; test_win: deja las 4 fundaciones completas y el tablero vacio, y arranca la animacion de victoria
test_win:   xor  a
            in   a,(0FEh)           ; espera a que se suelten las teclas (la animacion se corta con cualquiera)
            cpl
            and  1Fh
            jr   nz,test_win
            ld   hl,tab_n
            ld   b,7
tw1:        ld   (hl),0
            inc  hl
            djnz tw1
            xor  a
            ld   (stock_n),a
            ld   (waste_n),a
            ld   (waste_vis),a
            ld   hl,found_n
            ld   de,found_s
            ld   b,4
            ld   c,0
tw2:        ld   (hl),13
            ld   a,c
            ld   (de),a
            inc  hl
            inc  de
            inc  c
            djnz tw2
            call draw_board
            xor  a
            ld   (auto_on),a
            ld   (wn_suit),a
            inc  a
            ld   (win_on),a
            ret
            endif

            include "cards.inc"
            include "mouselib.inc"

            end start
