# Sprites fuera de 256x192, modo 160x200 y modo mixto gráfico + texto (sondeo, 28/10/2026)

**Estado: solo análisis, sin decidir ni implementar.** Se recogen las posibilidades para decidir qué hacer con el poco espacio que queda en la FPGA. No hay código tocado por esto.
Contexto: rama `fpga-mode160` (modo 160x100 a 256 colores, paleta de 16, ver `pdm_color.md`).

## 1. Punto de partida

- **FPGA hoy** (síntesis con la paleta de 16 colores): LUTs 3.889 / 5.720 (**67 %**), slices 1.272 / 1.430 (**88 %**), BRAM **32/32**, todas las restricciones cumplidas. Los caminos de 208 MHz del PDM cumplían con unos 1,3 ns de margen cuando estábamos en el 90 %.
- **Doble buffer / blit:** copia **un único bloque de 8 KB** (`HFILE` → `front_blk`, ~630 µs) y arranca en la línea 254, justo tras el área normal (62..253). No hay BRAM para un segundo bloque. Corrección de algo que se dijo antes en la conversación: *no* es cierto que el doble buffer pueda copiar dos bloques; tal como está el blit, un modo de 16 KB no tendría doble buffer. Esto es lo que hace especialmente interesante que el mixto quepa en **un solo bloque**.
- `mode160` ocupa las líneas 58..257 (200 líneas) y hoy no tiene blit, doble buffer, scroll ni sprites.

## 2. Cómo funcionan los sprites hoy

- `sprite_engine.v`: 64 sprites de 8x8, hasta `K = 12` por línea. La tabla vive en la sombra (`$0C00` + sprite*32 para 0-31 y `$1800` para 32-63: activo, X, Y, color por fila, píxel por fila, máscara por fila). Prioridad al índice más alto.
- Va a 26 MHz y lee la tabla por el puerto B entre el sincronismo horizontal y el pixel `limit` (120 en modos normales, 195 en los de 13 MHz), cuando el vídeo no lee, y no mientras copia el blit o el motor de scroll.
- El motor **no depende del modo**: recibe `pos_x`, `pos_y`, `row_ok` y devuelve `hit`, `pixel` y `color` (tinta y papel de 4 bits cada uno).
- Lo que lo ata a 256x192 está en `SD81.v`:
  - la posición sale de `pixel_cnt` menos un offset calibrado por modo (`SPR_X_FUDGE*`, `SPR_Y_FUDGE*`), con el recorte fijo `spr_in_display` a 256x192;
  - el color entra por el camino normal de 4 bits: `video = sprite_active_final ? ~sprite_pixel_final : serial_output` y `{ink_active, paper_active} = sprite_active_final ? sprite_color_final : attr_o`.
- En los modos de 70/80 columnas `pixel_cnt` va a 13 MHz: según el código el sprite sale con la mitad de tamaño y solo en los primeros 256 ciclos de la línea. No está mirado en la placa.

## 3. Sprites en el modo 160 (256 o 16 colores)

- En `mode160` el camino normal de 4 bits sigue calculándose (`c4`), y cuando hay sprite lleva su color. Bastaría que, donde hay sprite, la salida fuese `pal16[c4]` en lugar de `pal[idx_cur]`: los sprites serían de **16 colores** (los de `pal16`) sobre un fondo de 256.
- `pos_x` = el contador de píxel de `mode160` (`pcnt`, un píxel cada 3 ciclos de 13 MHz); `pos_y` = fila de la imagen (con líneas dobles, el motor recarga en cada línea los mismos datos).
- Memoria: la imagen empieza en `x_start - 4` ≈ 242, por encima del límite de 195 del motor, así que el puerto B tiene hueco para él.
- Límites: sprites de 8x8 píxeles del modo (1,5 veces más anchos que los normales), 12 por línea, 2 colores por fila. La posición hay que calibrarla en la placa.
- **Coste estimado: 30–60 LUTs.**

## 4. Modo 160x200

- Hoy cada fila de la imagen se repite en dos líneas (`row = ly[7:1]`). Para 200 filas distintas basta direccionar con 8 bits de fila (`ly[7:0]`) y la misma cuenta `fila * 160`. **Unas 5–10 LUTs.**
- 8 bpp: 32.000 bytes = 4 bloques. 4 bpp: **16.000 bytes = 2 bloques** (384 de sobra). En los dos casos sin doble buffer (ver §1).
- Proporción del píxel: 160x200 es el de CGA / C64 multicolor.
- **4 bpp:** un byte por dos píxeles, con el índice por `pal16` (la misma tabla que usan los modos normales y los sprites). Desempaquetado: **unas 20–30 LUTs**. Hay que decidir el orden de los nibbles.

## 5. Modo mixto: gráfico arriba y texto de 42 columnas abajo

Idea del usuario: líneas simples, no dobles; como un 160x200 partido en dos mitades. Texto con color por carácter, estilo Spectrum.

**Por qué es razonable:** `mode160` y el texto de 70/80 columnas comparten temporización (reloj de píxel de 13 MHz, 828 ciclos por línea, sincronismo adelantado). Cambiar de región en un límite de línea no toca el reloj ni los sincronismos. Eso era lo que hacía arriesgado mezclar con los modos de 256 píxeles de reloj de 6,5 MHz.

### 5.1 Variante de dos bloques (descartada para el doble buffer)

160x104 a 4 bpp (8.320 bytes) + zona de texto en bitmap de 1 bpp a 2 ciclos por píxel (256 bytes por fila de texto + atributos), con el texto dibujado por software (EXPLORER ya lo hace con su fuente de 6 px). Hardware: registro de línea de corte, desplazador, contador de dirección y registros de color; estimado en unas 60–100 LUTs. Cabría 160x104 + 11 filas = 192 líneas, o + 12 filas = 200. Sin doble buffer y con el texto costando CPU.

### 5.2 Variante de un solo bloque (la que más interesa)

Todo en 8.192 bytes: gráfico a 4 bpp (80 bytes por línea) y texto con generador de caracteres, **84 bytes por fila de texto** (42 códigos + 42 atributos de 8 bits, tinta y papel de 4 bits como índices de `pal16`).

| Gráfico (4 bpp) | Filas de texto | Bytes | Líneas totales | Notas |
|---|---|---|---|---|
| 160x96 (7.680) | 6 | 8.184 | **144** | cabe (8 bytes de sobra), pero la imagen es 48 líneas más baja que la normal |
| 160x88 (7.040) | 13 | 8.132 | **192** | área de siempre (62..253): el blit actual (línea 254) no cambia |
| 160x80 (6.400) | 15 | 7.660 | 200 | misma geometría que el 160x200; el blit tendría que arrancar después de la 257 |
| 160x96 | 12 | 8.688 | 192 | **no cabe** (496 bytes de más) |
| 160x104 | cualquiera | 8.320 | — | **no cabe**: solo el gráfico pasa de 8.192 |

Variante con un atributo por cada **dos** caracteres (63 bytes por fila): 160x96 + 8 filas = 8.184 bytes y 160 líneas.

### 5.3 El generador de texto

- Con un solo bloque no vale el bitmap de 1 bpp (una fila de texto son 256 bytes): hace falta generador de caracteres, que lee código, glifo y atributo por celda de 12 ciclos (6 píxeles a 2 ciclos), el glifo con los bits duplicados.
- **Fuente:** unos 768 bytes (96 caracteres de 6x8) fuera del bloque, en otro; con base propia. La fuente de 8x8 de la ROM recortada a 6 columnas estropea glifos.
- **Reutilizar el texto superfast de 80 columnas:** ya lee carácter, glifo y atributo, con las bases independientes de POKE 2096–2098 (pantalla) y 2059–2061 (atributos). Haría falta una celda de 12 ciclos con glifo duplicado, 42 columnas y el corte por línea. Ese código tiene muchas compensaciones calibradas en la placa (`attr_col_80`, `col_cnt_b7`, offsets `+4`, `+12`...) y **no está leído a fondo**: no se puede asegurar que se reutilice sin tocarlo. Primer paso si se sigue adelante.
- **Generador nuevo:** contadores (celda 0..11, columna 0..41), puntero de dirección, dirección del glifo, desplazador de 6 bits y registro de atributo.

### 5.4 Sprites en el mixto

La posición la daría la región activa: el gráfico avanza cada 3 ciclos y el texto cada 2. Un multiplexor de `pos_x`/`pos_y` y la región activa.

## 6. Presupuesto de la FPGA (estimaciones, no medidas)

| Pieza | LUTs estimadas |
|---|---|
| Sprites en `mode160` | 30–60 |
| 160x200 | 5–10 |
| 4 bpp | 20–30 |
| Mixto reutilizando el texto superfast (celda de 12 + corte por línea) | 50–80 |
| Mixto con generador nuevo | 100–150 |

Con todo junto los slices pasarían del 88 % a más o menos el **91–93 %**. El riesgo no es el número de LUTs sino el empaquetado: por encima del 93–95 % cuesta cerrar los caminos de 208 MHz. Candidatos para liberar sitio, sin medir:
- la paleta de 256 entradas (~128 LUTs), solo si el 4 bpp sustituyera al 8 bpp;
- bajar los slots de sprites de 12 a 8;
- revisar qué del scroll / sprites / depurador se puede compartir.

## 7. Orden de trabajo propuesto (si se decide seguir)

1. 160x200 (flag de altura) y sprites en `mode160`: baratos y reutilizan lo que hay. Probar con el banco de pruebas `tb_mode160.v` y calibrar en la placa.
2. Medir los slices.
3. 4 bpp.
4. Leer a fondo el texto superfast de 80 columnas y decidir si se reutiliza.
5. El mixto, empezando por una versión sin sprites.

## 8. Decisiones pendientes

- ¿Altura del mixto: 144, 192 o 200 líneas? (opciones del §5.2)
- ¿Un atributo por carácter (84 bytes por fila) o uno por cada dos (63)?
- ¿Orden de los nibbles en 4 bpp y paleta: `pal16` o las 16 primeras de la de 256?
- ¿Los sprites del modo 160 con los 16 colores de `pal16`, o hace falta algún modo con sprites de 256 colores?
- ¿Se quiere doble buffer en el mixto? (solo es posible en la variante de un bloque)
- ¿Qué se sacrifica si el presupuesto no alcanza?
