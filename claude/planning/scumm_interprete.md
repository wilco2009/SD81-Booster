# Intérprete de scripts SCUMM v2 en el Z80: primer diseño

Fase 2 del [plan](scumm_plan.md). Fecha: 2026-10-21. Datos: Maniac Mansion DOS v2.
Es un diseño inicial. Las cifras de código del Z80 son estimaciones a ojo, sin medir.

## 1. Lo hecho en esta fase

| Herramienta | Qué hace |
|---|---|
| `SCUMM/V2/TOOLS/scummdis.py` | Desensamblador propio de los 256 opcodes de v2. Las tablas salen de `setupOpcodes` y de los manejadores `o2_*`, `o5_*`, `o3_*`, `o4_*` de ScummVM. Es la especificación del intérprete. |
| `SCUMM/V2/TOOLS/compara_descumm.py` | Lo compara con `descumm` sobre todos los scripts. |

Resultado de la comparación (descumm en listado plano, `-i -e -f -w -b`):

- **604 bloques de código y 8578 instrucciones: 0 diferencias** en offset y opcode de cada
  instrucción (es decir, en su longitud) y en el destino de cada salto.
- La única excepción es el script 59, que tiene código muerto tras un `stopScript`. Las dos
  herramientas listan lo mismo hasta ahí.
- `beginOverride` (`58`) consume en ScummVM el salto `18 xx xx` que le sigue; descumm lo lista aparte.
  Lo he tratado como una sola instrucción de 4 bytes.
- Los nombres coinciden con los de descumm (`o2_setStateIntrinsicOn` = `setState08`, etc.).

## 2. Cómo se codifica una instrucción v2

Un opcode de 1 byte y sus operandos. Los bits `0x80`, `0x40` y `0x20` del opcode dicen si el
parámetro 1, 2 o 3 es una variable (1 byte con su número) o un valor inmediato.

| Operando | Bytes | Significado |
|---|---|---|
| `R` | 1 | número de variable donde dejar el resultado |
| `v` | 1 | variable |
| `B1`, `B2`, `B3` | 1 | variable si el bit del opcode está a 1; si no, byte inmediato |
| `W1`, `W2`, `W3` | 1 ó 2 | variable (1 byte) o inmediato de 16 bits con signo |
| `b`, `w` | 1, 2 | byte o palabra inmediatos |
| `J` | 2 | salto relativo con signo, desde el final del operando |
| `S` | n | cadena de diálogo hasta 0; un byte con `(c & 0x7F)` en 4..7 lleva un byte más |
| `N` | n | cadena cruda hasta 0 (nombres de objeto y de verbo) |

Casos especiales: `actorOps`, `verbOps`, `doSentence`, `setVarRange`, `pseudoRoom` (operandos que
dependen de un subopcode) están en `scummdis.py`.

## 3. Qué usa Maniac Mansion

Medido sobre todos los scripts: 8578 instrucciones, 46 KB de código.

| Tipo de bloque | Bloques | Bytes | Mayor |
|---|---|---|---|
| Scripts globales | 169 | 31 430 | 2670 |
| Código de objeto (verbos) | 358 | 13 584 | 262 |
| Entrada de sala | 50 | 638 | 86 |
| Salida de sala | 27 | 322 | 54 |

- **Variables:** la mayor referenciada es la 214, y los bits sueltos viven en las mismas variables. Con
  256 variables de 16 bits bastan **512 bytes**.
- **89 de los 107 manejadores** distintos aparecen en el juego. No aparecen: `addIndirect`,
  `assignVarByte`, `drawSentence`, `getActorElevation`, `getActorWalkBox`, `ifClassOfIs`,
  `ifStatePickupable`, `ifStateTouchable`, `ifStateUntouchable`, `setObjPreposition`,
  `setStatePickupable`, `setStateUnpickupable`, `subIndirect`, `switchCostumeSet`,
  `waitForSentence`, `getActorCostume`, `getActorFacing`, `pseudoRoom`. Para Maniac Mansion se
  pueden dejar para el final, pero Zak v2 puede usar alguno.
- **Más usados:** `stopObjectCode` 980, `isEqual` 697, `startScript` 558, `move` 436, `jumpRelative` 398,
  `delay` 388, `print` 350, `printEgo` 292, `breakHere` 268, `setStateIntrinsicOff` 262, `startSound` 247,
  `animateActor` 227, `setStateIntrinsicOn` 219, `resourceRoutines` 217.
- **Objetos:** 460 en 54 salas, hasta 27 por sala (sala 18). Más 780 entradas globales (objetos que
  pasan a inventario, actores y objetos sueltos). Hay 25 actores en Maniac Mansion.
- **Recursos que no son gráficos:** scripts 32 KB, trajes 50 KB (24), sonidos 24 KB (59), código de
  objeto 14 KB. **Todo junto, unos 121 KB**, y los fondos ya convertidos suman 356 KB.

Consecuencia: los 512 KB de RAM paginada podrían **guardar casi todo el juego cargado desde el
arranque**. Si se precargan scripts, trajes y sonidos, `resourceRoutines` (cargar, bloquear) pasa a ser
una operación vacía. Lo que quedaría por cargar bajo demanda son los gráficos de las salas. A decidir
tras medir el tiempo de lectura desde la SD (ver riesgos).

## 4. Modelo de ejecución (de ScummVM)

- **Ranuras de script:** cada una guarda estado (muerto, en ejecución, pausado), puntero, retardo,
  banderas (resistente a congelación, recursivo) y su origen (sala, inventario). `startScript` toma una
  ranura libre; `stopObjectCode` la libera.
- **Cooperativo:** un script se ejecuta hasta `breakHere`, `delay`, `waitFor*` o `stopObjectCode`. En
  cada fotograma se recorren las ranuras en ejecución. `delay` y `waitForActor` dejan la ranura en
  pausa y repiten la instrucción.
- **Cutscene y override:** `cutscene` congela scripts y cursor; `beginOverride` guarda un punto al que
  saltar cuando el jugador pulsa la tecla de salto.
- **Frases:** hasta 6 `doSentence` en cola. Con subopcode 1 lanzan el script del objeto con el verbo;
  con 2 actualizan el texto de la línea de verbos.
- **Objetos:** cada uno tiene dueño y estado (4 bits cada uno en la tabla global). Los estados
  (`intrinsic`, `touchable`, `locked`, `pickupable`) son bits.
- **Verbos de objeto:** se busca el verbo en la tabla del bloque OBCD y se salta a su offset (o al
  verbo `FF` si no está).

## 5. Diseño para el Z80

### 5.1 Despachador guiado por tabla

Un solo bucle genérico en el núcleo residente. Para cada opcode hay una entrada en una tabla:

```
opcode -> (overlay, dirección del manejador, índice de especificación)
```

La **especificación** es una cadena corta de códigos de operando (`R`, `B1`, `W2`, `J`…). El bucle
genérico los recorre, lee cada operando del script, resuelve las variables y deja los valores en un
array `args[]` de 16 bits. Después llama al manejador, que ya recibe todo resuelto. Así:

- los operandos se decodifican en un solo sitio, ya comprobado contra descumm;
- cada manejador es pequeño;
- hay unas 45 especificaciones distintas para 89 manejadores.

Casos que no caben en una cadena (`actorOps`, `verbOps`, `doSentence`, `setVarRange`, `pseudoRoom`,
`print`) llevan su propio decodificador en el overlay del manejador.

Coste estimado: entre 150 y 400 T por instrucción con 2-3 operandos. A 3,25 MHz son del orden de
10 000 instrucciones por segundo, unas 200 por fotograma a 50 Hz. El 6502 del original a 1 MHz hacía
bastantes menos, así que el margen parece amplio. **Hay que medirlo en el arnés.**

### 5.2 Los scripts son datos

Los scripts se leen como **datos**, no se ejecutan, así que pueden estar en los bloques 4-7. Cada
ranura guarda (página, offset). Para ejecutar una ranura se mapea su página en un bloque de datos
(`OUT (C),r`, unos 30 T). Los scripts conviene empaquetarlos para que ninguno cruce el límite de una
página de 8 KB (el mayor mide 2674 bytes).

### 5.3 Datos en RAM

| Dato | Tamaño | Dónde |
|---|---|---|
| Variables | 256 x 2 = 512 B | bloques 4-7 |
| Dueño y estado de objetos globales | 780 B | bloques 4-7 |
| Ranuras de script | por decidir (ScummVM usa varias decenas) x ~16 B | núcleo o datos |
| Objetos de la sala actual | hasta 27 x ~20 B | datos |
| Actores | 25 x ~24 B | datos |
| Cola de frases, verbos, inventario | unos cientos de bytes | datos |

Todo lo que se escribe tiene que quedar **fuera** de `$3800-$3FFF` y `$C000-$C7FF` (fuente y colores)
y fuera del DFILE, porque la sombra de la FPGA captura todas las escrituras (plan, 3.2).

### 5.4 Reparto en overlays

Basado en las frecuencias de la sección 3. **Estimación a ojo**, a ajustar al ensamblar:

| Dónde | Contenido | Por qué |
|---|---|---|
| Núcleo residente (bloques 2-3) | bucle de despacho y decodificador de operandos; lectura y escritura de variables; saltos y comparaciones (`isEqual`, `isNotEqual`, `jumpRelative`, `equalZero`, etc.); `move`, `increment`; `startScript`, `stopScript`, `stopObjectCode`, `breakHere`, `delay`; planificador de ranuras; trampolines | son el 49 % de las instrucciones del código (recuento estático, no de ejecución) y no deben cruzar ventana |
| Overlay A | estados y dueños de objetos (`setState*`, `ifState*`, `setOwnerOf`, `pickupObject`, `drawObject`); aritmética poco usada | |
| Overlay B | actores y cámara (`putActor`, `walkActorTo`, `animateActor`, `faceActor`, `loadRoom*`, `roomOps`, `panCameraTo`) | llevan los cálculos de cajas y caminos |
| Overlay C | texto y verbos (`print`, `printEgo`, `verbOps`, `doSentence`, cola de frases, cursor) | |
| Overlay D | recursos y sistema (`resourceRoutines`, `startSound`, `stopSound`, `cursorCommand`, `lights`, `saveLoadGame`) | |

La ventana del bloque 0 (8 KB) y la del bloque 1 (6 KB) dan 14 KB de código intercambiable. Un
overlay por ventana en cada momento, con el resto de manejadores a través de trampolines.

## 6. Lo que queda fuera de este diseño

- **Actores y caminos:** el movimiento sobre las cajas de caminar (con la matriz de rutas) y la
  animación de trajes. Es probablemente lo más caro en CPU y en código.
- **Trajes:** formato por parsear. En v2 se animan por piezas.
- **Sonido:** conversión a notas de AY.
- **Cursor y entrada:** teclado y joystick DB9.
- **Gráficos de objetos** (el estado «dibujado» de un objeto) y máscaras de profundidad.

## 7. Riesgos

1. **Número de ranuras de script** y su coste: hay que medir en ScummVM cuántas corren a la vez.
2. **Tiempo de carga desde la SD** de los 121 KB de recursos no gráficos y de los fondos: no medido.
   Decide entre precargar todo o cargar por sala.
3. **Rendimiento del despachador** guiado por tabla: la estimación de 150-400 T por instrucción
   está sin medir.
4. **Corrección de semántica:** ScummVM aplica correcciones específicas de Maniac Mansion v2 (por
   ejemplo en `startScript` y `stopScript`). Hay que decidir cuáles se reproducen.
5. **`doSentence` con operando variable:** el desensamblador supone que un primer operando variable
   no es `FB`/`FC` (que acortan la instrucción). El intérprete lo resuelve en ejecución.

## 8. Siguiente paso

Hecho en la sección 9. Siguiente: las instrucciones que el núcleo todavía manda al anfitrión (actores,
texto, verbos, sonido), empezando por lo que más se usa: `print`, `printEgo`, `startSound`,
`animateActor`, `putActor`, `loadRoom`.

## 9. Despachador en Z80, ejecutado en el PC (2026-10-21)

Está en `SCUMM/V2/ENGINE/vm.asm` (más `tables.inc`, que genera `SCUMM/V2/TOOLS/gen_vmtables.py`). El arnés de PC
es `SCUMM/V2/ENGINE/test/` y se lanza con `build_test.sh`.

**Qué hay**

- Núcleo de **3424 bytes** en `$4200` (resident, bloques 2-3), con ranuras de script, scripts anidados,
  variables (con la redirección de 14-16 y la corrección de la 40), bits, aritmética, saltos y
  comparaciones, estados de objeto, `startScript`/`stopScript`/`chainScript`/`isScriptRunning`, las
  esperas, retardos de 32 bits y el bucle de fotograma (`vm_frame`). Todo guiado por la tabla de despacho.
- 50 manejadores distintos: 48 del núcleo y dos genéricos (`h_host`, `h_host_r`) que sacan el resto de
  opcodes por el puerto `$F1`. En la placa serán los manejadores de los overlays.
- Decodificadores propios para `actorOps`, `verbOps`, `doSentence`, `pseudoRoom` y las cadenas de diálogo.
- El script se lee como dato por la ventana del bloque 4 (`$8000`); cada ranura guarda página y
  puntero, y el cambio de script remapea la página con `OUT (C),r` al puerto `E7h`.

**Cómo se ha comprobado**

El arnés ejecuta `vm.bin` en el Z80 de EightyOne con la memoria paginada de verdad (64 páginas, 8
bloques, bloque 0 de solo lectura) y compara la traza de **2162 escenarios** con la de un modelo
independiente en Python (`SCUMM/V2/TOOLS/vmmodel.py`), que es una transcripción de `script.cpp`,
`script_v2.cpp` y `script_v5.cpp` de ScummVM:

- cada una de las 54 salas (entrada, 30 fotogramas, salida), cada verbo de cada objeto (846 verbos en 358
  objetos) y cada script global (168), con dos semillas de variables;
- 34 escenarios sintéticos (`vmsynth.py`) para lo que los scripts reales casi no tocan: variables
  14-16, la corrección de la 40, comparaciones sin signo, estados de objeto (con las excepciones del
  182 y el 193), retardos de 24 bits, un hijo que mata al padre, scripts que se encadenan o se
  arrancan a sí mismos, errores de rango, y los decodificadores de operandos con subopcodes y cadenas
  con escapes;
- la traza incluye cada llamada al anfitrión (opcode y operandos) y, tras cada orden y cada fotograma,
  un resumen de las variables, los estados de objeto y las ranuras (estado, punteros y retardos).

**Resultado: 2162 de 2162 escenarios idénticos.** Dos escenarios acaban en un bucle sin ceder el
control (error de presupuesto) y se excluyen de la comparación.

**Pruebas de mutación.** Para saber si la comparación detecta fallos, se estropea el Z80 a propósito y se
mira cuántos escenarios cambian. Se hicieron 23 mutaciones (un salto invertido, una variable sin
redirección, el retardo mal calculado, la cadena sin su byte extra, el padre que no se restaura, etc.):

- **20 se detectan**, entre 1 y 1789 escenarios distintos cada una. Varias solo se detectaron tras añadir
  los escenarios sintéticos (las variables 14-16, `startScript(155)`, `stopScript` sobre un padre).
- **3 no se detectan**: no comprobar el estado de la ranura del padre al volver de un script anidado, no
  limpiar `CUR` en `chainScript` (ya lo hace `stop_script`), y no mirar el script 89 en la corrección de
  `startScript(87)`. Las dos primeras no cambian nada observable en estos escenarios; la tercera exige un
  caso (89 en marcha sin 88) que no se ha construido.

**Medidas** (T-states del núcleo de EightyOne, sin contención):

| Qué | Medida |
|---|---|
| Fotograma sin ninguna instrucción de script | **6967 T** (recorrer las 24 ranuras dos veces) |
| Instrucción de script, sin el coste fijo | **1088 T** de media |
| Presupuesto de un fotograma PAL (3,25 MHz, 50 Hz) | 65 000 T |

Lectura: un fotograma vacío se come el 10,7 % del presupuesto, y se pueden ejecutar unas 3000
instrucciones de script por segundo, del orden de 60 por fotograma. Es suficiente para Maniac Mansion
(sus scripts están casi siempre en pausa), pero hay mucho margen de mejora:

- el coste fijo viene de `decrease_delay` y `run_all` con 24 ranuras y `slot_addr` en cada vuelta;
  recorriendo con un puntero y con 8 ranuras bajaría a unos 1500 T. **El máximo de ranuras
  simultáneas medido en los escenarios es 4.**
- `startScript` y `stopScript` recorren todas las ranuras y `h_host` decodifica con el bucle genérico.

**Lo que NO está** (en ScummVM, no en el núcleo ni en el modelo): congelar scripts y las cutscenes,
`beginOverride`, las correcciones específicas de Maniac Mansion en `stopScript`, y los efectos de
`loadRoom`, `doSentence` y los recursos sobre las ranuras. También falta el control de que un script no
exceda su página de 8 KB (lo garantiza el empaquetador, `Packer`).

## 10. Capa de juego: sonido, mensajes, actores y cambio de sala (2026-10-21)

`SCUMM/V2/ENGINE/game.asm` (incluido desde `vm.asm`) y su modelo en `SCUMM/V2/TOOLS/vmgame.py`. Sustituyen al
anfitrión simulado en 13 opcodes: `print`, `printEgo`, `startSound`, `startMusic`, `stopSound`,
`stopMusic`, `isSoundRunning`, `putActor`, `putActorInRoom`, `getActorRoom`, `animateActor`, `actorOps`
y `loadRoom`.

**Criterio:** el núcleo hace la lógica y el estado, y lo que toca hardware sale al *back-end* como
sucesos por el puerto `$F2` (iniciar o parar un sonido, consultarlo, escribir un texto, cargar una sala).
En el PC los recoge el arnés; en la placa serán el MCU (sonido y disco) y el vídeo.

**Qué hace cada cosa** (transcrito de ScummVM):

- **Sonido:** `VAR_LAST_SOUND` (37), `VAR_MUSIC_TIMER` (17) y los sucesos. Si un sonido sigue sonando lo
  dirá el back-end.
- **Mensajes:** el estado de ScummVM (`_haveMsg`, `_talkDelay`, `_keepText`, `_msgCount`),
  `VAR_HAVE_MSG` (3), `VAR_CHARCOUNT` (7) y `VAR_TALK_ACTOR` (41). El texto se convierte al bufer
  (códigos 1 salto, 2 mantener, 3 esperar, 4 variable en decimal, 7 cadena en variables, bit 7 =
  espacio, la arroba se descarta) y se «escribe» entero en el fotograma, con un retardo de
  60 + 6 por carácter. `waitForMessage` espera de verdad.
- **Actores** (25, 16 bytes cada uno): sala, posición, traje, dirección, fotograma, movimiento y si
  están visibles. Mostrar, ocultar, `putActor`, cambiar de traje, la animación de hablar y las
  animaciones de `animateActor` (parar, girar, dirección inmediata, fotograma).
- **`loadRoom` (`startScene`):** deja de hablar, ejecuta la salida de la sala que se deja, mata los
  scripts de sala y locales, reinicia la frase, oculta los actores, pone `VAR_ROOM` y las variables de
  cámara, carga la sala, muestra los actores que estén en ella, ejecuta la entrada y arranca el script 5.
  Cambiar a la sala en la que ya se está no hace nada.
- **Fotograma** (`vm_frame`): retardos de scripts, retardo de mensaje, `displayDialog`, `VAR_HAVE_MSG`,
  scripts. El incremento de tiempo se limita a 15, como en `scummLoop`.

**Dato que lo acota:** en todos los scripts de Maniac Mansion las cadenas de `print` solo usan los
códigos 1, 2, 3, 4 y 7, de modo que no hacen falta los nombres de verbos ni de objetos (códigos 5 y 6)
para que el tiempo de los mensajes salga bien.

**Lo que NO está** (el modelo y el Z80 coinciden en ello, pero no es lo de ScummVM):

- El ajuste de la posición a las cajas de caminar al poner un actor (`adjustActorPos`).
- El giro y la caminata por fotograma: `animateActor` deja el movimiento marcado pero nada lo termina,
  así que `waitForActor` sigue siendo una llamada simulada.
- La decodificación de trajes: el fotograma es el último que se pidió, sin limbs.
- Cámara, paleta del actor, nombres de actor, de verbo y de objeto, inventario, nombres nuevos de objeto.
- Los efectos del resto de opcodes sobre las ranuras (`doSentence`, cutscenes, congelar).

**Comprobación.** El arnés gana el puerto `$F2`, los directorios de salas (`ROOMDIR`) y la orden
`LOADROOM`; el resumen de cada fotograma incluye ahora el estado de los actores y del mensaje. Se han
añadido 5 scripts sintéticos (actores, mensajes, sonido, un script de sala que debe morir al cambiar de
sala, y el límite de 15 del retardo).

- **2176 de 2176 escenarios idénticos**: las 54 salas con `LOADROOM`, 846 verbos de objeto y 168 scripts
  globales (cada uno con dos semillas de variables), y 40 sintéticos.
- Hay **41 mutaciones** reproducibles (`SCUMM/V2/ENGINE/test/mutaciones.py`): **38 se detectan**. Las
  tres que no: no arrancar el script 5 (su efecto no se ve), no validar el actor que habla cuando
  `VAR_TALK_ACTOR` apunta a uno que no existe, y no comparar el fotograma antes de animar al hablar. Los
  tres cambios no alteran nada observable con los escenarios actuales.

**Tamaño y coste:** núcleo + capa de juego = **5223 bytes** (3424 el núcleo solo). El fotograma vacío
cuesta 7226 T-states (10,9 % del fotograma PAL) y una instrucción de script 1115 T-states de media;
sigue habiendo margen de optimización en el recorrido de ranuras.

## 11. Movimiento de los actores (2026-10-21)

`SCUMM/V2/ENGINE/geom.asm` y `walk.asm` (incluidos desde `game.asm`), con su modelo en `vmgame.py`. Pasan de
llamadas simuladas a Z80: `walkActorTo`, `walkActorToObject`, `walkActorToActor`, `putActorAtObject`,
`faceActor`, `waitForActor` (con el estado real: espera mientras el actor se mueve) y los consultores
`getActorX/Y/Moving/Facing/WalkBox/Costume/Elevation` y `setActorElevation`. `getActorX/Y` aceptan también un
número de objeto (como `getObjX` de ScummVM; esto último está escrito de memoria, sin cotejarlo con la fuente).

- **Registro de actor de 48 bytes** (antes 16): añade caja, elevación, destino, caja de destino, dirección al
  llegar, caja en curso, siguiente punto, fracciones y sumandos del paso, umbral y dirección de la caminata.
- **Fotogramas fijos de v2** corregidos (también en el Z80): caminar 0, parado 1, inicio 2, dejar de hablar 4,
  hablar 5.
- **Datos de sala** (`ROOMDIR` pasa a 16 bytes por sala): un bloque con cajas (8 bytes), matriz de rutas
  `n*(n+1)` y objetos (id, x, y, dirección), que `start_scene` copia a `$FA00`.
- **Geometría** (`geom.asm`): producto y cociente de 32 bits con signo y truncado a cero (como C),
  `checkXYInBoxBounds`, `adjustXYToBeInBox`, punto más cercano de una recta y de una caja, `getNextBox`.
- **Caminata** (`walk.asm`): giro por la tabla de giros, `actorWalkStep`, `calcMovementFactor`,
  `startWalkActor`, `walkActor` por tramos según la matriz, y `walkActors` después de los scripts en cada
  fotograma (solo si hay sala).
- **Cajas especiales:** cerradas (`$40`), solo para el jugador (`$20`) e invisibles (`$80`); el jugador es
  `VAR(42) <= actor <= VAR(43)`.

**Comprobación:** 2382 de 2382 escenarios idénticos al modelo (todas las salas con una caminata al azar:
tres destinos dentro y fuera de las cajas, un objeto, otro actor; más 4 salas inventadas con trapecios de
aristas inclinadas, cajas cerradas, invisibles y solo-jugador, y matrices con huecos; más un script
sintético con todos los consultores y `faceActor` a 0..3 de distancia).
**109 mutaciones, 93 detectadas.** Las 16 sin detectar son equivalentes con estos datos o inalcanzables:
la rama «matriz que empieza en `$FF`» de `getNextBox`, destino o origen sin caja (255), la caja `n` leída
como `n-1`, las ramas vertical y horizontal de `closestPtOnLine` (la recta es vertical u horizontal, así que
cambiar un extremo por otro no cambia nada), el tope de x o y al llegar, la máscara `& 7` de las banderas
(solo hay 0 y 5 en el juego), y las de la capa anterior (script 5, hablante inválido, animación 59/60).

**Coste:** núcleo + capa de juego = 9508 bytes; el fotograma vacío sube de unos 7200 a unos 10000 T-states,
por el recorrido de los 24 actores (se puede evitar con un contador de actores en movimiento).
