; =============================================================
; SCROLLM.ASM -- prueba del MOTOR de scroll por hardware (fase 2)
;
; Scroll horizontal CONTINUO en Superfast HiRes nativo (POKE 2045,171) o
; Spectrum (172): el fino (POKE 2090, 0-7 px) y, cuando llega a 7 (o a 0 al
; ir a la derecha), la orden de movimiento por el puerto del depurador
; ($3FEF, registro 9) que mueve la memoria UN byte. Ver
; claude/planning/scroll_hardware.md.
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
; Teclas:  P = izquierda (se repite)   O = derecha (se repite)
;          M = atributos de HiRes modo 0 / modo 1 (se redibuja todo)
;          T = HiRes / Spectrum (se redibuja todo)
;          H = HUD: las filas 0 y 23 no se mueven (mascara de filas, POKE 2091-2093)
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
;   Ruido: $9F00-$9FFF.  Tablas de trabajo del programa desde $A400.
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
NOISE   equ 9F00h               ; zona libre donde escribe el ruido
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
        jr z,m_free
        call getbusy            ; hay una orden: el motor ha acabado?
        jr nz,main              ; todavia no: nada mas en este cuadro
        ld a,(pend)             ; acabado: cerrar el movimiento
        ld b,a
        xor a
        ld (pend),a
        ld a,b
        cp 1
        jr z,m_left
        ld hl,wc0               ; derecha: la columna 0 de pantalla es una antes en el mundo
        dec (hl)
        ld a,7
        ld (fx),a
        ld (2090),a
        call put_a1col0
        call put_L
        jr main
m_left: ld hl,wc0               ; izquierda
        inc (hl)
        xor a
        ld (fx),a
        ld (2090),a
        call put_R
        jr main
m_free: call keys
        or a
        jr nz,quit
        call step
        jr main

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
        in a,(0afh)
        rrca
        jr c,wn1
wn2:    ld (hl),a
        inc l
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
        ret

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
st_a:   ld a,(adir)
        or a
        jr z,st_left
        jr st_right
st_keys:ld a,0dfh
        in a,(0feh)
        bit 0,a                 ; P
        jr z,st_left
        bit 1,a                 ; O
        jr z,st_right
        ret
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
        jr z,k_end
        ld a,(auto)
        xor 1
        ld (auto),a
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

; src_bm / src_a0 / src_a1 / src_a1s: A = columna del mundo; HL = su tabla (BMC, A0C, A1C, A1S)
src_bm: ld hl,BMC
        ld de,192
        and 1
        jr mulb
src_a0: ld hl,A0C
        ld de,192
        and 7
        jr mulb
src_a1: ld hl,A1C
        ld de,24
        and 7
        jr mulb
src_a1s:ld hl,A1S
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
        jp put_L

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

; put_R: la columna del mundo wc0+32 en los bufers de la derecha
put_R:  ld a,(vmode)
        or a
        jr nz,put_R_sp
        ld a,(wc0)
        add a,32
        ld (pwc),a
        call src_bm
        ld de,RBM
        ld bc,192
        ldir
        ld a,(pwc)
        call src_a0
        ld de,RA0
        ld bc,192
        ldir
        ld a,(pwc)
        call src_a1
        push hl
        pop ix
        ld hl,A1+32
        jp copy_a1
put_R_sp:
        ld a,(wc0)
        add a,32
        ld (pwc),a
        call src_bm
        ld de,SPRB
        ld bc,192
        ldir
        ld a,(pwc)
        call src_a1s
        ld de,SPRA
        ld bc,24
        ldir
        ret

; put_L: la columna del mundo wc0-1 en los bufers de la izquierda
put_L:  ld a,(vmode)
        or a
        jr nz,put_L_sp
        ld a,(wc0)
        dec a
        ld (pwc),a
        call src_bm
        ld de,LBM
        ld bc,192
        ldir
        ld a,(pwc)
        call src_a0
        ld de,LA0
        ld bc,192
        ldir
        ret
put_L_sp:
        ld a,(wc0)
        dec a
        ld (pwc),a
        call src_bm
        ld de,SPLB
        ld bc,192
        ldir
        ld a,(pwc)
        call src_a1s
        ld de,SPLA
        ld bc,24
        ldir
        ret

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
fx:     defb 0
pend:   defb 0                  ; orden mandada y sin cerrar (1 izquierda, 2 derecha)
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
