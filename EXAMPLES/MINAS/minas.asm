; =====================================================================
;  MINAS.ASM - buscaminas con el raton Bluetooth del SD81 Booster, TODO en codigo maquina.
;
;  Tablero de 16 x 16 casillas con 40 minas, en el modo de texto normal del ZX81 (una casilla = un caracter de 8x8 pixeles).
;    boton izquierdo: destapa la casilla (el primer clic nunca cae en una mina ni a su lado)
;    boton derecho:   pone o quita la bandera
;    boton NEW:       empieza otra partida          cualquier tecla: sale al BASIC
;  El cursor es el sprite por hardware de mouselib.inc.
;
;  Requisitos: modo FAST (sin NMI: en SLOW la pantalla parpadeaba) y el D-FILE expandido (32 caracteres por linea; los prepara MINAS.B81).
;  Como en FAST la ROM no explora el teclado, la tecla de salida se lee directamente del puerto $FE.  Entrada: RAND USR 24576
;  Memoria:  $6000- programa y mouselib;  $7C00- buzon (28 bytes);  $7D00 minas y cuentas;  $7E00 estado;  $7F00 cola de la apertura en cascada
;  Ensamblar: pasmo -I ..\MOUSE minas.asm minas.bin minas.sym   (compila.bat)
; =====================================================================

MS_BASE     equ 7C00h
ML_CLOCK    equ 4034h               ; FRAMES: reloj del doble clic (en FAST lo simula la FPGA); el Buscaminas no usa el doble clic
D_FILE      equ 400Ch

BOARD       equ 7D00h               ; por casilla: bit 7 = mina, bits 0-3 = minas vecinas
STATE       equ 7E00h               ; por casilla: 0 tapada, 1 abierta, 2 bandera
QUEUE       equ 7F00h               ; cola circular de casillas por expandir

MINES       equ 40
GRID_ROW    equ 2                   ; fila y columna de texto de la primera casilla
GRID_COL    equ 8

CH_HIDDEN   equ 08h                 ; gris
CH_FLAG     equ 0ABh                ; 'F' inverso
CH_MINE     equ 097h                ; '*' inverso

            org 24576

start:      xor  a                  ; espera a que se suelte la tecla con la que se lanzo (A = 0: se leen las 8 filas a la vez)
            in   a,(0FEh)
            cpl
            and  1Fh
            jr   nz,start

            ld   bc,175*256+255     ; 22 filas de texto: Y maximo 175
            call ml_init
            call ml_cur_init
            call new_game

loop:       call ml_poll
            call ml_cur_show

            ld   a,(ML_PRESS)
            and  3                  ; izquierdo o derecho recien pulsado
            jr   z,keycheck

            ld   bc,144*256+152     ; boton NEW: x 144..183, y 152..159
            ld   de,184*256+160
            call ml_hit
            jr   nc,notnew
            ld   a,(ML_PRESS)
            rra
            jr   nc,keycheck
            call new_game
            jr   keycheck

notnew:     ld   a,(game)
            or   a
            jr   nz,keycheck        ; partida terminada: solo NEW
            ld   bc,64*256+16       ; el tablero: x 64..191, y 16..143
            ld   de,192*256+144
            call ml_hit
            jr   nc,keycheck
            ld   b,3
            call ml_cell            ; H = columna de texto, L = fila de texto
            ld   a,l
            sub  GRID_ROW
            add  a,a
            add  a,a
            add  a,a
            add  a,a                ; fila * 16
            ld   c,a
            ld   a,h
            sub  GRID_COL
            or   c                  ; A = casilla
            ld   c,a
            ld   a,(ML_PRESS)
            rra
            ld   a,c
            jr   nc,rightclk
            call do_open
            jr   keycheck
rightclk:   call do_flag

keycheck:   xor  a                  ; cualquier tecla sale
            in   a,(0FEh)
            cpl
            and  1Fh
            jp   z,loop

            call ml_cur_hide
            ld   bc,191*256+255     ; devuelve al MCU el rango por defecto
            call ml_init
            ld   bc,0
            ret

; ---------------------------------------------------------------------
;  new_game: tablero tapado, contadores a cero, textos.
; ---------------------------------------------------------------------
new_game:   ld   hl,BOARD
            ld   (hl),0
            ld   de,BOARD+1
            ld   bc,511             ; BOARD y STATE a cero
            ldir
            xor  a
            ld   (game),a
            ld   (flags),a
            ld   (opened),a
            ld   (iter),a
            inc  a
            ld   (first),a
ng_lp:      ld   a,(iter)
            call draw_cell
            ld   a,(iter)
            inc  a
            ld   (iter),a
            jr   nz,ng_lp

            ld   b,0
            ld   c,11
            call scr_addr
            ex   de,hl
            ld   hl,txt_title
            call puts
            ld   b,19
            ld   c,8
            call scr_addr
            ex   de,hl
            ld   hl,txt_minas
            call puts
            ld   b,19
            ld   c,18
            call scr_addr
            ex   de,hl
            ld   hl,btn_new
            ld   bc,5
            ldir
            call upd_cnt
            ld   hl,msg_play
            jp   status

; ---------------------------------------------------------------------
;  do_open: A = casilla. Destapa; si no es de nadie la mina, se expande por las vacias.
; ---------------------------------------------------------------------
do_open:    ld   (op_idx),a
            ld   a,(first)
            or   a
            jr   z,do_go
            xor  a
            ld   (first),a
            ld   a,(op_idx)
            call place_mines
do_go:      ld   a,(op_idx)
            ld   e,a
            ld   d,0
            ld   hl,STATE
            add  hl,de
            ld   a,(hl)
            or   a
            ret  nz                 ; ya abierta o con bandera
            ld   hl,BOARD
            add  hl,de
            bit  7,(hl)
            jp   nz,lose
            xor  a
            ld   (q_head),a
            ld   (q_tail),a
            ld   a,(op_idx)
            call visit
            call fl_main
            ld   a,(opened)
            cp   256-MINES
            ret  nz
            jp   win

; ---------------------------------------------------------------------
;  do_flag: A = casilla. Alterna la bandera en una casilla tapada.
; ---------------------------------------------------------------------
do_flag:    ld   e,a
            ld   d,0
            ld   hl,STATE
            add  hl,de
            ld   a,(hl)
            cp   1
            ret  z                  ; abierta
            or   a
            jr   nz,df_off
            ld   (hl),2
            ld   a,(flags)
            inc  a
            jr   df_set
df_off:     ld   (hl),0
            ld   a,(flags)
            dec  a
df_set:     ld   (flags),a
            ld   a,e
            call draw_cell
            jp   upd_cnt

; ---------------------------------------------------------------------
;  visit: A = casilla. Si esta tapada la abre, la pinta y, si no tiene minas vecinas, la pone en la cola.
; ---------------------------------------------------------------------
visit:      ld   e,a
            ld   d,0
            ld   hl,STATE
            add  hl,de
            ld   a,(hl)
            or   a
            ret  nz
            ld   (hl),1
            ld   a,(opened)
            inc  a
            ld   (opened),a
            push de
            ld   a,e
            call draw_cell
            pop  de
            ld   hl,BOARD
            add  hl,de
            ld   a,(hl)
            and  0Fh
            ret  nz
            ld   a,e
enq:        ld   c,a
            ld   a,(q_tail)
            ld   e,a
            ld   d,QUEUE/256
            ld   a,c
            ld   (de),a
            ld   a,e
            inc  a
            ld   (q_tail),a
            ret

; fl_main: vacia la cola; de cada casilla sacada visita sus ocho vecinas
fl_main:    ld   a,(q_head)
            ld   hl,q_tail
            cp   (hl)
            ret  z
            ld   e,a
            ld   d,QUEUE/256
            inc  a
            ld   (q_head),a
            ld   a,(de)             ; A = casilla
            ld   c,a
            rrca
            rrca
            rrca
            rrca
            and  0Fh
            ld   d,a                ; D = fila
            ld   a,c
            and  0Fh
            ld   e,a                ; E = columna
            ld   h,0FFh             ; H = desplazamiento de fila: -1, 0, 1
fl_r:       ld   a,d
            add  a,h
            cp   16
            jr   nc,fl_rn           ; fuera del tablero (los negativos dan >= 16)
            add  a,a
            add  a,a
            add  a,a
            add  a,a
            ld   l,a                ; L = fila vecina * 16
            ld   c,0FFh             ; C = desplazamiento de columna
fl_c:       ld   a,e
            add  a,c
            cp   16
            jr   nc,fl_cn
            or   l
            push bc
            push de
            push hl
            call visit
            pop  hl
            pop  de
            pop  bc
fl_cn:      inc  c
            ld   a,c
            cp   2
            jr   nz,fl_c
fl_rn:      inc  h
            ld   a,h
            cp   2
            jr   nz,fl_r
            jr   fl_main

; ---------------------------------------------------------------------
;  place_mines: A = casilla del primer clic. Reparte las minas (no en ella ni en sus vecinas) y calcula las cuentas.
; ---------------------------------------------------------------------
place_mines:
            ld   (pm_click),a
            ld   hl,(FRAMES)        ; semilla: el reloj de fotogramas, el refresco, y la posicion del puntero
            ld   a,r
            xor  h
            ld   h,a
            ld   a,(MP_X)
            xor  l
            ld   l,a
            ld   a,(MP_Y)
            rlca
            rlca
            xor  h
            ld   h,a
            ld   a,l
            or   1                  ; la semilla no puede ser 0
            ld   l,a
            ld   (seed),hl
            ld   a,MINES
            ld   (pm_cnt),a
pm_loop:    call xrnd
            ld   a,l
            ld   (pm_c),a
            and  0Fh
            ld   d,a                ; columna candidata
            ld   a,(pm_click)
            and  0Fh
            sub  d
            inc  a
            cp   3
            jr   nc,pm_ok           ; lejos en horizontal
            ld   a,(pm_c)
            rrca
            rrca
            rrca
            rrca
            and  0Fh
            ld   d,a                ; fila candidata
            ld   a,(pm_click)
            rrca
            rrca
            rrca
            rrca
            and  0Fh
            sub  d
            inc  a
            cp   3
            jr   c,pm_loop          ; en la casilla del clic o pegada a ella
pm_ok:      ld   a,(pm_c)
            ld   e,a
            ld   d,0
            ld   hl,BOARD
            add  hl,de
            bit  7,(hl)
            jr   nz,pm_loop
            set  7,(hl)
            ld   a,(pm_cnt)
            dec  a
            ld   (pm_cnt),a
            jr   nz,pm_loop

            xor  a                  ; cuentas de minas vecinas
            ld   (iter),a
pm_cn:      ld   a,(iter)
            ld   e,a
            ld   d,0
            ld   hl,BOARD
            add  hl,de
            bit  7,(hl)
            jr   nz,pm_nx
            ld   a,e
            call count_nb
            ld   e,a                ; count_nb conserva HL
            ld   (hl),e
pm_nx:      ld   a,(iter)
            inc  a
            ld   (iter),a
            jr   nz,pm_cn
            ret

; count_nb: A = casilla. Devuelve en A las minas de sus vecinas. Conserva HL.
count_nb:   push hl
            ld   c,a
            ld   b,0                ; B = cuenta
            rrca
            rrca
            rrca
            rrca
            and  0Fh
            ld   d,a                ; fila
            ld   a,c
            and  0Fh
            ld   e,a                ; columna
            ld   h,0FFh
cn_r:       ld   a,d
            add  a,h
            cp   16
            jr   nc,cn_rn
            add  a,a
            add  a,a
            add  a,a
            add  a,a
            ld   l,a
            ld   c,0FFh
cn_c:       ld   a,e
            add  a,c
            cp   16
            jr   nc,cn_cn
            or   l
            push hl
            ld   l,a
            ld   h,BOARD/256
            bit  7,(hl)
            pop  hl
            jr   z,cn_cn
            inc  b
cn_cn:      inc  c
            ld   a,c
            cp   2
            jr   nz,cn_c
cn_rn:      inc  h
            ld   a,h
            cp   2
            jr   nz,cn_r
            ld   a,b
            pop  hl
            ret

; xrnd: xorshift de 16 bits. HL = numero aleatorio (la semilla no puede ser 0)
xrnd:       ld   hl,(seed)
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

; ---------------------------------------------------------------------
;  Fin de partida
; ---------------------------------------------------------------------
lose:       ld   a,1
            ld   (game),a
            xor  a
            ld   (iter),a
ls_lp:      ld   a,(iter)
            ld   e,a
            ld   d,0
            ld   hl,BOARD
            add  hl,de
            bit  7,(hl)
            jr   z,ls_nx
            ld   hl,STATE
            add  hl,de
            ld   a,(hl)
            cp   2                  ; las banderas puestas en minas se quedan
            jr   z,ls_nx
            ld   (hl),1
            ld   a,e
            call draw_cell
ls_nx:      ld   a,(iter)
            inc  a
            ld   (iter),a
            jr   nz,ls_lp
            ld   hl,msg_lose
            jp   status

win:        ld   a,2
            ld   (game),a
            ld   a,MINES
            ld   (flags),a
            xor  a
            ld   (iter),a
wn_lp:      ld   a,(iter)
            ld   e,a
            ld   d,0
            ld   hl,BOARD
            add  hl,de
            bit  7,(hl)
            jr   z,wn_nx
            ld   hl,STATE
            add  hl,de
            ld   (hl),2
            ld   a,e
            call draw_cell
wn_nx:      ld   a,(iter)
            inc  a
            ld   (iter),a
            jr   nz,wn_lp
            call upd_cnt
            ld   hl,msg_win
            jp   status

; ---------------------------------------------------------------------
;  Pantalla
; ---------------------------------------------------------------------
; scr_addr: B = fila, C = columna de texto -> HL = direccion en el D-FILE. Destruye DE.
scr_addr:   ld   l,b
            ld   h,0
            ld   d,h
            ld   e,l
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,de              ; fila * 33
            ld   e,c
            ld   d,0
            add  hl,de
            ld   de,(D_FILE)
            inc  de                 ; salta el primer NEWLINE
            add  hl,de
            ret

; draw_cell: A = casilla. La pinta segun su estado. Destruye todo salvo IX e IY.
draw_cell:  push af
            ld   c,a
            rrca
            rrca
            rrca
            rrca
            and  0Fh
            add  a,GRID_ROW
            ld   b,a
            ld   a,c
            and  0Fh
            add  a,GRID_COL
            ld   c,a
            call scr_addr
            pop  af
            ld   e,a
            ld   d,0
            push hl
            ld   hl,STATE
            add  hl,de
            ld   a,(hl)
            ld   hl,BOARD
            add  hl,de
            ld   b,(hl)
            pop  hl
            or   a
            jr   nz,dc_s1
            ld   a,CH_HIDDEN
            jr   dc_put
dc_s1:      cp   2
            jr   nz,dc_open
            ld   a,CH_FLAG
            jr   dc_put
dc_open:    bit  7,b
            jr   z,dc_num
            ld   a,CH_MINE
            jr   dc_put
dc_num:     ld   a,b
            and  0Fh
            jr   z,dc_put           ; sin vecinas: en blanco
            add  a,28               ; digito
dc_put:     ld   (hl),a
            ret

; upd_cnt: minas que faltan por marcar (MINES - banderas) en dos digitos, fila 19, columna 14
upd_cnt:    ld   a,(flags)
            ld   b,a
            ld   a,MINES
            sub  b
            jr   nc,uc_ok
            xor  a
uc_ok:      ld   d,28               ; decenas, ya como caracter
uc_t:       cp   10
            jr   c,uc_u
            sub  10
            inc  d
            jr   uc_t
uc_u:       add  a,28
            ld   e,a
            push de
            ld   b,19
            ld   c,14
            call scr_addr
            pop  de
            ld   (hl),d
            inc  hl
            ld   (hl),e
            ret

; status: HL = texto ASCII terminado en 0. Lo pone en la fila 20, columna 8, borrando antes el sitio.
status:     push hl
            ld   b,20
            ld   c,8
            call scr_addr
            ld   b,20
st_clr:     ld   (hl),0
            inc  hl
            djnz st_clr
            ld   b,20
            ld   c,8
            call scr_addr
            ex   de,hl
            pop  hl
; puts: HL = texto ASCII terminado en 0, DE = destino en el D-FILE. Convierte a codigos del ZX81 (espacio, 0-9, A-Z, '.' y '-').
puts:       ld   a,(hl)
            or   a
            ret  z
            inc  hl
            cp   ' '
            jr   nz,pu_dig
            xor  a
            jr   pu_put
pu_dig:     cp   '0'
            jr   c,pu_oth
            cp   ':'
            jr   nc,pu_alp
            sub  20                 ; '0' (48) -> 28
            jr   pu_put
pu_alp:     cp   'A'
            jr   c,pu_oth
            cp   '['
            jr   nc,pu_oth
            sub  27                 ; 'A' (65) -> 38
            jr   pu_put
pu_oth:     cp   '.'
            ld   a,27
            jr   z,pu_put
            ld   a,22               ; cualquier otro: '-'
pu_put:     ld   (de),a
            inc  de
            jr   puts

; ---------------------------------------------------------------------
;  Datos
; ---------------------------------------------------------------------
txt_title:  defb "BUSCAMINAS",0
txt_minas:  defb "MINAS",0
msg_play:   defb "BOTON IZQ DESTAPA",0
msg_lose:   defb "BOOM - PERDISTE",0
msg_win:    defb "GANASTE",0
btn_new:    defb 80h,0B3h,0AAh,0BCh,80h     ; " NEW " en inverso

game:       defb 0                  ; 0 jugando, 1 perdida, 2 ganada
first:      defb 1                  ; 1 = faltan por repartir las minas
flags:      defb 0
opened:     defb 0
iter:       defb 0
op_idx:     defb 0
pm_click:   defb 0
pm_c:       defb 0
pm_cnt:     defb 0
q_head:     defb 0
q_tail:     defb 0
seed:       defw 0ACE1h

            include "mouselib.inc"
