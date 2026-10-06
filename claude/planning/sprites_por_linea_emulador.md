# Sprites por línea y 64 sprites (ramas `fpga-camino-b` y `fpga-sprites64`): qué cambia para el emulador

Los sprites de la FPGA pasan de ser 32 "slots" con memoria propia que comparan su
posición en cada píxel, a leerse de la copia que ya hay en la RAM de sombra cuando empieza
cada línea, y de 32 a **64 sprites**. Los sprites 0-31 están en `$0C00 + sprite*32 + campo`
y los 32-63 en `$1800 + (sprite-32)*32 + campo`. El comportamiento visible es el
mismo salvo en los puntos de abajo. Código: `FPGA/SD81V2.1000/sprite_engine.v`; el banco de
pruebas que lo compara con los slots de la v1.6.0 es `tb_sprite_engine.v`.

## Lo que sigue igual

- Sprites de 8×8, posición X de 9 bits y Y de 8 bits, coordenadas desplazadas 32
  (el sprite en 32,32 es el píxel 0,0), recorte al área visible 256 × 192.
- POKEs 2100 (elige sprite, 0-63) y 2101-2128 (los 28 campos): activo, X bajo, X alto,
  Y, color por fila (8), píxel por fila (8), máscara por fila (8).
- Un píxel de sprite se ve si la máscara es 1; el color de la fila sustituye al del fondo.
- Prioridad: gana el sprite de índice más alto.
- El estado "activo" se borra con el reset del Z80.
- Un POKE 2101-2128 también se copia en la sombra (ver arriba la dirección de cada tabla),
  y los snapshots leen y escriben las dos tablas. En el formato `.Z81` las líneas
  `SPRITE n ...` admiten ahora n de 00 a 3F.

## Cómo se comporta con más de 12 sprites en una línea

El límite se aplica **línea de pantalla a línea de pantalla**, no al sprite entero:

- Un sprite con 8 filas puede dibujarse solo en algunas. Si en las 4 últimas filas hay 13 o más
  sprites que cortan esa línea, las 4 primeras salen y las 4 últimas no (ver `EXAMPLES/SPRDBUF`).
- Lo que cuenta es que el sprite **corte la línea en vertical** y esté activo, no dónde esté en X:
  trece sprites repartidos a lo ancho de la pantalla ya superan el límite, y un sprite activo
  con la X fuera de pantalla ocupa un hueco. Un sprite con la máscara a cero también ocupa
  hueco mientras siga activo. Para no gastarlo hay que desactivarlo (`POKE 2101,0`, `LOAD
  *SPRITE n STOP`).
- Los que se pierden son siempre los de menor índice (el 63 es el que gana, el 0 el que pierde).
  No hay otro criterio de prioridad.

## Lo que cambia

1. **Máximo 12 sprites por línea de pantalla.** En cada línea visible se toman los sprites
   activos que la cortan (`0 <= (y_linea + 32) - Y < 8`, módulo 512) recorriéndolos del 63 al 0.
   Los 12 primeros se dibujan; los demás no aparecen **en esa línea**. Como se recorre de mayor a
   menor índice, los que se pierden son siempre los de menor prioridad.
2. **Cuándo se decide qué sprites hay en una línea.** Al empezar el sincronismo horizontal de
   esa línea (unos pocos relojes de píxel después de que `line_cnt` cambie). Un POKE a un sprite
   que llega después se ve a partir de la línea siguiente. Antes se veía en el mismo píxel.
3. **Sprites 64 y mayores.** `POKE 2100,n` con `n >= 64` no hace nada: sus POKEs 2101-2128 no
   modifican ningún sprite ni se copian a la sombra. (Con la FPGA de 32 sprites, un `n >= 32`
   caía en la copia de la sombra del sprite `n mod 32`.)
3b. **ROM (nueva).** `LOAD *SPRITE`, `*SPRCOL`, `*SPRPIX` y `*SPRMASK` aceptan 0-63, y el arranque de la
   ROM pone a cero los 64 sprites. Una ROM antigua con la FPGA nueva deja los sprites 32-63 con
   lo que hubiera en esa zona de la sombra (los bytes de la ROM, pero desactivados: el bit de
   activo lo pone solo el POKE 2101).
4. **Un sprite sin activar.** El bit de activo no se lee de la sombra: la FPGA guarda uno por
   sprite aparte, lo borra con el reset y solo lo cambia el POKE 2101. Un `n` de 0 a 63 que
   nunca se activó no se ve aunque la sombra tenga datos.
5. **Los campos que no se escriben tras un reset.** Antes los slots conservaban su X, Y y
   gráficos de antes del reset (solo se borraba el activo). Ahora esos campos son los que haya
   en la sombra, que tras un reset de la máquina son los de la ROM, salvo que la ROM los limpie
   (la ROM nueva los pone a cero). Un sprite se activa siempre con su POKE de activo, así que
   un programa que escribe todos sus campos no nota nada.

## Cómo implementarlo en el emulador

```
al empezar el sincronismo de cada línea visible (0..191):
    slots = []
    para s de 63 a 0:
        si activo[s] y 0 <= (linea + 32) - Y[s] (mod 512) < 8:
            fila = ((linea + 32) - Y[s]) & 7
            slots.añadir( X[s] (9 bits), color[s][fila], pixel[s][fila], mascara[s][fila] )
            si len(slots) == 12: salir
en cada píxel:
    para cada slot, en orden:     # el primero es el de mayor índice
        d = (pos_x - X) mod 512
        si d < 8 y mascara[7 - (d & 7)]:
            devolver (pixel[7 - (d & 7)], color)
```

El límite de tiempo del hardware no hace falta emularlo: el motor va a 26 MHz y lee la sombra
entre el sincronismo horizontal y el píxel 120, un paso cada dos ciclos; el caso peor (64
sprites activos y los 12 que cortan la línea siendo los de menor índice) cabe en 32 columnas.

## Los modos de 70 y 80 columnas

**Los sprites no están implementados en 70 y 80 columnas**: no hay ninguna lógica específica
para esos modos (la posición sale de `pixel_cnt` con las mismas constantes que en 32 columnas,
y allí el contador va al doble de velocidad), y el manual no los describe. El emulador no tiene
que reproducir lo que la FPGA pinte en ellos. Lo único que se garantiza es que esta
modificación no cambia lo que ya hacía la FPGA en esos modos (el motor da la misma salida que
los slots antiguos, comprobado en simulación), así que lo razonable es que el emulador no dibuje
sprites con 70/80 columnas activas o los trate igual que ya lo hace.
