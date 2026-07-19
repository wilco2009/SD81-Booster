# EXPLORER -- Explorador de archivos SD81 Booster (42 columnas)

Navega las carpetas de la SD en modo Superfast HiRes Spectrum (42 columnas
mediante fuente comprimida de 6 píxeles).

| Tecla | Acción |
|-------|--------|
| 5 | Ir a la carpeta padre (sube un nivel) |
| 6 | Mover selección abajo |
| 7 | Mover selección arriba |
| 1 / 2 | Página arriba / página abajo (salta una pantalla completa, `MAXVIS` filas) |
| 8 / ENTER | Entrar en carpeta, o devolver el control al BASIC con el nombre del archivo seleccionado (ver más abajo) |
| ESPACIO | Salir al BASIC (sin nombre de archivo) |
| N | Nueva carpeta (pide el nombre por teclado) |
| D | Borrar el fichero o carpeta seleccionado (pide confirmación; las carpetas solo se borran si están vacías) |
| R | Renombrar el fichero o carpeta seleccionado (pide el nombre nuevo) |
| C | Marcar el seleccionado para copiar |
| X | Marcar el seleccionado para mover |
| V | Pegar (copia o mueve, según lo marcado con C/X, a la carpeta actual) |

Los textos en pantalla (diálogos) están en inglés. El borde de pantalla se
pone en azul al entrar (`video_on`, puerto $FB) — solo como primera prueba
de color; fácil de cambiar si se prueban otras combinaciones.

## Decoración de pantalla (banner + barra de iconos)

Las filas 0 y 23 de la pantalla son artwork fijo (no texto), pegado con
`blit_row` en cada `refresh_screen`:

- **Fila 0**: banner "SD81 BOOSTER", puramente decorativo.
- **Fila 22**: aquí se imprime la ruta del directorio actual (se movió
  aquí porque las filas 0/23 pasaron a ser artwork).
- **Fila 23**: barra de iconos de teclas (sustituye a las dos líneas de
  ayuda en texto que había antes).
- Filas 1-21 (`MAXVIS`): listado, sin cambios.

El origen de esas dos filas es una captura de pantalla Spectrum completa
(`explorer.scr`, 6912 bytes: 6144 bitmap + 768 atributos). El script
[`extract_bg.py`](extract_bg.py) extrae UNA fila (0-23) de un `.scr` a un
recurso binario de 288 bytes (256 de bitmap en formato "lineal" — 8
scanlines de 32 bytes seguidas, no en el formato de tercios de la pantalla
real — más 32 de atributo) que se incluye con `INCBIN` en `explorer.asm`
(`BG_ROW0`/`BG_ROW23`). `blit_row` convierte ese formato lineal a las
direcciones reales de pantalla al pegarlo, con el mismo cuidado que
`clear_row`/`copy_row` (recalcula el inicio de cada scanline antes de
avanzar, en vez de fiarse de dónde lo deja el `ldir`).

Para rediseñar el banner o la barra de iconos: edita/regenera
`explorer.scr` con el diseño nuevo y vuelve a correr, por ejemplo:

```
python3 extract_bg.py explorer.scr 23 bg_row23.bin
```

sobre la fila que cambie, y recompila. No hace falta tocar `explorer.asm`
salvo que se quiera decorar una fila **distinta** de la 0/23 (en ese caso,
generar el `.bin` correspondiente, añadir su `INCBIN` y una llamada a
`blit_row` con la fila deseada).

Para copiar o mover: selecciona el origen y pulsa **C** o **X**, navega con
5/6/7/8 hasta la carpeta destino, y pulsa **V** para completar la
operación.

Durante la entrada de texto (nueva carpeta / renombrar), editor con
cursor real (insertar/borrar en la posición del cursor, no solo al
final):

| Tecla | Acción |
|-------|--------|
| A-Z, 0-9, `.` | Carácter literal |
| SHIFT+Z/X/C/V | `:` `;` `?` `/` |
| SHIFT+J/K/L | `-` `+` `=` |
| SHIFT+N/M/.  | `<` `>` `,` |
| SHIFT+B | `*` |
| SHIFT+ESPACIO | `£` |
| SHIFT+5 / SHIFT+8 | Mover cursor izquierda / derecha |
| SHIFT+0 | Borrar (RUBOUT real del ZX81, carácter a la izquierda del cursor) |
| SHIFT+1 | Restaurar el nombre original (EDIT real del ZX81 — solo tiene efecto al renombrar) |
| ENTER | Confirmar |
| ESPACIO | Cancelar |

Los tokens de más de un carácter (palabras clave BASIC accesibles con
SHIFT+ENTER+tecla) no están soportados por simplicidad.

## Cómo se devuelve un archivo seleccionado al BASIC

Este programa vive en RAM como código máquina suelto (cargado con `LOAD ...
CODE`), por lo que **no puede** hacer él mismo nada que toque la zona
`$4009+` (programa/variables BASIC): esa zona es precisamente donde vive el
propio stub BASIC que lo invocó, y una operación en curso podría corromper
la pila o el código en ejecución antes de terminar. Por eso el explorador
no intenta cargar `.P` ni visualizar texto por sí mismo -- se limita a
devolver el nombre del archivo seleccionado (sea del tipo que sea) al
BASIC, que decide qué hacer con él.

Al pulsar ENTER/8 sobre cualquier archivo (no carpeta), el explorador:

1. Copia el nombre del fichero (ya convertido a ASCII) al buffer estable
   `retname`, en la dirección fija **24579** (`ORG`+3, justo tras el
   `JP start` inicial). Vive en RAM estándar, que `video_off` no remapea —
   a diferencia del buffer de trabajo `namebuf` (en el bloque de pantalla,
   `VIDBLOCK`), que desaparecería al restaurar el vídeo porque ese bloque
   vuelve a espejar su página normal.
2. Restaura el modo de vídeo estándar (desactiva Superfast, el bloque
   `VIDBLOCK` vuelve a espejar su página).
3. Hace `RET` devolviendo en `BC` la longitud del nombre (0 si el usuario
   pulsó ESPACIO para salir). En ZX81, `USR direccion` como función
   devuelve el contenido de `BC`.

El **stub BASIC** (el programa que hay que teclear/grabar una vez y que se
lanza de verdad) recoge ese nombre y decide qué hacer con él -- por ejemplo,
mirar la extensión y hacer `LOAD FAST` solo si es un `.P` (mecanismo ya
probado de `sdhandler.inc.asm`: carga a `VERSN` + autoarranque vía
`THEN GOTO`). El ejemplo de abajo asume que siempre es un `.P`; el filtrado
por tipo de archivo queda a criterio de quien adapte el stub:

```basic
5 LOAD THEN CLEAR 24575
10 FAST
20 LOAD FAST "EXPLORER.BIN" CODE 24576
30 LET N=USR 24576
40 IF N=0 THEN STOP
50 LET F$=""
60 FOR I=0 TO N-1
70 LET F$=F$+CHR$ (PEEK (24579+I))
80 NEXT I
90 LOAD FAST F$ THEN GOTO 0
100 GOTO 10
```

(`24576` = `ORG` del código; `24579` = dirección de `retname` (`ORG`+3).
`CLEAR 24575` protege el código reservando la RAM por encima. **Todo el
programa debe quedar por debajo de `$8000`** porque en el ZX81 de 16K
`$8000+` es un espejo de `$4000+`, donde vive el sistema BASIC.)

La línea 80 solo se alcanza si la carga falla (fichero no encontrado, etc.);
si tiene éxito, `THEN GOTO 0` transfiere el control al programa recién
cargado.

## Ensamblar y cargar

Con [pasmo](http://pasmo.speccy.org/):

```
pasmo explorer.asm EXPLORER.BIN
```

Copia `EXPLORER.BIN` a la SD (o a la carpeta de la SD virtual del emulador
EightyOne). Graba el stub BASIC de arriba como programa autoarranque (o
añádelo antes de las líneas 10-80 lo que ya tengas), y ejecútalo con `RUN`.

## Notas para la primera prueba en hardware/emulador

- La aritmética de desplazamiento de ventana (`do_down`/`do_up` en
  `explorer.asm`) y el port completo del algoritmo de compresión de
  columna de `print42.bas` son las partes con más superficie para un fallo
  fino de primera pasada; si el listado no se desplaza bien o algún
  carácter sale corrupto, son los primeros sitios a mirar.
- El bloque de pantalla se controla con las constantes `VIDBLOCK` (6 o 7),
  `VIDPAGE` y `VIDMIRRORPAGE` al principio de `explorer.asm`. Se remapea a
  la página física `VIDPAGE` al entrar (`video_on`) y se restaura a
  `VIDMIRRORPAGE` (la página que le corresponde como espejo en modo ZX81
  normal: bloque 6→página 2, bloque 7→página 3) al salir (`video_off`). Si
  `VIDPAGE` ya la usas para otra cosa en tu configuración, cambia esa
  constante.
