; =============================================================
; SCROLLM.ASM -- prueba del MOTOR de scroll por hardware (fases 2 y 3)
;
; Scroll CONTINUO horizontal y vertical en Superfast HiRes nativo (POKE 2045,171) o
; Spectrum (172): el fino (X: POKE 2090; Y: registro 8 del puerto $3FEF; 0-7 px) y,
; cuando llega al extremo, la orden de movimiento por el puerto del depurador
; ($3FEF, registro 9) que mueve la memoria UN byte (horizontal) o UNA FILA de texto
; (vertical). Ver claude/planning/scroll_hardware.md.
;
; El mundo es una pantalla infinita (se repite cada 256 columnas) de
; diagonales negras de un pixel de ancho sobre colores. Las diagonales
; cruzan los limites de byte, asi que cualquier junta mal puesta se ve como
; un escalon. El color de fondo depende de la columna del mundo y, segun el
; modo de atributos, de la linea (HiRes modo 0, 8x1) o de la fila (HiRes
; modo 1 y Spectrum, 8x8).
;
; Protocolo de un paso a la izquierda (tecla P):
;   cuadro n    el fino llega a 7 y SE MANDA la orden "izquierda" (OUT $41,1)
;   (el motor la ejecuta en el borrado, al final del cuadro n)
;   cuadro n+1  si el estado (OUT $20, IN) ya no esta ocupado: el fino vuelve
;               a 0 y el Z80 rellena el bufer de la derecha con la columna que
;               entra. El bufer de la izquierda ya esta bien: es el byte que ha
;               salido, que el motor guarda solo
; A la derecha (tecla O) lo mismo al reves: la orden se manda cuando el fino
; llega a 0 y, al acabar, el fino pasa a 7 y se rellena el bufer de la izquierda
; (en HiRes modo 1 tambien la columna 0 de la rejilla de atributos, que no tiene
; bufer de la izquierda). El de la derecha es el byte que ha salido.
;
; Vertical: igual con el fino Y. Arriba (Q): el fino sube a 7 y se manda la orden 3; al acabar, el fino
; vuelve a 0 y el Z80 rellena el bufer de ABAJO (B) y la esquina, y la columna L de la fila que ha entrado.
; Abajo (A): la orden 4 con el fino en 0; al acabar, fino a 7 y se rellena el bufer de ARRIBA (T) y las
; columnas R y L de la fila que ha entrado (en HiRes modo 1, la fila entera de la rejilla). El bufer
; opuesto es la fila que ha salido, que el motor guarda solo. Despues de cada movimiento HORIZONTAL
; hay que rellenar B y T, porque el motor horizontal no los mueve (se hace al empezar el siguiente paso vertical).
; El mundo se repite cada 256 filas; una fila abajo equivale a una columna a la derecha (las diagonales).
;
; Teclas:  P = izquierda (se repite)   O = derecha (se repite)
;          Q = arriba (se repite)      A = abajo (se repite); con P / O hacen diagonales
;          V = el automatico (SPACE) va arriba / abajo en vez de izquierda / derecha
;          M = atributos de HiRes modo 0 / modo 1 (se redibuja todo)
;          T = HiRes / Spectrum (se redibuja todo)
;          H = HUD: las filas 0 y 23 no se mueven (mascara de filas, POKE 2091-2093; region vertical 1-22)
;          D = doble buffer AUTO (front = bloque 7) si / no
;          S = dos sprites fijos si / no
;          N = ruido: el Z80 escribe sin parar en una zona libre de la pantalla,
;              tambien mientras corre el motor
;          SPACE = automatico (160 cuadros a la izquierda, 160 a la derecha, ...)
;          X = salir a BASIC
;
; Memoria (HFILE = $8000)
;   HiRes:    bitmap $8000-$97FF, entrada derecha $9800, entrada izquierda $9C00 (192 bytes
;             cada una); atributos de modo 0 en $C000 (192 x 32), sus bufers en $D800 y $DC00;
;             atributos de modo 1: 25 filas de 33 bytes desde $A000 (base 2059/2060 = $9FFF)
;   Spectrum: bitmap entrelazado $8000-$97FF, atributos $9800-$9AFF (24 x 32), entrada derecha
;             $9B00 (bitmap) y $9BC0 (24 atributos), entrada izquierda $9C00 y $9CC0
;   Bufers verticales, en el area que sobra de cada bloque de 8 KB:
;     HiRes:    B bitmap $9900 (8 lineas de 32), esquina $9A00 (8), T bitmap $9D00;
;               modo 0: B atributos $D900, esquina $DA00, T atributos $DD00; modo 1: B = fila 24 de la rejilla
;     Spectrum: B bitmap $9D00, esquina $9E40, T bitmap $9F00; atributos B $9E00, T $9E20, esquina $9E48
;   Ruido: $9E80-$9EFF.  Tablas de trabajo del programa desde $A400.
;
; Necesita RAM en $A000-$DFFF (no vale el modo 32K).
; Ensamblar: pasmo scrollm.asm scrollm.bin scrollm.sym
; Cargar desde BASIC: SCROLLM.B81 (LOAD FAST "SCROLLM.BIN" CODE 30000)
; =============================================================

        org 30000

DBGPORT equ 3FEFh               ; puerto del depurador: OUT $41 + dato = orden del motor; OUT $20 + IN = su estado
BM      equ 8000h               ; bitmap (HFILE = $8000)
RBM     equ 9800h               ; HiRes: entrada derecha, bitmap (192, uno por linea)
LBM     equ 9C00h               ; HiRes: entrada izquierda, bitmap (192)
SPA     equ 9800h               ; Spectrum: atributos (24 x 32)
SPRB    equ 9B00h               ; Spectrum: entrada derecha, bitmap (192)
SPRA    equ 9BC0h               ; Spectrum: entrada derecha, atributos (24)
SPLB    equ 9C00h               ; Spectrum: entrada izquierda, bitmap (192)
SPLA    equ 9CC0h               ; Spectrum: entrada izquierda, atributos (24)
NOISE   equ 9E80h               ; zona libre donde escribe el ruido (128 bytes)
A1      equ 0A000h              ; HiRes modo 1: 25 filas de 33 (base 2059/2060 = $9FFF, el modo 1 suma 1)
A0      equ 0C000h              ; HiRes modo 0: un atributo por byte de linea (6144)
RA0     equ 0D800h              ; entrada derecha, modo 0 (192)
LA0     equ 0DC00h              ; entrada izquierda, modo 0 (192)
BMC     equ 0A400h              ; tabla: la columna de bitmap del mundo, 2 paridades x 192
A0C     equ 0A600h              ; tabla: atributos de HiRes modo 0 de una columna, 8 x 192
A1C     equ 0AC00h              ; tabla: atributos de HiRes modo 1 de una columna, 8 x 24
BMT     equ 0AD00h              ; tabla de trabajo: 2 x 16 bytes del dibujo
A1S     equ 0AE00h              ; tabla: atributos Spectrum de una columna, 8 x 24
SPR_SEL equ 2100
SPR_EN  equ 2101
SPR_XL  equ 2102
SPR_XH  equ 2103
SPR_Y   equ 2104
SPR_COL equ 2105                ; 8 filas
SPR_PIX equ 2113                ; 8 filas
SPR_MSK equ 2121                ; 8 filas

start:  di
        push ix                 ; la ROM del ZX81 espera IX e IY intactos al volver
        push iy
        xor a
        ld (2043),a
        ld a,80h
        ld (2044),a             ; HFILE = $8000
        ld a,0ffh
        ld (2059),a
        ld a,9fh
        ld (2060),a
        ld a,170
        ld (2061),a             ; base de los atributos de HiRes modo 1 = $9FFF (+1 = $A000)
        xor a
        ld (2090),a             ; fino X = 0
        out (0fbh),a            ; borde negro
        call build_tables
        call redraw_all
        call set_vmode          ; modo Superfast HiRes nativo

; -------------------------------------------------------------
; Bucle principal: una vuelta por imagen, sincronizada al VSYNC.
; Los registros se escriben nada mas empezar el VSYNC, con el haz
; fuera del area visible.
; -------------------------------------------------------------
main:   call waitvs
        ld a,(pend)
        or a
        jp z,m_free
        call getbusy            ; hay una orden: el motor ha acabado?
        jr nz,main              ; todavia no: nada mas en este cuadro
        ld a,(pend)             ; acabado: cerrar el movimiento
        ld b,a
        xor a
        ld (pend),a
        ld a,b
        cp 1
        jr z,m_left
        cp 3
        jr z,m_up
        cp 4
        jr z,m_down
        ld hl,wc0               ; derecha: la columna 0 de pantalla es una antes en el mundo
        dec (hl)
        ld a,7
        ld (fx),a
        ld (2090),a
        call put_a1col0
        call put_L
        ld a,1
        ld (btd),a              ; el motor horizontal no mueve B ni T: se rellenan cuando haga falta (put_BTi)
        jp main
m_left: ld hl,wc0               ; izquierda
        inc (hl)
        xor a
        ld (fx),a
        ld (2090),a
        call put_R
        ld a,1
        ld (btd),a
        jp main
m_up:   ld hl,wr0               ; arriba: la fila 0 de pantalla es una despues en el mundo
        inc (hl)
        xor a
        ld (fy),a
        call setfy
        call put_B              ; la fila nueva de abajo y su esquina
        ld a,(rbot)             ; la fila que ha entrado: su L (el R viene de la esquina)
        call put_L_row
        jp main
m_down: ld hl,wr0               ; abajo
        dec (hl)
        ld a,7
        ld (fy),a
        call setfy
        call put_T              ; la fila nueva de arriba
        ld a,(rtop)             ; la fila que ha entrado: R y L (arriba no hay bufer de esquina)
        push af
        call put_R_row
        pop af
        call put_L_row
        call put_a1top
        jp main
m_free: call keys
        or a
        jr nz,quit
        call step
        jp main

; --- salida: todo a cero y modo normal ---
quit:   xor a
        ld (2090),a
        ld a,0ffh
        ld (2091),a
        ld (2092),a
        ld (2093),a             ; mascara de filas por defecto
        xor a
        ld (spr),a
        call apply_spr          ; sprites fuera
        xor a
        ld (fy),a               ; fino Y = 0 y la region entera (filas 0-23)
        call setfy
        ld bc,DBGPORT
        ld a,42h
        out (c),a
        xor a
        out (c),a
        ld a,43h
        out (c),a
        ld a,23
        out (c),a
        ld a,85
        ld (2057),a             ; doble buffer OFF
        ld a,20h
        ld bc,7fefh
        out (c),a               ; Chroma en modo 0
        ld a,85
        ld (2061),a             ; sin base propia de atributos
        ld (2045),a             ; modo ZX81 normal
        pop iy
        pop ix
        ret

; -------------------------------------------------------------
; waitvs: espera el flanco de subida del VSYNC (bit 0, puerto $AF). Con el ruido
; puesto, escribe en una zona libre sin parar mientras espera
; -------------------------------------------------------------
waitvs: ld a,(noise)
        or a
        jr nz,wv_n
wv1:    in a,(0afh)
        rrca
        jr c,wv1
wv2:    in a,(0afh)
        rrca
        jr nc,wv2
        ret
wv_n:   ld hl,NOISE
wn1:    ld (hl),a
        inc l
        set 7,l
        in a,(0afh)
        rrca
        jr c,wn1
wn2:    ld (hl),a
        inc l
        set 7,l
        in a,(0afh)
        rrca
        jr nc,wn2
        ret

; getbusy: Z si el motor esta libre (OUT $20 elige el indice 16 del puerto; bit 7 = ocupado)
getbusy:ld bc,DBGPORT
        ld a,20h
        out (c),a
        in a,(c)
        and 80h
        ret

; send_cmd: A = orden (1 izquierda, 2 derecha); queda pendiente hasta el borrado
send_cmd:
        ld (pend),a
        ld d,a
        ld bc,DBGPORT
        ld a,41h
        out (c),a
        ld a,d
        out (c),a
        ret

; setfy: fino Y (registro 8 del puerto $3FEF: OUT $40 y el dato)
setfy:  ld bc,DBGPORT
        ld a,40h
        out (c),a
        ld a,(fy)
        out (c),a
        ret

; set_region: la region vertical que se mueve (registros 10 y 11): con el HUD las filas 1-22, si no 0-23
set_region:
        ld a,(hud)
        or a
        jr z,sr_full
        ld a,1
        ld (rtop),a
        ld a,22
        ld (rbot),a
        jr sr_go
sr_full:xor a
        ld (rtop),a
        ld a,23
        ld (rbot),a
sr_go:  ld bc,DBGPORT
        ld a,42h
        out (c),a
        ld a,(rtop)
        out (c),a
        ld a,43h
        out (c),a
        ld a,(rbot)
        out (c),a
        ret

; set_chroma: Chroma encendido; en HiRes modo 0 / 1 segun amode ($7FEF, bit 4); en Spectrum, siempre modo 0
set_chroma:
        ld a,(vmode)
        or a
        ld a,20h
        jr nz,sc_go
        ld a,(amode)
        rlca
        rlca
        rlca
        rlca
        or 20h
sc_go:  ld bc,7fefh
        out (c),a
        ret

; set_vmode: POKE 2045 con HiRes nativo (171) o Spectrum (172)
set_vmode:
        ld a,(vmode)
        add a,171
        ld (2045),a
        ret

; apply_mask: HUD -> las filas 0 y 23 no se mueven ($FE, $FF, $7F); si no, todas ($FF)
apply_mask:
        ld a,(hud)
        or a
        ld a,0ffh
        jr z,am1
        ld a,0feh
am1:    ld (2091),a
        ld a,0ffh
        ld (2092),a
        ld a,(hud)
        or a
        ld a,0ffh
        jr z,am2
        ld a,7fh
am2:    ld (2093),a
        jp set_region

; apply_dbuf: doble buffer AUTO con el bloque 7 de delante (168 + 7), o apagado (85)
apply_dbuf:
        ld a,(dbuf)
        or a
        ld a,85
        jr z,ad1
        ld a,175
ad1:    ld (2057),a
        ret

; apply_spr: dos sprites fijos de 8x8 (aro de color) o ninguno
apply_spr:
        ld a,(spr)
        or a
        jr z,as_off
        ld b,0
        ld de,120
        ld c,88
        ld a,14                 ; amarillo brillante
        call spr_def
        ld b,1
        ld de,180
        ld c,88
        ld a,11                 ; magenta brillante
        jp spr_def
as_off: xor a
        ld (SPR_SEL),a
        ld (SPR_EN),a
        inc a
        ld (SPR_SEL),a
        xor a
        ld (SPR_EN),a
        ret

; spr_def: B = sprite, DE = X (9 bits), C = Y, A = tinta (0-15), papel negro
spr_def:push af
        ld a,b
        ld (SPR_SEL),a
        ld a,e
        ld (SPR_XL),a
        ld a,d
        ld (SPR_XH),a
        ld a,c
        ld (SPR_Y),a
        pop af
        add a,a
        add a,a
        add a,a
        add a,a                 ; tinta * 16 + papel 0
        ld hl,SPR_COL
        ld b,8
sd1:    ld (hl),a
        inc hl
        djnz sd1
        ld hl,ring
        ld de,SPR_PIX
        ld bc,8
        ldir
        ld hl,full
        ld de,SPR_MSK
        ld bc,8
        ldir
        ld a,1
        ld (SPR_EN),a
        ret

; -------------------------------------------------------------
; step: un paso de scroll en el sentido que toque (teclas o automatico)
; -------------------------------------------------------------
step:   ld a,(auto)
        or a
        jr z,st_keys
        ld hl,acnt
        inc (hl)
        ld a,(hl)
        cp 160
        jr c,st_a
        ld (hl),0
        ld a,(adir)
        xor 1
        ld (adir),a
st_a:   ld a,(avert)
        or a
        ld a,(adir)
        jr nz,st_av
        or a
        jr z,st_left
        jr st_right
st_av:  or a
        jr z,st_up
        jr st_down
st_keys:ld a,0dfh
        in a,(0feh)
        bit 0,a                 ; P
        jr nz,sk_o
        call st_left
        jr sk_v
sk_o:   bit 1,a                 ; O
        jr nz,sk_v
        call st_right
sk_v:   ld a,(pend)
        or a
        ret nz                  ; ya hay una orden: la otra direccion espera
        ld a,0fbh
        in a,(0feh)
        bit 0,a                 ; Q: arriba
        jr nz,sk_a
        jp st_up
sk_a:   ld a,0fdh
        in a,(0feh)
        bit 0,a                 ; A: abajo
        ret nz
        jp st_down
st_left:ld a,(fx)
        cp 7
        jr nc,st_l7             ; ya esta en 7: solo la orden
        inc a
        ld (fx),a
        ld (2090),a
        cp 7
        ret nz
st_l7:  ld a,1                  ; en 7: mover la memoria un byte a la izquierda
        jp send_cmd
st_right:
        ld a,(fx)
        or a
        jr z,st_r0
        dec a
        ld (fx),a
        ld (2090),a
        ret nz
st_r0:  ld a,2                  ; en 0: mover la memoria un byte a la derecha
        jp send_cmd

; st_up / st_down: un paso vertical. Arriba el fino sube y, en 7, se manda la orden 3; abajo baja y, en 0, la orden 4
st_up:  call put_BTi            ; B tiene que estar bien (se ve con el fino > 0) y T tambien (lo recibe al subir)
        ld a,(fy)
        cp 7
        jr nc,st_u7             ; ya esta en 7: solo la orden
        inc a
        ld (fy),a
        call setfy
        ld a,(fy)
        cp 7
        ret nz
st_u7:  ld a,3
        jp send_cmd
st_down:call put_BTi
        ld a,(fy)
        or a
        jr z,st_d0
        dec a
        ld (fy),a
        call setfy
        ld a,(fy)
        or a
        ret nz
st_d0:  ld a,4
        jp send_cmd

; -------------------------------------------------------------
; tog: A = fila del teclado (byte alto del puerto), C = mascara de la tecla, HL = su "estaba pulsada".
; Devuelve A distinto de 0 solo en el cuadro en que se pulsa
; -------------------------------------------------------------
tog:    in a,(0feh)
        cpl
        and c
        ld b,a
        ld a,(hl)
        ld (hl),b
        or a
        jr nz,tg_no
        ld a,b
        ret
tg_no:  xor a
        ret

; -------------------------------------------------------------
; keys: A = 1 si se pide salir (X); las demas teclas, una vez por pulsacion
; -------------------------------------------------------------
keys:   ld a,0feh
        in a,(0feh)
        bit 2,a                 ; X
        jr nz,k1
        ld a,1
        ret
k1:     ld a,(vmode)            ; M: atributos de HiRes (en Spectrum no hay)
        or a
        jr nz,k_h
        ld a,7fh
        ld c,4
        ld hl,prevm
        call tog
        or a
        jr z,k_h
        ld a,(amode)
        xor 1
        ld (amode),a
        call redraw_all
k_h:    ld a,0bfh               ; H: HUD
        ld c,10h
        ld hl,prevh
        call tog
        or a
        jr z,k_d
        ld a,(hud)
        xor 1
        ld (hud),a
        call apply_mask
        call put_BT             ; la region cambia: B y T son otras filas del mundo
        ld a,(hud)
        or a
        jr z,kh_off
        call stamp_hud
        jr k_d
kh_off: call redraw_all
k_d:    ld a,0fdh               ; D: doble buffer
        ld c,4
        ld hl,prevd
        call tog
        or a
        jr z,k_s
        ld a,(dbuf)
        xor 1
        ld (dbuf),a
        call apply_dbuf
k_s:    ld a,0fdh               ; S: sprites
        ld c,2
        ld hl,prevs2
        call tog
        or a
        jr z,k_t
        ld a,(spr)
        xor 1
        ld (spr),a
        call apply_spr
k_t:    ld a,0fbh               ; T: HiRes / Spectrum
        ld c,10h
        ld hl,prevt
        call tog
        or a
        jr z,k_n
        ld a,(vmode)
        xor 1
        ld (vmode),a
        call set_vmode
        call redraw_all
k_n:    ld a,7fh                ; N: ruido
        ld c,8
        ld hl,prevn
        call tog
        or a
        jr z,k_sp
        ld a,(noise)
        xor 1
        ld (noise),a
k_sp:   ld a,7fh                ; SPACE: automatico
        ld c,1
        ld hl,prevs
        call tog
        or a
        jr z,k_v
        ld a,(auto)
        xor 1
        ld (auto),a
k_v:    ld a,0feh               ; V: el automatico va en vertical
        ld c,10h
        ld hl,prevv
        call tog
        or a
        jr z,k_end
        ld a,(avert)
        xor 1
        ld (avert),a
k_end:  xor a
        ret

; -------------------------------------------------------------
; Tablas de trabajo
;   BMT   2 x 16 bytes: el bitmap del mundo para una columna par / impar y 16 lineas.
;         El pixel X de la linea y esta encendido si ((X + y) mod 16) < 2: diagonales
;         que cruzan los limites de byte
;   BMC   la columna de 192 lineas de cada paridad (12 repeticiones)
;   A0C   los atributos de HiRes modo 0 de una columna, para cada (columna mod 8): papel = att[(col + y) mod 8]
;   A1C   los de HiRes modo 1, para cada (columna mod 8): papel = att[(col + fila) mod 8]
;   A1S   los del Spectrum, para cada (columna mod 8): papel = satt[(col + fila) mod 8]
; -------------------------------------------------------------
build_tables:
        ld hl,BMT
        ld c,0                  ; 8 * paridad
bt_p:   ld e,0                  ; linea (0-15)
bt_y:   ld d,0
        ld b,0                  ; pixel (0-7)
bt_k:   ld a,c
        add a,b
        add a,e
        and 15
        cp 2                    ; acarreo si ((8p + k + y) mod 16) < 2
        rl d
        inc b
        ld a,b
        cp 8
        jr nz,bt_k
        ld (hl),d
        inc hl
        inc e
        ld a,e
        cp 16
        jr nz,bt_y
        ld a,c
        add a,8
        ld c,a
        cp 16
        jr nz,bt_p
        ld de,BMC
        ld hl,BMT
        ld a,12
        call rep16
        ld hl,BMT+16
        ld a,12
        call rep16
        ld de,A0C
        ld c,0
bt_a0:  ld hl,att
        ld b,0
        add hl,bc
        push bc
        ld a,24
        call rep8
        pop bc
        inc c
        ld a,c
        cp 8
        jr nz,bt_a0
        ld de,A1C
        ld c,0
bt_a1:  ld hl,att
        ld b,0
        add hl,bc
        push bc
        ld a,3
        call rep8
        pop bc
        inc c
        ld a,c
        cp 8
        jr nz,bt_a1
        ld de,A1S
        ld c,0
bt_as:  ld hl,satt
        ld b,0
        add hl,bc
        push bc
        ld a,3
        call rep8
        pop bc
        inc c
        ld a,c
        cp 8
        jr nz,bt_as
        ret

; rep16 / rep8: HL = origen, DE = destino, A = repeticiones de 16 / 8 bytes
rep16:  ld (rcnt),a
r16:    push hl
        ld bc,16
        ldir
        pop hl
        ld a,(rcnt)
        dec a
        ld (rcnt),a
        jr nz,r16
        ret
rep8:   ld (rcnt),a
r8:     push hl
        ld bc,8
        ldir
        pop hl
        ld a,(rcnt)
        dec a
        ld (rcnt),a
        jr nz,r8
        ret

; src_bm / src_a0 / src_a1 / src_a1s: A = columna del mundo; HL = su tabla (BMC, A0C, A1C, A1S).
; Con el scroll vertical el mundo de la pantalla empieza en la fila wr0: el bitmap y los atributos 8x8 usan
; (columna + wr0) (una fila abajo = una columna a la derecha); los de 8x1 no (8 lineas = 0 mod 8)
plus_wr:ld b,a
        ld a,(wr0)
        add a,b
        ret
src_bm: call plus_wr           ; el bitmap depende de la paridad de (columna + fila del mundo)
        ld hl,BMC
        ld de,192
        and 1
        jr mulb
src_a0: ld hl,A0C
        ld de,192
        and 7
        jr mulb
src_a1: call plus_wr
        ld hl,A1C
        ld de,24
        and 7
        jr mulb
src_a1s:call plus_wr
        ld hl,A1S
        ld de,24
        and 7
mulb:   ret z
mb1:    add hl,de
        dec a
        jr nz,mb1
        ret

; -------------------------------------------------------------
; redraw_all: Chroma y todo el mundo visible desde wc0; con el HUD, sus filas fijas
; redraw: pinta las 32 columnas y los dos bufers
; -------------------------------------------------------------
redraw_all:
        call set_chroma
        call redraw
        ld a,(hud)
        or a
        ret z
        jp stamp_hud

redraw: xor a
        ld (rcol),a
rd1:    ld a,(wc0)
        ld hl,rcol
        add a,(hl)
        ld (pwc),a
        call put_main
        ld hl,rcol
        inc (hl)
        ld a,(hl)
        cp 32
        jr nz,rd1
        call put_R
        call put_L
        jp put_BT

; put_main: la columna del mundo (pwc) en la columna de pantalla (rcol)
put_main:
        ld a,(vmode)
        or a
        jp nz,put_main_sp
        ld a,(pwc)
        call src_bm
        push hl
        pop ix                  ; IX = bitmap de la columna
        ld a,(pwc)
        call src_a0
        push hl
        pop iy                  ; IY = atributos de modo 0
        ld a,(rcol)
        ld e,a
        ld d,0
        ld hl,BM
        add hl,de
        push hl
        ld hl,A0
        add hl,de
        ex de,hl
        pop hl                  ; HL = bitmap destino, DE = atributo destino
        call copy_col32
        ld a,(pwc)
        call src_a1
        push hl
        pop ix
        ld a,(rcol)
        ld e,a
        ld d,0
        ld hl,A1
        add hl,de
        jp copy_a1

; put_main_sp: lo mismo en Spectrum: bitmap entrelazado y atributos 8x8 en $9800
put_main_sp:
        ld a,(pwc)
        call src_bm
        push hl
        pop ix
        ld hl,BM
        call hpc
        call copy_col_sp
        ld a,(pwc)
        call src_a1s
        push hl
        pop ix
        ld hl,SPA
        call hpc
        jp copy_a32

; hpc: HL = HL + rcol (8 bits)
hpc:    ld a,(rcol)
        add a,l
        ld l,a
        ret nc
        inc h
        ret

; copy_col32: 192 lineas; IX = bitmap origen, IY = atributos origen, HL / DE = destinos; cada linea +32
copy_col32:
        ld b,192
cc1:    ld a,(ix+0)
        ld (hl),a
        ld a,(iy+0)
        ld (de),a
        inc ix
        inc iy
        ld a,l
        add a,32
        ld l,a
        jr nc,cc2
        inc h
cc2:    ld a,e
        add a,32
        ld e,a
        jr nc,cc3
        inc d
cc3:    djnz cc1
        ret

; copy_col_sp: 192 lineas en el orden del Spectrum; IX = bitmap origen (192 bytes), HL = destino de la linea 0.
; La linea y = tercio*64 + fila*8 + linea: la direccion sube 256 por linea, 32 por fila y 2048 por tercio
copy_col_sp:
        ld d,3
sp_t:   push hl
        ld c,8
sp_c:   push hl
        ld b,8
sp_l:   ld a,(ix+0)
        ld (hl),a
        inc ix
        inc h
        djnz sp_l
        pop hl
        ld a,l
        add a,32
        ld l,a
        dec c
        jr nz,sp_c
        pop hl
        ld a,h
        add a,8
        ld h,a
        dec d
        jr nz,sp_t
        ret

; copy_a1 / copy_a32: 24 filas; IX = origen, HL = destino; cada fila +33 / +32
copy_a1:ld b,24
ca1:    ld a,(ix+0)
        ld (hl),a
        inc ix
        ld a,l
        add a,33
        ld l,a
        jr nc,ca2
        inc h
ca2:    djnz ca1
        ret
copy_a32:
        ld b,24
ca3:    ld a,(ix+0)
        ld (hl),a
        inc ix
        ld a,l
        add a,32
        ld l,a
        jr nc,ca4
        inc h
ca4:    djnz ca3
        ret

; put_R: la columna del mundo wc0+32 en los bufers de la derecha. put_R_row (A = fila): solo esa fila de texto.
; (put_Rr rellena rrn filas desde rr0)
put_R:  xor a
        ld (rr0),a
        ld a,24
        ld (rrn),a
put_Rr: ld a,(vmode)
        or a
        jr nz,put_R_sp
        ld a,(wc0)
        add a,32
        ld (pwc),a
        call src_bm
        ld de,RBM
        call lr
        ldir
        ld a,(pwc)
        call src_a0
        ld de,RA0
        call lr
        ldir
        ld a,(pwc)
        call src_a1
        push hl
        pop ix
        ld hl,A1+32
        jp copy_a1r
put_R_sp:
        ld a,(wc0)
        add a,32
        ld (pwc),a
        call src_bm
        ld de,SPRB
        call lr
        ldir
        ld a,(pwc)
        call src_a1s
        ld de,SPRA
        call lr1
        ldir
        ret
put_R_row:
        ld (rr0),a
        ld a,1
        ld (rrn),a
        jr put_Rr

; lr: HL (origen) y DE (destino) avanzan 8 * rr0 lineas y BC = 8 * rrn; lr1: lo mismo con un byte por fila
lr:     ld a,(rr0)
        add a,a
        add a,a
        add a,a
        ld c,a
        ld b,0
        add hl,bc
        ex de,hl
        add hl,bc
        ex de,hl
        ld a,(rrn)
        add a,a
        add a,a
        add a,a
        ld c,a
        ret
lr1:    ld a,(rr0)
        ld c,a
        ld b,0
        add hl,bc
        ex de,hl
        add hl,bc
        ex de,hl
        ld a,(rrn)
        ld c,a
        ret

; copy_a1r: como copy_a1 pero desde la fila rr0 y rrn filas (IX = origen de la fila 0, HL = destino de la fila 0)
copy_a1r:
        ld a,(rr0)
        ld c,a
        ld b,0
        add ix,bc
        or a
        jr z,c1r_n
        ld de,33
c1r_l:  add hl,de
        dec a
        jr nz,c1r_l
c1r_n:  ld a,(rrn)
        ld b,a
        jp ca1

; put_L: la columna del mundo wc0-1 en los bufers de la izquierda. put_L_row (A = fila): solo esa fila de texto
put_L:  xor a
        ld (rr0),a
        ld a,24
        ld (rrn),a
put_Lr: ld a,(vmode)
        or a
        jr nz,put_L_sp
        ld a,(wc0)
        dec a
        ld (pwc),a
        call src_bm
        ld de,LBM
        call lr
        ldir
        ld a,(pwc)
        call src_a0
        ld de,LA0
        call lr
        ldir
        ret
put_L_sp:
        ld a,(wc0)
        dec a
        ld (pwc),a
        call src_bm
        ld de,SPLB
        call lr
        ldir
        ld a,(pwc)
        call src_a1s
        ld de,SPLA
        call lr1
        ldir
        ret
put_L_row:
        ld (rr0),a
        ld a,1
        ld (rrn),a
        jr put_Lr

; -------------------------------------------------------------
; Bufers de entrada de abajo (B) y de arriba (T): una fila del mundo entera (8 lineas de 32 bytes) con sus atributos y,
; el de abajo, la esquina (la columna R de esa fila). B es la fila wr0 + rbot + 1; T, la wr0 + rtop - 1.
; Las columnas son wc0..wc0+31 (la esquina, wc0+32): despues de un movimiento horizontal hay que rellenarlos
; -------------------------------------------------------------
put_BT: xor a
        ld (btd),a
        call put_B
        jp put_T
put_BTi:ld a,(btd)              ; si un movimiento horizontal ha dejado B y T sin alinear, rellenarlos ahora
        or a
        ret z
        jr put_BT

put_B:  ld a,(wr0)
        ld hl,rbot
        add a,(hl)
        inc a
        ld (pwr),a
        ld a,(vmode)
        or a
        jr nz,pb_sp
        ld hl,9900h             ; HiRes: bitmap de abajo
        call bm_rows
        ld de,9A00h             ; ... y la esquina
        call bm_corner
        ld a,(amode)
        or a
        jr nz,pb_m1
        ld de,0D900h            ; modo 0: atributos de abajo (8 lineas)
        call a0_rows
        ld a,(wc0)              ; ... y su esquina: att[(wc0 + k) & 7]
        and 7
        ld c,a
        ld b,0
        ld hl,att
        add hl,bc
        ld de,0DA00h
        ld bc,8
        ldir
        ret
pb_m1:  ld de,A1+24*33          ; modo 1: la fila 24 de la rejilla (33 celdas; la ultima es la esquina)
        jp a1_row
pb_sp:  ld hl,9D00h             ; Spectrum
        call bm_rows
        ld de,9E40h
        call bm_corner
        ld de,9E00h
        call sp_attr_row
        ld (9E48h),a            ; atributo de la esquina
        ret

put_T:  ld a,(wr0)
        ld hl,rtop
        add a,(hl)
        dec a
        ld (pwr),a
        ld a,(vmode)
        or a
        jr nz,pt_sp
        ld hl,9D00h             ; HiRes: bitmap de arriba
        call bm_rows
        ld a,(amode)
        or a
        ret nz                  ; modo 1: no hay bufer de arriba
        ld de,0DD00h
        jp a0_rows
pt_sp:  ld hl,9F00h
        call bm_rows
        ld de,9E20h
        jp sp_attr_row

; bm_base: IX = los 8 bytes del bitmap del mundo para las columnas PARES de pantalla (paridad de wc0) y la fila (pwr);
; IY = para las impares. Valor de la linea k = BMT[paridad * 16 + 8 * (fila & 1) + k]
bm_base:ld a,(pwr)
        and 1
        add a,a
        add a,a
        add a,a
        ld b,a                  ; B = 8 * (fila & 1)
        ld a,(wc0)
        and 1
        ld c,a                  ; C = paridad de wc0
        rrca
        rrca
        rrca
        rrca                    ; 16 * paridad
        add a,b
        ld e,a
        ld d,0
        ld ix,BMT
        add ix,de
        ld a,c
        xor 1
        rrca
        rrca
        rrca
        rrca
        add a,b
        ld e,a
        ld iy,BMT
        add iy,de
        ret

; bm_rows: 8 lineas de 32 bytes (contiguas) en HL
bm_rows:call bm_base
        ld b,8
br_k:   ld a,(ix+0)
        ld c,(iy+0)
        ld d,16
br_p:   ld (hl),a
        inc hl
        ld (hl),c
        inc hl
        dec d
        jr nz,br_p
        inc ix
        inc iy
        djnz br_k
        ret

; bm_corner: 8 bytes (uno por linea) de la columna wc0+32 en DE; su paridad es la de wc0, la de IX
bm_corner:
        push de
        call bm_base
        push ix
        pop hl
        pop de
        ld bc,8
        ldir
        ret

; a0_rows: HiRes modo 0, 8 lineas de 32 atributos (contiguas) en DE: att[(wc0 + c + k) & 7]
a0_rows:xor a
        ld (rk),a
ar_k:   ld a,(wc0)
        ld hl,rk
        add a,(hl)
        and 7
        ld l,a
        ld h,0
        ld bc,att
        add hl,bc
        ld a,4
        call rep8
        ld hl,rk
        inc (hl)
        ld a,(hl)
        cp 8
        jr nz,ar_k
        ret

; a1_row: HiRes modo 1, una fila de 33 celdas en DE: att[(wc0 + c + fila del mundo (pwr)) & 7]
a1_row: ld a,(wc0)
        ld hl,pwr
        add a,(hl)
        and 7
        ld l,a
        ld h,0
        ld bc,att
        add hl,bc
        push hl
        ld a,4
        call rep8
        pop hl
        ldi                     ; la celda 32 = la 0
        ret

; sp_attr_row: Spectrum, 32 atributos en DE: satt[(wc0 + c + pwr) & 7]; devuelve en A el de la columna 32 (el de la 0)
sp_attr_row:
        ld a,(wc0)
        ld hl,pwr
        add a,(hl)
        and 7
        ld l,a
        ld h,0
        ld bc,satt
        add hl,bc
        ld a,(hl)
        push af
        ld a,4
        call rep8
        pop af
        ret

; put_a1top: HiRes modo 1, tras bajar: la fila de arriba de la region (rtop) entera, que no tiene bufer
put_a1top:
        ld a,(vmode)
        or a
        ret nz
        ld a,(amode)
        or a
        ret z
        ld a,(wr0)
        ld hl,rtop
        add a,(hl)
        ld (pwr),a
        ld a,(rtop)
        ld hl,A1
        or a
        jr z,pa_g
        ld de,33
pa_l:   add hl,de
        dec a
        jr nz,pa_l
pa_g:   ex de,hl
        jp a1_row

; put_a1col0: la columna 0 de la rejilla de HiRes modo 1 (no tiene bufer de la izquierda) con la del mundo wc0.
; Con el HUD, las celdas de las filas 0 y 23 son suyas
put_a1col0:
        ld a,(vmode)
        or a
        ret nz
        ld a,(wc0)
        call src_a1
        push hl
        pop ix
        ld hl,A1
        call copy_a1
        ld a,(hud)
        or a
        ret z
        ld a,0f0h
        ld (A1),a
        ld (A1+23*33),a
        ret

; -------------------------------------------------------------
; stamp_hud: las filas 0 y 23 con un dibujo fijo (rayas verticales de 4 pixeles, papel blanco brillante).
; El motor y el fino no las tocan mientras la mascara las deje fuera
; -------------------------------------------------------------
stamp_hud:
        xor a
        ld (rcol),a
sh1:    ld a,(rcol)
        and 1
        ld a,0fh
        jr z,sh2
        ld a,0f0h
sh2:    ld (pb),a
        ld a,(vmode)
        or a
        jr nz,sh_sp
        ld hl,BM                ; HiRes: bitmap, atributos de modo 0 y de modo 1
        call hpc
        ld a,(pb)
        call fill8
        ld hl,BM+184*32
        call hpc
        ld a,(pb)
        call fill8
        ld hl,A0
        call hpc
        ld a,0f0h
        call fill8
        ld hl,A0+184*32
        call hpc
        ld a,0f0h
        call fill8
        ld hl,A1
        call hpc
        ld (hl),0f0h
        ld hl,A1+23*33
        call hpc
        ld (hl),0f0h
        jr sh3
sh_sp:  ld hl,BM                ; Spectrum: bitmap entrelazado y atributos
        call hpc
        ld a,(pb)
        call fill8h
        ld hl,BM+2*2048+7*32
        call hpc
        ld a,(pb)
        call fill8h
        ld hl,SPA
        call hpc
        ld (hl),78h
        ld hl,SPA+23*32
        call hpc
        ld (hl),78h
sh3:    ld hl,rcol
        inc (hl)
        ld a,(hl)
        cp 32
        jp nz,sh1
        ret

; fill8: 8 lineas con A, HL = primera, +32 por linea. fill8h: +256 por linea
fill8:  ld b,8
        ld de,32
f8:     ld (hl),a
        add hl,de
        djnz f8
        ret
fill8h: ld b,8
f8h:    ld (hl),a
        inc h
        djnz f8h
        ret

; -------------------------------------------------------------
; datos y variables
; -------------------------------------------------------------
att:    defb 10h,20h,30h,40h,50h,60h,70h,90h    ; papeles Chroma: azul, rojo, magenta, verde, cian, amarillo, blanco, azul brillante
        defb 10h,20h,30h,40h,50h,60h,70h,90h
satt:   defb 08h,10h,18h,20h,28h,30h,38h,48h    ; papeles Spectrum: azul ... blanco, azul brillante
        defb 08h,10h,18h,20h,28h,30h,38h,48h
ring:   defb 0ffh,81h,81h,81h,81h,81h,81h,0ffh
full:   defb 0ffh,0ffh,0ffh,0ffh,0ffh,0ffh,0ffh,0ffh
wc0:    defb 0                  ; columna del mundo que esta en la columna 0 de pantalla
wr0:    defb 0                  ; fila del mundo que esta en la fila 0 de pantalla
fx:     defb 0
fy:     defb 0                  ; fino Y (0-7)
rtop:   defb 0                  ; region vertical: primera fila (0, o 1 con el HUD)
rbot:   defb 23                 ; ... y ultima (23, o 22 con el HUD)
avert:  defb 0                  ; el automatico va en vertical
prevv:  defb 0
btd:    defb 0                  ; 1 = B y T no estan alineados con wc0 (un movimiento horizontal los ha dejado atras)
pwr:    defb 0                  ; fila del mundo de los bufers B / T
rk:     defb 0
rr0:    defb 0                  ; put_R / put_L: primera fila de texto
rrn:    defb 24                 ; ... y numero de filas
pend:   defb 0                  ; orden mandada y sin cerrar (1 izquierda, 2 derecha, 3 arriba, 4 abajo)
vmode:  defb 0                  ; 0 = HiRes nativo, 1 = Spectrum
amode:  defb 0                  ; atributos de HiRes: 0 = modo 0 (8x1), 1 = modo 1 (8x8)
hud:    defb 0
dbuf:   defb 0
spr:    defb 0
noise:  defb 0
auto:   defb 0
adir:   defb 0
acnt:   defb 0
prevm:  defb 0
prevh:  defb 0
prevd:  defb 0
prevs2: defb 0
prevt:  defb 0
prevn:  defb 0
prevs:  defb 0
rcol:   defb 0
pwc:    defb 0
rcnt:   defb 0
pb:     defb 0

        end start
