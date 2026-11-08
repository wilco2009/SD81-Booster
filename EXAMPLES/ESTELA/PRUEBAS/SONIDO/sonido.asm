; =============================================================
; SONIDO.ASM -- fase 0 (c) de ESTELA: musica VGM y efectos a la vez.
;
; La musica (MUSICA.VGM, de la carpeta actual, en bucle) la toca el MCU
; en su AY emulado, sin gastar CPU (comandos 38 y 34). Los efectos salen
; por otros caminos, para ver cuales conviven con ella:
;
;   1 = laser en el canal C del AY A de la FPGA (ZonX, puertos $CF/$0F):
;       el Z80 escribe los registros en cada imagen (tono que baja, 15
;       imagenes). El canal C sale por los dos lados (el A solo por la
;       izquierda y el B por la derecha: SD81.v, sample_l / sample_r)
;   2 = explosion en el canal C del AY B de la FPGA ($C7/$07): ruido, 30
;       imagenes
;   3 = efecto PEG "coin" (comandos 40 y 41): va al AY del MCU, el mismo
;       de la musica
;   4 = voz "WARNING" (comando 23, en segundo plano con "*")
;   5 = los cuatro a la vez
;   6 = diagnostico: 1 s de acorde en los tres canales de los dos AY de la
;       FPGA (volumen 15), como el test 23 de SD81TEST
;   M = parar / seguir la musica (comandos 36 y 37)
;   X = salir (para la musica y deja los AY de la FPGA callados)
;
; Pantalla (modo 160x100 a 256 colores): cuatro barras, una por efecto,
; que se encienden mientras suena (la 3 y la 4 un tiempo fijo, porque no
; se sabe cuando acaban). Borde: verde = musica sonando, amarillo = parada,
; rojo = el MCU no ha podido abrir MUSICA.VGM.
;
; El bucle va a 50 imagenes por segundo con el VSYNC, como el juego.
;
; Ensamblar: pasmo sonido.asm sonido.bin sonido.sym
; Cargar desde BASIC: SONIDO.B81 (CLEAR 18431, LOAD FAST "SONIDO.BIN" CODE 18432)
; =============================================================

        org 4800h

AYA_SEL equ 0CFh                ; AY A (ZonX): seleccion de registro / dato
AYA_DAT equ 0Fh
AYB_SEL equ 0C7h                ; AY B (puertos impares: los pares los ve la ULA)
AYB_DAT equ 07h
CMD_SAY equ 23
CMD_PLAYVGM equ 34
CMD_STOPVGM equ 35
CMD_PAUSEVGM equ 36
CMD_CONTVGM equ 37
CMD_LOOPVGM equ 38
CMD_LOADPEG equ 40
CMD_PLAYPEG equ 41
TLASER  equ 15                  ; imagenes de cada efecto
TBOOM   equ 30
TPEG    equ 12
TSAY    equ 50
TCHORD  equ 50

        jp start

        include "../../../PHOTO/mcu.asm"
        include "../comun.asm"

start:  di
        call ayquiet
        xor a
        call mode175
        call bars
        ld b,4                  ; las cuatro barras apagadas
        ld c,1
st1:    push bc
        call baroff
        pop bc
        inc c
        djnz st1
        ld hl,peg_coin          ; el efecto PEG, en la direccion 0 del MCU
        ld b,PEG_COIN_N
        ld a,CMD_LOADPEG
        call mcu_send
        xor a
        call mcu_send
        ld a,b
        call mcu_send
st2:    ld a,(hl)
        call mcu_send
        inc hl
        djnz st2
        ld a,CMD_LOOPVGM        ; la musica, en bucle
        call mcu_send
        ld a,1
        call mcu_send
        ld hl,s_vgm
        ld a,CMD_PLAYVGM
        call mcu_name
        ld (vgmerr),a
        ld a,1
        ld (playing),a
        call showbd

main:   call waitvs
        call fxa
        call fxb
        call fxtimers
        call keys
        jr nz,main

quit:   ld a,(vgmerr)
        or a
        jr nz,q1
        ld a,CMD_STOPVGM
        call mcu_send
q1:     call ayquiet
        call mode85
        ld bc,0
        ret

; -------------------------------------------------------------
; keys: teclas nuevas (las que no estaban pulsadas en la vuelta anterior).
; NZ = seguir, Z = X pulsada.
; -------------------------------------------------------------
keys:   ld a,0f7h               ; 1 2 3 4 5
        in a,(0feh)
        cpl
        and 1fh
        ld c,a
        ld a,(prevk)
        cpl
        and c                   ; A = recien pulsadas
        ld b,a
        ld a,c
        ld (prevk),a
        bit 4,b                 ; 5 = todas
        jr z,k0
        ld a,0fh
        or b
        ld b,a
k0:     bit 0,b
        call nz,golaser
        bit 1,b
        call nz,goboom
        bit 2,b
        call nz,gopeg
        bit 3,b
        call nz,gosay
        ld a,0efh               ; 6 (fila 0 9 8 7 6, bit 4): acorde de prueba
        in a,(0feh)
        cpl
        and 10h
        ld c,a
        ld a,(prev6)
        cpl
        and c
        ld b,a
        ld a,c
        ld (prev6),a
        bit 4,b
        call nz,gochord
        ld a,7fh                ; M: musica parada / sonando
        in a,(0feh)
        and 4
        ld c,a
        ld a,(prevm)
        ld b,a
        ld a,c
        ld (prevm),a
        or a
        jr nz,k2                ; no pulsada
        ld a,b
        or a
        jr z,k2                 ; ya lo estaba
        ld a,(vgmerr)
        or a
        jr nz,k2
        ld a,(playing)
        xor 1
        ld (playing),a
        ld a,CMD_PAUSEVGM
        jr z,k1
        ld a,CMD_CONTVGM
k1:     call mcu_send
        call showbd
k2:     ld a,0feh               ; X: salir
        in a,(0feh)
        and 4
        ret

; borde: rojo si no hay musica, verde sonando, amarillo parada
showbd: ld a,(vgmerr)
        or a
        ld a,2
        jr nz,sb1
        ld a,(playing)
        or a
        ld a,4
        jr nz,sb1
        ld a,6
sb1:    jp border

; -------------------------------------------------------------
; Efectos. Cada uno pone su contador y enciende su barra; fxa y fxb
; escriben los AY de la FPGA en cada imagen.
; -------------------------------------------------------------
golaser:
        push bc
        ld a,TLASER
        ld (ta),a
        ld hl,40
        ld (pera),hl
        ld c,AYA_SEL
        ld de,073bh             ; mezclador: solo el tono del canal C
        call aywr
        ld c,1
        call baron
        pop bc
        ret

goboom: push bc
        ld a,TBOOM
        ld (tb),a
        ld a,4
        ld (noib),a
        ld c,AYB_SEL
        ld de,071fh             ; mezclador: solo el ruido del canal C
        call aywr
        ld c,2
        call baron
        pop bc
        ret

gopeg:  push bc
        ld a,CMD_PLAYPEG        ; hilo 0, direccion 0
        call mcu_send
        xor a
        call mcu_send
        xor a
        call mcu_send
        ld a,TPEG
        ld (tp),a
        ld c,3
        call baron
        pop bc
        ret

gosay:  push bc
        ld hl,s_say
        ld a,CMD_SAY
        call mcu_name
        ld a,TSAY
        ld (ts),a
        ld c,4
        call baron
        pop bc
        ret

; fxa: laser. Volumen = imagenes que quedan, el tono baja
fxa:    ld a,(ta)
        or a
        ret z
        dec a
        ld (ta),a
        ld e,a
        ld d,10                 ; R10 = volumen del canal C
        ld c,AYA_SEL
        call aywr
        ld hl,(pera)
        ld de,12
        add hl,de
        ld (pera),hl
        ld e,l
        ld d,4                  ; R4, R5 = periodo del canal C
        call aywr
        ld e,h
        ld d,5
        call aywr
        ld a,(ta)
        or a
        ret nz
        ld c,1
        jp baroff

; fxb: explosion. Ruido cada vez mas grave, volumen que baja cada 2 imagenes
fxb:    ld a,(tb)
        or a
        ret z
        dec a
        ld (tb),a
        srl a
        ld e,a
        ld d,10                 ; R10 = volumen del canal C
        ld c,AYB_SEL
        call aywr
        ld a,(noib)
        cp 31
        jr nc,fb1
        inc a
        ld (noib),a
fb1:    ld e,a
        ld d,6                  ; R6 = periodo del ruido
        call aywr
        ld a,(tb)
        or a
        ret nz
        ld c,2
        jp baroff

; gochord: 1 s de tono en los tres canales de los dos AY de la FPGA
; (volumen 15), para comprobar que se oyen; luego los calla
gochord:
        push bc
        ld c,AYA_SEL
        ld hl,chordA
        call gc1
        ld c,AYB_SEL
        ld hl,chordB
        call gc1
        ld a,TCHORD
        ld (tc),a
        xor a                   ; el laser y la explosion quedan cortados
        ld (ta),a
        ld (tb),a
        ld c,1
        call baroff
        ld c,2
        call baroff
        pop bc
        ret
gc1:    ld d,0                  ; R0-R5: periodos
gc2:    ld e,(hl)
        inc hl
        call aywr
        inc d
        ld a,d
        cp 6
        jr nz,gc2
        ld de,0738h             ; mezclador: los tres tonos
        call aywr
        ld de,080fh
        call aywr
        ld de,090fh
        call aywr
        ld de,0a0fh
        jp aywr
chordA: defw 400,330,270        ; AY A: canales A, B y C
chordB: defw 220,180,145        ; AY B

; fxtimers: las barras de PEG y voz se apagan pasado su tiempo; el acorde
; se calla
fxtimers:
        ld a,(tc)
        or a
        jr z,ftp
        dec a
        ld (tc),a
        call z,ayquiet
ftp:    ld hl,tp
        ld c,3
        call ft1
        ld hl,ts
        ld c,4
ft1:    ld a,(hl)
        or a
        ret z
        dec (hl)
        ret nz
        jp baroff

; aywr: C = puerto de seleccion del chip, D = registro, E = valor.
; El puerto del dato es C AND $3F ($CF -> $0F, $C7 -> $07). Conserva todo menos A.
aywr:   out (c),d
        push bc
        ld a,c
        and 3fh
        ld c,a
        out (c),e
        pop bc
        ret

; ayquiet: los dos AY de la FPGA callados (mezclador apagado, volumenes a 0)
ayquiet:
        ld c,AYA_SEL
        call aq1
        ld c,AYB_SEL
aq1:    ld de,073fh
        call aywr
        ld de,0800h
        call aywr
        ld de,0900h
        call aywr
        ld de,0a00h
        jp aywr

; mcu_name: A = comando, HL = nombre en ASCII acabado en 0 -> lo manda en
; codigo ZX81 (longitud y caracteres) y devuelve A = estado (0 = bien)
mcu_name:
        call mcu_send
        push hl
        ld b,0
mn1:    ld a,(hl)
        or a
        jr z,mn2
        inc b
        inc hl
        jr mn1
mn2:    pop hl
        ld a,b
        call mcu_send
mn3:    ld a,(hl)
        or a
        jp z,mcu_recv
        call asc2zx
        call mcu_send
        inc hl
        jr mn3

; asc2zx: A = ASCII (mayusculas, cifras, . y *) -> codigo ZX81
asc2zx: cp 'A'
        jr c,az1
        sub 'A'-26h
        ret
az1:    cp '0'
        jr c,az2
        sub '0'-1ch
        ret
az2:    cp '.'
        ld a,1bh
        ret z
        ld a,17h                ; '*'
        ret

; -------------------------------------------------------------
; Pantalla: cuatro barras de 40 pixeles (colores 1-4) en las filas 20-79
; -------------------------------------------------------------
bars:   ld hl,IMG+20*160
        ld c,60
br1:    ld a,1
br2:    ld b,40
br3:    ld (hl),a
        inc hl
        djnz br3
        inc a
        cp 5
        jr nz,br2
        dec c
        jr nz,br1
        ret

; baron / baroff: C = barra (1-4) -> su color de la paleta, encendido o apagado
baron:  push hl
        ld hl,colon-2
        jr bo1
baroff: push hl
        ld hl,coloff-2
bo1:    push de
        ld a,c
        add a,a
        add a,l
        ld l,a
        jr nc,bo2
        inc h
bo2:    ld e,(hl)
        inc hl
        ld d,(hl)
        call setpal1
        pop de
        pop hl
        ret

; {G,B} y {BRIGHT,R} de cada barra
colon:  defb 0f0h,0fh           ; 1 laser: amarillo
        defb 00h,0fh            ; 2 explosion: rojo
        defb 0f5h,0f0h          ; 3 PEG: verde claro
        defb 0afh,0f0h          ; 4 voz: azul claro
coloff: defb 22h,02h            ; apagadas: gris oscuro
        defb 22h,02h
        defb 22h,02h
        defb 22h,02h

; coin.peb (EXAMPLES/PEG): tres notas subiendo, 240 ms
peg_coin:
        defb 3Eh,07h,00h,01h,0Ch,08h,96h,00h,3Ch,90h,64h,00h
        defb 50h,90h,4Bh,00h,64h,90h,00h,08h,3Fh,07h,10h,0A0h
PEG_COIN_N equ $-peg_coin

; -------------------------------------------------------------
; variables
; -------------------------------------------------------------
s_vgm:  defb "MUSICA.VGM",0
s_say:  defb "*WARNING",0
vgmerr: defb 0
playing: defb 0
prevk:  defb 0
prevm:  defb 4
prev6:  defb 0
tc:     defb 0
ta:     defb 0
tb:     defb 0
tp:     defb 0
ts:     defb 0
noib:   defb 0
pera:   defw 0

        end start
