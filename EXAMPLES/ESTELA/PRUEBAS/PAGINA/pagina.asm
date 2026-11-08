; =============================================================
; PAGINA.ASM -- fase 0 (d) de ESTELA: codigo copiado a una pagina de la
; SRAM y ejecutado desde el bloque 1 ($2000) y desde el bloque 0 ($0000,
; en lugar de la ROM), comprobando que la ventana de POKEs de control
; (paleta y sprites) sigue funcionando desde alli.
;
; El codigo que se copia ("blob") no depende de donde este: solo saltos
; relativos, y su tabla la encuentra con CALL getpc (una rutina fija del
; bloque 2 que devuelve en HL la direccion de vuelta). Durante 2 segundos
; (100 imagenes, con el VSYNC):
;   - gira los colores de las cuatro barras cada 8 imagenes
;     (POKE 2063/2064/2065: paleta de 256);
;   - mueve el sprite 0 (un cuadrado blanco) de izquierda a derecha
;     (POKE 2100 y 2102: la ventana de los sprites).
;
; Fases (el borde dice cual se esta ejecutando):
;   0 blanco   el blob en su sitio ($48xx), con la ROM: la referencia
;   1 azul     copiado en la pagina 20, mapeada en el bloque 1: en $2000
;   2 magenta  copiado en la pagina 21, mapeada en el bloque 0: en $1000
;   3 cyan     la misma pagina 21, el blob en $07F0: se ejecuta DENTRO de
;              la ventana de POKEs ($07F6-$0853), y sus escrituras a 2063,
;              2064 y 2065 caen sobre sus propios bytes
;   fin verde  todo ha vuelto a su sitio; rojo si algo no cuadra
;
; Al acabar vuelve a BASIC con las paginas de antes. Devuelve en BC la suma de:
;   1 = la pagina 21 ha cambiado mientras estaba en el bloque 0 (el bloque
;       0 no era de solo lectura); cuantos bytes: PEEK 18435 + 256*PEEK 18436
;   2 = el bloque 0 no ha vuelto a tener la ROM
;   4 = el bloque 1 no ha vuelto a tener su pagina
; 0 = todo bien.
;
; Antes de mapear nada: DI y modo FAST (el .B81 lo pone), porque mientras
; la pagina esta en el bloque 0 no hay ROM (ni rutinas, ni NMI, ni INT).
; Las paginas se rellenan mapeandolas en el bloque 7 ($E000), que no se ve.
;
; Ensamblar: pasmo pagina.asm pagina.bin pagina.sym
; Cargar desde BASIC: PAGINA.B81 (CLEAR 18431, LOAD FAST "PAGINA.BIN" CODE 18432)
; =============================================================

        org 4800h

PG1     equ 20                  ; pagina para el bloque 1
PG0     equ 21                  ; pagina para el bloque 0
WIN     equ 0E000h              ; bloque 7: donde se rellenan las paginas
AT1     equ 2000h               ; donde se ejecuta en el bloque 1
AT0A    equ 1000h               ; y en el bloque 0
AT0B    equ 07F0h               ; ... dentro de la ventana de POKEs

        jp start
nerr:   defw 0                  ; 18435-18436: bytes cambiados en la pagina 21

        include "../comun.asm"

start:  di
        push ix                 ; la ROM usa IX en SLOW
        ld c,0                  ; paginas de antes
        call read_page
        ld (sv0),a
        ld c,1
        call read_page
        ld (sv1),a
        ld c,7
        call read_page
        ld (sv7),a

        ld a,PG1                ; pagina 20: blob en el offset 0 ($2000)
        call fillpg
        ld de,WIN+(AT1-2000h)
        call copyblob
        ld a,PG0                ; pagina 21: blob en $1000 y en $07F0
        call fillpg
        ld de,WIN+AT0A
        call copyblob
        ld de,WIN+AT0B
        call copyblob
        ld a,(sv7)
        ld c,7
        call map_page

        xor a                   ; modo 175, barras y sprite 0
        call mode175
        call bars
        call sprite0

        ld a,7                  ; 0: referencia, en su sitio
        call border
        call blob

        ld a,1                  ; 1: bloque 1
        call border
        ld a,PG1
        ld c,1
        call map_page
        call AT1
        ld a,(sv1)
        ld c,1
        call map_page

        ld a,3                  ; 2: bloque 0, en $1000
        call border
        ld a,PG0
        ld c,0
        call map_page
        call AT0A
        ld a,(sv0)
        ld c,0
        call map_page

        ld a,5                  ; 3: bloque 0, dentro de la ventana
        call border
        ld a,PG0
        ld c,0
        call map_page
        call AT0B
        ld a,(sv0)
        ld c,0
        call map_page

; --- comprobaciones ---
        ld e,0                  ; E = resultado
        ld a,PG0                ; la pagina 21 tiene que seguir igual
        ld c,7
        call map_page
        call cmppg
        ld a,(sv7)
        ld c,7
        call map_page
        ld hl,(nerr)
        ld a,h
        or l
        jr z,c1
        set 0,e
c1:     ld c,0                  ; el bloque 0 con su pagina y la ROM ($D3 = OUT (n),A)
        call read_page
        ld hl,sv0
        cp (hl)
        jr nz,c2
        ld a,(0)
        cp 0d3h
        jr z,c3
c2:     set 1,e
c3:     ld c,1
        call read_page
        ld hl,sv1
        cp (hl)
        jr z,c4
        set 2,e
c4:     ld a,4                  ; verde si todo bien, rojo si no
        inc e
        dec e
        jr z,c5
        ld a,2
c5:     call border
        push de
        ld b,100                ; 2 s para verlo
c6:     call waitvs
        djnz c6
        xor a                   ; sprite 0 apagado
        ld (2100),a
        ld (2101),a
        call mode85
        pop de
        pop ix
        ld c,e
        ld b,0
        ret

; -------------------------------------------------------------
; fillpg: A = pagina -> mapeada en el bloque 7 y puesta a 0
; -------------------------------------------------------------
fillpg: ld c,7
        call map_page
        ld hl,WIN
        ld de,WIN+1
        ld bc,1fffh
        ld (hl),0
        ldir
        ret

; copyblob: DE = destino
copyblob:
        ld hl,blob
        ld bc,blobend-blob
        ldir
        ret

; cmppg: compara la pagina del bloque 7 con lo que se escribio -> nerr
cmppg:  ld hl,0
        ld (nerr),hl
        ld hl,WIN
cp1:    ld a,(hl)               ; lo esperado: 0, salvo las dos copias del blob
        push hl
        call expect
        pop hl
        cp (hl)
        jr z,cp2
        push hl
        ld hl,(nerr)
        inc hl
        ld (nerr),hl
        pop hl
cp2:    inc hl
        ld a,h
        or l
        jr nz,cp1               ; hasta $FFFF
        ret

; expect: HL = direccion en WIN -> A = el byte que tiene que haber
expect: ld bc,-WIN
        add hl,bc               ; HL = offset en la pagina
        ld bc,-AT0B
        add hl,bc
        jr nc,ex0               ; antes de $07F0
        ld bc,blobend-blob
        or a
        sbc hl,bc
        jr c,exb                ; en la copia de $07F0
        add hl,bc
        ld bc,-(AT0A-AT0B)
        add hl,bc
        jr nc,ex0               ; entre las dos copias
        ld bc,blobend-blob
        or a
        sbc hl,bc
        jr nc,ex0               ; despues de la copia de $1000
exb:    add hl,bc               ; HL = offset dentro del blob
        ld bc,blob
        add hl,bc
        ld a,(hl)
        ret
ex0:    xor a
        ret

; -------------------------------------------------------------
; bars: cuatro barras de 40 pixeles (colores 1-4) en las filas 10-89
; sprite0: un cuadrado blanco de 8x8 (formato A, tinta 15 de pal16)
; -------------------------------------------------------------
bars:   ld hl,IMG+10*160
        ld c,80
br1:    ld a,1
br2:    ld b,40
br3:    ld (hl),a
        inc hl
        djnz br3
        inc a
        cp 5
        jr nz,br2
        dec c
        jr nz,br1
        ld b,0                  ; colores de partida
        jp setbars

sprite0:
        ld a,15                 ; pal16[15] = blanco
        ld (2063),a
        ld a,0ffh
        ld (2064),a
        ld (2066),a
        xor a
        ld (2100),a             ; sprite 0
        ld (2103),a             ; X < 256, transparente 0
        ld (2130),a             ; formato A
        ld a,32
        ld (2102),a
        ld a,32+46              ; fila 46 de la imagen
        ld (2104),a
        ld hl,2105
        ld b,8
sp1:    ld (hl),0f0h            ; tinta 15, papel 0
        inc hl
        djnz sp1
        ld b,16                 ; pixeles y mascara llenos
sp2:    ld (hl),0ffh
        inc hl
        djnz sp2
        ld a,1
        ld (2101),a
        ret

; setbars: B = fase -> la barra k (1-4) toma el color (k + B) mod 4 de la tabla
; (esta es la version de la ROM; el blob lleva la suya)
setbars:
        ld c,1
sb1:    ld a,c
        add a,b
        and 3
        add a,a
        ld hl,coltab
        add a,l
        ld l,a
        jr nc,sb2
        inc h
sb2:    ld a,c
        ld (2063),a
        ld a,(hl)
        ld (2064),a
        inc hl
        ld a,(hl)
        ld (2065),a
        inc c
        ld a,c
        cp 5
        jr nz,sb1
        ret

coltab: defb 00h,0fh            ; rojo        ({G,B}, {BRIGHT,R})
        defb 0f0h,00h           ; verde
        defb 0fh,00h            ; azul
        defb 0ffh,0ffh          ; blanco

; getpc: HL = direccion de vuelta (la del que llama)
getpc:  pop hl
        push hl
        ret

; -------------------------------------------------------------
; blob: lo que se copia a las paginas. Solo saltos relativos y el CALL a
; getpc; nada de la ROM. Toca todos los registros menos SP.
; -------------------------------------------------------------
blob:   call getpc              ; getpc esta en el bloque 2, siempre mapeado: el
bhere:  push hl                 ; CALL es absoluto pero vuelve aqui, donde este
        pop ix                  ; IX = bhere
        ld b,0                  ; B = imagen (0-99)
bl1:    in a,(0afh)             ; esperar el VSYNC
        rrca
        jr c,bl1
bl2:    in a,(0afh)
        rrca
        jr nc,bl2
        xor a                   ; sprite 0: X = 32 + imagen
        ld (2100),a
        ld a,b
        add a,32
        ld (2102),a
        ld a,b                  ; fase = imagen / 8
        rrca
        rrca
        rrca
        and 1fh
        ld d,a
        ld c,1                  ; barras 1-4
bl3:    ld a,c
        add a,d
        and 3
        add a,a
        add a,btab-bhere
        ld e,a
        push ix
        pop hl
        ld a,l                  ; HL = IX + E
        add a,e
        ld l,a
        jr nc,bl4
        inc h
bl4:    ld a,c
        ld (2063),a
        ld a,(hl)
        ld (2064),a
        inc hl
        ld a,(hl)
        ld (2065),a
        inc c
        ld a,c
        cp 5
        jr nz,bl3
        inc b
        ld a,b
        cp 100
        jr nz,bl1
        ret
btab:   defb 00h,0fh            ; rojo
        defb 0f0h,00h           ; verde
        defb 0fh,00h            ; azul
        defb 0ffh,0ffh          ; blanco
blobend:

; -------------------------------------------------------------
; variables
; -------------------------------------------------------------
sv0:    defb 0
sv1:    defb 0
sv7:    defb 0

        end start
