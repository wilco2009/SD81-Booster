# PHOTO -- visor de fotos en el modo 160x100 a 8 bpp (256 colores a la vez)

Visor para la **FPGA con el modo de 256 colores** (rama `fpga-mode160`, ver `claude/planning/pdm_color.md`). **Solo funciona en la placa con esa FPGA**: el emulador no tiene el modo.
Lee `IMG001.I16`, `IMG002.I16`... de la carpeta actual y los muestra a 160x100, con una paleta propia de 256 colores por foto.
Si falta el `IMGnnn.I16`, prueba **`IMGnnn.BMP`**: el MCU lo convierte al abrirlo (ver "BMP directo" más abajo), sin tener que convertir nada en el PC.

## Pasos

1. Prepara cada foto como **PNG de 160x100 con hasta 256 colores** (en tu editor: reducir a paleta indexada de 256 colores; también valen RGB / gris con 256 colores distintos o menos).
2. Conviértelas (el orden de la línea de comandos es el orden en el visor):

   ```
   python png2i16.py foto1.png foto2.png foto3.png            (escribe IMG001.I16, IMG002.I16, IMG003.I16 en la carpeta actual)
   python png2i16.py -o D:\FOTOS --start 10 otra.png          (IMG010.I16 en D:\FOTOS)
   ```
3. `compila.bat` ensambla el visor (`photo.bin`); con PNG como parámetros (`compila.bat foto1.png foto2.png`) también los convierte.
4. Copia **`PHOTO.BIN`, `PHOTO.B81` y las `IMGnnn.I16`** a la misma carpeta de la SD y carga `PHOTO.B81`.

Teclas: **N** siguiente, **B** anterior, **F** cambia cómo se coloca un BMP y lo recarga, **ESPACIO** sale a BASIC. Los números que faltan se saltan (y también un fichero que no sea `.I16`), y al llegar al final se da la vuelta.
Si no hay ninguna `IMGnnn.I16`, el programa vuelve a BASIC y escribe `NO HAY IMG001.I16`.
Entre una foto y otra la pantalla pasa por negro (menos de un segundo): se pone la paleta a negro, se lee la imagen sobre `$8000` y entonces se carga la paleta nueva, así nunca se ve una foto a medias ni con la paleta de la anterior.

## BMP directo (comando 76 del MCU)

Copia un `IMG006.BMP` (o el número que sea) junto a `PHOTO.BIN` y `PHOTO.B81`, sin convertirlo, y el visor lo muestra como cualquier otra imagen.
El STM32 lo convierte al abrirlo (`Arduino/SD81BoosterV2_039_STM32/BMP.cpp`): escala a 160x100 con la media por área y reduce a una paleta de 256
colores propia de la imagen (cortes de mediana). **Tarda unos segundos** según el tamaño del BMP y la SD (se lee dos veces); la pantalla se queda como
estaba hasta que termina. Si falta el número como `.I16` y el `.BMP` no se puede convertir, el visor lo salta como un hueco; el motivo queda en
`/MAN/BMPERR.TXT` (`LOAD THEN PRINT "*BMPERR"`).

- **BMP admitidos:** sin compresión, de 1, 4, 8, 24 y 32 bits, de abajo arriba o de arriba abajo (cabeceras CORE, INFO, V4 y V5), hasta 16.384 x 16.384 y 16 millones de píxeles.
  No: RLE, 16 bits ni máscaras raras (el programa del PC o de otro editor puede guardarlo como "24 bits" sin compresión).
- **F**: la pantalla es 4:3 y los 160x100 también, pero un BMP puede tener otra proporción. Cada pulsación pasa a la siguiente colocación y recarga la imagen:
  **estirado** (llena la pantalla, distorsiona si no es 4:3; por omisión), **con su proporción** (bandas negras) y **recortado** (llena la pantalla sin distorsionar).
  Un `.I16` no cambia.
- **Memoria del MCU:** unos 23 KB mientras la imagen está abierta (el visor la cierra al cargarla).
- `bmp2i16.py` hace lo mismo en el PC (el mismo algoritmo, byte a byte: es la referencia con la que se prueba el código del STM32):

  ```
  python bmp2i16.py foto1.bmp foto2.bmp             (IMG001.I16, IMG002.I16...)
  python bmp2i16.py -o D:\FOTOS --start 10 --fit fit otra.bmp
  ```

## Formato `.I16` (16.516 bytes)

| Desplazamiento | Tamaño | Contenido |
|---|---|---|
| 0 | 4 | `I160` |
| 4 | 512 | paleta: por entrada, byte bajo `{G[3:0],B[3:0]}` y byte alto `{BRIGHT[3:0],R[3:0]}` (los valores de `POKE 2064` / `2065`, ver `mode160.v`) |
| 516 | 16000 | píxeles, fila a fila (100 filas de 160), un byte por píxel = índice de la paleta |

## Colores

`png2i16.py` pasa cada color RGB de 8 bits a las cuatro densidades de pulsos (R, G, B y BRIGHT, de 0 a 15) con el modelo de la red analógica de la SCART de `../MODE160/pal160.py`
(tensión = 0,455 V por la densidad del canal + 0,261 V por la de BRIGHT, sin gamma: el valor del PNG es la tensión). Lo que la red no puede dar (el canal más alto menos el más bajo no pasa
de 0,455 V, sobre un máximo de 0,717 V) se aproxima al color más parecido, y el programa escribe **la desviación máxima** de cada foto. Los colores muy saturados salen más lavados,
y los grises y los tonos pastel salen casi exactos. `--gamma 1.4` (o 0,8) aclara u oscurece los medios tonos si la foto se ve fuera de tono en tu televisor.

Solo hace falta Python 3 con la biblioteca estándar. Si tienes Pillow (`pip install Pillow`) también acepta PNG entrelazados, con más de 256 colores (los reduce a 256 sin tramado)
y de otro tamaño con `--resize`.

## Pruebas en el PC

`claude/mqtttest/build_bmp.sh` compara el conversor del STM32 (`BMP.cpp`) con `bmp2i16.py`, byte a byte, sobre BMP de todos los tipos (más de 300 comprobaciones).
`claude/mqtttest/build_photo.sh` genera cuatro PNG de prueba (indexado de 8 bits con 256 colores, RGB de 40 colores, indexado de 4 bits y gris, con los cinco filtros de PNG),
los convierte, comprueba los `.I16` contra un cálculo independiente y ejecuta el visor en el Z80 de EightyOne con un MCU simulado: N / B / ESPACIO, un hueco de numeración, un fichero que no es
`.I16`, la vuelta al principio, que la paleta de cada carga sea la del fichero con la imagen ya completa, y la carpeta vacía. **No prueba la FPGA ni la placa.**
