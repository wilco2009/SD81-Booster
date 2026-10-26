# SCROLLT -- prueba del scroll fino por hardware (fase 1)

Programa de prueba para la **FPGA con scroll por hardware** (rama `fpga-scroll`, ver
`claude/planning/scroll_hardware.md`). Modo Superfast HiRes Spectrum. **Solo funciona en la placa con esa
FPGA**: el emulador todavía no tiene los registros nuevos del puerto `$3FEF`.

Lo que prueba la fase 1:

- el **scroll fino horizontal** (`POKE 2090`, 0-7 px) con una **columna 33 de verdad**: por la derecha entran los
  datos del búfer de entrada (`$9B00`/`$9BC0` con `HFILE = $8000`), ya no da la vuelta a la columna 0;
- el **scroll fino vertical** (puerto `$3FEF`, registro 8, `OUT $40` y el dato) con una **fila 25**: por abajo entran
  las líneas del búfer de abajo (`$9D00`/`$9E00`);
- la **esquina** de abajo a la derecha (`$9E40`/`$9E48`) cuando se mueven los dos;
- la **región vertical** (registros 10 y 11, `OUT $42` y `OUT $43`) y la **máscara de filas** (`POKE 2091-2093`):
  con el HUD puesto, las filas 0 y 23 no se mueven.

## Cómo se ve

El fondo es una cuadrícula de celdas de 8x8 con una diagonal, y un color de papel que depende de (fila + columna)
mod 6. Arriba (fila 0) sale el estado en texto: `FX=n FY=n H=n A=n`.

| Entrada | Color | Dónde |
|---|---|---|
| Derecha (columna 33) | puntos **amarillos** sobre negro (`$AA`, atributo `$46`) | a la derecha, `FX` píxeles |
| Abajo (fila 25) | **verde** brillante macizo (`$FF`, `$44`) | abajo de la región, `FY` líneas |
| Esquina | **blanco** brillante macizo (`$FF`, `$47`) | abajo a la derecha, si `FX` y `FY` son > 0 |

## Qué se tiene que ver

- **FX = n**: la cuadrícula se desplaza n píxeles a la izquierda; **los colores van con sus celdas** (el atributo viaja
  con su byte); a la derecha asoman n píxeles de la columna amarilla y negra.
- **FY = n**: la cuadrícula sube n líneas; los colores también; por abajo asoman n líneas verdes.
- **HUD = 1** (por defecto): las filas **0** (el texto) y **23** se quedan quietas en X y en Y; lo que se mueve en
  vertical es la región 1-22, así que las líneas verdes salen justo encima de la fila 23, no al final de la pantalla.
- **HUD = 0**: se mueve toda la pantalla (filas 0-23) y las líneas verdes salen en el borde de abajo.
- **FX = 0 y FY = 0**: la imagen es idéntica a la de antes de la fase 1 (sin ninguna franja de entrada).
- Automático: FX va y viene de 0 a 7 cada 3 cuadros y FY cada 21; recorre todas las combinaciones, con la esquina blanca
  cuando los dos son distintos de 0.

Si por la derecha ves la columna 0 de la pantalla en vez de la amarilla, la columna 33 no está leyendo del búfer (el
comportamiento antiguo, con la vuelta). Si el color de una celda no se mueve junto con su dibujo, el atributo no sigue
al bitmap.

## Teclas

| Tecla | Acción |
|-------|--------|
| P / O | FX + / - (se repite) |
| Q / A | FY + / - (se repite) |
| H | HUD sí / no |
| SPACE | modo automático |
| X | salir a BASIC (deja todo a cero y el modo ZX81) |

## Probado en el PC

`claude/mqtttest/build_scrollt.sh` ejecuta el programa en el Z80 de EightyOne con la memoria, los puertos, el teclado y
el VSYNC simulados y comprueba el dibujo, los registros y el recorrido de cada tecla (no prueba la FPGA).

## Cargar

`SCROLLT.BIN` y `SCROLLT.B81` en la SD (`LOAD FAST "SCROLLT.BIN" CODE 30000`; `SCROLLT.B81` lo hace solo).
Ensamblar: `pasmo scrollt.asm scrollt.bin scrollt.sym` (o `compila.bat`).
