; =============================================================
; SD81TEST.ASM -- Test de hardware del SD81 Booster
;
; v0.6: menu (en el stub BASIC, con submenus) y una entrada fija por
; prueba en la tabla de saltos del principio: prueba N en USR 24576+3*N.
;   USR 24576 -> test de MEMORIA (paginas del mapper y enrutado por bloques)
;   USR 24579 -> test de MC45 (ejecucion de codigo en los bloques 4 y 5)
;   USR 24582 -> test de la extension de MC45 a los bloques 6 y 7
;   USR 24585 -> estres del mapper (OUT $E7 aleatorios + relectura)
;   USR 24588 -> captura de POKE 2045 (Superfast), medida con FRAMES
;   USR 24591 -> interrupciones simuladas (POKE 2038-2040)
;   USR 24594 -> ROMLOCK (con el bloqueo POKE 2045 no hace nada)
;   USR 24597 -> estres del protocolo con el MCU (SETBYTE/GETBYTE)
;   USR 24600 -> informacion de la maquina
;   USR 24603 -> memoria no paginada en los bloques 4-7
; Pensado para ir creciendo con mas pruebas (puertos mapeados en memoria,
; mapper, Superfast...) y para comprobar el interface en maquinas nuevas
; (TS1500, TS1000, clones...).
;
; TEST DE MEMORIA -- que hace:
;   1. Guarda el mapeo actual de los 8 bloques (puerto $E7).
;   2. Detecta si la paginacion es simple (32 paginas, 256K) o completa
;      (64 paginas, 512K, LOAD *FULLPAG).
;   3. Marca como SISTEMA las paginas mapeadas en los bloques 0-3 (ROM,
;      ROM de expansion, BASIC, este programa y su pila): no se tocan.
;   4. Pasada A: rellena TODAS las demas paginas, vistas una a una por el
;      bloque 6 ($C000), con el patron (H AND 1Fh) XOR L XOR pagina, y
;      despues las verifica TODAS. Detecta bits de datos, lineas de
;      direccion dentro de la pagina y paginas que se solapan entre si
;      (escribir en una machaca otra): el patron depende de la pagina.
;   5. Pasada B: lo mismo con el patron invertido (cada bit a 0 y a 1).
;   6. Enrutado: vuelve a leer cada pagina a traves de los bloques 4, 5, 6
;      y 7 (32 muestras por pagina) para comprobar que cada bloque manda
;      sus accesos a la pagina que dice el mapper.
;   7. Restaura el mapeo de los bloques 4-7 y deja el resumen en pantalla.
;
; ES DESTRUCTIVO para todas las paginas que no son de sistema (incluidos
; discos RAM de CP/M, pantallas Superfast guardadas, etc.).
;
; Se ejecuta en SLOW, con la pantalla encendida, para ver el progreso.
; Eso obliga a:
;   - No tocar IX, IY, I ni AF' (los usa la ROM para generar el video) ni
;     deshabilitar las interrupciones.
;   - Modo 48K (el de defecto): el video nativo ejecuta D_FILE+$8000 y en
;     modo 48K la FPGA manda esas busquedas de $C000-$FFFF a los bloques
;     2/3, asi que remapear 6/7 no le afecta. En modo 32K (LOAD *RAM48
;     STOP) el bloque 6 ES el espejo que lee el video: el programa lo
;     detecta (bloque 6 = misma pagina que el 2) y no arranca.
; La pantalla se escribe directamente en D_FILE (ampliado, 33 bytes por
; linea), sin RST 10h: mas rapido y sin riesgo de error de pantalla llena.
;
; Devuelve en BC (valor de USR) el numero de paginas con errores, o 255 si
; no se ha ejecutado (modo 32K).
;
; Ensamblar con pasmo: pasmo sd81test.asm SD81TEST.BIN
; Cargar/usar: ver README.md (incluye el stub BASIC, SD81TEST.B81).
; =============================================================
        org 24576

; Tabla de saltos: un punto de entrada fijo por prueba
        jp mem_test             ; USR 24576
        jp mc45_test            ; USR 24579
        jp mc67_test            ; USR 24582
        jp ms_test              ; USR 24585
        jp pk_test              ; USR 24588
        jp si_test              ; USR 24591
        jp rl_test              ; USR 24594
        jp mu_test              ; USR 24597
        jp info_test            ; USR 24600
        jp up_test              ; USR 24603

MAPPORT equ 0E7h        ; puerto del mapper
WINBLK  equ 6           ; bloque ventana para rellenar/verificar (4-6; el 7
                        ; necesitaria WINEND = 0, que da la vuelta)
WINBASE equ WINBLK*32   ; byte alto de su primera direccion
WINEND  equ WINBASE+32  ; byte alto al salir del bloque
DFILE   equ 400Ch       ; variable de sistema D_FILE
DATAPORT equ 0A7h       ; protocolo con el MCU: datos
CLKPORT  equ 0AFh       ; protocolo con el MCU: reloj (bit 7)
CMD_MC45_ON  equ 13h    ; comandos MCU (ver sdhandler.inc.asm)
CMD_MC45_OFF equ 14h
SET_FAST equ 02E7h      ; ROM: apaga el video si estaba en SLOW
SLOW_FAST equ 0207h     ; ROM: vuelve al modo que pide CDFLAG
POKE_EXT67 equ 2062     ; POKE 2062,170/85: MC45 en los bloques 6/7
PRBUFF  equ 403Ch       ; buffer de impresora (33 bytes libres)
CMD_VER      equ 01h    ; mas comandos MCU (ver sdhandler.inc.asm)
CMD_FPGAVER  equ 1Fh
CMD_GETBYTE  equ 20h
CMD_SETBYTE  equ 21h
CMD_ROMLOCK_ON  equ 44h
CMD_ROMLOCK_OFF equ 45h
POKE_SF    equ 2045     ; 170 = Superfast texto, 85 = video nativo
POKE_INTLO equ 2038     ; direccion de la rutina de interrupcion simulada
POKE_INTHI equ 2039
POKE_INTEN equ 2040     ; 1/0: activa/desactiva las interrupciones simuladas
FRAMES  equ 4034h       ; variable de sistema FRAMES (16436)
MARGIN  equ 4028h
RAMTOP  equ 4004h
ROMVER  equ 2004h       ; byte de version de la ROM del interface
DETBLK  equ 5           ; bloque para detectar la paginacion (siempre libre)

; Codigos de caracter del ZX81
Z_SP    equ 00h
Z_DASH  equ 16h         ; -  pagina pendiente
Z_DOT   equ 1Bh         ; .  pagina correcta
Z_R     equ 37h         ; R  falla solo el enrutado
Z_S     equ 38h         ; S  pagina de sistema
Z_X     equ 3Dh         ; X  falla
Z_0     equ 1Ch         ; '0'; los digitos y 'A'-'F' son consecutivos
Z_Q     equ 0Fh         ; ?
Z_COLON equ 0Eh         ; :
Z_INV   equ 80h         ; bit de video inverso

; Filas de pantalla
ROW_GRID  equ 4         ; 4-11: rejilla de paginas (8 por fila)
ROW_PHASE equ 13        ; fase en curso
ROW_RES   equ 15        ; 15-17: resultado de cada fase
ROW_SUM   equ 18        ; 18-20: resumen y primer error
                        ; (la 21 es para el mensaje del stub BASIC)

; -------------------------------------------------------------
; Test de memoria (USR 24576)
; -------------------------------------------------------------
mem_test:
        call save_map
        ld b,0
        ld c,0
        call set_at
        ld hl,s_title
        call out_str

        ld a,(saved_map+2)      ; modo 32K: el bloque 6 espeja al 2
        ld b,a
        ld a,(saved_map+6)
        cp b
        jr nz,st1
        ld b,1
        ld c,0
        call set_at
        ld hl,s_mode32
        call out_str
        ld bc,255
        ret

st1:    call detect_pages
        call init_status
        xor a
        ld (first_done),a
        call draw_header
        call draw_grid

        ld hl,s_ph_a            ; pasada A: patron directo
        ld (ph_name),hl
        ld e,0
        ld b,ROW_RES
        call run_pass
        ld hl,s_ph_b            ; pasada B: patron invertido
        ld (ph_name),hl
        ld e,0FFh
        ld b,ROW_RES+1
        call run_pass
        call run_routing        ; usa el contenido de la pasada B

        call restore_map
        ld b,ROW_PHASE
        call clear_line
        call draw_summary
        call count_bad          ; BC = paginas con error (valor de USR)
        ret

; -------------------------------------------------------------
; Test de MC45 (USR 24579)
;
; Escribe LD BC,0302h / RET (01 02 03 C9) en varios puntos de los bloques
; 4 y 5 y lo ejecuta con BC=0:
;   - con MC45 activo tiene que devolver 0302h;
;   - con MC45 apagado, 01, 02 y 03 (bit 6 a 0, A15=1) tienen que llegar
;     a la CPU como NOPs forzados y solo se ejecuta el RET (C9, bit 6 a
;     1): tiene que devolver 0000h.
; La primera mitad comprueba que MC45 funciona; la segunda, que la maquina
; fuerza de verdad los NOPs por encima de 32K (lo que MC45 anula).
; Deja MC45 apagado. Machaca esos pocos bytes de los bloques 4 y 5.
; Devuelve en BC el numero de comprobaciones que fallan (0-10).
; -------------------------------------------------------------
MC45_N  equ 5           ; numero de direcciones de prueba

mc45_test:
        ld b,0
        ld c,0
        call set_at
        ld hl,s_mc_title
        call out_str
        ld b,2
        ld c,0
        call set_at
        ld hl,s_mc_code
        call out_str
        ld b,3
        ld c,0
        call set_at
        ld hl,s_mc_on
        call out_str
        ld b,4
        ld c,0
        call set_at
        ld hl,s_mc_off
        call out_str
        ld b,6
        ld c,0
        call set_at
        ld hl,s_mc_head
        call out_str

        ld hl,mc45_addrs        ; copia la rutina en cada direccion
        ld b,MC45_N
mt1:    ld e,(hl)
        inc hl
        ld d,(hl)
        inc hl
        push hl
        push bc
        ld hl,mc45_code
        ld bc,4
        ldir
        pop bc
        pop hl
        djnz mt1

        ld hl,mc45_addrs
        ld (run_tab),hl
        ld a,MC45_N
        ld (run_n),a
        xor a
        ld (mc_fails),a
        ld a,CMD_MC45_ON        ; --- MC45 activo: tiene que dar 0302h ---
        call mcu_cmd
        ld de,0302h
        ld c,6                  ; columna de la tabla
        call mc45_run
        ld a,CMD_MC45_OFF       ; --- MC45 apagado: NOPs, 0000h ---
        call mcu_cmd
        ld de,0000h
        ld c,18
        call mc45_run

        ld b,MC45_N+8           ; resultado
        ld c,0
        call set_at
        ld hl,s_mc_res
        call out_str
        ld a,(mc_fails)
        or a
        jr nz,mt2
        ld hl,s_ok
        call out_str
        jr mt3
mt2:    call out_dec2
        ld hl,s_fail_n
        call out_str
mt3:    ld a,(mc_fails)
        ld c,a
        ld b,0
        ret

; Ejecuta la rutina en cada direccion de la tabla (run_tab), (run_n)
; entradas. DE = valor esperado en BC,
; C = columna donde imprimir el resultado. Imprime la direccion en la
; columna 0 (fila 7 en adelante) y "vvvv OK" / "vvvv BAD".
mc45_run:
        ld hl,(run_tab)
        ld b,7                  ; fila
mr1:    push bc
        ld a,c
        ld (mr_col),a
        ld c,0
        call set_at
        ld a,(hl)               ; direccion de prueba
        inc hl
        push hl
        ld h,(hl)
        ld l,a
        call out_hex16
        ld a,(mr_col)
        ld c,a
        call set_at
        push de
        ld bc,0
        call call_hl            ; ejecuta la rutina de prueba -> BC
        pop de
        ld h,b
        ld l,c
        call out_hex16
        ld a,h                  ; compara con DE
        cp d
        jr nz,mr2
        ld a,l
        cp e
        jr nz,mr2
        ld hl,s_ok_sp
        call out_str
        jr mr3
mr2:    ld hl,s_bad_sp
        call out_str
        ld a,(mc_fails)
        inc a
        ld (mc_fails),a
mr3:    pop hl
        inc hl
        pop bc
        inc b
        ld a,(run_n)
        add a,7
        cp b
        jr nz,mr1
        ret

; -------------------------------------------------------------
; Test de la extension de MC45 a los bloques 6/7 (USR 24582)
;
; Con MC45 y POKE 2062,170 se puede ejecutar codigo en los bloques 6/7
; (paginas propias, no el espejo de 2/3). Prueba:
;   1. MC45 + extension: LD BC,0302h / RET en cuatro direcciones de los
;      bloques 6 y 7 tiene que devolver 0302h.
;   2. MC45 sin la extension (POKE 2062,85): en modo 48K la FPGA manda la
;      busqueda de $C000-$FFFF al mismo desplazamiento de los bloques 2/3,
;      asi que tiene que devolver 0000h. Por eso solo se prueba en
;      direcciones cuyo "espejo" controlamos y contiene un RET (C9):
;        $C03C y $C040 -> $403C/$4040, buffer de impresora (se restaura);
;        mc67_slot+8000h -> este mismo programa (mc67_slot).
;      Saltar a cualquier otra direccion ejecutaria variables de sistema o
;      BASIC y colgaria la maquina.
; El video nativo ejecuta D_FILE+$8000, que con la extension activa ya no
; iria a los bloques 2/3: la prueba se hace en FAST (SET_FAST/SLOW_FAST de
; la ROM, igual que el LOAD del interface) y deja MC45 y la extension
; apagados. Requiere que los bloques 6/7 no sean espejo de 2/3 (modo 48K) y
; ROMLOCK apagado (si no, POKE 2062 no tiene efecto).
; Devuelve en BC el numero de comprobaciones que fallan (0-8), o 255 si
; no se ha ejecutado.
; -------------------------------------------------------------
MC67_N  equ 4

mc67_test:
        ld b,0
        ld c,0
        call set_at
        ld hl,s_m67_title
        call out_str

        ld c,6                  ; bloques 6/7 espejo de 2/3: no se puede
        call read_page
        ld b,a
        ld c,2
        call read_page
        cp b
        jr z,m6x
        ld c,7
        call read_page
        ld b,a
        ld c,3
        call read_page
        cp b
        jr nz,m6ok
m6x:    ld b,2
        ld c,0
        call set_at
        ld hl,s_m67_mirror
        call out_str
        ld bc,255
        ret

m6ok:   ld b,2
        ld c,0
        call set_at
        ld hl,s_m67_code
        call out_str
        ld b,3
        ld c,0
        call set_at
        ld hl,s_m67_on
        call out_str
        ld b,4
        ld c,0
        call set_at
        ld hl,s_m67_off
        call out_str
        ld b,6
        ld c,0
        call set_at
        ld hl,s_m67_head
        call out_str

        call SET_FAST           ; sin video: D_FILE+$8000 no debe ejecutarse

        ld hl,(PRBUFF)          ; guarda los 2 bytes del buffer que se usan
        ld (m67_save1),hl
        ld hl,(PRBUFF+4)
        ld (m67_save2),hl
        ld a,0C9h               ; RET en los "espejos" de $C03C/$C040
        ld (PRBUFF),a
        ld (PRBUFF+4),a

        ld hl,mc67_addrs        ; rutina de prueba en los bloques 6/7
        ld b,MC67_N
m61:    ld e,(hl)
        inc hl
        ld d,(hl)
        inc hl
        push hl
        push bc
        ld hl,mc45_code
        ld bc,4
        ldir
        pop bc
        pop hl
        djnz m61

        ld hl,mc67_addrs
        ld (run_tab),hl
        ld a,MC67_N
        ld (run_n),a
        xor a
        ld (mc_fails),a

        ld a,CMD_MC45_ON        ; --- MC45 + extension: 0302h ---
        call mcu_cmd
        ld a,170
        ld (POKE_EXT67),a
        ld de,0302h
        ld c,6
        call mc45_run
        ld a,85                 ; --- sin extension: espejo, 0000h ---
        ld (POKE_EXT67),a
        ld de,0000h
        ld c,18
        call mc45_run
        ld a,CMD_MC45_OFF
        call mcu_cmd

        ld hl,(m67_save1)       ; restaura el buffer de impresora
        ld (PRBUFF),hl
        ld hl,(m67_save2)
        ld (PRBUFF+4),hl
        call SLOW_FAST          ; vuelve al modo en que estaba

        ld b,MC67_N+8           ; resultado
        ld c,0
        call set_at
        ld hl,s_mc_res
        call out_str
        ld a,(mc_fails)
        or a
        jr nz,m62
        ld hl,s_ok
        call out_str
        jr m63
m62:    call out_dec2
        ld hl,s_fail_n
        call out_str
m63:    ld a,(mc_fails)
        ld c,a
        ld b,0
        ret

; Direcciones de prueba en los bloques 6/7 (ver cabecera: su espejo en los
; bloques 2/3 tiene que ser un RET)
mc67_addrs: dw 0C03Ch, 0C040h, mc67_slot+8000h, mc67_slot+8004h
; "Espejo" en este programa de mc67_slot+8000h y +8004h: con la extension
; apagada la CPU acaba aqui, y ejecuta directamente el RET.
mc67_slot:  db 0C9h,0,0,0, 0C9h,0,0,0

; =============================================================
; Utilidades comunes de las pruebas
; =============================================================

; HL = texto -> fila 0. Conserva BC.
title:
        push bc
        ld b,0
        ld c,0
        call set_at
        call out_str
        pop bc
        ret

; B = fila, HL = texto (en la columna 0). Conserva BC; deja el cursor detras.
; Ojo: etiqueta + numero no pueden pasar de 32 columnas (se pisaria el
; NEWLINE de la linea en D_FILE).
line:
        push bc
        ld c,0
        call set_at
        call out_str
        pop bc
        ret

; B = fila, HL = etiqueta, DE = numero -> "etiqueta" + numero. Conserva BC.
line_num:
        call line
        ex de,hl
        call out_dec16
        ex de,hl
        ret

; B = fila, HL = total de errores -> "RESULT: OK" / "RESULT: FAIL"
line_result:
        push hl
        ld hl,s_mc_res
        call line
        pop hl
        ld a,h
        or l
        ld hl,s_ok
        jr z,lr1
        ld hl,s_fail
lr1:    jp out_str

; HL en decimal, sin ceros a la izquierda. Conserva BC, DE, HL.
out_dec16:
        push bc
        push de
        push hl
        ld e,0                  ; E = 1 cuando ya se ha impreso un digito
        ld bc,-10000
        call od16
        ld bc,-1000
        call od16
        ld bc,-100
        call od16
        ld bc,-10
        call od16
        ld a,l
        add a,Z_0
        call out_char
        pop hl
        pop de
        pop bc
        ret
od16:   ld a,Z_0-1
od16a:  inc a
        add hl,bc
        jr c,od16a
        sbc hl,bc               ; deshace la ultima resta
        cp Z_0
        jr nz,od16b
        bit 0,e
        ret z                   ; cero a la izquierda: no se imprime
od16b:  ld e,1
        jp out_char

; A = version (nibble alto = mayor, bajo = menor) -> "x.y"
out_ver:
        push af
        rrca
        rrca
        rrca
        rrca
        and 0Fh
        add a,Z_0
        call out_char
        ld a,Z_DOT
        call out_char
        pop af
        and 0Fh
        add a,Z_0
        jp out_char

; Suma 1 al contador de 16 bits apuntado por HL (sin desbordar). Pisa A, HL.
inc16:
        push de
        ld e,(hl)
        inc hl
        ld d,(hl)
        inc de
        ld a,d
        or e
        jr z,i16a               ; 65535 + 1: se queda en 65535
        ld (hl),d
        dec hl
        ld (hl),e
i16a:   pop de
        ret

; Aleatorio de 16 bits (xorshift de John Metcalf) -> HL, A = H.
; La semilla vive en el propio LD HL,nn.
xrnd:   ld hl,1
        ld a,h
        rra
        ld a,l
        rra
        xor h
        ld h,a
        ld a,l
        rra
        ld a,h
        rra
        xor l
        ld l,a
        xor h
        ld h,a
        ld (xrnd+1),hl
        ret

; Espera unos 1,5 cuadros (~98000 T-states). Pisa A y BC.
wait_frames:
        ld bc,3800
wf1:    dec bc
        ld a,b
        or c
        jr nz,wf1
        ret

; -------------------------------------------------------------
; Protocolo con el MCU con limite de tiempo. C = reloj al empezar el
; comando (bit 7), igual que en la ROM: "diff" espera a que el reloj sea
; distinto de C, "eq" a que vuelva a ser igual. Carry = timeout (~1 s).
; -------------------------------------------------------------
out_wdiff:
        out (DATAPORT),a
wait_diff:
        push de
        ld de,0
wd1:    in a,(CLKPORT)
        xor c
        jp m,wd2
        dec de
        ld a,d
        or e
        jr nz,wd1
        pop de
        scf
        ret
wd2:    pop de
        or a
        ret

out_weq:
        out (DATAPORT),a
wait_eq:
        push de
        ld de,0
we1:    in a,(CLKPORT)
        xor c
        jp p,we2
        dec de
        ld a,d
        or e
        jr nz,we1
        pop de
        scf
        ret
we2:    pop de
        or a
        ret

; A = comando con respuesta de 1 byte (VER, FPGAVER) -> A. Carry = timeout.
; Pisa BC.
mcu_get1:
        ld b,a
        in a,(CLKPORT)
        ld c,a
        ld a,b
        call out_wdiff
        ret c
        in a,(DATAPORT)
        ld b,a
        call wait_eq
        ld a,b
        ret

; SETBYTE (mu_idx) = (mu_val). Carry = timeout. Pisa BC.
mcu_setbyte:
        in a,(CLKPORT)
        ld c,a
        ld a,CMD_SETBYTE
        call out_wdiff
        ret c
        ld a,(mu_idx)
        call out_weq
        ret c
        ld a,(mu_val)
        jp out_wdiff

; GETBYTE (mu_idx) -> A. Carry = timeout. Pisa BC.
mcu_getbyte:
        in a,(CLKPORT)
        ld c,a
        ld a,CMD_GETBYTE
        call out_wdiff
        ret c
        ld a,(mu_idx)
        call out_weq
        ret c
        in a,(DATAPORT)
        ld b,a
        call wait_diff
        ld a,b
        ret

; -------------------------------------------------------------
; Superfast como sonda: en Superfast (POKE 2045,170) la FPGA decrementa su
; copia de FRAMES en cada VSYNC y es la que se lee en 16436; con video
; nativo se lee la RAM, que en FAST nadie toca.
; A = valor para POKE 2045 -> A = 1 si FRAMES corre (Superfast activo),
; 0 si no se ha movido (video nativo), 2 si da algo raro.
; -------------------------------------------------------------
probe_sf:
        ld (POKE_SF),a
        ld hl,1000
        ld (FRAMES),hl          ; la FPGA tambien captura esta escritura
        call wait_frames
        ld hl,(FRAMES)
        ld de,1000
        or a
        sbc hl,de               ; HL = leido - 1000
        ld a,h
        or l
        ret z                   ; 0: no ha corrido
        ld a,h
        inc a
        jr nz,psx               ; no es un negativo pequeno
        ld a,l
        cp 0FDh                 ; -1..-3 (de 1 a 3 cuadros)
        jr c,psx
        ld a,1
        ret
psx:    ld a,2
        ret

; =============================================================
; Test 3: estres del mapper (USR 24585)
;
; Escribe en cada pagina que no es de sistema una firma (numero de pagina
; y su complemento en los 2 primeros bytes; los originales se guardan y se
; restauran). Despues:
;   - 2048 OUT $E7 sueltos: bloque 4-7 y pagina al azar;
;   - 512 rafagas: los 4 bloques seguidos y luego se comprueban los 4.
; En cada comprobacion se relee el registro del mapper (READBACK) y se lee
; la firma a traves del bloque (ROUTING). En FAST.
; =============================================================
MS_SINGLE equ 2048
MS_BURST  equ 512

ms_test:
        ld hl,s_ms_title
        call title
        ld b,2
        ld hl,s_ms_d1
        call line
        ld b,3
        ld hl,s_ms_d2
        call line
        call SET_FAST
        call save_map
        call detect_pages
        call init_status
        call sig_write
        ld hl,0
        ld (err_a),hl
        ld (err_b),hl

        ld bc,MS_SINGLE         ; --- OUT sueltos ---
ms1:    push bc
        call xrnd
        ld a,l
        and 3
        add a,4
        ld c,a                  ; C = bloque 4-7
        call rnd_page
        ld (mc_page),a
        call map_page
        call check_blk
        pop bc
        dec bc
        ld a,b
        or c
        jr nz,ms1

        ld bc,MS_BURST          ; --- rafagas de 4 bloques ---
ms2:    push bc
        ld c,4
ms3:    call rnd_page
        call burst_slot
        ld (hl),a
        call map_page
        inc c
        ld a,c
        cp 8
        jr nz,ms3
        ld c,4
ms4:    call burst_slot
        ld a,(hl)
        ld (mc_page),a
        call check_blk
        inc c
        ld a,c
        cp 8
        jr nz,ms4
        pop bc
        dec bc
        ld a,b
        or c
        jr nz,ms2

        call sig_restore
        call restore_map
        call SLOW_FAST

        ld b,5
        ld hl,s_ms_n
        call line
        ld b,7
        ld hl,s_ms_rb
        ld de,(err_a)
        call line_num
        ld b,8
        ld hl,s_ms_rt
        ld de,(err_b)
        call line_num
        ld hl,(err_a)
        ld de,(err_b)
        add hl,de
        ld b,10
        call line_result
        ld hl,(err_a)
        ld de,(err_b)
        add hl,de
        ld b,h
        ld c,l
        ret

; C = bloque (4-7) -> HL = &burst_pages[C-4]. Conserva A y BC.
burst_slot:
        push af
        ld a,c
        sub 4
        ld hl,burst_pages
        add a,l
        ld l,a
        jr nc,bs1
        inc h
bs1:    pop af
        ret

; Pagina al azar que no es de sistema -> A. Conserva BC y DE.
rnd_page:
        call xrnd
        ld a,(npages)
        dec a
        and h
        ld (rp_tmp),a
        call is_system
        jr z,rnd_page
        ld a,(rp_tmp)
        ret

; C = bloque, (mc_page) = pagina que deberia tener. Cuenta en err_a si el
; registro no la devuelve y en err_b si la firma leida por el bloque no es
; la de esa pagina. Conserva BC y DE.
check_blk:
        call read_page
        ld hl,mc_page
        cp (hl)
        jr z,ck1
        ld hl,err_a
        call inc16
ck1:    ld a,c
        rrca
        rrca
        rrca
        ld h,a
        ld l,0                  ; HL = base del bloque
        ld a,(mc_page)
        cp (hl)
        jr nz,ck2
        cpl
        inc hl
        cp (hl)
        ret z
ck2:    ld hl,err_b
        jp inc16

; Firma en cada pagina que no es de sistema, vista por el bloque DETBLK.
sig_write:
        ld d,0
sw1:    ld a,d
        call is_system
        jr z,sw2
        call sig_map            ; HL = &sig_save[2*D]
        ld a,(DETBLK*2000h)
        ld (hl),a
        inc hl
        ld a,(DETBLK*2000h+1)
        ld (hl),a
        ld a,d
        ld (DETBLK*2000h),a
        cpl
        ld (DETBLK*2000h+1),a
sw2:    inc d
        ld a,(npages)
        cp d
        jr nz,sw1
        ret

sig_restore:
        ld d,0
sr2a:   ld a,d
        call is_system
        jr z,sr2b
        call sig_map
        ld a,(hl)
        ld (DETBLK*2000h),a
        inc hl
        ld a,(hl)
        ld (DETBLK*2000h+1),a
sr2b:   inc d
        ld a,(npages)
        cp d
        jr nz,sr2a
        ret

; D = pagina: la mapea en DETBLK y devuelve HL = &sig_save[2*D]. Pisa C.
sig_map:
        ld a,d
        ld c,DETBLK
        call map_page
        ld a,d
        add a,a
        ld l,a
        ld h,0
        push de
        ld de,sig_save
        add hl,de
        pop de
        ret

; =============================================================
; Test 4: captura de POKE 2045 (USR 24588)
; 100 veces: POKE 2045,170 tiene que poner en marcha FRAMES (Superfast) y
; POKE 2045,85 tiene que pararlo (video nativo). En FAST.
; =============================================================
PK_N    equ 100

pk_test:
        ld hl,s_pk_title
        call title
        ld b,2
        ld hl,s_pk_d1
        call line
        ld b,3
        ld hl,s_pk_d2
        call line
        call SET_FAST
        ld hl,(FRAMES)
        ld (frames_save),hl
        ld hl,0
        ld (err_a),hl
        ld (err_b),hl
        ld b,PK_N
pk1:    push bc
        ld a,170
        call probe_sf
        cp 1
        jr z,pk2
        ld hl,err_a
        call inc16
pk2:    ld a,85
        call probe_sf
        or a
        jr z,pk3
        ld hl,err_b
        call inc16
pk3:    pop bc
        djnz pk1
        ld a,85
        ld (POKE_SF),a
        ld hl,(frames_save)
        ld (FRAMES),hl
        call SLOW_FAST

        ld b,5
        ld hl,s_pk_on
        ld de,(err_a)
        call line_num
        ld b,6
        ld hl,s_pk_off
        ld de,(err_b)
        call line_num
        jr two_results

; Comun: err_a + err_b -> linea de resultado en la fila 8 y BC
two_results:
        ld hl,(err_a)
        ld de,(err_b)
        add hl,de
        ld b,8
        call line_result
        ld hl,(err_a)
        ld de,(err_b)
        add hl,de
        ld b,h
        ld c,l
        ret

; =============================================================
; Test 5: interrupciones simuladas (USR 24591)
; Con POKE 2040,1 y Superfast, la FPGA sustituye el fetch de $0038 por
; JP (POKE 2038/2039). Se prueba con RST 38h explicito (con DI), 100 veces
; en tres casos: activas + Superfast (tiene que saltar a la rutina),
; desactivadas y sin Superfast (no tiene que saltar). Si no salta, se
; ejecuta la rutina de la ROM en $0038, preparada para volver a
; rst_fallback sin peligro (ver try_rst). En FAST.
; =============================================================
SI_N    equ 100

si_test:
        ld hl,s_si_title
        call title
        ld b,2
        ld hl,s_si_d1
        call line
        ld b,3
        ld hl,s_si_d2
        call line
        call SET_FAST
        di
        ld hl,int_handler
        ld a,l
        ld (POKE_INTLO),a
        ld a,h
        ld (POKE_INTHI),a
        ld hl,0
        ld (err_a),hl
        ld (err_b),hl
        ld (err_c),hl
        ld b,SI_N
si1:    push bc
        ld a,170                ; activas + Superfast: tiene que saltar
        ld (POKE_SF),a
        ld a,1
        ld (POKE_INTEN),a
        call try_rst
        cp 1
        jr z,si2
        ld hl,err_a
        call inc16
si2:    xor a                   ; desactivadas: no
        ld (POKE_INTEN),a
        call try_rst
        or a
        jr z,si3
        ld hl,err_b
        call inc16
si3:    ld a,85                 ; activas sin Superfast: no
        ld (POKE_SF),a
        ld a,1
        ld (POKE_INTEN),a
        call try_rst
        or a
        jr z,si4
        ld hl,err_c
        call inc16
si4:    xor a
        ld (POKE_INTEN),a
        pop bc
        djnz si1
        ld a,85
        ld (POKE_SF),a
        call SLOW_FAST

        ld b,5
        ld hl,s_si_en
        ld de,(err_a)
        call line_num
        ld b,6
        ld hl,s_si_dis
        ld de,(err_b)
        call line_num
        ld b,7
        ld hl,s_si_sf
        ld de,(err_c)
        call line_num
        ld hl,(err_a)
        ld de,(err_b)
        add hl,de
        ld de,(err_c)
        add hl,de
        push hl
        ld b,9
        call line_result
        pop bc
        ret

; RST 38h -> A = 1 si la FPGA lo ha llevado a int_handler, 0 si ha
; ejecutado la ROM. En la ROM, con C=2: DEC C -> JP NZ,$0045 -> POP DE
; (quita la direccion de retorno) -> JR $0041 -> LD R,A -> EI -> JP (HL).
; Con A=40h, R tiene el bit 6 a 1 durante 64 busquedas: /INT (que en el
; ZX81 sale de A6 en el refresco) no se activa antes del DI de
; rst_fallback. Pisa BC y HL.
try_rst:
        xor a
        ld (int_hit),a
        ld c,2
        ld hl,rst_fallback
        ld a,40h
        rst 38h
        jr tr1                  ; viene de int_handler
rst_fallback:
        di                      ; viene de la ROM (pila ya equilibrada)
tr1:    ld a,(int_hit)
        ret

int_handler:
        ld a,1
        ld (int_hit),a
        ret                     ; vuelve detras del RST 38h

; =============================================================
; Test 6: ROMLOCK (USR 24594)
; 10 veces: sin bloqueo POKE 2045,170/85 tiene que funcionar; con
; LOAD *ROMLOCK (comando $44), POKE 2045,170 no tiene que tener efecto.
; Deja ROMLOCK apagado. En FAST.
; =============================================================
RL_N    equ 10

rl_test:
        ld hl,s_rl_title
        call title
        ld b,2
        ld hl,s_rl_d1
        call line
        ld b,3
        ld hl,s_rl_d2
        call line
        call SET_FAST
        ld hl,(FRAMES)
        ld (frames_save),hl
        ld hl,0
        ld (err_a),hl
        ld (err_b),hl
        ld b,RL_N
rl1:    push bc
        ld a,CMD_ROMLOCK_OFF    ; sin bloqueo: on y off
        call mcu_cmd
        ld a,170
        call probe_sf
        cp 1
        jr z,rl2
        ld hl,err_a
        call inc16
rl2:    ld a,85
        call probe_sf
        or a
        jr z,rl3
        ld hl,err_a
        call inc16
rl3:    ld a,CMD_ROMLOCK_ON     ; con bloqueo: POKE 2045,170 sin efecto
        call mcu_cmd
        ld a,170
        call probe_sf
        or a
        jr z,rl4
        ld hl,err_b
        call inc16
rl4:    pop bc
        djnz rl1
        ld a,CMD_ROMLOCK_OFF
        call mcu_cmd
        ld a,85
        ld (POKE_SF),a
        ld hl,(frames_save)
        ld (FRAMES),hl
        call SLOW_FAST

        ld b,5
        ld hl,s_rl_off
        ld de,(err_a)
        call line_num
        ld b,6
        ld hl,s_rl_on
        ld de,(err_b)
        call line_num
        jp two_results

; =============================================================
; Test 7: protocolo con el MCU (USR 24597)
; 2000 veces SETBYTE + GETBYTE en un indice volatil al azar (64-127, que
; no usa nadie) con un valor al azar; se compara lo leido. Cada espera
; tiene limite de tiempo: si el MCU deja de contestar se para y lo dice.
; Devuelve los errores, o 9999 si hubo timeout.
; =============================================================
MU_N    equ 2000

mu_test:
        ld hl,s_mu_title
        call title
        ld b,2
        ld hl,s_mu_d1
        call line
        ld b,3
        ld hl,s_mu_d2
        call line
        call SET_FAST
        ld hl,0
        ld (err_a),hl
        ld bc,MU_N
mu1:    push bc
        call xrnd
        ld a,l
        and 63
        add a,64
        ld (mu_idx),a
        ld a,h
        ld (mu_val),a
        call mcu_setbyte
        jr c,mu_to
        call mcu_getbyte
        jr c,mu_to
        ld hl,mu_val
        cp (hl)
        jr z,mu2
        ld hl,err_a
        call inc16
mu2:    pop bc
        dec bc
        ld a,b
        or c
        jr nz,mu1
        call SLOW_FAST
        ld b,5
        ld hl,s_mu_n
        call line
        ld b,6
        ld hl,s_mu_err
        ld de,(err_a)
        call line_num
        ld hl,(err_a)
        ld b,8
        call line_result
        ld bc,(err_a)
        ret

mu_to:  pop bc                  ; transferencia = MU_N - restantes + 1
        ld hl,MU_N+1
        or a
        sbc hl,bc
        ex de,hl
        call SLOW_FAST
        ld b,5
        ld hl,s_mu_to
        call line_num
        ld hl,1
        ld b,8
        call line_result
        ld bc,9999
        ret

; =============================================================
; Test 8: informacion de la maquina (USR 24600)
; =============================================================
info_test:
        ld hl,s_in_title
        call title
        call SET_FAST

        ld b,2                  ; versiones
        ld hl,s_in_mcu
        call line
        ld a,CMD_VER
        call info_ver
        ld hl,s_in_rom
        call out_str
        ld a,(ROMVER)
        call out_ver
        ld hl,s_in_fpga
        call out_str
        ld a,CMD_FPGAVER
        call info_ver

        ld b,3                  ; puente 50/60 Hz: bit 6 del puerto FE
        ld hl,s_in_fe
        call line
        ld a,0FFh
        in a,(0FEh)
        out (0FFh),a            ; termina el VSYNC que empieza el IN $FE
        ld hl,s_in_60
        bit 6,a
        jr z,in1
        ld hl,s_in_50
in1:    call out_str

        ld b,4
        ld hl,s_in_margin
        ld a,(MARGIN)
        ld e,a
        ld d,0
        call line_num

        call save_map
        call detect_pages
        ld b,5
        ld hl,s_in_paging
        call line
        ld hl,s_in_full
        ld a,(npages)
        cp 64
        jr z,in2
        ld hl,s_in_half
in2:    call out_str

        ld b,6                  ; bloques 6/7: paginas propias o espejo
        ld hl,s_in_67
        call line
        ld a,(saved_map+2)
        ld c,a
        ld a,(saved_map+6)
        cp c
        ld hl,s_in_own
        jr nz,in3
        ld hl,s_in_mirror
in3:    call out_str

        ld b,8                  ; tabla del mapper
        ld hl,s_in_map
        call line
        ld b,9
        ld d,0
        call info_map4
        ld b,10
        call info_map4

        ld b,12
        ld hl,s_in_ramtop
        ld de,(RAMTOP)
        call line_num
        call SLOW_FAST
        ld bc,0
        ret

; A = comando de version -> imprime "x.y" o "??" si el MCU no contesta.
; Conserva BC.
info_ver:
        push bc
        call mcu_get1
        pop bc
        jp nc,out_ver
        ld a,Z_Q
        call out_char
        jp out_char

; B = fila, D = primer bloque -> "b:pp " x4; D queda en el bloque siguiente
info_map4:
        ld c,0
        call set_at
        ld e,4
im1:    ld a,d
        add a,Z_0
        call out_char
        ld a,Z_COLON
        call out_char
        ld hl,saved_map
        ld a,l
        add a,d
        ld l,a
        jr nc,im2
        inc h
im2:    ld a,(hl)
        call out_dec2
        ld a,Z_SP
        call out_char
        inc d
        dec e
        jr nz,im1
        ret

; =============================================================
; Test 9: memoria no paginada en los bloques 4-7 (USR 24603)
;
; Busca algo dentro de la maquina que conteste en $8000-$FFFF a la vez que
; el interface y no dependa de la pagina (p.ej. la RAM interna del TS1500
; o un espejo de la ROM). Para cada bloque 4-7 y 4 desplazamientos, con
; dos paginas libres P y Q:
;   escribe A5 con P mapeada, 5A con Q, y relee con P (1) y con Q (2):
;     1=A5 2=5A -> PAGED (lo sirve el interface, como debe)
;     1=5A 2=5A -> FIXED (algo sin paginar se queda la ultima escritura)
;     otra cosa -> CONFL (dos dispositivos pelean por el bus)
; Ademas, en el primer desplazamiento del bloque:
;   0/F: escribe 00 / FF y relee (forma del choque);
;   1/2: lo leido con P y con Q;
;   L/R: lo que hay en el espejo de RAM baja ($4000/$6000) y de ROM ($0000).
; Machaca esos pocos bytes de las paginas P y Q. En FAST.
; Devuelve el numero de comprobaciones que no son PAGED (0-16).
; =============================================================
up_test:
        ld hl,s_up_title
        call title
        ld b,1
        ld hl,s_up_d1
        call line
        ld b,2
        ld hl,s_up_d2
        call line
        ld b,3
        ld hl,s_up_d3
        call line
        call SET_FAST
        call save_map
        call detect_pages
        call init_status
        ld d,8                  ; P y Q: dos paginas libres desde la 8
        call up_find
        ld (up_p),a
        inc a
        ld d,a
        call up_find
        ld (up_q),a
        ld hl,0
        ld (err_a),hl

        ld c,4                  ; C = bloque
up1:    xor a
        ld (up_cnt),a
        ld (up_cnt+1),a
        ld (up_cnt+2),a
        ld hl,up_offs
        ld b,4                  ; B = desplazamientos que quedan
up2:    push bc
        ld e,(hl)
        inc hl
        ld d,(hl)
        inc hl
        push hl
        ld a,c
        rrca
        rrca
        rrca
        add a,d
        ld h,a
        ld l,e                  ; HL = base del bloque + desplazamiento
        ld a,(up_p)
        call map_page
        ld (hl),0A5h
        ld a,(up_q)
        call map_page
        ld (hl),05Ah
        ld a,(up_p)
        call map_page
        ld a,(hl)
        ld (up_v1),a
        ld a,(up_q)
        call map_page
        ld a,(hl)
        ld (up_v2),a
        ld a,b                  ; primer desplazamiento: se guarda el detalle
        cp 4
        jr nz,up3
        ld a,(up_v1)
        ld (up_d1),a
        ld a,(up_v2)
        ld (up_d2),a
up3:    ld a,(up_v1)            ; clasificacion
        cp 0A5h
        jr nz,up4
        ld a,(up_v2)
        cp 05Ah
        jr nz,up6
        ld hl,up_cnt            ; PAGED
        inc (hl)
        jr up8
up4:    cp 05Ah
        jr nz,up6
        ld a,(up_v2)
        cp 05Ah
        jr nz,up6
        ld hl,up_cnt+1          ; FIXED
        inc (hl)
        jr up7
up6:    ld hl,up_cnt+2          ; CONFL
        inc (hl)
up7:    ld hl,err_a
        call inc16
up8:    pop hl
        pop bc
        djnz up2

        ld a,c                  ; forma del choque y espejos (desplazamiento 0)
        rrca
        rrca
        rrca
        ld h,a
        ld l,0
        ld a,(up_p)
        call map_page
        ld (hl),0
        ld a,(hl)
        ld (up_r0),a
        ld (hl),0FFh
        ld a,(hl)
        ld (up_rf),a
        ld a,h
        and 3Fh
        or 40h
        ld d,a
        ld e,0
        ld a,(de)               ; espejo de RAM baja: $4000 o $6000
        ld (up_lo),a
        ld a,h
        and 1Fh
        ld d,a
        ld a,(de)               ; espejo de ROM: $0000
        ld (up_ro),a

        ld a,c                  ; fila = 5 + 2*(bloque-4)
        sub 4
        add a,a
        add a,5
        ld b,a
        push bc
        ld hl,s_up_blk
        call line
        ld a,c
        add a,Z_0
        call out_char
        ld hl,s_up_pg
        call out_str
        ld a,(up_cnt)
        add a,Z_0
        call out_char
        ld hl,s_up_fx
        call out_str
        ld a,(up_cnt+1)
        add a,Z_0
        call out_char
        ld hl,s_up_cf
        call out_str
        ld a,(up_cnt+2)
        add a,Z_0
        call out_char
        pop bc
        inc b
        push bc
        ld c,0
        call set_at
        ld hl,s_up_0
        call out_str
        ld a,(up_r0)
        call out_hex8
        ld hl,s_up_f
        call out_str
        ld a,(up_rf)
        call out_hex8
        ld hl,s_up_1
        call out_str
        ld a,(up_d1)
        call out_hex8
        ld hl,s_up_2
        call out_str
        ld a,(up_d2)
        call out_hex8
        ld hl,s_up_l
        call out_str
        ld a,(up_lo)
        call out_hex8
        ld hl,s_up_r
        call out_str
        ld a,(up_ro)
        call out_hex8
        pop bc
        inc c
        ld a,c
        cp 8
        jp nz,up1

        call restore_map
        call SLOW_FAST
        ld hl,(err_a)
        ld b,14
        call line_result
        ld bc,(err_a)
        ret

; D = pagina inicial -> A = primera pagina desde D que no es de sistema
up_find:
        ld a,d
        call is_system
        ld a,d
        ret nz
        inc d
        jr up_find

up_offs: dw 0000h, 0555h, 1AAAh, 1FFFh

call_hl:
        jp (hl)

mc45_code:  db 01h,02h,03h,0C9h         ; LD BC,0302h / RET
mc45_addrs: dw 8000h, 9C40h, 9FFCh, 0A000h, 0BFFCh

; A = comando MCU sin parametros ni respuesta (p.ej. MC45 ON/OFF): se
; manda y se espera a que el MCU cambie el reloj, igual que la ROM
; (OutWaitDiff en sdhandler.inc.asm).
mcu_cmd:
        push bc
        ld b,a
        in a,(CLKPORT)
        ld c,a                  ; bit 7 de C = reloj actual
        ld a,b
        out (DATAPORT),a
mc1:    in a,(CLKPORT)
        xor c
        jp p,mc1                ; hasta que el bit 7 cambie
        pop bc
        ret

; -------------------------------------------------------------
; Mapper
; -------------------------------------------------------------

; A = pagina (0-63), C = bloque (0-7). Sirve para los dos modos: en
; paginacion simple el mapper toma la pagina de D7-D3 del dato (0-31), en
; completa de A13-A8 (el registro B en OUT (C),A). Solo pisa A.
map_page:
        push bc
        ld b,a                  ; B = pagina completa (A13-A8)
        and 1Fh
        rlca
        rlca
        rlca
        or c                    ; A = pagina(0-31)<<3 | bloque
        ld c,MAPPORT
        out (c),a
        pop bc
        ret

; C = bloque (0-7) -> A = pagina mapeada en el. Solo pisa A.
read_page:
        push bc
        ld b,c                  ; A10-A8 = bloque a leer
        ld c,MAPPORT
        in a,(c)
        and 3Fh
        pop bc
        ret

save_map:
        ld hl,saved_map
        ld c,0
sm1:    call read_page
        ld (hl),a
        inc hl
        inc c
        ld a,c
        cp 8
        jr nz,sm1
        ret

restore_map:                    ; solo 4-7: los bloques 0-3 no se tocan
        ld c,4
rm1:    ld hl,saved_map
        ld a,l
        add a,c
        ld l,a
        jr nc,rm2
        inc h
rm2:    ld a,(hl)
        call map_page
        inc c
        ld a,c
        cp 8
        jr nz,rm1
        ret

; Pagina 33 en el bloque 5 y se lee de vuelta: en paginacion completa
; vuelve 33; en simple el mapper solo ve D7-D3 del dato (pagina 1).
detect_pages:
        ld c,DETBLK
        ld a,33
        call map_page
        call read_page
        ld b,32
        cp 33
        jr nz,dp1
        ld b,64
dp1:    ld a,b
        ld (npages),a
        ld a,(saved_map+DETBLK)
        jp map_page

; -------------------------------------------------------------
; Tabla de estado por pagina (codigos ZX81 listos para imprimir)
; -------------------------------------------------------------

; A = pagina -> HL = &status[pagina]. Pisa A.
status_ptr:
        ld hl,status
        add a,l
        ld l,a
        ret nc
        inc h
        ret

; A = pagina -> Z si es de sistema (no se prueba). Pisa A.
is_system:
        push hl
        call status_ptr
        ld a,(hl)
        pop hl
        cp Z_S
        ret

init_status:
        ld hl,status
        ld b,64
is1:    ld (hl),Z_DASH
        inc hl
        djnz is1
        ld de,saved_map         ; paginas de los bloques 0-3 = sistema
        ld b,4
is2:    ld a,(de)
        call status_ptr
        ld (hl),Z_S
        inc de
        djnz is2
        ret

; C = numero de paginas marcadas X o R (B = 0)
count_bad:
        push de
        push hl
        ld bc,0
        ld a,(npages)
        ld e,a
        ld hl,status
cb1:    ld a,(hl)
        cp Z_X
        jr z,cb2
        cp Z_R
        jr nz,cb3
cb2:    inc bc
cb3:    inc hl
        dec e
        jr nz,cb1
        pop hl
        pop de
        ret

; -------------------------------------------------------------
; Pasada completa: rellena todas las paginas y luego las verifica todas.
; E = mascara XOR del patron (0 o FFh), B = fila del resultado,
; (ph_name) = nombre de la pasada.
; -------------------------------------------------------------
run_pass:
        ld a,b
        ld (res_row),a
        call count_bad
        ld a,c
        ld (bad_before),a
        ld a,WINBLK
        ld (cur_blk),a

        ld hl,s_write           ; --- escritura ---
        call show_phase
        ld d,0                  ; D = pagina
ps1:    ld a,d
        call is_system
        jr z,ps2
        ld a,Z_INV
        call draw_cell          ; cursor
        ld a,d
        ld c,WINBLK
        call map_page
        call fill
        xor a
        call draw_cell
ps2:    inc d
        ld a,(npages)
        cp d
        jr nz,ps1

        ld hl,s_verify          ; --- verificacion ---
        call show_phase
        ld d,0
ps3:    ld a,d
        call is_system
        jr z,ps4
        ld a,Z_INV
        call draw_cell
        ld a,d
        ld c,WINBLK
        call map_page
        call verify
        ld a,d                  ; pendiente y sin error -> correcta
        call status_ptr
        ld a,(hl)
        cp Z_DASH
        jr nz,ps5
        ld (hl),Z_DOT
ps5:    xor a
        call draw_cell
ps4:    inc d
        ld a,(npages)
        cp d
        jr nz,ps3

        ld hl,(ph_name)         ; --- resultado ---
        jp show_result

; Rellena el bloque ventana con (H AND 1Fh) XOR L XOR D XOR E
fill:
        ld hl,WINBASE*256
fl1:    ld a,h
        and 1Fh
        xor l
        xor d
        xor e
        ld (hl),a
        inc hl
        ld a,h
        cp WINEND
        jr nz,fl1
        ret

verify:
        ld hl,WINBASE*256
vf1:    ld a,h
        and 1Fh
        xor l
        xor d
        xor e
        cp (hl)
        call nz,err_fail
        inc hl
        ld a,h
        cp WINEND
        jr nz,vf1
        ret

; -------------------------------------------------------------
; Enrutado: cada pagina leida a traves de los bloques 4-7 (32 muestras,
; una por cada 256 bytes, con L variando). Espera el patron de la
; pasada B (E = FFh).
; -------------------------------------------------------------
run_routing:
        ld a,ROW_RES+2
        ld (res_row),a
        call count_bad
        ld a,c
        ld (bad_before),a
        ld hl,s_ph_r
        ld (ph_name),hl
        ld e,0FFh
        ld c,4                  ; C = bloque
rt1:    ld a,c
        ld (cur_blk),a
        ld hl,s_routing
        call show_phase         ; "ROUTING BLOCK " + numero
        ld a,c
        add a,Z_0
        call out_char
        ld d,0
rt2:    ld a,d
        call is_system
        jr z,rt3
        ld a,Z_INV
        call draw_cell
        ld a,d
        call map_page
        call check_samples
        xor a
        call draw_cell
rt3:    inc d
        ld a,(npages)
        cp d
        jr nz,rt2
        ld a,c                  ; devuelve el bloque a su pagina
        push hl
        ld hl,saved_map
        add a,l
        ld l,a
        jr nc,rt4
        inc h
rt4:    ld a,(hl)
        pop hl
        call map_page
        inc c
        ld a,c
        cp 8
        jr nz,rt1
        ld hl,(ph_name)
        jp show_result

; C = bloque, D = pagina, E = mascara
check_samples:
        push bc
        ld a,c
        rrca
        rrca
        rrca                    ; A = bloque*32 = byte alto de su base
        ld h,a
        ld l,0
        ld b,32
cs1:    ld a,h
        and 1Fh
        xor l
        xor d
        xor e
        cp (hl)
        call nz,err_route
        inc h
        ld a,l
        add a,37
        ld l,a
        djnz cs1
        pop bc
        ret

; -------------------------------------------------------------
; Registro de errores. Entrada: A = valor esperado, HL = direccion,
; D = pagina, (cur_blk) = bloque. Conserva todos los registros.
; X (fallo en la pasada A/B) siempre gana; R (solo falla el enrutado) se
; pone unicamente sobre una pagina que estaba bien.
; -------------------------------------------------------------
err_fail:
        ld (tmp_exp),a
        push af
        ld a,Z_X
        jr err_rec
err_route:
        ld (tmp_exp),a
        push af
        ld a,Z_R
err_rec:
        push bc
        push hl
        ld c,a                  ; C = marca
        ld a,d
        call status_ptr
        ld a,(hl)
        cp Z_X
        jr z,er2                ; ya marcada con X
        ld b,a
        ld a,c
        cp Z_X
        jr z,er1
        ld a,b
        cp Z_DOT
        jr nz,er2
er1:    ld (hl),c
er2:    pop hl                  ; HL = direccion del fallo
        push hl
        ld a,(first_done)
        or a
        jr nz,er3
        inc a
        ld (first_done),a
        ld (fe_addr),hl
        ld a,(hl)
        ld (fe_got),a
        ld a,(tmp_exp)
        ld (fe_exp),a
        ld a,d
        ld (fe_page),a
        ld a,(cur_blk)
        ld (fe_blk),a
er3:    pop hl
        pop bc
        pop af
        ret

; -------------------------------------------------------------
; Pantalla
; -------------------------------------------------------------

draw_header:
        ld b,1
        ld c,0
        call set_at
        ld hl,s_pages
        call out_str
        ld a,(npages)
        call out_dec2
        ld hl,s_full
        ld a,(npages)
        cp 64
        jr z,dh1
        ld hl,s_half
dh1:    call out_str
        ld b,2
        ld c,0
        call set_at
        ld hl,s_legend
        jp out_str

; Etiquetas de fila y estado inicial de todas las paginas
draw_grid:
        ld d,0
dg1:    ld a,d
        rrca
        rrca
        rrca
        and 1Fh
        add a,ROW_GRID
        ld b,a
        ld c,0
        call set_at
        ld a,d
        call out_dec2
        ld e,8
dg2:    xor a
        call draw_cell
        inc d
        dec e
        jr nz,dg2
        ld a,(npages)
        cp d
        jr nz,dg1
        ret

; D = pagina, A = 0 (normal) o Z_INV (cursor). Conserva BC, DE, HL.
draw_cell:
        push bc
        push de
        push hl
        ld c,a
        ld a,d
        call status_ptr
        ld a,(hl)
        or c
        ld e,a                  ; E = caracter a pintar
        ld a,d
        rrca
        rrca
        rrca
        and 1Fh
        add a,ROW_GRID
        ld b,a
        ld a,d
        and 7
        add a,a
        add a,3
        ld c,a
        call at_addr
        ld (hl),e
        pop hl
        pop de
        pop bc
        ret

; HL = texto de la fase en curso (fila ROW_PHASE); deja el cursor detras
show_phase:
        push bc
        ld b,ROW_PHASE
        call clear_line
        ld b,ROW_PHASE
        ld c,0
        call set_at
        call out_str
        pop bc
        ret

; HL = nombre de la fase; imprime "nombre: OK" o "nombre: NN BAD" en
; (res_row) con las paginas que han empezado a fallar en esta fase.
show_result:
        push hl
        ld a,(res_row)
        ld b,a
        ld c,0
        call set_at
        pop hl
        call out_str
        call count_bad
        ld a,(bad_before)
        ld b,a
        ld a,c
        sub b
        jr nz,sr1
        ld hl,s_ok
        jp out_str
sr1:    call out_dec2
        ld hl,s_bad_n
        jp out_str

draw_summary:
        ld b,ROW_SUM
        ld c,0
        call set_at
        ld hl,s_bad
        call out_str
        call count_bad
        ld a,c
        call out_dec2
        ld a,(first_done)
        or a
        ret z
        ld b,ROW_SUM+1
        ld c,0
        call set_at
        ld hl,s_first
        call out_str
        ld a,(fe_page)
        call out_dec2
        ld hl,s_block
        call out_str
        ld a,(fe_blk)
        add a,Z_0
        call out_char
        ld b,ROW_SUM+2
        ld c,0
        call set_at
        ld hl,s_addr
        call out_str
        ld hl,(fe_addr)
        call out_hex16
        ld hl,s_exp
        call out_str
        ld a,(fe_exp)
        call out_hex8
        ld hl,s_got
        call out_str
        ld a,(fe_got)
        jp out_hex8

; -------------------------------------------------------------
; Escritura directa en D_FILE (ampliado: NEWLINE inicial y 33 bytes por
; linea). Se deja un cursor en (cur_ptr).
; -------------------------------------------------------------

; B = fila, C = columna -> HL = direccion en D_FILE. Pisa A.
at_addr:
        push de
        push bc
        ld hl,(DFILE)
        inc hl                  ; salta el NEWLINE inicial
        ld de,33
        inc b
aa1:    dec b
        jr z,aa2
        add hl,de
        jr aa1
aa2:    ld e,c                  ; D sigue a 0
        add hl,de
        pop bc
        pop de
        ret

; B = fila, C = columna -> cursor. Pisa A.
set_at:
        push hl
        call at_addr
        ld (cur_ptr),hl
        pop hl
        ret

; B = fila -> la llena de espacios. Pisa A.
clear_line:
        push bc
        ld c,0
        call set_at
        ld b,32
cl1:    xor a
        call out_char
        djnz cl1
        pop bc
        ret

; A = codigo ZX81 en el cursor, y avanza. Solo pisa A (y el cursor).
out_char:
        push hl
        ld hl,(cur_ptr)
        ld (hl),a
        inc hl
        ld (cur_ptr),hl
        pop hl
        ret

; HL = cadena ASCII terminada en 0. Pisa A y HL.
out_str:
        ld a,(hl)
        or a
        ret z
        push hl
        call asc2zx
        call out_char
        pop hl
        inc hl
        jr out_str

; A = ASCII -> A = codigo ZX81 (solo mayusculas). Pisa HL.
asc2zx:
        cp 'A'
        jr c,az2
        sub 'A'-26h
        ret
az2:    cp '0'
        jr c,az3
        cp '9'+1
        jr nc,az3
        sub '0'-Z_0
        ret
az3:    ld hl,asctab
az4:    cp (hl)
        inc hl
        jr z,az5
        inc hl
        push af
        ld a,(hl)
        or a
        jr z,az6                ; fin de tabla: espacio
        pop af
        jr az4
az5:    ld a,(hl)
        ret
az6:    pop af
        xor a
        ret

asctab: db ' ',00h, '"',0Bh, '$',0Dh, ':',0Eh, '?',0Fh, '(',10h
        db ')',11h, '>',12h, '<',13h, '=',14h, '+',15h, '-',16h
        db '*',17h, '/',18h, ';',19h, ',',1Ah, '.',1Bh, 0

; A (0-99) en decimal, 2 digitos. Pisa A.
out_dec2:
        push bc
        ld b,0
od1:    sub 10
        jr c,od2
        inc b
        jr od1
od2:    add a,10
        push af
        ld a,b
        add a,Z_0
        call out_char
        pop af
        add a,Z_0
        call out_char
        pop bc
        ret

; A en hexadecimal (el ZX81 tiene '0'-'9' y 'A'-'F' consecutivos)
out_hex8:
        push af
        rrca
        rrca
        rrca
        rrca
        and 0Fh
        add a,Z_0
        call out_char
        pop af
        and 0Fh
        add a,Z_0
        jp out_char

out_hex16:
        ld a,h
        call out_hex8
        ld a,l
        jp out_hex8

; -------------------------------------------------------------
; Textos (32 columnas como maximo)
; -------------------------------------------------------------
s_title:   db "SD81 BOOSTER TEST - MEMORY V0.6",0
s_mode32:  db "32K MODE: USE LOAD *RAM48 FIRST",0
s_pages:   db "PAGES: ",0
s_full:    db " (FULL PAGING)",0
s_half:    db " (HALF: LOAD *FULLPAG)",0
s_legend:  db "S=SYSTEM .=OK X=FAIL R=ROUTING",0
s_write:   db "WRITING ",0
s_verify:  db "VERIFYING ",0
s_routing: db "ROUTING BLOCK ",0
s_ph_a:    db "PASS A (DIRECT): ",0
s_ph_b:    db "PASS B (INVERTED): ",0
s_ph_r:    db "ROUTING (BLOCKS 4-7): ",0
s_ok:      db "OK",0
s_bad_n:   db " BAD",0
s_bad:     db "PAGES WITH ERRORS: ",0
s_first:   db "FIRST: PAGE ",0
s_block:   db " BLOCK ",0
s_addr:    db "ADDR ",0
s_exp:     db " EXP ",0
s_got:     db " GOT ",0
s_mc_title: db "SD81 BOOSTER TEST - MC45 V0.6",0
s_mc_code: db "CODE IN BLOCKS 4-5: LD BC,0302",0
s_mc_on:   db "MC45 ON:  MUST RETURN 0302",0
s_mc_off:  db "MC45 OFF: FORCED NOPS, 0000",0
s_mc_head: db "ADDR  MC45 ON     MC45 OFF",0
s_mc_res:  db "RESULT: ",0
s_fail:    db "FAIL",0
s_ms_title: db "SD81 TEST - MAPPER STRESS V0.6",0
s_ms_d1:   db "RANDOM OUT $E7 TO BLOCKS 4-7",0
s_ms_d2:   db "READBACK + PAGE SIGNATURE",0
s_ms_n:    db "2048 SINGLE, 512 BURSTS OF 4",0
s_ms_rb:   db "READBACK ERRORS: ",0
s_ms_rt:   db "ROUTING ERRORS: ",0
s_pk_title: db "SD81 TEST - POKE 2045 V0.6",0
s_pk_d1:   db "100 X POKE 2045,170 AND 85",0
s_pk_d2:   db "FRAMES MUST RUN ONLY WHEN ON",0
s_pk_on:   db "170 (SUPERFAST ON) FAILS: ",0
s_pk_off:  db "85 (NATIVE) FAILS: ",0
s_si_title: db "SD81 TEST - SIMULATED INT V0.6",0
s_si_d1:   db "RST 38H X100, POKE 2038-2040",0
s_si_d2:   db "MUST JUMP ONLY IF ON+SUPERFAST",0
s_si_en:   db "ON+SUPERFAST (JUMP) FAILS: ",0
s_si_dis:  db "OFF (NO JUMP) FAILS: ",0
s_si_sf:   db "ON,NATIVE (NO JUMP) FAILS: ",0
s_rl_title: db "SD81 TEST - ROMLOCK V0.6",0
s_rl_d1:   db "10 X POKE 2045 WITH/WITHOUT",0
s_rl_d2:   db "LOAD *ROMLOCK",0
s_rl_off:  db "UNLOCKED (MUST WORK) FAILS: ",0
s_rl_on:   db "LOCKED (NO EFFECT) FAILS: ",0
s_mu_title: db "SD81 TEST - MCU PROTOCOL V0.6",0
s_mu_d1:   db "2000 X SETBYTE + GETBYTE",0
s_mu_d2:   db "RANDOM INDEX 64-127 AND VALUE",0
s_mu_n:    db "2000 TRANSFERS DONE",0
s_mu_err:  db "WRONG VALUES: ",0
s_mu_to:   db "MCU TIMEOUT AT TRANSFER ",0
s_in_title: db "SD81 TEST - MACHINE INFO V0.6",0
s_in_mcu:  db "MCU ",0
s_in_rom:  db "  ROM ",0
s_in_fpga: db "  FPGA ",0
s_in_fe:   db "PORT FE BIT 6: ",0
s_in_50:   db "1 (50HZ)",0
s_in_60:   db "0 (60HZ)",0
s_in_margin: db "MARGIN: ",0
s_in_paging: db "PAGING: ",0
s_in_full: db "FULL (64 PAGES)",0
s_in_half: db "HALF (32 PAGES)",0
s_in_67:   db "BLOCKS 6/7: ",0
s_in_own:  db "OWN PAGES (48K)",0
s_in_mirror: db "MIRROR OF 2/3 (32K)",0
s_in_map:  db "MAPPER (BLOCK:PAGE)",0
s_in_ramtop: db "RAMTOP: ",0
s_up_title: db "SD81 TEST - UNPAGED MEMORY V0.6",0
s_up_d1:   db "BLOCKS 4-7, PAGES P/Q, 4 OFFS.",0
s_up_d2:   db "0/F:WRITE 00/FF 1/2:PAGE P/Q",0
s_up_d3:   db "L/R: LOW RAM / ROM ALIAS",0
s_up_blk:  db "BLK",0
s_up_pg:   db " PAGED:",0
s_up_fx:   db " FIXED:",0
s_up_cf:   db " CONFL:",0
s_up_0:    db " 0:",0
s_up_f:    db " F:",0
s_up_1:    db " 1:",0
s_up_2:    db " 2:",0
s_up_l:    db " L:",0
s_up_r:    db " R:",0
s_ok_sp:   db " OK",0
s_bad_sp:  db " BAD",0
s_fail_n:  db " FAILED",0
s_m67_title: db "SD81 TEST - MC45 BLOCKS 6-7 V0.6",0
s_m67_mirror: db "6/7 MIRROR 2/3: LOAD *RAM48",0
s_m67_code: db "CODE IN BLOCKS 6-7: LD BC,0302",0
s_m67_on:  db "MC45+POKE 2062,170: 0302",0
s_m67_off: db "MC45+POKE 2062,85: MIRROR 0000",0
s_m67_head: db "ADDR  EXT ON      EXT OFF",0

; -------------------------------------------------------------
; Variables
; -------------------------------------------------------------
saved_map:  ds 8        ; pagina de cada bloque al empezar
npages:     db 0        ; 32 o 64
cur_blk:    db 0        ; bloque por el que se esta leyendo
first_done: db 0        ; 1 = ya se guardo el primer error
tmp_exp:    db 0
fe_page:    db 0        ; primer error: pagina, bloque, direccion, valores
fe_blk:     db 0
fe_addr:    dw 0
fe_exp:     db 0
fe_got:     db 0
cur_ptr:    dw 0        ; cursor de escritura en D_FILE
ph_name:    dw 0        ; nombre de la pasada en curso
res_row:    db 0        ; fila de su resultado
bad_before: db 0        ; paginas con error al empezar la fase
status:     ds 64       ; estado de cada pagina (codigo ZX81)
mc_fails:   db 0        ; test MC45: comprobaciones fallidas
mr_col:     db 0        ; test MC45: columna de resultados
run_tab:    dw 0        ; test MC45: tabla de direcciones en curso
run_n:      db 0        ; test MC45: numero de direcciones
m67_save1:  dw 0        ; test 6/7: bytes guardados del buffer de
m67_save2:  dw 0        ; impresora
err_a:      dw 0        ; contadores de errores de las pruebas
err_b:      dw 0
err_c:      dw 0
mc_page:    db 0        ; estres del mapper: pagina esperada
rp_tmp:     db 0
burst_pages: ds 4
sig_save:   ds 128      ; 2 bytes originales por pagina
frames_save: dw 0
int_hit:    db 0        ; interrupciones simuladas: 1 = salto a la rutina
mu_idx:     db 0        ; protocolo MCU: indice y valor en curso
mu_val:     db 0
up_p:       db 0        ; memoria no paginada: paginas P y Q
up_q:       db 0
up_cnt:     ds 3        ; PAGED, FIXED, CONFL del bloque en curso
up_v1:      db 0        ; lo leido con P y con Q
up_v2:      db 0
up_d1:      db 0        ; detalle del primer desplazamiento
up_d2:      db 0
up_r0:      db 0
up_rf:      db 0
up_lo:      db 0
up_ro:      db 0
