; =============================================================
; ESTELA.ASM -- matamarcianos vertical para el modo 160x100 a 256 colores
; del SD81 Booster. Plan: claude/planning/juego160.md
;
; FASE 1: esqueleto. Modo 175, paleta de prueba, anillo de 160x102 en
; $8000 con dos paneles laterales fijos de 16 pixeles y scroll vertical
; continuo de un fondo de tiles de 16x16.
;
;   - El tileset es una imagen de 160x96 en $C000 con el formato de un .I16
;     (60 tiles, 10 por fila, 160 bytes de una fila a la siguiente). En la
;     fase 1 la genera el propio codigo; en la fase 2 sera el .I16 del nivel.
;     Los bloques 6 y 7 se mapean a las paginas 6 y 7 (libres en los modos
;     del SD81) mientras corre el juego.
;   - El mapa es de 8 tiles de ancho (128 pixeles). En la fase 1 tambien se
;     genera: tile(fila, columna) = ((fila*5 + columna*3) XOR (fila/4)) mod 60,
;     en 8 bits.
;   - Anillo: 102 filas de 160 bytes (POKE 2073,102). La fila del anillo
;     (vofs + r) mod 102 tiene la fila cy + r del mundo, r = 0..99. Para
;     subir un pixel se pinta la fila oculta vofs-1 con la fila cy-1 del
;     mundo y luego vofs y cy bajan uno; el nuevo vofs se escribe en POKE 2072
;     justo despues del siguiente VSYNC. Con las dos filas ocultas se pueden
;     dar 2 pixeles por imagen.
;   - Paneles: las columnas 0-15 y 144-159 son iguales en todas las filas del
;     anillo: se ven quietas aunque el anillo se mueva. Se pintan una vez.
;
; Teclas: S = velocidad (1 px por imagen, borde verde; 1 px cada 2,
;         cyan; 2 px por imagen, amarillo; parado, rojo)
;         X = salir a BASIC
;
; Ensamblar: pasmo estela.asm estela.bin estela.sym   (o compila.bat)
; Cargar: ESTELA.B81 (CLEAR 18431, LOAD FAST "ESTELA.BIN" CODE 18432)
; El codigo tiene que acabar antes de $7000 (la hoja de los sprites, fase 3).
; =============================================================

        org 4800h

IMG     equ 8000h               ; anillo
RING    equ 102                 ; filas del anillo
VIS     equ 100                 ; filas visibles
TILES   equ 0C000h              ; tileset (formato .I16, 160 bytes por fila)
TROWS   equ 96                  ; filas del tileset con tiles (6 x 16)
NTILES  equ 60
PANEL   equ 16                  ; ancho de cada panel
PGTIL   equ 6                   ; paginas para los bloques 6 y 7
MAPPORT equ 0E7h

        jp start

; -------------------------------------------------------------
; arranque
; -------------------------------------------------------------
start:  di
        push ix                 ; la ROM usa IX en SLOW
        ld c,6                  ; paginas de los bloques 6 y 7 (las de antes)
        call read_page
        ld (sv6),a
        ld c,7
        call read_page
        ld (sv7),a
        ld a,PGTIL
        ld c,6
        call map_page
        ld a,PGTIL+1
        ld c,7
        call map_page

        call blackpal           ; todo negro mientras se pinta
        xor a                   ; HFILE = $8000
        ld (2043),a
        ld a,IMG/256
        ld (2044),a
        xor a
        ld (2072),a             ; anillo: sin desplazar, 102 filas de 160
        ld (2074),a
        ld (2075),a
        ld a,RING
        ld (2073),a
        ld bc,7fefh             ; color Chroma81, borde negro
        ld a,20h
        out (c),a
        ld a,175                ; modo 160x100 a 8 bpp
        ld (2045),a

        call mktiles
        call mktabs
        call panels
        xor a                   ; el mundo desde la fila 0
        ld (vofs),a
        ld (mapok),a
        ld hl,0
        ld (cy),hl
        ld c,0                  ; filas 0..101 del anillo = filas 0..101 del mundo
st1:    ld a,c
        ld l,c
        ld h,0
        push bc
        call prow
        pop bc
        inc c
        ld a,c
        cp RING
        jr nz,st1
        call setpal
        xor a
        ld (speed),a
        ld (fcnt),a
        call showspd

; -------------------------------------------------------------
; bucle principal: una vuelta por imagen
; -------------------------------------------------------------
main:   call waitvs
        ld a,(vofs)             ; lo preparado en la vuelta anterior
        ld (2072),a
        call nsteps             ; B = pasos de esta imagen (0, 1 o 2)
        inc b
        jr mn2
mn1:    push bc
        call step
        pop bc
mn2:    djnz mn1
        call keys
        jr nz,main

; --- salida: modo normal, color apagado, paginas de antes ---
quit:   xor a
        ld (2072),a
        ld (2073),a
        ld (2074),a
        ld (2075),a
        ld a,85
        ld (2045),a
        ld bc,7fefh             ; color de Chroma81 apagado: el BASIC se lee
        ld a,0ch
        out (c),a
        ld a,(sv6)
        ld c,6
        call map_page
        ld a,(sv7)
        ld c,7
        call map_page
        ld hl,4000h             ; la sombra de $C000-$FFFF vuelve a ser la de
        ld de,0C000h            ; $4000-$7FFF (son las mismas paginas: se
        ld bc,4000h             ; reescriben los mismos bytes)
        ldir
        pop ix
        ld bc,0
        ret

; -------------------------------------------------------------
; step: sube un pixel. Pinta la fila oculta vofs-1 del anillo con la fila
; cy-1 del mundo y baja vofs y cy.
; -------------------------------------------------------------
step:   ld a,(vofs)
        or a
        jr nz,sp1
        ld a,RING
sp1:    dec a
        ld hl,(cy)
        dec hl
        push af
        push hl
        call prow
        pop hl
        pop af
        ld (vofs),a
        ld (cy),hl
        ret

; nsteps: B = pasos segun la velocidad (0: 1, 1: 1 cada 2 imagenes, 2: 2, 3: 0)
nsteps: ld hl,fcnt
        inc (hl)
        ld a,(speed)
        ld b,1
        or a
        ret z
        dec a
        jr nz,ns1
        ld b,(hl)               ; 1 cada 2: el bit 0 del contador
        ld a,b
        and 1
        ld b,a
        ret
ns1:    dec a
        ld b,2
        ret z
        ld b,0
        ret

; -------------------------------------------------------------
; prow: pinta las columnas 16-143 de la fila A del anillo con la fila HL
; del mundo. ~2.600 T. Toca todos los registros menos IX.
; -------------------------------------------------------------
prow:   push hl
        ld l,a                  ; DE = rowtab[A] + PANEL
        ld h,0
        add hl,hl
        ld de,rowtab
        add hl,de
        ld e,(hl)
        inc hl
        ld d,(hl)
        ld hl,PANEL
        add hl,de
        ex de,hl
        pop hl                  ; HL = fila del mundo
        ld a,l                  ; lyoff = (y mod 16) * 160
        and 15
        add a,a
        ld c,a
        ld b,0
        push hl
        ld hl,lytab
        add hl,bc
        ld c,(hl)
        inc hl
        ld b,(hl)
        ld (lyoff),bc
        pop hl
        ld a,l                  ; A = fila de tiles (y / 16, 8 bits)
        rrca
        rrca
        rrca
        rrca
        and 0fh
        ld c,a
        ld a,h
        add a,a
        add a,a
        add a,a
        add a,a
        or c
        push de                 ; getmap usa D y E
        call getmap
        pop de
        ld hl,mapbuf
        ld b,8
pr1:    push bc
        ld a,(hl)               ; tile de esta columna
        inc hl
        push hl
        add a,a                 ; HL = tbase[t] + lyoff
        ld l,a
        ld h,0
        ld bc,tbase
        add hl,bc
        ld a,(hl)
        inc hl
        ld h,(hl)
        ld l,a
        ld bc,(lyoff)
        add hl,bc
        ldi                     ; 16 pixeles
        ldi
        ldi
        ldi
        ldi
        ldi
        ldi
        ldi
        ldi
        ldi
        ldi
        ldi
        ldi
        ldi
        ldi
        ldi
        pop hl
        pop bc
        djnz pr1
        ret

; getmap: A = fila de tiles -> mapbuf = sus 8 tiles (se guarda la ultima)
; FASE 1: generado. tile(f, c) = ((f*5 + c*3) XOR (f/4)) mod 60, en 8 bits
getmap: ld hl,mapok
        ld c,a
        ld a,(hl)
        or a
        jr z,gm0
        ld a,(mapcur)
        cp c
        ret z
gm0:    ld (hl),1
        ld a,c
        ld (mapcur),a
        add a,a                 ; D = f*5
        add a,a
        add a,c
        ld d,a
        ld a,c                  ; E = f/4
        rrca
        rrca
        and 3fh
        ld e,a
        ld hl,mapbuf
        ld b,8
gm1:    ld a,d
        xor e
gm2:    cp NTILES
        jr c,gm3
        sub NTILES
        jr gm2
gm3:    ld (hl),a
        inc hl
        ld a,d
        add a,3
        ld d,a
        djnz gm1
        ret

; -------------------------------------------------------------
; mktiles: FASE 1: el tileset generado en $C000 (en la fase 2, el .I16).
; Pixel (x, y) del tile t: 0 (negro) en x = 0 o y = 0; 255 (blanco) en la
; diagonal x = y; si no, c1(t) si (x XOR y) AND 8, c2(t) si no, con
; c1(t) = (7t mod 220) + 8 y c2(t) = (13t mod 220) + 12.
; -------------------------------------------------------------
mktiles:
        ld ix,ctab              ; ctab[t] = c1(t), ctab[60+t] = c2(t)
        ld b,NTILES
        ld c,0
        ld e,0
mt0:    ld a,c
        add a,8
        ld (ix+0),a
        ld a,e
        add a,12
        ld (ix+NTILES),a
        inc ix
        ld a,c
        add a,7
        cp 220
        jr c,mt1
        sub 220
mt1:    ld c,a
        ld a,e
        add a,13
        cp 220
        jr c,mt2
        sub 220
mt2:    ld e,a
        djnz mt0
        ld hl,TILES
        ld d,0                  ; D = fila (0-95)
my:     ld e,0                  ; E = columna (0-159)
mx:     ld a,e
        and 15
        ld b,a                  ; B = x
        ld a,d
        and 15
        ld c,a                  ; C = y
        xor a
        cp b
        jr z,mpx                ; x = 0: negro
        cp c
        jr z,mpx                ; y = 0: negro
        ld a,b
        cp c
        ld a,255
        jr z,mpx                ; diagonal: blanco
        ld a,b
        xor c
        and 8
        ld b,a                  ; B = 8: c1, 0: c2
        ld a,d                  ; t = (fila / 16) * 10 + columna / 16
        and 0f0h
        rrca
        ld c,a
        rrca
        rrca
        add a,c
        ld c,a
        ld a,e
        rrca
        rrca
        rrca
        rrca
        and 0fh
        add a,c
        push hl
        ld hl,ctab
        bit 3,b
        jr nz,mc1
        ld hl,ctab+NTILES
mc1:    add a,l
        ld l,a
        jr nc,mc2
        inc h
mc2:    ld a,(hl)
        pop hl
mpx:    ld (hl),a
        inc hl
        inc e
        ld a,e
        cp 160
        jr nz,mx
        inc d
        ld a,d
        cp TROWS
        jr nz,my
        ret

; -------------------------------------------------------------
; mktabs: rowtab (principio de cada fila del anillo), tbase (principio de
; cada tile en el tileset) y lytab (y * 160)
; -------------------------------------------------------------
mktabs: ld hl,IMG
        ld ix,rowtab
        ld de,160
        ld b,RING
mk1:    ld (ix+0),l
        ld (ix+1),h
        inc ix
        inc ix
        add hl,de
        djnz mk1
        ld ix,tbase
        ld hl,TILES             ; HL = principio de la fila de tiles
        ld c,6
mk2:    push hl
        ld b,10
mk3:    ld (ix+0),l
        ld (ix+1),h
        inc ix
        inc ix
        ld de,16
        add hl,de
        djnz mk3
        pop hl
        ld de,16*160
        add hl,de
        dec c
        jr nz,mk2
        ld ix,lytab
        ld hl,0
        ld de,160
        ld b,16
mk4:    ld (ix+0),l
        ld (ix+1),h
        inc ix
        inc ix
        add hl,de
        djnz mk4
        ret

; panels: las columnas 0-15 y 144-159 de las 102 filas del anillo
panels: ld hl,IMG
        ld b,RING
pa1:    push bc
        ex de,hl
        ld hl,pnl_l
        ld bc,PANEL
        ldir                    ; DE = columna 16
        ld hl,160-2*PANEL
        add hl,de
        ex de,hl
        ld hl,pnl_r
        ld bc,PANEL
        ldir                    ; DE = columna 0 de la fila siguiente
        ex de,hl
        pop bc
        djnz pa1
        ret

; colores de los paneles: rampa de grises 240-255 de la paleta
pnl_l:  defb 242,244,246,248,250,252,254,255,253,251,249,247,245,243,241,240
pnl_r:  defb 240,241,243,245,247,249,251,253,255,254,252,250,248,246,244,242

; -------------------------------------------------------------
; Paleta. 2063 puntero, 2064 {G,B}, 2065 {BRIGHT,R}: escribe y avanza.
; setpal: 0-239 RGB 3-3-2 (la de fabrica), 240-255 grises (0-15 en los
; cuatro canales, el 255 blanco).
; -------------------------------------------------------------
blackpal:
        xor a
        ld (2063),a
        ld b,0
bp1:    ld (2064),a
        ld (2065),a
        djnz bp1
        ret

setpal: xor a
        ld (2063),a
        ld c,0
sp8:    ld a,c
        cp 240
        jr nc,spg
        rrca                    ; G = bits 4-2
        rrca
        and 7
        call lev3
        add a,a
        add a,a
        add a,a
        add a,a
        ld b,a
        ld a,c                  ; B = bits 1-0: 0, 5, 10, 15
        and 3
        ld d,a
        add a,a
        add a,a
        add a,d
        or b
        ld (2064),a
        ld a,c                  ; R = bits 7-5
        rlca
        rlca
        rlca
        and 7
        call lev3
        ld (2065),a
        jr sp9
spg:    and 0fh                 ; gris d: los cuatro canales a d
        ld b,a
        add a,a
        add a,a
        add a,a
        add a,a
        or b
        ld (2064),a
        ld (2065),a
sp9:    inc c
        jr nz,sp8
        ret
lev3:   push hl
        ld hl,l3tab
        add a,l
        ld l,a
        jr nc,lv1
        inc h
lv1:    ld a,(hl)
        pop hl
        ret
l3tab:  defb 0,2,4,6,9,11,13,15

; -------------------------------------------------------------
; keys: S cambia la velocidad (al pulsar). NZ = seguir, Z = X pulsada.
; -------------------------------------------------------------
keys:   ld a,0fdh               ; A S D F G
        in a,(0feh)
        and 2
        ld c,a
        ld a,(prevs)
        ld b,a
        ld a,c
        ld (prevs),a
        or a
        jr nz,ky1               ; S no pulsada
        ld a,b
        or a
        jr z,ky1                ; ya lo estaba
        ld a,(speed)
        inc a
        and 3
        ld (speed),a
        call showspd
ky1:    ld a,0feh               ; X: salir
        in a,(0feh)
        and 4
        ret

; showspd: el borde segun la velocidad
showspd:
        ld a,(speed)
        ld e,a
        ld d,0
        ld hl,spdcol
        add hl,de
        ld a,(hl)
        or 20h
        ld bc,7fefh
        out (c),a
        ret
spdcol: defb 4,5,6,2            ; verde, cyan, amarillo, rojo

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

; Mapeador (puerto E7h; vale en paginacion simple y completa)
; map_page: A = pagina, C = bloque. Solo pisa A.
map_page:
        push bc
        ld b,a
        and 1fh
        rlca
        rlca
        rlca
        or c
        ld c,MAPPORT
        out (c),a
        pop bc
        ret
; read_page: C = bloque -> A = pagina. Solo pisa A.
read_page:
        push bc
        ld b,c
        ld c,MAPPORT
        in a,(c)
        and 3fh
        pop bc
        ret

; -------------------------------------------------------------
; variables y tablas
; -------------------------------------------------------------
vofs:   defb 0                  ; fila del anillo arriba (la que se escribe en el VSYNC)
cy:     defw 0                  ; fila del mundo arriba (la de vofs)
speed:  defb 0
fcnt:   defb 0
prevs:  defb 2
sv6:    defb 0
sv7:    defb 0
mapok:  defb 0
mapcur: defb 0
lyoff:  defw 0
mapbuf: defs 8
ctab:   defs 2*NTILES
rowtab: defs 2*RING
tbase:  defs 2*NTILES
lytab:  defs 2*16
codeend:

        end start
