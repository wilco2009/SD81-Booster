; =============================================================
; SD81MEM.ASM -- SD81TEST, modulo MEM: memoria, mapper, ejecucion y puertos mapeados en memoria
;
; Se carga en MOD_ORG ($6000) con
;   LOAD FAST "SD81MEM.BIN" CODE 24576
; (lo hace el stub BASIC) y se ejecuta a traves de la tabla de saltos
; del nucleo (SD81TEST.BIN), cuyas rutinas usa por su .sym. Ensamblar
; despues del nucleo:
;   pasmo sd81test.asm SD81TEST.BIN sd81test.sym
;   pasmo sd81mem.asm SD81MEM.BIN
; mbss_end (en el .sym de este modulo, si se pide) tiene que quedar
; por debajo de $8000.
; =============================================================
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
; =============================================================
        include "sd81test.sym"

        org MOD_ORG
        db MOD_MEM              ; cabecera: id del modulo
        dw bss_end              ; y fin del nucleo con el que se ensamblo
; Tabla de saltos del modulo (el nucleo entra por MOD_ORG+3+3*n)
        jp mem_test            ; 0: USR 22528
        jp mc45_test           ; 1: USR 22531
        jp mc67_test           ; 2: USR 22534
        jp ms_test             ; 3: USR 22537
        jp pk_test             ; 4: USR 22540
        jp rl_test             ; 5: USR 22546
        jp up_test             ; 6: USR 22555
        jp rg_test             ; 7: USR 22558
        jp b0_test             ; 8: USR 22561
        jp sp_test             ; 9: USR 22579
        jp si_test             ; 10: USR 22543

s_title equ s_up_title+8    ; comparte texto
s_bad_sp equ s_bad_n    ; comparte texto

; -------------------------------------------------------------
; Test de memoria (USR 22528)
; -------------------------------------------------------------
mem_test:
        call save_map
        ld hl,s_title
        call title

        ld a,(saved_map+2)      ; modo 32K: el bloque 6 espeja al 2
        ld b,a
        ld a,(saved_map+6)
        cp b
        jr nz,st1
        ld bc,1*256
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
; Test de MC45 (USR 22531)
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
        ld hl,s_mc_title
        call title
        ld bc,2*256
        call set_at
        ld hl,s_mc_code
        call out_str
        ld bc,3*256
        call set_at
        ld hl,s_mc_on
        call out_str
        ld bc,4*256
        call set_at
        ld hl,s_mc_off
        call out_str
        ld bc,6*256
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
; Test de la extension de MC45 a los bloques 6/7 (USR 22534)
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
        ld hl,s_m67_title
        call title

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
m6x:    ld bc,2*256
        call set_at
        ld hl,s_m67_mirror
        call out_str
        ld bc,255
        ret

m6ok:   ld bc,2*256
        call set_at
        ld hl,s_m67_code
        call out_str
        ld bc,3*256
        call set_at
        ld hl,s_m67_on
        call out_str
        ld bc,4*256
        call set_at
        ld hl,s_m67_off
        call out_str
        ld bc,6*256
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

; Espera unos 1,5 cuadros (~98000 T-states). Pisa A y BC.
wait_frames:
        ld bc,3800
wf1:    dec bc
        ld a,b
        or c
        jr nz,wf1
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
; Test 3: estres del mapper (USR 22537)
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
        call plines
        db 2
        dw s_ms_d1
        db 3
        dw s_ms_d2
        db 0FFh
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

        call pline
        db 5
        dw s_ms_n
        call plnum
        db 7
        dw s_ms_rb,err_a
        call plnum
        db 8
        dw s_ms_rt,err_b
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
; Test 4: captura de POKE 2045 (USR 22540)
; 100 veces: POKE 2045,170 tiene que poner en marcha FRAMES (Superfast) y
; POKE 2045,85 tiene que pararlo (video nativo). En FAST.
; =============================================================
PK_N    equ 100

pk_test:
        ld hl,s_pk_title
        call title
        call plines
        db 2
        dw s_pk_d1
        db 3
        dw s_pk_d2
        db 0FFh
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

        call plnum
        db 5
        dw s_pk_on,err_a
        call plnum
        db 6
        dw s_pk_off,err_b
        jp two_results

; =============================================================
; Test 6: ROMLOCK (USR 22546)
; 10 veces: sin bloqueo POKE 2045,170/85 tiene que funcionar; con
; LOAD *ROMLOCK (comando $44), POKE 2045,170 no tiene que tener efecto.
; Deja ROMLOCK apagado. En FAST.
; =============================================================
RL_N    equ 10

rl_test:
        ld hl,s_rl_title
        call title
        call plines
        db 2
        dw s_rl_d1
        db 3
        dw s_rl_d2
        db 0FFh
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

        call plnum
        db 5
        dw s_rl_off,err_a
        call plnum
        db 6
        dw s_rl_on,err_b
        jp two_results

; =============================================================
; Test 5: interrupciones simuladas (USR 22543)
;
; Las pruebas de EXAMPLES/SIMINT en una (ver sim_int.v). La FPGA no tiene
; contadores de prueba: todo se comprueba con lo que se ve desde el Z80.
;   1. Inyeccion: una carga de trabajo determinista con todas las familias
;      de prefijos, sin y con interrupciones (la rutina solo cuenta). Mismo
;      checksum (un RST que partiera una instruccion lo cambiaria), alguna
;      interrupcion y tantas como tramas (+-1).
;   2. HALT: 50 HALT y 50 DD HALT tienen que esperar una trama cada uno y
;      volver detras (100 vueltas, 100 interrupciones, 100 tramas +-1); y
;      20000 vueltas de CB 76, ED 76 y DD CB d 76, que no son HALT: pocas
;      tramas y tantas interrupciones como tramas.
; En FAST, con DI y en Superfast texto (las interrupciones solo van en
; Superfast): la pantalla se ve en directo mientras corre, y la rutina de
; interrupcion hace girar un caracter al final de la fila 19. Unos 4 s.
; Devuelve las comprobaciones que fallan (0-8), o 9999 si la FPGA no
; tiene las interrupciones simuladas (firma 51h, o 52h con el depurador).
; =============================================================
SI_DBG   equ 3FEFh      ; puerto de la FPGA (firma)
SI_PASS  equ 50         ; vueltas de 256 de la carga de trabajo
SI_NHALT equ 100        ; HALT de la prueba 2 (par)
SI_NPFX  equ 20000      ; vueltas de CB/ED/DD CB 76

si_test:
        ld hl,s_si_title
        call title
        ld hl,0
        ld (err_a),hl
        call SET_FAST
        di
        push ix                 ; las pruebas cambian IX
        ld hl,(FRAMES)
        ld (frames_save),hl
        ld bc,SI_DBG            ; hay interrupciones simuladas?
        ld a,15
        out (c),a
        in a,(c)
        cp 51h
        jr z,si_go
        cp 52h
        jr z,si_go
        call pline
        db 2
        dw s_si_none
        ld hl,9999
        ld (err_a),hl
        jp si_exit
si_go:  call pline
        db 19
        dw s_si_run
        ld bc,19*256+31         ; el caracter que gira la rutina
        call at_addr
        ld (si_tick),hl
        ld a,170                ; Superfast texto: se ve D_FILE en directo
        ld (POKE_SF),a
        ld hl,si_isr
        ld (2038),hl
        xor a
        ld (2040),a

        call si_work            ; --- 1. inyeccion ---
        ld hl,(si_chk)          ; sin interrupciones: referencia
        ld (si_ref),hl
        call si_begin
        call si_work
        call si_end
        ld hl,(si_chk)          ; mismo checksum
        ld de,(si_ref)
        call si_eq
        ld de,(si_ri)           ; tramas = interrupciones +-1
        ld hl,(si_rfr)
        call si_near
        ld hl,(si_ri)           ; y alguna ha habido
        ld a,h
        or l
        call z,si_bad
        call plnum
        db 2
        dw s_si_c0,si_ref
        call plnum
        db 3
        dw s_si_c1,si_chk
        call plnum
        db 4
        dw s_si_ii,si_ri
        call plnum
        db 5
        dw s_si_fr,si_rfr

        call si_begin           ; --- 2. HALT y DD HALT ---
        ld de,0
        ld b,SI_NHALT/2
si_h1:  halt
        inc de                  ; una vez por HALT
        defb 0DDh,076h          ; DD HALT
        inc de
        djnz si_h1
        ld (si_v1),de
        call si_end
        ld de,SI_NHALT
        ld hl,(si_v1)
        call si_eq
        ld hl,(si_ri)
        call si_eq
        ld hl,(si_rfr)
        call si_near
        call plnum
        db 7
        dw s_si_hr,si_v1
        call plnum
        db 8
        dw s_si_hi,si_ri
        call plnum
        db 9
        dw s_si_hf,si_rfr

        call si_begin           ; --- CB 76, ED 76 y DD CB d 76 ---
        ld hl,si_wbuf
        ld ix,si_wbuf
        ld bc,SI_NPFX
si_p1:  bit 6,(hl)              ; CB 76
        defb 0EDh,076h          ; ED 76 (IM 1)
        bit 6,(ix+0)            ; DD CB 00 76
        dec bc
        ld a,b
        or c
        jr nz,si_p1
        call si_end
        ld de,(si_ri)           ; tramas = interrupciones +-1
        ld hl,(si_rfr)
        call si_near
        ld hl,(si_rfr)          ; pocas tramas: ninguno ha esperado
        ld de,60
        or a
        sbc hl,de
        call nc,si_bad
        call pline
        db 11
        dw s_si_76
        call plnum
        db 12
        dw s_si_ii,si_ri
        call plnum
        db 13
        dw s_si_fr,si_rfr
        ld hl,(si_tick)
        ld (hl),Z_SP

si_exit:
        xor a
        ld (2040),a             ; interrupciones fuera
        ld a,85
        ld (POKE_SF),a          ; video nativo
        pop ix
        ld hl,(frames_save)
        ld (FRAMES),hl
        call SLOW_FAST
        ld hl,(err_a)
        ld b,19
        call line_result
        ld bc,(err_a)
        ret

; HL = valor, DE = esperado: si no son iguales, un fallo mas. Conserva DE.
si_eq:  or a
        sbc hl,de
        ret z
si_bad: push hl
        ld hl,err_a
        call inc16
        pop hl
        ret

; HL = valor, DE = esperado: fallo si no esta entre DE-1 y DE+1. Conserva DE.
si_near:
        or a
        sbc hl,de
        inc hl                  ; -1..+1 -> 0..2
        ld a,h
        or a
        jr nz,si_bad
        ld a,l
        cp 3
        ret c
        jr si_bad

; Empieza una medida: interrupciones a 0, FRAMES de partida, y las
; interrupciones activas
si_begin:
        ld hl,0
        ld (si_ints),hl
        ld hl,(FRAMES)
        ld (si_frm0),hl
        ld a,1
        ld (2040),a
        ret

; Acaba una medida: las desactiva y deja las de la rutina (si_ri) y las
; tramas que han pasado (si_rfr)
si_end:
        xor a
        ld (2040),a
        ld hl,(FRAMES)          ; FRAMES cuenta hacia abajo (15 bits)
        ex de,hl
        ld hl,(si_frm0)
        or a
        sbc hl,de
        ld a,h
        and 7Fh
        ld h,a
        ld (si_rfr),hl
        ld hl,(si_ints)
        ld (si_ri),hl
        ret

; La rutina de interrupcion: cuenta, gira el caracter y vuelve con un RET
; normal (el epilogo que sirve la FPGA corrige la vuelta)
si_isr: push af
        push hl
        ld hl,(si_ints)
        inc hl
        ld (si_ints),hl
        ld hl,(si_tick)
        inc (hl)
        pop hl
        pop af
        ret

; Carga de trabajo: todo lo que hace entra en el checksum (si_chk), asi
; que un byte perdido o una instruccion partida lo cambiarian
si_work:
        ld hl,1234h
        ld (si_chk),hl
        ld hl,si_src            ; estado inicial de los buffers
        ld de,si_wbuf
        ld bc,8
        ldir
        ld ix,si_wbuf
        ld c,SI_PASS
siw1:   ld b,0
siw2:   ld a,(si_chk)
        rlc a                           ; CB 07
        xor b
        neg                             ; ED 44
        add a,(ix+1)                    ; DD 86 01
        rlc (ix+2)                      ; DD CB 02 06
        ld (ix+1),a                     ; DD 77 01
        srl a                           ; CB 3F
        bit 3,(ix+3)                    ; DD CB 03 5E
        jr z,siw3
        inc a
siw3:   defb 0DDh,0DDh,086h,003h        ; DD DD ADD A,(IX+3)
        defb 0DDh,0EDh,044h             ; DD NEG
        push bc
        ld hl,si_wbuf                   ; LDIR de 4 bytes
        ld de,si_wbuf+4
        ld bc,4
        ldir
        pop bc
        ld hl,(si_chk)
        ld e,a
        ld d,0
        add hl,de
        add hl,hl
        jr nc,siw4
        inc hl
siw4:   ld (si_chk),hl
        djnz siw2
        dec c
        jr nz,siw1
        ret

si_src: db 11h,22h,33h,44h,55h,66h,77h,88h

; =============================================================
; Test 9: memoria no paginada en los bloques 4-7 (USR 22555)
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
        call plines
        db 1
        dw s_up_d1
        db 2
        dw s_up_d2
        db 3
        dw s_up_d3
        db 0FFh
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

; =============================================================
; Test 10: registros del mapper (USR 22558)
; Cada pagina en cada bloque 4-7, con el formato del modo activo: en
; paginacion simple la pagina va en D7-D3 del dato (y B, que no debe
; usarse, lleva basura); en completa va en B (y D7-D3 llevan basura).
; Tras cada escritura se releen los 4 bloques: el escrito tiene que tener
; su pagina y los otros tres, la suya de antes. En FAST.
; =============================================================
rg_test:
        ld hl,s_rg_title
        call title
        call plines
        db 2
        dw s_rg_d1
        db 3
        dw s_rg_d2
        db 0FFh
        call SET_FAST
        call save_map
        call detect_pages
        ld hl,saved_map+4       ; paginas esperadas de los bloques 4-7
        ld de,rg_exp
        ld bc,4
        ldir
        ld hl,0
        ld (err_a),hl
        ld (err_b),hl           ; err_b = numero de escrituras

        ld c,4                  ; C = bloque
rg1:    ld d,0                  ; D = pagina
rg2:    push bc
        push de
        call xrnd               ; basura para el campo que no toca
        ld a,(npages)
        cp 64
        jr z,rg3
        ld b,h                  ; simple: pagina en D7-D3, basura en B
        ld a,d
        rlca
        rlca
        rlca
        or c
        jr rg4
rg3:    ld b,d                  ; completa: pagina en B, basura en D7-D3
        ld a,l
        and 0F8h
        or c
rg4:    ld e,c
        ld c,MAPPORT
        out (c),a
        ld c,e
        ld a,c                  ; rg_exp[bloque-4] = pagina
        sub 4
        ld hl,rg_exp
        add a,l
        ld l,a
        jr nc,rg5
        inc h
rg5:    ld (hl),d
        ld hl,err_b
        call inc16
        call rg_check
        pop de
        pop bc
        inc d
        ld a,(npages)
        cp d
        jr nz,rg2
        inc c
        ld a,c
        cp 8
        jr nz,rg1

        call restore_map
        call SLOW_FAST
        call plnum
        db 5
        dw s_rg_w,err_b
        call plnum
        db 6
        dw s_rg_e,err_a
        ld hl,(err_a)
        ld b,8
        call line_result
        ld bc,(err_a)
        ret

; Relee los bloques 4-7 y cuenta en err_a los que no coinciden con rg_exp
rg_check:
        ld c,4
        ld hl,rg_exp
rc1:    call read_page
        cp (hl)
        jr z,rc2
        push hl
        ld hl,err_a
        call inc16
        pop hl
rc2:    inc hl
        inc c
        ld a,c
        cp 8
        jr nz,rc1
        ret

; =============================================================
; Test 11: proteccion del bloque 0 (USR 22561)
; El bloque 0 es de solo lectura: escribir el complemento en direcciones
; de la ROM que no son puertos de configuracion (2038-2130 se evitan) no
; tiene que cambiar nada. Si alguna cambia se restaura en el acto. En FAST
; y con las interrupciones deshabilitadas.
; =============================================================
B0_N    equ 10

b0_test:
        ld hl,s_b0_title
        call title
        call plines
        db 2
        dw s_b0_d1
        db 3
        dw s_b0_d2
        db 0FFh
        call SET_FAST
        di
        ld hl,0
        ld (err_a),hl
        ld hl,b0_addrs
        ld b,B0_N
b01:    ld e,(hl)
        inc hl
        ld d,(hl)
        inc hl
        push hl
        ex de,hl                ; HL = direccion a probar
        ld a,(hl)
        ld e,a                  ; E = original
        cpl
        ld (hl),a
        ld a,(hl)
        ld (hl),e               ; restaura por si se habia escrito
        cp e
        jr z,b02
        ld hl,err_a
        call inc16
b02:    pop hl
        djnz b01
        call SLOW_FAST
        call pline
        db 5
        dw s_b0_n
        call plnum
        db 6
        dw s_b0_w,err_a
        ld hl,(err_a)
        ld b,8
        call line_result
        ld bc,(err_a)
        ret

b0_addrs: dw 0000h, 0001h, 0100h, 0555h, 07F0h, 0900h, 0AAAh, 1000h, 1555h, 1FFFh

; =============================================================
; Test 17: paginas de sistema, no destructivo (USR 22579)
; Las paginas de los bloques 0-3 (ROM, ROM de expansion, BASIC, este
; programa) vistas por el bloque 5: cada byte se lee, se escribe su
; complemento, se relee y se restaura. En FAST y con DI. La pagina donde
; esta el propio bucle de prueba (la 3, este programa) se prueba con una
; copia del bucle en el buffer de impresora, para no modificar nunca los
; bytes que se estan ejecutando.
; =============================================================
sp_test:
        ld hl,s_sp_title
        call title
        call pline
        db 2
        dw s_sp_d1
        call SET_FAST
        di
        call save_map
        ld hl,PRBUFF            ; guarda el buffer de impresora
        ld de,sp_save
        ld bc,SPL_LEN
        ldir
        ld hl,sp_loop           ; copia del bucle en el buffer
        ld de,PRBUFF
        ld bc,SPL_LEN
        ldir
        ld hl,0
        ld (err_a),hl
        ld c,0                  ; C = bloque 0-3 (su pagina)
spt1:   push bc
        ld hl,saved_map
        ld a,l
        add a,c
        ld l,a
        jr nc,spt2
        inc h
spt2:   ld a,(hl)
        ld (sp_page),a
        ld c,DETBLK
        call map_page
        ld hl,DETBLK*2000h
        ld bc,0                 ; BC = errores de esta pagina
        ld a,(saved_map+SPL_BLK) ; la pagina donde esta sp_loop?
        ld e,a
        ld a,(sp_page)
        cp e
        jr z,spt3
        call sp_loop            ; bucle en su sitio
        jr spt4
spt3:   call PRBUFF             ; bucle copiado en el buffer
spt4:   ld (sp_err),bc
        pop bc
        push bc
        ld a,c                  ; fila 4 + bloque
        add a,4
        ld b,a
        ld hl,s_sp_page
        call line
        ld a,(sp_page)
        call out_dec2
        ld hl,s_sp_errs
        ld de,(sp_err)
        call line_num_c
        ld hl,(err_a)
        ld de,(sp_err)
        add hl,de
        ld (err_a),hl
        pop bc
        inc c
        ld a,c
        cp 4
        jp nz,spt1

        ld a,(saved_map+DETBLK) ; restaura el bloque 5 y el buffer
        ld c,DETBLK
        call map_page
        ld hl,sp_save
        ld de,PRBUFF
        ld bc,SPL_LEN
        ldir
        call SLOW_FAST
        ld hl,(err_a)
        ld b,9
        call line_result
        ld bc,(err_a)
        ret

; HL = etiqueta, DE = numero -> los imprime en la posicion actual
line_num_c:
        call out_str
        ex de,hl
        call out_dec16
        ex de,hl
        ret

; Bucle de prueba (independiente de la posicion: solo saltos relativos).
; HL = inicio del bloque ventana, BC = contador de errores.
sp_loop:
        ld a,(hl)
        ld e,a
        cpl
        ld (hl),a
        cp (hl)                 ; Z si se escribio bien
        ld (hl),e               ; restaura
        jr z,spl1
        inc bc
spl1:   inc hl
        ld a,h
        cp DETBLK*32+32
        jr nz,sp_loop
        ret
SPL_LEN equ $-sp_loop
SPL_BLK equ sp_loop/2000h      ; bloque donde queda sp_loop (tiene que ser
                               ; el 3: la copia va en el buffer de
                               ; impresora, que esta en el bloque 2)

call_hl:
        jp (hl)

mc45_code:  db 01h,02h,03h,0C9h         ; LD BC,0302h / RET
mc45_addrs: dw 8000h, 9C40h, 9FFCh, 0A000h, 0BFFCh

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
        ld bc,3FEFh             ; con el monitor del depurador cargado (firma
        ld a,15                 ; 52h y armado), la pagina 63 es suya: la
        out (c),a               ; FPGA no deja escribirla
        in a,(c)
        cp 52h
        ret nz
        xor a
        out (c),a
        in a,(c)
        rla                     ; bit 7: armado
        ret nc
        ld a,63
        call status_ptr
        ld (hl),Z_S
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

draw_header: ld bc,1*256
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
        ld bc,2*256
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
        ld bc,ROW_PHASE*256
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

draw_summary: ld bc,ROW_SUM*256
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
s_mode32:  db "32K MODE: USE LOAD *RAM48 FIRS",'T'+80h
s_pages:   db "PAGES:",' '+80h
s_full:    db " (FULL PAGING",')'+80h
s_half:    db " (HALF: LOAD *FULLPAG",')'+80h
s_legend:  db "S=SYSTEM .=OK X=FAIL R=ROUTIN",'G'+80h
s_write:   db "WRITING",' '+80h
s_verify:  db "VERIFYING",' '+80h
s_routing: db "ROUTING BLOCK",' '+80h
s_ph_a:    db "PASS A (DIRECT):",' '+80h
s_ph_b:    db "PASS B (INVERTED):",' '+80h
s_ph_r:    db "ROUTING (BLOCKS 4-7):",' '+80h
s_bad_n:   db " BA",'D'+80h
s_bad:     db "PAGES WITH ERRORS:",' '+80h
s_first:   db "FIRST: PAGE",' '+80h
s_block equ s_routing+7    ; comparte texto
s_addr:    db "ADDR",' '+80h
s_exp:     db " EXP",' '+80h
s_got:     db " GOT",' '+80h
s_mc_title: db "MC4",'5'+80h
s_mc_code: db "CODE IN BLOCKS 4-5: LD BC,030",'2'+80h
s_mc_on:   db "MC45 ON:  MUST RETURN 030",'2'+80h
s_mc_off:  db "MC45 OFF: FORCED NOPS, 000",'0'+80h
s_mc_head: db "ADDR  MC45 ON     MC45 OF",'F'+80h
s_ms_title: db "MAPPER STRES",'S'+80h
s_ms_d1:   db "RANDOM OUT $E7 TO BLOCKS 4-",'7'+80h
s_ms_d2:   db "READBACK + PAGE SIGNATUR",'E'+80h
s_ms_n:    db "2048 SINGLE, 512 BURSTS OF ",'4'+80h
s_ms_rb:   db "READBACK ERRORS:",' '+80h
s_ms_rt:   db "ROUTING ERRORS:",' '+80h
s_pk_title: db "POKE 204",'5'+80h
s_pk_d1:   db "100 X POKE 2045,170 AND 8",'5'+80h
s_pk_d2:   db "FRAMES MUST RUN ONLY WHEN O",'N'+80h
s_pk_on:   db "170 (SUPERFAST ON) FAILS:",' '+80h
s_pk_off:  db "85 (NATIVE) FAILS:",' '+80h
s_si_title: db "SIM. INTERRUPT",'S'+80h
s_si_none: db "FPGA WITHOUT SIMULATED INT",'S'+80h
s_si_run:  db "RUNNING..",'.'+80h
s_si_c0:   db "CHECKSUM WITHOUT INTS:",' '+80h
s_si_c1:   db "CHECKSUM WITH INTS:",' '+80h
s_si_ii:   db "INTERRUPTIONS:",' '+80h
s_si_fr:   db "FRAMES:",' '+80h
s_si_hr:   db "HALT RETURNS (100):",' '+80h
s_si_hi:   db "HALT INTERRUPTIONS (100):",' '+80h
s_si_hf:   db "HALT FRAMES (100):",' '+80h
s_si_76:   db "CB 76, ED 76, DD CB D 76 (20000",')'+80h
s_rl_title equ s_rl_d2+6    ; comparte texto
s_rl_d1:   db "10 X POKE 2045 WITH/WITHOU",'T'+80h
s_rl_d2:   db "LOAD *ROMLOC",'K'+80h
s_rl_off:  db "UNLOCKED (MUST WORK) FAILS:",' '+80h
s_rl_on:   db "LOCKED (NO EFFECT) FAILS:",' '+80h
s_up_title: db "UNPAGED MEMOR",'Y'+80h
s_up_d1:   db "BLOCKS 4-7, PAGES P/Q, 4 OFFS",'.'+80h
s_up_d2:   db "0/F:WRITE 00/FF 1/2:PAGE P/",'Q'+80h
s_up_d3:   db "L/R: LOW RAM / ROM ALIA",'S'+80h
s_up_blk:  db "BL",'K'+80h
s_up_pg:   db " PAGED",':'+80h
s_up_fx:   db " FIXED",':'+80h
s_up_cf:   db " CONFL",':'+80h
s_up_0:    db " 0",':'+80h
s_up_f:    db " F",':'+80h
s_up_1:    db " 1",':'+80h
s_up_2:    db " 2",':'+80h
s_up_l:    db " L",':'+80h
s_up_r:    db " R",':'+80h
s_rg_title: db "MAPPER REG",'S'+80h
s_rg_d1:   db "EVERY PAGE IN BLOCKS 4-",'7'+80h
s_rg_d2:   db "OTHER FIELD RANDOM, READ ALL ",'4'+80h
s_rg_w:    db "WRITES:",' '+80h
s_rg_e equ s_ms_rb    ; comparte texto
s_b0_title: db "BLOCK 0 PROTEC",'T'+80h
s_b0_d1:   db "WRITES TO THE ROM (NOT PORTS",')'+80h
s_b0_d2:   db "MUST BE IGNORE",'D'+80h
s_b0_n:    db "10 ADDRESSES TESTE",'D'+80h
s_b0_w:    db "WRITABLE:",' '+80h
s_sp_title: db "SYSTEM PAGE",'S'+80h
s_sp_d1:   db "BLOCKS 0-3, NON DESTRUCTIV",'E'+80h
s_sp_page equ s_first+7    ; comparte texto
s_sp_errs equ s_bad+10    ; comparte texto
s_fail_n:  db " FAILE",'D'+80h
s_m67_title: db "MC45 BLOCKS 6-",'7'+80h
s_m67_code: db "CODE IN BLOCKS 6-7: LD BC,030",'2'+80h
s_m67_on:  db "MC45+POKE 2062,170: 030",'2'+80h
s_m67_off: db "MC45+POKE 2062,85: MIRROR 000",'0'+80h
s_m67_head: db "ADDR  EXT ON      EXT OF",'F'+80h
cur_blk:    db 0        ; bloque por el que se esta leyendo
tmp_exp:    db 0
fe_page:    db 0        ; primer error: pagina, bloque, direccion, valores
fe_blk:     db 0
fe_exp:     db 0
fe_got:     db 0
ph_name:    dw 0        ; nombre de la pasada en curso
res_row:    db 0        ; fila de su resultado
bad_before: db 0        ; paginas con error al empezar la fase
mc_fails:   db 0        ; test MC45: comprobaciones fallidas
mr_col:     db 0        ; test MC45: columna de resultados
run_tab:    dw 0        ; test MC45: tabla de direcciones en curso
run_n:      db 0        ; test MC45: numero de direcciones
m67_save1:  dw 0        ; test 6/7: bytes guardados del buffer de
m67_save2:  dw 0        ; impresora
mc_page:    db 0        ; estres del mapper: pagina esperada
rp_tmp:     db 0
frames_save: dw 0
up_p:       db 0        ; memoria no paginada: paginas P y Q
up_q:       db 0
up_v1:      db 0        ; lo leido con P y con Q
up_v2:      db 0
up_d1:      db 0        ; detalle del primer desplazamiento
up_d2:      db 0
up_r0:      db 0
up_rf:      db 0
up_lo:      db 0
up_ro:      db 0
sp_page:    db 0        ; paginas de sistema: pagina en curso
sp_err:     dw 0        ; errores de esa pagina
si_tick:    dw 0        ; interrupciones simuladas: el caracter que gira
si_ints:    dw 0        ; las que cuenta la rutina
si_frm0:    dw 0        ; FRAMES al empezar la medida
si_ri:      dw 0        ; resultado de la medida: interrupciones y tramas
si_rfr:     dw 0
si_chk:     dw 0        ; checksum de la carga de trabajo
si_ref:     dw 0        ; el de la pasada sin interrupciones
si_v1:      dw 0        ; vueltas detras del HALT

; -------------------------------------------------------------
; Buffers del modulo sin valor inicial (no van en el .bin).
; mbss_end tiene que quedar por debajo de $8000.
; -------------------------------------------------------------
mbss:
status       equ mbss   ; estado de cada pagina (codigo ZX81)
burst_pages  equ status+64
sig_save     equ burst_pages+4   ; 2 bytes originales por pagina
up_cnt       equ sig_save+128   ; PAGED, FIXED, CONFL del bloque en curso
rg_exp       equ up_cnt+3   ; registros del mapper: paginas esperadas 4-7
sp_save      equ rg_exp+4   ; copia del buffer de impresora
si_wbuf      equ sp_save+33   ; interrupciones simuladas: buffer de la carga
mbss_end     equ si_wbuf+8
