# Especificación para el emulador: modo 160x100 a 256 colores, paleta de 16 colores y modo 160x200 a 4 bpp

Resumen (28/10/2026) de lo que hay que implementar en el emulador para igualar a la FPGA de la rama `fpga-mode160`. Diseño y razones: `pdm_color.md`.
Referencias en el repositorio: `FPGA/SD81V2.1000/mode160.v` (el modo y las dos paletas), `SD81.v` (los POKEs y el multiplexor de salida),
`FPGA/SD81V2.1000/tb_mode160.v` (el banco de pruebas), `EXAMPLES/MODE160` (programa de prueba), `EXAMPLES/PHOTO` (visor de fotos), `EXAMPLES/COLORTEST` (paletas de 16).

**Estado:** el modo 160x100 y su paleta de 256 están **probados en la placa** (27/10/2026, funciona). La paleta de 16 colores está sintetizada y probada en
simulación y con el Z80 en el PC, **sin probar en la placa**. La versión de la FPGA (`FPGAVERSION.TXT`) no se ha subido todavía.

**Estado del §5 (modo 160x200 a 4 bpp) en el emulador (1/11/2026): IMPLEMENTADO, sin probar todavia** con `MODE160W` y `mode1604`: `RenderMode160x4` en `SD81Booster.cpp` (imagen por `pal16` con ventana, borde por `pal16` en las filas fuera de ella y digital fuera de la rejilla, sprites 8x8 con K=12 y X/Y+32, texto 40x25 con glifos de `ROMTABLE[15:11]`), POKEs 2068-2071 (bloqueados por ROMLOCK) y 2045,176; instantáneas: línea `MODE4 <4bpp> <win_first> <win_last> <txt_en> <txt_col>` (más `WIDE_COLS A0`). Los sprites ignoran el color de Chroma (usan siempre los bytes de color de la fila).

**Estado en el emulador (28/10/2026): IMPLEMENTADO, sin probar todavia** con `MODE160`, `PHOTO` y `COLORTEST`. Modo y POKEs en `SD81Booster.cpp` (`MapperWrite`, `HandleFpgaRegisters`, `RenderMode160`, `SyncPal16`); el modo se dibuja en el buffer ancho de 80 columnas, donde un ciclo de 13 MHz es un pixel (un pixel del modo = 3), empezando a 32 + `POKE 2099` del texto de 80 columnas y desde la linea 52 (= 58 de la FPGA, con el texto en 56 en vez de 62). La paleta de 16 se aplica a `Colours[]` desde `RecalcPalette()` (`accdraw.cpp`), solo con el color de Chroma encendido y fuera del modo 160x100; las entradas que nadie ha escrito dejan el color del emulador. Instantaneas: lineas `PAL160`, `PAL256` y `PAL16` (solo si se han tocado las paletas o esta el modo) y `WIDE_COLS A0` para el modo.

## 1. POKEs nuevos

| POKE | Función |
|---|---|
| `2045,175` | modo 160x100 a 8 bpp (activa también la temporización de 80 columnas, `sf80`); cualquier otro valor de 2045 (170, 171, 172, 173, 174, 85) lo apaga |
| `2043`, `2044` | HFILE como siempre: la imagen empieza en `{HFILE[15:13], 13'b0}` (usar bloques 0-6, el 7 se sale de los 64 KB) |
| `2063,n` | puntero de la paleta (8 bits; da la vuelta en 255). Compartido por las dos paletas |
| `2064,lo` | byte bajo `{G[3:0], B[3:0]}` (se guarda; compartido por las dos paletas) |
| `2065,hi` | byte alto `{BRIGHT[3:0], R[3:0]}`: escribe la entrada **de 256** `pal[puntero] = {hi, lo}` y el puntero avanza |
| `2066,hi` | igual pero escribe la entrada **de 16** `pal16[puntero & 15] = {hi, lo}` y el puntero avanza (8 bits completos) |
| `2067,v` | bit 0: paleta de 16 activada (1) o desactivada (0). El reset la desactiva |
| `2099,n` | posición horizontal del modo 160x100, n de 0 a 15 (por omisión 8), en ciclos de 13 MHz |
| puerto `$7FEF` | el borde sale de sus 4 bits `{bright, G, R, B}` (como en Chroma); en este modo se ignora `color_enable` |

Todos se capturan como los demás POKEs de configuración (en T3 de la escritura, si no está bloqueado el bloque de control).

## 2. Modo 160x100 a 256 colores (`POKE 2045,175`)

- **Memoria:** 160 x 100 bytes, un byte por píxel = índice de la paleta, filas seguidas desde `{HFILE[15:13], 13'b0}` (dirección = base + fila*160 + columna;
  16.000 bytes: dos bloques de 8 KB). Se lee de la sombra directamente (no del bloque de delante del doble buffer).
- **Geometría:** cada fila sale en **dos líneas de TV** (200 líneas, de la **58 a la 257**) y cada píxel dura **3 ciclos de 13 MHz** (231 ns, 1,5 píxeles normales): 160 píxeles = 480 de los 828
  ciclos de la línea. La imagen empieza en `pixel_cnt = SCR_START_X_80 + 36 + n` (n = `POKE 2099`, 8 por omisión; `SCR_START_X_80` = 202 + 8 por el valor de `POKE 2094`), la misma
  referencia que el texto de 70/80 columnas. En un emulador que dibuja por cuadro basta con dibujarla centrada: 240 píxeles normales de ancho por 200 líneas.
- **Fuera de la imagen:** el borde (`$7FEF`, 4 bits, lo mismo que Chroma) y en el borrado todo a 0. No hay atributos ni texto.
- **No hay** sprites, scroll fino, motor de scroll, blit ni doble buffer en este modo (la línea 254 es donde empezaría el blit: no se usa).
- **Paleta de 256 por omisión** (hasta que se escribe): RGB 3-3-2 sin brillo, con rojo y verde de 0 a 7 y azul de 0 a 3; las densidades son
  `{0, 2, 4, 6, 9, 11, 13, 15}` para los canales de 3 bits y `{0, 5, 10, 15}` para el azul, con BRIGHT a 0.

### Del color de la paleta al RGB de la pantalla

Cada entrada son cuatro densidades de pulsos de 0 a 15 (`R`, `G`, `B` y `BRIGHT`). Los pines dan un tren de pulsos de densidad exacta `n/15` y la red de resistencias de la SCART
(`pdm_color.md`) lo convierte en una tensión por canal:

```
V_canal = 0,455 * (d_canal / 15) + 0,261 * (BRIGHT / 15)       (de 0 a 0,717 V)
nivel de 0 a 255 = 255 * V_canal / 0,717 = 255 * (0,6351 * d_canal / 15 + 0,3649 * BRIGHT / 15)
```

El emulador puede dibujar ese nivel directamente (el televisor y el cable hacen de filtro del PDM; no hace falta modelar el rizado). Con `BRIGHT = 0` y `d = 15` un canal vale 0,635 (162 de 255),
y con `BRIGHT = 15` y `d = 0` vale 0,365 (93 de 255): por eso un primario brillante puro no existe y los colores muy saturados salen más lavados (límite: canal más alto menos el más bajo, 0,635 de 255).

## 3. Paleta de 16 colores (modos normales)

- **Índice:** el color de 4 bits del camino normal `{BRIGHT, G, R, B}` (bit 3 = BRIGHT, 2 = G, 1 = R, 0 = B: el mismo de los atributos y de `$7FEF`), ya con los sprites. **Vale para todos los modos
  menos el 160x100** (texto, HiRes, Spectrum, 70/80 columnas, sprites y borde) y solo con el color encendido (`$7FEF` bit 5); con el modo 160x100 activo manda la de 256 y se ignora esta.
- **Desactivada** (reset y `POKE 2067,0`): sale lo de siempre, los colores por defecto de Chroma (el emulador no cambia nada). **Activada** (`POKE 2067,1`): el color k se dibuja con la entrada
  `pal16[k]`, convertida a RGB con la misma fórmula de arriba.
- **Valores de fábrica:** cada entrada es el color digital de Chroma (cada canal a 0 o 15, y BRIGHT a 0 o 15): activarla sin cargar nada **no cambia lo que se ve**. Los valores se conservan al desactivarla
  (solo se pierden con un reset de la FPGA).
- **Carga:** `2063,n` (solo cuentan los 4 bits bajos del puntero para la de 16), `2064,lo` y `2066,hi`, que escribe y avanza el puntero (a 8 bits: tras la 15 sigue la 0 por los 4 bits bajos). La de 256 y la de 16
  no se pisan: `2065` solo escribe la de 256 y `2066` solo la de 16.
- **Retardo:** unos 4 ciclos de 208 MHz (menos de 1/6 de un píxel normal) que comparte con el borrado: no hay nada que emular.

## 4. Lo que no cambia

- Los modos que no usan la paleta de 16 se dibujan como siempre. Con la paleta de 16 desactivada no hay diferencia con el emulador de ahora.
- Sin cambios en el MCU para este modo ni para las paletas. El comando 76 (BMP como `.I16`) es otro tema: `bmp_emulador.md`.
- El formato de instantáneas del emulador no necesita nada nuevo para el modo salvo guardar y restaurar: la paleta de 256, la de 16, su estado activado y `POKE 2099`.

## 5. Modo 160x200 a 4 bpp con ventana, sprites y texto (`POKE 2045,176`, rama `fpga-mode160-4bpp`, 1/11/2026)

Simulado en ModelSim (`tb_mode160_4bpp.v`, `tb_mode160_spr.v`, `tb_mode160_txt.v`, con `-GWF=n -GWL=m`), sintetizado (LUTs 70 %, slices 92 %, 208 MHz con 1,55 ns de holgura), **sin probar en la placa**.
Diseño y razones: `SCUMM/AGI/ESTUDIO_160x168.md`. El 8 bpp (`2045,175`) no cambia. Mismos relojes y misma temporización de 80 columnas que el 160x100.

| POKE | Función |
|---|---|
| `2045,176` | modo 160x200 a 4 bpp (activa la temporización de 80 columnas, `sf80`); cualquier otro valor de 2045 lo apaga (el 175 vuelve al 8 bpp) |
| `2070,n` | `win_first` (0-24): primera fila de texto (de 8 líneas) con imagen. Por omisión 0. El reset lo vuelve a 0 |
| `2071,n` | `win_last` (1-25): fila siguiente a la última con imagen. Por omisión 25. El reset lo vuelve a 25 |
| `2069,v` | bit 0: plano de texto activado. El reset lo desactiva |
| `2068,v` | color del texto `{papel[3:0], tinta[3:0]}`, índices de la paleta de 16. Por omisión `$0F` (tinta 15, papel 0) |
| `2063/2064/2066` | la paleta de 16 de siempre (§3): aquí es **la única paleta** del modo (los píxeles, los sprites, el texto y el borde pasan por ella, con la paleta de 16 «activada» o no) |

- **Rejilla:** 160x200 píxeles = 25 filas de texto de 8 líneas, de la línea **58** a la **257** (una línea de TV por línea de rejilla, no dos), cada píxel 3 ciclos de 13 MHz, como el 160x100.
- **Imagen:** 4 bits por píxel, 80 bytes por fila, el **nibble alto es el píxel de la izquierda**; el índice (0-15) se pasa por `pal16`. Empieza en `{HFILE[15:13], 13'b0}`; la fila `k` de la imagen
  es la línea `8*win_first + k` de la rejilla. Ocupa `(win_last - win_first) * 640` bytes (200 líneas = 16.000; AGI, `win_first=1`, `win_last=22` = 168 líneas = 13.440). Lee de la sombra directamente.
- **Fuera de la ventana** (filas de texto fuera de `[win_first, win_last)`): no se lee la imagen y sale el color del borde (`$7FEF`, 4 bits, como Chroma); el borde pasa por `pal16` como cualquier índice.
- **Sprites:** el motor de 64 sprites de siempre (8x8, 2 colores por fila, K = 12 por línea, tablas en la sombra), pero con otra referencia de posición: X de sprite = columna de la rejilla + 32 (0-159) e Y de sprite = línea
  de la rejilla - 8*`win_first` + 32, es decir, fila de la imagen + 32. Solo hay sprites en las líneas de la ventana (el motor no carga el resto) y solo sobre los 160 píxeles. El píxel del sprite pisa al
  del fondo con su tinta o su papel (el color de 4 bits de la fila, que pasa por `pal16`); el sprite de índice más alto gana, como siempre.
- **Plano de texto** (con `2069` bit 0): 40 columnas x 25 filas de celdas de 4 píxeles de imagen (12 ciclos de 13 MHz) x 8 líneas; el glifo es de **6x8** y cada píxel de glifo dura 2 ciclos (6 x 2 = 12), desde
  el principio de la celda. Va **por encima de la imagen y de los sprites**.
  - **Códigos:** 1.000 bytes, `fila*40 + columna`, justo detrás de la imagen: dirección `{HFILE[15:13],13'b0} + (win_last - win_first)*640 + fila*40 + columna`. Con la ventana de 22 filas o menos, imagen y códigos
    caben en los mismos dos bloques de 8 KB.
  - **Glifos:** de la tabla de 256 caracteres del texto superfast (`ROMTABLE[15:11]`, el registro I del Z80; tabla de 2 KB alineada): byte de la fila `r` del carácter `c` = `tabla[c*8 + r]`; el bit 7
    del byte es el píxel de la izquierda, y solo cuentan los 6 bits altos.
  - **Código:** `c < 32` = celda **transparente** (se ve la imagen o el sprite); `c >= 32` = celda **opaca**: cada píxel de glifo es tinta (1) o papel (0), y el **bit 7 del código invierte** el glifo
    (el carácter `c` y el `c+128` pueden tener definiciones distintas: la inversión se aplica a la del código completo, como en el texto de 256 caracteres).
  - Las celdas de las filas fuera de la ventana también se dibujan (la barra de estado y las líneas de mensajes de AGI).
- **Lo que no hay:** scroll, doble buffer ni blit; el 8 bpp sigue sin sprites ni texto.
- **Temporización a tener en cuenta (no hace falta emularla, pero explica los límites):** el texto lee su código y su glifo en los huecos impares de la lectura del 4 bpp, y empieza 4 píxeles antes
  que la imagen para tener la primera celda a tiempo; el motor de sprites carga sus slots entre el sincronismo horizontal y el ciclo 195, antes de que empiece este modo, igual que en 80 columnas.
- **Instantáneas:** guardar y restaurar además `4bpp` (el modo 176), `win_first`, `win_last`, `txt_en` y `txt_col`, y la paleta de 16 (ya incluida).
- **Cómo probarlo:** `EXAMPLES/MODE160/mode160w.asm` (`MODE160W.B81`): ventana 1-22 o 3-17 (teclas 1 y 2), imagen de barras y diagonales, texto (T) con barra de estado en inverso y caja opaca sobre la imagen,
  anillo con Q A O P y dos cuadrados en las esquinas de la ventana; `mode1604.asm` (200 líneas, sin nada más) y `png2i4.py` para convertir una imagen de 160x200 con 16 colores. Los bancos de pruebas de la
  FPGA dicen qué se espera de cada píxel.

## Cómo probarlo

- `EXAMPLES/MODE160` (`MODE160.B81`): muestra 256 colores a la vez (grises, primarios, rueda de tonos); `P` / `O` mueven la imagen de uno en uno entre 0 y 15 (`POKE 2099`).
- `EXAMPLES/PHOTO` (`PHOTO.B81`): visor de `IMGnnn.I16` (y de `IMGnnn.BMP` con el comando 76). Sirve para ver paletas y fotos reales.
- `EXAMPLES/COLORTEST` (`COLORTEST.B81`): la tecla `P` recorre las paletas de 16 (gris, rojo, verde, azul, amarillo, cian, magenta y desactivada): con una rampa puesta, cada muestra se ve en tonos
  de ese color y los colores con brillo (8-15) quedan más claros que los normales (0-7).
- Los bancos de prueba de la FPGA (`tb_mode160.v`) y los arneses del PC (`claude/mqtttest/build_mode160.sh`, `build_colortest.sh`, `build_photo.sh`) dicen qué ejecuta el Z80 y qué se espera de la FPGA.
