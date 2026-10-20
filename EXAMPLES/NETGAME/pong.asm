; =====================================================================
;  PONG.ASM - un Pong para dos ZX81 por la red (MQTT), para ver si la latencia del SD81 Booster da para juegos en tiempo real.
;  Diseno: claude/planning/latencia_red.md y EXAMPLES/NETCHAT/netq.inc (mensajes de red sin esperas).
;
;  - LA BOLA ES DE QUIEN LA RECIBE: la decide (golpe o tanto) el jugador hacia cuya mitad viaja, con SU pala y SU bola al dia, sin esperar a la red. Cuando
;    golpea, la bola pasa a ser del otro: se lo dice en un contador de golpes dentro del mensaje (con la posicion, la velocidad y los golpes seguidos) y el
;    otro la adelanta lo que tardo el mensaje y sigue desde ahi. Entre tanto el que no manda la bola la sigue y la mueve con la misma fisica (paredes).
;    Asi ningun golpe se discute: el que golpea lo ve y vale. La latencia solo se nota en que el rival ve el golpe un poco despues.
;  - Tantos: los marca el que falla (la bola pasa su linea) y los suma al otro; el mensaje lleva los dos marcadores. El que ha perdido el tanto saca (la bola
;    sale del centro hacia el, tras 1 s), como en el Pong original (SERVE_TO_LOSER = 0 lo cambia: saca el que marco). Gana quien llega a 15; el CREADOR decide
;    la nueva partida (ENTER), que se anuncia con un numero de partida (P_GEN) en los mensajes.
;  - Cada uno mueve SU pala sin retraso. El ping sale abajo (ida y vuelta, en ms, por los sellos de fotograma y la "edad" de lo que se devuelve).
;  - La red va por netq.inc (sin esperas, sin firmar); la sala, el menu y el teclado, del lobby de los demas juegos.
;  - Graficos: sprites por hardware (Apendice H): palas de 4 sprites (0-3 izquierda, 4-7 derecha) y la bola (8).
;  - Teclas: Q / 7 = subir, A / 6 = bajar, X = salir, ENTER = otra partida (el creador, cuando acaba).
;
;  Mensaje (cada fotograma, los dos igual, 15 bytes): [1][fr][mi pala][sello del otro][edad][flags][tantos izq][tantos der][partida][golpes: contador][golpes seguidos]
;  [bola x][bola y][vx][vy]. El contador de golpes sube con cada golpe y con cada saque del que lo manda.
;
;  Memoria:   $6000-$7FFF el programa;  $8800- pantalla de texto del lobby;  $9000-$9750 variables;  $A000-$BAFF la pantalla grafica
;  Cargar:    PONG.B81 (CLEAR 24575, LOAD FAST ... CODE 24576, RAND USR 24576).
;  Ensamblar: pasmo pong.asm pong.bin pong.sym   (compila.bat)
; =====================================================================

NB_BASE     equ 9000h
UI_BASE     equ 9400h
PG_BASE     equ 9440h
NQ_BASE     equ 9500h
DFILE       equ 8800h           ; pantalla de texto del lobby
GFX_SQUARES equ 0
BOARD_ROW   equ 0               ; (gfx.inc: solo para draw_square)
BOARD_COL   equ 0

; --- variables ---
P_ROLE      equ PG_BASE+0       ; 0 = creador (izquierda), 1 = invitado (derecha)
P_FR        equ PG_BASE+1       ; contador de fotogramas
P_BX        equ PG_BASE+2       ; la bola: x, y (pixeles de pantalla), velocidad con signo
P_BY        equ PG_BASE+3
P_VX        equ PG_BASE+4
P_VY        equ PG_BASE+5
P_MY        equ PG_BASE+6       ; mi pala (y de arriba)
P_OY        equ PG_BASE+7       ; la del rival (la que se ve)
P_SL        equ PG_BASE+8       ; tantos: izquierda, derecha
P_SR        equ PG_BASE+9
P_FLAGS     equ PG_BASE+10      ; bit 0 = sacando (bola quieta en el centro), bit 1 = partida acabada
P_SERVE     equ PG_BASE+11      ; fotogramas que faltan para sacar (el que saca)
P_PING      equ PG_BASE+12      ; ultima ida y vuelta, en fotogramas
P_LAST      equ PG_BASE+13      ; el ultimo sello del rival (se lo devuelvo)
P_HITS      equ PG_BASE+14      ; golpes seguidos del punto (la velocidad sube)
P_RXOK      equ PG_BASE+17      ; 1 si ya ha llegado algo del rival
P_SIL       equ PG_BASE+18      ; fotogramas sin oir al rival
P_MSG       equ PG_BASE+19      ; el aviso que se ve abajo (0 normal)
P_DS        equ PG_BASE+20      ; (4) lo ultimo dibujado: tantos, ping, aviso
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
P_SHP       equ PG_BASE+80      ; 1 = se ve el ping abajo (tecla P)
P_KP        equ PG_BASE+81      ; la tecla P en el fotograma anterior (1 = suelta)
P_DSS       equ PG_BASE+24      ; (P_DS+4) lo ultimo dibujado del ping: 1 = visible
SV_VAL      equ PG_BASE+96      ; (lo que pide el lobby; este juego no guarda partidas)
SV_CONT     equ PG_BASE+97
GM_ME       equ PG_BASE+98
GM_PLY      equ PG_BASE+99      ; (2)

SPR_SEL     equ 2100
SPR_EN      equ 2101
SPR_XL      equ 2102
SPR_XH      equ 2103
SPR_Y       equ 2104
SPR_COL     equ 2105

PL_X        equ 12              ; pala izquierda: columnas 12-19
PR_X        equ 236             ; derecha: 236-243
PAD_H       equ 32
PAD_MIN     equ 16
PAD_MAX     equ 144
WIN_SCORE   equ 15
SERVE_TO_LOSER equ 1            ; 1 = saca el que ha perdido el tanto (la bola va hacia el); 0 = el que lo ha marcado
FRDLY       equ 1950            ; la pausa fija de cada fotograma (vueltas de 26 T): el trabajo + esto = ~20 ms

            org  24576

            include "lobby.inc"

; ---------------------------------------------------------------------
;  Lo que pide el lobby (sin partida por turnos, sin guardar)
; ---------------------------------------------------------------------
pick_tc:    or   a
            ret
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
pl_ok:      call pg_init
pg_loop:    ld   hl,P_FR
            inc  (hl)
            call pg_keys
            jp   c,pg_quit
            call pg_pkey
            call pg_osm
            call pg_phys
            call pg_net
            call pg_draw
            call pg_delay
            jr   pg_loop

pg_quit:    call pg_sprites_off
            call nq_close           ; que el modem conteste lo pendiente antes de volver a net.inc
            call gfx_off
            call room_leave
            jp   menu

; tecla P: ense�a u oculta el ping (en ms) abajo; al pulsarla, una vez
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

pg_delay:   ld   bc,FRDLY
pd_lp:      dec  bc
            ld   a,b
            or   c
            jr   nz,pd_lp
            ret

; ---------------------------------------------------------------------
;  Estado inicial, pista y sprites
; ---------------------------------------------------------------------
pg_init:    ld   a,(NB_SLOT)
            push af
            ld   hl,PG_BASE
            ld   de,PG_BASE+1
            ld   bc,95
            ld   (hl),0
            ldir
            pop  af
            ld   (P_ROLE),a
            ld   a,80
            ld   (P_MY),a
            ld   (P_OY),a
            ld   (P_OYT),a
            ld   a,1
            ld   (P_KP),a
            ld   a,0FFh             ; fuerza el primer dibujo de los marcadores (y, abajo, el del aviso)
            ld   (P_DS),a
            xor  a                  ; la primera vez saca el creador (la bola va hacia el)
            call pg_begin_serve
            call gfx_on
            ; la pista: una raya arriba (lineas 14 y 15) y otra abajo (176 y 177), y la red en el centro
            ld   b,1
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
            ld   b,2
pi_net:     push bc
            ld   c,15
            call gaddr
            ld   b,4
pi_dash:    ld   (hl),001h          ; la red, centrada: el ultimo punto de la celda 15 (x = 127) y el primero de la 16 (x = 128)
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
            ; textos fijos
            ld   hl,t_title
            ld   bc,ROW_TITLE*256+14
            call gprint_at
            ld   hl,t_p1
            ld   bc,ROW_TITLE*256+2
            call gprint_at
            ld   hl,t_p2
            ld   bc,ROW_TITLE*256+28
            call gprint_at
            ld   a,(P_ROLE)         ; mi etiqueta, en verde; la del rival, en rojo
            or   a
            ld   a,044h
            ld   d,042h
            jr   z,pi_att
            ld   a,042h
            ld   d,044h
pi_att:     push de
            ld   b,ROW_TITLE
            ld   c,2
            ld   e,2
            call gattr_span
            pop  de
            ld   a,d
            ld   b,ROW_TITLE
            ld   c,28
            ld   e,2
            call gattr_span
            ; sprites: palas (verde la mia, roja la del rival) y bola
            ld   a,(P_ROLE)
            or   a
            ld   hl,spr_green
            ld   de,spr_red
            jr   z,pi_def
            ex   de,hl
pi_def:     push de                 ; izquierda = HL, derecha = DE
            ld   b,4
            ld   c,0
pi_dl:      push bc
            push hl
            ld   a,c
            call spr_def
            pop  hl
            pop  bc
            inc  c
            djnz pi_dl
            pop  hl                 ; el color de la derecha
            ld   b,4
            ld   c,4
pi_dr:      push bc
            push hl
            ld   a,c
            call spr_def
            pop  hl
            pop  bc
            inc  c
            djnz pi_dr
            ld   a,8
            ld   hl,spr_ball
            call spr_def
            ld   a,0FFh             ; fuerza el primer dibujo del aviso
            ld   (P_DS+3),a
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

pg_sprites_off: ld b,9
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
            cp   PAD_MIN
            jr   nc,pk_set
            ld   a,PAD_MIN
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
            cp   PAD_MAX+1
            jr   c,pk_set
            ld   a,PAD_MAX
pk_set:     ld   (P_MY),a
pk_x:       ld   bc,0FEFEh          ; fila SHIFT Z X C V
            in   a,(c)
            bit  2,a
            jr   nz,pk_no
            scf
            ret
pk_no:      or   a
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

; saco yo: la bola sale del centro hacia mi, con una inclinacion que sale del contador de fotogramas; es un "golpe" mas (el otro lo ve en el contador)
pg_launch:  ld   a,(P_FLAGS)
            and  0FEh
            ld   (P_FLAGS),a
            ld   b,3
            ld   a,(P_ROLE)
            or   a
            ld   a,b
            jr   nz,pl_set          ; derecha: la bola va a la derecha
            neg                     ; izquierda: a la izquierda
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

; la bola era mia y ha pasado mi linea: tanto del otro; saca (segun SERVE_TO_LOSER) yo o el
pg_missed:  ld   a,(P_ROLE)
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

; A = 1 si la bola va hacia mi (es mia: la decido yo), 0 si no (o esta quieta)
is_owner:   ld   a,(P_VX)
            or   a
            jr   z,io_no
            rlca
            ld   a,0
            rla                     ; A = 1 si va a la izquierda (vx negativa)
            ld   b,a
            ld   a,(P_ROLE)
            xor  1                  ; el de la izquierda (0) es el dueño con vx negativa (1); el de la derecha, con vx positiva (0)
            cp   b
            jr   nz,io_no
            ld   a,1
            ret
io_no:      xor  a
            ret

; ---------------------------------------------------------------------
;  La bola, un fotograma: paredes y, solo si es mia, mi pala y mi linea
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
bs_pad:     ld   a,(P_SIM)          ; (adelantando la bola: ni golpes ni tantos)
            or   a
            jr   nz,bs_clamp
            call is_owner
            or   a
            jr   z,bs_clamp         ; no es mia: la mueve (y decide) el otro
            ld   a,(P_ROLE)
            or   a
            jr   nz,bs_r
            ld   a,(P_BX)           ; soy el de la izquierda y la bola viene hacia mi
            cp   5
            jp   c,pg_missed
            cp   21
            ret  nc
            ld   a,(P_MY)
            call hit_test           ; CF = 1 si la toca (C = y de mi pala)
            ret  nc
            ld   a,21
            ld   (P_BX),a
            call hit_speed          ; A = nueva velocidad
            jr   bs_hit
bs_r:       ld   a,(P_BX)           ; soy el de la derecha
            cp   244
            jp   nc,pg_missed
            cp   228
            ret  c
            ld   a,(P_MY)
            call hit_test
            ret  nc
            ld   a,227
            ld   (P_BX),a
            call hit_speed
            neg
bs_hit:     ld   (P_VX),a
            call hit_vy
            ld   hl,P_HC            ; golpe: el otro lo ve en el contador y la bola pasa a ser suya
            inc  (hl)
            ld   a,12               ; y hasta que lo vea, no hago caso de sus estados anteriores al golpe
            ld   (P_PEND),a
            ret
bs_clamp:   ld   a,(P_BX)           ; no es mia: espera en los bordes (4-244) lo que diga el dueño
            cp   4
            jr   nc,bs_g1
            ld   a,4
bs_g1:      cp   245
            jr   c,bs_g2
            ld   a,244
bs_g2:      ld   (P_BX),a
            ret

neg_vy:     ld   a,(P_VY)
            neg
            ld   (P_VY),a
            ret

; un rebote en una pared: hasta que el dueño lo confirme se ignoran sus estados que lo contradicen
set_pend:   ld   a,(P_SIM)
            or   a
            ret  nz
            ld   a,12
            ld   (P_PEND),a
            ret

; A = y de arriba de la pala izquierda / derecha
left_y:     ld   a,(P_ROLE)
            or   a
            ld   a,(P_MY)
            ret  z
            ld   a,(P_OY)
            ret
right_y:    ld   a,(P_ROLE)
            or   a
            ld   a,(P_OY)
            ret  z
            ld   a,(P_MY)
            ret

; A = y de la pala: CF = 1 si la bola (y de arriba P_BY, 8 de alto) solapa con ella (32 de alto). Deja C = y de la pala
hit_test:   ld   c,a
            ld   a,(P_BY)
            add  a,8
            cp   c
            jr   c,ht_no
            jr   z,ht_no            ; by + 8 <= pala: por encima
            ld   a,c
            add  a,PAD_H
            ld   d,a
            ld   a,(P_BY)
            cp   d
            jr   nc,ht_no           ; by >= pala + 32: por debajo
            scf
            ret
ht_no:      or   a
            ret


; un golpe mas: A = la nueva velocidad (3, +1 cada dos golpes, hasta 6)
hit_speed:  ld   a,(P_HITS)
            inc  a
            ld   (P_HITS),a
            srl  a
            add  a,3
            cp   7
            ret  c
            ld   a,6
            ret

; la inclinacion segun donde toca la pala (C = y de la pala): (by - pala - 12) / 4, de -4 a 4
hit_vy:     ld   a,(P_BY)
            sub  c
            sub  12
            sra  a
            sra  a
            bit  7,a
            jr   z,hv_ok
            cp   0FCh
            jr   nc,hv_ok
            ld   a,0FCh
hv_ok:      ld   (P_VY),a
            ret

; ---------------------------------------------------------------------
;  La red: cada fotograma manda lo mio y trata lo que llegue
; ---------------------------------------------------------------------
pg_net:     call pg_send
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
pr_ok:      ld   a,(ix+1)
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

; un golpe o un saque del otro (cambia su contador): la bola es la suya y la adelanto; si no, sigo su bola si es suya
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
            jp   rv_lead
rb_no:      ld   a,(P_FLAGS)
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
pg_draw:    call left_y
            ld   e,a                ; las 4 piezas de cada pala: sprites 0-3 y 4-7
            ld   d,0
pd_l:       push de
            ld   a,d
            ld   b,PL_X
            ld   c,e
            call sp_pos
            pop  de
            ld   a,e
            add  a,8
            ld   e,a
            inc  d
            ld   a,d
            cp   4
            jr   nz,pd_l
            call right_y
            ld   e,a
pd_r:       push de
            ld   a,d
            ld   b,PR_X
            ld   c,e
            call sp_pos
            pop  de
            ld   a,e
            add  a,8
            ld   e,a
            inc  d
            ld   a,d
            cp   8
            jr   nz,pd_r
            ld   a,(P_BX)
            ld   b,a
            ld   a,(P_BY)
            ld   c,a
            ld   a,8
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

; A = tantos (0-15), C = columna de las decenas (la de las unidades, la siguiente): las dos cifras, con la de decenas en blanco si es 0
dr_score:   ld   e,0
            cp   10
            jr   c,ds_u
            sub  10
            inc  e
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

t_title:    defm "PONG"
            defb 0
t_sides:    defm "PLAYER 1 SERVES"
            defb 0
t_connecting: defm "OPENING THE ROOM CHANNEL..."
            defb 0
t_p1:       defm "P1"
            defb 0
t_p2:       defm "P2"
            defb 0
t_ping:     defm "PING "
t_blank12:  defm "            "
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

