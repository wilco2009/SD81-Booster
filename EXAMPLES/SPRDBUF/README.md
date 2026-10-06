# SPRDBUF -- prueba de sprites por línea con el doble buffer

Prueba sintética para el FPGA con **sprites por línea** (los sprites se leen de
la RAM de sombra, `sprite_engine.v`, máximo 8 por línea; en la rama de 64
sprites hay dos tablas en la sombra) y el **doble buffer**
(`POKE 2057`, que copia 8 KB por el mismo puerto de la BRAM en el blanking).
Ningún programa usaba las dos cosas a la vez. Modo Superfast HiRes Spectrum,
con la pelota de `EXAMPLES/DBUF` rebotando de fondo.

## Teclas

| Tecla | Acción |
|-------|--------|
| SPACE | Conmutar el doble buffer: borde **verde** = ON, **rojo** = OFF |
| M     | Congelar / reanudar la pelota |
| Q     | Salir a BASIC (apaga los sprites) |

## Qué se tiene que ver

Cada sprite es un cuadrado de 8×8 con un aro de color y el centro negro; el
color del aro dice qué sprite es.

| Zona | Sprites | Qué debe verse |
|------|---------|----------------|
| Fila de arriba (Y=16) | 0-11, los 12 en la misma línea | **8 cuadrados**: los 4 de la izquierda (aros azul, rojo, magenta y verde, sprites 0-3) NO aparecen. Se ven del 4 al 11 |
| Segunda fila (Y=56) | 12-17 | Los **6** cuadrados, enteros |
| Tercera fila, izquierda (Y=96) y derecha (Y=100) | 18-22 y 23-27 | En las 4 líneas donde coinciden (10 sprites) faltan los dos más a la izquierda: los dos primeros cuadrados de la izquierda salen **cortados por la mitad** (solo sus 4 filas de arriba). Los otros 8 salen enteros |
| Un cuadrado que recorre la pantalla en horizontal (sprite 28, Y=150) y otro en vertical (sprite 29, X=230) | 28 y 29 | Pasan por encima de la pelota y del fondo sin parpadear ni partirse |
| Fila de abajo (Y=170), a caballo de las dos tablas (solo con la FPGA de 64 sprites) | 30-39 | Se ven **8**: los sprites 32 a 39. El 30 y el 31 (aros azul y rojo) no aparecen |
| Última fila (Y=185), solo con la FPGA de 64 sprites | 62 y 63 | Dos cuadrados, de aros verde y cian |

## Lo que comprueba

- **Límite de 8 por línea**, y que lo que se pierde son los sprites de índice
  más bajo, y solo en las líneas donde coinciden más de 8.
- **Con el doble buffer ON y OFF los sprites se ven exactamente igual.** Es lo
  que prueba que el motor de sprites y la copia del doble buffer (puerto B de
  la BRAM) no se pisan. Alterna con SPACE: lo único que debe cambiar es que la
  pelota parpadea con el doble buffer OFF.
- Los sprites se mueven nada más empezar el VSYNC, sin cortes, y el de la
  derecha cruza el límite de 256 píxeles (X de 9 bits).
- Que el vídeo de fondo (bitmap) no se altera con los sprites activos.

Si algo falla: una columna de sprites desplazada o que cambia de sitio al
conmutar el doble buffer apunta a un problema de arbitraje del puerto B; un
sprite que debería verse y falta, a un problema de tiempo del motor (lee
entre el sincronismo horizontal y el píxel 120).

## Ensamblar y ejecutar

```
pasmo sprdbuf.asm sprdbuf.bin sprdbuf.sym
```

Copia `SPRDBUF.BIN` y `SPRDBUF.B81` a la SD y carga el `.B81`
(`LOAD "SPRDBUF.B81"`, ver el cargador de listados), o a mano:

```basic
10 FAST
20 LOAD THEN CLEAR 29999
30 LOAD FAST "SPRDBUF.BIN" CODE 30000
40 RAND USR 30000
50 SLOW
```
