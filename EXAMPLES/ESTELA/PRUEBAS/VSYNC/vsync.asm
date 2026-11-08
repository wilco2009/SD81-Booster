; =============================================================
; VSYNC.ASM -- fase 0 (b) de ESTELA: T-estados entre el VSYNC y la
; primera linea de la imagen en el modo 160x100 a 256 colores.
;
; En cada imagen, justo al ver el flanco de subida del VSYNC (puerto $AF,
; bit 0) el color 0 de la paleta pasa a NEGRO, y D T-estados despues pasa
; a BLANCO. Toda la imagen es del color 0 (salvo la regla y las cifras), asi
; que se ve negra por encima del corte y blanca por debajo:
;   - si D es pequeno, el corte cae antes de la imagen: toda blanca;
;   - el D con el que la primera linea de la imagen se vuelve negra es el
;     tiempo del VSYNC a la linea 58 (la primera de la imagen);
;   - cada fila de la imagen son 2 lineas de TV (~414 T).
;
; D = 116 + 26*n T-estados, contados desde el final del IN que ve el bit a
; 1 hasta el final de la escritura de POKE 2065 que pone el blanco. La
; espera del VSYNC tiene un margen de 27 T (lo que dura una vuelta).
;
; Pantalla: regla verde a los lados (raya corta cada 10 filas, larga cada
; 50) y, en rojo, D, F (T-estados de una imagen, de VSYNC a VSYNC) y W
; (lo que dura el VSYNC a 1), medidos al empezar y con M.
;
; Teclas: Q / A = D +26 / -26 T (mantener)   P / O = D +208 / -208 T
;         M = volver a medir F y W            X = salir
;
; Ensamblar: pasmo vsync.asm vsync.bin vsync.sym
; Cargar desde BASIC: VSYNC.B81 (CLEAR 18431, LOAD FAST "VSYNC.BIN" CODE 18432)
; =============================================================

        org 4800h

DK      equ 116                 ; D = DK + 26*n
NMAX    equ 2300                ; D como mucho 59.916 T

start:  di
        push ix                 ; la ROM usa IX en SLOW
        xor a
        call mode175            ; imagen a 0, modo 175
        ld a,1                  ; borde azul: se ve donde acaba la imagen
        call border
        ld c,0                  ; 0 = blanco hasta que empiece el bucle
        ld de,0ffffh
        call setpal1
        ld c,1                  ; 1 = rojo (cifras)
        ld de,0f00h
        call setpal1
        ld c,2                  ; 2 = verde (regla)
        ld de,00f0h
        call setpal1
        call ruler
        call labels
        call measure
        call showfw
        call shownum

; -------------------------------------------------------------
; Bucle: una vuelta por imagen. Los T-estados de aqui hasta la escritura
; del blanco estan contados (DK); no cambiar sin recontar.
; -------------------------------------------------------------
main:
w1:     in a,(0afh)             ; esperar a que el VSYNC este a 0
        rrca
        jr c,w1
w2:     in a,(0afh)             ; 11  flanco de subida: aqui empieza D
        rrca                    ; 4
        jr nc,w2                ; 7 (sin saltar)
        xor a                   ; 4
        ld (2063),a             ; 13  color 0 = negro
        ld (2064),a             ; 13
        ld (2065),a             ; 13
        ld bc,(nval)            ; 20
dly:    dec bc                  ; 6
        ld a,b                  ; 4
        or c                    ; 4
        jr nz,dly               ; 12 (7 la ultima vez)
        xor a                   ; 4
        ld (2063),a             ; 13
        dec a                   ; 4
        ld (2064),a             ; 13
        ld (2065),a             ; 13  color 0 = blanco: el corte
                                ; D = 4+7+4+39+20 + 26n-5 + 47 = 116 + 26n
        call keys
        jr nz,main

quit:   call mode85
        pop ix
        ld bc,0
        ret

; -------------------------------------------------------------
; keys: Q A P O mueven n, M vuelve a medir. NZ = seguir, Z = X pulsada.
; -------------------------------------------------------------
keys:   ld hl,(nval)
        ld d,h
        ld e,l                  ; DE = n de antes
        ld a,0fbh               ; Q: +1
        in a,(0feh)
        rrca
        jr c,k1
        inc hl
k1:     ld a,0fdh               ; A: -1
        in a,(0feh)
        rrca
        jr c,k2
        dec hl
k2:     ld a,0dfh               ; P: +8, O: -8
        in a,(0feh)
        rrca
        jr c,k3
        ld bc,8
        add hl,bc
k3:     rrca
        jr c,k4
        ld bc,-8
        add hl,bc
k4:     bit 7,h                 ; por debajo de 1 -> 1
        jr nz,klo
        ld a,h
        or l
        jr nz,k5
klo:    ld hl,1
k5:     push hl                 ; por encima de NMAX -> NMAX
        ld bc,NMAX+1
        or a
        sbc hl,bc
        pop hl
        jr c,k6
        ld hl,NMAX
k6:     ld (nval),hl
        or a
        sbc hl,de
        call nz,shownum         ; ha cambiado: escribir D
        ld a,7fh                ; M: medir otra vez
        in a,(0feh)
        and 4
        jr nz,k7
        call measure
        call showfw
km:     ld a,7fh                ; esperar a que se suelte
        in a,(0feh)
        and 4
        jr z,km
k7:     ld a,0feh               ; X: salir
        in a,(0feh)
        and 4
        ret

; -------------------------------------------------------------
; measure: cuenta vueltas de 33 T con el VSYNC a 1 y luego a 0.
; W = 33 * vueltas a 1, F = 33 * (todas) + 26.
; -------------------------------------------------------------
measure:
        call waitvs             ; justo despues del flanco de subida
        ld hl,0
ms1:    inc hl                  ; 6
        in a,(0afh)             ; 11
        rrca                    ; 4
        jr c,ms1                ; 12
        ld (wcnt),hl            ; 16
        ld hl,0                 ; 10
ms2:    inc hl
        in a,(0afh)
        rrca
        jr nc,ms2
        ld de,(wcnt)
        add hl,de
        call mul33
        ld de,26
        add hl,de
        ld (fval),hl
        ld hl,(wcnt)
        call mul33
        ld (wval),hl
        ret

mul33:  ld d,h                  ; HL = 33 * HL
        ld e,l
        add hl,hl
        add hl,hl
        add hl,hl
        add hl,hl
        add hl,hl
        add hl,de
        ret

; -------------------------------------------------------------
; Pantalla
; -------------------------------------------------------------
ROWD    equ 20                  ; filas de los tres numeros
ROWF    equ 40
ROWW    equ 60
COLT    equ 52                  ; columna de la letra

; labels: las letras D, F y W
labels: ld hl,IMG+ROWD*160+COLT
        ld a,10
        call lab1
        ld hl,IMG+ROWF*160+COLT
        ld a,11
        call lab1
        ld hl,IMG+ROWW*160+COLT
        ld a,12
lab1:   ld (pn_dst),hl
        push af
        ld a,1
        ld (pn_col),a
        pop af
        jp putgl

; shownum: D (se calcula de n)
shownum:
        ld hl,(nval)            ; D = DK + 26*n
        ld d,h
        ld e,l
        add hl,hl               ; 2n
        add hl,hl               ; 4n
        add hl,hl               ; 8n
        push hl
        add hl,hl               ; 16n
        pop bc
        add hl,bc               ; 24n
        add hl,de
        add hl,de               ; 26n
        ld de,DK
        add hl,de
        ld (dval),hl
        ld de,IMG+ROWD*160+COLT+16
        ld a,1
        jp putnum

; showfw: F y W
showfw: ld hl,(fval)
        ld de,IMG+ROWF*160+COLT+16
        ld a,1
        call putnum
        ld hl,(wval)
        ld de,IMG+ROWW*160+COLT+16
        ld a,1
        jp putnum

; ruler: rayas verdes a los dos lados, cortas cada 10 filas y largas cada 50
ruler:  ld hl,IMG
        ld c,0                  ; C = fila
        ld e,0                  ; E = fila mod 10
rl1:    ld a,e
        or a
        jr nz,rl4
        ld b,4                  ; raya corta
        ld a,c
        or a
        jr z,rl2
        cp 50
        jr nz,rl3
rl2:    ld b,8                  ; filas 0 y 50: larga
rl3:    push hl
        push bc
rl3a:   ld (hl),2               ; izquierda
        inc hl
        djnz rl3a
        pop bc
        pop hl
        push hl
        push bc
        ld a,160
        sub b
        add a,l
        ld l,a
        jr nc,rl3b
        inc h
rl3b:   ld (hl),2               ; derecha
        inc hl
        djnz rl3b
        pop bc
        pop hl
rl4:    ld a,c                  ; fila 99: raya corta, para ver el final
        cp 99
        jr nz,rl5
        push hl
        ld b,4
rl4a:   ld (hl),2
        inc hl
        djnz rl4a
        pop hl
rl5:    ld de,160               ; (pisa E: se recalcula)
        add hl,de
        inc c
        ld a,c
        call mod10
        ld e,a
        ld a,c
        cp 100
        jr c,rl1
        ret

mod10:  sub 10                  ; A = A mod 10
        jr nc,mod10
        add a,10
        ret

        include "../comun.asm"

; -------------------------------------------------------------
; variables
; -------------------------------------------------------------
nval:   defw 400                ; n (D = 10.516 T al empezar)
dval:   defw 0
fval:   defw 0
wval:   defw 0
wcnt:   defw 0

        end start
