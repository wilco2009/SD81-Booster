; =============================================================
; COMUN.ASM -- rutinas comunes de las pruebas de la fase 0 de ESTELA
; (se incluye con include "../comun.asm")
; =============================================================

IMG       equ 8000h             ; imagen de 160x100, un byte por pixel
IMGLEN    equ 160*100
MAPPORT   equ 0E7h

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
; setpal1: entrada C de la paleta de 256 = DE (D = {BRIGHT,R}, E = {G,B})
; -------------------------------------------------------------
setpal1:
        ld a,c
        ld (2063),a
        ld a,e
        ld (2064),a
        ld a,d
        ld (2065),a
        ret

; -------------------------------------------------------------
; mode175: imagen a A, HFILE = $8000, sin scroll, color Chroma81 y modo
; 160x100 a 8 bpp (POKE 2045,175)
; -------------------------------------------------------------
mode175:
        ld hl,IMG
        ld de,IMG+1
        ld bc,IMGLEN-1
        ld (hl),a
        ldir
        xor a                   ; HFILE = $8000
        ld (2043),a
        ld a,IMG/256
        ld (2044),a
        xor a                   ; anillo del tamano de la imagen, sin desplazar
        ld (2072),a
        ld (2073),a
        ld (2074),a
        ld (2075),a
        ld bc,7fefh             ; color Chroma81, borde negro
        ld a,20h
        out (c),a
        ld a,175
        ld (2045),a
        ret

; mode85: vuelve al modo normal (sin scroll) y apaga el color de Chroma81
; ($7FEF = $0C, como el explorador): con el color encendido en el modo de
; codigo de caracter, el BASIC sale con los colores de la tabla y no se lee
mode85: xor a
        ld (2072),a
        ld (2073),a
        ld (2074),a
        ld (2075),a
        ld a,85
        ld (2045),a
        push bc
        ld bc,7fefh
        ld a,0ch
        out (c),a
        pop bc
        ret

; border: A = color del borde (0-15, como Chroma: bit 3 brillo, GRB)
border: push bc
        and 0fh
        or 20h
        ld bc,7fefh
        out (c),a
        pop bc
        ret

; -------------------------------------------------------------
; Mapeador (puerto E7h). Valen en paginacion simple y completa: en simple
; el mapeador toma la pagina de D7-D3 del dato (0-31), en completa de
; A13-A8 (el registro B de OUT (C),A).
; -------------------------------------------------------------
; map_page: A = pagina (0-63), C = bloque (0-7). Solo pisa A.
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

; read_page: C = bloque (0-7) -> A = pagina mapeada en el. Solo pisa A.
read_page:
        push bc
        ld b,c
        ld c,MAPPORT
        in a,(c)
        and 3fh
        pop bc
        ret

; -------------------------------------------------------------
; Cifras de 3x5 pixeles, a doble tamano (6x10) en la imagen.
; putnum: HL = numero, DE = direccion en la imagen, A = color.
; Escribe 5 cifras (con ceros a la izquierda). Toca todo.
; -------------------------------------------------------------
putnum: ld (pn_col),a
        ld (pn_dst),de
        ld ix,pn_pow
        ld b,5
pn1:    push bc
        ld e,(ix+0)
        ld d,(ix+1)
        xor a                   ; A = cifra: cuantas veces cabe DE en HL
pn2:    or a
        sbc hl,de
        jr c,pn3
        inc a
        jr pn2
pn3:    add hl,de
        push hl
        call putgl
        pop hl
        inc ix
        inc ix
        pop bc
        djnz pn1
        ret
pn_pow: defw 10000,1000,100,10,1

; putgl: A = glifo (0-9 cifras, 10 = D, 11 = F, 12 = W, 13 = espacio) en
; (pn_dst) con el color (pn_col); pn_dst avanza 8 pixeles.
putgl:  ld l,a
        ld h,0
        ld e,l
        ld d,h
        add hl,hl
        add hl,hl
        add hl,de               ; HL = 5 * glifo
        ld de,font35
        add hl,de
        ex de,hl                ; DE = las 5 filas del glifo
        ld hl,(pn_dst)
        ld c,5
pg1:    ld a,(de)
        inc de
        push de
        ld b,3                  ; 3 columnas, bit 2 = la de la izquierda
        rlca
        rlca
        rlca
        rlca
        rlca                    ; bit 2 -> bit 7
pg2:    rlca
        push af
        ld a,(pn_col)
        jr c,pg3
        xor a                   ; fondo: color 0
pg3:    ld (hl),a               ; 2x2 pixeles
        inc hl
        ld (hl),a
        ld de,159
        add hl,de
        ld (hl),a
        inc hl
        ld (hl),a
        ld de,-159
        add hl,de
        pop af
        djnz pg2
        ld de,320-6             ; dos filas mas abajo, al principio del glifo
        add hl,de
        pop de
        dec c
        jr nz,pg1
        ld hl,(pn_dst)
        ld de,8
        add hl,de
        ld (pn_dst),hl
        ret

font35: defb 7,5,5,5,7          ; 0
        defb 2,6,2,2,7          ; 1
        defb 7,1,7,4,7          ; 2
        defb 7,1,3,1,7          ; 3
        defb 5,5,7,1,1          ; 4
        defb 7,4,7,1,7          ; 5
        defb 7,4,7,5,7          ; 6
        defb 7,1,1,2,2          ; 7
        defb 7,5,7,5,7          ; 8
        defb 7,5,7,1,7          ; 9
        defb 6,5,5,5,6          ; D
        defb 7,4,6,4,4          ; F
        defb 5,5,5,7,5          ; W
        defb 0,0,0,0,0          ; espacio

pn_col: defb 0
pn_dst: defw 0
