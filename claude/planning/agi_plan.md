# AGI (Sierra) en el Z80 del SD81 Booster: plan inicial

Estado: solo plan (2026-10-26). Carpeta de trabajo prevista: `AGI/V2/` (hermana de `SCUMM/`), un nivel por version del intérprete (`V2`, `V3`).
Los datos de los juegos van en `AGI/V2/<Juego>/` y se ignoran por git, igual que en SCUMM.

## Qué es AGI v2 (de memoria; hay que contrastarlo con los ficheros reales)

- Ficheros: `LOGDIR`, `PICDIR`, `VIEWDIR`, `SNDDIR` (3 bytes por recurso: 4 bits de volumen + 20 de desplazamiento, `FFFFFF` = vacío), `VOL.0`…`VOL.n`,
  `WORDS.TOK` (vocabulario del parser) y `OBJECT` (objetos del inventario).
- Recurso en un `VOL`: cabecera `12 34`, byte de volumen, longitud u16 (little endian).
- **Lógica**: bytecode con un conjunto pequeño de opcodes (condiciones, variables 0-255, banderas, cadenas) y una sección de mensajes cifrada con XOR con la clave "Avis Durgan".
- **Imágenes**: vectoriales, de 160x168. Órdenes `F0`-`FA` (colores visual y de prioridad, líneas, rellenos, pincel) y `FF` al final. Cada imagen tiene dos planos: visual (16 colores EGA) y prioridad (16 niveles, con líneas de control 0-3).
- **Vistas**: bucles de celdas con RLE (color + longitud), color transparente y espejo.
- **Sonido**: tres tonos y ruido (PCjr/Tandy).
- Pantalla de texto de 25 filas: fila 0 de estado, 21 filas de imagen y las de entrada y mensajes.

## Decisión de vídeo que hay que tomar antes que nada (corrige lo dicho en la charla)

Dije que las imágenes se dibujaban "directamente" en el HiRes 8x1. **No es tan directo:** el 8x1 permite solo dos colores por cada 8 píxeles, y una imagen AGI usa 16 colores por píxel.
Con 160 píxeles de ancho, 4 AGI-píxeles caben en una celda de 8 si cada uno ocupa 2 píxeles HiRes (320, no cabe en 256), y a 1:1 hay 8 por celda. En ambos casos hay choque de color en las zonas con muchos colores o con dithering.

Opciones a medir con un prototipo (como `medir8x1.py` en SCUMM) sobre imágenes reales de KQ1, SQ1 y LSL1:
1. 1 px HiRes por píxel AGI en horizontal (160 de 256, con marco de 48 px a cada lado) y 1 línea por fila (168 de 192): la geometría encaja con la fila de estado y las 21 de imagen. Proporción 1:1 en vez de la 2:1 original.
2. Estirado a 256 (factor 1,6) con reparto 2,2,1,2,2: más ancho, pero con irregularidades.
3. Tratamiento del choque de color: elegir los 2 colores dominantes de cada celda de 8x1, o evitarlo con las imágenes, que suelen usar pocos colores por zona.

Resultado esperado de este paso: una cifra de error por imagen y un veredicto sobre la opción 1 o 2.

## Fases

1. **Lector de datos + visor en PC** (Python): directorios, volúmenes, descifrado de mensajes, vuelco de lógica, vistas e imágenes a PNG. Sirve de referencia para el Z80 y para medir el choque de color.
2. **Modelo de referencia del intérprete en Python** (como `vmmodel.py` en SCUMM): opcodes, variables, banderas, objetos animados. Es la base de la suite de equivalencia con el Z80.
3. **Conversor a formato del SD81**: recurso por fichero o paquete con índice, imágenes ya pasadas a su plano visual/prioridad o dejadas vectoriales (decidir según tiempo de dibujo).
4. **Intérprete Z80** (<`$8000` el código, datos en la RAM extendida paginada): despachador de opcodes, dibujo de imágenes, sprites con prioridad, parser.
5. **Sonido** hacia el AY emulado de la placa.

## Memoria (idea)

- Plano visual y atributos HiRes como en SCUMM (`$A000` y `$C000`); plano de prioridad aparte (160x168 a 4 bits son unos 13 KB; se puede guardar en páginas de la RAM extendida).
- Lógica, imágenes y vistas se cargan de la SD bajo demanda.

## Primeros pasos (a hacer cuando el usuario tenga los juegos)

- Un juego sencillo y bien documentado como primera diana: King's Quest I, Space Quest I o Leisure Suit Larry I (ediciones AGI v2).
- Comprobar las cabeceras (`12 34`) para confirmar que son v2 y no v3.

## Resultado de la medida del choque de color (2026-10-26, `AGI/V2/TOOLS/medir8x1.py`)

Pixeles cuyo color cambia al dejar 2 colores por celda de 8 (los 2 mas frecuentes; el resto, al mas cercano), sobre las imagenes de KQ1, KQ2, Larry1, PQ1, SQ1 y SQ2 (451 imagenes):

| Geometria | media | mediana | peor |
|---|---|---|---|
| A: 160 px de ancho (1 px HiRes por px AGI) | 2,7-3,3 % | 2,6-3,3 % | 6,3-9,3 % |
| C: estirado a 256 (x1,6), 32 celdas por linea | 1,5-2,0 % | 1,4-1,9 % | 3,6-5,5 % |

Mirando las peores (SQ2 imagen 15, Larry1 imagen 40) el aspecto es practicamente el original. **Se elige la geometria C** (256 de ancho): menos choque, ocupa todo el ancho
y conserva mejor la proporcion (256/168 = 1,52 frente a ~1,59 del original; con A saldria 0,95). Queda por ver el efecto de los sprites, que se dibujan encima y pueden meter un tercer color en la celda.
