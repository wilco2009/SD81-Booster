# ESTELA — matamarcianos vertical (en desarrollo)

Juego original para el modo **160x100 a 256 colores** del SD81 Booster
(`POKE 2045,175`): una nave que sube por un escenario de tiles que pasa por
debajo con el scroll en anillo de la FPGA, sprites de formato B y música VGM.
El plan entero (fases, mapa de memoria, sprites) está en
`claude/planning/juego160.md`.

| Carpeta / fichero | Qué es |
|---|---|
| `estela.asm`, `ESTELA.B81`, `compila.bat` | el juego (núcleo Z80) |
| `PRUEBAS/` | pruebas de la fase 0 en la placa (carga, VSYNC, sonido, paginación) |
| `tools/`, `assets/`, `out/` | herramientas del PC y gráficos del nivel (las lleva otra sesión; §8 del plan) |

## Estado: fase 6 (sonido)

Lo que hay ahora es el **fondo del nivel 1 con scroll**, la **nave del jugador**
con sus disparos, las **oleadas de enemigos** del nivel, con sus disparos,
colisiones, explosiones y el marcador, y el **sonido**: música y efectos. Faltan el
jefe, las mejoras, las vidas, el fin de partida y la voz de los avisos (fase 7). La
fase 5 no cambió el juego: es la comprobación en el arnés de que, con el límite de
12 sprites por línea del motor, no se pierde ningún trozo de sprite (ver abajo):

- **Carga de la SD** (de la misma carpeta que `ESTELA.BIN`), con la pantalla en
  negro:
  - `NIVEL1.I16`: la paleta de 256 del nivel y el tileset (60 tiles de 16x16), que
    va tal cual a `$C000` (los bloques 6 y 7 se mapean a las páginas 6 y 7 mientras
    corre el juego y se devuelven al salir);
  - `NIVEL1.MAP`: cabecera, oleadas y mapa de 8 tiles de ancho, detrás del código;
  - `HOJA.BIN`: la hoja de los sprites (celdas de 8x8 a 16 colores), en `$7000`;
  - `NIVEL1.VGM` (opcional): la música, que la toca el MCU en bucle sin gastar
    CPU. Si no está, el juego va sin música.

  Los tres los genera `tools/make.py` en `out/` (§8 del plan).
- **Modo 175**, **anillo de 160x102** en `$8000` (100 filas visibles y 2 ocultas
  donde se pinta lo que va a entrar por arriba) y **paneles laterales** de 16
  píxeles sacados de la fila 96 del tileset, iguales en todas las filas: se ven
  **quietos**.
- **Scroll vertical** del mapa del nivel desde su principio (la línea de abajo del
  texto de `NIVEL1.LVL`) a la velocidad del nivel (`LVL_SCROLL`: 1 píxel por
  imagen). Al llegar al final del nivel (`LVL_END`, 4.380 píxeles: algo menos de un
  minuto y medio) el scroll **se para**.
- **Ciclo de paleta**: el agua (colores 224-239) gira una posición cada 4 imágenes,
  la lava (240-247) cada 6 y las luces (248-255) cada 8, en el borrado. El resto de
  la paleta no cambia.
- **Nave** de 16x16 en sprites de formato B (4 sprites, con los colores de la paleta
  de 16 de la hoja), que se inclina hacia donde se mueve, con la **llama del motor**
  debajo (dos fases que se alternan) y **disparos** que suben 3 píxeles por imagen
  (hasta 8 a la vez). La nave se mueve 1 píxel por imagen dentro de la zona de juego
  (entre los paneles) y no sale de la pantalla. Los sprites no se mueven con el
  scroll.
- **Oleadas** de la tabla de `NIVEL1.MAP`: cada una sale cuando el scroll llega a
  su posición, en fila (todos a la vez) o de uno en uno, y cada enemigo sigue su
  trayectoria (`DOWN`, `SINE`, `ZIGZAG`, `DIVE`, `SWEEP`, `HOVER`...), con los
  6 tipos del nivel animados (2 fases). Los de suelo (los tanques y torretas)
  bajan pegados al fondo. Hasta 8 enemigos a la vez. Las oleadas del jefe y de las
  mejoras se saltan por ahora.
- **Disparos enemigos**: los platillos, cangrejos, giratorios, bombarderos y
  tanques disparan cada cierto tiempo hacia la nave (los dardos no); bajan 2
  píxeles por imagen, inclinados si la nave estaba lejos en horizontal.
- **Colisiones**: un disparo que toca a un enemigo lo hace **explotar** y suma sus
  puntos (10 a 60). Si a la nave le da un disparo enemigo o choca con un enemigo
  (no con los de suelo), la nave **explota**, vuelve abajo en el centro y
  **parpadea** 2 segundos sin que le pase nada. De momento no hay vidas: se puede
  seguir sin fin.
- **Marcador** de 6 cifras en una fila centrada arriba de la zona de juego, solo
  después de cada muerte (mientras la nave explota y parpadea, unos 2,3 segundos) y
  al final del nivel. El modo de 256 colores no tiene plano de texto (el de 40x25
  es solo del modo de 16 colores) y con el scroll vertical no se puede dejar una
  franja fija arriba, así que el marcador son sprites.
- **Efectos** en los dos AY de la FPGA (los del ZonX), que no tocan el AY del MCU
  donde suena la música (la prueba SONIDO de la fase 0 lo comprobó en la placa):
  - **disparo** de la nave: tono corto que baja (6 imágenes), canal C del AY A,
    por los dos lados;
  - **disparo enemigo**: tono corto que sube, por el canal A del AY A
    (izquierda) si el enemigo está en la mitad izquierda, o por el B (derecha);
  - **explosión** de un enemigo: ruido que se apaga (16 imágenes), canal C del
    AY B;
  - **explosión de la nave**: ruido más largo y fuerte (48 imágenes, casi 1
    segundo), en la misma voz, y una explosión de un enemigo no la corta.

## Teclas

| Tecla | Acción |
|-------|--------|
| O P Q A | Mover la nave (izquierda, derecha, arriba, abajo; mantener) |
| ESPACIO | Disparar (mantenido: un disparo cada 6 imágenes). Con `LOAD *JOY "OPQA "` el joystick DB9 hace lo mismo |
| S | Velocidad del scroll: la del nivel al empezar (1 píxel por imagen, borde **verde**) → 1 cada 2 imágenes (**cyan**) → 2 por imagen (**amarillo**) → parado (**rojo**) → 1 por imagen |
| R | Volver al principio del nivel, con la nave en su sitio (la pantalla se queda en negro unas 4 imágenes mientras se repinta) |
| X | Salir a BASIC (modo normal, color apagado, páginas de antes) |

Si falta algo, el programa vuelve a BASIC enseguida y escribe `ERROR DE CARGA n`:

| n | Qué pasa |
|---|---|
| 1 | no hay `NIVEL1.I16` en la carpeta |
| 2 | `NIVEL1.I16` no es un `.I16` (no empieza por `I160`) |
| 3 | no hay `NIVEL1.MAP` |
| 4 | `NIVEL1.MAP` no es un nivel (no empieza por `ELV1`) |
| 5 | el nivel no cabe entre el código y `$7000` (caben 2.683 bytes; el de ejemplo tiene 2.488) |
| 6 | la cabecera del nivel no cuadra (menos de 7 filas, o el mapa se sale del fichero) |
| 7 | no hay `HOJA.BIN` |

## Qué tiene que verse

- El fondo del nivel (hierba, agua, lava, metal...) baja **suave, sin saltos ni
  temblores**, a las tres velocidades, con los bordes de los tiles **continuos**:
  ninguna fila repetida, cortada o de otro color al pasar por el borde del anillo
  (cada 102 filas) ni al cambiar de fila de tiles (cada 16).
- El **agua, la lava y las luces se mueven** solas (ciclo de paleta) y el resto de
  colores se queda fijo.
- Por **arriba no entra basura**: lo que aparece ya está pintado.
- Los **paneles** de los lados no se mueven nada.
- La **nave** se ve entera (sin cortes ni colores raros), se inclina al ir a la
  izquierda o a la derecha y vuelve a recta al soltar; la llama parpadea debajo.
  Se para en los cuatro bordes de la zona de juego, sin meterse en los paneles.
- Los **disparos** salen del morro, suben rectos y desaparecen por arriba, uno cada
  6 imágenes con ESPACIO mantenido.
- La nave y los disparos no tiemblan ni se cortan aunque el fondo se mueva.
- Los **enemigos** entran por arriba siguiendo sus trayectorias (serpenteando, en
  zigzag, en picado, cruzando de lado a lado, quietos disparando), se animan y
  salen por los bordes. Los **tanques y torretas** van pegados al suelo, quietos
  respecto al fondo.
- Al darles, **explotan**; al darle a la nave, explota, reaparece abajo y parpadea
  un rato, y mientras tanto sale el **marcador** arriba.
- No desaparecen trozos de sprites (como mucho hay 10 sprites en una misma línea
  y el límite es 12).
- R vuelve al principio; al final del nivel el fondo se para.
- Suena la **música** (`compila.bat` copia como `NIVEL1.VGM` la de la prueba
  SONIDO, `EXAMPLES/VGM/music0002.vgm`, hasta que haya una del juego) y, a la vez,
  los **efectos**: cada disparo, los de los enemigos por el lado del que salen, las
  explosiones, y la de la nave más larga. Con R los efectos se cortan y la música
  sigue.
- Al salir, el BASIC se lee y la música se para.

**Qué hay que mandar:** si se oye así; si no, qué falla. Y, de oído: si los
efectos se oyen bien con la música, si el volumen relativo es razonable, si el
disparo (que suena cada 6 imágenes con ESPACIO pulsado) cansa, y si se nota el lado
de los disparos enemigos.

## Comprobado en el PC

`claude/mqtttest/build_estela.sh`: el Z80 de EightyOne con un modelo de la memoria
paginada, el VSYNC (64.584 T por imagen, 8 líneas, como lo midió `PRUEBAS/VSYNC` en
la placa), el teclado, la paleta, los registros de los sprites, el direccionamiento
del anillo de la FPGA y un MCU que sirve los ficheros de `out/`. El modelo del nivel
lee `NIVEL1.I16` y `NIVEL1.MAP` por su cuenta, y el de la nave está escrito con la
especificación, aparte del código del Z80:

- el tileset de `$C000` es el del `.I16` y el nivel está entero detrás del código;
- en **464 instantes al azar**, lo visible es **exactamente** el nivel desde la
  posición que había cuando se escribió `POKE 2072`, con los paneles;
- nunca se escribe una fila del anillo que se esté viendo (salvo con R, en negro);
- `POKE 2072` sale como mucho 71 T después del flanco del VSYNC, una vez por imagen;
- la paleta, en **cada imagen**: la del `.I16` con el agua, la lava y las luces
  giradas lo que toca; nunca giran dos rangos en la misma imagen, y los giros
  acaban antes de 10.700 T (la imagen empieza hacia los 12.600);
- el **juego, en cada imagen**: un modelo escrito aparte con la especificación (las
  oleadas de `NIVEL1.MAP`, las trayectorias leídas del texto de `LEVEL.INC`, los
  enemigos, sus disparos, las colisiones, las explosiones y el marcador) se compara
  con los **64 sprites** que deja el Z80. En una partida del **nivel entero**
  (4.389 imágenes, disparando y moviéndose sin parar): 28 oleadas, 63 enemigos, 35
  derribados, 12 disparos enemigos, la nave tocada 5 veces, 910 puntos, y los 64
  sprites iguales al modelo en todas las imágenes; el marcador, solo en las 584
  imágenes de después de las muertes. Ningún enemigo se queda sin
  hueco; como mucho 5 enemigos a la vez y 9 sprites en una misma línea;
- el **reparto por línea** (fase 5) con el modelo del motor de sprites
  (`sprites_scroll160_emulador.md` §1.6: del 63 al 0, 12 huecos y el límite de
  171 pasos): en esa partida y en dos más del nivel entero **sin disparar** (quieta
  abajo y moviéndose; los enemigos no caen, la nave muere 12-13 veces y el marcador
  sale en unas 1.400-1.500 imágenes) no se pierde **ninguna línea de sprite**; como
  mucho 10 en una línea;
- el **caso peor** del nivel, solo con el modelo: el nivel entero con los enemigos
  inmortales (ni se derriban ni chocan) da como mucho **3 enemigos en una línea**
  (la oleada 7, en fila de 3). Con la nave (2), la llama (1), un disparo (1) y 2
  disparos enemigos son 12: caben. Con el marcador (6, solo tras una muerte) en sus
  líneas (y 2-9) puede no caber, y entonces lo que se pierde unas imágenes son unas
  líneas de arriba del enemigo de índice más bajo mientras entra. Por eso no hace
  falta rotar los índices de los enemigos. Si un nivel nuevo pone 4 en una línea,
  el arnés falla. Las dos cuentas se prueban también en negativo (14 sprites en una
  línea; la oleada 7 en fila de 5);
- los **sprites de la nave, en cada imagen** (893): la nave (posición, inclinación), la llama
  y cada disparo son los del modelo con las teclas de la imagen anterior, con la
  máscara entera y el transparente 0, y ningún otro sprite encendido; la nave
  llega a los cuatro bordes y se para; 64 disparos salen y se borran arriba, hasta
  5 a la vez; con los 64 sprites se escriben antes de 9.800 T tras el flanco;
- la hoja de `$7000` es `HOJA.BIN`, la paleta de 16 es la de `HOJA.PAL`, activa, y
  la hoja en el bloque 3 durante todo el juego;
- velocidad del nivel y S (1, 1 cada 2, 2 y 0 filas por imagen); R vuelve al
  principio;
- un nivel corto de 10 filas: el scroll se para en `LVL_END` y no pinta más allá
  del mapa;
- los 8 errores de carga vuelven con su número y todo restaurado;
- al salir: modo 85, anillo a 0, `$7FEF = $0C`, páginas de los bloques 6 y 7 como
  estaban, IX, los 64 sprites apagados y en formato A, sin extra, la hoja en el
  bloque 2 y la paleta de 16 desactivada, y nada escrito fuera de su sitio;
- el **sonido**: un modelo de los efectos (las 4 voces, las prioridades, los
  disparos de la nave, los enemigos por su lado y las explosiones del modelo del
  juego, con las tablas de la memoria del Z80) da en **cada imagen** los registros
  de los dos AY de la FPGA (periodos, ruido, volúmenes, mezcladores): iguales en
  todas las partidas (en la del nivel entero, 718 disparos, 12 disparos enemigos,
  35 explosiones y 5 de la nave). Si se cambia en el código el lado de los
  disparos enemigos, el arnés lo ve. Música: `LOOP_VGM 1` y `PLAY_VGM "NIVEL1.VGM"`
  después de cargar, `STOP_VGM` al salir y ninguna otra orden; sin `NIVEL1.VGM`, el
  juego sigue igual y sin `STOP_VGM`; con un error de carga, ninguna orden de
  música. Al salir, los dos AY callados;
- la vuelta más larga del bucle: unos 32.000 T de los 64.584 de una imagen.

Las fases 1 a 4 y la 6 están **probadas en la placa** (2026-11-08). La primera prueba de la fase 1 falló por un `estela.bin` viejo,
y desde entonces el arnés ensambla y prueba el `estela.bin` de esta carpeta, el
mismo que va a la SD.

## Ensamblar y ejecutar

`compila.bat` ensambla con pasmo, comprueba que el binario no pasa de `$6FFF`
(la hoja de los sprites está en `$7000` y el ZX81 no ejecuta código por encima de
`$8000`) y lo copia al emulador (`SD81\SD81\GAMES\ESTELA`) con `out\NIVEL1.I16`,
`out\NIVEL1.MAP`, `out\HOJA.BIN` y la música como `NIVEL1.VGM` (`out\NIVEL1.VGM` si
existe; si no, `..\VGM\music0002.vgm`). Para la placa, copia `ESTELA.BIN`,
`ESTELA.B81`, `NIVEL1.I16`, `NIVEL1.MAP`, `HOJA.BIN` y `NIVEL1.VGM` a una misma
carpeta de la SD y carga `ESTELA.B81`.

Memoria: código y tablas en `$4800-$6384` (las trayectorias de `LEVEL.INC` y la
`PAL16` de `HOJA.INC` ya van dentro), paleta del nivel en `$6385-$6584`,
`NIVEL1.MAP` desde `$6585` (hasta `$6FFF`: caben 2.683 bytes y el nivel tiene
2.488; para la fase 7 hará falta más sitio), hoja de los sprites en `$7000-$7F1F`
(bloque 3), anillo en `$8000-$BFBF`, tileset en `$C000-$FE7F` (páginas 6 y 7).
Sprites: enemigos en el 0-31 (4 cada uno), disparos de la nave en el 34-41,
disparos enemigos en el 42-48, la llama en el 49, la nave en el 50-53 y el
marcador en el 58-63 (apagados mientras no se ve).

**El emulador** todavía no tiene el scroll en anillo de los modos 160
(`sprites_scroll160_emulador.md`): ahí el fondo no se moverá bien. Hay que probarlo
en la placa.
