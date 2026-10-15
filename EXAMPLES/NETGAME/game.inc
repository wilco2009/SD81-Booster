; =====================================================================
;  GAME.INC - capa C del juego en red: el controlador comun de una partida
;  por turnos entre dos ZX81, encima de net.inc (la sala y los mensajes
;  firmados). No sabe nada de las reglas: las pone el juego con seis rutinas.
;  Diseno: claude/planning/juego_en_red.md, seccion 5.
;
;  El que incluye este fichero define ANTES:
;      GM_BASE      equ 7400h   ; RAM de trabajo (unos 100 bytes)
;      G_STATE_LEN  equ 42      ; bytes del estado completo del juego (<= 90)
;  y despues  include "net.inc"  y  include "game.inc"; el juego define:
;      g_reset      empieza una partida (posicion inicial)
;      g_valid      A = jugada, B = jugador (1 o 2): CF = 1 si NO es legal
;      g_apply      A = jugada, B = jugador: la hace (ya se sabe que es legal)
;      g_winner     A = 0 sigue, 1 o 2 gana ese jugador, 3 tablas
;      g_save       HL = destino: escribe los G_STATE_LEN bytes del estado
;      g_load       HL = origen: lee los G_STATE_LEN bytes y rehace lo derivado
;  Las jugadas son de un byte. Empieza el jugador 1 (el que crea la sala);
;  mueve el 1 con un numero de jugada par y el 2 con uno impar.
;
;  Rutinas (CALL; destruyen todos los registros):
;      game_start    tras room_new / room_join: yo soy el jugador NB_SLOT+1
;      game_new      partida nueva (sin avisar al rival)
;      game_rematch  partida nueva y se lo dice al rival
;      game_turn_of  A = el jugador al que le toca (1 o 2)
;      game_move     A = mi jugada. Devuelve A = 0 hecha y enviada, 1 no es legal,
;                    2 no me toca, 3 la partida ha acabado, 9 error de red (la
;                    jugada esta hecha en local: se puede repetir con game_resend)
;      game_resend   vuelve a mandar mi ultima jugada
;      game_poll     una vuelta de sondeo (usa room_poll_recv: se puede pedir antes
;                    con room_poll_send). Devuelve A =
;                      0 nada                    1 el rival ha jugado (GM_MOVE)
;                      2 el rival ha entrado     3 el rival se ha ido
;                      4 estado completo recibido (se ha vuelto a sincronizar)
;                      5 desincronizado: he pedido el estado   7 error del modem
;                      8 el rival quiere otra partida (ya esta puesta)
;  Variables: GM_ME (mi numero de jugador), GM_PLY (2 bytes: jugadas hechas),
;  GM_OVER (0 sigue, 1 o 2 ha ganado, 3 tablas), GM_MOVE (ultima jugada del rival).
;
;  Mensajes (payload; el turno va en NB_TURN = numero de jugada, que ya firma net.inc):
;      1 jugada     [1][jugada][hash 2 bytes del estado DESPUES de la jugada]
;      2 pido estado [2]
;      3 estado     [3][G_STATE_LEN bytes]
;      4 otra partida [4]
;  Cualquier desfase (una jugada que no es la siguiente, ilegal, o con otro hash)
;  se arregla pidiendo el estado completo. Las jugadas viejas (un retenido) se ignoran.
; =====================================================================

GM_ME       equ GM_BASE+0
GM_PLY      equ GM_BASE+1       ; (2)
GM_OVER     equ GM_BASE+3
GM_MOVE     equ GM_BASE+4
GM_HASH     equ GM_BASE+5       ; (2)
GM_TMP      equ GM_BASE+8       ; (G_STATE_LEN) estado para el resumen

game_start: ld   a,(NB_SLOT)
            inc  a
            ld   (GM_ME),a
            jp   game_new

game_new:   xor  a
            ld   (GM_PLY),a
            ld   (GM_PLY+1),a
            ld   (GM_OVER),a
            jp   g_reset

game_rematch: ld a,4
            ld   (NB_TX),a
            ld   a,1
            ld   (NB_LEN),a
            ld   hl,(GM_PLY)
            ld   (NB_TURN),hl
            call room_send
            push af
            call game_new
            pop  af
            ret

game_turn_of: ld a,(GM_PLY)
            and  1
            inc  a
            ret

gm_ply_inc: ld   hl,(GM_PLY)
            inc  hl
            ld   (GM_PLY),hl
            ret

; HL = resumen de 16 bits del estado (rota y mezcla cada byte)
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

game_move:  ld   (GM_MOVE),a
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
            ld   a,(GM_MOVE)
            call g_valid
            jr   nc,gm_ok
            ld   a,1
            ret
gm_ok:      ld   a,(GM_ME)
            ld   b,a
            ld   a,(GM_MOVE)
            call g_apply
            call gm_ply_inc
            call g_winner
            ld   (GM_OVER),a
game_resend: call game_hash
            ld   (GM_HASH),hl
            ld   a,1
            ld   (NB_TX),a
            ld   a,(GM_MOVE)
            ld   (NB_TX+1),a
            ld   hl,(GM_HASH)
            ld   (NB_TX+2),hl
            ld   a,4
            ld   (NB_LEN),a
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

game_send_state: ld a,3
            ld   (NB_TX),a
            ld   hl,NB_TX+1
            call g_save
            ld   a,G_STATE_LEN+1
            ld   (NB_LEN),a
            ld   hl,(GM_PLY)
            ld   (NB_TURN),hl
            jp   room_send

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
            jr   z,gp_move
            cp   2
            jr   z,gp_sreq
            cp   3
            jr   z,gp_state
            cp   4
            jr   z,gp_new
gp_ign:     xor  a
            ret
gp_sreq:    call game_send_state
            xor  a
            ret
gp_new:     call game_new
            ld   a,8
            ret
gp_state:   ld   a,(NB_LEN)
            cp   G_STATE_LEN+1
            jr   nz,gp_ign
            ld   hl,NB_RX+1
            call g_load
            ld   hl,(NB_TURN)
            ld   (GM_PLY),hl
            call g_winner
            ld   (GM_OVER),a
            ld   a,4
            ret

; una jugada del rival: tiene que ser la siguiente, ser legal y dejar el mismo estado
gp_move:    ld   a,(NB_LEN)
            cp   4
            jr   nz,gp_ign
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
            jr   c,gp_ign
            jr   z,gp_ign
            jr   gp_miss
gp_next:    ld   a,(GM_OVER)
            or   a
            jr   nz,gp_ign
            call game_turn_of
            ld   hl,GM_ME
            cp   (hl)
            jr   z,gp_miss          ; me tocaba a mi: algo no cuadra
            ld   a,(NB_RX+1)
            ld   (GM_MOVE),a
            call game_turn_of
            ld   b,a
            ld   a,(GM_MOVE)
            call g_valid
            jr   c,gp_miss
            call game_turn_of
            ld   b,a
            ld   a,(GM_MOVE)
            call g_apply
            call gm_ply_inc
            call g_winner
            ld   (GM_OVER),a
            call game_hash
            ld   de,(NB_RX+2)
            or   a
            sbc  hl,de
            jr   nz,gp_miss
            ld   a,1
            ret
gp_miss:    ld   a,2                ; pido el estado completo
            call gm_send_type
            ld   a,5
            ret
