; =====================================================================
;  NETMOUSE.INC - el raton Bluetooth en los juegos en red de NETGAME (el ajedrez y el Scrabble). Se lee por el puerto de la FPGA
;  (EXAMPLES/MOUSE/mouseport.inc: $3FEF, indice 20) y su cursor es el sprite 63. Los gestos se convierten en TECLAS que se meten
;  en una cola: el juego las toma en su bucle como si vinieran del teclado, asi que el raton no cambia las reglas ni la entrada.
;
;  El juego define antes NM_BASE (RAM de trabajo, NM_LEN bytes: se pone a cero en nm_init) y NETMOUSE = 1 (lobby.inc), e incluye este
;  fichero despues de lobby.inc (usa scr_addr y DFILE). Las coordenadas son las normales de 256 x 192: la celda de 8 x 8 en la que esta
;  el cursor es (fila = Y / 8, columna = X / 8), en los dos modos (texto de 32 columnas y HiRes Spectrum).
;
;  Rutinas (conservan BC, DE y HL salvo que se diga):
;      nm_init        pone a cero las variables y define el cursor (sprite 63, apagado hasta que haya raton)
;      nm_off         apaga el cursor
;      nm_poll        lee el raton: NM_X, NM_Y (pixeles), NM_EVT (botones recien pulsados: bit 0 izquierdo, bit 1 derecho; vale hasta el
;                     siguiente nm_poll) y coloca el cursor. Hay que llamarla una vez por vuelta del bucle
;      nm_cell        B = fila y C = columna de la celda del cursor (destruye A)
;      nm_push        A = tecla a la cola (si cabe)
;      nm_pop         A = la primera tecla de la cola (la quita), o 0
;      nm_zone        HL = tabla de zonas (fila, columna inicial, columna final + 1, tecla; acaba en fila 0), B = fila, C = columna
;                     -> A = la tecla de la zona que los contiene, o 0 (destruye HL)
;      nm_menu        (lobby) un clic en una linea de menu "X = ..." -> A = X (el ASCII de la tecla), o 0
; =====================================================================

NM_QMAX     equ 24                  ; teclas que caben en la cola
NM_X        equ NM_BASE+0
NM_Y        equ NM_BASE+1
NM_PREV     equ NM_BASE+2           ; botones de la lectura anterior
NM_EVT      equ NM_BASE+3
NM_QN       equ NM_BASE+4           ; teclas en la cola
NM_QB       equ NM_BASE+5           ; (NM_QMAX) la cola
NM_LEN      equ 5+NM_QMAX
NM_SPR      equ 63                  ; el sprite del cursor: el 63 tapa a todos los demas (el Scrabble usa el 0, el 1 y el 2)
NM_PORT     equ 3FEFh

nm_init:    push bc
            push de
            push hl
            ld   hl,NM_BASE
            ld   de,NM_BASE+1
            ld   bc,NM_LEN-1
            ld   (hl),0
            ldir
            ld   a,NM_SPR
            ld   (2100),a
            xor  a
            ld   (2101),a           ; apagado hasta que el raton este conectado
            ld   (2130),a           ; formato A
            ld   hl,nm_arrow
            ld   de,2105
            ld   bc,24
            ldir                    ; color, pixeles y mascara (2105-2128)
            ld   bc,NM_PORT
            ld   a,24h
            out  (c),a
            in   a,(c)              ; x
            in   a,(c)              ; y
            in   a,(c)              ; botones
            and  3
            ld   (NM_PREV),a        ; el boton con el que se haya arrancado no cuenta como clic
            pop  hl
            pop  de
            pop  bc
            ret

nm_off:     ld   a,NM_SPR
            ld   (2100),a
            xor  a
            ld   (2101),a
            ret

nm_poll:    push bc
            push de
            push hl
            xor  a
            ld   (NM_EVT),a
            ld   bc,NM_PORT
            ld   a,24h
            out  (c),a
            in   a,(c)
            ld   (NM_X),a
            in   a,(c)
            ld   (NM_Y),a
            in   a,(c)
            and  3
            ld   d,a                ; D = botones
            in   a,(c)              ; rueda
            in   a,(c)              ; rawx
            in   a,(c)              ; rawy
            in   a,(c)              ; seq
            in   a,(c)              ; estado: bit 0 = conectado
            and  1
            ld   e,a
            ld   a,NM_SPR
            ld   (2100),a
            ld   a,e
            ld   (2101),a           ; el cursor, solo con raton
            jr   z,nmp_end
            ld   a,(NM_X)           ; sprite en (x+31, y+31): +32 del origen de los sprites, -1 del punto caliente
            ld   l,a
            ld   h,0
            ld   bc,31
            add  hl,bc
            ld   a,l
            ld   (2102),a
            ld   a,h
            ld   (2103),a
            ld   a,(NM_Y)
            add  a,31
            ld   (2104),a
            ld   a,(NM_PREV)        ; botones recien pulsados
            cpl
            and  d
            ld   (NM_EVT),a
            ld   a,d
            ld   (NM_PREV),a
nmp_end:    pop  hl
            pop  de
            pop  bc
            ret

nm_cell:    ld   a,(NM_Y)
            rrca
            rrca
            rrca
            and  1Fh
            ld   b,a
            ld   a,(NM_X)
            rrca
            rrca
            rrca
            and  1Fh
            ld   c,a
            ret

nm_push:    push hl
            push bc
            ld   b,a                ; B = tecla
            ld   a,(NM_QN)
            cp   NM_QMAX
            jr   nc,npu_x
            ld   c,a
            ld   a,b
            ld   hl,NM_QB
            ld   b,0
            add  hl,bc              ; HL = NM_QB + (las que hay)
            ld   (hl),a
            ld   hl,NM_QN
            inc  (hl)
npu_x:      pop  bc
            pop  hl
            ret

nm_pop:     ld   a,(NM_QN)
            or   a
            ret  z
            push hl
            push de
            push bc
            dec  a
            ld   (NM_QN),a
            ld   c,a                ; C = las que quedan
            ld   hl,NM_QB
            ld   a,(hl)             ; la primera
            ld   b,a
            ld   a,c
            or   a
            jr   z,npo_d
            ld   d,h
            ld   e,l
            inc  hl
            push bc
            ld   b,0
            ldir                    ; el resto sube un puesto
            pop  bc
npo_d:      ld   a,b
            pop  bc
            pop  de
            pop  hl
            ret

nm_zone:    ld   a,(hl)             ; fila (0 = fin de la tabla)
            or   a
            ret  z
            cp   b
            jr   nz,nz_n0
            inc  hl
            ld   a,c
            cp   (hl)
            jr   c,nz_n1            ; antes de la zona
            inc  hl
            cp   (hl)
            jr   nc,nz_n2           ; despues
            inc  hl
            ld   a,(hl)             ; la tecla
            ret
nz_n0:      inc  hl                 ; siguiente zona (4 bytes): se entra por aqui, por nz_n1 o por nz_n2 segun lo avanzado
nz_n1:      inc  hl
nz_n2:      inc  hl
            inc  hl
            jr   nm_zone

IF NETMOUSE
; nm_menu: lee el raton y, si ha habido un clic izquierdo en una linea del menu de texto (el lobby) con la forma "X = ...", devuelve X
; (el ASCII de la tecla). La linea se mira en la pantalla: el '=' (codigo 20 del ZX81) tiene que ir tras un espacio y una sola tecla
; (cifra o letra) que empiece la linea o vaya tras un espacio: asi "1 = NEW GAME" o "Q = QUIT" valen y "DEL = SHIFT+0" no.
nm_menu:    call nm_poll
            ld   a,(NM_EVT)
            and  1
            ret  z
            call nm_cell            ; B = fila
            ld   c,0
            call scr_addr           ; HL = el principio de esa fila de la pantalla (destruye DE)
            inc  hl
            inc  hl
            ld   d,2                ; D = la columna de HL
nmm_l:      ld   a,(hl)
            cp   20                 ; '='
            jr   z,nmm_f
            inc  hl
            inc  d
            ld   a,d
            cp   32
            jr   c,nmm_l
            xor  a
            ret
nmm_f:      dec  hl
            ld   a,(hl)
            or   a
            jr   nz,nmm_n           ; delante del '=' tiene que haber un espacio
            dec  hl
            ld   a,(hl)             ; la tecla
            cp   28
            jr   c,nmm_n
            cp   64
            jr   nc,nmm_n
            ld   b,a
            ld   a,d
            cp   2
            jr   z,nmm_k            ; el '=' en la columna 2: la tecla va la primera
            dec  hl
            ld   a,(hl)
            or   a
            jr   nz,nmm_n           ; antes de la tecla tiene que haber un espacio
nmm_k:      ld   a,b
            cp   38
            jr   nc,nmm_a
            add  a,20               ; cifras: 28-37 -> '0'-'9'
            ret
nmm_a:      add  a,27               ; letras: 38-63 -> 'A'-'Z'
            ret
nmm_n:      xor  a
            ret
ENDIF

; el cursor: una flecha blanca con borde negro (la de EXAMPLES/MOUSE/mouselib.inc): color, pixeles y mascara de las 8 filas
nm_arrow:   defb 0F0h,0F0h,0F0h,0F0h,0F0h,0F0h,0F0h,0F0h
            defb 000h,040h,060h,070h,078h,07Ch,058h,018h
            defb 0E0h,0F0h,0F8h,0FCh,0FEh,0FEh,0FEh,0FCh
