# SPRDBUF -- prueba de sprites por línea con el doble buffer

Prueba sintética para el FPGA con **sprites por línea** (los sprites se leen de
la RAM de sombra, `sprite_engine.v`, máximo 12 por línea; en la rama de 64
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

Casi todos los sprites usan el **formato B, de 4 bits por píxel con hoja de
gráficos**: el programa escribe 16 celdas de 32 bytes (una por color) en `$7C00`
(bloque 3 de 8 KB, `POKE 2129,3`) y cada sprite apunta a la suya con su página
(`POKE 2130`, página = (dirección − `$6000`) / 32; la página 0 es el formato A)
y deja su máscara (`POKE 2121-2128`) a `$FF`. El centro negro es el índice 8 de
`pal16` reprogramado a negro, así que el programa activa la paleta de 16 colores
(`POKE 2067,1`) y al salir la apaga. El color que no se dibuja es el 0, el que
hay por defecto en el nibble alto de `POKE 2103`. Al salir, el programa deja
todos los sprites en página 0: es lo que esperan los programas de formato A.

| Zona | Sprites | Qué debe verse |
|------|---------|----------------|
| Fila de arriba (Y=16) | 0-15, los 16 en la misma línea | **12 cuadrados**: los 4 de la izquierda (aros azul, rojo, magenta y verde, sprites 0-3) NO aparecen. Se ven del 4 al 15 |
| Segunda fila (Y=56) | 16-21 | Los **6** cuadrados, enteros |
| Tercera fila, izquierda (Y=96) y derecha (Y=100) | 36-42 y 43-49 | En las 4 líneas donde coinciden (14 sprites) faltan los dos más a la izquierda: los dos primeros cuadrados de la izquierda salen **cortados por la mitad** (solo sus 4 filas de arriba). Los otros 12 salen enteros |
| Un cuadrado que recorre la pantalla en horizontal (sprite 60, Y=150) y otro en vertical (sprite 61, X=230) | 60 y 61 | Pasan por encima de la pelota y del fondo sin parpadear ni partirse |
| Fila de abajo (Y=170), a caballo de las dos tablas (solo con la FPGA de 64 sprites) | 22-35 | Se ven **12**: los sprites 24 a 35. El 22 y el 23 (aros azul y rojo) no aparecen |
| Última fila (Y=185), solo con la FPGA de 64 sprites | 62 y 63 | Dos cuadrados, de aros verde y cian |
| Fila de sprites extra (Y=116) | extra 0-2 (`POKE 2131,3`), de izquierda a derecha | **extra 0**, formato A: igual que el 50. **extra 1**, formato B: aro cian y centro negro. **extra 2**, la misma celda con el color 8 transparente: solo el aro cian. Sus entradas son las páginas 0-2 de la hoja (`$6000`), escritas como RAM, sin `POKE 2100` |
| Fila de formatos (Y=130) | 50-54, de izquierda a derecha | **50**, formato A: aro con un color distinto en cada fila y centro negro. **51**, formato B: aro rojo y centro negro. **52**, la misma celda con máscara `$F0`: solo la mitad izquierda. **53**, la misma celda con el color 8 transparente: solo el aro rojo, con el fondo en el centro. **54**, celda del color 0 con el color 15 transparente: cuadrado negro de 8×8 entero (con el 0 transparente se vería de 6×6) |

## Lo que comprueba

- **Límite de 12 por línea**, y que lo que se pierde son los sprites de índice
  más bajo, y solo en las líneas donde coinciden más de 8.
- **Con el doble buffer ON y OFF los sprites se ven exactamente igual.** Es lo
  que prueba que el motor de sprites y la copia del doble buffer (puerto B de
  la BRAM) no se pisan. Alterna con SPACE: lo único que debe cambiar es la
  pelota. Con el doble buffer OFF (borde rojo) desaparece mientras está en la
  parte de arriba de la pantalla (unas 65 primeras líneas) y se ve en el resto:
  el programa la borra nada más empezar el VSYNC y la vuelve a dibujar ~7 ms
  después, y como va sincronizado con la imagen, el haz pasa por esa zona
  siempre con la pelota borrada. Por eso se ve "durante unos botes sí y otros
  no". Es lo que demuestra la prueba (igual que `EXAMPLES/DBUF`), no un fallo.
- Los sprites se mueven nada más empezar el VSYNC, sin cortes, y el de la
  derecha cruza el límite de 256 píxeles (X de 9 bits).
- Que el vídeo de fondo (bitmap) no se altera con los sprites activos.
- **Los sprites extra** (`POKE 2131`): se leen de la hoja, en los dos formatos.
- **Los dos formatos en la misma línea**, y que en el B la máscara y el color
  transparente son de cada sprite aunque la celda sea compartida.

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
