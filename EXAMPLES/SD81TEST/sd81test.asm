; =============================================================
; SD81TEST.ASM -- Test de hardware del SD81 Booster
;
; v0.4: menu (en el stub BASIC) con tres pruebas, cada una con su punto de
; entrada en la tabla de saltos del principio:
;   USR 24576 -> test de MEMORIA (paginas del mapper y enrutado por bloques)
;   USR 24579 -> test de MC45 (ejecucion de codigo en los bloques 4 y 5)
;   USR 24582 -> test de la extension de MC45 a los bloques 6 y 7
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

; Codigos de caracter del ZX81
Z_SP    equ 00h
Z_DASH  equ 16h         ; -  pagina pendiente
Z_DOT   equ 1Bh         ; .  pagina correcta
Z_R     equ 37h         ; R  falla solo el enrutado
Z_S     equ 38h         ; S  pagina de sistema
Z_X     equ 3Dh         ; X  falla
Z_0     equ 1Ch         ; '0'; los digitos y 'A'-'F' son consecutivos
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

; Pagina 33 en el bloque ventana y se lee de vuelta: en paginacion completa
; vuelve 33; en simple el mapper solo ve D7-D3 del dato (pagina 1).
detect_pages:
        ld c,WINBLK
        ld a,33
        call map_page
        call read_page
        ld b,32
        cp 33
        jr nz,dp1
        ld b,64
dp1:    ld a,b
        ld (npages),a
        ld a,(saved_map+WINBLK)
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
s_title:   db "SD81 BOOSTER TEST - MEMORY V0.4",0
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
s_mc_title: db "SD81 BOOSTER TEST - MC45 V0.4",0
s_mc_code: db "CODE IN BLOCKS 4-5: LD BC,0302",0
s_mc_on:   db "MC45 ON:  MUST RETURN 0302",0
s_mc_off:  db "MC45 OFF: FORCED NOPS, 0000",0
s_mc_head: db "ADDR  MC45 ON     MC45 OFF",0
s_mc_res:  db "RESULT: ",0
s_ok_sp:   db " OK",0
s_bad_sp:  db " BAD",0
s_fail_n:  db " FAILED",0
s_m67_title: db "SD81 TEST - MC45 BLOCKS 6-7 V0.4",0
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
