; =====================================================================
;  CONNECT4.INC - las reglas del 4 en raya (7 columnas, 6 filas) para game.inc.
;  Una jugada es la columna (0-6). El estado son las 42 casillas, fila a fila
;  desde abajo: casilla = columna + 7 * fila; 0 vacia, 1 y 2 el jugador.
;
;  El que lo incluye define antes  C4_BASE equ ...  (unos 60 bytes de RAM)
;  y  G_STATE_LEN equ 42  y  G_MOVE_LEN equ 1.
; =====================================================================

G_BOARD     equ C4_BASE+0       ; (42)
G_HEIGHT    equ C4_BASE+42      ; (7) fichas por columna
C4_X        equ C4_BASE+50
C4_Y        equ C4_BASE+51
C4_P        equ C4_BASE+52
C4_D        equ C4_BASE+53
C4_DX       equ C4_BASE+54
C4_DY       equ C4_BASE+55
C4_K        equ C4_BASE+56

g_reset:    ld   hl,G_BOARD
            ld   de,G_BOARD+1
            ld   bc,48
            ld   (hl),0
            ldir
            ret

; HL = la jugada (la columna, 1 byte): CF = 1 si no se puede (fuera del tablero o columna llena)
g_valid:    ld   a,(hl)
            cp   7
            jr   nc,gv_bad
            ld   e,a
            ld   d,0
            ld   hl,G_HEIGHT
            add  hl,de
            ld   a,(hl)
            cp   6
            jr   nc,gv_bad
            or   a
            ret
gv_bad:     scf
            ret

; HL = la columna, B = jugador: la ficha cae a la primera casilla libre
g_apply:    ld   a,(hl)
            ld   e,a
            ld   d,0
            ld   hl,G_HEIGHT
            add  hl,de
            ld   c,(hl)
            inc  (hl)
            ld   a,c
            add  a,a
            add  a,a
            add  a,a
            sub  c                  ; 7 * fila
            add  a,e                ; + columna
            ld   e,a
            ld   d,0
            ld   hl,G_BOARD
            add  hl,de
            ld   (hl),b
            ret

; B = columna, C = fila -> A = la casilla, o 0FFh si esta fuera. Conserva B y C
c4_cell:    ld   a,b
            cp   7
            jr   nc,c4c_out
            ld   a,c
            cp   6
            jr   nc,c4c_out
            add  a,a
            add  a,a
            add  a,a
            sub  c
            add  a,b
            ld   e,a
            ld   d,0
            ld   hl,G_BOARD
            add  hl,de
            ld   a,(hl)
            ret
c4c_out:    ld   a,0FFh
            ret

; A = 0 sigue, 1 o 2 gana ese jugador (cuatro seguidas), 3 tablas (tablero lleno)
g_winner:   xor  a
            ld   (C4_Y),a
gw_y:       xor  a
            ld   (C4_X),a
gw_x:       ld   a,(C4_X)
            ld   b,a
            ld   a,(C4_Y)
            ld   c,a
            call c4_cell
            or   a
            jr   z,gw_nx
            ld   (C4_P),a
            xor  a
            ld   (C4_D),a
gw_d:       ld   a,(C4_D)
            add  a,a
            ld   e,a
            ld   d,0
            ld   hl,c4_dirs
            add  hl,de
            ld   a,(hl)
            ld   (C4_DX),a
            inc  hl
            ld   a,(hl)
            ld   (C4_DY),a
            ld   a,(C4_X)
            ld   b,a
            ld   a,(C4_Y)
            ld   c,a
            ld   a,3
            ld   (C4_K),a
gw_k:       ld   a,(C4_DX)
            add  a,b
            ld   b,a
            ld   a,(C4_DY)
            add  a,c
            ld   c,a
            call c4_cell
            ld   hl,C4_P
            cp   (hl)
            jr   nz,gw_nd
            ld   hl,C4_K
            dec  (hl)
            jr   nz,gw_k
            ld   a,(C4_P)
            ret
gw_nd:      ld   hl,C4_D
            inc  (hl)
            ld   a,(hl)
            cp   4
            jr   c,gw_d
gw_nx:      ld   hl,C4_X
            inc  (hl)
            ld   a,(hl)
            cp   7
            jr   c,gw_x
            ld   hl,C4_Y
            inc  (hl)
            ld   a,(hl)
            cp   6
            jr   c,gw_y
            ld   hl,G_BOARD
            ld   b,42
gw_f:       ld   a,(hl)
            or   a
            jr   z,gw_none
            inc  hl
            djnz gw_f
            ld   a,3
            ret
gw_none:    xor  a
            ret

; direcciones (dx, dy): derecha, arriba, diagonal arriba-derecha, diagonal abajo-derecha
c4_dirs:    defb 1,0, 0,1, 1,1, 1,0FFh

; HL = destino: las 42 casillas
g_save:     ex   de,hl
            ld   hl,G_BOARD
            ld   bc,42
            ldir
            ret

; HL = origen: las 42 casillas; rehace las alturas
g_load:     ld   de,G_BOARD
            ld   bc,42
            ldir
            ld   hl,G_HEIGHT
            ld   b,7
gl_z:       ld   (hl),0
            inc  hl
            djnz gl_z
            xor  a
            ld   (C4_Y),a
gl_y:       xor  a
            ld   (C4_X),a
gl_x:       ld   a,(C4_X)
            ld   b,a
            ld   a,(C4_Y)
            ld   c,a
            call c4_cell
            or   a
            jr   z,gl_nx
            ld   a,(C4_X)
            ld   e,a
            ld   d,0
            ld   hl,G_HEIGHT
            add  hl,de
            ld   a,(C4_Y)
            inc  a
            ld   (hl),a
gl_nx:      ld   hl,C4_X
            inc  (hl)
            ld   a,(hl)
            cp   7
            jr   c,gl_x
            ld   hl,C4_Y
            inc  (hl)
            ld   a,(hl)
            cp   6
            jr   c,gl_y
            ret
