; =====================================================================
;  DICTTEST.ASM - prueba de la busqueda binaria de una palabra en un diccionario de la SD, hecha por
;  el Z80 (sin ayuda del MCU: solo los comandos de fichero de siempre). Es el ensayo para el Scrabble.
;
;  El diccionario es DICT.DIC, en la carpeta actual (lo genera TOOLS/make_dict.py): registros de 16
;  bytes, el 0 es la cabecera ("SDICT01", el numero de palabras N en 4 bytes) y los demas, una palabra
;  cada uno en mayusculas y rellena con ceros, ordenadas por sus bytes. La palabra n esta en el
;  desplazamiento 16 * (n + 1). Cada paso de la busqueda es un fseek (comando 54) y un fread (55) de
;  16 bytes: con N = 500.000, unos 19 pasos por palabra.
;
;  Entradas (el programa se carga en 24576, $6000):
;      24576  run_test      mide 32 busquedas: 16 palabras que estan (elegidas repartidas por todo el
;                           fichero) y las mismas con un '~' detras, que no pueden estar. Devuelve en
;                           BC 0 si todo fue bien o un codigo de error (1 no abre DICT.DIC, 2 no es un
;                           diccionario, 3 vacio). Deja los resultados en RESL (abajo) para que los
;                           imprima el BASIC (DICTTEST.B81). El tiempo, de la hora del MCU (como
;                           LOAD *RTC), al centisegundo.
;      24579  dict_open     abre DICT.DIC y lee la cabecera (BC = 0 bien, o el error 1, 2 o 3)
;      24582  dict_lookup   busca la palabra de WBUF (16 bytes, mayusculas, rellena con ceros):
;                           BC = 1 si esta, 0 si no
;  Resultados (RESL = $7C00):
;      +0 estado (0 bien)   +1 las 16 que estaban y las encontro   +2 las 16 que no y no las encontro
;      +3..5 hora de salida (minutos, segundos, centesimas)   +6..8 hora al acabar (lo mismo)
;      +9..10 pasos de busqueda en total   +11..13 N (palabras)
;
;  Ensamblar: pasmo dicttest.asm dicttest.bin dicttest.sym   (compila.bat)
; =====================================================================

DataPort    equ 0A7h
ClkPort     equ 0AFh

RESL        equ 7C00h
WBUF        equ 7C40h           ; (16) la palabra que se busca
TGT         equ 7C50h           ; (16) copia de trabajo
REC         equ 7C60h           ; (16) el registro leido
FH          equ 7C70h           ; el handle del fichero
FHV         equ RESL+15         ; 1 si FH vale (hay un fichero abierto): lo pone a cero run_test
LO          equ 7C71h           ; (3) los limites de la busqueda (indices de 24 bits)
HI          equ 7C74h           ; (3)
MID         equ 7C77h           ; (3)
NN          equ 7C7Ah           ; (3) numero de palabras
IDX         equ 7C7Dh           ; (3) indice del registro que se va a leer
PROBES      equ 7C80h           ; (2) pasos de busqueda
KK          equ 7C82h           ; contador de la prueba
STEP        equ 7C83h           ; (3) separacion entre las palabras de la prueba
OKP         equ 7C86h
OKA         equ 7C87h
RTCB        equ 7CA0h           ; (22) la hora del MCU
WORDS       equ 7D00h           ; (512) 16 palabras que estan y 16 que no

            org  24576
            jp   run_test           ; 24576
            jp   dict_open_bc       ; 24579
            jp   dict_lookup_bc     ; 24582

; ---------------------------------------------------------------------
;  La prueba
; ---------------------------------------------------------------------
run_test:   ld   hl,RESL
            ld   de,RESL+1
            ld   bc,15
            ld   (hl),0
            ldir
            call dict_open
            or   a
            jp   nz,rt_ret
            ; fase A (no se mide): 16 palabras repartidas por el fichero, y sus versiones "~"
            ld   hl,(NN)
            ld   a,(NN+2)
            ld   e,a                ; E:HL = N
            ld   b,4
rt_sh:      srl  e
            rr   h
            rr   l
            djnz rt_sh              ; N / 16
            ld   (STEP),hl
            ld   a,e
            ld   (STEP+2),a
            srl  e
            rr   h
            rr   l                  ; N / 32: el primero
            ld   (IDX),hl
            ld   a,e
            ld   (IDX+2),a
            xor  a
            ld   (KK),a
rt_pa:      call get_pair
            ld   hl,(IDX)
            ld   a,(IDX+2)
            ld   e,a
            ld   bc,(STEP)
            add  hl,bc
            ld   a,(STEP+2)
            adc  a,e
            ld   e,a
            ld   (IDX),hl
            ld   a,e
            ld   (IDX+2),a
            ld   a,(KK)
            inc  a
            ld   (KK),a
            cp   16
            jr   nz,rt_pa
            ; fase B (se mide): las 32 busquedas
            ld   hl,0
            ld   (PROBES),hl
            xor  a
            ld   (OKP),a
            ld   (OKA),a
            ld   (KK),a
            ld   de,RESL+3
            call rtc_read           ; hora de salida
rt_lk:      ld   a,(KK)             ; la palabra KK de WORDS
            ld   l,a
            ld   h,0
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl
            ld   de,WORDS
            add  hl,de
            ld   de,WBUF
            ld   bc,16
            ldir
            call dict_lookup        ; A = 1 si esta
            ld   b,a
            ld   a,(KK)
            cp   16
            jr   nc,rt_abs
            bit  0,b                ; tenia que estar
            jr   z,rt_nx
            ld   hl,OKP
            inc  (hl)
            jr   rt_nx
rt_abs:     bit  0,b                ; no tenia que estar
            jr   nz,rt_nx
            ld   hl,OKA
            inc  (hl)
rt_nx:      ld   a,(KK)
            inc  a
            ld   (KK),a
            cp   32
            jr   nz,rt_lk
            ld   de,RESL+6
            call rtc_read           ; hora al acabar
            ld   a,(OKP)
            ld   (RESL+1),a
            ld   a,(OKA)
            ld   (RESL+2),a
            ld   hl,(PROBES)
            ld   (RESL+9),hl
            ld   hl,(NN)
            ld   (RESL+11),hl
            ld   a,(NN+2)
            ld   (RESL+13),a
            xor  a
rt_ret:     ld   (RESL),a
            ld   c,a
            ld   b,0
            ret

; la palabra del indice IDX, a WORDS + 16 * KK; y su version con '~' (que no puede estar), 256 bytes mas alla
get_pair:   call seek_idx
            ld   a,(KK)
            ld   l,a
            ld   h,0
            add  hl,hl
            add  hl,hl
            add  hl,hl
            add  hl,hl
            ld   de,WORDS
            add  hl,de
            push hl
            ld   de,16
            call f_read
            pop  hl                 ; la palabra
            push hl
            ld   de,256
            add  hl,de
            ex   de,hl              ; DE = la copia
            pop  hl
            ld   bc,16
            ldir
            ex   de,hl              ; HL = el final de la copia
            ld   de,16
            or   a
            sbc  hl,de              ; HL = el principio de la copia
            ld   b,16
gp_z:       ld   a,(hl)
            or   a
            jr   z,gp_t
            inc  hl
            djnz gp_z
            ret                     ; (ocupa los 16: no puede pasar con 15 letras como maximo)
gp_t:       ld   (hl),'~'
            ret

; ---------------------------------------------------------------------
;  El diccionario
; ---------------------------------------------------------------------
; abre DICT.DIC y lee la cabecera -> A = 0 bien, 1 no se abre, 2 no es un diccionario, 3 vacio
dict_open:  ld   a,(FHV)            ; si ya habia uno abierto, se cierra (el MCU solo tiene 4 handles)
            or   a
            call nz,do_cl
            ld   a,53
            call f_open
            cp   0FFh
            jr   nz,do_ok
            ld   a,1
            ret
do_ok:      ld   (FH),a
            ld   a,1
            ld   (FHV),a
            ld   hl,REC
            ld   de,16
            call f_read             ; la cabecera: la posicion es 0 al abrir
            ld   hl,REC
            ld   de,d_magic
            ld   b,7
do_m:       ld   a,(de)
            cp   (hl)
            jr   nz,do_e2
            inc  hl
            inc  de
            djnz do_m
            ld   hl,(REC+8)
            ld   a,(REC+10)
            ld   (NN),hl
            ld   (NN+2),a
            or   h
            or   l
            jr   z,do_e3
            xor  a
            ret
do_e2:      call do_cl
            ld   a,2
            ret
do_e3:      call do_cl
            ld   a,3
            ret
do_cl:      ld   a,(FH)             ; cierra el fichero (si falla la cabecera, que no quede abierto)
            call f_close
            xor  a
            ld   (FHV),a
            ret

dict_open_bc: call dict_open
            ld   c,a
            ld   b,0
            ret

dict_lookup_bc: call dict_lookup
            ld   c,a
            ld   b,0
            ret

; busca la palabra de WBUF -> A = 1 si esta, 0 si no
dict_lookup: ld  hl,WBUF
            ld   de,TGT
            ld   bc,16
            ldir
            xor  a
            ld   (LO),a
            ld   (LO+1),a
            ld   (LO+2),a
            ld   hl,(NN)            ; hi = N - 1
            ld   a,(NN+2)
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
            ld   (HI),hl
            ld   a,e
            ld   (HI+2),a
lk_loop:    call lo_gt_hi           ; si lo > hi, no esta
            jp   c,lk_no
            ld   a,(LO)             ; mid = (lo + hi) / 2, de 24 bits
            ld   hl,HI
            add  a,(hl)
            ld   (MID),a
            ld   a,(LO+1)
            inc  hl
            adc  a,(hl)
            ld   (MID+1),a
            ld   a,(LO+2)
            inc  hl
            adc  a,(hl)
            ld   (MID+2),a          ; el acarreo es el bit 24
            rra
            ld   (MID+2),a
            ld   a,(MID+1)
            rra
            ld   (MID+1),a
            ld   a,(MID)
            rra
            ld   (MID),a
            ld   hl,(MID)
            ld   a,(MID+2)
            ld   (IDX),hl
            ld   (IDX+2),a
            call seek_idx
            ld   hl,REC
            ld   de,16
            call f_read
            ld   hl,(PROBES)
            inc  hl
            ld   (PROBES),hl
            ld   hl,TGT             ; REC contra la palabra buscada, 16 bytes
            ld   de,REC
            ld   b,16
lk_c:       ld   a,(de)
            cp   (hl)
            jr   nz,lk_ne
            inc  hl
            inc  de
            djnz lk_c
            ld   a,1                ; igual: esta
            ret
lk_ne:      jr   c,lk_up            ; REC < palabra: seguir por arriba (lo = mid + 1)
            ld   hl,(MID)           ; REC > palabra: por abajo (hi = mid - 1)
            ld   a,(MID+2)
            ld   e,a
            ld   a,h
            or   l
            or   e
            jp   z,lk_no            ; mid era 0: no hay nada por debajo
            ld   a,l
            sub  1
            ld   l,a
            ld   a,h
            sbc  a,0
            ld   h,a
            ld   a,e
            sbc  a,0
            ld   e,a
            ld   (HI),hl
            ld   a,e
            ld   (HI+2),a
            jp   lk_loop
lk_up:      ld   hl,(MID)
            ld   a,(MID+2)
            ld   e,a
            inc  hl
            ld   a,h
            or   l
            jr   nz,lk_u1
            inc  e
lk_u1:      ld   (LO),hl
            ld   a,e
            ld   (LO+2),a
            jp   lk_loop
lk_no:      xor  a
            ret

; CF = 1 si lo > hi (24 bits)
lo_gt_hi:   ld   a,(HI+2)
            ld   hl,LO+2
            cp   (hl)
            ret  nz
            ld   a,(HI+1)
            dec  hl
            cp   (hl)
            ret  nz
            ld   a,(HI)
            dec  hl
            cp   (hl)
            ret

; fseek al registro IDX: desplazamiento = (IDX + 1) * 16
seek_idx:   ld   hl,(IDX)
            ld   a,(IDX+2)
            ld   e,a
            ld   d,0
            inc  hl
            ld   a,h
            or   l
            jr   nz,si_1
            inc  e
si_1:       ld   b,4
si_sh:      add  hl,hl
            rl   e
            rl   d
            djnz si_sh              ; D:E:H:L = el desplazamiento de 32 bits
            ; sigue en f_seek

; ---------------------------------------------------------------------
;  Los comandos de fichero del MCU (C es el reloj del handshake: sd_clk lo carga en cada comando)
; ---------------------------------------------------------------------
; fseek (54): handle FH, desplazamiento D:E:H:L -> A = estado
f_seek:     call sd_clk
            ld   a,54
            call mcu_send
            ld   a,(FH)
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

; fread (55): handle FH, HL = destino, DE = cuantos -> A = estado (0 completo)
f_read:     call sd_clk
            ld   a,55
            call mcu_send
            ld   a,(FH)
            call mcu_send
            ld   a,e
            call mcu_send
            ld   a,d
            call mcu_send
fr_lp:      call mcu_recv
            ld   (hl),a
            inc  hl
            dec  de
            ld   a,d
            or   e
            jr   nz,fr_lp
            jp   mcu_recv

; fclose (57): A = el handle
f_close:    ld   e,a
            call sd_clk
            ld   a,57
            call mcu_send
            ld   a,e
            call mcu_send
            jp   mcu_recv

; fopen (53): DICT.DIC -> A = el handle o 0FFh
f_open:     ld   e,a
            call sd_clk
            ld   a,e
            call mcu_send
            ld   a,8
            call mcu_send           ; longitud del nombre
            ld   hl,d_name
            ld   b,8
fo_lp:      ld   a,(hl)
            inc  hl
            call mcu_send
            djnz fo_lp
            jp   mcu_recv

; la hora del MCU (comando 50, como LOAD *RTC) -> DE: minutos, segundos y centesimas
rtc_read:   push de
            call sd_clk
            ld   a,50
            call mcu_send
            xor  a
            call mcu_send           ; longitud 0: pide la fecha y la hora
            ld   hl,RTCB
            ld   b,22
rr_lp:      call mcu_recv
            ld   (hl),a
            inc  hl
            djnz rr_lp
            call mcu_recv           ; el estado
            pop  de
            ld   hl,RTCB+14         ; "AAAA-MM-DD hh:mm:ss.cc" en codigos del ZX81
            call dig2
            ld   (de),a             ; mm
            inc  de
            ld   hl,RTCB+17
            call dig2
            ld   (de),a             ; ss
            inc  de
            ld   hl,RTCB+20
            call dig2
            ld   (de),a             ; cc
            ret

; HL = dos digitos en codigos del ZX81 -> A = su valor
dig2:       ld   a,(hl)
            sub  28
            ld   b,a
            add  a,a
            add  a,a
            add  a,b
            add  a,a                ; 10 * decenas
            inc  hl
            ld   b,a
            ld   a,(hl)
            sub  28
            add  a,b
            ret

sd_clk:     in   a,(ClkPort)
            ld   c,a
            ret

mcu_send:   out  (DataPort),a
            ld   a,c
            cpl
            ld   c,a
ms_w:       in   a,(ClkPort)
            xor  c
            jp   m,ms_w
            ret

mcu_recv:   in   a,(DataPort)
            push af
            ld   a,c
            cpl
            ld   c,a
mr_w:       in   a,(ClkPort)
            xor  c
            jp   m,mr_w
            pop  af
            ret

d_name:     defm "DICT.DIC"
d_magic:    defm "SDICT01"
