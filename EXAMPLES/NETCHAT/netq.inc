; =====================================================================
;  NETQ.INC - mensajes de red SIN ESPERAS, para juegos en tiempo real (un Pong, por ejemplo). Se incluye DESPUES
;  de net.inc, que pone la sala (room_new / room_join) y las rutinas del modem; esta capa solo pasa mensajes cortos
;  entre los dos jugadores sin dejar nunca al Z80 esperando una respuesta del modem:
;
;    - nq_send mete un AT+MQTTPUB y su dato en una cola (en RAM del Z80); no espera el '>' ni el OK;
;    - nq_pump, una vez por fotograma: añade un AT+MQTTRX si no hay ya dos pedidos sin contestar, vuelca la cola al
;      tubo del MCU (NET_WRITE, lo que acepte) y lee lo que haya llegado (NET_READ), que un analizador de lineas
;      desmenuza: '>' y OK/ERROR se cuentan contra una cola de lo mandado, +MQTTRX:topic,n deja los n bytes
;      siguientes en un buzon.
;    - nq_get da el ultimo mensaje completo del rival (A = longitud; 0 = ninguno).
;  El coste por fotograma son unos pocos comandos al MCU (0,25 ms cada uno vacios, ~0,05 ms por byte). Nada firma ni
;  comprueba los mensajes (sin HMAC): el topic va ligado al codigo de la sala, que es aleatorio. Es para jugar, no para
;  guardar secretos.
;
;  El que incluye este fichero define ANTES:   NQ_BASE equ ...   (592 bytes de RAM de trabajo, fuera del codigo)
;  Rutinas (CALL; destruyen todos los registros):
;      nq_open     (con espera, una vez, con la sala ya creada o unida) deja la suscripcion solo al topic del RIVAL
;                  (<sala>/<su ranura>), para no recibir el eco de lo propio ni la presencia; tira lo pendiente. A = 0 Z bien.
;      nq_init     pone a cero el estado de esta capa (nq_open ya lo hace).
;      nq_send     HL = datos, A = longitud (1-100). CF = 1 si no se ha podido encolar (cola ocupada: se pierde ese mensaje).
;      nq_pump     el trabajo de cada fotograma (ver arriba).
;      nq_get      A = longitud del ultimo mensaje del rival (0 = ninguno nuevo), HL = NQ_RXBUF. Lo da una sola vez.
;      nq_close    (con espera, hasta ~1,2 s) al acabar y ANTES de volver a usar net.inc: deja de pedir y espera a que el modem conteste lo pendiente.
;  Contadores utiles (16 bits, little endian): NQ_NRX recibidos, NQ_NTX enviados; (8 bits) NQ_DROP mensajes perdidos por
;  cola ocupada, NQ_ERR respuestas ERROR del modem, NQ_LOST veces que se vacio la cola por no contestar el modem.
; =====================================================================

NQ_ASKMAX   equ 4               ; AT+MQTTRX pedidos sin contestar como maximo
NQ_WDMAX    equ 50              ; fotogramas (1 s) sin respuesta antes de dar la cola por perdida: una respuesta tardia no hace dano (se ignora)

NQ_TXN      equ NQ_BASE+0       ; bytes que quedan por volcar al tubo
NQ_TXP      equ NQ_BASE+1       ; (2) el siguiente
NQ_FC       equ NQ_BASE+3       ; lo mandado y sin contestar: cuantos
NQ_FH       equ NQ_BASE+4       ; su primero (indice 0-7)
NQ_ASKS     equ NQ_BASE+5       ; de ellos, AT+MQTTRX
NQ_ST       equ NQ_BASE+6       ; analizador: 0 lineas, 1 datos de un mensaje
NQ_LL       equ NQ_BASE+7       ; longitud de la linea en curso
NQ_DLEFT    equ NQ_BASE+8       ; bytes que faltan del mensaje en curso
NQ_DP       equ NQ_BASE+9       ; (2) donde van
NQ_PLEN     equ NQ_BASE+11      ; longitud del mensaje en curso
NQ_RXF      equ NQ_BASE+12      ; 1 si hay un mensaje completo sin recoger
NQ_RXLEN    equ NQ_BASE+13
NQ_WD       equ NQ_BASE+14      ; (2) fotogramas sin respuesta
NQ_NRX      equ NQ_BASE+16      ; (2)
NQ_NTX      equ NQ_BASE+18      ; (2)
NQ_ERR      equ NQ_BASE+20
NQ_DROP     equ NQ_BASE+21
NQ_LOST     equ NQ_BASE+22
NQ_SLEN     equ NQ_BASE+23      ; nq_send: longitud
NQ_SDP      equ NQ_BASE+24      ; (2) nq_send: datos
NQ_RDC      equ NQ_BASE+26      ; nq_pump: lo leido
NQ_RDN      equ NQ_BASE+27      ; nq_pump: vueltas de lectura que quedan
NQ_QUIET    equ NQ_BASE+28      ; 1 = no pedir mas (nq_close)
NQ_FIFO     equ NQ_BASE+32      ; (8) 1 = AT+MQTTPUB, 2 = AT+MQTTRX
NQ_LINE     equ NQ_BASE+40      ; (64) linea en curso
NQ_RD       equ NQ_BASE+104     ; (64) lo que trae NET_READ
NQ_PBUF     equ NQ_BASE+168     ; (120) mensaje en curso
NQ_RXBUF    equ NQ_BASE+288     ; (120) ultimo mensaje completo
NQ_TX       equ NQ_BASE+416     ; (176) cola de salida
NQ_END      equ NQ_BASE+592
NQ_TXMAX    equ 176

; ---------------------------------------------------------------------
nq_init:    ld   hl,NQ_BASE
            ld   de,NQ_BASE+1
            ld   bc,NQ_END-NQ_BASE-1
            ld   (hl),0
            ldir
            ret

; Con espera (una vez): dejar solo el topic del rival
nq_open:    call nq_init
            call flush_rx
            call cb_init            ; AT+MQTTUNSUB=<sala>/+
            ld   hl,s_unsub
            call cb_puts
            ld   hl,V_BASE
            call cb_puts
            ld   hl,nq_s_plus
            call cb_puts
            ld   hl,V_CMD
            ld   de,TMO_S
            call at_cmd
            ret  nz
            call cb_init            ; AT+MQTTSUB=<sala>/<ranura del rival>,0
            ld   hl,s_sub
            call cb_puts
            ld   hl,V_BASE
            call cb_puts
            ld   a,'/'
            call cb_putc
            ld   a,(NB_SLOT)
            xor  1
            add  a,'0'
            call cb_putc
            ld   hl,nq_s_q0
            call cb_puts
            ld   hl,V_CMD
            ld   de,TMO_S
            call at_cmd
            ret  nz
            ld   b,12               ; tira lo que ya hubiera en la cola del modem
nqo_dr:     push bc
            ld   hl,s_rx
            ld   de,TMO_S
            call at_cmd
            pop  bc
            ld   a,(V_RESPLEN)
            or   a
            jr   z,nqo_ok
            djnz nqo_dr
nqo_ok:     xor  a
            ret

; ---------------------------------------------------------------------
;  Lo mandado y sin contestar (cola circular de 8)
; ---------------------------------------------------------------------
nq_fpush:   ld   c,a                ; A = 1 o 2
            ld   a,(NQ_FC)
            cp   8
            ret  nc
            ld   b,a
            ld   a,(NQ_FH)
            add  a,b
            and  7
            ld   e,a
            ld   d,0
            ld   hl,NQ_FIFO
            add  hl,de
            ld   (hl),c
            ld   hl,NQ_FC
            inc  (hl)
            ret

; A = el mas antiguo (0 si no hay)
nq_fpop:    ld   a,(NQ_FC)
            or   a
            ret  z
            dec  a
            ld   (NQ_FC),a
            ld   a,(NQ_FH)
            ld   e,a
            inc  a
            and  7
            ld   (NQ_FH),a
            ld   d,0
            ld   hl,NQ_FIFO
            add  hl,de
            ld   a,(hl)
            ret

; ---------------------------------------------------------------------
;  nq_send: HL = datos, A = longitud (1-100). CF = 1 si no cabe
; ---------------------------------------------------------------------
nq_send:    ld   (NQ_SLEN),a
            ld   (NQ_SDP),hl
            ld   a,(NQ_TXN)
            or   a
            jr   nz,ns_busy
            ld   a,(NQ_FC)
            cp   6
            jr   nc,ns_busy
            ld   hl,NQ_TX
            ld   (NQ_TXP),hl
            call cb_init            ; AT+MQTTPUB=<sala>/<mi ranura>,0,0,<n>
            ld   hl,s_pub
            call cb_puts
            xor  a
            call bd_topic
            ld   hl,nq_s_t00
            call cb_puts
            ld   a,(NQ_SLEN)
            call cb_dec
            ld   hl,V_CMD
            ld   de,NQ_TX
ns_cp:      ld   a,(hl)
            or   a
            jr   z,ns_cd
            ld   (de),a
            inc  hl
            inc  de
            jr   ns_cp
ns_cd:      ld   a,13
            ld   (de),a
            inc  de
            ld   hl,(NQ_SDP)
            ld   a,(NQ_SLEN)
            ld   c,a
            ld   b,0
            ldir
            ex   de,hl
            ld   de,NQ_TX
            or   a
            sbc  hl,de
            ld   a,l
            ld   (NQ_TXN),a
            ld   a,1
            call nq_fpush
            ld   hl,(NQ_NTX)
            inc  hl
            ld   (NQ_NTX),hl
            or   a                  ; CF = 0
            ret
ns_busy:    ld   hl,NQ_DROP
            inc  (hl)
            scf
            ret

; ---------------------------------------------------------------------
;  nq_pump: una vez por fotograma
; ---------------------------------------------------------------------
nq_pump:    ; 1) pedir lo que haya (AT+MQTTRX), hasta dos por llamada si no hay ya NQ_ASKMAX sin contestar: con un mensaje del
            ;    rival por fotograma, una peticion por fotograma no deja que la cola del modem se vacie y los mensajes llegan viejos
            ld   a,(NQ_QUIET)
            or   a
            jr   nz,np_tx
            call np_ask
            call np_ask
            ; 2) volcar la cola al tubo
np_tx:      ld   a,(NQ_TXN)
            or   a
            jr   z,np_rd0
            ld   b,a
            ld   hl,(NQ_TXP)
            call net_write          ; B = aceptados
            ld   a,b
            or   a
            jr   z,np_rd0
            ld   c,b
            ld   b,0
            ld   hl,(NQ_TXP)
            add  hl,bc
            ld   (NQ_TXP),hl
            ld   a,(NQ_TXN)
            sub  c
            ld   (NQ_TXN),a
            ; 3) leer lo que haya llegado
np_rd0:     ld   a,4
            ld   (NQ_RDN),a
np_rl:      ld   hl,NQ_RD
            ld   b,64
            call net_read
            ld   a,b
            or   a
            jr   z,np_wd
            ld   (NQ_RDC),a
            ld   hl,NQ_RD
np_f:       ld   a,(hl)
            push hl
            push bc
            call nq_feed
            pop  bc
            pop  hl
            inc  hl
            djnz np_f
            ld   a,(NQ_RDC)
            cp   64
            jr   nz,np_wd
            ld   hl,NQ_RDN
            dec  (hl)
            jr   nz,np_rl
            ; 4) perro guardian: si hay cosas mandadas y el modem no contesta en mucho tiempo, se vacia la cuenta
np_wd:      ld   a,(NQ_FC)
            or   a
            jr   z,np_wd0
            ld   hl,(NQ_WD)
            inc  hl
            ld   (NQ_WD),hl
            ld   de,NQ_WDMAX
            or   a
            sbc  hl,de
            ret  c
            xor  a
            ld   (NQ_FC),a
            ld   (NQ_FH),a
            ld   (NQ_ASKS),a
            ld   (NQ_ST),a
            ld   (NQ_LL),a
            ld   hl,NQ_LOST
            inc  (hl)
np_wd0:     ld   hl,0
            ld   (NQ_WD),hl
            ret

np_ask:
            ld   a,(NQ_ASKS)
            cp   NQ_ASKMAX
            ret  nc
            ld   a,(NQ_FC)
            cp   7
            ret  nc
            ld   a,(NQ_TXN)
            or   a
            jr   nz,na_a1
            ld   hl,NQ_TX
            ld   (NQ_TXP),hl
na_a1:      ld   hl,(NQ_TXP)
            ld   de,NQ_TX
            or   a
            sbc  hl,de
            ld   a,h
            or   l
            ret  nz                 ; ya se esta volcando una parte: no se mezcla
            ld   a,(NQ_TXN)
            cp   NQ_TXMAX-12
            ret  nc
            ld   e,a
            ld   d,0
            ld   hl,NQ_TX
            add  hl,de
            ex   de,hl              ; DE = al final de la cola
            ld   hl,s_rx
            ld   bc,9
            ldir
            ld   a,13
            ld   (de),a
            ld   a,(NQ_TXN)
            add  a,10
            ld   (NQ_TXN),a
            ld   a,2
            call nq_fpush
            ld   hl,NQ_ASKS
            inc  (hl)
            ret

; ---------------------------------------------------------------------
;  nq_close: ANTES de volver a usar el modem con net.inc (room_leave...): deja de pedir y espera (hasta ~1,2 s) a que el modem conteste
;  todo lo que se le habia mandado sin esperar; si no, sus '>' y OK atrasados se tomarian por la respuesta del siguiente comando AT.
;  Acaba tirando lo que quede (flush_rx).
; ---------------------------------------------------------------------
nq_close:   ld   a,1
            ld   (NQ_QUIET),a
            ld   b,60
nqc_lp:     push bc
            call nq_pump
            pop  bc
            ld   a,(NQ_FC)
            ld   hl,NQ_TXN
            or   (hl)
            jr   z,nqc_done
            push bc
            call delay20
            pop  bc
            djnz nqc_lp
nqc_done:   xor  a
            ld   (NQ_ST),a
            ld   (NQ_LL),a
            jp   flush_rx

; ---------------------------------------------------------------------
;  nq_get: A = longitud del ultimo mensaje (0 = ninguno nuevo), HL = NQ_RXBUF
; ---------------------------------------------------------------------
nq_get:     ld   a,(NQ_RXF)
            or   a
            ret  z
            xor  a
            ld   (NQ_RXF),a
            ld   hl,NQ_RXBUF
            ld   a,(NQ_RXLEN)
            ret

; ---------------------------------------------------------------------
;  El analizador: A = un byte del modem
;  Lineas terminadas en LF (los CR se ignoran; un '>' al principio de la linea tambien: el modem lo manda sin salto de
;  linea, pegado al OK que sigue). "OK" y "ERROR" contestan lo mas antiguo de la cola; "+MQTTRX:topic,n" va seguida de
;  los n bytes del mensaje.
; ---------------------------------------------------------------------
nq_feed:    ld   c,a
            ld   a,(NQ_ST)
            or   a
            jr   nz,nf_data
            ld   a,c
            cp   13
            ret  z
            cp   10
            jr   z,nf_eol
            ld   b,a
            ld   a,(NQ_LL)
            or   a
            jr   nz,nf_st
            ld   a,b
            cp   '>'
            ret  z
nf_st:      ld   a,(NQ_LL)
            cp   62
            ret  nc
            ld   e,a
            ld   d,0
            ld   hl,NQ_LINE
            add  hl,de
            ld   (hl),c
            inc  a
            ld   (NQ_LL),a
            ret
nf_data:    ld   hl,(NQ_DP)
            ld   (hl),c
            inc  hl
            ld   (NQ_DP),hl
            ld   hl,NQ_DLEFT
            dec  (hl)
            ret  nz
            xor  a                  ; mensaje completo: al buzon (el que se esta leyendo no se pisa)
            ld   (NQ_ST),a
            ld   a,(NQ_PLEN)
            ld   (NQ_RXLEN),a
            ld   c,a
            ld   b,0
            ld   hl,NQ_PBUF
            ld   de,NQ_RXBUF
            ldir
            ld   a,1
            ld   (NQ_RXF),a
            ld   hl,(NQ_NRX)
            inc  hl
            ld   (NQ_NRX),hl
            ret
nf_eol:     ld   a,(NQ_LL)
            or   a
            ret  z
            ld   e,a
            ld   d,0
            ld   hl,NQ_LINE
            add  hl,de
            ld   (hl),0
            xor  a
            ld   (NQ_LL),a
            ld   a,(NQ_LINE)
            cp   'O'
            jr   z,nf_ok
            cp   'E'
            jr   z,nf_err
            cp   '+'
            ret  nz
            ld   a,(NQ_LINE+1)
            cp   'M'
            ret  nz
            ld   hl,NQ_LINE         ; "+MQTTRX:topic,n": n = lo que sigue a la ultima coma
            ld   de,0
nf_s:       ld   a,(hl)
            or   a
            jr   z,nf_sd
            inc  hl
            cp   ','
            jr   nz,nf_s
            ld   d,h
            ld   e,l
            jr   nf_s
nf_sd:      ld   a,d
            or   e
            ret  z
            ex   de,hl
            ld   c,0
nf_n:       ld   a,(hl)
            or   a
            jr   z,nf_nd
            sub  '0'
            cp   10
            ret  nc
            ld   b,a
            ld   a,c
            add  a,a
            ld   c,a
            add  a,a
            add  a,a
            add  a,c
            add  a,b
            ld   c,a
            inc  hl
            jr   nf_n
nf_nd:      ld   a,c
            or   a
            ret  z
            cp   121
            ret  nc
            ld   (NQ_PLEN),a
            ld   (NQ_DLEFT),a
            ld   hl,NQ_PBUF
            ld   (NQ_DP),hl
            ld   a,1
            ld   (NQ_ST),a
            ret
nf_err:     ld   hl,NQ_ERR
            inc  (hl)
nf_ok:      call nq_fpop
            cp   2
            jr   nz,nf_okw
            ld   hl,NQ_ASKS
            ld   a,(hl)
            or   a
            jr   z,nf_okw
            dec  (hl)
nf_okw:     ld   hl,0
            ld   (NQ_WD),hl
            ret

nq_s_plus:  defm "/+",0
nq_s_q0:    defm ",0",0
nq_s_t00:   defm ",0,0,",0
