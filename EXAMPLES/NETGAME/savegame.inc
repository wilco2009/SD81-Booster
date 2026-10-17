; =====================================================================
;  SAVEGAME.INC - guardar la partida en la SD y seguirla otro dia (juego asincrono).
;
;  Los mensajes del broker solo guardan la ULTIMA jugada de cada jugador, y el tablero vive en la
;  RAM de cada ordenador: sin esto, apagar el ZX81 perdia la partida. Aqui cada juego guarda su
;  estado en un fichero de la carpeta actual de la SD, la del juego (CONNECT4.SAV, CHESS.SAV), cada vez que cambia, y el menu
;  ofrece "3 = CONTINUE SAVED GAME": lee el fichero, vuelve a entrar en la MISMA sala y en la misma
;  ranura, y si la ultima jugada fue mia la manda otra vez (por si el broker la olvido).
;  No hace falta que los dos esten conectados a la vez: cada uno solo necesita su estado guardado y
;  que el broker conserve el ultimo mensaje del rival hasta que este lo recoja.
;
;  El juego que lo incluye define antes:
;      SV_BASE      equ ...   ; RAM de trabajo (unos 110 bytes)
;      SV_ID        equ ...   ; un byte distinto por juego (1 = 4 en raya, 2 = ajedrez)
;      SV_NAMELEN   equ ...   ; y la etiqueta  sv_name  con el nombre (ASCII, SIN "/" delante: el MCU lo busca en la
;                             carpeta actual; asi cada carpeta de juego tiene su partida, y dos ZX81 que comparten SD no se pisan)
;  y G_STATE_LEN / G_CLOCK / g_save / g_load / game.inc / net.inc como siempre.
;
;  Formato del fichero (SV_LEN = 24 + G_STATE_LEN bytes):
;       0 SV_ID   1 version (1)   2 mi ranura (0 o 1)   3 control de tiempo (GM_TC)
;       4-13 el codigo de sala (codigos del ZX81)       14-15 GM_PLY   16 GM_OVER   17-18 GM_MOVE
;      19-22 los relojes (GM_REM)   23 GM_WHY   24.. el estado del juego (g_save)
;
;  Rutinas (destruyen todos los registros):
;      sv_maybe_save   guarda si GM_PLY o GM_OVER no son lo ultimo guardado (draw_all la llama)
;      sv_exists       CF = 1 si no hay fichero
;      sv_load         lee y valida el fichero: CF = 1 si no hay o no vale
;      sv_apply        pone en el juego lo leido (codigo de sala, ranura, jugadas, estado, relojes)
;  Usa los comandos de fichero del MCU: 71 crea o vacia y abre, 53 abre, 55 lee, 56 escribe, 57 cierra
;  (nombre en ASCII puro, ver COMMANDS.cpp, do_f_open). C es el reloj del handshake: no se toca.
; =====================================================================

SV_LEN      equ 24+G_STATE_LEN
SV_H        equ SV_BASE+0       ; el handle del fichero
SV_ST       equ SV_BASE+1       ; el estado de la ultima lectura o escritura
SV_LP       equ SV_BASE+2       ; (2) GM_PLY de lo ultimo guardado
SV_LO       equ SV_BASE+4       ; GM_OVER de lo ultimo guardado
SV_VAL      equ SV_BASE+5       ; 1 si SV_LP y SV_LO valen (0 al arrancar)
SV_CONT     equ SV_BASE+6       ; 1 si el menu ha encontrado un fichero
SV_BUF      equ SV_BASE+8       ; (SV_LEN) el fichero

; --- guardar si ha cambiado ---------------------------------------------------
sv_maybe_save:
            ld   a,(SV_VAL)
            or   a
            jr   z,sm_do
            ld   hl,(GM_PLY)
            ld   de,(SV_LP)
            or   a
            sbc  hl,de
            jr   nz,sm_do
            ld   a,(GM_OVER)
            ld   hl,SV_LO
            cp   (hl)
            ret  z
sm_do:      call sv_pack
            ld   a,71               ; crea o vacia, y abre
            call sv_open
            cp   0FFh
            ret  z                  ; sin SD: no pasa nada, se sigue jugando
            ld   (SV_H),a
            ld   hl,SV_BUF
            ld   de,SV_LEN
            call sv_wr
            ld   (SV_ST),a
            ld   a,(SV_H)
            call sv_close
            ld   a,(SV_ST)
            or   a
            ret  nz                 ; no se ha escrito entero: se volvera a intentar
            ld   hl,(GM_PLY)
            ld   (SV_LP),hl
            ld   a,(GM_OVER)
            ld   (SV_LO),a
            ld   a,1
            ld   (SV_VAL),a
            ret

; el estado actual, en SV_BUF
sv_pack:    ld   a,SV_ID
            ld   (SV_BUF),a
            ld   a,1
            ld   (SV_BUF+1),a
            ld   a,(NB_SLOT)
            ld   (SV_BUF+2),a
            xor  a
IF G_CLOCK
            ld   a,(GM_TC)
ENDIF
            ld   (SV_BUF+3),a
            ld   hl,NB_CODE81
            ld   de,SV_BUF+4
            ld   bc,10
            ldir
            ld   hl,(GM_PLY)
            ld   (SV_BUF+14),hl
            ld   a,(GM_OVER)
            ld   (SV_BUF+16),a
            ld   hl,(GM_MOVE)
            ld   (SV_BUF+17),hl
IF G_CLOCK
            ld   hl,GM_REM
            ld   de,SV_BUF+19
            ld   bc,4
            ldir
            ld   a,(GM_WHY)
            ld   (SV_BUF+23),a
ELSE
            ld   hl,SV_BUF+19
            ld   de,SV_BUF+20
            ld   bc,4
            ld   (hl),0
            ldir                    ; 19-23 a cero
ENDIF
            ld   hl,SV_BUF+24
            jp   g_save

; --- leer ---------------------------------------------------------------------
sv_exists:  ld   a,53               ; abrir
            call sv_open
            cp   0FFh
            jr   z,se_no
            call sv_close           ; A = el handle
            or   a                  ; CF = 0
            ret
se_no:      scf
            ret

sv_load:    ld   a,53
            call sv_open
            cp   0FFh
            jr   z,sl_none
            ld   (SV_H),a
            ld   hl,SV_BUF
            ld   de,SV_LEN
            call sv_rd
            ld   (SV_ST),a
            ld   a,(SV_H)
            call sv_close
            ld   a,(SV_ST)
            or   a
            jr   nz,sl_none         ; corto: no es de este programa o esta a medias
            ld   a,(SV_BUF)
            cp   SV_ID
            jr   nz,sl_none
            ld   a,(SV_BUF+1)
            cp   1
            jr   nz,sl_none
            ld   a,(SV_BUF+2)
            cp   2
            jr   nc,sl_none
            ld   a,(SV_BUF+16)
            cp   4
            jr   nc,sl_none
            or   a
            ret
sl_none:    scf
            ret

; lo leido, al juego
sv_apply:   ld   a,(SV_BUF+2)
            ld   (NB_SLOT),a
            inc  a
            ld   (GM_ME),a
IF G_CLOCK
            ld   a,(SV_BUF+3)
            ld   (GM_TC),a
            ld   hl,SV_BUF+19
            ld   de,GM_REM
            ld   bc,4
            ldir
            ld   a,(SV_BUF+23)
            ld   (GM_WHY),a
            xor  a
            ld   (GM_CKON),a        ; los relojes arrancan en play (game_clock_go): el tiempo fuera no cuenta
ENDIF
            ld   hl,SV_BUF+4
            ld   de,NB_CODE81
            ld   bc,10
            ldir
            ld   hl,(SV_BUF+14)
            ld   (GM_PLY),hl
            ld   a,(SV_BUF+16)
            ld   (GM_OVER),a
            ld   hl,(SV_BUF+17)
            ld   (GM_MOVE),hl
            xor  a
            ld   (SV_VAL),a         ; la proxima vez que se dibuje, se guarda
            ld   hl,SV_BUF+24
            jp   g_load

; --- el fichero, por el MCU --------------------------------------------------
; A = 71 (crear o vaciar) o 53 (abrir) el fichero sv_name -> A = el handle (0-3) o 0FFh
sv_open:    ld   e,a
            call sd_clk
            ld   a,e
            call mcu_send
            ld   a,SV_NAMELEN
            call mcu_send
            ld   hl,sv_name
            ld   b,SV_NAMELEN
so_lp:      ld   a,(hl)
            inc  hl
            call mcu_send
            djnz so_lp
            jp   mcu_recv           ; el handle

; A = el handle -> A = el estado (0 bien)
sv_close:   ld   e,a
            call sd_clk
            ld   a,57
            call mcu_send
            ld   a,e
            call mcu_send
            jp   mcu_recv

; A = el handle, HL = datos, DE = cuantos (1 o mas, como mucho 512) -> A = el estado (0 bien)
sv_wr:      ld   b,a
            call sd_clk
            ld   a,56
            call mcu_send
            ld   a,b
            call mcu_send
            ld   a,e
            call mcu_send
            ld   a,d
            call mcu_send
sw_lp:      ld   a,(hl)
            inc  hl
            call mcu_send
            dec  de
            ld   a,d
            or   e
            jr   nz,sw_lp
            jp   mcu_recv

; A = el handle, HL = destino, DE = cuantos -> A = el estado (0 completo, 1 el fichero era mas corto)
sv_rd:      ld   b,a
            call sd_clk
            ld   a,55
            call mcu_send
            ld   a,b
            call mcu_send
            ld   a,e
            call mcu_send
            ld   a,d
            call mcu_send
sr_lp:      call mcu_recv
            ld   (hl),a
            inc  hl
            dec  de
            ld   a,d
            or   e
            jr   nz,sr_lp
            jp   mcu_recv
