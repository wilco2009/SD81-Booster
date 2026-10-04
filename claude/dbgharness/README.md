# Bancos de pruebas del depurador (en el PC)

Prueban el depurador por hardware sin el SD81 Booster, antes de llevarlo
al hardware. Ver `claude/planning/hw_debugger_plan.md`.

```
sh build.sh
```

Necesitan `g++`/`gcc` (MinGW), `pasmo` y los fuentes del núcleo Z80 de
EightyOne (`/c/ClaudeCode/Eightyone2/src`, o la ruta que diga la variable
`Z80SRC`). Compilan en `build/`.

## `logic.cpp`: la lógica del MCU

`Arduino/SD81BoosterV2_039_STM32/DEBUGGER.cpp` contra un Z80 de mentira que
habla el protocolo `DBG_BREAK` / `DBG_POLL` byte a byte. El programa son
instrucciones de un byte; los breakpoints `FF` y los pasos rompen como lo
haría la FPGA. Comprueba:
- los breakpoints por software: se ponen al continuar y se quitan al parar;
- el paso para saltar el breakpoint del PC y volver a ponerlo;
- los pasos;
- el `RST 38h` que no es un breakpoint (se emula al continuar);
- cambiar registros;
- un reset con un breakpoint puesto;
- las órdenes `m`, `d`, `e` e `io`.

## `cosim.cpp`: el monitor de verdad y el MCU de verdad

Cosimulación:
- **El monitor de verdad** (`z80rom/debugmon.asm`, ensamblado) corre en el
  núcleo Z80 de EightyOne.
- **`DEBUGGER.cpp`** corre en otro hilo. Se hablan por los puertos `$A7` y
  `$AF` con el mismo protocolo de reloj que en el hardware.
- **La FPGA** se modela por instrucción: la ruptura es el `RST` más el
  `CALL $2000` con la página 63 en el bloque 1, y la salida en `$003B` es el
  epílogo. También están la protección del bloque 0 y de la página 63, el
  mapper y el puerto `$3FEF`.

El programa de prueba (`prog.asm`, en `$6000`) carga todos los registros,
comprueba en un bucle que R avanza siempre 11 entre dos `LD A,R` (aunque se
pare por medio) y usa la pila. El banco lo para varias veces y comprueba:
- que el bucle no se rompe;
- los pasos;
- que `m` y `e` en `$2000` van a la página del programa y no a la del
  monitor;
- un breakpoint y su vuelta;
- que, al sacarlo de un bucle con `x pc=…` y `x hl=…`, todos los registros,
  los alternativos y SP llegan intactos (y HL cambiado).
- los snapshots (`snap`): con el programa parado, solo las páginas mapeadas;
  con `-a` (y FULL_PAGING), todas; y con el programa en marcha y el nombre
  automático (`NONAME001.Z81`), que para, graba y sigue. Lee cada `.Z81` y
  lo compara con la memoria emulada: `[CPU]`, `[MEMORY]`, `MAPPER`,
  `HW_POKES`, `DISPLAY_MODE` y cada `RAM_PAGE` (sin la página 0 si sigue
  siendo la ROM, ni la 63). La "SD" son ficheros `sd_*.Z81` en `build/`, y
  la BRAM de sombra del puerto `$3FEF` (índice 2) copia lo que escribe la
  CPU fuera del monitor.
  También los AY (las claves `AY1`/`AY3` y su registro elegido), los
  sprites (`SPRITE`, de su copia en la sombra), `SHADOW` y el estado del MCU
  (un `FILE_HANDLE` de mentira).
- la carga (`LOAD *Z81` con el monitor): desordena memoria, mapper, sombra,
  POKEs, AY y sprites, carga `prueba1` y comprueba que todo vuelve a como
  estaba (registros con R, paginas, la sombra entera, los POKEs y su orden,
  los AY, los sprites); lo mismo con `NMI 01`; y un `.Z81` estilo EightyOne
  sin `MAPPER` ni `HW_POKES`.
- las páginas escritas: una página no mapeada que escribe el programa (la
  20) entra en el snapshot normal y vuelve con la carga.
- el botón QS (pulsado 1,5 s de verdad: `millis` es el reloj del PC) y el
  teclado del ZX81 en la pausa: `Z` graba y se queda parado, `L` vuelve a
  cargarlo (tras cambiar HL y la memoria con la consola), `S` graba y sigue,
  espacio sigue.
- SLOW: con una NMI cada 40 instrucciones (una rutina en `$0066` que cuenta
  en A'), la pausa para en la NMI y el MCU la deshace; al seguir vuelve por
  `OUT ($FE)`/`RET`; un breakpoint en el PC.
- la fase 3: `o` sobre un `CALL`, `s` + `u`, `g`, un punto de vigilancia de
  escritura y `o` con el comparador ocupado (con un `FF` temporal).
- `v`: el 2045 a 170 con la orden 10, los pasos la mantienen, al quitarla
  (con `v` o al seguir) el 2045 y su sombra como estaban y el D_FILE
  alternativo otra vez; con un programa ya Superfast no hace nada.
- `v dir`: alineada (HFILE = dir, 2045 a 171, sin tocar la sombra) y sin
  alinear (el mapa de bits copiado al bloque y, al quitarla con `v` o al
  seguir, la sombra exactamente como estaba, HFILE y 2045 a 0 / 85); `v 1234`
  y `v F000` no se aceptan.
- `ui`, la pantalla del depurador:
  - 2045 a 174, el D_FILE alternativo en `$1000`, el monitor esperando con
    I = `$1E` (SETI) y el Chroma sin color;
  - el título, los registros y las teclas en la sombra;
  - un paso y las teclas S, 6 y Q;
  - al seguir se quita y al parar vuelve;
  - al quitarla, la sombra exactamente como estaba, el color y la I del
    programa;
  - las teclas: `B` ENTER y `B` dir, `R P`, `E` con bytes, `M`, `D`, `W W`
    y `W` ENTER, `G` (y la pantalla otra vez al llegar), `V` ida y vuelta
    y `Q`.
- la pausa del botón QS (prueba 11): con la pantalla del depurador; `Z`, `L`
  y espacio desde ella; `V` a la del programa (ahí `S` es snapshot y sigue)
  y `V` otra vez a la del depurador.
- `sym`:
  - un `.SYM` de pasmo (también con `=` y `:`), cargado a mano y al cargar
    un programa;
  - etiquetas y operandos en `d`;
  - `b`, `bc` y `x pc=` con nombres;
  - la parada con `(LOOP)` y `(SUB1+2)`;
  - otro programa sin `.SYM` los quita.
- `t`, la traza lenta: `t 4` (CALL, LD A, RET, LD HL) y `th` con los
  registros; `t` hasta un breakpoint sin ejecutarlo; `t` sin fin, parada por
  una orden de la consola; `T` y `H` en la pantalla.
- el historial (la traza de la FPGA, modelada en `step_cpu` con la orden
  12 y los índices 7/8): `tron`, `th 5` tras correr a velocidad real, un
  snapshot sin `SHADOW 00`, `troff`; `H` en la pantalla tras un paso.
- La pantalla del depurador está en `$0000` (`UIDF`).
- la web (`CMD_DBG`): órdenes con número (`web()`, como las del ESP32), la
  salida por `dbg_out_read` desde donde se iba, un reintento que no se
  repite y el estado.
