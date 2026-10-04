; =====================================================================
;  DEBUGMON.ASM - monitor del depurador por hardware del SD81 Booster
;
;  Va en la SD como /SYS/DEBUG.BIN. El MCU lo escribe al encender en la
;  pagina 63 y arma el depurador; el Z80 lo ve en $2000-$3FFF mientras esta
;  parado (la FPGA sirve CALL $2000 al romper y pone la pagina 63 en el
;  bloque 1). Ver claude/planning/hw_debugger_plan.md y
;  FPGA/SD81V2.1000/sim_int.v.
;
;  Es un servidor minimo: toda la logica (consola, desensamblador,
;  breakpoints por software...) esta en el MCU.
;   1. Guarda todos los registros y cambia a su propia pila.
;   2. DBG_BREAK (comando 73): manda al MCU el bloque de registros.
;   3. DBG_POLL (comando 74): manda el resultado de la peticion anterior y
;      recibe la siguiente. El MCU no contesta hasta tener una.
;   4. Al recibir CONTINUAR, repone todo y vuelve con JP $003B.
;
;  Bloque de registros (30 bytes, el que se manda y el que el MCU puede
;  reescribir con SETREGS):
;     0 HL'   2 DE'   4 BC'   6 AF'   8 IY   10 IX
;    12 HL   14 DE   16 BC   18 AF   20 SP   22 PC
;    24 I    25 R    26 IFF2 (0/1)
;    27 estado del depurador ($3FEF, indice 0: armado, NMI, nivel, motivo)
;    28 estado de las interrupciones simuladas ($3FEF, indice 1)
;    29 pagina del programa en el bloque 1
;  SP y PC son los del programa en el punto de ruptura (la instruccion en
;  PC todavia no se ha ejecutado). 27-29 son solo informacion.
;
;  Peticiones del MCU: op(1), direccion(2), n(2), en READP y WRITEP un byte
;  mas (la pagina) y, en WRITE, SETREGS, WRITEP, BRAMW y AYWRITE, n bytes de
;  datos:
;    1 READ     n bytes desde la direccion (1-256)   -> resultado: n bytes
;    2 WRITE    n bytes en la direccion (1-256)       -> nada
;    3 SETREGS  el bloque de registros (n = 30), o 31 con el modo de
;               interrupcion detras (0-2), que se pone al salir -> nada
;    4 OUT      direccion = puerto, n bajo = valor    -> nada
;    5 IN       direccion = puerto                     -> 1 byte
;    6 CONT     continuar                              -> (vuelve al programa)
;    7 INSEQ    direccion = puerto: n IN seguidos (1-256)  -> n bytes
;    8 READP    n bytes (1-256) de la pagina dada, desde el desplazamiento
;               "direccion" (0-$1FFF), mapeandola un momento en el bloque 7
;                                                     -> n bytes
;    9 WRITEP   n bytes (1-256) en la pagina dada, desde el desplazamiento
;               "direccion", igual que READP          -> nada
;   10 BRAMW    n bytes (1-256) a la BRAM de sombra, en su puntero (OUT $87
;               y el dato al puerto $3FEF por cada uno)  -> nada
;   11 AYREAD   direccion = puerto de seleccion de un AY: OUT (puerto),i e
;               IN (puerto) para i = 0..n-1           -> n bytes
;   12 AYWRITE  direccion = puerto de seleccion: OUT (puerto),i y el dato i
;               al puerto de datos (el mismo con A7 = 0), i = 0..n-1 -> nada
;   13 SETI     n bajo = I mientras el monitor espera (0 = la del programa)
;                                                     -> nada
;  INSEQ y READP son para los snapshots: los POKEs de control, en la BRAM de
;  sombra ($3FEF, indice 2), y las paginas que no estan mapeadas. WRITEP y
;  BRAMW, para cargarlos.
;  Un OUT al mapper ($E7) vuelve a leer las paginas de los bloques 1 y 7:
;  las lecturas y escrituras de $2000-$3FFF y la salida usan las nuevas.
;  Las lecturas y escrituras de $2000-$3FFF van a la pagina del programa
;  (no a la del monitor), mapeandola un momento en el bloque 7. El bloque 0
;  (ROM) se puede escribir mientras corre el monitor.
;
;  SETI es para la pantalla del depurador: en Superfast la FPGA saca la
;  fuente de I (la sigue en cada refresco), y parado I es la del programa
;  (en un WRX, cualquier cosa). Con SETI 1Eh se ve la de la ROM. Se queda
;  puesto: cada entrada vuelve a poner esa I en cuanto ha guardado la del
;  programa, hasta un SETI 0. Al continuar siempre vuelve la del programa.
;
;  R: el monitor descuenta las M1 que hay entre la ruptura y su LD A,R, y al
;  salir las que hay entre su LD R,A y la vuelta a PC, asi que el programa
;  ve R como si no se hubiera parado.
;
;  Ensamblar: pasmo debugmon.asm "../SD Content/SYS/DEBUG.BIN"
;  Bancos de pruebas en el PC (con el MCU de verdad): claude/dbgharness.
; =====================================================================

DBG         equ 3FEFh           ; puerto del depurador
DATAP       equ 0A7h            ; protocolo con el MCU: datos
CLKP        equ 0AFh            ; protocolo con el MCU: reloj (bit 7)
MAPP        equ 0E7h            ; mapper
TMPBLK      equ 7               ; bloque para ver la pagina tapada del bloque 1
CMD_BREAK   equ 73
CMD_POLL    equ 74
R_IN        equ 10              ; M1 desde la ruptura hasta el LD A,R
R_OUT       equ 22              ; M1 desde el LD R,A hasta volver a PC

            org  2000h

            jp   entry
            defb "SD81DBG",4    ; $2003: firma y version (4: SETI)

; ---------------------------------------------------------------------
;  Entrada. La pila del programa: [SP] = $003B (del CALL), [SP+2] = PC+1
;  (del RST). Las M1 hasta el LD A,R son R_IN: FF, CD, C3 (el JP de $2000),
;  D3, ED 73, 31, F5 y ED 5F.
;  Lo primero, OUT ($FD),A: la NMI apagada (en SLOW la FPGA solo para en
;  una ventana justo despues de la NMI de una linea, para que esto llegue
;  antes de la siguiente; su rutina usa AF'). Si estaba encendida, el MCU la
;  vuelve a encender al continuar (bit 6 del estado).
; ---------------------------------------------------------------------
entry:      out  (0FDh),a       ; NMI apagada (no toca ningun registro)
            ld   (save_sp),sp
            ld   sp,regs+20     ; el bloque de registros se rellena con PUSH
            push af
            ld   a,r
            ld   (r_raw),a
            ld   a,i            ; P/V = IFF2
            di
            ld   (regs+24),a
            ld   a,0
            jp   po,e_iff
            inc  a
e_iff:      ld   (regs+26),a
            push bc
            push de
            push hl
            push ix
            push iy
            ex   af,af'
            exx
            push af
            push bc
            push de
            push hl
            exx
            ex   af,af'
            ld   sp,mon_stack

            ld   a,(r_raw)      ; R del programa
            ld   c,a
            sub  R_IN
            call r_fix
            ld   (regs+25),a

            ld   bc,1*256+MAPP  ; paginas de los bloques 1 y 7
            in   a,(c)
            and  3Fh
            ld   (regs+29),a
            ld   bc,TMPBLK*256+MAPP
            in   a,(c)
            and  3Fh
            ld   (pg_tmp),a

            ld   bc,DBG         ; estado del depurador y de sim_int
            xor  a
            out  (c),a
            in   a,(c)
            ld   (regs+27),a
            ld   a,1
            out  (c),a
            in   a,(c)
            ld   (regs+28),a

            ld   hl,(save_sp)   ; PC = [SP+2] - 1, SP = SP + 4
            inc  hl
            inc  hl
            call rd_byte
            ld   e,a
            inc  hl
            call rd_byte
            ld   d,a
            dec  de
            ld   (regs+22),de
            inc  hl
            ld   (regs+20),hl

            ld   a,(ui_i)       ; SETI puesto: su I mientras espera
            or   a
            jr   z,e_noi
            ld   i,a
e_noi:      ld   a,CMD_BREAK    ; avisar al MCU
            call mcu_send
            ld   hl,regs
            ld   b,30
eb1:        ld   a,(hl)
            call mcu_send
            inc  hl
            djnz eb1
            ld   hl,0
            ld   (rlen),hl

; ---------------------------------------------------------------------
;  Bucle: resultado de la peticion anterior -> peticion siguiente
; ---------------------------------------------------------------------
poll:       ld   a,CMD_POLL
            call mcu_send
            ld   de,(rlen)
            ld   a,e
            call mcu_send
            ld   a,d
            call mcu_send
            ld   hl,buf
pr1:        ld   a,d
            or   e
            jr   z,pr2
            ld   a,(hl)
            call mcu_send
            inc  hl
            dec  de
            jr   pr1
pr2:        ld   hl,0
            ld   (rlen),hl
            call mcu_recv       ; la peticion: el MCU no contesta hasta
            ld   (op),a         ; tenerla
            call mcu_recv
            ld   (raddr),a
            call mcu_recv
            ld   (raddr+1),a
            call mcu_recv
            ld   (rn),a
            call mcu_recv
            ld   (rn+1),a
            ld   a,(op)
            cp   8
            jr   z,pr4
            cp   9
            jr   nz,pr3
pr4:        call mcu_recv       ; READP y WRITEP: la pagina
            ld   (rpage),a
pr3:        ld   a,(op)
            cp   1
            jp   z,q_read
            cp   2
            jp   z,q_write
            cp   3
            jp   z,q_regs
            cp   4
            jp   z,q_out
            cp   5
            jp   z,q_in
            cp   6
            jp   z,q_cont
            cp   7
            jp   z,q_inseq
            cp   8
            jp   z,q_readp
            cp   9
            jp   z,q_writep
            cp   10
            jp   z,q_bramw
            cp   11
            jp   z,q_ayread
            cp   12
            jp   z,q_aywrite
            cp   13
            jp   z,q_seti
            jp   poll

q_seti:     ld   a,(rn)         ; I mientras espera (0 = la del programa)
            ld   (ui_i),a
            or   a
            jr   nz,qs1
            ld   a,(regs+24)
qs1:        ld   i,a
            jp   poll

q_inseq:    ld   de,(rn)        ; n IN seguidos del mismo puerto
            ld   (rlen),de
            ld   hl,buf
            ld   bc,(raddr)
qi1:        ld   a,d
            or   e
            jp   z,poll
            in   a,(c)
            ld   (hl),a
            inc  hl
            dec  de
            jr   qi1

q_readp:    ld   a,(rpage)      ; la pagina, un momento en el bloque 7
            call map_tmp
            ld   hl,(raddr)
            ld   a,h
            and  1Fh
            or   0E0h
            ld   h,a
            ld   bc,(rn)
            ld   (rlen),bc
            ld   de,buf
            ldir
            ld   a,(pg_tmp)
            call map_tmp
            jp   poll

q_writep:   call recv_n         ; los datos a buf
            ld   a,(rpage)      ; la pagina, un momento en el bloque 7
            call map_tmp
            ld   hl,(raddr)
            ld   a,h
            and  1Fh
            or   0E0h
            ld   d,a
            ld   e,l
            ld   hl,buf
            ld   bc,(rn)
            ldir
            ld   a,(pg_tmp)
            call map_tmp
            jp   poll

q_bramw:    call recv_n         ; los datos a buf
            ld   hl,buf
            ld   de,(rn)
            ld   bc,DBG
qb1:        ld   a,d
            or   e
            jp   z,poll
            ld   a,87h          ; registro 7: escribir en la BRAM
            out  (c),a
            ld   a,(hl)
            out  (c),a
            inc  hl
            dec  de
            jr   qb1

q_ayread:   ld   de,(rn)        ; n registros de un AY
            ld   (rlen),de
            ld   hl,buf
            ld   bc,(raddr)
            xor  a
qa1:        ld   d,a            ; D = registro (E = cuantos quedan)
            out  (c),a          ; elegirlo
            in   a,(c)          ; y leerlo
            ld   (hl),a
            inc  hl
            ld   a,d
            inc  a
            dec  e
            jr   nz,qa1
            jp   poll

q_aywrite:  call recv_n         ; los valores a buf
            ld   hl,buf
            ld   de,(rn)
            ld   bc,(raddr)
            xor  a
qw2:        out  (c),a          ; elegir el registro
            res  7,c            ; puerto de datos
            ld   d,a
            ld   a,(hl)
            out  (c),a
            set  7,c
            inc  hl
            ld   a,d
            inc  a
            dec  e
            jr   nz,qw2
            jp   poll

q_read:     ld   hl,(raddr)
            ld   bc,(rn)
            ld   (rlen),bc
            ld   de,buf
qr1:        ld   a,b
            or   c
            jp   z,poll
            call rd_byte
            ld   (de),a
            inc  hl
            inc  de
            dec  bc
            jr   qr1

q_write:    call recv_n         ; los datos a buf
            ld   hl,(raddr)
            ld   bc,(rn)
            ld   de,buf
qw1:        ld   a,b
            or   c
            jp   z,poll
            ld   a,(de)
            call wr_byte
            inc  hl
            inc  de
            dec  bc
            jr   qw1

q_regs:     call recv_n         ; 30 bytes, o 31 con el IM (im_req)
            ld   hl,buf
            ld   de,regs
            ld   bc,(rn)
            ldir
            jp   poll

q_out:      ld   bc,(raddr)
            ld   a,(rn)
            out  (c),a
            ld   a,c            ; el mapper: volver a leer los bloques 1 y 7
            cp   MAPP
            jp   nz,poll
            ld   bc,1*256+MAPP
            in   a,(c)
            and  3Fh
            ld   (regs+29),a
            ld   bc,TMPBLK*256+MAPP
            in   a,(c)
            and  3Fh
            ld   (pg_tmp),a
            jp   poll

q_in:       ld   bc,(raddr)
            in   a,(c)
            ld   (buf),a
            ld   hl,1
            ld   (rlen),hl
            jp   poll

; los n bytes de datos de la peticion -> buf
recv_n:     ld   bc,(rn)
            ld   hl,buf
rn1:        ld   a,b
            or   c
            ret  z
            call mcu_recv
            ld   (hl),a
            inc  hl
            dec  bc
            jr   rn1

; ---------------------------------------------------------------------
;  Continuar. Se rehace la vuelta con el SP y el PC del bloque (el MCU los
;  ha podido cambiar): [SP-2] = PC+1 y SP-2, y JP $003B. El epilogo de la
;  FPGA (EX (SP),HL / DEC HL / EX (SP),HL / RET) vuelve a PC con SP.
; ---------------------------------------------------------------------
q_cont:     ld   a,(im_req)     ; IM pedido con SETREGS (carga de snapshots
            cp   0FFh           ; sin pasar por la ROM)
            jr   z,qc0
            ld   b,a
            ld   a,0FFh
            ld   (im_req),a
            ld   a,b
            or   a
            jr   nz,qc2
            im   0
            jr   qc0
qc2:        dec  a
            jr   nz,qc3
            im   1
            jr   qc0
qc3:        im   2
qc0:        ld   hl,(regs+20)
            dec  hl
            dec  hl
            ld   (sp_ret),hl
            ld   de,(regs+22)
            inc  de
            ld   a,e
            call wr_byte
            inc  hl
            ld   a,d
            call wr_byte

            ld   a,(regs+25)    ; R: descontar las M1 hasta volver a PC
            ld   c,a
            sub  R_OUT
            call r_fix
            ld   (r_new),a
            ld   a,(regs+24)
            ld   i,a
            ld   a,(regs+26)    ; EI o NOP (mismo numero de M1)
            or   a
            ld   a,0
            jr   z,qc1
            ld   a,0FBh
qc1:        ld   (ei_nop),a

            ld   a,(pg_tmp)     ; por si acaso: el bloque 7 como estaba
            call map_tmp

            ld   sp,regs        ; los registros del programa
            ld   a,(r_new)
            ld   r,a            ; desde aqui, R_OUT M1 hasta PC
            pop  hl
            pop  de
            pop  bc
            pop  af
            exx
            ex   af,af'
            pop  iy
            pop  ix
            pop  hl
            pop  de
            pop  bc
            pop  af
            ld   sp,(sp_ret)
ei_nop:     nop
            jp   003Bh

; A = R calculado (los 7 bits bajos dan la vuelta solos), C = R de partida
; -> A con los 7 bits bajos de A y el bit 7 de C (el bit 7 de R no cambia
; con las M1)
r_fix:      and  7Fh
            ld   b,a
            ld   a,c
            and  80h
            or   b
            ret

; ---------------------------------------------------------------------
;  Memoria del programa. HL = direccion. En $2000-$3FFF esta la ventana del
;  monitor: se mapea un momento la pagina del programa en el bloque 7.
;  Conservan BC, DE y HL.
; ---------------------------------------------------------------------
rd_byte:    ld   a,h
            and  0E0h
            cp   20h
            jr   z,rb1
            ld   a,(hl)
            ret
rb1:        push hl
            push bc
            ld   a,(regs+29)
            call map_tmp
            ld   a,h
            add  a,0C0h         ; $20xx-$3Fxx -> $E0xx-$FFxx
            ld   h,a
            ld   b,(hl)
            ld   a,(pg_tmp)
            call map_tmp
            ld   a,b
            pop  bc
            pop  hl
            ret

wr_byte:    push af
            ld   a,h
            and  0E0h
            cp   20h
            jr   z,wb1
            pop  af
            ld   (hl),a
            ret
wb1:        pop  af
            push hl
            push bc
            ld   b,a
            ld   a,(regs+29)
            call map_tmp
            ld   a,h
            add  a,0C0h
            ld   h,a
            ld   (hl),b
            ld   a,(pg_tmp)
            call map_tmp
            pop  bc
            pop  hl
            ret

; A = pagina -> bloque TMPBLK. Vale para los dos modos del mapper: en el
; simple la pagina va en D7-D3 del dato, en el completo en B. Conserva BC.
map_tmp:    push bc
            ld   b,a
            and  1Fh
            rlca
            rlca
            rlca
            or   TMPBLK
            ld   c,MAPP
            out  (c),a
            pop  bc
            ret

; ---------------------------------------------------------------------
;  Protocolo con el MCU (como el del explorador y el editor): cada llamada
;  mira el reloj; BC, DE y HL se conservan (A no sobrevive a mcu_send)
; ---------------------------------------------------------------------
mcu_send:   push bc
            ld   b,a
            in   a,(CLKP)
            ld   c,a
            ld   a,b
            out  (DATAP),a
msw:        in   a,(CLKP)
            xor  c
            jp   p,msw
            pop  bc
            ret

mcu_recv:   push bc
            in   a,(CLKP)
            ld   c,a
            in   a,(DATAP)
            ld   b,a
mrw:        in   a,(CLKP)
            xor  c
            jp   p,mrw
            ld   a,b
            pop  bc
            ret

; ---------------------------------------------------------------------
;  Variables (en la pagina 63: solo las ve el monitor)
; ---------------------------------------------------------------------
regs:       defs 30
im_req:     defb 0FFh           ; byte 31 de SETREGS: IM al salir (FF = no tocar)
ui_i:       defb 0              ; SETI: I mientras espera (0 = la del programa)
save_sp:    defw 0
sp_ret:     defw 0
r_raw:      defb 0
r_new:      defb 0
pg_tmp:     defb 0
op:         defb 0
raddr:      defw 0
rn:         defw 0
rpage:      defb 0
rlen:       defw 0
buf:        defs 256
            defs 64
mon_stack:

            end
