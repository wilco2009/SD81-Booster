; =====================================================================
;  MOUSEPORT.INC - lee el raton Bluetooth por un PUERTO de la FPGA: sin comando del MCU y sin esperas.
;  La FPGA guarda el ultimo estado que le ha empujado el MCU (mouse_regs.v) y el Z80 lo lee con INs: ~100 us en total, en un bucle de juego
;  cuesta lo mismo que leer el teclado. Necesita la FPGA con sim_int 0.21 o posterior.
;
;  Es el indice 20 del puerto $3FEF (el del depurador): OUT $24 lo elige y toma una foto de los 8 bytes; cada IN devuelve el siguiente.
;
;  El que incluye este fichero define ANTES  MS_BASE equ 7C00h  (8 bytes de buzon), y despues  include "mouseport.inc"
;
;  Rutina:  mp_read   (CALL; destruye A, B, C, D, HL)   deja en el buzon, en este orden:
;      MP_X       +0  posicion X absoluta 0..255
;      MP_Y       +1  posicion Y absoluta (0..191 por defecto)
;      MP_BTN     +2  botones: bit0 izquierdo, bit1 derecho, bit2 central
;      MP_WHEEL   +3  rueda: contador de 8 bits que da la vuelta (resta la lectura anterior para el giro: positivo o negativo)
;      MP_RAWX    +4  movimiento en X en pixeles: contador que da la vuelta; resta la lectura anterior = movimiento RELATIVO (juegos)
;      MP_RAWY    +5  lo mismo en Y
;      MP_SEQ     +6  cuenta los paquetes del MCU: si no cambia desde la lectura anterior, no hay nada nuevo
;      MP_STAT    +7  bit0: el MCU ha recibido algo del raton desde el arranque
;  Los contadores de 8 bits se restan sin mas:   ld a,(MP_RAWX) / ld b,a / ... / sub b  -> dx con signo en A (-128..127)
;
;  Rango y sensibilidad (mouse_config) y centrar (mouse_center) siguen siendo comandos del MCU (mouse.inc): no hacen falta en cada fotograma.
; =====================================================================

MP_PORT     equ 3FEFh
MP_SELECT   equ 24h

MP_X        equ MS_BASE+0
MP_Y        equ MS_BASE+1
MP_BTN      equ MS_BASE+2
MP_WHEEL    equ MS_BASE+3
MP_RAWX     equ MS_BASE+4
MP_RAWY     equ MS_BASE+5
MP_SEQ      equ MS_BASE+6
MP_STAT     equ MS_BASE+7

mp_read:    ld   bc,MP_PORT
            ld   a,MP_SELECT
            out  (c),a              ; elige el raton y toma la foto
            ld   hl,MS_BASE
            ld   d,8
mpr_lp:     in   a,(c)
            ld   (hl),a
            inc  hl
            dec  d
            jr   nz,mpr_lp
            ret
