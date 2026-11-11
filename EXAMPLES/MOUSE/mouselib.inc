; =====================================================================
;  MOUSELIB.INC - libreria de raton para programas en ensamblador del SD81 Booster.
;
;  Se construye sobre mouse.inc (comando 77 del MCU: configurar y centrar) y mouseport.inc (lectura por el puerto de la FPGA, sin esperas).
;  Anade lo que necesita casi cualquier programa con raton:
;      - eventos de boton: pulsado, soltado y doble clic (flancos, no estados)
;      - la rueda como un incremento con signo
;      - "esta el raton dentro de este rectangulo?" y "en que celda esta?" (para botones, menus, tableros)
;      - un cursor por HARDWARE (el sprite 63): no borra ni repinta nada de la pantalla
;
;  El programa define ANTES  MS_BASE equ 7C00h  (28 bytes de buzon: 16 de mouse.inc/mouseport.inc y 12 de esta libreria) y
;  ML_CLOCK equ 4034h  (el reloj del doble clic: ver mas abajo), y despues
;      include "mouselib.inc"
;  (incluye mouse.inc y mouseport.inc por su cuenta: no los incluyas aparte).
;
;  Uso tipico:
;            ld   bc,191*256+255     ; B = Y maximo, C = X maximo
;            call ml_init            ; configura, centra y limpia los eventos
;            call ml_cur_init        ; flecha por defecto (sprite 63)
;  bucle:    call ml_poll            ; UNA vez por vuelta del bucle (unos 100 us)
;            call ml_cur_show        ; coloca el cursor
;            ld   a,(ML_PRESS)       ; bit0 = se acaba de pulsar el izquierdo, bit1 el derecho, bit2 el central
;            ...
;  salida:   call ml_cur_hide        ; apaga el sprite antes de volver al BASIC
;
;  Variables (se leen despues de ml_poll):
;      MP_X, MP_Y   posicion absoluta del puntero, X 0..255, Y 0..(Y maximo)   [de mouseport.inc]
;      ML_BTN       botones pulsados ahora (bit0 izquierdo, bit1 derecho, bit2 central)
;      ML_PRESS     botones que se han pulsado desde el poll anterior (flanco de subida)
;      ML_REL       botones que se han soltado desde el poll anterior (flanco de bajada)
;      ML_DBL       bit0: el izquierdo acaba de dar un DOBLE clic (el segundo clic tambien aparece en ML_PRESS)
;      ML_WHEEL     giro de la rueda desde el poll anterior, con signo (-128..127)
;
;  Rutinas (CALL):                                       destruyen
;      ml_init        BC = (Y maximo, X maximo)          AF BC DE HL
;      ml_poll        lee el raton y calcula los eventos AF BC DE HL
;      ml_connected   NZ si el MCU ha recibido algo del raton desde el arranque (lee el puerto)   AF BC DE HL
;      ml_hit         B=x0 C=y0 D=x1 E=y1 (x1, y1 excluidos): CARRY si el puntero esta dentro   AF
;      ml_cell        B = desplazamiento (3 = celdas de 8, 4 = de 16, ...): H = columna, L = fila   AF BC HL
;      ml_cur_init    cursor: flecha por defecto         AF BC DE HL
;      ml_cur_set     cursor: HL = 24 bytes (8 de color, 8 de pixeles y 8 de mascara, como el sprite formato A del Apendice H)   AF BC DE HL
;      ml_cur_show    coloca y enciende el cursor        AF DE HL
;      ml_cur_hide    apaga el cursor                    AF
;
;  Reglas: no toca IX, IY, I ni los registros alternativos (el NMI y la ROM los usan en SLOW). El doble clic mide el
;  tiempo con FRAMES, que la ROM actualiza en SLOW y la FPGA simula en FAST (con DI, mejor un contador propio, como Paint). Necesita la FPGA con sim_int 0.21 o posterior (puerto del raton, indice 20 de $3FEF) y la
;  ROM sin LOAD *ROMLOCK (el cursor escribe en los registros de sprite 2100-2131).
;
;  El cursor es un sprite de 8x8: el punto caliente (la punta de la flecha) esta en el pixel (1,1) del sprite, asi que el sprite se
;  dibuja en (X-1, Y-1). Las coordenadas de sprite van desplazadas 32 pixeles (Apendice H): en pantalla (X, Y) -> sprite (X+32, Y+32).
; =====================================================================

            include "mouse.inc"
            include "mouseport.inc"

FRAMES      equ 4034h               ; contador de fotogramas de la ROM (en FAST lo simula la FPGA); basta el byte bajo
; ML_CLOCK lo define el PROGRAMA antes del include: la direccion de un byte que cambia con el tiempo. En SLOW y en FAST vale  ML_CLOCK equ 4034h
; (FRAMES); con DI (los modos de 160 pixeles) el programa tiene su propio byte y lo incrementa una vez por fotograma.
ML_DBLTIME  equ 20                 ; doble clic: maximo de fotogramas entre los dos clics (20 = 0,4 s a 50 Hz)
ML_SPR      equ 63                  ; sprite del cursor: el 63 tapa a todos los demas

ML_BASE     equ MS_BASE+16
ML_BTN      equ ML_BASE+0
ML_PRESS    equ ML_BASE+1
ML_REL      equ ML_BASE+2
ML_DBL      equ ML_BASE+3
ML_WHEEL    equ ML_BASE+4
ML_PWHEEL   equ ML_BASE+5           ; rueda en el poll anterior
ML_LCLK     equ ML_BASE+6           ; FRAMES (byte bajo) del ultimo clic izquierdo
ML_LFLAG    equ ML_BASE+7           ; 1 = hay un primer clic esperando al segundo
ML_BYTES    equ 8                   ; los que borra ml_init

SPR_SEL     equ 2100                ; registros de sprite (Apendice H)
SPR_EN      equ 2101
SPR_XL      equ 2102
SPR_XH      equ 2103
SPR_Y       equ 2104
SPR_COL     equ 2105
SPR_PIX     equ 2113
SPR_MASK    equ 2121
SPR_PAGE    equ 2130

; ---------------------------------------------------------------------
;  ml_init: BC = (Y maximo, X maximo). Configura el rango (comando 77, op 2), centra el cursor (op 1) y deja a cero los eventos.
;  Y maximo 191 y X maximo 255 son los valores por defecto del MCU; en un modo de texto de 22 filas, 175.
; ---------------------------------------------------------------------
ml_init:    ld   a,c
            ld   (MS_CFG_X),a
            ld   a,b
            ld   (MS_CFG_Y),a
            xor  a
            ld   (MS_CFG_DIV),a     ; sensibilidad: la de por defecto
            call mouse_config
            call mouse_center
            ld   hl,ML_BASE
            ld   b,ML_BYTES
            xor  a
mli_clr:    ld   (hl),a
            inc  hl
            djnz mli_clr
            call mp_read            ; foto inicial: ni un boton ya pulsado ni un giro de rueda cuentan como evento
            ld   a,(MP_BTN)
            and  7
            ld   (ML_BTN),a
            ld   a,(MP_WHEEL)
            ld   (ML_PWHEEL),a
            ret

; ---------------------------------------------------------------------
;  ml_poll: una lectura del raton y los eventos desde la anterior.
; ---------------------------------------------------------------------
ml_poll:    call mp_read
            ld   a,(ML_BTN)
            ld   c,a                ; C = botones del poll anterior
            ld   a,(MP_BTN)
            and  7
            ld   b,a                ; B = botones ahora
            ld   (ML_BTN),a
            ld   a,c
            cpl
            and  b                  ; pulsados ahora y no antes
            ld   (ML_PRESS),a
            ld   d,a
            ld   a,b
            cpl
            and  c                  ; antes y no ahora
            ld   (ML_REL),a

            xor  a
            ld   (ML_DBL),a
            bit  0,d                ; solo el izquierdo cuenta para el doble clic
            jr   z,mlp_wheel
            ld   a,(ML_LFLAG)
            or   a
            jr   z,mlp_first
            ld   a,(ML_CLOCK)
            ld   hl,ML_LCLK
            sub  (hl)
            jr   nc,mlp_abs
            neg                     ; FRAMES sube o baja segun el modo: vale la distancia en cualquier sentido
mlp_abs:    cp   ML_DBLTIME
            jr   nc,mlp_first       ; tarde: este clic pasa a ser el primero de otra pareja
            ld   a,1
            ld   (ML_DBL),a
            xor  a
            ld   (ML_LFLAG),a
            jr   mlp_wheel
mlp_first:  ld   a,(ML_CLOCK)
            ld   (ML_LCLK),a
            ld   a,1
            ld   (ML_LFLAG),a

mlp_wheel:  ld   a,(MP_WHEEL)
            ld   b,a
            ld   a,(ML_PWHEEL)
            ld   c,a
            ld   a,b
            ld   (ML_PWHEEL),a
            sub  c                  ; el contador de 8 bits da la vuelta: la resta sale bien siempre
            ld   (ML_WHEEL),a
            ret

; ---------------------------------------------------------------------
;  ml_connected: lee el puerto y devuelve NZ si el MCU ha recibido algo del raton (bit 0 de MP_STAT).
; ---------------------------------------------------------------------
ml_connected:
            call mp_read
            ld   a,(MP_STAT)
            and  1
            ret

; ---------------------------------------------------------------------
;  ml_hit: B=x0 C=y0 D=x1 E=y1 (x1 e y1 excluidos). CARRY = el puntero (MP_X, MP_Y) esta dentro; NC = fuera.
; ---------------------------------------------------------------------
ml_hit:     ld   a,(MP_X)
            cp   b
            jr   c,mlh_no
            cp   d
            jr   nc,mlh_no
            ld   a,(MP_Y)
            cp   c
            jr   c,mlh_no
            cp   e
            jr   nc,mlh_no
            scf
            ret
mlh_no:     and  a                  ; borra el carry
            ret

; ---------------------------------------------------------------------
;  ml_cell: B = desplazamiento (3 = celdas de 8 pixeles, 4 = de 16, ...). Devuelve H = MP_X >> B, L = MP_Y >> B.
; ---------------------------------------------------------------------
ml_cell:    ld   c,b
            ld   a,(MP_X)
            call mlc_sh
            ld   h,a
            ld   a,(MP_Y)
            call mlc_sh
            ld   l,a
            ret
mlc_sh:     ld   b,c
            inc  b                  ; B = desplazamiento + 1: el primer DJNZ no hace nada si el desplazamiento es 0
            jr   mlc_t
mlc_l:      srl  a
mlc_t:      djnz mlc_l
            ret

; ---------------------------------------------------------------------
;  Cursor: el sprite ML_SPR, formato A (color, pixeles y mascara por filas, Apendice H).
; ---------------------------------------------------------------------
ml_cur_init:
            ld   hl,mlc_arrow
ml_cur_set: ld   a,ML_SPR
            ld   (SPR_SEL),a
            xor  a
            ld   (SPR_PAGE),a       ; formato A
            ld   (SPR_EN),a         ; apagado hasta el primer ml_cur_show
            ld   de,SPR_COL
            ld   bc,8
            ldir
            ld   de,SPR_PIX
            ld   bc,8
            ldir
            ld   de,SPR_MASK
            ld   bc,8
            ldir
            ret

ml_cur_show:
            ld   a,ML_SPR
            ld   (SPR_SEL),a
            ld   a,(MP_X)
            ld   l,a
            ld   h,0
            ld   de,32-1            ; desplazamiento de las coordenadas de sprite, menos 1 del punto caliente
            add  hl,de
            ld   a,l
            ld   (SPR_XL),a
            ld   a,h                ; bit 8 de X (los bits 4-7, el transparente del formato B, a 0)
            ld   (SPR_XH),a
            ld   a,(MP_Y)
            add  a,32-1
            ld   (SPR_Y),a
            ld   a,1
            ld   (SPR_EN),a
            ret

ml_cur_hide:
            ld   a,ML_SPR
            ld   (SPR_SEL),a
            xor  a
            ld   (SPR_EN),a
            ret

; Flecha blanca con borde negro; el punto caliente es el pixel (1,1).
mlc_arrow:  defb 0F0h,0F0h,0F0h,0F0h,0F0h,0F0h,0F0h,0F0h   ; color: tinta blanca brillante (15), papel negro (0)
            defb 000h,040h,060h,070h,078h,07Ch,058h,018h   ; pixeles (1 = tinta)
            defb 0E0h,0F0h,0F8h,0FCh,0FEh,0FEh,0FEh,0FCh   ; mascara: la flecha y su borde de un pixel
