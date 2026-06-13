; dual_ay_test.asm — Test de los dos chips AY del SD81 Booster
;
; Chip 0 (Verilog chip 1, A3=1): latch=0xCF, data=0x0F  → canal A a 440 Hz
; Chip 1 (Verilog chip 0, A3=0): latch=0xC6, data=0x06  → canal A a 880 Hz
;
; Clock AY = 1.7734 MHz
; Periodo = 1773400 / (16 * f)
;   440 Hz → 252 = 0xFC
;   880 Hz → 126 = 0x7E
;
; Ensamblar: nasm -f bin dual_ay_test.asm -o dual_ay_test.bin
; Cargar en ZX81: LOAD "dual_ay_test" CODE 32768
; Ejecutar:       RAND USR 32768
;
; O desde SDBOOST.ROM BASIC: LOAD "dual_ay_test.bin" CODE
;                             RAND USR 32768

    org 32768           ; 0x8000 — RAM del mapper SD81

; ─────────────────────────────────────────────────────────────
; Macro: escribir registro AY
; ─────────────────────────────────────────────────────────────
%macro AY_WRITE 3           ; %1=latch_port, %2=data_port, %3=reg, siguiente byte=val
    ld  a, %3
    out (%1), a
    ld  a, %4
    out (%2), a
%endmacro

; ─────────────────────────────────────────────────────────────
; Chip 0 (A3=1): 440 Hz en canal A, salida activa
; ─────────────────────────────────────────────────────────────
start:
    ; Reg 0: tono fino canal A = 0xFC (252)
    ld  a, 0
    out (0CFh), a
    ld  a, 0FCh
    out (00Fh), a

    ; Reg 1: tono grueso canal A = 0
    ld  a, 1
    out (0CFh), a
    xor a
    out (00Fh), a

    ; Reg 7: mixer — canal A tono activado, ruido desactivado
    ;   bit 0=0 → tono A activo
    ;   bits 3-5=1 → ruido desactivado
    ld  a, 7
    out (0CFh), a
    ld  a, 0F8h         ; 1111 1000
    out (00Fh), a

    ; Reg 8: volumen canal A = máximo (0x0F)
    ld  a, 8
    out (0CFh), a
    ld  a, 0Fh
    out (00Fh), a

; ─────────────────────────────────────────────────────────────
; Chip 1 (A3=0): 880 Hz en canal A, salida activa
; ─────────────────────────────────────────────────────────────

    ; Reg 0: tono fino canal A = 0x7E (126)
    ld  a, 0
    out (0C6h), a
    ld  a, 07Eh
    out (006h), a

    ; Reg 1: tono grueso canal A = 0
    ld  a, 1
    out (0C6h), a
    xor a
    out (006h), a

    ; Reg 7: canal A tono activado
    ld  a, 7
    out (0C6h), a
    ld  a, 0F8h
    out (006h), a

    ; Reg 8: volumen canal A = máximo
    ld  a, 8
    out (0C6h), a
    ld  a, 0Fh
    out (006h), a

; ─────────────────────────────────────────────────────────────
; Bucle infinito — los chips siguen sonando autónomamente
; ─────────────────────────────────────────────────────────────
loop:
    halt                ; espera siguiente interrupción
    jr  loop

; ─────────────────────────────────────────────────────────────
; Para silenciar: volumen = 0 en ambos chips
; (no se llama aquí, referencia para otros programas)
; ─────────────────────────────────────────────────────────────
silence:
    ld  a, 8
    out (0CFh), a
    xor a
    out (00Fh), a

    ld  a, 8
    out (0C6h), a
    xor a
    out (006h), a
    ret
