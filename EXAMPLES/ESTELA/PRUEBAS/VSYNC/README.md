# VSYNC — fase 0 (b) de ESTELA

Mide los **T-estados entre el flanco del VSYNC y la primera línea de la imagen**
(la 58) en el modo 160x100 a 256 colores (`POKE 2045,175`). Es el tiempo que
tendrá el juego, en cada imagen, para mover el scroll, los sprites y la paleta
sin que se vea a medias.

## Cómo funciona

En cada imagen, nada más ver el flanco de subida del VSYNC (puerto `$AF`, bit 0)
el programa pone el **color 0 de la paleta en negro**, y **D T-estados después** lo
pone en **blanco**. Toda la imagen es del color 0 (menos la regla y las cifras), así
que se ve **negra por encima del corte y blanca por debajo**:

- con D pequeño, el corte cae antes de que empiece la imagen: **toda blanca**;
- al subir D, la imagen se vuelve negra **desde arriba**;
- cada fila de la imagen son 2 líneas de TV, unos 414 T.

`D = 116 + 26·n` T-estados, contados desde el final del `IN` que ve el bit a 1
hasta el final de la escritura que pone el blanco. La espera del VSYNC tiene un
margen de 27 T (una vuelta del bucle), así que la línea del corte puede temblar
un poco o cortarse a media línea: es normal.

## Pantalla

- **Regla verde** a los dos lados: raya corta cada 10 filas de la imagen, larga en
  las filas 0 y 50 (y una corta en la 99, la última).
- **Borde azul**: se ve dónde acaba la imagen.
- En rojo, tres números (en la primera versión salían en azul: el rojo estaba mal
  escrito en la paleta):
  - **D**: el retardo actual, en T-estados (empieza en 10.516).
  - **F**: T-estados de una imagen (de VSYNC a VSYNC), medidos al empezar.
  - **W**: T-estados que el VSYNC está a 1, medidos al empezar.

## Teclas

| Tecla | Acción |
|-------|--------|
| Q / A | D + 26 / − 26 T (mantener) |
| P / O | D + 208 / − 208 T, casi una línea de TV (mantener) |
| M     | Volver a medir F y W |
| X     | Salir a BASIC |

D va de 142 a 59.916 T. Con D muy grande, al cambiarlo se salta alguna imagen
(escribir el número tarda) y la imagen parpadea mientras se pulsa: es normal.

**Solo vale en la placa.** EightyOne dibuja el modo 160x100 de una vez por imagen
con la paleta del final, así que en el emulador la imagen sale toda blanca con
cualquier D y no hay corte que buscar (sí mide F y W: en el emulador, F = 64.145 y
W = 1.650, con 310 líneas por imagen).

## Cómo se prueba

Copia `VSYNC.BIN` y `VSYNC.B81` a una carpeta de la SD y carga `VSYNC.B81`.

1. Si la imagen sale toda negra o con el corte por en medio, baja D con **O** hasta
   que esté toda blanca. Luego súbelo con **P** hasta que la primera línea de arriba
   se vuelva negra, y afina con **Q** y **A**: el valor buscado es el **D más pequeño
   con el que la primera línea de la imagen ya sale negra**. Llamémoslo **D0**.
2. Comprobación: sube D con **P** y **Q** hasta D0 + 4.140 (míralo en el número D):
   el corte tiene que quedar en la **fila 10** de la regla (la primera raya corta).
   Con D0 + 20.700, en la **fila 50** (la raya larga del medio).
3. Apunta F y W.

**Qué hay que mandar:** D0, F y W, y si el corte cae en las filas 10 y 50 donde dice
el punto 2 (si no, en qué fila cae).

Previsión del plan (`claude/planning/juego160.md`): unas 58 líneas, ~12.000 T.
F debería estar cerca de 64.600 (312 líneas de ~207 T).

## Comprobado en el PC

`claude/mqtttest/build_estela0.sh`: con el Z80 de EightyOne y un VSYNC simulado,
en más de 1.000 imágenes el blanco se escribe exactamente `116 + 26·n` T después
del final del `IN` del flanco, el negro en los 60 T siguientes al flanco, las
teclas respetan los topes, el D que se escribe en pantalla es el de n, y F y W
salen con menos de 40 T de error. Lo que no puede saber el PC es en qué línea de
la imagen cae cada T: eso es lo que mide la placa. **Sin probar en la placa.**
