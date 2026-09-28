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
        jp mu_test             ; 0: USR 20501
        jp info_test           ; 1: USR 20504
        jp fr_test             ; 2: USR 20516
        jp rt_test             ; 3: USR 20519
        jp sr_test             ; 4: USR 20525
        jp sw_test             ; 5: USR 20528

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
; Test 7: protocolo con el MCU (USR 20501)
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
; Test 8: informacion de la maquina (USR 20504)
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
; Test 12: frecuencia de cuadro (USR 20516)
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
; Test 13: RTC y bateria (USR 20519)
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
; Test 15: lectura de la SD (USR 20525)
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
; Test 16: escritura de la SD (USR 20528)
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
