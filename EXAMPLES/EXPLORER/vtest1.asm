; =============================================================
; VTEST1.ASM -- paso 1 del diagnostico incremental en HW real
;
; Lo MINIMO posible: monta la pantalla Superfast HiRes Spectrum estatica
; (bloque 4 = $8000, mapeo por defecto, igual que bounce.asm) y se
; congela en un bucle infinito justo despues. Sin teclado, sin video_off,
; sin volver al BASIC -- para que nada de lo que venga despues pueda
; corromper lo que se ve en pantalla.
;
; Resultado esperado: pantalla ENTERA magenta brillante, borde azul, y
; se queda asi PARA SIEMPRE (para salir hay que apagar/resetear).
;   - Si sale eso y se queda fijo -> el setup de video puro es correcto.
;   - Si sale negro/basura -> el fallo esta en este mismo setup (Chroma81,
;     modo Superfast, o el propio hardware en este punto).
;
; Cargar igual que el explorador (mismo stub BASIC, USR 24576). NO vuelve
; nunca al BASIC (hay que apagar/resetear la maquina).
; Ensamblar: pasmo vtest1.asm VTEST1.BIN
; =============================================================
        org 24576

VIDBASE         equ 8000h       ; bloque 4, mapeo por defecto (como bounce)
ATTRBASE        equ VIDBASE+1800h
TEST_ATTR       equ 05Fh        ; papel magenta brillante, tinta blanca

        jp start
        defs 3                  ; hueco (mantiene el mapa parecido al explorador)

start:
        di

        ; --- HFILE = $8000 (bloque 4) ---
        xor a
        ld (2043),a             ; HFILE bajo
        ld a,80h
        ld (2044),a             ; HFILE alto

        ; --- limpiar bitmap (6144 bytes a 0 -> pantalla solo "papel") ---
        ld hl,VIDBASE
        ld de,VIDBASE+1
        ld bc,17ffh
        ld (hl),0
        ldir

        ; --- atributos: color de prueba solido (768 bytes) ---
        ld hl,ATTRBASE
        ld de,ATTRBASE+1
        ld bc,2ffh
        ld a,TEST_ATTR
        ld (hl),a
        ldir

        ; --- Chroma81 ON (bit5), igual que bounce ---
        ld bc,7fefh
        ld a,20h
        out (c),a

        ; --- modo Superfast HiRes Spectrum ---
        ld a,172
        ld (2045),a

        ; --- borde azul ---
        ld a,1
        out (0fbh),a

forever:
        jr forever              ; congelado para siempre, nada mas se ejecuta

        end
