; =============================================================
; SD81TEST.ASM -- Test de hardware del SD81 Booster: NUCLEO
;
; v0.9: menu (en el stub BASIC, con submenus) y una entrada fija por
; prueba en la tabla de saltos del principio: prueba N en USR 22528+3*N.
;   USR 22528 -> test de MEMORIA (paginas del mapper y enrutado por bloques)
;   USR 22531 -> test de MC45 (ejecucion de codigo en los bloques 4 y 5)
;   USR 22534 -> test de la extension de MC45 a los bloques 6 y 7
;   USR 22537 -> estres del mapper (OUT $E7 aleatorios + relectura)
;   USR 22540 -> captura de POKE 2045 (Superfast), medida con FRAMES
;   USR 22543 -> (reservada: interrupciones simuladas, quitada de momento)
;   USR 22546 -> ROMLOCK (con el bloqueo POKE 2045 no hace nada)
;   USR 22549 -> estres del protocolo con el MCU (SETBYTE/GETBYTE)
;   USR 22552 -> informacion de la maquina
;   USR 22555 -> memoria no paginada en los bloques 4-7
;   USR 22558 -> registros del mapper (todas las paginas, bloques 4-7)
;   USR 22561 -> proteccion del bloque 0
;   USR 22564 -> frecuencia de cuadro (VSYNC por segundo del RTC)
;   USR 22567 -> RTC y bateria
;   USR 22570 -> registros de los chips AY
;   USR 22573 -> lectura de la SD (SDBOOST.ROM contra la ROM en memoria)
;   USR 22576 -> escritura de la SD (SAVE, F_SEEK/F_WRITE, DEL)
;   USR 22579 -> paginas de sistema (bloques 0-3), no destructivo
;   Interactivas (el usuario confirma con Y/N; necesitan SLOW):
;   USR 22582 -> sprites por hardware
;   USR 22585 -> patron de borde
;   USR 22588 -> Chroma81 modo 0 (barras de color)
;   USR 22591 -> modos Superfast (texto, HiRes nativo, Spectrum)
;   USR 22594 -> teclado y joystick en vivo
;   USR 22597 -> sonido (tonos AY y SAY)
;   Utilidades para el stub BASIC:
;   USR 22600 -> espera una cifra 0-9 y devuelve su valor
;   USR 22603 -> espera una tecla cualquiera
;   Video 4.x (interactivas):
;   USR 22606 -> juegos de caracteres 128C y 256C
;   USR 22609 -> 70 y 80 columnas (pantalla alternativa)
;   USR 22612 -> scroll fino por filas
;   USR 22615 -> Chroma81 modo 1 (tabla normal y alternativa)
;   USR 22618 -> doble buffer (HiRes, AUTO y MANUAL)
;   Sonido:
;   USR 22621 -> registros del AY del MCU (automatica)
;   USR 22624 -> beeper del modo Spectrum
;   USR 22627 -> reproductor VGM
;   USR 22630 -> generador de efectos PEG
;   USR 22633 -> alta resolucion WRX (en $A000 y en 8-16K)
;   USR 22636 -> reloj de la CPU (bucle en FAST contra el RTC)
;   USR 22639 -> borra los resultados apuntados (utilidad)
;   USR 22642 -> resumen de las pruebas apuntadas; lo guarda en
;                SD81TEST.TXT
;
; ESTRUCTURA: este fichero es el nucleo (tabla de saltos, rutinas comunes,
; textos y variables compartidas), en 22528 ($5800). Las pruebas estan en
; tres modulos que se cargan de uno en uno en MOD_ORG ($6000, el bloque 3
; entero):
;   SD81MEM.BIN (sd81mem.asm): memoria, mapper, ejecucion y puertos
;   SD81SYS.BIN (sd81sys.asm): MCU, RTC, SD, frecuencia de cuadro e info
;   SD81AV.BIN  (sd81av.asm):  sonido, video interactivo y entrada
; Los modulos usan las rutinas del nucleo a traves de su .sym, asi que
; cualquier cambio en el nucleo obliga a reensamblarlos (run_t lo
; detecta y no los ejecuta).
; Pensado para ir creciendo con mas pruebas (puertos mapeados en memoria,
; mapper, Superfast...) y para comprobar el interface en maquinas nuevas
; (TS1500, TS1000, clones...).
;
; Ensamblar con pasmo, primero el nucleo (genera el .sym que incluyen
; los modulos) y despues los modulos:
;   pasmo sd81test.asm SD81TEST.BIN sd81test.sym
;   pasmo sd81mem.asm SD81MEM.BIN
;   pasmo sd81sys.asm SD81SYS.BIN
;   pasmo sd81av.asm SD81AV.BIN
; Cargar/usar: ver README.md (incluye el stub BASIC, SD81TEST.B81).
; =============================================================
; Todo tiene que quedar en los bloques 2-3 (por debajo de $8000): las
; pruebas remapean los bloques 4-7 y machacarian lo que hubiera ahi. El
; nucleo va de 22528 a MOD_ORG (comprobar bss_end en sd81test.sym) y cada
; modulo de MOD_ORG a $8000 (comprobar mbss_end en su .sym). El nucleo no
; empieza en 20480 para dejarle sitio al BASIC: el stub, con su D_FILE y
; sus variables, llega casi a 20480 y se quedaba sin memoria (informe 4).
MOD_ORG equ 6000h       ; donde se cargan los modulos (byte bajo = 0)
MOD_MEM equ 1           ; id de cada modulo (primer byte de su cabecera)
MOD_SYS equ 2
MOD_AV  equ 3

        org 22528

; Tabla de saltos: un punto de entrada fijo por prueba (prueba N en
; USR 22528+3*N). Las pruebas viven en tres modulos (MEM, SYS y AV) que
; el stub BASIC carga en MOD_ORG; cada entrada hace CALL run_t, que saca
; N de la direccion de retorno, comprueba que el modulo cargado es el
; suyo, ejecuta la prueba y apunta su resultado en rep_res (para el
; resumen).
usr_tab:
        call run_t             ; USR 22528 mem_test (MEM)
        call run_t             ; USR 22531 mc45_test (MEM)
        call run_t             ; USR 22534 mc67_test (MEM)
        call run_t             ; USR 22537 ms_test (MEM)
        call run_t             ; USR 22540 pk_test (MEM)
        jp ia_noslow           ; USR 22543 (reservada, devuelve 9999)
        call run_t             ; USR 22546 rl_test (MEM)
        call run_t             ; USR 22549 mu_test (SYS)
        call run_t             ; USR 22552 info_test (SYS)
        call run_t             ; USR 22555 up_test (MEM)
        call run_t             ; USR 22558 rg_test (MEM)
        call run_t             ; USR 22561 b0_test (MEM)
        call run_t             ; USR 22564 fr_test (SYS)
        call run_t             ; USR 22567 rt_test (SYS)
        call run_t             ; USR 22570 ay_test (AV)
        call run_t             ; USR 22573 sr_test (SYS)
        call run_t             ; USR 22576 sw_test (SYS)
        call run_t             ; USR 22579 sp_test (MEM)
        call run_t             ; USR 22582 spr_test (AV)
        call run_t             ; USR 22585 bd_test (AV)
        call run_t             ; USR 22588 ch_test (AV)
        call run_t             ; USR 22591 sf_test (AV)
        call run_t             ; USR 22594 kb_test (AV)
        call run_t             ; USR 22597 so_test (AV)
; Utilidades para el stub BASIC (no son pruebas; las pruebas nuevas van
; al final de la tabla)
        jp menu_key            ; USR 22600: espera una cifra 0-9 -> valor
        jp key_any             ; USR 22603: espera una tecla cualquiera
        call run_t             ; USR 22606 cs_test (AV)
        call run_t             ; USR 22609 wd_test (AV)
        call run_t             ; USR 22612 fs_test (AV)
        call run_t             ; USR 22615 c1_test (AV)
        call run_t             ; USR 22618 db_test (AV)
        call run_t             ; USR 22621 ma_test (AV)
        call run_t             ; USR 22624 bp_test (AV)
        call run_t             ; USR 22627 vg_test (AV)
        call run_t             ; USR 22630 pg_test (AV)
        call run_t             ; USR 22633 wx_test (AV)
        call run_t             ; USR 22636 ck_test (SYS)
        jp rep_clear           ; USR 22639: borra los resultados apuntados
        call run_t             ; USR 22642 rp_test (SYS): resumen

; Entrada comun de las pruebas (CALL run_t desde la tabla): N = (retorno
; - usr_tab - 3) / 3. Busca en mt_tab su modulo y su numero dentro de el;
; si en MOD_ORG esta ese modulo, ensamblado con este nucleo (la cabecera
; lleva su bss_end), ejecuta su entrada (MOD_ORG+3+3*n); si no, lo dice y
; da 9999. El resultado (BC) se apunta en rep_res[N] y se devuelve.
run_t:
        pop hl
        ld de,-(usr_tab+3)
        add hl,de               ; HL = 3*N
        ld a,l
        ld b,0FFh
rt0:    inc b
        sub 3
        jr nc,rt0
        ld a,b
        ld (rep_t),a
        ld hl,mt_tab
        add a,l
        ld l,a
        jr nc,rtm1
        inc h
rtm1:    ld a,(hl)
        ld c,a
        ld hl,MOD_ORG
        rlca
        rlca
        rlca
        and 7
        cp (hl)
        jr nz,rm_bad
        inc hl
        ld a,(hl)
        cp bss_end-bss_end/256*256
        jr nz,rm_bad
        inc hl
        ld a,(hl)
        cp bss_end/256
        jr nz,rm_bad
        ld a,c
        and 1Fh
        ld l,a
        add a,a
        add a,l
        add a,3
        ld l,a
        ld h,MOD_ORG/256
        call rt_hl              ; la prueba: BC = resultado
rm_rec: ld a,(rep_t)            ; rep_res[N] = BC
        ld l,a
        ld h,0
        add hl,hl
        ld de,rep_res
        add hl,de
        ld (hl),c
        inc hl
        ld (hl),b
        ret
rm_bad: call pline
        db 2
        dw s_nomod
        ld bc,9999
        jr rm_rec
rt_hl:  jp (hl)

; Borra los resultados apuntados (FFFFh = no ejecutada)
rep_clear:
        ld hl,rep_res
        ld b,REP_N*2
rclr1:    ld (hl),0FFh
        inc hl
        djnz rclr1
        ret

; modulo*32 + numero dentro del modulo de cada prueba N (0 = no es de
; ningun modulo: la entrada no pasa por run_t)
mt_tab:
        db MOD_MEM*32+0
        db MOD_MEM*32+1
        db MOD_MEM*32+2
        db MOD_MEM*32+3
        db MOD_MEM*32+4
        db 0                 ; 5
        db MOD_MEM*32+5
        db MOD_SYS*32+0
        db MOD_SYS*32+1
        db MOD_MEM*32+6
        db MOD_MEM*32+7
        db MOD_MEM*32+8
        db MOD_SYS*32+2
        db MOD_SYS*32+3
        db MOD_AV*32+0
        db MOD_SYS*32+4
        db MOD_SYS*32+5
        db MOD_MEM*32+9
        db MOD_AV*32+1
        db MOD_AV*32+2
        db MOD_AV*32+3
        db MOD_AV*32+4
        db MOD_AV*32+5
        db MOD_AV*32+6
        db 0                 ; 24
        db 0                 ; 25
        db MOD_AV*32+7
        db MOD_AV*32+8
        db MOD_AV*32+9
        db MOD_AV*32+10
        db MOD_AV*32+11
        db MOD_AV*32+12
        db MOD_AV*32+13
        db MOD_AV*32+14
        db MOD_AV*32+15
        db MOD_AV*32+16
        db MOD_SYS*32+6
        db 0                 ; 37
        db MOD_SYS*32+7
REP_N   equ 39              ; entradas de la tabla con resultado



; -------------------------------------------------------------
; Teclado. En BASIC, un bucle con INKEY$ en SLOW da una vuelta cada
; muchos milisegundos y se come las pulsaciones cortas: estas rutinas
; leen el teclado sin parar. (Leer el puerto FE en SLOW no molesta al
; video: con el generador de NMI encendido no provoca VSYNC.)
; -------------------------------------------------------------

; Espera a que se pulse una cifra (0-9) y a que se suelten todas las
; teclas -> BC = la cifra. Si al entrar ya habia una cifra pulsada (p.ej.
; mientras el BASIC pintaba el menu), la acepta.
menu_key:
        ld a,0F7h               ; semifila 1 2 3 4 5 (bit 0 = 1)
        in a,(0FEh)
        cpl
        and 1Fh
        jr z,mk2
        ld c,1
mk1:    rrca
        jr c,mk4
        inc c
        jr mk1
mk2:    ld a,0EFh               ; semifila 0 9 8 7 6 (bit 0 = 0)
        in a,(0FEh)
        cpl
        and 1Fh
        jr z,menu_key
        ld c,10
mk3:    rrca
        jr c,mk4
        dec c
        jr mk3
mk4:    ld a,c                  ; 10 -> 0
        cp 10
        jr nz,mk5
        ld c,0
mk5:    ld b,0
        push bc
        call key_release
        pop bc
        ret

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

; Espera a que no haya ninguna tecla pulsada
key_release:
        xor a
        in a,(0FEh)
        and 1Fh
        cp 1Fh
        jr nz,key_release
        ret

; Espera una tecla cualquiera y a que se suelte
key_any:
        call key_release
ka1:    xor a
        in a,(0FEh)
        and 1Fh
        cp 1Fh
        jr z,ka1
        jp key_release

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
FRAMES  equ 4034h       ; variable de sistema FRAMES (16436)
MARGIN  equ 4028h
RAMTOP  equ 4004h
ROMVER  equ 2004h       ; byte de version de la ROM del interface
DETBLK  equ 5           ; bloque para detectar la paginacion (siempre libre)
CMD_RTC equ 32h         ; LOAD *RTC: fecha/hora como texto ZX81
CMD_BAT equ 34h         ; LOAD *BAT: tension de la bateria del RTC
CDFLAG  equ 403Bh       ; bit 7 = video encendido (SLOW)

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

s_ok equ s_ok_sp+1    ; comparte texto

; =============================================================
; Utilidades comunes de las pruebas
; =============================================================

; HL = nombre de la prueba -> fila 0: "SD81 TEST - " + nombre + " V0.9"
; (la version solo esta en s_tver). Conserva BC.
title:
        push bc
        push hl
        ld bc,0
        call set_at
        ld hl,s_tpre
        call out_str
        pop hl
        call out_str
        ld hl,s_tver
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

; Llamadas con los datos detras del CALL (ahorran los LD de cada linea):
;   call pline  / db fila / dw texto                -> como line
;   call plines / db fila / dw texto ... / db 0FFh  -> varias lineas
;   call plnum  / db fila / dw texto, variable      -> como line_num con
;                                                      DE = (variable)
; Dejan B = fila (la ultima) y el cursor detras; conservan C. pline y
; plines conservan DE; plnum deja DE = valor.
pline:
        ex (sp),hl
        call pl_one
        ex (sp),hl
        ret
plines:
        ex (sp),hl
pls1:   call pl_one
        ld a,(hl)
        inc a
        jr nz,pls1
        inc hl
        ex (sp),hl
        ret
plnum:
        ex (sp),hl
        call pl_one
        ld e,(hl)
        inc hl
        ld d,(hl)
        inc hl
        ex (sp),hl
        ex de,hl
        ld e,(hl)
        inc hl
        ld d,(hl)
        ex de,hl
        call out_dec16
        ex de,hl
        ret
; HL -> db fila / dw texto: lo pinta y deja HL detras. B = fila.
pl_one:
        ld b,(hl)
        inc hl
        ld a,(hl)
        inc hl
        push hl
        ld h,(hl)
        ld l,a
        call line
        pop hl
        inc hl
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
wd1:    call in_clk
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

; Lee el puerto $AF y suma a vs_total los VSYNC que trae (D6-D1).
; Conserva todo menos A (que queda con la lectura).
in_clk:
        in a,(CLKPORT)
vs_acc: push af
        push hl
        rrca
        and 3Fh
        ld hl,(vs_total)
        add a,l
        ld l,a
        jr nc,va1
        inc h
va1:    ld (vs_total),hl
        pop hl
        pop af
        ret

; =============================================================
; Protocolo "un byte, un cambio de reloj" (el del explorador), con limite
; de tiempo: sirve para los comandos de fichero (F_OPEN, F_READ...), SAVE
; y DEL, en los que el MCU confirma cada byte con un cambio de reloj.
; Carry = timeout.
; =============================================================

; A = byte -> lo manda. Conserva BC, DE, HL.
m_send:
        push bc
        ld b,a
        call in_clk
        ld c,a
        ld a,b
        call out_wdiff
        pop bc
        ret

; -> A = byte recibido. Conserva BC, DE, HL.
m_recv:
        push bc
        call in_clk
        ld c,a
        in a,(DATAPORT)
        ld b,a
        call wait_diff
        ld a,b
        pop bc
        ret

; HL = nombre ASCII terminado en 0 -> manda longitud y caracteres. Con
; E = 1 los pasa a codigo ZX81 (SAVE, DEL), con E = 0 van en ASCII
; (F_OPEN). Carry = timeout.
send_name:
        push hl
        ld b,0
sn1:    ld a,(hl)
        or a
        jr z,sn2
        inc b
        inc hl
        jr sn1
sn2:    pop hl
        ld a,b
        call m_send
        ret c
sn3:    ld a,(hl)
        or a
        ret z
        push hl
        bit 0,e
        call nz,asc2zx
        call m_send
        pop hl
        ret c
        inc hl
        jr sn3

; A = comando, HL = nombre, E = 1 ZX81 / 0 ASCII -> A = respuesta de un
; byte (handle o estado). Carry = timeout.
cmd_name:
        call m_send
        ret c
        call send_name
        ret c
        jp m_recv

ia_noslow:
        ld bc,9999
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

; A = codigo ZX81 en el cursor, y avanza. Solo pisa A (y el cursor).
out_char:
        push hl
        ld hl,(cur_ptr)
        ld (hl),a
        inc hl
        ld (cur_ptr),hl
        pop hl
        ret

; HL = cadena ASCII con el bit 7 del ultimo caracter a 1 (los nombres
; que van al MCU, en cambio, terminan en 0). Pisa A y HL.
out_str:
        ld a,(hl)
        push af
        and 7Fh
        push hl
        call asc2zx
        call out_char
        pop hl
        inc hl
        pop af
        rla
        jr nc,out_str
        ret

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
s_nomod:   db "MODULE MISSING OR OUTDATE",'D'+80h
s_tpre:    db "SD81 TEST -",' '+80h
s_tver:    db " V0.",'9'+80h
s_mc_res:  db "RESULT:",' '+80h
s_fail:    db "FAI",'L'+80h
s_ok_sp:   db " O",'K'+80h
s_m67_mirror: db "6/7 MIRROR 2/3: LOAD *RAM4",'8'+80h

; -------------------------------------------------------------
; Variables
; -------------------------------------------------------------
npages:     db 0        ; 32 o 64
first_done: db 0        ; 1 = ya se guardo el primer error
fe_addr:    dw 0
cur_ptr:    dw 0        ; cursor de escritura en D_FILE
err_a:      dw 0        ; contadores de errores de las pruebas
err_b:      dw 0
vs_total:   dw 0        ; VSYNC acumulados de las lecturas de $AF

; -------------------------------------------------------------
; Buffers sin valor inicial: no ocupan sitio en el .bin (se rellenan
; antes de usarlos). bss_end tiene que quedar por debajo de MOD_ORG.
; -------------------------------------------------------------
bss:
saved_map    equ bss   ; pagina de cada bloque al empezar
rep_res      equ saved_map+8   ; resultado de cada prueba (FFFFh = no ejecutada)
rep_t        equ rep_res+REP_N*2   ; N de la prueba en curso
bss_end      equ rep_t+1
