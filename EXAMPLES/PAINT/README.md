# PAINT: programa de dibujo con el ratón Bluetooth

Paint en ensamblador (`paint.asm`) sobre `EXAMPLES/MOUSE/mouselib.inc`. Dos modos de vídeo, a elegir al empezar y también desde el propio programa:

| Modo | Vídeo | Colores | Fichero |
|---|---|---|---|
| 160x100 | `POKE 2045,175`, un byte por píxel | 256 de la paleta de `pal160` (grises, rojo, verde, azul, amarillo, cian, magenta, los 16 del Chroma81 y una rueda de tonos) | `PAINTn.I16` |
| 160x200 | `POKE 2045,176`, un nibble por píxel | 16 de la paleta de 16 (los colores digitales del Chroma81) | `PAINTn.I4` |

Necesita la FPGA con los modos de 160 píxeles y la de `sim_int 0.21` (puerto del ratón), y un ratón emparejado (`LOAD *MOUSE THEN NEW`, ver `EXAMPLES/MOUSE`).

## Cargarlo

Copia `PAINT.BIN` y `PAINT.B81` a la misma carpeta de la SD, carga el `.B81` y `RUN`: pregunta el modo (1 o 2) y arranca. Se puede lanzar directamente con `POKE 24579,0` (160x100) o `POKE 24579,1` (160x200) y `RAND USR 24576`.

## Pantalla

- **Arriba:** 16 muestras de color. Izquierdo = **tinta**, derecho = **papel**. La tinta lleva un marco doble (negro y blanco) y el papel uno negro. En el modo de 256 colores se ven 16 a la vez y **la rueda cambia de banco** (16 bancos, que son las 16 filas de la paleta).
- **Debajo:** 10 botones y la casilla de la ranura. De izquierda a derecha: lápiz, línea, rectángulo, rectángulo relleno, bote de pintura, cuentagotas, borrar el lienzo, cambiar de modo (borra el lienzo), **guardar** y **cargar**. La herramienta elegida se ve en amarillo.
- **La casilla de la derecha** es la **ranura** (0-9): botón izquierdo la sube y derecho la baja.
- **El resto:** el lienzo.

## Manejo

| Gesto | Qué hace |
|---|---|
| Botón izquierdo en el lienzo | dibuja con la tinta |
| Botón derecho en el lienzo | dibuja con el papel (sirve de goma) |
| Lápiz | une cada posición del ratón con la anterior mediante una línea, sin huecos |
| Línea, rectángulos | se arrastra y se dibuja al **soltar** (sin vista previa) |
| Bote | rellena la zona del color de debajo (relleno por tramos; si hay muchísimas semillas pendientes, la pila de 1,5 KB se llena y descarta las que sobran) |
| Cuentagotas | izquierdo toma la tinta y derecho el papel, del lienzo (en 256 colores, el banco pasa al del color tomado) |
| Guardar / cargar | escribe o lee `PAINTn.I16` / `PAINTn.I4` (n = la ranura) en la carpeta actual. El borde se pone **verde** si ha ido bien y **rojo** si no (no existe, no es un fichero de este modo, error de la SD) |
| Tecla **X** | sale al BASIC |

## Formatos de fichero

- **`.I16`** (160x100): es el formato de siempre, el que abren el explorador y `IMGVIEW`: `"I160"`, 512 bytes de paleta (256 entradas de byte bajo `{G,B}` y byte alto `{BRIGHT,R}`) y 16.000 píxeles. Se puede cargar en Paint cualquier `.I16` renombrado a `PAINTn.I16` (por ejemplo una foto convertida con `EXAMPLES/PHOTO`).
- **`.I4`** (160x200): `"I400"`, **32 bytes de paleta** (16 entradas con el mismo formato) y 16.000 bytes de píxeles (80 por fila; el nibble alto es el píxel de la izquierda). `png2i4.py` de `EXAMPLES/MODE160` genera la imagen y la paleta por separado (`pic4.bin`, `pal4.bin`): concatenando `I400` + `pal4.bin` + `pic4.bin` sale un `.I4`. Al cargarlo, Paint activa esa paleta (`POKE 2067,1`).
- **Las barras no se guardan:** al guardar, las dos barras se dejan en color papel, de modo que las primeras 16 filas (160x100) o 32 (160x200) de la imagen guardada salen en blanco. Al cargar una imagen, esas filas quedan tapadas por la interfaz y se pierden en el siguiente guardado.

## Memoria

`$6000-$6EC0` programa y librería (**debe acabar antes de `$7000`**) · `$7000-$71FF` paleta en uso · `$7800-$7DFF` pila del relleno · `$7E00-` buzón del ratón · `$8000-$BE7F` imagen (16.000 bytes en los dos modos).

Corre con `DI`, como los de `EXAMPLES/MODE160`: el doble clic de `mouselib` mide con un contador propio que se incrementa una vez por fotograma (`ML_CLOCK`). El programa no usa el doble clic.

## Sin probar

Solo está ensamblado con `pasmo -I ..\MOUSE paint.asm paint.bin paint.sym` (`compila.bat`, 3.776 bytes): no se ha ejecutado ni en la placa ni en el emulador. Hay que comprobar sobre todo:

- que la flecha del cursor (el sprite 63) coincide con el píxel en el que se dibuja, en los dos modos;
- los colores de los botones y de las muestras con la paleta `pal160` (el fondo gris, el amarillo y el blanco son las entradas 119, 126 y 127);
- que el MCU acepta `FCREATE` (71) y `FWRITE` (56) en el mismo formato que usa `EXAMPLES/EDIT`, y que `PAINTn.I16` se abre luego desde el explorador;
- el color del borde que se usa como aviso (`$24` verde y `$22` rojo en `$7FEF`);
- el relleno en áreas grandes.

## Ideas pendientes

Vista previa al arrastrar líneas y rectángulos, círculos y elipses, pincel de varios píxeles, paleta editable y poder poner nombre al fichero.
