; =====================================================================
;  MOUSEBLE.ASM - emparejar / conectar / olvidar el raton Bluetooth DESDE BASIC (sin tocar la ROM: son dos USR).
;
;  Carga:   LOAD FAST "MOUSEBLE.BIN" CODE 24576     (despues de LOAD THEN CLEAR 24575)
;  Orden:   POKE 31755,orden : POKE 31756,arg : LET R=USR 24576      R = 0 aceptada, 128 orden no valida
;              orden: 1 emparejar (arg = segundos de la ventana, 0 = 60), 2 conectar, 3 olvidar, 4 activar, 5 parar
;  Estado:  LET S=USR 24579 : LET T=PEEK 31758
;              S: bit0 conectado, bit1 emparejando, bit2 activado, bit3 hay raton guardado, bit7 el modulo WiFi ya ha contestado
;              (con el bit7 a cero lo demas no vale: modulo apagado o con firmware viejo). T = segundos que quedan de la ventana.
;  La orden la recoge el ESP32 en ~0,5 s: mira el estado un momento despues.
;  No toca IX, IY, I ni los registros alternativos.
; =====================================================================

MS_BASE     equ 7C00h               ; 31744: MS_BLE_ORD = 31755, MS_BLE_ARG = 31756, MS_BLE_ST = 31757, MS_BLE_LEFT = 31758

            org 24576

            jp   ble_cmd            ; 24576
            jp   ble_stat           ; 24579

ble_cmd:    call mouse_ble_order
            ld   a,(MS_BLE_ST)      ; 0 aceptada, 80h no valida
            ld   c,a
            ld   b,0
            ret

ble_stat:   call mouse_ble_status
            ld   a,(MS_BLE_ST)
            ld   c,a
            ld   b,0
            ret

            include "mouse.inc"
