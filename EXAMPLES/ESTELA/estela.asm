; =============================================================
; ESTELA.ASM -- matamarcianos vertical para el modo 160x100 a 256 colores
; del SD81 Booster. Plan: claude/planning/juego160.md
;
; FASE 7c: mejoras (tras el jefe de la 7b). Modo 175, anillo de 160x102 en $8000 con dos paneles
; laterales fijos de 16 pixeles, scroll vertical del nivel 1, la nave con sus
; disparos y las oleadas de enemigos del nivel, con sus disparos, colisiones,
; explosiones y el marcador, todo en sprites de formato B:
;
;   - NIVEL1.I16 (de la SD, carpeta actual): la paleta de 256 del nivel y el
;     tileset, 160x100 con 60 tiles de 16x16 (10 por fila), en $C000 tal cual
;     (TILEADR de LEVEL.INC). La fila 96 lleva el dibujo de los paneles
;     (PANEL_ADDR: +0..15 el izquierdo, +144..159 el derecho). Los bloques 6 y
;     7 se mapean a las paginas 6 y 7 (libres en los modos del SD81).
;   - NIVEL1.MAP (de la SD): cabecera, oleadas y mapa de 8 tiles de ancho
;     (formato en juego160.md 8.3), en LVLBUF = $2000: la pagina LVLPG de la
;     SRAM, mapeada en el bloque 1 mientras corre el juego (sin ROM: todo con
;     DI; hasta 8 KB de nivel). Al salir el bloque 1 vuelve a tener su pagina y
;     se reescribe sobre si mismo, para que la sombra de la FPGA de
;     $2000-$3FFF vuelva a tener lo de la ROM.
;   - Lineas del nivel: la fila r del mapa ocupa las lineas 16r..16r+15
;     contando desde abajo; la linea j del tile (0 = arriba) es la 16r+15-j.
;     Con s pixeles de scroll hechos, la fila k de la pantalla (0 = arriba)
;     es la linea 99+s-k, y la que entra por arriba en el siguiente paso es
;     la 100+s. Al llegar a s = LVL_END el scroll se para.
;   - Anillo: 102 filas de 160 bytes (POKE 2073,102). La fila del anillo
;     (vofs + k) mod 102 tiene la linea 99+s-k, k = 0..99. Para subir un
;     pixel se pinta la fila oculta vofs-1 con la linea 100+s y luego vofs
;     baja y s sube; el nuevo vofs se escribe en POKE 2072 justo despues del
;     siguiente VSYNC. Con las dos filas ocultas se pueden dar 2 px por imagen.
;   - Paneles: las columnas 0-15 y 144-159 son iguales en todas las filas
;     del anillo: se ven quietas aunque el anillo se mueva. Se pintan una vez.
;   - Ciclo de paleta: el agua (PAL_WATER), la lava (PAL_LAVA) y las luces
;     (PAL_LIGHTS) giran una posicion cada TCYW, TCYL y TCYG imagenes, en el
;     borrado, a partir de la paleta del .I16 (PALBUF).
;   - Sprites (formato B): HOJA.BIN (de la SD) en SHEET_ADDR = $7000 (bloque
;     3, POKE 2129,3) y la paleta de 16 PAL16 de HOJA.INC (POKE 2067,1). La
;     nave es 16x16 (sprites SP_NAVE..+3: arriba-izquierda, arriba-derecha,
;     abajo-izquierda, abajo-derecha) con 3 inclinaciones (NAVE_0 izquierda,
;     NAVE_1 recta, NAVE_2 derecha), la llama del motor debajo (SP_LLAMA, 2
;     fases que cambian cada 4 imagenes) y hasta NBALAS disparos (sprites
;     SP_BALA..+7). Coordenadas de pantalla: la zona de juego es x 16-143;
;     X de hardware = x + 32, Y = y + 32.
;   - Oleadas (tabla de NIVEL1.MAP, juego160.md 8.3): la siguiente sale en el
;     primer tick con s >= WAVE_POS. Con WF_ROW salen todos sus enemigos a la
;     vez (X + i*SPACING); si no, uno cada SPACING ticks (hasta NSPN oleadas
;     soltando enemigos a la vez). El tipo 6 es el jefe y los 8-10 son
;     mejoras (ver abajo).
;   - Enemigos: hasta NENE a la vez, el e en los sprites 4e..4e+3, en
;     coordenadas de hardware (X = x + 32, Y = y + 32). Salen en Y = 16
;     (y = -16) y cada tick suman el paso de su trayectoria (PATHTAB; dx con
;     el signo cambiado con WF_MIRROR; pasado el ultimo, se repite); los de
;     WF_GROUND bajan ademas lo que haya avanzado el scroll en la imagen. Se
;     borran al salir: X < 33 o X > 175 o Y < 16 o Y > 131. Animacion: 2
;     fases, cambian cada 8 ticks.
;   - Disparos enemigos: hasta NEB (sprites SP_EB..), salen de los enemigos
;     de los tipos con cadencia (eftab) cuando estan en Y 32-92, bajan EBVY
;     pixeles por tick y van 1 pixel por tick hacia la nave si esta a 16 o
;     mas de distancia en X al salir.
;   - Colisiones por cajas (coordenadas de hardware): disparo [x+3,x+4] x
;     [y,y+7] contra enemigo [X+1,X+14] x [Y+1,Y+14]: el enemigo explota
;     (EXT ticks, las 4 fases de EXPL) y suma sus puntos (ptstab). Contra la
;     nave [X+4,X+11] x [Y+3,Y+13]: disparo enemigo [X+3,X+4] x [Y+3,Y+4] y
;     enemigo que no es de suelo [X+2,X+13] x [Y+2,Y+13]: la nave explota
;     (EXT ticks), vuelve a su sitio e invulnerable INVT ticks (parpadea).
;   - Vidas: LIVES0 naves al empezar; cada muerte quita una. Si al acabar la
;     explosion no queda ninguna, fin de partida (pstate 3): sin nave, los
;     enemigos no disparan, sale el cartel GAME OVER en el centro (dos filas de
;     4 letras, x 64-95, y 40 y 50; tabla cartel) y, pasados GOT ticks, ESPACIO (una pulsacion nueva) vuelve a
;     empezar el nivel con LIVES0 vidas y el marcador a 0. Las vidas que
;     quedan se ven con el marcador: iconos VIDA (sprites SP_VIDA..) en una
;     fila debajo de las cifras (x 68-91, y 12).
;   - Marcador: 6 cifras BCD en los sprites SP_DIG.., en una fila centrada
;     arriba de la zona de juego (x 56-103, y 2), solo mientras la nave
;     explota o parpadea despues de una muerte, en el fin de partida y al
;     final del nivel.
;   - Jefe (oleada de tipo 6): en cuanto sale la oleada, los enemigos nuevos
;     solo usan los huecos 0-4 (el jefe usa los sprites 20-31, los de los
;     huecos 5-7). Cuando esos tres quedan libres, aviso: cartel WARNING
;     parpadeando WARNT ticks (sprites 24-30). Luego entra en (WAVE_X, -24)
;     con su trayectoria (ENTER: baja hasta y = 26) y, al acabarla, va de lado
;     a lado 1 px por tick (X de hardware BXMIN-BXMAX) y cada BFT0 ticks
;     dispara dos tiros (como los enemigos, desde X y X+16, Y+8) si la nave no
;     explota ni es el fin de partida. Caja [X+2,X+29] x [Y+2,Y+21]: los
;     disparos de la nave le quitan una de sus BHP0 vidas (parpadea 6 ticks)
;     y la nave viva que lo toca explota. Al quedarse sin vidas explota (dos
;     explosiones de 16x16, BEXT ticks), suma 1000 puntos y sale el cartel
;     WELL DONE CLEART ticks; despues el nivel vuelve a empezar con el mismo
;     marcador y las mismas vidas (lap).
;   - Mejoras (oleadas de tipo 8-10: arma, escudo, bomba): una a la vez (si
;     ya hay una, la nueva se pierde), 8x8 en el sprite 54 (SP_ITEM; en el fin
;     de partida es la V del cartel: ahi no se ve), sale en (WAVE_X, -8) con su
;     trayectoria, no se puede derribar y se borra al salir por abajo (Y >=
;     132). Con la nave viva o parpadeando, si su caja [X+1,X+6] x [Y+1,Y+6]
;     toca la de la nave, se recoge (suena fxitem, voz FX_SHOT): ARMA = disparo
;     doble (dos disparos en x + 0 y x + 8, en dos de los mismos 8 huecos) hasta
;     perder la nave; ESCUDO = invulnerable SHLDT ticks (como el pstate 2: la
;     nave parpadea, y sale el marcador); BOMBA = una bomba mas (BOMBMAX), con
;     iconos BOMBA en los sprites 32-33 (x 100-115, y VIDAY) junto a las vidas,
;     solo cuando sale el marcador y no en el fin de partida (el cartel usa el
;     32 y el 33). Al recoger una mejora sale el marcador SCT ticks (stimer). Tecla B (pulsacion nueva): gasta una bomba, todos los
;     enemigos vivos explotan (con sus puntos) y el jefe, si entra o pelea,
;     pierde BOMBHP vidas sin bajar de 1; suena fxbomb (ruido grave de ~64
;     imagenes, prioridad 3: no lo pisan las explosiones que provoca) y toda la
;     pantalla tiembla SHKN imagenes (hofs, POKE 2074, con la tabla shktab, escrito
;     en el VSYNC junto al 2072; los paneles dan la vuelta; a 0 al acabar, en el
;     fin de partida, con R y al salir). Las bombas y el arma
;     se pierden al volver a empezar (gminit); el arma, tambien al perder la nave.
;   - Letras del cartel: G A M E O V R de 7x7 (letras, en el codigo), que se
;     copian detras de HOJA.BIN, en las celdas libres del bloque 3 (paginas
;     LETRAS..LETRAS+6, en SHEET_END): la hoja tiene que dejar 7 celdas.
;     W N I L D (letras2) van en las paginas LETRAS2..+4 del mismo bloque 3
;     ($6F60-$6FFF: las paginas de menos de 128 tambien son celdas); el
;     codigo y PALBUF tienen que acabar antes.
;   - Musica: NIVEL1.VGM (de la SD, carpeta actual) en bucle, la toca el MCU
;     en su AY emulado sin gastar CPU (comandos 38 y 34; 35 al salir). En el
;     fin de partida se para y suena GAMEOVER.VGM una vez (sin bucle); al
;     volver a empezar, otra vez NIVEL1.VGM en bucle. Si una no esta, el juego
;     sigue sin esa musica.
;   - Efectos en los AY de la FPGA (ZonX, puertos $CF/$0F y $C7/$07), que no
;     tocan el del MCU (la fase 0 lo comprobo en la placa): 4 voces, cada una
;     con una tabla de pares (periodo o ruido, volumen) que avanza un par por
;     imagen (sfx): el disparo de la nave en el canal C del AY A (sale por los
;     dos lados), los disparos enemigos en el A (izquierda) o el B (derecha)
;     del AY A segun el lado del que salen, y las explosiones con ruido en el
;     canal C del AY B. Un efecto nuevo corta el de su voz si no tiene menos
;     prioridad: la explosion de la nave no la corta la de un enemigo.
;   - Cada imagen: en el borrado, los sprites con el estado calculado en la
;     vuelta anterior, el ciclo de la paleta y el scroll; despues, update
;     (nave, disparos, enemigos, colisiones, oleadas), que se ve en la imagen
;     siguiente.
;
; Teclas: O P Q A = mover la nave (1 pixel por imagen), ESPACIO = disparar
;         (mantenido: uno cada TFIRE imagenes); con LOAD *JOY "OPQA " el
;         joystick hace lo mismo
;         S = velocidad (la del nivel al empezar: 1 px por imagen, borde
;         verde, o 1 px cada 2, cyan; luego 2 px por imagen, amarillo;
;         parado, rojo)
;         R = volver al principio del nivel (y la nave a su sitio, con
;         LIVES0 vidas y el marcador a 0); en el fin de partida, ESPACIO
;         B = bomba (si hay alguna)
;         I = vidas infinitas si / no (para probar; el borde, brillante)
;         X = salir a BASIC
; Devuelve en BC 0, o un error de carga: 1 no hay NIVEL1.I16, 2 no es un
; .I16, 3 no hay NIVEL1.MAP, 4 no es un nivel (ELV1), 5 pasa de
; 8 KB, 6 cabecera del nivel incoherente, 7 no hay HOJA.BIN.
;
; Ensamblar: pasmo estela.asm estela.bin estela.sym   (o compila.bat)
; Cargar: ESTELA.B81 (CLEAR 18431, LOAD FAST "ESTELA.BIN" CODE 18432);
; NIVEL1.I16, NIVEL1.MAP y HOJA.BIN (de out/) y NIVEL1.VGM (opcional) en la
; misma carpeta de la SD.
; El codigo y PALBUF tienen que acabar antes de $6F60 (letras2 y la hoja de los sprites).
; =============================================================

        org 4800h

IMG     equ 8000h               ; anillo
RING    equ 102                 ; filas del anillo
VIS     equ 100                 ; filas visibles
PANEL   equ 16                  ; ancho de cada panel
PGTIL   equ 6                   ; paginas para los bloques 6 y 7
MAPPORT equ 0E7h
I16PIX  equ 16000               ; pixeles de un .I16
TCYW    equ 4                   ; imagenes por paso del ciclo del agua
TCYL    equ 6                   ; de la lava
TCYG    equ 8                   ; de las luces
CYW0    equ TCYW                ; contadores al empezar: el agua gira en las
CYL0    equ 1                   ;   vueltas 4, 8, 12...; la lava en la 1, 7,
CYG0    equ 2                   ;   13... (impares) y las luces en la 2, 10,
                                ;   18...: nunca dos rangos en la misma imagen
; sprites (ventana de POKEs, Apendice H)
SPR_SEL equ 2100
SPR_EN  equ 2101
SPR_XL  equ 2102
SPR_XH  equ 2103                ; bit 0: X bit 8; bits 4-7: transparente
SPR_Y   equ 2104
SPR_MSK equ 2121                ; 2121-2128: mascara de las 8 filas
SPR_BLK equ 2129                ; bloque de la hoja
SPR_PG  equ 2130                ; pagina (0 = formato A)
SPR_XTR equ 2131                ; sprites extra
SP_NAVE equ 50                  ; nave: 50-53 (juego160.md 4.1)
SP_LLAMA equ 49                 ; llama del motor
SP_BALA equ 34                  ; disparos del jugador: 34-41
NBALAS  equ 8
SOFS    equ 32                  ; coordenada de sprite = pixel + 32
XMIN    equ 16                  ; la nave (esquina de arriba a la izquierda)
XMAX    equ 128                 ;   en x 16-128 (la zona de juego es 16-143)
YMIN    equ 0
YMAX    equ 76                  ;   y en y 0-76 (la llama, en 92-99)
XSTART  equ 72
YSTART  equ 76
VBALA   equ 3                   ; pixeles por imagen de los disparos
TFIRE   equ 6                   ; imagenes entre disparo y disparo
SP_ENE  equ 0                   ; enemigos: 0-31 (4 sprites cada uno)
NENE    equ 8
ESZ     equ 16                  ; registro de un enemigo:
E_ST    equ 0                   ;   0 libre, 1 vivo, 2 explotando
E_TYP   equ 1                   ;   tipo (0-5)
E_X     equ 2                   ;   X de hardware
E_Y     equ 3                   ;   Y de hardware
E_PTR   equ 4                   ;   (2) paso actual de la trayectoria
E_LEFT  equ 6                   ;   pasos que quedan (con el actual)
E_FLG   equ 7                   ;   WF_* de la oleada
E_TMR   equ 8                   ;   ticks de explosion que quedan
E_FT    equ 9                   ;   ticks hasta el siguiente disparo (0 = no dispara)
SP_EB   equ 42                  ; disparos enemigos: 42-48
NEB     equ 7                   ; registro: activo, X, Y, dx
EBVY    equ 2
SP_DIG  equ 58                  ; marcador: 58-63
DIGX    equ 56                  ; x de pantalla de la primera cifra (6 x 8 = 48 px, centradas en 16-143)
DIGY    equ 2                   ; y de pantalla de las cifras
NSPN    equ 4                   ; oleadas soltando enemigos a la vez
SPSZ    equ 4                   ; registro: quedan, oleada (2), espera
EXT     equ 16                  ; ticks de una explosion
INVT    equ 100                 ; ticks de invulnerabilidad
AYA_SEL equ 0CFh                ; AY A de la FPGA (ZonX): registro / dato
AYA_DAT equ 0Fh
AYB_SEL equ 0C7h                ; AY B (puertos impares: los pares son de la ULA)
AYB_DAT equ 07h
CMD_PLAYVGM equ 34              ; ordenes del MCU para la musica
CMD_STOPVGM equ 35
CMD_LOOPVGM equ 38
NFX     equ 4                   ; voces de efectos (ver fxdesc)
FX_SHOT equ 0                   ;   disparo de la nave
FX_EBL  equ 1                   ;   disparo enemigo por la izquierda
FX_EBR  equ 2                   ;   por la derecha
FX_BOOM equ 3                   ;   explosiones
LVLPG   equ 20                  ; pagina del nivel (en el bloque 1); la de la prueba PAGINA
LVLBUF  equ 2000h               ; NIVEL1.MAP
MAXLVL  equ 2000h               ; hasta 8 KB
LIVES0  equ 3                   ; vidas al empezar
GOT     equ 100                 ; ticks del fin de partida antes de aceptar ESPACIO
NLET    equ 7                   ; (SHEET_CELLS + NLET <= 128: lo comprueba el arnes)
LETRAS2 equ 123                 ; pagina de la W (luego N I L D), en $6F60
LET2ADR equ 6000h+LETRAS2*32
SP_JEFE equ 20                  ; jefe: sprites 20-31 (los de los huecos 5-7)
NSLOTJ  equ 5                   ; huecos de enemigos mientras hay jefe
BHP0    equ 40                  ; disparos que aguanta
BFT0    equ 30                  ; ticks entre salva y salva
WARNT   equ 100                 ; ticks del aviso
BEXT    equ 48                  ; ticks de su explosion
CLEART  equ 150                 ; ticks del cartel WELL DONE
BXMIN   equ SOFS+16             ; X de hardware de su lado izquierdo: 48-144
BXMAX   equ SOFS+143-31         ;   (x de pantalla 16-112)
CARTX   equ 64                  ; cartel: x de pantalla de la primera letra
CARTY   equ 40                  ;   y de GAME (OVER, 10 mas abajo)
SP_VIDA equ 55                  ; iconos de las vidas: 55-57
VIDAX   equ 68                  ; x de pantalla del primero (3 x 8 px, centrados)
VIDAY   equ DIGY+10             ; y de pantalla: debajo de las cifras
SP_ITEM equ 54                  ; mejora: sprite 54 (en el fin de partida es la V del cartel)
SP_BOMB equ 32                  ; iconos de las bombas: 32-33 (en el fin de partida, el cartel)
BOMBX   equ 100                 ; x de pantalla del primero (junto a las vidas, y VIDAY)
BOMBMAX equ 2                   ; bombas que se pueden llevar
BOMBHP  equ 10                  ; vidas que le quita una bomba al jefe (sin bajar de 1)
SHLDT   equ 250                 ; ticks del escudo
SHKN    equ 24                  ; imagenes que tiembla la pantalla con una bomba (tabla shktab)
SCT     equ 60                  ; ticks que se ve el marcador al recoger una mejora

        jp start

; -------------------------------------------------------------
; arranque
; -------------------------------------------------------------
start:  di
        push ix                 ; la ROM usa IX en SLOW
        xor a                   ; sin musica hasta que el MCU la acepte
        ld (vgmok),a
        call ayquiet
        ld c,6                  ; paginas de los bloques 6 y 7 (las de antes)
        call read_page
        ld (sv6),a
        ld c,7
        call read_page
        ld (sv7),a
        ld c,1                  ; el bloque 1 (la ROM): su pagina, para volver
        call read_page
        ld (sv1),a
        ld a,LVLPG              ; y la del nivel en su lugar
        ld c,1
        call map_page
        ld a,PGTIL
        ld c,6
        call map_page
        ld a,PGTIL+1
        ld c,7
        call map_page

        call blackpal           ; todo negro mientras se carga y se pinta
        xor a                   ; HFILE = $8000
        ld (2043),a
        ld a,IMG/256
        ld (2044),a
        xor a
        ld (2072),a             ; anillo: sin desplazar, 102 filas de 160
        ld (2074),a
        ld (2075),a
        ld a,RING
        ld (2073),a
        ld bc,7fefh             ; color Chroma81, borde negro
        ld a,20h
        out (c),a
        ld a,175                ; modo 160x100 a 8 bpp
        ld (2045),a

        call loadlvl            ; A = 0, o el error
        or a
        jp nz,quit
        call sprinit
        call gminit
        call mktabs
        call panels
        call fillring
        call cycinit
        call setpal
        call aysetup
        xor a                   ; la musica del nivel
        call music
        ld a,(LVLBUF+LVL_SCROLL) ; la velocidad del nivel: 1 -> 1 px por
        cp 2                    ; imagen (0), 2 o mas -> 1 cada 2 (1)
        ld a,0
        jr c,st1
        inc a
st1:    ld (speed),a
        xor a
        ld (fcnt),a
        ld a,2                  ; S y R sin pulsar
        ld (prevs),a
        ld a,8
        ld (prevr),a
        ld a,1                  ; ESPACIO sin pulsar
        ld (prevsp),a
        ld a,4                  ; I sin pulsar, sin vidas infinitas
        ld (previ),a
        xor a
        ld (infl),a
        call showspd

; -------------------------------------------------------------
; bucle principal: una vuelta por imagen
; -------------------------------------------------------------
main:   call waitvs
        ld a,(vofs)             ; lo preparado en la vuelta anterior
        ld (2072),a
        ld a,(hofv)             ; el temblor de la bomba: sin latch, junto al 2072
        ld (2074),a
        call drawspr            ; los sprites, en el borrado
        call cycle              ; ciclo de la paleta, en el borrado
        xor a                   ; pasos de scroll hechos en esta imagen
        ld (nstp),a
        call nsteps             ; B = pasos de esta imagen (0, 1 o 2)
        inc b
        jr mn2
mn1:    push bc
        call step
        pop bc
mn2:    djnz mn1
        call update             ; el juego (se ve en la imagen siguiente)
        call hstep              ; el desplazamiento horizontal de la imagen siguiente
        call sfx                ; los efectos que tocan en esta imagen
        call keys
        jr nz,main
        xor a                   ; sin error

; --- salida (A = error): modo normal, color apagado, paginas de antes ---
quit:   ld (errc),a
        call sproff
        ld a,(vgmok)
        or a
        jr z,qt1
        ld a,CMD_STOPVGM
        call mcu_send
qt1:    call ayquiet
        xor a
        ld (2072),a
        ld (2073),a
        ld (2074),a
        ld (2075),a
        ld a,85
        ld (2045),a
        ld bc,7fefh             ; color de Chroma81 apagado: el BASIC se lee
        ld a,0ch
        out (c),a
        ld a,(sv6)
        ld c,6
        call map_page
        ld a,(sv7)
        ld c,7
        call map_page
        ld a,(sv1)              ; el bloque 1 vuelve a tener la ROM; reescrita
        ld c,1                  ; sobre si misma, la sombra de $2000-$3FFF
        call map_page           ; vuelve a ser la de la ROM (en la ROM no se
        ld hl,2000h             ; escribe nada: son los mismos bytes)
        ld de,2000h
        ld bc,2000h
        ldir
        ld hl,4000h             ; la sombra de $C000-$FFFF vuelve a ser la de
        ld de,0C000h            ; $4000-$7FFF (son las mismas paginas: se
        ld bc,4000h             ; reescriben los mismos bytes)
        ldir
        pop ix
        ld a,(errc)
        ld c,a
        ld b,0
        ret

; -------------------------------------------------------------
; step: sube un pixel si no se ha llegado al final del nivel. Pinta la
; fila oculta vofs-1 del anillo con la linea 100+s, baja vofs y sube s.
; -------------------------------------------------------------
step:   ld hl,(scr)
        ld de,(lvlend)
        or a
        sbc hl,de
        ret nc                  ; s >= LVL_END: final del nivel
        ld a,(vofs)
        or a
        jr nz,sp1
        ld a,RING
sp1:    dec a
        ld hl,(scr)
        ld de,VIS
        add hl,de
        push af
        call prow
        pop af
        ld (vofs),a
        ld hl,(scr)
        inc hl
        ld (scr),hl
        ld hl,nstp
        inc (hl)
        ret

; nsteps: B = pasos segun la velocidad (0: 1, 1: 1 cada 2 imagenes, 2: 2, 3: 0)
nsteps: ld hl,fcnt
        inc (hl)
        ld a,(speed)
        ld b,1
        or a
        ret z
        dec a
        jr nz,ns1
        ld b,(hl)               ; 1 cada 2: el bit 0 del contador
        ld a,b
        and 1
        ld b,a
        ret
ns1:    dec a
        ld b,2
        ret z
        ld b,0
        ret

; hstep: hofv = el desplazamiento horizontal (POKE 2074) de la imagen siguiente:
; con shk > 0 (imagenes de temblor que quedan), shktab[SHKN - shk] y shk baja; si no, 0
hstep:  ld a,(shk)
        or a
        jr z,hs1                ; 0: sin desplazar
        ld c,a
        ld a,SHKN
        sub c
        ld e,a
        ld d,0
        ld hl,shktab
        add hl,de
        ld a,(hl)
        dec c
        ld hl,shk
        ld (hl),c
hs1:    ld (hofv),a
        ret

; shktab: desplazamientos del temblor (modulo 160: 157 = -3), se apagan poco a poco
shktab: defb 3,157,3,157,3,157,2,158,2,158,2,158
        defb 1,159,1,159,1,159,1,159,1,159,1,0

; fillring: el principio del nivel (s = 0, vofs = 0): la fila c del anillo
; tiene la linea 99-c (c = 0..99); las ocultas 100 y 101, las 101 y 100.
fillring:
        xor a
        ld (vofs),a
        ld hl,0
        ld (scr),hl
        ld c,0
fr1:    ld a,c
        cp VIS
        ld a,VIS-1
        jr c,fr2
        ld a,VIS+RING-1
fr2:    sub c
        ld l,a
        ld h,0
        ld a,c
        push bc
        call prow
        pop bc
        inc c
        ld a,c
        cp RING
        jr nz,fr1
        ret

; -------------------------------------------------------------
; prow: pinta las columnas 16-143 de la fila A del anillo con la linea HL
; del nivel. ~2.600 T. Toca todos los registros menos IX.
; -------------------------------------------------------------
prow:   push hl
        ld l,a                  ; DE = rowtab[A] + PANEL
        ld h,0
        add hl,hl
        ld de,rowtab
        add hl,de
        ld e,(hl)
        inc hl
        ld d,(hl)
        ld hl,PANEL
        add hl,de
        ex de,hl
        pop hl                  ; HL = linea
        ld a,l                  ; lyoff = (15 - (L mod 16)) * 160
        and 15
        xor 15
        add a,a
        ld c,a
        ld b,0
        push hl
        ld hl,lytab
        add hl,bc
        ld c,(hl)
        inc hl
        ld b,(hl)
        ld (lyoff),bc
        pop hl
        ld a,l                  ; HL = mapbase + (L / 16) * 8
        and 0f0h                ;    = mapbase + (L AND $FFF0) / 2
        ld l,a
        srl h
        rr l
        ld bc,(mapbase)
        add hl,bc
        ld b,8
pr1:    push bc
        ld a,(hl)               ; tile de esta columna
        inc hl
        push hl
        add a,a                 ; HL = TILEADR[t] + lyoff
        ld l,a
        ld h,0
        ld bc,TILEADR
        add hl,bc
        ld a,(hl)
        inc hl
        ld h,(hl)
        ld l,a
        ld bc,(lyoff)
        add hl,bc
        ldi                     ; 16 pixeles
        ldi
        ldi
        ldi
        ldi
        ldi
        ldi
        ldi
        ldi
        ldi
        ldi
        ldi
        ldi
        ldi
        ldi
        ldi
        pop hl
        pop bc
        djnz pr1
        ret

; -------------------------------------------------------------
; loadlvl: NIVEL1.I16 (paleta a PALBUF, pixeles a $C000) y NIVEL1.MAP
; (a LVLBUF). A = 0 bien, o el error (ver la cabecera).
; -------------------------------------------------------------
loadlvl:
        ld hl,nmi16
        call f_open
        cp 0ffh
        ld e,1
        jp z,lderr0
        ld (fh),a
        ld hl,hdr
        ld bc,4
        call f_read_n
        ld hl,hdr
        ld de,magi16
        call cmp4
        ld e,2
        jp nz,lderr
        ld a,(fh)
        ld hl,PALBUF
        ld bc,512
        call f_read_n
        ld a,(fh)
        ld hl,TILESET_BASE
        ld bc,I16PIX
        call f_read_n
        ld a,(fh)
        call f_close

        ld hl,nmmap
        call f_open
        cp 0ffh
        ld e,3
        jp z,lderr0
        ld (fh),a
        ld hl,LVLBUF
        ld bc,LVL_HDR_SIZE
        call f_read_n
        ld hl,LVLBUF
        ld de,magmap
        call cmp4
        ld e,4
        jp nz,lderr
        ld hl,(LVLBUF+LVL_LEN)  ; LVL_HDR_SIZE <= longitud <= MAXLVL
        ld de,MAXLVL+1
        or a
        sbc hl,de
        ld e,5
        jp nc,lderr
        ld hl,(LVLBUF+LVL_LEN)
        ld de,LVL_HDR_SIZE
        or a
        sbc hl,de
        ld e,6
        jp c,lderr
        ld b,h                  ; el resto del fichero
        ld c,l
        ld a,(fh)
        ld hl,LVLBUF+LVL_HDR_SIZE
        call f_read_n
        ld a,(fh)
        call f_close
        ld hl,(LVLBUF+LVL_ROWS) ; al menos 7 filas (las 102 lineas del anillo)
        ld de,7
        or a
        sbc hl,de
        ld a,6
        ret c
        ld hl,(LVLBUF+LVL_ROWS) ; LVL_MAP + 8 * filas <= longitud
        add hl,hl
        add hl,hl
        add hl,hl
        ld de,(LVLBUF+LVL_MAP)
        add hl,de
        ex de,hl
        ld hl,(LVLBUF+LVL_LEN)
        or a
        sbc hl,de
        ld a,6
        ret c
        ld hl,(LVLBUF+LVL_MAP)
        ld de,LVLBUF
        add hl,de
        ld (mapbase),hl
        ld hl,(LVLBUF+LVL_END)
        ld (lvlend),hl
        ld hl,nmhoja            ; la hoja de los sprites
        call f_open
        cp 0ffh
        ld e,7
        jp z,lderr0
        ld (fh),a
        ld hl,SHEET_ADDR
        ld bc,SHEET_END-SHEET_ADDR
        call f_read_n
        ld a,(fh)
        call f_close
        ld hl,letras            ; las letras del cartel, detras
        ld de,SHEET_END
        ld bc,32*NLET
        ldir
        ld hl,letras2           ; y W N I L D en las paginas LETRAS2..
        ld de,LET2ADR
        ld bc,32*5
        ldir
        xor a
        ret
lderr:  push de                 ; E = error, con el fichero abierto
        ld a,(fh)
        call f_close
        pop de
lderr0: ld a,e
        ret

; cmp4: Z si los 4 bytes de HL y DE son iguales
cmp4:   ld b,4
cm1:    ld a,(de)
        cp (hl)
        ret nz
        inc hl
        inc de
        djnz cm1
        ret

; -------------------------------------------------------------
; mktabs: rowtab (principio de cada fila del anillo) y lytab (y * 160)
; -------------------------------------------------------------
mktabs: ld hl,IMG
        ld ix,rowtab
        ld de,160
        ld b,RING
mk1:    ld (ix+0),l
        ld (ix+1),h
        inc ix
        inc ix
        add hl,de
        djnz mk1
        ld ix,lytab
        ld hl,0
        ld b,16
mk4:    ld (ix+0),l
        ld (ix+1),h
        inc ix
        inc ix
        add hl,de
        djnz mk4
        ret

; panels: las columnas 0-15 y 144-159 de las 102 filas del anillo, de la
; fila de los paneles del tileset
panels: ld hl,IMG
        ld b,RING
pa1:    push bc
        ex de,hl
        ld hl,PANEL_ADDR
        ld bc,PANEL
        ldir                    ; DE = columna 16
        ld hl,160-2*PANEL
        add hl,de
        ex de,hl
        ld hl,PANEL_ADDR+160-PANEL
        ld bc,PANEL
        ldir                    ; DE = columna 0 de la fila siguiente
        ex de,hl
        pop bc
        djnz pa1
        ret

; -------------------------------------------------------------
; Paleta. 2063 puntero, 2064 {G,B}, 2065 {BRIGHT,R}: escribe y avanza.
; -------------------------------------------------------------
blackpal:
        xor a
        ld (2063),a
        ld b,0
bp1:    ld (2064),a
        ld (2065),a
        djnz bp1
        ret

; setpal: las 256 entradas de PALBUF (la paleta del .I16)
setpal: xor a
        ld (2063),a
        ld hl,PALBUF
        ld b,0
sp8:    ld a,(hl)
        ld (2064),a
        inc hl
        ld a,(hl)
        ld (2065),a
        inc hl
        djnz sp8
        ret

; cycinit: los tres rangos sin girar y los contadores al principio
cycinit:
        xor a
        ld (ofw),a
        ld (ofl),a
        ld (ofg),a
        ld a,CYW0
        ld (cyw),a
        ld a,CYL0
        ld (cyl),a
        ld a,CYG0
        ld (cyg),a
        ret

; cycle: una vez por imagen; cada rango gira una posicion cuando su
; contador llega a 0 (como mucho un rango por imagen: 16 entradas, ~1.200 T)
cycle:  ld hl,cyw
        ld de,ofw
        ld bc,PAL_WATER_N*256+PAL_WATER
        ld a,TCYW
        call cyc1
        ld hl,cyl
        ld de,ofl
        ld bc,PAL_LAVA_N*256+PAL_LAVA
        ld a,TCYL
        call cyc1
        ld hl,cyg
        ld de,ofg
        ld bc,PAL_LIGHTS_N*256+PAL_LIGHTS
        ld a,TCYG
        ; sigue en cyc1
; HL = contador, DE = giro, A = imagenes por paso, C = primera entrada,
; B = cuantas
cyc1:   dec (hl)
        ret nz
        ld (hl),a
        ld a,(de)               ; giro = (giro + 1) mod B
        inc a
        cp b
        jr c,cc1
        xor a
cc1:    ld (de),a
        ld e,a
; cycrng: entradas C..C+B-1 = las de PALBUF giradas E posiciones (la
; entrada C+i toma el color C+((i+E) mod B), E < B): primero los B-E colores
; desde C+E y luego los E desde C
cycrng: ld a,c
        ld (2063),a
        ld l,c
        ld h,0
        add hl,hl
        push de
        ld de,PALBUF
        add hl,de               ; HL = el color C
        pop de
        push hl
        ld a,e                  ; HL = el color C+E
        add a,a
        add a,l
        ld l,a
        jr nc,cr1
        inc h
cr1:    ld a,b
        sub e
        ld b,a
        call crcopy
        pop hl
        ld b,e
        ld a,b
        or a
        ret z
; crcopy: B colores seguidos desde HL a la paleta (el puntero avanza solo)
crcopy: ld a,(hl)
        ld (2064),a
        inc hl
        ld a,(hl)
        ld (2065),a
        inc hl
        djnz crcopy
        ret

; -------------------------------------------------------------
; keys: S cambia la velocidad y R vuelve al principio (al pulsar).
; NZ = seguir, Z = X pulsada.
; -------------------------------------------------------------
keys:   ld a,0fdh               ; A S D F G
        in a,(0feh)
        and 2
        ld c,a
        ld a,(prevs)
        ld b,a
        ld a,c
        ld (prevs),a
        or a
        jr nz,ky1               ; S no pulsada
        ld a,b
        or a
        jr z,ky1                ; ya lo estaba
        ld a,(speed)
        inc a
        and 3
        ld (speed),a
        call showspd
ky1:    ld a,0fbh               ; Q W E R T
        in a,(0feh)
        and 8
        ld c,a
        ld a,(prevr)
        ld b,a
        ld a,c
        ld (prevr),a
        or a
        jr nz,ky2               ; R no pulsada
        ld a,b
        or a
        jr z,ky2
        call restart
ky2:    ld a,07fh               ; ESPACIO (bit 0): en el fin de partida,
        in a,(0feh)             ; pasada la espera, una pulsacion nueva
        and 1                   ; vuelve a empezar
        ld c,a
        ld a,(prevsp)
        ld b,a
        ld a,c
        ld (prevsp),a
        or a
        jr nz,ky3
        ld a,b
        or a
        jr z,ky3
        ld a,(pstate)
        cp 3
        jr nz,ky3
        ld a,(ptimer)
        or a
        call z,restart
ky3:    ld a,0dfh               ; I (fila Y U I O P, bit 2): vidas infinitas
        in a,(0feh)             ; si / no, con cada pulsacion nueva
        and 4
        ld c,a
        ld a,(previ)
        ld b,a
        ld a,c
        ld (previ),a
        or a
        jr nz,ky4
        ld a,b
        or a
        jr z,ky4
        ld a,(infl)
        xor 1
        ld (infl),a
        call showspd
ky4:    ld a,0feh               ; X: salir
        in a,(0feh)
        and 4
        ret

; restart: el nivel desde el principio, con la paleta en negro mientras
; se repinta el anillo (unas 4 imagenes)
restart:
        ld a,(musno)            ; si sonaba la del fin de partida, otra vez la
        or a                    ; del nivel (si no, la musica sigue)
        jr z,rs1
        xor a
        call music
rs1:    call gminit             ; (las voces de los efectos, libres)
        call aysetup            ; y calladas
        call blackpal
        call fillring
        call cycinit
        jp setpal

; showspd: el borde segun la velocidad
showspd:
        ld a,(speed)
        ld e,a
        ld d,0
        ld hl,spdcol
        add hl,de
        ld a,(infl)             ; con vidas infinitas, el color brillante
        or a
        ld a,(hl)
        jr z,ss1
        or 8
ss1:    or 20h
        ld bc,7fefh
        out (c),a
        ret
spdcol: defb 4,5,6,2            ; verde, cyan, amarillo, rojo

; -------------------------------------------------------------
; Sonido
; -------------------------------------------------------------
; music: A = 0 la del nivel (NIVEL1.VGM, en bucle), 1 la del fin de partida
; (GAMEOVER.VGM, una vez). Para antes la que sonaba; si el MCU no puede abrir
; la nueva, sin musica
music:  ld (musno),a
        ld a,(vgmok)
        or a
        jr z,mu0
        ld a,CMD_STOPVGM
        call mcu_send
mu0:    ld hl,nmvgm
        ld e,1                  ; en bucle
        ld a,(musno)
        or a
        jr z,mu1
        ld hl,nmgov
        ld e,0                  ; una vez
mu1:    ld a,CMD_LOOPVGM
        call mcu_send
        ld a,e
        call mcu_send
        ld a,CMD_PLAYVGM
        call mcu_name           ; A = 0 si suena
        or a
        ld a,1
        jr z,mu2
        xor a
mu2:    ld (vgmok),a
        ret

; sfxgo: C = voz, HL = efecto (prioridad y pares (valor, volumen); volumen
; $FF = fin). Empieza si la voz esta libre o lo que suena no tiene mas
; prioridad. Conserva todo.
sfxgo:  push af
        push bc
        push de
        push hl
        ld a,c
        add a,a
        add a,c
        ex de,hl                ; DE = efecto
        ld hl,fxv               ; HL = la voz (3C): puntero (2), prioridad
        add a,l
        ld l,a
        jr nc,sg0
        inc h
sg0:
        ld a,(hl)
        inc hl
        or (hl)
        jr z,sg1                ; libre
        inc hl
        ld a,(de)
        cp (hl)
        jr c,sg2                ; menos prioridad que lo que suena
        dec hl
sg1:    dec hl
        ld a,(de)
        inc de
        ld (hl),e
        inc hl
        ld (hl),d
        inc hl
        ld (hl),a
sg2:    pop hl
        pop de
        pop bc
        pop af
        ret

; sfx: una vez por imagen, el siguiente par de cada voz que suena: el valor
; al registro de periodo (o de ruido) y el volumen al de volumen; al acabar,
; volumen 0 y la voz libre
sfx:    ld ix,fxv
        ld hl,fxdesc
        ld b,NFX
sx1:    push bc
        ld e,(ix+0)
        ld d,(ix+1)
        ld a,d
        or e
        jr z,sx4                ; libre
        ld c,(hl)               ; C = puerto del chip
        inc hl
        ld a,(de)               ; B = valor, A = volumen
        ld b,a
        inc de
        ld a,(de)
        inc de
        ld (ix+0),e
        ld (ix+1),d
        cp 0ffh
        jr nz,sx2
        xor a                   ; fin
        ld (ix+0),a
        ld (ix+1),a
        inc hl
        ld e,a
        jr sx3
sx2:    ld d,(hl)               ; periodo o ruido
        ld e,b
        call aywr
        inc hl
        ld e,a
sx3:    ld d,(hl)               ; volumen
        call aywr
        inc hl
        jr sx5
sx4:    inc hl
        inc hl
        inc hl
sx5:    ld de,3
        add ix,de
        pop bc
        djnz sx1
        ret

; fxdesc: cada voz: puerto del chip, registro del periodo (o del ruido),
; registro del volumen
fxdesc: defb AYA_SEL,4,10       ; FX_SHOT: AY A, canal C
        defb AYA_SEL,0,8        ; FX_EBL: AY A, canal A (izquierda)
        defb AYA_SEL,2,9        ; FX_EBR: AY A, canal B (derecha)
        defb AYB_SEL,6,10       ; FX_BOOM: AY B, ruido en el canal C

; aysetup: AY A con los tonos de los tres canales (periodos altos a 0) y
; AY B con el ruido del canal C; volumenes a 0
aysetup:
        call ayquiet
        ld c,AYA_SEL
        ld de,0738h
        call aywr
        ld de,0100h
        call aywr
        ld de,0300h
        call aywr
        ld de,0500h
        call aywr
        ld c,AYB_SEL
        ld de,071fh
        jp aywr

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

; aywr: C = puerto de seleccion del chip, D = registro, E = valor. El del
; dato es C AND $3F ($CF -> $0F, $C7 -> $07). Conserva todos los registros.
aywr:   out (c),d
        push bc
        push af
        ld a,c
        and 3fh
        ld c,a
        out (c),e
        pop af
        pop bc
        ret

; mcu_name: A = orden, HL = nombre en ASCII acabado en 0 -> lo manda en
; codigo ZX81 (longitud y caracteres) y devuelve A = estado (0 = bien)
mcu_name:
        call mcu_send
        push hl
        ld b,0
nn1:    ld a,(hl)
        or a
        jr z,nn2
        inc b
        inc hl
        jr nn1
nn2:    pop hl
        ld a,b
        call mcu_send
nn3:    ld a,(hl)
        or a
        jp z,mcu_recv
        cp 'A'                  ; ASCII (mayusculas, cifras y .) -> ZX81
        jr c,nn4
        sub 'A'-26h
        jr nn6
nn4:    cp '0'
        jr c,nn5
        sub '0'-1ch
        jr nn6
nn5:    ld a,1bh                ; '.'
nn6:    call mcu_send
        inc hl
        jr nn3

; cartel: sprite, X, Y, pagina de cada letra (GAME arriba, OVER debajo)
cartel: defb 50,SOFS+CARTX,SOFS+CARTY,LETRAS+0          ; G
        defb 51,SOFS+CARTX+8,SOFS+CARTY,LETRAS+1        ; A
        defb 52,SOFS+CARTX+16,SOFS+CARTY,LETRAS+2       ; M
        defb 53,SOFS+CARTX+24,SOFS+CARTY,LETRAS+3       ; E
        defb 49,SOFS+CARTX,SOFS+CARTY+10,LETRAS+4       ; O
        defb 54,SOFS+CARTX+8,SOFS+CARTY+10,LETRAS+5     ; V
        defb 32,SOFS+CARTX+16,SOFS+CARTY+10,LETRAS+3    ; E
        defb 33,SOFS+CARTX+24,SOFS+CARTY+10,LETRAS+6    ; R
; WARNING (sprites 24-30, x 52-107, y 40) y WELL DONE (20-27, como GAME OVER)
warncar:
        defb 24,SOFS+52,SOFS+40,LETRAS2+0       ; W
        defb 25,SOFS+60,SOFS+40,LETRAS+1        ; A
        defb 26,SOFS+68,SOFS+40,LETRAS+6        ; R
        defb 27,SOFS+76,SOFS+40,LETRAS2+1       ; N
        defb 28,SOFS+84,SOFS+40,LETRAS2+2       ; I
        defb 29,SOFS+92,SOFS+40,LETRAS2+1       ; N
        defb 30,SOFS+100,SOFS+40,LETRAS+0       ; G
wellcar:
        defb 20,SOFS+CARTX,SOFS+CARTY,LETRAS2+0         ; W
        defb 21,SOFS+CARTX+8,SOFS+CARTY,LETRAS+3        ; E
        defb 22,SOFS+CARTX+16,SOFS+CARTY,LETRAS2+3      ; L
        defb 23,SOFS+CARTX+24,SOFS+CARTY,LETRAS2+3      ; L
        defb 24,SOFS+CARTX,SOFS+CARTY+10,LETRAS2+4      ; D
        defb 25,SOFS+CARTX+8,SOFS+CARTY+10,LETRAS+4     ; O
        defb 26,SOFS+CARTX+16,SOFS+CARTY+10,LETRAS2+1   ; N
        defb 27,SOFS+CARTX+24,SOFS+CARTY+10,LETRAS+3    ; E
; letras2: W N I L D (como letras)
letras2:
        defb 0CCh,000h,00Ch,0C0h,0CCh,010h,00Ch,0C1h,0CCh,010h,00Ch,0C1h,0DDh,01Dh,00Dh,0D1h    ; W
        defb 0DDh,0DDh,0DDh,0D1h,099h,091h,099h,091h,099h,011h,009h,091h,001h,010h,000h,011h
        defb 0CCh,000h,00Ch,0C0h,0CCh,0C0h,00Ch,0C1h,0CCh,0CCh,00Ch,0C1h,0DDh,01Dh,0DDh,0D1h    ; N
        defb 0DDh,010h,0DDh,0D1h,099h,010h,009h,091h,099h,010h,009h,091h,001h,010h,000h,011h
        defb 00Ch,0CCh,0CCh,000h,000h,0CCh,0C1h,010h,000h,0CCh,0C1h,000h,000h,0DDh,0D1h,000h    ; I
        defb 000h,0DDh,0D1h,000h,000h,099h,091h,000h,009h,099h,099h,000h,000h,011h,011h,010h
        defb 0CCh,000h,000h,000h,0CCh,010h,000h,000h,0CCh,010h,000h,000h,0DDh,010h,000h,000h    ; L
        defb 0DDh,010h,000h,000h,099h,010h,000h,000h,099h,099h,099h,090h,001h,011h,011h,011h
        defb 0CCh,0CCh,0CCh,000h,0CCh,011h,01Ch,0C0h,0CCh,010h,00Ch,0C1h,0DDh,010h,00Dh,0D1h    ; D
        defb 0DDh,010h,00Dh,0D1h,099h,010h,009h,091h,099h,099h,099h,011h,001h,011h,011h,010h

; letras: G A M E O V R, celdas del formato B (8 filas de 4 bytes, nibble
; alto = pixel de la izquierda); colores de PAL16: 12 amarillo, 13 naranja,
; 9 rojo (degradado por filas) y 1 (oscuro) de sombra abajo a la derecha
letras:
        defb 00Ch,0CCh,0CCh,000h,0CCh,011h,011h,010h,0CCh,010h,000h,000h,0DDh,01Dh,0DDh,0D0h    ; G
        defb 0DDh,010h,01Dh,0D1h,099h,010h,009h,091h,009h,099h,099h,011h,000h,011h,011h,010h
        defb 000h,0CCh,0C0h,000h,00Ch,0C1h,0CCh,000h,0CCh,011h,00Ch,0C0h,0DDh,010h,00Dh,0D1h    ; A
        defb 0DDh,0DDh,0DDh,0D1h,099h,011h,019h,091h,099h,010h,009h,091h,001h,010h,000h,011h
        defb 0CCh,000h,00Ch,0C0h,0CCh,0C0h,0CCh,0C1h,0CCh,0CCh,0CCh,0C1h,0DDh,01Dh,01Dh,0D1h    ; M
        defb 0DDh,010h,01Dh,0D1h,099h,010h,009h,091h,099h,010h,009h,091h,001h,010h,000h,011h
        defb 0CCh,0CCh,0CCh,0C0h,0CCh,011h,011h,011h,0CCh,010h,000h,000h,0DDh,0DDh,0DDh,000h    ; E
        defb 0DDh,011h,011h,010h,099h,010h,000h,000h,099h,099h,099h,090h,001h,011h,011h,011h
        defb 00Ch,0CCh,0CCh,000h,0CCh,011h,01Ch,0C0h,0CCh,010h,00Ch,0C1h,0DDh,010h,00Dh,0D1h    ; O
        defb 0DDh,010h,00Dh,0D1h,099h,010h,009h,091h,009h,099h,099h,011h,000h,011h,011h,010h
        defb 0CCh,000h,00Ch,0C0h,0CCh,010h,00Ch,0C1h,0CCh,010h,00Ch,0C1h,0DDh,010h,00Dh,0D1h    ; V
        defb 00Dh,0D0h,0DDh,011h,000h,099h,091h,010h,000h,009h,011h,000h,000h,000h,010h,000h
        defb 0CCh,0CCh,0CCh,000h,0CCh,011h,01Ch,0C0h,0CCh,010h,00Ch,0C1h,0DDh,0DDh,0DDh,011h    ; R
        defb 0DDh,01Dh,0D1h,010h,099h,010h,099h,000h,099h,010h,009h,090h,001h,010h,000h,011h

; efectos: prioridad y pares (periodo del tono o del ruido, volumen) por
; imagen; volumen $FF = fin
fxshot: ; disparo de la nave: tono que baja
        defb 1                   ; prioridad
        defb 30,11,44,10,58,9,72,8
        defb 86,6,100,4
        defb 0,0FFh
fxebs:  ; disparo enemigo: tono que sube
        defb 1                   ; prioridad
        defb 150,9,140,9,130,8,120,7
        defb 110,5,100,3
        defb 0,0FFh
fxboom: ; explosion de un enemigo: ruido que se apaga
        defb 1                   ; prioridad
        defb 6,13,7,13,8,12,9,11
        defb 10,10,11,9,12,9,13,8
        defb 14,7,15,6,16,5,17,5
        defb 18,4,19,3,20,2,21,1
        defb 0,0FFh
fxitem: ; mejora recogida: tono que sube
        defb 1                   ; prioridad
        defb 100,12,80,12,62,11,48,11
        defb 36,10,26,9
        defb 0,0FFh
fxhit:  ; disparo que da al jefe: chasquido de ruido
        defb 1                   ; prioridad
        defb 2,11,3,8,4,5
        defb 0,0FFh
fxbomb: ; bomba: ruido grave largo (periodo 24-31, volumen 15 que baja despacio)
        defb 3                   ; prioridad (mayor que fxdie)
        defb 24,15,24,15,24,15,24,15
        defb 24,15,24,14,24,14,24,14
        defb 25,14,25,13,25,13,25,13
        defb 25,13,25,13,25,12,25,12
        defb 26,12,26,12,26,11,26,11
        defb 26,11,26,11,26,11,26,10
        defb 27,10,27,10,27,10,27,9
        defb 27,9,27,9,27,9,27,9
        defb 28,8,28,8,28,8,28,8
        defb 28,7,28,7,28,7,28,7
        defb 29,7,29,6,29,6,29,6
        defb 29,6,29,5,29,5,29,5
        defb 30,5,30,5,30,4,30,4
        defb 30,4,30,4,30,3,30,3
        defb 31,3,31,3,31,3,31,2
        defb 31,2,31,2,31,2,31,1
        defb 0,0FFh
fxdie:  ; explosion de la nave: mas larga y fuerte
        defb 2                   ; prioridad
        defb 8,15,8,15,9,15,9,15
        defb 10,14,10,14,11,14,11,13
        defb 12,13,12,13,13,12,13,12
        defb 14,12,14,11,15,11,15,11
        defb 16,10,16,10,17,10,17,10
        defb 18,9,18,9,19,9,19,8
        defb 20,8,20,8,21,7,21,7
        defb 22,7,22,6,23,6,23,6
        defb 24,5,24,5,25,5,25,5
        defb 26,4,26,4,27,4,27,3
        defb 28,3,28,3,29,2,29,2
        defb 30,2,30,1,31,1,31,1
        defb 0,0FFh

; -------------------------------------------------------------
; waitvs: espera el flanco de subida del VSYNC (bit 0, puerto $AF)
; -------------------------------------------------------------
waitvs: in a,(0afh)
        rrca
        jr c,waitvs
wvs2:   in a,(0afh)
        rrca
        jr nc,wvs2
        ret

; Mapeador (puerto E7h; vale en paginacion simple y completa)
; map_page: A = pagina, C = bloque. Solo pisa A.
map_page:
        push bc
        ld b,a
        and 1fh
        rlca
        rlca
        rlca
        or c
        ld c,MAPPORT
        out (c),a
        pop bc
        ret
; read_page: C = bloque -> A = pagina. Solo pisa A.
read_page:
        push bc
        ld b,c
        ld c,MAPPORT
        in a,(c)
        and 3fh
        pop bc
        ret

; -------------------------------------------------------------
; sprinit: hoja en el bloque SHEET_BLOCK, sin extra, la paleta de 16 de los
; sprites (PAL16) activada y los sprites de la nave, la llama y los
; disparos en formato B, con las 8 filas de la mascara a 1, transparente 0
; y apagados
; -------------------------------------------------------------
sprinit:
        ld a,SHEET_BLOCK
        ld (SPR_BLK),a
        xor a
        ld (SPR_XTR),a
        ld c,0                  ; los 64
si1:    ld a,c
        ld (SPR_SEL),a
        xor a
        ld (SPR_EN),a
        ld (SPR_XH),a
        ld hl,SPR_MSK
        ld b,8
        ld a,0ffh
si2:    ld (hl),a
        inc hl
        djnz si2
        ld a,NAVE_1
        ld (SPR_PG),a
        inc c
        ld a,c
        cp 64
        jr nz,si1
        xor a                   ; PAL16: POKE 2063 puntero, 2064 {G,B},
        ld (2063),a             ; 2066 {BRIGHT,R} (escribe y avanza)
        ld hl,PAL16
        ld b,16
si3:    ld a,(hl)
        ld (2064),a
        inc hl
        ld a,(hl)
        ld (2066),a
        inc hl
        djnz si3
        ld a,1                  ; paleta de 16 activa
        ld (2067),a
        ret

; sproff: los 64 sprites apagados y en formato A, sin extra, la hoja en el
; bloque 2 y la paleta de 16 desactivada (como despues de un reset)
sproff: ld c,0
so1:    ld a,c
        ld (SPR_SEL),a
        xor a
        ld (SPR_EN),a
        ld (SPR_PG),a
        inc c
        ld a,c
        cp 64
        jr nz,so1
        xor a
        ld (SPR_XTR),a
        ld (2067),a
        ld a,2
        ld (SPR_BLK),a
        ret

; gminit: la partida desde el principio: la nave abajo en el centro, recta
; y viva, sin disparos ni enemigos, el marcador a 0 y las oleadas desde la
; primera
gminit: ld a,XSTART
        ld (shx),a
        ld a,YSTART
        ld (shy),a
        ld a,1
        ld (tilt),a
        ld hl,gvars
        ld de,gvars+1
        ld bc,GVLEN-1
        ld (hl),0
        ldir
        ld a,LIVES0
        ld (lives),a
        ld hl,(LVLBUF+LVL_WAVES)
        ld de,LVLBUF
        add hl,de
        ld (wptr),hl
        ret

; -------------------------------------------------------------
; drawspr: los sprites con el estado del juego. En el borrado (acaba antes de
; ~9.800 T tras el flanco: 64 sprites; luego va el ciclo de paleta).
; -------------------------------------------------------------
drawspr:
        ld a,(pstate)
        cp 3
        jp z,dcart              ; fin de partida: el cartel en lugar de la nave
        ld c,SP_BOMB            ; si no, los sprites del cartel que no son de
        call off1               ; la nave ni de la llama, apagados (32-33: luego
        inc c                   ; los iconos de las bombas)
        call off1
        ld c,SP_ITEM            ; la mejora, si hay (si no, apagado)
        ld a,(ityp)
        or a
        jr nz,dit1
        call off1
        jr dit2
dit1:   add a,ITEM1-1
        ld d,a
        ld a,(itx)
        ld e,a
        ld a,(ity)
        ld l,a
        call put1
dit2:   ld a,(pstate)
        cp 2
        jr nz,dn1
        ld a,(ptimer)           ; invulnerable: parpadea (apagada si bit 2)
        and 4
        jr z,dn1
        ld c,SP_LLAMA           ; nave y llama apagadas (49-53)
        ld b,5
dn0:    call off1
        inc c
        djnz dn0
        jr dbal
dn1:    ld a,(pstate)
        dec a
        jr nz,dn2
        ld a,(ptimer)           ; explotando: EXPL + ((EXT - ptimer) / 4) * 4
        ld b,a
        ld a,EXT
        sub b
        and 0ch
        add a,EXPL
        jr dn3
dn2:    ld a,(tilt)             ; viva: la inclinacion
        add a,a
        add a,a
        add a,NAVE
dn3:    ld d,a
        ld a,(shx)
        add a,SOFS
        ld e,a
        ld a,(shy)
        add a,SOFS
        ld l,a
        ld c,SP_NAVE
        call put4
        ld a,(pstate)
        dec a
        jr nz,dn4
        ld c,SP_LLAMA           ; explotando: sin llama
        call off1
        jr dbal
dn4:    ld a,(shx)              ; llama: (x + 4, y + 16)
        add a,SOFS+4
        ld e,a
        ld a,(shy)
        add a,SOFS+16
        ld l,a
        ld a,(tick)
        rrca
        rrca
        and 1
        add a,LLAMA
        ld d,a
        ld c,SP_LLAMA
        call put1
dbal:   ld ix,balas             ; disparos del jugador
        ld c,SP_BALA
ds1:    ld a,(ix+0)
        or a
        jr z,ds2
        ld a,(ix+1)
        add a,SOFS
        ld e,a
        ld a,(ix+2)
        add a,SOFS
        ld l,a
        ld d,BALAJ_0
        call put1
        jr ds3
ds2:    call off1
ds3:    inc ix
        inc ix
        inc ix
        inc c
        ld a,c
        cp SP_BALA+NBALAS
        jr nz,ds1
        ld ix,enes              ; enemigos: el e en los sprites 4e..4e+3
        ld c,SP_ENE
de1:    ld a,(ix+E_ST)
        or a
        jr nz,de2
        call off1               ; libre: los 4 apagados
        inc c
        call off1
        inc c
        call off1
        inc c
        call off1
        inc c
        jr de4
de2:    dec a
        jr nz,de3
        ld e,(ix+E_TYP)         ; vivo: su grafico y la fase (tick / 8) mod 2
        ld d,0
        ld hl,etab
        add hl,de
        ld a,(tick)
        and 8
        rrca
        add a,(hl)
        jr de5
de3:    ld a,(ix+E_TMR)         ; explotando
        ld b,a
        ld a,EXT
        sub b
        and 0ch
        add a,EXPL
de5:    ld d,a
        ld e,(ix+E_X)
        ld l,(ix+E_Y)
        call put4
de4:    ld de,ESZ
        add ix,de
        ld a,(bst)              ; con jefe, los huecos 0-4
        or a
        ld a,SP_ENE+4*NENE
        jr z,de6
        ld a,SP_ENE+4*NSLOTJ
de6:    cp c
        jr nz,de1
        ld a,(bst)              ; el jefe (sprites 20-31)
        or a
        call nz,dboss
        ld ix,ebs               ; disparos enemigos
        ld c,SP_EB
db1:    ld a,(ix+0)
        or a
        jr z,db2
        ld e,(ix+1)
        ld l,(ix+2)
        ld d,BALAE_0
        call put1
        jr db3
db2:    call off1
db3:    inc ix
        inc ix
        inc ix
        inc ix
        inc c
        ld a,c
        cp SP_EB+NEB
        jr nz,db1
        ld c,SP_DIG             ; marcador: con la nave explotando o
        ld a,(pstate)           ; parpadeando, o al final del nivel
        or a
        jr nz,dd0
        ld a,(stimer)           ; o un instante tras recoger una mejora
        or a
        jr nz,dd0
        ld hl,(scr)
        ld de,(lvlend)
        or a
        sbc hl,de
        jr nc,dd0
        ld b,6                  ; si no, apagado: las cifras
dd2:    call off1
        inc c
        djnz dd2
        ld c,SP_VIDA            ; y las vidas
        ld b,3
dd3:    call off1
        inc c
        djnz dd3
        ret
dd0:    ld hl,score
        ld b,3
dd1:    ld a,(hl)
        rrca
        rrca
        rrca
        rrca
        and 0fh
        call digit
        ld a,(hl)
        and 0fh
        call digit
        inc hl
        djnz dd1
        ld a,(lives)            ; las vidas: VIDA en SP_VIDA+i si i < vidas
        ld b,a
        ld c,SP_VIDA
        ld e,SOFS+VIDAX
dv1:    ld a,c
        ld (SPR_SEL),a
        sub SP_VIDA
        cp b
        jr nc,dv2
        ld a,e
        ld (SPR_XL),a
        ld a,SOFS+VIDAY
        ld (SPR_Y),a
        ld a,VIDA
        ld (SPR_PG),a
        ld a,1
        jr dv3
dv2:    xor a
dv3:    ld (SPR_EN),a
        ld a,e
        add a,8
        ld e,a
        inc c
        ld a,c
        cp SP_VIDA+3
        jr nz,dv1
        ld a,(pstate)           ; las bombas: BOMBA en SP_BOMB+i, i < bombas
        cp 3                    ; (no en el fin de partida: 32-33 son del cartel)
        ret z
        ld a,(bombs)
        or a
        ret z
        ld c,SP_BOMB
        ld e,SOFS+BOMBX
        ld l,SOFS+VIDAY
        ld d,BOMBA
        call put1
        ld a,(bombs)
        cp 2
        ret c
        inc c
        ld a,e
        add a,8
        ld e,a
        jp put1

; dcart: el cartel GAME OVER con la tabla cartel (8 sprites: los de la nave y
; la llama, el 54, el 32 y el 33); luego los disparos y lo demas
dcart:  ld hl,cartel
        ld b,8
        call dsign
        jp dbal

; dsign: B letras de la tabla HL (sprite, X, Y, pagina)
dsign:  push bc
        ld c,(hl)
        inc hl
        ld e,(hl)
        inc hl
        ld a,(hl)
        inc hl
        ld d,(hl)
        inc hl
        push hl
        ld l,a
        call put1
        pop hl
        pop bc
        djnz dsign
        ret

; dboss: A = bst. Los sprites 20-31 apagados y luego, segun el estado:
; 1 WARNING (parpadea: bit 4 de btmr), 2 y 3 el jefe (2 fases cada 8 ticks;
; apagado con el bit 1 de bflash), 4 dos explosiones, 5 WELL DONE
dboss:  push af
        ld c,SP_JEFE
        ld b,12
dbo0:   call off1
        inc c
        djnz dbo0
        pop af
        dec a
        jr nz,dbo1
        ld a,(btmr)
        and 16
        ret z
        ld hl,warncar
        ld b,7
        jr dsign
dbo1:   cp 3
        jr c,dbjefe
        jr z,dbexpl
        ld hl,wellcar
        ld b,8
        jr dsign
dbjefe: ld a,(bflash)
        and 2
        ret nz
        ld a,(tick)
        and 8
        jr z,dj0
        ld a,JEFE_STEP
dj0:    add a,JEFE
        ld d,a
        ld c,SP_JEFE
        ld a,(by)
        ld l,a
        ld h,3
dj1:    ld a,(bx)
        ld e,a
        ld b,4
dj2:    call put1
        inc c
        inc d
        ld a,e
        add a,8
        ld e,a
        djnz dj2
        ld a,l
        add a,8
        ld l,a
        dec h
        jr nz,dj1
        ret
dbexpl: ld a,(btmr)             ; EXPL + ((BEXT - btmr) / 2) and 12, y la otra
        ld b,a                  ; 8 por delante, en (X+16, Y+8)
        ld a,BEXT
        sub b
        rrca
        and 0ch
        ld b,a
        add a,EXPL
        ld d,a
        ld a,(bx)
        ld e,a
        ld a,(by)
        ld l,a
        ld c,SP_JEFE
        push bc
        call put4
        pop bc
        ld a,b
        add a,8
        and 0ch
        add a,EXPL
        ld d,a
        ld a,(bx)
        add a,16
        ld e,a
        ld a,(by)
        add a,8
        ld l,a
        ld c,SP_JEFE+4
        jp put4

; digit: la cifra A en el sprite C (el SP_DIG+i en x = DIGX + 8i); C sube 1
digit:  add a,CIFRA
        ld d,a
        ld a,c
        ld (SPR_SEL),a
        sub SP_DIG
        add a,a
        add a,a
        add a,a
        add a,SOFS+DIGX
        ld (SPR_XL),a
        ld a,SOFS+DIGY
        ld (SPR_Y),a
        ld a,d
        ld (SPR_PG),a
        ld a,1
        ld (SPR_EN),a
        inc c
        ret

; put4: un grafico de 16x16 en los sprites C..C+3 (paginas D..D+3: arriba
; a la izquierda, arriba a la derecha, abajo a la izquierda, abajo a la
; derecha) en X = E, Y = L. C sube 4.
put4:   call put1
        inc c
        inc d
        ld a,e
        add a,8
        ld e,a
        call put1
        inc c
        inc d
        ld a,e
        sub 8
        ld e,a
        ld a,l
        add a,8
        ld l,a
        call put1
        inc c
        inc d
        ld a,e
        add a,8
        ld e,a
        call put1
        inc c
        ret

; put1: sprite C en X = E, Y = L, pagina D, encendido (X < 256)
put1:   ld a,c
        ld (SPR_SEL),a
        ld a,e
        ld (SPR_XL),a
        ld a,l
        ld (SPR_Y),a
        ld a,d
        ld (SPR_PG),a
        ld a,1
        ld (SPR_EN),a
        ret

; off1: sprite C apagado
off1:   ld a,c
        ld (SPR_SEL),a
        xor a
        ld (SPR_EN),a
        ret

; -------------------------------------------------------------
; update: una vez por imagen, despues del scroll (un tick de juego):
;  1. la nave (si no esta explotando): O P Q A la mueven un pixel dentro de
;     XMIN-XMAX e YMIN-YMAX y la inclinan segun dx
;  2. los disparos suben VBALA y se borran con y < -7
;  3. si no esta explotando, pfire (ESPACIO)
;  4. upene, upeb, collide, efire, waves, upplay
; -------------------------------------------------------------
update: ld hl,tick
        inc (hl)
        ld hl,stimer            ; el marcador de la mejora recogida, cuenta atras
        ld a,(hl)
        or a
        jr z,up0
        dec (hl)
up0:
        ld a,(pstate)
        cp 1
        jr z,up6                ; explotando o fin de partida: ni se mueve
        cp 3                    ; ni dispara
        jr z,up6
        ld a,0dfh               ; Y U I O P: P bit 0, O bit 1
        in a,(0feh)
        cpl
        ld b,a
        ld c,0                  ; C = dx
        bit 0,b
        jr z,up1
        inc c
up1:    bit 1,b
        jr z,up2
        dec c
up2:    ld a,c                  ; inclinacion: dx + 1
        inc a
        ld (tilt),a
        ld a,(shx)
        add a,c
        cp XMIN
        jr c,up3
        cp XMAX+1
        jr c,up4
        ld a,XMAX
        jr up4
up3:    ld a,XMIN
up4:    ld (shx),a
        ld a,0fbh               ; Q: arriba
        in a,(0feh)
        cpl
        and 1
        ld b,a
        ld a,0fdh               ; A: abajo
        in a,(0feh)
        cpl
        and 1
        sub b                   ; dy
        ld c,a
        ld a,(shy)
        add a,c
        cp YMAX+1
        jr c,up5                ; dentro (0..YMAX)
        bit 7,c
        ld a,YMIN               ; se ha pasado por arriba
        jr nz,up5
        ld a,YMAX               ; o por abajo
up5:    ld (shy),a
up6:    ld ix,balas             ; los disparos suben
        ld b,NBALAS
ub1:    ld a,(ix+0)
        or a
        jr z,ub2
        ld a,(ix+2)
        sub VBALA
        ld (ix+2),a
        add a,7                 ; y < -7: fuera por arriba
        jp p,ub2
        ld (ix+0),0
ub2:    inc ix
        inc ix
        inc ix
        djnz ub1
        ld a,(pstate)
        cp 1
        jr z,up7
        cp 3
        jr z,up7
        call pfire
        call pbomb
up7:    call upene
        call upeb
        call collide
        call upitem
        call efire
        call waves
        call upboss
        jp upplay

; pfire: la espera (cd) baja 1; con ESPACIO pulsado y la espera a 0 sale un
; disparo nuevo en (x + 4, y - 6), en el primer hueco, y cd = TFIRE
pfire:  ld hl,cd
        ld a,(hl)
        or a
        jr z,uf1
        dec (hl)
        ret nz                  ; todavia esperando
uf1:    ld a,07fh               ; ESPACIO (fila B N M . ESPACIO, bit 0)
        in a,(0feh)
        rrca
        ret c                   ; no pulsado
        ld ix,balas
        ld b,NBALAS
uf2:    ld a,(ix+0)
        or a
        jr z,uf3
        inc ix
        inc ix
        inc ix
        djnz uf2
        ret                     ; sin hueco
uf3:    ld e,4                  ; un disparo en x + 4; con el arma, dos: x + 0 en
        ld a,(weap)             ; este hueco y x + 8 en el siguiente libre
        or a
        jr z,uf5
        ld e,0
uf5:    call putbul
        ld a,TFIRE
        ld (cd),a
        ld c,FX_SHOT
        ld hl,fxshot
        call sfxgo
        ld a,(weap)
        or a
        ret z
uf6:    inc ix
        inc ix
        inc ix
        dec b
        ret z                   ; no queda hueco para el segundo
        ld a,(ix+0)
        or a
        jr nz,uf6
        ld e,8
putbul: ld (ix+0),1             ; disparo IX en (x + E, y - 6)
        ld a,(shx)
        add a,e
        ld (ix+1),a
        ld a,(shy)
        sub 6
        ld (ix+2),a
        ret

; pbomb: B (fila B N M . ESPACIO, bit 4), pulsacion nueva y con alguna bomba:
; gasta una; todos los enemigos vivos explotan (con sus puntos), el jefe pierde
; BOMBHP vidas (sin bajar de 1) y suena fxdie
pbomb:  ld a,07fh
        in a,(0feh)
        and 10h
        ld c,a
        ld a,(prevb)
        ld b,a
        ld a,c
        ld (prevb),a
        or a
        ret nz                  ; no pulsada
        ld a,b
        or a
        ret z                   ; ya lo estaba
        ld a,(bombs)
        or a
        ret z
        dec a
        ld (bombs),a
        ld a,SHKN               ; la pantalla tiembla
        ld (shk),a
        ld ix,enes
        ld b,NENE
pb1:    ld a,(ix+E_ST)
        dec a
        call z,ekill
        ld de,ESZ
        add ix,de
        djnz pb1
        ld a,(bst)              ; el jefe, si esta entrando o peleando
        cp 2
        jr c,pb2
        cp 4
        jr nc,pb2
        ld a,6
        ld (bflash),a
        ld a,(bhp)
        sub BOMBHP
        jr z,pb3
        jr nc,pb4
pb3:    ld a,1
pb4:    ld (bhp),a
pb2:    ld c,FX_BOOM
        ld hl,fxbomb
        jp sfxgo

; upene: los enemigos vivos avanzan un paso de su trayectoria (y los de
; suelo, lo que haya avanzado el scroll) y se borran al salir; los que
; explotan cuentan y quedan libres al acabar
upene:  ld ix,enes
        ld b,NENE
ue1:    ld a,(ix+E_ST)
        dec a
        jr z,ue2
        dec a
        jr nz,ue6
        dec (ix+E_TMR)          ; explotando
        jr nz,ue6
        ld (ix+E_ST),0
        jr ue6
ue2:    ld l,(ix+E_PTR)         ; vivo: el paso (dx, dy)
        ld h,(ix+E_PTR+1)
        ld a,(hl)
        bit 0,(ix+E_FLG)        ; WF_MIRROR
        jr z,ue3
        neg
ue3:    add a,(ix+E_X)
        ld (ix+E_X),a
        inc hl
        ld a,(hl)
        add a,(ix+E_Y)
        bit 2,(ix+E_FLG)        ; WF_GROUND: y lo que ha subido el scroll
        jr z,ue4
        ld c,a
        ld a,(nstp)
        add a,c
ue4:    ld (ix+E_Y),a
        ld a,(ix+E_LEFT)        ; el siguiente paso (el ultimo se repite)
        dec a
        jr z,ue5
        ld (ix+E_LEFT),a
        inc hl
        ld (ix+E_PTR),l
        ld (ix+E_PTR+1),h
ue5:    ld a,(ix+E_X)           ; fuera de la zona?
        cp 33
        jr c,ue7
        cp 176
        jr nc,ue7
        ld a,(ix+E_Y)
        cp 16
        jr c,ue7
        cp 132
        jr c,ue6
ue7:    ld (ix+E_ST),0
ue6:    ld de,ESZ
        add ix,de
        djnz ue1
        ret

; upeb: los disparos enemigos bajan EBVY y van dx en X; se borran al salir
upeb:   ld ix,ebs
        ld b,NEB
eb1:    ld a,(ix+0)
        or a
        jr z,eb3
        ld a,(ix+1)
        add a,(ix+3)
        ld (ix+1),a
        cp 24
        jr c,eb2
        cp 176
        jr nc,eb2
        ld a,(ix+2)
        add a,EBVY
        ld (ix+2),a
        cp 132
        jr c,eb3
eb2:    ld (ix+0),0
eb3:    inc ix
        inc ix
        inc ix
        inc ix
        djnz eb1
        ret

; ovl: los intervalos [B,C] y [D,E] se solapan -> NC (C = no)
ovl:    ld a,e
        cp b
        ret c
        ld a,c
        cp d
        ret

; collide: disparos contra enemigos (en el orden de los huecos; cada
; disparo, contra el primer enemigo vivo que toca) y, con la nave viva,
; disparos enemigos y enemigos (no de suelo) contra la nave
collide:
        ld hl,balas
        ld b,NBALAS
cl1:    ld a,(hl)
        or a
        jr z,cl2
        push bc
        push hl
        inc hl
        ld a,(hl)
        add a,SOFS+3
        ld (cbx),a
        inc hl
        ld a,(hl)
        add a,SOFS
        ld (cby),a
        call hitene
        pop hl
        pop bc
        jr nc,cl2
        ld (hl),0               ; ha dado: el disparo se borra
cl2:    inc hl
        inc hl
        inc hl
        djnz cl1
        ld a,(pstate)
        or a
        ret nz                  ; la nave no esta viva
        ld a,(shx)              ; caja de la nave
        add a,SOFS+4
        ld (cpx),a
        ld a,(shy)
        add a,SOFS+3
        ld (cpy),a
        ld ix,ebs               ; disparos enemigos
        ld b,NEB
cl3:    ld a,(ix+0)
        or a
        jr z,cl4
        push bc
        ld a,(ix+1)
        add a,3
        ld d,a
        inc a
        ld e,a
        ld a,(cpx)
        ld b,a
        add a,7
        ld c,a
        call ovl
        jr c,cl3n
        ld a,(ix+2)
        add a,3
        ld d,a
        inc a
        ld e,a
        ld a,(cpy)
        ld b,a
        add a,10
        ld c,a
        call ovl
        jr c,cl3n
        pop bc
        ld (ix+0),0
        jp pdie
cl3n:   pop bc
cl4:    ld de,4
        add ix,de
        djnz cl3
        ld ix,enes              ; enemigos contra la nave
        ld b,NENE
cl5:    ld a,(ix+E_ST)
        dec a
        jr nz,cl6
        bit 2,(ix+E_FLG)        ; los de suelo no chocan
        jr nz,cl6
        push bc
        ld a,(ix+E_X)
        add a,2
        ld d,a
        add a,11
        ld e,a
        ld a,(cpx)
        ld b,a
        add a,7
        ld c,a
        call ovl
        jr c,cl5n
        ld a,(ix+E_Y)
        add a,2
        ld d,a
        add a,11
        ld e,a
        ld a,(cpy)
        ld b,a
        add a,10
        ld c,a
        call ovl
        jr c,cl5n
        pop bc
        call ekill              ; el enemigo tambien explota (y da sus puntos)
        jp pdie
cl5n:   pop bc
cl6:    ld de,ESZ
        add ix,de
        djnz cl5
        ret

; hitene: el disparo [cbx, cbx+1] x [cby, cby+7] contra los enemigos vivos;
; C = ha dado (y el enemigo explota)
hitene: ld ix,enes
        ld b,NENE
he1:    ld a,(ix+E_ST)
        dec a
        jr nz,he3
        push bc
        ld a,(cbx)
        ld b,a
        ld c,a
        inc c
        ld a,(ix+E_X)
        inc a
        ld d,a
        add a,13
        ld e,a
        call ovl
        jr c,he2
        ld a,(cby)
        ld b,a
        add a,7
        ld c,a
        ld a,(ix+E_Y)
        inc a
        ld d,a
        add a,13
        ld e,a
        call ovl
        jr c,he2
        pop bc
        call ekill
        scf
        ret
he2:    pop bc
he3:    ld de,ESZ
        add ix,de
        djnz he1
        or a
        ret

; ekill: el enemigo IX explota y sus puntos van al marcador (BCD)
ekill:  ld (ix+E_ST),2
        ld (ix+E_TMR),EXT
        ld e,(ix+E_TYP)
        ld d,0
        ld hl,ptstab
        add hl,de
        ld a,(hl)
        ld hl,score+2
        add a,(hl)
        daa
        ld (hl),a
        dec hl
        ld a,(hl)
        adc a,0
        daa
        ld (hl),a
        dec hl
        ld a,(hl)
        adc a,0
        daa
        ld (hl),a
        ld c,FX_BOOM
        ld hl,fxboom
        jp sfxgo

; upboss: el jefe, una vez por tick (despues de las oleadas)
upboss: ld a,(bflash)
        or a
        jr z,bo0
        dec a
        ld (bflash),a
bo0:    ld a,(bst)
        or a
        jr nz,bo1
        ld a,(bpend)            ; 0: esperando a que los huecos 5-7 se vacien
        or a
        ret z
        ld a,(enes+5*ESZ+E_ST)
        ld hl,enes+6*ESZ+E_ST
        or (hl)
        ld hl,enes+7*ESZ+E_ST
        or (hl)
        ret nz
        ld a,1
        ld (bst),a
        ld a,WARNT
        ld (btmr),a
        ret
bo1:    dec a
        jr nz,bo2
        ld hl,btmr              ; 1: aviso
        dec (hl)
        ret nz
        ld a,2                  ; entra por arriba
        ld (bst),a
        ld a,(bwx)
        add a,SOFS
        ld (bx),a
        ld a,SOFS-24
        ld (by),a
        ld a,BHP0
        ld (bhp),a
        ld a,(bpath)
        add a,a
        ld e,a
        ld d,0
        ld hl,PATHTAB
        add hl,de
        ld e,(hl)
        inc hl
        ld d,(hl)
        ld a,(de)
        ld (bleft),a
        inc de
        ld (bptr),de
        ret
bo2:    dec a
        jr nz,bo3
        ld hl,(bptr)            ; 2: un paso de la trayectoria
        ld a,(bx)
        add a,(hl)
        ld (bx),a
        inc hl
        ld a,(by)
        add a,(hl)
        ld (by),a
        inc hl
        ld a,(bleft)
        dec a
        jr z,bo2a
        ld (bleft),a
        ld (bptr),hl
        jr bhit
bo2a:   ld a,3                  ; acabada: a pelear
        ld (bst),a
        ld a,1
        ld (bdir),a
        ld a,BFT0
        ld (bft),a
        jr bhit
bo3:    dec a
        jr nz,bo4
        ld a,(bdir)             ; 3: de lado a lado
        ld b,a
        ld a,(bx)
        add a,b
        ld (bx),a
        cp BXMIN
        jr z,bo3a
        cp BXMAX
        jr nz,ub3b
bo3a:   ld a,b
        neg
        ld (bdir),a
ub3b:   ld hl,bft
        dec (hl)
        jr nz,bhit
        ld (hl),BFT0
        ld a,(pstate)           ; dispara si la nave no explota ni es el fin
        cp 1
        jr z,bhit
        cp 3
        jr z,bhit
        ld ix,bfake             ; dos tiros, como los de los enemigos
        ld a,(bx)
        ld (ix+E_X),a
        ld a,(by)
        add a,8
        ld (ix+E_Y),a
        call ebfire
        ld a,(bx)
        add a,16
        ld (ix+E_X),a
        call ebfire
        jr bhit
bo4:    dec a
        jr nz,bo5
        ld hl,btmr              ; 4: explotando
        dec (hl)
        ret nz
        ld a,5
        ld (bst),a
        ld a,CLEART
        ld (btmr),a
        ret
bo5:    ld hl,btmr              ; 5: WELL DONE; luego otra vuelta
        dec (hl)
        ret nz
        jp lap

; bhit: los disparos de la nave contra el jefe (cada uno le quita una vida)
; y la nave viva contra el jefe
bhit:   ld ix,balas
        ld b,NBALAS
bh1:    ld a,(ix+0)
        or a
        jr z,bh2
        push bc
        ld a,(ix+1)             ; [x+35, x+36] x [y+32, y+39]
        add a,SOFS+3
        ld b,a
        inc a
        ld c,a
        ld a,(bx)
        add a,2
        ld d,a
        add a,27
        ld e,a
        call ovl
        jr c,bh1n
        ld a,(ix+2)
        add a,SOFS
        ld b,a
        add a,7
        ld c,a
        ld a,(by)
        add a,2
        ld d,a
        add a,19
        ld e,a
        call ovl
        jr c,bh1n
        ld (ix+0),0             ; tocado: el disparo se borra
        call bdmg
bh1n:   pop bc
        ld a,(bst)
        cp 4
        ret z                   ; ha explotado
bh2:    inc ix
        inc ix
        inc ix
        djnz bh1
        ld a,(pstate)
        or a
        ret nz
        ld a,(shx)              ; la nave: [X+4,X+11] x [Y+3,Y+13]
        add a,SOFS+4
        ld b,a
        add a,7
        ld c,a
        ld a,(bx)
        add a,2
        ld d,a
        add a,27
        ld e,a
        call ovl
        ret c
        ld a,(shy)
        add a,SOFS+3
        ld b,a
        add a,10
        ld c,a
        ld a,(by)
        add a,2
        ld d,a
        add a,19
        ld e,a
        call ovl
        ret c
        jp pdie

; bdmg: el jefe recibe un disparo
bdmg:   ld a,6
        ld (bflash),a
        ld hl,bhp
        dec (hl)
        jr z,bdie
        ld c,FX_BOOM
        ld hl,fxhit
        jp sfxgo
bdie:   ld a,4                  ; sin vidas: explota y 1000 puntos
        ld (bst),a
        ld a,BEXT
        ld (btmr),a
        ld hl,score+1
        ld a,(hl)
        add a,10h
        daa
        ld (hl),a
        dec hl
        ld a,(hl)
        adc a,0
        daa
        ld (hl),a
        ld c,FX_BOOM
        ld hl,fxdie
        jp sfxgo

; lap: el nivel otra vez desde el principio con el mismo marcador y vidas
lap:    ld hl,score
        ld de,lapsv
        ld bc,3
        ldir
        ld a,(lives)
        ld (de),a
        call restart
        ld hl,lapsv
        ld de,score
        ld bc,3
        ldir
        ld a,(hl)
        ld (lives),a
        ret

; pdie: la nave explota
pdie:   xor a                   ; se pierde el arma
        ld (weap),a
        inc a
        ld (pstate),a
        ld a,EXT
        ld (ptimer),a
        ld hl,deaths
        inc (hl)
        ld a,(infl)             ; con vidas infinitas no se pierde ninguna
        or a
        jr nz,pd1
        ld hl,lives
        dec (hl)
pd1:    ld c,FX_BOOM
        ld hl,fxdie
        jp sfxgo

; efire: cada enemigo vivo de un tipo con cadencia cuenta; al llegar a 0
; vuelve a empezar y, si esta en Y 32-92 y la nave no esta explotando,
; dispara
efire:  ld ix,enes
        ld b,NENE
ef1:    ld a,(ix+E_ST)
        dec a
        jr nz,ef3
        ld a,(ix+E_FT)
        or a
        jr z,ef3                ; este tipo no dispara
        dec (ix+E_FT)
        jr nz,ef3
        push bc
        ld e,(ix+E_TYP)
        ld d,0
        ld hl,eftab
        add hl,de
        ld a,(hl)
        ld (ix+E_FT),a
        ld a,(ix+E_Y)
        cp 32
        jr c,ef2
        cp 93
        jr nc,ef2
        ld a,(pstate)
        cp 1
        jr z,ef2
        cp 3
        call nz,ebfire
ef2:    pop bc
ef3:    ld de,ESZ
        add ix,de
        djnz ef1
        ret

; ebfire: un disparo del enemigo IX en (X + 4, Y + 12), en el primer hueco;
; dx = +1 si el centro de la nave esta 16 o mas a la derecha del suyo, -1 si
; 16 o mas a la izquierda, 0 si no
ebfire: ld hl,ebs
        ld b,NEB
eb4:    ld a,(hl)
        or a
        jr z,eb5
        inc hl
        inc hl
        inc hl
        inc hl
        djnz eb4
        ret                     ; sin hueco
eb5:    ld (hl),1
        inc hl
        ld a,(ix+E_X)
        add a,4
        ld (hl),a
        inc hl
        ld a,(ix+E_Y)
        add a,12
        ld (hl),a
        inc hl
        ld a,(ix+E_X)           ; E = centro del enemigo
        add a,8
        ld e,a
        ld a,(shx)              ; D = centro de la nave
        add a,SOFS+8
        ld d,a
        ld c,0
        ld a,e
        add a,16
        ld b,a
        ld a,d
        cp b
        jr c,eb6                ; nave < enemigo + 16
        inc c
        jr eb7
eb6:    add a,16                ; nave + 16 <= enemigo?
        cp e
        jr z,eb8
        jr nc,eb7
eb8:    dec c
eb7:    ld (hl),c
        ld c,FX_EBL             ; el sonido, por el lado del enemigo
        ld a,e
        cp SOFS+80
        jr c,eb9
        inc c
eb9:    ld hl,fxebs
        jp sfxgo

; waves: las oleadas que ya tocan (s >= WAVE_POS) y las que sueltan
; enemigos de uno en uno (espera 0: sale uno y la espera pasa a SPACING)
waves:  ld hl,(wptr)
        ld e,(hl)
        inc hl
        ld d,(hl)
        ld hl,(scr)
        or a
        sbc hl,de
        jr c,wv2
        ld hl,(wptr)
        push hl
        ld de,WAVE_SIZE
        add hl,de
        ld (wptr),hl
        pop hl
        call wstart
        jr waves
wv2:    ld ix,spns
        ld b,NSPN
wv3:    ld a,(ix+0)             ; quedan
        or a
        jr z,wv5
        ld a,(ix+3)             ; espera
        or a
        jr z,wv4
        dec a
        ld (ix+3),a
        jr nz,wv5
wv4:    push bc
        ld l,(ix+1)
        ld h,(ix+2)
        push hl
        ld de,WAVE_X
        add hl,de
        ld c,(hl)
        pop hl
        call spawn
        pop bc
        dec (ix+0)
        ld l,(ix+1)
        ld h,(ix+2)
        ld de,WAVE_SPACING
        add hl,de
        ld a,(hl)
        ld (ix+3),a
wv5:    ld de,SPSZ
        add ix,de
        djnz wv3
        ret

; wsboss: la oleada del jefe HL: se apunta (sale cuando los huecos 5-7
; esten libres)
wsboss: push hl
        ld de,WAVE_X
        add hl,de
        ld a,(hl)
        ld (bwx),a
        pop hl
        ld de,WAVE_PATH
        add hl,de
        ld a,(hl)
        ld (bpath),a
        ld a,1
        ld (bpend),a
        ret

; wsitem: la oleada HL es una mejora (A = tipo 8-10: arma, escudo, bomba):
; una a la vez (si ya hay una, esta se pierde); sale en (WAVE_X, -8) con su
; trayectoria, como los enemigos. ityp = tipo - 7 (1-3; 0 = ninguna)
wsitem: cp 8
        ret c
        sub 7
        ld c,a
        ld a,(ityp)
        or a
        ret nz
        ld a,c
        ld (ityp),a
        push hl
        ld de,WAVE_X
        add hl,de
        ld a,(hl)
        add a,SOFS
        ld (itx),a
        ld a,SOFS-8
        ld (ity),a
        pop hl
        ld de,WAVE_PATH
        add hl,de
        ld a,(hl)
        add a,a
        ld e,a
        ld d,0
        ld hl,PATHTAB
        add hl,de
        ld e,(hl)
        inc hl
        ld d,(hl)
        ld a,(de)
        ld (ileft),a
        inc de
        ld (iptr),de
        ret

; upitem: la mejora avanza un paso de su trayectoria (el ultimo se repite) y
; se borra al salir por abajo (Y >= 132). Con la nave viva o parpadeando, si
; su caja [X+1,X+6] x [Y+1,Y+6] toca la de la nave, se recoge: arma (disparo
; doble hasta perder la nave), escudo (invulnerable SHLDT ticks) o una bomba
; (hasta BOMBMAX); suena fxitem
upitem: ld a,(ityp)
        or a
        ret z
        ld hl,(iptr)
        ld a,(itx)
        add a,(hl)
        ld (itx),a
        inc hl
        ld a,(ity)
        add a,(hl)
        ld (ity),a
        inc hl
        ld a,(ileft)
        dec a
        jr z,ui1
        ld (ileft),a
        ld (iptr),hl
ui1:    ld a,(ity)
        cp 132
        jr c,ui2
        xor a
        ld (ityp),a
        ret
ui2:    ld a,(pstate)
        and 1
        ret nz                  ; explotando o fin de partida
        ld a,(shx)
        add a,SOFS+4
        ld b,a
        add a,7
        ld c,a
        ld a,(itx)
        inc a
        ld d,a
        add a,5
        ld e,a
        call ovl
        ret c
        ld a,(shy)
        add a,SOFS+3
        ld b,a
        add a,10
        ld c,a
        ld a,(ity)
        inc a
        ld d,a
        add a,5
        ld e,a
        call ovl
        ret c
        ld a,SCT                ; recogida: el marcador un instante
        ld (stimer),a
        ld a,(ityp)
        ld b,a
        xor a
        ld (ityp),a
        ld c,FX_SHOT
        ld hl,fxitem
        call sfxgo
        djnz ui3                ; B = 1: arma
        ld a,1
        ld (weap),a
        ret
ui3:    djnz ui4                ; 2: escudo
        ld a,2
        ld (pstate),a
        ld a,SHLDT
        ld (ptimer),a
        ret
ui4:    ld hl,bombs             ; 3: bomba
        ld a,(hl)
        cp BOMBMAX
        ret nc
        inc (hl)
        ret

; wstart: empieza la oleada HL
wstart: push hl
        ld de,WAVE_TYPE
        add hl,de
        ld a,(hl)
        pop hl
        cp 6
        jp z,wsboss             ; el jefe
        jp nc,wsitem            ; 8-10: mejoras
        push hl
        ld de,WAVE_FLAGS
        add hl,de
        ld a,(hl)
        pop hl
        and WF_ROW
        jr z,ws3
        push hl                 ; en fila: todos a la vez
        ld de,WAVE_SPACING
        add hl,de
        ld e,(hl)
        pop hl
        push hl
        ld bc,WAVE_COUNT
        add hl,bc
        ld b,(hl)
        pop hl
        push hl
        push de
        ld de,WAVE_X
        add hl,de
        ld c,(hl)
        pop de
        pop hl
        ld a,b
        or a
        ret z
ws1:    push bc
        push de
        push hl
        call spawn
        pop hl
        pop de
        pop bc
        ld a,c
        add a,e
        ld c,a
        djnz ws1
        ret
ws3:    ld ix,spns              ; de uno en uno: un hueco de NSPN
        ld b,NSPN
ws4:    ld a,(ix+0)
        or a
        jr z,ws5
        ld de,SPSZ
        add ix,de
        djnz ws4
        ld hl,dropw             ; sin hueco: la oleada se pierde
        inc (hl)
        ret
ws5:    push hl
        ld de,WAVE_COUNT
        add hl,de
        ld a,(hl)
        pop hl
        ld (ix+0),a
        ld (ix+1),l
        ld (ix+2),h
        ld (ix+3),0
        ret

; spawn: un enemigo de la oleada HL en x = C (de pantalla), en el primer
; hueco libre, en Y = 16 (y = -16), al principio de su trayectoria.
; Conserva IX.
spawn:  push ix
        ld ix,enes
        ld b,NENE
        ld a,(bpend)            ; con jefe, solo los huecos 0-4
        or a
        jr z,sw1
        ld b,NSLOTJ
sw1:    ld a,(ix+E_ST)
        or a
        jr z,sw2
        ld de,ESZ
        add ix,de
        djnz sw1
        ld hl,drope             ; sin hueco: se pierde
        inc (hl)
        pop ix
        ret
sw2:    ld (ix+E_ST),1
        ld a,c
        add a,SOFS
        ld (ix+E_X),a
        ld (ix+E_Y),SOFS-16
        ld (ix+E_TMR),0
        push hl
        ld de,WAVE_FLAGS
        add hl,de
        ld a,(hl)
        ld (ix+E_FLG),a
        pop hl
        push hl
        ld de,WAVE_TYPE
        add hl,de
        ld a,(hl)
        ld (ix+E_TYP),a
        ld e,a
        ld d,0
        ld hl,eftab
        add hl,de
        ld a,(hl)
        ld (ix+E_FT),a
        pop hl
        ld de,WAVE_PATH
        add hl,de
        ld a,(hl)               ; trayectoria: [n][dx dy]*n
        add a,a
        ld e,a
        ld d,0
        ld hl,PATHTAB
        add hl,de
        ld e,(hl)
        inc hl
        ld d,(hl)
        ld a,(de)
        ld (ix+E_LEFT),a
        inc de
        ld (ix+E_PTR),e
        ld (ix+E_PTR+1),d
        pop ix
        ret

; upplay: la explosion de la nave y la invulnerabilidad cuentan; al acabar
; la explosion, la nave vuelve a su sitio, recta e invulnerable INVT ticks
upplay: ld a,(pstate)
        or a
        ret z
        ld hl,ptimer
        cp 3
        jr nz,pp0
        ld a,(hl)               ; fin de partida: la espera baja hasta 0
        or a
        ret z
        dec (hl)
        ret
pp0:    dec (hl)
        ret nz
        dec a
        jr nz,pp1
        ld a,(lives)            ; acaba la explosion: sin vidas, fin
        or a
        jr z,pp2
        ld a,XSTART
        ld (shx),a
        ld a,YSTART
        ld (shy),a
        ld a,1
        ld (tilt),a
        ld a,2
        ld (pstate),a
        ld a,INVT
        ld (ptimer),a
        ret
pp1:    xor a
        ld (pstate),a
        ret
pp2:    xor a                   ; fin de partida: sin temblor
        ld (shk),a
        ld a,3
        ld (pstate),a
        ld a,GOT
        ld (ptimer),a
        ld a,1                  ; la musica del fin de partida
        jp music

; ficheros de la SD (ordenes 53, 55 y 57 del MCU)
        include "../PHOTO/mcu.asm"

; formatos del nivel, trayectorias y TILEADR (generado por tools/map2bin.py)
        include "out/LEVEL.INC"

; paginas de cada grafico de la hoja y PAL16 (generado por tools/spr2cel.py)
        include "out/HOJA.INC"
LETRAS  equ SHEET_PAGE0+SHEET_CELLS     ; pagina de la G de las letras del cartel (luego A M E O V R)

; -------------------------------------------------------------
; variables y tablas
; -------------------------------------------------------------
nmi16:  defb "NIVEL1.I16",0
nmmap:  defb "NIVEL1.MAP",0
nmhoja: defb "HOJA.BIN",0
nmvgm:  defb "NIVEL1.VGM",0
sv1:    defb 0                  ; pagina del bloque 1 al empezar (la ROM)
prevsp: defb 1
previ:  defb 4
infl:   defb 0                  ; 1 = vidas infinitas (tecla I; hasta salir)
bfake:  defs 4                  ; registro de enemigo para los tiros del jefe (E_X, E_Y)
lapsv:  defs 4                  ; marcador y vidas durante lap
nmgov:  defb "GAMEOVER.VGM",0
vgmok:  defb 0                  ; 1 = el MCU toca (o ha tocado) una musica: STOP_VGM al cambiar o salir
musno:  defb 0                  ; la que suena: 0 la del nivel, 1 la del fin de partida
magi16: defb "I160"
magmap: defb "ELV1"
vofs:   defb 0                  ; fila del anillo arriba (la que se escribe en el VSYNC)
scr:    defw 0                  ; s: pixeles de scroll hechos (arriba, la linea 99+s)
lvlend: defw 0                  ; LVL_END del nivel
mapbase: defw 0                 ; principio del mapa (LVLBUF + LVL_MAP)
speed:  defb 0
fcnt:   defb 0
prevs:  defb 2
prevr:  defb 8
sv6:    defb 0
sv7:    defb 0
errc:   defb 0
fh:     defb 0
hdr:    defs 4
lyoff:  defw 0
cyw:    defb 0                  ; ciclo de la paleta: contadores y giros
cyl:    defb 0
cyg:    defb 0
ofw:    defb 0
ofl:    defb 0
ofg:    defb 0
shx:    defb 0                  ; nave: x, y de pantalla, inclinacion (0-2)
shy:    defb 0
tilt:   defb 0
etab:   defb ENEMY1,ENEMY2,ENEMY3,ENEMY4,ENEMY5,ENEMY6   ; pagina de cada tipo
eftab:  defb 96,0,80,72,36,60   ; ticks entre disparo y disparo (0 = no dispara)
ptstab: defb 10h,20h,30h,40h,60h,50h    ; puntos (BCD)
wptr:   defw 0                  ; siguiente oleada
nstp:   defb 0                  ; pasos de scroll de esta imagen
cbx:    defb 0                  ; colisiones: cajas de trabajo
cby:    defb 0
cpx:    defb 0
cpy:    defb 0
; lo que gminit pone a 0
gvars:
cd:     defb 0                  ; imagenes hasta poder disparar otra vez
tick:   defb 0                  ; contador de ticks (llama, animaciones)
pstate: defb 0                  ; nave: 0 viva, 1 explotando, 2 invulnerable, 3 fin de partida
lives:  defb 0                  ; vidas (con la nave en juego)
bpend:  defb 0                  ; jefe: 1 = su oleada ha salido (huecos 0-4 para los demas)
bst:    defb 0                  ;   0 nada o esperando, 1 aviso, 2 entra, 3 pelea, 4 explota, 5 WELL DONE
btmr:   defb 0
bx:     defb 0                  ;   X, Y de hardware (esquina de arriba a la izquierda)
by:     defb 0
bptr:   defw 0                  ;   trayectoria: paso actual y los que quedan
bleft:  defb 0
bhp:    defb 0
bdir:   defb 0
bft:    defb 0
bflash: defb 0
bwx:    defb 0                  ;   WAVE_X y WAVE_PATH de su oleada
bpath:  defb 0
ityp:   defb 0                  ; mejora: 0 ninguna, 1 arma, 2 escudo, 3 bomba
itx:    defb 0                  ;   X, Y de hardware
ity:    defb 0
iptr:   defw 0                  ;   trayectoria: paso actual y los que quedan
ileft:  defb 0
weap:   defb 0                  ; 1 = disparo doble (hasta perder la nave)
bombs:  defb 0                  ; bombas (0-BOMBMAX)
prevb:  defb 0                  ; B en la imagen anterior (bit 4; 0 = pulsada)
shk:    defb 0                  ; imagenes de temblor que quedan
hofv:   defb 0                  ; hofs (POKE 2074) de la imagen siguiente
stimer: defb 0                  ; ticks que quedan de marcador por una mejora recogida
ptimer: defb 0                  ; ticks que le quedan a la explosion o a la invulnerabilidad
deaths: defb 0                  ; veces que han dado a la nave
drope:  defb 0                  ; enemigos que no cupieron (sin hueco)
dropw:  defb 0                  ; oleadas que no cupieron (NSPN ocupadas)
score:  defs 3                  ; marcador en BCD, la cifra mas alta primero
balas:  defs 3*NBALAS           ; disparos: activo, x, y (con signo)
enes:   defs ESZ*NENE           ; enemigos
ebs:    defs 4*NEB              ; disparos enemigos: activo, X, Y, dx
spns:   defs SPSZ*NSPN          ; oleadas soltando enemigos
fxv:    defs 3*NFX              ; voces de los efectos: puntero (0 = libre), prioridad
gvend:
rowtab: defs 2*RING
lytab:  defs 2*16
codeend:

; detras del codigo, sin ocupar el binario
PALBUF  equ codeend             ; la paleta del .I16 (512 bytes)
PALEND  equ PALBUF+512          ; hasta $7000 (la hoja de los sprites)
GVLEN   equ gvend-gvars

        end start
