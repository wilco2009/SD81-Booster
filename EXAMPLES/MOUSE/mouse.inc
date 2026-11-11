; =====================================================================
;  MOUSE.INC - lee el raton Bluetooth del SD81 Booster desde el Z80 (comando 77 del MCU, "MOUSE").
;  El raton lo lleva el ESP32 (cliente BLE) y el STM32 integra sus movimientos en una posicion: aqui solo se LEE. Para emparejarlo: consola
;  web del modulo WiFi (/debug), "esp ble on" (claude/planning/raton_bluetooth.md).
;
;  El que incluye este fichero define ANTES:
;      MS_BASE  equ 7C00h     ; RAM de trabajo: 16 bytes (8 de resultado, 3 de configuracion, 5 del emparejado)
;  y despues  include "mouse.inc"  (en pasmo, el codigo va donde este el org).
;
;  Rutinas (CALL; destruyen A, B, C, HL):
;      mouse_read     lee el raton y deja en el buzon (7 bytes, en este orden):
;                       MS_X       posicion X absoluta 0..255
;                       MS_Y       posicion Y absoluta 0..191
;                       MS_BTN     botones: bit0 izquierdo, bit1 derecho, bit2 central
;                       MS_DX      movimiento en X DESDE LA LECTURA ANTERIOR (con signo, -128..127)
;                       MS_DY      lo mismo en Y
;                       MS_WHEEL   rueda desde la lectura anterior (con signo)
;                       MS_STATUS  bit0: el MCU ha recibido algo del raton desde que arranco; bit1: hay datos nuevos desde la lectura anterior
;                     (dx, dy y rueda se ponen a cero al leer: si no llamas, se acumulan; si el cursor te basta, ignora dx/dy)
;      mouse_center   pone el cursor en el centro de la pantalla
;      mouse_config   rango y sensibilidad, desde MS_CFG_X, MS_CFG_Y y MS_CFG_DIV (0 = no cambiar):
;                       X maximo (255 por defecto), Y maximo (191), cuentas del raton por pixel (4; mas alto = mas lento)
;
;  REGLAS DEL MCU: C es el reloj del handshake (cada comando lo vuelve a leer con mu_clk) y A no sobrevive a mu_send.
;  Funciona en SLOW y en FAST. Cada lectura cuesta ~0,3 ms: de sobra para llamarla en cada fotograma.
; =====================================================================

MU_DATA     equ 0A7h
MU_CLK      equ 0AFh
CMD_MOUSE   equ 77

MS_X        equ MS_BASE+0
MS_Y        equ MS_BASE+1
MS_BTN      equ MS_BASE+2
MS_DX       equ MS_BASE+3
MS_DY       equ MS_BASE+4
MS_WHEEL    equ MS_BASE+5
MS_STATUS   equ MS_BASE+6
MS_CFG_X    equ MS_BASE+8
MS_CFG_Y    equ MS_BASE+9
MS_CFG_DIV  equ MS_BASE+10
MS_BLE_ORD  equ MS_BASE+11          ; mouse_ble_order: orden (BLE_PAIR..BLE_OFF) y argumento
MS_BLE_ARG  equ MS_BASE+12
MS_BLE_ST   equ MS_BASE+13          ; mouse_ble_status: estado y segundos de ventana; mouse_ble_order deja aqui 0 = aceptada, 80h = no valida
MS_BLE_LEFT equ MS_BASE+14

BLE_PAIR    equ 1                   ; argumento = segundos de la ventana (0 = 60)
BLE_CONNECT equ 2
BLE_FORGET  equ 3
BLE_ON      equ 4
BLE_OFF     equ 5

mouse_read: call mu_clk
            ld   a,CMD_MOUSE
            call mu_send
            xor  a                  ; op 0: leer
            call mu_send
            ld   hl,MS_BASE
            ld   b,7
mr_lp:      call mu_recv
            ld   (hl),a
            inc  hl
            djnz mr_lp
            ret

mouse_center:
            call mu_clk
            ld   a,CMD_MOUSE
            call mu_send
            ld   a,1                ; op 1: centrar
            call mu_send
            call mu_recv            ; estado
            ld   (MS_STATUS),a
            ret

mouse_config:
            call mu_clk
            ld   a,CMD_MOUSE
            call mu_send
            ld   a,2                ; op 2: configurar
            call mu_send
            ld   a,(MS_CFG_X)
            call mu_send
            ld   a,(MS_CFG_Y)
            call mu_send
            ld   a,(MS_CFG_DIV)
            call mu_send
            call mu_recv            ; estado
            ld   (MS_STATUS),a
            ret

; ---- emparejado (no hace falta dentro de un bucle de juego) ----
; mouse_ble_order: orden en MS_BLE_ORD y argumento en MS_BLE_ARG. El MCU la deja pendiente y el ESP32 la recoge en ~0,5 s: mira el
; resultado con mouse_ble_status unos instantes despues. Deja en MS_BLE_ST 0 si la acepta o 80h si la orden no existe.
mouse_ble_order:
            call mu_clk
            ld   a,CMD_MOUSE
            call mu_send
            ld   a,3                ; op 3: orden del BLE
            call mu_send
            ld   a,(MS_BLE_ORD)
            call mu_send
            ld   a,(MS_BLE_ARG)
            call mu_send
            call mu_recv
            ld   (MS_BLE_ST),a
            ret

; mouse_ble_status: MS_BLE_ST = bit0 conectado, bit1 emparejando, bit2 activado, bit3 hay raton guardado, bit7 el ESP32 ya ha contestado
;                   (con el bit7 a cero el resto no vale: modulo WiFi apagado o con firmware viejo); MS_BLE_LEFT = segundos que quedan de la ventana
mouse_ble_status:
            call mu_clk
            ld   a,CMD_MOUSE
            call mu_send
            ld   a,4                ; op 4: estado del BLE
            call mu_send
            call mu_recv
            ld   (MS_BLE_ST),a
            call mu_recv
            ld   (MS_BLE_LEFT),a
            ret

mu_clk:     in   a,(MU_CLK)
            ld   c,a
            ret

mu_send:    out  (MU_DATA),a
            ld   a,c
            cpl
            ld   c,a
mus_w:      in   a,(MU_CLK)
            xor  c
            jp   m,mus_w
            ret

mu_recv:    in   a,(MU_DATA)
            push af
            ld   a,c
            cpl
            ld   c,a
mur_w:      in   a,(MU_CLK)
            xor  c
            jp   m,mur_w
            pop  af
            ret
