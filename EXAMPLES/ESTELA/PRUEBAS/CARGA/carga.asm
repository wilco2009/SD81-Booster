; =============================================================
; CARGA.ASM -- fase 0 (a) de ESTELA: un codigo cargado en $4800 con
; CLEAR 18431 desde un .B81 arranca, y los 14 KB de $4800 a $7FFF se
; cargan y se pueden ejecutar.
;
; El binario ocupa exactamente $4800-$7FFF (14.336 bytes):
;   - el programa, al principio;
;   - un patron conocido hasta $7FEF: el byte de la direccion a vale
;     (a mod 256) XOR (a / 256) XOR $A5;
;   - una rutina en $7FF0, en los ultimos 16 bytes.
;
; El programa comprueba el patron byte a byte, llama a la rutina del final
; y mira RAMTOP. Devuelve en BC (USR) la suma de:
;   1 = el patron tiene errores (cuantos: PEEK 18434 + 256*PEEK 18435)
;   2 = la rutina de $7FF0 no ha devuelto lo esperado
;   4 = RAMTOP no es 18432
;   8 = la pila de la maquina no esta por debajo de $4800
; 0 = todo bien.
;
; Ensamblar: pasmo carga.asm carga.bin carga.sym
; Cargar desde BASIC: CARGA.B81
; =============================================================

        org 4800h

RAMTOP  equ 4004h
TOPRTN  equ 7FF0h

start:  jr go
nerr:   defw 0                  ; 18434-18435: errores del patron
go:     ld hl,0
        ld (nerr),hl
        ld e,0                  ; E = resultado
        ld hl,pstart            ; patron: de pstart a $7FEF
pl:     ld a,l
        xor h
        xor 0a5h
        cp (hl)
        jr z,pok
        push hl
        ld hl,(nerr)
        inc hl
        ld (nerr),hl
        pop hl
pok:    inc hl
        ld a,h                  ; hasta $7FF0
        cp TOPRTN/256
        jr nz,pl
        ld a,l
        cp TOPRTN%256
        jr nz,pl
        ld hl,(nerr)
        ld a,h
        or l
        jr z,r1
        set 0,e
r1:     push de
        ld a,12h
        call TOPRTN             ; tiene que volver con A = $12 xor $5A y HL = TOPRTN
        pop de
        cp 12h xor 5ah
        jr nz,r2
        ld bc,TOPRTN
        or a
        sbc hl,bc
        jr z,r3
r2:     set 1,e
r3:     ld hl,(RAMTOP)
        ld bc,4800h
        or a
        sbc hl,bc
        jr z,r4
        set 2,e
r4:     ld hl,0
        add hl,sp
        ld bc,4800h
        or a
        sbc hl,bc
        jr c,r5
        set 3,e
r5:     ld c,e
        ld b,0
        ret

pstart:
        rept TOPRTN-pstart
        defb ($ & 0ffh) xor ($ / 256) xor 0a5h
        endm

; --- en $7FF0: los ultimos 16 bytes ---
toprtn: xor 5ah                 ; A = A xor $5A
        call trhere             ; HL = direccion de vuelta (TOPRTN+5)
trhere: pop hl
        ld bc,-(trhere-toprtn)
        add hl,bc               ; HL = TOPRTN
        ret
        defs 8000h-$            ; relleno hasta $7FFF

        end start
