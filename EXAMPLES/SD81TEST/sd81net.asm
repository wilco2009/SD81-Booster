; =============================================================
; SD81NET.ASM -- SD81TEST, modulo NET: red a traves del modulo WiFi
;
; Se carga en MOD_ORG ($6000) con
;   LOAD FAST "SD81NET.BIN" CODE 24576
; (lo hace el stub BASIC) y se ejecuta a traves de la tabla de saltos
; del nucleo (SD81TEST.BIN), cuyas rutinas usa por su .sym. Ensamblar
; despues del nucleo:
;   pasmo sd81test.asm SD81TEST.BIN sd81test.sym
;   pasmo sd81net.asm SD81NET.BIN
; mbss_end (en el .sym de este modulo, si se pide) tiene que quedar
; por debajo de $8000.
;
; El puente de red del SD81 es un modem Hayes ya enchufado: el Z80 manda
; y recibe bytes con NET_WRITE (67) y NET_READ (66), y el ESP32
; interpreta los comandos AT (ATZ, ATE0, ATDT host:puerto, ATH, +++) y
; lleva el socket TCP. No hay ICMP (ping) ni UDP (NTP): la prueba de
; conexion es HTTP. Servidor: example.org, reservado por la RFC 2606 para
; ejemplos y mantenido por la IANA; si falla, example.net (example.com lo
; bloquean algunos routers). Las esperas se miden con FRAMES: necesitan
; SLOW. Los datos recibidos se acumulan en NW_BUF (bloque 4).
; Ver claude/planning/net_bridge_emulator.md.
; =============================================================
        include "sd81test.sym"

        org MOD_ORG
        db MOD_NET              ; cabecera: id del modulo
        dw bss_end              ; y fin del nucleo con el que se ensamblo
; Tabla de saltos del modulo (el nucleo entra por MOD_ORG+3+3*n)
        jp nw_wifi             ; 0: USR 22645
        jp nw_http             ; 1: USR 22648
        jp nw_time             ; 2: USR 22651

NW_BUF  equ 8000h               ; lo recibido (terminado en 0)
NW_END  equ NW_BUF+3000         ; tope (lo que pase se descarta)
CMD_NRD equ 66                  ; NET_READ
CMD_NWR equ 67                  ; NET_WRITE

; =============================================================
; Test 39: modulo WiFi (USR 22645)
; El STM32 escribe /MAN/IP.TXT al arrancar con "NO CONNEXION..."; el
; ESP32 lo sobrescribe al conectarse con su version y su IP. Se lee (el
; mismo fichero que LOAD THEN PRINT "*IP") y se buscan esas lineas.
; Devuelve 0 si el modulo esta conectado, 1 si no.
; =============================================================
nw_wifi:
        ld hl,s_nw_title
        call title
        call pline
        db 2
        dw s_nw_d1
        ld hl,s_ipfile          ; F_OPEN (ASCII)
        ld e,0
        ld a,53
        call cmd_name
        jp c,nw_to
        cp 0FFh
        jr nz,nwf2
        call pline
        db 4
        dw s_nw_noip
        jr nwf_bad
nwf2:   ld (nw_h),a
        call nw_fread
        jp c,nw_to
        ld a,(nw_h)
        call nw_fclose
        jp c,nw_to
        ld hl,t_ip              ; "IP ADDRESS: "
        call nw_find
        jr nz,nwf3
        ld b,4
        call nw_pline           ; linea de la IP
        ld hl,t_fw              ; "FIRMWARE VERSION: "
        call nw_find
        jr nz,nwf4
        ld b,5
        call nw_pline
nwf4:   ld hl,0
        jr nwf_res
nwf3:   ld hl,NW_BUF            ; no conectado: la primera linea del aviso
        ld b,4
        call nw_pline
nwf_bad:
        ld hl,1
nwf_res:
        ld (err_a),hl
        ld b,7
        call line_result
        ld bc,(err_a)
        ret

; =============================================================
; Test 40: conexion TCP (USR 22648)
; ATZ, ATE0, ATDT example.org:80 (o example.net), HEAD / HTTP/1.0 y la
; respuesta hasta que el servidor cuelga (NO CARRIER). Tiene que empezar
; por "HTTP/1.". Devuelve 0 si todo va bien, 1 si falla algo.
; =============================================================
nw_http:
        ld hl,s_nh_title
        call title
        call nw_slow
        jp z,nw_noslow
        call pline
        db 2
        dw s_nh_d1
        call nw_fetch
        or a
        ld hl,1
        jr nz,nwh1
        ld hl,t_http            ; la linea de estado
        call nw_find
        ld b,7
        call nw_pline
        ld hl,0
nwh1:   ld (err_a),hl
        ld b,9
        call line_result
        ld bc,(err_a)
        ret

; =============================================================
; Test 41: hora contra Internet (USR 22651)
; La cabecera Date: de la respuesta HTTP (hora UTC de un servidor
; sincronizado por NTP) contra el RTC. El RTC lleva la hora local (con
; el desfase y el horario de verano de NTP.CFG), asi que solo se comparan
; minutos y segundos: vale en todas las zonas con desfase de horas
; enteras. Tiene que diferir 3 s como mucho. Devuelve 0 o 1 (9999 si no
; hay conexion o no hay fecha).
; =============================================================
nw_time:
        ld hl,s_nt_title
        call title
        call nw_slow
        jp z,nw_noslow
        call pline
        db 2
        dw s_nt_d1
        call nw_fetch
        or a
        jp nz,nw_nodate
        call nw_rtc             ; RTC justo despues de la respuesta
        jp c,nw_to
        ld hl,t_date            ; "ate: " (Date: o date:)
        call nw_find
        jr z,nt1
        ld hl,t_date2           ; "ATE: "
        call nw_find
        jp nz,nw_nodate
nt1:    ld de,5                 ; detras de "ate: "
        add hl,de
nt2:    ld a,(hl)               ; primer ':' (el de HH:MM:SS)
        or a
        jp z,nw_nodate
        cp 0Dh
        jp z,nw_nodate
        cp ':'
        jr z,nt3
        inc hl
        jr nt2
nt3:    dec hl                  ; HL -> "HH:MM:SS"
        dec hl
        ld (nw_tp),hl
        call pline
        db 7
        dw s_nt_srv
        ld hl,(nw_tp)
        ld b,8
nt4:    ld a,(hl)
        call nw_char
        inc hl
        djnz nt4
        call pline
        db 8
        dw s_nt_rtc
        ld hl,nw_rtcb+11        ; "HH:MM:SS" en codigos ZX81
        ld b,8
nt5:    ld a,(hl)
        call out_char
        inc hl
        djnz nt5
        ld hl,(nw_tp)           ; segundos del servidor dentro de la hora
        inc hl
        inc hl
        inc hl
        call nw_ms_asc
        push hl
        ld de,nw_rtcb+14        ; y del RTC
        call nw_ms_zx
        pop de                  ; HL = RTC, DE = servidor
        or a
        sbc hl,de
        jr nc,nt6
        ex de,hl                ; valor absoluto
        ld hl,0
        or a
        sbc hl,de
nt6:    ld de,1800              ; la vuelta de la hora: min(d, 3600-d)
        or a
        sbc hl,de
        add hl,de
        jr c,nt7
        ex de,hl
        ld hl,3600
        or a
        sbc hl,de
nt7:    ld (nw_diff),hl
        call plnum
        db 9
        dw s_nt_diff,nw_diff
        ld hl,(nw_diff)
        ld de,4
        or a
        sbc hl,de
        ld hl,0
        jr c,nt8
        inc hl
        push hl
        call pline
        db 10
        dw s_nt_ntp
        pop hl
nt8:    ld (err_a),hl
        ld b,12
        call line_result
        ld bc,(err_a)
        ret

nw_nodate:
        call pline
        db 7
        dw s_nt_nodate
        ld bc,9999
        ret

nw_to:  call pline
        db 7
        dw s_nw_to
        ld bc,9999
        ret

nw_noslow:
        call pline
        db 4
        dw s_nw_slow
        ld bc,9999
        ret

; Z si NO estamos en SLOW (las esperas cuentan cuadros con FRAMES)
nw_slow:
        ld a,(CDFLAG)
        and 80h
        ret

; -------------------------------------------------------------
; Conexion y peticion HTTP. Pinta el progreso en las filas 4-6.
; -> A = 0 bien (la respuesta en NW_BUF), 1 el modem no contesta, 2 no
; conecta, 3 no hay respuesta HTTP.
; -------------------------------------------------------------
nw_fetch:
        call nw_clear
        call nw_rd              ; si habia una conexion abierta, colgar
        ld a,(nw_st)
        cp 2
        call z,nw_hangup
        call pline
        db 4
        dw s_nw_modem
        ld hl,a_atz             ; ATZ
        call nw_cmd_ok
        jr nz,nwft_1
        ld hl,a_ate0            ; ATE0
        call nw_cmd_ok
        jr nz,nwft_1
        ld hl,s_ok
        call out_str
        ld hl,h_org
        call nw_dial
        jr z,nwft_c
        ld hl,h_net
        call nw_dial
        jr z,nwft_c
        ld a,2
        ret
nwft_1: ld hl,s_nw_noans
        call out_str
        ld a,1
        ret
nwft_c: call nw_clear           ; HEAD / HTTP/1.0 con Host
        ld hl,r_head
        call nw_wr
        ld hl,(nw_host)
        call nw_wr
        ld hl,r_tail
        call nw_wr
        ld hl,t_nocar           ; hasta que el servidor cuelga
        ld (nw_tl),hl
        ld hl,0
        ld (nw_tl+2),hl
        ld hl,nw_tl
        ld bc,500               ; 10 s
        call nw_wait
        or a
        call z,nw_hangup        ; no ha colgado: colgar nosotros
        ld hl,t_http
        call nw_find
        ld a,0
        ret z
        call pline
        db 6
        dw s_nw_nohttp
        ld a,3
        ret

; HL = host (ASCII, 0) -> "DIAL host: CONNECT/NO CARRIER" en la fila 5.
; Z si conecta.
nw_dial:
        ld (nw_host),hl
        push hl
        call pline
        db 5
        dw s_nw_dial
        pop hl
        push hl
        call nw_puts
        ld a,':'
        call nw_char
        xor a
        call out_char
        call nw_clear
        ld hl,a_atdt
        call nw_wr
        pop hl
        call nw_wr
        ld hl,a_port
        call nw_wr
        ld hl,t_connect         ; CONNECT, NO CARRIER o ERROR
        ld (nw_tl),hl
        ld hl,t_nocar
        ld (nw_tl+2),hl
        ld hl,t_error
        ld (nw_tl+4),hl
        ld hl,nw_tl
        ld bc,1000              ; 20 s (DNS + conexion)
        call nw_wait
        cp 1
        ld hl,s_nw_conn
        jr z,nwd1
        ld hl,s_nw_nocar
nwd1:   push af
        call out_str
        pop af
        cp 1
        ret

; HL = comando AT -> Z si contesta OK en 2 s
nw_cmd_ok:
        push hl
        call nw_clear
        pop hl
        call nw_wr
        ld hl,t_ok
        ld (nw_tl),hl
        ld hl,0
        ld (nw_tl+2),hl
        ld hl,nw_tl
        ld bc,100
        call nw_wait
        cp 1
        ret

; Colgar sin perder nada: 1 s de silencio, "+++", 1 s, ATH
nw_hangup:
        ld b,55
        call nw_frames
        ld hl,a_esc
        call nw_wr
        ld b,55
        call nw_frames
        ld hl,a_ath
        call nw_wr
        ld b,25
        jp nw_frames

; -------------------------------------------------------------
; Tubo: NET_READ / NET_WRITE
; -------------------------------------------------------------

; Vacia NW_BUF
nw_clear:
        ld hl,NW_BUF
        ld (nw_ptr),hl
        ld (hl),0
        ret

; NET_READ de hasta 255 bytes, anadidos a NW_BUF (y un 0 detras).
; (nw_st) = estado de la conexion. Carry = timeout.
nw_rd:
        ld a,CMD_NRD
        call m_send
        ret c
        ld a,255
        call m_send
        ret c
        call m_recv             ; cuantos
        ret c
        ld hl,(nw_ptr)
        or a
        jr z,nwr3
        ld b,a
nwr1:   call m_recv
        ret c
        ld (hl),a
        push de
        ld de,NW_END            ; lleno: se descarta lo que sobra
        ex de,hl
        or a
        sbc hl,de
        ex de,hl
        pop de
        jr z,nwr2
        inc hl
nwr2:   djnz nwr1
        ld (nw_ptr),hl
        ld (hl),0
nwr3:   call m_recv             ; pendientes
        ret c
        call m_recv             ; estado
        ret c
        ld (nw_st),a
        or a                    ; NC
        ret

; HL = texto ASCII terminado en 0 -> NET_WRITE; si se acepta menos, se
; reenvia el resto. Carry = timeout (o el MCU no acepta nada).
nw_wr:
        ld a,100
        ld (nw_try),a
nww0:   push hl                 ; B = longitud de lo que queda
        ld b,0
nww1:   ld a,(hl)
        or a
        jr z,nww2
        inc b
        inc hl
        jr nww1
nww2:   pop hl
        ld a,b
        or a
        ret z
        ld a,CMD_NWR
        call m_send
        ret c
        ld a,b
        call m_send
        ret c
        push hl
nww3:   ld a,(hl)
        call m_send
        jr c,nwwx
        inc hl
        djnz nww3
        pop hl
        call m_recv             ; aceptados
        ret c
        ld e,a
        ld d,0
        add hl,de
        call m_recv             ; estado
        ret c
        ld a,e
        or a
        jr nz,nww0
        ld a,(nw_try)           ; nada aceptado: esperar y reintentar
        dec a
        ld (nw_try),a
        scf
        ret z
        ld b,2
        call nw_frames
        jr nww0
nwwx:   pop hl
        ret

; HL = tabla de textos (dw t1, t2..., 0), BC = cuadros -> A = 1..n el
; primero que aparece en NW_BUF, 0 si no aparece a tiempo
nw_wait:
        ld (nw_tab),hl
        ld (nw_left),bc
nwt1:   call nw_rd
        jr c,nwt4
        ld hl,(nw_tab)
        ld c,0
nwt2:   ld e,(hl)
        inc hl
        ld d,(hl)
        inc hl
        ld a,d
        or e
        jr z,nwt3
        inc c
        push hl
        push bc
        ex de,hl
        call nw_find
        pop bc
        pop hl
        jr nz,nwt2
        ld a,c
        ret
nwt3:   call nw_frame
        ld hl,(nw_left)
        dec hl
        ld (nw_left),hl
        ld a,h
        or l
        jr nz,nwt1
nwt4:   xor a
        ret

; HL = texto (0) -> Z si esta en NW_BUF, con HL = donde empieza
nw_find:
        ex de,hl
        ld hl,NW_BUF
nfd1:   ld a,(hl)
        or a
        jr z,nfd4
        push hl
        push de
nfd2:   ld a,(de)
        or a
        jr z,nfd3
        cp (hl)
        jr nz,nfd5
        inc hl
        inc de
        jr nfd2
nfd5:   pop de
        pop hl
        inc hl
        jr nfd1
nfd3:   pop de
        pop hl
        xor a
        ret
nfd4:   or 1
        ret

; Espera un cuadro (FRAMES cambia; lo decrementa la ROM en SLOW). Pisa A, C.
nw_frame:
        ld a,(FRAMES)
        ld c,a
nwfr1:  ld a,(FRAMES)
        cp c
        jr z,nwfr1
        ret

; B = cuadros
nw_frames:
        call nw_frame
        djnz nw_frames
        ret

; -------------------------------------------------------------
; Pantalla
; -------------------------------------------------------------

; A = caracter ASCII -> pantalla (minusculas a mayusculas)
nw_char:
        cp 'a'
        jr c,nwc1
        cp 'z'+1
        jr nc,nwc1
        sub 20h
nwc1:   push hl
        call asc2zx
        call out_char
        pop hl
        ret

; HL = texto ASCII (0) -> pantalla
nw_puts:
        ld a,(hl)
        or a
        ret z
        call nw_char
        inc hl
        jr nw_puts

; B = fila, HL = texto en NW_BUF -> la linea (hasta CR, LF o 0; 32 como
; mucho)
nw_pline:
        push hl
        ld c,0
        call set_at
        pop hl
        ld b,32
nwp1:   ld a,(hl)
        or a
        ret z
        cp 0Dh
        ret z
        cp 0Ah
        ret z
        call nw_char
        inc hl
        djnz nwp1
        ret

; HL -> "MM:SS" ASCII -> HL = MM*60+SS
nw_ms_asc:
        ld a,(hl)
        sub '0'
        ld b,a
        inc hl
        ld a,(hl)
        sub '0'
        inc hl
        inc hl
        push hl
        call nw_2dig            ; A = MM
        pop hl
        push af
        ld a,(hl)
        sub '0'
        ld b,a
        inc hl
        ld a,(hl)
        sub '0'
        call nw_2dig            ; A = SS
        ld e,a
        pop af
        jr nw_ms

; DE -> "MM:SS" en codigos ZX81 -> HL = MM*60+SS
nw_ms_zx:
        ex de,hl
        ld a,(hl)
        sub Z_0
        ld b,a
        inc hl
        ld a,(hl)
        sub Z_0
        inc hl
        inc hl
        push hl
        call nw_2dig
        pop hl
        push af
        ld a,(hl)
        sub Z_0
        ld b,a
        inc hl
        ld a,(hl)
        sub Z_0
        call nw_2dig
        ld e,a
        pop af
; A = minutos, E = segundos -> HL = A*60+E
nw_ms:
        ld l,a
        ld h,0
        add hl,hl
        add hl,hl               ; *4
        push hl
        add hl,hl
        add hl,hl
        add hl,hl
        add hl,hl               ; *64
        pop bc
        or a
        sbc hl,bc               ; *60
        ld d,0
        add hl,de
        ret

; B = decenas, A = unidades -> A = B*10+A
nw_2dig:
        ld c,a
        ld a,b
        add a,a
        add a,a
        add a,b
        add a,a
        add a,c
        ret

; -------------------------------------------------------------
; Ficheros y RTC (copias pequenas de las del modulo SYS)
; -------------------------------------------------------------

; (nw_h) -> 255 bytes del fichero en NW_BUF (ceros si es mas corto) y un
; 0 detras. Carry = timeout.
nw_fread:
        ld a,55                 ; F_READ
        call m_send
        ret c
        ld a,(nw_h)
        call m_send
        ret c
        ld a,255
        call m_send
        ret c
        xor a
        call m_send
        ret c
        ld hl,NW_BUF
        ld b,255
nfr1:   call m_recv
        ret c
        ld (hl),a
        inc hl
        djnz nfr1
        ld (hl),0
        jp m_recv               ; estado

; A = handle -> F_CLOSE. Carry = timeout.
nw_fclose:
        push af
        ld a,57
        call m_send
        pop bc
        ret c
        ld a,b
        call m_send
        ret c
        jp m_recv

; LOAD *RTC ($32 con cadena vacia) -> 22 caracteres ZX81 en nw_rtcb.
; Carry = timeout.
nw_rtc:
        ld a,CMD_RTC            ; un cambio de reloj por byte, como la ROM
        call m_send
        ret c
        xor a                   ; cadena vacia: leer
        call m_send
        ret c
        ld hl,nw_rtcb
        ld b,22
nrt1:   call m_recv
        ret c
        ld (hl),a
        inc hl
        djnz nrt1
        jp m_recv               ; estado

; -------------------------------------------------------------
; Textos
; -------------------------------------------------------------
s_nw_title: db "WIFI MODUL",'E'+80h
s_nw_d1:   db "/MAN/IP.TXT (WRITTEN BY ESP32)",':'+80h
s_nw_noip: db "NO /MAN/IP.TX",'T'+80h
s_nh_title: db "TCP / HTT",'P'+80h
s_nh_d1:   db "ATDT EXAMPLE.ORG:80, HEAD ",'/'+80h
s_nt_title: db "INTERNET TIM",'E'+80h
s_nt_d1:   db "HTTP DATE: VS RTC (MIN:SEC ONLY",')'+80h
s_nt_srv:  db "INTERNET (UTC):",' '+80h
s_nt_rtc:  db "RTC (LOCAL):   ",' '+80h
s_nt_diff: db "DIFFERENCE (S):",' '+80h
s_nt_ntp:  db "OFF: TRY LOAD *NTP (WIFI SYNC",')'+80h
s_nt_nodate: db "NO CONNECTION OR NO DAT",'E'+80h
s_nw_to:   db "MCU TIMEOU",'T'+80h
s_nw_slow: db "RUN IT IN SLOW MOD",'E'+80h
s_nw_modem: db "MODEM (ESP32):",' '+80h
s_nw_noans: db "NOT ANSWERIN",'G'+80h
s_nw_dial: db "DIAL",' '+80h
s_nw_conn: db "CONNEC",'T'+80h
s_nw_nocar: db "NO CARRIE",'R'+80h
s_nw_nohttp: db "NO HTTP REPL",'Y'+80h

s_ipfile:  db "/MAN/IP.TXT",0
h_org:     db "example.org",0
h_net:     db "example.net",0
a_atz:     db "ATZ",13,0
a_ate0:    db "ATE0",13,0
a_atdt:    db "ATDT ",0
a_port:    db ":80",13,0
a_esc:     db "+++",0
a_ath:     db "ATH",13,0
r_head:    db "HEAD / HTTP/1.0",13,10,"Host: ",0
r_tail:    db 13,10,"User-Agent: SD81TEST",13,10
           db "Connection: close",13,10,13,10,0
t_ok:      db "OK",13,0
t_connect: db "CONNECT",0
t_nocar:   db "NO CARRIER",0
t_error:   db "ERROR",0
t_http:    db "HTTP/1.",0
t_date:    db "ate: ",0
t_date2:   db "ATE: ",0
t_ip:      db "IP ADDRESS: ",0
t_fw:      db "FIRMWARE VERSION: ",0

nw_ptr:     dw 0        ; final de lo recibido en NW_BUF
nw_st:      db 0        ; estado de la conexion (NET_READ)
nw_tab:     dw 0        ; nw_wait: tabla de textos y cuadros que quedan
nw_left:    dw 0
nw_tl:      dw 0,0,0,0  ; tabla de textos de nw_wait (hasta 3 y el 0)
nw_try:     db 0        ; nw_wr: reintentos
nw_host:    dw 0        ; host marcado
nw_h:       db 0        ; handle de /MAN/IP.TXT
nw_tp:      dw 0        ; "HH:MM:SS" de la cabecera Date:
nw_diff:    dw 0        ; diferencia con el RTC (s)

; -------------------------------------------------------------
; Buffers del modulo sin valor inicial (no van en el .bin).
; mbss_end tiene que quedar por debajo de $8000.
; -------------------------------------------------------------
mbss:
nw_rtcb      equ mbss   ; "AAAA-MM-DD HH:MM:SS.CC" (codigos ZX81)
mbss_end     equ nw_rtcb+22
