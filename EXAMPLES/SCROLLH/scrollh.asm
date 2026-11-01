; =============================================================
; SCROLLH.ASM -- prueba del scroll fino por hardware (fase 1b)
;
; Modo Superfast HiRes NATIVO (POKE 2045,171): bitmap lineal de 6 KB.
; Es la hermana de SCROLLT (Spectrum). Comprueba, sobre la placa, el scroll
; fino horizontal (POKE 2090) y vertical (puerto $3FEF, registro 8) con los
; dos tipos de atributo de Chroma que tiene el HiRes:
;
;   modo 0  un atributo por cada byte de linea (8x1), area paralela al
;           bitmap en $C000 (HFILE no cuenta: es el bloque 6)
;   modo 1  un atributo por celda de 8x8 y por posicion, paso de 33 bytes
;           por fila (el 33 es el hueco del NEWLINE), base POKE 2059-2061
;
; Ver claude/planning/scroll_hardware.md (apartado HiRes nativo).
;
; Fondo: cuadricula de celdas de 8x8 con una diagonal.
;   modo 0: papel = ((linea + columna) mod 6) + 1  -> franjas diagonales de
;           UNA linea de alto (8x1): en vertical el color tiene que seguir a
;           cada linea, no a cada celda
;   modo 1: papel = ((fila + columna) mod 6) + 1   -> celdas de colores 8x8
;
; Bufers de entrada (HFILE = $8000; $9800-$9FFF es la zona libre del bloque):
;   bitmap:  derecha $9800 (192), abajo $9900 (7x32), esquina $9A00 (7)
;   modo 0:  derecha $D800 (192), abajo $D900 (7x32), esquina $DA00 (7)
;   modo 1:  columna 32 de cada fila de la tabla, y la fila 24 entera
;            (tabla en $A000, base 2059/2060 = $9FFF, paso 33)
; Colores de entrada (Chroma: papel en el nibble alto, tinta en el bajo,
; bit 3 de cada nibble = brillo):
;   derecha = puntos amarillos sobre negro ($AA, atributo $0E)
;   abajo   = verde brillante macizo ($FF, atributo $0C)
;   esquina = blanco brillante macizo ($FF, atributo $0F)
;
; Que se tiene que ver: lo mismo que en SCROLLT (README.md), con la tecla M
; para cambiar de modo de atributos. NO se mueve nada de memoria: es solo el
; scroll fino (la visualizacion), el motor de movimiento llegara despues.
;
; Teclas:  P / O = FX + / -      Q / A = FY + / -   (se repiten)
;          H = HUD si / no       SPACE = automatico (recorre FX y FY)
;          M = atributos modo 0 / modo 1
;          X = salir a BASIC
;
; Necesita RAM en $C000-$DFFF (no vale el modo 32K).
; Ensamblar: pasmo scrollh.asm scrollh.bin scrollh.sym
; Cargar desde BASIC: SCROLLH.B81 (LOAD FAST "SCROLLH.BIN" CODE 30000)
; =============================================================

        org 30000

DBGPORT equ 3FEFh               ; puerto del depurador: OUT $40 + dato = fino Y, $42 = fila superior, $43 = fila inferior
BM      equ 8000h               ; bitmap lineal (HFILE = $8000), y*32 + columna
RBM     equ 9800h               ; entrada derecha: bitmap (192, uno por linea)
BBM     equ 9900h               ; entrada de abajo: bitmap (7 lineas de 32; se rellenan 256)
CBM     equ 9A00h               ; esquina: bitmap (8)
A0      equ 0C000h              ; atributos de modo 0: uno por byte de linea (6144)
RA0     equ 0D800h              ; entrada derecha, modo 0 (192)
BA0     equ 0D900h              ; entrada de abajo, modo 0 (256)
CA0     equ 0DA00h              ; esquina, modo 0 (8)
A1      equ 0A000h              ; atributos de modo 1: 25 filas de 33 (base 2059/2060 = $9FFF, el modo 1 suma 1)
FONT    equ 1E00h               ; juego de caracteres de la ROM del ZX81

start:  di
        ; --- HFILE = $8000 ---
        xor a
        ld (2043),a
        ld a,80h
        ld (2044),a
        ; --- base de los atributos de modo 1: $9FFF (+1 = $A000) ---
        ld a,0ffh
        ld (2059),a
        ld a,9fh
        ld (2060),a
        ld a,170
        ld (2061),a
        ; --- color y borde negro ---
        ld bc,7fefh
        ld a,20h                ; Chroma on, modo 0
        out (c),a
        xor a
        out (0fbh),a
        ; --- modo Superfast HiRes nativo ---
        ld a,171
        ld (2045),a

        call draw_grid
        call draw_attr0
        call draw_attr1
        call fill_buffers
        call apply_all
        call draw_status

; -------------------------------------------------------------
; Bucle principal: una vuelta por imagen, sincronizada al VSYNC.
; Los registros se escriben nada mas empezar el VSYNC, con el haz
; fuera del area visible.
; -------------------------------------------------------------
main:   call waitvs
        call keys
        or a
        jr nz,quit
        call auto_step
        call apply_all
        ld a,(dirty)
        or a
        jr z,main
        xor a
        ld (dirty),a
        call draw_status
        jr main

; --- salida: todo a cero y modo normal ---
quit:   xor a
        ld (2090),a
        ld bc,DBGPORT
        ld a,40h
        out (c),a
        xor a
        out (c),a               ; fino Y = 0
        ld a,42h
        out (c),a
        xor a
        out (c),a               ; region desde la fila 0
        ld a,43h
        out (c),a
        ld a,23
        out (c),a               ; ... hasta la 23
        ld a,0ffh
        ld (2091),a
        ld (2092),a
        ld (2093),a
        ld bc,7fefh
        ld a,20h
        out (c),a               ; atributos de modo 0 otra vez
        ld a,85
        ld (2061),a             ; sin base propia de atributos
        ld (2045),a             ; modo ZX81 normal
        ret

; -------------------------------------------------------------
; waitvs: espera el flanco de subida del VSYNC (bit 0, puerto $AF)
; -------------------------------------------------------------
waitvs: in a,(0afh)
        rrca
        jr c,waitvs
wvs2:   in a,(0afh)
        rrca
        jr nc,wvs2
        ret

; -------------------------------------------------------------
; draw_grid: 192 lineas de 32 bytes; el bitmap depende solo de la
; linea dentro de la celda (8 lineas distintas)
; -------------------------------------------------------------
draw_grid:
        ld hl,BM
        ld d,0                  ; y
dg1:    ld a,d
        and 7
        ld c,a
        ld b,0
        push hl
        ld hl,celltab
        add hl,bc
        ld a,(hl)
        pop hl
        ld b,32
dg2:    ld (hl),a
        inc hl
        djnz dg2
        inc d
        ld a,d
        cp 192
        jr nz,dg1
        ret

; -------------------------------------------------------------
; draw_attr0: modo 0 (8x1). papel = ((y + columna) mod 6) + 1, tinta negra,
; un byte por cada byte de linea
; -------------------------------------------------------------
draw_attr0:
        ld hl,A0
        ld d,0                  ; y
        ld e,0                  ; y mod 6
d0y:    ld b,32
        ld c,e                  ; k = (y + columna) mod 6
d0x:    ld a,c
        inc a
        add a,a
        add a,a
        add a,a
        add a,a                 ; (k + 1) << 4: el papel en el nibble alto
        ld (hl),a
        inc hl
        inc c
        ld a,c
        cp 6
        jr c,d0n
        ld c,0
d0n:    djnz d0x
        inc e
        ld a,e
        cp 6
        jr c,d0e
        ld e,0
d0e:    inc d
        ld a,d
        cp 192
        jr nz,d0y
        ret

; -------------------------------------------------------------
; draw_attr1: modo 1 (8x8). papel = ((fila + columna) mod 6) + 1, tinta negra.
; Tabla de 25 filas de 33 bytes: la columna 32 de las filas 0-23 es la
; entrada de la derecha ($0E) y la fila 24 entera la de abajo ($0C; la
; esquina, $0F)
; -------------------------------------------------------------
draw_attr1:
        ld hl,A1
        ld d,0                  ; fila
        ld e,0                  ; fila mod 6
d1y:    ld b,32
        ld c,e
d1x:    ld a,c
        inc a
        add a,a
        add a,a
        add a,a
        add a,a
        ld (hl),a
        inc hl
        inc c
        ld a,c
        cp 6
        jr c,d1n
        ld c,0
d1n:    djnz d1x
        ld (hl),0eh             ; columna 32: entrada de la derecha
        inc hl
        inc e
        ld a,e
        cp 6
        jr c,d1e
        ld e,0
d1e:    inc d
        ld a,d
        cp 24
        jr nz,d1y
        ld b,32                 ; fila 24: entrada de abajo
        ld a,0ch
d1b:    ld (hl),a
        inc hl
        djnz d1b
        ld (hl),0fh             ; y la esquina
        ret

; -------------------------------------------------------------
; fill_buffers: los bufers de entrada del bitmap y del modo 0
; -------------------------------------------------------------
fill_buffers:
        ld hl,RBM
        ld b,192
        ld a,0aah
        call fillb
        ld hl,RA0
        ld b,192
        ld a,0eh
        call fillb
        ld hl,BBM
        ld b,0                  ; 256
        ld a,0ffh
        call fillb
        ld hl,BA0
        ld b,0
        ld a,0ch
        call fillb
        ld hl,CBM
        ld b,8
        ld a,0ffh
        call fillb
        ld hl,CA0
        ld b,8
        ld a,0fh
fillb:  ld (hl),a
        inc hl
        djnz fillb
        ret

; -------------------------------------------------------------
; apply_all: escribe los registros del scroll (cada cuadro)
;   modo de atributos: puerto $7FEF (bit 4)
;   fino X: POKE 2090; mascara de filas: 2091-2093
;   fino Y y region: puerto $3FEF
; -------------------------------------------------------------
apply_all:
        ld bc,7fefh
        ld a,(amode)
        rrca
        rrca
        rrca
        rrca                    ; modo 1 -> $10
        or 20h                  ; Chroma on
        out (c),a
        ld a,(fx)
        ld (2090),a
        ld bc,DBGPORT
        ld a,40h
        out (c),a
        ld a,(fy)
        out (c),a
        ld a,(hud)
        or a
        jr z,aa_full
        ld a,42h                ; HUD: la region que se mueve es la 1-22
        out (c),a
        ld a,1
        out (c),a
        ld a,43h
        out (c),a
        ld a,22
        out (c),a
        ld a,0feh               ; y en horizontal, las filas 0 y 23 fijas
        ld (2091),a
        ld a,0ffh
        ld (2092),a
        ld a,7fh
        ld (2093),a
        ret
aa_full:
        ld a,42h                ; todo se mueve
        out (c),a
        xor a
        out (c),a
        ld a,43h
        out (c),a
        ld a,23
        out (c),a
        ld a,0ffh
        ld (2091),a
        ld (2092),a
        ld (2093),a
        ret

; -------------------------------------------------------------
; draw_status: "FX=n FY=n H=n A=n M=n" en la fila 0, con la fuente de la ROM
; -------------------------------------------------------------
draw_status:
        ld hl,BM
        ld (cursor),hl
        ld a,43                 ; F
        call putch
        ld a,61                 ; X
        call putch
        ld a,20                 ; =
        call putch
        ld a,(fx)
        add a,28
        call putch
        xor a
        call putch
        ld a,43                 ; F
        call putch
        ld a,62                 ; Y
        call putch
        ld a,20
        call putch
        ld a,(fy)
        add a,28
        call putch
        xor a
        call putch
        ld a,45                 ; H
        call putch
        ld a,20
        call putch
        ld a,(hud)
        add a,28
        call putch
        xor a
        call putch
        ld a,38                 ; A
        call putch
        ld a,20
        call putch
        ld a,(auto)
        add a,28
        call putch
        xor a
        call putch
        ld a,50                 ; M
        call putch
        ld a,20
        call putch
        ld a,(amode)
        add a,28
        jp putch

; putch: A = codigo ZX81 del caracter; lo pinta en el cursor (bitmap, 8 lineas) y avanza.
; La fila 0 ocupa los 256 primeros bytes del bitmap: la linea siguiente es +32.
putch:  ld l,a
        ld h,0
        add hl,hl
        add hl,hl
        add hl,hl
        ld de,FONT
        add hl,de               ; HL = el dibujo del caracter en la ROM
        ld de,(cursor)
        ld b,8
pc1:    ld a,(hl)
        ld (de),a
        inc hl
        ld a,e
        add a,32                ; la linea siguiente de la celda: +32 (sin acarreo en la fila 0)
        ld e,a
        djnz pc1
        ld hl,(cursor)
        inc hl
        ld (cursor),hl
        ret

; -------------------------------------------------------------
; keys: devuelve A = 1 si se pide salir (X)
;   P/O: FX +/-   Q/A: FY +/-  (se repiten cada 6 cuadros)
;   H, SPACE, M: una vez por pulsacion
; -------------------------------------------------------------
keys:   ld a,0feh
        in a,(0feh)
        bit 2,a                 ; X
        jr nz,k1
        ld a,1
        ret
k1:     ld hl,rep
        ld a,(hl)
        or a
        jr z,k_scan
        dec (hl)
        jr k_tog
k_scan: ld b,0                  ; B = 1 si alguna tecla ha cambiado algo
        ld a,0fbh
        in a,(0feh)
        bit 0,a                 ; Q: FY +
        jr nz,k2
        ld a,(fy)
        cp 7
        jr z,k2
        inc a
        ld (fy),a
        ld b,1
k2:     ld a,0fdh
        in a,(0feh)
        bit 0,a                 ; A: FY -
        jr nz,k3
        ld a,(fy)
        or a
        jr z,k3
        dec a
        ld (fy),a
        ld b,1
k3:     ld a,0dfh
        in a,(0feh)
        bit 0,a                 ; P: FX +
        jr nz,k4
        ld a,(fx)
        cp 7
        jr z,k4
        inc a
        ld (fx),a
        ld b,1
k4:     ld a,0dfh
        in a,(0feh)
        bit 1,a                 ; O: FX -
        jr nz,k5
        ld a,(fx)
        or a
        jr z,k5
        dec a
        ld (fx),a
        ld b,1
k5:     ld a,b
        or a
        jr z,k_tog
        ld a,5
        ld (rep),a
        ld a,1
        ld (dirty),a
k_tog:  ld a,0bfh               ; H: HUD (una vez por pulsacion)
        in a,(0feh)
        cpl
        and 10h
        ld c,a
        ld hl,prevh
        ld a,(hl)
        ld (hl),c
        or a
        jr nz,k_sp              ; ya estaba pulsada
        ld a,c
        or a
        jr z,k_sp
        ld a,(hud)
        xor 1
        ld (hud),a
        ld a,1
        ld (dirty),a
k_sp:   ld a,7fh                ; SPACE: automatico
        in a,(0feh)
        cpl
        and 1
        ld c,a
        ld hl,prevs
        ld a,(hl)
        ld (hl),c
        or a
        jr nz,k_m
        ld a,c
        or a
        jr z,k_m
        ld a,(auto)
        xor 1
        ld (auto),a
        ld a,1
        ld (dirty),a
k_m:    ld a,7fh                ; M: modo de atributos
        in a,(0feh)
        cpl
        and 4
        ld c,a
        ld hl,prevm
        ld a,(hl)
        ld (hl),c
        or a
        jr nz,k_end
        ld a,c
        or a
        jr z,k_end
        ld a,(amode)
        xor 1
        ld (amode),a
        ld a,1
        ld (dirty),a
k_end:  xor a
        ret

; -------------------------------------------------------------
; auto_step: con el modo automatico, FX va y viene de 0 a 7 cada 3
; cuadros y FY cada 21 (asi los dos recorren todas las combinaciones)
; -------------------------------------------------------------
auto_step:
        ld a,(auto)
        or a
        ret z
        ld hl,ac1
        inc (hl)
        ld a,(hl)
        cp 3
        jr c,as2
        ld (hl),0
        ld hl,q1
        ld a,(hl)
        inc a
        cp 14
        jr c,as1
        xor a
as1:    ld (hl),a
        call pp
        ld (fx),a
        ld a,1
        ld (dirty),a
as2:    ld hl,ac2
        inc (hl)
        ld a,(hl)
        cp 21
        ret c
        ld (hl),0
        ld hl,q2
        ld a,(hl)
        inc a
        cp 14
        jr c,as3
        xor a
as3:    ld (hl),a
        call pp
        ld (fy),a
        ld a,1
        ld (dirty),a
        ret

; pp: A = 0..13 -> 0..7..1 (ida y vuelta)
pp:     cp 8
        ret c
        ld b,a
        ld a,14
        sub b
        ret

; -------------------------------------------------------------
; datos y variables
; -------------------------------------------------------------
celltab: defb 0ffh,0c0h,0a0h,90h,88h,84h,82h,81h   ; una celda: arriba, y una diagonal
fx:     defb 0
fy:     defb 0
hud:    defb 1
auto:   defb 0
amode:  defb 0
dirty:  defb 0
rep:    defb 0
prevh:  defb 0
prevs:  defb 0
prevm:  defb 0
ac1:    defb 0
ac2:    defb 0
q1:     defb 0
q2:     defb 0
cursor: defw 0

        end start
