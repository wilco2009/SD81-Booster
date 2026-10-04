# Plan: depurador por hardware del SD81 Booster

Decidido el 1 de octubre de 2026. Se apoya en las interrupciones simuladas
(`FPGA/SD81V2.1000/sim_int.v`, rev 0.05, probadas en el hardware). La
inyección en límites de instrucción, el `CALL` en `$0038` y el epílogo en
`$003B` son la base del depurador.

## 1. Decisiones

| # | Tema | Decisión | Motivo |
|---|---|---|---|
| 1 | Ventana del monitor | **Bloque 1** (`$2000`-`$3FFF`), fijo | El bloque 0 tiene la NMI (`$0066`) y la rutina de vídeo de la ROM (impediría el SLOW), está protegido contra escritura, sus escrituras van a la fuente de la BRAM, y en CP/M es la página cero del programa |
| 2 | Página del monitor | **63, fija** (la impone el cableado de la carga, §6) | En paginación simple (por defecto) el Z80 no llega a las páginas 32–63. Conflictos: el disco RAM de CP/M 3 (15–63) y el test de memoria de SD81TEST (ver §8) |
| 3 | Carga del monitor | **El MCU escribe `/SYS/DEBUG.BIN` al encender**, justo después de la ROM y sin soltar el reset del Z80, en `$E000`-`$FFFF`. Con la orden 9 del canal de configuración ("monitor cargado"), la FPGA conduce durante el reset A16x–A18x = A13x·A14x·A15x: el bloque 7 del MCU va a la página 63. El mismo bit **arma** el depurador. Siempre cargado, si el fichero existe | No toca la ROM, no necesita buffer y sobrevive a los resets (los registros de configuración no se borran con el reset del Z80). El snapshot y la pausa están siempre disponibles |
| 4 | Breakpoints | **Un comparador** en la FPGA (ejecución, lectura, escritura o E/S) y **breakpoints por software sin límite** (`FF` en memoria), en RAM y en ROM | El software no puede hacer puntos de vigilancia ni sobrevivir a un `LOAD` o a código que se reescribe; para lo demás bastan los de software |
| 5 | Alcance de la v1 | **FAST y Superfast.** En SLOW, la ruptura se aplaza hasta que el programa pase a FAST | Con el vídeo nativo, la NMI y la ROM imponen restricciones serias (AF', IX, EI): fase 4 |
| 6 | Convivencia con las interrupciones simuladas | **Anidado:** el depurador tiene su propia máquina de estados y puede entrar dentro de la rutina de interrupción | Un breakpoint por software dentro de la rutina sería un `RST 38h` real sin atender y colgaría el Z80 |
| 7 | Contadores de prueba del detector | **Fuera.** Ninguna lógica de la FPGA solo para pruebas | La FPGA va al 95 % de slices y al 100 % de BRAM |
| 8 | Entradas al depurador | Consola USB, **pulsación larga (1 s) del botón QuickSilva**, joystick (arriba y abajo a la vez, necesita adaptación del hardware), trampa `OUT` a `$3FEF` en el programa, breakpoints y paso, y el cargador | |
| 9 | Sin PC conectado | El programa queda **parado** con el LED en magenta; al conectar el USB, la consola enseña dónde está; otra pulsación larga del botón **continúa** | Permite congelar un juego y conectar el PC después |
| 10 | Monitor y MCU | **Monitor Z80 mínimo** con dos comandos nuevos, `DBG_BREAK` y `DBG_POLL` (bloqueante). Toda la lógica, en el MCU | El monitor se queda en 1–2 KB; las mejoras van en el firmware, que es C y se actualiza desde la SD |
| 11 | Traza e interfaz en la pantalla del ZX81 | **Se deciden en la fase 3**, con la síntesis de la v1 delante | Compiten por los 7 KB libres de la BRAM (`$0000`-`$1BFF`); la traza cuesta lógica en la FPGA |
| 12 | Snapshots | **Fase 2b**, sobre el mismo mecanismo: el monitor lee y el MCU escribe el `.Z81`; lectura de la BRAM de sombra para el estado de los POKEs; `LOAD *Z81` ampliado. **Disparo:** mantener el botón QuickSilva 3 s, o `snap [fichero]` en la consola. **Nombre automático:** el del último fichero cargado con un número (`MAZOGS001.Z81`), o `NONAME001.Z81` si no se ha cargado nada. Qué páginas se guardan, por decidir en la fase 2b | El depurador ya para el programa y lee registros y memoria |

## 2. Arquitectura

```
 PC (terminal USB)          botón QS (1 s)      joystick (arriba+abajo)
        │                        │                       │
        ▼                        ▼                       │
  ┌──────────── MCU (STM32) ──────────────┐              │
  │ consola, desensamblador, tabla de     │              │
  │ breakpoints por software, símbolos    │              │
  └──┬───────────────────────────┬────────┘              │
     │ $A7/$AF                   │ canal de config,      │
     │ DBG_BREAK / DBG_POLL      │ orden 8 (pausa)       │
     ▼                           ▼                       ▼
  ┌ Monitor Z80 ┐         ┌──────────── FPGA ─────────────────────┐
  │ página 63,  │◄────────│ motor de ruptura: pausa, comparador,  │
  │ en el bloque│  ventana│ paso N, FF de memoria; inyección FF,  │
  │ 1 ($2000)   │         │ CALL $2000, epílogo; cambio de página │
  └─────────────┘         └───────────────────────────────────────┘
```

## 3. FPGA

### 3.1 Máquina de estados del depurador

Es independiente de la de `sim_int` y tiene prioridad sobre ella. Mientras
está activa, es la dueña de `$0038`-`$003E` y el estado de `sim_int` queda
congelado. Basta con dos niveles: una ruptura empieza y acaba entre dos
instrucciones del programa.

**Fuentes de ruptura.** Solo con el depurador **armado**, es decir, con el
monitor cargado (lo arma el MCU, §6). Sin armar, la FPGA ignora todas estas
fuentes.
- **Pausa pendiente**, puesta por:
  - la orden 8 del canal de configuración del MCU (consola o botón
    QuickSilva). Va por conmutación: el MCU cambia el bit y la FPGA detecta
    el cambio en el dominio de `iclock`;
  - el joystick, con arriba y abajo a la vez, sincronizado;
  - la trampa: un `OUT` a `$3FEF` con la orden "pausa".
- **Comparador** (dirección de 16 bits y modo):
  - ejecución: la M1 que empieza instrucción en esa dirección;
  - lectura, escritura o E/S: el acceso marca una ruptura pendiente para el
    siguiente límite de instrucción, justo detrás de la instrucción culpable.
- **Contador N** (16 bits, descendente, no se puede leer): "romper tras N
  instrucciones". El paso a paso es N = 1. **Cuenta solo las instrucciones
  del nivel donde se arrancó:**
  - si se arranca en el programa principal, una interrupción simulada se
    ejecuta entera sin romper;
  - si se arranca dentro de la rutina de interrupción, avanza por ella y
    después por el programa principal.
- **Breakpoint por software:** una M1 en un límite de instrucción que lee
  `FF` de la memoria, sin que lo haya inyectado la FPGA. Es un `RST 38h` real:
  la CPU ya guarda X+1 y salta a `$0038`, y la FPGA sirve allí el `CALL` al
  monitor. Es la misma comparación que la del HALT.

**Dónde no se rompe nunca:**
- en las M1 que sirve la FPGA (`$0038`-`$003E` durante una secuencia);
- dentro del monitor;
- en la M1 de X al continuar: la instrucción en la que se paró se ejecuta
  sin volver a entrar. Hay que definir bien este "saltarse una" cuando en
  esa M1 entra una interrupción simulada; se comprobará en el banco de
  pruebas;
- con el generador de NMI encendido (SLOW, v1). La FPGA lo sabe porque ve
  los `OUT` del Z80: `A0=0` (`$FE`) lo enciende y `A1=0` (`$FD`) lo apaga.
  Cuesta un biestable.

**Secuencia de entrada:**
1. Si la ruptura es por inyección, en la M1 de X se sirve `FF`. Se decide
   en la subida de T2 con el opcode real, como en `sim_int` 0.05. La CPU
   guarda X+1 y va a `$0038`.
2. En `$0038` se sirve `CD 00 20` (`CALL $2000`). La CPU guarda `$003B`
   con dos escrituras **todavía en la página del programa**.
3. **Después de la segunda escritura del `CALL`**, la FPGA guarda la página
   del bloque 1 en un registro propio y pone la página del monitor. Así la
   primera M1 en `$2000` ya lee del monitor, aunque la pila del programa
   esté en el bloque 1.

**Mientras el monitor está activo:**
- el Z80 puede escribir en la SRAM del bloque 0, para los breakpoints por
  software en la ROM. Es un término más en la condición que ahora protege
  el bloque 0;
- las escrituras no se copian a la BRAM de vídeo. La pantalla del programa,
  la fuente del bloque 0 y la RAM de caracteres del bloque 1 quedan
  intactas;
- no se entregan interrupciones simuladas. Los VSYNC quedan en uno
  pendiente, que se entrega al continuar. FRAMES sigue corriendo.

**Salida:**
1. El monitor repone los registros y deja SP como estaba **antes del
   `CALL`**, es decir, sin el `$003B` que dejó el `CALL`: el epílogo tiene
   que encontrar X+1 arriba (en las interrupciones simuladas lo quita el
   `RET` de la rutina). Sale con **`JP $003B`**, no con `RET`, así que no lee
   la pila del programa con la ventana puesta.
2. En la M1 de `$003B` se sirve `E3`. Al acabar esa M1, y antes de que
   `EX (SP),HL` lea la pila, se repone la página del programa en el bloque 1.
3. Se sirven `2B E3 C9` en `$003C`-`$003E`, como en `sim_int`, y se vuelve
   a X.

**Canal de configuración del MCU** (registros en el dominio de `CFG_CLK`,
que no se borran con el reset del Z80):
- **orden 8:** pausa, por conmutación. El MCU cambia el bit y la FPGA
  detecta el cambio en el dominio de `iclock`;
- **orden 9**, un bit: **"monitor cargado"**. Se manda una sola vez y se
  queda puesta:
  - **con el Z80 en reset**, cuando el MCU escribe la SRAM, la FPGA conduce
    las líneas altas que ahora deja en alta impedancia (`SD81.v`, línea
    2006): A16x = A17x = A18x = A13x·A14x·A15x. Así las escrituras del MCU
    en `$E000`-`$FFFF` van a la página 63, y las demás no cambian, de modo
    que da igual el orden o una recarga posterior de la ROM. Esas escrituras
    tampoco se copian a la BRAM. El mapper no se toca;
  - **con el Z80 en marcha**, el depurador está armado.

### 3.2 Puerto `$3FEF`

Cerrado en la fase 1 (`sim_int.v` 0.06). Se decodifican los 16 bits de la
dirección: `OUT (C)` / `IN (C)` con BC = `$3FEF`.

| OUT | Qué hace |
|---|---|
| `$00`-`$0F` | elige lo que devuelve IN |
| `$10` | trampa: pausa en la instrucción siguiente |
| `$80`+r | el OUT siguiente es el dato del registro r |

| r | Registro |
|---|---|
| 0 / 1 | comparador: dirección baja / alta |
| 2 | modo del comparador: 0 apagado, 1 ejecución, 2 lectura, 3 escritura, 4 E/S (byte bajo del puerto) |
| 3 / 4 | N baja / alta: romper tras N instrucciones (0 = no). Se carga al escribir la alta |

| IN (índice) | Qué devuelve |
|---|---|
| 0 | estado: bit 7 armado, bit 6 NMI encendida, bit 5 nivel (1 = dentro de la rutina de interrupción), bits 2-0 motivo (1 MCU, 2 joystick, 3 trampa, 4 paso, 5 comparador de ejecución, 6 punto de vigilancia, 7 `FF` de memoria) |
| 1 | interrupciones simuladas: activas, pendiente, arm, Superfast, halted, call_now, fase (2 bits) |
| 15 | firma `52h` |

El armado no va aquí: lo pone el MCU con la orden 9. La página del monitor
es fija (63). La página del programa en el bloque 1 no hace falta
guardarla: la ventana se superpone en el multiplexor de dirección de la
SRAM sin tocar el mapper, así que el monitor la lee con `IN` del puerto
`$E7` (B = 1).

**Se quitan del detector:** los contadores de M1, de instrucciones y de
`DD`/`FD CB`, sus copias congeladas, el último opcode, el estado legible, el
multiplexor de lectura de 16 entradas y el contador de interrupciones
entregadas (índices 9/10). El contador de instrucciones pasa a ser el
contador N del depurador.

### 3.3 Coste estimado

| | Biestables | LUTs |
|---|---|---|
| Se quita | ~125 | ~100 |
| Se añade: máquina de estados, comparador, N, página guardada, pausa, NMI y puerto | ~90 | ~80 |

Son cuentas a ojo; lo decide la síntesis. Si no cabe, lo primero que se
recorta es el modo E/S del comparador y el joystick.

## 4. Monitor Z80 (`DEBUG.BIN`)

- Vive en la **página 63**, que se ve en `$2000`-`$3FFF` cuando la FPGA la
  pone en el bloque 1.
- `$2000`: `JP entrada`. `$2003`: la firma (por ejemplo `"SD81DBG"` y la
  versión), para comprobar después de un reset que sigue cargado.
- **Al entrar:**
  1. guarda todos los registros en su página: AF, BC, DE, HL, IX, IY, los
     alternativos, I, R e IFF2 (con `LD A,I`);
  2. calcula el PC (X = el valor guardado en `[SP+2]` menos 1) y el SP del
     programa;
  3. cambia a su propia pila;
  4. envía `DBG_BREAK`.
- **Bucle:** `DBG_POLL`, ejecutar la petición, repetir.
- **Lecturas y escrituras en `$2000`-`$3FFF`:** se redirigen a la página
  guardada. La mapea un momento en otro bloque y deja ese bloque como
  estaba.
- **Restricción desde la v1:** no modificar IX, IY ni I (los guarda y los
  repone sin usarlos), para que el SLOW de la fase 4 no obligue a
  reescribirlo.
- **Sale con `JP $003B`**, con SP = el del programa + 2 (sin el `$003B` del
  `CALL`).
- **Tamaño previsto:** 1–2 KB, más la pila, la zona de registros y un buffer
  de 256 bytes.

## 5. MCU

- **Comandos nuevos** (subir `LAST_COMMAND` en `COMMANDS.h`):
  - **`DBG_BREAK`:** el monitor envía el motivo, los registros, el PC, el SP,
    la página guardada y el estado de la FPGA;
  - **`DBG_POLL`:** el MCU no contesta hasta tener una petición. Queda como
    estado pendiente del bucle principal, sin bloquear el sonido ni el
    ESP32.
- **Peticiones:**
  - leer y escribir memoria, por bloques de hasta 256 bytes;
  - escribir el bloque de registros;
  - programar la FPGA (comparador, N, página);
  - continuar;
  - dar N pasos.
- **Lógica en el MCU:**
  - tabla de breakpoints por software, y el baile al continuar desde uno:
    reponer el byte original, dar un paso, volver a poner `FF` y continuar;
  - un `RST 38h` real del programa: si X no está en la tabla, se enseña como
    tal;
  - paso por encima de un `CALL` y "ejecutar hasta aquí", con un breakpoint
    temporal por software (la longitud de la instrucción la da el
    desensamblador);
  - desensamblado con `z80-disassembler.cpp`, que ya existe;
  - más adelante, los símbolos de los `.sym` de pasmo.
- **Botón QuickSilva:** una máquina de estados que no bloquea. Sustituye al
  `while (!digitalRead(QSPIN)) delay(100)` actual, que bloquea el bucle:
  - **menos de 1 s y soltar:** cambia QuickSilva. Ahora cambia al soltar, no
    al pulsar;
  - **a 1 s:** manda la pausa con la orden 8 y el LED se pone magenta. Si se
    suelta aquí, el programa queda parado para el depurador. Si ya estaba
    parado, una pulsación de 1 s continúa;
  - **a 3 s** (fase 2b): el LED se pone amarillo. Al soltar se graba un
    snapshot (el LED parpadea mientras graba) y el programa continúa solo;
  - nunca hace más de una cosa a la vez.
- **LED STAT:** magenta mientras el programa está parado en el monitor;
  amarillo al pasar de 3 s; parpadeo mientras se graba un snapshot.
- **Consola USB (v1):** `r` (registros), `x reg=valor`, `s [n]` (pasos),
  `o` (pasar por encima), `c` (continuar), `p` (pausa), `b addr` / `bc` /
  `bl` (breakpoints), `w addr` / `wr` / `ww` / `wi` (comparador: ejecución,
  lectura, escritura o E/S), `m addr [n]` (volcado), `e addr bytes`
  (escribir), `d addr [n]` (desensamblar), `u addr` (ejecutar hasta),
  `snap [fichero]` (snapshot, fase 2b), `dbg reload` (vuelve a escribir
  `DEBUG.BIN` con el Z80 en reset, para desarrollar el monitor). Al parar,
  enseña los registros y el desensamblado alrededor del PC.
- **Sin terminal:** el programa sigue parado, con el LED en magenta. Al
  conectar el terminal, se enseña la ruptura. Para saber si hay un terminal
  abierto se mira la señal DTR del USB CDC, si STM32duino la da (ver §9).

## 6. Carga del monitor al encender (MCU)

En el arranque del firmware, justo después de `load_ROM("/SYS/SDBOOST.ROM")`
y con el Z80 todavía en reset:

1. Si no existe `/SYS/DEBUG.BIN`, no se hace nada: el depurador queda
   desarmado y todo funciona como ahora.
2. Orden 9 = 1 ("monitor cargado").
3. Sin soltar el reset, `write_sram(0xE000 + i, …)` del fichero (8 KB como
   máximo). Las escrituras van a la página 63 y no a la BRAM. El monitor se
   ensambla para ejecutarse en `$2000`, que es donde lo verá el Z80 por la
   ventana del bloque 1: la dirección de carga del MCU no importa.
4. Soltar el reset, como ahora. El depurador queda armado.

- **Tras un reset del Z80:** la SRAM conserva el monitor y los registros de
  configuración no se borran. No hay que hacer nada.
- **Tras `LOAD` de un `.ROM`:** no hay que hacer nada. Ese camino solo baja
  `FPGA_RESET` (el `nRESET` de la FPGA) mientras escribe la ROM, y los
  registros de configuración no dependen de `nRESET`. La orden 9 sigue
  puesta y la recarga no toca la página 63, porque solo se desvía el
  bloque 7.
- **Para desarrollar el monitor:** `dbg reload` en la consola hace los pasos
  3 y 4 con el Z80 en reset y lo suelta. Es como un reset.
- **Para entrar al monitor y poner breakpoints antes de lanzar un programa,**
  basta con la pausa (consola o botón) en el prompt del BASIC.

## 7. Fases

Cada fase se prueba en el hardware antes de pasar a la siguiente.

**Fase 1. FPGA, el motor de ruptura** (y lo mínimo del MCU):
- todo §3: máquina de estados anidada, fuentes de ruptura, cambio de página,
  escritura en el bloque 0 durante el monitor, BRAM protegida, órdenes 8 y 9
  ("monitor cargado": carga en la página 63 y armado), joystick, seguimiento
  de la NMI y el puerto
  `$3FEF` con la firma `52h`;
- en el MCU, solo la carga de `DEBUG.BIN` al encender (§6) y la orden 8 desde
  la consola;
- quitar los contadores del detector;
- banco de pruebas en ModelSim/ISim: entrada y salida, cambio de página
  después de las escrituras del `CALL`, anidado dentro de una rutina de
  `sim_int`, el paso que se queda en su nivel, `FF` de memoria, comparador en
  sus cuatro modos y "saltarse una" al continuar;
- prueba en el ZX81 con un **monitor de juguete** como `DEBUG.BIN`, sin
  el protocolo con el MCU. Guarda en la RAM del programa los PC en los que
  para, y se comprueba:
  - un breakpoint por software y uno del comparador;
  - N pasos;
  - un punto de vigilancia de escritura;
  - una trampa `OUT`;
  - una ruptura dentro de una rutina de `sim_int`.

  Todo se comprueba con lo que deja el monitor de juguete, sin lógica de
  prueba en la FPGA;
- adaptar las pruebas existentes (ver §8).

**Fase 2. Monitor y MCU** (hecha y probada en hardware; también en el PC
con `claude/dbgharness`):
- el monitor de verdad como `DEBUG.BIN`, y `dbg reload`;
- `DBG_BREAK` y `DBG_POLL`;
- consola USB, botón QuickSilva, LED;
- breakpoints por software en RAM y ROM;
- prueba con un programa propio en Superfast (por ejemplo `INTTEST`) y con
  el explorador.

**Fase 2b. Snapshots** (después de que la consola funcione):

> **Estado (1 de octubre de 2026):** el guardado está hecho y pasa los
> bancos del PC (`tb_dbg.v` y `claude/dbgharness`); falta probarlo en el
> hardware. FPGA rev 0.07: índices 2 (BRAM, con puntero en los registros 5/6)
> y 3 (Chroma81) del puerto `$3FEF`. Monitor: peticiones 7 (`INSEQ`) y 8
> (`READP`). MCU: `snap [-a] [f]` y el botón QS. Páginas: las mapeadas, o
> todas con `-a` (hasta la 31 sin FULL_PAGING); no se guardan las 0-1 si
> siguen siendo la ROM, ni la 63, ni (con `-a`) las que están todas a `FF`.
> Detalle en `hw_debugger_emulator.md`, sección 11. Falta la carga
> (`LOAD *Z81` ampliado).
>
> **Actualización (2 de octubre de 2026, tarde):** hecho y probado en el
> hardware: la carga por el monitor (sección 12), los AY, sprites, `SHADOW`
> y el estado del MCU (commit 75edeb3). Después, para el usuario sin
> consola: la FPGA (rev 0.09) apunta las páginas escritas y el snapshot
> normal guarda las mapeadas más las escritas, sin tener que elegir; y en la
> pausa del botón QS, el teclado: `S` snapshot y sigue, `Z` snapshot y
> parado, `L` carga el último, espacio sigue (sin menú en pantalla, por
> ahora). `snap` desde la consola deja el programa parado.
- Es "parar, leer todo y continuar" con el mismo mecanismo del depurador. El
  monitor Z80 solo lee; el **MCU monta el `.Z81`** (ya lo sabe leer en
  `cmd_loadZ81`) y lo escribe en la SD.
- Qué se guarda:
  - los registros. IM no se puede leer: se guarda IM 1. R se corrige
    descontando las instrucciones del propio monitor;
  - `[MEMORY]` (la vista del Z80);
  - el mapper (`IN $E7` más la página tapada del bloque 1);
  - el estado que configura el MCU, que ya lo conoce;
  - **el estado que se programa con POKEs** (Superfast, HFILE, DFILE
    alternativo, atributos, borde, scroll, sprites, doble buffer, WRX, MC45
    en 6/7, interrupciones simuladas…). Es de solo escritura en la FPGA,
    pero sus últimos valores están en la BRAM de sombra (`$07F6`-`$0850`).
    Hace falta **un camino para leer la BRAM desde el Z80** (un puntero y un
    `IN`, unas 30 LUTs). Es el mismo que necesitaría la lectura de la traza
    de la fase 3, así que se adelanta aquí.
- **Ampliar `LOAD *Z81`** para que restaure lo mismo: `MAPPER`, los
  `RAM_PAGE` de `[SD81BOOSTER]` y los POKEs. Es el pendiente de
  `z81_snapshot_loading.md`.
- **Disparo:** mantener el botón QuickSilva 3 s (el LED pasa de magenta a
  amarillo; al soltar graba y continúa), o `snap [fichero]` en la consola.
  La trampa `OUT` con una orden "snapshot" (un juego la podría usar para
  guardar partida) y el joystick se pueden añadir sin decidir nada más.
- **Nombre:** sin PC no se puede escribir. El MCU usa el nombre del último
  fichero cargado más un número, en la carpeta actual y sin pisar nunca uno
  anterior: `MAZOGS.P` → `MAZOGS001.Z81` (sin separador: el ZX81 no tiene `_`). Si todavía no se ha cargado nada,
  `NONAME001.Z81`.
- Siempre disponible: el monitor se carga al encender (§6).
- **Por decidir en esta fase:** qué páginas se guardan (todas, 512 KB y
  decenas de segundos; solo las mapeadas; o elegidas a mano).
- Vale para FAST y Superfast. Los programas en SLOW, que son la mayoría de
  los juegos clásicos, esperan a la fase 4.

**Fase 3. Extras:**
- paso por encima, "ejecutar hasta", puntos de vigilancia en la consola
  (hecho el 3 de octubre de 2026: `o`, `g`, `u` y `w`, solo en el MCU;
  `hw_debugger_emulator.md` sección 10.8);
- **decidir la traza y la interfaz en la pantalla del ZX81** (§1, 11). La
  interfaz quedó decidida el 3 de octubre de 2026 (abajo). La traza sigue
  pendiente: necesita memoria en la FPGA, y la BRAM está a 32/32. Hecha
  una traza lenta sin FPGA (el MCU paso a paso; `t`, `th`, y `T`/`H` en la
  pantalla): `hw_debugger_emulator.md` sección 13.8.

**Fase 3b. Pantalla del depurador en el ZX81, sin FPGA** (decidida el 3
de octubre de 2026). Hecha en el monitor (versión 4, `SETI`) y en el MCU
(`ui` y `D` en la pausa QS); probada en hardware. Después, desde el teclado
del ZX81, lo mismo que la consola: breakpoints, vigilancia, ir a,
registros, poke, desensamblar y volcar desde una dirección, la pantalla del
programa (`V`) y cargar el último snapshot (en verde en el arnés).
Detalle en `hw_debugger_emulator.md`, sección 13.6. Las slices están al 95 %, así que todo
sale de lo que ya existe: firmware y `DEBUG.BIN`, sin tocar la FPGA ni
`SDBOOST.ROM`. Generaliza la orden `v`.

- **Cómo se ve.** Mientras está parado, la FPGA pinta en Superfast desde la
  BRAM de sombra: 80 columnas (`POKE 2045,174`, caracteres de 7 píxeles) o
  32 (`170`). El D_FILE sale del override (2096/2097 y `2098,170`) y apunta
  a una zona de la sombra que el programa no usa. El MCU escribe ahí con
  `BRAMW`, sin tocar la SRAM: el programa no se entera. Color opcional, con
  la base de atributos independiente (2059/2060 y `2061,170`) y el Chroma
  en modo 1.
- **Qué lee la FPGA de la sombra** (comprobado en `SD81.v`): las celdas
  (`DFILE_eff`), los atributos (`$C000…` o el override), la fuente
  (`ROMTABLE`, que sigue a I en cada refresco) y el HiRes (`vpage`). El
  espejo de sprites de `$0C00-$0FFF` solo lo lee el MCU, para los
  snapshots. Por eso la sombra del bloque de la ROM está libre, salvo los
  POKEs (2038-2100), el espejo de sprites y la fuente (`$1E00-$1FFF`):
  - pantalla de 80 × 24: `1 + 24 × 81` = 1945 bytes en `$1000-$1798`;
  - atributos: otros 1945 en `$0000-$0798` (por debajo de 2038).
- **La fuente.** I está en la tabla de caracteres y, parado, I es la del
  programa (en un WRX, cualquier cosa). En este modo el monitor pone
  I = `$1E` mientras espera órdenes; la I del programa vuelve con `CONT`,
  como el resto de los registros. Es el único cambio en `debugmon.asm`: una
  petición nueva o un indicador.
- **Al salir** (continuar, snapshot o tecla): se reponen 2045, 2096-2098,
  2059-2061 y el Chroma, como en `view_off()`. La sombra de las dos zonas se
  guarda al entrar y se repone al salir, como en `v dir`.
- **Contenido.** Registros, unas líneas de desensamblado alrededor del PC
  (el desensamblador del MCU ya existe), un volcado de memoria, los
  breakpoints y la línea de estado. Solo el juego de caracteres del ZX81:
  mayúsculas, y vídeo inverso para resaltar el PC.
- **Teclado.** El de la pausa del botón QS, que ya existe: `S` paso, `O`
  por encima, `U` salir, `C` continuar, cursores para mover el volcado o el
  desensamblado, y las teclas de snapshot de siempre. Se entra con el
  botón QS (1 s) o con una orden de la consola.
- **Velocidad.** Cada byte de `BRAMW` pasa por el handshake con el monitor.
  Se mide con `v dir` en una dirección sin alinear: imprime lo que tardan
  las tres partes de la copia (6144 bytes cada una). Medido en hardware
  (3 de octubre de 2026): leer la sombra 429 ms, leer la memoria 521 ms y
  escribir la sombra 518 ms, unos 84 µs por byte (12 KB/s) al escribir.
  Una pantalla de 80 × 24 entera son ~165 ms, y con atributos el doble.
  Por eso:
  - al entrar se guarda la sombra de las dos zonas (~0,3 s) y se pintan la
    pantalla y los atributos (~0,33 s): una vez;
  - en cada paso solo se envían las líneas que cambian (el MCU guarda una
    copia de lo que hay en pantalla): un paso normal cambia los registros
    y unas pocas líneas, unos 20-40 ms;
  - los atributos solo cambian para mover el resaltado del PC.
- **Lo rico** (listados largos, editar memoria, breakpoints con el ratón)
  va en la web del ESP32 (fase 5), que tampoco cuesta FPGA.
- **Por comprobar antes de programar:** la velocidad de `BRAMW`; que la
  fuente de 7 píxeles se lee bien en 80 columnas; y si un programa que
  escribe en la zona de la ROM (no debería) ensucia la sombra elegida.

**Fase 4. Vídeo nativo (SLOW):** (3 de octubre de 2026: hecha la versión
de menor coste y probada en hardware, commit 37e12d5;
`hw_debugger_emulator.md` sección 13: en SLOW se para en la entrada de la
NMI y el MCU la deshace, el monitor apaga la NMI y el MCU la vuelve a
encender al seguir. La pantalla queda en negro mientras está parado; los
pasos en SLOW son aproximados.)
- que el monitor conviva con la NMI (o pase a FAST con Superfast texto
  mientras está parado);
- que la FPGA no rompa en la rutina de vídeo ni ejecutando el DFILE.

**Fase 5. Interfaz web en el ESP32 y símbolos de pasmo.** Los símbolos,
hechos el 3 de octubre de 2026 (solo el MCU; `hw_debugger_emulator.md`
sección 13.7). Falta la web.

**Emulador:** hecho para la fase 1: `hw_debugger_emulator.md` (nuevo) y
`sim_int_emulator.md` actualizado a la rev 0.06 (sin contadores, puerto
`$3FEF` nuevo). Cada fase siguiente ampliará `hw_debugger_emulator.md`.

## 8. Cambios fuera del depurador

| Qué | Cuándo | Dónde |
|---|---|---|
| Borrar `M1TEST` | Fase 1 | `EXAMPLES/SIMINT` |
| `INTTEST` y `HALTTEST`: quitar la comparación con el contador de la FPGA (índices 9/10); aceptar las firmas `51h` y `52h` | Fase 1 | `EXAMPLES/SIMINT` |
| Prueba 5 de SD81TEST: quitar la parte del detector y las comparaciones con la FPGA (de 14 comprobaciones a 8); aceptar `51h` y `52h` | Fase 1 | `EXAMPLES/SD81TEST` |
| Test de memoria de SD81TEST: tratar la página 63 como de sistema | Hecho (fase 1) | `EXAMPLES/SD81TEST` |
| Disco RAM de CP/M 3: páginas 15–62 (DSM 48 → 47) | Antes de depurar CP/M | Proyecto CPM3_SD81, en su sesión |
| Firmware: nuevo comportamiento del botón QuickSilva | Fase 2 | `Arduino/SD81BoosterV2_039_STM32` |
| Manual: el depurador y el botón | Al cerrar la fase 2 | `MANUAL/` |

## 9. Riesgos y cosas por comprobar

- **Que quepa en la FPGA:** se verá en la síntesis de la fase 1.
- **El instante exacto del cambio de página:** se detecta la segunda
  escritura del `CALL` en el dominio de `iclock`, y se repone al final de
  la M1 de `$003B`. Hay que comprobarlo en el banco de pruebas con el
  retraso real del reloj, como el de `tb_m1_tracker`.
- **Detección del terminal USB (DTR) en STM32duino:** si no se puede
  detectar, el caso "sin PC" funciona igual (parado y LED magenta); solo
  cambia lo que se enseña al conectar.
- **Página 63 siempre ocupada:** con el monitor cargado al encender, un
  programa con FULLPAG que use la página 63 lo machacaría. **Hecho en la
  fase 1:** con la orden 9 puesta, la FPGA no deja escribir la página 63
  fuera del monitor (`p63_lock` en `SD81.v`). Ese programa la puede leer,
  pero sus escrituras no llegan a la SRAM. El test de memoria de SD81TEST
  la trata como de sistema.
- **Comprobado:** A16x–A18x no llegan a ningún pin del MCU. Con la orden 9,
  la FPGA pasa a conducirlas durante el reset.
- **No romper en código que esté hablando con el MCU** (el gestor de la SD a
  mitad de un `LOAD`): la conversación del monitor se mezclaría con la que
  está en curso. En la v1 queda documentado. Más adelante, el firmware
  podría aparcar el comando en curso.
- **Breakpoint por software en un `LOAD`:** el `LOAD` lo machaca. Para eso
  está el comparador. Más adelante, el MCU podría meter el `FF` en el flujo
  de los `.P` que envía.
