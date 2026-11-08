; =============================================================
; ESTELA.ASM -- matamarcianos vertical para el modo 160x100 a 256 colores
; del SD81 Booster. Plan: claude/planning/juego160.md
;
; FASE 3: la nave del jugador sobre el fondo del nivel. Modo 175, anillo
; de 160x102 en $8000 con dos paneles laterales fijos de 16 pixeles, scroll
; vertical del nivel 1 y la nave con sus disparos en sprites de formato B:
;
;   - NIVEL1.I16 (de la SD, carpeta actual): la paleta de 256 del nivel y el
;     tileset, 160x100 con 60 tiles de 16x16 (10 por fila), en $C000 tal cual
;     (TILEADR de LEVEL.INC). La fila 96 lleva el dibujo de los paneles
;     (PANEL_ADDR: +0..15 el izquierdo, +144..159 el derecho). Los bloques 6 y
;     7 se mapean a las paginas 6 y 7 (libres en los modos del SD81).
;   - NIVEL1.MAP (de la SD): cabecera, oleadas y mapa de 8 tiles de ancho
;     (formato en juego160.md 8.3), detras del codigo (LVLBUF).
;   - Lineas del nivel: la fila r del mapa ocupa las lineas 16r..16r+15
;     contando desde abajo; la linea j del tile (0 = arriba) es la 16r+15-j.
;     Con s pixeles de scroll hechos, la fila k de la pantalla (0 = arriba)
;     es la linea 99+s-k, y la que entra por arriba en el siguiente paso es
;     la 100+s. Al llegar a s = LVL_END el scroll se para.
;   - Anillo: 102 filas de 160 bytes (POKE 2073,102). La fila del anillo
;     (vofs + k) mod 102 tiene la linea 99+s-k, k = 0..99. Para subir un
;     pixel se pinta la fila oculta vofs-1 con la linea 100+s y luego vofs
;     baja y s sube; el nuevo vofs se escribe en POKE 2072 justo despues del
;     siguiente VSYNC. Con las dos filas ocultas se pueden dar 2 px por imagen.
;   - Paneles: las columnas 0-15 y 144-159 son iguales en todas las filas
;     del anillo: se ven quietas aunque el anillo se mueva. Se pintan una vez.
;   - Ciclo de paleta: el agua (PAL_WATER), la lava (PAL_LAVA) y las luces
;     (PAL_LIGHTS) giran una posicion cada TCYW, TCYL y TCYG imagenes, en el
;     borrado, a partir de la paleta del .I16 (PALBUF).
;   - Sprites (formato B): HOJA.BIN (de la SD) en SHEET_ADDR = $7000 (bloque
;     3, POKE 2129,3) y la paleta de 16 PAL16 de HOJA.INC (POKE 2067,1). La
;     nave es 16x16 (sprites SP_NAVE..+3: arriba-izquierda, arriba-derecha,
;     abajo-izquierda, abajo-derecha) con 3 inclinaciones (NAVE_0 izquierda,
;     NAVE_1 recta, NAVE_2 derecha), la llama del motor debajo (SP_LLAMA, 2
;     fases que cambian cada 4 imagenes) y hasta NBALAS disparos (sprites
;     SP_BALA..+7). Coordenadas de pantalla: la zona de juego es x 16-143;
;     X de hardware = x + 32, Y = y + 32.
;   - Cada imagen: en el borrado, los sprites con el estado calculado en la
;     vuelta anterior, el ciclo de la paleta y el scroll; despues, las
;     teclas y el movimiento (update), que se ven en la imagen siguiente.
;
; Teclas: O P Q A = mover la nave (1 pixel por imagen), ESPACIO = disparar
;         (mantenido: uno cada TFIRE imagenes); con LOAD *JOY "OPQA " el
;         joystick hace lo mismo
;         S = velocidad (la del nivel al empezar: 1 px por imagen, borde
;         verde, o 1 px cada 2, cyan; luego 2 px por imagen, amarillo;
;         parado, rojo)
;         R = volver al principio del nivel (y la nave a su sitio)
;         X = salir a BASIC
; Devuelve en BC 0, o un error de carga: 1 no hay NIVEL1.I16, 2 no es un
; .I16, 3 no hay NIVEL1.MAP, 4 no es un nivel (ELV1), 5 no cabe antes de
; $7000, 6 cabecera del nivel incoherente, 7 no hay HOJA.BIN.
;
; Ensamblar: pasmo estela.asm estela.bin estela.sym   (o compila.bat)
; Cargar: ESTELA.B81 (CLEAR 18431, LOAD FAST "ESTELA.BIN" CODE 18432);
; NIVEL1.I16, NIVEL1.MAP y HOJA.BIN (de out/) en la misma carpeta de la SD.
; El codigo tiene que acabar antes de $7000 (la hoja de los sprites).
; =============================================================

        org 4800h

IMG     equ 8000h               ; anillo
RING    equ 102                 ; filas del anillo
VIS     equ 100                 ; filas visibles
PANEL   equ 16                  ; ancho de cada panel
PGTIL   equ 6                   ; paginas para los bloques 6 y 7
MAPPORT equ 0E7h
I16PIX  equ 16000               ; pixeles de un .I16
TCYW    equ 4                   ; imagenes por paso del ciclo del agua
TCYL    equ 6                   ; de la lava
TCYG    equ 8                   ; de las luces
; sprites (ventana de POKEs, Apendice H)
SPR_SEL equ 2100
SPR_EN  equ 2101
SPR_XL  equ 2102
SPR_XH  equ 2103                ; bit 0: X bit 8; bits 4-7: transparente
SPR_Y   equ 2104
SPR_MSK equ 2121                ; 2121-2128: mascara de las 8 filas
SPR_BLK equ 2129                ; bloque de la hoja
SPR_PG  equ 2130                ; pagina (0 = formato A)
SPR_XTR equ 2131                ; sprites extra
SP_NAVE equ 50                  ; nave: 50-53 (juego160.md 4.1)
SP_LLAMA equ 49                 ; llama del motor
SP_BALA equ 34                  ; disparos del jugador: 34-41
NBALAS  equ 8
SOFS    equ 32                  ; coordenada de sprite = pixel + 32
XMIN    equ 16                  ; la nave (esquina de arriba a la izquierda)
XMAX    equ 128                 ;   en x 16-128 (la zona de juego es 16-143)
YMIN    equ 0
YMAX    equ 76                  ;   y en y 0-76 (la llama, en 92-99)
XSTART  equ 72
YSTART  equ 76
VBALA   equ 3                   ; pixeles por imagen de los disparos
TFIRE   equ 6                   ; imagenes entre disparo y disparo

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

        call blackpal           ; todo negro mientras se carga y se pinta
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

        call loadlvl            ; A = 0, o el error
        or a
        jp nz,quit
        call sprinit
        call gminit
        call mktabs
        call panels
        call fillring
        call cycinit
        call setpal
        ld a,(LVLBUF+LVL_SCROLL) ; la velocidad del nivel: 1 -> 1 px por
        cp 2                    ; imagen (0), 2 o mas -> 1 cada 2 (1)
        ld a,0
        jr c,st1
        inc a
st1:    ld (speed),a
        xor a
        ld (fcnt),a
        ld a,2                  ; S y R sin pulsar
        ld (prevs),a
        ld a,8
        ld (prevr),a
        call showspd

; -------------------------------------------------------------
; bucle principal: una vuelta por imagen
; -------------------------------------------------------------
main:   call waitvs
        ld a,(vofs)             ; lo preparado en la vuelta anterior
        ld (2072),a
        call drawspr            ; los sprites, en el borrado
        call cycle              ; ciclo de la paleta, en el borrado
        call nsteps             ; B = pasos de esta imagen (0, 1 o 2)
        inc b
        jr mn2
mn1:    push bc
        call step
        pop bc
mn2:    djnz mn1
        call update             ; teclas, nave y disparos (para la siguiente)
        call keys
        jr nz,main
        xor a                   ; sin error

; --- salida (A = error): modo normal, color apagado, paginas de antes ---
quit:   ld (errc),a
        call sproff
        xor a
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
        ld a,(errc)
        ld c,a
        ld b,0
        ret

; -------------------------------------------------------------
; step: sube un pixel si no se ha llegado al final del nivel. Pinta la
; fila oculta vofs-1 del anillo con la linea 100+s, baja vofs y sube s.
; -------------------------------------------------------------
step:   ld hl,(scr)
        ld de,(lvlend)
        or a
        sbc hl,de
        ret nc                  ; s >= LVL_END: final del nivel
        ld a,(vofs)
        or a
        jr nz,sp1
        ld a,RING
sp1:    dec a
        ld hl,(scr)
        ld de,VIS
        add hl,de
        push af
        call prow
        pop af
        ld (vofs),a
        ld hl,(scr)
        inc hl
        ld (scr),hl
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

; fillring: el principio del nivel (s = 0, vofs = 0): la fila c del anillo
; tiene la linea 99-c (c = 0..99); las ocultas 100 y 101, las 101 y 100.
fillring:
        xor a
        ld (vofs),a
        ld hl,0
        ld (scr),hl
        ld c,0
fr1:    ld a,c
        cp VIS
        ld a,VIS-1
        jr c,fr2
        ld a,VIS+RING-1
fr2:    sub c
        ld l,a
        ld h,0
        ld a,c
        push bc
        call prow
        pop bc
        inc c
        ld a,c
        cp RING
        jr nz,fr1
        ret

; -------------------------------------------------------------
; prow: pinta las columnas 16-143 de la fila A del anillo con la linea HL
; del nivel. ~2.600 T. Toca todos los registros menos IX.
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
        pop hl                  ; HL = linea
        ld a,l                  ; lyoff = (15 - (L mod 16)) * 160
        and 15
        xor 15
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
        ld a,l                  ; HL = mapbase + (L / 16) * 8
        and 0f0h                ;    = mapbase + (L AND $FFF0) / 2
        ld l,a
        srl h
        rr l
        ld bc,(mapbase)
        add hl,bc
        ld b,8
pr1:    push bc
        ld a,(hl)               ; tile de esta columna
        inc hl
        push hl
        add a,a                 ; HL = TILEADR[t] + lyoff
        ld l,a
        ld h,0
        ld bc,TILEADR
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

; -------------------------------------------------------------
; loadlvl: NIVEL1.I16 (paleta a PALBUF, pixeles a $C000) y NIVEL1.MAP
; (a LVLBUF). A = 0 bien, o el error (ver la cabecera).
; -------------------------------------------------------------
loadlvl:
        ld hl,nmi16
        call f_open
        cp 0ffh
        ld e,1
        jp z,lderr0
        ld (fh),a
        ld hl,hdr
        ld bc,4
        call f_read_n
        ld hl,hdr
        ld de,magi16
        call cmp4
        ld e,2
        jp nz,lderr
        ld a,(fh)
        ld hl,PALBUF
        ld bc,512
        call f_read_n
        ld a,(fh)
        ld hl,TILESET_BASE
        ld bc,I16PIX
        call f_read_n
        ld a,(fh)
        call f_close

        ld hl,nmmap
        call f_open
        cp 0ffh
        ld e,3
        jp z,lderr0
        ld (fh),a
        ld hl,LVLBUF
        ld bc,LVL_HDR_SIZE
        call f_read_n
        ld hl,LVLBUF
        ld de,magmap
        call cmp4
        ld e,4
        jp nz,lderr
        ld hl,(LVLBUF+LVL_LEN)  ; LVL_HDR_SIZE <= longitud <= MAXLVL
        ld de,MAXLVL+1
        or a
        sbc hl,de
        ld e,5
        jp nc,lderr
        ld hl,(LVLBUF+LVL_LEN)
        ld de,LVL_HDR_SIZE
        or a
        sbc hl,de
        ld e,6
        jp c,lderr
        ld b,h                  ; el resto del fichero
        ld c,l
        ld a,(fh)
        ld hl,LVLBUF+LVL_HDR_SIZE
        call f_read_n
        ld a,(fh)
        call f_close
        ld hl,(LVLBUF+LVL_ROWS) ; al menos 7 filas (las 102 lineas del anillo)
        ld de,7
        or a
        sbc hl,de
        ld a,6
        ret c
        ld hl,(LVLBUF+LVL_ROWS) ; LVL_MAP + 8 * filas <= longitud
        add hl,hl
        add hl,hl
        add hl,hl
        ld de,(LVLBUF+LVL_MAP)
        add hl,de
        ex de,hl
        ld hl,(LVLBUF+LVL_LEN)
        or a
        sbc hl,de
        ld a,6
        ret c
        ld hl,(LVLBUF+LVL_MAP)
        ld de,LVLBUF
        add hl,de
        ld (mapbase),hl
        ld hl,(LVLBUF+LVL_END)
        ld (lvlend),hl
        ld hl,nmhoja            ; la hoja de los sprites
        call f_open
        cp 0ffh
        ld e,7
        jp z,lderr0
        ld (fh),a
        ld hl,SHEET_ADDR
        ld bc,SHEET_END-SHEET_ADDR
        call f_read_n
        ld a,(fh)
        call f_close
        xor a
        ret
lderr:  push de                 ; E = error, con el fichero abierto
        ld a,(fh)
        call f_close
        pop de
lderr0: ld a,e
        ret

; cmp4: Z si los 4 bytes de HL y DE son iguales
cmp4:   ld b,4
cm1:    ld a,(de)
        cp (hl)
        ret nz
        inc hl
        inc de
        djnz cm1
        ret

; -------------------------------------------------------------
; mktabs: rowtab (principio de cada fila del anillo) y lytab (y * 160)
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
        ld ix,lytab
        ld hl,0
        ld b,16
mk4:    ld (ix+0),l
        ld (ix+1),h
        inc ix
        inc ix
        add hl,de
        djnz mk4
        ret

; panels: las columnas 0-15 y 144-159 de las 102 filas del anillo, de la
; fila de los paneles del tileset
panels: ld hl,IMG
        ld b,RING
pa1:    push bc
        ex de,hl
        ld hl,PANEL_ADDR
        ld bc,PANEL
        ldir                    ; DE = columna 16
        ld hl,160-2*PANEL
        add hl,de
        ex de,hl
        ld hl,PANEL_ADDR+160-PANEL
        ld bc,PANEL
        ldir                    ; DE = columna 0 de la fila siguiente
        ex de,hl
        pop bc
        djnz pa1
        ret

; -------------------------------------------------------------
; Paleta. 2063 puntero, 2064 {G,B}, 2065 {BRIGHT,R}: escribe y avanza.
; -------------------------------------------------------------
blackpal:
        xor a
        ld (2063),a
        ld b,0
bp1:    ld (2064),a
        ld (2065),a
        djnz bp1
        ret

; setpal: las 256 entradas de PALBUF (la paleta del .I16)
setpal: xor a
        ld (2063),a
        ld hl,PALBUF
        ld b,0
sp8:    ld a,(hl)
        ld (2064),a
        inc hl
        ld a,(hl)
        ld (2065),a
        inc hl
        djnz sp8
        ret

; cycinit: los tres rangos sin girar y los contadores al principio
cycinit:
        xor a
        ld (ofw),a
        ld (ofl),a
        ld (ofg),a
        ld a,TCYW
        ld (cyw),a
        ld a,TCYL
        ld (cyl),a
        ld a,TCYG
        ld (cyg),a
        ret

; cycle: una vez por imagen; cada rango gira una posicion cuando su
; contador llega a 0 (como mucho 32 entradas, ~2.300 T)
cycle:  ld hl,cyw
        ld de,ofw
        ld bc,PAL_WATER_N*256+PAL_WATER
        ld a,TCYW
        call cyc1
        ld hl,cyl
        ld de,ofl
        ld bc,PAL_LAVA_N*256+PAL_LAVA
        ld a,TCYL
        call cyc1
        ld hl,cyg
        ld de,ofg
        ld bc,PAL_LIGHTS_N*256+PAL_LIGHTS
        ld a,TCYG
        ; sigue en cyc1
; HL = contador, DE = giro, A = imagenes por paso, C = primera entrada,
; B = cuantas
cyc1:   dec (hl)
        ret nz
        ld (hl),a
        ld a,(de)               ; giro = (giro + 1) mod B
        inc a
        cp b
        jr c,cc1
        xor a
cc1:    ld (de),a
        ld e,a
; cycrng: entradas C..C+B-1 = las de PALBUF giradas E posiciones: la
; entrada C+i toma el color C+((i+E) mod B)
cycrng: ld a,c
        ld (2063),a
        ld d,0
cr1:    ld a,d
        add a,e
        cp b
        jr c,cr2
        sub b
cr2:    add a,c
        ld l,a
        ld h,0
        add hl,hl
        push de
        ld de,PALBUF
        add hl,de
        pop de
        ld a,(hl)
        ld (2064),a
        inc hl
        ld a,(hl)
        ld (2065),a
        inc d
        ld a,d
        cp b
        jr nz,cr1
        ret

; -------------------------------------------------------------
; keys: S cambia la velocidad y R vuelve al principio (al pulsar).
; NZ = seguir, Z = X pulsada.
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
ky1:    ld a,0fbh               ; Q W E R T
        in a,(0feh)
        and 8
        ld c,a
        ld a,(prevr)
        ld b,a
        ld a,c
        ld (prevr),a
        or a
        jr nz,ky2               ; R no pulsada
        ld a,b
        or a
        jr z,ky2
        call restart
ky2:    ld a,0feh               ; X: salir
        in a,(0feh)
        and 4
        ret

; restart: el nivel desde el principio, con la paleta en negro mientras
; se repinta el anillo (unas 4 imagenes)
restart:
        call gminit
        call blackpal
        call fillring
        call cycinit
        jp setpal

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
; sprinit: hoja en el bloque SHEET_BLOCK, sin extra, la paleta de 16 de los
; sprites (PAL16) activada y los sprites de la nave, la llama y los
; disparos en formato B, con las 8 filas de la mascara a 1, transparente 0
; y apagados
; -------------------------------------------------------------
sprinit:
        ld a,SHEET_BLOCK
        ld (SPR_BLK),a
        xor a
        ld (SPR_XTR),a
        ld c,SP_BALA
si1:    ld a,c
        ld (SPR_SEL),a
        xor a
        ld (SPR_EN),a
        ld (SPR_XH),a
        ld hl,SPR_MSK
        ld b,8
        ld a,0ffh
si2:    ld (hl),a
        inc hl
        djnz si2
        ld a,NAVE_1
        ld (SPR_PG),a
        inc c
        ld a,c
        cp SP_NAVE+4
        jr nz,si1
        xor a                   ; PAL16: POKE 2063 puntero, 2064 {G,B},
        ld (2063),a             ; 2066 {BRIGHT,R} (escribe y avanza)
        ld hl,PAL16
        ld b,16
si3:    ld a,(hl)
        ld (2064),a
        inc hl
        ld a,(hl)
        ld (2066),a
        inc hl
        djnz si3
        ld a,1                  ; paleta de 16 activa
        ld (2067),a
        ret

; sproff: los 64 sprites apagados y en formato A, sin extra, la hoja en el
; bloque 2 y la paleta de 16 desactivada (como despues de un reset)
sproff: ld c,0
so1:    ld a,c
        ld (SPR_SEL),a
        xor a
        ld (SPR_EN),a
        ld (SPR_PG),a
        inc c
        ld a,c
        cp 64
        jr nz,so1
        xor a
        ld (SPR_XTR),a
        ld (2067),a
        ld a,2
        ld (SPR_BLK),a
        ret

; gminit: la nave abajo en el centro, recta, sin disparos
gminit: ld a,XSTART
        ld (shx),a
        ld a,YSTART
        ld (shy),a
        ld a,1
        ld (tilt),a
        xor a
        ld (cd),a
        ld (tick),a
        ld hl,balas
        ld b,3*NBALAS
gi1:    ld (hl),a
        inc hl
        djnz gi1
        ret

; -------------------------------------------------------------
; drawspr: los sprites con el estado de (shx, shy, tilt, tick, balas).
; ~1.300 T, en el borrado.
; -------------------------------------------------------------
drawspr:
        ld a,(tilt)             ; D = pagina de arriba a la izquierda
        add a,a
        add a,a
        add a,NAVE
        ld d,a
        ld a,(shx)
        add a,SOFS
        ld e,a                  ; E = X
        ld a,(shy)
        add a,SOFS
        ld l,a                  ; L = Y
        ld c,SP_NAVE
        call put1               ; arriba a la izquierda
        inc c
        inc d
        ld a,e
        add a,8
        ld e,a
        call put1               ; arriba a la derecha
        inc c
        inc d
        ld a,e
        sub 8
        ld e,a
        ld a,l
        add a,8
        ld l,a
        call put1               ; abajo a la izquierda
        inc c
        inc d
        ld a,e
        add a,8
        ld e,a
        call put1               ; abajo a la derecha
        ld a,e                  ; llama: (x + 4, y + 16)
        sub 4
        ld e,a
        ld a,l
        add a,8
        ld l,a
        ld a,(tick)
        rrca
        rrca
        and 1
        add a,LLAMA
        ld d,a
        ld c,SP_LLAMA
        call put1
        ld ix,balas             ; disparos
        ld c,SP_BALA
ds1:    ld a,(ix+0)
        or a
        jr z,ds2
        ld a,(ix+1)
        add a,SOFS
        ld e,a
        ld a,(ix+2)
        add a,SOFS
        ld l,a
        ld d,BALAJ_0
        call put1
        jr ds3
ds2:    ld a,c                  ; libre: apagado
        ld (SPR_SEL),a
        xor a
        ld (SPR_EN),a
ds3:    inc ix
        inc ix
        inc ix
        inc c
        ld a,c
        cp SP_BALA+NBALAS
        jr nz,ds1
        ret

; put1: sprite C en X = E, Y = L, pagina D, encendido (X < 256)
put1:   ld a,c
        ld (SPR_SEL),a
        ld a,e
        ld (SPR_XL),a
        ld a,l
        ld (SPR_Y),a
        ld a,d
        ld (SPR_PG),a
        ld a,1
        ld (SPR_EN),a
        ret

; -------------------------------------------------------------
; update: una vez por imagen, despues del scroll. O P Q A mueven la nave un
; pixel (dentro de XMIN-XMAX e YMIN-YMAX) e inclinan la nave segun el
; movimiento en X; los disparos suben VBALA pixeles y se borran al salir
; por arriba (y < -7); con ESPACIO pulsado y la espera (cd) a 0 sale un
; disparo nuevo en (x + 4, y - 6), en el primer hueco, y cd = TFIRE.
; -------------------------------------------------------------
update: ld hl,tick
        inc (hl)
        ld a,0dfh               ; Y U I O P: P bit 0, O bit 1
        in a,(0feh)
        cpl
        ld b,a
        ld c,0                  ; C = dx
        bit 0,b
        jr z,up1
        inc c
up1:    bit 1,b
        jr z,up2
        dec c
up2:    ld a,c                  ; inclinacion: dx + 1
        inc a
        ld (tilt),a
        ld a,(shx)
        add a,c
        cp XMIN
        jr c,up3
        cp XMAX+1
        jr c,up4
        ld a,XMAX
        jr up4
up3:    ld a,XMIN
up4:    ld (shx),a
        ld a,0fbh               ; Q: arriba
        in a,(0feh)
        cpl
        and 1
        ld b,a
        ld a,0fdh               ; A: abajo
        in a,(0feh)
        cpl
        and 1
        sub b                   ; dy
        ld c,a
        ld a,(shy)
        add a,c
        cp YMAX+1
        jr c,up5                ; dentro (0..YMAX)
        bit 7,c
        ld a,YMIN               ; se ha pasado por arriba
        jr nz,up5
        ld a,YMAX               ; o por abajo
up5:    ld (shy),a
        ld ix,balas             ; los disparos suben
        ld b,NBALAS
ub1:    ld a,(ix+0)
        or a
        jr z,ub2
        ld a,(ix+2)
        sub VBALA
        ld (ix+2),a
        add a,7                 ; y < -7: fuera por arriba
        jp p,ub2
        ld (ix+0),0
ub2:    inc ix
        inc ix
        inc ix
        djnz ub1
        ld hl,cd                ; espera entre disparos
        ld a,(hl)
        or a
        jr z,uf1
        dec (hl)
        ret nz                  ; todavia esperando
uf1:    ld a,07fh               ; ESPACIO (fila B N M . ESPACIO, bit 0)
        in a,(0feh)
        rrca
        ret c                   ; no pulsado
        ld ix,balas
        ld b,NBALAS
uf2:    ld a,(ix+0)
        or a
        jr z,uf3
        inc ix
        inc ix
        inc ix
        djnz uf2
        ret                     ; sin hueco
uf3:    ld (ix+0),1
        ld a,(shx)
        add a,4
        ld (ix+1),a
        ld a,(shy)
        sub 6
        ld (ix+2),a
        ld a,TFIRE
        ld (cd),a
        ret

; ficheros de la SD (ordenes 53, 55 y 57 del MCU)
        include "../PHOTO/mcu.asm"

; formatos del nivel, trayectorias y TILEADR (generado por tools/map2bin.py)
        include "out/LEVEL.INC"

; paginas de cada grafico de la hoja y PAL16 (generado por tools/spr2cel.py)
        include "out/HOJA.INC"

; -------------------------------------------------------------
; variables y tablas
; -------------------------------------------------------------
nmi16:  defb "NIVEL1.I16",0
nmmap:  defb "NIVEL1.MAP",0
nmhoja: defb "HOJA.BIN",0
magi16: defb "I160"
magmap: defb "ELV1"
vofs:   defb 0                  ; fila del anillo arriba (la que se escribe en el VSYNC)
scr:    defw 0                  ; s: pixeles de scroll hechos (arriba, la linea 99+s)
lvlend: defw 0                  ; LVL_END del nivel
mapbase: defw 0                 ; principio del mapa (LVLBUF + LVL_MAP)
speed:  defb 0
fcnt:   defb 0
prevs:  defb 2
prevr:  defb 8
sv6:    defb 0
sv7:    defb 0
errc:   defb 0
fh:     defb 0
hdr:    defs 4
lyoff:  defw 0
cyw:    defb 0                  ; ciclo de la paleta: contadores y giros
cyl:    defb 0
cyg:    defb 0
ofw:    defb 0
ofl:    defb 0
ofg:    defb 0
shx:    defb 0                  ; nave: x, y de pantalla, inclinacion (0-2)
shy:    defb 0
tilt:   defb 0
cd:     defb 0                  ; imagenes hasta poder disparar otra vez
tick:   defb 0                  ; contador de imagenes (la llama)
balas:  defs 3*NBALAS           ; disparos: activo, x, y (con signo)
rowtab: defs 2*RING
lytab:  defs 2*16
codeend:

; detras del codigo, sin ocupar el binario
PALBUF  equ codeend             ; la paleta del .I16 (512 bytes)
LVLBUF  equ PALBUF+512          ; NIVEL1.MAP
MAXLVL  equ 7000h-LVLBUF        ; hasta la hoja de los sprites

        end start
