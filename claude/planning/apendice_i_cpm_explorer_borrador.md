> **Borrador para validar contenido antes de tocar los `.docx`.** Nada de
> esto se ha integrado todavía en `MANUAL/ES/SD81_Manual_ES.docx` ni en el
> `.md` derivado — es solo el texto propuesto, para que confirmes que
> encaja con lo que realmente hace el software antes de invertir el
> trabajo (más laborioso y menos reversible) de editar el `.docx`.
>
> Dudas que necesito que resuelvas antes de integrar:
>
> 1. **¿Un solo Apéndice I con las dos secciones (CP/M+ y explorador), o
>    dos apéndices separados (I y J)?** Lo he escrito como uno solo,
>    siguiendo tu mensaje ("un anexo... que se incluya el CP/M y el
>    explorer"), con dos secciones `## I.1`/`## I.2`.
> 2. **¿Hago EN ahora también, o primero cierro ES?** Dado el tamaño,
>    propongo cerrar ES primero.
> 3. He **condensado** la explicación de MC45 en la sección de CP/M+
>    (tu manual original la explica en detalle, con imagen del circuito
>    de generación de NOPs) porque el capítulo 11 del manual principal
>    ya la cubre — incluida la extensión a los bloques 6/7 que añadimos
>    hace poco (`POKE 2062`). ¿Te parece bien remitir ahí en vez de
>    repetirla, o prefieres mantener la explicación completa también
>    aquí?
> 4. **El fichero `SD Content/explorer/EXPLORER.B81`** (el que de verdad
>    va en la tarjeta SD) todavía comprueba la extensión `.SCR`/`.VGM` y
>    las trata aparte (líneas 110/120) — pero el propio `explorer.asm`
>    actual ya las intercepta ANTES de devolver el control al BASIC
>    (`vt_activate`: `.VGM`/`.PEB`/`.SCR`/`.TXT` se quedan dentro del
>    explorador, solo lo demás vuelve). Esas dos líneas del stub parecen
>    código muerto de una versión anterior. Lo señalo abajo en una nota;
>    dime si prefieres que documente el stub "tal cual está" (con la
>    nota) o que proponga una versión limpia sin las ramas muertas.

---

# Apéndice I — Software de ejemplo: CP/M+ y el explorador de archivos

Dos programas más allá del BASIC de serie, pensados para usarse juntos:
**CP/M+**, un sistema operativo completo con más memoria de programa y
comunicaciones por red, y el **explorador de archivos**, que ahorra tener
que recordar nombres de fichero y comandos `LOAD` para moverse por la
tarjeta SD.

## I.1 CP/M — un sistema operativo alternativo

Además del BASIC de serie del ZX81, el SD81 Booster puede arrancar
**CP/M**, el sistema operativo estándar de los microordenadores
profesionales de 8 bits de finales de los 70 y los 80. Existen dos
puertos para este hardware:

- **CP/M 2.2**, la versión clásica, sin bancos de memoria: hasta unos
  41 KB de TPA (área de programa transitorio) en modo MC45.
- **CP/M 3 (CP/M Plus) bancado**, que usa el paginador de memoria del
  SD81 Booster para repartir la RAM en bancos de 8 KB independientes de
  sistema y de usuario. Es el que documenta este apéndice.

CP/M 3 aporta, sobre el 2.2: más TPA (el sistema y el usuario ya no
comparten el mismo banco), un disco RAM, el reloj en tiempo real
integrado en el sellado de fecha de los ficheros, una consola con 256
caracteres y color, y un driver de red para BBS y sistemas de
comunicaciones por WiFi.

El sistema sigue el estándar de Digital Research (DRI) hasta donde el
hardware lo permite: mismo formato de CCP, BDOS y BIOS, mismas
convenciones de programa `.COM`, misma tabla pública de llamadas al
BIOS. Lo específico de este puerto es la capa de paginación de memoria
(el XIOS) y los drivers de dispositivo — disco, teclado, consola,
reloj y red — escritos para el hardware del SD81 Booster.

### CP/M 2.2: los dos modos de compilación

CP/M 2.2 se distribuye en dos variantes, seleccionadas al compilar,
según se use o no el modo **MC45** (ver 11.3 y "Extensión de MC45 a los
bloques 6 y 7"):

| Modo | MC45 | TPA | RAM de vídeo | Recomendado para |
|---|---|---|---|---|
| 32 KB | No | ~24 KB | `$8000` | Uso general de CP/M |
| 48 KB | Sí | ~41 KB | `$C000` | Turbo Pascal y programas grandes |

El modo de 32 KB funciona en cualquier SD81 Booster sin nada especial
que activar. El de 48 KB necesita MC45 encendido, y da casi el doble
de TPA — la diferencia entre poder compilar con Turbo Pascal o no.

CP/M 3 necesita **toda** la memoria por encima de los 32 KB para su
paginación en bancos, no solo los bloques 4 y 5 — de ahí que use la
extensión de MC45 a los bloques 6 y 7 descrita en el capítulo 11
(`POKE 2062,170`). El propio arranque de CP/M 3 la activa por su
cuenta, sin que el usuario tenga que hacer nada.

### Instalación y arranque

El sistema se distribuye como un único fichero, `SYSTEM.BIN`, que
contiene el BIOS, la BDOS y el CCP ya enlazados. Se carga desde BASIC
igual que cualquier bloque de código máquina y se arranca con `USR`:

```
LOAD FAST "SYSTEM.BIN" CODE 24576
RAND USR 24576
```

Además de `SYSTEM.BIN`, la tarjeta necesita las imágenes de disco:
`A.IMG` a `D.IMG` (256 KB de directorio, 2 MB de datos cada una) en la
raíz de la SD. Las imágenes de CP/M 3 tienen un formato propio y **no**
son compatibles con las de CP/M 2.2 ni con las de BASIC — no se pueden
mezclar. Una quinta unidad, `E:`, es un disco RAM que no necesita
imagen: se crea vacío en cada arranque y su contenido se pierde al
apagar o resetear.

### Memoria bancada

El SD81 Booster pagina la memoria en 8 bloques de 8 KB. CP/M 3 usa esa
capacidad para mantener **dos contextos** completos: uno de sistema
(donde vive el BIOS y la BDOS) y uno de usuario (la TPA, donde corren
los programas). El bloque más alto (`$E000`-`$FFFF`) es común a los
dos contextos — ahí vive todo lo que necesita ser alcanzable sin
importar cuál esté activo, incluida la tabla pública de llamadas al
BIOS (ver más abajo).

Un programa de usuario no necesita saber nada de esto: el cambio de
contexto lo gestiona el propio sistema en cada llamada a la BDOS o al
BIOS. Solo importa si se escribe código que accede a memoria fuera de
la TPA, o que necesita ir más rápido que una llamada normal a la BDOS
(ver "Llamar al BIOS directamente").

### Discos: `A:`-`D:` (tarjeta SD) y `E:` (RAM)

`A:` a `D:` son las cuatro unidades respaldadas por la tarjeta SD, con
formato CP/M 3 estándar (2 KB por bloque de asignación, 256 entradas
de directorio). `E:` es un disco RAM de unos 390 KB, rápido pero
volátil — útil para compilaciones, ficheros temporales o cualquier
cosa que no necesite sobrevivir a un reset.

`MOUNT3.COM` permite cambiar en caliente la imagen montada en una
unidad sin reiniciar el sistema — necesario para intercambiar
disquetes virtuales durante una sesión larga.

### Teclado

El ZX81 no tiene teclado ASCII completo: le faltan símbolos, y solo
tiene una tecla de flecha por combinación con `SHIFT`. La tabla
siguiente resume cómo se llega a cada carácter. Ver "I.3 Tabla de
teclado de CP/M" para el listado completo.

- **Directa** y **`SHIFT`+tecla**: letras, dígitos y las flechas
  (estilo WordStar: `SHIFT`+5/6/7/8 = izquierda/abajo/arriba/derecha).
- **`ENTER`+tecla**: los símbolos que el ZX81 no tiene serigrafiados
  directamente — incluidos los que hacen falta para programar en C o
  usar una BBS (`@ \ | ~ \` _ # % & !`, entre otros).
- **`SHIFT`+`ENTER`, y luego una tecla**: modo control. Da `^A`-`^Z`
  con las letras, y `NUL` con la barra espaciadora.
- **`SHIFT`+9** y **`SHIFT`+0**: retroceso (`BS`, `$08`) y borrado
  (`DEL`, `$7F`) — las dos, porque distinto software remoto espera un
  byte distinto para la misma función.
- **`SHIFT`+`.`**: tabulador (`TAB`, `$09`).

### Reloj en tiempo real

CP/M 3 sella la fecha y hora de creación/modificación de cada fichero
usando el RTC de la placa. La hora se ajusta con el propio RTC del
SD81 Booster (por ejemplo desde BASIC, o sincronizándolo por NTP si
hay módulo WiFi) — CP/M solo lee lo que el RTC ya tiene.

### Consola: terminal con 256 caracteres y color

La consola local entiende un subconjunto de secuencias ANSI/VT100,
suficiente para BBS, editores de pantalla completa y cualquier
programa que pinte con color:

- Movimiento de cursor: `ESC[fila;colH` (o `f`), `ESC[nA/B/C/D`
  (arriba/abajo/derecha/izquierda), `ESC[s`/`ESC[u` (guardar/restaurar
  posición).
- Borrado: `ESC[2J` (pantalla), `ESC[K` (fin de línea).
- Color: `ESC[...m` (SGR) — ver la utilidad `SGR` más abajo para la
  lista de códigos soportados.
- `ESC[6n` (petición de posición del cursor): se contesta de verdad,
  con `ESC[fila;colR` — necesario porque algunos programas (BBS
  incluidas) miden el tamaño del terminal así antes de arrancar.
- 256 caracteres (juego CP437 completo, no solo el ASCII imprimible).

### Llamar al BIOS directamente

Todas las funciones del BIOS son alcanzables desde un programa de
usuario sin pasar por la BDOS, a través de una tabla pública de saltos
de 3 bytes cada uno, empezando en la dirección `$FF00`:

| Función | Índice | Dirección |
|---|---|---|
| CONST | 2 | `$FF06` |
| CONIN | 3 | `$FF09` |
| CONOUT | 4 | `$FF0C` |
| AUXOUT | 6 | `$FF12` |
| AUXIN | 7 | `$FF15` |
| AUXIST | 18 | `$FF36` |
| AUXOST | 19 | `$FF39` |

(dirección = `$FF00 + índice × 3`; la lista completa de 33 entradas
sigue el orden estándar de CP/M 3).

Es la misma técnica que usan WordStar y Turbo Pascal para no pagar el
coste de la BDOS en operaciones de consola frecuentes, y está pensada
para programas que necesiten el máximo rendimiento posible — por
ejemplo, un terminal de comunicaciones. El programa `TERM` incluido
con el sistema (ver más abajo) es un ejemplo real: llama a `CONOUT`
así, y eso multiplica por varias veces la velocidad de la consola
frente a pasar por la BDOS.

### Comunicaciones

El sistema incluye un driver de red (`NET`) que expone al Z80 un flujo
de bytes crudo, como si tuviera un módem conectado — el propio
software decide qué hacer con ese flujo (por ejemplo, mandar comandos
`AT` para marcar una conexión). El puente WiFi real lo gestiona el
módulo ESP32 de la placa; el Z80 no sabe nada de sockets ni de
direcciones.

**`TERM.COM`** es el terminal incluido para usar `NET`. A diferencia de
un programa CP/M típico, **no** pasa por el dispositivo lógico `AUX:`
ni por la BDOS: habla con el hardware de red directamente, con el mismo
truco de llamada directa al BIOS descrito arriba para la salida por
pantalla. La razón es de rendimiento — un terminal genérico que
respetara `DEVICE AUX:` sería varias veces más lento — y significa que
`TERM` está pensado específicamente para `NET`, no como sustituto de
un terminal de comunicaciones de propósito general.

Teclas de `TERM` (todas con `ENTER`+tecla):

| Tecla | Función |
|---|---|
| `ENTER`+`0` | Salir |
| `ENTER`+`9` | Conmutar el eco local (verlo escrito mientras se teclea) |
| `ENTER`+`8` | Conmutar el volcado a fichero (con `TERM fichero.BIN`) |

### Utilidades incluidas

| Programa | Función |
|---|---|
| `MOUNT3.COM` | Cambia en caliente la imagen montada en una unidad |
| `TERM.COM` | Terminal de comunicaciones (ver arriba) |
| `CLS.COM` | Borra la pantalla y sitúa el cursor en el origen |
| `SGR.COM` | Cambia el color de tinta/papel de la consola |
| `BORDER.COM` | Cambia el color del borde |

**`SGR parámetros`** manda la secuencia ANSI `ESC[parámetros m` a la
consola, separados por `;`:

| Código | Efecto |
|---|---|
| `0` | Restaurar los colores por defecto |
| `1` | Tinta brillante |
| `30`-`37` | Color de tinta: negro, rojo, verde, amarillo, azul, magenta, cian, blanco |
| `40`-`47` | Color de papel, misma tabla de colores |

```
SGR 0            reset
SGR 33           tinta amarilla
SGR 1;33;44      tinta amarilla brillante, papel azul
```

**`BORDER n`**, con `n` de 0 a 7 (color normal) o de 8 a 15 (mismo
color brillante, sumando 8) — ver "Escritura (OUT 7FEFh)" en el
capítulo de programadores para el formato completo del puerto.

### I.2 Tabla de teclado de CP/M

![Teclado del ZX81 con las combinaciones de CP/M](keyboardSD81_CPM.png)

Las teclas `ENTER`+`8` y `ENTER`+`9` se rotulan por su función (`LOG`,
`ECO`) en vez de por el código de control que mandan (`$1E`, `$1C`):
son las que usa `TERM` para su propio menú.

| Tecla | Directa | `SHIFT` | `ENTER` |
|---|---|---|---|
| 1 | `1` | `ESC` | `!` |
| 2 | `2` | — | `@` |
| 3 | `3` | — | `#` |
| 4 | `4` | — | `\|` |
| 5 | `5` | ← | `%` |
| 6 | `6` | ↓ | `&` |
| 7 | `7` | ↑ | — |
| 8 | `8` | → | — |
| 9 | `9` | `BS` | `^\` |
| 0 | `0` | `DEL` | `^]` |
| Q | q | Q | `'` |
| W | w | W | `{` |
| E | e | E | `}` |
| R | r | R | `[` |
| T | t | T | `_` |
| Y | y | Y | `]` |
| U | u | U | `$` |
| I | i | I | `(` |
| O | o | O | `)` |
| P | p | P | `"` |
| A | a | A | — |
| S | s | S | `\` |
| D | d | D | — |
| F | f | F | `~` |
| G | g | G | `` ` `` |
| H | h | H | `^` |
| J | j | J | `-` |
| K | k | K | `+` |
| L | l | L | `=` |
| Z | z | Z | `:` |
| X | x | X | `;` |
| C | c | C | `?` |
| V | v | V | `/` |
| B | b | B | `*` |
| N | n | N | `<` |
| M | m | M | `>` |
| `.` | `.` | `TAB` | `,` |
| ESPACIO | espacio | espacio | espacio |
| ENTER | `CR` | `CR` | `CR` |

Además, `SHIFT`+`ENTER` y luego una letra da `^A`-`^Z` (letra `AND
$1F`), y `SHIFT`+`ENTER`+espacio da `NUL`.

### I.3 Mapa de memoria de CP/M 3 (resumen)

| Rango | Contenido |
|---|---|
| `$0000`-`$DFFF` | Bancado: TPA (contexto de usuario) o BIOS/BDOS/CCP (contexto de sistema), según cuál esté activo |
| `$E000`-`$FFFF` | Común a los dos contextos: el SCB (`$FE00`), la tabla pública de llamadas al BIOS (`$FF00`), y el resto del estado que tiene que verse igual desde ambos lados |

El tamaño exacto de la TPA depende de la configuración final del
sistema; se puede consultar arrancando y mirando el mensaje inicial de
CP/M 3 (`Banked memory, NN.NK TPA`).

---

## I.4 El explorador de archivos

El explorador (`EXPLORER.BIN`) es un navegador de la tarjeta SD en modo
Superfast HiRes Spectrum (42 columnas, con una fuente comprimida de 6
píxeles), pensado para no tener que recordar nombres de fichero ni
comandos `LOAD` para cargar un programa, ver una imagen o gestionar la
tarjeta.

### Arranque

```basic
   2 LET ORG=24576
   3 LOAD FAST
  10 FAST
  12 LOAD *FULLPAG
  15 LOAD THEN CLEAR ORG-1
  20 LOAD FAST "/EXPLORER/EXPLORER.BIN" CODE ORG
  30 LET N=USR ORG
  40 IF N=0 THEN STOP
  50 LET F$=""
  55 LET EXT = 255
  60 FOR I=0 TO N-1
  65 LET A$=CHR$ (PEEK (ORG+3+I))
  67 IF A$="." THEN LET EXT = I+1
  70 LET F$=F$+A$
  80 NEXT I
  85 SLOW
  90 LET E$=F$(EXT TO )
  95 LOAD THEN CLEAR 32768
 100 IF (E$ = ".P") OR (E$=".WAV") OR (E$=".ROM") THEN LOAD FAST F$
 300 GOTO 10
```

Al salir del explorador con un archivo seleccionado (tecla `ENTER` u `8`
sobre un fichero que no sea `.VGM`, `.PEB`, `.SCR` ni `.TXT` — esos
cuatro los gestiona el propio explorador, ver más abajo), este stub
recoge el nombre en `F$` y decide qué hacer según la extensión. Tal
como está, solo actúa sobre `.P`, `.WAV` y `.ROM`; para cualquier otra
extensión que quieras cargar de otra forma, añade tu propia condición
antes de la línea `300`.

### Navegación

| Tecla | Acción |
|---|---|
| `6` | Bajar en la lista |
| `7` | Subir en la lista |
| `1` | Página anterior |
| `2` | Página siguiente |
| `5` | Subir a la carpeta padre |
| `8` / `ENTER` | Entrar en la carpeta seleccionada, o abrir/cargar el archivo |
| `ESPACIO` | Salir al BASIC (sin ningún archivo seleccionado) |
| `.` | Filtrar el listado por comodines (`*`, `?`) |
| `SHIFT`+`1` | Quitar el filtro activo |

El filtro persiste mientras navegas entre carpetas; solo se borra con
`SHIFT`+`1`.

### Qué pasa al seleccionar un archivo

Según la extensión, `ENTER`/`8` hace una de estas cuatro cosas **sin
salir del explorador**, o devuelve el nombre al BASIC:

| Extensión | Comportamiento |
|---|---|
| `.VGM` | Se carga y empieza a sonar en el acto |
| `.PEB` | Igual que `.VGM`, para efectos generados con el editor PEG |
| `.SCR` | Se muestra a pantalla completa (captura Spectrum, 6912 bytes); cualquier tecla vuelve al listado |
| `.TXT` | Se abre en el visor de texto (ver abajo) |
| cualquier otra | El explorador sale al BASIC con el nombre del archivo, para que el stub decida qué hacer |

### Operaciones de fichero

| Tecla | Acción |
|---|---|
| `N` | Nueva carpeta (pide el nombre) |
| `D` | Borrar el fichero o carpeta seleccionado (pide confirmación; las carpetas solo se borran si están vacías) |
| `R` | Renombrar (pide el nombre nuevo, precargado con el actual) |
| `C` | Marcar el seleccionado para copiar |
| `X` | Marcar el seleccionado para mover |
| `V` | Pegar (copia o mueve, según lo marcado con `C`/`X`, en la carpeta actual) |

Al marcar con `C` o `X`, el icono correspondiente de la barra inferior
se tiñe de azul como recordatorio de que hay algo pendiente de pegar.

### Edición de texto (nueva carpeta / renombrar / filtro)

Editor con cursor real (inserta y borra en la posición del cursor, no
solo al final):

| Tecla | Acción |
|---|---|
| letras, dígitos, `.` | Carácter literal |
| `SHIFT`+`Z`/`X`/`C`/`V` | `:` `;` `?` `/` |
| `SHIFT`+`J`/`K`/`L` | `-` `+` `=` |
| `SHIFT`+`N`/`M`/`.` | `<` `>` `,` |
| `SHIFT`+`B` | `*` |
| `SHIFT`+`` ` `` (barra espaciadora) | `` ` `` |
| `SHIFT`+`5` / `SHIFT`+`8` | Mover el cursor izquierda / derecha |
| `SHIFT`+`0` | Borrar (el carácter a la izquierda del cursor) |
| `SHIFT`+`1` | Restaurar el nombre original (solo al renombrar) |
| `ENTER` | Confirmar |
| `ESPACIO` | Cancelar |

### Panel de configuración (`S`)

La tecla `S` abre y cierra un panel lateral con el estado de la FPGA y
del reproductor de música, y permite cambiarlo sin salir al BASIC:

| Tecla | Acción |
|---|---|
| `W` | Conmutar WRX (generador de caracteres en la RAM de 8-16K) |
| `F` | Conmutar paginación completa (`FULLPAG`) |
| `M` | Conmutar MC45 |
| `A` | Conmutar el juego de caracteres entre 64 y 128 |
| `J` | Editar las cinco teclas del joystick (arriba, abajo, izquierda, derecha, fuego) |
| `T` | Parar la música (VGM/PEB) en curso |
| `Y` | Pausarla |
| `U` | Continuarla |
| `I` | Ver `/MAN/IP.TXT` (la IP del módulo WiFi, si hay conexión) |

El panel también muestra, en su última línea, la versión del MCU, la
ROM y la FPGA (`Mx.y,Rx.y,Fx.y`). `W`/`F`/`M`/`A`/`J`/`T`/`Y`/`U` solo
actúan con el panel visible.

### Visor de texto (`.TXT`)

| Tecla | Acción |
|---|---|
| `6` / `7` | Línea siguiente / anterior |
| `1` / `2` | Página anterior / siguiente |
| `ESPACIO` | Salir al listado |

### Visor hexadecimal

Se abre desde el panel de configuración con la tecla `H`, sobre el
archivo seleccionado en el listado (no disponible para carpetas):

| Tecla | Acción |
|---|---|
| `6` / `7` | Fila siguiente / anterior |
| `1` / `2` | Página anterior / siguiente |
| `G` | Ir a una dirección (hexadecimal) |
| `Z` | Alternar la columna derecha entre ASCII y códigos de carácter ZX81 |
| `ESPACIO` | Salir al listado |

### Cómo se devuelve un archivo al BASIC

El explorador vive en RAM como código máquina suelto (cargado con
`LOAD ... CODE`), así que no puede tocar él mismo la zona de programa y
variables BASIC — se limita a devolver el nombre del archivo
seleccionado (para los tipos que no gestiona internamente) y dejar que
el stub decida. Al salir:

1. Copia el nombre (ya en ASCII) a una dirección fija, `ORG+3`
   (24579 con el `ORG` de arriba), que sobrevive a la restauración de
   vídeo.
2. Restaura el modo de vídeo estándar y el mapeo de memoria normal
   (identidad: cada bloque a su página del mismo número).
3. Devuelve en `BC` la longitud del nombre (0 si se salió con
   `ESPACIO`) — en ZX81, `USR dirección` como función devuelve el
   contenido de `BC`.

### Ensamblar

Con [pasmo](http://pasmo.speccy.org/):

```
pasmo explorer.asm EXPLORER.BIN
```
