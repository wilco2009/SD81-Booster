# Plan: intérprete SCUMM v2 en el Z80 del SD81 Booster

Estado: planificación, nada implementado salvo el conversor de gráficos de prueba
(`SCUMM/TOOLS/lfl2gfx.py`). Fecha: 2026-10-21.

## 1. Decisiones tomadas

| Tema | Decisión |
|---|---|
| Motor | SCUMM **v2** oficial, con los datos originales que aporta el usuario. La v1 (C64/NES) y la v3 quedan fuera por ahora. |
| Dónde corre | **En el Z80.** El STM32 solo sirve ficheros (como con el diccionario del Scrabble). Nada de intérprete en el MCU. |
| Memoria | Código en overlay con trampolines, preparado **desde el principio**. Dos ventanas: **bloque 0** (siempre de solo lectura, cambiando su página) y **bloque 1**. Variables y datos en los bloques 4-7. Evitar MC45/MC67. |
| Gráficos | **Chroma modo 0** (color por código de carácter y por scanline), Superfast texto de 32 columnas, **recorte de la vista a 32 columnas** (SCUMM usa 40), presupuesto de **tiles por ventana de cámara**. |
| Conversión | **Offline** en el PC. El Z80 nunca convierte imágenes. |
| Cursor | Lo gestiona el Z80. |
| Datos de LucasArts | No se redistribuyen ni se versionan. El usuario los pone en la SD tras pasarlos por el conversor. |

Descartado y por qué (para no repetirlo):

- **Intérprete en el STM32**: rompe la esencia del interface.
- **Bitmap 256x192x16 colores por píxel**: no existe y no se hará; ocuparía ~48 KB de RAM.
- **Modo 1 de Chroma (atributo por celda, 2 colores por 8x8)**: más error que el modo 0
  (mediana ~9 % de píxeles distintos frente a ~4-5 %) y el texto pintado de las salas se degrada.
- **Escalar 320 a 256**: no reduce tiles más que recortar y deforma los dibujos.

## 2. Datos de partida (Maniac Mansion DOS v2)

Muestras en `SCUMM/V2/ManiacMansionDOS` (54 ficheros, 676 KB) y `SCUMM/V1/ManiacMansionNES` (solo
referencia, no se usa).

- Cada `NN.LFL` es un fichero cifrado con **XOR 0xFF**. Contiene la sala (fondo, máscaras, objetos,
  scripts de sala) y recursos de ese volumen (trajes, sonidos, scripts).
- Cabecera de sala (ya decodificada en el conversor): tamaño u16 en +0; **ancho u16 en +4**, **alto u16
  en +6** (siempre 128); **offset de la imagen u16 en +0x0A**.
- Imagen de fondo: columnas de 128 píxeles en RLE. Byte con bit 7 = «repetir la columna anterior»
  `n` píxeles; si no, run = nibble alto (0 = leer otro byte) y color = nibble bajo. La máscara de
  profundidad (Z-plane) sigue a la imagen.
- Salas de 320 a 960 px de ancho (53 con imagen).
- Referencia: `engines/scumm` de ScummVM (`resource*.cpp`, `room.cpp`, `object.cpp`, `script*.cpp`,
  `boxes.cpp`, `gfx.cpp`). Las páginas del wiki de ScummVM están bloqueadas para mi lector.

### 2.1 Formato, ya parseado (fase 1, `SCUMM/TOOLS/scumm2.py`)

**Índice `00.LFL`** («Enhanced V2», magia `0100`):
`u16 magia`, `u16 N` objetos globales, `N` bytes (dueño en los 4 bits bajos, estado en los altos),
y cuatro listas (salas, trajes, scripts, sonidos), cada una: `u8 n`, `n` bytes de fichero (en salas, de
disco y sin uso), `n x u16` offset dentro del fichero. `FFFF` = no existe. Maniac Mansion DOS: 780
objetos, 61 salas (54 con fichero), 40 trajes, 179 scripts, 120 sonidos. Hay entradas sobrantes con
basura (las salas 54-60 tienen offsets que son texto ASCII); ScummVM las lee igual y no las usa.

**Recurso** (traje, script, sonido): empieza en su offset de `NN.LFL` con `u16 tamaño` (incluye la
cabecera) y 2 bytes más; el código empieza en +4. Una sala puede cargar recursos que están en otro
fichero (la sala 17 carga el sonido 50, que está en `53.LFL`).

**Sala** (primer recurso de `NN.LFL`, `u16 tamaño` en +0): ancho u16 +4, alto u16 +6 (siempre 128),
imagen u16 +0x0A, nº de objetos u8 +0x14, cajas u8 +0x15 (offset), nº de sonidos +0x16, nº de scripts
+0x17, código de salida (EXCD) u16 +0x18, de entrada (ENCD) u16 +0x1A. Desde +0x1C: `n x u16` offsets de
imagen de objeto (OBIM), `n x u16` offsets de bloque de objeto (OBCD), ids de sonidos, ids de scripts.
Sin imagen, un objeto apunta con su OBIM al final de las imágenes. Los bloques OBCD, EXCD y ENCD
quedan contiguos hasta el final del recurso de sala (comprobado en las 54).

**Objeto (OBCD)**: `u16 tamaño`, 2 bytes, `u16 id` en +4, +7 x (en bloques de 8 px), +8 y (bit 7 =
estado del padre), +9 ancho, +10 padre, +11 x de acercamiento, +12 y de acercamiento (bits 0-4) y
preposición (bits 5-7), +13 dirección (bits 0-2) y alto (bits 3-7), +14 offset del nombre, +15 tabla de
verbos: pares (verbo, offset dentro del bloque) acabada en 0. El nombre es una cadena acabada en 0.

**Cajas de caminar**: `u8 n`, luego `n x 8 bytes` (uy, ly, ulx, urx, llx, lrx, máscara, flags) y la
matriz de rutas de `n x (n+1)` bytes.

### 2.2 Comprobación contra `descumm`

- `TOOLS/descumm/build.sh` baja los fuentes de `descumm` (scummvm-tools, GPL) y los compila con
  cabeceras mínimas propias; así no se versiona código ajeno.
- `scumm2.py DIR extract SALIDA` escribe 843 recursos y `check` comprueba el índice y los bloques.
- `scumm2.py DIR dump SALIDA RUTA_DESCUMM` escribe un texto por sala y por script con el listado de
  `descumm -2`, y compara la tabla de verbos de cada objeto con la que imprime `descumm`:
  **460 objetos, 0 diferencias**.
- `descumm` desensambla sin error 706 bloques de código (scripts globales, de entrada, de salida y de
  objeto). La única excepción es el script 59, con código muerto detrás de un `stopScript`.
- Esto comprueba los límites de cada script. El desensamblador propio de los opcodes está por hacer
  (fase 2, junto al intérprete).

## 3. Gráficos

### 3.1 Qué se mide y qué sale

`lfl2gfx.py` reduce cada fila de 8 píxeles a 2 colores (modo 0), funde tiles hasta que ninguna ventana
de 32 columnas pase de 190 tiles distintos, y compara con el original. Resultados de las 53 salas con
190 tiles: error del 0 al 17,7 % (mediana ~4-5 %). Las peores son la 04 (17,7 %) y la 05 (15,2 %).
Detalle en `SCUMM/preview/resumen.md`.

### 3.2 Reglas del hardware que condicionan el diseño

Del manual (Apéndice F y capítulo de 128/256 caracteres) y del `memory_map.md` del CP/M 3:

- Modo **Superfast texto** (`LOAD *SFAST`), que es el único con **256 caracteres**
  (`LOAD *256C`). Tabla de caracteres de 2 KB en `$3800-$3FFF`. El registro **I** permite colocarla
  donde se quiera, pero por encima de 16 KB hay que desactivar antes el modo WRX. De momento se deja
  en `$3800`.
- Con 256C, la tabla de color del modo 0 mide 2 KB (8 bytes por código) en `$C000-$C7FF`,
  papel en el nibble alto y tinta en el bajo.
- Para cargar un tile hay que escribir 8 bytes en `$3800+8*código` y 8 bytes en `$C000+8*código`.
- **Scroll fino**: `POKE 2090` desplaza 0-7 píxeles. El hueco de la derecha lo rellena el byte de
  NEWLINE del DFILE (posición 32 de cada fila), y lo debe escribir el software. Con 2091-2093 se
  elige qué filas se desplazan, así que la parte de verbos y frase se queda fija mientras scrollea la
  sala. En modo 0 el color de esa columna extra se calcula bien (en el modo 1 no).
- **Pantalla alternativa** (`POKE 2096/2097`, o `DFILE_OVR`): el DFILE puede vivir donde convenga.
- La FPGA lee fuente, pantalla y colores de una **sombra** indexada por dirección lógica, que captura
  todas las escrituras del Z80 sin mirar el paginado. Consecuencias: (a) cambiar la página de un
  bloque no altera lo que se ve; (b) no se puede usar `$3800-$3FFF` ni `$C000-$C7FF` ni el DFILE para
  nada que no sea vídeo.

### 3.3 Pantalla

32 columnas x 24 filas = 24 filas de 8 píxeles = 192 px de alto.

- Filas 0-15 (128 px): sala, con scroll fino por hardware.
- Filas 16-23: línea de frase y verbos (la interfaz de v2 está pensada para 40 columnas, habrá que
  recolocarla). Sin scroll (registros 2091-2093).

### 3.4 Tiles

- **Presupuesto**: unos 190 tiles para el fondo por ventana de cámara. Los ~64 restantes se reservan para fuente
  de la interfaz, cursor si no va en sprite, y objetos que el motor dibuja sobre el fondo. Como el
  color está ligado al código, cada combinación de color de fuente cuenta como un tile aparte.
- **Cambio de sala**: se carga un juego de tiles nuevo, con pantalla en negro si hace falta. No hay
  restricción entre salas.
- **Dentro de una sala con scroll**: el Z80 renueva tiles en los bordes. Coste: 16 bytes por tile.
- Una sala tiene hasta 585 tiles distintos y ninguna ventana pasa de 190. Un tile del mapa no cabe en
  un byte, así que el mapa de la sala no puede guardar el «id global».

### 3.5 Formato `ROOMnn.GFX` (implementado en `lfl2gfx.py`, 2026-10-21)

Un fichero por sala, sin guion bajo en el nombre (el ZX81 no lo tiene; ver
`feedback_zx81_no_underscore`). Todo en little endian.

**Cómo se resuelve el problema de los slots.** Cada tile se agrupa en *instancias*: las columnas donde
aparece, unidas mientras la distancia entre dos apariciones sea menor que la ventana (32). Una instancia
está visible en un intervalo continuo de posiciones de cámara, así que asignar slots es un coloreado de
intervalos, y el algoritmo voraz usa exactamente tantos slots como tiles distintos haya en la peor
ventana. Como la fusión de tiles ya garantiza ≤ 190 por ventana, el coloreado cabe siempre en 190 slots
(comprobado en las 53 salas). Todas las celdas de una instancia llevan el mismo código de carácter.

```
+0   "SGFX"
+4   u8  versión (1)
+5   u8  nslots   (slots usados, ≤ presupuesto)
+6   u16 cols     (ancho de la sala en columnas de 8 px)
+8   u8  rows     (16)
+9   u8  view     (32, columnas visibles)
+10  u16 npos     (posiciones de cámara = cols - view + 1)
+12  u16 n        (instancias)
+14  u16 code_base (primer código de carácter de los slots)
+16  mapa: cols*rows bytes, por filas (celda (c,r) en r*cols+c) = código de carácter
     rstart[npos+1] u16   índice de la primera instancia que entra al mover la cámara a la derecha a cada posición
     instancias: n x 17 bytes, ordenadas por posición de entrada:
         u8 código de carácter, 8 bytes de forma, 8 bytes de color
     lstart[npos+1] u16   igual, para entrar por la izquierda
     llist[n] u16         índices de instancias ordenadas por posición de salida
```

Color de cada fila: papel en el nibble alto, tinta en el bajo (como la tabla de `$C000`); el bit 1 de
la forma es tinta. Uso en el Z80:

- Cámara en 0: cargar las instancias `rstart[0] .. rstart[1]-1`.
- Mover a la derecha a la posición `s`: cargar `rstart[s] .. rstart[s+1]-1`.
- Mover a la izquierda a la posición `s`: cargar `llist[lstart[s] .. lstart[s+1]-1]`.
- Salto de cámara: recorrer los pasos intermedios.
- «Cargar» = copiar 8 bytes a `$3800+8*código` y otros 8 a `$C000+8*código`, y copiar al DFILE las 32
  columnas visibles del mapa (16 LDIR de 32 bytes).

`verify_gfx()` simula ese procedimiento (derecha, izquierda y 60 saltos) y comprueba que cada celda
visible tiene los bytes de su tile en cada posición de cámara. Las 53 salas dan OK.
Tamaño: de 4,8 a 13,5 KB por sala; 356 KB todas.

**Presupuesto y calidad (2026-10-21):** a la vista de la sala 4 (418 tiles distintos en la peor ventana, casi todos únicos) el presupuesto por ventana sube de 190 a **216**, y los 40 códigos 216-255 se reservan para la fuente de la interfaz (espacio, cifras, letras, `/ : .`). El error de fusión se mide ahora por distancia de color (no por píxeles distintos), pesa más los colores raros de la sala y el coste de fundir un tile crece con la raíz de sus apariciones. Cada sala admite ajustes en `SCUMM/TOOLS/gfx_rooms.cfg` (`protect=x0,y0,x1,y1` protege zonas como rótulos o personajes, `tiles=`, `rarity=`); las zonas protegidas salen en magenta en la banda del original de `preview/salaNN.png`. Alternativas descartadas: bitmap con atributos para las salas malas (el scroll costaría demasiado en el Z80).

**Códigos ≥ 128 (2026-10-21):** el bit 7 del código de carácter invierte el vídeo también en 256C. El conversor
guarda al revés (`^ $FF`) la forma de las instancias cuyo código es ≥ 128, y `verify_gfx` lo deshace al
comprobar; el Z80 copia los 8 bytes tal cual. El visor reserva los códigos 192-255 para el juego de
caracteres de la ROM (texto de la línea de estado; el 192 es el espacio y hace de tile en blanco), y los
slots llegan como mucho a 189.

Falta: máscara de profundidad, objetos, y los scripts y recursos que no son gráficos.

### 3.6 Colores y pendiente de gráficos

- **Correspondencia EGA → Chroma**: la paleta de Chroma en el conversor es un supuesto (brillo como en
  EGA: 170/0 normal, 255/85 brillante). El marrón del EGA (6) no existe en Chroma y por defecto va al
  amarillo (6). Con el rojo (2) se confunde con el rojo del EGA y se pierde, por ejemplo, el foco del
  suelo de la sala 04. Se ajusta con `--map 6=2`. **Se decide probando en la placa** distintas
  combinaciones.
- Objetos que el motor pinta sobre el fondo, con sus estados: se verá al convertir el primer juego.
- Máscara de profundidad: el hardware solo dibuja los sprites **sobre** el fondo y la prioridad es
  solo por número de sprite (el 63 tapa al 0, manual Apéndice H). No hay prioridad respecto al fondo.
  Los objetos que deban tapar a un personaje habrá que resolverlos con sprites de número mayor o
  pintando el personaje en los tiles; se decide al convertir el primer juego.
- Trajes de personajes (v2: animación por piezas): probablemente sprites hardware.

## 4. Mapa de memoria del Z80

Valores iniciales para discutir. Los bloques son de 8 KB y el modo es Superfast texto con 256C.

| Bloque | Rango | Uso propuesto |
|---|---|---|
| 0 | `$0000-$1FFF` | **Ventana de overlay de código, de solo lectura**: 8 KB. Se mantiene `block0Writable` apagado (sin `POKE 2056`), así que los sprites y los puertos mapeados en memoria (POKE 2038-2098) siguen activos. El código se carga en una página mientras está mapeada en un bloque 4-7 (R/W) y luego se mapea en el bloque 0 para ejecutarlo. Las variables no pueden estar aquí. |
| 1 | `$2000-$37FF` | **Ventana de overlay de código**: 6 KB útiles, porque `$3800-$3FFF` es la tabla de caracteres. Ninguna página de overlay debe tener código ni datos en esa mitad. |
| 2-3 | `$4000-$7FFF` | **Núcleo residente**, 16 KB: despachador de opcodes, planificador de scripts, variables, tablas de objetos y actores, trampolines, tick de 50 Hz. Variables del sistema en `$4000-$407D`. |
| 4-5 | `$8000-$BFFF` | Datos paginados: sala actual (imagen, máscaras, mapa de slots), recursos cargados. Solo datos, no código. |
| 6 | `$C000-$DFFF` | `$C000-$C7FF` es la tabla de color del modo 0. Resto libre para datos. |
| 7 | `$E000-$FFFF` | DFILE alternativo, mapa de pantalla, datos. |

- El motor completo en el modo Superfast: los bloques 6 y 7 son libres porque el vídeo es hardware.
  Hay que comprobarlo con la combinación exacta de modos (256C + Chroma 0 + DFILE alternativo).
- Los 512 KB son para **datos**: se leen del SD por el protocolo de ficheros del MCU y se paginan.
- **Trampolines**: viven en el núcleo residente. Una llamada entre overlays guarda el bloque 1 actual,
  mapea el overlay destino, llama y restaura. Es el mismo patrón que el CP/M 3, y como allí se
  programa con `OUT (C),A` con C=E7h y no con el registro dependiente de `block0Writable`.
- **Bloque 0 como overlay** (propuesto por el usuario, 2026-10-21): mientras tenga una página de
  código mapeada deja de ser la ROM del ZX81.
  - **Interrupciones:** con el sistema arrancado, RST y NMI no sirven y deben estar desactivadas
    (DI obligatorio), así que el overlay no necesita vectores en `$0000/$0038/$0066`. La FPGA da
    interrupciones **simuladas** a 50 Hz en los modos Superfast con DI: sirve ella misma los bytes en
    `$0038-$003E` (un `CALL` a la rutina que se haya indicado) y cambia los `HALT` por una espera al
    VSYNC. Se activan con `POKE 2038/2039` (dirección de la rutina) y `POKE 2040` (bit 0 = activar);
    esos POKE se capturan del bus aunque el bloque 0 sea de solo lectura (solo dejan de capturarse si
    el bloque 0 es RAM). Referencia: `claude/planning/sim_int_emulator.md`.
  - **Consecuencia:** la rutina de interrupción tiene que vivir en el núcleo residente (bloques 2-3).
    Puede saltar en cualquier límite de instrucción, incluso entre «mapear overlay» y «llamar», así
    que no debe tocar el mapeo de bloques sin restaurarlo. La alternativa sin interrupciones es
    sondear el contador de VSYNC del puerto `AFh` (bits 6-1), que además sirve de reloj de
    fotogramas para los timers de SCUMM.
  - **Handshake con el MCU:** las rutinas (`SendString` `$201C`, `GetData` `$2031`, etc.) viven en la
    ROM de expansión del bloque 1 (`$2000-$203D`). Mientras el bloque 1 lleve un overlay no están.
    No hace falta copiarlas todas, solo las necesarias (por ejemplo la carga de archivos de la SD),
    en el núcleo residente. Se ve en detalle cuando toque.
  - **Cambio de página:** se hace por el puerto `E7h` (manual 15.3), sin depender de
    `block0Writable`. Con 64 páginas se usa `OUT (C),r` con C=E7h, B = página (0-63) y r = bloque.
    `FULLPAGING` se activa una sola vez al arrancar el programa, así que el resto del código no se
    ocupa de ello.
- Con los dos overlays hay 14 KB de código intercambiable (8 + 6) y 16 KB residentes.
- **Reparto en overlays** (tentativo; el tamaño es una estimación a ojo, sin medir):

| Overlay | Contenido |
|---|---|
| A | Grupo de opcodes de script (variables, flujo, objetos) |
| B | Carga de sala, gestión de tiles, scroll |
| C | Actores, trajes, caminos |
| D | Verbos, frase, texto y cursor |
| E | Sonido (AY) |
| F | Cargador de recursos desde SD |

Regla de diseño: lo que se llama en cada ciclo de script o de fotograma (bucle de scripts, dibujo de
actores, scroll) debe vivir en el mismo overlay o en el residente, para no cruzar ventanas con cada
instrucción. Esto se fija **antes** de escribir código.

## 5. El intérprete

Por contrastar con `script_v2.cpp` y `descumm` antes de diseñarlo:

- Conjunto de opcodes de v2 (unos cien, muchos con parámetros que pueden ser inmediato o variable).
- Máquina de scripts cooperativos: tabla de slots con estado y puntero, scripts de sala, locales y
  globales, tablas de objetos (dueño, estado, clase) y de actores.
- Sentencias del jugador: verbo + objeto(s) y la cola de tareas.
- Planificación por fotograma con la interrupción simulada de la FPGA (`sim_int`) o el VSYNC.

La presión de CPU debería ser baja: el original corría en un 6502 a 1 MHz y el Z80 va a 3,25 MHz.
Lo que más tiempo se llevará será el pintado y el scroll, no los scripts.

## 6. Sonido

El AY ya está emulado. Los sonidos de la v2 de DOS son de PC speaker/AdLib y habrá que convertirlos
offline a notas del AY. Se dejan para el final.

## 7. Fases

| Fase | Objetivo | Qué demuestra |
|---|---|---|
| 0 | Terminar el conversor: coloreado de slots, correspondencia de colores, formato de salida | Que el formato es viable en las 53 salas. **Hecho para el fondo** (2026-10-21, ver 3.5); faltan máscaras y objetos |
| 1 | Parser de recursos (índice, scripts, objetos) en el PC | Que el modelo de datos está entendido, con volcados comparables con `descumm`. **Hecho** (2026-10-21, ver 2.1 y 2.2); faltan trajes, sonidos e imágenes de objeto |
| 2 | Intérprete de scripts en el Z80, ejecutado **sin vídeo** en el arnés de PC (como `claude/dbgharness`) | Que ejecuta los scripts de Maniac Mansion sin colgarse y que el reparto en overlays funciona. **Hecho:** desensamblador propio de los opcodes v2, 0 diferencias con descumm en 8578 instrucciones, y primer diseño en [scumm_interprete.md](scumm_interprete.md). **Hecho también:** el despachador en Z80 (3424 B) con arnés de PC: 2162 escenarios idénticos al modelo en Python (sección 9 de scumm_interprete.md). **Hecho también:** la capa de juego (sonido, mensajes, actores, `loadRoom`) con back-end por sucesos, 2176 escenarios idénticos (sección 10). **Falta:** caminos y cajas, trajes, verbos y sentencias, inventario, cutscenes |
| 3 | Sala en pantalla: carga de tiles, scroll fino y recarga en los bordes | Que el modo 0 con scroll funciona en la placa. **Preparado (2026-10-21):** `SCUMM/VIEWER/roomview.asm` (visor con O/P/B/N, ver su README) y su arnés de PC (`SCUMM/VIEWER/test/run_test.sh`, 9 comprobaciones OK contra la conversión). **Funciona en el emulador (2026-10-21); falta la placa.** Para que la columna que asoma con el scroll fino tenga su tile, el conversor usa ahora ventanas de 33 columnas (`--view 33`, por defecto) y la cámara llega a `npos` (la última posición no tiene columna 33) |
| 4 | Objetos, máscaras, actores y trajes | Personajes que andan por la sala |
| 5 | Verbos, frase y cursor con teclado/joystick | Que se puede jugar |
| 6 | Sonido | Música y efectos |
| 7 | Maniac Mansion completo; después Zak v2 | |

Se prueba primero en el arnés de PC y en el emulador EightyOne, y después en la placa. No hay
autocompilación por mi parte: compila y flashea el usuario (`feedback_no_autocompile`).

## 8. Riesgos y preguntas abiertas

1. ~~Coloreado de slots~~: resuelto (3.5), 190 slots bastan en las 53 salas.
2. **Máscaras de profundidad y objetos** sobre el fondo: los sprites van siempre encima del fondo y
   la prioridad es solo por número de sprite. Se decide al convertir el primer juego.
3. **Tiempo de carga** de una sala desde la SD a través del MCU: no está medido para este uso.
4. **Texto pintado** en las salas (rótulos): el presupuesto de tiles lo degrada en las peores salas.
5. Interfaz de verbos pensada para 40 columnas: hay que recolocarla en 32.
6. Que el reparto de overlays en 8 + 6 KB sea suficiente: estimación sin medir.
6b. Cómo tratar el temporizado de los scripts: interrupciones simuladas o sondeo de `AFh` (ver
   sección 4). Decidir cuando se diseñe el bucle principal.
7. Que Chroma 0 + 256C + DFILE alternativo coexistan en los registros actuales de la FPGA: se
   comprueba con un programa de prueba antes de la fase 3.
8. Tamaño real del conjunto de datos convertidos frente a lo que cabe en una SD de ejemplo (el
   original ocupa 676 KB).

## 9. Repositorio y licencia

- `SCUMM/V1`, `SCUMM/V2` y `SCUMM/preview` contienen datos de LucasArts y derivados: **añadirlos a
  `.gitignore`** antes de cualquier commit. `SCUMM/TOOLS` sí es código propio.
- El conversor y el motor no incluyen datos del juego. Cada usuario aporta los suyos.
