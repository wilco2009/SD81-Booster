; =====================================================================
;  CHESS.INC - las reglas del ajedrez para game.inc: jugadas legales (enroque,
;  al paso, promocion), jaque mate, ahogado, tablas por material insuficiente y
;  por la regla de las 50 jugadas. NO hay triple repeticion.
;
;  El que lo incluye define antes (despues de game.inc / net.inc):
;      CH_BASE      equ ...    ; ALINEADO A 256: el tablero 0x88 (128 bytes), un
;                              ; estado para deshacer, las variables y la lista de
;                              ; jugadas (en total unos 1,3 KB desde CH_BASE)
;      G_STATE_LEN  equ 67     ; 64 casillas + enroques + al paso + reloj de medias jugadas
;      G_MOVE_LEN   equ 2
;
;  Una jugada son dos bytes: [origen | (corona << 6)] [destino], con casillas de 0 a 63
;  (a1 = 0, b1 = 1 ... h1 = 7, a2 = 8 ... h8 = 63) y la corona 0 dama, 1 torre, 2 alfil,
;  3 caballo (solo vale al coronar; para el resto de jugadas, 0).
;  Jugador 1 = blancas (mueven con jugada par), jugador 2 = negras.
;
;  Piezas: 0 vacia; 1 peon, 2 caballo, 3 alfil, 4 torre, 5 dama, 6 rey; +8 las negras.
;  Estado externo (g_save / g_load): las 64 casillas (rank 1 primero), los enroques
;  (1 corto blanco, 2 largo blanco, 4 corto negro, 8 largo negro), la columna del al paso
;  (0FFh ninguna) y el reloj de medias jugadas.
; =====================================================================

CH_BOARD    equ CH_BASE         ; (128) tablero 0x88: casilla = fila * 16 + columna
CH_CASTLE   equ CH_BASE+128     ; derechos de enroque
CH_EP       equ CH_BASE+129     ; columna del peon que acaba de avanzar dos, o 0FFh
CH_HALF     equ CH_BASE+130     ; medias jugadas sin captura ni peon
CH_SAVE     equ CH_BASE+132     ; (131) copia del estado, para deshacer
CH_VARS     equ CH_BASE+272
CH_CB       equ CH_VARS+0       ; color (0 blancas / 8 negras) que genera
CH_N        equ CH_VARS+1       ; jugadas en la lista
CH_PTR      equ CH_VARS+2       ; (2) donde se escribe la siguiente
CH_F        equ CH_VARS+4       ; casilla de origen
CH_PR       equ CH_VARS+5       ; corona de la jugada que se anade
CH_FWD      equ CH_VARS+6       ; avance del peon (+16 / -16)
CH_TP       equ CH_VARS+7       ; (2) puntero a una tabla de direcciones
CH_D        equ CH_VARS+9
CH_B0       equ CH_VARS+10
CH_RK       equ CH_VARS+11
CH_EN       equ CH_VARS+12
CH_RD       equ CH_VARS+13      ; (2) lectura al filtrar
CH_WR       equ CH_VARS+15      ; (2) escritura al filtrar
CH_CNT      equ CH_VARS+17
DM_F        equ CH_VARS+18
DM_T        equ CH_VARS+19
DM_PR       equ CH_VARS+20
DM_P        equ CH_VARS+21
DM_CAP      equ CH_VARS+22
CH_MV       equ CH_VARS+23      ; (3) una jugada en formato interno: origen, destino, corona
CH_SIDE     equ CH_VARS+26
T_SQ        equ CH_VARS+27
T_CB        equ CH_VARS+28
T_W         equ CH_VARS+29
T_Q         equ CH_VARS+30
T_N         equ CH_VARS+31
T_PTR       equ CH_VARS+32      ; (2)
CH_LIST     equ CH_BASE+512     ; (3 bytes por jugada, hasta 256): origen, destino, corona (0x88)

; --- direcciones (en el tablero 0x88: +1 = columna siguiente, +16 = fila siguiente) ---
d_ortho:    defb 1,0FFh,16,0F0h
d_diag:     defb 15,17,0F1h,0EFh
d_knight:   defb 14,18,31,33,0F2h,0EEh,0E1h,0DFh,0
d_king:     defb 1,0FFh,16,0F0h,15,17,0F1h,0EFh,0
pd_white:   defb 0F1h,0EFh,0    ; un peon blanco ataca desde (casilla - 15) y (casilla - 17)
pd_black:   defb 15,17,0
prom_tab:   defb 5,4,3,2        ; corona 0 dama, 1 torre, 2 alfil, 3 caballo
rt_tab:     defb 00h,2, 07h,1, 04h,3, 70h,8, 77h,4, 74h,12, 0FFh
init_w:     defb 4,2,3,5,6,3,2,4
init_b:     defb 12,10,11,13,14,11,10,12

; =====================================================================
;  Utilidades del tablero
; =====================================================================
; A = casilla, B = delta -> A = la pieza en casilla + delta, o 0FFh si cae fuera.
; Solo destruye A, H y L
ch_at:      add  a,b
            ld   l,a
            and  88h
            jr   nz,cha_off
            ld   h,CH_BOARD/256
            ld   a,(hl)
            ret
cha_off:    ld   a,0FFh
            ret

; HL = tabla de deltas (acaba en 0), A = la pieza que se busca: CF = 1 si esta en
; alguna de (T_SQ + delta)
ca_steps:   ld   (T_W),a
cs_lp:      ld   b,(hl)
            ld   a,b
            or   a
            ret  z
            inc  hl
            push hl
            ld   a,(T_SQ)
            call ch_at
            pop  hl
            ld   c,a
            ld   a,(T_W)
            cp   c
            jr   nz,cs_lp
            scf
            ret

; HL = tabla de 4 direcciones, A = torre o alfil del color (T_CB): CF = 1 si algo asi (o
; una dama) ve la casilla T_SQ
ca_rays:    ld   (T_W),a
            ld   a,(T_CB)
            or   5
            ld   (T_Q),a
            ld   a,4
            ld   (T_N),a
            ld   (T_PTR),hl
cr_dir:     ld   hl,(T_PTR)
            ld   b,(hl)
            inc  hl
            ld   (T_PTR),hl
            ld   a,(T_SQ)
            ld   c,a
cr_walk:    ld   a,c
            call ch_at
            ld   d,a
            ld   a,c
            add  a,b
            ld   c,a
            ld   a,d
            cp   0FFh
            jr   z,cr_next
            or   a
            jr   z,cr_walk
            ld   hl,T_W
            cp   (hl)
            jr   z,cr_hit
            ld   hl,T_Q
            cp   (hl)
            jr   z,cr_hit
            jr   cr_next
cr_hit:     scf
            ret
cr_next:    ld   hl,T_N
            dec  (hl)
            jr   nz,cr_dir
            or   a
            ret

; A = casilla, C = color (0 / 8) del atacante: CF = 1 si ese color ataca la casilla
ch_attacked: ld  (T_SQ),a
            ld   a,c
            ld   (T_CB),a
            ld   hl,pd_white
            or   a
            jr   z,ca_1
            ld   hl,pd_black
ca_1:       ld   a,(T_CB)
            or   1
            call ca_steps
            ret  c
            ld   hl,d_knight
            ld   a,(T_CB)
            or   2
            call ca_steps
            ret  c
            ld   hl,d_king
            ld   a,(T_CB)
            or   6
            call ca_steps
            ret  c
            ld   hl,d_ortho
            ld   a,(T_CB)
            or   4
            call ca_rays
            ret  c
            ld   hl,d_diag
            ld   a,(T_CB)
            or   3
            jp   ca_rays

; A = color (0 / 8) -> A = la casilla de su rey, o 0FFh
ch_find_king: or 6
            ld   c,a
            ld   l,0
            ld   h,CH_BOARD/256
fk_lp:      ld   a,l
            and  88h
            jr   nz,fk_skip
            ld   a,(hl)
            cp   c
            jr   z,fk_found
fk_skip:    inc  l
            bit  7,l
            jr   z,fk_lp
            ld   a,0FFh
            ret
fk_found:   ld   a,l
            ret

; =====================================================================
;  Generacion de jugadas (casi legales) y filtro de legalidad
; =====================================================================
; A = origen (CH_F), A = destino -> la anade a la lista con la corona CH_PR
ch_add:     ld   e,a
            ld   hl,(CH_PTR)
            ld   a,(CH_F)
            ld   (hl),a
            inc  hl
            ld   (hl),e
            inc  hl
            ld   a,(CH_PR)
            ld   (hl),a
            inc  hl
            ld   (CH_PTR),hl
            ld   hl,CH_N
            inc  (hl)
            ret

; A = destino de un peon: si corona, anade las cuatro
ch_add_pawn: ld  c,a
            ld   a,(CH_CB)
            or   a
            ld   a,c
            jr   nz,apb
            and  0F0h
            cp   70h
            jr   nz,ap_one
            jr   ap_promo
apb:        and  0F0h
            jr   nz,ap_one
ap_promo:   xor  a
            ld   (CH_PR),a
ap_pl:      ld   a,c
            call ch_add
            ld   hl,CH_PR
            inc  (hl)
            ld   a,(hl)
            cp   4
            jr   c,ap_pl
            xor  a
            ld   (CH_PR),a
            ret
ap_one:     ld   a,c
            jp   ch_add

; A = color (0 blancas / 8 negras): llena CH_LIST con las jugadas posibles (sin mirar
; si dejan al rey en jaque) y CH_N con cuantas
ch_gen_pseudo: ld (CH_CB),a
            xor  a
            ld   (CH_N),a
            ld   (CH_PR),a
            ld   hl,CH_LIST
            ld   (CH_PTR),hl
            ld   (CH_F),a
cg_sq:      ld   a,(CH_F)
            and  88h
            jr   nz,cg_next
            ld   a,(CH_F)
            ld   l,a
            ld   h,CH_BOARD/256
            ld   a,(hl)
            or   a
            jr   z,cg_next
            ld   b,a
            and  8
            ld   c,a
            ld   a,(CH_CB)
            cp   c
            jr   nz,cg_next
            ld   a,b
            and  7
            cp   1
            jp   z,gen_pawn
            cp   2
            jr   z,gen_knight
            cp   3
            jr   z,gen_bishop
            cp   4
            jr   z,gen_rook
            cp   5
            jr   z,gen_queen
            ld   hl,d_king          ; rey
            call gen_steps
            call gen_castle
cg_next:    ld   hl,CH_F
            inc  (hl)
            bit  7,(hl)
            jr   z,cg_sq
            ret
gen_knight: ld   hl,d_knight
            call gen_steps
            jr   cg_next
gen_bishop: ld   hl,d_diag
            call gen_slide
            jr   cg_next
gen_rook:   ld   hl,d_ortho
            call gen_slide
            jr   cg_next
gen_queen:  ld   hl,d_ortho
            call gen_slide
            ld   hl,d_diag
            call gen_slide
            jr   cg_next

; HL = tabla de deltas (acaba en 0): un paso en cada una (caballo y rey)
gen_steps:  ld   (CH_TP),hl
gs_lp:      ld   hl,(CH_TP)
            ld   b,(hl)
            ld   a,b
            or   a
            ret  z
            inc  hl
            ld   (CH_TP),hl
            ld   a,(CH_F)
            call ch_at
            cp   0FFh
            jr   z,gs_lp
            or   a
            jr   z,gs_add
            and  8
            ld   c,a
            ld   a,(CH_CB)
            cp   c
            jr   z,gs_lp            ; una propia
gs_add:     ld   a,(CH_F)
            add  a,b
            call ch_add
            jr   gs_lp

; HL = tabla de 4 direcciones: camina hasta chocar (alfil, torre y dama)
gen_slide:  ld   (CH_TP),hl
            ld   a,4
            ld   (CH_D),a
gl_dir:     ld   hl,(CH_TP)
            ld   b,(hl)
            inc  hl
            ld   (CH_TP),hl
            ld   a,(CH_F)
            ld   c,a
gl_walk:    ld   a,c
            call ch_at
            ld   d,a
            ld   a,c
            add  a,b
            ld   c,a
            ld   a,d
            cp   0FFh
            jr   z,gl_next
            or   a
            jr   z,gl_empty
            and  8
            ld   d,a
            ld   a,(CH_CB)
            cp   d
            jr   z,gl_next          ; propia: bloquea
            ld   a,c
            call ch_add             ; enemiga: la captura y para
            jr   gl_next
gl_empty:   ld   a,c
            call ch_add
            jr   gl_walk
gl_next:    ld   hl,CH_D
            dec  (hl)
            jr   nz,gl_dir
            ret

; el peon de CH_F
gen_pawn:   ld   a,(CH_CB)
            or   a
            ld   b,16
            jr   z,gpw1
            ld   b,0F0h
gpw1:       ld   a,b
            ld   (CH_FWD),a
            ld   a,(CH_F)           ; un paso
            call ch_at
            or   a
            jr   nz,gpw_cap         ; ocupada o fuera
            ld   a,(CH_F)
            add  a,b
            call ch_add_pawn
            ld   a,(CH_FWD)
            ld   b,a
            ld   a,(CH_F)           ; dos pasos desde la fila de salida
            ld   e,a
            ld   a,(CH_CB)
            or   a
            ld   a,e
            jr   nz,gpw_blk
            and  0F0h
            cp   10h
            jr   nz,gpw_cap
            jr   gpw_dbl
gpw_blk:    and  0F0h
            cp   60h
            jr   nz,gpw_cap
gpw_dbl:    ld   a,(CH_F)
            add  a,b
            call ch_at
            or   a
            jr   nz,gpw_cap
            ld   a,(CH_F)
            add  a,b
            add  a,b
            call ch_add
gpw_cap:    ld   a,(CH_FWD)         ; capturas
            inc  a
            ld   b,a
            call gp_pcap
            ld   a,(CH_FWD)
            dec  a
            ld   b,a
            call gp_pcap
            jp   cg_next

; B = delta en diagonal del peon: captura o al paso
gp_pcap:    ld   a,(CH_F)
            call ch_at
            cp   0FFh
            ret  z
            or   a
            jr   z,gpc_ep
            and  8
            ld   d,a
            ld   a,(CH_CB)
            cp   d
            ret  z                  ; una propia
            ld   a,(CH_F)
            add  a,b
            jp   ch_add_pawn
gpc_ep:     ld   a,(CH_EP)
            cp   0FFh
            ret  z
            ld   c,a
            ld   a,(CH_CB)
            or   a
            ld   a,50h              ; las blancas capturan al paso en la fila 6 (0x5x)
            jr   z,gpe1
            ld   a,20h              ; las negras, en la fila 3 (0x2x)
gpe1:       or   c
            ld   c,a
            ld   a,(CH_F)
            add  a,b
            cp   c
            ret  nz
            jp   ch_add

; enroque del rey de CH_F (solo si esta en su casilla de salida)
gen_castle: ld   a,(CH_CB)
            or   a
            ld   b,0
            ld   c,1
            ld   d,8
            jr   z,gc_set
            ld   b,70h
            ld   c,4
            ld   d,0
gc_set:     ld   a,b
            add  a,4
            ld   e,a
            ld   a,(CH_F)
            cp   e
            ret  nz
            ld   a,b
            ld   (CH_B0),a
            ld   a,c
            ld   (CH_RK),a
            ld   a,d
            ld   (CH_EN),a
            ld   a,(CH_CASTLE)      ; corto
            ld   hl,CH_RK
            and  (hl)
            jr   z,gc_q
            ld   a,(CH_B0)
            add  a,5
            ld   l,a
            ld   h,CH_BOARD/256
            ld   a,(hl)
            inc  l
            or   (hl)
            jr   nz,gc_q            ; f y g vacias
            ld   a,(CH_EN)
            ld   c,a
            ld   a,(CH_B0)
            add  a,4
            call ch_attacked
            jr   c,gc_q             ; el rey en jaque
            ld   a,(CH_EN)
            ld   c,a
            ld   a,(CH_B0)
            add  a,5
            call ch_attacked
            jr   c,gc_q             ; pasa por una casilla atacada
            ld   a,(CH_B0)
            add  a,6
            call ch_add
gc_q:       ld   a,(CH_RK)          ; largo
            add  a,a
            ld   c,a
            ld   a,(CH_CASTLE)
            and  c
            ret  z
            ld   a,(CH_B0)
            ld   l,a
            ld   h,CH_BOARD/256
            inc  l
            ld   a,(hl)
            inc  l
            or   (hl)
            inc  l
            or   (hl)
            ret  nz                 ; b, c y d vacias
            ld   a,(CH_EN)
            ld   c,a
            ld   a,(CH_B0)
            add  a,4
            call ch_attacked
            ret  c
            ld   a,(CH_EN)
            ld   c,a
            ld   a,(CH_B0)
            add  a,3
            call ch_attacked
            ret  c
            ld   a,(CH_B0)
            add  a,2
            jp   ch_add

; A = color: la lista de jugadas legales de ese color
ch_gen_legal: call ch_gen_pseudo

; deja en CH_LIST solo las jugadas que no dejan al rey propio en jaque
ch_filter:  ld   hl,CH_LIST
            ld   (CH_RD),hl
            ld   (CH_WR),hl
            ld   a,(CH_N)
            ld   (CH_CNT),a
            xor  a
            ld   (CH_N),a
cx_lp:      ld   a,(CH_CNT)
            or   a
            ret  z
            dec  a
            ld   (CH_CNT),a
            ld   hl,CH_BOARD        ; guarda el estado
            ld   de,CH_SAVE
            ld   bc,131
            ldir
            ld   hl,(CH_RD)
            call ch_do_move
            ld   a,(CH_CB)
            call ch_find_king
            ld   e,a
            ld   a,(CH_CB)
            xor  8
            ld   c,a
            ld   a,e
            call ch_attacked        ; CF = el rey queda atacado
            ld   hl,CH_SAVE         ; restaura (LDIR no toca el acarreo)
            ld   de,CH_BOARD
            ld   bc,131
            ldir
            jr   c,cx_skip
            ld   hl,(CH_RD)
            ld   de,(CH_WR)
            ld   bc,3
            ldir
            ld   (CH_WR),de
            ld   hl,CH_N
            inc  (hl)
cx_skip:    ld   hl,(CH_RD)
            inc  hl
            inc  hl
            inc  hl
            ld   (CH_RD),hl
            jr   cx_lp

; =====================================================================
;  Hacer una jugada (la usan g_apply y el filtro)
; =====================================================================
; HL = origen, destino, corona (0x88): la hace con sus efectos
ch_do_move: ld   a,(hl)
            ld   (DM_F),a
            inc  hl
            ld   a,(hl)
            ld   (DM_T),a
            inc  hl
            ld   a,(hl)
            ld   (DM_PR),a
            ld   a,(DM_F)
            ld   l,a
            ld   h,CH_BOARD/256
            ld   a,(hl)
            ld   (DM_P),a
            ld   (hl),0             ; fuera del origen
            ld   a,(DM_T)
            ld   l,a
            ld   a,(hl)
            ld   (DM_CAP),a         ; lo que hay en el destino
            ld   a,(CH_HALF)
            inc  a
            ld   (CH_HALF),a
            ld   a,(DM_CAP)
            or   a
            jr   z,dm1
            xor  a
            ld   (CH_HALF),a        ; captura: reloj a cero
dm1:        ld   a,0FFh
            ld   (CH_EP),a
            ld   a,(DM_P)
            and  7
            cp   1
            jr   nz,dm_notpawn
            xor  a
            ld   (CH_HALF),a        ; peon: reloj a cero
            ld   a,(DM_CAP)
            or   a
            jr   nz,dm_pnc
            ld   a,(DM_F)           ; en diagonal sin captura: al paso
            ld   b,a
            ld   a,(DM_T)
            xor  b
            and  0Fh
            jr   z,dm_pnc
            ld   a,(DM_F)           ; el peon capturado: fila del origen, columna del destino
            and  0F0h
            ld   b,a
            ld   a,(DM_T)
            and  0Fh
            or   b
            ld   l,a
            ld   h,CH_BOARD/256
            ld   (hl),0
dm_pnc:     ld   a,(DM_T)           ; corona
            and  0F0h
            jr   z,dm_prom
            cp   70h
            jr   nz,dm_dblchk
dm_prom:    ld   a,(DM_PR)
            and  3
            ld   e,a
            ld   d,0
            ld   hl,prom_tab
            add  hl,de
            ld   b,(hl)
            ld   a,(DM_P)
            and  8
            or   b
            ld   (DM_P),a
dm_dblchk:  ld   a,(DM_F)           ; avanza dos: al paso para el rival
            ld   b,a
            ld   a,(DM_T)
            sub  b
            cp   32
            jr   z,dm_dbl
            cp   0E0h
            jr   nz,dm_rights
dm_dbl:     ld   a,(DM_F)
            and  7
            ld   (CH_EP),a
            jr   dm_rights
dm_notpawn: cp   6                  ; enroque: el rey se mueve dos columnas
            jr   nz,dm_rights
            ld   a,(DM_T)
            ld   b,a
            ld   a,(DM_F)
            ld   c,a
            ld   a,b
            sub  c
            cp   2
            jr   z,dm_ck
            cp   0FEh
            jr   nz,dm_rights
            ld   a,(DM_F)           ; largo: la torre de F-4 pasa a F-1
            sub  4
            ld   l,a
            ld   h,CH_BOARD/256
            ld   b,(hl)
            ld   (hl),0
            ld   a,(DM_F)
            dec  a
            ld   l,a
            ld   (hl),b
            jr   dm_rights
dm_ck:      ld   a,(DM_F)           ; corto: la torre de F+3 pasa a F+1
            add  a,3
            ld   l,a
            ld   h,CH_BOARD/256
            ld   b,(hl)
            ld   (hl),0
            ld   a,(DM_F)
            inc  a
            ld   l,a
            ld   (hl),b
dm_rights:  ld   a,(DM_F)           ; los enroques que pierde (el rey o una torre se mueve, o se captura una torre)
            call ch_rights_clear
            ld   a,(DM_T)
            call ch_rights_clear
            ld   a,(DM_T)
            ld   l,a
            ld   h,CH_BOARD/256
            ld   a,(DM_P)
            ld   (hl),a
            ret

; A = casilla: quita de CH_CASTLE los derechos que dependen de ella
ch_rights_clear: ld e,a
            ld   hl,rt_tab
rcl_lp:     ld   a,(hl)
            cp   0FFh
            ret  z
            inc  hl
            cp   e
            ld   a,(hl)
            inc  hl
            jr   nz,rcl_lp
            cpl
            ld   b,a
            ld   a,(CH_CASTLE)
            and  b
            ld   (CH_CASTLE),a
            ret

; =====================================================================
;  Las seis rutinas de game.inc
; =====================================================================
; posicion inicial
g_reset:    ld   hl,CH_BOARD
            ld   de,CH_BOARD+1
            ld   bc,127
            ld   (hl),0
            ldir
            ld   hl,init_w
            ld   de,CH_BOARD
            ld   bc,8
            ldir
            ld   hl,CH_BOARD+16
            ld   b,8
            ld   a,1
gr_pw:      ld   (hl),a
            inc  l
            djnz gr_pw
            ld   hl,CH_BOARD+96
            ld   b,8
            ld   a,9
gr_pb:      ld   (hl),a
            inc  l
            djnz gr_pb
            ld   hl,init_b
            ld   de,CH_BOARD+112
            ld   bc,8
            ldir
            ld   a,0Fh
            ld   (CH_CASTLE),a
            ld   a,0FFh
            ld   (CH_EP),a
            xor  a
            ld   (CH_HALF),a
            ret

; A = casilla 0-63 -> A = casilla 0x88
ch_sq88:    ld   b,a
            and  38h
            add  a,a
            ld   c,a
            ld   a,b
            and  7
            or   c
            ret

; HL = la jugada externa (2 bytes) -> CH_MV (origen, destino, corona) en 0x88
ch_ext_to_int: ld a,(hl)
            ld   d,a
            and  3Fh
            call ch_sq88
            ld   (CH_MV),a
            ld   a,d
            rlca
            rlca
            and  3
            ld   (CH_MV+2),a
            inc  hl
            ld   a,(hl)
            and  3Fh
            call ch_sq88
            ld   (CH_MV+1),a
            ret

; HL = la jugada, B = jugador (1 blancas, 2 negras): CF = 1 si no es legal
g_valid:    ld   a,b                ; ch_ext_to_int destruye B
            ld   (CH_SIDE),a
            call ch_ext_to_int
            ld   a,(CH_SIDE)
            dec  a
            add  a,a
            add  a,a
            add  a,a                ; 0 blancas / 8 negras
            call ch_gen_legal
            ld   hl,CH_LIST
            ld   a,(CH_N)
            or   a
            jr   z,gv_bad
            ld   b,a
gv_s:       ld   a,(CH_MV)
            cp   (hl)
            jr   nz,gv_n
            push hl
            inc  hl
            ld   a,(CH_MV+1)
            cp   (hl)
            jr   nz,gv_n1
            inc  hl
            ld   a,(CH_MV+2)
            cp   (hl)
            jr   nz,gv_n1
            pop  hl
            or   a
            ret
gv_n1:      pop  hl
gv_n:       inc  hl
            inc  hl
            inc  hl
            djnz gv_s
gv_bad:     scf
            ret

; HL = la jugada (legal), B = jugador: la hace
g_apply:    call ch_ext_to_int
            ld   hl,CH_MV
            jp   ch_do_move

; A = 0 sigue, 1 o 2 gana ese jugador (mate), 3 tablas (ahogado, 50 jugadas o material
; insuficiente). Le toca a las blancas con GM_PLY par
g_winner:   ld   a,(GM_PLY)
            and  1
            add  a,a
            add  a,a
            add  a,a
            ld   (CH_SIDE),a
            call ch_gen_legal
            ld   a,(CH_N)
            or   a
            jr   nz,gw_has
            ld   a,(CH_SIDE)        ; sin jugadas: mate si esta en jaque, si no ahogado
            call ch_find_king
            ld   e,a
            ld   a,(CH_SIDE)
            xor  8
            ld   c,a
            ld   a,e
            call ch_attacked
            jr   c,gw_mate
            ld   a,3
            ret
gw_mate:    ld   a,(CH_SIDE)
            or   a
            ld   a,2                ; le toca a las blancas y esta mate: ganan las negras (2)
            ret  z
            ld   a,1
            ret
gw_has:     ld   a,(CH_HALF)
            cp   100
            jr   c,gw_mat
            ld   a,3
            ret
gw_mat:     ld   l,0                ; material insuficiente: solo reyes, o un caballo o un alfil
            ld   h,CH_BOARD/256
            ld   b,0
            ld   c,0
gi_lp:      ld   a,l
            and  88h
            jr   nz,gi_n
            ld   a,(hl)
            or   a
            jr   z,gi_n
            and  7
            cp   6
            jr   z,gi_n
            ld   c,a
            inc  b
gi_n:       inc  l
            bit  7,l
            jr   z,gi_lp
            ld   a,b
            or   a
            jr   z,gi_draw
            cp   1
            jr   nz,gi_no
            ld   a,c
            cp   2
            jr   z,gi_draw
            cp   3
            jr   z,gi_draw
gi_no:      xor  a
            ret
gi_draw:    ld   a,3
            ret

; A = color (0 blancas / 8 negras): CF = 1 si su rey esta en jaque
ch_in_check: ld  d,a
            call ch_find_king
            ld   e,a
            ld   a,d
            xor  8
            ld   c,a
            ld   a,e
            jp   ch_attacked

; HL = destino: 64 casillas, enroques, al paso y reloj (67 bytes)
g_save:     ex   de,hl
            ld   hl,CH_BOARD
            ld   b,8
gsv_r:      push bc
            ld   bc,8
            ldir
            pop  bc
            ld   a,l
            add  a,8
            ld   l,a
            djnz gsv_r
            ld   hl,CH_CASTLE
            ld   bc,3
            ldir
            ret

; HL = origen: lo mismo que g_save
g_load:     ld   de,CH_BOARD
            ld   b,8
gld_r:      push bc
            ld   bc,8
            ldir
            pop  bc
            ld   a,e
            add  a,8
            ld   e,a
            djnz gld_r
            ld   de,CH_CASTLE
            ld   bc,3
            ldir
            ret
