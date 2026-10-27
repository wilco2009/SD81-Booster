; =====================================================================
;  SPORTS.ASM - los deportes del AY-3-8500 para dos ZX81 por la red (MQTT): Tennis, Hockey, Squash y Practice (uno solo), con un menu de juego y de opciones.
;  Diseno: claude/planning/latencia_red.md y EXAMPLES/NETCHAT/netq.inc (mensajes de red sin esperas). Es el Pong (pong.asm) generalizado.
;
;  - LA BOLA ES DE QUIEN LA RECIBE: la decide (golpe, pared o tanto) el jugador hacia cuya mitad viaja, con SU pala y SU bola al dia, sin esperar a la red. Cuando
;    la devuelve, pasa a ser del otro: se lo dice con un contador de golpes dentro del mensaje (con la posicion, la velocidad y los golpes seguidos) y el otro la
;    adelanta lo que tardo el mensaje y sigue desde ahi. Entre tanto el que no manda la bola la sigue y la mueve con la misma fisica.
;  - Los JUEGOS: TENNIS (una pala por jugador, red en el medio), HOCKEY (dos palas por jugador, la porteria y el delantero, que se mueven juntas; las paredes de los
;    extremos rebotan la bola salvo en el hueco de la porteria, donde es gol), SQUASH (una pared a la derecha; los dos jugadores se turnan: tras cada golpe le toca
;    al otro devolver la bola cuando vuelve de la pared), PRACTICE (uno solo contra la pared: cuenta los golpes seguidos y el mejor).
;  - OPCIONES (como los interruptores del chip): palas normales o pequenas, bola lenta o rapida, angulos suaves o amplios. El que crea la sala las elige y viajan
;    en la primera letra del codigo de la sala (5 bits: juego y opciones), asi el que entra juega con las mismas sin que se las digan.
;  - Tantos: los marca el que falla y se suman al otro; gana quien llega a 15. Saca el que ha perdido el tanto (SERVE_TO_LOSER = 0: el que lo ha marcado). El CREADOR
;    decide la nueva partida (ENTER), que se anuncia con un numero de partida en los mensajes.
;  - Cada uno mueve SU pala sin retraso. El ping se ve con la tecla P (ida y vuelta, en ms, por los sellos de fotograma y la "edad" de lo que se devuelve).
;  - Graficos: sprites por hardware (Apendice H): 4 palas de hasta 4 sprites (0-15) y la bola (16).
;  - Teclas: Q / 7 = subir, A / 6 = bajar, P = ping, X = salir, ENTER = otra partida (el creador, cuando acaba).
;
;  El tiempo: FRAMES ($4034, un VSYNC cada 20 ms). Cada vuelta da tantos pasos de fisica como VSYNC hayan pasado (hasta MAX_STEPS) y espera al siguiente; los mensajes
;  salen cuando cambia algo y, si no, cada SEND_TICKS VSYNC.
;  Mensaje (los dos igual, 15 bytes): [1][fr][mi pala][sello del otro][edad][flags][tantos izq][tantos der][partida][golpes: contador][golpes seguidos]
;  [bola x][bola y][vx][vy]. El contador de golpes sube con cada golpe, cada rebote en una pared de hockey y cada saque del que lo manda.
;
;  Memoria:   $4400-$7FFF el programa;  $8800- pantalla de texto del lobby;  $9000-$9750 variables;  $A000-$BAFF la pantalla grafica
;  Cargar:    SPORTS.B81 (CLEAR 17407, LOAD FAST ... CODE 17408, RAND USR 17408).
;  Ensamblar: pasmo sports.asm sports.bin sports.sym   (compila_sports.bat)
; =====================================================================

NB_BASE     equ 9000h
UI_BASE     equ 9400h
PG_BASE     equ 9440h
NQ_BASE     equ 9500h
DFILE       equ 8800h           ; pantalla de texto del lobby
GFX_SQUARES equ 0
BOARD_ROW   equ 0               ; (gfx.inc: solo para draw_square)
BOARD_COL   equ 0

; --- variables del menu (UI_BASE: el lobby las pone a cero al arrancar)
SP_CFG      equ UI_BASE+10      ; la configuracion elegida: bits 0-1 juego, 2 palas pequenas, 3 bola rapida, 4 angulos amplios
SP_LOCAL    equ UI_BASE+11      ; 1 = Practice (sin red)
SP_G        equ UI_BASE+12      ; el juego elegido en el menu (0-3)
SP_O        equ UI_BASE+13      ; las opciones: bit 0 palas pequenas, 1 bola rapida, 2 angulos amplios

; --- variables de la partida ---
P_ROLE      equ PG_BASE+0       ; 0 = creador (izquierda), 1 = invitado (derecha)
P_FR        equ PG_BASE+1       ; contador de fotogramas
P_BX        equ PG_BASE+2       ; la bola: x, y (pixeles de pantalla), velocidad con signo
P_BY        equ PG_BASE+3
P_VX        equ PG_BASE+4
P_VY        equ PG_BASE+5
P_MY        equ PG_BASE+6       ; mi pala (y de arriba)
P_OY        equ PG_BASE+7       ; la del rival (la que se ve)
P_SL        equ PG_BASE+8       ; tantos: izquierda, derecha (en Practice: golpes seguidos y el mejor)
P_SR        equ PG_BASE+9
P_FLAGS     equ PG_BASE+10      ; bit 0 = sacando (bola quieta en el centro), bit 1 = partida acabada
P_SERVE     equ PG_BASE+11      ; fotogramas que faltan para sacar (el que saca)
P_PING      equ PG_BASE+12      ; ultima ida y vuelta, en fotogramas
P_LAST      equ PG_BASE+13      ; el ultimo sello del rival (se lo devuelvo)
P_HITS      equ PG_BASE+14      ; golpes seguidos del punto (la velocidad sube)
P_GAME      equ PG_BASE+15      ; 0 tennis, 1 hockey, 2 squash, 3 practice
P_BATH      equ PG_BASE+16      ; alto de las palas: 32 o 16
P_RXOK      equ PG_BASE+17      ; 1 si ya ha llegado algo del rival
P_SIL       equ PG_BASE+18      ; fotogramas sin oir al rival
P_MSG       equ PG_BASE+19      ; el aviso que se ve abajo (0 normal)
P_DS        equ PG_BASE+20      ; (4) lo ultimo dibujado: tantos, ping, aviso
P_DSS       equ PG_BASE+24      ; lo ultimo dibujado del ping: 1 = visible
P_ASH       equ PG_BASE+25      ; desplazamientos del angulo del golpe (mas = mas suave)
P_SPD0      equ PG_BASE+26      ; velocidad base de la bola (3 lenta, 4 rapida)
P_TURN      equ PG_BASE+27      ; squash: a quien le toca devolver la bola
P_BUF       equ PG_BASE+28      ; (20) el mensaje que se manda
P_TXT       equ PG_BASE+48      ; (16) texto del ping
P_OYT       equ PG_BASE+64      ; la pala del rival tal como la manda (la que se ve se acerca a ella a saltos de 6 px por fotograma)
P_PEND      equ PG_BASE+65      ; fotogramas que quedan de "rebote mio sin confirmar": se ignoran los estados del rival que lo contradicen
P_SIM       equ PG_BASE+66      ; 1 mientras se adelanta la bola (no cuenta como rebote, golpe ni tanto)
P_RXF       equ PG_BASE+67      ; fotograma en que llego lo ultimo del rival
LBX         equ PG_BASE+68      ; la bola local, guardada al llegar un estado
LBY         equ PG_BASE+69
LVX         equ PG_BASE+70
LVY         equ PG_BASE+71
P_GEN       equ PG_BASE+72      ; numero de partida (lo sube el creador con ENTER)
P_HC        equ PG_BASE+73      ; mi contador de golpes (y saques): el rival ve que cambia
P_PHC       equ PG_BASE+74      ; el ultimo del rival que he visto
P_SRV       equ PG_BASE+75      ; quien saca: 0 creador, 1 invitado
P_PMAX      equ PG_BASE+76      ; la y mas baja de una pala (176 - alto)
P_LOCAL     equ PG_BASE+77      ; 1 = sin red (Practice)
P_CFG       equ PG_BASE+78      ; la configuracion de esta partida
P_SHP       equ PG_BASE+80      ; 1 = se ve el ping abajo (tecla P)
P_KP        equ PG_BASE+81      ; la tecla P en el fotograma anterior (1 = suelta)
P_DBGON     equ PG_BASE+89      ; 1 = la fila de diagnostico (fila 1) esta pintada
P_RXN       equ PG_BASE+90      ; (2) mensajes del rival que han llegado (0-9999)
P_ACN       equ PG_BASE+92      ; (2) los que han pasado los filtros (el resto, viejos o de otra partida)
P_DBG       equ PG_BASE+94      ; (16) texto de diagnostico
P_TL        equ PG_BASE+120     ; (2) el FRAMES que vi al empezar esta vuelta
P_NT        equ PG_BASE+122     ; pasos de fisica que quedan por dar en esta vuelta
P_TX        equ PG_BASE+123     ; VSYNC desde el ultimo mensaje que mande (tope 255)
P_LSN       equ PG_BASE+124     ; (6) lo ultimo que mande: mi pala, flags, tantos izquierda y derecha, partida, golpes
DW_R        equ PG_BASE+82      ; (variables de trabajo del dibujo y de la fisica)
DW_J        equ PG_BASE+83
DW_X        equ PG_BASE+84
DW_Y        equ PG_BASE+85
DW_S        equ PG_BASE+86
OW_J        equ PG_BASE+87
OW_X        equ PG_BASE+88
SV_VAL      equ PG_BASE+110     ; (lo que pide el lobby; este juego no guarda partidas)
SV_CONT     equ PG_BASE+111
GM_ME       equ PG_BASE+112
GM_PLY      equ PG_BASE+113     ; (2)

SPR_SEL     equ 2100
SPR_EN      equ 2101
SPR_XL      equ 2102
SPR_XH      equ 2103
SPR_Y       equ 2104
SPR_COL     equ 2105

WIN_SCORE   equ 15
SERVE_TO_LOSER equ 1            ; 1 = saca el que ha perdido el tanto (la bola va hacia el); 0 = el que lo ha marcado
FRAMES      equ 4034h           ; el contador de VSYNC de la FPGA (Superfast): baja en 1 cada 20 ms, a 50 Hz exactos
MAX_STEPS   equ 4               ; pasos de fisica como mucho por vuelta (si la vuelta tarda mas, la bola va algo mas despacio, no se lanza de golpe)
SEND_TICKS  equ 3               ; sin cambios que contar, un mensaje cada 3 VSYNC (60 ms) para que el otro siga mi bola y sepa que sigo aqui
WAIT_MAX    equ 2000            ; vueltas de espera del VSYNC como mucho (~30 ms, a 48 T cada una): por si FRAMES no corre, que no se cuelgue
SPR_BALL    equ 16              ; el sprite de la bola; las palas, del 0 al 15: (jugador * 2 + pala) * 4 + trozo

            org  17408

            include "lobby.inc"

; ---------------------------------------------------------------------
;  Lo que pide el lobby (sin partida por turnos, sin guardar)
; ---------------------------------------------------------------------
game_start: ld   a,(NB_SLOT)
            inc  a
            ld   (GM_ME),a
            ret
game_poll:  jp   room_poll_recv
game_turn_of: xor a
            ret
game_resend: ret
sv_exists:  scf
            ret
sv_load:    scf
            ret
sv_apply:   ret

; ---------------------------------------------------------------------
;  Menu del juego y de las opciones (lo llama el lobby al crear la sala): deja en NB_TC (bit 7 a 1) el juego y las opciones, que
;  viajan en la primera letra del codigo. Practice no necesita sala: empieza aqui mismo. CF = 1: volver al menu
; ---------------------------------------------------------------------
pick_tc:    call cls
            ld   hl,t_title
            ld   bc,ROW_TITLE*256+9
            call print_at
            ld   hl,t_sel
            ld   bc,3*256+6
            call print_at
            ld   hl,t_g1
            ld   bc,5*256+8
            call print_at
            ld   hl,t_g2
            ld   bc,7*256+8
            call print_at
            ld   hl,t_g3
            ld   bc,9*256+8
            call print_at
            ld   hl,t_g4
            ld   bc,11*256+8
            call print_at
            ld   hl,t_pickq
            ld   bc,ROW_HELP*256+4
            call print_at
pt_key:     call key_wait
            cp   'Q'
            jr   nz,pt_k1
            scf
            ret
pt_k1:      sub  '1'
            jr   c,pt_key
            cp   4
            jr   nc,pt_key
            ld   (SP_G),a
            xor  a
            ld   (SP_O),a
pt_opts:    call cls
            ld   hl,t_title
            ld   bc,ROW_TITLE*256+9
            call print_at
            ld   hl,t_opt
            ld   bc,3*256+9
            call print_at
            ld   hl,t_o1
            ld   a,(SP_O)
            and  1
            call po_pick
            ld   bc,5*256+5
            call print_at
            ld   hl,t_o2
            ld   a,(SP_O)
            and  2
            call po_pick
            ld   bc,7*256+5
            call print_at
            ld   hl,t_o3
            ld   a,(SP_O)
            and  4
            call po_pick
            ld   bc,9*256+5
            call print_at
            ld   hl,t_go
            ld   bc,12*256+5
            call print_at
            ld   hl,t_pickq
            ld   bc,ROW_HELP*256+4
            call print_at
po_key:     call key_wait
            cp   'Q'
            jp   z,pick_tc
            cp   13
            jr   z,po_go
            ld   b,1
            cp   '1'
            jr   z,po_tog
            ld   b,2
            cp   '2'
            jr   z,po_tog
            ld   b,4
            cp   '3'
            jr   nz,po_key
po_tog:     ld   a,(SP_O)
            xor  b
            ld   (SP_O),a
            jp   pt_opts
po_go:      ld   a,(SP_O)           ; la configuracion: juego, palas, bola, angulos
            ld   b,a
            ld   a,(SP_G)
            bit  0,b
            jr   z,pg_c1
            or   4
pg_c1:      bit  1,b
            jr   z,pg_c2
            or   8
pg_c2:      bit  2,b
            jr   z,pg_c3
            or   16
pg_c3:      ld   (SP_CFG),a
            and  3
            cp   3
            jp   z,practice         ; Practice: sin sala
            ld   a,(SP_CFG)
            or   80h
            ld   (NB_TC),a
            or   a
            ret
; HL = tabla de dos punteros a texto (el de A = 0 y el de A <> 0): HL = el texto que toca
po_pick:    or   a
            jr   z,pp_1
            inc  hl
            inc  hl
pp_1:       ld   e,(hl)
            inc  hl
            ld   d,(hl)
            ex   de,hl
            ret

; Practice: solo, contra la pared, sin sala ni red
practice:   ld   a,1
            ld   (SP_LOCAL),a
            call pg_init
            call pg_tinit
            jp   pg_loop

; ---------------------------------------------------------------------
;  La partida
; ---------------------------------------------------------------------
play:       call cls
            ld   hl,t_title
            ld   bc,ROW_TITLE*256+11
            call print_at
            ld   hl,t_connecting
            ld   bc,7*256+6
            call print_at
            call nq_open            ; (con espera, una vez) solo el topic del rival
            or   a
            jr   z,pl_ok
            call show_err
            jp   menu
pl_ok:      call cfg_from_code      ; el juego y las opciones: de la primera letra del codigo
            xor  a
            ld   (SP_LOCAL),a
            call pg_init
            call pg_tinit
; Una vuelta = los VSYNC que han pasado desde la anterior (1, o mas si la vuelta se alargo): la fisica, las teclas y la pala del rival dan tantos pasos
; como VSYNC (hasta MAX_STEPS), asi la bola va a la misma velocidad en las dos maquinas aunque la red o el STM32 alarguen una vuelta; se dibuja una vez
; y se espera al siguiente VSYNC. P_FR cuenta VSYNC de verdad (los sellos, el ping y los plazos van en 20 ms).
pg_loop:    call pg_tick
            call pg_pkey
pl_step:    call pg_keys
            jp   c,pg_quit
            call pg_osm
            call pg_phys
            ld   hl,P_NT
            dec  (hl)
            jr   nz,pl_step
            call pg_net
            call pg_draw
            call pg_dbg
            call pg_wait
            jr   pg_loop

pg_quit:    call pg_sprites_off
            ld   a,(P_LOCAL)
            or   a
            jr   nz,pq_local
            call nq_close           ; que el modem conteste lo pendiente antes de volver a net.inc
            call gfx_off
            call room_leave
            jp   menu
pq_local:   call gfx_off
            jp   menu

; HL = contador de 16 bits (0-9999): +1
cnt_inc:    ld   e,(hl)
            inc  hl
            ld   d,(hl)
            inc  de
            ld   a,d
            cp   27h
            jr   nz,ci_s
            ld   a,e
            cp   10h
            jr   nz,ci_s
            ld   de,0
ci_s:       ld   (hl),d
            dec  hl
            ld   (hl),e
            ret

; con la tecla P, en la fila 1: "R nnnn A nnnn" = mensajes del rival recibidos y aceptados (si dejan de subir los dos, es la red; si solo sube R, los filtros)
pg_dbg:     ld   a,(P_SHP)
            or   a
            jr   nz,pdb_on
            ld   a,(P_DBGON)
            or   a
            ret  z
            xor  a
            ld   (P_DBGON),a
            ld   b,1
            jp   gclr_row
pdb_on:     ld   a,1
            ld   (P_DBGON),a
            ld   hl,(P_RXN)
            ld   de,P_DBG+2
            call dec4
            ld   hl,(P_ACN)
            ld   de,P_DBG+9
            call dec4
            ld   hl,P_DBG
            ld   (hl),'R'
            inc  hl
            ld   (hl),' '
            ld   hl,P_DBG+6
            ld   (hl),' '
            inc  hl
            ld   (hl),'A'
            inc  hl
            ld   (hl),' '
            ld   hl,P_DBG+13
            ld   (hl),0
            ld   hl,P_DBG
            ld   bc,1*256+0
            jp   gprint_at

; --- el tiempo: FRAMES ($4034) baja con cada VSYNC (Superfast, 50 Hz) ---
pg_tinit:   ld   hl,(FRAMES)
            ld   (P_TL),hl
            ld   a,255              ; el primer mensaje, enseguida
            ld   (P_TX),a
            ret

; los VSYNC desde la vuelta anterior: P_FR avanza ese tiempo (hasta 50), P_NT = los pasos de fisica (1 a MAX_STEPS), P_TX acumula
pg_tick:    ld   hl,(FRAMES)
            ld   de,(P_TL)
            ld   (P_TL),hl
            ex   de,hl              ; HL = el de antes, DE = el de ahora
            or   a
            sbc  hl,de              ; FRAMES baja: antes - ahora = VSYNC transcurridos
            ld   a,h
            or   a
            ld   a,50
            jr   nz,pt_1            ; mas de 255: tope
            ld   a,l
            or   a
            jr   nz,pt_0
            inc  a                  ; 0: FRAMES no se mueve (parado): cuenta uno
pt_0:       cp   51
            jr   c,pt_1
            ld   a,50
pt_1:       ld   b,a                ; B = tiempo real transcurrido
            ld   hl,P_FR
            add  a,(hl)
            ld   (hl),a
            ld   hl,P_TX
            ld   a,(hl)
            add  a,b
            jr   nc,pt_tx
            ld   a,255
pt_tx:      ld   (hl),a
            ld   a,b
            cp   MAX_STEPS+1
            jr   c,pt_st
            ld   a,MAX_STEPS
pt_st:      ld   (P_NT),a
            ret

; espera al siguiente VSYNC (si la vuelta ya duro mas de uno, no espera nada); acotada por si FRAMES no corre
pg_wait:    ld   a,(P_TL)
            ld   b,a
            ld   de,WAIT_MAX
pw_l:       ld   a,(FRAMES)
            cp   b
            ret  nz
            dec  de
            ld   a,d
            or   e
            jr   nz,pw_l
            ret

; la primera letra del codigo de la sala (ASCII, en V_CODE) es el indice (0-31) de la tabla de letras: eso es la configuracion (Practice no va por la red: Tennis)
cfg_from_code: ld a,(V_CODE)
            ld   hl,s_alpha
            ld   b,32
            ld   c,0
cf_l:       cp   (hl)
            jr   z,cf_f
            inc  hl
            inc  c
            djnz cf_l
            ld   c,0
cf_f:       ld   a,c
            and  3
            cp   3
            ld   a,c
            jr   nz,cf_ok
            and  1Ch                ; el 3 (Practice) no es de red: Tennis con las mismas opciones
cf_ok:       ld   (SP_CFG),a
            ret

; la configuracion de la partida (P_CFG) a las variables del juego
pg_cfg:     ld   a,(P_CFG)
            and  3
            ld   (P_GAME),a
            ld   a,(P_CFG)
            ld   b,32
            bit  2,a
            jr   z,pc_b
            ld   b,16
pc_b:       ld   a,b
            ld   (P_BATH),a
            ld   c,a
            ld   a,176
            sub  c
            ld   (P_PMAX),a
            ld   a,(P_CFG)
            ld   b,3
            bit  3,a
            jr   z,pc_s
            ld   b,4
pc_s:       ld   a,b
            ld   (P_SPD0),a
            ld   a,(P_CFG)
            ld   c,3                ; angulos suaves: 3 desplazamientos con la pala de 32 (2 con la de 16)
            bit  4,a
            jr   z,pc_a
            ld   c,2                ; amplios: 2 (1 con la pequena)
pc_a:       ld   a,(P_BATH)
            cp   32
            jr   z,pc_a2
            dec  c
pc_a2:      ld   a,c
            ld   (P_ASH),a
            ret

; ---------------------------------------------------------------------
;  Estado inicial, pista y sprites
; ---------------------------------------------------------------------
pg_init:    ld   a,(NB_SLOT)
            push af
            ld   hl,PG_BASE
            ld   de,PG_BASE+1
            ld   bc,109
            ld   (hl),0
            ldir
            pop  af
            ld   (P_ROLE),a
            ld   a,(SP_CFG)
            ld   (P_CFG),a
            call pg_cfg
            ld   a,(SP_LOCAL)
            ld   (P_LOCAL),a
            or   a
            jr   z,pi_nl
            xor  a                  ; Practice: yo soy el jugador 0 y no espero a nadie
            ld   (P_ROLE),a
            inc  a
            ld   (P_RXOK),a
pi_nl:      ld   a,(P_BATH)         ; mi pala empieza en el medio
            srl  a
            ld   b,a
            ld   a,96
            sub  b
            ld   (P_MY),a
            ld   (P_OY),a
            ld   (P_OYT),a
            ld   a,1
            ld   (P_KP),a
            ld   a,0FFh             ; fuerza el primer dibujo de los marcadores (y, abajo, el del aviso)
            ld   (P_DS),a
            ld   (P_DS+3),a
            xor  a                  ; la primera vez saca el creador
            call pg_begin_serve
            call gfx_on
            call pg_court
            ld   a,(P_GAME)         ; el titulo: el nombre del juego, entre los dos marcadores (columnas 10-21)
            add  a,a
            add  a,a                ; 4 bytes por juego
            ld   e,a
            ld   d,0
            ld   hl,gname_tab
            add  hl,de
            ld   e,(hl)
            inc  hl
            ld   d,(hl)
            inc  hl
            ld   c,(hl)             ; la columna (centrado)
            ex   de,hl
            ld   b,ROW_TITLE
            call gprint_at
            ld   hl,t_p1
            ld   de,t_p2
            ld   a,(P_GAME)
            cp   3
            jr   nz,pi_lab
            ld   hl,t_hits
            ld   de,t_best
pi_lab:     push de
            ld   bc,ROW_TITLE*256+1
            call gprint_at
            pop  hl
            ld   bc,ROW_TITLE*256+26
            call gprint_at
            ld   a,(P_ROLE)         ; mi etiqueta, en verde; la del rival, en rojo (en Practice, las dos en verde)
            or   a
            ld   a,044h
            ld   d,042h
            jr   z,pi_att
            ld   a,042h
            ld   d,044h
pi_att:     ld   e,a
            ld   a,(P_GAME)
            cp   3
            jr   nz,pi_at2
            ld   d,044h
pi_at2:     ld   a,e
            push de
            ld   b,ROW_TITLE
            ld   c,1
            ld   e,4
            call gattr_span
            pop  de
            ld   a,d
            ld   b,ROW_TITLE
            ld   c,26
            ld   e,4
            call gattr_span
            xor  a                  ; los sprites: las 16 piezas de las palas (verde las mias, rojas las del rival) y la bola
            ld   (DW_S),a
pi_sd:      ld   a,(DW_S)
            srl  a
            srl  a
            srl  a
            ld   b,a
            ld   a,(P_ROLE)
            cp   b
            ld   hl,spr_green
            jr   z,pi_c
            ld   hl,spr_red
pi_c:       ld   a,(DW_S)
            call spr_def
            ld   hl,DW_S
            inc  (hl)
            ld   a,(hl)
            cp   16
            jr   nz,pi_sd
            ld   a,SPR_BALL
            ld   hl,spr_ball
            jp   spr_def

; la pista: las paredes de arriba (lineas 14 y 15) y de abajo (176 y 177) y lo propio de cada juego
pg_court:   ld   b,1
            ld   c,0
            call gaddr
            ld   a,h
            add  a,6
            ld   h,a
            call pg_line
            inc  h
            call pg_line
            ld   b,22
            ld   c,0
            call gaddr
            call pg_line
            inc  h
            call pg_line
            ld   a,(P_GAME)
            cp   2
            jr   nc,pc_wall
            call pc_net             ; Tennis y Hockey: la red del centro
            ld   a,(P_GAME)
            or   a
            ret  z
            ld   b,2                ; Hockey: las paredes de los extremos, con el hueco de la porteria (filas 6-17: y 48-143)
pc_hl:      ld   a,b
            cp   6
            jr   c,pc_hd
            cp   18
            jr   c,pc_hs
pc_hd:      push bc
            ld   c,0
            ld   d,0FFh
            call cell8
            pop  bc
            push bc
            ld   c,31
            ld   d,0FFh
            call cell8
            pop  bc
pc_hs:      inc  b
            ld   a,b
            cp   22
            jr   nz,pc_hl
            ret
pc_wall:    ld   b,2                ; Squash y Practice: la pared de la derecha, entera
pc_wl:      push bc
            ld   c,31
            ld   d,0FFh
            call cell8
            pop  bc
            inc  b
            ld   a,b
            cp   22
            jr   nz,pc_wl
            ret
pc_net:     ld   b,2                ; la red: puntos en el centro (x = 127 y 128), cuatro lineas de cada ocho
pi_net:     push bc
            ld   c,15
            call gaddr
            ld   b,4
pi_dash:    ld   (hl),001h
            inc  l
            ld   (hl),080h
            dec  l
            inc  h
            djnz pi_dash
            pop  bc
            inc  b
            ld   a,b
            cp   22
            jr   nz,pi_net
            ret

; B = fila, C = columna, D = byte: las 8 lineas de esa celda
cell8:      call gaddr
            ld   e,8
c8_l:       ld   (hl),d
            inc  h
            dec  e
            jr   nz,c8_l
            ret

pg_line:    push hl
            ld   b,32
pl_l:       ld   (hl),0FFh
            inc  l
            djnz pl_l
            pop  hl
            ret

; A = sprite, HL = 24 bytes (color, pixeles y mascara de cada fila): lo define, apagado
spr_def:    ld   (SPR_SEL),a
            xor  a
            ld   (SPR_EN),a
            ld   de,SPR_COL
            ld   bc,24
            ldir
            ret

pg_sprites_off: ld b,17
            ld   c,0
so_l:       ld   a,c
            ld   (SPR_SEL),a
            xor  a
            ld   (SPR_EN),a
            inc  c
            djnz so_l
            ret

; A = sprite, B = x, C = y (pixeles de pantalla): lo coloca y lo enciende
sp_pos:     ld   (SPR_SEL),a
            ld   a,b
            add  a,32               ; las coordenadas de sprite llevan 32 pixeles de mas
            ld   (SPR_XL),a
            ld   a,0
            rla                     ; el acarreo: el bit 8 de x
            ld   (SPR_XH),a
            ld   a,c
            add  a,32
            ld   (SPR_Y),a
            ld   a,1
            ld   (SPR_EN),a
            ret


; ---------------------------------------------------------------------
;  Teclado: Q / 7 arriba, A / 6 abajo, X salir. CF = 1 si hay que salir
; ---------------------------------------------------------------------
pg_keys:    ld   a,(P_MY)
            ld   d,a
            ld   bc,0FBFEh          ; fila Q W E R T
            in   a,(c)
            bit  0,a
            jr   z,pk_up
            ld   bc,0EFFEh          ; fila 0 9 8 7 6
            in   a,(c)
            bit  3,a
            jr   nz,pk_dn
pk_up:      ld   a,d
            sub  4
            cp   16
            jr   nc,pk_set
            ld   a,16
            jr   pk_set
pk_dn:      ld   bc,0FDFEh          ; fila A S D F G
            in   a,(c)
            bit  0,a
            jr   z,pk_down
            ld   bc,0EFFEh
            in   a,(c)
            bit  4,a
            jr   nz,pk_x
pk_down:    ld   a,d
            add  a,4
            ld   b,a
            ld   a,(P_PMAX)
            cp   b
            ld   a,b
            jr   nc,pk_set
            ld   a,(P_PMAX)
pk_set:     ld   (P_MY),a
pk_x:       ld   bc,0FEFEh          ; fila SHIFT Z X C V
            in   a,(c)
            bit  2,a
            jr   nz,pk_no
            scf
            ret
pk_no:      or   a
            ret

; tecla P: ensena u oculta el ping (en ms) abajo; al pulsarla, una vez
pg_pkey:    ld   bc,0DFFEh          ; fila P O I U Y
            in   a,(c)
            and  1                  ; 0 = pulsada
            ld   b,a
            ld   a,(P_KP)
            ld   c,a
            ld   a,b
            ld   (P_KP),a
            dec  c
            ret  nz                 ; antes ya estaba pulsada
            or   a
            ret  nz                 ; ahora esta suelta
            ld   a,(P_SHP)
            xor  1
            ld   (P_SHP),a
            ret

; la pala del rival se acerca a la que manda, 6 px por fotograma como mucho (sin saltos)
pg_osm:     ld   a,(P_OYT)
            ld   b,a
            ld   a,(P_OY)
            cp   b
            ret  z
            jr   c,os_inc
            sub  6
            jr   c,os_set
            cp   b
            jr   nc,os_st
os_set:     ld   a,b
os_st:      ld   (P_OY),a
            ret
os_inc:     add  a,6
            jr   c,os_set
            cp   b
            jr   c,os_st
            jr   os_set



; ---------------------------------------------------------------------
;  La bola es de quien la recibe
; ---------------------------------------------------------------------
pg_phys:    ld   hl,P_PEND
            ld   a,(hl)
            or   a
            jr   z,pp_0
            dec  (hl)
pp_0:       ld   a,(P_FLAGS)
            bit  1,a
            jr   nz,pp_over
            bit  0,a
            jr   nz,pp_serve
            jp   ball_step
pp_serve:   ld   a,(P_SRV)          ; el que saca cuenta atras y lanza; el otro espera su aviso
            ld   b,a
            ld   a,(P_ROLE)
            cp   b
            ret  nz
            ld   hl,P_SERVE
            dec  (hl)
            ret  nz
            jp   pg_launch
pp_over:    ld   a,(P_ROLE)         ; ENTER: otra partida (solo el creador)
            or   a
            ret  nz
            ld   bc,0BFFEh
            in   a,(c)
            bit  0,a
            ret  nz
            jp   pg_restart

; A = quien saca (0 creador, 1 invitado): bola al centro y quieta, 1 s de espera
pg_begin_serve: ld (P_SRV),a
            ld   a,(P_FLAGS)
            or   1
            ld   (P_FLAGS),a
            call pg_center
            xor  a
            ld   (P_HITS),a
            ld   (P_PEND),a
            ld   a,50
            ld   (P_SERVE),a
            ret
pg_center:  ld   a,124
            ld   (P_BX),a
            ld   a,92
            ld   (P_BY),a
            xor  a
            ld   (P_VX),a
            ld   (P_VY),a
            ret

; saco yo: Tennis y Hockey, la bola sale del centro hacia mi; Squash y Practice, hacia la pared (y le toca al otro / a mi); con una inclinacion que sale del contador
; de fotogramas; es un "golpe" mas (el otro lo ve en el contador)
pg_launch:  ld   a,(P_FLAGS)
            and  0FEh
            ld   (P_FLAGS),a
            ld   a,(P_SPD0)
            ld   b,a
            ld   a,(P_GAME)
            cp   2
            jr   nc,pl_wall
            ld   a,(P_ROLE)
            or   a
            ld   a,b
            jr   nz,pl_set          ; derecha: la bola va a la derecha
            neg                     ; izquierda: a la izquierda
            jr   pl_set
pl_wall:    ld   a,(P_GAME)         ; contra la pared: el turno es del otro (en Practice, mio)
            cp   3
            ld   a,(P_ROLE)
            jr   z,pl_t
            xor  1
pl_t:       ld   (P_TURN),a
            ld   a,b
pl_set:     ld   (P_VX),a
            ld   a,(P_FR)
            and  3
            ld   e,a
            ld   d,0
            ld   hl,vy_tab
            add  hl,de
            ld   a,(hl)
            ld   (P_VY),a
            xor  a
            ld   (P_HITS),a
            ld   hl,P_HC
            inc  (hl)
            ret
vy_tab:     defb 0FEh,0FFh,1,2

; el creador: ENTER tras el final: marcadores a cero, otro numero de partida y vuelve a sacar el creador
pg_restart: xor  a
            ld   (P_SL),a
            ld   (P_SR),a
            ld   hl,P_GEN
            inc  (hl)
            ld   a,(P_FLAGS)
            and  0FDh
            ld   (P_FLAGS),a
            xor  a
            jp   pg_begin_serve

; marca la bandera de partida acabada si alguien ha llegado a WIN_SCORE
pg_chk_over: ld  a,(P_SL)
            cp   WIN_SCORE
            jr   nc,co_y
            ld   a,(P_SR)
            cp   WIN_SCORE
            jr   c,co_n
co_y:       ld   a,(P_FLAGS)
            or   2
            ld   (P_FLAGS),a
            ret
co_n:       ld   a,(P_FLAGS)
            and  0FDh
            ld   (P_FLAGS),a
            ret

; la bola era mia y ha pasado mi linea: tanto del otro; saca (segun SERVE_TO_LOSER) yo o el. En Practice: se acaba la racha
pg_missed:  ld   a,(P_GAME)
            cp   3
            jr   z,pm_pr
            ld   a,(P_ROLE)
            or   a
            ld   hl,P_SR            ; falla el de la izquierda: tanto de la derecha
            jr   z,pm_a
            ld   hl,P_SL
pm_a:       inc  (hl)
            call pg_chk_over
            ld   a,(P_ROLE)
IF SERVE_TO_LOSER = 0
            xor  1
ENDIF
            jp   pg_begin_serve
pm_pr:      ld   a,(P_SL)           ; Practice: la racha pasa a ser el mejor si lo supera
            ld   hl,P_SR
            cp   (hl)
            jr   c,pm_p2
            ld   (hl),a
pm_p2:      xor  a
            ld   (P_SL),a
            jp   pg_begin_serve

; A = 1 si la bola es mia (la decido yo): Tennis y Hockey, si va hacia mi mitad; Squash y Practice, si va hacia mi (a la izquierda) y es mi turno. 0 si no (o esta quieta)
is_owner:   ld   a,(P_VX)
            or   a
            jr   z,io_no
            rlca
            ld   a,0
            rla                     ; A = 1 si va a la izquierda (vx negativa)
            ld   b,a
            ld   a,(P_GAME)
            cp   2
            jr   nc,io_sq
            ld   a,(P_ROLE)
            xor  1                  ; el de la izquierda (0) es el dueno con vx negativa (1); el de la derecha, con vx positiva (0)
            cp   b
            jr   nz,io_no
            ld   a,1
            ret
io_sq:      ld   a,b
            or   a
            jr   z,io_no
            ld   a,(P_TURN)
            ld   b,a
            ld   a,(P_ROLE)
            cp   b
            jr   nz,io_no
            ld   a,1
            ret
io_no:      xor  a
            ret

; ---------------------------------------------------------------------
;  La bola, un fotograma: paredes y, solo si es mia, mis palas y mi linea
; ---------------------------------------------------------------------
ball_step:  ld   a,(P_VX)           ; x = x + vx (vx con signo), recortada a 0-255
            ld   e,a
            add  a,a
            sbc  a,a
            ld   d,a
            ld   a,(P_BX)
            ld   l,a
            ld   h,0
            add  hl,de
            ld   a,h
            or   a
            ld   a,l
            jr   z,bs_xs
            bit  7,h
            ld   a,0
            jr   nz,bs_xs
            ld   a,255
bs_xs:      ld   (P_BX),a
            ld   a,(P_VY)           ; y = y + vy; rebota arriba (16) y abajo (168)
            ld   b,a
            ld   a,(P_BY)
            add  a,b
            cp   16
            jr   c,bs_top
            cp   169
            jr   nc,bs_bot
            ld   (P_BY),a
            jr   bs_pad
bs_top:     ld   a,16
            ld   (P_BY),a
            call neg_vy
            call set_pend
            jr   bs_pad
bs_bot:     ld   a,168
            ld   (P_BY),a
            call neg_vy
            call set_pend
bs_pad:     ld   a,(P_GAME)         ; Squash y Practice: la pared de la derecha (en x = 248) la rebota, la vea quien la vea
            cp   2
            jr   c,bs_nw
            ld   a,(P_BX)
            cp   241
            jr   c,bs_nw
            ld   a,240
            ld   (P_BX),a
            call neg_vx
            call set_pend
bs_nw:      ld   a,(P_SIM)          ; (adelantando la bola: ni golpes ni tantos)
            or   a
            jr   nz,bs_clamp
            call is_owner
            or   a
            jr   z,bs_clamp         ; no es mia: la mueve (y decide) el otro
            ld   a,(P_GAME)
            cp   2
            jp   nc,own_left        ; Squash y Practice: mis palas estan a la izquierda
            ld   a,(P_ROLE)
            or   a
            jp   z,own_left
            jp   own_right
bs_clamp:   ld   a,(P_BX)           ; no es mia: espera en los bordes (4-244) lo que diga el dueno
            cp   4
            jr   nc,bs_g1
            ld   a,4
bs_g1:      cp   245
            jr   c,bs_g2
            ld   a,244
bs_g2:      ld   (P_BX),a
            ret

; la bola va hacia la izquierda y es mia: mis palas, en el orden en que las alcanza (la del delantero y luego la de la porteria), y luego el borde
own_left:   xor  a
            ld   (OW_J),a
ol_j:       ld   a,(P_ROLE)
            ld   b,a
            ld   a,(OW_J)
            ld   c,a
            call bat_x
            ld   (OW_X),a
            ld   a,(P_BX)
            ld   b,a
            ld   a,(OW_X)
            add  a,8                ; aun no ha llegado a esta pala: nada mas que mirar
            cp   b
            ret  c
            ld   a,(OW_X)
            sub  7
            ld   c,a
            ld   a,b
            cp   c
            jr   c,ol_next          ; ya la ha pasado
            ld   a,(P_MY)
            call hit_test           ; CF = 1 si toca esta pala
            jr   nc,ol_next
            ld   a,(OW_X)           ; golpe: sale por delante de la pala, hacia la derecha
            add  a,9
            ld   (P_BX),a
            call hit_speed
            ld   (P_VX),a
            jp   own_hit
ol_next:    ld   hl,OW_J
            inc  (hl)
            ld   a,(hl)
            ld   b,a
            call nbats
            cp   b
            jr   nz,ol_j
            ld   a,(P_GAME)         ; ninguna pala: el borde de la izquierda
            cp   1
            jr   z,ol_hk
            ld   a,(P_BX)
            cp   5
            jp   c,pg_missed
            ret
ol_hk:      ld   a,(P_BX)           ; Hockey: la pared (x < 8) rebota salvo en el hueco de la porteria, donde es gol
            cp   8
            ret  nc
            ld   a,(P_BY)
            add  a,4
            sub  48
            cp   96
            jr   nc,ol_wall
            ld   a,(P_BX)
            cp   2
            jp   c,pg_missed
            ret
ol_wall:    ld   a,8
            ld   (P_BX),a
            call neg_vx
            jp   own_ev

; la bola va hacia la derecha y es mia (Tennis y Hockey, el de la derecha)
own_right:  xor  a
            ld   (OW_J),a
or_j:       ld   a,(P_ROLE)
            ld   b,a
            ld   a,(OW_J)
            ld   c,a
            call bat_x
            ld   (OW_X),a
            ld   a,(OW_X)
            sub  8                  ; aun no ha llegado a esta pala
            ld   c,a
            ld   a,(P_BX)
            cp   c
            ret  c
            ld   a,(OW_X)
            add  a,7
            ld   c,a
            ld   a,(P_BX)
            ld   b,a
            ld   a,c
            cp   b
            jr   c,or_next          ; ya la ha pasado
            ld   a,(P_MY)
            call hit_test
            jr   nc,or_next
            ld   a,(OW_X)           ; golpe: sale por delante de la pala, hacia la izquierda
            sub  9
            ld   (P_BX),a
            call hit_speed
            neg
            ld   (P_VX),a
            jp   own_hit
or_next:    ld   hl,OW_J
            inc  (hl)
            ld   a,(hl)
            ld   b,a
            call nbats
            cp   b
            jr   nz,or_j
            ld   a,(P_GAME)
            cp   1
            jr   z,or_hk
            ld   a,(P_BX)
            cp   244
            jp   nc,pg_missed
            ret
or_hk:      ld   a,(P_BX)           ; Hockey: la pared (x > 240) rebota salvo en el hueco de la porteria
            cp   241
            ret  c
            ld   a,(P_BY)
            add  a,4
            sub  48
            cp   96
            jr   nc,or_wall
            ld   a,(P_BX)
            cp   246
            jp   nc,pg_missed
            ret
or_wall:    ld   a,240
            ld   (P_BX),a
            call neg_vx
            jp   own_ev

; un golpe mio: la inclinacion segun donde toca; y es un evento (la bola pasa a ser del otro, o vuelve de la pared)
own_hit:    ld   a,(P_MY)
            ld   c,a
            call hit_vy
            ld   a,(P_GAME)         ; Squash: ahora le toca al otro; Practice: un golpe mas en la racha (hasta 99)
            cp   2
            jr   c,own_ev
            jr   nz,oh_pr
            ld   a,(P_ROLE)
            xor  1
            ld   (P_TURN),a
            jr   own_ev
oh_pr:      ld   a,(P_SL)
            cp   99
            jr   nc,own_ev
            inc  a
            ld   (P_SL),a
own_ev:     ld   hl,P_HC            ; el otro lo ve en el contador
            inc  (hl)
            ld   a,12               ; y hasta que lo vea, no hago caso de sus estados anteriores al evento
            ld   (P_PEND),a
            ret

neg_vy:     ld   a,(P_VY)
            neg
            ld   (P_VY),a
            ret
neg_vx:     ld   a,(P_VX)
            neg
            ld   (P_VX),a
            ret

; un rebote en una pared: hasta que el dueno lo confirme se ignoran sus estados que lo contradicen
set_pend:   ld   a,(P_SIM)
            or   a
            ret  nz
            ld   a,12
            ld   (P_PEND),a
            ret

; B = jugador (0 o 1), C = pala (0 o 1): A = la x de esa pala segun el juego. Cada jugador va de la pala que alcanza antes la bola a la que alcanza despues
bat_x:      ld   a,(P_GAME)
            add  a,a
            add  a,a
            ld   e,a
            ld   a,b
            add  a,a
            add  a,c
            add  a,e
            ld   e,a
            ld   d,0
            ld   hl,bx_tab
            add  hl,de
            ld   a,(hl)
            ret
bx_tab:     defb 12,0,236,0         ; Tennis
            defb 52,12,196,236      ; Hockey: el delantero y la porteria de cada uno
            defb 12,0,28,0          ; Squash: los dos a la izquierda, uno detras de otro
            defb 12,0,12,0          ; Practice

; A = palas por jugador (Hockey 2, los demas 1) / A = jugadores (Practice 1, los demas 2)
nbats:      ld   a,(P_GAME)
            cp   1
            ld   a,1
            ret  nz
            inc  a
            ret
nplayers:   ld   a,(P_GAME)
            cp   3
            ld   a,2
            ret  nz
            dec  a
            ret

; A = y de la pala: CF = 1 si la bola (y de arriba P_BY, 8 de alto) solapa con ella (P_BATH de alto). Deja C = y de la pala
hit_test:   ld   c,a
            ld   a,(P_BY)
            add  a,8
            cp   c
            jr   c,ht_no
            jr   z,ht_no            ; by + 8 <= pala: por encima
            ld   a,(P_BATH)
            add  a,c
            ld   d,a
            ld   a,(P_BY)
            cp   d
            jr   nc,ht_no           ; by >= pala + alto: por debajo
            scf
            ret
ht_no:      or   a
            ret

; un golpe mas: A = la nueva velocidad (la base, +1 cada dos golpes, hasta 3 mas)
hit_speed:  ld   a,(P_HITS)
            inc  a
            ld   (P_HITS),a
            srl  a
            cp   4
            jr   c,hs_1
            ld   a,3
hs_1:       ld   b,a
            ld   a,(P_SPD0)
            add  a,b
            ret

; la inclinacion segun donde toca la pala (C = y de la pala): (by - pala - (alto / 2 - 4)) desplazado P_ASH veces, de -4 a 4
hit_vy:     ld   a,(P_BATH)
            srl  a
            sub  4
            ld   d,a
            ld   a,(P_BY)
            sub  c
            sub  d
            ld   b,a
            ld   a,(P_ASH)
            ld   e,a
            ld   a,b
hv_sh:      sra  a
            dec  e
            jr   nz,hv_sh
            bit  7,a
            jr   z,hv_pos
            cp   0FCh
            jr   nc,hv_ok
            ld   a,0FCh
            jr   hv_ok
hv_pos:     cp   5
            jr   c,hv_ok
            ld   a,4
hv_ok:      ld   (P_VY),a
            ret

; ---------------------------------------------------------------------
;  La red: cada fotograma manda lo mio y trata lo que llegue (Practice no usa la red)
; ---------------------------------------------------------------------
pg_net:     ld   a,(P_LOCAL)
            or   a
            jr   z,pn_net
            xor  a
            ld   (P_SIL),a
            ret
pn_net:     call pg_send_if
            call nq_pump
pn_get:     call nq_get
            or   a
            jr   z,pn_done
            call pg_recv
            jr   pn_get
pn_done:    ld   hl,P_SIL           ; cuantos fotogramas llevo sin oir al rival
            ld   a,(hl)
            cp   255
            ret  z
            inc  (hl)
            ret

; manda mi estado si ha cambiado algo (mi pala, los flags, los tantos, la partida o un golpe, rebote o saque: el contador) o, si no, cada SEND_TICKS VSYNC
pg_send_if: ld   a,(P_TX)
            cp   SEND_TICKS
            jr   nc,ps_go
            ld   hl,P_LSN
            ld   a,(P_MY)
            cp   (hl)
            jr   nz,ps_go
            inc  hl
            ld   a,(P_FLAGS)
            cp   (hl)
            jr   nz,ps_go
            inc  hl
            ld   a,(P_SL)
            cp   (hl)
            jr   nz,ps_go
            inc  hl
            ld   a,(P_SR)
            cp   (hl)
            jr   nz,ps_go
            inc  hl
            ld   a,(P_GEN)
            cp   (hl)
            jr   nz,ps_go
            inc  hl
            ld   a,(P_HC)
            cp   (hl)
            ret  z
ps_go:      xor  a
            ld   (P_TX),a
            ld   hl,P_LSN
            ld   a,(P_MY)
            ld   (hl),a
            inc  hl
            ld   a,(P_FLAGS)
            ld   (hl),a
            inc  hl
            ld   a,(P_SL)
            ld   (hl),a
            inc  hl
            ld   a,(P_SR)
            ld   (hl),a
            inc  hl
            ld   a,(P_GEN)
            ld   (hl),a
            inc  hl
            ld   a,(P_HC)
            ld   (hl),a
pg_send:    ld   hl,P_BUF
            ld   (hl),1
            inc  hl
            ld   a,(P_FR)
            ld   (hl),a
            inc  hl
            ld   a,(P_MY)
            ld   (hl),a
            inc  hl
            ld   a,(P_LAST)
            ld   (hl),a
            inc  hl
            call ps_age
            ld   (hl),a
            inc  hl
            ld   a,(P_FLAGS)
            ld   (hl),a
            inc  hl
            ld   a,(P_SL)
            ld   (hl),a
            inc  hl
            ld   a,(P_SR)
            ld   (hl),a
            inc  hl
            ld   a,(P_GEN)
            ld   (hl),a
            inc  hl
            ld   a,(P_HC)
            ld   (hl),a
            inc  hl
            ld   a,(P_HITS)
            ld   (hl),a
            inc  hl
            ld   a,(P_BX)
            ld   (hl),a
            inc  hl
            ld   a,(P_BY)
            ld   (hl),a
            inc  hl
            ld   a,(P_VX)
            ld   (hl),a
            inc  hl
            ld   a,(P_VY)
            ld   (hl),a
            ld   hl,P_BUF
            ld   a,15
            jp   nq_send
; A = fotogramas desde que llego el sello que devuelvo (el otro lo resta de su ping: eso no es red, es espera mia)
ps_age:     ld   a,(P_RXF)
            ld   b,a
            ld   a,(P_FR)
            sub  b
            ret

; HL = mensaje, A = longitud
pg_recv:    ld   c,a
            push hl
            ld   hl,P_RXN
            call cnt_inc
            pop  hl
            ld   a,(hl)
            cp   1
            ret  nz
            ld   a,c
            cp   15
            ret  c
            push hl
            pop  ix
            ld   a,(P_LAST)         ; un mensaje mas viejo que el ultimo (llegado tarde): se ignora
            ld   b,a
            ld   a,(ix+1)
            sub  b
            bit  7,a
            ret  nz
            ld   a,(ix+8)           ; de una partida anterior a la mia (los mensajes en camino cuando el creador empezo otra): se ignora entero
            ld   b,a
            ld   a,(P_GEN)
            ld   c,a
            ld   a,b
            sub  c
            jr   z,pr_ok
            bit  7,a
            ret  nz
pr_ok:      ld   hl,P_ACN
            call cnt_inc
            ld   a,(ix+1)
            ld   (P_LAST),a
            ld   a,(ix+2)
            ld   (P_OYT),a
            ld   b,(ix+3)           ; el sello mio que me devuelve
            ld   c,(ix+4)           ; y la edad que tenia
            call rv_ping
            call rv_score
            jp   rv_ball

; los marcadores y la partida: el creador manda con la nueva partida; un marcador que sube es un tanto del rival o mio
rv_score:   ld   a,(ix+8)
            ld   b,a
            ld   a,(P_GEN)
            cp   b
            jr   z,rs_same
            ld   a,b                ; otra partida: marcadores del mensaje, saca el creador
            ld   (P_GEN),a
            ld   a,(ix+6)
            ld   (P_SL),a
            ld   a,(ix+7)
            ld   (P_SR),a
            ld   a,(P_FLAGS)
            and  0FDh
            ld   (P_FLAGS),a
            call pg_chk_over
            xor  a
            jp   pg_begin_serve
rs_same:    ld   c,0                ; bit 0: ha subido el de la izquierda; bit 1: el de la derecha
            ld   a,(P_SL)
            ld   b,a
            ld   a,(ix+6)
            cp   b
            jr   c,rs_1
            jr   z,rs_1
            ld   (P_SL),a
            set  0,c
rs_1:       ld   a,(P_SR)
            ld   b,a
            ld   a,(ix+7)
            cp   b
            jr   c,rs_2
            jr   z,rs_2
            ld   (P_SR),a
            set  1,c
rs_2:       ld   a,c
            or   a
            ret  z
            call pg_chk_over
            ld   a,0                ; tanto del otro: ha perdido el de la derecha (1) si sube el de la izquierda, o el de la izquierda (0)
            bit  0,c
            jr   z,rs_l
            ld   a,1
rs_l:
IF SERVE_TO_LOSER = 0
            xor  1
ENDIF
            jp   pg_begin_serve

; un golpe, un rebote en una pared de hockey o un saque del otro (cambia su contador): la bola es la suya y la adelanto; si no, sigo su bola si es suya
rv_ball:    ld   a,(P_FLAGS)
            and  2
            ret  nz
            ld   a,(ix+9)
            ld   b,a
            ld   a,(P_PHC)
            cp   b
            jr   z,rb_no
            ld   a,b
            ld   (P_PHC),a
            ld   a,(ix+10)
            ld   (P_HITS),a
            ld   a,(ix+11)
            ld   (P_BX),a
            ld   a,(ix+12)
            ld   (P_BY),a
            ld   a,(ix+13)
            ld   (P_VX),a
            ld   a,(ix+14)
            ld   (P_VY),a
            ld   a,(P_FLAGS)
            and  0FEh               ; ya no se esta sacando
            ld   (P_FLAGS),a
            xor  a
            ld   (P_PEND),a
            ld   a,(P_GAME)         ; Squash: tras su golpe, me toca a mi devolver la bola
            cp   2
            jr   nz,rb_nt
            ld   a,(P_ROLE)
            ld   (P_TURN),a
rb_nt:      jp   rv_lead
rb_no:      ld   a,(ix+5)           ; el otro sigue sacando (en su mensaje la bola esta quieta, vieja: yo ya he sacado y no lo sabe): no le hago caso
            and  1
            ret  nz
            ld   a,(P_FLAGS)
            and  1
            ret  nz                 ; sacando: la bola esta quieta
            call is_owner
            or   a
            ret  nz                 ; la bola es mia: no hago caso de la del otro
            ld   de,LBX             ; la bola de antes (la local), por si hay que conservarla
            ld   a,(P_BX)
            ld   (de),a
            inc  de
            ld   a,(P_BY)
            ld   (de),a
            inc  de
            ld   a,(P_VX)
            ld   (de),a
            inc  de
            ld   a,(P_VY)
            ld   (de),a
            ld   a,(ix+11)
            ld   (P_BX),a
            ld   a,(ix+12)
            ld   (P_BY),a
            ld   a,(ix+13)
            ld   (P_VX),a
            ld   a,(ix+14)
            ld   (P_VY),a
            call rv_lead
            jp   rv_cmp

; adelanta la bola la mitad del ping (hasta 6 fotogramas): el estado llega viejo
rv_lead:    ld   a,(P_PING)
            srl  a
            cp   7
            jr   c,rl_ok
            ld   a,6
rl_ok:      or   a
            ret  z
            ld   b,a
            ld   a,1
            ld   (P_SIM),a
rl_ls:      push bc
            call ball_step
            pop  bc
            djnz rl_ls
            xor  a
            ld   (P_SIM),a
            ret

rv_cmp:     ld   a,(LVX)            ; el estado contradice lo que yo veia? (la direccion de la bola en x o en y, si no estaba quieta)
            ld   b,a
            ld   a,(P_VX)
            call rv_dis
            jr   c,rv_dir
            ld   a,(LVY)
            ld   b,a
            ld   a,(P_VY)
            call rv_dis
            jr   nc,rv_blend
rv_dir:     ld   a,(P_PEND)         ; si acabo de rebotar y el creador aun no lo sabe, no le hago caso: la bola sigue la mia
            or   a
            jr   z,rv_snap
            ld   hl,LBX
            ld   de,P_BX
            ld   bc,4
            ldir
            ret
rv_blend:   xor  a                  ; de acuerdo: el creador confirma lo mio y la bola se corrige la mitad del error (si es grande, de golpe)
            ld   (P_PEND),a
            ld   a,(LBX)
            ld   hl,P_BX
            call rv_half
            ld   a,(LBY)
            ld   hl,P_BY
            jp   rv_half
rv_snap:    xor  a
            ld   (P_PEND),a
            ret


; CF = 1 si A (nuevo) y B (viejo) son velocidades de signo contrario (el 0 no cuenta)
rv_dis:     or   a
            jr   z,rd_no
            ld   c,a
            ld   a,b
            or   a
            jr   z,rd_no
            xor  c
            rlca
            ret
rd_no:      or   a
            ret
; A = valor local de antes, (HL) = el del estado (ya adelantado): si estan a menos de 24, (HL) = A + (diferencia / 2)
rv_half:    ld   c,a
            ld   a,(hl)
            sub  c
            jr   c,rh_neg
            cp   24
            ret  nc
            srl  a
            add  a,c
            ld   (hl),a
            ret
rh_neg:     neg
            cp   24
            ret  nc
            srl  a
            ld   b,a
            ld   a,c
            sub  b
            ld   (hl),a
            ret
; B = el sello mio que me devuelven, C = la edad que tenia: ping = (ahora - sello) - edad
rv_ping:    ld   a,(P_FR)
            ld   (P_RXF),a
            sub  b
            sub  c
            jr   nc,rp_ok
            xor  a
rp_ok:      ld   (P_PING),a
            ld   a,1
            ld   (P_RXOK),a
            xor  a
            ld   (P_SIL),a
            ret



; ---------------------------------------------------------------------
;  Dibujo: los sprites cada fotograma; lo de abajo solo cuando cambia
; ---------------------------------------------------------------------
pg_draw:    call pg_bats
            ld   a,(P_BX)
            ld   b,a
            ld   a,(P_BY)
            ld   c,a
            ld   a,SPR_BALL
            call sp_pos
            call pg_which           ; el aviso de abajo: cual toca
            ld   (P_MSG),a
            ; los tantos, grandes, a los lados del titulo (dos cifras): solo si cambian
            ld   hl,P_DS
            ld   a,(P_SL)
            cp   (hl)
            jr   nz,dd_sc
            inc  hl
            ld   a,(P_SR)
            cp   (hl)
            jr   z,dd_ms
dd_sc:      ld   a,(P_SL)
            ld   (P_DS),a
            ld   c,7
            call dr_score
            ld   a,(P_SR)
            ld   (P_DS+1),a
            ld   c,22
            call dr_score
            ; el aviso (la fila de abajo, a partir de la columna 12): solo si cambia (es casi siempre el mismo); la fila se borra entera
dd_ms:      ld   a,(P_MSG)
            ld   hl,P_DS+3
            cp   (hl)
            jr   z,dd_pg
            ld   (hl),a
            ld   b,23
            call gclr_row
            ld   a,(P_MSG)
            add  a,a
            ld   e,a
            ld   d,0
            ld   hl,msg_tab
            add  hl,de
            ld   e,(hl)
            inc  hl
            ld   d,(hl)
            ex   de,hl
            ld   bc,23*256+12
            call gprint_at
            xor  a                  ; el borrado se llevo el ping: que se repinte si se ve
            ld   (P_DSS),a
            ; el ping, "PING nnnn MS", en las 12 celdas de la izquierda: con la tecla P; solo se repinta si cambia
dd_pg:      ld   a,(P_SHP)
            ld   hl,P_DSS
            cp   (hl)
            jr   z,dp_same
            ld   (hl),a
            or   a
            jr   nz,dp_draw
            ld   hl,t_blank12       ; ya no se ve: se borran sus celdas
            ld   bc,23*256+0
            jp   gprint_at
dp_same:    or   a
            ret  z                  ; no se ve y sigue sin verse
            ld   a,(P_PING)
            ld   hl,P_DS+2
            cp   (hl)
            ret  z
dp_draw:    ld   a,(P_PING)
            ld   (P_DS+2),a
            ld   l,a                ; ms = ping * 20 (en fotogramas de 20 ms)
            ld   h,0
            ld   d,h
            ld   e,l
            add  hl,hl
            add  hl,hl
            ld   b,h
            ld   c,l                ; 4 p
            add  hl,hl
            add  hl,hl              ; 16 p
            add  hl,bc              ; 20 p
            ld   de,P_TXT+5
            call dec4
            ld   hl,t_ping
            ld   de,P_TXT
            ld   bc,5
            ldir
            ld   hl,P_TXT+5+4
            ld   (hl),' '
            inc  hl
            ld   (hl),'M'
            inc  hl
            ld   (hl),'S'
            inc  hl
            ld   (hl),0
            ld   hl,P_TXT
            ld   bc,23*256+0
            jp   gprint_at

; las palas de cada jugador (hasta 2 por jugador, de 4 o 2 sprites cada una): sprites (jugador * 2 + pala) * 4 + trozo; la mia en mi y, la del rival en la suya
pg_bats:    xor  a
            ld   (DW_R),a
pb_r:       xor  a
            ld   (DW_J),a
pb_j:       ld   a,(DW_R)
            ld   b,a
            ld   a,(DW_J)
            ld   c,a
            call bat_x
            ld   (DW_X),a
            ld   a,(DW_R)
            ld   b,a
            ld   a,(P_ROLE)
            cp   b
            ld   a,(P_MY)
            jr   z,pb_y
            ld   a,(P_OY)
pb_y:       ld   (DW_Y),a
            ld   a,(DW_R)
            add  a,a
            ld   b,a
            ld   a,(DW_J)
            add  a,b
            add  a,a
            add  a,a
            ld   (DW_S),a
            ld   a,(P_BATH)
            srl  a
            srl  a
            srl  a
            ld   b,a
pb_s:       push bc
            ld   a,(DW_X)
            ld   b,a
            ld   a,(DW_Y)
            ld   c,a
            ld   a,(DW_S)
            call sp_pos
            ld   hl,DW_S
            inc  (hl)
            ld   a,(DW_Y)
            add  a,8
            ld   (DW_Y),a
            pop  bc
            djnz pb_s
            ld   hl,DW_J
            inc  (hl)
            call nbats
            ld   b,a
            ld   a,(DW_J)
            cp   b
            jr   nz,pb_j
            ld   hl,DW_R
            inc  (hl)
            call nplayers
            ld   b,a
            ld   a,(DW_R)
            cp   b
            jr   nz,pb_r
            ret

; A = tantos (0-99), C = columna de las decenas (la de las unidades, la siguiente): las dos cifras, con la de decenas en blanco si es 0
dr_score:   ld   e,0
ds_d:       cp   10
            jr   c,ds_u
            sub  10
            inc  e
            jr   ds_d
ds_u:       add  a,28
            push af                 ; la cifra de las unidades
            ld   a,e
            or   a
            jr   z,ds_t
            add  a,28
ds_t:       push bc
            ld   b,ROW_TITLE
            push af
            call gaddr
            pop  af
            call gglyph
            pop  bc
            inc  c
            ld   b,ROW_TITLE
            call gaddr
            pop  af
            jp   gglyph

; cual aviso toca (el numero de t_m...): 1 conectando, 2 sin oir al rival, 3 sacando, 4-7 fin de partida, 0 el normal
pg_which:   ld   a,(P_RXOK)
            or   a
            ld   a,1
            ret  z
            ld   a,(P_SIL)
            cp   100                ; ~2 s sin oir nada
            ld   a,2
            ret  nc
            ld   a,(P_FLAGS)
            bit  1,a
            jr   nz,pw_over
            bit  0,a
            ld   a,3
            ret  nz
            xor  a
            ret
pw_over:    ld   a,(P_SL)           ; B = 1 si ha ganado la izquierda (el creador)
            cp   WIN_SCORE
            ld   b,0
            jr   c,pw_w1
            ld   b,1
pw_w1:      ld   a,(P_ROLE)
            xor  b
            ld   c,a                ; el bit 0 de C: 1 si he ganado yo
            ld   a,(P_ROLE)
            or   a
            ld   a,6                ; el creador: 6 gano / 7 pierdo (ENTER = otra partida)
            jr   z,pw_c
            ld   a,4                ; el invitado: 4 gano / 5 pierdo (a esperar al creador)
pw_c:       bit  0,c
            ret  nz
            inc  a
            ret

; HL = numero (0-9999): 4 caracteres ASCII en (DE), con los ceros de la izquierda como espacios
dec4:       push de
            ld   bc,-1000
            call d4_dig
            ld   bc,-100
            call d4_dig
            ld   bc,-10
            call d4_dig
            ld   a,l
            add  a,'0'
            ld   (de),a
            pop  hl
            ld   b,3
d4_z:       ld   a,(hl)
            cp   '0'
            ret  nz
            ld   (hl),' '
            inc  hl
            djnz d4_z
            ret
d4_dig:     ld   a,'0'-1
d4_l:       inc  a
            add  hl,bc
            jr   c,d4_l
            sbc  hl,bc
            ld   (de),a
            inc  de
            ret



; ---------------------------------------------------------------------
;  Datos: sprites (color, pixeles y mascara de cada fila) y textos
; ---------------------------------------------------------------------
spr_green:  defb 0C0h,0C0h,0C0h,0C0h,0C0h,0C0h,0C0h,0C0h
            defb 0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh
            defb 0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh
spr_red:    defb 0A0h,0A0h,0A0h,0A0h,0A0h,0A0h,0A0h,0A0h
            defb 0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh
            defb 0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh,0FFh
spr_ball:   defb 0F0h,0F0h,0F0h,0F0h,0F0h,0F0h,0F0h,0F0h
            defb 03Ch,07Eh,0FFh,0FFh,0FFh,0FFh,07Eh,03Ch
            defb 03Ch,07Eh,0FFh,0FFh,0FFh,0FFh,07Eh,03Ch

t_title:    defm "NETWORK SPORTS"
            defb 0
gname_tab:  defw gn_0
            defb 13,0               ; el texto y la columna (centrado entre los marcadores)
            defw gn_1
            defb 13,0
            defw gn_2
            defb 13,0
            defw gn_3
            defb 12,0
gn_0:       defm "TENNIS"
            defb 0
gn_1:       defm "HOCKEY"
            defb 0
gn_2:       defm "SQUASH"
            defb 0
gn_3:       defm "PRACTICE"
            defb 0
t_sides:    defm "THE LOSER SERVES"
            defb 0
t_connecting: defm "OPENING THE ROOM CHANNEL..."
            defb 0
t_p1:       defm "P1"
            defb 0
t_p2:       defm "P2"
            defb 0
t_hits:     defm "HITS"
            defb 0
t_best:     defm "BEST"
            defb 0
t_ping:     defm "PING "
t_blank12:  defm "            "
            defb 0
t_sel:      defm "CHOOSE THE GAME"
            defb 0
t_g1:       defm "1 TENNIS"
            defb 0
t_g2:       defm "2 HOCKEY"
            defb 0
t_g3:       defm "3 SQUASH"
            defb 0
t_g4:       defm "4 PRACTICE (ALONE)"
            defb 0
t_opt:      defm "OPTIONS"
            defb 0
t_o1:       defw t_o1a,t_o1b
t_o2:       defw t_o2a,t_o2b
t_o3:       defw t_o3a,t_o3b
t_o1a:      defm "1 BATS:   NORMAL"
            defb 0
t_o1b:      defm "1 BATS:   SMALL "
            defb 0
t_o2a:      defm "2 BALL:   SLOW"
            defb 0
t_o2b:      defm "2 BALL:   FAST"
            defb 0
t_o3a:      defm "3 ANGLES: SOFT"
            defb 0
t_o3b:      defm "3 ANGLES: WIDE"
            defb 0
t_go:       defm "ENTER = START"
            defb 0
msg_tab:    defw m_0,m_1,m_2,m_3,m_4,m_5,m_6,m_7
m_0:        defm "Q/A  P=PING  X=QUIT"
            defb 0
m_1:        defm "WAITING FOR RIVAL"
            defb 0
m_2:        defm "RIVAL NOT ANSWERING"
            defb 0
m_3:        defm "GET READY!"
            defb 0
m_4:        defm "YOU WIN! WAIT HOST"
            defb 0
m_5:        defm "YOU LOSE. WAIT HOST"
            defb 0
m_6:        defm "YOU WIN! ENTER=AGAIN"
            defb 0
m_7:        defm "YOU LOSE ENTER=AGAIN"
            defb 0

            include "../NETCHAT/net.inc"
            include "../NETCHAT/netq.inc"
            include "gfx.inc"
