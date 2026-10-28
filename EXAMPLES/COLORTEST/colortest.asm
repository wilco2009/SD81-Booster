; =====================================================================
;  COLORTEST.ASM - prueba de colores del modo Superfast HiRes Spectrum (256x192, un atributo por celda de 8x8, el
;  bitmap en $A000 y los atributos en $B800). Pinta, para los 8 colores (0 negro, 1 azul, 2 rojo, 3 magenta, 4 verde, 5 cian,
;  6 amarillo, 7 blanco), en este orden de arriba abajo y en las mismas 8 columnas de 4 celdas:
;
;      PAPEL   con brillo (bit 6 = 1)         y debajo, sin brillo
;      TINTA   con brillo                     y debajo, sin brillo
;      y, para diagnosticar, los mismos papeles con el bit 7 (FLASH en el Spectrum) puesto: sin brillo y con brillo
;
;  Si el brillo funciona, cada fila "BRIGHT 1" tiene que verse mas viva que la "BRIGHT 0" que tiene debajo (el negro es igual
;  en las dos).
;
;  P recorre las paletas de 16 colores de la FPGA (POKE 2063 puntero, 2064 byte bajo, 2066 byte alto, 2067,1 / 0): con cada pulsacion, la siguiente
;  de pal16.inc (gen_pal16.py: una rampa de gris y de rojo, verde, azul, amarillo, cian y magenta; los 16 colores de la pantalla pasan a ser los
;  16 niveles de la rampa) y, tras la ultima, la paleta desactivada: vuelven los colores por defecto. Cualquier otra tecla sale a BASIC (con la
;  paleta desactivada).
;
;  Cargar: COLORTEST.B81 (CLEAR 24575, LOAD FAST ... CODE 24576, FAST, USR 24576, SLOW).
;  Ensamblar: pasmo colortest.asm colortest.bin colortest.sym   (compila.bat)
; =====================================================================

SET_FAST    equ 02E7h           ; ROM: apaga el video si estaba en SLOW
SLOW_FAST   equ 0207h           ; ROM: vuelve al modo que pide CDFLAG
HFILE       equ 0A000h
ATTRB       equ HFILE+1800h
ROMFONT     equ 1E00h
ATTR_TEXT   equ 047h            ; tinta blanca brillante sobre negro
CHROMA_P    equ 07FEFh
CHROMA_ON   equ 020h            ; color, modo 0, borde negro
CHROMA_OFF  equ 00Ch

SW_ROW      equ 7F00h           ; variables de trabajo (RAM de datos)
SW_HI       equ 7F01h
SW_INK      equ 7F02h
SW_C        equ 7F03h
SW_R        equ 7F04h
SW_K        equ 7F05h
SW_PAL      equ 7F06h           ; 0 = paleta de 16 desactivada, 1..NPAL = la paleta de pal16.inc que esta cargada y activa
NPAL        equ 7               ; paletas en pal16.inc (gris, rojo, verde, azul, amarillo, cian, magenta)

            org  24576

start:      call SET_FAST
            call gfx_on
            ld   hl,t_title
            ld   bc,0*256+1
            call print
            ld   b,0                ; los numeros de color, sobre cada columna de 4 celdas
dg_l:       push bc
            ld   a,b
            add  a,a
            add  a,a
            inc  a
            ld   c,a
            ld   a,b
            add  a,28               ; la cifra
            push af
            ld   b,1
            call gaddr
            pop  af
            call glyph
            pop  bc
            inc  b
            ld   a,b
            cp   8
            jr   nz,dg_l

            ld   hl,t_p1            ; papel con brillo y sin brillo
            ld   bc,2*256+0
            call print
            ld   b,3
            ld   h,40h
            ld   l,0
            call swatches
            ld   hl,t_p0
            ld   bc,5*256+0
            call print
            ld   b,6
            ld   h,0
            ld   l,0
            call swatches

            ld   hl,t_i1            ; tinta con brillo y sin brillo
            ld   bc,9*256+0
            call print
            ld   b,10
            ld   h,40h
            ld   l,1
            call swatches
            ld   hl,t_i0
            ld   bc,12*256+0
            call print
            ld   b,13
            ld   h,0
            ld   l,1
            call swatches

            ld   hl,t_f0            ; diagnostico: el bit 7 puesto, sin y con el bit 6
            ld   bc,16*256+0
            call print
            ld   b,17
            ld   h,80h
            ld   l,0
            call swatches
            ld   hl,t_f1
            ld   bc,19*256+0
            call print
            ld   b,20
            ld   h,0C0h
            ld   l,0
            call swatches

            ld   hl,t_key
            ld   bc,23*256+1
            call print

            xor  a                  ; paleta de 16 desactivada
            ld   (SW_PAL),a
            ld   (2067),a
            call pal_show
            call key_up             ; espera una tecla (soltando antes la que lanzo el programa)
wait_k:     ld   a,0DFh             ; fila P-Y: P = bit 0
            in   a,(0FEh)
            bit  0,a
            jr   nz,not_p
            call pal_next
            call debounce           ; el rebote de la pulsacion (la P se lee a veces abierta, a veces cerrada)
            call key_up
            call debounce           ; y el de la suelta: sin esto, la P parece pulsada otra vez y cambia dos veces de paleta
            call key_up
            jr   wait_k
not_p:      ld   bc,0FEFEh          ; salir con cualquier tecla MENOS la P: se barren las 8 filas una a una y en la de la P se ignora su bit
np_l:       in   a,(c)              ; (con un solo barrido de todas las filas, el rebote de la P al pulsarla, abierta en la lectura de arriba
            ld   d,a                ; y cerrada en esta, parecia otra tecla y se salia a BASIC)
            ld   a,b
            cp   0DFh
            ld   a,d
            jr   nz,np_n
            or   1                  ; fila P-Y: el bit 0 es la P
np_n:       cpl
            and  1Fh
            jr   nz,exit
            rlc  b                  ; FE, FD, FB, F7, EF, DF, BF, 7F y de nuevo FE (acarreo 0)
            jr   c,np_l
            jr   wait_k

exit:       xor  a                  ; paleta de 16 desactivada: colores por defecto
            ld   (2067),a
            ld   a,85               ; video nativo
            ld   (2045),a
            ld   bc,CHROMA_P
            ld   a,CHROMA_OFF
            out  (c),a
            call SLOW_FAST
            ld   bc,0
            ret

; siguiente paleta: 0 (desactivada) -> 1 -> ... -> NPAL -> 0. Para cambiar de paleta se desactiva, se cargan las 16 entradas y se vuelve a activar
pal_next:   xor  a
            ld   (2067),a
            ld   a,(SW_PAL)
            inc  a
            cp   NPAL+1
            jr   c,pn_1
            xor  a
pn_1:       ld   (SW_PAL),a
            or   a
            jr   z,pn_off
            call pal_load
            ld   a,1
pn_off:     ld   (2067),a
            jp   pal_show

; carga las 16 entradas de la paleta SW_PAL (1..NPAL) de pal16.inc: 32 bytes cada una (el puntero 2063 avanza solo con cada POKE 2066)
pal_load:   ld   a,(SW_PAL)
            dec  a
            ld   l,a
            ld   h,0
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl
            ld   de,pal16
            add  hl,de
            xor  a
            ld   (2063),a
            ld   b,16
pl_l:       ld   a,(hl)
            inc  hl
            ld   (2064),a
            ld   a,(hl)
            inc  hl
            ld   (2066),a
            djnz pl_l
            ret

; escribe el estado de la paleta en la fila 22: la etiqueta y el nombre (9 bytes por nombre, 8 letras y el 0)
pal_show:   ld   hl,t_plab
            ld   bc,22*256+1
            call print
            ld   a,(SW_PAL)
            ld   l,a
            ld   h,0
            ld   d,h
            ld   e,l
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,de
            ld   de,t_pn
            add  hl,de
            ld   bc,22*256+10
            jp   print

; unos 20 ms (a 3,25 MHz: 20 vueltas de 256 DJNZ de 13 T-states)
debounce:   ld   c,20
db_o:       ld   b,0
db_i:       djnz db_i
            dec  c
            jr   nz,db_o
            ret

key_up:     xor  a
            in   a,(0FEh)
            cpl
            and  1Fh
            jr   nz,key_up
            ret
key_down:   xor  a
            in   a,(0FEh)
            cpl
            and  1Fh
            jr   z,key_down
            ret

; --- el modo (como gfx.inc de NETGAME) -----------------------------------
gfx_on:     xor  a
            ld   (2043),a           ; HFILE
            ld   a,HFILE/256
            ld   (2044),a
            ld   hl,HFILE           ; bitmap en blanco
            ld   de,HFILE+1
            ld   bc,17FFh
            ld   (hl),0
            ldir
            ld   hl,ATTRB           ; atributos del texto
            ld   de,ATTRB+1
            ld   bc,2FFh
            ld   (hl),ATTR_TEXT
            ldir
            ld   bc,CHROMA_P        ; color activado, modo 0, borde negro
            ld   a,CHROMA_ON
            out  (c),a
            ld   a,172              ; Superfast HiRes Spectrum
            ld   (2045),a
            xor  a
            out  (0FBh),a
            ret

; fila B, columna C -> HL = HFILE + (B and 18h)*256 + (B and 7)*32 + C
gaddr:      ld   a,b
            and  18h
            or   HFILE/256
            ld   h,a
            ld   a,b
            and  7
            rrca
            rrca
            rrca
            add  a,c
            ld   l,a
            ret

; fila B, columna C -> HL = ATTRB + B*32 + C
gattr:      ld   l,b
            ld   h,0
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl
            ld   e,c
            ld   d,0
            add  hl,de
            ld   de,ATTRB
            add  hl,de
            ret

; A = codigo de caracter del ZX81, HL = celda: sus 8 lineas
glyph:      push hl
            ld   l,a
            ld   h,0
            add  hl,hl
            add  hl,hl
            add  hl,hl
            ld   de,ROMFONT
            add  hl,de
            ex   de,hl
            pop  hl
            ld   b,8
gl_l:       ld   a,(de)
            ld   (hl),a
            inc  de
            inc  h
            djnz gl_l
            ret

; HL = texto ASCII (0), B = fila, C = columna: letras, cifras, espacio, '-' y ':'
print:      ld   a,(hl)
            or   a
            ret  z
            inc  hl
            push hl
            push bc
            ld   e,0                ; espacio y lo desconocido
            cp   '0'
            jr   c,pr_s
            cp   '9'+1
            jr   nc,pr_a
            sub  '0'-28
            ld   e,a
            jr   pr_g
pr_a:       cp   'A'
            jr   c,pr_s
            cp   'Z'+1
            jr   nc,pr_s
            sub  'A'-38
            ld   e,a
            jr   pr_g
pr_s:       cp   '-'
            jr   nz,pr_c
            ld   e,22
            jr   pr_g
pr_c:       cp   ':'
            jr   nz,pr_g
            ld   e,14
pr_g:       push de
            call gaddr
            pop  de
            ld   a,e
            call glyph
            pop  bc
            inc  c
            pop  hl
            jr   print

; B = fila de arriba, H = los bits altos del atributo (0, 40h, 80h, C0h), L = 0 si el color va en el papel o 1 si va en la tinta:
; 8 muestras de 4 x 2 celdas, una por color. Con el color en el papel el bitmap esta a cero; en la tinta, a $FF (el papel negro)
swatches:   ld   a,b
            ld   (SW_ROW),a
            ld   a,h
            ld   (SW_HI),a
            ld   a,l
            ld   (SW_INK),a
            xor  a
            ld   (SW_C),a
sw_c:       xor  a
            ld   (SW_R),a
sw_r:       xor  a
            ld   (SW_K),a
sw_k:       ld   a,(SW_R)           ; la celda (fila, columna)
            ld   hl,SW_ROW
            add  a,(hl)
            ld   b,a
            ld   a,(SW_C)
            add  a,a
            add  a,a
            ld   hl,SW_K
            add  a,(hl)
            ld   c,a
            push bc
            call gaddr              ; el bitmap
            ld   a,(SW_INK)
            neg
            ld   d,a                ; 0 (papel) o $FF (tinta)
            ld   e,8
sw_b:       ld   (hl),d
            inc  h
            dec  e
            jr   nz,sw_b
            pop  bc
            call gattr              ; el atributo
            ld   a,(SW_C)
            ld   d,a
            ld   a,(SW_INK)
            or   a
            ld   a,d
            jr   nz,sw_at
            add  a,a                ; papel: el color en los bits 3-5
            add  a,a
            add  a,a
sw_at:      ld   d,a
            ld   a,(SW_HI)
            or   d
            ld   (hl),a
            ld   hl,SW_K
            inc  (hl)
            ld   a,(hl)
            cp   4
            jr   nz,sw_k
            ld   hl,SW_R
            inc  (hl)
            ld   a,(hl)
            cp   2
            jr   nz,sw_r
            ld   hl,SW_C
            inc  (hl)
            ld   a,(hl)
            cp   8
            jr   nz,sw_c
            ret

t_title:    defm "COLOR TEST - HIRES SPECTRUM",0
t_p1:       defm "PAPER BRIGHT 1",0
t_p0:       defm "PAPER BRIGHT 0",0
t_i1:       defm "INK BRIGHT 1",0
t_i0:       defm "INK BRIGHT 0",0
t_f0:       defm "BIT 7 SET, BIT 6 CLEAR",0
t_f1:       defm "BIT 7 SET, BIT 6 SET",0
t_key:      defm "P - PALETTE, ANY OTHER KEY EXITS",0
t_plab:     defm "PALETTE:",0
t_pn:       defm "OFF     ",0
            defm "GREY    ",0
            defm "RED     ",0
            defm "GREEN   ",0
            defm "BLUE    ",0
            defm "YELLOW  ",0
            defm "CYAN    ",0
            defm "MAGENTA ",0

pal16:
            include "pal16.inc"
