; =====================================================================
;  TELNET.ASM - terminal telnet para el ZX81 con SD81 Booster y modulo
;  WiFi. 80x24 en Superfast (caracteres de 7 pixeles), fuente CP437 de
;  256 caracteres, colores ANSI (Chroma81 modo 1).
;
;  Esta hecho a partir del terminal de CP/M Plus (CPM3_SD81):
;    - term.z80: el bucle (NET_READ de hasta 128 bytes, pintar, mirar el
;      teclado, NET_WRITE de cada tecla) y las teclas de control.
;    - chario.z80: el emulador de terminal (ADM-3A + ANSI: CUP, CUU/CUD/
;      CUF/CUB, ED, EL, SGR, SCP/RCP, DSR), el teclado del ZX81 (SHIFT,
;      ENTER+tecla = simbolos, SHIFT+ENTER = modo CTRL) y el cursor
;      parpadeante. Copiado SIN cambios de logica; en CP/M esto es el
;      CONOUT/CONIN del BIOS, aqui se llama directamente.
;    - system.z80: la puesta en marcha del video.
;    - font_image.z80: la fuente (font.inc).
;  Lo nuevo: la negociacion telnet (IAC). Ni el ESP32 ni el TERM de
;  CP/M la tratan, y muchas BBS la mandan al conectar; sin filtrarla
;  salen bytes raros en pantalla. Se aceptan ECHO y SUPPRESS-GO-AHEAD (lo
;  que espera una BBS: que el eco lo haga ella) y se rechaza lo demas;
;  las subnegociaciones se saltan enteras.
;
;  Conexion: como con un modem Hayes, tecleando en la propia terminal
;      ATDT host:puerto        (p.ej. ATDT bbs.ejemplo.org:23)
;  El ESP32 contesta CONNECT o NO CARRIER; ATH cuelga, +++ vuelve a
;  modo comando sin colgar. Ver claude/planning/net_bridge_emulator.md.
;
;  Teclas:
;      ENTER+0          salir (cuelga si hay conexion)
;      ENTER+9          eco local si/no (para cuando el otro lado no
;                       hace eco; arranca apagado)
;      ENTER+8          colores: blanco sobre negro (al arrancar), verde
;                       sobre negro, amarillo sobre azul
;      SHIFT+1          ESC
;      SHIFT+ENTER, tecla   CTRL+tecla
;      ENTER+tecla      simbolos (los serigrafiados y @ \ | ~ ` { } [ ] _ ...)
;      SHIFT+0 / SHIFT+9    DEL / BS
;      SHIFT+5/6/7/8    flechas: mandan ESC [ D/B/A/C (ANSI); en CP/M son
;                       las de WordStar (^S ^X ^E ^D)
;  Con el eco local, los codigos de control se ven como ^X.
;
;  Memoria (vale en modo 32K y 48K):
;      $6000-          este programa (y la fuente, que se copia arriba)
;      $8000-$8798     pantalla (1 + 24 filas de 81 bytes)
;      $8800-$8F98     atributos, misma geometria
;      $9000-$97FF     fuente (el modo de 256 caracteres pide 2 KB
;                      alineados: I = $90)
;  Machaca lo que hubiera en esas direcciones de los bloques 4 y 5.
;
;  Ensamblar: pasmo telnet.asm TELNET.BIN (compila.bat)
;  Cargar: stub BASIC TELNET.B81 (CLEAR 24575, LOAD FAST ... CODE 24576,
;  RAND USR 24576).
; =====================================================================

; --- ROM y sistema ---------------------------------------------------
SET_FAST    equ 02E7h           ; ROM: apaga el video si estaba en SLOW
SLOW_FAST   equ 0207h           ; ROM: vuelve al modo que pide CDFLAG

; --- MCU -------------------------------------------------------------
DataPort    equ 0A7h
ClkPort     equ 0AFh
CMD_NET_RD  equ 66
CMD_NET_WR  equ 67
CMD_64C     equ 1Ch             ; SEL_64CHARS
CMD_128C    equ 1Bh             ; SEL_128CHARS
CMD_256C    equ 41h             ; SEL_256CHARS

; --- video -----------------------------------------------------------
DFILE       equ 8000h           ; pantalla (DFILE_OVR)
ATTR        equ 8800h           ; atributos (ATTR_OVR)
ATTR_OFF    equ ATTR-DFILE
FONT_ADDR   equ 9000h
FONT_I      equ FONT_ADDR/256
SCRW        equ 80
SCRH        equ 24
ROWSTRIDE   equ 81              ; 80 caracteres + 1 byte de relleno
; Colores: atributo PPPP IIII (papel arriba, tinta abajo; en cada nibble
; brillo, verde, rojo, azul). El color por defecto sale de scheme_tab y
; se cambia con ENTER+8.
CHROMA_PORT equ 07FEFh
CHROMA_BASE equ 030h            ; color + modo 1 (atributos); + el borde
CHROMA_OFF  equ 00Ch
CURSOR_BLOCK equ 0DBh           ; bloque solido CP437 (cursor normal)
CURSOR_UNDER equ 05Fh           ; guion bajo (cursor en modo CTRL)
BLINK_RATE  equ 25              ; VSYNC por semiciclo de parpadeo

; --- terminal --------------------------------------------------------
RXMAX       equ 128             ; lo que cabe en un solo NET_READ
EXITKEY     equ 01Dh            ; ^]  (ENTER+0)
ECHOKEY     equ 01Ch            ; ^\  (ENTER+9)
COLORKEY    equ 01Eh            ; ^^  (ENTER+8)
; Flechas (SHIFT+7/6/8/5): codigos internos que el bucle convierte en las
; secuencias ANSI ESC [ A/B/C/D, que es lo que entienden las BBS. En CP/M
; son ^E ^X ^D ^S (WordStar), que aqui no le sirven a nadie.
KEY_UP      equ 081h            ; -> ESC [ A
KEY_DOWN    equ 082h            ; -> ESC [ B
KEY_RIGHT   equ 083h            ; -> ESC [ C
KEY_LEFT    equ 084h            ; -> ESC [ D

; --- telnet ----------------------------------------------------------
T_SE        equ 0F0h
T_SB        equ 0FAh
T_WILL      equ 0FBh
T_WONT      equ 0FCh
T_DO        equ 0FDh
T_DONT      equ 0FEh
T_IAC       equ 0FFh
T_ECHO      equ 1
T_SGA       equ 3

            org  24576

; =====================================================================
;  Arranque: video en 80 columnas, fuente, y a la terminal
; =====================================================================
start:      ld   a,i
            ld   (sv_i),a
            call SET_FAST           ; sin NMI: el video lo da la FPGA

            ld   hl,vars            ; variables a cero
            ld   de,vars+1
            ld   bc,VARS_LEN-1
            ld   (hl),0
            ldir
            ld   a,(scheme_tab)     ; esquema de color 0
            ld   (def_attr),a
            ld   (cur_attr),a

            ld   hl,FONT_ADDR       ; fuente: codigos 0-31 a cero (no se
            ld   de,FONT_ADDR+1     ; imprimen) y 32-255 de font.inc
            ld   bc,255
            ld   (hl),0
            ldir
            ld   hl,font_src
            ld   de,FONT_ADDR+100h
            ld   bc,FONT_LEN
            ldir
            ld   a,FONT_I
            ld   i,a

            call cls                ; pantalla y atributos, antes de verlos
            ld   hl,DFILE           ; pantalla y atributos alternativos
            ld   (2096),hl
            ld   a,170
            ld   (2098),a
            ld   hl,ATTR
            ld   (2059),hl
            ld   a,170
            ld   (2061),a
            ld   bc,CHROMA_PORT
            ld   a,(scheme_tab+1)   ; borde del esquema 0
            or   CHROMA_BASE
            out  (c),a
            xor  a
            ld   (2094),a           ; sin desplazar ni recortar
            ld   (2095),a
            ld   a,174              ; Superfast texto, 80 columnas
            ld   (2045),a
            ld   a,CMD_256C
            call mcu_cmd1

            ld   hl,msg_on
            call puts

            ld   hl,rxbuf           ; sin conexion: eco del modem encendido
            ld   b,0                ; (ATE1), por si alguien lo dejo en ATE0,
            call net_read           ; y se tira la respuesta
            ld   a,(net_st)
            cp   2
            jr   z,loop
            ld   hl,at_e1
            ld   b,AT_E1_LEN
            call net_write
            ld   b,25
            call wait_vs
            ld   b,8
drain:      push bc
            ld   hl,rxbuf
            ld   b,RXMAX
            call net_read
            pop  bc
            djnz drain

; ---------------------------------------------------------------------
;  Bucle principal (term.z80): hasta 128 bytes de una, pintarlos (por el
;  filtro telnet), y mirar el teclado -- sin bloquear en ninguna
;  direccion.
; ---------------------------------------------------------------------
loop:       ld   hl,rxbuf
            ld   b,RXMAX
            call net_read           ; B = cuantos ha traido de verdad
            ld   a,b
            or   a
            jr   z,do_kbd
            ld   hl,rxbuf
pr_loop:    ld   a,(hl)
            push hl
            push bc
            call rx_byte
            pop  bc
            pop  hl
            inc  hl
            djnz pr_loop

do_kbd:     call con_cist           ; hay tecla?
            or   a
            jr   z,loop
            call kbget              ; si: leerla
            cp   EXITKEY
            jp   z,done
            cp   ECHOKEY
            jr   z,do_echo
            cp   COLORKEY
            jp   z,do_color
            cp   KEY_UP             ; una flecha?
            jr   c,kb_send
            cp   KEY_LEFT+1
            jr   nc,kb_send
            sub  KEY_UP-'A'         ; KEY_UP..KEY_LEFT -> 'A'..'D'
            ld   (arrow_seq+2),a
            ld   hl,arrow_seq       ; ESC [ letra
            ld   b,3
            call net_write
            jp   loop
kb_send:    ld   (txbyte),a
            ld   hl,txbyte
            ld   b,1
            call net_write
            ld   a,(localecho)
            or   a
            jp   z,loop
            ld   a,(txbyte)         ; y, si toca, pintarla aqui tambien
            call echo_char
            jp   loop

; A = tecla mandada -> eco local. Al otro lado va solo el CR, pero en
; pantalla hace falta tambien el LF. Los codigos de control no pasan por
; el emulador de terminal (los ignoraria, y un ESC empezaria una
; secuencia): se ensenan como ^X, igual que en los terminales de siempre.
; BS y TAB si se ejecutan.
echo_char:  cp   00Dh
            jr   nz,ec1
            ld   c,a
            call con_co
            ld   c,00Ah
            jp   con_co
ec1:        cp   008h
            jr   z,ec_out
            cp   009h
            jr   z,ec_out
            cp   07Fh               ; DEL -> ^?
            jr   z,ec_ctl
            cp   020h
            jr   nc,ec_out
ec_ctl:     push af
            ld   c,'^'
            call con_co
            pop  af
            xor  040h               ; ^A..^_ y ^? (7Fh xor 40h = 3Fh)
ec_out:     ld   c,a
            jp   con_co

; --- conmutar el eco local, y decir como ha quedado -------------------
do_echo:    ld   a,(localecho)
            xor  1
            ld   (localecho),a
            ld   hl,msg_eco_on
            or   a
            jr   nz,de_say
            ld   hl,msg_eco_off
de_say:     call puts
            jp   loop

; --- siguiente esquema de color: repinta lo que tenia el color por
; defecto (lo que ha pintado la BBS con sus colores se queda) -----------
do_color:   ld   a,(scheme)
            inc  a
            cp   N_SCHEMES
            jr   c,dc1
            xor  a
dc1:        ld   (scheme),a
            add  a,a
            ld   e,a
            ld   d,0
            ld   hl,scheme_tab
            add  hl,de
            ld   d,(hl)             ; D = color nuevo
            inc  hl
            ld   a,(hl)             ; borde
            or   CHROMA_BASE
            ld   bc,CHROMA_PORT
            out  (c),a
            ld   a,(def_attr)
            ld   e,a                ; E = color viejo
            ld   hl,ATTR
            ld   bc,1+ROWSTRIDE*SCRH
dc2:        ld   a,(hl)
            cp   e
            jr   nz,dc3
            ld   (hl),d
dc3:        inc  hl
            dec  bc
            ld   a,b
            or   c
            jr   nz,dc2
            ld   a,(cur_attr)
            cp   e
            jr   nz,dc4
            ld   a,d
            ld   (cur_attr),a
dc4:        ld   a,d
            ld   (def_attr),a
            jp   loop

; Esquemas de color: atributo por defecto y borde (0-7, GRB)
scheme_tab: defb 00Fh,0             ; blanco brillante sobre negro (al arrancar)
            defb 00Ch,0             ; verde brillante sobre negro
            defb 01Eh,1             ; amarillo brillante sobre azul (CP/M)
N_SCHEMES   equ 3

; --- salir: colgar si hay conexion y devolver el video al BASIC -------
done:       ld   hl,msg_off
            call puts
            ld   hl,rxbuf
            ld   b,0
            call net_read           ; estado de la conexion
            ld   a,(net_st)
            cp   2
            jr   nz,dn1
            ld   b,55               ; +++ con 1 s de silencio antes y despues,
            call wait_vs            ; y ATH
            ld   hl,at_esc
            ld   b,3
            call net_write
            ld   b,55
            call wait_vs
            ld   hl,at_h
            ld   b,AT_H_LEN
            call net_write
            ld   b,10
            call wait_vs
dn1:        ld   a,85               ; video nativo (apaga tambien la pantalla
            ld   (2045),a           ; y los atributos alternativos)
            ld   bc,CHROMA_PORT
            ld   a,CHROMA_OFF
            out  (c),a
            ld   a,(sv_i)           ; el modo de caracteres de antes
            cp   3Ch
            ld   b,CMD_128C
            jr   z,dn2
            cp   38h
            ld   b,CMD_256C
            jr   z,dn2
            ld   b,CMD_64C
dn2:        ld   a,b
            call mcu_cmd1
            ld   a,(sv_i)
            ld   i,a
            call SLOW_FAST
            ld   bc,0
            ret

; =====================================================================
;  Telnet: filtro de IAC. A = byte recibido; lo que no es negociacion
;  se pinta. Estados (tn_state): 0 normal, 1 detras de IAC, 2 esperando
;  la opcion de WILL/WONT/DO/DONT, 3 dentro de una subnegociacion, 4 IAC
;  dentro de ella. Respuestas: WILL ECHO/SGA -> DO (el eco lo hace el
;  servidor), DO SGA -> WILL; cualquier otro WILL -> DONT y cualquier
;  otro DO -> WONT. WONT/DONT no se contestan.
; =====================================================================
rx_byte:    ld   c,a
            ld   a,(tn_state)
            or   a
            jr   nz,tn_seq
            ld   a,c
            cp   T_IAC
            jp   nz,con_co          ; lo normal: al terminal
            ld   a,1
            ld   (tn_state),a
            ret
tn_seq:     dec  a
            jr   nz,tn_s2
            ld   a,c                ; estado 1: el comando
            cp   T_IAC
            jr   nz,tn1
            xor  a                  ; IAC IAC = el byte 255
            ld   (tn_state),a
            jp   con_co
tn1:        cp   T_SB
            jr   nz,tn2
            ld   a,3
            ld   (tn_state),a
            ret
tn2:        cp   T_WILL
            jr   c,tn_end           ; NOP, GA...: comandos de 2 bytes
            ld   (tn_cmd),a
            ld   a,2
            ld   (tn_state),a
            ret
tn_s2:      dec  a
            jr   nz,tn_s3
            ld   a,c                ; estado 2: la opcion
            ld   (tn_opt),a
            xor  a
            ld   (tn_state),a
            ld   a,(tn_cmd)
            cp   T_WILL
            jr   nz,tn_do
            ld   a,(tn_opt)
            cp   T_ECHO
            jr   z,tn_rdo
            cp   T_SGA
            jr   z,tn_rdo
            ld   a,T_DONT
            jr   tn_send
tn_rdo:     ld   a,T_DO
            jr   tn_send
tn_do:      cp   T_DO
            ret  nz                 ; WONT/DONT: no se contestan
            ld   a,(tn_opt)
            cp   T_SGA
            ld   a,T_WILL
            jr   z,tn_send
            ld   a,T_WONT
tn_send:    ld   (tn_resp+1),a
            ld   a,(tn_opt)
            ld   (tn_resp+2),a
            ld   hl,tn_resp
            ld   b,3
            jp   net_write
tn_s3:      dec  a
            jr   nz,tn_s4
            ld   a,c                ; estado 3: dentro de SB, hasta IAC
            cp   T_IAC
            ret  nz
            ld   a,4
            ld   (tn_state),a
            ret
tn_s4:      ld   a,c                ; estado 4: IAC SE termina; otra cosa
            cp   T_SE               ; (IAC IAC) sigue dentro
            ld   a,3
            jr   nz,tn_st
tn_end:     xor  a
tn_st:      ld   (tn_state),a
            ret

; --- utilidades ------------------------------------------------------

; HL = texto terminado en 0 -> al terminal
puts:       ld   a,(hl)
            or   a
            ret  z
            push hl
            ld   c,a
            call con_co
            pop  hl
            inc  hl
            jr   puts

; B = VSYNC que esperar (los cuenta el puerto $AF: bits 6-1, desde la
; ultima lectura). Vale en FAST: el video lo genera la FPGA.
wait_vs:    in   a,(ClkPort)
            and  07Eh
            rrca
            ld   c,a
            ld   a,b
            sub  c
            ret  c
            ret  z
            ld   b,a
            jr   wait_vs

; A = comando MCU sin parametros ni respuesta: se manda y se espera a que
; el MCU cambie el reloj.
mcu_cmd1:   ld   b,a
            in   a,(ClkPort)
            ld   c,a
            ld   a,b
            out  (DataPort),a
mcc1:       in   a,(ClkPort)
            xor  c
            jp   p,mcc1
            ret

; ---------------------------------------------------------------------
;  net_read  - lee hasta B bytes en (HL). Devuelve B = leidos; el estado
;              de la conexion en (net_st).
;  net_write - envia B bytes desde (HL). Devuelve B = aceptados.
;  Como las de term.z80: C es el reloj del handshake y A no sobrevive a
;  mcu_send.
; ---------------------------------------------------------------------
net_read:   push hl
            ld   d,b
            call sd_clk
            ld   a,CMD_NET_RD
            call mcu_send
            ld   a,d
            call mcu_send           ; max
            call mcu_recv
            ld   b,a                ; count
            ld   d,a                ; copia a salvo (C es el reloj)
            or   a
            jr   z,nr_tail
nr_loop:    call mcu_recv
            ld   (hl),a
            inc  hl
            djnz nr_loop
nr_tail:    call mcu_recv           ; pendientes
            call mcu_recv           ; estado
            ld   (net_st),a
            ld   b,d
            pop  hl
            ret

net_write:  call sd_clk
            ld   a,CMD_NET_WR
            call mcu_send
            ld   a,b
            call mcu_send           ; count
            ld   a,b                ; RECARGAR: mcu_send se ha comido A
            or   a
            jr   z,nw_tail
nw_loop:    ld   a,(hl)
            call mcu_send
            inc  hl
            djnz nw_loop
nw_tail:    call mcu_recv           ; aceptados
            ld   b,a
            call mcu_recv           ; estado
            ld   (net_st),a
            ret

sd_clk:     in   a,(ClkPort)
            ld   c,a
            ret

mcu_send:   out  (DataPort),a
            ld   a,c
            cpl
            ld   c,a
ms_w:       in   a,(ClkPort)
            xor  c
            jp   m,ms_w
            ret

mcu_recv:   in   a,(DataPort)
            push af
            ld   a,c
            cpl
            ld   c,a
mr_w:       in   a,(ClkPort)
            xor  c
            jp   m,mr_w
            pop  af
            ret

; =====================================================================
;  Emulador de terminal y teclado: chario.z80 de CPM3_SD81, sin cambios
;  de logica. con_cist = ?cist (consola), kbget = ?ci, con_co = ?co
;  (C = caracter).
; =====================================================================
con_cist:                           ; A=0FFh si hay tecla, 0 si no
            ld   a,(ab_cnt)         ; respuesta a un DSR pendiente?
            or   a
            jr   nz,cst_yes         ; cuenta como tecla disponible
            call scan_keys          ; A=indice/0FFh, E: bit0=SHIFT bit1=ENTER
            cp   0FFh
            jr   nz,cst_yes         ; hay tecla normal
            bit  1,e                ; ENTER pulsado?
            jr   nz,cst_ent
            xor  a                  ; ENTER suelto: limpiar el residual
            ld   (enter_used),a
            jr   cst_cur
cst_ent:    ld   a,(enter_used)     ; ENTER residual (ya dio simbolo)?
            or   a
            jr   z,cst_yes          ; no -> hay tecla (producira CR/CTRL)
cst_cur:    push de
            call cursor_blink_poll  ; sin tecla util: hacer avanzar el parpadeo
            pop  de
            xor  a
            ret
cst_yes:    ld   a,0FFh
            ret

con_co:
            call cursor_clear       ; borrar el cursor antes de imprimir
            ld   a,(term_state)
            or   a
            jr   nz,co_seq          ; en medio de una secuencia ESC
            ld   a,c
            cp   01Bh               ; ESC
            jr   z,co_esc
            cp   01Ah               ; ^Z  -> borrar pantalla + home
            jp   z,cls
            cp   0Dh
            jr   z,co_cr
            cp   0Ah
            jr   z,co_lf
            cp   08h
            jr   z,co_bs
            cp   09h
            jr   z,co_tab
            cp   20h
            ret  c                  ; otros controles -> ignorar
            jp   print_glyph
co_esc:     ld   a,1
            ld   (term_state),a     ; estado 1: recibido ESC
            ret
co_cr:      xor  a
            ld   (cur_col),a
            ret
co_lf:      jp   vtnl
co_bs:      ld   a,(cur_col)
            or   a
            ret  z
            dec  a
            ld   (cur_col),a
            ret
co_tab:     ld   a,(cur_col)        ; TAB: a la siguiente columna multiplo
            and  0F8h               ; de 8 (no esta en chario.z80: alli lo
            add  a,8                ; expande el BDOS)
            cp   SCRW
            jr   c,ct_ok
            ld   a,SCRW-1
ct_ok:      ld   (cur_col),a
            ret
; --- secuencia ESC '=' fila col (gotoxy ADM-3A, offset $20) ---
co_seq:     cp   4
            jp   z,ansi_char        ; estado 4: dentro de ESC [
            dec  a
            jr   nz,co_seq23        ; estado != 1
            ld   a,c                ; estado 1: '=' (ADM-3A) o '[' (ANSI)
            cp   '='
            jr   z,co_adm3a
            cp   '['
            jr   z,co_ansi
            jr   co_abort           ; otra secuencia -> ignorar
co_adm3a:   ld   a,2
            ld   (term_state),a
            ret
co_ansi:    xor  a                  ; empieza ESC [ : parametros a cero
            ld   (ansi_n),a
            ld   (ansi_cnt),a
            ld   (ansi_priv),a
            ld   a,4
            ld   (term_state),a
            ret
co_seq23:   dec  a
            jr   nz,co_seqcol       ; estado 3 (columna)
            ld   a,c                ; estado 2: fila
            sub  020h
            cp   SCRH
            jr   c,co_rowok
            ld   a,SCRH-1
co_rowok:   ld   (cur_row),a
            ld   a,3
            ld   (term_state),a
            ret
co_seqcol:  ld   a,c                ; estado 3: columna
            sub  020h
            cp   SCRW
            jr   c,co_colok
            ld   a,SCRW-1
co_colok:   ld   (cur_col),a
co_abort:   xor  a
            ld   (term_state),a     ; fin de secuencia
            ret

; ---------------------------------------------------------------------
;  ansi_char - un byte dentro de una secuencia CSI (ESC [ ...). ECMA-48:
;  $20-$2F intermedios, $30-$3F parametros (digitos, ';' y $3A-$3F
;  privados), $40-$7E el byte final. Las secuencias privadas se ignoran
;  enteras.
; ---------------------------------------------------------------------
ansi_char:  ld   a,c
            cp   ';'
            jr   z,ansi_sep
            cp   '0'
            jr   c,ansi_int         ; $20-$2F: intermedio, absorber
            cp   '9'+1
            jr   nc,ansi_hi         ; $3A-$3F privado, o $40-$7E final
            sub  '0'                ; digito: n = n*10 + digito
            ld   e,a
            ld   a,(ansi_n)
            ld   d,a
            add  a,a
            add  a,a
            add  a,d
            add  a,a
            add  a,e
            ld   (ansi_n),a
            ret
ansi_sep:   jp   ansi_push          ; ';' cierra un parametro

ansi_hi:
            cp   040h
            jr   nc,ansi_end        ; $40-$7E: la letra final de verdad
            ld   (ansi_priv),a      ; $3A-$3F: marcar como privada
ansi_int:   ret                     ; absorber y seguir dentro de la secuencia

ansi_end:   ld   a,(ansi_priv)
            or   a
            jp   nz,ansi_done       ; secuencia privada: ignorarla entera
            call ansi_push          ; una letra cierra el ultimo y ejecuta
            ld   a,c
            cp   'm'
            jp   z,ansi_sgr
            cp   'H'
            jp   z,ansi_cup         ; CUP  ESC[fila;colH
            cp   'f'
            jp   z,ansi_cup         ; HVP, lo mismo
            cp   'A'
            jp   z,ansi_cuu         ; arriba
            cp   'B'
            jp   z,ansi_cud         ; abajo
            cp   'C'
            jp   z,ansi_cuf         ; derecha
            cp   'D'
            jp   z,ansi_cub         ; izquierda
            cp   'J'
            jp   z,ansi_ed          ; borrar pantalla
            cp   'K'
            jp   z,ansi_el          ; borrar linea
            cp   's'
            jp   z,ansi_scp         ; guardar posicion del cursor
            cp   'u'
            jp   z,ansi_rcp         ; restaurarla
            cp   'n'
            jp   z,ansi_dsr         ; "donde esta el cursor?"
ansi_done:  xor  a
            ld   (term_state),a
            ret

; --- helpers ---------------------------------------------------------
ansi_n1:    ld   a,(ansi_par)       ; primer parametro, 1 por defecto
            or   a
            ret  nz
            inc  a
            ret

ansi_fill:  ld   a,b                ; BC posiciones desde HL: espacio y cur_attr
            or   c
            ret  z
af_loop:    ld   (hl),' '
            push hl
            ld   de,ATTR_OFF
            add  hl,de
            ld   a,(cur_attr)
            ld   (hl),a
            pop  hl
            inc  hl
            dec  bc
            ld   a,b
            or   c
            jr   nz,af_loop
            ret

; --- CUP: ESC [ fila ; col H  (1-based; sin parametros, esquina) ------
ansi_cup:   ld   a,(ansi_par)
            or   a
            jr   nz,cup_r
            inc  a
cup_r:      dec  a
            cp   SCRH
            jr   c,cup_rok
            ld   a,SCRH-1
cup_rok:    ld   (cur_row),a
            ld   a,(ansi_cnt)
            cp   2
            jr   c,cup_c0           ; sin columna: la 1
            ld   a,(ansi_par+1)
            or   a
            jr   nz,cup_c
            inc  a
cup_c:      dec  a
            jr   cup_cc
cup_c0:     xor  a
cup_cc:     cp   SCRW
            jr   c,cup_cok
            ld   a,SCRW-1
cup_cok:    ld   (cur_col),a
            jp   ansi_done

; --- movimiento del cursor -------------------------------------------
ansi_cuu:   call ansi_n1
            ld   b,a
            ld   a,(cur_row)
            sub  b
            jr   nc,cuu_ok
            xor  a                  ; tope arriba
cuu_ok:     ld   (cur_row),a
            jp   ansi_done

ansi_cud:   call ansi_n1            ; el "jr c" hace falta: ESC[255B
            ld   b,a
            ld   a,(cur_row)
            add  a,b
            jr   c,cud_max          ; se salio de los 8 bits: al tope
            cp   SCRH
            jr   c,cud_ok
cud_max:    ld   a,SCRH-1           ; tope abajo
cud_ok:     ld   (cur_row),a
            jp   ansi_done

ansi_cuf:   call ansi_n1
            ld   b,a
            ld   a,(cur_col)
            add  a,b
            jr   c,cuf_max
            cp   SCRW
            jr   c,cuf_ok
cuf_max:    ld   a,SCRW-1           ; tope derecha
cuf_ok:     ld   (cur_col),a
            jp   ansi_done

ansi_cub:   call ansi_n1
            ld   b,a
            ld   a,(cur_col)
            sub  b
            jr   nc,cub_ok
            xor  a                  ; tope izquierda
cub_ok:     ld   (cur_col),a
            jp   ansi_done

; --- ED: ESC[J del cursor al final, ESC[2J toda la pantalla ----------
ansi_ed:    ld   a,(ansi_par)
            cp   2
            jr   nz,ed_0
            call cls                ; CALL, no JP: hay que dejar term_state a 0
            jp   ansi_done
ed_0:       ld   a,(ansi_par)
            or   a
            jp   nz,ansi_done       ; ESC[1J no implementado
            call char_addr          ; del cursor al final de la pantalla
            push hl
            ex   de,hl
            ld   hl,DFILE+1+ROWSTRIDE*SCRH
            or   a
            sbc  hl,de
            ld   b,h
            ld   c,l
            pop  hl
            call ansi_fill
            jp   ansi_done

; --- EL: ESC[K del cursor al fin de linea, ESC[2K la linea entera -----
ansi_el:    ld   a,(ansi_par)
            cp   2
            jr   z,el_all
            or   a
            jp   nz,ansi_done       ; ESC[1K no implementado
            call char_addr          ; del cursor al fin de linea
            ld   a,(cur_col)
            ld   b,a
            ld   a,SCRW
            sub  b
            jr   el_fill
el_all:     ld   a,(cur_col)        ; la linea entera: retroceder al margen
            push af
            xor  a
            ld   (cur_col),a
            call char_addr
            pop  af
            ld   (cur_col),a
            ld   a,SCRW
el_fill:    ld   c,a
            ld   b,0
            call ansi_fill
            jp   ansi_done

ansi_push:  ld   a,(ansi_cnt)
            cp   4
            jr   nc,ap_full         ; mas de 4 parametros: se descartan
            ld   hl,ansi_par
            ld   e,a
            ld   d,0
            add  hl,de
            ld   a,(ansi_n)
            ld   (hl),a
            ld   a,(ansi_cnt)
            inc  a
            ld   (ansi_cnt),a
ap_full:    xor  a
            ld   (ansi_n),a
            ret

ansi_sgr:   ld   a,(ansi_cnt)
            or   a
            jr   nz,sgr_loop0
            ld   a,(def_attr)       ; "ESC [ m" a secas = reset
            ld   (cur_attr),a
            jp   ansi_done
sgr_loop0:  ld   b,a
            ld   hl,ansi_par
sgr_loop:   ld   a,(hl)
            push hl
            push bc
            call sgr_apply
            pop  bc
            pop  hl
            inc  hl
            djnz sgr_loop
            jp   ansi_done

sgr_apply:  or   a                  ; 0 = todo a los valores por defecto
            jr   nz,sgr_n1
            ld   a,(def_attr)
            ld   (cur_attr),a
            ret
sgr_n1:     cp   1                  ; 1 = brillo en la tinta
            jr   nz,sgr_fg
            ld   a,(cur_attr)
            or   008h
            ld   (cur_attr),a
            ret
sgr_fg:     cp   30                 ; 30-37 = color de tinta
            ret  c
            cp   38
            jr   nc,sgr_bg
            sub  30
            call ansi2chroma
            ld   b,a
            ld   a,(cur_attr)
            and  0F8h               ; conserva papel y el brillo de tinta
            or   b
            ld   (cur_attr),a
            ret
sgr_bg:     cp   40                 ; 40-47 = color de papel
            ret  c
            cp   48
            ret  nc
            sub  40
            call ansi2chroma
            add  a,a
            add  a,a
            add  a,a
            add  a,a                ; el papel va en los bits 7-4
            ld   b,a
            ld   a,(cur_attr)
            and  08Fh               ; conserva tinta y el brillo del papel
            or   b
            ld   (cur_attr),a
            ret

ansi2chroma:                        ; A = color ANSI (0-7) -> color del hardware
            ld   hl,ansi_ctab
            ld   e,a
            ld   d,0
            add  hl,de
            ld   a,(hl)
            ret
ansi_ctab:  defb 0,2,4,6,1,3,5,7    ; negro rojo verde amarillo azul magenta cian blanco

; --- SCP/RCP: ESC[s guarda la posicion, ESC[u la restaura ------------
ansi_scp:   ld   a,(cur_row)
            ld   (sav_row),a
            ld   a,(cur_col)
            ld   (sav_col),a
            jp   ansi_done

ansi_rcp:   ld   a,(sav_row)
            ld   (cur_row),a
            ld   a,(sav_col)
            ld   (cur_col),a
            jp   ansi_done

; ---------------------------------------------------------------------
;  DSR: ESC[6n -> ESC[fila;colR. Las BBS lo usan para medir la pantalla.
;  La respuesta sale por el canal de entrada: kbget/con_cist la sirven
;  antes que el teclado, como si se hubiera tecleado.
; ---------------------------------------------------------------------
ansi_dsr:   ld   a,(ansi_par)
            cp   6
            jp   nz,ansi_done       ; solo se entiende el DSR 6
            ld   hl,ab_buf
            ld   (hl),01Bh
            inc  hl
            ld   (hl),'['
            inc  hl
            ld   a,(cur_row)
            cp   SCRH
            jr   c,dsr_r
            ld   a,SCRH-1
dsr_r:      inc  a
            call ab_num
            ld   (hl),';'
            inc  hl
            ld   a,(cur_col)
            cp   SCRW
            jr   c,dsr_c
            ld   a,SCRW-1
dsr_c:      inc  a
            call ab_num
            ld   (hl),'R'
            inc  hl
            ld   de,ab_buf
            or   a
            sbc  hl,de
            ld   a,l                ; A = cuantos bytes se han montado
            ld   (ab_cnt),a
            ld   hl,ab_buf
            ld   (ab_ptr),hl
            jp   ansi_done

ab_num:     ld   b,'0'-1            ; A (1..99) en decimal a (HL)
ab_d10:     inc  b
            sub  10
            jr   nc,ab_d10
            add  a,10
            ld   c,a                ; C = unidades
            ld   a,b
            cp   '0'
            jr   z,ab_uni           ; no hay decenas que sacar
            ld   (hl),a
            inc  hl
ab_uni:     ld   a,c
            add  a,'0'
            ld   (hl),a
            inc  hl
            ret

; --- tabla de filas para char_addr -----------------------------------
row_tab:    defw DFILE+1+ROWSTRIDE*0,  DFILE+1+ROWSTRIDE*1
            defw DFILE+1+ROWSTRIDE*2,  DFILE+1+ROWSTRIDE*3
            defw DFILE+1+ROWSTRIDE*4,  DFILE+1+ROWSTRIDE*5
            defw DFILE+1+ROWSTRIDE*6,  DFILE+1+ROWSTRIDE*7
            defw DFILE+1+ROWSTRIDE*8,  DFILE+1+ROWSTRIDE*9
            defw DFILE+1+ROWSTRIDE*10, DFILE+1+ROWSTRIDE*11
            defw DFILE+1+ROWSTRIDE*12, DFILE+1+ROWSTRIDE*13
            defw DFILE+1+ROWSTRIDE*14, DFILE+1+ROWSTRIDE*15
            defw DFILE+1+ROWSTRIDE*16, DFILE+1+ROWSTRIDE*17
            defw DFILE+1+ROWSTRIDE*18, DFILE+1+ROWSTRIDE*19
            defw DFILE+1+ROWSTRIDE*20, DFILE+1+ROWSTRIDE*21
            defw DFILE+1+ROWSTRIDE*22, DFILE+1+ROWSTRIDE*23

print_glyph:                        ; A = caracter en el cursor
            push af
            call char_addr          ; HL = direccion en pantalla
            pop  af
            ld   (hl),a
            push af                 ; y el atributo, en la posicion equivalente
            push hl
            ld   de,ATTR_OFF
            add  hl,de
            ld   a,(cur_attr)
            ld   (hl),a
            pop  hl
            pop  af
            ld   a,(cur_col)
            inc  a
            cp   SCRW
            jr   c,pg_set
            xor  a
            ld   (cur_col),a
            jp   vtnl
pg_set:     ld   (cur_col),a
            ret

; ---------------------------------------------------------------------
;  char_addr - HL = DFILE+1+cur_row*ROWSTRIDE+cur_col. NO debe tocar DE
;  ni BC vistos desde fuera (ver la nota en chario.z80).
; ---------------------------------------------------------------------
char_addr:  push bc
            ld   a,(cur_row)
            add  a,a                ; fila*2 = indice en la tabla
            ld   c,a
            ld   b,0
            ld   hl,row_tab
            add  hl,bc
            ld   a,(hl)
            inc  hl
            ld   h,(hl)
            ld   l,a                ; HL = principio de la fila
            ld   a,(cur_col)
            add  a,l
            ld   l,a
            jr   nc,ca_ret
            inc  h
ca_ret:     pop  bc
            ret

vtnl:       ld   a,(cur_row)
            inc  a
            cp   SCRH
            jr   c,nl_st
            call scroll
            ld   a,SCRH-1
nl_st:      ld   (cur_row),a
            ret

scroll:     ld   hl,DFILE+1+ROWSTRIDE
            ld   de,DFILE+1
            ld   bc,ROWSTRIDE*(SCRH-1)
            ldir
            ld   hl,DFILE+1+ROWSTRIDE*(SCRH-1)
            ld   (hl),' '
            ld   de,DFILE+1+ROWSTRIDE*(SCRH-1)+1
            ld   bc,ROWSTRIDE-1
            ldir
            ld   hl,ATTR+1+ROWSTRIDE    ; y los atributos
            ld   de,ATTR+1
            ld   bc,ROWSTRIDE*(SCRH-1)
            ldir
            ld   a,(cur_attr)       ; la linea nueva, con el color en curso
            ld   hl,ATTR+1+ROWSTRIDE*(SCRH-1)
            ld   (hl),a
            ld   de,ATTR+1+ROWSTRIDE*(SCRH-1)+1
            ld   bc,ROWSTRIDE-1
            ldir
            ret

cls:        ld   hl,DFILE
            ld   (hl),' '
            ld   de,DFILE+1
            ld   bc,1+ROWSTRIDE*SCRH-1
            ldir
            ld   a,(cur_attr)
            ld   hl,ATTR
            ld   (hl),a
            ld   de,ATTR+1
            ld   bc,1+ROWSTRIDE*SCRH-1
            ldir
            xor  a
            ld   (cur_col),a
            ld   (cur_row),a
            ret

; ---------------------------------------------------------------------
;  kbget - teclado ZX81 -> A = ASCII (bloqueante)
; ---------------------------------------------------------------------
kbget:
            ld   a,(ab_cnt)         ; la respuesta al DSR va primero
            or   a
            jr   nz,kb_ans
ck_top:     ld   a,(ctrl_prefix)
            or   a
            jr   nz,ck_pwait        ; modo CTRL armado -> esperar la tecla
            xor  a
            ld   (mods_seen),a      ; modo normal: reiniciar mods vistos
ck_nwait:   call wait_scan          ; A=idx/FF ; E: bit0=SHIFT bit1=ENTER
            cp   0FFh
            jr   nz,ck_key          ; hay tecla -> procesar
            ld   a,(mods_seen)      ; sin tecla: acumular modificadores vistos
            or   e
            ld   (mods_seen),a
            ld   a,e
            and  3
            jr   nz,ck_nwait        ; algun modificador aun pulsado -> seguir
            ld   a,(mods_seen)      ; todo suelto: decidir segun lo visto
            and  3
            jr   z,ck_clr           ; sin modificadores
            cp   3
            jr   z,ck_arm           ; SHIFT+ENTER -> armar modo CTRL
            bit  1,a
            jr   z,ck_clr           ; solo SHIFT -> nada
            ld   a,(enter_used)     ; solo ENTER
            or   a
            jr   nz,ck_clr          ; ENTER ya genero simbolo -> no CR
            call cursor_clear
            ld   a,0Dh              ; ENTER solo -> CR
            ret
kb_ans:     dec  a                  ; un byte de la respuesta al DSR
            ld   (ab_cnt),a
            ld   hl,(ab_ptr)
            ld   a,(hl)
            inc  hl
            ld   (ab_ptr),hl
            ret

ck_clr:     xor  a
            ld   (enter_used),a
            jr   ck_top
ck_arm:     call cursor_clear
            ld   a,1
            ld   (ctrl_prefix),a    ; modo CTRL (cursor pasa a subrayado)
            jr   ck_top
; --- modo CTRL armado: la siguiente tecla -> CTRL+letra ---
ck_pwait:   call wait_scan
            cp   0FFh
            jr   z,ck_pwait
            push af
            call cursor_clear
            pop  af
            ld   (km_idx),a
ck_prel:    call scan_keys          ; esperar a soltar la tecla
            cp   0FFh
            jr   nz,ck_prel
            xor  a
            ld   (ctrl_prefix),a    ; desarmar modo CTRL
            ld   hl,keymap
            ld   a,(km_idx)
            ld   c,a
            ld   b,0
            add  hl,bc
            ld   a,(hl)
            and  01Fh               ; CTRL = letra AND $1F
            ret
; --- tecla con modificadores (SHIFT / ENTER / ambos) ---
ck_key:     push af
            call cursor_clear
            pop  af
            ld   (km_idx),a
            ld   a,e
            ld   (km_mods),a
ck_rel:     call scan_keys          ; esperar a soltar la tecla; ACUMULAR mods
            push af
            ld   a,(km_mods)
            or   e
            ld   (km_mods),a
            pop  af
            cp   0FFh
            jr   nz,ck_rel
            ld   a,(km_mods)
            cp   3                  ; SHIFT+ENTER+tecla directo -> CTRL
            jr   z,ck_ctrl
            ld   hl,keymap
            bit  1,a                ; ENTER -> simbolos
            jr   z,ck_nse
            ld   hl,keymap_sym
            jr   ck_tbl
ck_nse:     bit  0,a                ; SHIFT -> mayusculas/funciones
            jr   z,ck_tbl
            ld   hl,keymap_shift
ck_tbl:     ld   a,(km_idx)
            ld   c,a
            ld   b,0
            add  hl,bc
            ld   a,(hl)
            or   a
            jp   z,ck_top           ; sin ASCII -> reesperar
            jr   ck_mods
ck_ctrl:    ld   hl,keymap
            ld   a,(km_idx)
            ld   c,a
            ld   b,0
            add  hl,bc
            ld   a,(hl)
            and  01Fh
ck_mods:    ld   c,a                ; codigo a devolver
            ld   a,(km_mods)        ; si ENTER fue modificador, marcar consumido
            bit  1,a
            jr   z,ck_ret
            ld   a,1
            ld   (enter_used),a
ck_ret:     ld   a,c
            ret

scan_keys:  ; A=indice (1..39) o 0FFh ; E: bit0=SHIFT, bit1=ENTER
            ld   bc,0FEFEh          ; SHIFT (A8 D0)
            in   a,(c)
            ld   e,0
            rra
            jr   c,sk_r0
            inc  e                  ; E bit0 = SHIFT
sk_r0:      or   0F0h               ; A8: D0=Z D1=X D2=C D3=V
            ld   d,1
            ld   c,4
sk_r0b:     rra
            jr   nc,sk_done
            inc  d
            dec  c
            jr   nz,sk_r0b
            ld   b,0BFh             ; ENTER (A14 D0)
            ld   c,0FEh
            in   a,(c)
            bit  0,a
            jr   nz,sk_e0
            set  1,e                ; E bit1 = ENTER
sk_e0:      ld   hl,row_lines+1     ; filas A9..A15
            ld   d,5
sk_row:     ld   a,(hl)
            or   a
            jr   z,sk_no
            ld   b,a
            ld   c,0FEh
            in   a,(c)
            or   0E0h
            bit  6,b                ; A14 (0BFh): bit6=0 -> ignorar D0 (ENTER)
            jr   nz,sk_nm
            or   1
sk_nm:      ld   c,5
sk_rb:      rra
            jr   nc,sk_done
            inc  d
            dec  c
            jr   nz,sk_rb
            inc  hl
            jr   sk_row
sk_no:      ld   a,0FFh
            ret
sk_done:    ld   a,d
            ret

; ---------------------------------------------------------------------
;  Cursor parpadeante en (cur_col,cur_row): bloque solido / subrayado
;  segun ctrl_prefix, al ritmo de los VSYNC (puerto $AF).
; ---------------------------------------------------------------------
wait_scan:  call scan_keys          ; hace avanzar el parpadeo si no hay tecla
            cp   0FFh
            ret  nz
            push de
            call cursor_blink_poll
            pop  de
            ld   a,0FFh
            ret

cursor_clear:                       ; si el cursor esta dibujado, borrarlo
            ld   a,(cursor_on)
            or   a
            ret  z
            jp   cursor_toggle

cursor_toggle:                      ; alterna entre el caracter real y el cursor
            call char_addr
            ld   a,(cursor_on)
            or   a
            jr   nz,curt_hide
curt_show:  ld   a,(hl)
            ld   (cursor_save),a
            ld   a,(ctrl_prefix)
            or   a
            ld   a,CURSOR_BLOCK
            jr   z,curt_put
            ld   a,CURSOR_UNDER     ; modo CTRL: cursor de subrayado
curt_put:   ld   (hl),a
            ld   a,1
            ld   (cursor_on),a
            ret
curt_hide:  ld   a,(cursor_save)
            ld   (hl),a
            xor  a
            ld   (cursor_on),a
            ret

cursor_blink_poll:
            in   a,(0AFh)
            and  07Eh               ; bits 6..1 = VSYNC desde la ultima lectura
            rrca
            or   a
            ret  z
            ld   c,a
            ld   hl,blink
            ld   a,(hl)
            add  a,c
            cp   BLINK_RATE
            jr   c,cbp_store
            sub  BLINK_RATE
            ld   (hl),a
            jp   cursor_toggle
cbp_store:  ld   (hl),a
            ret

row_lines:  defb 0FEh,0FDh,0FBh,0F7h,0EFh,0DFh,0BFh,07Fh,000h
keymap:     defb 000h,'z','x','c','v'      ; A8 : SHIFT Z X C V (base)
            defb 'a','s','d','f','g'       ; A9
            defb 'q','w','e','r','t'       ; A10
            defb '1','2','3','4','5'       ; A11
            defb '0','9','8','7','6'       ; A12
            defb 'p','o','i','u','y'       ; A13
            defb 00Dh,'l','k','j','h'      ; A14
            defb ' ','.','m','n','b'       ; A15

keymap_shift: ; SHIFT: mayusculas + funciones (0=DEL, 9=BS, 5/6/7/8=flechas ANSI)
            defb 000h,'Z','X','C','V'      ; A8
            defb 'A','S','D','F','G'       ; A9
            defb 'Q','W','E','R','T'       ; A10
            defb 01Bh,000h,000h,000h,KEY_LEFT       ; A11: 1=ESC, 5=izquierda
            defb 07Fh,008h,KEY_RIGHT,KEY_UP,KEY_DOWN ; A12: 0=DEL,9=BS,8=der,7=arr,6=abj
            defb 'P','O','I','U','Y'       ; A13
            defb 00Dh,'L','K','J','H'      ; A14
            defb ' ',009h,'M','N','B'      ; A15: .=TAB

keymap_sym:  ; ENTER+tecla: simbolos serigrafiados y los que faltan
            defb 000h,':',';','?','/'      ; A8
            defb 000h,05Ch,000h,07Eh,060h  ; A9 : S=\ F=~ G=`
            defb 027h,07Bh,07Dh,05Bh,05Fh  ; A10: Q=' W={ E=} R=[ T=_
            defb '!',040h,'#',07Ch,'%'     ; A11: 1=! 2=@ 3=# 4=| 5=%
            defb 01Dh,01Ch,01Eh,000h,'&'   ; A12: 0=^] 9=^\ 8=^^ 6=&
            defb 022h,')','(','$',05Dh     ; A13: P=" O=) I=( U=$ Y=]
            defb 00Dh,'=','+','-',05Eh     ; A14: L== K=+ J=- H=^
            defb ' ',',','>','<','*'       ; A15: .=, M=> N=< B=*

; ---------------------------------------------------------------------
;  Textos (ASCII, la fuente tiene minusculas)
; ---------------------------------------------------------------------
msg_on:     defb "SD81 TELNET - terminal ANSI 80x24",13,10
            defb "Connect: ATDT host:port (ATH hangs up, +++ = command mode)",13,10
            defb "ENTER+0 exit, ENTER+9 local echo, ENTER+8 colours",13,10
            defb "SHIFT+ENTER then key = CTRL, SHIFT+5/6/7/8 = arrows",13,10
            defb 13,10,0
msg_eco_on: defb 13,10,"[local echo on]",13,10,0
msg_eco_off: defb 13,10,"[local echo off]",13,10,0
msg_off:    defb 13,10,"[bye]",13,10,0
at_e1:      defb "ATE1",13
AT_E1_LEN   equ $-at_e1
at_esc:     defb "+++"
at_h:       defb "ATH",13
AT_H_LEN    equ $-at_h
tn_resp:    defb T_IAC,0,0
arrow_seq:  defb 01Bh,'[',0

; ---------------------------------------------------------------------
;  Variables (se ponen a cero al arrancar)
; ---------------------------------------------------------------------
vars:
cur_col:    defs 1
cur_row:    defs 1
km_idx:     defs 1
km_mods:    defs 1
enter_used: defs 1
blink:      defs 1
cursor_on:  defs 1
term_state: defs 1
cur_attr:   defs 1              ; color con el que se imprime
def_attr:   defs 1              ; color por defecto (el del esquema)
scheme:     defs 1              ; esquema de color en curso (0-2)
ansi_n:     defs 1              ; parametro ANSI que se acumula
ansi_par:   defs 4              ; parametros ya cerrados
ansi_cnt:   defs 1
ansi_priv:  defs 1              ; !=0 si la secuencia lleva un byte privado
mods_seen:  defs 1
ctrl_prefix: defs 1
cursor_save: defs 1
sav_row:    defs 1              ; ESC[s / ESC[u
sav_col:    defs 1
ab_buf:     defs 10             ; respuesta al DSR: ESC[nn;nnR
ab_cnt:     defs 1              ; cuantos quedan por servir
ab_ptr:     defs 2
localecho:  defs 1              ; 1 = eco local
txbyte:     defs 1              ; la tecla que se acaba de mandar
net_st:     defs 1              ; estado de la conexion (NET_READ/WRITE)
tn_state:   defs 1              ; filtro telnet: estado, comando y opcion
tn_cmd:     defs 1
tn_opt:     defs 1
VARS_LEN    equ $-vars
sv_i:       defs 1              ; registro I al entrar (fuera de vars: se
                                ; guarda antes de ponerlas a cero)
rxbuf:      defs RXMAX

            include "font.inc"

            end
