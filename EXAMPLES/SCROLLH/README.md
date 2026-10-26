# SCROLLH -- prueba del scroll fino por hardware en HiRes nativo (fase 1b)

Hermana de `SCROLLT` (Spectrum) para el modo **Superfast HiRes nativo** (`POKE 2045,171`): bitmap lineal de 6 KB, con
los dos tipos de atributo de Chroma. Para la **FPGA con scroll por hardware** (rama `fpga-scroll`, ver
`claude/planning/scroll_hardware.md`). **Solo funciona en la placa con esa FPGA**: el emulador todavía no tiene los
registros nuevos del puerto `$3FEF`. Necesita RAM en `$C000-$DFFF` (no vale el modo 32K).

Lo que prueba:

- **fino X** (`POKE 2090`, 0-7 px) con una **columna 33 de verdad**: por la derecha entra el búfer de entrada, ya no da la
  vuelta a la columna 0;
- **fino Y** (puerto `$3FEF`, registro 8) con una **fila 25**: por abajo entra el búfer de abajo;
- la **esquina** de abajo a la derecha cuando se mueven los dos;
- la **región vertical** (registros 10 y 11) y la **máscara de filas** (`POKE 2091-2093`): con el HUD puesto, las filas 0 y 23 no se mueven;
- los **dos atributos**: Chroma **modo 0** (un atributo por cada byte de línea, 8x1) y **modo 1** (un atributo por celda de 8x8
  y por posición, base `POKE 2059-2061`, paso 33). Se alternan con la tecla **M**.

## Cómo se ve

El fondo es la misma cuadrícula de celdas de 8x8 con una diagonal. Arriba (fila 0) sale el estado: `FX=n FY=n H=n A=n M=n`.

| Atributos | Fondo |
|---|---|
| **Modo 0** (`M=0`, por defecto) | franjas diagonales de **una línea** de alto: papel = ((línea + columna) mod 6) + 1 |
| **Modo 1** (`M=1`) | celdas de color de **8x8**: papel = ((fila + columna) mod 6) + 1 |

| Entrada | Color | Dónde |
|---|---|---|
| Derecha (columna 33) | puntos **amarillos** sobre negro | a la derecha, `FX` píxeles |
| Abajo (fila 25) | **verde** brillante macizo | abajo de la región, `FY` líneas |
| Esquina | **blanco** brillante macizo | abajo a la derecha, si `FX` y `FY` son > 0 |

## Qué se tiene que ver

Lo mismo que en SCROLLT (`EXAMPLES/SCROLLT/README.md`), en los dos modos de atributos:

- **FX = n**: la cuadrícula se desplaza n píxeles a la izquierda **con sus colores**; asoman n píxeles de la columna amarilla.
- **FY = n**: la cuadrícula sube n líneas **con sus colores**; asoman n líneas verdes. En modo 0 cada franja de una línea sube con su línea; en modo 1 la celda entera de 8x8.
- **HUD = 1**: las filas 0 y 23 quietas; la región que se mueve en vertical es la 1-22 (las líneas verdes salen encima de la fila 23).
- **HUD = 0**: se mueve todo.
- **FX = 0 y FY = 0**: la imagen es la de antes de esta fase (sin franjas de entrada), en los dos modos.
- Al pulsar **M** la imagen no se mueve: solo cambian los colores del fondo y de la entrada (se ven los dos conjuntos de búferes).

Si por la derecha ves la columna 0 en vez de la amarilla, la columna 33 no lee del búfer. Si en modo 0 las franjas de color no suben junto con sus líneas, o en
modo 1 la celda de color no viaja con su dibujo, el atributo no sigue al bitmap. Si la entrada de abajo sale con los colores del fondo, la fila 25 de atributos no se lee bien
(modo 1: la fila 24 de la rejilla en `$A000 + 24*33`).

## Teclas

| Tecla | Acción |
|-------|--------|
| P / O | FX + / - (se repite) |
| Q / A | FY + / - (se repite) |
| H | HUD sí / no |
| SPACE | modo automático |
| M | atributos modo 0 / modo 1 |
| X | salir a BASIC (deja todo a cero y el modo ZX81) |

## Memoria (HFILE = `$8000`)

| Qué | Dónde |
|---|---|
| Bitmap lineal (`y*32 + columna`) | `$8000-$97FF` |
| Entrada: derecha / abajo / esquina (bitmap) | `$9800` / `$9900` / `$9A00` |
| Atributos modo 0 (uno por byte de línea) | `$C000-$D7FF` |
| Entrada modo 0: derecha / abajo / esquina | `$D800` / `$D900` / `$DA00` |
| Atributos modo 1 (25 filas de 33; base `$9FFF` + 1) | `$A000-$A338` |

Los búferes de la izquierda y de arriba (`$9C00`/`$9D00` y `$DC00`/`$DD00`) quedan reservados para el motor de movimiento y no se tocan.

## Probado en el PC

`claude/mqtttest/build_scrollh.sh` ejecuta el programa en el Z80 de EightyOne con la memoria, los puertos (`$3FEF`, `$7FEF`), el teclado y el VSYNC simulados y
comprueba el dibujo, los dos juegos de atributos, los búferes, los registros y el recorrido de cada tecla, incluida la M (no prueba la FPGA; las direcciones
las prueba `FPGA/SD81V2.1000/tb_lin_scroll_addr.v`).

## Cargar

`SCROLLH.BIN` y `SCROLLH.B81` en la SD (`LOAD FAST "SCROLLH.BIN" CODE 30000`; `SCROLLH.B81` lo hace solo).
Ensamblar: `pasmo scrollh.asm scrollh.bin scrollh.sym` (o `compila.bat`).
