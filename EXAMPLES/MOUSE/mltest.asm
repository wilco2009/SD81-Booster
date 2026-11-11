; =====================================================================
;  MLTEST.ASM - prueba de mouselib.inc, TODO en codigo maquina.
;
;  Tres botones de texto (fila 5). Un clic izquierdo sobre uno lo enciende o lo apaga (se ve en inverso). Un DOBLE clic sobre el tercero sale.
;  Fila 8: el estado de los tres botones del raton (000 .. 111) y, a la derecha, un contador 0..9 que mueve la rueda.
;  El cursor es un sprite por hardware (ml_cur_*): la pantalla no se toca para pintarlo. Cualquier tecla sale tambien.
;
;  Requisitos: modo SLOW y el D-FILE expandido (los prepara MLTEST.B81).  Entrada: RAND USR 24576
;  Ensamblar: pasmo mltest.asm mltest.bin mltest.sym   (compila.bat)
; =====================================================================

MS_BASE     equ 7C00h
ML_CLOCK    equ 4034h               ; FRAMES: reloj del doble clic (SLOW)
D_FILE      equ 400Ch
LAST_K      equ 4025h

BTN_ROW     equ 5
BTN_COL0    equ 4                   ; columna del primer boton; cada uno ocupa 6 y los separan 2
BTN_PX0     equ BTN_COL0*8          ; lo mismo en pixeles (el raton va en pixeles)
BTN_PY0     equ BTN_ROW*8

            org 24576

start:      ld   a,(LAST_K)         ; espera a que se suelte la tecla con la que se lanzo
            inc  a
            jr   nz,start

            ld   bc,175*256+255     ; 22 filas de texto: Y maximo 175
            call ml_init
            call ml_cur_init

            xor  a
            call draw_btn
            ld   a,1
            call draw_btn
            ld   a,2
            call draw_btn

loop:       call ml_poll
            call ml_cur_show

            ; ---- clic izquierdo: que boton (si alguno) ----
            ld   a,(ML_PRESS)
            rra
            jr   nc,lp_wheel
            xor  a
            ld   (idx),a
lp_btn:     ld   a,(idx)
            rrca
            rrca
            and  0C0h               ; idx * 64: los botones estan a 64 pixeles unos de otros
            add  a,BTN_PX0
            ld   b,a                ; x0
            add  a,48
            ld   d,a                ; x1: 6 celdas de ancho
            ld   c,BTN_PY0
            ld   e,BTN_PY0+8
            call ml_hit
            jr   nc,lp_next
            ld   a,(idx)
            cp   2
            jr   nz,lp_toggle
            ld   a,(ML_DBL)         ; el tercero: el doble clic sale
            rra
            jr   c,exit
lp_toggle:  ld   a,(idx)
            ld   e,a
            ld   d,0
            ld   hl,btn_state
            add  hl,de
            ld   a,(hl)
            xor  1
            ld   (hl),a
            ld   a,(idx)
            call draw_btn
lp_next:    ld   a,(idx)
            inc  a
            ld   (idx),a
            cp   3
            jr   nz,lp_btn

            ; ---- rueda: contador 0..9 ----
lp_wheel:   ld   a,(ML_WHEEL)
            or   a
            jr   z,lp_show
            ld   hl,cnt
            jp   m,lp_down
            ld   a,(hl)
            inc  a
            cp   10
            jr   c,lp_set
            xor  a
            jr   lp_set
lp_down:    ld   a,(hl)
            dec  a
            jp   p,lp_set
            ld   a,9
lp_set:     ld   (hl),a

            ; ---- fila 8: botones del raton y contador ----
lp_show:    ld   hl,(D_FILE)
            ld   de,1+8*33+4
            add  hl,de
            ld   a,(ML_BTN)
            ld   c,a
            ld   b,3
lp_bit:     ld   a,28               ; '0'
            rr   c
            jr   nc,lp_put
            inc  a                  ; '1'
lp_put:     ld   (hl),a
            inc  hl
            djnz lp_bit
            ld   hl,(D_FILE)
            ld   de,1+8*33+15
            add  hl,de
            ld   a,(cnt)
            add  a,28
            ld   (hl),a

            ld   a,(LAST_K)         ; cualquier tecla sale
            inc  a
            jp   z,loop

exit:       call ml_cur_hide
            ld   bc,0
            ret

; ---- draw_btn: A = boton (0..2): su etiqueta, en inverso si esta encendido ----
draw_btn:   ld   b,a
            ld   e,a
            ld   d,0
            ld   hl,btn_state
            add  hl,de
            ld   c,(hl)             ; C = estado
            ld   a,b
            add  a,a
            add  a,b
            add  a,a                ; * 6
            ld   e,a
            ld   d,0
            ld   hl,labels
            add  hl,de
            push hl                 ; origen: la etiqueta
            ld   a,b
            add  a,a
            add  a,a
            add  a,a                ; * 8
            add  a,BTN_COL0
            ld   e,a
            ld   d,0
            ld   hl,(D_FILE)
            add  hl,de
            ld   de,1+BTN_ROW*33
            add  hl,de              ; HL = celda de pantalla
            ex   de,hl              ; DE = destino
            pop  hl
            ld   b,6
db_lp:      ld   a,(hl)
            bit  0,c
            jr   z,db_put
            add  a,80h              ; inverso
db_put:     ld   (de),a
            inc  hl
            inc  de
            djnz db_lp
            ret

idx:        defb 0
cnt:        defb 0
btn_state:  defb 0,0,0
; etiquetas de 6 caracteres, en codigos del ZX81 (espacio 0, digitos 28-37, letras 38-63)
labels:     defb 0,39,57,51,29,0 ; " BTN1 "
            defb 0,39,57,51,30,0    ; " BTN2 "
            defb 0,39,57,51,31,0    ; " BTN3 "

            include "mouselib.inc"
