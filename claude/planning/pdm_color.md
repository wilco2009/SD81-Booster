# Modo de 256 colores: 160x100 a 8 bpp con paleta y PDM

Rama `fpga-mode160` (desde `fpga-scroll`). Idea del usuario (27/10/2026): sacar un tren de pulsos (PDM) en vez de un nivel fijo por los pines
`VRED`, `VGREEN`, `VBLUE` y `BRIGHT` para pasar de los 16 colores de Chroma81, y un modo nuevo de 160x100 a 8 bpp para tener 256 **a la vez**.
Decisiones: primera versión sin blit ni scroll ("la pantalla a pelo"), geometría de 3 ciclos de 13 MHz por píxel.

## Estado (27/10/2026)

- Escrito `FPGA/SD81V2.1000/mode160.v`, conectado en `SD81.v`, probado en ModelSim (`tb_mode160.v`, 8,2 millones de comprobaciones, 8 mutaciones: 7 detectadas y la
  otra es un margen de temporización equivalente), y **sintetizado** en la VM (ISE 14.7) con todas las restricciones cumplidas.
  **Sin probar en la placa.**
- Programa de prueba `EXAMPLES/MODE160` (el Z80 pasa por `claude/mqtttest/build_mode160.sh`) y paleta de prueba `pal160.py`.
- Recursos tras Map/PAR (antes entre paréntesis, de `fpga-scroll`): LUTs **3.947 (69 %)** (3.475, 60 %), slices **1.284 (89 %)** (1.190, 83 %), registros 2.523 (2.354),
  BRAM 32/32 (igual), IOB 99/102. La paleta (RAM distribuida de 256x16, doble puerto) son unas 128 LUTs; el resto, contadores, sumadores y la sincronización de 208 MHz.
- Temporización: los caminos de 208 MHz (reloj `CLK104`, ver abajo) cumplen con 1,29 ns de margen sobre 4,81 ns; el resto de restricciones, igual que antes.
- Ficheros nuevos de la síntesis (`SD81.bit`, `sd81boster.mcs`, informes) sustituyen en la carpeta a los de la rama anterior; los de antes se guardaron aparte. No se ha
  estampado la versión de la FPGA (`append_fpga_version.py` necesita Python 3, que no está en la VM) ni se ha copiado a `sd81.mcs`.

## Registros

| Dirección | Función |
|---|---|
| `POKE 2045,175` | modo 160x100 a 8 bpp (activa `sf80_en` y `sf160_en`: usa la temporización de 80 columnas; los otros valores de 2045 lo apagan) |
| `POKE 2043/2044` | HFILE: la imagen empieza en `{HFILE[15:13], 13'b0}` (con 7 se sale de los 64 KB: usar 0-6) |
| `POKE 2063,n` | puntero de la paleta (0-255) |
| `POKE 2064,lo` | byte bajo de la entrada `{G[3:0], B[3:0]}` (se guarda) |
| `POKE 2065,hi` | byte alto `{BRIGHT[3:0], R[3:0]}`: escribe la entrada y el puntero avanza (da la vuelta en 255) |
| `POKE 2099,n` | posición horizontal (0-15, por defecto 8): `x_start = SCR_START_X_80 + 36 + n`, en ciclos de 13 MHz (3 por píxel) |
| puerto `$7FEF` | el borde sale de sus 4 bits (como en Chroma); `color_enable` se ignora en este modo |

Los pines R, G, B y BRIGHT llevan cada uno una densidad de pulsos `n/15` (n de 0 a 15) de su campo de la paleta. Paleta por defecto (sin escribir nada): RGB 3-3-2 sin brillo
(rojo y verde 0-7, azul 0-3), para ver algo sin cargarla.

## Geometría y temporización

- Píxel: 3 ciclos de `pixel_clk` a 13 MHz (4,33 MHz, 231 ns); 160 píxeles = 480 ciclos de los 828 de la línea. La ventana empieza en `x_start`.
  La primera versión se hace con 3 y no con 4 ciclos (3,25 MHz, 640 ciclos) porque cabe en la ventana medida de 80 columnas (202-769); con 4 hay que comprobar el hueco real (en el
  modo de 80 columnas se midió que un televisor no enseña los primeros ciclos tras el back porch).
- Vertical: cada fila se repite en dos líneas (200 líneas, de la 58 a la 257, centradas en 62-253). La 254 es donde empieza el blit/motor de scroll: en este modo **no se pueden usar**.
- Lectura: un byte por píxel, pedido un píxel antes por el puerto B de la BRAM de sombra (`v_addr_160`, el mux del puerto B ahora elige entre sprites, blit, motor y vídeo de siempre o este);
  el dato llega dos flancos de `system_clk` después y se queda en `idx_next`; en cada principio de píxel pasa a `idx_cur` y la paleta (lectura asíncrona) saca los 4 niveles.
- La fila se calcula con `fila*160 = fila*128 + fila*32` y un puntero que avanza.
- Los sprites no se dibujan en este modo (se pasa por encima del camino de ink/paper/sprites), ni el scroll fino ni el motor.

## La salida: PDM a 208 MHz

- El reloj `CLK104` de `SD81.v` es el DCM de `CLK100` (pin 14), y **sale a 208 MHz** (el nombre es histórico: en `ipcore_dir/clk104.xco` `clkout1_requested_out_freq=208`; por eso
  `pulse_cnt[4]` es de 6,5 MHz y `pulse_cnt[5]` de 3,25 MHz). Un píxel son unas 48 ranuras de 4,8 ns, aunque sin relación de fase (los dos relojes salen de cristales distintos).
- Un modulador de primer orden por pin: acumulador de 4 bits módulo 15; salida 1 cuando `acc + n >= 15`. Densidad `n/15` exacta con `n=15` siempre a 1 y `n=0` siempre a 0.
  No se reinicia en cada píxel: el error pasa de un píxel al siguiente.
- Cruce de dominios: los niveles (registro `lvl` en `system_clk`), `img_on`, el borrado y el borde pasan con dos registros a 208 MHz, y las salidas se registran allí
  (dos etapas para no cargar los 4,8 ns). Un nivel que cambia justo cuando se muestrea puede dar una sola ranura de más o de menos: es invisible.
- `UCF` (`SD81XC6.ucf`): `SLEW = FAST` en `VRED`, `VGREEN`, `VBLUE` y `BRIGHT` (con SLOW los pulsos de una ranura casi no existirían) y `TS_CLK100 = PERIOD 10 ns` para que
  la herramienta mida los caminos de 208 MHz. Los dos cambios también valen para el resto de modos.
- Latencia: del cambio de `pixel_cnt` al pin pasan unos 6 ciclos de `system_clk` más 5 de `CLK104` (menos de 1 ciclo de 13 MHz): por eso `x_start` lleva el `+36 + n`; se calibra con `POKE 2099`.

## La red analógica (esquema, SCART)

Por canal: 270 Ω desde el pin del canal, 130 Ω a masa, 470 Ω desde `BRIGHT` (común a los tres), y a la SCART directa (carga de 75 Ω del televisor), **sin condensador**.
Por superposición, la tensión de un canal es `V = 0,455 · d + 0,261 · b` (d y b, de 0 a 1, son las densidades del pin del canal y de BRIGHT): 0 a 0,717 V.

- Los 16 colores de Chroma81 se reproducen exactamente con `d = 0 / 1` y `b = 0 / 1` (la fila 7 de la paleta de prueba): 0, 0,261, 0,455 y 0,717 V por canal. El negro brillante es un gris de 0,261 V.
- Con PDM en los 4 pines, cualquier color con (canal más alto - canal más bajo) <= 0,455 V es alcanzable (elegir `b` y luego `d` por canal). Un primario puro brillante (1, 0, 0) **no**
  (el máximo es (1, 0,36, 0,36), como Chroma81). Lo calcula `EXAMPLES/MODE160/pal160.py` (`solve`).
- Para llegar a todo el cubo bastaría quitar los 130 Ω (R19, R27, R28) y los 470 Ω (R33, R34, R35): `d` iría de 0 a 0,717 V (270 Ω contra 75 Ω).
- El filtrado lo hacen hoy el cable y el televisor. La impedancia de Thevenin en el nodo es de unos 37 Ω; un condensador de 330-470 pF en paralelo con cada 130 Ω da una frecuencia de corte de
  10-13 MHz (tiempo de subida de unos 27 ns, 1/8 de píxel).

## Qué falta / riesgos

- Probarlo en la placa: imagen centrada y estable, grises sin bandas, filas y bordes de bloque limpios, y los 16 de Chroma81 como en el modo normal. Si hay rizado, los condensadores.
- Si hay que centrar más allá de `POKE 2099`, `SCR_START_X_80 + 36` en `SD81.v`; en vertical, `Y0` en `mode160.v` (hoy 58).
- Un modo de 4 ciclos por píxel (3,25 MHz) para píxeles casi cuadrados, si cabe.
- Sprites, scroll, blit y doble buffer en este modo (hoy no); paleta por línea (raster) si se quiere más de 256 colores a la vez.
- Una herramienta que convierta una imagen a 160x100 con paleta propia (necesita Pillow, que no está instalado): `pal160.py` ya tiene el `solve` de RGB a las 4 densidades.
- El manual: ver `manual_pendiente.md`.

## Paleta de 16 colores para los modos normales (27/10/2026)

Misma salida PDM, pero para el texto, HiRes, Spectrum, sprites y borde: el color de 4 bits `{BRIGHT,G,R,B}` que sale del camino normal (`cbright`, `cgreen`, `cred`, `cblue` en `SD81.v`, ya con
sprites) indexa una tabla de 16 entradas con el mismo formato que la de 256 (cuatro densidades 0-15). Desactivada, los pines salen del camino digital de siempre: los colores por defecto.

| Dirección | Función |
|---|---|
| `POKE 2063,n` | puntero (el mismo de la paleta de 256; para la de 16 cuentan los 4 bits bajos) |
| `POKE 2064,lo` | byte bajo `{G,B}` (el mismo registro) |
| `POKE 2066,hi` | byte alto `{BRIGHT,R}`: escribe `pal16[puntero & 15]` y el puntero avanza |
| `POKE 2067,1` / `2067,0` | activa / desactiva la paleta de 16 (solo cuenta con el color encendido, `$7FEF` bit 5; al reset, desactivada) |

- La tabla arranca con los colores digitales de Chroma (cada canal a 0 o 15, BRIGHT a 0 o 15): activarla sin cargar nada no cambia lo que se ve. Los valores se conservan al desactivarla.
- Las dos paletas no se pisan: 2065 solo escribe la de 256 y 2066 solo la de 16. Con el modo 160x100 activo manda la de 256 (`use16 = pal16_en & ~en`).
- Va dentro de `mode160.v` (comparte los modulados PDM): `lvl16 <= pal16[c4]` a 26 MHz, y en `s1` se elige `lvl16` (con la imagen siempre "encendida") o `lvl`. Coste: una RAM distribuida de 16x16 y un multiplexor.
  El retardo de unos 4 ciclos de 208 MHz (20 ns, un 13 % de un píxel de 6,5 MHz) lo comparte el borrado, que pasa por el mismo camino.
- Como el camino de 4 bits se muestrea a 26 MHz, un píxel de HiRes (6,5 MHz) son 4 muestras: no hay problema; las transiciones salen igual de limpias.
- Sintetizada: LUTs 3.889 (67 %), slices 1.272 (88 %), todas las restricciones cumplidas. `tb_mode160.v` (fase 6) y `claude/mqtttest/build_colortest.sh` pasan; 4 de 4 errores puestos a propósito detectados
  (sin `use16 |` en la imagen, puntero de 3 bits, orden de bits de `c4` cruzado, la de 16 sin ceder al modo 160).
- Ejemplo: `EXAMPLES/COLORTEST` (tecla P recorre rampas de gris, rojo, verde, azul, amarillo, cian y magenta y luego desactiva, `gen_pal16.py` -> `pal16.inc`).
- Límites: es la misma red analógica, así que un color solo es alcanzable si su canal más alto menos el más bajo no pasa de 0,455 V; los primarios brillantes saturados se aproximan.

## Pruebas

- `tb_mode160.v` (`vlog mode160.v tb_mode160.v; vsim -c tb_mode160 -do "run -all; quit"`): paleta por defecto y cargada (con autoincremento y vuelta), geometría (32.000 píxeles por cuadro,
  100 filas dobles, ventana y bordes), cambio de base, densidad de pulsos de los 4 pines sobre un color constante, borde con los 4 bits distintos, borrado (sincronismo, back porch y vsync)
  y apagar / encender el modo. Con 8 errores puestos a propósito (filas de 144, última columna, sin autoincremento, borde cambiado, sin borrado, palabras de la paleta cruzadas, fila mal
  calculada) falla en todos salvo uno (capturar el dato un flanco antes: es margen, no error).
- `claude/mqtttest/build_mode160.sh`: el Z80 de EightyOne ejecuta `mode160.asm` con la paleta, el teclado y el VSYNC simulados y comprueba paleta (256 entradas por el protocolo), imagen (byte a byte), orden (el
  modo se activa después), `P` / `O` y la salida.
