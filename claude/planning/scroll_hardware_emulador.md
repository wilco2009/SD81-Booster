# Scroll por hardware: lo que falta en el emulador (EightyOne)

Resumen (2026-10-26) de lo que hay que implementar en el emulador (`C:\ClaudeCode\Eightyone2\src\SD81Booster\`) para igualar a la FPGA
de la rama `fpga-scroll`. Diseño completo en `scroll_hardware.md`; la FPGA de esta rama está sintetizada solo hasta la fase 1
(Spectrum) y probada en simulación la 1b (HiRes nativo): **sin probar en la placa**, así que conviene esperar a esa prueba antes de
dar el emulador por cerrado (las direcciones de la columna 32 y de la fila de abajo son lo que más puede cambiar).

## Estado (26 de octubre de 2026): IMPLEMENTADO EN EL EMULADOR

Los puntos 1 a 6 de «Pendiente» están hechos (marcados abajo con ✅). Lo que quedó fuera o por probar:

- **Probado:** las direcciones de las dos fórmulas (HiRes nativo y Spectrum) se han cruzado con una traducción bit a bit de `lin_scroll_addr.v` y
  `spec_scroll_addr.v` en **51.904.512 casos** (fila, línea, columna 0-32, fino Y, superior, inferior): 0 diferencias; y la regresión (fino Y = 0 y
  columna < 32 dan las direcciones de antes): 0 diferencias. La cadena del motor (izquierda / derecha, con y sin `L`) también. Programa:
  `tools/test_scroll_addr.cpp` del repositorio del emulador (`g++ -O2 -std=c++17 -I src/SD81Booster tools/test_scroll_addr.cpp`).
- **Probado por el usuario (26/10/2026):** `SCROLLT.BIN`, `SCROLLH.BIN` y `SCROLLM.BIN` cargados en el emulador: funcionan bien. Queda comparar con la placa cuando se pruebe la FPGA de esta rama: las direcciones de la columna 32 y de la fila de abajo siguen siendo lo que más puede cambiar.
- **No hecho (es del firmware, no del emulador):** ampliar `SCROLL` y el comando de la consola del depurador del MCU (`mcu/DEBUGGER.cpp`) con los registros
  nuevos. El emulador **sí** guarda y restaura sus propias instantáneas con una línea nueva `SCROLLY` (fino Y, fila superior, fila inferior; ver 3). Las
  instantáneas hechas por el depurador del hardware, mientras no los incluyan, pierden el fino Y al restaurarse.
- **Motor vertical (órdenes 3 y 4):** ya está en la FPGA (fase 3, 2026-10-26); en el emulador **falta**: ver el apartado 7.

## Lo que el emulador ya tiene

- Fino X: `POKE 2090` (`m_sfHScroll`) y la máscara de filas 2091-2093, en los tres submodos Superfast (`SD81Booster.cpp` ~1540-1860):
  ventana de 16 bits con la columna siguiente, el color de los píxeles prestados es el de la columna de al lado (`drawScrolledColumn`).
- Texto: la columna 32 sale del byte de NEWLINE del DFILE (igual que la FPGA).
- HiRes nativo y Spectrum: la columna 32 **da la vuelta a la 0** (`col & 0x1F`; comentarios en ~1596 y ~1680). Es el comportamiento
  antiguo de la FPGA, que ya no vale.
- Doble buffer, `vidRead()` con `vpage`, el espejo de la sombra `m_shadowMirror[8][8192]`, Chroma modo 0 y modo 1 (`chromaAttrAddrM1`),
  `POKE 2059-2061`, snapshots con la línea `SCROLL` (4 valores) y el depurador del MCU (`mcu/DEBUGGER.cpp`).

## Pendiente, por orden

### 1. Puerto `$3FEF` extendido (sim_int rev 0.19) — `SimIntDebugWrite` / `SimIntDebugRead`

✅ **Hecho.** `SimIntDebugWrite/Read` en `SD81Booster.cpp`: `$20+n` (IN 16-31), `$40+r` (registros 8-23), registros 8 (fino Y), 9 (orden del motor), 10 y 11 (región), IN 16-19; valores por defecto 0 / 0 / 23 en el reset y en las instantáneas.

Hoy: `d & 0x80` → registro `d & 7`; `d == 0x10` → trampa; `d <= 0x0F` → índice IN. Hay que añadir, sin romper nada de lo anterior
(los códigos `$11-$1F`, `$30-$3F`, `$50-$7F` siguen sin hacer nada):

| OUT | Efecto |
|---|---|
| `$20+n` (n 0-15) | el IN devuelve el índice 16+n (`m_dbgIdx` pasa a ser de 5 bits) |
| `$40+r` (r 0-15) | el OUT siguiente es el dato del registro 8+r (`m_dbgRegSel` = 8+r) |

Registros de escritura nuevos (los demás, 12-23, no hacen nada): **8** = fino Y (solo bits 2:0), **9** = orden del motor (fase 2; de
momento nada), **10** = fila superior de la región (bits 4:0), **11** = fila inferior (bits 4:0). Valores por defecto tras reset:
fino Y = 0, superior = 0, inferior = 23.

IN nuevos: **16** = estado del scroll (bit 7 ocupado, bit 6 orden pendiente, bit 5 en marcha; 0 hasta que haya motor), **17** = fino Y,
**18** = fila superior, **19** = fila inferior (los 17-19 devuelven lo que dicen los registros, con los bits no usados a 0).
El IN 11 (`0xC4`, «esta FPGA tiene los índices 9 y 10») y el 15 (`0x52`) no cambian. El reset del emulador y `SnapRestore*` tienen que dar
los valores por defecto.

### 2. Dibujo: HiRes nativo y Spectrum con columna 32 real, fino Y, región y fila de abajo

✅ **Hecho.** Las fórmulas están en `src/SD81Booster/scroll_addr.h` (`LineOf`, `HiResOff`, `SpecPixOff`, `SpecAttrOff`) y las usa el dibujo de los dos modos; el atributo de modo 1 usa la fila de origen (`srcRow`, 24 abajo) y el de modo 0 la misma `off` que el bitmap, en el bloque 6.

Misma función para las dos cosas (`y = charRow*8 + charLine`):

- `in_region = charRow >= top && charRow <= bot` (con `top > bot` la región es vacía y no se mueve nada).
- `ys = y + (in_region ? fineY : 0)` (máx. 198, no se sale del byte).
- `below = in_region && (ys >> 3) > bot` (la línea viene del búfer de abajo; `k = ys & 7`, de 0 a 6).
- `col` va de 0 a **32** (la 32 solo se pide con fino X ≠ 0 en la fila, como hoy).

**HiRes nativo** (`lin_scroll_addr.v`), dirección en el bloque de 8 KB (`vpage` para el bitmap; `$C000` fijo para el atributo de modo 0):

| Caso | Dirección (offset de 13 bits) |
|---|---|
| normal | `ys*32 + col` (col < 32) |
| columna 32 | `$1800 + ys` |
| abajo | `$1900 + k*32 + col` |
| abajo, columna 32 (esquina) | `$1A00 + k` |

- **Bitmap**: `{vpage, off}`. **Atributo de modo 0** (8x1, Chroma modo 0): `{$C000, off}`, **misma `off`**, sin `vpage` (lo de siempre).
- **Atributo de modo 1** (8x8): fila de origen `src_row = below ? 24 : ys>>3`, dirección `ATTR/DFILE_eff + 1 + src_row*33 + col` con `col` de 0 a 32
  (la 32 es el hueco del NEWLINE, paso 33). Mismo `chromaAttrAddrM1` de hoy.

**Spectrum** (`spec_scroll_addr.v`), offset de 13 bits, `vpage` en los dos (el atributo también lleva `vpage`):

| Caso | Bitmap | Atributo |
|---|---|---|
| normal | `{ys[7:6], ys[2:0], ys[5:3], col}` | `$1800 + (ys>>3)*32 + col` |
| columna 32 | `$1B00 + ys` | `$1BC0 + (ys>>3)` |
| abajo | `$1D00 + k*32 + col` | `$1E00 + col` |
| abajo, col. 32 (esquina) | `$1E40 + k` | `$1E48` |

En los dos modos el atributo sigue a la línea / fila **de origen** (así el color viaja con su bitmap en el scroll fino vertical).

Con fino Y = 0 y col < 32 las direcciones son exactamente las de hoy: es la regresión que hay que comprobar. El dibujo del fino X (ventana de
16 bits y colores prestados) no cambia, solo que la columna siguiente de la 31 es ahora la 32 de verdad y no la 0. Los sprites van aparte y no se ven afectados.

### 3. Snapshots y depurador

✅ **Hecho en el emulador** (no en el depurador del firmware, ver arriba): `SnapGetScrollY` / `SnapRestoreScrollY` y la línea `SCROLLY %02X %02X %02X` en `snap.cpp` (se escribe siempre y se lee si está; un fichero antiguo sin ella da 0 / 0 / 23). Se dejó la línea `SCROLL` de 4 valores como estaba, para no romper la compatibilidad con el lector del firmware.

- `SnapGetScroll` / `SnapRestoreScroll` y la línea `SCROLL` de `snap.cpp`: añadir fino Y, fila superior, fila inferior (y el estado del motor cuando exista).
  La lectura tiene que aceptar la línea antigua de 4 valores (los nuevos, por defecto 0 / 0 / 23).
- `mcu/DEBUGGER.cpp` (`SCROLL %02X ×4`, `pkd(2090…)`, ~1581, ~2044, ~3636): los registros nuevos no son POKEs, se leen con los IN 17-19 del puerto. Si
  se quiere que el depurador y las instantáneas del hardware los incluyan, hay que ampliar la línea y el comando de la consola; si no, el efecto es que al
  restaurar una instantánea se pierde el fino Y.

### 4. Motor de movimiento horizontal (fase 2, ya escrito en la FPGA; la vertical aún no)

✅ **Hecho.** `ScrollMotorRun()` en `SD81Booster.cpp`, enganchado en `UpdatePlay` antes del blit del doble buffer; la orden (registro 9) queda pendiente y el ocupado (IN 16 = `$C0`) sube al instante; se ejecuta en el `UpdatePlay` del cuadro si `frametstates < 254 * tperscanline`, y si no, en el del siguiente. La cadena `ShiftChain` (en `scroll_addr.h`) mueve el bitmap y los atributos según el modo.

La FPGA lo hace en `scroll_motor.v` / `scroll_sync.v` / `sim_int.v` 0.20 (detalle en `scroll_hardware.md`, «Fase 2»). El emulador tiene que:

- **Registro 9** (`$40+1`): 1 = izquierda, 2 = derecha (3 y 4 reservadas: se aceptan y no hacen nada; 0 no hace nada). Una orden con el motor **ocupado se ignora**.
  Escribirla deja la orden **pendiente** y el ocupado sube al instante.
- **IN 16**: bit 7 ocupado (pendiente o en marcha), bit 6 pendiente, bit 5 en marcha.
- **Cuándo se ejecuta:** en el borrado vertical del cuadro, desde la línea 254 (como el blit); una orden dada después de esa línea espera al cuadro siguiente. En el
  emulador basta con hacerlo de golpe en ese punto del cuadro y mantener «en marcha» ~1,5 ms (en la FPGA son 0,8 ms con Spectrum y ~1,5 ms con HiRes y atributos
  de modo 0). Con el doble buffer AUTO el blit va después.
- **Dónde engancharlo en el código** (mirado el 26/10): el emulador pinta el cuadro entero al acabarlo (`g_sfastRenderFn`, `main_frame.cpp:4010`, con el estado de los
  registros y de la memoria de ese instante) y justo después llama a `SD81Booster::UpdatePlay(frametstates)` (`main_frame.cpp:4027`), que ya hace ahí el blit del doble buffer
  (`RefreshDbufFront()`, `SD81Booster.cpp:1120`). El motor va en `UpdatePlay`, **antes** del `RefreshDbufFront()`: se mueve después de pintar (como la FPGA, que lo hace tras el área
  visible) y el blit copia ya lo movido. Al escribir el registro 9 se mira `frametstates`: si es menor que `254 * machine.tperscanline` (207 por defecto) la orden se ejecuta en el
  `UpdatePlay` de este cuadro; si no, en el del siguiente. El ocupado es «orden sin ejecutar» (bit 6, y el 7); el bit 5 «en marcha» no se llega a ver, porque el movimiento es instantáneo
  al final del cuadro. Un bucle que espere el ocupado dentro del cuadro no se bloquea: el cuadro acaba, se ejecuta la orden y el siguiente ya la ve libre.
- **Qué mueve** (sobre `m_shadowMirror`, no sobre la RAM del Z80; tras el primer movimiento **ambas dejan de coincidir**), el bloque `HFILE[15:13]`, no el de delante:
  cadena `L m0 ... m31 R` por línea. **Izquierda:** `m_c <- m_{c+1}` para c = 0..30, `m31 <- R`, `L <- m0` (el byte que sale). **Derecha:** `m_c <- m_{c-1}` para c = 31..1, `m0 <- L`, `R <- m31`.
  Ir a la izquierda y volver a la derecha deja la memoria como estaba.
- **Qué cadenas** (offsets en el bloque; `y` = línea 0-191, `r` = fila de texto 0-23; solo las filas cuyo bit de `POKE 2091-2093` es 1):

  | Modo | bitmap: m_c / R / L | atributos: m_c / R / L |
  |---|---|---|
  | HiRes nativo | `y*32+c` / `$1800+y` / `$1C00+y` | modo 0 (color activo, `$7FEF` bit 5 = 1 y bit 4 = 0): `$C000+y*32+c` / `$C000+$1800+y` / `$C000+$1C00+y`; modo 1 (bit 4 = 1): `base+r*33+c` / `base+r*33+32` / **sin L** (al ir a la derecha `m0` no cambia). `base` = el de `chromaAttrAddrM1` para la fila 0, columna 0 (las direcciones dan la vuelta a 16 bits) |
  | Spectrum | `{y[7:6],y[2:0],y[5:3],c}` / `$1B00+y` / `$1C00+y` | `$1800+r*32+c` / `$1BC0+r` / `$1CC0+r` (siempre, haya o no color) |

  Sin efecto si no es HiRes ni Spectrum (texto, o sin Superfast). Con el color apagado en HiRes **no se mueven atributos**.
- Búferes de la izquierda y de arriba: ya listados arriba. Las órdenes 3 y 4 (vertical) llegarán con la fase 3.
- El protocolo del programa (orden con el fino en 7 o en 0; al acabar, fino a 0 o a 7 y relleno del búfer de entrada) lo ejercita `EXAMPLES/SCROLLM`; su arnés
  `claude/mqtttest/test_z80scrollm.cpp` tiene un modelo en C++ del motor y del ocupado que se puede portar tal cual.

### 5. Pruebas del emulador

✅ **Hecho la parte automática** (el cruce con las direcciones de la FPGA y la regresión, ver «Estado»). Los tres programas de prueba (`SCROLLT`, `SCROLLH`, `SCROLLM`) funcionan en el emulador; falta compararlos con la placa.

- Un cruce con la FPGA: sacar de `tb_lin_scroll_addr.v` / `tb_spec_scroll_addr.v` las direcciones para un conjunto de casos (fila, línea, col, fine_y, top, bot) y comparar con la función
  del emulador (o portar el modelo aritmético del testbench a C++ y compararlo con la función real del emulador; los testbench ya dan 1.165.824 casos cada uno).
- Cargar `SCROLLT.BIN` (Spectrum), `SCROLLH.BIN` (HiRes) y `SCROLLM.BIN` (el motor, HiRes continuo) en el emulador y comparar con lo que se ve en la placa (README de cada uno: «Qué se tiene que ver»).
- `claude/mqtttest/build_scrollt.sh` y `build_scrollh.sh` ya ejecutan los programas en el Z80 con el puerto simulado: sirven de modelo de las secuencias de escritura.

### 6. Manual y documentación

✅ El emulador queda igual que la FPGA de esta rama; el manual sigue en `manual_pendiente.md` (no es parte del emulador).

Anotado en `manual_pendiente.md` (sección «Scroll por hardware»). El emulador es una de las tres cosas que hay que mantener iguales (FPGA, emulador, manual).

### 7. Motor vertical (fase 3, escrito en la FPGA el 2026-10-26, probado en ModelSim; sin sintetizar ni probar en la placa)

Órdenes del registro 9: **3 = arriba** (el contenido sube: cada fila toma la de debajo, entra por abajo) y **4 = abajo** (el contenido baja, entra por arriba). Todo lo demás
(pendiente / ocupado, arranque en la línea 254, el bloque HFILE, sin efecto en texto ni sin Superfast, orden ignorada con el motor ocupado) es igual que en el horizontal.
La orden con la región vacía (`reg_top > min(reg_bot, 23)`) termina al instante sin mover nada. `reg_bot` mayor que 23 cuenta como 23. **La máscara de filas (POKE 2091-2093) no cuenta
en vertical**: solo la región (registros 10 y 11).

**Cadena de filas:** `T  fila_top ... fila_bot  B`, con `T` = búfer de arriba y `B` = búfer de abajo.

- Arriba: `T <- fila_top`, `fila_r <- fila_r+1` (r de top a bot-1), `fila_bot <- B`. El `B` queda como estaba (el Z80 lo rellena después); lo que sale se guarda en `T`.
- Abajo: `B <- fila_bot`, `fila_r <- fila_r-1` (r de bot a top+1), `fila_top <- T`. `T` queda como estaba; lo que sale se guarda en `B`.
- Cada «fila» es la fila de texto entera: 8 líneas de bitmap, en los 34 nodos `L, m0..m31, R` (los de los bufers de los lados viajan con su fila). En los atributos de Spectrum y de
  HiRes modo 1, una sola línea por fila; en HiRes modo 0, 8 líneas como el bitmap (área de `$C000`, bloque 6).

**Qué nodos no existen** (el movimiento no los toca, quedan como estaban):

| Par | `L` | `m0..m31` | `R` |
|---|---|---|---|
| con `T` (arriba: `T <- fila_top`; abajo: `fila_top <- T`) | no existe | `T` | no existe |
| con `B` (arriba: `fila_bot <- B`; abajo: `B <- fila_bot`) | no existe | `B` | **la esquina** (`$1A00+k` HiRes, `$1E40+k` Spectrum, atributo `$1E48`) |
| dos filas de la región | su fila | su fila | su fila |
| HiRes **modo 1** (atributos 8x8) | no hay `L` | `T` no existe: el par con `T` no hace nada. `B` = la **fila 24** de la rejilla (`base + 24*33`), aunque la región acabe antes | columna 32 de esa fila |

Direcciones de los búferes de arriba y de abajo (dentro del bloque de la pantalla; las mismas que ya usa el dibujo para `B` y las de `lin_scroll_addr.v` / `spec_scroll_addr.v` para `T`):

| | Bitmap `T` | Bitmap `B` | Atributo `T` | Atributo `B` |
|---|---|---|---|---|
| Spectrum | `$1F00 + k*32 + col` | `$1D00 + k*32 + col` | `$1E20 + col` | `$1E00 + col` |
| HiRes | `$1D00 + k*32 + col` | `$1900 + k*32 + col` | modo 0: lo mismo en `$C000+` | modo 0: lo mismo en `$C000+` |

`k` va de 0 a 7 (el dibujo solo usa 0-6 de `B`). Las esquinas: HiRes `$1A00 + k`, Spectrum `$1E40 + k` y atributo `$1E48`.

**Orden de recorrido (importa solo si el emulador mueve en sitio):** por pares `(fila r-1, fila r)`, de `r = top` hasta `r = bot+1` si es arriba y al revés si es abajo; en cada par primero se
leen los 34 nodos de las 8 líneas y luego se escriben (el modelo de `tb_scroll_motor.v` coge una instantánea de la cadena y la aplica entera, que da el mismo resultado).

**Duración** (a 26 MHz, 3 ciclos por byte, región completa): Spectrum 22.926 ciclos = 0,88 ms; HiRes sin color 20.378 = 0,78 ms; HiRes modo 1 22.821 = 0,88 ms; HiRes modo 0 40.755 = 1,57 ms.

**Protocolo desde el Z80 (sin saltos), igual que en horizontal:** arriba: mandar la orden 3 en el cuadro en que el fino Y llega a 7, dejarlo, y en el siguiente (libre) ponerlo a 0 y rellenar `B` (y la
esquina, y la fila 24 en modo 1); abajo: mandar la orden 4 cuando el fino Y llega a 0, y en el siguiente ponerlo a 7 y rellenar `T`. Después de un movimiento vertical, las **columnas `R` y `L` de la fila que entra**
están como estaban (en `B`-`R` entra la esquina; en `T` no hay ni `R` ni `L`): el Z80 tiene que escribirlas si el scroll horizontal las va a mostrar.

Pruebas en el PC: `tb_scroll_motor.v` (128 casos verticales contra un modelo: 4 tipos, 2 sentidos, 8 regiones, con y sin paradas; con 4 errores puestos a propósito falla) y `tb_dbg.v` (orden 3 y 4 por el puerto,
región por los registros 10 y 11).

## Observaciones que NO se implementan todavía (por decisión del usuario, a comprobar en la placa)

- **Texto Superfast, fino X y Chroma modo 1:** `scr_col2` es de 5 bits, así que el atributo de la columna 33 (el hueco del NEWLINE) parece dar la vuelta a la columna 0, mientras el
  carácter sí lee la 32 real (`sf_col_idx`, 6 bits). Visto solo en el código. Si se confirma en la placa, el emulador debería reproducir lo que haga la FPGA y no lo que
  «debería» hacer.
- **Spectrum con `color_mode = 1`:** la FPGA lee entonces los atributos por posición del DFILE y no los del Spectrum; el emulador (`IsChromaAttributeFileMode()`) ya hace lo mismo. Solo es coherente con Chroma modo 0.
- **Atributos de HiRes y doble buffer:** ni el modo 0 ni el 1 pasan por el doble buffer (solo el bitmap cambia de página). El emulador ya lo hace así.
