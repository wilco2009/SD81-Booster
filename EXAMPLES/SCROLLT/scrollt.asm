; =============================================================
; SCROLLT.ASM -- prueba del scroll fino por hardware (fase 1)
;
; Modo Superfast HiRes Spectrum. Comprueba, sobre la placa, lo que
; hace la FPGA con el scroll fino horizontal (POKE 2090) y vertical
; (puerto $3FEF, registro 8) cuando entran datos nuevos por la
; derecha y por abajo. Ver claude/planning/scroll_hardware.md.
;
; Fondo: cuadricula de celdas de 8x8 con una diagonal, y un color de
; papel que depende de (fila + columna) mod 6: asi se ve que el
; atributo viaja con su bitmap durante el scroll fino.
;
; Bufers de entrada (en los 1.280 bytes que sobran del bloque de 8 KB):
;   derecha  ($9B00, $9BC0): bitmap $AA, atributo $46 (puntos amarillos sobre negro)
;   abajo    ($9D00, $9E00): bitmap $FF, atributo $44 (verde brillante, macizo)
;   esquina  ($9E40, $9E48): bitmap $FF, atributo $47 (blanco brillante, macizo)
;
; Que se tiene que ver:
;   FX = n  la cuadricula se desplaza n pixeles a la izquierda y por la
;           derecha entran n pixeles de la columna AMARILLA y NEGRA (la 33)
;   FY = n  la cuadricula sube n lineas y por abajo entran n lineas VERDES
;   los dos a la vez: en la esquina de abajo a la derecha, un trozo BLANCO
;   HUD = 1: las filas 0 (el texto) y 23 NO se mueven ni en X ni en Y;
;           la region que se mueve en vertical es la 1-22
;   HUD = 0: se mueve toda la pantalla (filas 0-23)
;
; Teclas:  P / O = FX + / -      Q / A = FY + / -   (se repiten)
;          H = HUD si / no       SPACE = automatico (recorre FX y FY)
;          X = salir a BASIC
;
; Ensamblar: pasmo scrollt.asm scrollt.bin scrollt.sym
; Cargar desde BASIC: SCROLLT.B81 (LOAD FAST "SCROLLT.BIN" CODE 30000)
; =============================================================

        org 30000

DBGPORT equ 3FEFh               ; puerto del depurador: OUT $40 + dato = fino Y, $42 = fila superior, $43 = fila inferior
BM      equ 8000h               ; bitmap (HFILE = $8000)
ATTR    equ 9800h               ; atributos
RBM     equ 9B00h               ; entrada derecha: bitmap (192, uno por linea)
RAT     equ 9BC0h               ; entrada derecha: atributos (24)
BBM     equ 9D00h               ; entrada de abajo: bitmap (8 lineas de 32)
BAT     equ 9E00h               ; entrada de abajo: atributos (32)
CBM     equ 9E40h               ; esquina: bitmap (8)
CAT     equ 9E48h               ; esquina: atributo
FONT    equ 1E00h               ; juego de caracteres de la ROM del ZX81

start:  di
        ; --- HFILE = $8000 ---
        xor a
        ld (2043),a
        ld a,80h
        ld (2044),a
        ; --- color y borde negro ---
        ld bc,7fefh
        ld a,20h
        out (c),a
        xor a
        out (0fbh),a
        ; --- modo Superfast HiRes Spectrum ---
        ld a,172
        ld (2045),a

        call draw_grid
        call draw_attr
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
        ld a,85
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
; draw_grid: el bitmap depende solo de la linea (8 bytes iguales por celda)
; -------------------------------------------------------------
draw_grid:
        ld d,80h                ; byte alto de la direccion (80..97)
dg1:    ld a,d
        and 7                   ; la linea dentro de la celda
        ld c,a
        ld b,0
        ld hl,celltab
        add hl,bc
        ld a,(hl)
        ld h,d
        ld l,0
        ld b,0                  ; 256 bytes
dg2:    ld (hl),a
        inc l
        djnz dg2
        inc d
        ld a,d
        cp 98h
        jr nz,dg1
        ret

; -------------------------------------------------------------
; draw_attr: papel = ((fila + columna) mod 6) + 1, tinta negra
; -------------------------------------------------------------
draw_attr:
        ld hl,ATTR
        ld d,0                  ; fila
da1:    ld e,0                  ; columna
da2:    ld a,d
        add a,e
da3:    cp 6
        jr c,da4
        sub 6
        jr da3
da4:    inc a
        add a,a
        add a,a
        add a,a
        ld (hl),a
        inc hl
        inc e
        ld a,e
        cp 32
        jr nz,da2
        inc d
        ld a,d
        cp 24
        jr nz,da1
        ret

; -------------------------------------------------------------
; fill_buffers: los bufers de entrada de la derecha, de abajo y la esquina
; -------------------------------------------------------------
fill_buffers:
        ld hl,RBM
        ld b,192
        ld a,0aah
        call fillb
        ld hl,RAT
        ld b,24
        ld a,46h
        call fillb
        ld hl,BBM
        ld b,0                  ; 256
        ld a,0ffh
        call fillb
        ld hl,BAT
        ld b,32
        ld a,44h
        call fillb
        ld hl,CBM
        ld b,8
        ld a,0ffh
        call fillb
        ld hl,CAT
        ld b,1
        ld a,47h
fillb:  ld (hl),a
        inc hl
        djnz fillb
        ret

; -------------------------------------------------------------
; apply_all: escribe los registros del scroll (cada cuadro)
;   fino X: POKE 2090; mascara de filas: 2091-2093
;   fino Y y region: puerto $3FEF
; -------------------------------------------------------------
apply_all:
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
; draw_status: "FX=n FY=n H=n A=n" en la fila 0, con la fuente de la ROM
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
        jp putch

; putch: A = codigo ZX81 del caracter; lo pinta en el cursor (bitmap, 8 lineas) y avanza
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
        inc d                   ; la linea siguiente de la celda: +256
        djnz pc1
        ld hl,(cursor)
        inc hl
        ld (cursor),hl
        ret

; -------------------------------------------------------------
; keys: devuelve A = 1 si se pide salir (X)
;   P/O: FX +/-   Q/A: FY +/-  (se repiten cada 6 cuadros)
;   H, SPACE: una vez por pulsacion
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
        jr nz,k_end
        ld a,c
        or a
        jr z,k_end
        ld a,(auto)
        xor 1
        ld (auto),a
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
dirty:  defb 0
rep:    defb 0
prevh:  defb 0
prevs:  defb 0
ac1:    defb 0
ac2:    defb 0
q1:     defb 0
q2:     defb 0
cursor: defw 0

        end start
