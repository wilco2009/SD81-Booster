; =====================================================================
;  MOUSETEST.ASM - "paint" con el raton Bluetooth, TODO en codigo maquina.
;
;  Boton izquierdo dibuja (bloque negro), derecho borra, cualquier tecla sale.
; Lee el raton POR EL PUERTO de la FPGA (mp_read, mouseport.inc); solo centra y configura con el comando del MCU.
;  El cursor es una celda de caracter (32 x 22); se mueve con la posicion absoluta del raton (X/8, Y/8).
;
;  Requisitos: modo SLOW (en SLOW el NMI de la ROM dibuja la pantalla mientras corre el codigo maquina, y actualiza LAST_K cada fotograma)
;  y el D-FILE EXPANDIDO (32 caracteres por linea; lo prepara MOUSETEST.B81 con PRINT AT I,31).
;  No toca IX, IY, I ni los registros alternativos (el NMI y la ROM los usan).
;
;  Entrada: RAND USR 24576   (vuelve a BASIC con BC = 0)
;  Memoria:  $6000- este programa y mouse.inc;  $7C00- el buzon (MS_BASE)
;  Ensamblar: pasmo mousetest.asm mousetest.bin mousetest.sym   (compila.bat)
; =====================================================================

MS_BASE     equ 7C00h
D_FILE      equ 400Ch
LAST_K      equ 4025h           ; L = fila del teclado, FFh = ninguna tecla

CH_BLOCK    equ 80h             ; bloque negro (espacio inverso)
CH_CURSOR   equ 97h             ; '*' inverso
CH_SPACE    equ 00h

            org 24576

paint:      ; esperar a que se suelte la tecla con la que se lanzo el programa (RUN/NEWLINE)
pt_nokey:   ld   a,(LAST_K)
            inc  a
            jr   nz,pt_nokey

            ld   a,255              ; rango: X 0..255, Y 0..175 (22 filas x 8), sensibilidad la de por defecto
            ld   (MS_CFG_X),a
            ld   a,175
            ld   (MS_CFG_Y),a
            xor  a
            ld   (MS_CFG_DIV),a
            call mouse_config
            call mouse_center
            ld   hl,0
            ld   (old_addr),hl      ; 0 = no hay celda que restaurar

pt_loop:    call mp_read            ; por el puerto de la FPGA: sin comando del MCU y sin esperas (mouseport.inc)

            ; ---- direccion de la celda: D_FILE + 1 + fila*33 + columna ----
            ld   a,(MS_Y)
            rrca
            rrca
            rrca
            and  1Fh                ; fila = Y / 8
            cp   22
            jr   c,pt_row
            ld   a,21
pt_row:     ld   l,a
            ld   h,0
            ld   d,h
            ld   e,l                ; DE = fila
            add  hl,hl              ; x32
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,de              ; HL = fila * 33
            ld   a,(MS_X)
            rrca
            rrca
            rrca
            and  1Fh                ; columna = X / 8
            ld   e,a
            ld   d,0
            add  hl,de
            ld   de,(D_FILE)
            inc  de                 ; salta el primer NEWLINE
            add  hl,de              ; HL = celda

            ; ---- quitar el cursor de la celda anterior ----
            ld   de,(old_addr)
            ld   a,d
            or   e
            jr   z,pt_new
            ld   a,(saved)
            ld   (de),a

            ; ---- celda nueva: recordar su contenido, o dibujar / borrar segun el boton ----
pt_new:     ld   b,(hl)             ; contenido actual
            ld   a,(MS_BTN)
            rra                     ; bit0: izquierdo
            jr   nc,pt_right
            ld   b,CH_BLOCK
            jr   pt_put
pt_right:   rra                     ; bit1: derecho
            jr   nc,pt_put
            ld   b,CH_SPACE
pt_put:     ld   a,b
            ld   (saved),a
            ld   (old_addr),hl
            ld   (hl),CH_CURSOR

            ; ---- cualquier tecla sale ----
            ld   a,(LAST_K)
            inc  a
            jp   z,pt_loop

            ld   hl,(old_addr)      ; deja la pantalla sin el cursor
            ld   a,(saved)
            ld   (hl),a
            ld   bc,0
            ret

old_addr:   defw 0
saved:      defb 0

            include "mouse.inc"         ; mouse_config y mouse_center: comandos del MCU, una sola vez al empezar
            include "mouseport.inc"     ; mp_read: la lectura de cada fotograma
