# Sprites por hoja (formato B): decisiones y mediciones

Rama `fpga-sprites-hoja`. Complementa `sprites_hoja_traspaso.md` (encargo, no commiteado, en el repo principal).

## Decisiones del usuario (2026-11-06)

1. **Puntero de 1 byte**: página de 32 B dentro de un bloque de 8 KB con base en un registro (hoja alineada). 7 pasos por sprite cargado.
2. **Se mantiene la ventana `POKE 2100/2101..`** y la tabla de la sombra (`$0C00` / `$1800`, 32 B por sprite) con campos nuevos.
3. **Negro opaco = `pal16[8]`** (el gris oscuro, negro con brillo; corregido por el usuario el 2026-11-06, antes `pal16[1]`). Se reprograma a negro con la paleta de 16 activa; el azul (índice 1) se queda como está. El 0 es transparente.
4. **B sustituye a A** en esta rama; A no convive. Los juegos no funcionan aquí hasta convertirlos.
5. K = 12 no se baja sin consultar (si la medición dice que no cabe, se vuelve a preguntar con los números).
6. **Las hojas viven en la BRAM** (la sombra de 64 K x 8, que espeja lo que escribe el Z80). Decidido el 2026-11-06 tras estudiar leer la SRAM: la FPGA gobierna las 19 líneas de dirección de la SRAM pero el dato solo llega por el bus del Z80, la SRAM es de un solo puerto y no se ha confirmado que `nWAIT` llegue al Z80. Descartado de momento; queda como línea aparte (copiar una página de SRAM a un bloque de la BRAM en el blanking, o leer SRAM con WAIT).

## Consecuencias de leer de la BRAM

- Los gráficos tienen que estar en direcciones del Z80 donde la CPU haya escrito (o cargarse por `BRAMW` del MCU); cambiar `sram_page` no cambia lo que ve el motor.
- Registro base de 3 bits (bloque de 8 KB de los 64 KB de la sombra) + página de 1 byte por sprite: un POKE al base cambia la hoja de todos los sprites a la vez. Propuesta pendiente de confirmar: latchear el base en el VSYNC para que no se parta el fotograma.
- A comprobar antes de convertir juegos: que `LOAD *SPRITE`, los snapshots y la carga por el MCU dejen la hoja también en la BRAM.
- El área no depende del número de sprites (la tabla está en BRAM); lo que limita es el tiempo de escaneo (1 paso por sprite). 64 sprites de momento; tabla de cabecera compacta posible.

## Fase 0: área medida del motor actual (A, K=12, 64 sprites)

Síntesis XST con `-keep_hierarchy Yes` de master 5db1c6d en la VM (`~/hier_spr`), informe con `fpga_hier_report.py`.

| Módulo | LUTs | FFs |
|---|---:|---:|
| `sprite_engine` | 738 | 478 |
| `sim_int` | 391 | 218 |
| `mode160` | 364 | 230 |
| `ay_3_8192` | 358 | 292 |
| `scroll_motor` | 314 | 84 |
| `SD81` (resto, incluye los 64 FFs de `spr_en`) | 1950 | 738 |

Total XST jerárquico: 4888 LUTs (85 %), 2433 registros. Es una cota alta: sin jerarquía XST optimiza entre módulos y el MAP final da ~70 % de LUTs. Lo útil es el reparto: el motor son ~738 LUTs, ~55 por slot de línea más la máquina de carga y el escaneo de los 64 sprites.

## Fase 1: FPGA con el formato B (2026-11-06)

Hecho en la rama (sin commitear):
- `sprite_engine.v` reescrito: cabecera de 5 campos (activo, X bajo, X alto, Y, **página**), hoja en la sombra en `{sheet_base, página, fila, byte}`, 4 bytes por fila (nibble alto = píxel de la izquierda), índice 0 transparente, 7 pasos por sprite cargado, slots de 41 bits (X de 9 + 32 de píxeles). Salida `hit` + `pix[3:0]`.
- `SD81.v`: `POKE 2105` = página (campo 4 de la ventana, antes color de la fila 0); `POKE 2129` = bloque de 8 KB de las hojas (0-7), latcheado en el VSYNC (`spr_base`), por defecto 2; el índice del sprite hace de tinta y papel (`{pix,pix}`) para que cada píxel lleve su color en los modos normales; `m160_spr = sf160_en` (sprites también en el 8 bpp).
- `mode160.v`: `spr_y` del 8 bpp = fila de la imagen / 2 + 32 (100 filas); `spr8` elige `pal16` por píxel donde hay sprite en el 8 bpp.
- Bancos de prueba: `tb_sprite_engine.v` (+ `.sh`) contra una especificación (K más altos, prioridad, transparencia, hojas aleatorias en bloques 2-7 con páginas compartidas, caso peor); `tb_mode160_spr.v` con `-gM4=1` (4 bpp) y `-gM4=0` (8 bpp). Todo pasa en ModelSim.

Medido (VM de ISE, flujo completo XST+MAP+PAR, mismo método que master):

| | master (build del 1-nov) | formato B, K=12 |
|---|---:|---:|
| Slice LUTs | 4.037 (70 %) | 4.168 (72 %) |
| Slices ocupados | 1.320 (92 %) | 1.325 (92 %) |
| Registros | 2.607 | 2.726 |
| BRAM | 32/32 | 32/32 |
| Temporización | cumplida | cumplida (holgura >= 3 ns en el reloj de 208 MHz, ~30 ns en el del Z80) |

Es decir, **K=12 cabe**: +131 LUTs y +5 slices. En XST jerárquico el motor pasa de 738 a 885 LUTs y de 478 a 601 registros.

Tiempo (simulado): el caso peor de 80 columnas (12 sprites de menor índice, 64 activos) necesita `limit` >= ~175; con 195 hay ~20 pasos de margen. En 32 columnas pasa con 120.

Cosas a tener en cuenta:
- En los modos normales sin `pal16` el índice del sprite es el código `{bright,G,R,B}` del color, y **el negro opaco no existe** (el índice 0 es transparente y el 8 es BRIGHT solo, gris oscuro). El negro opaco = `pal16[8]` reprogramado a negro exige activar la paleta de 16 (`POKE 2067,1`) en esos modos.
- Falta probar `SD81.v` completo en simulación (no hay banco de pruebas del top): la integración está validada por la síntesis y por los bancos del motor y de `mode160`.
- `sprite_slot.v` y `sprite_array_demo.v` siguen siendo del formato A; ya no tienen referencia en el banco de pruebas.

## Fase 2: software de base (2026-11-06, sin commitear)

Hecho:
- **ROM** (`z80rom/extracmdcode.inc.asm`, `extracmdlist.inc.asm`): fuera `LOAD *SPRCOL`, `*SPRPIX`, `*SPRMASK`; dentro `LOAD *SPRPAGE <n>,<página>` (POKE 2100 + 2105) y `LOAD *SPRBASE <b>` (POKE 2129, 0-7). `LOAD *SPRITE` igual. El borrado de los 64 sprites del arranque (`sdhandler.inc.asm`, 28 campos) sigue valiendo. Ensambla con pasmo en el scratchpad (12.257 bytes, 22 menos que antes). **No** se ha generado ni grabado ninguna ROM.
- **MCU** (`Arduino/SD81BoosterV2_039_STM32/DEBUGGER.cpp`; **sin compilar**, lo compila el usuario): snapshot guarda y carga `SPRITE_BASE` (POKE 2129, leído de la BRAM; no se escribe si sigue con el byte de la ROM); el campo 4 de `SPRITE n ...` es ahora la página (el formato del fichero no cambia, el resto de campos van a 0); `shadow0_is_rom` ignora también 2129; la captura de pantalla del depurador lee 2129 y añade la hoja (lo que quepa de 8 KB; `CAP_NRG` 8 -> 9) y pone el bloque en el byte 19 de la cabecera del documento.
- **Web del depurador** (`Arduino/Wifi_module_01/DEBUG_PAGE.h`): pinta sprites de 4 bits desde la hoja (sin `pal16`).
- **SPRDBUF** (`EXAMPLES/SPRDBUF`): hoja de 16 celdas en `$7C00` (bloque 3), `pal16[8]` a negro, `POKE 2067,1`; ensambla con pasmo (684 bytes). **Sin probar** (ni en emulador ni en placa).
- `claude/planning/manual_pendiente.md`: punto nuevo con todo lo que el manual tendrá que decir (no se ha tocado ningún `.docx`).

Pendiente de la fase 2:
- **Emulador** (repo `C:\ClaudeCode\Eightyone2`, rama `main` con cambios sin commitear del usuario: usar una worktree propia): dibujar el formato B en `SD81Booster.cpp`/`zx81.cpp`, `snap.cpp`/`DEBUGGER.cpp` (claves `SPRITE_BASE`/`SPRITE`), `COMMANDS.cpp`/`DEBUG_PAGE.h` (copias de los ficheros del MCU y del ESP32), `accdraw.cpp`. Hasta entonces SPRDBUF no se puede ver en el emulador.
- Ejemplos y juegos que usan el formato antiguo (GALAXIAN, SD81TEST, MODE160, SCROLLM, NETGAME: pong, sports, scrabble): los convierte el usuario.
- `sprite_slot.v` / `sprite_array_demo.v`: del formato A, ya sin uso real.
