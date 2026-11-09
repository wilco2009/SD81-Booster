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

## Estado: fase 7d (título y récord: hecha en el PC, sin probar en la placa)

Lo que hay ahora es el **fondo del nivel 1 con scroll**, la **nave del jugador**
con sus disparos, las **oleadas de enemigos** del nivel, con sus disparos,
colisiones, explosiones y el marcador, el **sonido** (música y efectos) y las
**vidas** con el **fin de partida** (cartel GAME OVER), el **jefe** del final del
nivel, las **mejoras** (arma, escudo y bombas) y, antes de jugar, el **título con
el récord**. La fase 7 va por partes y la 7d (la última) está hecha y comprobada
en el PC, **sin probar en la placa**; la 7c ya está probada.

**Título y récord (7d):**
- `TITULO.I16` (opcional, 160x100 con su paleta, en la misma carpeta; lo hace la
  sesión de herramientas): si está, sale antes de cargar el nivel en el anillo, con el
  **récord** en las 6 cifras de los sprites 58-63 (las del marcador: x 56-103, **y 84**,
  `RECY` en el código; la imagen tiene que dejar libre esa franja y puede poner
  encima la palabra "RECORD"). **ESPACIO** empieza la partida y **X** sale al BASIC.
  Si no está (o no empieza por `I160`), no hay título y el juego empieza directamente.
  `compila.bat` copia `out\TITULO.I16` si existe.
- El **récord va en un fichero de la SD**, no en la EEPROM: `ESTELA.REC`, en la
  carpeta actual (la del juego), de 7 bytes: la firma `EREC` y 3 bytes BCD (la cifra más
  alta primero, como el marcador). Al empezar se lee (órdenes 53, 55 y 57 del MCU);
  si no existe, es más corto o la firma no cuadra, el récord es 0. Al llegar el
  **fin de partida**, si el marcador **lo supera** (estrictamente), se escribe con la
  orden 71 `fcreate` (nombre en ASCII; crea o vacía el fichero), la 56 `fwrite` y la 57;
  si no, no se toca nada. No se guarda al salir con X ni al terminar el nivel. El fichero
  se escribe al acabar la explosión de la última nave: puede notarse una pausa de
  decenas de milisegundos, mientras el MCU crea el fichero. No hace falta tocar el firmware.
La fase 5 no cambió el juego: es la comprobación en el arnés de que, con el límite de
12 sprites por línea del motor, no se pierde ningún trozo de sprite (ver abajo):

- **Carga de la SD** (de la misma carpeta que `ESTELA.BIN`), con la pantalla en
  negro:
  - `NIVEL1.I16`: la paleta de 256 del nivel y el tileset (60 tiles de 16x16), que
    va tal cual a `$C000` (los bloques 6 y 7 se mapean a las páginas 6 y 7 mientras
    corre el juego y se devuelven al salir);
  - `NIVEL1.MAP`: cabecera, oleadas y mapa de 8 tiles de ancho, en una página de
    la SRAM (la 20) que mientras se juega está en el bloque 1 (`$2000`, en lugar
    de la ROM; caben niveles de hasta 7 KB: al final de esa página van las dos paletas
    de 512 bytes, la del nivel (`PALBUF`) y la del título (`TPALBUF`));
  - `HOJA.BIN`: la hoja de los sprites (celdas de 8x8 a 16 colores), en `$7000`;
  - `NIVEL1.VGM` (opcional): la música, que la toca el MCU en bucle sin gastar
    CPU. Si no está, el juego va sin música;
  - `GAMEOVER.VGM` (opcional): la música del fin de partida, que suena una vez (sin
    bucle) en lugar de la del nivel; al volver a empezar vuelve la del nivel.

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
  mejoras (tipos 8-10) las llevan sus propias reglas (ver abajo).
- **Disparos enemigos**: los platillos, cangrejos, giratorios, bombarderos y
  tanques disparan cada cierto tiempo hacia la nave (los dardos no); bajan 2
  píxeles por imagen, inclinados si la nave estaba lejos en horizontal.
- **Colisiones**: un disparo que toca a un enemigo lo hace **explotar** y suma sus
  puntos (10 a 60). Si a la nave le da un disparo enemigo o choca con un enemigo
  (no con los de suelo), la nave **explota**, vuelve abajo en el centro y
  **parpadea** 2 segundos sin que le pase nada.
- **Vidas**: 3 naves. Cada vez que le dan a la nave se pierde una; las que quedan se
  ven con el marcador, como iconos debajo de las cifras. Si explota la última:
  **fin de partida**. La nave no vuelve, los enemigos siguen pasando pero no
  disparan, el marcador se queda a la vista, sale el cartel **GAME OVER** en el
  centro (dos filas, "GAME" y "OVER") y, pasados 2 segundos, **ESPACIO** vuelve a empezar el nivel con 3
  vidas y el marcador a 0.
- **Marcador** de 6 cifras en una fila centrada arriba de la zona de juego, con las
  vidas debajo, solo después de cada muerte (mientras la nave explota y parpadea,
  unos 2,3 segundos), en el fin de partida y al final del nivel. El modo de 256 colores no tiene plano de texto (el de 40x25
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
    segundo), en la misma voz, y una explosión de un enemigo no la corta;
  - **impacto en el jefe**: chasquido corto de ruido (la misma voz).
- **Jefe** (la última oleada del nivel, en `s = 4150`): cartel **WARNING**
  parpadeando 2 segundos; luego el jefe (32x24) baja desde arriba hasta y = 26 y
  va de lado a lado, disparando dos tiros apuntados cada 30 imágenes. Aguanta
  **40 disparos** (parpadea con cada uno) y, si toca a la nave, la nave explota.
  Al derribarlo: dos explosiones, **1000 puntos**, cartel **WELL DONE** 3 segundos
  y el nivel vuelve a empezar con el mismo marcador y las mismas vidas.
- **Mejoras** (las oleadas de tipo 8-10 de `NIVEL1.MAP`: en el nivel de ejemplo, en
  `s` = 700 un arma, en 2310 un escudo y en 3980 una bomba, con la trayectoria `SLOW`):
  un icono de 8x8 cae despacio desde arriba en la X de su oleada, no se puede
  derribar y se pierde si sale por abajo. Una a la vez (si hay una en pantalla, la
  siguiente se pierde). Se recoge **tocándolo con la nave** (viva o parpadeando); suena
  un tono corto que sube y sale el **marcador** (con las vidas y las bombas) 60
  imágenes (poco más de 1 s):
  - **arma**: **disparo doble**, dos disparos por salva (en x + 0 y
    x + 8) hasta que pierdas la nave;
  - **escudo**: **invulnerable 250 imágenes** (unos 5 s), la nave
    parpadea como tras una muerte y, mientras dura, sale el marcador;
  - **bomba**: una bomba más, hasta **2**; se ven como iconos junto a las vidas
    (a la derecha de ellas) cuando sale el marcador. La tecla **B** gasta una: todos
    los enemigos en pantalla explotan (con sus puntos), el jefe pierde 10 de sus 40
    vidas (sin bajar de 1) y suena la explosión grande. Los disparos enemigos no se
    borran. La bomba **se nota**: la pantalla entera tiembla 24 imágenes (los paneles dan
    la vuelta por los lados y los sprites no tiemblan) y suena un ruido grave largo (64
    imágenes, más fuerte que cualquier explosión, que no lo pisan).
  Las bombas y el arma se pierden al volver a empezar; el arma, también al perder la
  nave. En el fin de partida no se ve mejora ni bombas (el cartel usa esos sprites).

## Teclas

| Tecla | Acción |
|-------|--------|
| O P Q A | Mover la nave (izquierda, derecha, arriba, abajo; mantener) |
| B | Bomba (pulsación nueva; si llevas alguna): toda la pantalla **tiembla** 24 imágenes con un ruido grave, los enemigos en pantalla explotan y el jefe pierde 10 vidas |
| ESPACIO | Disparar (mantenido: un disparo cada 6 imágenes). Con `LOAD *JOY "OPQA "` el joystick DB9 hace lo mismo. En el fin de partida, pasados 2 segundos, una pulsación nueva vuelve a empezar |
| S | Velocidad del scroll: la del nivel al empezar (1 píxel por imagen, borde **verde**) → 1 cada 2 imágenes (**cyan**) → 2 por imagen (**amarillo**) → parado (**rojo**) → 1 por imagen |
| R | Volver al principio del nivel, con la nave en su sitio, 3 vidas y el marcador a 0 (la pantalla se queda en negro unas 4 imágenes mientras se repinta) |
| I | Vidas infinitas sí / no, para probar (el borde se pone del mismo color, pero brillante); dura hasta salir |
| X | Salir a BASIC (modo normal, color apagado, páginas de antes) |

Si falta algo, el programa vuelve a BASIC enseguida y escribe `ERROR DE CARGA n`:

| n | Qué pasa |
|---|---|
| 1 | no hay `NIVEL1.I16` en la carpeta |
| 2 | `NIVEL1.I16` no es un `.I16` (no empieza por `I160`) |
| 3 | no hay `NIVEL1.MAP` |
| 4 | `NIVEL1.MAP` no es un nivel (no empieza por `ELV1`) |
| 5 | el nivel pasa de 7 KB (el de ejemplo tiene 2.488 bytes) |
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
  un rato, y mientras tanto sale el **marcador** arriba con las **vidas** que
  quedan (2 iconos tras la primera muerte, 1 tras la segunda).
- A la tercera, **fin de partida**: sin nave, marcador fijo sin iconos, el cartel
  **GAME OVER** en el centro (letras amarillas que pasan a rojo, con sombra) y,
  pasados 2 segundos, ESPACIO vuelve a empezar (hay que soltarlo y
  pulsarlo otra vez).
- Al final del nivel, WARNING parpadeando, el jefe que baja y va de lado a lado
  disparando, parpadea con cada disparo que le da y, a los 40, explota; WELL DONE
  y otra vez el principio del nivel con el marcador y las vidas que había.
- Las **mejoras**: el icono baja despacio y recto; al tocarlo con la nave desaparece,
  suena el tono que sube, sale el marcador un instante y se nota el efecto: con el arma salen **dos disparos** a la
  vez (uno a cada lado de la nave); con el escudo la nave **parpadea unos 5 s** y no
  le hace daño nada (sale el marcador); con la bomba aparece su icono junto a las
  vidas (cuando sale el marcador: al recoger una mejora, tras una muerte o con el escudo). Con B explotan
  todos los enemigos y, con el jefe, aguanta 10 disparos menos. Si se pierde la nave
  vuelve el disparo simple. Una mejora que se deja pasar sale por abajo sin más.
- No desaparecen trozos de sprites (como mucho hay 11 sprites en una misma línea
  y el límite es 12; con el escudo, el marcador se ve 5 s y, si justo entonces
  entran 3 enemigos por arriba bajo las cifras, pueden faltar unas líneas de arriba
  de uno de ellos durante una imagen).
- R vuelve al principio; al final del nivel el fondo se para.
- Suena la **música** (`compila.bat` copia como `NIVEL1.VGM` la de la prueba
  SONIDO, `EXAMPLES/VGM/music0002.vgm`, hasta que haya una del juego) y, a la vez,
  los **efectos**: cada disparo, los de los enemigos por el lado del que salen, las
  explosiones, y la de la nave más larga. Con R los efectos se cortan y la música
  sigue.
- En el **fin de partida** se para la música del nivel y suena `GAMEOVER.VGM` una
  vez (`compila.bat` copia de momento `EXAMPLES/VGM/ledstorm-2.vgm`, la más corta,
  hasta que haya una del juego); al volver a empezar (ESPACIO o R), otra vez la del
  nivel desde el principio.
- Al salir, el BASIC se lee y la música se para.

**Qué hay que mandar:** si se ve y se oye así; si no, qué falla. En particular, de
las mejoras (7c): si se ven bien los tres iconos (arma, escudo, bomba) y se
distinguen, si el tono de recogida suena, si el disparo doble y los 250 de escudo
(5 s) están bien de duración, y si B hace lo que debe (también con el jefe). De
la pelea con el jefe: si 40 disparos son muchos o pocos, si sus tiros (cada 30
imágenes) son demasiados, y si los carteles WARNING y WELL DONE se leen bien. Para
llegar antes al jefe vale S (2 píxeles por imagen), y la tecla I da vidas infinitas.
Las mejoras salen en las posiciones 700, 2310 y 3980 del scroll (con S, antes).
Del título y el récord (7d, con un `TITULO.I16` en `out\`): si el título sale entero y a
la primera, si las 6 cifras del récord caen donde la imagen las espera (x 56-103, y 84),
si ESPACIO empieza y X sale, si tras el primer fin de partida con puntos aparece
`ESTELA.REC` en la carpeta (7 bytes) y la próxima vez el título lo enseña, si la pausa al
guardar se nota, y que no se escribe nada si no se supera el récord.

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
  disparos enemigos, el arnés lo ve. Música: la secuencia entera de órdenes contra un
  modelo: `LOOP_VGM 1` y `PLAY_VGM "NIVEL1.VGM"` después de cargar; en cada fin de
  partida `STOP_VGM`, `LOOP_VGM 0` y `PLAY_VGM "GAMEOVER.VGM"`; al volver a empezar,
  otra vez la del nivel; `STOP_VGM` al salir; ninguna otra orden. Sin `NIVEL1.VGM` o
  sin `GAMEOVER.VGM`, el juego sigue igual y sin el `STOP_VGM` de la que falta; con
  un error de carga, ninguna orden de música. Al salir, los dos AY callados;
- **vidas y fin de partida** en el modelo del juego: los sprites (sin nave, iconos
  de las vidas) siguen iguales al modelo en cada imagen; en las partidas del nivel
  entero hay 2, 5 y 1 fines de partida, y en las que pulsan ESPACIO se vuelve a
  empezar las mismas veces; el cartel (8 sprites con las 7 letras, que se copian
  detrás de `HOJA.BIN`) sale en cada imagen del fin de partida y en ninguna otra;
  ninguna orden de voz;
- el nivel en la página 20 (en el bloque 1 mientras se juega); al salir, el bloque 1
  vuelve a tener su página y la "ROM" (rellenada con un patrón en el arnés) no
  cambia ni un byte;
- el **jefe**, en una partida aparte: un nivel de 10 filas con solo su oleada y vidas
  infinitas (tecla I: la nave tocada 16 veces sin fin de partida, borde brillante), 4.000 imágenes: sale 4 veces
  (aviso, entrada, 160 impactos, 73 salvas, 4 derribados y 3 vueltas al nivel con
  el marcador y las vidas), y los 64 sprites (WARNING, el jefe con sus 2 fases y su
  parpadeo, las explosiones, WELL DONE), los AY, lo visible y la paleta, iguales al
  modelo en cada imagen; como mucho 11 sprites en una línea, sin perder ninguna; los
  sprites escritos antes de 11.300 T;
- las **mejoras** (7c), en dos partidas con niveles hechos en el arnés. **A** (150 filas
  con una oleada de cada tipo y el jefe al final; la nave quieta en x = 72, donde cae
  el icono, y luego en el borde derecho): el modelo (escrito aparte) y los 64 sprites
  coinciden en **cada imagen** (mejora en el 54, iconos de las bombas en el 32-33, disparo
  doble, nave parpadeando, marcador de 60 imágenes al recoger); 2 armas, 2 escudos y 4
  bombas recogidas (la tercera, con el tope de 2), una mejora que se va por abajo, 130 salvas
  dobles, el arma perdida al perder la nave, B sin bombas (no hace nada) y con ellas (3
  enemigos explotan con sus puntos) y **una bomba contra el jefe**. En cada imagen, las
  **vidas del jefe** (`bhp`, leídas de la memoria del Z80) son las del modelo y nunca
  bajan de 1. **Temblor**: `POKE 2074` en cada imagen es el del modelo (tabla de 24
  desplazamientos 3, -3, ... que se apagan, 0 al acabar), con lo visible comprobado
  también durante el temblor (el píxel x sale de la columna (x + hofs) mod 160), y
  `fxbomb` (prioridad 3, 64 pares de ruido de periodo 24-31 con volumen 15 que baja) en
  cada bomba. **B**: el jefe, una bomba cada 100 de scroll y B cada 30 imágenes: 35 bombas
  gastadas, 15 sobre el jefe (5 de ellas con 10 vidas o menos: se queda en 1), el jefe
  sigue pudiendo caer (7 derribados); sin líneas perdidas. Los AY, lo visible y la
  paleta, iguales al modelo; los sprites se escriben antes de **11.617 T** tras el
  flanco (con el jefe, el marcador, las bombas, una mejora y el temblor a la vez). Con
  el escudo y 3 enemigos entrando bajo el marcador el arnés admite, como mucho, 2 imágenes
  con unas líneas de arriba de un enemigo perdidas (en esta partida, 0); en el nivel
  entero real, ninguna. **Pruebas en negativo** (cambiando el binario a propósito; el
  arnés tiene que verlo): el escudo de 200 ticks, el tope de 3 bombas, el segundo disparo
  en x + 9, el temblor de 23 imágenes, el primer desplazamiento del temblor en 4 y la
  bomba que quita 9 vidas al jefe: las seis se detectan (en sprites, en `POKE 2074` o
  en las vidas del jefe);
- el **título y el récord** (7d): el MCU del arnés entiende ahora también la orden 71
  (`fcreate`) y la 56 (`fwrite`) y el fichero se escribe en `sdfiles`; el modelo lee
  `ESTELA.REC` al empezar y sube el récord con cada fin de partida que lo supera. En
  **todas** las ejecuciones se comprueba al salir que hay tantas órdenes 71 y 56 como subidas
  del modelo y que el contenido es exactamente `EREC` + el récord en BCD (o que el fichero
  sigue intacto). Título: con `TITULO.I16` (hecho en el arnés), en su primera espera de teclas
  la paleta, los 16.000 píxeles del anillo, el modo 175 sin desplazar y las cifras `001234` en
  los sprites 58-63 (x 56-103, y `RECY`) son los esperados y los demás sprites están apagados;
  ESPACIO empieza y la partida va como siempre (64 sprites, lo visible y la paleta iguales
  al modelo, con el anillo y los paneles repintados); X en el título sale sin jugar (BC = 0,
  nada de música, todo restaurado); ESPACIO ya pulsado empieza al primer vistazo; un
  `TITULO.I16` sin la firma `I160` se salta y la partida empieza sola; sin `TITULO.I16` no
  hay título (todas las demás pruebas). Récord, con el nivel entero (2 fines de partida):
  sin `ESTELA.REC` se guarda una vez (el segundo marcador, más bajo, no); con `999999` ni una
  orden 71 ni 56; con la firma `XREC` vale 0 y se sobrescribe; con `000100`, se sube. **Pruebas
  en negativo** (binario cambiado a propósito): la firma `FREC` en lugar de `EREC` y
  guardar siempre aunque no se supere: el arnés ve las dos;
- la vuelta más larga del bucle: unos 32.000 T de los 64.584 de una imagen. Sprites escritos
  antes de 11.618 T tras el flanco y ciclo de paleta antes de 11.977 T.

Las fases 1 a 4, la 6, la 7a, la 7b y la 7c están **probadas en la placa** (la 7d, no) (2026-11-08/09). La primera prueba de la fase 1 falló por un `estela.bin` viejo,
y desde entonces el arnés ensambla y prueba el `estela.bin` de esta carpeta, el
mismo que va a la SD.

## Ensamblar y ejecutar

`compila.bat` ensambla con pasmo, comprueba que el binario no pasa de `$6FFF`
(la hoja de los sprites está en `$7000` y el ZX81 no ejecuta código por encima de
`$8000`) y lo copia al emulador (`SD81\SD81\GAMES\ESTELA`) con `out\NIVEL1.I16`,
`out\NIVEL1.MAP`, `out\HOJA.BIN`, `out\TITULO.I16` (si existe) y las músicas como `NIVEL1.VGM` (`out\NIVEL1.VGM` si
existe; si no, `..\VGM\music0002.vgm`) y `GAMEOVER.VGM` (`out\GAMEOVER.VGM`; si no,
`..\VGM\ledstorm-2.vgm`). Para la placa, copia `ESTELA.BIN`, `ESTELA.B81`,
`NIVEL1.I16`, `NIVEL1.MAP`, `HOJA.BIN`, `NIVEL1.VGM`, `GAMEOVER.VGM` y, si lo hay,
`TITULO.I16` a una misma carpeta de la SD y carga `ESTELA.B81`; el juego crea solo
`ESTELA.REC` (el récord) en esa carpeta.

Memoria: código y tablas en `$4800-$6D9D` (9.630 bytes; las trayectorias de `LEVEL.INC` y la
`PAL16` de `HOJA.INC` ya van dentro; quedan **450 bytes libres** hasta `$6F60`;
`compila.bat` deja 10.080 como máximo). La paleta del nivel (`PALBUF`) y la del título
(`TPALBUF`) ya no están detrás del código: van en los últimos 1.024 bytes de la página del nivel
(`$3C00-$3FFF`, bloque 1), que por eso admite niveles de 7 KB en lugar de 8. Las letras W N I L D en
`$6F60-$6FFF` (páginas 123-127), hoja de los sprites en `$7000-$7F1F` y las letras
G A M E O V R detrás, en `$7F20-$7FFF` (bloque 3), anillo en `$8000-$BFBF`,
tileset en `$C000-$FE7F`
(páginas 6 y 7), `NIVEL1.MAP` en la página 20 de la SRAM, mapeada en el bloque 1
(`$2000-$3FFF`) mientras se juega. Al salir, el bloque 1 vuelve a tener la ROM y
se reescribe sobre sí mismo (los mismos bytes), para que la copia de la FPGA de
esas direcciones vuelva a ser la de la ROM, como se hace con `$C000-$FFFF`.
Sprites: enemigos en el 0-31 (4 cada uno), disparos de la nave en el 34-41,
disparos enemigos en el 42-48, la llama en el 49, la nave en el 50-53, las vidas
en el 55-57 y el marcador en el 58-63 (apagados mientras no se ven), la mejora en el
54 y los iconos de las bombas en el 32-33; el cartel
GAME OVER usa los de la nave y la llama, el 54, el 32 y el 33; el jefe, WARNING y
WELL DONE, los 20-31 (mientras hay jefe, los enemigos solo usan los huecos 0-4).

**El emulador** todavía no tiene el scroll en anillo de los modos 160
(`sprites_scroll160_emulador.md`): ahí el fondo no se moverá bien. Hay que probarlo
en la placa.
