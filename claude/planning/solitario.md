# SOLITARIO — Klondike con ratón en HiRes 8x1 (plan)

Decidido con el usuario el 2026-11-13. Carpeta: `EXAMPLES/SOLITARIO/` (`solitario.asm`, `SOLITARIO.B81`, `compila.bat`,
`gen_cards.py`, `cartas.png`, `cards.inc`, `README.md`). Todo en código máquina, pasmo (`pasmo -I ..\MOUSE ...`, como MINAS).
Sin `_` en nombres de fichero de la SD.

## 1. Decisiones (del usuario)

- **Vídeo**: Superfast HiRes NATIVO (POKE 2045,171) + Chroma **modo 0** = un atributo por cada byte de línea (8x1).
  Bitmap lineal en HFILE = `$A000` (línea*32 + columna, 6 KB); atributos en `$C000` (fijo, bloque 6), mismo orden.
  Atributo de pantalla: **papel en el nibble alto, tinta en el bajo** (bit 3 de cada nibble = brillo).
  Arranque y apagado: copiar de `EXAMPLES/SCROLLH/scrollh.asm` (POKE 2043/2044, `OUT $7FEF`, POKE 2045, salida con 85).
- **Juego**: Klondike, robar de **1 o de 3** (botón en la barra; cambiar empieza partida nueva).
- **Ratón**: **arrastrar con sprites** (mantener el botón izquierdo). Cursor = sprite 63 de `EXAMPLES/MOUSE/mouselib.inc`.
- **Extras v1**: doble clic y clic derecho mandan la carta a la fundación; **puntos y tiempo** (estilo Windows);
  **autocompletar** y **animación de victoria** (cartas rebotando con estela). Sin deshacer.

## 2. Geometría (píxeles, x siempre múltiplo de 8)

- Carta: **24 x 32** (3 bytes de ancho). Columna k (0-6): x = 16 + 32*k (hueco de 8 entre columnas, margen 16).
- Fila de arriba (y = 2): mazo en col 0, descarte en col 1 (en robo de 3, abanico de hasta 3 cartas con paso de 8 px
  hacia la derecha, ocupa col 1-2), fundaciones en col 3-6.
- Tablero: desde y = 40 hasta y = 182. Barra de estado en y = 184-191 (texto de 8x8 con la fuente de la ROM `$1E00`):
  `SCORE 0000  TIME 00:00   NEW  DRAW 1  QUIT`.
- Solape vertical **dinámico por columna**: tapadas 4 px, destapadas 10 px; si la columna no cabe en 142 px, bajar
  primero el paso de las destapadas (mínimo 7) y luego el de las tapadas (mínimo 2). Peor caso: 6 tapadas + 12 destapadas
  + carta entera.
- Banda visible de una carta solapada (sus 9-10 px de arriba): rango a la izquierda y palo pequeño a la derecha.

## 3. Aspecto de las cartas (8x1 = 2 colores por byte de línea)

- Tapete verde. Cara: papel **blanco brillante**; tinta **roja** (corazones, diamantes) o **negra** (picas, tréboles).
- Sin borde lateral (el contraste con el tapete basta). La **línea 0** de cada carta es una raya negra (separa las cartas
  apiladas) con las esquinas en color tapete (tinta negra, papel verde en ese byte); esquinas redondeadas igual en la
  última línea.
- Centro: palo grande; J/Q/K con la letra grande (o figura simple si cabe). Dorso: trama azul/cian.
- Hueco vacío (mazo agotado, fundación, columna vacía): contorno punteado sobre el tapete.
- Fuente: `gen_cards.py` genera `cartas.png` (hoja a escala 1:1, que el usuario podrá retocar a mano, como
  `piezas_16x16.png` del ajedrez) y de ella `cards.inc`. Rangos A 2-9 10 J Q K en glifos de ~5x7 (el 10 estrecho),
  palos de 7x7, palo grande de ~16x16.
- **Renderizador único con destino y paso**: `draw_card` escribe bitmap + atributo línea a línea en un destino con paso
  configurable (32 = pantalla; 3 = búfer de sprites). Así la carta arrastrada sale de los mismos datos y no hay que leer
  la pantalla. Puede dibujar solo la banda superior (cartas tapadas por otra) o la carta entera.

## 4. Arrastre con sprites

- Al pulsar sobre una carta destapada del tablero (se lleva ella y las de debajo), la carta superior del descarte o la de
  una fundación: se compone el montón en un búfer (alto = solapes + 32, máx. 13 cartas ≈ 152 px = 19 filas x 3 = **57
  sprites** + cursor 63; por línea solo 3 + 1, lejos del límite de 12) y se cargan los sprites 0-56 en **formato A**.
  **Ojo**: el color de sprite es **tinta*16 + papel** (al revés que el atributo de pantalla): girar los nibbles.
  Máscara = 0 en los píxeles de tapete (esquinas), 1 en el resto. Coordenadas de sprite = pantalla + 32 (Apéndice H).
- Después se redibuja la columna de origen sin esas cartas (el montón ya se ve en sprites) y se mueven los sprites con
  el cursor manteniendo el desplazamiento del punto donde se pulsó.
- Al soltar: destino = la columna/fundación sobre la que está el **centro de la carta superior** arrastrada (o el
  cursor, lo que sea más simple y se sienta bien). Si es legal, se mueve en el modelo y se redibujan origen y destino;
  si no, vuelve a su sitio (deslizamiento de unas pocas imágenes o salto directo). Apagar todos los sprites al acabar.
- Pulsar y soltar sin mover (< 3 px): es un **clic**, no un arrastre.
- Antes de volver a BASIC dejar todos los sprites apagados y en página 0.

## 5. Reglas y entrada

- Reparto: 28 cartas en 7 columnas (la última de cada una destapada), 24 al mazo. Barajado Fisher-Yates con un
  generador pseudoaleatorio sembrado con R, FRAMES y la posición del ratón.
- Tablero: alternando color y bajando uno; columna vacía solo admite Rey. Fundación: mismo palo subiendo desde el As.
  Se permite bajar de la fundación al tablero.
- Clic en el mazo: roba 1 o 3 al descarte; con el mazo vacío, clic en el hueco recicla el descarte (sin límite de vueltas).
- Al quedar destapable la última tapada de una columna, se **destapa sola**.
- Doble clic (`ml_` de mouselib, mide con FRAMES) o clic derecho sobre la carta superior de una columna o del descarte:
  a la fundación si es legal.
- Teclas: N = partida nueva, Q = salir (lectura directa del puerto `$FE`, como MINAS, porque en FAST la ROM no explora).
- Barra: NEW, DRAW 1/DRAW 3 (cambia y reparte), QUIT, con zonas de clic.

## 6. Puntos y tiempo (Windows estándar)

descarte→tablero +5, descarte→fundación +10, tablero→fundación +10, destapar carta +5, fundación→tablero -15,
reciclar el descarte -100 (robo de 1) / -20 (robo de 3), -2 cada 10 s; nunca por debajo de 0. Al ganar, si han pasado
más de 30 s, bonificación 700000 / segundos (16 bits no bastan: puntos en 24 o 32 bits, o tope a 65535).
Reloj: FRAMES (`$4034`, en FAST lo simula la FPGA) a 50 Hz; el juego no usa comandos del MCU durante la partida (el
contador del puerto `$AF` no vale, ver la nota del reloj del ajedrez). El reloj empieza con el primer movimiento.

## 7. Autocompletar y victoria

- Autocompletar: cuando mazo y descarte están vacíos y todo el tablero está destapado, aparece `AUTO` en la barra (o se
  lanza solo): mueve una carta por vez a las fundaciones, deslizándola con sprites.
- Victoria: cartas que saltan de las fundaciones (de la K al As) con gravedad y rebote en el suelo, dejando **estela**
  dibujada en el bitmap; x en pasos de 8 px (byte entero, así no hay choque de color), y libre. Cualquier clic o tecla
  -> partida nueva.

## 8. Memoria

- Código en `$5000-$7FFF` (12 KB; nada de código por encima de `$8000`). `SOLITARIO.B81`: `FAST`, `LOAD THEN CLEAR 20479`,
  `LOAD FAST "SOLITARIO.BIN" CODE 20480`, `RAND USR 20480`, `SLOW` (copiar el estilo de `MINAS.B81`).
  `compila.bat` comprueba que el `.bin` no pasa de 12288 B.
- Bloque 4 (`$8000-$9FFF`): variables, modelo (52 cartas: palo*16+rango, bit 7 = destapada; 7 columnas, mazo, descarte,
  4 fundaciones), buzón de mouselib (`MS_BASE`), búfer de sprites del arrastre (57 x 24 B ≈ 1,4 KB), tablas de
  `cards.inc`, pila.
- Bitmap `$A000-$B7FF`, atributos `$C000-$D7FF`.
- Necesita la ROM sin `LOAD *ROMLOCK` (sprites en 2100-2131). No tocar IX, IY, I ni registros alternativos si se
  vuelve a SLOW en algún momento.

## 9. Fases (una sesión Sonnet cada una, de una en una: tocan los mismos ficheros)

0. **Gráficos**: `gen_cards.py` -> `cartas.png` (1:1) + `cards.inc`. Enseñar el PNG al usuario (a 1:1, y a x4 solo para
   el chat) y **parar hasta que lo apruebe**.
1. **Vídeo y dibujo**: arranque del modo 8x1, tapete, `draw_card` con destino/paso, barra de estado, cursor del ratón;
   una mesa de muestra estática con todas las cartas visibles. Copiar a `C:\ClaudeCode\Eightyone2\EightyOne\SD81\SD81\GAMES\SOLITARIO`
   (como MINAS, ESTELA: un juego para un jugador, no en `NETGAMES`).
2. **Modelo y mesa**: barajar, repartir, dibujar la mesa entera con el solape dinámico; prueba de impacto
   (qué carta/hueco hay bajo (x, y)); clic en el mazo (robo 1/3, reciclar).
3. **Arrastre**: sprites, reglas, mover en el modelo y redibujo parcial, destapar sola.
4. **Doble clic / clic derecho, puntos, tiempo, botones** NEW / DRAW / QUIT.
5. **Autocompletar y victoria**.
6. **Pruebas y documentación**: reglas en el arnés del PC si encaja (`claude/mqtttest`, ver `build_z80.sh`), README de la
   carpeta y entrada en `claude/planning/manual_pendiente.md`.

El usuario compila y prueba él mismo en el emulador y la placa: las sesiones pueden ensamblar con pasmo (`compila.bat`
vía PowerShell, no `cmd.exe /c` desde bash), pero no flashear nada. Sin commits hasta que el usuario lo pida.

## Estado

- 2026-11-13: **fase 4 hecha (sin probar con ratón real/placa; sin commit de esta fase)**. `solitario.asm` 5417 B de 12288;
  `SOLITARIO.BIN`/`.B81` ya copiados a `...\SD81\SD81\GAMES\SOLITARIO`. Añade: **clic derecho o doble clic** (`ML_DBL`) sobre
  la carta de arriba de una columna o del descarte -> `quick_found` la manda a la fundación si cabe (As a la primera libre;
  el resto, sobre su palo y rango anterior), reutilizando `commit`; **puntos** estilo Windows (descarte->tablero +5,
  ->fundación +10, destapar +5, fundación->tablero -15, reciclar -100 con robo de 1 / -20 con robo de 3, -2 cada 10 s;
  tope 9999, nunca bajo 0; `add_score`/`sub_score`/`score_move`); **tiempo** mm:ss con FRAMES (`tick_clock`: acumula
  imágenes suba o baje, 50 = 1 s, arranca con el primer movimiento o clic en el mazo, también corre durante el arrastre);
  **barra** repintada con puntos y tiempo (`bar_update`, `bar_txt`, solo cuando cambian) y **botones** NEW (x 152-175),
  D3/D1 (184-199, cambia el robo y reparte) y QUIT (208-239) por `bar_click`; `new_deal` junta reparto + puntos/tiempo a cero
  + mesa + barra (lo usan el arranque, N, D y los botones). Verificado en el PC (`build_solitario.sh`): `quick_found`
  (As de corazones a la primera fundación libre con +10 y +5 por destapar, As de picas a la segunda, un 5 que no cabe no se
  mueve), `bar_update` ("SCORE 1234  02:05") y `tick_clock` (10 s = -2 puntos, también con FRAMES bajando), y las pruebas
  de las fases 1-3 siguen igual (sin regresiones). El arnés ahora permite escribir en el texto de la barra
  (autoescritura en el programa). **No verificado**: clic derecho/doble clic y botones con el ratón real (`main`),
  y que FRAMES avance en FAST en el emulador/placa. Siguiente: fase 5 (autocompletar y victoria).

- 2026-11-13: el usuario confirma en el emulador que, con el arreglo de `col_addr`/`commit`, las tapadas ya se
  destapan solas al quedar expuestas. **Fase 3 dada por buena** (sin probar en placa real; sin commit).
- 2026-11-13: **fallo real encontrado y corregido**: el usuario probó en el emulador (foto: As de corazones movido de
  la 2ª columna a la fundación) confirmando que la carta sí llega a su sitio nuevo pero la tapada que queda debajo no
  se destapa. Tres escenarios sencillos (A, D, E, F, todos con destino una COLUMNA) no lo reproducían porque, por
  coincidencia, la columna de origen siempre era la 0 (índice de columna = índice de la carta a destapar = 0 en los
  dos casos). Al añadir el escenario G (destino una FUNDACIÓN, origen columna 1, exactamente como en la foto) sí
  falló en el PC. Causa: en `commit`, el código que destapa la carta de debajo guardaba su índice en **C** antes de
  llamar a `col_addr` — pero `col_addr` usa C como registro de trabajo (`ld c,a` al principio) y lo pisa con el
  número de columna, así que al volver se marcaba boca arriba la casilla equivocada (la que tenía el número de
  columna en vez de el índice correcto; con origen=0 ambos coinciden por casualidad y el fallo queda oculto). El resto
  del fichero sí guarda estos índices en memoria (`dt_n`) antes de llamar a `col_addr`, un paso más abajo en la misma
  rutina incluido: se cambió el código que fallaba para usar el mismo patrón. `solitario.asm` ahora 4808 B de 12288
  (sin commit); `SOLITARIO.BIN`/`.B81` ya copiados a `...\SD81\SD81\GAMES\SOLITARIO`. Se añadieron 4 escenarios al
  arnés del PC (A/D/E/F con destino columna, que no reproducían el fallo; G con destino fundación, que sí) — con el
  arreglo, los 7 escenarios de la fase 3 pasan y las pruebas de las fases 1 y 2 siguen igual (sin regresiones).
  **Pendiente**: confirmar en el emulador/placa que ahora sí se destapa.
- 2026-11-13: la carpeta de pruebas del emulador pasa de `SD81\test\SOLITARIO` (temporal) a `SD81\SD81\GAMES\SOLITARIO`
  (juegos para un jugador, como MINAS y ESTELA); `compila.bat` ya apunta ahí.
- 2026-11-13: plan escrito.
- 2026-11-13: **fase 3 hecha (sin probar en emulador/placa; sin commit)**. Se retomó el borrador que se había cortado por
  límite de uso (`fase3-borrador.asm.txt`, ya fusionado en `solitario.asm` y borrado): tenía una etiqueta duplicada (`sh_lp`,
  del barajado de la fase 2 y de `spr_hide_all`; se renombró la segunda a `sph_lp`) y tres saltos relativos fuera de rango
  (`jr z/nc,pk_fail` / `cancel_slide` reescritos como `jr <inversa>,salto / jp <destino>`, cambio mecánico que no toca la
  lógica). `solitario.asm` ahora 4803 B de 12288; `SOLITARIO.BIN`/`.B81` ya copiados a `...\SD81\test\SOLITARIO`.
  Añade: `pickup` (agarra la carta destapada del tablero, la de arriba del descarte o de una fundación; la quita del
  modelo y redibuja el origen al momento), `spr_compose` (el montón `dr_cards` a `SBM/SAT/SMK`, $8800/$89E0/$8BC0, 20 filas;
  color de sprite = tinta\*16+papel, máscara a 0 en las esquinas de tapete), `spr_load_all`/`spr_hide_all`/`spr_move`
  (sprites 0-56 formato A, cursor 63), `drag_follow`/`drag_loop` (sigue al cursor, `dr_moved` a partir de 3 px), `drop`
  (clic sin mover = vuelve sin deslizar), `find_target`/`can_place`/`found_slot` (reglas de Klondike: columna vacía solo
  Rey, si no rango+1 y color contrario; fundación: As o rango+1 mismo palo), `commit` (mueve en el modelo, destapa sola la
  carta de debajo en el origen, redibuja) y `cancel_slide` (4 pasos a mitad de camino con `half_to`, cae en `cancel_now`).
  Buzón de arrastre `DRV` en $8400. **Verificado en el PC** (`claude/mqtttest/build_solitario.sh`, llamando a `pickup`/`drop`
  directamente, sin pasar por `drag_loop` que necesita el protocolo del ratón con el MCU; se añadió una VSYNC simulada en
  el puerto $AF, antes fija, para que `cancel_slide` no se cuelgue): un movimiento legal (6H sobre 7S, se añade a la columna
  destino), uno ilegal (6H sobre 7H, mismo color, se cancela y la carta vuelve) y un clic sin arrastrar (vuelve sin
  deslizarse), los tres sin escrituras fuera de sitio; **las pruebas de las fases 1 y 2 se repitieron y siguen pasando
  igual (sin regresiones)**. **No verificado**: el ratón real (`drag_follow`/`drag_loop` con `ml_poll`), el color y la
  máscara de los sprites compuestos (no hay modelo en Python para ellos, a diferencia de `draw_card`), y el emulador/placa.
- 2026-11-13: **fase 2 hecha (sin probar en emulador/placa; sin commit)**. `solitario.asm` ahora 3208 B; `SOLITARIO.BIN` ya
  copiado a `...\SD81\test\SOLITARIO`. Modelo en $8200 (carta = palo*16+rango, bit 7 = boca arriba, igual que el código de
  `draw_card`; `tab` 7x20, `tab_n`, `stock`, `waste`, `waste_vis`, `found_n/found_s`, `draw_mode`), `new_game` (semilla con
  R, FRAMES y posición del ratón) / `shuffle_deal` (Fisher-Yates + reparto 1..7, 24 al mazo), `draw_board` (mesa entera;
  `draw_stock`, `draw_waste` con abanico de 8 px, `draw_found`, `draw_col` con `col_steps`: solape 4/10 y, si no cabe,
  baja primero el de las destapadas hasta `UP_MIN` y luego el de las tapadas hasta 2), `hit_test` (área mazo/descarte/
  fundación/columna + índice + carta de arriba bajo (MP_X, MP_Y) en `ht_area/ht_idx/ht_card`) y `stock_click` (robo de 1/3,
  recicla el descarte sin límite). Teclas provisionales en el bucle: **N** partida nueva, **D** robo de 1/3 (cambia el botón
  `D3` de la barra y reparte), **Q** salir; el clic izquierdo sobre el mazo roba. **Decisión que toca al plan**: `UP_MIN` = 8
  y no 7 (con 7 el índice de 7 líneas queda cortado; con 8 el peor caso mide 140 de 142 y cabe). Verificado en el PC
  (`claude/mqtttest/build_solitario.sh`, Z80 de EightyOne + modelo independiente en Python `cmp_solitario.py`): pantalla
  idéntica a la de referencia tras el reparto y tras cada uno de 10 clics en el mazo (robo de 3 y de 1, incluido el reciclado),
  `hit_test` idéntico en una rejilla de 128x96 puntos en 7 estados, baraja de 52 cartas únicas y semillas distintas = repartos
  distintos, y un tablero a tope (columna de 6 tapadas + 13 destapadas) que acaba en y=180. **No verificado**: el bucle
  principal con el ratón real (clic -> `hit_test` -> `stock_click`), las teclas N/D y el HiRes en emulador/placa.
- 2026-11-13: **fase 1 hecha (sin probar en emulador/placa; sin commit)**. `EXAMPLES/SOLITARIO/solitario.asm` (org $5000,
  2137 B de 12288; `pasmo -I ..\MOUSE`), `SOLITARIO.B81` (estilo MINAS: FAST, CLEAR 20479, CODE 20480) y `compila.bat`
  (comprueba el tamaño y copia `.BIN`/`.B81` a `C:\ClaudeCode\Eightyone2\EightyOne\SD81\test\SOLITARIO`, ya copiados).
  Contiene: arranque HiRes nativo con HFILE=$A000 y Chroma modo 0 (se dibuja antes de poner el modo), `clear_screen`,
  `draw_card` (A=código bit7=boca arriba|palo*16|rango, `CD_HOLE`=$FF hueco; HL bitmap, DE atributos, B líneas; variables
  `dc_pitch` 32/3 y `dc_swap` que gira los nibbles para sprites), `place_card` (A, B líneas, D columna x/8, E y),
  `draw_table` (mesa de muestra desde la tabla `table`, columna/y/código/líneas), barra de estado con la fuente de la ROM
  (`putch`, `zxcode`, `bar_data`), cursor de mouselib (Y máx. 191), salida con Q (cursor apagado, `ml_init` por defecto,
  POKE 2045,85). Memoria: buzón del ratón en $8000, variables en $8100. Verificado en el PC con el Z80 de EightyOne
  (`claude/mqtttest/build_solitario.sh`): la pantalla coincide punto a punto con `preview_mesa.png` y `draw_card` con paso 3
  (con y sin giro de nibbles, banda de 4/10 líneas, dorso, hueco) coincide con el modelo de `gen_cards.py`. **No
  verificado**: ratón/cursor y bucle principal (ml_init habla con el MCU), HiRes real en emulador/placa. Nota:
  `compila.bat` desde la sesión de Claude da "Acceso denegado" (cmd.exe no se puede lanzar ahí); en la consola del usuario
  debería ir como el de MINAS. Para la fase 3: el sprite necesita además la máscara (0 en los puntos de tapete de las
  esquinas), que `draw_card` no genera.
- 2026-11-13: **fase 0 hecha y APROBADA por el usuario** (aspecto de cartas y mesa); la fase 1 puede empezar.
  `EXAMPLES/SOLITARIO/gen_cards.py` (Pillow; modos `png [--force]`, `inc`, `preview`, sin argumentos = inc + preview)
  genera `cartas.png` (264x94, 1:1; casillas editables en blanco/negro + muestras en color), `cards.inc` (258 líneas,
  ensambla con pasmo, 659 B; formato documentado en la cabecera del script) y `preview_mesa.png` (256x192, mesa de
  ejemplo con la regla de 2 colores por byte). Decisiones de diseño: atributo uniforme en los 3 bytes de cada línea de
  carta; esquinas solo con bytes de borde (líneas 0, 1, 30, 31; la 0 negra con atributo $40, las demás tinta blanca sobre
  papel tapete); rango en el byte 0 y palo pequeño en el byte 2 (líneas 2-8), palo/letra grande de 3 bytes (líneas
  12-27); dorso papel azul/tinta cian con marco que se ve en la banda de 4 px; hueco punteado negro.
  **Avisos para el usuario**: (1) el índice ocupa las líneas 2-8, así que el paso de destapadas necesita >= 9 px para
  verse entero; el mínimo 7 del §2 lo recorta, y con mínimo 8 el peor caso (6 tapadas a 2 + 12 destapadas + carta) da
  140 px de 142. (2) la barra del §2 (42 caracteres) no cabe en 32 columnas: la muestra usa `SCORE 0000  00:00  NEW D3
  QUIT`. (3) en el abanico del descarte (paso 8) solo se ve el rango de las cartas de debajo (byte 0).
