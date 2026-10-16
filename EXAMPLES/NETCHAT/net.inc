; =====================================================================
;  NET.INC - capa B del juego en red: sala, mensajes firmados y un
;  canal de mensajes cortos entre dos ZX81, por MQTT, sin servidor
;  propio. Usa los comandos AT genericos del modulo WiFi (capa A,
;  NET_MQTT.cpp): AT+MQTT, AT+MQTTSUB/PUB/RX/WILL, AT+SHA256, AT+HMAC y
;  AT+RAND. Diseno: claude/planning/juego_en_red.md.
;
;  El que incluye este fichero define ANTES:
;      NB_BASE  equ 7000h     ; RAM de trabajo (1.2 KB, fuera del codigo)
;  y despues  include "net.inc"  (en pasmo, el codigo va donde este el org).
;
;  Rutinas publicas (CALL; destruyen todos los registros; A = resultado,
;  tambien en (NB_RES)):
;      net_init    ATZ, ATE0 y borra lo pendiente del modem.
;      room_new    crea una sala: genera el codigo (NB_CODE81, 10 codigos
;                  de caracter del ZX81), conecta y se suscribe. Soy el 0.
;      room_join   entra en la sala cuyo codigo esta en NB_CODE81. Mira quien hay: si el creador
;                  esta y la ranura 1 libre soy el 1 (NB_SLOT); si el creador se fue y la
;                  ranura 1 esta ocupada, vuelvo a ser el 0 (el creador reentra). Si la sala existio
;                  pero esta vacia (las dos presencias a '0': se cayeron los dos), devuelve 8 y
;                  hay que elegir ranura con room_join_as.
;      room_join_as  igual que room_join, pero entrando en la ranura A (0 = el creador, 1 = el otro),
;                  sin mirar quien hay: para el 8 de room_join.
;      room_send   manda NB_LEN bytes (0-100) de NB_TX con el turno NB_TURN.
;      room_poll   mira si ha llegado algo: A = 0 nada, 1 mensaje (NB_RX,
;                  NB_LEN, NB_TURN), 2 el rival entro, 3 el rival salio,
;                  4 error del modem. Espera la respuesta del modem (~30 ms).
;      room_poll_send / room_poll_recv   lo mismo en dos pasos: _send solo manda la
;                  pregunta y vuelve al momento; _recv recoge la respuesta (sin esperar
;                  si ya ha llegado). Entre las dos el programa hace otra cosa (pintar,
;                  esperar un fotograma). Sin _send previo, _recv pregunta y espera.
;      room_leave  avisa de que me voy y cierra la sesion.
;  Resultados de las demas (A): 0 bien, 1 el modem dijo ERROR, 2 plazo
;  agotado, 3 respuesta inesperada, 4 codigo de sala invalido, 5 mensaje
;  demasiado largo, 6 la sala esta llena (room_join: ya hay otro jugador en la
;  ranura 1), 7 la sala no existe (room_join: nadie la ha creado), 8 la sala existio pero esta
;  vacia (room_join: hay que elegir ranura con room_join_as). En 6, 7 y 8 no se llega a entrar: se cierra la sesion sin tocar la presencia.
;
;  REGLAS DEL MCU: C es el reloj del handshake y A no sobrevive a
;  mcu_send (claude/planning/net_bridge_emulator.md).
;  Funciona en SLOW (con la NMI activa) y en FAST: no hace falta cambiar de modo. (Un cuelgue
;  que se atribuyo a la NMI era un netchat.bin viejo, con un fallo ya arreglado en flush_rx.)
; =====================================================================

DataPort    equ 0A7h
ClkPort     equ 0AFh
CMD_NET_RD  equ 66
CMD_NET_WR  equ 67

TMO_S       equ 150             ; 3 s (en pasos de ~20 ms)
TMO_L       equ 1800            ; 36 s: AT+MQTT conecta con hasta 3 brokers

; --- variables: el buzon (BASIC puede PEEK/POKE) ----------------------
NB_RES      equ NB_BASE+0       ; resultado de la ultima llamada
NB_SLOT     equ NB_BASE+1       ; mi ranura (0 creo, 1 me uni)
NB_PEER     equ NB_BASE+2       ; 1 si el rival esta
NB_TURN     equ NB_BASE+3       ; (2) turno de lo que se manda / se recibe
NB_LEN      equ NB_BASE+5       ; bytes de NB_TX / NB_RX
NB_CODE81   equ NB_BASE+8       ; (10) codigo de sala, codigos de caracter ZX81
NB_TX       equ NB_BASE+32      ; (100) lo que se manda
NB_RX       equ NB_BASE+144     ; (100) lo que llega
; --- privadas -----------------------------------------------------------
V_CODE      equ NB_BASE+256     ; (11) codigo en ASCII y 0
V_BASE      equ NB_BASE+272     ; (24) "sd81/" + 12 hex + 0
V_SEQ       equ NB_BASE+296     ; (2) mi numero de secuencia
V_PSEQ      equ NB_BASE+298     ; (2) el ultimo del rival
V_PSEQV     equ NB_BASE+300     ; 1 si V_PSEQ vale
V_P0        equ NB_BASE+301     ; room_join: la presencia retenida de la ranura 0, tal como llego ('1' ocupada)
V_P1        equ NB_BASE+311     ; lo mismo de la ranura 1
V_TMO       equ NB_BASE+302     ; (2) pasos que quedan
V_RXN       equ NB_BASE+304     ; bytes en V_RXBUF
V_RXP       equ NB_BASE+305     ; (2) siguiente
V_RESPLEN   equ NB_BASE+307     ; longitud de la primera linea '+'
V_RAWP      equ NB_BASE+308     ; (2) datos que van tras '>'
V_RAWL      equ NB_BASE+310     ; cuantos
V_CP        equ NB_BASE+312     ; (2) donde se escribe el comando
V_N         equ NB_BASE+314     ; cuerpo del mensaje
V_RXLEN     equ NB_BASE+315     ; longitud de lo que dijo AT+MQTTRX
V_I         equ NB_BASE+316
V_NETST     equ NB_BASE+317     ; estado de la conexion (NET_READ/WRITE)
V_PEND      equ NB_BASE+318     ; 1 si hay un AT+MQTTRX pedido por room_poll_send
V_CHK       equ NB_BASE+319     ; 1 mientras room_join mira si la ranura 1 esta ocupada
V_RESP      equ NB_BASE+320     ; (96) primera linea '+' de la respuesta
V_LINE      equ NB_BASE+416     ; (96) linea en curso
V_TOPIC     equ NB_BASE+512     ; (48) topic recibido
V_SCR       equ NB_BASE+560     ; (32) borrador
V_CMD       equ NB_BASE+592     ; (80) comando AT que se esta armando
V_MAC       equ NB_BASE+672     ; (8) HMAC truncado
V_RND       equ NB_BASE+680     ; (8) bytes aleatorios
V_WK        equ NB_BASE+688     ; (192) clave (64) + cuerpo (hasta 120)
V_RXBUF     equ NB_BASE+880     ; (128) lo que trae NET_READ

; =====================================================================
;  El tubo: comandos 66/67 del MCU (las de EXAMPLES/TELNET)
; =====================================================================
; lee hasta B bytes en (HL). Devuelve B = leidos
net_read:   push hl
            ld   d,b
            call sd_clk
            ld   a,CMD_NET_RD
            call mcu_send
            ld   a,d
            call mcu_send           ; max
            call mcu_recv
            ld   b,a                ; count
            ld   d,a
            or   a
            jr   z,nr_tail
nr_loop:    call mcu_recv
            ld   (hl),a
            inc  hl
            djnz nr_loop
nr_tail:    call mcu_recv           ; pendientes
            call mcu_recv           ; estado
            ld   (V_NETST),a
            ld   b,d
            pop  hl
            ret

; envia B bytes desde (HL). Devuelve B = aceptados (HL ha avanzado B original)
net_write:  call sd_clk
            ld   a,CMD_NET_WR
            call mcu_send
            ld   a,b
            call mcu_send           ; count
            ld   a,b                ; mcu_send se ha comido A
            or   a
            jr   z,nw_tail
nw_loop:    ld   a,(hl)
            call mcu_send
            inc  hl
            djnz nw_loop
nw_tail:    call mcu_recv           ; aceptados
            ld   b,a
            call mcu_recv           ; estado
            ld   (V_NETST),a
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
;  Tiempo, envio y recepcion de bytes
; =====================================================================
; ~20 ms a 3,25 MHz (en SLOW dura mas). No usa VSYNC: leer $AF pone a cero
; su contador y el handshake lo lee todo el rato.
delay20:    ld   bc,2500
dl_lp:      dec  bc
            ld   a,b
            or   c
            jr   nz,dl_lp
            ret

; HL = datos, E = cuantos (0-255). Reintenta si el modem no acepta
tx_bytes:   ld   d,40
tb_lp:      ld   a,e
            or   a
            ret  z
            push hl
            ld   b,e
            call net_write          ; B = aceptados
            pop  hl
            ld   a,b
            or   a
            jr   nz,tb_ok
            call delay20
            dec  d
            jr   nz,tb_lp
            ret                     ; no acepta nada: se rinde (la respuesta dara plazo)
tb_ok:      ld   a,e
            sub  b
            ld   e,a
            ld   c,b
            ld   b,0
            add  hl,bc
            ld   d,40
            jr   tb_lp

; HL = cadena terminada en 0
tx_str:     push hl
            ld   e,0
ts_len:     ld   a,(hl)
            or   a
            jr   z,ts_go
            inc  hl
            inc  e
            jr   ts_len
ts_go:      pop  hl
            jp   tx_bytes

tx_cr:      ld   hl,s_cr
            ld   e,1
            jp   tx_bytes

; siguiente byte recibido en A; CF = 1 si se agota V_TMO (pasos de ~20 ms)
rx_get:     ld   a,(V_RXN)
            or   a
            jr   nz,rg_have
rg_try:     ld   hl,V_RXBUF
            ld   b,128
            call net_read
            ld   a,b
            or   a
            jr   nz,rg_got
            ld   hl,(V_TMO)
            ld   a,h
            or   l
            jr   z,rg_to
            dec  hl
            ld   (V_TMO),hl
            call delay20
            jr   rg_try
rg_got:     ld   (V_RXN),a
            ld   hl,V_RXBUF
            ld   (V_RXP),hl
rg_have:    ld   hl,(V_RXP)
            ld   a,(hl)
            inc  hl
            ld   (V_RXP),hl
            ld   hl,V_RXN
            dec  (hl)
            or   a                  ; CF = 0
            ret
rg_to:      scf
            ret

; tira lo que haya (hasta 100 ms de silencio)
flush_rx:   ld   e,5            ; (net_read destruye D)
fl_lp:      ld   hl,V_RXBUF
            ld   b,128
            call net_read
            ld   a,b
            or   a
            jr   nz,flush_rx
            call delay20
            dec  e
            jr   nz,fl_lp
            xor  a
            ld   (V_RXN),a
            ret

; una linea (termina en CR o LF) en V_LINE; A = longitud, CF = plazo
rx_line:    ld   hl,V_LINE
            ld   b,0
rl_lp:      push hl
            push bc
            call rx_get
            pop  bc
            pop  hl
            ret  c
            cp   13
            jr   z,rl_end
            cp   10
            jr   z,rl_end
            ld   (hl),a
            inc  hl
            inc  b
            ld   a,b
            cp   95
            jr   c,rl_lp
rl_end:     ld   (hl),0
            ld   a,b
            or   a
            ret

; =====================================================================
;  Cadenas
; =====================================================================
; Z si (HL) y (DE) son iguales (cadenas con 0)
str_eq:     ld   a,(de)
            cp   (hl)
            ret  nz
            or   a
            ret  z
            inc  hl
            inc  de
            jr   str_eq

; copia (HL) -> (DE) con el 0; A = longitud sin el 0
str_copy:   ld   b,0
sc_lp:      ld   a,(hl)
            ld   (de),a
            inc  hl
            inc  de
            or   a
            jr   z,sc_end
            inc  b
            jr   sc_lp
sc_end:     ld   a,b
            ret

; A = ASCII hexadecimal en mayusculas -> A = 0-15; CF si no lo es
hex2nib:    sub  '0'
            ret  c
            cp   10
            jr   c,hn_ok
            sub  7
            cp   10
            ret  c
            cp   16
            jr   c,hn_ok
            scf
            ret
hn_ok:      or   a
            ret

; (HL) texto hexadecimal -> B bytes en (DE); CF si hay un caracter malo
hex_dec:    ld   a,(hl)
            inc  hl
            call hex2nib
            ret  c
            add  a,a
            add  a,a
            add  a,a
            add  a,a
            ld   c,a
            ld   a,(hl)
            inc  hl
            call hex2nib
            ret  c
            or   c
            ld   (de),a
            inc  de
            djnz hex_dec
            or   a
            ret

; =====================================================================
;  Armar un comando AT en V_CMD
; =====================================================================
cb_init:    ld   hl,V_CMD
            ld   (V_CP),hl
            ld   (hl),0
            ret

; HL = cadena que se anade
cb_puts:    ld   de,(V_CP)
cp_lp:      ld   a,(hl)
            ld   (de),a
            or   a
            jr   z,cp_end
            inc  hl
            inc  de
            jr   cp_lp
cp_end:     ld   (V_CP),de
            ret

; A = caracter que se anade
cb_putc:    ld   de,(V_CP)
            ld   (de),a
            inc  de
            xor  a
            ld   (de),a
            ld   (V_CP),de
            ret

; A = numero (0-255) en decimal
cb_dec:     ld   hl,(V_CP)
            ld   b,0
            ld   c,100
            call cd_dig
            ld   c,10
            call cd_dig
            add  a,'0'
            ld   (hl),a
            inc  hl
            ld   (hl),0
            ld   (V_CP),hl
            ret
cd_dig:     ld   d,0
cd_l:       cp   c
            jr   c,cd_done
            sub  c
            inc  d
            jr   cd_l
cd_done:    ld   e,a
            ld   a,d
            or   a
            jr   nz,cd_pr
            ld   a,b
            or   a
            jr   z,cd_skip
            xor  a
cd_pr:      add  a,'0'
            ld   (hl),a
            inc  hl
            ld   b,1
cd_skip:    ld   a,e
            ret

; =====================================================================
;  Dialogo con el modem
; =====================================================================
; Lee lineas hasta OK o ERROR. A = 0 OK (Z), 1 ERROR, 2 plazo. La primera
; linea que empieza por '+' queda en V_RESP (V_RESPLEN = su longitud).
at_wait:    xor  a
            ld   (V_RESPLEN),a
aw_lp:      call rx_line
            jr   c,aw_to
            or   a
            jr   z,aw_lp
            ld   hl,V_LINE
            ld   de,s_ok
            call str_eq
            jr   z,aw_ok
            ld   hl,V_LINE
            ld   de,s_error
            call str_eq
            jr   z,aw_err
            ld   a,(V_LINE)
            cp   '+'
            jr   nz,aw_lp
            ld   a,(V_RESPLEN)
            or   a
            jr   nz,aw_lp
            ld   hl,V_LINE
            ld   de,V_RESP
            call str_copy
            ld   (V_RESPLEN),a
            jr   aw_lp
aw_ok:      xor  a
            ret
aw_err:     ld   a,1
            or   a
            ret
aw_to:      ld   a,2
            or   a
            ret

; HL = comando (cadena), DE = pasos de ~20 ms
at_cmd:     ld   (V_TMO),de
            call tx_str
            call tx_cr
            jp   at_wait

; HL = comando, DE = plazo; los datos, en (V_RAWP) y V_RAWL bytes. El modem
; contesta '>' y se mandan; luego OK / ERROR
at_raw:     ld   (V_TMO),de
            call tx_str
            call tx_cr
ar_prompt:  call rx_get
            jr   c,ar_to
            cp   '>'
            jr   z,ar_go
            cp   'E'
            jr   nz,ar_prompt
            call rx_line            ; el resto de ERROR
            ld   a,1
            or   a
            ret
ar_go:      ld   hl,(V_RAWP)
            ld   a,(V_RAWL)
            ld   e,a
            call tx_bytes
            jp   at_wait
ar_to:      ld   a,2
            or   a
            ret

; HL = datos, E = cuantos. SHA-256: el resumen en hexadecimal queda en V_RESP+8
sha256_of:  ld   (V_RAWP),hl
            ld   a,e
            ld   (V_RAWL),a
            call cb_init
            ld   hl,s_sha
            call cb_puts
            ld   a,(V_RAWL)
            call cb_dec
            ld   hl,V_CMD
            ld   de,TMO_S
            call at_raw
            ret  nz
            ld   a,(V_RESPLEN)
            cp   72                 ; "+SHA256:" + 64
            jr   z,sh_ok
            ld   a,3
            or   a
            ret
sh_ok:      xor  a
            ret

; V_WK = clave (64) + cuerpo de V_N bytes: HMAC. Deja los 8 primeros bytes en V_MAC
hmac_wk:    ld   a,(V_N)
            add  a,64
            ld   (V_RAWL),a
            ld   hl,V_WK
            ld   (V_RAWP),hl
            call cb_init
            ld   hl,s_hmac
            call cb_puts
            ld   a,(V_N)
            call cb_dec
            ld   hl,V_CMD
            ld   de,TMO_S
            call at_raw
            ret  nz
            ld   a,(V_RESPLEN)
            cp   70                 ; "+HMAC:" + 64
            jr   z,hm_hex
            ld   a,3
            or   a
            ret
hm_hex:     ld   hl,V_RESP+6
            ld   de,V_MAC
            ld   b,8
            call hex_dec
            jr   c,hm_bad
            xor  a
            ret
hm_bad:     ld   a,3
            or   a
            ret

; =====================================================================
;  La sala: codigo, topic y clave
; =====================================================================
; codigo nuevo: 10 caracteres de un alfabeto de 32 (50 bits) a partir de AT+RAND=7
gen_code:   call cb_init
            ld   hl,s_rand7
            call cb_puts
            ld   hl,V_CMD
            ld   de,TMO_S
            call at_cmd
            ret  nz
            ld   a,(V_RESPLEN)
            cp   20                 ; "+RAND:" + 14
            jr   z,gc_hex
            ld   a,3
            or   a
            ret
gc_hex:     ld   hl,V_RESP+6
            ld   de,V_RND
            ld   b,7
            call hex_dec
            jr   nc,gc_enc
            ld   a,3
            or   a
            ret
gc_enc:     xor  a
            ld   (V_I),a
gc_lp:      ld   a,(V_I)
            ld   b,a
            add  a,a
            add  a,a
            add  a,b                ; p = 5*i
            ld   c,a
            rrca
            rrca
            rrca
            and  1Fh                ; n = p / 8
            ld   e,a
            ld   d,0
            ld   hl,V_RND
            add  hl,de
            ld   d,(hl)
            inc  hl
            ld   e,(hl)             ; DE = dos bytes seguidos
            ld   a,c
            and  7
            ld   b,a
            ld   a,11
            sub  b
            ld   b,a                ; desplazamiento 11 - (p mod 8)
gc_sh:      srl  d
            rr   e
            djnz gc_sh
            ld   a,e
            and  1Fh
            ld   e,a
            ld   d,0
            ld   hl,s_alpha
            add  hl,de
            ld   a,(hl)
            ld   hl,V_CODE
            ld   de,(V_I)
            ld   d,0
            add  hl,de
            ld   (hl),a
            ld   a,(V_I)
            inc  a
            ld   (V_I),a
            cp   10
            jr   c,gc_lp
            ld   hl,V_CODE+10
            ld   (hl),0
            xor  a
            ret

; V_CODE (ASCII) -> NB_CODE81 (codigos de caracter del ZX81)
code_to_81: ld   hl,V_CODE
            ld   de,NB_CODE81
            ld   b,10
c81_lp:     ld   a,(hl)
            inc  hl
            cp   'A'
            jr   c,c81_dig
            sub  27                 ; 'A' (65) -> 38
            jr   c81_st
c81_dig:    sub  20                 ; '0' (48) -> 28
c81_st:     ld   (de),a
            inc  de
            djnz c81_lp
            ret

; NB_CODE81 -> V_CODE (ASCII); A = 0 bien, 4 si algun caracter no esta en el alfabeto
code_from_81: ld hl,NB_CODE81
            ld   de,V_CODE
            ld   b,10
cf_lp:      ld   a,(hl)
            inc  hl
            cp   38
            jr   c,cf_dig
            add  a,27               ; 38 -> 'A'
            jr   cf_chk
cf_dig:     add  a,20               ; 28 -> '0'
cf_chk:     ld   (V_I),a
            push hl
            push de
            push bc
            ld   hl,s_alpha
            ld   b,32
cf_s:       ld   a,(V_I)
            cp   (hl)
            jr   z,cf_found
            inc  hl
            djnz cf_s
            pop  bc
            pop  de
            pop  hl
            ld   a,4
            or   a
            ret
cf_found:   pop  bc
            pop  de
            pop  hl
            ld   a,(V_I)
            ld   (de),a
            inc  de
            djnz cf_lp
            xor  a
            ld   (de),a
            ret

; V_CODE -> topic V_BASE ("sd81/" + 12 hex de SHA256("topic"+codigo")) y clave V_WK (64 hex de SHA256("key"+codigo))
derive:     ld   hl,s_topic
            ld   de,V_SCR
            ld   bc,5
            ldir
            ld   hl,V_CODE
            ld   bc,10
            ldir
            ld   hl,V_SCR
            ld   e,15
            call sha256_of
            ret  nz
            ld   hl,s_sd81
            ld   de,V_BASE
            ld   bc,5
            ldir
            ld   hl,V_RESP+8
            ld   bc,12
            ldir
            xor  a
            ld   (de),a
            ld   hl,s_key
            ld   de,V_SCR
            ld   bc,3
            ldir
            ld   hl,V_CODE
            ld   bc,10
            ldir
            ld   hl,V_SCR
            ld   e,13
            call sha256_of
            ret  nz
            ld   hl,V_RESP+8
            ld   de,V_WK
            ld   bc,64
            ldir
            xor  a
            ret

; =====================================================================
;  Armar los comandos de la sala
; =====================================================================
; V_BASE + "/" (+ "p" si A = 'p') + ranura
bd_topic:   push af
            ld   hl,V_BASE
            call cb_puts
            ld   a,'/'
            call cb_putc
            pop  af
            or   a
            jr   z,bt_slot
            call cb_putc
bt_slot:    ld   a,(NB_SLOT)
            add  a,'0'
            jp   cb_putc

; =====================================================================
;  Entrar en la sala (soy NB_SLOT; V_CODE ya esta)
; =====================================================================
room_go:    call derive
            ret  nz
room_go_d:  ; last will: mi presencia a 0
            call cb_init
            ld   hl,s_will
            call cb_puts
            ld   a,'p'
            call bd_topic
            ld   hl,s_will_t
            call cb_puts
            ld   hl,V_CMD
            ld   de,TMO_S
            call at_cmd
            ret  nz
            ; conectar a un broker de /SYS/NET.CFG o a los de siempre
            ld   hl,s_mqtt
            ld   de,TMO_L
            call at_cmd
            ret  nz
rg_suball:  ; suscribirme a todo lo de la sala
            call cb_init
            ld   hl,s_sub
            call cb_puts
            ld   hl,V_BASE
            call cb_puts
            ld   hl,s_sub_t
            call cb_puts
            ld   hl,V_CMD
            ld   de,TMO_S
            call at_cmd
            ret  nz
            ; mi presencia a 1
            ld   a,'1'
            call pub_presence
            ret  nz
            ; empiezo mi secuencia en un punto cualquiera
            ld   hl,s_rand2
            ld   de,TMO_S
            call at_cmd
            jr   nz,rg_noseq
            ld   a,(V_RESPLEN)
            cp   10                 ; "+RAND:" + 4
            jr   nz,rg_noseq
            ld   hl,V_RESP+6
            ld   de,V_SEQ
            ld   b,2
            call hex_dec
rg_noseq:   xor  a
            ld   (V_PSEQV),a
            ld   (NB_PEER),a
            ret

; Sonda de la sala (ya conectado, sin last will): me suscribo a <sala>/p0 y <sala>/p1 y miro las
; presencias retenidas. Decide mi ranura (NB_SLOT):
;   creador (0) presente y la 1 libre  -> soy el 2 (NB_SLOT = 1), A = 0
;   creador ausente y la 1 ocupada     -> reentro como creador (NB_SLOT = 0), A = 0
;   las dos ocupadas                   -> A = 6 (sala llena)
;   las dos libres                     -> A = 7 (esa sala no existe)
; Cualquier otro valor de A es un error del modem. La sesion la cierra quien llama.
room_probe: call cb_init
            ld   hl,s_sub
            call cb_puts
            ld   hl,V_BASE
            call cb_puts
            ld   hl,s_pone
            call cb_puts
            ld   hl,V_CMD
            ld   de,TMO_S
            call at_cmd
            ret  nz                 ; error (1, 2 o 3)
            call cb_init
            ld   hl,s_sub
            call cb_puts
            ld   hl,V_BASE
            call cb_puts
            ld   hl,s_pzero
            call cb_puts
            ld   hl,V_CMD
            ld   de,TMO_S
            call at_cmd
            ret  nz
            xor  a
            ld   (V_P0),a
            ld   (V_P1),a
            ld   e,6                ; los retenidos llegan justo detras del SUBACK: un momento
rp_w1:      call delay20
            dec  e
            jr   nz,rp_w1
            ld   a,1
            ld   (V_CHK),a
            call room_poll          ; apunta V_P0 y V_P1; 0 si no hay mas
            or   a
            jr   nz,rp_end          ; 4: error del modem
            ld   a,(V_P0)
            cp   '1'
            jr   z,rp_decide        ; el creador esta: ya se lo que hay
            ld   e,10               ; si no, puede que lo retenido aun no haya llegado: un poco mas
rp_w2:      call delay20
            dec  e
            jr   nz,rp_w2
            call room_poll
            or   a
            jr   nz,rp_end
rp_decide:  ld   a,(V_P0)
            cp   '1'
            jr   nz,rp_nocr
            ld   a,(V_P1)
            cp   '1'
            ld   a,6                ; las dos ocupadas
            jr   z,rp_end
            ld   a,1                ; el creador esta y la 1 libre: soy el 2
            ld   (NB_SLOT),a
            xor  a
            jr   rp_end
rp_nocr:    ld   a,(V_P1)
            cp   '1'
            jr   z,rp_re0           ; el rival esta y el creador no: vuelvo a ser el creador
            ld   a,(V_P0)           ; ninguna ocupada: sin ningun mensaje de presencia (ni siquiera un '0'
            ld   b,a                ; de los que se dejan al salir) es que nadie la ha creado
            ld   a,(V_P1)
            or   b
            ld   a,7
            jr   z,rp_end
            ld   a,8                ; existio y esta vacia: que el usuario elija ranura (room_join_as)
            jr   rp_end
rp_re0:     xor  a
            ld   (NB_SLOT),a
rp_end:     push af
            xor  a
            ld   (V_CHK),a
            pop  af
            or   a
            ret

; A = '1' o '0': publica mi presencia retenida
pub_presence: ld (V_SCR),a
            ld   hl,V_SCR
            ld   (V_RAWP),hl
            ld   a,1
            ld   (V_RAWL),a
            call cb_init
            ld   hl,s_pub
            call cb_puts
            ld   a,'p'
            call bd_topic
            ld   hl,s_pub_t1
            call cb_puts
            ld   hl,V_CMD
            ld   de,TMO_S
            jp   at_raw

; =====================================================================
;  API
; =====================================================================
net_init:   xor  a
            ld   (V_RXN),a
            ld   (V_PEND),a
            call flush_rx
            ld   hl,s_atz
            ld   de,TMO_S
            call at_cmd
            jr   nz,ni_end
            ld   hl,s_ate0
            ld   de,TMO_S
            call at_cmd
ni_end:     jp   set_res

room_new:   xor  a
            ld   (NB_SLOT),a
            call gen_code
            jr   nz,set_res
            call code_to_81
            call room_go
            jp   set_res

room_join:  ld   a,1
            ld   (NB_SLOT),a
            call code_from_81
            jr   nz,set_res
            call derive
            jr   nz,set_res
            ld   hl,s_mqtt          ; primero una sonda: conectar sin last will y ver quien hay
            ld   de,TMO_L
            call at_cmd
            jr   nz,set_res
            call room_probe         ; elige mi ranura (el 2, o el creador que vuelve) o dice por que no
            push af
            ld   hl,s_close
            ld   de,TMO_S
            call at_cmd             ; cerrar bien: no hay last will que salte
            pop  af
            or   a
            jr   nz,set_res
            call room_go_d          ; y entro de verdad, con el will de mi ranura
            jp   set_res

; entra en la ranura A (0 = el creador, 1 = el otro) sin sonda: para el caso 8 de room_join
room_join_as: ld  (NB_SLOT),a
            call code_from_81
            jr   nz,set_res
            call room_go
            jp   set_res

room_leave: ld   a,'0'
            call pub_presence
            ld   hl,s_close
            ld   de,TMO_S
            call at_cmd
            jp   set_res

set_res:    ld   (NB_RES),a
            ret

; --- mandar NB_LEN bytes de NB_TX con el turno NB_TURN
room_send:  ld   a,(NB_LEN)
            cp   101
            jr   c,rs_ok
            ld   a,5
            jp   set_res
rs_ok:      ld   hl,V_WK+64
            ld   (hl),1             ; version
            inc  hl
            ld   de,(V_SEQ)
            inc  de
            ld   (V_SEQ),de
            ld   (hl),e
            inc  hl
            ld   (hl),d
            inc  hl
            ld   de,(NB_TURN)
            ld   (hl),e
            inc  hl
            ld   (hl),d
            ld   a,(NB_LEN)
            add  a,5
            ld   (V_N),a
            ld   a,(NB_LEN)
            or   a
            jr   z,rs_mac
            ld   c,a
            ld   b,0
            ld   hl,NB_TX
            ld   de,V_WK+69
            ldir
rs_mac:     call hmac_wk
            jr   nz,set_res
            ld   a,(V_N)
            ld   e,a
            ld   d,0
            ld   hl,V_WK+64
            add  hl,de
            ex   de,hl              ; DE = donde va el MAC
            ld   hl,V_MAC
            ld   bc,8
            ldir
            ld   a,(V_N)
            add  a,8
            ld   (V_RAWL),a
            ld   hl,V_WK+64
            ld   (V_RAWP),hl
            call cb_init
            ld   hl,s_pub
            call cb_puts
            xor  a
            call bd_topic
            ld   hl,s_pub_t2
            call cb_puts
            ld   a,(V_RAWL)
            call cb_dec
            ld   hl,V_CMD
            ld   de,TMO_S
            call at_raw
            jp   set_res

; --- ha llegado algo?
room_poll:  call room_poll_send
            jp   room_poll_recv

room_poll_send: call cb_init
            ld   hl,s_rx
            call cb_puts
            ld   hl,V_CMD
            call tx_str
            call tx_cr
            ld   a,1
            ld   (V_PEND),a
            xor  a
            jp   set_res

room_poll_recv: ld a,(V_PEND)
            or   a
            jr   z,room_poll        ; nada pedido: pregunta y espera
            xor  a
            ld   (V_PEND),a
            ld   de,TMO_S
            ld   (V_TMO),de
rp_line:    call rx_line
            jr   c,rp_timeout
            or   a
            jr   z,rp_line
            ld   hl,V_LINE
            ld   de,s_ok
            call str_eq
            jr   z,rp_none
            ld   hl,V_LINE
            ld   de,s_error
            call str_eq
            jr   z,rp_err
            call parse_rx
            jr   c,rp_bad
            call rx_get             ; el LF que sigue al CR de la linea (rx_line corta en el CR)
            jr   c,rp_timeout
            ; los bytes del mensaje, a V_WK+64
            ld   a,(V_RXLEN)
            or   a
            jr   z,rp_drain
            cp   121
            jr   nc,rp_bad
            ld   b,a
            ld   hl,V_WK+64
rp_data:    push hl
            push bc
            call rx_get
            pop  bc
            pop  hl
            jr   c,rp_timeout
            ld   (hl),a
            inc  hl
            djnz rp_data
rp_drain:   call rx_line            ; hasta el OK final
            jr   c,rp_timeout
            or   a
            jr   z,rp_drain
            ld   hl,V_LINE
            ld   de,s_ok
            call str_eq
            jr   nz,rp_drain
            jp   rp_classify
rp_none:    xor  a
            jp   set_res
rp_err:     ld   a,4
            jp   set_res
rp_timeout: ld   a,4
            jp   set_res
rp_bad:     ld   a,4
            jp   set_res

; clasificar lo recibido (V_TOPIC, V_RXLEN, datos en V_WK+64)
rp_classify: ld  hl,V_TOPIC
            ld   de,V_BASE
            ld   b,17
rc_pre:     ld   a,(de)
            cp   (hl)
            jp   nz,room_poll       ; otra sala: se ignora
            inc  hl
            inc  de
            djnz rc_pre
            ld   a,(hl)
            cp   '/'
            jp   nz,room_poll
            inc  hl
            ld   a,(hl)
            cp   'p'
            jr   z,rc_pres
            sub  '0'
            ld   b,a
            ld   a,(NB_SLOT)
            cp   b
            jp   z,room_poll        ; el eco de lo mio
            jr   rc_msg
rc_pres:    inc  hl
            ld   a,(hl)
            sub  '0'
            ld   b,a
            ld   a,(V_CHK)
            or   a
            jr   z,rc_pres2
            ld   a,b                ; sondeando la sala: apunto las presencias de las dos ranuras
            or   a
            ld   a,(V_WK+64)
            jr   nz,rc_chk1
            ld   (V_P0),a           ; '1' ocupada, '0' libre
            jp   room_poll
rc_chk1:    ld   (V_P1),a
            jp   room_poll
rc_pres2:   ld   a,(NB_SLOT)
            cp   b
            jp   z,room_poll        ; mi propia presencia
            ld   a,(V_WK+64)
            cp   '1'
            jr   z,rc_in
            xor  a
            ld   (NB_PEER),a
            ld   a,3
            jp   set_res
rc_in:      ld   a,1
            ld   (NB_PEER),a
            ld   a,2
            jp   set_res

rc_msg:     ld   a,(V_RXLEN)
            cp   13                 ; 5 de cabecera + 8 de MAC como minimo
            jp   c,room_poll
            sub  8
            ld   (V_N),a
            call hmac_wk
            jp   nz,room_poll
            ld   a,(V_N)
            ld   e,a
            ld   d,0
            ld   hl,V_WK+64
            add  hl,de              ; HL = MAC recibido
            ld   de,V_MAC
            ld   b,8
rc_cmp:     ld   a,(de)
            cp   (hl)
            jp   nz,room_poll       ; firma mala: se tira
            inc  hl
            inc  de
            djnz rc_cmp
            ld   a,(V_WK+64)
            cp   1
            jp   nz,room_poll       ; version desconocida
            ld   hl,(V_WK+65)       ; secuencia
            ld   a,(V_PSEQV)
            or   a
            jr   z,rc_new
            ld   de,(V_PSEQ)
            or   a
            sbc  hl,de
            jp   z,room_poll        ; repetido
            ld   hl,(V_WK+65)
rc_new:     ld   (V_PSEQ),hl
            ld   a,1
            ld   (V_PSEQV),a
            ld   hl,(V_WK+67)
            ld   (NB_TURN),hl
            ld   a,(V_N)
            sub  5
            ld   (NB_LEN),a
            or   a
            jr   z,rc_done
            cp   101
            jr   c,rc_cp
            ld   a,100
            ld   (NB_LEN),a
rc_cp:      ld   c,a
            ld   b,0
            ld   hl,V_WK+69
            ld   de,NB_RX
            ldir
rc_done:    ld   a,1
            jp   set_res

; "+MQTTRX:topic,len" en V_LINE -> V_TOPIC y V_RXLEN; CF si no es asi
parse_rx:   ld   hl,V_LINE
            ld   de,s_rxp
pr_pre:     ld   a,(de)
            or   a
            jr   z,pr_top
            cp   (hl)
            jr   nz,pr_bad
            inc  hl
            inc  de
            jr   pr_pre
pr_top:     ld   de,V_TOPIC
            ld   b,40
pr_t:       ld   a,(hl)
            or   a
            jr   z,pr_bad
            inc  hl
            cp   ','
            jr   z,pr_len
            ld   (de),a
            inc  de
            djnz pr_t
            jr   pr_bad
pr_len:     xor  a
            ld   (de),a
pr_d:       ld   c,a
            ld   a,(hl)
            or   a
            jr   z,pr_done
            sub  '0'
            cp   10
            jr   nc,pr_bad
            ld   b,a
            ld   a,c
            add  a,a
            ld   c,a
            add  a,a
            add  a,a
            add  a,c
            add  a,b
            inc  hl
            jr   pr_d
pr_done:    ld   a,c
            ld   (V_RXLEN),a
            or   a
            ret
pr_bad:     scf
            ret

; =====================================================================
;  Textos
; =====================================================================
s_cr:       defb 13
s_ok:       defm "OK"
            defb 0
s_error:    defm "ERROR"
            defb 0
s_atz:      defm "ATZ"
            defb 0
s_ate0:     defm "ATE0"
            defb 0
s_mqtt:     defm "AT+MQTT"
            defb 0
s_close:    defm "AT+MQTTCLOSE"
            defb 0
s_rx:       defm "AT+MQTTRX"
            defb 0
s_rxp:      defm "+MQTTRX:"
            defb 0
s_rand7:    defm "AT+RAND=7"
            defb 0
s_rand2:    defm "AT+RAND=2"
            defb 0
s_sha:      defm "AT+SHA256="
            defb 0
s_hmac:     defm "AT+HMAC=64,"
            defb 0
s_will:     defm "AT+MQTTWILL="
            defb 0
s_will_t:   defm ",1,0"
            defb 0
s_sub:      defm "AT+MQTTSUB="
            defb 0
s_sub_t:    defm "/+,1"
            defb 0
s_pone:     defm "/p1,1"
            defb 0
s_pone2:    defm "/p1"
            defb 0
s_pzero:    defm "/p0,1"
            defb 0
s_pzero2:   defm "/p0"
            defb 0
s_unsub:    defm "AT+MQTTUNSUB="
            defb 0
s_pub:      defm "AT+MQTTPUB="
            defb 0
s_pub_t1:   defm ",1,1,1"
            defb 0
s_pub_t2:   defm ",1,1,"
            defb 0
s_topic:    defm "topic"
s_sd81:     defm "sd81/"
s_key:      defm "key"
s_alpha:    defm "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
