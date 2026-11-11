; =====================================================================
;  PAINT.ASM - programa de dibujo con el raton Bluetooth del SD81 Booster, TODO en codigo maquina.
;
;  Dos modos de video, a elegir al empezar (POKE 24579) o desde el propio programa (ultimo boton):
;      0 = 160x100 a 256 colores (POKE 2045,175): un byte por pixel, paleta de 256 (la RGB 3-3-2 por defecto)
;      1 = 160x200 a 16 colores  (POKE 2045,176): un nibble por pixel, paleta de 16 (los colores digitales del Chroma81)
;
;  Pantalla: arriba una fila de 16 muestras de color (la rueda del raton cambia de banco de 16 en el modo de 256 colores) y debajo
;  una fila de 10 botones y la casilla de la ranura; el resto es el lienzo. Los botones, de izquierda a derecha:
;      lapiz | linea | rectangulo | rectangulo relleno | bote de pintura | cuentagotas | borrar lienzo | cambiar de modo (borra)
;      | guardar | cargar
;  La casilla de la derecha es la RANURA (0-9): izquierdo la sube, derecho la baja. Guardar y cargar usan PAINTn.I16 (160x100, un fichero
;  .I16 de verdad: "I160", paleta de 512 bytes y 16.000 pixeles; lo abre tambien el explorador) o PAINTn.I4 (160x200: "I400", paleta de 16
;  entradas, 32 bytes, y 16.000 bytes de pixeles) en la carpeta actual. El borde se pone verde si ha ido bien y rojo si no.
;  Al guardar, las dos barras se dejan en color papel: la imagen guardada no lleva interfaz.
;  Boton izquierdo: dibuja con la TINTA.  Boton derecho: dibuja con el PAPEL (el color de fondo, para borrar).
;  Sobre una muestra: izquierdo = tinta, derecho = papel.  Con el cuentagotas: izquierdo = tinta, derecho = papel, tomados del lienzo.
;  Linea y rectangulos: se arrastra y se dibujan al soltar. Tecla X: salir al BASIC.
;
;  Marcas: la tinta lleva un marco doble (negro y blanco) y el papel uno negro; el boton de la herramienta elegida se ve en amarillo.
;
;  Memoria: $6000- programa y mouselib (debe acabar antes de $7000);  $7000-$71FF la paleta;  $7800-$7DFF pila del relleno (semillas de 2 bytes);  $7E00- buzon (28 bytes);  $8000-$BE7F la imagen.
;  Entrada: RAND USR 24576  (con el modo en POKE 24579: 0 o 1).  No hay SLOW: el programa corre con DI, como los de EXAMPLES/MODE160.
;  Ensamblar: pasmo -I ..\MOUSE paint.asm paint.bin paint.sym   (compila.bat)
; =====================================================================

MS_BASE     equ 7E00h
ML_CLOCK    equ tick                ; el doble clic no se usa, pero la libreria lo mide con este byte (se incrementa cada fotograma)

IMG         equ 8000h
STK         equ 7800h
STKSZ       equ 0600h
PALBUF      equ 7000h               ; la paleta en uso (512 bytes en 160x100, 32 en 160x200): se guarda y se carga con el dibujo

DataPort    equ 0A7h                ; el MCU: ficheros de la SD
ClkPort     equ 0AFh
CMD_FOPEN   equ 53
CMD_FREAD   equ 55
CMD_FWRITE  equ 56
CMD_FCLOSE  equ 57
CMD_FCREATE equ 71
CHUNK       equ 512                 ; lo que cabe en un fread / fwrite

            org 24576

            jp   start
mode4:      defb 0                  ; 24579: 0 = 160x100 a 256 colores, 1 = 160x200 a 16 colores

start:      di
            ld   a,(mode4)
            and  1
            ld   (mode4),a
            call setmode
            call ml_cur_init

; ---------------------------------------------------------------------
;  Bucle principal: un fotograma por vuelta
; ---------------------------------------------------------------------
main:       call waitvs
            ld   hl,tick
            inc  (hl)
            call ml_poll
            call ml_cur_show

            ld   a,0FEh             ; tecla X
            in   a,(0FEh)
            bit  2,a
            jp   z,quit

            ld   a,(ML_WHEEL)       ; la rueda cambia de banco de 16 colores (solo en el modo de 256)
            or   a
            jr   z,m_nowh
            ld   a,(mode4)
            or   a
            jr   nz,m_nowh
            ld   a,(ML_WHEEL)
            ld   b,a
            ld   a,(bank)
            bit  7,b
            jr   nz,m_wd
            inc  a
            jr   m_ws
m_wd:       dec  a
m_ws:       and  0Fh
            ld   (bank),a
            call draw_ui
m_nowh:
            ld   a,(ML_PRESS)
            and  3
            jr   z,m_nopress
            ld   a,(MP_Y)
            ld   hl,cy0
            cp   (hl)
            jr   nc,m_cpress
            call ui_press
            jr   m_nopress
m_cpress:   call canvas_press
m_nopress:  call canvas_hold
            jp   main

quit:       call ml_cur_hide
            ld   bc,191*256+255     ; el rango por defecto del MCU para los demas programas
            call ml_init
            xor  a
            ld   (2069),a           ; sin texto de 40x25
            ld   (2070),a           ; ventana de imagen por defecto
            ld   a,25
            ld   (2071),a
            ld   a,85
            ld   (2045),a           ; modo ZX81 normal
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
;  setmode: pone los parametros del modo (mode4), la imagen en blanco, el raton y la interfaz.
; ---------------------------------------------------------------------
setmode:    ld   a,(mode4)
            ld   hl,parm0
            or   a
            jr   z,sm_p
            ld   hl,parm1
sm_p:       ld   de,hgt
            ld   bc,6
            ldir
            ld   a,(swh)
            ld   b,a
            ld   a,(tbh)
            add  a,b
            ld   (cy0),a            ; el lienzo empieza debajo de las dos barras
            ld   a,(hgt)
            dec  a
            ld   (ymaxv),a
            ld   a,(ui_wh)
            ld   (paper),a          ; papel blanco, tinta negra
            xor  a
            ld   (ink),a
            ld   (tool),a
            ld   (bank),a
            ld   (stroke),a
            ld   a,(mode4)
            or   a
            ld   a,160
            jr   z,sm_b
            ld   a,80
sm_b:       ld   (bpr),a            ; bytes por fila

            xor  a                  ; video: HFILE = $8000, colores activados, sin texto, sin ventana ni scroll
            ld   (2043),a
            ld   a,80h
            ld   (2044),a
            ld   bc,7FEFh
            ld   a,20h
            out  (c),a
            xor  a
            ld   (2069),a
            ld   (2070),a
            ld   (2072),a
            ld   (2073),a
            ld   (2074),a
            ld   (2075),a
            ld   a,25
            ld   (2071),a

            ld   a,(mode4)          ; paleta por defecto del modo
            or   a
            ld   hl,pal160
            ld   bc,512
            jr   z,sm_pal
            ld   hl,pal16
            ld   bc,32
sm_pal:     ld   (palsize),bc
            ld   de,PALBUF
            ldir
            xor  a
            ld   (2067),a           ; la paleta de 16 queda apagada: salen los colores por defecto del Chroma81
            ld   a,(mode4)
            or   a
            call z,setpal           ; en 160x100, la paleta de 256

            call calc_fill
            ld   d,0
            ld   a,(hgt)
            call clear_rows         ; toda la imagen en blanco

            ld   a,(hgt)
            dec  a
            ld   b,a                ; Y maximo del raton: la ultima fila
            ld   c,159
            call ml_init
            call draw_ui

            ld   a,(mode4)
            add  a,175              ; 175 = 160x100 a 256 colores, 176 = 160x200 a 16 colores
            ld   (2045),a
            ret

; calc_fill: fillb = el byte que llena la imagen de color papel (en 4 bpp, el nibble en los dos lados)
calc_fill:  ld   a,(paper)
            ld   b,a
            ld   a,(mode4)
            or   a
            ld   a,b
            jr   z,cf_s
            rlca
            rlca
            rlca
            rlca
            or   b
cf_s:       ld   (fillb),a
            ret

; clear_rows: D = primera fila, A = numero de filas (>0). Las llena con fillb.
clear_rows: ld   (cr_rows),a
            ld   e,0
            call addrm              ; HL = direccion de (x=0, y=D)
            push hl
            ld   a,(bpr)
            ld   e,a
            ld   d,0
            ld   hl,0
            ld   a,(cr_rows)
            ld   b,a
cr_m:       add  hl,de
            djnz cr_m               ; HL = filas * bytes por fila
            ld   b,h
            ld   c,l
            dec  bc
            pop  hl
            ld   a,(fillb)
            ld   (hl),a
            ld   d,h
            ld   e,l
            inc  de
            ldir
            ret

; ---------------------------------------------------------------------
;  Pixeles.  D = y, E = x.  Todas conservan BC y DE.
; ---------------------------------------------------------------------
addrm:      ld   a,(mode4)
            or   a
            jr   z,addr8
; addr4: HL = IMG + y*80 + x/2
addr4:      push bc
            push de
            ld   l,d
            ld   h,0
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl              ; y*16
            ld   b,h
            ld   c,l
            add  hl,hl
            add  hl,hl              ; y*64
            add  hl,bc              ; y*80
            ld   a,e
            srl  a
            ld   c,a
            ld   b,0
            add  hl,bc
            set  7,h                ; + $8000 (el resultado es menor que $4000)
            pop  de
            pop  bc
            ret
; addr8: HL = IMG + y*160 + x
addr8:      push bc
            push de
            ld   l,d
            ld   h,0
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl              ; y*32
            ld   b,h
            ld   c,l
            add  hl,hl
            add  hl,hl              ; y*128
            add  hl,bc              ; y*160
            ld   c,e
            ld   b,0
            add  hl,bc
            set  7,h
            pop  de
            pop  bc
            ret

; put_pixel: A = color
put_pixel:  push bc
            ld   c,a
            ld   a,(mode4)
            or   a
            jr   nz,pp4
            call addr8
            ld   (hl),c
            pop  bc
            ret
pp4:        call addr4
            ld   a,e
            and  1
            jr   nz,pp4l
            ld   a,c                ; x par: nibble alto
            and  0Fh
            rlca
            rlca
            rlca
            rlca
            ld   b,a
            ld   a,(hl)
            and  0Fh
            or   b
            ld   (hl),a
            pop  bc
            ret
pp4l:       ld   a,c                ; x impar: nibble bajo
            and  0Fh
            ld   b,a
            ld   a,(hl)
            and  0F0h
            or   b
            ld   (hl),a
            pop  bc
            ret

; get_pixel: devuelve A = color
get_pixel:  ld   a,(mode4)
            or   a
            jr   nz,gp4
            call addr8
            ld   a,(hl)
            ret
gp4:        call addr4
            ld   a,e
            rra                     ; bit 0 de x al carry
            ld   a,(hl)
            jr   c,gp4l
            rrca
            rrca
            rrca
            rrca
gp4l:       and  0Fh
            ret

; ---------------------------------------------------------------------
;  Figuras.  Color en (col); rectangulo en (rx0, ry0, rx1, ry1), con rx0<=rx1 y ry0<=ry1.
; ---------------------------------------------------------------------
; hspan: D = y, E = x0, B = x1 (>= x0), A = color. Conserva D.
hspan:      ld   c,a
hs_lp:      ld   a,c
            call put_pixel
            ld   a,e
            cp   b
            ret  z
            inc  e
            jr   hs_lp

; vspan: E = x, D = y0, B = y1 (>= y0), A = color. Conserva E.
vspan:      ld   c,a
vs_lp:      ld   a,c
            call put_pixel
            ld   a,d
            cp   b
            ret  z
            inc  d
            jr   vs_lp

frect:      ld   a,(ry0)
            ld   d,a
fr_lp:      ld   a,(rx0)
            ld   e,a
            ld   a,(rx1)
            ld   b,a
            ld   a,(col)
            call hspan
            ld   a,(ry1)
            cp   d
            ret  z
            inc  d
            jr   fr_lp

rect:       ld   a,(ry0)
            ld   d,a
            ld   a,(rx0)
            ld   e,a
            ld   a,(rx1)
            ld   b,a
            ld   a,(col)
            call hspan
            ld   a,(ry1)
            ld   d,a
            ld   a,(rx0)
            ld   e,a
            ld   a,(rx1)
            ld   b,a
            ld   a,(col)
            call hspan
            ld   a,(rx0)
            ld   e,a
            ld   a,(ry0)
            ld   d,a
            ld   a,(ry1)
            ld   b,a
            ld   a,(col)
            call vspan
            ld   a,(rx1)
            ld   e,a
            ld   a,(ry0)
            ld   d,a
            ld   a,(ry1)
            ld   b,a
            ld   a,(col)
            jp   vspan

; minmax: A y B -> B = el menor, C = el mayor
minmax:     cp   b
            jr   c,mm1
            ld   c,a
            ret
mm1:        ld   c,b
            ld   b,a
            ret

; norm_rect: (rx0..ry1) = rectangulo entre el punto inicial (sx0, sy0) y el cursor (cx, cy)
norm_rect:  ld   a,(sx0)
            ld   b,a
            ld   a,(cx)
            call minmax
            ld   a,b
            ld   (rx0),a
            ld   a,c
            ld   (rx1),a
            ld   a,(sy0)
            ld   b,a
            ld   a,(cy)
            call minmax
            ld   a,b
            ld   (ry0),a
            ld   a,c
            ld   (ry1),a
            ret

; line: Bresenham de (lx0, ly0) a (lx1, ly1) con el color (col). Estropea lx0 y ly0.
line:       ld   a,(lx1)
            ld   hl,lx0
            sub  (hl)
            ld   b,1
            jr   nc,ln1
            neg
            ld   b,0FFh
ln1:        ld   l,a
            ld   h,0
            ld   (ldx),hl           ; dx = |x1 - x0|
            ld   a,b
            ld   (lsx),a
            ld   a,(ly1)
            ld   hl,ly0
            sub  (hl)
            ld   b,1
            jr   nc,ln2
            neg
            ld   b,0FFh
ln2:        ld   e,a
            ld   d,0
            ld   hl,0
            or   a
            sbc  hl,de              ; dy = -|y1 - y0|
            ld   (ldy),hl
            ld   a,b
            ld   (lsy),a
            ld   hl,(ldx)
            ld   de,(ldy)
            add  hl,de
            ld   (lerr),hl          ; err = dx + dy
ln_loop:    ld   a,(lx0)
            ld   e,a
            ld   a,(ly0)
            ld   d,a
            ld   a,(col)
            call put_pixel
            ld   a,(lx0)
            ld   hl,lx1
            cp   (hl)
            jr   nz,ln_step
            ld   a,(ly0)
            ld   hl,ly1
            cp   (hl)
            ret  z
ln_step:    ld   hl,(lerr)
            add  hl,hl
            ld   (le2),hl           ; e2 = 2 * err
            ld   de,(ldy)
            or   a
            sbc  hl,de              ; e2 - dy
            jp   m,ln_nox
            ld   hl,(lerr)
            ld   de,(ldy)
            add  hl,de
            ld   (lerr),hl
            ld   a,(lsx)
            ld   hl,lx0
            add  a,(hl)
            ld   (hl),a
ln_nox:     ld   hl,(ldx)
            ld   de,(le2)
            or   a
            sbc  hl,de              ; dx - e2
            jp   m,ln_loop
            ld   hl,(lerr)
            ld   de,(ldx)
            add  hl,de
            ld   (lerr),hl
            ld   a,(lsy)
            ld   hl,ly0
            add  a,(hl)
            ld   (hl),a
            jr   ln_loop

; ---------------------------------------------------------------------
;  Relleno: A = color nuevo, D = y, E = x de la semilla. Por tramos, con una pila de semillas (x, y) en STK.
; ---------------------------------------------------------------------
fill:       ld   (fl_new),a
            call get_pixel
            ld   hl,fl_new
            cp   (hl)
            ret  z                  ; ya es de ese color
            ld   (fl_tgt),a
            ld   hl,STK
            ld   (fl_sp),hl
            call fpush
fl_main:    ld   hl,(fl_sp)
            ld   de,STK
            or   a
            sbc  hl,de
            ret  z                  ; pila vacia: terminado
            call fpop               ; D = y, E = x
            ld   a,e
            ld   (fl_x),a
            ld   a,d
            ld   (fl_y),a
            call get_pixel
            ld   hl,fl_tgt
            cp   (hl)
            jr   nz,fl_main         ; otra semilla ya lo habia llenado
            ld   a,(fl_x)
            ld   b,a                ; B = extremo izquierdo
fl_l:       ld   a,b
            or   a
            jr   z,fl_lx
            dec  a
            ld   e,a
            call get_pixel
            ld   hl,fl_tgt
            cp   (hl)
            jr   nz,fl_lx
            dec  b
            jr   fl_l
fl_lx:      ld   a,(fl_x)
            ld   c,a                ; C = extremo derecho
fl_r:       ld   a,c
            cp   159
            jr   z,fl_rx
            inc  a
            ld   e,a
            call get_pixel
            ld   hl,fl_tgt
            cp   (hl)
            jr   nz,fl_rx
            inc  c
            jr   fl_r
fl_rx:      ld   a,b
            ld   (fl_xl),a
            ld   a,c
            ld   (fl_xr),a
            ld   e,b
            ld   b,c                ; hspan: E = x0, B = x1
            ld   a,(fl_new)
            call hspan
            ld   a,(fl_y)           ; semillas en la fila de arriba (si no es la primera del lienzo)
            ld   hl,cy0
            cp   (hl)
            jr   z,fl_dn
            dec  a
            ld   d,a
            call fscan
fl_dn:      ld   a,(fl_y)           ; y en la de abajo
            ld   hl,ymaxv
            cp   (hl)
            jp   z,fl_main
            inc  a
            ld   d,a
            call fscan
            jp   fl_main

; fscan: D = fila; pone una semilla al principio de cada tramo de color objetivo entre fl_xl y fl_xr
fscan:      ld   a,(fl_xl)
            ld   e,a
            ld   b,0                ; B = 1 si estamos dentro de un tramo
fs_lp:      call get_pixel
            ld   hl,fl_tgt
            cp   (hl)
            jr   nz,fs_no
            ld   a,b
            or   a
            jr   nz,fs_nx
            ld   b,1
            call fpush
            jr   fs_nx
fs_no:      ld   b,0
fs_nx:      ld   a,(fl_xr)
            cp   e
            ret  z
            inc  e
            jr   fs_lp

; fpush: guarda la semilla (E = x, D = y); si la pila esta llena la descarta. Conserva BC y DE.
fpush:      push de
            ld   hl,(fl_sp)
            ld   de,STK+STKSZ
            or   a
            sbc  hl,de
            pop  de
            ret  nc
            ld   hl,(fl_sp)
            ld   (hl),e
            inc  hl
            ld   (hl),d
            inc  hl
            ld   (fl_sp),hl
            ret
; fpop: D = y, E = x de la ultima semilla
fpop:       ld   hl,(fl_sp)
            dec  hl
            ld   d,(hl)
            dec  hl
            ld   e,(hl)
            ld   (fl_sp),hl
            ret

; ---------------------------------------------------------------------
;  Interfaz
; ---------------------------------------------------------------------
; draw_ui: las 16 muestras, sus marcas, y los 8 botones
draw_ui:    xor  a
            ld   (ui_i),a
du_sw:      ld   a,(ui_i)
            ld   c,a
            add  a,a
            ld   b,a                ; 2i
            add  a,a
            add  a,a                ; 8i
            add  a,b                ; 10i
            ld   (rx0),a
            add  a,9
            ld   (rx1),a
            xor  a
            ld   (ry0),a
            ld   a,(swh)
            dec  a
            ld   (ry1),a
            ld   b,c                ; B = i
            ld   a,(mode4)
            or   a
            ld   a,b
            jr   nz,du_c
            ld   a,(bank)           ; 256 colores: banco * 16 + i
            add  a,a
            add  a,a
            add  a,a
            add  a,a
            or   b
du_c:       ld   (col),a
            call frect
            ld   a,(ui_i)
            inc  a
            ld   (ui_i),a
            cp   16
            jr   nz,du_sw

            ld   a,(paper)
            ld   c,0
            call sw_frame           ; marco negro: el papel
            ld   a,(ink)
            ld   c,1
            call sw_frame           ; marco doble: la tinta

            xor  a
            ld   (ui_i),a
du_tb:      ld   a,(ui_i)
            add  a,a
            ld   b,a                ; 2i
            add  a,a
            ld   c,a                ; 4i
            add  a,a                ; 8i
            add  a,c
            add  a,b                ; 14i: cada boton ocupa 14 pixeles
            ld   (rx0),a
            add  a,13
            ld   b,a
            ld   a,(ui_i)
            cp   10
            ld   a,b
            jr   nz,du_x1
            ld   a,159              ; la casilla de la ranura llega al borde
du_x1:      ld   (rx1),a
            ld   a,(swh)
            ld   (ry0),a
            ld   b,a
            ld   a,(tbh)
            add  a,b
            dec  a
            ld   (ry1),a
            ld   a,(tool)
            ld   hl,ui_i
            cp   (hl)
            ld   a,(ui_bg)
            jr   nz,du_t1
            ld   a,(ui_sel)         ; la herramienta elegida, en amarillo
du_t1:      ld   (col),a
            call frect
            ld   a,(ui_i)
            cp   10
            jr   z,du_slot
            add  a,a
            add  a,a
            add  a,a
            ld   e,a
            ld   d,0
            ld   hl,icons
            add  hl,de
            ld   a,(rx0)
            add  a,3
            jr   du_gl
du_slot:    ld   a,(slot)           ; la casilla de la ranura: su digito
            add  a,a
            add  a,a
            add  a,a
            ld   e,a
            ld   d,0
            ld   hl,digits
            add  hl,de
            ld   a,(rx0)
            add  a,6
du_gl:      ld   (ic_x),a
            call draw_glyph
            ld   a,(ui_i)
            inc  a
            ld   (ui_i),a
            cp   11
            jr   nz,du_tb
            ret

; sw_frame: A = color, C = 1 marco doble / 0 sencillo. Si el color no esta en el banco que se ve, no hace nada.
sw_frame:   ld   b,a
            ld   a,c
            ld   (sf_dbl),a
            ld   a,(mode4)
            or   a
            ld   a,b
            jr   nz,sf_i
            ld   e,a
            rrca
            rrca
            rrca
            rrca
            and  0Fh
            ld   hl,bank
            cp   (hl)
            ret  nz
            ld   a,e
            and  0Fh
sf_i:       ld   b,a
            add  a,a
            ld   d,a
            add  a,a
            add  a,a
            add  a,d
            ld   (rx0),a
            add  a,9
            ld   (rx1),a
            xor  a
            ld   (ry0),a
            ld   a,(swh)
            dec  a
            ld   (ry1),a
            xor  a
            ld   (col),a
            call rect               ; marco negro
            ld   a,(sf_dbl)
            or   a
            ret  z
            ld   a,(rx0)            ; y dentro, otro blanco
            inc  a
            ld   (rx0),a
            ld   a,(rx1)
            dec  a
            ld   (rx1),a
            ld   a,(ry0)
            inc  a
            ld   (ry0),a
            ld   a,(ry1)
            dec  a
            ld   (ry1),a
            ld   a,(ui_wh)
            ld   (col),a
            jp   rect

; draw_glyph: HL = 8 bytes (una fila por byte), ic_x = columna, el boton en (ry0). En 160x200 las filas se duplican. Negro.
draw_glyph: ld   a,(mode4)
            inc  a
            ld   (ic_sy),a          ; 1 o 2
            ld   b,a
            ld   a,(ry0)
            add  a,b
            ld   (ic_y),a
            ld   b,8
ic_row:     push bc
            ld   c,(hl)
            inc  hl
            push hl
            ld   b,8
            ld   a,(ic_x)
            ld   e,a
ic_col:     rl   c
            jr   nc,ic_skip
            push bc
            ld   a,(ic_y)
            ld   d,a
            ld   a,(ic_sy)
            ld   b,a
ic_k:       xor  a
            call put_pixel
            inc  d
            djnz ic_k
            pop  bc
ic_skip:    inc  e
            djnz ic_col
            ld   a,(ic_sy)
            ld   b,a
            ld   a,(ic_y)
            add  a,b
            ld   (ic_y),a
            pop  hl
            pop  bc
            djnz ic_row
            ret

; ui_press: clic en las barras. Muestra: izquierdo = tinta, derecho = papel. Botones: elige herramienta o ejecuta.
ui_press:   ld   a,(MP_Y)
            ld   hl,swh
            cp   (hl)
            jr   nc,up_tools
            ld   a,(MP_X)           ; muestra i = x / 10
            ld   b,0
up_d:       cp   10
            jr   c,up_dd
            sub  10
            inc  b
            jr   up_d
up_dd:      ld   a,(mode4)
            or   a
            ld   a,b
            jr   nz,up_s
            ld   a,(bank)
            add  a,a
            add  a,a
            add  a,a
            add  a,a
            or   b
up_s:       ld   c,a
            ld   a,(ML_PRESS)
            rra
            jr   nc,up_right
            ld   a,c
            ld   (ink),a
            jp   draw_ui
up_right:   ld   a,c
            ld   (paper),a
            jp   draw_ui

up_tools:   ld   a,(MP_X)           ; boton t = x / 20
            ld   b,0
up_t:       cp   14
            jr   c,up_tt
            sub  14
            inc  b
            jr   up_t
up_tt:      ld   a,b
            cp   6
            jr   c,up_sel
            jr   z,up_clear
            cp   8
            jr   c,up_mode
            jp   z,do_save
            cp   10
            jp   c,do_load
            ld   a,(slot)           ; 10: la casilla de la ranura
            ld   b,a
            ld   a,(ML_PRESS)
            rra
            ld   a,b
            jr   nc,up_sdn
            inc  a
            cp   10
            jr   c,up_sset
            xor  a
            jr   up_sset
up_sdn:     or   a
            jr   nz,up_sd1
            ld   a,10
up_sd1:     dec  a
up_sset:    ld   (slot),a
            jp   draw_ui
up_mode:    ld   a,(mode4)          ; 7: cambiar de modo (borra el lienzo)
            xor  1
            ld   (mode4),a
            jp   setmode
up_clear:   call calc_fill
            ld   a,(cy0)
            ld   d,a
            ld   a,(hgt)
            sub  d
            jp   clear_rows
up_sel:     ld   (tool),a
            jp   draw_ui

; ---------------------------------------------------------------------
;  Lienzo
; ---------------------------------------------------------------------
; getcur: (cx, cy) = el puntero recortado al lienzo
getcur:     ld   a,(MP_X)
            cp   160
            jr   c,gc1
            ld   a,159
gc1:        ld   (cx),a
            ld   a,(MP_Y)
            ld   hl,cy0
            cp   (hl)
            jr   nc,gc2
            ld   a,(hl)
gc2:        ld   hl,ymaxv
            cp   (hl)
            jr   z,gc3
            jr   c,gc3
            ld   a,(hl)
gc3:        ld   (cy),a
            ret

canvas_press:
            call getcur
            ld   a,(ML_PRESS)
            rra
            jr   nc,cp_r
            ld   a,(ink)
            ld   b,1
            jr   cp_go
cp_r:       ld   a,(paper)
            ld   b,2
cp_go:      ld   (scol),a
            ld   a,b
            ld   (smask),a
            ld   a,(tool)
            or   a
            jr   z,cp_pen
            cp   4
            jr   z,cp_fill
            cp   5
            jr   z,cp_pick
            ld   a,(cx)             ; linea y rectangulos: se recuerda el punto inicial
            ld   (sx0),a
            ld   a,(cy)
            ld   (sy0),a
            ld   a,(smask)
            ld   (stroke),a
            ret
cp_pen:     ld   a,(cx)
            ld   (px),a
            ld   e,a
            ld   a,(cy)
            ld   (py),a
            ld   d,a
            ld   a,(scol)
            call put_pixel
            ld   a,(smask)
            ld   (stroke),a
            ret
cp_fill:    ld   a,(cx)
            ld   e,a
            ld   a,(cy)
            ld   d,a
            ld   a,(scol)
            jp   fill
cp_pick:    ld   a,(cx)
            ld   e,a
            ld   a,(cy)
            ld   d,a
            call get_pixel
            ld   c,a
            ld   a,(smask)
            rra
            jr   nc,pk_r
            ld   a,c
            ld   (ink),a
            ld   a,(mode4)
            or   a
            jr   nz,pk_d
            ld   a,c                ; en 256 colores, el banco pasa a ser el del color tomado
            rrca
            rrca
            rrca
            rrca
            and  0Fh
            ld   (bank),a
            jr   pk_d
pk_r:       ld   a,c
            ld   (paper),a
pk_d:       jp   draw_ui

canvas_hold:
            ld   a,(stroke)
            or   a
            ret  z
            ld   b,a
            ld   a,(ML_BTN)
            and  b
            jr   z,ch_rel
            ld   a,(tool)           ; con el boton pulsado: solo el lapiz dibuja (une cada punto con el anterior)
            or   a
            ret  nz
            call getcur
            ld   a,(px)
            ld   (lx0),a
            ld   a,(py)
            ld   (ly0),a
            ld   a,(cx)
            ld   (lx1),a
            ld   (px),a
            ld   a,(cy)
            ld   (ly1),a
            ld   (py),a
            ld   a,(scol)
            ld   (col),a
            jp   line
ch_rel:     xor  a                  ; soltado: se dibuja la figura
            ld   (stroke),a
            ld   a,(tool)
            or   a
            ret  z
            call getcur
            ld   a,(scol)
            ld   (col),a
            ld   a,(tool)
            cp   1
            jr   nz,ch_r2
            ld   a,(sx0)
            ld   (lx0),a
            ld   a,(sy0)
            ld   (ly0),a
            ld   a,(cx)
            ld   (lx1),a
            ld   a,(cy)
            ld   (ly1),a
            jp   line
ch_r2:      call norm_rect
            ld   a,(tool)
            cp   2
            jp   z,rect
            jp   frect

; ---------------------------------------------------------------------
;  Guardar y cargar (la SD, por el MCU)
; ---------------------------------------------------------------------
; mk_name: nm_buf = PAINTn.I16 (160x100) o PAINTn.I4 (160x200), con n = la ranura. Devuelve HL = nm_buf, B = longitud.
mk_name:    ld   a,(mode4)
            or   a
            ld   hl,name0
            ld   b,10
            jr   z,mn_c
            ld   hl,name1
            ld   b,9
mn_c:       ld   a,b
            ld   (nm_len),a
            ld   c,b
            ld   b,0
            ld   de,nm_buf
            ldir
            ld   a,(slot)
            add  a,'0'
            ld   (nm_buf+5),a
            ld   hl,nm_buf
            ld   a,(nm_len)
            ld   b,a
            ret

; blank_bar: las dos barras en color papel, para que la imagen guardada no lleve interfaz
blank_bar:  call calc_fill
            ld   d,0
            ld   a,(cy0)
            jp   clear_rows

do_save:    call blank_bar
            call mk_name
            ld   a,CMD_FCREATE
            call f_name_cmd
            cp   0FFh
            jr   z,sv_bad
            ld   (fhandle),a
            xor  a
            ld   (ferr),a
            ld   hl,magic0
            ld   a,(mode4)
            or   a
            jr   z,sv_m
            ld   hl,magic1
sv_m:       ld   bc,4
            call f_write_n
            ld   hl,PALBUF
            ld   bc,(palsize)
            call f_write_n
            ld   hl,IMG
            ld   bc,16000
            call f_write_n
            ld   a,(fhandle)
            call f_close
            call draw_ui
            ld   a,(ferr)
            or   a
            jp   nz,flash_err
            jp   flash_ok
sv_bad:     call draw_ui
            jp   flash_err

do_load:    call mk_name
            ld   a,CMD_FOPEN
            call f_name_cmd
            cp   0FFh
            jp   z,flash_err
            ld   (fhandle),a
            ld   hl,hdrbuf
            ld   bc,4
            call f_read_n
            ld   hl,hdrbuf
            ld   de,magic0
            ld   a,(mode4)
            or   a
            jr   z,ld_m
            ld   de,magic1
ld_m:       ld   b,4
ld_c:       ld   a,(de)
            cp   (hl)
            jr   nz,ld_bad          ; no es un fichero de este modo
            inc  hl
            inc  de
            djnz ld_c
            ld   hl,PALBUF
            ld   bc,(palsize)
            call f_read_n
            ld   hl,IMG
            ld   bc,16000
            call f_read_n
            ld   a,(fhandle)
            call f_close
            call setpal
            call draw_ui
            jp   flash_ok
ld_bad:     ld   a,(fhandle)
            call f_close
            jp   flash_err

; setpal: la paleta de PALBUF al hardware (256 entradas en 160x100; 16 y activada en 160x200)
setpal:     xor  a
            ld   (2063),a
            ld   hl,PALBUF
            ld   a,(mode4)
            or   a
            jr   nz,sp16
            ld   b,0
sp_l:       ld   a,(hl)
            ld   (2064),a
            inc  hl
            ld   a,(hl)
            ld   (2065),a
            inc  hl
            djnz sp_l
            ret
sp16:       ld   b,16
sp16l:      ld   a,(hl)
            ld   (2064),a
            inc  hl
            ld   a,(hl)
            ld   (2066),a
            inc  hl
            djnz sp16l
            ld   a,1
            ld   (2067),a
            ret

; el borde avisa: verde = bien, rojo = mal (medio segundo)
flash_ok:   ld   a,24h
            jr   fl_go
flash_err:  ld   a,22h
fl_go:      ld   bc,7FEFh
            out  (c),a
            ld   b,25
fl_w:       call waitvs
            djnz fl_w
            ld   bc,7FEFh
            ld   a,20h
            out  (c),a              ; color encendido, borde negro
            ret

; --- ordenes de fichero del MCU ---
; handshake: cada llamada mira el reloj del MCU; BC, DE y HL se conservan (A no sobrevive a mcu_send)
mcu_send:   push bc
            ld   b,a
            in   a,(ClkPort)
            ld   c,a
            ld   a,b
            out  (DataPort),a
msw:        in   a,(ClkPort)
            xor  c
            jp   p,msw
            pop  bc
            ret

mcu_recv:   push bc
            in   a,(ClkPort)
            ld   c,a
            in   a,(DataPort)
            ld   b,a
mrw:        in   a,(ClkPort)
            xor  c
            jp   p,mrw
            ld   a,b
            pop  bc
            ret

; A = CMD_FOPEN o CMD_FCREATE, HL = nombre, B = longitud -> A = manejador ($FF si no se puede)
f_name_cmd: call mcu_send
            ld   a,b
            call mcu_send
fnc1:       ld   a,(hl)
            inc  hl
            call mcu_send
            djnz fnc1
            jp   mcu_recv

f_close:    ld   c,a
            ld   a,CMD_FCLOSE
            call mcu_send
            ld   a,c
            call mcu_send
            jp   mcu_recv

; f_read: A = manejador, DE = cuantos (<= CHUNK), HL = destino -> HL avanza; A = estado
f_read:     ld   c,a
            ld   a,CMD_FREAD
            call mcu_send
            ld   a,c
            call mcu_send
            ld   a,e
            call mcu_send
            ld   a,d
            call mcu_send
fr1:        call mcu_recv
            ld   (hl),a
            inc  hl
            dec  de
            ld   a,d
            or   e
            jr   nz,fr1
            jp   mcu_recv

; f_read_n: (fhandle), HL = destino, BC = cuantos (cualquier cantidad)
f_read_n:   ld   (fx_ptr),hl
            ld   (fx_cnt),bc
frn_l:      ld   bc,(fx_cnt)
            ld   a,b
            or   c
            ret  z
            ld   hl,CHUNK
            or   a
            sbc  hl,bc
            jr   c,frn_full
            ld   d,b
            ld   e,c
            jr   frn_go
frn_full:   ld   de,CHUNK
frn_go:     push de
            ld   hl,(fx_ptr)
            ld   a,(fhandle)
            call f_read
            ld   (fx_ptr),hl
            pop  de
            ld   hl,(fx_cnt)
            or   a
            sbc  hl,de
            ld   (fx_cnt),hl
            jr   frn_l

; f_write: A = manejador, DE = cuantos (<= CHUNK), HL = origen -> HL avanza; A = estado (0 = bien)
f_write:    ld   c,a
            ld   a,CMD_FWRITE
            call mcu_send
            ld   a,c
            call mcu_send
            ld   a,e
            call mcu_send
            ld   a,d
            call mcu_send
fw1:        ld   a,(hl)
            inc  hl
            call mcu_send
            dec  de
            ld   a,d
            or   e
            jr   nz,fw1
            jp   mcu_recv

; f_write_n: (fhandle), HL = origen, BC = cuantos; si el MCU da un error, queda en (ferr)
f_write_n:  ld   (fx_ptr),hl
            ld   (fx_cnt),bc
fwn_l:      ld   bc,(fx_cnt)
            ld   a,b
            or   c
            ret  z
            ld   hl,CHUNK
            or   a
            sbc  hl,bc
            jr   c,fwn_full
            ld   d,b
            ld   e,c
            jr   fwn_go
fwn_full:   ld   de,CHUNK
fwn_go:     push de
            ld   hl,(fx_ptr)
            ld   a,(fhandle)
            call f_write
            or   a
            jr   z,fwn_ok
            ld   (ferr),a
fwn_ok:     ld   (fx_ptr),hl
            pop  de
            ld   hl,(fx_cnt)
            or   a
            sbc  hl,de
            ld   (fx_cnt),hl
            jr   fwn_l

; ---------------------------------------------------------------------
;  Datos
; ---------------------------------------------------------------------
; parametros de cada modo, en el orden de las variables hgt..ui_wh
parm0:      defb 100,6,10,119,126,127       ; 160x100: alto, alto de las muestras, alto de los botones, fondo gris, amarillo, blanco (de la fila 7 de pal160: los colores del Chroma81)
parm1:      defb 200,12,20,7,14,15          ; 160x200: los colores digitales del Chroma81 (7 = gris claro, 14 = amarillo brillante, 15 = blanco brillante)

; iconos de 8x8: lapiz, linea, rectangulo, rectangulo relleno, bote, cuentagotas, borrar, modo, guardar, cargar
icons:      defb 003h,007h,00Eh,01Ch,038h,070h,0E0h,0C0h
            defb 001h,002h,004h,008h,010h,020h,040h,080h
            defb 0FFh,081h,081h,081h,081h,081h,081h,0FFh
            defb 0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh
            defb 018h,018h,03Ch,03Ch,07Eh,07Eh,07Eh,03Ch
            defb 018h,018h,018h,0FFh,0FFh,018h,018h,018h
            defb 081h,042h,024h,018h,018h,024h,042h,081h
            defb 081h,0C3h,0A5h,099h,081h,081h,081h,081h
            defb 018h,018h,018h,07Eh,03Ch,018h,000h,0FFh     ; guardar: flecha hacia la bandeja
            defb 018h,03Ch,07Eh,018h,018h,018h,000h,0FFh     ; cargar: flecha hacia arriba

name0:      defb "PAINT0.I16"
name1:      defb "PAINT0.I4"
magic0:     defb "I160"
magic1:     defb "I400"
; digitos de la ranura, 8x8
digits:     defb 070h,088h,098h,0A8h,0C8h,088h,070h,000h
            defb 020h,060h,020h,020h,020h,020h,070h,000h
            defb 070h,088h,008h,010h,020h,040h,0F8h,000h
            defb 0F0h,008h,008h,070h,008h,008h,0F0h,000h
            defb 010h,030h,050h,090h,0F8h,010h,010h,000h
            defb 0F8h,080h,0F0h,008h,008h,088h,070h,000h
            defb 030h,040h,080h,0F0h,088h,088h,070h,000h
            defb 0F8h,008h,010h,020h,040h,040h,040h,000h
            defb 070h,088h,088h,070h,088h,088h,070h,000h
            defb 070h,088h,088h,078h,008h,010h,060h,000h
; paleta de 16 por defecto (los colores digitales del Chroma81): por entrada, byte bajo {G,B} y byte alto {BRIGHT,R}
pal16:      defb 000h,000h,00Fh,000h,000h,00Fh,00Fh,00Fh,0F0h,000h,0FFh,000h,0F0h,00Fh,0FFh,00Fh
            defb 000h,0F0h,00Fh,0F0h,000h,0FFh,00Fh,0FFh,0F0h,0F0h,0FFh,0F0h,0F0h,0FFh,0FFh,0FFh
; paleta de 256 (EXAMPLES/MODE160/pal160.py): grises, rojo, verde, azul, amarillo, cian, magenta, los 16 del Chroma81 y una rueda de tonos
pal160:     incbin "pal160.bin"

hgt:        defb 100                ; alto de la imagen (estas seis, contiguas, las copia setmode)
swh:        defb 6                  ; alto de la fila de muestras
tbh:        defb 10                 ; alto de la fila de botones
ui_bg:      defb 0                  ; color de fondo de los botones
ui_sel:     defb 0                  ; color del boton elegido
ui_wh:      defb 0                  ; blanco
cy0:        defb 16                 ; primera fila del lienzo
ymaxv:      defb 99                 ; ultima fila
bpr:        defb 160                ; bytes por fila
fillb:      defb 0
ink:        defb 0
paper:      defb 0
tool:       defb 0
bank:       defb 0
stroke:     defb 0                  ; 0 = nada, 1 / 2 = boton con el que se dibuja
smask:      defb 0
scol:       defb 0                  ; color del trazo en curso
col:        defb 0                  ; color de las rutinas de figuras
cx:         defb 0
cy:         defb 0
px:         defb 0
py:         defb 0
sx0:        defb 0
sy0:        defb 0
rx0:        defb 0
ry0:        defb 0
rx1:        defb 0
ry1:        defb 0
lx0:        defb 0
ly0:        defb 0
lx1:        defb 0
ly1:        defb 0
lsx:        defb 0
lsy:        defb 0
ldx:        defw 0
ldy:        defw 0
lerr:       defw 0
le2:        defw 0
fl_sp:      defw 0
fl_new:     defb 0
fl_tgt:     defb 0
fl_x:       defb 0
fl_y:       defb 0
fl_xl:      defb 0
fl_xr:      defb 0
ui_i:       defb 0
sf_dbl:     defb 0
ic_x:       defb 0
ic_y:       defb 0
ic_sy:      defb 0
cr_rows:    defb 0
tick:       defb 0
slot:       defb 0                  ; ranura 0-9 de guardar y cargar
palsize:    defw 512
fhandle:    defb 0
ferr:       defb 0
fx_ptr:     defw 0
fx_cnt:     defw 0
nm_len:     defb 0
nm_buf:     defs 16
hdrbuf:     defs 4

            include "mouselib.inc"
