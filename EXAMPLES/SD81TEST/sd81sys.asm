; =============================================================
; SD81SYS.ASM -- SD81TEST, modulo SYS: MCU, RTC, SD, frecuencia de cuadro e informacion
;
; Se carga en MOD_ORG ($6000) con
;   LOAD FAST "SD81SYS.BIN" CODE 24576
; (lo hace el stub BASIC) y se ejecuta a traves de la tabla de saltos
; del nucleo (SD81TEST.BIN), cuyas rutinas usa por su .sym. Ensamblar
; despues del nucleo:
;   pasmo sd81test.asm SD81TEST.BIN sd81test.sym
;   pasmo sd81sys.asm SD81SYS.BIN
; mbss_end (en el .sym de este modulo, si se pide) tiene que quedar
; por debajo de $8000.
; =============================================================
        include "sd81test.sym"

        org MOD_ORG
        db MOD_SYS              ; cabecera: id del modulo
        dw bss_end              ; y fin del nucleo con el que se ensamblo
; Tabla de saltos del modulo (el nucleo entra por MOD_ORG+3+3*n)
        jp mu_test             ; 0: USR 22549
        jp info_test           ; 1: USR 22552
        jp fr_test             ; 2: USR 22564
        jp rt_test             ; 3: USR 22567
        jp sr_test             ; 4: USR 22573
        jp sw_test             ; 5: USR 22576
        jp ck_test             ; 6: USR 22636
        jp rp_test             ; 7: USR 22642

; B = fila, HL = etiqueta, DE = numero -> "etiqueta" + numero. Conserva BC.
line_num:
        call line
        ex de,hl
        call out_dec16
        ex de,hl
        ret

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

out_weq:
        out (DATAPORT),a
wait_eq:
        push de
        ld de,0
we1:    call in_clk
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

; =============================================================
; Test 7: protocolo con el MCU (USR 22549)
; 2000 veces SETBYTE + GETBYTE en un indice volatil al azar (64-127, que
; no usa nadie) con un valor al azar; se compara lo leido. Cada espera
; tiene limite de tiempo: si el MCU deja de contestar se para y lo dice.
; Devuelve los errores, o 9999 si hubo timeout.
; =============================================================
MU_N    equ 2000

mu_test:
        ld hl,s_mu_title
        call title
        call plines
        db 2
        dw s_mu_d1
        db 3
        dw s_mu_d2
        db 0FFh
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
        call pline
        db 5
        dw s_mu_n
        call plnum
        db 6
        dw s_mu_err,err_a
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
        ld b,5
        ld hl,s_mu_to
        call line_num
        ld hl,1
        ld b,8
        call line_result
        ld bc,9999
        ret

; =============================================================
; Test 8: informacion de la maquina (USR 22552)
; =============================================================
info_test:
        ld hl,s_in_title
        call title

        call pline
        db 2
        dw s_in_mcu  ; versiones
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

        call pline
        db 3
        dw s_in_fe  ; puente 50/60 Hz: bit 6 del puerto FE
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
        call pline
        db 5
        dw s_in_paging
        ld hl,s_in_full
        ld a,(npages)
        cp 64
        jr z,in2
        ld hl,s_in_half
in2:    call out_str

        call pline
        db 6
        dw s_in_67  ; bloques 6/7: paginas propias o espejo
        ld a,(saved_map+2)
        ld c,a
        ld a,(saved_map+6)
        cp c
        ld hl,s_in_own
        jr nz,in3
        ld hl,s_in_mirror
in3:    call out_str

        call pline
        db 8
        dw s_in_map  ; tabla del mapper
        ld b,9
        ld d,0
        call info_map4
        ld b,10
        call info_map4

        call plnum
        db 12
        dw s_in_ramtop,RAMTOP
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
; Test 12: frecuencia de cuadro (USR 22564)
; Cuenta VSYNC durante un segundo del RTC del MCU. El puerto $AF da en
; D6-D1 los VSYNC desde la lectura anterior y se pone a 0 al leerlo, asi
; que se acumulan TODAS sus lecturas (in_clk), incluidas las del propio
; protocolo con el MCU. Video nativo (necesita SLOW: sin video no hay
; VSYNC) y Superfast (VSYNC de la FPGA).
; =============================================================
fr_test:
        ld hl,s_fr_title
        call title
        call pline
        db 2
        dw s_fr_d1
        ld hl,0
        ld (err_a),hl

        call pline
        db 4
        dw s_fr_nat  ; --- video nativo ---
        ld a,(CDFLAG)
        bit 7,a                 ; video encendido (SLOW)?
        jr nz,fr1
        ld hl,s_fr_slow
        call out_str
        jr fr3
fr1:    call measure_fps
        jr c,fr_to
        ld (fr_n),hl
        ex de,hl
        call out_dec16x
        ld hl,s_fr_fps
        call out_str
        ld a,(MARGIN)           ; 31 = 60 Hz, 55 = 50 Hz
        ld e,50
        cp 31
        jr nz,fr2
        ld e,60
fr2:    ld hl,(fr_n)
        call fr_check

fr3:    ld a,170                ; --- Superfast ---
        ld (POKE_SF),a
        call measure_fps
        push af
        ld a,85
        ld (POKE_SF),a
        pop af
        jr c,fr_to
        ld (fr_n),hl
        ld b,5
        push hl
        ld hl,s_fr_sf
        call line
        pop de
        call out_dec16x
        ld hl,s_fr_fps
        call out_str
        ld e,50
        ld hl,(fr_n)
        call fr_check

        ld hl,(err_a)
        ld b,7
        call line_result
        ld bc,(err_a)
        ret

fr_to:  call pline
        db 6
        dw s_rtc_to
        ld hl,1
        ld b,7
        call line_result
        ld bc,9999
        ret

; DE = numero -> lo imprime (out_dec16 con el valor en DE)
out_dec16x:
        ex de,hl
        call out_dec16
        ex de,hl
        ret

; HL = cuadros medidos, E = esperados -> si |HL-E| > 2, err_a+1
fr_check:
        ld a,h
        or a
        jr nz,fc2
        ld a,l
        sub e
        jr nc,fc1
        neg
fc1:    cp 3
        ret c
fc2:    ld hl,err_a
        jp inc16

; Sincroniza con un cambio de segundo del RTC y cuenta los VSYNC hasta el
; siguiente -> HL. Carry = el RTC no contesta o no avanza.
measure_fps:
        call wait_sec
        ret c
        ld hl,0
        ld (vs_total),hl
        call wait_sec
        ld hl,(vs_total)
        ret

; Espera a que cambien los segundos del RTC (hasta ~250 lecturas).
; Carry = timeout.
wait_sec:
        call rtc_read
        ret c
        call rtc_sec
        ld (sec0),a
        ld b,250
wsc1:   push bc
        call rtc_read
        pop bc
        ret c
        call rtc_sec
        ld hl,sec0
        cp (hl)
        ret nz                  ; NC: ha cambiado
        djnz wsc1
        scf
        ret

; Segundos (0-59) de rtc_buf ("AAAA-MM-DD HH:MM:SS.CC", codigos ZX81)
rtc_sec:
        ld a,(rtc_buf+17)
        sub Z_0
        ld b,a
        add a,a
        add a,a
        add a,b
        add a,a                 ; decenas*10
        ld b,a
        ld a,(rtc_buf+18)
        sub Z_0
        add a,b
        ret

; LOAD *RTC: comando $32 con cadena vacia -> 22 caracteres ZX81 en
; rtc_buf + estado. Carry = timeout.
rtc_read:
        call in_clk
        ld c,a
        ld a,CMD_RTC
        call out_wdiff
        ret c
        xor a
        call out_weq
        ret c
        ld hl,rtc_buf
        ld b,22
rrd1:   in a,(DATAPORT)
        ld (hl),a
        inc hl
        call wait_diff
        ret c
        ld a,c                  ; C = reloj actual
        xor 80h
        ld c,a
        djnz rrd1
        in a,(DATAPORT)         ; estado
        jp wait_diff

; LOAD *BAT: comando $34 -> 5 caracteres ZX81 ("V.mmm") en bat_buf +
; estado. Carry = timeout.
bat_read:
        call in_clk
        ld c,a
        ld a,CMD_BAT
        call out_wdiff
        ret c
        ld a,c
        xor 80h
        ld c,a
        ld hl,bat_buf
        ld b,5
brd1:   in a,(DATAPORT)
        ld (hl),a
        inc hl
        call wait_diff
        ret c
        ld a,c
        xor 80h
        ld c,a
        djnz brd1
        in a,(DATAPORT)
        jp wait_diff

; =============================================================
; Test 36: reloj de la CPU (USR 22636)
; En FAST (sin NMI ni video, la CPU a toda velocidad): se sincroniza con
; un cambio de segundo del RTC, ejecuta un bucle de CK_N*3276 =
; 16.248.960 T (5,0 s a 3,25 MHz) y vuelve a leer el RTC. Con las
; centesimas del RTC (el del STM32 las da de verdad), MHz = T /
; transcurrido; resolucion 1/100 s en 5 s, un 0,2%. La latencia de las
; dos lecturas del RTC es la misma y se cancela. Tiene que salir 3,25
; MHz +-1,5%. Sin centesimas (el RTC siempre da .00) no se puede medir.
; =============================================================
CK_N    equ 4960                ; vueltas de 3276 T
; MHz*100 = (CK_N*3276/100) / transcurrido = 162490 / cs; 162490 =
; 2*65536 + 31418 (se escribe en dos partes: pasmo trabaja con 16 bits)
CK_NUMH equ 2
CK_NUML equ 31418

ck_test:
        ld hl,s_ck_title
        call title
        call plines
        db 2
        dw s_ck_d1
        db 3
        dw s_ck_d2
        db 0FFh
        ld hl,0
        ld (err_a),hl
        call SET_FAST
        call wait_sec           ; rtc_buf = justo despues del cambio
        jp c,ck_to
        call ck_stamp
        ld (ck_t0),hl
        ld de,CK_N              ; --- bucle: 3276 T por vuelta ---
ckl1:   ld b,250                ; (7)
ckl2:   djnz ckl2               ; (249*13 + 8)
        dec de                  ; (6)
        ld a,d                  ; (4)
        or e                    ; (4)
        jp nz,ckl1              ; (10)
        call rtc_read
        jp c,ck_to
        call ck_stamp
        ld (ck_t1),hl
        or a                    ; centesimas a 0: puede ser casualidad o
        jr nz,ck1               ; un RTC sin centesimas; 1/4 s despues se
        ld de,250               ; vuelve a mirar
ckl3:   ld b,250
ckl4:   djnz ckl4
        dec de
        ld a,d
        or e
        jp nz,ckl3
        call rtc_read
        jp c,ck_to
        call ck_stamp
        or a
        jp z,ck_nocc
ck1:    call SLOW_FAST
        ld hl,(ck_t1)           ; transcurrido = t1 - t0 (1/100 s)
        ld de,(ck_t0)
        or a
        sbc hl,de
        jr nc,ck2
        ld de,6000              ; cambio de minuto
        add hl,de
ck2:    ld (ck_el),hl
        call plnum
        db 5
        dw s_ck_el,ck_el
        ld de,(ck_el)
        ld a,d
        or e
        jp z,ck_nocc2
        ld a,CK_NUMH            ; A:HL = 162490
        ld hl,CK_NUML
        ld bc,0                 ; BC = CK_NUM / transcurrido (MHz*100)
ckd1:   or a
        sbc hl,de
        sbc a,0
        jr c,ckd2
        inc bc
        jr ckd1
ckd2:   ld (ck_mhz),bc
        ld h,b                  ; parte entera y centesimas
        ld l,c
        ld e,0
        ld bc,100
ckp1:   or a
        sbc hl,bc
        jr c,ckp2
        inc e
        jr ckp1
ckp2:   add hl,bc               ; L = centesimas, E = MHz
        push hl
        push de
        call pline
        db 6
        dw s_ck_mhz
        pop de
        ld a,e
        cp 10
        ld a,Z_Q
        jr nc,ckp3
        ld a,e
        add a,Z_0
ckp3:   call out_char
        ld a,Z_DOT
        call out_char
        pop hl
        ld a,l
        call out_dec2
        ld hl,s_ck_unit
        call out_str
        ld hl,(ck_mhz)          ; 3,20-3,30 MHz
        ld de,320
        or a
        sbc hl,de
        jr c,ck_bad
        ld de,11
        or a
        sbc hl,de
        jr c,ck_ok
ck_bad: ld hl,err_a
        call inc16
ck_ok:  ld hl,(err_a)
        ld b,8
        call line_result
        ld bc,(err_a)
        ret

ck_nocc:
        call SLOW_FAST
ck_nocc2:
        call pline
        db 5
        dw s_ck_nocc
        ld bc,9999
        ret

ck_to:  call SLOW_FAST
        call pline
        db 5
        dw s_rtc_to
        ld bc,9999
        ret

; rtc_buf -> HL = segundos*100 + centesimas, A = centesimas
ck_stamp:
        call rtc_sec
        ld hl,0
        or a
        jr z,cks2
        ld b,a
        ld de,100
cks1:   add hl,de
        djnz cks1
cks2:   ld a,(rtc_buf+20)
        sub Z_0
        ld b,a
        add a,a
        add a,a
        add a,b
        add a,a                 ; decenas*10
        ld b,a
        ld a,(rtc_buf+21)
        sub Z_0
        add a,b
        ld e,a
        ld d,0
        add hl,de
        ret

; =============================================================
; Test 38: resumen (USR 22642)
; El nucleo apunta en rep_res el resultado de cada prueba que se ejecuta
; (FFFFh = no ejecutada; USR 22639 lo borra). Esto lo muestra para las
; pruebas automaticas de rp_list (OK si 0, FAIL y el valor si no) y lo
; guarda en SD81TEST.TXT (en la carpeta actual; ASCII con CR LF), con la
; fecha del RTC y las versiones. El texto se monta en RP_BUF (bloque 4)
; en codigos ZX81 con las mismas rutinas de pantalla (cur_ptr apunta al
; buffer; 76h = fin de linea) y se pasa a ASCII al mandarlo.
; Devuelve el numero de pruebas con FAIL.
; =============================================================
RP_BUF  equ 8000h

rp_test:
        ld hl,s_rp_title
        call title
        xor a                   ; --- pantalla: filas 2-19 ---
        ld (rp_txt),a
        ld a,1
        ld (rp_row),a
        call rp_body

        ld a,1                  ; --- texto ---
        ld (rp_txt),a
        ld hl,RP_BUF
        ld (cur_ptr),hl
        ld hl,s_rp_head
        call out_str
        call rp_nl
        call rtc_read
        jr c,rpt2
        ld hl,s_rp_date
        call out_str
        ld hl,rtc_buf           ; "AAAA-MM-DD HH:MM:SS" (sin centesimas)
        ld b,19
rpt1:   ld a,(hl)
        call out_char
        inc hl
        djnz rpt1
        call rp_nl
rpt2:   ld hl,s_in_mcu
        call out_str
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
        call rp_nl
        call rp_body            ; empieza con una linea en blanco
        call rp_nl
        ld hl,(cur_ptr)         ; bytes del buffer y del fichero (cada
        ld de,RP_BUF            ; 76h se convierte en CR LF)
        or a
        sbc hl,de
        ld (rp_n),hl
        ld b,h
        ld c,l
rpt3:   ld a,(de)
        cp 76h
        jr nz,rpt4
        inc hl
rpt4:   inc de
        dec bc
        ld a,b
        or c
        jr nz,rpt3
        ld (rp_len),hl

        ld a,10                 ; --- SAVE ---
        call m_send
        jp c,rp_to
        ld hl,s_rp_file
        ld e,1
        call send_name
        jp c,rp_to
        ld a,(rp_len)
        call m_send
        jp c,rp_to
        ld a,(rp_len+1)
        call m_send
        jp c,rp_to
        ld hl,RP_BUF
        ld bc,(rp_n)
rps1:   ld a,(hl)
        cp 76h
        jr nz,rps2
        ld a,0Dh
        call m_send
        jp c,rp_to
        ld a,0Ah
        jr rps3
rps2:   call zx2asc
rps3:   call m_send
        jp c,rp_to
        inc hl
        dec bc
        ld a,b
        or c
        jr nz,rps1
        call m_recv             ; estado del SAVE
        jp c,rp_to
        or a
        jr nz,rp_err
        call pline
        db 20
        dw s_rp_saved
        jr rp_end
rp_err: push af
        call pline
        db 20
        dw s_rp_err
        pop af
        call rp_num
        jr rp_end
rp_to:  call pline
        db 20
        dw s_sd_to
rp_end: ld a,(rp_bad)
        ld c,a
        ld b,0
        ret

; Una linea por prueba de rp_list y la de totales. Cuenta OK, FAIL y no
; ejecutadas. Cada linea empieza con rp_nl.
rp_body:
        xor a
        ld (rp_ok),a
        ld (rp_bad),a
        ld (rp_nr),a
        ld hl,rp_list
rb1:    ld a,(hl)
        cp 0FFh
        jr z,rb2
        push hl
        call rp_nl
        pop hl
        ld a,(hl)               ; "NN NOMBRE RESULTADO"
        inc hl
        push af
        call out_dec2
        xor a
        call out_char
        call out_str            ; nombre (17 columnas); HL -> la siguiente
        xor a
        call out_char
        pop af
        push hl
        call rp_result
        pop hl
        jr rb1
rb2:    call rp_nl
        ld hl,s_rp_ok
        call out_str
        ld a,(rp_ok)
        call rp_num
        ld hl,s_rp_fail
        call out_str
        ld a,(rp_bad)
        call rp_num
        ld hl,s_rp_nr
        call out_str
        ld a,(rp_nr)
rp_num: ld l,a
        ld h,0
        jp out_dec16

; A = N -> "OK", "FAIL n" o "NOT RUN" segun rep_res[N]
rp_result:
        ld l,a
        ld h,0
        add hl,hl
        ld de,rep_res
        add hl,de
        ld e,(hl)
        inc hl
        ld d,(hl)
        ld a,d
        and e
        inc a
        jr nz,rr1
        ld hl,rp_nr             ; FFFFh: no se ha ejecutado
        inc (hl)
        ld hl,s_rp_notrun
        jp out_str
rr1:    ld a,d
        or e
        jr nz,rr2
        ld hl,rp_ok
        inc (hl)
        ld hl,s_ok
        jp out_str
rr2:    ld hl,rp_bad
        inc (hl)
        ld hl,s_rp_failn
        call out_str
        ex de,hl
        jp out_dec16

; Fin de linea: en pantalla, a la fila siguiente; en el texto, 76h
rp_nl:
        ld a,(rp_txt)
        or a
        jr nz,rpn1
        ld a,(rp_row)
        inc a
        ld (rp_row),a
        push bc
        ld b,a
        ld c,0
        call set_at
        pop bc
        ret
rpn1:   ld a,76h
        jp out_char

; A = codigo ZX81 -> ASCII (el video inverso se ignora; lo que no esta
; en asctab sale como '?'). Conserva BC, DE, HL.
zx2asc:
        and 7Fh
        cp 26h
        jr c,za1
        add a,'A'-26h
        ret
za1:    cp 1Ch
        jr c,za2
        add a,'0'-1Ch
        ret
za2:    push hl
        push de
        ld e,a
        ld hl,asctab
za3:    ld a,(hl)
        or a
        jr z,za4
        ld d,a
        inc hl
        ld a,(hl)
        inc hl
        cp e
        jr nz,za3
        ld a,d
        pop de
        pop hl
        ret
za4:    ld a,'?'
        pop de
        pop hl
        ret

; =============================================================
; Test 13: RTC y bateria (USR 22567)
; Muestra la fecha/hora, comprueba que los segundos avanzan y que la
; bateria del RTC esta entre 2,5 y 3,6 V.
; =============================================================
rt_test:
        ld hl,s_rt_title
        call title
        ld hl,0
        ld (err_a),hl
        call rtc_read
        jp c,rt_to
        call pline
        db 2
        dw s_rt_now
        ld hl,rtc_buf
        ld b,22
rtt1:    ld a,(hl)
        call out_char
        inc hl
        djnz rtt1
        call pline
        db 3
        dw s_rt_run
        call wait_sec
        jr c,rt_to
        ld hl,s_ok
        call out_str

        call bat_read
        jr c,rt_to
        call pline
        db 5
        dw s_rt_bat
        ld hl,bat_buf
        ld b,5
rtt2:    ld a,(hl)
        call out_char
        inc hl
        djnz rtt2
        ld a,(bat_buf)          ; decimas de voltio: V*10 + primera decimal
        sub Z_0
        ld b,a
        add a,a
        add a,a
        add a,b
        add a,a
        ld b,a
        ld a,(bat_buf+2)
        sub Z_0
        add a,b
        cp 25
        jr c,rtt3
        cp 37
        jr c,rtt4
rtt3:    ld hl,s_rt_range
        call out_str
        ld hl,err_a
        call inc16
rtt4:    ld hl,(err_a)
        ld b,7
        call line_result
        ld bc,(err_a)
        ret

rt_to:  call pline
        db 6
        dw s_rtc_to
        ld hl,1
        ld b,7
        call line_result
        ld bc,9999
        ret

; A = handle -> A = estado. Carry = timeout.
f_close1:
        push af
        ld a,57                 ; F_CLOSE
        call m_send
        pop bc
        ret c
        ld a,b
        call m_send
        ret c
        jp m_recv

; A = handle -> (fs_size) = tamano (4 bytes). A = estado. Carry = timeout.
f_stat1:
        push af
        ld a,59                 ; F_STAT
        call m_send
        pop bc
        ret c
        ld a,b
        call m_send
        ret c
        ld hl,fs_size
        ld b,8                  ; tamano (4) + fecha y hora (4)
fs1:    call m_recv
        ret c
        ld (hl),a
        inc hl
        djnz fs1
        jp m_recv

; A = handle, DE = cuenta (<= 256) -> rd_buf. A = estado. Carry = timeout.
f_read1:
        push af
        ld a,55                 ; F_READ
        call m_send
        pop bc
        ret c
        ld a,b
        call m_send
        ret c
        ld a,e
        call m_send
        ret c
        ld a,d
        call m_send
        ret c
        ld hl,rd_buf
fr1a:   call m_recv
        ret c
        ld (hl),a
        inc hl
        dec de
        ld a,d
        or e
        jr nz,fr1a
        jp m_recv

; A = handle, HL = desplazamiento (16 bits) -> A = estado. Carry = timeout.
f_seek1:
        push af
        ld a,54                 ; F_SEEK
        call m_send
        pop bc
        ret c
        ld a,b
        call m_send
        ret c
        ld a,l
        call m_send
        ret c
        ld a,h
        call m_send
        ret c
        xor a
        call m_send
        ret c
        xor a
        call m_send
        ret c
        jp m_recv

; =============================================================
; Test 15: lectura de la SD (USR 22573)
; Lee /SYS/SDBOOST.ROM (F_OPEN/F_STAT/F_READ, bloques de 256) y lo compara
; con la ROM cargada en memoria (se carga en la direccion 0: bloques 0 y
; 1). Lo lee dos veces y compara las sumas de control de las dos lecturas.
; =============================================================
sr_test:
        ld hl,s_sr_title
        call title
        call pline
        db 2
        dw s_sr_d1
        call SET_FAST           ; tarda: en FAST va unas 4 veces mas rapido
        ld hl,0
        ld (err_a),hl           ; diferencias en el bloque 0
        ld (err_b),hl           ; diferencias en el bloque 1
        ld (err_c),hl           ; fallos de la segunda lectura
        xor a
        ld (first_done),a
        ld (sr_second),a

        call sr_pass            ; primera lectura: compara con memoria
        jp c,sd_to
        ld hl,(sr_sum)
        ld (sr_sum1),hl
        ld a,1
        ld (sr_second),a
        call sr_pass            ; segunda: solo la suma
        jp c,sd_to
        ld hl,(sr_sum)
        ld de,(sr_sum1)
        or a
        sbc hl,de
        jr z,srt1
        ld hl,err_c
        call inc16
srt1:   call SLOW_FAST
        call plnum
        db 4
        dw s_sr_size,fs_size
        call plnum
        db 5
        dw s_sr_b0,err_a
        call plnum
        db 6
        dw s_sr_b1,err_b
        call pline
        db 7
        dw s_sr_2nd
        ld hl,s_ok
        ld a,(err_c)
        or a
        jr z,srt2
        ld hl,s_fail
srt2:    call out_str
        ld a,(first_done)
        or a
        jr z,srt3
        call pline
        db 8
        dw s_sr_first
        ld hl,(fe_addr)
        call out_hex16
srt3:    ld hl,(err_a)
        ld de,(err_b)
        add hl,de
        ld de,(err_c)
        add hl,de
        push hl
        ld b,10
        call line_result
        pop bc
        ret

; Una lectura completa del fichero. (sr_second) = 0: compara con memoria;
; 1: solo suma. Deja la suma en sr_sum. Carry = timeout / no se abre.
sr_pass:
        ld hl,0
        ld (sr_sum),hl
        ld (sr_off),hl
        ld hl,s_romfile
        ld e,0
        ld a,53                 ; F_OPEN (ASCII)
        call cmd_name
        ret c
        cp 0FFh
        scf
        ret z                   ; no existe
        ld (sr_h),a
        call f_stat1
        ret c
srp1:   ld hl,(fs_size)         ; quedan = tamano - desplazamiento
        ld de,(sr_off)
        or a
        sbc hl,de
        jr z,srp4               ; terminado
        ld a,h
        or a
        ld de,256
        jr nz,srp2
        ld e,l                  ; ultimo bloque corto
        ld d,0
srp2:   ld (sr_cnt),de
        ld a,(sr_h)
        call f_read1
        ret c
        ld hl,rd_buf            ; suma y comparacion
        ld de,(sr_off)
        ld bc,(sr_cnt)
srp3:   push bc
        ld a,(hl)
        push hl
        ld hl,(sr_sum)
        add a,l
        ld l,a
        jr nc,srp3a
        inc h
srp3a:  ld (sr_sum),hl
        pop hl
        ld a,(sr_second)
        or a
        jr nz,srp3c
        ld a,(de)               ; la ROM en memoria, misma direccion
        cp (hl)
        jr z,srp3c
        push hl
        ld hl,err_a
        ld a,d
        cp 20h
        jr c,srp3b
        ld hl,err_b
srp3b:  call inc16
        ld a,(first_done)
        or a
        jr nz,srp3d
        inc a
        ld (first_done),a
        ld (fe_addr),de
srp3d:  pop hl
srp3c:  inc hl
        inc de
        pop bc
        dec bc
        ld a,b
        or c
        jr nz,srp3
        ld (sr_off),de
        jp srp1
srp4:   ld a,(sr_h)
        call f_close1
        ret

sd_to:  call SLOW_FAST
        call pline
        db 6
        dw s_sd_to
        ld hl,1
        ld b,8
        call line_result
        ld bc,9999
        ret

; =============================================================
; Test 16: escritura de la SD (USR 22576)
;   1. SAVE /SD81TEST.TMP con 4096 bytes pseudoaleatorios (semilla fija).
;   2. F_OPEN + F_STAT (tamano 4096) + F_READ: tiene que coincidir.
;   3. F_SEEK 1000 + F_WRITE de 256 bytes nuevos; F_SEEK + F_READ: igual.
;   4. F_CLOSE + DEL; volver a abrirlo tiene que fallar.
; =============================================================
SW_SIZE equ 4096

sw_test:
        ld hl,s_sw_title
        call title
        call pline
        db 2
        dw s_sw_d1
        call SET_FAST           ; tarda: en FAST va unas 4 veces mas rapido
        ld hl,0
        ld (err_a),hl
        ld (err_b),hl

        ld hl,1234h             ; --- 1: SAVE ---
        ld (xrnd+1),hl
        ld a,10                 ; SAVE
        call m_send
        jp c,sw_to
        ld hl,s_tmpfile
        ld e,1
        call send_name
        jp c,sw_to
        xor a                   ; byte bajo de SW_SIZE (4096 = 1000h)
        call m_send
        jp c,sw_to
        ld a,SW_SIZE/256
        call m_send
        jp c,sw_to
        ld bc,SW_SIZE
swt1:    push bc
        call xrnd
        call m_send
        pop bc
        jp c,sw_to
        dec bc
        ld a,b
        or c
        jr nz,swt1
        call m_recv             ; estado del SAVE
        jp c,sw_to
        push af
        call pline
        db 4
        dw s_sw_save
        pop af
        call sw_okfail

        ld hl,s_tmpfile         ; --- 2: abrir, tamano y releer ---
        ld e,0
        ld a,53
        call cmd_name
        jp c,sw_to
        cp 0FFh
        jp z,sw_noopen
        ld (sr_h),a
        call f_stat1
        jp c,sw_to
        ld hl,(fs_size)
        ld de,SW_SIZE
        or a
        sbc hl,de
        jr z,swt2
        ld hl,err_a
        call inc16
swt2:    ld hl,1234h
        ld (xrnd+1),hl
        ld b,SW_SIZE/256
swt3:    push bc
        ld a,(sr_h)
        ld de,256
        call f_read1
        call nc,sw_cmp256
        pop bc
        jp c,sw_to
        djnz swt3
        call plnum
        db 5
        dw s_sw_read,err_a

        ld a,(sr_h)             ; --- 3: F_SEEK + F_WRITE + relectura ---
        ld hl,1000
        call f_seek1
        jp c,sw_to
        ld hl,5678h
        ld (xrnd+1),hl
        ld a,56                 ; F_WRITE
        call m_send
        jp c,sw_to
        ld a,(sr_h)
        call m_send
        jp c,sw_to
        xor a
        call m_send             ; 256 = 0100h
        jp c,sw_to
        ld a,1
        call m_send
        jp c,sw_to
        ld b,0
swt4:    push bc
        call xrnd
        call m_send
        pop bc
        jp c,sw_to
        djnz swt4
        call m_recv             ; estado del F_WRITE
        jp c,sw_to
        or a
        jr z,swt5
        ld hl,err_b
        call inc16
swt5:    ld a,(sr_h)
        ld hl,1000
        call f_seek1
        jp c,sw_to
        ld hl,5678h
        ld (xrnd+1),hl
        ld a,(sr_h)
        ld de,256
        call f_read1
        jp c,sw_to
        ld hl,(err_a)           ; sw_cmp256 cuenta en err_a: se pasa a err_b
        push hl
        ld hl,0
        ld (err_a),hl
        call sw_cmp256
        ld hl,(err_a)
        ld de,(err_b)
        add hl,de
        ld (err_b),hl
        pop hl
        ld (err_a),hl
        call plnum
        db 6
        dw s_sw_seek,err_b

        ld a,(sr_h)             ; --- 4: cerrar, borrar y comprobar ---
        call f_close1
        jp c,sw_to
        ld hl,s_tmpfile
        ld e,1
        ld a,4                  ; DEL
        call cmd_name
        jp c,sw_to
        push af
        call pline
        db 7
        dw s_sw_del
        pop af
        call sw_okfail
        ld hl,s_tmpfile
        ld e,0
        ld a,53
        call cmd_name
        jp c,sw_to
        cp 0FFh
        jr z,swt6
        call f_close1           ; se ha abierto: no se habia borrado
        ld hl,err_b
        call inc16
        ld hl,s_sw_still
        call out_str
swt6:   call SLOW_FAST
        jp two_results

; A = estado -> imprime OK (0) o FAIL (y cuenta en err_b)
sw_okfail:
        or a
        ld hl,s_ok
        jr z,sof1
        push af
        ld hl,err_b
        call inc16
        pop af
        ld hl,s_fail
sof1:   jp out_str

; Compara rd_buf (256 bytes) con la secuencia de xrnd; cuenta en err_a.
sw_cmp256:
        push af
        ld de,rd_buf
        ld b,0
sc1:    push bc
        push de
        call xrnd
        pop de
        ex de,hl
        cp (hl)
        ex de,hl
        jr z,sc2
        push de
        ld hl,err_a
        call inc16
        pop de
sc2:    inc de
        pop bc
        djnz sc1
        pop af
        or a                    ; NC
        ret

sw_noopen:
        call SLOW_FAST
        call pline
        db 5
        dw s_sw_noopen
        ld hl,1
        ld b,8
        call line_result
        ld bc,9999
        ret

sw_to:  jp sd_to
s_mu_title: db "MCU PROTOCO",'L'+80h
s_mu_d1:   db "2000 X SETBYTE + GETBYT",'E'+80h
s_mu_d2:   db "RANDOM INDEX 64-127 AND VALU",'E'+80h
s_mu_n:    db "2000 TRANSFERS DON",'E'+80h
s_mu_err:  db "WRONG VALUES:",' '+80h
s_mu_to:   db "MCU TIMEOUT AT TRANSFER",' '+80h
s_in_title: db "MACHINE INF",'O'+80h
s_in_mcu:  db "MCU",' '+80h
s_in_rom:  db "  ROM",' '+80h
s_in_fpga: db "  FPGA",' '+80h
s_in_fe:   db "PORT FE BIT 6:",' '+80h
s_in_50:   db "1 (50HZ",')'+80h
s_in_60:   db "0 (60HZ",')'+80h
s_in_margin: db "MARGIN:",' '+80h
s_in_paging: db "PAGING:",' '+80h
s_in_full: db "FULL (64 PAGES",')'+80h
s_in_half: db "HALF (32 PAGES",')'+80h
s_in_67:   db "BLOCKS 6/7:",' '+80h
s_in_own:  db "OWN PAGES (48K",')'+80h
s_in_mirror: db "MIRROR OF 2/3 (32K",')'+80h
s_in_map:  db "MAPPER (BLOCK:PAGE",')'+80h
s_in_ramtop: db "RAMTOP:",' '+80h
s_fr_title: db "FRAME RAT",'E'+80h
s_fr_d1:   db "VSYNC PER RTC SECON",'D'+80h
s_fr_nat:  db "NATIVE:",' '+80h
s_fr_slow: db "NEEDS SLOW MOD",'E'+80h
s_fr_sf:   db "SUPERFAST:",' '+80h
s_fr_fps:  db " FP",'S'+80h
s_rtc_to:  db "RTC TIMEOUT / NOT RUNNIN",'G'+80h
s_ck_title: db "CPU CLOC",'K'+80h
s_ck_d1:   db "FAST LOOP OF 16248960 T, TIME",'D'+80h
s_ck_d2:   db "WITH THE RTC (1/100 S",')'+80h
s_ck_el:   db "ELAPSED (1/100 S):",' '+80h
s_ck_mhz:  db "CPU CLOCK:",' '+80h
s_ck_unit: db " MHZ (3.25",')'+80h
s_ck_nocc: db "RTC WITHOUT 1/100 S: NO MEASUR",'E'+80h
s_rp_title: db "SUMMAR",'Y'+80h
s_rp_head: db "SD81 BOOSTER TEST V0.9 - SUMMAR",'Y'+80h
s_rp_date: db "DATE",' '+80h
s_rp_ok:   db "OK:",' '+80h
s_rp_fail: db "  FAIL:",' '+80h
s_rp_nr:   db "  NOT RUN:",' '+80h
s_rp_notrun: db "NOT RU",'N'+80h
s_rp_failn: db "FAIL",' '+80h
s_rp_saved: db "SAVED TO SD81TEST.TX",'T'+80h
s_rp_err:  db "SD ERROR",' '+80h
s_rp_file: db "SD81TEST.TXT",0
; Pruebas automaticas del resumen: N y nombre (17 columnas)
rp_list:
        db 1, "MC45 BLOCKS 4-5 ",' '+80h
        db 2, "MC45 BLOCKS 6-7 ",' '+80h
        db 3, "MAPPER STRESS   ",' '+80h
        db 4, "POKE 2045       ",' '+80h
        db 6, "ROMLOCK         ",' '+80h
        db 7, "MCU PROTOCOL    ",' '+80h
        db 9, "UNPAGED MEMORY  ",' '+80h
        db 10, "MAPPER REGISTERS",' '+80h
        db 11, "BLOCK 0 PROTECT ",' '+80h
        db 12, "FRAME RATE      ",' '+80h
        db 13, "RTC AND BATTERY ",' '+80h
        db 14, "AY REGISTERS    ",' '+80h
        db 15, "SD READ         ",' '+80h
        db 16, "SD WRITE        ",' '+80h
        db 17, "SYSTEM PAGES    ",' '+80h
        db 31, "MCU AY REGISTERS",' '+80h
        db 36, "CPU CLOCK       ",' '+80h
        db 0FFh
s_rt_title: db "RTC AND BATTER",'Y'+80h
s_rt_now:  db "RTC",' '+80h
s_rt_run:  db "SECONDS RUNNING:",' '+80h
s_rt_bat:  db "BATTERY:",' '+80h
s_rt_range: db " OUT OF RANG",'E'+80h
s_sr_title: db "SD REA",'D'+80h
s_sr_d1:   db "SDBOOST.ROM VS THE ROM IN RA",'M'+80h
s_sr_size: db "FILE SIZE:",' '+80h
s_sr_b0:   db "DIFFERENT IN BLOCK 0:",' '+80h
s_sr_b1:   db "DIFFERENT IN BLOCK 1:",' '+80h
s_sr_2nd:  db "SECOND READ CHECKSUM:",' '+80h
s_sr_first: db "FIRST DIFFERENCE AT",' '+80h
s_sd_to:   db "SD/MCU TIMEOUT OR NO FIL",'E'+80h
s_romfile: db "/SYS/SDBOOST.ROM",0
s_tmpfile: db "/SD81TEST.TMP",0
s_sw_title: db "SD WRIT",'E'+80h
s_sw_d1:   db "/SD81TEST.TMP, 4096 BYTE",'S'+80h
s_sw_save: db "SAVE:",' '+80h
s_sw_read: db "READ BACK ERRORS:",' '+80h
s_sw_seek: db "SEEK+WRITE ERRORS:",' '+80h
s_sw_del:  db "DELETE:",' '+80h
s_sw_still: db " STILL THER",'E'+80h
s_sw_noopen: db "CANNOT OPEN THE FIL",'E'+80h
err_c:      dw 0
mu_idx:     db 0        ; protocolo MCU: indice y valor en curso
mu_val:     db 0
fr_n:       dw 0        ; frecuencia de cuadro: cuadros medidos
sec0:       db 0        ; segundos del RTC al empezar a esperar
ck_t0:      dw 0        ; reloj de la CPU: instantes (s*100+cs),
ck_t1:      dw 0        ; transcurrido (1/100 s) y MHz*100
ck_el:      dw 0
ck_mhz:     dw 0
rp_txt:     db 0        ; resumen: 0 = pantalla, 1 = texto
rp_row:     db 0        ; fila de pantalla en curso
rp_ok:      db 0        ; pruebas OK, FAIL y no ejecutadas
rp_bad:     db 0
rp_nr:      db 0
rp_n:       dw 0        ; bytes del texto y del fichero
rp_len:     dw 0
sr_sum:     dw 0        ; lectura de SD: suma de la pasada en curso
sr_sum1:    dw 0        ; suma de la primera pasada
sr_off:     dw 0        ; desplazamiento en el fichero
sr_cnt:     dw 0        ; bytes del bloque en curso
sr_h:       db 0        ; handle del fichero abierto
sr_second:  db 0        ; 1 = segunda pasada (solo suma)

; -------------------------------------------------------------
; Buffers del modulo sin valor inicial (no van en el .bin).
; mbss_end tiene que quedar por debajo de $8000.
; -------------------------------------------------------------
mbss:
rtc_buf      equ mbss   ; "AAAA-MM-DD HH:MM:SS.CC" (codigos ZX81)
bat_buf      equ rtc_buf+22   ; "V.mmm" (codigos ZX81)
fs_size      equ bat_buf+5   ; F_STAT: tamano (4) + fecha y hora (4)
rd_buf       equ fs_size+8   ; bloque leido con F_READ
mbss_end     equ rd_buf+256
