# SCRL160 -- scroll en anillo de los modos 160x100 y 160x200

Prueba el scroll por hardware de los modos de 160 píxeles (`FPGA/SD81V2.1000/mode160.v`):

| POKE | Qué hace |
|------|----------|
| `2072` | fila de la imagen que sale arriba |
| `2073` | filas del anillo (0 = las visibles: 100, o las de la ventana en 4 bpp) |
| `2074` | píxel de la imagen que sale a la izquierda (en 4 bpp, impar = medio byte) |
| `2075` | bytes por fila del anillo (0 = 160 en 8 bpp, 80 en 4 bpp) |

La imagen es un anillo: lo que sale por un lado vuelve a entrar por el otro, y
la FPGA solo cambia desde dónde empieza a leer. No se copia nada. Para meter
contenido nuevo, el anillo tiene **una fila y una columna más que la pantalla**:
antes de mover, el programa pinta allí lo que va a entrar, sin que se vea.

El programa enseña un mundo infinito de baldosas de 16×16 píxeles, cada una con
la rejilla blanca, una diagonal negra y un color que depende de su posición.
Todo lo que entra se calcula al vuelo.

| Modo | Anillo | Borde |
|---|---|---|
| 160x100 a 8 bpp (`POKE 2045,175`) | 101 filas × 161 bytes (16.261 bytes desde `$8000`) | verde |
| 160x200 a 4 bpp (`POKE 2045,176`) | 201 filas × 81 bytes (162 píxeles) | azul |

## Teclas

| Tecla | Acción |
|-------|--------|
| Q A O P | Mover arriba, abajo, izquierda y derecha, un píxel por imagen (mantener pulsada) |
| S | Movimiento automático en diagonal (sí / no) |
| M | Cambiar de modo. El mundo vuelve a (0,0) y se ve cómo se pinta el anillo (unos segundos) |
| X | Salir a BASIC (deja el scroll a 0, la ventana del 4 bpp por defecto y el modo normal) |

## Qué se tiene que ver

- El mundo se mueve **píxel a píxel** en las cuatro direcciones y en diagonal,
  suave, sin saltos ni temblores.
- Las rejillas y las diagonales **siguen rectas y continuas** al pasar por el
  borde del anillo: ninguna fila ni columna repetida, cortada o de otro color.
  En 4 bpp, con los desplazamientos impares (medio byte) la imagen no salta.
- Por los bordes de la pantalla no entra basura: lo que aparece ya está
  pintado.

Si una columna o una fila sale de otro color o repetida, el fallo está en el
anillo de la FPGA (o en el programa, si pasa solo en un modo).

## Comprobado en el PC

Con el Z80 de EightyOne y un modelo de los POKEs y del teclado:

- **Guion de teclas:** en los dos modos, las cuatro direcciones, la diagonal
  automática y vueltas completas al anillo en los dos sentidos.
- **Comprobación de lo visible:** en 400 instantes al azar (también a mitad de
  imagen, mientras el programa pinta lo que va a entrar), lo visible según el
  direccionamiento de la FPGA es exactamente el mundo desde el origen que se
  escribió en el VSYNC. Esto da 0 errores en 333 instantes; los otros 67 caen
  mientras se pinta el anillo al cambiar de modo y no se comprueban.
- **Errores puestos a propósito:** la comprobación detecta los cuatro.

El direccionamiento de la FPGA está simulado aparte, en ModelSim
(`tb_mode160.v`, `tb_mode160_4bpp.v` y `tb_mode160_txt.v` con `-GVO -GVR -GHO -GHR`).
**Sin probar en la placa.**

## Ensamblar y ejecutar

```
pasmo scrl160.asm scrl160.bin scrl160.sym
```

Copia `SCRL160.BIN` y `SCRL160.B81` a la SD y carga el `.B81`
(`LOAD "SCRL160.B81"`).

Necesita la FPGA con el scroll de los modos 160. El programa ocupa
30000-$7A46 y el anillo `$8000-$BF99`.
