# Scrabble en red para el ZX81 (EXAMPLES/NETGAME/scrabble)

Estado: **el juego entero (motor, pantalla, entrada de la jugada, lobby y red) hecho y probado en el PC con un rival simulado** (2026-10-18); falta probarlo en el emulador y la placa, y revisar el diseño de la pantalla con el usuario. Decisiones del usuario: **dos idiomas desde el principio** (inglés y castellano),
**validación automática** (el juego rechaza las palabras que no existen, sin impugnación) y empezar por el
**motor probado en el PC** con el Z80 de verdad (como el ajedrez y el 4 en raya).

Todo lo hace el Z80: reglas, puntuación, búsqueda en el diccionario de la SD (ensayo `EXAMPLES/DICTTEST`: en la
placa 42,5 ms por palabra con 168.551 palabras, 2,59 ms por paso) y la red (MQTT, igual que los otros juegos).

## 1. Idea central: el estado es una función de la semilla y las jugadas

Los dos clientes parten de una **semilla de 4 bytes** (la elige el creador de la sala con `AT+RAND` y va en el
primer mensaje) y barajan con ella la misma bolsa de 100 fichas con un generador sencillo (xorshift de 32 bits,
Fisher-Yates). Las fichas que se roban salen de la bolsa en orden: el que juega roba tantas como ha puesto, y
los dos saben cuántas son. Con eso **cada cliente reconstruye también el atril del rival**: la partida entera
(tablero, atriles, bolsa, puntos) es determinista a partir de la semilla y la lista de jugadas.

- Consecuencia buena: no hace falta mandar atriles ni la bolsa; una jugada son unos pocos bytes.
- Consecuencia a aceptar: **el atril del rival no es secreto frente a quien modifique el programa** (sale de la
  semilla). El programa nunca lo muestra. Es un juego entre amigos con el código de sala; igual que los relojes
  del ajedrez, se asume honradez. Anotarlo en el manual.
- Cambiar fichas: las devueltas van al final de la bolsa y su identidad viaja en el mensaje (así el rival
  sabe qué hay al final). Solo se permite con 7 o más fichas en la bolsa, como en las reglas.

## 2. Datos

| Cosa | Tamaño | Notas |
|---|---|---|
| Tablero | 225 bytes | 15×15; 0 vacío; código de ficha; comodín jugado = código OR $80 |
| Bolsa | 100 bytes | barajada; `BAGP` = siguiente ficha a robar |
| Atriles | 2 × 7 | código de ficha; comodín = `?` |
| Puntos | 2 × 2 | 16 bits |
| Contadores | pocos | pases seguidos, número de jugada, turno, fin |

Alfabeto = **los bytes del diccionario** (el orden que importa es el de los bytes, y `make_dict.py` ya ordena
por bytes). Inglés: `A-Z`. Castellano: `A-Z` más `Ñ`, `CH`, `LL` y `RR` como **una sola ficha cada una** (como el
Scrabble en castellano tradicional); se codifican con bytes fuera de `A-Z` (`[ \ ] ^`) y el dibujo los saca
de la fuente propia del HiRes. Palabras de hasta 15 fichas (registro de 16 bytes).

Tablas por idioma (las dos caben en pocos cientos de bytes): reparto de fichas y valor de cada una. Las casillas
especiales (DL, TL, DW, TW, centro) son las mismas en los dos idiomas y simétricas: se guarda un cuadrante de 8×8.

- Inglés (100): A9 B2 C2 D4 E12 F2 G3 H2 I9 J1 K1 L4 M2 N6 O8 P2 Q1 R6 S4 T6 U4 V2 W2 X1 Y2 Z1 + 2 comodines.
  Valores estándar (K5, J8, X8, Q10, Z10...).
- Castellano (100): A12 B2 C4 CH1 D5 E12 F1 G2 H2 I6 J1 L4 LL1 M2 N5 Ñ1 O9 P2 Q1 R5 RR1 S6 T4 U5 V1 X1 Y1 Z1 + 2
  comodines. Valores: 1 (A E I L N O R S T U), 2 (D G), 3 (B C M P), 4 (F H V Y), 5 (CH Q), 8 (J LL Ñ RR X),
  10 (Z). **Comprobar con una fuente fiable antes de fijarlas** (las del Scrabble de Mattel/FISE).

Diccionarios: `DICTEN.DIC` y `DICTES.DIC` en la carpeta del juego (`make_dict.py`; sin `_` en los nombres).
Inglés: enable1 (dominio público, ya probado). Castellano: **pendiente de elegir la fuente**: una lista de
formas con licencia usable (por ejemplo expandir el diccionario hunspell `es`, LGPL) más la codificación de
`Ñ/CH/LL/RR`. El idioma lo elige quien crea la sala y viaja en el primer mensaje.

## 3. Jugar: entrada y reglas

Una jugada se teclea como en el ajedrez: casilla de salida, sentido (`R` derecha, `D` abajo) y **la palabra
entera tal como quedará**, letras ya puestas incluidas: `H8 R QUIZ`. Las casillas vacías gastan fichas del
atril; si la letra no está en el atril pero hay comodín, se usa el comodín (con `?` delante se fuerza el comodín).
Otras órdenes: pasar, cambiar fichas, rendirse, repetir el último envío.

Validación (todo en el Z80, antes de enviar): dentro del tablero; las letras nuevas salen del atril; al menos una
ficha nueva; la palabra es **la palabra completa** (las casillas antes y después de la palabra están vacías);
la primera jugada cubre la casilla central y tiene ≥2 fichas; las demás tocan alguna ficha ya puesta; la
palabra principal **y todas las cruzadas** (de 2 o más letras) están en el diccionario. Con ≤7 fichas nuevas son
como mucho 8 búsquedas: ~0,35 s en la placa.

Puntuación: casillas especiales solo con las fichas nuevas, las dos multiplicaciones de palabra se acumulan,
las fichas comodín valen 0, +50 por usar las 7 fichas. Fin: un jugador se queda sin fichas con la bolsa
vacía (suma el valor del atril del otro y el otro lo resta) o seis pases seguidos.

## 4. Red: lo que no cabe en `game.inc`

`game.inc` (4 en raya, ajedrez) supone **jugadas de 1 o 2 bytes** y un **estado completo de ≤ 90 bytes en un
solo mensaje**. El Scrabble necesita:

1. **Jugadas de longitud variable** (≤ ~12 bytes): tipo (jugar/pasar/cambiar/rendirse), x, y y sentido, número
   de fichas y las fichas puestas (comodín = código OR $80). Cabe en un mensaje.
2. **Estado completo por trozos** para la resincronización: tablero 225 + atriles + bolsa (la bolsa sale de la
   semilla y del puntero: no hace falta) + puntos y contadores ≈ 250 bytes → 3 mensajes `[3][n][datos]`. El
   módem guarda hasta 8 mensajes recibidos de 128 bytes, así que caben seguidos.
3. El hash del estado de la jugada (`GM_HASH`) se calcula sobre un `g_save` de ≤ 90 bytes: para el Scrabble
   bastan los puntos, el puntero de la bolsa y un resumen del tablero.

Plan: **generalizar `game.inc`** (longitud de jugada variable y estado en trozos) sin romper los otros dos
juegos; las pruebas de los tres juegos en `claude/mqtttest` tienen que seguir en verde.

Guardar y seguir otro día (`savegame.inc`): cada cliente guarda el estado completo en su SD (carpeta del juego,
`SCRABBLE.SAV` sin `_`); igual que en el 4 en raya y el ajedrez.

## 5. Pantalla

HiRes Spectrum (`gfx.inc`): casillas de **8×8 puntos = una celda de atributos**: 15×15 = 120×120 puntos, una
letra de la fuente de 8×8 por casilla y el color de la casilla (premios) en el atributo, sin conflicto de color.
A la derecha, el panel (sala, puntos de los dos, bolsa, última jugada) y **el atril propio en fichas de 16×16**
con su valor (el valor no cabe en 8×8). Debajo, estado, avisos y la entrada de jugada. Colores: TW rojo, DW
rosa, TL azul, DL cian, centro con estrella, la última jugada resaltada.

## 6. Memoria

El código tiene que quedar por debajo de $8000 (el ZX81 no ejecuta por encima). El ajedrez ocupa 9,7 KB desde
$5000; el Scrabble tendrá lobby + red + `game.inc` (compartidos) + dibujo + reglas + búsqueda + tablas. Hay que
vigilar el tamaño desde el principio (`build_z80.sh` ya avisa). Los datos van por encima de $8000, donde sí
pueden estar. Si no cabe, el código se carga desde $4400 (`CLEAR 17407`).

## 7. Etapas

1. **Motor** (`scrabble.inc` + `dict.inc`, extraído de `dicttest.asm` con el nombre del fichero y el handle como
   parámetros): bolsa y generador, atril, análisis de la jugada tecleada, validación, puntuación, fin de
   partida. Pruebas en `claude/mqtttest` contra una referencia en C++ (y el diccionario de enable1).
2. **Pantalla** del tablero y del atril, con el arnés de capturas.
3. **Red**: generalizar `game.inc`, el primer mensaje con la semilla y el idioma, resincronización por trozos,
   guardar/seguir.
4. **Castellano**: lista de palabras, codificación de `Ñ CH LL RR`, tablas.
5. **Placa**: lista de pruebas (`scrabble_pruebas.md`), tiempos con `DICT500K.DIC`.

## 8. Preguntas abiertas

- Fuente y licencia de la lista de palabras en castellano (y si se incluyen formas verbales conjugadas: sí,
  sin ellas el juego es injugable).
- ¿Las fichas `CH`/`LL`/`RR` se mantienen o se deja el castellano con el alfabeto de 27 letras? (Mantenerlas
  complica el teclado: una jugada se teclea con una tecla para cada una.)
- ¿Quién puede cambiar de idioma/diccionario una vez creada la sala? Propuesta: nadie.

## 9. Etapa 1 hecha: el motor (2026-10-18)

- `EXAMPLES/NETGAME/dict.inc` (búsqueda binaria, extraída de `dicttest.asm`; el nombre del fichero y el handle ya no están fijos),
  `EXAMPLES/NETGAME/scrabble.inc` (bolsa con semilla, atriles, `sb_check`/`sb_apply`/`sb_pass`/`sb_exchange`, puntuación, fin).
  Con el diccionario ocupa unos 2,7 KB de Z80 y 440 bytes de variables (`SB_LEN`; el estado de la partida son los primeros 354).
- Pruebas: `claude/mqtttest/scrtest.asm` + `test_z80scrabble.cpp` (en `build_z80.sh`): una referencia en C++ escrita aparte
  (casillas especiales con las coordenadas del tablero oficial, no con la tabla del Z80). Barajadas de la bolsa en los dos idiomas,
  partidas enteras al azar con palabras reales de enable1 (el castellano de prueba es el mismo diccionario con CH, LL, RR y NY cambiados
  por las fichas dobles), comparando TODO el estado tras cada jugada, 3.000 jugadas mal formadas y casos fijos con puntos calculados a mano
  (QUIZ 44, ZEBRA 52, RETAINS 66 con el bingo, el comodín vale 0). Todo coincide. `GAMES=n` cambia el número de partidas por idioma (4 por defecto, ~2 min).
- Detalles de las reglas ya fijados: la jugada se teclea con la palabra entera (`x y sentido n códigos`); el comodín se usa solo si falta la letra
  (o con el bit 7); la primera jugada cubre el centro; seis pases acaban la partida y cada uno resta su atril; al quedarse sin fichas con la
  bolsa vacía, el que acaba suma el atril del otro y el otro lo resta.
- Valores y reparto del castellano **comprobados** con una fuente externa (100 fichas, K y W ausentes, CH/LL/RR siempre una sola
  ficha; la Q a 5 puntos varía algo entre ediciones).

## 10. Diccionario en castellano (2026-10-18)

Se han mirado las dos listas propuestas. `kamilmielnik/scrabble-dictionaries` (carpeta `spanish`) trae **FISE-2 (2009)** y **FILE 2017**,
las listas oficiales de las dos federaciones de Scrabble en español: ~636.600 y ~639.300 palabras en UTF-8, en minúscula, **sin
tildes, con la ñ, con todas las formas conjugadas y los plurales**, ordenadas y sin repetidas. `JorgeDuenasLerin/diccionario-espanol-txt`
es un rastreador de la RAE (sin conjugaciones ni plurales completos, licencia sin declarar): no sirve para jugar.

- **Elegida FISE-2** (la oficial del Scrabble en español, y la que usa las fichas CH/LL/RR). Con la tokenización voraz (CH, LL, RR = una
  ficha, ñ = `[`) **ninguna palabra pasa de 15 fichas** y no se descarta ninguna. FILE 2017 es casi igual (26.000 palabras que FISE-2 no
  tiene y 23.500 al revés) y una de sus palabras tiene 20 fichas: queda como alternativa.
- `python TOOLS/make_dict.py --lang es fise-2.txt DICTES.DIC` (ya soporta `--lang es`) da 636.599 palabras = **9,7 MB**, con ~20 pasos por
  búsqueda (~52 ms en la placa por lo medido con el inglés).
- **Licencia: sin resolver.** El repositorio no declara ninguna (el `LICENSE` es un 404) y las listas son léxicos oficiales publicados por las
  federaciones (FISE-2 viene de redeletras.com; FILE, de un correo de un jugador). Por eso **no se mete `DICTES.DIC` en los zips de la
  release**: se documenta cómo generarlo. El inglés (enable1) es de dominio público.
- La prueba del motor (`test_z80scrabble`) juega con la lista real si se le pasa (`ESLIST=ruta/fise-2.txt sh build_z80.sh`): 13.000
  comprobaciones y partidas enteras en castellano coinciden con la referencia.

## 11. Etapa 2 hecha: la pantalla (2026-10-18)

- `EXAMPLES/NETGAME/scrabble_ui.inc` (con `gfx.inc`): tablero de 15×15 casillas de **una celda** (letra de la fuente de la ROM + color del
  premio en el atributo), atril de 7 fichas de 16×16 (letra al doble de tamaño, con una celda de separación y su valor debajo), panel
  (puntos, bolsa, turno, última jugada, leyenda DL/TL/DW/TW), filas de estado, aviso, ayuda y entrada. La Ñ, CH, LL, RR y la estrella del
  centro se dibujan de tablas propias. La última jugada se resalta en verde. Ocupa ~1,8 KB.
- Pruebas: `claude/mqtttest/scrui.asm` + `test_z80scrabbleui.cpp` (en `build_z80.sh`), con la fuente de la ROM de verdad: comprueba
  letras, colores, fichas del atril, valores, puntos (también negativos y de tres cifras) y textos. Con `SCRDIR=<carpeta>` guarda las
  pantallas (`.SCR`) para `EXAMPLES/NETGAME/scr2png.py`.
- Falta: la entrada de la jugada por teclado (`H8 R QUIZ`), el lobby y la red (etapa 3).

## 12. La entrada de la jugada (2026-10-18)

`EXAMPLES/NETGAME/scrabble_in.inc` (con `scrabble_ui.inc`): se guardan las teclas y tras cada cambio se **reinterpretan enteras**; una tecla que no
cuadra en esa posición no entra, y así borrar, los límites del tablero y la fusión CH/LL/RR salen solos.

- Jugada: `H8 R QUIZ` + ENTER = columna A-O, fila 1-15 (dos cifras: `A10`, `O15`), sentido `R`/`D`, y la palabra **entera tal como quedará**. Los
  espacios no cuentan. SHIFT+0 borra. La palabra no deja pasar de las 15 casillas del tablero.
- Comodín: un punto antes de la letra (`.A`) lo fuerza; sin el punto solo se usa si falta la letra. (El teclado del ZX81 solo deja las teclas
  sin SHIFT, así que no hay `?`; el punto sí es una tecla.)
- Castellano: `C H`, `L L` y `R R` se juntan solos en una ficha (como dicen las reglas: no se pueden hacer con C+H, L+L o R+R); la tecla `1` es la
  N con tilde. Borrar la H deja la C sola, etc.
- `P` + ENTER pasa; `X letras` + ENTER cambia de 1 a 7 fichas (con un punto para el comodín).
- Se dibuja en la fila 23: el texto (`H8 R `) y las fichas con su glifo (una celda por ficha, también CH/LL/RR/Ñ), en amarillo, el comodín forzado en su
  color, y un bloque de cursor; la ayuda (fila 22) cambia con la etapa. `sbi_errmsg` da el aviso de cada error de `sb_check` (en el 8 y el 9, con la
  palabra que falla, y en un cambio el 4 es «la bolsa tiene menos de 7»).
- Teclas que quedan para el juego, como en el ajedrez: `0` salir, `9` repetir el envío; ENTER sin nada tecleado = otra partida si ha acabado.
- Pruebas: en `test_z80scrabbleui.cpp` (~110 comprobaciones de teclas, rechazos, borrado, comodín, pasar, cambiar, castellano, dibujo y avisos).
- Tamaño de todo el Z80 hecho hasta ahora (motor + diccionario + gfx + pantalla + entrada): 6,2 KB. Con lobby, red, `game.inc`, `savegame.inc` y el
  controlador de partida se acercará a los 12 KB que caben entre `$5000` y `$8000`: **si no cabe, cargar desde `$4400` (`CLEAR 17407`)**, que da 15 KB.

## 13. El juego entero: lobby y red (2026-10-18)

`EXAMPLES/NETGAME/scrabble.asm` (+ `scrabble_game.inc`): **12.633 bytes**, se carga en `$4400` (`CLEAR 17407`; desde `$5000` no cabía). Cargador
`SCRABBLE.B81`, `compila.bat` (copia también `DICTEN.DIC`/`DICTES.DIC` desde `build/dict` al emulador), README de `EXAMPLES/NETGAME`.

- **Idioma y semilla desde el código de sala** (no hay mensaje de arranque): la primera letra del código es el idioma (A inglés, B castellano; la
  elige el creador al crear la sala con el gancho `pick_tc` del lobby, como el control de tiempo del ajedrez) y la semilla es un resumen de las
  10 letras y del número de partida (`SB_GNUM`: 0 en una sala nueva, +1 en cada «otra partida»; `SC_RC` recuerda la sala de la última partida).
- **`game.inc` generalizado** sin cambiar el comportamiento de los otros juegos (opciones a 0): `G_MOVE_VAR` (jugadas `[L][tipo][datos]` de hasta 21
  bytes; `GM_MOVE` se reubica porque el hueco de 2 bytes se solapaba con `GM_HASH`) y `G_BIG` (estado de 355 bytes en 4 trozos de 96 bytes;
  `g_hash` del juego; al acabar de mandar el estado se reenvía la última jugada si fue mía). `savegame.inc` guarda además la última jugada entera.
- **Jugada**: tipo 1 jugar (x, y, sentido, n, fichas), 2 pasar, 3 cambiar. `g_valid` carga la jugada en `WB` y usa `sb_check`/`sb_xcheck`; si la pantalla
  ya la comprobó (`SC_PRE`) no se repite. `g_apply` hace la jugada y apunta la última jugada para el resaltado y el panel.
- **Teclas**: con nada tecleado, `Q` sale y `R` reenvía (los dígitos son de las filas del tablero); ENTER vacío = otra partida si ha acabado.
- Prueba (`test_z80scrabblegame.cpp`, ~90 comprobaciones): idioma y código, estado idéntico al de la referencia, avisos, mi jugada (mensaje y hash), la del
  rival, pasar, cambiar, hash que no cuadra → estado por trozos en los dos sentidos, guardar y seguir tras «apagar», fin de partida y otra partida
  (otra semilla), castellano, entrar como jugador 2.
- Lo que la prueba del PC **no** cubre: el aspecto real en el emulador/placa, los tiempos con el diccionario de verdad (la placa: ~40 ms/palabra, hasta unas 8
  búsquedas por jugada), el ancho de banda real de la red con los 4 trozos, y una partida con fichas CH/LL/RR/Ñ en la jugada (el azar no las eligió; el motor y
  la entrada sí están probados con ellas).
- Pendiente tras probarlo: revisar con el usuario el diseño de la pantalla; `DICTES.DIC` real (FISE-2, 9,7 MB) y su licencia; pruebas en la placa.

## 14. El cursor y la flecha de sentido (2026-10-18, a propuesta del usuario)

Sin líneas de separación en el tablero era difícil decir la columna y la fila. Ahora (`scrabble_in.inc`): las flechas del ZX81 **5 6 7 8**
(izquierda, abajo, arriba, derecha) mueven un **cursor**, **0** elige la casilla de salida (teclea por ti la columna y la fila) y **8** (derecha) o
**6** (abajo) elige el sentido. El teclado sigue valiendo (`H8 R QUIZ`); 5-8 y 0 solo son flechas con nada tecleado y 8/6 solo son el sentido en las
etapas 2 y 3 (después de la columna y la fila).

- **Sprites de hardware** (Apéndice H del manual, POKEs 2100-2128; no tocan el tablero ni sus colores): el 0 es el cursor (un marco de 8×8, tinta
  negra con el centro transparente; **parpadea** cada 16 vueltas del bucle **cambiando la máscara a 0** (a propuesta del usuario: así no tapa ni un punto de las
  letras de debajo mientras está apagado, y el color y los píxeles no cambian), el 1 la flecha
  a la derecha y el 2 la de abajo (negras con borde blanco, en la casilla de al lado de la de salida). Máximo 3 sprites, lejos del límite de 12 por línea.
- El cursor empieza en H8, se queda donde estaba tras jugar y sigue a la casilla tecleada a mano. Con pasar, cambiar o la partida acabada no hay cursor;
  al salir (`Q`) se apagan.
- El arnés del PC simula los registros de sprites (`z80net_env.h`: `sprs[]`), así que los tests comprueban dónde está cada sprite, si está encendido y su
  color. `test_z80scrabbleui.cpp` (~40 comprobaciones nuevas) y el test del juego entero (el jugador 2 juega moviendo el cursor).
- Tamaño: 13.225 bytes (+0,55 KB); `SI_BASE` pasa a `$9870`. Pendiente de ver en el emulador: si el marco que parpadea y las flechas se ven bien
  sobre todos los colores, y si hace falta otro color o forma.
