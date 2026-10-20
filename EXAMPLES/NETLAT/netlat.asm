; =====================================================================
;  NETLAT.ASM - mide la latencia de la red del SD81 Booster desde el Z80, capa por capa, con la propia net.inc. Es una
;  tabla de saltos para el BASIC de NETLAT.B81 (como NETCHAT). Los tiempos salen del reloj del MCU (el comando 50, el de
;  LOAD *RTC), que da CENTESIMAS: cada muestra sale redondeada a 10 ms, asi que lo que vale es el MEDIO de muchas.
;
;  Pruebas (cada una acumula n, minimo, maximo y suma en centesimas):
;      0  RTC       dos lecturas del reloj seguidas: lo que cuesta medir (se resta mentalmente de las demas)
;      1  NET_READ  5 tandas de 100 comandos NET_READ con max=0 (el "¿hay algo?" barato del Z80 al MCU): el tiempo de 100
;                   comandos; dice cuanto cuesta preguntar mas a menudo
;      2  AT        "AT" + CR hasta el "OK", leyendo SIN pausas: ida y vuelta Z80 - MCU - ESP32 - MCU - Z80 en el mejor caso
;      3  AT 20MS   lo mismo con at_cmd de net.inc (lee, espera 20 ms, vuelve a leer): lo que usan los juegos
;      4  MQTT      publica un mensaje a la sala (sd81/..../9) y pide AT+MQTTRX hasta que vuelve el eco (el servidor lo
;                   devuelve porque la sala esta suscrita con /+): Z80 - ESP32 - internet - servidor - internet - ESP32 - Z80.
;                   Hace falta haber llamado antes a room_new (la conexion al servidor)
;      5  PUB       solo publicar, hasta el OK, con at_raw (espera el '>' del modem antes de mandar el dato)
;      6  PUB P     lo mismo con at_raw_p (comando, CR y dato seguidos: se ahorra una ida y vuelta del puente)
;      7  MQTT P    el eco de la prueba 4 publicando con at_raw_p
;
;  Tabla de saltos (BC vuelve con el resultado: LET R=USR 24576):
;      24576  net_init     ATZ, ATE0
;      24579  room_new     crea una sala de prueba y conecta al servidor (hasta 40 s)
;      24582  run          las 8 pruebas (unos 40 s con red); resultados en 31744 ($7C00): 8 registros de 8 bytes
;                          n (1), minimo (2), maximo (2), suma (2) y fallos (1), todo en centesimas
;      24585  room_leave
;
;  Memoria:  $6000- este programa y net.inc;  $7000-$73FF el buzon de net.inc (NB_BASE);  $7C00 los resultados
;  Ensamblar: pasmo netlat.asm netlat.bin netlat.sym   (compila.bat)
; =====================================================================

NB_BASE     equ 7000h
RESL        equ 7C00h           ; 8 registros de 8 bytes
TV0         equ 7C60h           ; (3) la hora de salida: minutos, segundos, centesimas
TV1         equ 7C64h           ; (3) la de llegada
RTCB        equ 7C70h           ; (22) lo que contesta el reloj
SCNT        equ 7C90h           ; contador de repeticiones
SIDX        equ 7C91h           ; (2) contador de espera
SDRAIN      equ 7C93h
STEST       equ 7C94h           ; la prueba en curso (su registro)
SPIPE       equ 7C95h           ; 1: publicar con at_raw_p (sin esperar el '>')

N_RTC       equ 40
N_AT        equ 40
N_MQTT      equ 30

            org  24576

            jp   e_init             ; 24576
            jp   e_new              ; 24579
            jp   e_run              ; 24582
            jp   e_leave            ; 24585

e_init:     ld   hl,net_init
            jr   run
e_new:      ld   hl,room_new
            jr   run
e_leave:    ld   hl,room_leave
run:        call jp_hl
            ld   c,a
            ld   b,0
            ret
jp_hl:      jp   (hl)

; ---------------------------------------------------------------------
;  Las pruebas
; ---------------------------------------------------------------------
e_run:      ld   hl,RESL
            ld   de,RESL+1
            ld   bc,63
            ld   (hl),0
            ldir
            ; --- 0: el coste de leer el reloj
            ld   a,N_RTC
            ld   (SCNT),a
t0_l:       ld   de,TV0
            call t_now
            ld   de,TV1
            call t_now
            call t_diff
            ex   de,hl
            xor  a
            call stat_add
            ld   hl,SCNT
            dec  (hl)
            jr   nz,t0_l
            ; --- 1: 5 tandas de 100 comandos NET_READ con max = 0
            ld   a,5
            ld   (SCNT),a
t1_l:       ld   de,TV0
            call t_now
            ld   a,100
            ld   (SDRAIN),a
t1_c:       ld   hl,V_RXBUF
            ld   b,0
            call net_read
            ld   hl,SDRAIN
            dec  (hl)
            jr   nz,t1_c
            ld   de,TV1
            call t_now
            call t_diff
            ex   de,hl
            ld   a,1
            call stat_add
            ld   hl,SCNT
            dec  (hl)
            jr   nz,t1_l
            ; --- 2: AT sin pausas
            call flush_rx
            ld   a,N_AT
            ld   (SCNT),a
t2_l:       ld   de,TV0
            call t_now
            ld   hl,s_at
            call tx_str
            call tx_cr
            ld   hl,6000            ; plazo: vueltas de net_read sin pausa
            ld   (SIDX),hl
t2_w:       ld   hl,V_RXBUF
            ld   b,128
            call net_read
            ld   a,b
            or   a
            jr   z,t2_n
            ld   d,a
            ld   hl,V_RXBUF
t2_s:       ld   a,(hl)
            cp   'K'                ; el "OK"
            jr   z,t2_ok
            inc  hl
            dec  d
            jr   nz,t2_s
t2_n:       ld   hl,(SIDX)
            dec  hl
            ld   (SIDX),hl
            ld   a,h
            or   l
            jr   nz,t2_w
            ld   a,2                ; plazo: no contesto
            call stat_fail
            jr   t2_d
t2_ok:      ld   de,TV1
            call t_now
            call t_diff
            ex   de,hl
            ld   a,2
            call stat_add
t2_d:       ld   b,2                ; lo que quede (el CR LF finales), sin pausas
t2_dr:      push bc
            ld   hl,V_RXBUF
            ld   b,128
            call net_read
            pop  bc
            djnz t2_dr
            ld   hl,SCNT
            dec  (hl)
            jr   nz,t2_l
            ; --- 3: AT con at_cmd (pausa de 20 ms entre lecturas)
            call flush_rx
            ld   a,N_AT
            ld   (SCNT),a
t3_l:       ld   de,TV0
            call t_now
            ld   hl,s_at
            ld   de,TMO_S
            call at_cmd
            jr   nz,t3_bad
            ld   de,TV1
            call t_now
            call t_diff
            ex   de,hl
            ld   a,3
            call stat_add
            jr   t3_n
t3_bad:     ld   a,3
            call stat_fail
t3_n:       ld   hl,SCNT
            dec  (hl)
            jr   nz,t3_l
            ; --- 5 y 6: solo publicar (hasta el OK), con at_raw (espera el '>') y con at_raw_p (todo seguido)
            call flush_rx
            call drain
            ld   a,5
            ld   (STEST),a
            xor  a
            ld   (SPIPE),a
            ld   a,N_MQTT
            ld   (SCNT),a
t5_l:       call pub_only
            ld   hl,SCNT
            dec  (hl)
            jr   nz,t5_l
            ld   a,6
            ld   (STEST),a
            ld   a,1
            ld   (SPIPE),a
            ld   a,N_MQTT
            ld   (SCNT),a
t6_l:       call pub_only
            ld   hl,SCNT
            dec  (hl)
            jr   nz,t6_l
            ; --- 4 y 7: el eco de un mensaje MQTT, con at_raw y con at_raw_p
            call drain
            ld   a,4
            ld   (STEST),a
            xor  a
            ld   (SPIPE),a
            call echo_test
            call drain
            ld   a,7
            ld   (STEST),a
            ld   a,1
            ld   (SPIPE),a
            call echo_test
            ld   bc,0
            ret

; tira lo que haya pendiente en la sala (la presencia, los ecos de antes) hasta que AT+MQTTRX no traiga nada
drain:      ld   b,80
dr_lp:      push bc
            ld   hl,s_rx
            ld   de,TMO_S
            call at_cmd
            pop  bc
            ld   a,(V_RESPLEN)
            or   a
            ret  z
            djnz dr_lp
            ret

; AT+MQTTPUB=<sala>/9,0,0,1 y el dato en V_CMD / V_RAWP; llama a at_raw o at_raw_p segun SPIPE. Sale con Z si el OK
do_pub:     call cb_init
            ld   hl,s_pub
            call cb_puts
            ld   hl,V_BASE
            call cb_puts
            ld   hl,s_lat9
            call cb_puts
            ld   hl,s_x
            ld   (V_RAWP),hl
            ld   a,1
            ld   (V_RAWL),a
            ld   hl,V_CMD
            ld   de,TMO_S
            ld   a,(SPIPE)
            or   a
            jp   nz,at_raw_p
            jp   at_raw

; una publicacion; mide hasta el OK (prueba STEST)
pub_only:   ld   de,TV0
            call t_now
            call do_pub
            jr   nz,po_bad
            ld   de,TV1
            call t_now
            call t_diff
            ex   de,hl
            ld   a,(STEST)
            call stat_add
            ret
po_bad:     ld   a,(STEST)
            jp   stat_fail

; N_MQTT veces: publica y pide AT+MQTTRX hasta que vuelve el eco; mide de la salida a la llegada (prueba STEST)
echo_test:  ld   a,N_MQTT
            ld   (SCNT),a
et_l:       ld   de,TV0
            call t_now
            call do_pub
            jr   nz,et_bad
            ld   a,150              ; hasta 150 preguntas (unos 5 s)
            ld   (SDRAIN),a
et_w:       ld   hl,s_rx
            ld   de,TMO_S
            call at_cmd
            ld   a,(V_RESPLEN)
            or   a
            jr   nz,et_got
            ld   hl,SDRAIN
            dec  (hl)
            jr   nz,et_w
et_bad:     ld   a,(STEST)
            call stat_fail
            jr   et_n
et_got:     ld   de,TV1
            call t_now
            call t_diff
            ex   de,hl
            ld   a,(STEST)
            call stat_add
et_n:       ld   hl,SCNT
            dec  (hl)
            jr   nz,et_l
            ret

; ---------------------------------------------------------------------
;  Estadisticas: A = prueba, DE = valor (centesimas)
; ---------------------------------------------------------------------
rec_ix:     add  a,a                ; IX = RESL + 8 * prueba
            add  a,a
            add  a,a
            ld   c,a
            ld   b,0
            ld   ix,RESL
            add  ix,bc
            ret

stat_add:   push de
            call rec_ix
            pop  de
            ld   a,(ix+0)
            inc  (ix+0)
            or   a
            jr   nz,sa_c
            ld   (ix+1),e           ; la primera: minimo, maximo y suma son ella
            ld   (ix+2),d
            ld   (ix+3),e
            ld   (ix+4),d
            ld   (ix+5),e
            ld   (ix+6),d
            ret
sa_c:       ld   l,(ix+1)
            ld   h,(ix+2)
            or   a
            sbc  hl,de              ; minimo - valor
            jr   c,sa_x
            ld   (ix+1),e
            ld   (ix+2),d
sa_x:       ld   l,(ix+3)
            ld   h,(ix+4)
            or   a
            sbc  hl,de              ; maximo - valor
            jr   nc,sa_s
            ld   (ix+3),e
            ld   (ix+4),d
sa_s:       ld   l,(ix+5)
            ld   h,(ix+6)
            add  hl,de
            ld   (ix+5),l
            ld   (ix+6),h
            ret

stat_fail:  call rec_ix
            inc  (ix+7)
            ret

; ---------------------------------------------------------------------
;  El reloj del MCU (comando 50: la hora con centesimas)
; ---------------------------------------------------------------------
; DE = destino (3 bytes): minutos, segundos y centesimas
t_now:      push de
            call sd_clk
            ld   a,50
            call mcu_send
            xor  a
            call mcu_send           ; longitud 0: pide la fecha y la hora
            ld   hl,RTCB
            ld   b,22
tn_lp:      call mcu_recv
            ld   (hl),a
            inc  hl
            djnz tn_lp
            call mcu_recv           ; el estado
            pop  de
            ld   hl,RTCB+14         ; "AAAA-MM-DD hh:mm:ss.cc" en codigos del ZX81
            call dig2
            ld   (de),a             ; mm
            inc  de
            ld   hl,RTCB+17
            call dig2
            ld   (de),a             ; ss
            inc  de
            ld   hl,RTCB+20
            call dig2
            ld   (de),a             ; cc
            ret

dig2:       ld   a,(hl)
            sub  28
            ld   b,a
            add  a,a
            add  a,a
            add  a,b
            add  a,a                ; 10 * decenas
            inc  hl
            ld   b,a
            ld   a,(hl)
            sub  28
            add  a,b
            ret

; A = minutos, C = segundos -> HL = A * 60 + C
to_secs:    ld   l,a
            ld   h,0
            ld   d,h
            ld   e,l
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl              ; 16 m
            or   a
            sbc  hl,de              ; 15 m
            add  hl,hl
            add  hl,hl              ; 60 m
            ld   e,c
            ld   d,0
            add  hl,de
            ret

; HL = TV1 - TV0 en centesimas (da la vuelta a la hora)
t_diff:     ld   a,(TV1)
            ld   hl,TV1+1
            ld   c,(hl)
            call to_secs
            push hl
            ld   a,(TV0)
            ld   hl,TV0+1
            ld   c,(hl)
            call to_secs
            ex   de,hl
            pop  hl
            or   a
            sbc  hl,de              ; segundos de diferencia
            jr   nc,td_ok
            ld   de,3600
            add  hl,de
td_ok:      ld   d,h
            ld   e,l                ; DE = x
            add  hl,hl
            add  hl,hl              ; 4 x
            ld   b,h
            ld   c,l
            ex   de,hl              ; HL = x
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl              ; 32 x
            ld   d,h
            ld   e,l
            add  hl,hl              ; 64 x
            add  hl,de              ; 96 x
            add  hl,bc              ; 100 x
            ld   a,(TV1+2)
            ld   e,a
            ld   d,0
            add  hl,de
            ld   a,(TV0+2)
            ld   e,a
            ld   d,0
            or   a
            sbc  hl,de              ; + centesimas de llegada - centesimas de salida
            bit  7,h
            ret  z
            ld   de,100             ; negativo: el reloj dio las centesimas ya vueltas pero aun no el segundo (se vio una vez
            add  hl,de              ; en la placa simulada): el segundo que faltaba
            ret

s_at:       defm "AT",0
s_lat9:     defm "/9,0,0,1",0
s_x:        defm "x"

            include "../NETCHAT/net.inc"
