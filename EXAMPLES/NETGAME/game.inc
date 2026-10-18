; =====================================================================
;  GAME.INC - capa C del juego en red: el controlador comun de una partida
;  por turnos entre dos ZX81, encima de net.inc (la sala y los mensajes
;  firmados). No sabe nada de las reglas: las pone el juego con seis rutinas.
;  Diseno: claude/planning/juego_en_red.md, seccion 5.
;
;  El que incluye este fichero define ANTES:
;      GM_BASE      equ 7400h   ; RAM de trabajo (unos 100 bytes)
;      G_STATE_LEN  equ 42      ; bytes del estado completo del juego (<= 90)
;      G_MOVE_LEN   equ 1       ; bytes de una jugada (1 o 2)
;      G_MOVE_VAR   equ 0       ; 1 = jugadas de longitud variable (el Scrabble): G_MOVE_LEN es el MAXIMO, el primer byte de la
;                               jugada es su longitud (el mismo, contado entero) y se manda solo esa parte. No va con G_CLOCK
;      G_BIG        equ 0       ; 1 = estado grande (el Scrabble: no cabe en un mensaje de 100 bytes): el estado se manda en trozos
;                               de 96 bytes y el juego pone g_hash (HL = resumen de 16 bits del estado, sin copiarlo; el hash
;                               de siempre pasa por g_save y G_STATE_LEN <= 90). No va con G_CLOCK
;  y despues  include "net.inc"  y  include "game.inc"; el juego define:
;      g_reset      empieza una partida (posicion inicial)
;      g_valid      HL = la jugada (G_MOVE_LEN bytes), B = jugador (1 o 2): CF = 1 si NO es legal
;      g_apply      HL = la jugada, B = jugador: la hace (ya se sabe que es legal)
;      g_winner     A = 0 sigue, 1 o 2 gana ese jugador, 3 tablas
;      g_save       HL = destino: escribe los G_STATE_LEN bytes del estado
;      g_load       HL = origen: lee los G_STATE_LEN bytes y rehace lo derivado
;  Las jugadas son de G_MOVE_LEN bytes. Empieza el jugador 1 (el que crea la sala);
;  mueve el 1 con un numero de jugada par y el 2 con uno impar.
;
;  Rutinas (CALL; destruyen todos los registros):
;      game_start    tras room_new / room_join: yo soy el jugador NB_SLOT+1
;      game_new      partida nueva (sin avisar al rival)
;      game_rematch  partida nueva y se lo dice al rival
;      game_turn_of  A = el jugador al que le toca (1 o 2)
;      game_move     A = mi jugada (juegos de una jugada de 1 byte); game_move_p: HL = la
;                    jugada (G_MOVE_LEN bytes). Devuelve A = 0 hecha y enviada, 1 no es legal,
;                    2 no me toca, 3 la partida ha acabado, 9 error de red (la
;                    jugada esta hecha en local: se puede repetir con game_resend)
;      game_resend   vuelve a mandar mi ultima jugada
;      game_poll     una vuelta de sondeo (usa room_poll_recv: se puede pedir antes
;                    con room_poll_send). Devuelve A =
;                      0 nada                    1 el rival ha jugado (GM_MOVE)
;                      2 el rival ha entrado     3 el rival se ha ido
;                      4 estado completo recibido (se ha vuelto a sincronizar)
;                      5 desincronizado: he pedido el estado   7 error del modem
;                      6 al rival se le ha acabado el tiempo (he ganado; solo con G_CLOCK)
;                      8 el rival quiere otra partida (ya esta puesta)
;  Variables: GM_ME (mi numero de jugador), GM_PLY (2 bytes: jugadas hechas),
;  GM_OVER (0 sigue, 1 o 2 ha ganado, 3 tablas), GM_MOVE (G_MOVE_LEN bytes: la ultima jugada,
;  mia o del rival).
;
;  Mensajes (payload; el turno va en NB_TURN = numero de jugada, que ya firma net.inc):
;      1 jugada     [1][jugada: G_MOVE_LEN bytes][hash 2 bytes del estado DESPUES de la jugada]
;                   (con G_CLOCK: + [segundos que le quedan al que juega, 2 bytes])
;      2 pido estado [2]
;      3 estado     [3][G_STATE_LEN bytes] (con G_CLOCK: + los segundos del jugador 1 y del 2, 4 bytes)
;      4 otra partida [4]
;      5 se me ha acabado el tiempo [5] (solo con G_CLOCK)
;  Cualquier desfase (una jugada que no es la siguiente, ilegal, o con otro hash)
;  se arregla pidiendo el estado completo. Las jugadas viejas (un retenido) se ignoran.
; =====================================================================

GM_ME       equ GM_BASE+0
GM_PLY      equ GM_BASE+1       ; (2)
GM_OVER     equ GM_BASE+3
GM_HASH     equ GM_BASE+6       ; (2)
GM_TMP      equ GM_BASE+8       ; (G_STATE_LEN) estado para el resumen
GM_NEW      equ GM_TMP+G_STATE_LEN ; (G_MOVE_LEN) la jugada que se intenta (GM_MOVE solo cambia si es legal)
GM_BX       equ GM_NEW+G_MOVE_LEN+24 ; (con G_BIG) variables del estado por trozos, detras de las del reloj
GM_CI       equ GM_BX+0         ; el trozo que se manda
GM_CL       equ GM_BX+1         ; su longitud
GM_CP       equ GM_BX+2         ; (2) de donde sale
GM_CR       equ GM_BX+4         ; (2) bytes que faltan por mandar
GM_RXT      equ GM_BX+6         ; (2) el turno del estado que se esta recibiendo
GM_RXM      equ GM_BX+8         ; los trozos recibidos (un bit cada uno)
G_NCHUNK    equ (G_STATE_LEN+95)/96
G_LASTLEN   equ G_STATE_LEN-96*(G_NCHUNK-1)   ; bytes del ultimo trozo
IF G_MOVE_VAR
GM_MOVE     equ GM_BX+16        ; (G_MOVE_LEN) la jugada: con longitud variable no cabe en el hueco de 2 bytes de siempre
ELSE
GM_MOVE     equ GM_BASE+4       ; (2) la jugada
ENDIF
G_CLKB      equ G_CLOCK*2       ; bytes de reloj en cada jugada: los segundos del que la hace (solo con G_CLOCK = 1)
IF G_CLOCK
GM_TC       equ GM_NEW+G_MOVE_LEN   ; control de tiempo (indice de gm_tc_tab; 0 = sin reloj)
GM_REM      equ GM_TC+1             ; (4) segundos que le quedan al jugador 1 y al 2
GM_CKON     equ GM_REM+4            ; 1 si los relojes corren
GM_WHY      equ GM_CKON+1           ; 1 si la partida ha acabado por tiempo
GM_MKM      equ GM_WHY+1            ; (2) minutos del dia de la ultima marca
GM_MKS      equ GM_MKM+2            ; segundos de la ultima marca
GM_NOWM     equ GM_MKS+1            ; (2) la hora que acabo de leer: minutos del dia
GM_NOWS     equ GM_NOWM+2           ; y segundos
ENDIF

game_start: ld   a,(NB_SLOT)
            inc  a
            ld   (GM_ME),a
IF G_CLOCK
            call gm_tc_code
ENDIF
            jp   game_new

game_new:   xor  a
            ld   (GM_PLY),a
            ld   (GM_PLY+1),a
            ld   (GM_OVER),a
IF G_BIG
            ld   (GM_RXM),a
            ld   hl,0FFFFh
            ld   (GM_RXT),hl
ENDIF
IF G_CLOCK
            call gm_clk_reset
ENDIF
            jp   g_reset

game_rematch: call gm_drain
            ld   a,4
            ld   (NB_TX),a
            ld   a,1
            ld   (NB_LEN),a
            ld   hl,(GM_PLY)
            ld   (NB_TURN),hl
            call room_send
            push af
            call game_new
IF G_CLOCK
            call game_clock_go
ENDIF
            pop  af
            ret

game_turn_of: ld a,(GM_PLY)
            and  1
            inc  a
            ret

; Si el sondeo (room_poll_send) ha pedido un AT+MQTTRX y aun no se ha recogido, hay que recogerlo ANTES
; de mandar nada: el siguiente comando AT tira lo que traiga el modem (y se perderia una jugada del rival, o
; room_poll_recv esperaria unos segundos una respuesta que ya no llega: "MODEM ERROR"). game_poll la recoge
; y la procesa; su resultado se ignora (lo que cambie se ve al repintar).
gm_drain:   ld   a,(V_PEND)
            or   a
            ret  z
            jp   game_poll

gm_ply_inc: ld   hl,(GM_PLY)
            inc  hl
            ld   (GM_PLY),hl
            ret

; HL = resumen de 16 bits del estado (rota y mezcla cada byte)
IF G_BIG
game_hash:  jp   g_hash
ELSE
game_hash:  ld   hl,GM_TMP
            call g_save
            ld   hl,GM_TMP
            ld   b,G_STATE_LEN
            ld   de,5A5Ah
gh_lp:      ld   a,(hl)
            inc  hl
            xor  e
            ld   e,a
            ld   c,3
gh_rot:     sla  e
            rl   d
            jr   nc,gh_nc
            set  0,e
gh_nc:      dec  c
            jr   nz,gh_rot
            djnz gh_lp
            ex   de,hl
            ret
ENDIF

game_move:  ld   (GM_NEW),a            ; juegos de una jugada de 1 byte
            jr   gm_have
game_move_p: ld  de,GM_NEW          ; HL = la jugada
IF G_MOVE_VAR
            ld   a,(hl)             ; su longitud
            cp   2
            jr   c,gm_badlen
            cp   G_MOVE_LEN+1
            jr   nc,gm_badlen
            ld   c,a
            ld   b,0
ELSE
            ld   bc,G_MOVE_LEN
ENDIF
            ldir
            jr   gm_have
IF G_MOVE_VAR
gm_badlen:  ld   a,1                ; una longitud imposible: no es legal
            ret
ENDIF
gm_have:    call gm_drain
            ld   a,(GM_OVER)
            or   a
            jr   z,gm_o1
            ld   a,3
            ret
gm_o1:      call game_turn_of
            ld   hl,GM_ME
            cp   (hl)
            jr   z,gm_o2
            ld   a,2
            ret
gm_o2:      ld   a,(GM_ME)
            ld   b,a
            ld   hl,GM_NEW
            call g_valid
            jr   nc,gm_ok
            ld   a,1
            ret
gm_ok:      ld   hl,GM_NEW          ; es legal: ahora si es la ultima jugada (y la que reenvia 9)
            ld   de,GM_MOVE
            ld   bc,G_MOVE_LEN
            ldir
            ld   a,(GM_ME)
            ld   b,a
            ld   hl,GM_MOVE
            call g_apply
IF G_CLOCK
            call gm_settle          ; mi tiempo, hasta ahora (aun es mi turno)
ENDIF
            call gm_ply_inc
            call g_winner
            ld   (GM_OVER),a
            jr   gm_send
game_resend: call gm_drain
gm_send:    call game_hash
            ld   (GM_HASH),hl
            ld   a,1
            ld   (NB_TX),a
IF G_MOVE_VAR
            ld   a,(GM_MOVE)        ; [1][jugada: L bytes][hash]
            ld   c,a
            ld   b,0
            ld   hl,GM_MOVE
            ld   de,NB_TX+1
            ldir
            ld   hl,(GM_HASH)
            ld   a,l
            ld   (de),a
            inc  de
            ld   a,h
            ld   (de),a
            ld   a,(GM_MOVE)
            add  a,3
            ld   (NB_LEN),a
            jr   gm_sent
ELSE
            ld   hl,GM_MOVE
            ld   de,NB_TX+1
            ld   bc,G_MOVE_LEN
            ldir
            ld   hl,(GM_HASH)
            ld   (NB_TX+1+G_MOVE_LEN),hl
ENDIF
IF G_CLOCK
            ld   a,(GM_ME)
            call gm_get_rem
            ld   (NB_TX+3+G_MOVE_LEN),hl    ; mis segundos
ENDIF
            ld   a,3+G_MOVE_LEN+G_CLKB
            ld   (NB_LEN),a
IF G_MOVE_VAR
gm_sent:
ENDIF
            ld   hl,(GM_PLY)
            ld   (NB_TURN),hl
            call room_send
            or   a
            ret  z
            ld   a,9
            ret

; mensaje corto (A = tipo) con el turno actual
gm_send_type: ld (NB_TX),a
            ld   a,1
            ld   (NB_LEN),a
            ld   hl,(GM_PLY)
            ld   (NB_TURN),hl
            jp   room_send

IF G_BIG
; el estado en trozos de 96 bytes: [3][numero de trozo][cuantos son][datos]. Los mensajes son retenidos y cada uno pisa al
; anterior, asi que al final, si la ultima jugada fue mia, se vuelve a mandar para que sea lo ultimo que queda en el broker
game_send_state: ld hl,GM_TMP
            call g_save
            ld   hl,GM_TMP
            ld   (GM_CP),hl
            ld   hl,G_STATE_LEN
            ld   (GM_CR),hl
            xor  a
            ld   (GM_CI),a
gss_l:      ld   hl,(GM_CR)
            ld   a,h
            or   a
            jr   nz,gss_96
            ld   a,l
            cp   97
            jr   c,gss_go
gss_96:     ld   a,96
gss_go:     ld   (GM_CL),a
            add  a,3
            ld   (NB_LEN),a
            ld   a,3
            ld   (NB_TX),a
            ld   a,(GM_CI)
            ld   (NB_TX+1),a
            ld   a,G_NCHUNK
            ld   (NB_TX+2),a
            ld   a,(GM_CL)
            ld   c,a
            ld   b,0
            ld   hl,(GM_CP)
            ld   de,NB_TX+3
            ldir
            ld   (GM_CP),hl
            ld   a,(GM_CL)
            ld   e,a
            ld   d,0
            ld   hl,(GM_CR)
            or   a
            sbc  hl,de
            ld   (GM_CR),hl
            ld   hl,(GM_PLY)
            ld   (NB_TURN),hl
            call room_send
            or   a
            ret  nz
            ld   hl,GM_CI
            inc  (hl)
            ld   hl,(GM_CR)
            ld   a,h
            or   l
            jr   nz,gss_l
            ld   hl,(GM_PLY)        ; la ultima jugada, si fue mia
            ld   a,h
            or   l
            jr   z,gss_end
            call game_turn_of
            ld   hl,GM_ME
            cp   (hl)
            jr   z,gss_end          ; me toca a mi: la ultima fue del rival
            call gm_send
            ret
gss_end:    xor  a
            ret
ELSE
game_send_state: ld a,3
            ld   (NB_TX),a
            ld   hl,NB_TX+1
            call g_save
IF G_CLOCK
            ld   hl,GM_REM
            ld   de,NB_TX+1+G_STATE_LEN
            ld   bc,4
            ldir                    ; y los dos relojes
ENDIF
            ld   a,G_STATE_LEN+1+G_CLKB*2
            ld   (NB_LEN),a
            ld   hl,(GM_PLY)
            ld   (NB_TURN),hl
            jp   room_send
ENDIF

game_poll:  call room_poll_recv
            or   a
            ret  z
            cp   1
            jr   z,gp_msg
            cp   2
            jr   z,gp_in
            cp   3
            ret  z
            ld   a,7
            ret
gp_in:      ld   a,(GM_ME)          ; si yo cree la sala y ya hay partida, le mando el estado
            cp   1
            jr   nz,gp_in_end
            ld   hl,(GM_PLY)
            ld   a,h
            or   l
            jr   z,gp_in_end
            call game_send_state
gp_in_end:  ld   a,2
            ret

gp_msg:     ld   a,(NB_RX)
            cp   1
            jp   z,gp_move
            cp   2
            jr   z,gp_sreq
            cp   3
            jr   z,gp_state
            cp   4
            jr   z,gp_new
IF G_CLOCK
            cp   5
            jr   z,gp_flag
ENDIF
gp_ign:     xor  a
            ret
IF G_CLOCK
gp_flag:    ld   a,(GM_OVER)        ; el rival dice que se le ha acabado el tiempo: gano yo
            or   a
            jr   nz,gp_ign
            ld   a,(GM_ME)
            ld   (GM_OVER),a
            ld   a,1
            ld   (GM_WHY),a
            xor  a
            ld   (GM_CKON),a
            ld   a,6
            ret
ENDIF
gp_sreq:    call game_send_state
            xor  a
            ret
gp_new:     call game_new
IF G_CLOCK
            call game_clock_go
ENDIF
            ld   a,8
            ret
IF G_BIG
; un trozo del estado: [3][numero][cuantos][datos]. Se junta en GM_TMP y cuando estan todos se carga
gp_state:   ld   a,(NB_LEN)
            cp   4
            jr   c,gp_ign
            ld   a,(NB_RX+2)
            cp   G_NCHUNK
            jr   nz,gp_ign
            ld   b,a
            ld   a,(NB_RX+1)
            cp   b
            jr   nc,gp_ign
            ld   hl,(NB_TURN)       ; mas viejo que mi partida: sigo con lo mio
            ld   de,(GM_PLY)
            or   a
            sbc  hl,de
            jr   c,gp_ign
            ld   hl,(NB_TURN)       ; de otro turno que el que se estaba juntando: se empieza de nuevo
            ld   de,(GM_RXT)
            or   a
            sbc  hl,de
            jr   z,gps_same
            ld   hl,(NB_TURN)
            ld   (GM_RXT),hl
            xor  a
            ld   (GM_RXM),a
gps_same:   ld   a,(NB_RX+1)        ; destino = GM_TMP + 96 * numero
            ld   b,a
            ld   hl,GM_TMP
            ld   de,96
            inc  b
gps_ad:     dec  b
            jr   z,gps_at
            add  hl,de
            jr   gps_ad
gps_at:     ld   a,(NB_RX+1)        ; la longitud que tiene que traer: 96, o lo que quede en el ultimo
            inc  a
            ld   b,a
            ld   a,(NB_RX+2)
            cp   b
            ld   c,96
            jr   nz,gps_len
            ld   c,G_LASTLEN
gps_len:    ld   a,(NB_LEN)
            sub  3
            cp   c
            jp   nz,gp_ign
            ld   c,a
            ld   b,0
            ex   de,hl
            ld   hl,NB_RX+3
            ldir
            ld   a,(NB_RX+1)        ; el bit del trozo
            ld   b,a
            ld   a,1
            inc  b
gps_bit:    dec  b
            jr   z,gps_bd
            add  a,a
            jr   gps_bit
gps_bd:     ld   hl,GM_RXM
            or   (hl)
            ld   (hl),a
            cp   (1<<G_NCHUNK)-1
            jp   nz,gp_ign          ; faltan trozos
            xor  a
            ld   (GM_RXM),a
            ld   hl,GM_TMP
            call g_load
            ld   hl,(NB_TURN)
            ld   (GM_PLY),hl
            call g_winner
            ld   (GM_OVER),a
            ld   a,4
            ret
ELSE
gp_state:   ld   a,(NB_LEN)
            cp   G_STATE_LEN+1+G_CLKB*2
            jr   nz,gp_ign
            ld   hl,(NB_TURN)       ; un estado mas viejo que mi partida (el creador, que vuelve el primero, o un
            ld   de,(GM_PLY)        ; guardado antiguo) no me hace retroceder: sigo con lo mio
            or   a
            sbc  hl,de
            jr   c,gp_ign
            ld   hl,NB_RX+1
            call g_load
IF G_CLOCK
            ld   hl,NB_RX+1+G_STATE_LEN
            ld   de,GM_REM
            ld   bc,4
            ldir                    ; los relojes que dice el otro
ENDIF
            ld   hl,(NB_TURN)
            ld   (GM_PLY),hl
            call g_winner
            ld   (GM_OVER),a
IF G_CLOCK
            ld   a,(GM_CKON)
            or   a
            call nz,gm_mark_now
ENDIF
            ld   a,4
            ret
ENDIF

; una jugada del rival: tiene que ser la siguiente, ser legal y dejar el mismo estado
gp_move:
IF G_MOVE_VAR
            ld   a,(NB_RX+1)        ; [1][L ...][hash]: el mensaje mide 3 + L
            cp   2
            jp   c,gp_ign
            cp   G_MOVE_LEN+1
            jp   nc,gp_ign
            add  a,3
            ld   b,a
            ld   a,(NB_LEN)
            cp   b
            jp   nz,gp_ign
ELSE
            ld   a,(NB_LEN)
            cp   3+G_MOVE_LEN+G_CLKB
            jr   nz,gp_ign
ENDIF
            ld   hl,(GM_PLY)
            inc  hl
            ld   de,(NB_TURN)
            or   a
            sbc  hl,de
            jr   z,gp_next
            ld   hl,(NB_TURN)       ; no es la siguiente: vieja (retenida o repetida) o me faltan
            ld   de,(GM_PLY)
            or   a
            sbc  hl,de
            jp   c,gp_ign
            jp   z,gp_ign
            jr   gp_miss
gp_next:    ld   a,(GM_OVER)
            or   a
            jp   nz,gp_ign
            call game_turn_of
            ld   hl,GM_ME
            cp   (hl)
            jr   z,gp_miss          ; me tocaba a mi: algo no cuadra
            ld   hl,NB_RX+1
            ld   de,GM_MOVE
            ld   bc,G_MOVE_LEN
            ldir
            call game_turn_of
            ld   b,a
            ld   hl,GM_MOVE
            call g_valid
            jr   c,gp_miss
            call game_turn_of
            ld   b,a
            ld   hl,GM_MOVE
            call g_apply
            call gm_ply_inc
            call g_winner
            ld   (GM_OVER),a
            call game_hash
IF G_MOVE_VAR
            push hl                 ; el resumen del rival esta detras de la jugada
            ld   a,(NB_RX+1)
            ld   e,a
            ld   d,0
            ld   hl,NB_RX+1
            add  hl,de
            ld   e,(hl)
            inc  hl
            ld   d,(hl)
            pop  hl
ELSE
            ld   de,(NB_RX+1+G_MOVE_LEN)
ENDIF
            or   a
            sbc  hl,de
            jr   nz,gp_miss
IF G_CLOCK
            call gm_clk_rival       ; su reloj, el de su mensaje; el mio empieza
ENDIF
            ld   a,1
            ret
gp_miss:    ld   a,2                ; pido el estado completo
            call gm_send_type
            ld   a,5
            ret

IF G_CLOCK
; =====================================================================
;  El reloj de cada jugador (solo si el juego define G_CLOCK = 1)
;  El control de tiempo va en la primera letra del codigo de sala: A sin reloj, B 5 min, C 10 min,
;  D 15 min, E 30 min (por jugador, sin incremento). El tiempo sale del reloj del MCU (el comando
;  de LOAD *RTC): el contador de VSYNC no sirve, porque el handshake con el MCU lo pone a cero
;  en cada lectura. Cada programa es la autoridad de SU reloj: manda en cada jugada los segundos
;  que le quedan (el rival se los apunta) y es el que dice "se me acabo el tiempo" (tipo 5).
;  Entre jugada y jugada cada uno lleva en local una estimacion del reloj del otro.
;      game_clock_go   arranca los relojes (el juego ya esta en marcha: el rival esta)
;      game_tick       una vez por segundo, mas o menos: pone al dia el reloj del que mueve.
;                      A = 0 nada, 1 han cambiado los relojes, 2 se me ha acabado el tiempo (he
;                      perdido; ya se lo he dicho al rival), 3 al rival se le ha acabado (espero a
;                      que lo diga el)
;  Variables: GM_TC (0-4), GM_REM (2 palabras: segundos del jugador 1 y del 2), GM_CKON, GM_WHY
;  (1 = la partida acabo por tiempo).
; =====================================================================
gm_tc_tab:  defw 0,300,600,900,1800

; GM_TC a partir de la primera letra del codigo (los codigos de caracter del ZX81: A = 38)
gm_tc_code: ld   a,(NB_CODE81)
            sub  38
            jr   c,gtc0
            cp   5
            jr   c,gtc1
gtc0:       xor  a
gtc1:       ld   (GM_TC),a
            ret

; relojes a su valor inicial, parados (game_new)
gm_clk_reset: xor a
            ld   (GM_CKON),a
            ld   (GM_WHY),a
            ld   a,(GM_TC)
            add  a,a
            ld   e,a
            ld   d,0
            ld   hl,gm_tc_tab
            add  hl,de
            ld   e,(hl)
            inc  hl
            ld   d,(hl)
            ld   (GM_REM),de
            ld   (GM_REM+2),de
            ret

; arranca los relojes: la marca es ahora
game_clock_go: ld a,(GM_TC)
            or   a
            ret  z
            call gm_mark_now
            ld   a,1
            ld   (GM_CKON),a
            ret

; la marca pasa a ser la hora de ahora
gm_mark_now: call gm_rtc
            ld   hl,(GM_NOWM)
            ld   (GM_MKM),hl
            ld   a,(GM_NOWS)
            ld   (GM_MKS),a
            ret

; lee la hora del MCU: GM_NOWM = minutos desde las 0:00, GM_NOWS = segundos. Destruye C: es el
; reloj del handshake, que cada comando vuelve a leer (sd_clk)
gm_rtc:     call sd_clk
            ld   a,CMD_RTC
            call mcu_send
            xor  a
            call mcu_send           ; longitud 0: pide la fecha y la hora
            ld   hl,GM_TMP
            ld   b,22
gr_lp:      call mcu_recv
            ld   (hl),a
            inc  hl
            djnz gr_lp
            call mcu_recv           ; el estado
            ld   hl,GM_TMP+11       ; "YYYY-MM-DD hh:mm:ss.cc" en codigos del ZX81
            call gm_dig2
            ld   e,a                ; hh
            ld   hl,GM_TMP+14
            call gm_dig2
            ld   d,a                ; mm
            ld   hl,GM_TMP+17
            call gm_dig2
            ld   (GM_NOWS),a
            ld   l,e
            ld   h,0
            add  hl,hl
            add  hl,hl              ; hh * 4
            ld   b,h
            ld   c,l
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl              ; hh * 64
            or   a
            sbc  hl,bc              ; hh * 60
            ld   e,d
            ld   d,0
            add  hl,de              ; + mm
            ld   (GM_NOWM),hl
            ret

; (HL) y (HL+1) = dos cifras (codigos del ZX81, 0 = 28) -> A = su valor
gm_dig2:    ld   a,(hl)
            sub  28
            ld   c,a
            add  a,a
            add  a,a
            add  a,c
            add  a,a                ; * 10
            ld   c,a
            inc  hl
            ld   a,(hl)
            sub  28
            add  a,c
            ret

; HL = el reloj (GM_REM) del que mueve
gm_run_ptr: call game_turn_of
            ld   hl,GM_REM
            cp   2
            ret  c
            inc  hl
            inc  hl
            ret

; A = jugador (1 o 2) -> HL = sus segundos
gm_get_rem: ld   hl,GM_REM
            cp   2
            jr   c,ggr1
            inc  hl
            inc  hl
ggr1:       ld   a,(hl)
            inc  hl
            ld   h,(hl)
            ld   l,a
            ret

; A = jugador (1 o 2), HL = segundos: se los pone
gm_put_rem: ex   de,hl
            ld   hl,GM_REM
            cp   2
            jr   c,gpr1
            inc  hl
            inc  hl
gpr1:       ld   (hl),e
            inc  hl
            ld   (hl),d
            ret

; quita HL segundos al reloj del que mueve (sin pasar de 0)
gm_charge:  ex   de,hl
            call gm_run_ptr
            ld   c,(hl)
            inc  hl
            ld   b,(hl)
            dec  hl
            ld   a,c
            sub  e
            ld   c,a
            ld   a,b
            sbc  a,d
            ld   b,a
            jr   nc,gch_ok
            ld   bc,0
gch_ok:     ld   (hl),c
            inc  hl
            ld   (hl),b
            ret

; le descuenta al que mueve lo que ha pasado desde la ultima marca, y la marca pasa a ser ahora
gm_settle:  ld   a,(GM_CKON)
            or   a
            ret  z
            call gm_rtc
            ld   hl,(GM_NOWM)
            ld   de,(GM_MKM)
            or   a
            sbc  hl,de              ; minutos transcurridos
            jr   nc,gs_pos
            ld   de,1440
            add  hl,de              ; paso la medianoche
gs_pos:     ld   de,1001
            push hl
            or   a
            sbc  hl,de
            pop  hl
            jr   nc,gs_mark         ; mas de 16 h: el reloj del MCU ha dado un salto: lo ignoro
            ld   b,h
            ld   c,l
            add  hl,hl
            add  hl,hl              ; * 4
            ex   de,hl              ; DE = minutos * 4
            ld   h,b
            ld   l,c
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl              ; * 64
            or   a
            sbc  hl,de              ; * 60
            ld   a,(GM_NOWS)
            ld   e,a
            ld   d,0
            add  hl,de
            ld   a,(GM_MKS)
            ld   e,a
            ld   d,0
            or   a
            sbc  hl,de              ; + segundos de ahora - segundos de la marca
            jr   c,gs_mark          ; el reloj del MCU ha ido hacia atras: no descuento nada
            ld   a,h
            or   l
            call nz,gm_charge
gs_mark:    ld   hl,(GM_NOWM)
            ld   (GM_MKM),hl
            ld   a,(GM_NOWS)
            ld   (GM_MKS),a
            ret

; el rival acaba de jugar: su reloj es el que dice su mensaje (los 2 bytes tras la jugada y el hash) y
; el mio empieza ahora
gm_clk_rival: ld a,(GM_CKON)
            or   a
            ret  z
            ld   hl,(NB_RX+1+G_MOVE_LEN+2)
            ld   a,(GM_ME)
            ld   b,a
            ld   a,3
            sub  b                  ; el rival: 3 - GM_ME
            call gm_put_rem
            jp   gm_mark_now

game_tick:  ld   a,(GM_CKON)
            or   a
            ret  z
            ld   a,(GM_OVER)
            or   a
            ret  nz
            call gm_settle
            call gm_run_ptr
            ld   a,(hl)
            inc  hl
            or   (hl)
            jr   nz,gt_ok
            call game_turn_of       ; se ha acabado el tiempo del que mueve
            ld   hl,GM_ME
            cp   (hl)
            jr   nz,gt_rival
            ld   a,(GM_ME)          ; el mio: pierdo y se lo digo
            ld   b,a
            ld   a,3
            sub  b
            ld   (GM_OVER),a
            ld   a,1
            ld   (GM_WHY),a
            xor  a
            ld   (GM_CKON),a
            ld   a,5
            call gm_send_type
            ld   a,2
            ret
gt_rival:   ld   a,3
            ret
gt_ok:      ld   a,1
            ret
ENDIF
