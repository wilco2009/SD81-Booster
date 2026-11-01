# MODE160 -- prueba del modo de vídeo 160x100 a 8 bpp (paleta de 256 colores y PDM)

Programa de prueba para la **FPGA con el modo de 256 colores** (rama `fpga-mode160`, ver `claude/planning/pdm_color.md`
y `FPGA/SD81V2.1000/mode160.v`). **Solo funciona en la placa con esa FPGA**: el emulador no tiene el modo.

El modo (`POKE 2045,175`) es de 160x100 píxeles, un byte por píxel (índice de una paleta de 256 entradas), 16.000 bytes en
`$8000-$BE7F` (HFILE = `$8000`). Cada píxel dura 3 ciclos de 13 MHz (unos 230 ns) y cada fila se repite en dos líneas de TV.
Los colores salen por **PDM** (densidad de pulsos a 208 MHz) por los pines R, G, B y BRIGHT, de forma que la red de resistencias de la SCART
y el televisor los convierten en niveles intermedios: la paleta son 4 densidades de 0 a 15 por entrada, no 4 bits de color.

## Qué se ve

16 filas de 16 bloques de 10x4 píxeles (los índices 0-255: **toda la paleta**, por filas) y, debajo, cuatro barras de 16 pasos
(grises, rojo, verde, azul). Las filas de la paleta (`pal160.py`):

| Fila | Contenido |
|---|---|
| 0 | 16 grises (0 a 0,717 V, la tensión de la SCART con todo a 1) |
| 1-3 | rojo, verde, azul puros, de 0 a 0,455 V |
| 4-6 | amarillo, cian, magenta |
| 7 | los 16 colores de Chroma81, con exactamente los mismos niveles que el camino normal |
| 8-15 | rueda de 16 tonos, con la saturación creciente de arriba abajo (las últimas filas quedan fuera de lo que la red puede dar, y se acercan) |

## Teclas

`P` / `O` mueven la imagen a derecha / izquierda (`POKE 2099`, 0 a 15, un paso = 1/3 de píxel; por defecto 8). `X` sale a BASIC.

## Qué mirar en la placa

1. **Que sale la imagen**, centrada y con el borde negro. Si está desplazada, `P` y `O`; si hace falta más, hay que mover
   `SCR_START_X_80 + 36` en `SD81.v`. Verticalmente va de la línea 58 a la 257 (centrada en la ventana de 192 líneas).
2. **Los grises**: 16 pasos limpios, sin bandas ni dientes de sierra (el rizado de un PDM mal filtrado se ve como una
   textura fina en los pasos intermedios, sobre todo en los grises y en los niveles bajos).
3. **Los 16 colores de Chroma81** (fila 7) iguales que en el modo normal con un programa de color.
4. **Los bordes de los bloques**: si un bloque deja una franja del color del anterior, el PDM está emborronando píxeles
   (el filtro del televisor es demasiado lento) o la ventana va corrida.

Si el rizado se nota: un condensador de 330-470 pF a masa en cada canal (en paralelo con las resistencias de 130 ohm
R19, R27 y R28) lo filtra, con una frecuencia de corte de unos 10-13 MHz y sin emborronar mucho un píxel de 230 ns.

## Compilar

```
compila.bat                    (python pal160.py y pasmo; deja mode160.bin)
```

Copia `MODE160.BIN` y `MODE160.B81` a la SD (por ejemplo en `TOOLS\MODE160`) y carga `MODE160.B81`.

## Pruebas en el PC

- `claude/mqtttest/build_mode160.sh`: el Z80 de EightyOne ejecuta el programa con la paleta, el teclado y el VSYNC simulados y comprueba las 256
  entradas por el protocolo `2063/2064/2065`, la imagen byte a byte, el orden (el modo se activa después), `P`/`O` y la salida.
- `FPGA/SD81V2.1000/tb_mode160.v`: la FPGA (`mode160.v`) con una BRAM de latencia 1 y un reloj de 208 MHz sin relación de fase:
  paleta por defecto y cargada, geometría (32.000 píxeles por cuadro), densidad de pulsos de los cuatro pines, borde y borrado.

## Variante 160x200 a 4 bpp (rama `fpga-mode160-4bpp`)

`mode1604.asm` / `MODE1604.B81` prueban `POKE 2045,176`: 160x200 con **4 bits por píxel** (80 bytes por fila, 16.000 bytes en
`$8000-$BE7F`, nibble alto = píxel de la izquierda), una fila por línea de TV (58 a 257). El índice (0-15) entra en la **paleta de 16**
(`POKE 2063/2064/2066`, por defecto los colores digitales de Chroma81). El modo 175 (8 bpp) no cambia. El programa dibuja 16 barras
(arriba) y diagonales `(x+y) mod 16` (abajo); `X` sale. Para una imagen propia: `python png2i4.py img.png` (PNG de 160x200 con
hasta 16 colores) deja `pic4.bin` (píxeles) y `pal4.bin` (paleta lista para 2064/2066). Solo en la placa con esa FPGA; sin probar.

### Sprites y plano de texto en el modo 176 (160x200 a 4 bpp)

- **Sprites**: los 64 sprites 8x8 del motor de siempre (`POKE 2101`, tablas en la sombra) se dibujan sobre la imagen; coordenadas de sprite = pixel
  de imagen + 32 en X e Y (X 0-159, Y 0-199, es decir sprite X 32-191 / Y 32-231). Pixel de sprite = tinta o papel de su fila, por `pal16`.
- **Texto 40x25**: `POKE 2068,papel*16+tinta` (índices de `pal16`, por defecto tinta 15 y papel 0) y `POKE 2069,bloque*2+activo` (bloque de 8 KB
  que contiene el texto; el bit 0 lo activa). En ese bloque: **códigos** en `+0` (1.000 bytes, fila*40+columna), **fuente** en `+$0300 + código*8 + fila`
  (códigos 32-255 = 224 glifos propios, 6 píxeles por fila en los bits 7..2). Código `< 32` = celda transparente (se ve la imagen); `>= 32` = celda opaca con tinta y papel;
  bit 7 del código = invierte el glifo de ese código (que tiene su propia definición). El texto va por encima de la imagen y de los sprites. Cada celda mide 4x8 píxeles de imagen.
- Todo simulado en ModelSim (`tb_mode160_4bpp/_spr/_txt`), **sin probar en placa**.
