# Teclado virtual (rama `teclado-remoto`): qué tiene que hacer el emulador

Desde la página `/debug` se pueden "pulsar" teclas del ZX81 (el teclado del PC o uno en
pantalla). La FPGA suma a las lecturas del puerto `$FE` las teclas que el MCU le dice, igual que
suma las del joystick. El resto del camino (web, ESP32, MCU) no existe en el emulador: solo
hace falta la parte de la FPGA y su orden de configuración.

## La orden 13 del canal de configuración

El MCU manda una fila de la matriz con la orden **13** (`cfgcmd_KEYS`): el código de la orden en
4 bits y 8 bits de dato, el bit menos significativo primero (como las demás órdenes):

```
bits 0-2   la fila (0-7)
bits 3-7   las 5 columnas de esa fila: bit 3 = columna 0 (D0) ... bit 7 = columna 4 (D4); 1 = pulsada
```

La FPGA guarda 8 filas de 5 bits (40 bits, todos a 0 al arrancar), y escribir una fila no toca
las demás. El MCU suelta todas las filas al arrancar y, si deja de llegar el estado de la web
durante 1,5 s con alguna tecla pulsada, las suelta también.

## Efecto en el puerto `$FE`

Al leer el puerto `$FE` con A0 = 0 (la lectura de teclado de siempre), para cada fila `i` cuya
línea de dirección **A(8+i)** vale 0, las columnas pulsadas de esa fila se **suman** (a 0) a lo
que da el teclado real y el joystick. Si hay varias filas elegidas a la vez (A8 y A15 a 0, como
hace la ROM para mirar todo el teclado de una vez), se suman todas:

```
D(j) = 0 si hay una tecla real, del joystick o virtual pulsada en la columna j de alguna de las filas elegidas
```

## La matriz (igual que la del ZX81)

| Fila (línea) | D0 | D1 | D2 | D3 | D4 |
|---|---|---|---|---|---|
| 0 (A8)  | SHIFT | Z | X | C | V |
| 1 (A9)  | A | S | D | F | G |
| 2 (A10) | Q | W | E | R | T |
| 3 (A11) | 1 | 2 | 3 | 4 | 5 |
| 4 (A12) | 0 | 9 | 8 | 7 | 6 |
| 5 (A13) | P | O | I | U | Y |
| 6 (A14) | ENTER | L | K | J | H |
| 7 (A15) | SPACE | . | M | N | B |

Un SHIFT + tecla son dos bits, uno en la fila 0 y otro en la de la tecla. La ROM del ZX81 mira el
teclado una vez por imagen, así que una tecla tiene que estar pulsada al menos 2-3 imágenes
(la web la mantiene 120 ms como mínimo).

## Lo que no hay que replicar

La orden 13, la web, el ESP32 y la cola del MCU son del hardware real. En el emulador basta con
que el teclado virtual sea la misma matriz: una función que ponga o quite teclas (o una ventana)
y que se mezcle con el teclado y el joystick al leer `$FE`.
