; =====================================================================
;  DICT.INC - busqueda binaria de una palabra en un diccionario de la SD, hecha por el Z80 (sin ayuda del
;  MCU: solo los comandos de fichero de siempre). Extraido de EXAMPLES/DICTTEST/dicttest.asm, que lo ensayo
;  (en la placa: 42,5 ms por palabra con 168.551 palabras).
;
;  El diccionario (lo genera TOOLS/make_dict.py) son registros de 16 bytes: el 0 es la cabecera ("SDICT01",
;  el numero de palabras N en 4 bytes) y los demas, una palabra cada uno, rellena con ceros, ordenadas por sus
;  bytes. La palabra n esta en el desplazamiento 16 * (n + 1). Cada paso es un fseek (comando 54) y un fread
;  (55) de 16 bytes.
;
;  El que lo incluye define antes:
;      DC_BASE   equ ...   ; RAM de trabajo (DC_LEN bytes)
;  y tiene sd_clk, mcu_send y mcu_recv (net.inc o el banco de pruebas). C es el reloj del handshake.
;
;  Rutinas (destruyen todos los registros):
;      dc_open     HL = nombre del fichero (ASCII), B = su longitud  ->  A = 0 bien, 1 no se abre, 2 no es un
;                  diccionario, 3 vacio. Si ya habia uno abierto, lo cierra (el MCU solo tiene 4 handles)
;      dc_lookup   la palabra de DC_W (16 bytes, rellena con ceros)  ->  A = 1 si esta, 0 si no
;      dc_close    cierra el fichero
;  Variables: DC_W (16, la palabra), DC_NN (3, numero de palabras), DC_PROBES (2, pasos hechos en total)
; =====================================================================

DC_FH       equ DC_BASE+0       ; el handle del fichero
DC_FHV      equ DC_BASE+1       ; 1 si DC_FH vale (hay un fichero abierto)
DC_LO       equ DC_BASE+2       ; (3) los limites de la busqueda (indices de 24 bits)
DC_HI       equ DC_BASE+5       ; (3)
DC_MID      equ DC_BASE+8       ; (3)
DC_NN       equ DC_BASE+11      ; (3) numero de palabras
DC_IDX      equ DC_BASE+14      ; (3) indice del registro que se va a leer
DC_W        equ DC_BASE+17      ; (16) la palabra que se busca
DC_TGT      equ DC_BASE+33      ; (16) copia de trabajo
DC_REC      equ DC_BASE+49      ; (16) el registro leido
DC_PROBES   equ DC_BASE+65      ; (2) pasos de busqueda
DC_LEN      equ 67

dc_open:    push hl
            push bc
            ld   a,(DC_FHV)
            or   a
            call nz,dc_close
            pop  bc
            pop  hl
            call dc_fopen
            cp   0FFh
            jr   nz,dc_o1
            ld   a,1
            ret
dc_o1:      ld   (DC_FH),a
            ld   a,1
            ld   (DC_FHV),a
            ld   hl,DC_REC
            ld   de,16
            call dc_fread           ; la cabecera: la posicion es 0 al abrir
            ld   hl,DC_REC
            ld   de,dc_magic
            ld   b,7
dc_om:      ld   a,(de)
            cp   (hl)
            jr   nz,dc_e2
            inc  hl
            inc  de
            djnz dc_om
            ld   hl,(DC_REC+8)
            ld   a,(DC_REC+10)
            ld   (DC_NN),hl
            ld   (DC_NN+2),a
            or   h
            or   l
            jr   z,dc_e3
            xor  a
            ret
dc_e2:      call dc_close
            ld   a,2
            ret
dc_e3:      call dc_close
            ld   a,3
            ret

dc_close:   ld   a,(DC_FHV)
            or   a
            ret  z
            ld   a,(DC_FH)
            call dc_fclose
            xor  a
            ld   (DC_FHV),a
            ret

; busca la palabra de DC_W -> A = 1 si esta, 0 si no
dc_lookup:  ld   hl,DC_W
            ld   de,DC_TGT
            ld   bc,16
            ldir
            xor  a
            ld   (DC_LO),a
            ld   (DC_LO+1),a
            ld   (DC_LO+2),a
            ld   hl,(DC_NN)         ; hi = N - 1
            ld   a,(DC_NN+2)
            ld   e,a
            ld   a,l
            sub  1
            ld   l,a
            ld   a,h
            sbc  a,0
            ld   h,a
            ld   a,e
            sbc  a,0
            ld   e,a
            ld   (DC_HI),hl
            ld   a,e
            ld   (DC_HI+2),a
dc_loop:    call dc_lo_gt_hi        ; si lo > hi, no esta
            jp   c,dc_no
            ld   a,(DC_LO)          ; mid = (lo + hi) / 2, de 24 bits
            ld   hl,DC_HI
            add  a,(hl)
            ld   (DC_MID),a
            ld   a,(DC_LO+1)
            inc  hl
            adc  a,(hl)
            ld   (DC_MID+1),a
            ld   a,(DC_LO+2)
            inc  hl
            adc  a,(hl)
            ld   (DC_MID+2),a       ; el acarreo es el bit 24
            rra
            ld   (DC_MID+2),a
            ld   a,(DC_MID+1)
            rra
            ld   (DC_MID+1),a
            ld   a,(DC_MID)
            rra
            ld   (DC_MID),a
            ld   hl,(DC_MID)
            ld   a,(DC_MID+2)
            ld   (DC_IDX),hl
            ld   (DC_IDX+2),a
            call dc_seekidx
            ld   hl,DC_REC
            ld   de,16
            call dc_fread
            ld   hl,(DC_PROBES)
            inc  hl
            ld   (DC_PROBES),hl
            ld   hl,DC_TGT          ; DC_REC contra la palabra buscada, 16 bytes
            ld   de,DC_REC
            ld   b,16
dc_c:       ld   a,(de)
            cp   (hl)
            jr   nz,dc_ne
            inc  hl
            inc  de
            djnz dc_c
            ld   a,1                ; igual: esta
            ret
dc_ne:      jr   c,dc_up            ; registro < palabra: seguir por arriba (lo = mid + 1)
            ld   hl,(DC_MID)        ; registro > palabra: por abajo (hi = mid - 1)
            ld   a,(DC_MID+2)
            ld   e,a
            ld   a,h
            or   l
            or   e
            jp   z,dc_no            ; mid era 0: no hay nada por debajo
            ld   a,l
            sub  1
            ld   l,a
            ld   a,h
            sbc  a,0
            ld   h,a
            ld   a,e
            sbc  a,0
            ld   e,a
            ld   (DC_HI),hl
            ld   a,e
            ld   (DC_HI+2),a
            jp   dc_loop
dc_up:      ld   hl,(DC_MID)
            ld   a,(DC_MID+2)
            ld   e,a
            inc  hl
            ld   a,h
            or   l
            jr   nz,dc_u1
            inc  e
dc_u1:      ld   (DC_LO),hl
            ld   a,e
            ld   (DC_LO+2),a
            jp   dc_loop
dc_no:      xor  a
            ret

; CF = 1 si lo > hi (24 bits)
dc_lo_gt_hi: ld  a,(DC_HI+2)
            ld   hl,DC_LO+2
            cp   (hl)
            ret  nz
            ld   a,(DC_HI+1)
            dec  hl
            cp   (hl)
            ret  nz
            ld   a,(DC_HI)
            dec  hl
            cp   (hl)
            ret

; fseek al registro DC_IDX: desplazamiento = (DC_IDX + 1) * 16
dc_seekidx: ld   hl,(DC_IDX)
            ld   a,(DC_IDX+2)
            ld   e,a
            ld   d,0
            inc  hl
            ld   a,h
            or   l
            jr   nz,dc_si1
            inc  e
dc_si1:     ld   b,4
dc_sish:    add  hl,hl
            rl   e
            rl   d
            djnz dc_sish            ; D:E:H:L = el desplazamiento de 32 bits
            ; sigue en dc_fseek

; ---------------------------------------------------------------------
;  Los comandos de fichero del MCU (C es el reloj del handshake: sd_clk lo carga en cada comando)
; ---------------------------------------------------------------------
; fseek (54): handle DC_FH, desplazamiento D:E:H:L -> A = estado
dc_fseek:   call sd_clk
            ld   a,54
            call mcu_send
            ld   a,(DC_FH)
            call mcu_send
            ld   a,l
            call mcu_send
            ld   a,h
            call mcu_send
            ld   a,e
            call mcu_send
            ld   a,d
            call mcu_send
            jp   mcu_recv

; fread (55): handle DC_FH, HL = destino, DE = cuantos -> A = estado (0 completo)
dc_fread:   call sd_clk
            ld   a,55
            call mcu_send
            ld   a,(DC_FH)
            call mcu_send
            ld   a,e
            call mcu_send
            ld   a,d
            call mcu_send
dc_fr1:     call mcu_recv
            ld   (hl),a
            inc  hl
            dec  de
            ld   a,d
            or   e
            jr   nz,dc_fr1
            jp   mcu_recv

; fclose (57): A = el handle
dc_fclose:  ld   e,a
            call sd_clk
            ld   a,57
            call mcu_send
            ld   a,e
            call mcu_send
            jp   mcu_recv

; fopen (53): HL = nombre (ASCII), B = longitud -> A = el handle o 0FFh
dc_fopen:   call sd_clk
            ld   a,53
            call mcu_send
            ld   a,b
            call mcu_send           ; longitud del nombre
dc_fo1:     ld   a,(hl)
            inc  hl
            call mcu_send
            djnz dc_fo1
            jp   mcu_recv

dc_magic:   defm "SDICT01"
