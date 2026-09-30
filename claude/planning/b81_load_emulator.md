# Especificación para el emulador: LOAD "PROG.B81" y arreglos del cargador de listados

El firmware del STM32 (a partir de la 2.6) carga listados BASIC en texto
directamente desde el prompt: `LOAD "PROG.B81"`. No hay cambios en la ROM:
es el mismo comando 9 (`CMD_load`), y el STM32 decide por la extensión. Si
es `.B81`, convierte el listado a `.P` en memoria y lo envía como una carga
normal de `.P`. En la SD no se escribe ningún `.P`.

El emulador tiene que hacer lo mismo en su SD81 (primera parte). De paso,
conviene corregir en el cargador de listados de EightyOne los fallos que
aparecieron al comparar los dos (segunda parte).

Referencias en el repo SD81-Booster:

- `Arduino/SD81BoosterV2_039_STM32/B81.cpp` / `B81.h`: el conversor del
  STM32. No depende de Arduino. Es una adaptación de `IBasicLoader` +
  `zx81BasicLoader` con las opciones por defecto, más los arreglos de la
  segunda parte.
- `Arduino/SD81BoosterV2_039_STM32/COMMANDS.cpp`, `load_B81()` y la rama
  `"B81"` de `cmd_load()`: el protocolo.
- `claude/b81test/`: el banco de pruebas que compila los dos conversores en
  el PC y compara los `.P` (`sh compara.sh fichero.b81 ...`).

# 1. LOAD "PROG.B81" en el SD81 emulado

## Dónde

`src/SD81Booster/SD81Booster.cpp`, `case 0x09: // CMD_load`. Hay que añadir
una rama para la extensión `B81`, igual que las de `WAV` y `ROM`, y ponerla
antes de `LoadFile()`.

No recomiendo copiar `B81.cpp` al emulador: el emulador ya tiene el
cargador. Lo recomendable es aplicar los arreglos de la segunda parte a
`IBasicLoader` / `zx81BasicLoader` y llamarlo desde el SD81. Así el
cargador de cinta (`LoadB81File`) y el del SD81 son el mismo, y los dos
iguales al STM32.

## Qué hacer

1. Resolver la ruta con `ResolveHostPath(m_recvBuf)` y leer el fichero
   entero en binario (sin conversión de fin de línea).
2. Convertir con `zx81BasicLoader(false)` y **opciones fijas**, no las del
   diálogo `BasicLoaderOptionsDialog` ni las del `.ini`, porque el STM32 no
   tiene opciones:
   `LoadBasicFromString(text, name, false, false, false, false, false, 10)`
   (no tokenizar REM ni cadenas, conservar espacios, sin grafías
   alternativas, sin ZxToken, incremento 10).
3. **Arranque automático solo con `#!basic-start=N`**, sin mirar la
   casilla "Auto-run BASIC" del diálogo de cinta:
   - Sin la metainstrucción (o con N negativo), el programa no arranca:
     NXTLIN se queda apuntando a la zona de variables, que es lo que ya
     pone `OutputEndOfProgramData`, como un programa tecleado.
   - Con `#!basic-start=N` (`ExtractBasicStartLine`), NXTLIN apunta a la
     línea N o a la siguiente que exista.
   - Si N es mayor que la última línea, no arranca. Aquí hay que apartarse
     de `PatchZX81AutoRun`: su bucle recorre `dataLen` y, pasada la última
     línea, entra en el DFILE (lee 0x7676 como número de línea) y deja
     NXTLIN apuntando al DFILE. El STM32 recorre solo el programa, hasta
     D_FILE.
   - Ojo: `PatchZX81AutoRun(..., -1)` arranca desde la primera línea, así
     que sin metainstrucción no hay que llamarlo.
4. Sin nombre delante: se envía `ProgramData()` tal cual, igual que el
   contenido de un `.P`.

## Protocolo (igual que un .P)

- Si todo va bien: `size_lo, size_hi, datos[size], 0`. `size` es
  `ProgramLength()`, que equivale a E_LINE − 16393, lo mismo que se envía
  al cargar un `.P`.
- Si hay error de conversión: `0, 0, 10`. El estado 10 lo muestra la ROM
  como informe **P**, sin tocar la memoria.
- El STM32 limita el `.P` a 16375 bytes (32768 − 16393): el programa y el
  DFILE tienen que quedar por debajo de 32K. Si se pasa, es un error
  (estado 10, "PROGRAM TOO LARGE"). En el emulador conviene el mismo límite.

## Fichero de error

Si hay error, el STM32 escribe `/MAN/B81ERR.TXT` en la SD, y se ve con
`LOAD THEN PRINT "*B81ERR"`. Si la carga va bien, lo borra. Formato, con
líneas separadas por `\n`:

```
LOAD ERROR IN B81 FILE
/RUTA/PROG.B81
TEXT LINE 2, BASIC LINE 20
INVALID CHARACTER &
20 PRINT A&B
```

- La tercera línea no aparece si no hay línea de fichero, y
  ", BASIC LINE n" tampoco si aún no se conoce.
- La última es el principio de la línea que ha fallado, hasta 47
  caracteres; lo que no es ASCII imprimible sale como `?`.
- Los mensajes del STM32 son: `INVALID CHARACTER x`,
  `INVALID CHARACTER CODE x`, `INVALID GRAPHIC x`,
  `INCOMPLETE ESCAPE SEQUENCE`, `LINE NUMBER NOT INCREASING`,
  `LINE NUMBER OUT OF RANGE`, `INVALID LABEL`, `UNKNOWN LABEL`,
  `LABEL TOO LONG`, `TOO MANY LABELS`, `NUMBER OUT OF RANGE`,
  `PROGRAM TOO LARGE`, `LINE TOO LONG` y `OUT OF MEMORY`.

En el emulador basta con escribir el mensaje de `ErrorMsg()` en ese mismo
fichero, dentro de la carpeta que hace de SD. No hace falta que el texto
sea idéntico.

## Cómo probarlo

- `EXAMPLES/SD81TEST/SD81TEST.B81` en la carpeta de la SD y
  `LOAD "SD81TEST.B81"`: no tiene `#!basic-start`, así que carga y se
  queda parado (informe 0). `RUN` arranca el menú.
- El mismo fichero con `#!basic-start=0` como primera línea: arranca solo.
- Un `.B81` con `10 PRINT A&B`: informe P y el detalle con
  `LOAD THEN PRINT "*B81ERR"`.
- Etiquetas, continuación de línea y escapes: `claude/b81test/casos.b81`.
  Arranca en la línea 40, por `#!basic-start=30`, y va a la 20 por
  `GOTO @bucle`.

# 2. Arreglos del cargador de listados (IBasicLoader / zx81BasicLoader)

Los encontré al comparar `B81.cpp` con el cargador de EightyOne sobre 166
programas reales (los `.P` de la carpeta del emulador, pasados a `.B81` con
un listador equivalente al de EightyOne). Salieron 164 `.P` idénticos. Los
otros 2 dan el mismo error en los dos, y con razón: repiten números de
línea. Los fallos de abajo no aparecen en esos programas, pero salen con
listados escritos a mano. `B81.cpp` ya los tiene corregidos (ver el
comentario del principio). Una vez aplicados aquí, los dos conversores
deben dar exactamente lo mismo; se comprueba con `claude/b81test/compara.sh`.

1. **Palabra clave dentro de una variable** (`IBasicLoader::DoTokenise`).
   Si la primera coincidencia de un token no pasa la comprobación de
   límites (`startOk`/`endOk`), el `do ... while (tokenFound)` termina y
   ese token ya no se busca en el resto de la línea. Ejemplo:
   `IF STAT THEN PRINT AT 0,0;"X"`. `" THEN "` deja "STAT " con su
   espacio, `"AT "` se encuentra primero dentro de STAT, no vale, y el
   `AT` de verdad se queda sin tokenizar. Sale como las letras A y T, y
   el primer 0 se queda además sin número oculto, porque va detrás de una
   "variable" AT (comprobado: `fa 38 39 26 39 de f5 26 39 00 1c 1a ...`
   frente a `... f5 c1 1c 7e ...`).
   Arreglo: si la coincidencia no vale, seguir buscando desde
   `pMatch + 1` en vez de salir. En `B81.cpp`: `tokenise()`,
   `from = m + 1; continue;`.

2. **Un "." suelto cuelga el cargador** (`OutputEmbeddedNumber`). Con
   `PRINT .`, `StartOfNumber` dice que sí (un '.' detrás de un token),
   `strtod(".")` no consume nada, no se emite ningún carácter e `index--`
   vuelve a la misma posición. `OutputLine` hace `i++` y vuelve a caer en
   el '.': bucle hasta "Program size too large". Arreglo: si el número
   tiene longitud 0, no tratarlo como número y emitir el carácter normal.
   En `B81.cpp`, `embeddedNumber()` devuelve false y `processLine()` emite
   el carácter.

3. **Solo números decimales** (`OutputEmbeddedNumber`). `strtod` acepta
   también hexadecimal (`0X1F`), `INF` y `NAN`. En un listado del ZX81
   solo tiene sentido lo decimal: cifras, punto opcional con cifras y un
   exponente `E[+-]cifras` si detrás de la E hay alguna cifra. `B81.cpp`
   aísla eso y solo entonces llama a `strtod`.

4. **Exponente de la coma flotante** (`zx81BasicLoader::OutputFloatingPointEncoding`).
   `floor(DBL_EPSILON + log(v)/log(2.0))` depende del redondeo de `log`.
   En una potencia de 2 que salga 2.9999999999999996 en vez de 3, el
   exponente queda uno por debajo y la mantisa sale `0x80000000`, con el
   bit de signo puesto. En las pruebas no ha salido (8, 64, 1024 y 1E3
   dan lo mismo), pero depende de la libm. Arreglo exacto:
   `m = frexp(v, &e)` (v = m·2^e, 0.5 ≤ m < 1). Entonces el exponente
   es `e − 1` (mismo rango −129..126), el byte de exponente `e + 128` y la
   mantisa `floor((2m − 1) · 2^31)`.

5. **Accesos fuera del buffer** (comportamiento indefinido):
   - `BlankLineStart`: `mLineBuffer[--i] = ' '` con `i == 0` escribe en
     `mLineBuffer[-1]`. Pasa en una etiqueta sin espacio después de los
     dos puntos: `@bucle:PRINT 1`. Arreglo: solo si `i > 0`.
   - `StartOfNumber`: el `while (mLineBufferTokenised[index-1] == ' ')`
     no mira `index > 0`, y después se lee `mLineBuffer[index-1]`.
     Arreglo: parar en 0; si se llega a 0, es número.
   - `DoTokenise`: `isalnum(pMatch[-1])` si la coincidencia está en la
     posición 0. Arreglo: `pMatch == mLineBufferTokenised || !isalnum(...)`.

6. **Minúsculas en `%x`** (`zx81BasicLoader::ExtractInverseCharacters`).
   Llama a `AsciiToZX` sin pasar a mayúsculas, y `%a` da
   `0x80 | ('a' − 'A' + 38)`, un código sin sentido. Arreglo: `toupper`
   antes, como ya hace `ExtractSingleCharacters`.

7. **"£" en UTF-8.** `AsciiToZX` solo acepta `0xA3` (Latin-1). Un `.B81`
   guardado en UTF-8 trae `C2 A3` y da "Invalid character". Arreglo: al
   leer el texto, convertir la secuencia `C2 A3` en `A3`. En `B81.cpp`
   se hace en `nextByte()`.

Sin tocar a propósito, para no cambiar resultados; `B81.cpp` los reproduce
igual:

- `BasicLineExists` siempre devuelve true: busca desde los dos puntos y
  los encuentra a ellos mismos. Por eso una línea que solo tiene una
  etiqueta gasta un número de línea (queda un hueco de 10). Es inofensivo.
- Una línea con número y solo espacios detrás (`"10 "`) genera una línea
  BASIC vacía.
- Un número detrás del nombre de una orden `*` se sigue tratando como
  hasta ahora (`LOAD *MAP 7,63` lleva número oculto, `LOAD *128C` no).
