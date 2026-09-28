; =============================================================
; SD81AV.ASM -- SD81TEST, modulo AV: sonido, video interactivo y entrada
;
; Se carga en MOD_ORG ($6000) con
;   LOAD FAST "SD81AV.BIN" CODE 24576
; (lo hace el stub BASIC) y se ejecuta a traves de la tabla de saltos
; del nucleo (SD81TEST.BIN), cuyas rutinas usa por su .sym. Ensamblar
; despues del nucleo:
;   pasmo sd81test.asm SD81TEST.BIN sd81test.sym
;   pasmo sd81av.asm SD81AV.BIN
; mbss_end (en el .sym de este modulo, si se pide) tiene que quedar
; por debajo de $8000.
; =============================================================
        include "sd81test.sym"

        org MOD_ORG
        db MOD_AV              ; cabecera: id del modulo
        dw bss_end              ; y fin del nucleo con el que se ensamblo
; Tabla de saltos del modulo (el nucleo entra por MOD_ORG+3+3*n)
        jp ay_test             ; 0: USR 20522
        jp spr_test            ; 1: USR 20534
        jp bd_test             ; 2: USR 20537
        jp ch_test             ; 3: USR 20540
        jp sf_test             ; 4: USR 20543
        jp kb_test             ; 5: USR 20546
        jp so_test             ; 6: USR 20549
        jp cs_test             ; 7: USR 20558
        jp wd_test             ; 8: USR 20561
        jp fs_test             ; 9: USR 20564
        jp c1_test             ; 10: USR 20567
        jp db_test             ; 11: USR 20570

; =============================================================
; Test 14: registros de los chips AY (USR 20522)
; Los dos AY de la FPGA: seleccion de registro (A7=1) y dato (A7=0),
; lectura con IN del puerto de seleccion. Chip A (ZonX): $CF/$0F; chip B:
; $C7/$07 (la FPGA ignora A0; se usan puertos impares porque un IN/OUT a
; un puerto par tambien lo decodifica la ULA: teclado y NMI). Registros de
; 8 bits (0, 2, 4, 11, 12) con 12 patrones; se guardan y se restauran.
; =============================================================
AY_NREG equ 5
AY_NVAL equ 12

ay_test:
        ld hl,s_ay_title
        call title
        call plines
        db 2
        dw s_ay_d1
        db 3
        dw s_ay_d2
        db 0FFh
        ld hl,0
        ld (err_a),hl
        ld (err_b),hl
        ld c,0CFh               ; chip A
        ld hl,err_a
        call ay_chip
        ld c,0C7h               ; chip B
        ld hl,err_b
        call ay_chip
        call plnum
        db 5
        dw s_ay_a,err_a
        call plnum
        db 6
        dw s_ay_b,err_b
        jp two_results

; C = puerto de seleccion/lectura del chip (el de datos es C AND 7Fh),
; HL = contador de errores
ay_chip:
        ld (ay_err),hl
        ld hl,ay_regs           ; guarda los registros que se van a tocar
        ld de,ay_save
        ld b,AY_NREG
ac1:    ld a,(hl)
        out (c),a               ; selecciona
        in a,(c)                ; lee
        ld (de),a
        inc hl
        inc de
        djnz ac1

        ld hl,ay_regs
        ld b,AY_NREG
ac2:    push bc
        push hl
        ld e,(hl)               ; E = registro
        ld hl,ay_vals
        ld b,AY_NVAL
ac3:    ld a,e
        out (c),a               ; selecciona
        ld d,(hl)               ; D = valor
        ld a,c
        and 7Fh
        push bc
        ld c,a
        ld a,d
        out (c),a               ; escribe
        pop bc
        ld a,e
        out (c),a
        in a,(c)                ; relee
        cp d
        jr z,ac4
        push hl
        ld hl,(ay_err)
        call inc16
        pop hl
ac4:    inc hl
        djnz ac3
        pop hl
        pop bc
        inc hl
        djnz ac2

        ld hl,ay_regs           ; restaura
        ld de,ay_save
        ld b,AY_NREG
ac5:    ld a,(hl)
        out (c),a
        ld a,c
        and 7Fh
        push bc
        ld c,a
        ld a,(de)
        out (c),a
        pop bc
        inc hl
        inc de
        djnz ac5
        ret

ay_regs: db 0, 2, 4, 11, 12
ay_vals: db 00h, 0FFh, 55h, 0AAh, 01h, 02h, 04h, 08h, 10h, 20h, 40h, 80h

; =============================================================
; Pruebas interactivas: el programa pinta o suena algo y el usuario
; confirma con Y/N (cada N cuenta como fallo en err_a). Necesitan SLOW
; (pantalla visible mientras esperan tecla, y se temporizan con FRAMES).
; =============================================================

; B = fila, HL = pregunta -> "pregunta (Y/N) YES/NO". Z = si. Un NO
; suma 1 en err_a.
ask_yn:
        call line
        ld hl,s_yn
        call out_str
        call key_release
yn1:    ld a,0DFh               ; semifila P O I U Y
        in a,(0FEh)
        bit 4,a
        jr z,yn_yes
        ld a,7Fh                ; semifila SPACE . M N B
        in a,(0FEh)
        bit 3,a
        jr nz,yn1
        ld hl,s_no
        call out_str
        ld hl,err_a
        call inc16
        call key_release
        or 1                    ; NZ
        ret
yn_yes: ld hl,s_yes
        call out_str
        call key_release
        xor a                   ; Z
        ret

; Espera B cuadros (FRAMES lo decrementa la ROM en SLOW). Pisa A, BC.
wait_nf:
wn1:    ld a,(FRAMES)
        ld c,a
wn2:    ld a,(FRAMES)
        cp c
        jr z,wn2
        djnz wn1
        ret

; Comun: pone err_a a 0; Z si NO estamos en SLOW (y avisa en la fila 2)
ia_start:
        ld hl,0
        ld (err_a),hl
        ld a,(CDFLAG)
        and 80h
        ret nz
        call pline
        db 2
        dw s_ia_slow
        xor a
        ret

; Comun: resultado en la fila 19 y BC = err_a
ia_end:
        ld hl,(err_a)
        ld b,19
        call line_result
        ld bc,(err_a)
        ret

; Modo 32K (bloques 6/7 espejo de 2/3) -> Z y aviso
ia_mirror:
        ld a,(saved_map+2)
        ld c,a
        ld a,(saved_map+6)
        cp c
        jr z,iam1
        ld a,(saved_map+3)
        ld c,a
        ld a,(saved_map+7)
        cp c
        ret nz
iam1:   call pline
        db 2
        dw s_m67_mirror
        xor a
        ret

; =============================================================
; Test 18: sprites por hardware (USR 20534)
; 32 sprites (caja de 8x8) en una rejilla de 8x4; despues se desplazan 40
; pixeles a la derecha y vuelven, y al final se desactivan.
; =============================================================
SPR_SEL  equ 2100
SPR_EN   equ 2101
SPR_XL   equ 2102
SPR_XH   equ 2103
SPR_Y    equ 2104
SPR_COL  equ 2105
SPR_DATA equ 2113
SPR_MASK equ 2121

spr_test:
        ld hl,s_spr_title
        call title
        call ia_start
        jp z,ia_noslow
        call pline
        db 2
        dw s_spr_d1
        ld hl,0                 ; desplazamiento de la animacion
        ld (spr_dx),hl
        ld d,0                  ; D = sprite
sx1:    ld a,d
        ld (SPR_SEL),a
        ld a,1
        ld (SPR_EN),a
        call spr_setxy
        ld hl,SPR_COL           ; 8 filas: color, datos, mascara
        ld b,8
sx2:    ld a,d
        and 7
        rlca
        rlca
        rlca
        rlca
        or 7                    ; tinta = D AND 7, papel blanco
        ld (hl),a
        inc hl
        djnz sx2
        ld hl,SPR_DATA
        ld (hl),0FFh
        inc hl
        ld b,6
sx3:    ld (hl),81h
        inc hl
        djnz sx3
        ld (hl),0FFh
        ld hl,SPR_MASK
        ld b,8
sx4:    ld (hl),0FFh
        inc hl
        djnz sx4
        inc d
        ld a,d
        cp 32
        jr nz,sx1

        ld b,15
        ld hl,s_spr_q1
        call ask_yn

        ld e,40                 ; 40 pasos a la derecha
sx5:    ld hl,(spr_dx)
        inc hl
        ld (spr_dx),hl
        call spr_moveall
        dec e
        jr nz,sx5
        ld e,40                 ; y 40 de vuelta
sx6:    ld hl,(spr_dx)
        dec hl
        ld (spr_dx),hl
        call spr_moveall
        dec e
        jr nz,sx6
        ld b,16
        ld hl,s_spr_q2
        call ask_yn

        ld d,0                  ; desactiva los 32
sx7:    ld a,d
        ld (SPR_SEL),a
        xor a
        ld (SPR_EN),a
        inc d
        ld a,d
        cp 32
        jr nz,sx7
        ld b,17
        ld hl,s_spr_q3
        call ask_yn
        jp ia_end

; Mueve los 32 sprites a su sitio + spr_dx y espera un cuadro
spr_moveall:
        push de
        ld d,0
sm2:    ld a,d
        ld (SPR_SEL),a
        call spr_setxy
        inc d
        ld a,d
        cp 32
        jr nz,sm2
        ld b,1
        call wait_nf
        pop de
        ret

; D = sprite (ya seleccionado): X = 80 + (D AND 7)*28 + spr_dx,
; Y = 72 + (D/8)*24 (coordenadas de sprite: 32 = pixel 0). Pisa A, BC, HL.
spr_setxy:
        ld a,d
        and 7
        ld c,a
        add a,a
        add a,a
        add a,a                 ; *8
        ld b,a
        ld a,c
        add a,a
        add a,a                 ; *4
        add a,b                 ; *12
        ld b,a
        add a,a                 ; *24
        add a,c                 ; *25
        add a,c                 ; *26
        add a,c                 ; *27
        add a,c                 ; *28
        ld l,a
        ld h,0
        ld bc,80
        add hl,bc
        ld bc,(spr_dx)
        add hl,bc
        ld a,l
        ld (SPR_XL),a
        ld a,h
        and 1
        ld (SPR_XH),a
        ld a,d
        rrca
        rrca
        rrca
        and 3
        ld b,a                  ; fila de la rejilla 0-3
        add a,a
        add a,b                 ; *3
        add a,a
        add a,a
        add a,a                 ; *24
        add a,72
        ld (SPR_Y),a
        ret

; =============================================================
; Test 19: patron de borde (USR 20537)
; =============================================================
bd_test:
        ld hl,s_bd_title
        call title
        call ia_start
        jp z,ia_noslow
        call pline
        db 2
        dw s_bd_d1
        ld hl,2048              ; tablero de ajedrez
        ld b,4
bd1:    ld (hl),0AAh
        inc hl
        ld (hl),55h
        inc hl
        djnz bd1
        xor a
        ld (2046),a             ; tinta del patron
        ld a,170                ; el patron es solo de Superfast (texto:
        ld (POKE_SF),a          ; la misma pantalla)
        ld (2047),a
        ld b,5
        ld hl,s_bd_q1
        call ask_yn
        ld a,85
        ld (2047),a
        ld b,6
        ld hl,s_bd_q2
        call ask_yn
        ld a,85
        ld (POKE_SF),a
        jp ia_end

; =============================================================
; Test 20: Chroma81 modo 0 (USR 20540)
; Tabla de color por caracter en $C000 (8 bytes por codigo, uno por linea
; de pixeles; nibble alto papel, bajo tinta). Todo papel blanco y tinta
; negra salvo las letras A-H, con papel 0-7 (negro, azul, rojo, magenta,
; verde, cian, amarillo, blanco). Se pintan 8 filas con esas letras y una
; novena con la I, que tiene un papel distinto en cada linea de pixeles.
; Escribe en $C000-$C3FF (pagina del bloque 6): no en modo 32K.
; =============================================================
ch_test:
        ld hl,s_ch_title
        call title
        call ia_start
        jp z,ia_noslow
        call save_map
        call ia_mirror
        jp z,ia_noslow
        call pline
        db 2
        dw s_ch_d1
        ld hl,0C000h            ; todo: papel blanco, tinta negra
        ld bc,1024
ch1:    ld (hl),70h
        inc hl
        dec bc
        ld a,b
        or c
        jr nz,ch1
        ld d,0                  ; letras A-H: papel D
ch2:    ld a,26h                ; codigo de la letra A
        add a,d
        ld l,a
        ld h,0
        add hl,hl
        add hl,hl
        add hl,hl               ; *8
        ld bc,0C000h
        add hl,bc
        ld a,d
        rlca
        rlca
        rlca
        rlca
        ld e,a
        ld a,d                  ; tinta: blanca sobre los colores oscuros
        cp 4
        ld a,e
        jr nc,ch3
        or 7
ch3:    ld b,8
ch4:    ld (hl),a
        inc hl
        djnz ch4
        push de                 ; fila 4+D: 20 veces la letra
        ld a,d
        add a,4
        ld b,a
        ld c,0
        call set_at
        pop de
        ld b,20
ch5:    ld a,26h
        add a,d
        call out_char
        djnz ch5
        inc d
        ld a,d
        cp 8
        jr nz,ch2
        ld hl,0C000h+2Eh*8      ; letra I: papel = linea de pixeles (0-7)
        ld d,0
ch6:    ld a,d
        rlca
        rlca
        rlca
        rlca
        ld (hl),a
        inc hl
        inc d
        ld a,d
        cp 8
        jr nz,ch6
        ld bc,12*256  ; fila 12: 20 veces la I
        call set_at
        ld b,20
ch7:    ld a,2Eh
        call out_char
        djnz ch7
        ld bc,7FEFh             ; color on, modo 0, borde blanco
        ld a,27h
        out (c),a
        call pline
        db 14
        dw s_ch_q0
        ld b,15
        ld hl,s_ch_q1
        call ask_yn
        ld b,16
        ld hl,s_ch_q2
        call ask_yn
        ld bc,7FEFh             ; color off
        ld a,0Ch
        out (c),a
        jp ia_end

; =============================================================
; Test 21: modos Superfast (USR 20543)
;   texto: la misma pantalla (D_FILE) generada por la FPGA;
;   HiRes nativo: bitmap lineal en $E000 (arriba tablero, abajo barras);
;   Spectrum: bitmap con el orden del Spectrum en $E000 y atributos en
;   $F800: 8 bandas de color y una X (dos diagonales).
; Los bitmaps van en la pagina del bloque 7: no en modo 32K.
; =============================================================
sf_test:
        ld hl,s_sf_title
        call title
        call ia_start
        jp z,ia_noslow
        call save_map
        call ia_mirror
        jp z,ia_noslow
        ld a,170                ; --- texto ---
        ld (POKE_SF),a
        ld b,3
        ld hl,s_sf_q1
        call ask_yn
        ld a,85
        ld (POKE_SF),a

        ld hl,0E000h            ; --- HiRes nativo ---
        ld c,192
sf1:    ld a,c
        cp 97
        ld a,0F0h               ; mitad de abajo: barras verticales
        jr c,sf2
        ld a,c                  ; mitad de arriba: tablero
        and 1
        ld a,0AAh
        jr z,sf2
        ld a,55h
sf2:    ld b,32
sf3:    ld (hl),a
        inc hl
        djnz sf3
        dec c
        jr nz,sf1
        xor a
        ld (2043),a             ; HFILE = $E000
        ld a,0E0h
        ld (2044),a
        call pline
        db 5
        dw s_sf_key
        call key_any
        ld a,171
        ld (POKE_SF),a
        call key_any
        ld a,85
        ld (POKE_SF),a
        ld b,6
        ld hl,s_sf_q2
        call ask_yn

        ld hl,0E000h            ; --- Spectrum ---
        ld bc,6144
sf4:    ld (hl),0
        inc hl
        dec bc
        ld a,b
        or c
        jr nz,sf4
        ld hl,0F800h            ; atributos: banda D (3 filas) papel D
        ld d,0
sf5:    ld a,d
        rlca
        rlca
        rlca                    ; papel = D
        ld e,a
        ld a,d
        cp 4
        ld a,e
        jr nc,sf6
        or 7                    ; tinta blanca sobre los oscuros
sf6:    ld b,96                 ; 3 filas x 32
sf7:    ld (hl),a
        inc hl
        djnz sf7
        inc d
        ld a,d
        cp 8
        jr nz,sf5
        ld e,0                  ; la X: (y,y) y (191-y,y)
sf8:    ld a,e
        ld d,a
        call sp_plot
        ld a,191
        sub e
        ld d,a
        call sp_plot
        inc e
        ld a,e
        cp 192
        jr nz,sf8
        call pline
        db 8
        dw s_sf_key
        call key_any
        ld bc,7FEFh             ; el color del modo Spectrum pide Chroma: se
        ld a,27h                ; activa justo al entrar (antes pintaria la
        out (c),a               ; pantalla de texto con la tabla de $C000)
        ld a,172
        ld (POKE_SF),a
        call key_any
        ld a,85
        ld (POKE_SF),a
        ld bc,7FEFh
        ld a,0Ch
        out (c),a
        ld b,9
        ld hl,s_sf_q3
        call ask_yn
        jp ia_end

; E = y (0-191), D = x (0-255): pone el pixel en el bitmap Spectrum de $E000
sp_plot:
        ld a,e
        and 0C0h
        rrca
        rrca
        rrca                    ; tercio -> bits 3-4
        ld h,a
        ld a,e
        and 7                   ; linea dentro del caracter -> bits 0-2
        or h
        or 0E0h
        ld h,a
        ld a,e
        and 38h
        rlca
        rlca                    ; fila de caracter -> bits 5-7
        ld l,a
        ld a,d
        rrca
        rrca
        rrca
        and 1Fh                 ; columna
        or l
        ld l,a
        ld a,d
        and 7
        ld b,a
        ld a,80h
        jr z,spp2
spp1:   rrca
        djnz spp1
spp2:   or (hl)
        ld (hl),a
        ret

; =============================================================
; Test 22: teclado y joystick (USR 20546)
; Matriz de las 40 teclas en vivo (la pulsada en video inverso; el
; joystick se ve como las teclas que tiene asignadas) y los bits 6 (50/60
; Hz) y 7 (entrada de cinta) del puerto FE. Se sale con SHIFT+SPACE.
; =============================================================
KB_ROW  equ 5                   ; primera fila de la rejilla
KB_COL  equ 6                   ; primera columna

kb_test:
        ld hl,s_kb_title
        call title
        call ia_start
        jp z,ia_noslow
        call plines
        db 2
        dw s_kb_d1
        db 3
        dw s_kb_d2
        db 10
        dw s_kb_leg
        db 0FFh
kb1:    ld d,0                  ; D = semifila 0-7
        ld a,0FEh               ; A = byte alto del puerto
kb2:    push af
        in a,(0FEh)
        ld (kb_last),a
        ld e,a                  ; E = bits de las 5 teclas
        ld a,d                  ; salida: SHIFT (semifila 0) y SPACE (7)
        or a
        jr nz,kb2a
        ld a,e
        ld (kb_shift),a
kb2a:   ld b,0                  ; B = bit 0-4
kb3:    push bc
        push de
        call kb_draw
        pop de
        pop bc
        inc b
        ld a,b
        cp 5
        jr nz,kb3
        pop af
        rlca                    ; siguiente semifila
        inc d
        bit 3,d
        jr z,kb2
        call pline
        db 12
        dw s_kb_fe6  ; bits 6 y 7 del puerto FE
        ld a,(kb_last)
        rlca
        rlca
        and 1                   ; bit 6
        add a,Z_0
        call out_char
        ld hl,s_kb_fe7
        call out_str
        ld a,(kb_last)
        rlca
        and 1                   ; bit 7
        add a,Z_0
        call out_char
        ld a,(kb_shift)         ; SHIFT (bit 0 de la semifila 0) ...
        and 1
        jp nz,kb1
        ld a,(kb_last)          ; ... y SPACE (bit 0 de la 7, la ultima)
        and 1
        jp nz,kb1
        call key_release
        ld b,15
        ld hl,s_kb_q1
        call ask_yn
        jp ia_end

; D = semifila, B = bit (0-4), E = lectura -> pinta esa tecla.
; Pisa A, BC, E, HL.
kb_draw:
        ld a,e                  ; bit B de E al carry (B+1 rotaciones)
        ld c,b
        inc c
kd1:    rrca
        dec c
        jr nz,kd1
        ld c,80h                ; pulsada (bit a 0): video inverso
        jr nc,kd2
        ld c,0
kd2:    ld a,d                  ; caracter: kb_chars[D*5+B]
        add a,a
        add a,a
        add a,d
        add a,b
        ld hl,kb_chars
        call add_hl_a
        ld a,(hl)
        or c
        ld e,a                  ; E = caracter a pintar
        ld hl,kb_rows           ; fila de pantalla
        ld a,d
        call add_hl_a
        ld a,(hl)
        add a,KB_ROW
        ld h,a
        ld a,d                  ; semifilas 0-3: columna = bit;
        cp 4                    ; 4-7: columna = 9 - bit
        ld a,b
        jr c,kd3
        ld a,9
        sub b
kd3:    add a,a
        add a,KB_COL
        ld c,a
        ld b,h
        call set_at
        ld a,e
        jp out_char

; HL = HL + A
add_hl_a:
        add a,l
        ld l,a
        ret nc
        inc h
        ret

; Fila de pantalla (0-3) de cada semifila: FE FD FB F7 EF DF BF 7F
kb_rows:  db 3, 2, 1, 0, 0, 1, 2, 3
; Caracter de cada tecla (codigos ZX81), 5 por semifila, bit 0 primero.
; SHIFT = *, ENTER = >, SPACE = -
kb_chars: db 17h, 3Fh, 3Dh, 28h, 3Bh         ; SHIFT Z X C V
          db 26h, 38h, 29h, 2Bh, 2Ch         ; A S D F G
          db 36h, 3Ch, 2Ah, 37h, 39h         ; Q W E R T
          db 1Dh, 1Eh, 1Fh, 20h, 21h         ; 1 2 3 4 5
          db 1Ch, 25h, 24h, 23h, 22h         ; 0 9 8 7 6
          db 35h, 34h, 2Eh, 3Ah, 3Eh         ; P O I U Y
          db 12h, 31h, 30h, 2Fh, 2Dh         ; ENTER L K J H
          db 16h, 1Bh, 32h, 33h, 27h         ; SPACE . M N B

; =============================================================
; Test 23: sonido (USR 20549)
; 6 tonos cada vez mas agudos: canales A, B y C del chip A ($CF/$0F) y
; luego del chip B ($C6/$06), ~0,4 s cada uno. Despues, SAY "HELLO".
; Para escribir se usan los puertos documentados (en SLOW un OUT a puerto
; par solo enciende la NMI, que ya lo esta).
; =============================================================
so_test:
        ld hl,s_so_title
        call title
        call ia_start
        jp z,ia_noslow
        call pline
        db 2
        dw s_so_d1
        ld hl,so_periods
        ld c,0CFh               ; chip A
        call so_chip
        ld c,0C6h               ; chip B (puertos documentados $C6/$06)
        call so_chip
        ld b,5
        ld hl,s_so_q1
        call ask_yn
        ld hl,s_hello           ; SAY (comando 23, texto en codigo ZX81)
        ld e,1
        ld a,23
        call cmd_name
        jr nc,so1
        ld hl,err_a             ; el MCU no contesta
        call inc16
so1:    ld b,6
        ld hl,s_so_q2
        call ask_yn
        jp ia_end

; C = puerto de seleccion del chip, HL = 3 periodos -> 3 tonos
so_chip:
        ld b,0                  ; B = canal 0-2
sc_1:   push bc
        ld d,7                  ; mezclador: solo el tono de este canal
        ld a,1
        inc b
sc_2:   dec b
        jr z,sc_3
        rlca
        jr sc_2
sc_3:   cpl
        and 3Fh
        ld e,a
        pop bc
        push bc
        call ay_write
        ld a,b                  ; periodo: registros 2*canal y 2*canal+1
        add a,a
        ld d,a
        ld e,(hl)
        inc hl
        call ay_write
        inc d
        ld e,(hl)
        inc hl
        call ay_write
        ld a,b                  ; volumen 15
        add a,8
        ld d,a
        ld e,15
        call ay_write
        push hl
        push bc
        push de
        ld b,20                 ; ~0,4 s
        call wait_nf
        pop de
        pop bc
        pop hl
        ld e,0                  ; volumen 0
        call ay_write
        pop bc
        inc b
        ld a,b
        cp 3
        jr nz,sc_1
        ld d,7                  ; mezclador: todo apagado
        ld e,3Fh
        jp ay_write

; C = puerto de seleccion, D = registro, E = valor; el de datos es
; C AND 3Fh ($CF -> $0F, $C6 -> $06). Conserva todo menos A.
ay_write:
        out (c),d
        push bc
        ld a,c
        and 3Fh
        ld c,a
        out (c),e
        pop bc
        ret

; =============================================================
; Pruebas de video 4.x (interactivas, en SLOW). Varias montan su propia
; pantalla en OVR_SCR (bloque 4) y la muestran con la direccion de
; pantalla alternativa (POKE 2096-2098): el BASIC y la ROM siguen con su
; D_FILE, que en SLOW se ejecuta y no admite codigos con el bit 6 a 1 ni
; otro ancho de fila. Machacan el contenido de los bloques 4 y 5.
; =============================================================
OVR_SCR  equ 8000h
CMD_128C equ 1Bh        ; SEL_128CHARS (LOAD *128C)
CMD_64C  equ 1Ch        ; SEL_64CHARS (LOAD *64C)
CMD_256C equ 41h        ; SEL_256CHARS (LOAD *256C)

; =============================================================
; Test 26: juegos de caracteres 128C y 256C (USR 20558)
;   128C (video nativo, I = $3C): la tabla de $3C00-$3FFF viene
;   precargada con los caracteres de la ROM, asi que la pantalla tiene
;   que verse igual; al darle la vuelta a todos los glifos, toda la
;   pantalla sale boca abajo.
;   256C (Superfast texto, I = $38): los 256 codigos en 16 filas de 16,
;   en la pantalla alternativa. Se da la vuelta a los grupos 1 ($3A00,
;   codigos 64-127) y 3 ($3E00, 192-255): las cuatro bandas de 4 filas
;   salen normal, boca abajo, inversa e inversa boca abajo.
; Al final deja las tablas como estaban y restaura I y el modo.
; =============================================================
cs_test:
        ld hl,s_cs_title
        call title
        call ia_start
        jp z,ia_noslow
        ld a,i
        ld (cs_i),a
        ld a,85
        ld (2058),a             ; WRX apagado: 8-16K es la tabla
        ld bc,3*256+0           ; filas 3-6: codigos 0-63 y 128-191
        call cs_row
        ld bc,4*256+32
        call cs_row
        ld bc,5*256+128
        call cs_row
        ld bc,6*256+160
        call cs_row
        call pline
        db 8
        dw s_cs_d1
        ld a,CMD_128C
        call mcu_cmd
        ld a,3Ch
        ld i,a
        ld b,10
        ld hl,s_cs_q1
        call ask_yn
        ld hl,3C00h
        ld b,128
        call flip_gl
        ld b,11
        ld hl,s_cs_q2
        call ask_yn
        ld hl,3C00h
        ld b,128
        call flip_gl
        ld a,CMD_64C
        call mcu_cmd
        ld a,1Eh
        ld i,a

        ld hl,OVR_SCR           ; --- 256C ---
        ld bc,1+24*33
        xor a
        call fill_mem
        ld hl,OVR_SCR+1+4*33    ; filas 4-19, 16 codigos en columnas pares
        xor a
cs2:    ld b,16
cs3:    ld (hl),a
        inc hl
        inc hl
        inc a
        djnz cs3
        inc hl                  ; 32 + el byte de relleno = 33
        or a
        jr nz,cs2               ; hasta que A da la vuelta (256 codigos)
        call cs_flip13
        call plines
        db 12
        dw s_cs_d2
        db 13
        dw s_cs_d3
        db 0FFh
        ld a,CMD_256C
        call mcu_cmd
        ld a,38h
        ld i,a
        call ovr_on
        ld bc,14*256+170
        call sf_show
        ld a,CMD_64C
        call mcu_cmd
        ld a,1Eh
        ld i,a
        ld b,15
        ld hl,s_cs_q3
        call ask_yn
        call cs_flip13

        ld a,(cs_i)             ; el modo de caracteres de antes
        cp 3Ch
        ld b,CMD_128C
        jr z,cs4
        cp 38h
        ld b,CMD_256C
        jr z,cs4
        ld b,CMD_64C
cs4:    ld a,b
        call mcu_cmd
        ld a,(cs_i)
        ld i,a
        jp ia_end

; B = fila, C = primer codigo -> 32 codigos seguidos en la fila
cs_row:
        ld a,c
        push af
        ld c,0
        call set_at
        pop af
        ld e,32
csr1:   call out_char
        inc a
        dec e
        jr nz,csr1
        ret

; Da la vuelta a los grupos 1 y 3 de la tabla de 256 caracteres
cs_flip13:
        ld hl,3A00h
        ld b,64
        call flip_gl
        ld hl,3E00h
        ld b,64
        ; sigue en flip_gl

; HL = tabla, B = numero de glifos (0 = 256): invierte el orden de las 8
; lineas de cada uno (boca abajo). Hacerlo dos veces lo deja como estaba.
flip_gl:
        push bc
        push hl
        ld d,h
        ld e,l
        ld bc,7
        add hl,bc
        ld b,4
flg1:   ld a,(de)
        ld c,(hl)
        ld (hl),a
        ld a,c
        ld (de),a
        inc de
        dec hl
        djnz flg1
        pop hl
        ld bc,8
        add hl,bc
        pop bc
        djnz flip_gl
        ret

; HL = direccion, BC = bytes (> 0), A = valor
fill_mem:
        ld e,a
fm1:    ld (hl),e
        inc hl
        dec bc
        ld a,b
        or c
        jr nz,fm1
        ret

; Pantalla alternativa (POKE 2096/2097 = OVR_SCR, POKE 2098,170). POKE
; 2045,85 la apaga sola.
ovr_on:
        ld hl,OVR_SCR
        ld (2096),hl
        ld a,170
        ld (2098),a
        ret

; B = fila del aviso, C = modo (POKE 2045): "tecla: ver, otra: volver";
; muestra el modo hasta la segunda tecla y vuelve a video nativo.
sf_show:
        push bc
        ld hl,s_sf_key
        call line
        call key_any
        pop bc
        ld a,c
        ld (POKE_SF),a
        call key_any
        ld a,85
        ld (POKE_SF),a
        ret

; =============================================================
; Test 27: 70 y 80 columnas (USR 20561)
; Superfast texto ancho (POKE 2045,173 = 70 columnas de 8 pixeles, 174 =
; 80 de 7) sobre la pantalla alternativa: filas de columnas+1 bytes. En
; cada fila, un bloque en la primera y la ultima columna; arriba y abajo,
; una regla con las unidades y las decenas de la columna.
; =============================================================
wd_test:
        ld hl,s_wd_title
        call title
        call ia_start
        jp z,ia_noslow
        call pline
        db 2
        dw s_wd_d1
        ld a,70
        call wd_fill
        call ovr_on
        ld bc,4*256+173
        call sf_show
        ld b,5
        ld hl,s_wd_q1
        call ask_yn
        ld a,80
        call wd_fill
        call ovr_on             ; POKE 2045,85 la habia apagado
        ld bc,7*256+174
        call sf_show
        ld b,8
        ld hl,s_wd_q2
        call ask_yn
        jp ia_end

; A = columnas (70 u 80) -> pantalla de 24 filas en OVR_SCR
wd_fill:
        ld (wd_w),a
        ld hl,OVR_SCR
        ld (hl),0               ; byte inicial (el hardware no lo usa)
        inc hl
        ld d,0                  ; D = fila
wdf1:   ld e,0                  ; E = columna
        ld bc,0                 ; B = decenas, C = unidades de E
wdf2:   ld a,e
        or a
        jr z,wdf_ed             ; primera columna
        ld a,(wd_w)
        dec a
        cp e
        jr z,wdf_ed             ; ultima columna
        ld a,d
        or a
        jr z,wdf_un             ; filas 0 y 23: unidades
        cp 23
        jr z,wdf_un
        cp 1
        jr z,wdf_te             ; filas 1 y 22: decenas
        cp 22
        jr z,wdf_te
        xor a
        jr wdf_pt
wdf_ed: ld a,Z_INV              ; espacio inverso: un bloque
        jr wdf_pt
wdf_un: ld a,c
        add a,Z_0
        jr wdf_pt
wdf_te: ld a,b
        add a,Z_0
wdf_pt: ld (hl),a
        inc hl
        inc c
        ld a,c
        cp 10
        jr nz,wdf3
        ld c,0
        inc b
wdf3:   inc e
        ld a,(wd_w)
        cp e
        jr nz,wdf2
        ld (hl),0               ; byte de relleno de la fila
        inc hl
        inc d
        ld a,d
        cp 24
        jr nz,wdf1
        ret

; =============================================================
; Test 28: scroll fino (USR 20564)
; Barras verticales (un bloque cada 4 columnas, tambien en la columna 32,
; la que asoma al desplazar) en la pantalla alternativa, en Superfast
; texto. POKE 2091-2093 = 55h: solo las filas pares se desplazan. POKE
; 2090 va y viene de 0 a 7 cada 2 cuadros hasta que se pulsa una tecla.
; =============================================================
fs_test:
        ld hl,s_fs_title
        call title
        call ia_start
        jp z,ia_noslow
        call pline
        db 2
        dw s_fs_d1
        ld hl,OVR_SCR
        ld (hl),0
        inc hl
        ld d,24
fs1:    ld e,0                  ; 33 columnas por fila (0-32)
fs2:    ld a,e
        and 3
        ld a,0
        jr nz,fs3
        ld a,Z_INV
fs3:    ld (hl),a
        inc hl
        inc e
        ld a,e
        cp 33
        jr nz,fs2
        dec d
        jr nz,fs1
        ld a,55h                ; filas pares
        ld (2091),a
        ld (2092),a
        ld (2093),a
        xor a
        ld (2090),a
        ld b,4
        ld hl,s_sf_key
        call line
        call key_any
        call ovr_on
        ld a,170
        ld (POKE_SF),a
        ld hl,fs_tab
fs4:    ld a,(hl)
        cp 0FFh
        jr nz,fs5
        ld hl,fs_tab
        ld a,(hl)
fs5:    ld (2090),a
        inc hl
        push hl
        ld b,2
        call wait_nf
        pop hl
        xor a                   ; hasta que se pulse una tecla
        in a,(0FEh)
        and 1Fh
        cp 1Fh
        jr z,fs4
        call key_release
        ld a,85
        ld (POKE_SF),a
        xor a
        ld (2090),a
        ld a,0FFh               ; todas las filas, como tras un reset
        ld (2091),a
        ld (2092),a
        ld (2093),a
        ld b,5
        ld hl,s_fs_q1
        call ask_yn
        jp ia_end

fs_tab: db 0,1,2,3,4,5,6,7,6,5,4,3,2,1,0FFh

; =============================================================
; Test 29: Chroma81 modo 1, color por posicion (USR 20567)
; Superfast texto con color en modo 1 (OUT $7FEF,37h). Primero con la
; tabla de siempre, en D_FILE+$8000 (bloque 6): papel = columna/4, 8
; barras verticales. Despues con la tabla alternativa (POKE 2059-2061)
; en $A000: papel = fila/3, 8 barras horizontales. Tinta blanca sobre los
; papeles oscuros y negra sobre los claros. No en modo 32K.
; =============================================================
c1_test:
        ld hl,s_c1_title
        call title
        call ia_start
        jp z,ia_noslow
        call save_map
        call ia_mirror
        jp z,ia_noslow
        call pline
        db 2
        dw s_c1_d1
        ld hl,(DFILE)           ; tabla de siempre: D_FILE con el bit 15
        set 7,h
        call c1_vert
        ld hl,0A000h
        call c1_horz
        ld a,170
        ld (POKE_SF),a
        ld bc,7FEFh             ; color on, modo 1, borde blanco
        ld a,37h
        out (c),a
        ld b,4
        ld hl,s_c1_q1
        call ask_yn
        ld hl,0A000h            ; tabla alternativa
        ld (2059),hl
        ld a,170
        ld (2061),a
        ld b,5
        ld hl,s_c1_q2
        call ask_yn
        ld a,85
        ld (2061),a
        ld bc,7FEFh             ; color off
        ld a,0Ch
        out (c),a
        ld a,85
        ld (POKE_SF),a
        jp ia_end

; HL = tabla (misma geometria que la pantalla: 1 byte inicial y 24 filas
; de 33) -> papel = columna/4
c1_vert:
        ld (hl),0
        inc hl
        ld d,24
c1v1:   ld e,0
c1v2:   ld a,e
        rrca
        rrca
        and 7
        call c1_attr
        ld (hl),a
        inc hl
        inc e
        ld a,e
        cp 33
        jr nz,c1v2
        dec d
        jr nz,c1v1
        ret

; HL = tabla -> papel = fila/3
c1_horz:
        ld (hl),0
        inc hl
        ld d,0
        ld b,8
c1h1:   push bc
        ld a,d
        call c1_attr
        ld b,3*33
c1h2:   ld (hl),a
        inc hl
        djnz c1h2
        pop bc
        inc d
        djnz c1h1
        ret

; A = papel (0-7) -> A = atributo (papel arriba, tinta abajo). Conserva BC.
c1_attr:
        push bc
        ld b,a
        rlca
        rlca
        rlca
        rlca
        ld c,a
        ld a,b
        cp 4
        ld a,c
        jr nc,c1a1
        or 7                    ; tinta blanca sobre negro/azul/rojo/magenta
c1a1:   pop bc
        ret

; =============================================================
; Test 30: doble buffer (USR 20570)
; HiRes nativo con HFILE en $8000 (bloque 4). Dos partes:
;   AUTO (POKE 2057,168+5): la FPGA copia el bloque HFILE (4) al espejo
;   del front (5) en cada VSYNC. Tablero en el 4; se llena de ruido la
;   SRAM del 5 (no tiene que verse: la mascara protege el espejo del
;   front) y se pintan barras en el 4, que van apareciendo segun se
;   dibujan (cada cuadro es una foto del bloque 4 en ese momento).
;   MANUAL (POKE 2057,200+B): no hay copia; se ve el espejo del front.
;   Tablero en el 4 como front; barras en el 5, que no se ve; ruido en
;   el 4, que no tiene que verse; al pasar el front al 5, las barras
;   tienen que aparecer de golpe.
; =============================================================
db_test:
        ld hl,s_db_title
        call title
        call ia_start
        jp z,ia_noslow
        call plines
        db 2
        dw s_db_d1
        db 3
        dw s_db_d2
        db 4
        dw s_db_d3
        db 0FFh
        ld hl,8000h
        ld (2043),hl            ; HFILE = $8000 (bloque 4)
        call db_checker

        ld b,6                  ; --- AUTO ---
        ld hl,s_sf_key
        call line
        call key_any
        ld a,171                ; HiRes nativo
        ld (POKE_SF),a
        ld b,50
        call wait_nf
        ld a,168+5              ; AUTO, front = bloque 5
        ld (2057),a
        ld b,25
        call wait_nf
        ld de,0A000h            ; ruido en la SRAM del bloque 5 (front)
        call db_noise
        ld b,50
        call wait_nf
        ld hl,8000h             ; barras en el bloque 4 (HFILE)
        call db_stripes
        call key_any
        ld a,85
        ld (2057),a
        ld (POKE_SF),a
        ld b,7
        ld hl,s_db_q1
        call ask_yn

        call db_checker         ; --- MANUAL --- (doble buffer apagado:
        ld b,9                  ; el espejo del bloque 4 se actualiza)
        ld hl,s_sf_key
        call line
        call key_any
        ld a,200+4              ; MANUAL, front = bloque 4 (tablero)
        ld (2057),a
        ld a,171
        ld (POKE_SF),a
        ld b,50
        call wait_nf
        ld hl,0A000h            ; barras en el bloque 5, que no se ve
        call db_stripes
        ld de,8000h             ; ruido en el bloque 4, el front: no se
        call db_noise           ; tiene que ver
        ld b,25
        call wait_nf
        ld a,200+5              ; front = bloque 5: las barras de golpe
        ld (2057),a
        call key_any
        ld a,85
        ld (2057),a
        ld (POKE_SF),a
        ld b,10
        ld hl,s_db_q2
        call ask_yn
        ld b,11
        ld hl,s_db_q3
        call ask_yn
        jp ia_end

; Tablero (AA/55 por lineas) en el bitmap HiRes de $8000
db_checker:
        ld hl,8000h
        ld c,192
dbc1:   ld a,c
        and 1
        ld a,0AAh
        jr z,dbc2
        ld a,55h
dbc2:   ld b,32
dbc3:   ld (hl),a
        inc hl
        djnz dbc3
        dec c
        jr nz,dbc1
        ret

; HL = bitmap -> barras verticales (6144 bytes de F0h)
db_stripes:
        ld bc,6144
        ld a,0F0h
        jp fill_mem

; DE = bitmap -> 6144 bytes de ruido
db_noise:
        ld bc,6144
dbn1:   push bc
        push de
        call xrnd
        pop de
        ld (de),a
        inc de
        pop bc
        dec bc
        ld a,b
        or c
        jr nz,dbn1
        ret

so_periods: dw 400, 330, 270, 220, 180, 145
s_ay_title: db "AY REGISTER",'S'+80h
s_ay_d1:   db "A: $CF/$0F  B: $C7/$0",'7'+80h
s_ay_d2:   db "REGS 0,2,4,11,12 X 12 VALUE",'S'+80h
s_ay_a:    db "CHIP A ERRORS:",' '+80h
s_ay_b:    db "CHIP B ERRORS:",' '+80h
s_yn:      db " Y/N",' '+80h
s_yes:     db "YE",'S'+80h
s_no:      db "N",'O'+80h
s_ia_slow: db "RUN IT IN SLOW MOD",'E'+80h
s_spr_title: db "SPRITE",'S'+80h
s_spr_d1:  db "32 HARDWARE SPRITES, 8X4 GRI",'D'+80h
s_spr_q1:  db "32 SQUARES IN 8X4",'?'+80h
s_spr_q2:  db "MOVED RIGHT AND BACK",'?'+80h
s_spr_q3:  db "ALL SQUARES GONE",'?'+80h
s_bd_title: db "BORDE",'R'+80h
s_bd_d1:   db "POKE 2046-2055, PATTERN ON/OF",'F'+80h
s_bd_q1:   db "CHECKER IN THE BORDER",'?'+80h
s_bd_q2:   db "PLAIN BORDER AGAIN",'?'+80h
s_ch_title: db "CHROMA8",'1'+80h
s_ch_d1:   db "MODE 0: COLOUR PER CHARACTE",'R'+80h
s_ch_q0:   db "BARS BLACK,BLUE,RED,MAGENTA",','+80h
s_ch_q1:   db "GREEN,CYAN,YELLOW,WHITE",'?'+80h
s_ch_q2:   db "I ROW: 8 THIN STRIPES",'?'+80h
s_sf_title: db "SUPERFAS",'T'+80h
s_sf_q1:   db "TEXT: SAME SCREEN",'?'+80h
s_sf_key:  db "KEY: SHOW, KEY AGAIN: BAC",'K'+80h
s_sf_q2:   db "HIRES: CHECKER+BARS",'?'+80h
s_sf_q3:   db "SPECTRUM: 8 BANDS + X",'?'+80h
s_kb_title: db "KEYBOAR",'D'+80h
s_kb_d1:   db "PRESSED KEYS IN INVERSE VIDE",'O'+80h
s_kb_d2:   db "SHIFT+SPACE TO EN",'D'+80h
s_kb_leg:  db "*=SHIFT  >=ENTER  -=SPAC",'E'+80h
s_kb_fe6:  db "FE BIT6:",' '+80h
s_kb_fe7:  db "  BIT7:",' '+80h
s_kb_q1:   db "ALL KEYS/JOYSTICK OK",'?'+80h
s_so_title: db "SOUN",'D'+80h
s_so_d1:   db "6 TONES (AY A THEN B), HELL",'O'+80h
s_so_q1:   db "6 RISING TONES",'?'+80h
s_so_q2:   db "HEARD HELLO",'?'+80h
s_hello:   db "HELLO",0
s_cs_title: db "CHARSET",'S'+80h
s_cs_d1:   db "128C: ROWS 3-6, TABLE $3C0",'0'+80h
s_cs_q1:   db "128C: LOOKS NORMAL",'?'+80h
s_cs_q2:   db "ALL UPSIDE DOWN",'?'+80h
s_cs_d2:   db "256C BANDS: NORMAL, UPSIDE-DOWN",','+80h
s_cs_d3:   db "INVERSE, INVERSE UPSIDE-DOW",'N'+80h
s_cs_q3:   db "4 BANDS AS DESCRIBED",'?'+80h
s_wd_title: db "70/80 COLUMN",'S'+80h
s_wd_d1:   db "RULER 0-9, SOLID LEFT/RIGHT EDG",'E'+80h
s_wd_q1:   db "70 COLUMNS: ALL VISIBLE",'?'+80h
s_wd_q2:   db "80 COLUMNS: ALL VISIBLE",'?'+80h
s_fs_title: db "FINE SCROL",'L'+80h
s_fs_d1:   db "STRIPES, EVEN ROWS SCROLL 0-7P",'X'+80h
s_fs_q1:   db "ONLY EVEN ROWS MOVED",'?'+80h
s_c1_title: db "CHROMA81 MODE ",'1'+80h
s_c1_d1:   db "MODE 1: COLOUR PER POSITIO",'N'+80h
s_c1_q1:   db "8 VERTICAL COLOUR BARS",'?'+80h
s_c1_q2:   db "NOW 8 HORIZONTAL BARS",'?'+80h
s_db_title: db "DOUBLE BUFFE",'R'+80h
s_db_d1:   db "AUTO (FRONT 5): STRIPES DRAW",'N'+80h
s_db_d2:   db "MANUAL: FRONT 4->5, AT ONC",'E'+80h
s_db_d3:   db "NOISE MUST NEVER SHO",'W'+80h
s_db_q1:   db "AUTO: CHECKER, STRIPES",'?'+80h
s_db_q2:   db "MANUAL: STRIPES AT ONCE",'?'+80h
s_db_q3:   db "NEVER ANY NOISE",'?'+80h
ay_err:     dw 0        ; AY: contador de errores del chip en curso
spr_dx:     dw 0        ; sprites: desplazamiento de la animacion
kb_last:    db 0        ; teclado: ultima semifila leida (7F)
kb_shift:   db 0        ; teclado: semifila FE (SHIFT)
cs_i:       db 0        ; juegos de caracteres: I al empezar
wd_w:       db 0        ; 70/80 columnas: ancho en curso

; -------------------------------------------------------------
; Buffers del modulo sin valor inicial (no van en el .bin).
; mbss_end tiene que quedar por debajo de $8000.
; -------------------------------------------------------------
mbss:
ay_save      equ mbss   ; AY: registros originales
mbss_end     equ ay_save+5
