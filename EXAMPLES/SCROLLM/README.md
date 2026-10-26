# SCROLLM -- prueba del motor de scroll por hardware (fases 2 y 3)

Scroll **horizontal y vertical continuo** en Superfast HiRes nativo (`POKE 2045,171`) y en Spectrum (`172`) con el **motor de
movimiento** de la FPGA de la rama `fpga-scroll` (ver `claude/planning/scroll_hardware.md`). Es el paso que sigue a `SCROLLT` y
`SCROLLH`: allí solo se movía el fino (la visualización); aquí, cuando el fino llega al extremo, el motor mueve la **memoria** un byte
(horizontal) o una fila de texto (vertical) y el fino vuelve al otro extremo, así que se puede scrollar sin límite, en diagonal también. **Solo funciona en la placa con esa FPGA**: el emulador todavía no tiene el
motor ni los registros del puerto `$3FEF`. Necesita RAM en `$A000-$DFFF` (no vale el modo 32K).

## El mundo

Un mundo que se repite cada 256 columnas y cada 256 filas: **diagonales negras de un píxel** sobre colores, con un período de 16 píxeles
que cruza los límites de byte (una fila más abajo equivale a una columna más a la derecha). Cualquier junta mal puesta (un byte duplicado, perdido o desplazado) se ve como un
**escalón** en una diagonal. El color de fondo cambia con la columna del mundo y con los atributos:

| Modo | Fondo |
|---|---|
| HiRes, atributos **modo 0** (por defecto) | franjas de una línea de alto (8x1) |
| HiRes, atributos **modo 1** (tecla M) | celdas de color de 8x8 |
| **Spectrum** (tecla T) | celdas de color de 8x8 (atributos del Spectrum) |

## Cómo funciona (el protocolo del motor)

Un paso a la izquierda (tecla P), cada cuadro un píxel:

1. Cuadro n: el fino llega a 7 y se **manda la orden** (`OUT (C),$41` y `OUT (C),1`, con `BC = $3FEF`). El motor la ejecuta en el
   borrado, desde la línea 254, al final de ese cuadro. El fino sigue en 7.
2. Cuadro n+1: se lee el estado (`OUT $20` y `IN`; bit 7 = ocupado). Si ya está libre, el fino vuelve a 0 y el Z80 rellena el
   búfer de la derecha con la columna que entra. El de la izquierda ya está bien: es el byte que acaba de salir, que el motor
   guarda solo.
3. El resultado es el mismo que si el fino hubiera pasado de 7 a 8: ni un salto.

A la derecha (tecla O) es lo mismo al revés: la orden se manda cuando el fino llega a 0, y al acabar el fino pasa a 7 y se rellena el
búfer de la izquierda (en HiRes modo 1 también la columna 0 de la rejilla de atributos, que no tiene búfer de la izquierda).
**Cambiar de sentido no obliga a regenerar nada**: el byte que salió por un lado queda en el búfer de ese lado.

### Vertical (fase 3)

Igual con el fino Y (registro 8 del puerto: `OUT (C),$40` y el valor) y las órdenes 3 (arriba: el contenido sube) y 4 (abajo):

- **Arriba (tecla Q):** el fino sube hasta 7 y se manda la orden 3. Al acabar, el fino vuelve a 0 y el Z80 rellena el búfer de **abajo** (B: la fila
  del mundo que va a entrar, 8 líneas de 32 bytes con sus atributos) y su **esquina** (la columna R de esa fila), y la columna L de la fila que ha entrado.
  El búfer de **arriba** (T) es la fila que acaba de salir: el motor la guarda solo.
- **Abajo (tecla A):** la orden 4 con el fino en 0; al acabar, fino a 7 y se rellena T y las columnas R y L de la fila que ha entrado (arriba no hay esquina).
  En HiRes modo 1 la rejilla de atributos no tiene T, así que se escribe también la fila entera de arriba de la región.
- **Región:** el motor mueve las filas `reg_top`..`reg_bot` (registros 10 y 11): todas (0-23), o con el HUD las 1-22; la máscara de filas no cuenta en vertical.
- **Tras un paso horizontal** B y T quedan desalineados con la nueva columna 0 (el motor horizontal no los mueve); el programa los marca y los rellena al
  empezar el siguiente paso vertical.

## Teclas

| Tecla | Acción |
|-------|--------|
| P / O | izquierda / derecha (se repite, 1 píxel por cuadro) |
| Q / A | arriba / abajo (se repite, 1 píxel por cuadro). Con P u O a la vez: **diagonal** |
| V | el modo automático (SPACE) va arriba / abajo en vez de izquierda / derecha |
| M | atributos de HiRes modo 0 / modo 1 (se redibuja todo, unos 0,3 s). En Spectrum no hace nada |
| T | HiRes / Spectrum (se redibuja todo) |
| H | **HUD**: las filas 0 y 23 no se mueven (máscara de filas, `POKE 2091-2093`, y región vertical 1-22) |
| D | **doble buffer** AUTO (bloque 7 de delante) sí / no |
| S | **dos sprites** fijos (aros amarillo y magenta) sí / no |
| N | **ruido**: el Z80 escribe sin parar en `$9F00-$9FFF`, también mientras corre el motor |
| SPACE | modo automático: 160 cuadros a la izquierda, 160 a la derecha, y así |
| X | salir a BASIC (deja todo a cero y el modo ZX81) |

## Qué se tiene que ver

- Con **P** mantenida, el dibujo se desplaza **de forma continua hacia la izquierda**, sin tirones cada 8 píxeles ni escalones en las
  diagonales; con **O**, hacia la derecha. Una velocidad uniforme de 50 píxeles por segundo.
- Con **Q** mantenida, el dibujo sube **de forma continua**, sin tirones cada 8 líneas ni cortes en las diagonales; con **A**, baja. Una velocidad uniforme
  de 50 píxeles por segundo, y lo mismo en diagonal (P + Q, O + A...), donde cada eje se espera un cuadro cuando el otro está cerrando un movimiento.
- Los **colores van con su byte**, también al cruzar el límite de 8 píxeles (y de 8 líneas).
- Al **cambiar de sentido** (soltar P y pulsar O) tampoco hay ninguna junta: eso prueba que el byte que sale se guarda en el búfer opuesto.
- **H** (HUD): las filas 0 y 23 (una banda de rayas verticales de 4 píxeles sobre blanco brillante, arriba y abajo) **se quedan quietas** mientras
  el resto se desplaza. Si se mueven con el resto, el motor no respeta la máscara de filas; si se mueven solo a ratos (cada 8 píxeles), es el motor, no el fino.
- **D** (doble buffer): el scroll tiene que verse **igual**. Si cada 8 píxeles hay un salto de un cuadro, el blit se ha hecho antes de que acabara el motor.
- **S** (sprites): los dos aros no se mueven, no parpadean ni se parten mientras todo lo demás se desplaza.
- **N** (ruido): el scroll tiene que verse igual y sin ninguna junta, aunque el Z80 esté escribiendo en la RAM de la pantalla justo cuando corre el
  motor (en `$9E80-$9EFF`, una zona que no se mueve ni se ve). Es la prueba de que la CPU y el motor se reparten bien el puerto de la BRAM.
- **T** (Spectrum): lo mismo que en HiRes, con el orden de líneas y los atributos del Spectrum.
- **M** cambia los atributos de HiRes; el movimiento sigue igual con el nuevo juego.

Si al subir o bajar cada 8 líneas se ve un salto, una fila repetida o una fila de otro color, el motor vertical no mueve exactamente una fila, o el búfer
de entrada (B arriba / T abajo) no tenía la fila correcta; si solo falla el borde derecho o izquierdo de la fila que entra, son las columnas R y L (la esquina).
Con el HUD, las filas 0 y 23 no deben moverse ni al subir ni al bajar. Si en cada 8 píxeles se ve un salto, o una columna repetida o saltada, el motor no mueve exactamente un byte (o la orden llega con el fino mal
puesto). Si solo falla el color, el motor no mueve los atributos del modo activo. Si falla justo al invertir el sentido, es el búfer opuesto.
Si no se mueve nada, mirar el estado: `OUT $20` y `IN` tiene que dar `$C0` justo después de mandar la orden (ocupado y pendiente), luego `$A0` y `0`.

## Memoria (HFILE = `$8000`)

| Qué | Dónde |
|---|---|
| HiRes: bitmap lineal | `$8000-$97FF` |
| HiRes: entrada derecha / izquierda (bitmap) | `$9800` / `$9C00` (192 bytes) |
| HiRes modo 0: atributos, entrada derecha / izquierda | `$C000-$D7FF`, `$D800` / `$DC00` |
| HiRes modo 1: 25 filas de 33 (base `$9FFF` + 1) | `$A000-$A338` |
| Spectrum: bitmap entrelazado, atributos | `$8000-$97FF`, `$9800-$9AFF` |
| Spectrum: entrada derecha (bitmap, atributos) / izquierda | `$9B00`, `$9BC0` / `$9C00`, `$9CC0` |
| Vertical, HiRes: B bitmap / esquina / T bitmap | `$9900` (8 líneas de 32) / `$9A00` (8) / `$9D00` (8 líneas de 32) |
| Vertical, HiRes modo 0: B atributos / esquina / T atributos | `$D900` / `$DA00` / `$DD00` |
| Vertical, HiRes modo 1 | B = fila 24 de la rejilla (`$A318-$A338`); no hay T |
| Vertical, Spectrum: B bitmap / esquina / T bitmap | `$9D00` / `$9E40` (8) / `$9F00` |
| Vertical, Spectrum: B atributos / T atributos / esquina | `$9E00` / `$9E20` / `$9E48` |
| Ruido | `$9E80-$9EFF` |
| Tablas de trabajo del programa | `$A400-$AEBF` |

## Probado en el PC

`claude/mqtttest/build_scrollm.sh` ejecuta el programa en el Z80 de EightyOne con un **modelo en software del motor**
(escrito aparte, con la máscara de filas en horizontal y la región en vertical) y el estado de ocupado, y en cada cuadro estable compara la memoria con el
mundo esperado: las 32 columnas, los búferes de la derecha y de la izquierda, los de abajo, arriba y la esquina (cuando están alineados) y los atributos del modo
activo, con el HUD intacto. Recorre izquierda, derecha, arriba, abajo y las cuatro diagonales pasando por detrás del origen en HiRes modo 0 y 1 y en Spectrum, con y
sin HUD, doble buffer, ruido y sprites, y el automático horizontal y vertical (4.512 cuadros comprobados, 523 movimientos del motor). Comprueba también que la posición
vertical (8 * fila + fino Y) avanza un píxel por cuadro sin mover la horizontal. Con 6 errores puestos a propósito (en el programa y en el modelo), falla. No prueba la FPGA:
el motor está probado aparte en `FPGA/SD81V2.1000/tb_scroll_motor.v` (197 casos contra un modelo, memoria completa: 69 horizontales y 128 verticales) y `tb_dbg.v` (la orden, el estado y el motor encadenados).
Lo que **solo** se ve en la placa es el reparto de los puertos de la BRAM con la CPU, el blit y los sprites (`SD81.v` no se puede simular entero).

## Cargar

`SCROLLM.BIN` y `SCROLLM.B81` en la SD (`LOAD FAST "SCROLLM.BIN" CODE 30000`; `SCROLLM.B81` lo hace solo).
Ensamblar: `pasmo scrollm.asm scrollm.bin scrollm.sym` (o `compila.bat`).
