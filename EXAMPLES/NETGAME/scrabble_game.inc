; =====================================================================
;  SCRABBLE_GAME.INC - el Scrabble como juego de game.inc: las seis rutinas que pide el controlador de partida
;  (g_reset, g_valid, g_apply, g_winner, g_save, g_load) mas g_hash (G_BIG = 1: el estado no cabe en un mensaje y el
;  resumen se calcula aqui, sin copiarlo). Va con G_MOVE_VAR = 1 (jugadas de longitud variable) y G_BIG = 1.
;
;  El que lo incluye define antes (ademas de lo de game.inc, scrabble.inc, scrabble_ui.inc y dict.inc):
;      G_STATE_LEN equ SB_STATE_LEN   (355)         G_MOVE_LEN equ 21
;      SC_DICT, SC_PRE, SC_I (1 byte cada una) y SC_RC (10 bytes)
;
;  LA JUGADA (lo que viaja en el mensaje y se guarda en GM_MOVE): [L][tipo][datos], L = su longitud entera (2 a 21)
;      tipo 1  jugar:   x, y, sentido, n y las n fichas (el WB del motor: L = 6 + n)
;      tipo 2  pasar:   nada (L = 2)
;      tipo 3  cambiar: n y las n fichas (L = 3 + n)
;
;  EL IDIOMA Y LA SEMILLA no se mandan: salen del codigo de sala. La primera letra del codigo (A o B, la elige el creador al
;  crear la sala) es el idioma (A ingles, B castellano) y la semilla es un resumen de las 10 letras del codigo y del numero
;  de partida de esa sala (SB_GNUM: 0 la primera, mas uno en cada "otra partida"), asi que los dos ordenadores barajan
;  la misma bolsa sin decirse nada. El estado entero (scrabble.inc, de SB_BOARD, SB_STATE_LEN bytes) es lo que se guarda,
;  se manda por trozos si hay que resincronizar y se resume en g_hash.
; =====================================================================

; partida nueva (game_new: al crear la sala, al unirse y en "otra partida")
g_reset:    xor  a
            ld   (SC_PRE),a
            ld   hl,NB_CODE81       ; ¿es la misma sala que la ultima partida? entonces es "otra partida": GNUM + 1
            ld   de,SC_RC
            ld   b,10
gr_c:       ld   a,(de)
            cp   (hl)
            jr   nz,gr_new
            inc  hl
            inc  de
            djnz gr_c
            ld   hl,SB_GNUM
            inc  (hl)
            jr   gr_go
gr_new:     ld   hl,NB_CODE81       ; una sala nueva: la partida 0
            ld   de,SC_RC
            ld   bc,10
            ldir
            xor  a
            ld   (SB_GNUM),a
gr_go:      ld   a,(NB_CODE81)      ; el idioma: la primera letra (A = 38 en codigos del ZX81)
            sub  38
            and  1
            ld   (SB_LANG),a
            ld   a,(SB_GNUM)        ; la semilla: el numero de partida y las 10 letras del codigo, mezcladas
            ld   (SB_SEED),a
            ld   a,5Ah
            ld   (SB_SEED+1),a
            ld   a,0A5h
            ld   (SB_SEED+2),a
            ld   a,3Ch
            ld   (SB_SEED+3),a
            xor  a
            ld   (SC_I),a
gr_l:       ld   a,(SC_I)
            ld   e,a
            ld   d,0
            ld   hl,NB_CODE81
            add  hl,de
            ld   c,(hl)
            ld   a,e
            and  3
            ld   e,a
            ld   hl,SB_SEED
            add  hl,de
            ld   a,(hl)
            xor  c
            ld   (hl),a
            call sb_rnd
            ld   hl,SC_I
            inc  (hl)
            ld   a,(hl)
            cp   10
            jr   nz,gr_l
            call sb_init
            call sbu_reset
            call sc_me
            call sb_open_dict
            ld   (SC_DICT),a
            ret

; SU_ME = mi jugador (0 o 1) a partir de GM_ME (1 o 2)
sc_me:      ld   a,(GM_ME)
            or   a
            ret  z
            dec  a
            ld   (SU_ME),a
            ret

; 0 sigue, 1 o 2 gana ese jugador, 3 tablas
g_winner:   ld   a,(SB_OVER)
            or   a
            ret  z
            ld   hl,(SB_SCORE)
            ld   de,(SB_SCORE+2)
            or   a
            sbc  hl,de
            ld   a,3
            ret  z
            ld   a,h                ; el signo de la diferencia: negativa = gana el 2
            rlca
            and  1
            inc  a
            ret

; HL = la jugada, B = jugador (1 o 2): CF = 1 si NO es legal. Deja la jugada en WB para g_apply
g_valid:    ld   a,(SC_PRE)         ; la pantalla ya la ha comprobado (sb_check dejo NF y SB_TOT): no se repite
            or   a
            jr   z,gv_go
            xor  a
            ld   (SC_PRE),a
            ret                     ; CF = 0
gv_go:      ld   a,(SB_TURN)        ; el que juega tiene que ser el del turno
            ld   c,a
            ld   a,b
            dec  a
            cp   c
            jr   nz,gv_no
            ld   a,(hl)             ; L: los datos miden L - 2
            sub  2
            ld   (SC_I),a
            inc  hl
            ld   a,(hl)
            inc  hl
            cp   1
            jr   z,gv_play
            cp   2
            jr   z,gv_pass
            cp   3
            jr   z,gv_exch
gv_no:      scf
            ret
gv_play:    ld   a,(SC_I)           ; x, y, sentido, n y las n fichas: 4 + n bytes
            cp   6
            jr   c,gv_no
            cp   20
            jr   nc,gv_no
            ld   c,a
            ld   b,0
            ld   de,WB
            ldir
            ld   a,(WB+3)
            add  a,4
            ld   hl,SC_I
            cp   (hl)
            jr   nz,gv_no
            call sb_check
            or   a
            jr   nz,gv_no
            ret                     ; CF = 0
gv_pass:    ld   a,(SB_OVER)
            or   a
            jr   nz,gv_no
            ret                     ; CF = 0
gv_exch:    ld   a,(SC_I)           ; n y las n fichas
            cp   2
            jr   c,gv_no
            cp   9
            jr   nc,gv_no
            ld   c,a
            ld   b,0
            ld   de,WB+3
            ldir
            ld   a,(WB+3)
            inc  a
            ld   hl,SC_I
            cp   (hl)
            jr   nz,gv_no
            call sb_xcheck
            or   a
            jr   nz,gv_no
            ret                     ; CF = 0

; HL = la jugada (ya comprobada), B = jugador (1 o 2): la hace
g_apply:    inc  hl
            ld   a,(hl)             ; el tipo
            ld   c,b
            dec  c                  ; C = quien (0 o 1)
            cp   1
            jr   z,ga_play
            cp   2
            jr   z,ga_pass
            push bc                 ; cambiar: las fichas ya estan en WB
            call sb_exchange
            pop  bc
            ld   a,c
            ld   hl,sc_t_swap
            jp   sbu_note_text
ga_play:    push bc
            call sb_apply
            pop  bc
            ld   a,c
            jp   sbu_note_last
ga_pass:    push bc
            call sb_pass
            pop  bc
            ld   a,c
            ld   hl,sc_t_pass
            jp   sbu_note_text

sc_t_pass:  defm "PASS",0
sc_t_swap:  defm "SWAP",0

; HL = destino: el estado entero
g_save:     ex   de,hl
            ld   hl,SB_BOARD
            ld   bc,SB_STATE_LEN
            ldir
            ret

; HL = origen: el estado entero (de un trozo del rival o del fichero guardado). Rehace lo derivado: las tablas del idioma,
; el diccionario, la sala de esta partida y la pantalla
g_load:     xor  a
            ld   (SC_PRE),a
            ld   de,SB_BOARD
            ld   bc,SB_STATE_LEN
            ldir
            ld   hl,NB_CODE81       ; el numero de partida es de esta sala
            ld   de,SC_RC
            ld   bc,10
            ldir
            call sb_setlang
            call sbu_reset
            call sc_me
            call sb_open_dict
            ld   (SC_DICT),a
            ret

; HL = resumen de 16 bits del estado (el mismo mezclado que game_hash, sin copiar)
g_hash:     ld   hl,SB_BOARD
            ld   bc,SB_STATE_LEN
            ld   de,5A5Ah
gh_l_s:       ld   a,(hl)
            inc  hl
            xor  e
            ld   e,a
            sla  e
            rl   d
            jr   nc,gh_1
            set  0,e
gh_1:       sla  e
            rl   d
            jr   nc,gh_2
            set  0,e
gh_2:       sla  e
            rl   d
            jr   nc,gh_3
            set  0,e
gh_3:       dec  bc
            ld   a,b
            or   c
            jr   nz,gh_l_s
            ex   de,hl
            ret

; WB, tras sbi_build (A = 1 jugar, 2 pasar, 3 cambiar) -> HL = la jugada en el formato de game.inc (en MV_OUT)
sc_mkmove:  cp   1
            jr   z,mk_play
            cp   2
            jr   z,mk_pass
            ld   a,(WB+3)           ; cambiar: [3 + n][3][n][fichas]
            ld   c,a
            add  a,3
            ld   (MV_OUT),a
            ld   a,3
            ld   (MV_OUT+1),a
            ld   b,0
            inc  c                  ; n y las fichas
            ld   hl,WB+3
            ld   de,MV_OUT+2
            ldir
            ld   hl,MV_OUT
            ret
mk_pass:    ld   a,2
            ld   (MV_OUT),a
            ld   (MV_OUT+1),a
            ld   hl,MV_OUT
            ret
mk_play:    ld   a,(WB+3)           ; jugar: [6 + n][1][x][y][sentido][n][fichas]
            ld   c,a
            add  a,6
            ld   (MV_OUT),a
            ld   a,1
            ld   (MV_OUT+1),a
            ld   a,c
            add  a,4
            ld   c,a
            ld   b,0
            ld   hl,WB
            ld   de,MV_OUT+2
            ldir
            ld   hl,MV_OUT
            ret
