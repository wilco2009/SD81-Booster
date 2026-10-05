# Sprites por línea (rama `fpga-camino-b`): qué cambia para el emulador

Los 32 sprites de la FPGA pasan de ser 32 "slots" con memoria propia que comparan su
posición en cada píxel, a leerse de la copia que ya hay en la RAM de sombra
(`$0C00 + sprite*32 + campo`) cuando empieza cada línea. El comportamiento visible es el
mismo salvo en los puntos de abajo. Código: `FPGA/SD81V2.1000/sprite_engine.v`; el banco de
pruebas que lo compara con los slots de la v1.6.0 es `tb_sprite_engine.v`.

## Lo que sigue igual

- 32 sprites de 8×8, posición X de 9 bits y Y de 8 bits, coordenadas desplazadas 32
  (el sprite en 32,32 es el píxel 0,0), recorte al área visible 256 × 192.
- POKEs 2100 (elige sprite) y 2101-2128 (los 28 campos): activo, X bajo, X alto,
  Y, color por fila (8), píxel por fila (8), máscara por fila (8).
- Un píxel de sprite se ve si la máscara es 1; el color de la fila sustituye al del fondo.
- Prioridad: gana el sprite de índice más alto.
- El estado "activo" se borra con el reset del Z80.
- Un POKE 2101-2128 también se copia en la sombra en `$0C00 + sprite*32 + campo`, y los
  snapshots leen y escriben esa copia como hasta ahora (el MCU no cambia).

## Lo que cambia

1. **Máximo 8 sprites por línea de pantalla.** En cada línea visible se toman los sprites
   activos que la cortan (`0 <= (y_linea + 32) - Y < 8`, módulo 512) recorriéndolos del 31 al 0.
   Los 8 primeros se dibujan; los demás no aparecen **en esa línea**. Como se recorre de mayor a
   menor índice, los que se pierden son siempre los de menor prioridad.
2. **Cuándo se decide qué sprites hay en una línea.** Al empezar el sincronismo horizontal de
   esa línea (unos pocos relojes de píxel después de que `line_cnt` cambie). Un POKE a un sprite
   que llega después se ve a partir de la línea siguiente. Antes se veía en el mismo píxel.
3. **Sprites 32 y mayores.** `POKE 2100,n` con `n >= 32` ya no hace nada: sus POKEs 2101-2128 no
   modifican ningún sprite ni se copian a la sombra. Antes no tocaban ningún slot, pero sí
   caían en la copia de la sombra del sprite `n mod 32`.
4. **Los campos que no se escriben tras un reset.** Antes los slots conservaban su X, Y y
   gráficos de antes del reset (solo se borraba el activo). Ahora esos campos son los que haya
   en la sombra, que tras un reset de la máquina son los de la ROM, salvo que la ROM los limpie
   (la ROM nueva los pone a cero). Un sprite se activa siempre con su POKE de activo, así que
   un programa que escribe todos sus campos no nota nada.

## Cómo implementarlo en el emulador

```
al empezar el sincronismo de cada línea visible (0..191):
    slots = []
    para s de 31 a 0:
        si activo[s] y 0 <= (linea + 32) - Y[s] (mod 512) < 8:
            fila = ((linea + 32) - Y[s]) & 7
            slots.añadir( X[s] (9 bits), color[s][fila], pixel[s][fila], mascara[s][fila] )
            si len(slots) == 8: salir
en cada píxel:
    para cada slot, en orden:     # el primero es el de mayor índice
        d = (pos_x - X) mod 512
        si d < 8 y mascara[7 - (d & 7)]:
            devolver (pixel[7 - (d & 7)], color)
```

El límite de tiempo del hardware (el motor lee la sombra entre el sincronismo horizontal y el
píxel 120, 77 relojes de píxel en el caso peor) no hace falta emularlo: solo se agota con los
32 sprites activos y los 8 que cortan la línea siendo los de menor índice.

En los modos de 70 y 80 columnas, `pos_x` se calcula igual que antes con `pixel_cnt` de hasta
827, así que el eco de los sprites a +512 píxeles sigue ahí y usa los mismos datos de esa línea.
