; =============================================================
; VTEST2.ASM -- paso 2 del diagnostico incremental en HW real
;
; Parte de vtest1 (pantalla estatica, bloque 4 = $8000, sin remapeo,
; confirmado que funciona) y añade el "marco": banner fila 0 + barra de
; iconos fila 23, pegados con blit_row (identico al del explorador,
; solo con las direcciones recalculadas para el bloque 4 en vez del 7).
; Todavia SIN tocar el MCU (nada de listado de archivos ni ruta). Se
; congela en un bucle infinito justo despues -- nada de lo que venga
; despues puede corromper lo que se ve.
;
; Resultado esperado: fondo blanco, banner "SD81 BOOSTER" en la fila 0,
; barra de iconos en la fila 23, y se queda asi PARA SIEMPRE.
;   - Si sale bien -> el blit de recursos estaticos funciona en HW real;
;     el problema esta mas adelante (MCU/listado).
;   - Si sale mal -> el problema esta en blit_row o en los propios
;     recursos (bg_row0.bin/bg_row23.bin), no en el MCU.
;
; Cargar igual que el explorador (mismo stub BASIC, USR 24576). NO vuelve
; nunca al BASIC (hay que apagar/resetear la maquina).
; Ensamblar: pasmo vtest2.asm VTEST2.BIN
; =============================================================
        org 24576

VIDBASE         equ 8000h       ; bloque 4, mapeo por defecto (como bounce)
VIDBASE_HI      equ 80h
ATTRBASE_HI     equ VIDBASE_HI+18h
NORM_ATTR       equ 038h        ; papel blanco, tinta negra

        jp start
        defs 3                  ; hueco (mantiene el mapa parecido al explorador)

start:
        di

        ; --- HFILE = $8000 (bloque 4) ---
        xor a
        ld (2043),a             ; HFILE bajo
        ld a,VIDBASE_HI
        ld (2044),a             ; HFILE alto

        ; --- limpiar pantalla (bitmap a 0, atributos a NORM_ATTR) ---
        ld hl,VIDBASE
        ld de,VIDBASE+1
        ld bc,17ffh
        ld (hl),0
        ldir
        ld hl,VIDBASE+1800h
        ld de,VIDBASE+1801h
        ld bc,2ffh
        ld a,NORM_ATTR
        ld (hl),a
        ldir

        ; --- Chroma81 ON (bit5) ---
        ld bc,7fefh
        ld a,20h
        out (c),a

        ; --- modo Superfast HiRes Spectrum ---
        ld a,172
        ld (2045),a

        ; --- borde azul ---
        ld a,1
        out (0fbh),a

        ; --- pegar el marco: fila 0 (banner) y fila 23 (iconos) ---
        ld ix,BG_ROW0
        xor a
        call blit_row
        ld ix,BG_ROW23
        ld a,23
        call blit_row

forever:
        jr forever              ; congelado para siempre, nada mas se ejecuta

; -------------------------------------------------------------
; blit_row / calc_bmp_addr / calc_attr_addr: copia literal de
; explorer.asm (ver ahi los comentarios completos), solo con
; VIDBASE_HI/ATTRBASE_HI recalculados para el bloque 4.
; -------------------------------------------------------------
blit_row:
        push af
        call calc_bmp_addr
        pop af
        ld b,8
br_loop:
        push bc
        push hl
        ex de,hl
        push ix
        pop hl
        ld bc,32
        ldir
        push hl
        pop ix
        pop hl
        inc h
        pop bc
        djnz br_loop

        call calc_attr_addr
        ex de,hl
        push ix
        pop hl
        ld bc,32
        ldir
        ret

calc_bmp_addr:
        ld l,a
        and 0F8h
        add a,VIDBASE_HI
        ld h,a
        ld a,l
        and 7
        rrca
        rrca
        rrca
        ld l,a
        ret

calc_attr_addr:
        ld h,0
        ld l,a
        add hl,hl
        add hl,hl
        add hl,hl
        add hl,hl
        add hl,hl
        ld a,h
        add a,ATTRBASE_HI
        ld h,a
        ret

BG_ROW0:
        incbin "bg_row0.bin"
BG_ROW23:
        incbin "bg_row23.bin"

        end
