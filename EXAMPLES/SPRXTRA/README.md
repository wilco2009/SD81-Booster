# SPRXTRA -- sprites de 4 bits por píxel y más de 64 sprites

Prueba las dos funciones nuevas del motor de sprites de la FPGA:

- **Formato B** (`POKE 2130` = página): sprites de 8×8 a 4 bits por píxel. El
  gráfico es una celda de 32 bytes de la hoja (bloque 3, `POKE 2129,3`) y se
  anima cambiando la página.
- **Sprites extra** (`POKE 2131` = M): hasta 127 sprites más, de los dos
  formatos. No usan `POKE 2100`: el extra j son los 32 bytes de la página j de
  la hoja (aquí, `$6000 + 32*j`), con los mismos campos que la tabla, y el
  programa los escribe como RAM normal.

Modo **160x100 a 256 colores** (`POKE 2045,175`). Lo primero que hace es poner el
modo y cargar de fondo **`FONDO.JPG`** del directorio actual (con el comando 76 del
MCU, recortada para llenar la pantalla; tarda unos segundos). Si no está, o el MCU
no la puede convertir, sigue con el fondo negro. Vale cualquier JPG o BMP que
abra el visor de imágenes (`EXAMPLES/IMGVIEW`).

En este modo los sprites van en píxeles de la imagen: cada sprite son 8×8 de los
160×100, con coordenadas píxel + 32 (X 32-191, Y 32-131), y sus colores salen de
la paleta de 16 (`pal16`), no de la del fondo. El motor tiene el tiempo de las 80
columnas. Todos los sprites son bolas (máscara redonda):

- **Formato B**: arco iris en diagonal con los colores del 1 al 15, que se mueve
  (4 páginas).
- **Formato A**: rayas horizontales, un color por fila. No se animan.

## Teclas

| Tecla | Acción |
|-------|--------|
| E     | Número de extra: 64 (al empezar, borde **verde**) → 0 (**rojo**) → 32 (**azul**) → 48 (**magenta**) → 64 |
| O P Q A | Mover el fondo a la izquierda, a la derecha, arriba y abajo, un píxel por imagen (mantener pulsada). Scroll infinito: lo que sale por un lado entra por el contrario |
| S     | Parar / mover los sprites |
| X     | Salir a BASIC (apaga los sprites, deja la página a 0, `POKE 2131,0` y el fondo sin scroll) |

El fondo se mueve con el scroll en anillo del modo (`POKE 2072` y `2074`, con el
anillo del tamaño de la imagen: 100 filas y 160 píxeles). La FPGA solo cambia
desde dónde lee la imagen, sin copiar nada; los sprites no se mueven con él.

## Qué se tiene que ver

Doce franjas de 8 filas, desde la fila 2 de la imagen:

| Franja (filas) | Sprites | Qué debe verse |
|---|---|---|
| 0-4 (2-41) | normales 0-59, 12 por franja | Bolas B con arco iris animado, moviéndose a la derecha. Siempre todas, con cualquier número de extra |
| 5 (42-49) | normales 60-63 y extra 0-7 | Las 4 normales siempre; los 8 extra (B y A alternos, hacia la izquierda) con 32 o más |
| 6-10 (50-89) | extra 8-48: 11, 9, 8, 7 y 6 por franja | Bolas B y A alternas, hacia la izquierda. Cuanto más alto el número del extra, menos caben por línea, y por eso las franjas tienen cada vez menos. Con 32 extra se ven hasta el 31 (la franja 8, a medias); con 48, hasta el 47; con 64, todas |
| 11 (90-97) | extra 49-63, 15 bolas B quietas | Solo con 64 extra. Se ven **6** (las de la izquierda, extra 49-54): en esas líneas el motor tiene que mirar los 64 normales y los 64 extra antes de cargar, y no le da tiempo a más. Las otras 9 no aparecen, sin parpadeo |

Nada debe parpadear ni partirse. Los sprites normales se mueven por la ventana
de POKEs y los extra escribiendo en su entrada; los dos van igual de suaves.

## Comprobado en el PC

- El programa, con el Z80 de EightyOne, un modelo de los POKEs de sprites, de la
  paleta y del modo, y un MCU simulado: pone el modo 175 antes de pedir el
  fondo, lo lee entero (paleta e imagen) y lo deja en `$8000`. Sin `FONDO.JPG`,
  con uno que el MCU no convierte o con una cabecera que no es `I160`, sigue con
  la imagen a 0 y el color 0 en negro, sin dejar el fichero abierto. Deja las
  tablas, las entradas de los extra y `POKE 2131` como se describe arriba, y al
  salir deja los sprites apagados, en página 0 y sin extra.
- Esas mismas tablas, en el motor de la FPGA simulado con la temporización de
  80 columnas: carga en cada franja exactamente lo de la tabla de arriba con 0,
  32, 48 y 64 extra. Las franjas que se mueven tienen un margen de unos 9 pasos
  de los 171 de la línea; solo la de abajo está al límite, a propósito.

## Ensamblar y ejecutar

```
pasmo sprxtra.asm sprxtra.bin sprxtra.sym
```

Copia `SPRXTRA.BIN`, `SPRXTRA.B81` y, si quieres fondo, un `FONDO.JPG` a la misma
carpeta de la SD y carga el `.B81`
(`LOAD "SPRXTRA.B81"`), o a mano:

```basic
10 FAST
20 LOAD THEN CLEAR 29999
30 LOAD FAST "SPRXTRA.BIN" CODE 30000
40 RAND USR 30000
50 SLOW
```

Necesita la FPGA y la ROM con los sprites de dos formatos y los extra, y el
firmware con el comando 76 (sin él no hay fondo). Las entradas de los extra
ocupan `$6000-$67FF`, la paleta del fondo `$6800-$69FF` y la hoja `$7C00-$7C7F`,
por debajo de `$8000`; la imagen, `$8000-$BE7F`.
