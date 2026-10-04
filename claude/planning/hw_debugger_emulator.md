# Especificación para el emulador: depurador por hardware, snapshots y SLOW

El SD81 Booster tiene un depurador por hardware. La FPGA para el programa en
un límite de instrucción y entra en un **monitor** que vive en la **página
63** de la SRAM. Lo hace con el mismo mecanismo que las interrupciones
simuladas (`sim_int_emulator.md`): sirve `FF` (RST 38h), un `CALL` en `$0038`
y un epílogo en `$003B`. La diferencia es que, mientras corre el monitor, el
bloque 1 enseña la página 63. Con él se depura desde la consola del MCU, se
hacen snapshots `.Z81` (consola, botón QuickSilva y teclado del ZX81) y se
cargan con `LOAD *Z81`, también de programas en SLOW.

## Estado (4 de octubre de 2026)

Todo lo de este documento está hecho y probado en el hardware. Versiones:

| Pieza | Versión | Dónde |
|---|---|---|
| FPGA | `sim_int.v` rev **0.13** (traza) | `FPGA/SD81V2.1000/sim_int.v`, `SD81.v`, `ay38912.v` |
| Monitor | versión **4** (`"SD81DBG",4` en `$2003`), **1287 bytes** | `z80rom/debugmon.asm` → `/SYS/DEBUG.BIN` |
| ROM | `LOAD *Z81` con el comando 75; sprites a cero en el reset | `z80rom/sdhandler.inc.asm` → `/SYS/SDBOOST.ROM` |
| MCU | comandos 73, 74 y 75 (`LAST_COMMAND 75`); órdenes de configuración 8-12 (pausa, monitor cargado, POKEs del monitor, páginas escritas, traza) | `DEBUGGER.cpp`, `MCUSTATE.cpp`, `COMMANDS.cpp`, `SD_handle.cpp` |
| Módulo WiFi | la web del depurador (`CMD_DBG`, `CMD_DBG_VIEW`): no afecta al emulador | `Arduino/Wifi_module_01`, `WIFI_HANDLER.cpp` |

**Qué ha cambiado, por orden** (para encontrar lo que le falta al emulador):

| Commit | Qué | Secciones |
|---|---|---|
| 423ea97 | Fase 1: el motor de ruptura en la FPGA, la carga del monitor | 1-9 |
| 733933a | Fase 2: el monitor de verdad, los comandos 73/74, la consola, el botón QS | 10 |
| 75edeb3 | Fase 2b: snapshots (`snap`, botón 3 s); `LOAD *Z81` por el monitor (comando 75, **la ROM lo manda antes que el 70**); FPGA: índices 2-5 y registros 5-7 de `$3FEF`, orden 10, copia de los sprites en la sombra; peticiones 7-12 | 6, 11, 12 |
| a600553 | Páginas escritas (índice 6, orden 11); teclado en la pausa del botón (`S`, `Z`, `L`, espacio); `SETREGS` de 31 bytes con el IM; `snap` desde la consola deja el programa parado | 11.5, 11.6 |
| 37e12d5 | Fase 4: programas en SLOW (la FPGA para en la entrada de la NMI, el monitor la apaga, el MCU la deshace); teclas al soltarlas | 3, 13 |
| (siguiente) | Carga de snapshots: el sonido callado mientras carga; el estado del MCU (VGM, AY, PEG, ficheros) al final, al seguir. Solo el MCU | 12.4 |
| 8ba74cf | La web, con paneles: desensamblado, registros, pila, breakpoints, memoria y consola (`CMD_DBG_VIEW`, la vista estructurada que compone el MCU). STM32 y ESP32 | 13.10 |
| 6300e3a | Fase 5: la consola del depurador en la web del ESP32 (`/debug`, `CMD_DBG` en el protocolo UART). STM32 y ESP32 | 13.10 |
| 5823b2e | Historial: la traza de la FPGA (sim_int 0.13, orden 12, índices 7/8), el PC de cada instrucción en `$1000-$17FF`; la pantalla del depurador pasa a `$0000`; `th`/`H` lo enseñan, `tron`/`troff` | 3, 13.6, 13.9 |
| ce6d897 | Traza lenta sin FPGA: `t [n]`, `th [n]`; `T` y `H` en la pantalla. Solo el MCU | 13.8 |
| 7ee9719 | La pausa del botón QS abre la pantalla del depurador (se ve que está parado); `V` la cambia por la del programa con el teclado de la pausa; `ui` en marcha decide si sale en la parada siguiente. Solo el MCU | 11.6, 13.6 |
| 90a0740 | Fase 3b: pantalla del depurador en el ZX81 (`ui`). Monitor versión 4 con `SETI` (petición 13). Sin FPGA. Y los símbolos de pasmo (`sym`; `LOAD` lee `<nombre>.SYM`) | 12.2, 13.6, 13.7 |
| 8afd989 | Orden `v dir`: vídeo Superfast HiRes mientras está parado, con el mapa de bits (WRX) en `dir`; copia a la sombra si no está alineado. Solo el MCU. Probado en el arnés; en hardware sin confirmar con un WRX real (no se encontró la dirección del mapa de bits) | 13.4 |
| cc3f2f9 | Orden `v`: vídeo Superfast texto mientras está parado (programas en SLOW y FAST). Solo el MCU | 10.5, 13.4 |
| 26c37a9 | Fase 3: `o` (paso por encima), `u` (salir de la rutina), `g` (ejecutar hasta) y `w` (puntos de vigilancia) en la consola. Solo el MCU: la FPGA y el monitor no cambian | 10.5, 10.8 |

**Importante:** la ROM nueva manda el comando 75 antes que el 70. El
emulador tiene que contestarlo, aunque sea con `0xFF` ("sin monitor"), o
`LOAD *Z81` se queda esperando (sección 12.3).

**Lo que tiene que hacer el emulador**, en resumen (el detalle, en cada
sección):
- la máquina de estados del depurador, el puerto `$3FEF` (índices 0-8 y 15,
  registros 0-7) y la ventana de la página 63 (secciones 2-6);
- el monitor `DEBUG.BIN` corre tal cual: versión 4, con `SETI` (12.2);
- los snapshots y su carga por el monitor, con el comando 75 (11, 12);
- SLOW: romper en la entrada de la NMI (13.1, 13.5);
- Superfast desde la sombra con los override de D_FILE y atributos, el modo
  de 80 columnas y la fuente según I: es lo que usan `v` y la pantalla del
  depurador (13.4, 13.6);
- la traza de la FPGA con la orden 12 (13.9);
- todo lo demás (consola, pantalla, símbolos, traza lenta, web) es del MCU,
  que el emulador ejecuta o imita.

Referencias:
- `FPGA/SD81V2.1000/sim_int.v`: el depurador va dentro del módulo
  `sim_int`. `tb_dbg.v`: banco de pruebas ciclo a ciclo de todos los casos.
- `FPGA/SD81V2.1000/SD81.v`: la ventana, la carga, la protección, el bloque
  0, la sombra y las páginas escritas (busca `dbg_`, `blk0_ram`, `p63_lock`,
  `sram_page`, `spr_mirror` y `shadowram`).
- `z80rom/debugmon.asm`: el monitor.
- `Arduino/SD81BoosterV2_039_STM32/DEBUGGER.cpp`: toda la lógica del MCU
  (consola, breakpoints, snapshots, carga, teclado, SLOW); `MCUSTATE.cpp`:
  el estado del MCU en los snapshots; `SD_handle.cpp`, `load_debug_monitor()`.
- `claude/dbgharness` (`sh build.sh`): bancos de pruebas en el PC. `cosim.cpp`
  corre el monitor de verdad en el núcleo Z80 de EightyOne contra el
  `DEBUGGER.cpp` de verdad, con un modelo de la FPGA (y NMIs): es la mejor
  referencia de cómo encaja todo.
- `claude/planning/hw_debugger_plan.md`: el plan y el porqué de cada
  decisión. `snapshots_cambios_emulador.md`: las claves que añadió el
  emulador para el explorador (ya están en el hardware, sección 11.3).

**Recomendación:** igual que con las interrupciones simuladas, emularlo **a
nivel de byte** en la función que sirve las lecturas y escrituras de la CPU.
La pila tiene que quedar exactamente como en el hardware.

## 1. Lado del MCU

**Al encender**, después de cargar la ROM (`/SYS/SDBOOST.ROM`) y antes de
soltar el reset del Z80:
- **Si existe `/SYS/DEBUG.BIN`** (8 KB como mucho): se copia al **principio
  de la página 63**, que en un array de 512 KB es el offset `63 * 8192`. El
  monitor está ensamblado para `$2000`, que es donde lo verá el Z80. Además
  se pone **`dbg_loaded = 1`**: depurador armado.
- **Si no existe, o pasa de 8 KB:** `dbg_loaded = 0` y la página 63 no se
  toca.

En el hardware el MCU lo hace escribiendo en `$E000`-`$FFFF` con la orden 9
del canal de configuración, que desvía el bloque 7 a la página 63. En el
emulador basta con copiar al array.

**`dbg_loaded` no se borra con el reset del Z80** (vive en los registros de
configuración). Se mantiene hasta que se vuelva a cargar o se apague.

**Órdenes de consola del firmware** (el USB del STM32). Si el emulador no
tiene consola, conviene poner algo equivalente en un menú o en una tecla:
- **`DBG_PAUSE`:** pausa (sección 3, fuente "MCU"). Solo si `dbg_loaded`.
- **`DBG_RELOAD`:** como un reset del Z80 y de la FPGA, pero antes vuelve a
  copiar `/SYS/DEBUG.BIN` en la página 63 (o pone `dbg_loaded = 0` si ya no
  está).

## 2. Memoria: ventana, bloque 0 y protección

La fase del depurador (sección 4) decide tres cosas:

**La ventana.** En la fase MON, y en la primera M1 de la fase CALLED, todo
acceso a `$2000`-`$3FFF` (lecturas, escrituras y M1) va a la **página 63**, sea
cual sea la página del bloque 1.
- El mapper no se toca: `IN` del puerto `$E7` con B = 1 sigue devolviendo la
  página del programa.
- No afecta a los ciclos de refresco.

**El bloque 0 como RAM.** En la fase MON, el bloque 0 (`$0000`-`$1FFF`) se
puede escribir, salvo con la orden 10 (sección 12.1). Normalmente está protegido: es la ROM. Así el monitor puede
poner breakpoints por software en la ROM. Mientras dura:
- **no se capturan los POKEs de control** del bloque 0: 2038-2040, 2041-2058,
  2059-2062, 2090-2098, sprites… Es lo mismo que ya pasa con `POKE 2056`
  (CP/M);
- **las escrituras de la CPU no se copian al espejo de vídeo** (la BRAM).
  Si el emulador pinta desde la RAM y no desde un espejo propio, da igual:
  el monitor no escribe en zonas de vídeo, salvo que el usuario lo pida.

**Protección de la página 63.** Con `dbg_loaded = 1` y fuera de la fase MON,
las escrituras del Z80 que vayan a la página 63 **no llegan a la memoria**.
Solo pasa si un programa la ha mapeado con FULLPAG. Las lecturas son
normales. Con `dbg_loaded = 0`, la página 63 es como cualquier otra.

**La sombra (BRAM).** La FPGA copia en su BRAM de sombra (64 KB, por
dirección lógica) lo que escribe la CPU; el vídeo Superfast y los atributos
de Chroma salen de ahí. Dos excepciones del depurador:
- los POKEs de sprites 2101-2128 van a `$0C00 + sprite*32 + campo`, no a su
  dirección (sección 11.4);
- el puerto `$3FEF` la lee (índice 2) y la escribe (registro 7).

## 3. Cuándo se rompe

Solo con **`armado`** (= `dbg_loaded`). En **FAST o Superfast**, en
cualquier instrucción. En **SLOW**, solo en la primera M1 de la NMI
(`$0066`): ver la sección 13. SLOW es "ha habido una M1 en `$0066` hace
menos de un cuadro" (un contador de 16 bits, `nmi_age`), no lo que digan
los `OUT ($FE)`/`($FD)`.

**Fuentes que dejan una ruptura pendiente.** Solo se aceptan con el
depurador armado y en reposo (fase IDLE):

| Fuente | Cómo | Motivo |
|---|---|---|
| MCU | `DBG_PAUSE` | 1 |
| Joystick | arriba y abajo a la vez (al pulsarlo) | 2 |
| Trampa | `OUT` de `$10` al puerto `$3FEF` | 3 |
| Vigilancia | el comparador en modo lectura, escritura o E/S ve el acceso (sección 6) | 6 |

**En cada M1**, antes de servir el byte, con `op` el byte de la memoria:

```
prog_m1   = boundary                       (m1_tracker: empieza instruccion)
            && la M1 no la sirve sim_int   ($0038 en ENTRY, $003B en ISR,
                                            $003C-$003E en EPI)
            && depurador en reposo (IDLE)
sim_inj   = arm || halt_ok                 (lo que haria sim_int en esta M1)
counted   = prog_m1 && !sim_inj && (lvl || fase de sim_int == IDLE)
step_brk  = step_on && counted && step_cnt == 0
cmp_exec  = modo del comparador == 1 && direccion de la M1 == cmp_addr
swbp      = op == FF                       (un RST 38h de verdad en memoria)

slow      = nmi_age != $FFFF               (NMI hace menos de un cuadro)
brk_ok    = !slow || direccion == $0066    (seccion 13)

rompe = armado && brk_ok && prog_m1 &&
        ( swbp || spin_pend || step_brk ||
          (!skip && (pausa || vigilancia || cmp_exec)) )
spin  = armado && !brk_ok && prog_m1 && swbp   (FF en SLOW: se sirve JR $)
```

**Si rompe:**
- se sirve `FF` (aunque ya fuera `FF` en memoria) y `sim_int` no inyecta en
  esta M1;
- `motivo` = el primero que se cumpla de: 7 (`swbp` o `spin_pend`), 5 (`cmp_exec` sin
  `skip`), 6 (vigilancia sin `skip`), 4 (`step_brk`), o el de la pausa (1, 2
  o 3);
- `lvl` = 1 si la fase de `sim_int` no es IDLE (se ha roto dentro de una
  interrupción simulada);
- se borran la pausa, la vigilancia, `step_on`, `skip` y `spin_pend`;
- bit 6 del estado = `slow`.

**Si no rompe y es `prog_m1 && !sim_inj`** (una instrucción del programa
que se ejecuta de verdad):
- `skip = 0`;
- si `counted && step_on`: `step_cnt--`.

**`skip`** se pone a 1 al volver del monitor (fin de la fase EPI). Así, la
instrucción en la que se paró se ejecuta sin volver a romper por pausa ni
por el comparador. Si al volver entra antes una interrupción simulada, esa
M1 no cuenta: `skip` sigue a 1 hasta la primera instrucción del programa
que se ejecute. Un `FF` de memoria sí rompe aunque haya `skip`.

**El paso se queda en su nivel.** `lvl` es el de la última ruptura:
- si se paró en el programa principal (`lvl = 0`), solo cuentan las
  instrucciones con `sim_int` en IDLE: una interrupción por medio se ejecuta
  entera sin contar;
- si se paró dentro de la rutina (`lvl = 1`), cuentan todas, así que el paso
  avanza por la rutina y luego vuelve al principal.

**Una CPU en HALT** (sin interrupciones simuladas) hace M1 sin ejecutar
nada: si se rompe ahí, el `FF` no llega a ejecutarse. La fase ENTRY lo
detecta y se anula (sección 4).

## 4. Máquina de estados del depurador

```
fase: IDLE, ENTRY, CALLED, MON, EPI
```

**Bytes que se sirven** (solo en estas lecturas; el resto va a la memoria):

| Fase | Ciclo | Dirección | Byte |
|---|---|---|---|
| IDLE | M1 en la que rompe | cualquiera | `FF` (RST 38h) |
| ENTRY | M1 | `$0038` | `CD` (CALL nn) |
| ENTRY o CALLED | lectura | `$0039` | `00` |
| ENTRY o CALLED | lectura | `$003A` | `20` |
| MON | M1 | `$003B` | `E3` (EX (SP),HL) |
| EPI | M1 | `$003C` | `2B` (DEC HL) |
| EPI | M1 | `$003D` | `E3` (EX (SP),HL) |
| EPI | M1 | `$003E` | `C9` (RET) |

"Lectura" es una lectura de memoria que no es M1. **Mientras la fase no es
IDLE, `sim_int` no sirve nada** y su máquina de estados no avanza. La M1 de
la ruptura (la del `FF`) tampoco cuenta para `sim_int`.

**Transiciones:**

```
IDLE:   al acabar la M1 en la que rompe            -> ENTRY
ENTRY:  lectura de $003A                           -> CALLED
        al acabar una M1 que no sea la de $0038    -> IDLE  (el RST no llego:
                                                             CPU en HALT)
CALLED: al empezar la M1 siguiente (la de $2000)   -> MON   (la ventana ya esta
                                                             puesta en esa M1)
MON:    al acabar la M1 en $003B                   -> EPI   (la ventana se quita
                                                             al acabarla)
EPI:    al acabar la M1 en $003E                   -> IDLE; skip = 1
```

La ventana va desde la primera M1 en `$2000` hasta el final de la M1 en
`$003B`. **Las dos escrituras del `CALL` van todavía a la página del
programa** (si la pila del programa estuviera en el bloque 1, irían a su
sitio). El `EX (SP),HL` del epílogo lee y escribe ya con la página del
programa.

## 5. Lo que pasa en la CPU

Ruptura en la instrucción en X:

1. `FF`: la CPU mete **X+1** en la pila y va a `$0038`. Si era un `FF` de la
   memoria (breakpoint por software), es lo mismo.
2. `CD 00 20`: mete **`$003B`** y va a `$2000`, al monitor, en la página 63.
3. El monitor hace lo que quiera. **Antes de salir deja SP como estaba antes
   del `CALL`, sin el `$003B`**, y sale con **`JP $003B`**, no con `RET`. Así
   el epílogo encuentra X+1 arriba, y nadie lee la pila del programa con la
   ventana puesta.
4. En `$003B`-`$003E`: `EX (SP),HL / DEC HL / EX (SP),HL / RET`. X+1 pasa a X
   y se vuelve a la instrucción, que se ejecuta entera (o vuelve a romper si
   el monitor no ha quitado el `FF` de memoria).

Contrato del monitor, a la entrada: `[SP] = $003B`, `[SP+2] = X+1`. Para
cambiar el PC del programa, el monitor escribe el nuevo PC + 1 en `[SP+2]`.

Coste: RST 11 T + CALL 17 T + JP 10 T + epílogo 54 T, más el monitor.

## 6. Puerto `$3FEF`

Decodifica los **16 bits**: `OUT (C)` / `IN (C)` con BC = `$3FEF`. Para el
Z80 no hace falta estar en el monitor.

| OUT | Qué hace |
|---|---|
| `$00`-`$0F` | elige lo que devuelve IN |
| `$10` | trampa: pausa en la instrucción siguiente (motivo 3) |
| `$80`+r | el OUT siguiente es el dato del registro r |

| r | Registro |
|---|---|
| 0 / 1 | comparador: dirección baja / alta |
| 2 | modo del comparador: 0 apagado, 1 ejecución, 2 lectura, 3 escritura, 4 E/S |
| 3 / 4 | N baja / alta: romper tras N instrucciones (0 = no). Al escribir la alta: `step_cnt = N`, `step_on = (N != 0)` |
| 5 / 6 | puntero de la BRAM de sombra, bajo / alto. Se carga al escribir el alto |
| 7 | escribe el dato en la BRAM de sombra, en el puntero, y el puntero avanza (sección 12.1) |

| IN (índice) | Qué devuelve |
|---|---|
| 0 | estado: bit 7 armado, bit 6 en SLOW al parar (sección 13), bit 5 `lvl`, bits 2-0 motivo de la última ruptura |
| 1 | interrupciones simuladas: `enabled, pending, arm, superfast, halted, call_now, fase(2 bits)` |
| 2 | el byte de la BRAM de sombra en el puntero; el puntero avanza 1 al acabar cada `IN` |
| 3 | el registro de Chroma81 (lo último escrito con `OUT $7FEF`) |
| 4 / 5 | el registro elegido del AY A (A3=1, ZonX) / del AY B: el latch de dirección, que sus puertos no dejan leer |
| 6 | las páginas escritas por la CPU, una por `IN` (bit 0), de la 0 a la 63: el puntero vuelve a 0 al elegir el índice 6 y avanza al acabar cada `IN` (sección 11.5) |
| 7 / 8 | el puntero de la traza (rev 0.13): bajo / los 2 bits altos (sección 13.9) |
| 15 | firma `52h` |
| otros | 0 |

**El comparador en modo lectura, escritura o E/S** deja una ruptura
pendiente (motivo 6) en cuanto ve el acceso. Rompe en la siguiente
instrucción del programa, justo detrás de la culpable:
- **lectura:** una lectura de memoria que no es M1 ni refresco, en
  `cmp_addr`;
- **escritura:** una escritura de memoria en `cmp_addr`;
- **E/S:** un `IN` o un `OUT` (no el reconocimiento de interrupción) cuyo
  byte bajo de dirección sea el de `cmp_addr`.

Solo se mira con el depurador armado y en reposo: los accesos del propio
monitor no cuentan.

**Con el reset** se pone a 0 todo: fase, motivo, `lvl`, `skip`, pendientes,
`step_on`, `step_cnt`, comparador (dirección y modo), índice de lectura y
punteros; `nmi_age` a `$FFFF`. `dbg_loaded`, la sombra y las páginas
escritas no se tocan.

## 7. Detalles que cuentan

- Los `OUT` al puerto se procesan **una vez por instrucción**: el registro
  r espera exactamente el `OUT` siguiente.
- La decisión de romper se toma en cada M1 con el estado de antes de esa
  M1, igual que la de `sim_int`.
- Si en la misma M1 querrían inyectar el depurador y `sim_int`, gana el
  depurador. Al volver, `sim_int` inyecta en X si sigue teniendo una
  interrupción pendiente.
- Mientras el monitor está activo, los VSYNC siguen poniendo `pending` en
  `sim_int` (se acumulan en uno solo) y FRAMES sigue corriendo.
- Un `RST 38h` de verdad del programa (un `FF` en un límite de instrucción),
  con el depurador armado, entra en el monitor como breakpoint por software
  (motivo 7). En SLOW espera en `JR $` a la NMI (sección 13).

## 8. Lo que queda para después

Nada del plan: la traza (13.8, 13.9), la pantalla del ZX81 (13.6) y la web
(13.10) están hechas. Quedan dos riesgos documentados en el plan (§9):
parar en mitad de una conversación con el MCU (un `LOAD`) y los breakpoints
por software que borra un `LOAD`.

## 9. Prueba (`EXAMPLES/DBGTEST`)

Prueba la fase 1 con su propio **monitor de juguete**, no con el de verdad
de la fase 2 (que habla con el MCU): para pasarla, `/SYS/DEBUG.BIN` tiene que
ser el `debug.bin` de esta prueba.

- `dbgmon.asm` → `debug.bin`: monitor de juguete. Va en la SD como
  `/SYS/DEBUG.BIN`.
- `dbgtest.asm` → `dbgtest.bin` (en 24576), con el stub `DBGTEST.B81`.

```
pasmo dbgmon.asm debug.bin
pasmo dbgtest.asm dbgtest.bin
```

**Qué hace el monitor de juguete:** apunta cada ruptura (X y el estado del
índice 0) en un registro de la RAM del programa. Si el motivo es un `FF` de
memoria en la dirección que le dice el buzón, repone el byte. Si el buzón
pide pasos, programa N = 1. El buzón está en `$7F00`, con una marca `'D','T'`
en `$7F07`; sin ella el monitor vuelve sin tocar nada, así que una pausa
fuera de la prueba es inofensiva.

**Las 8 pruebas:**
1. trampa;
2. trampa y 4 pasos por `CB`/`ED`/`DD`;
3. comparador de ejecución, dos llamadas;
4. vigilancia de escritura;
5. breakpoint por software;
6. breakpoint por software dentro de la rutina de interrupción simulada,
   despertando de un HALT (`lvl = 1`);
7. 250 pasos con interrupciones simuladas por medio, sin ninguna ruptura en
   la rutina;
8. protección de la página 63. Necesita `LOAD *FULLPAG` antes; sin ella se
   salta.

**Resultado en el hardware:**

```
BREAKS IN LAST TEST   251
INTERRUPTIONS (TEST 7) 2

OK
```

El número de interrupciones de la prueba 7 depende de la velocidad: tiene
que ser al menos 1. Lo demás tiene que salir **exacto**:
- 251 rupturas en la última prueba que apunta (la 7);
- `OK`.

Si una prueba falla, sale `TEST n WRONG`. Mientras corre, la primera
columna de la pantalla dice la prueba en curso (en Superfast se ve en
directo).

Con el depurador armado, `INTTEST`, `HALTTEST` (`EXAMPLES/SIMINT`) y la
prueba 5 de `SD81TEST` tienen que seguir dando lo mismo que antes.

## 10. Fase 2: el monitor y el MCU

### 10.1 El monitor (`/SYS/DEBUG.BIN`)

Es `z80rom/debugmon.asm` ensamblado. Lo ejecuta el Z80 emulado tal cual: el
emulador no tiene que hacer nada especial con él, aparte de lo de las
secciones 1-6. Para depurarlo, `claude/dbgharness/cosim.cpp`.

Al parar:
1. guarda todos los registros en su página;
2. manda `DBG_BREAK` con el bloque de registros;
3. entra en un bucle de `DBG_POLL`: manda el resultado de la petición
   anterior (vacío la primera vez) y recibe la siguiente.

El monitor solo hace operaciones elementales. **Toda la lógica es del MCU**,
y es lo que el emulador tiene que reproducir en su emulación del STM32.

Detalles del monitor que el emulador tiene que respetar sin hacer nada:
- **Su primera instrucción es `OUT ($FD),A`:** apaga la NMI (sección 13).
- **R:** el monitor descuenta 10 M1 a la entrada y 22 a la salida, contando
  las de la FPGA (`FF`, `CD`, y `E3 2B E3 C9`). Si el emulador sirve esos
  bytes como M1 de verdad (sección 4), las cuentas salen solas.
- **Los accesos a `$2000`-`$3FFF`:** el monitor mapea un momento la página
  del programa en el bloque 7 con el puerto `$E7` (en los dos formatos del
  mapper) y la devuelve después.

### 10.2 Bloque de registros (30 bytes)

| Offset | Contenido |
|---|---|
| 0 / 2 / 4 / 6 | HL' / DE' / BC' / AF' |
| 8 / 10 | IY / IX |
| 12 / 14 / 16 / 18 | HL / DE / BC / AF |
| 20 | SP del programa en el punto de ruptura |
| 22 | PC del programa (la instrucción en PC todavía no se ha ejecutado) |
| 24 / 25 | I / R |
| 26 | IFF2 (0 o 1) |
| 27 | estado del depurador (`$3FEF`, índice 0: armado, SLOW, nivel, motivo) |
| 28 | estado de las interrupciones simuladas (`$3FEF`, índice 1) |
| 29 | página del programa en el bloque 1 |

Todo en little endian. 27-29 son solo información: con `SETREGS` el monitor
las ignora. `SETREGS` puede llevar un byte 31 con el modo de interrupción
(sección 11.6).

### 10.3 Los dos comandos

El handshake es el de siempre (puertos `$A7` y `$AF`, como `fread`/`fwrite`,
comandos 55/56):
- cada byte que manda el Z80 lo confirma el MCU cambiando el reloj;
- cada byte que manda el MCU va en el latch antes del cambio de reloj que
  confirma lo anterior;
- al final hay un cambio de reloj más.

`LAST_COMMAND` es 75 (el 75 es `LOAD *Z81` con el monitor, sección 12.3).

**`DBG_BREAK` (73)**
```
Z80 -> MCU:  73, bloque de registros (30 bytes)
MCU -> Z80:  (nada; solo las confirmaciones y el cambio final)
```

**`DBG_POLL` (74)**
```
Z80 -> MCU:  74, n (2, LE), resultado de la peticion anterior (n bytes)
MCU -> Z80:  op, direccion (2, LE), m (2, LE), [m bytes de datos]
```

**El MCU no confirma el último byte del Z80 hasta tener una petición.** El
último byte es el byte alto de `n` si no hay resultado, o el último byte del
resultado. Mientras tanto el Z80 espera dentro de su `mcu_send`. En el
firmware, el comando se queda activo y el bucle lo vuelve a llamar en cada
vuelta. En el emulador basta con no cambiar el reloj hasta que la consola
genere una petición.

**Peticiones (`op`):**

| op | Nombre | Dirección | m | Datos | Resultado |
|---|---|---|---|---|---|
| 1 | READ | dirección | 1-256 bytes | — | m bytes |
| 2 | WRITE | dirección | 1-256 bytes | m bytes | — |
| 3 | SETREGS | 0 | 30 o 31 | el bloque de registros (y el IM) | — |
| 4 | OUT | puerto (16 bits) | valor en el byte bajo | — | — |
| 5 | IN | puerto (16 bits) | 0 | — | 1 byte |
| 6 | CONT | 0 | 0 | — | (el monitor sale) |
| 7 | INSEQ (2b) | puerto (16 bits) | 1-256 | — | m `IN` seguidos del mismo puerto |
| 8 | READP (2b) | desplazamiento en la página | 1-256 bytes | — | m bytes de la página p |
| 9-12 | WRITEP, BRAMW, AYREAD, AYWRITE | | | | sección 12.2 |
| 13 | SETI | 0 | I en el byte bajo | — | — (sección 12.2) |

Con `CONT` el monitor rehace la vuelta con el SP y el PC del bloque
(`[SP-2] = PC+1`, `SP-2`) y sale con `JP $003B`. Por eso el MCU puede cambiar
PC y SP con `SETREGS`.

Para dar pasos, el MCU programa N en la FPGA con cuatro `OUT` al puerto
`$3FEF` (`$83`, N bajo, `$84`, N alto) antes de `CONT`.

### 10.4 Lógica del MCU (`DEBUGGER.cpp`)

**Estado:**
- parado o en marcha;
- el bloque de registros;
- una cola de peticiones;
- una tabla de hasta 16 breakpoints por software, cada uno con su dirección,
  el byte original y si está puesto en memoria.

**Breakpoints por software.** Son un `FF` en memoria que el propio programa
ejecuta como `RST 38h`; la FPGA lo ve (motivo 7).
- **Solo están en memoria mientras el programa corre.**
- **Al parar se quitan todos** (`WRITE` del byte original), así que la
  memoria y el desensamblado se ven limpios.
- **Al continuar se vuelven a poner** (`READ` del byte actual como original
  y `WRITE` de `FF`).
- **Si hay uno en el PC al continuar:** se ponen los demás, se pide un paso
  (N = 1) y `CONT`. Cuando llega la ruptura de ese paso (motivo 4), sin
  enseñar nada: se pone el del PC anterior y `CONT`. Si en ese paso se para
  por otra cosa (otro breakpoint, por ejemplo), es una parada normal.
- **Al pedir pasos (`s n`) no se pone ninguno.**
- **Un `FF` que no es un breakpoint:** si al parar con motivo 7 el PC no está
  en la tabla, es un `RST 38h` del propio programa. Al continuar se emula lo
  que haría la CPU: `WRITE` de PC+1 en SP-2, SP-2, PC = `$0038` (`SETREGS`).
  **Se decide al parar, no al continuar:** si después se borra el breakpoint
  en el que se ha parado, su `FF` ya se quitó de la memoria y no hay nada que
  emular.
- **Tras un reset del Z80**, los breakpoints que estaban puestos pasan a
  "desconocido": en la ROM siguen en memoria, en la RAM no se sabe. Al parar,
  se lee cada uno: si hay un `FF`, se repone el original.
- **En SLOW**, al parar en `$0066` el MCU deshace la NMI y, al seguir, vuelve
  por `OUT ($FE),A` / `RET` debajo de la pila (sección 13.3).

**Al parar** (llega `DBG_BREAK`): LED magenta y en la consola:
```
*** STOP at 6029: breakpoint
PC=6029 SP=7F00 AF=4B09 BC=0030 DE=2222 HL=3333 IX=A5A5 IY=5A5A
AF'=8800 BC'=4444 DE'=5555 HL'=6666 I=00 R=4B IFF=0  sz-h-pnC
*6029  4F        LD C,A
block 1 page 1, sim_int 00. Type h for help.
```
El motivo sale del estado (bloque, offset 27): pause (MCU), pause
(joystick), trap, step, comparator, watchpoint, breakpoint o RST 38h (not a
breakpoint). Si el bit 5 está a 1, se añade "(inside the simulated
interrupt)", y si el programa estaba en SLOW, "(SLOW)". El `*` delante de
una dirección marca un breakpoint.

### 10.5 La consola

Son las órdenes en minúsculas de la consola USB del STM32. Las de
mayúsculas (`DBG_PAUSE`, `DBG_RELOAD` y las que ya había) siguen igual. Los
números van en hexadecimal (con `$` o sin él). Con el programa en marcha
solo valen `p`, `snap` y `h`.

| Orden | Qué hace |
|---|---|
| `p` | pausa: cambia el bit de la orden 8 (fuente "MCU") |
| `r` | registros e instrucción en el PC |
| `s [n]` | n pasos (1) |
| `c` | continuar |
| `o` | paso por encima: `CALL`, `RST`, `DJNZ`, `HALT` y los repetidos (`LDIR`…) enteros; lo demás, un paso (sección 10.8) |
| `u` | salir de la rutina: ejecutar hasta la dirección que hay en `[SP]` |
| `g dir` | ejecutar hasta dir |
| `b dir` / `bc [dir]` / `bl` | poner, quitar (uno o todos) y listar breakpoints (y el punto de vigilancia) |
| `w r\|w\|io dir` / `w` | punto de vigilancia de lectura, escritura o E/S (el byte bajo del puerto) / quitarlo |
| `v` | vídeo mientras está parado: Superfast texto sí / no (sección 13.4) |
| `v dir` | vídeo mientras está parado: Superfast HiRes con el mapa de bits en `dir` (sección 13.4) |
| `d [dir] [n]` | desensamblar n instrucciones (10, como mucho 60) desde dir (PC) |
| `m dir [n]` | volcado de n bytes (64, como mucho 256) |
| `e dir b1 b2 …` | escribir bytes (hasta 32) |
| `x reg=val` | cambiar un registro: `af bc de hl ix iy af' bc' de' hl' sp pc i r` (`SETREGS`) |
| `io puerto [val]` | leer o escribir un puerto |
| `snap [-a] [f]` | snapshot (sección 11); vale también con el programa en marcha (lo para y lo deja parado) |
| `h`, `?` | ayuda |

En el emulador lo natural es una ventana de consola, o reutilizar el
depurador que ya tenga, enviando las mismas órdenes.

### 10.6 Botón QuickSilva

Ya no bloquea el bucle del MCU mientras se mantiene. **Todo pasa al
soltarlo** (cambiado en la fase 2b); el LED avisa de lo que hará:
- **menos de 1 s:** cambia QuickSilva;
- **de 1 a 3 s** (LED magenta al llegar a 1 s): si el programa corre,
  pausa y sale la pantalla del depurador, con su teclado (secciones 11.6 y
  13.6); si está parado, continúa;
- **3 s o más** (LED amarillo al llegar a 3 s): snapshot con el nombre
  automático (sección 11). Si el programa corría, sigue al acabar. Desde
  que se suelta, el LED parpadea en amarillo hasta que acaba.

Sin el monitor cargado, cualquier pulsación cambia QuickSilva. En el
emulador conviene una tecla o un botón para el QuickSilva "mantenido".

### 10.7 Cómo probarlo

- `claude/dbgharness` (`sh build.sh`): los dos bancos del PC. `cosim.cpp`
  es la mejor referencia de cómo encajan el monitor, el MCU y la FPGA.
- En el emulador:
  1. cargar el explorador u otro programa en FAST o Superfast;
  2. `p` → tiene que salir el `*** STOP …` con los registros;
  3. `b` en una dirección por la que pase el programa y `c` → tiene que
     parar ahí;
  4. `c` otra vez → tiene que volver a parar en la vuelta siguiente;
  5. `s 10`, `d`, `m`, `x`;
  6. al final `bc` y `c`: el programa sigue sin rastro de los `FF`;
  7. lo mismo con un programa en SLOW (BASIC sirve): `p` tiene que parar en
     el programa (no en `$0066`) con `(SLOW)`, y al seguir la imagen vuelve;
  8. el botón: 1-3 s y `Z`, `L`, `S`, espacio; y 3 s varias veces seguidas,
     también en SLOW;
  9. `snap`, y `LOAD *Z81` del fichero, dos veces seguidas.

### 10.8 Fase 3: `o`, `u`, `g` y `w`

Todo en el MCU; la FPGA ya tenía el comparador (sección 6) y el monitor no
cambia (se programa con peticiones `OUT` al puerto `$3FEF`: `$80`/`$81`
dirección, `$82` modo).

- **`w`** usa el comparador en modo 2 (lectura), 3 (escritura) o 4 (E/S).
  Se queda puesto hasta `w` sin nada o un reset. Para con el motivo 6 en la
  instrucción siguiente a la del acceso.
- **`g dir`** ("ejecutar hasta"). Si el comparador está libre (sin `w`) y el
  programa no está en SLOW, lo pone en modo 1 (ejecución) en `dir`: no toca
  la memoria, así que vale con código automodificable o que todavía no se ha
  cargado. Si no, pone un `FF` temporal en la tabla de breakpoints (en SLOW
  el comparador solo mira la M1 exacta y la FPGA solo para en la NMI; el
  `FF` espera en `JR $` y sí para). **Al parar, por lo que sea**, el
  comparador vuelve a como estaba (el `w`, o apagado) y los `FF` temporales
  salen de la tabla. La consola dice `reached` en vez del motivo.
- **`o`** lee 4 bytes en el PC y los desensambla: si es `CALL`, `CALL cc`,
  `RST`, `DJNZ`, `HALT` o un repetido (`ED B0`-`B3`, `B8`-`BB`), `g` a la
  instrucción siguiente; si no, `s 1`.
- **`u`** lee `[SP]` y hace `g` a esa dirección. Vale si la rutina no ha
  metido nada en la pila desde que la llamaron.

En el emulador, lo mismo con su comparador; la prueba 13 de `cosim.cpp` lo
recorre (un `CALL` con `o`, `s` + `u`, `g`, `w` de escritura, y `o` con el
comparador ocupado, que usa un `FF` temporal).

## 11. Fase 2b: snapshots

### 11.1 Qué lo lanza

- La consola: `snap [-a] [nombre]`. Sin `-a`, las páginas mapeadas en los 8
  bloques y las que ha escrito el programa (sección 11.5); con `-a`, todas
  las páginas 0-62 (sin FULL_PAGING, solo hasta la 31: el mapper no llega
  más allá). Desde la consola el programa **se queda parado** (si corría,
  se pausa antes).
- El botón QuickSilva mantenido 3 s (sección 10.6): si el programa corría,
  sigue al acabar.
- Las teclas `S` y `Z` en la pausa del botón (sección 11.6).

Mientras graba, el LED parpadea en amarillo y la consola contesta
`Snapshot in progress`.

**Nombre:** el que se dé (con `.Z81` si no lleva extensión), en el
directorio actual. Si no, el nombre (sin extensión) del último fichero
cargado con `LOAD` más `001.Z81`, `002.Z81`… (sin separador: el ZX81
no tiene `_`; `EXPLORER.P` → `EXPLORER001.Z81`); sin nada cargado, `NONAME001.Z81`,
`NONAME002.Z81`… Se usa el primer número libre.

### 11.2 Cómo lee el MCU el estado

Todo con peticiones al monitor, con el programa parado:
1. el mapper: 8 `IN` del puerto `(b << 8) | $E7`, b = 0-7 (6 bits);
2. Chroma81: `OUT $03` y `IN` del puerto `$3FEF` (índice 3);
3. los AY de la FPGA: `OUT $04`/`$05` e `IN` (el registro elegido de cada
   uno), `AYREAD` de 16 registros por `$00CF` (A) y `$00C7` (B), y después
   `OUT` para volver a elegir el que estaba;
4. las páginas escritas: `OUT $06` e `INSEQ` de 64 (sección 11.5);
5. la BRAM de sombra (índice 2): puntero a 2038 (`$85`, bajo, `$86`, alto),
   `OUT $02` e `INSEQ` de 61 bytes (2038-2098, los POKEs de control); después
   el puntero a `$0C00` e `INSEQ` de 1024 (los sprites, sección 11.4) y el
   puntero a 2100 e `INSEQ` de 1 (el sprite elegido). La sombra guarda lo
   último que escribió la CPU en cada dirección. Si un POKE vale el byte de
   la ROM en esa dirección, se toma como **nunca escrito**: la sombra se
   carga con la ROM al arrancar;
6. `[MEMORY]`: `READ` de 256 bytes de `$2000` a `$FFFF` (en `$2000`-`$3FFF`,
   la página del programa, no la del monitor). Con cada trozo de
   `$2000`-`$BFFF`, un `INSEQ` de la sombra del mismo trozo (puntero
   seguido desde `$2000`) para saber qué bloques no coinciden (`SHADOW`);
7. `[COLOUR]`: `INSEQ` de la sombra de `$C000`-`$FFFF`;
8. las páginas: `READP`. El monitor pone la página en el bloque 7 un
   momento (`OUT (C),A` con B = página y A = `(página & 31) << 3 | 7`, que
   vale con y sin FULL_PAGING) y lo devuelve como estaba;
9. los bloques de la sombra que se guardan: puntero a `bloque * $2000` e
   `INSEQ` de 8 KB.

El MCU no puede leer los bits de configuración en la FPGA: guarda el último
valor que mandó con cada orden de configuración (`cfg_value`).

### 11.3 El fichero

El formato de EightyOne (`.Z81`), con las secciones `[MACHINE]`, `[CPU]`,
`[ZX81]`, `[MEMORY]`, `[COLOUR]`, `[SD81BOOSTER]` y `[EOF]`. El RLE es el
suyo: `VV ` o `*NNNN VV `, 16 tokens por línea.

- `[CPU]`: los registros del bloque de la parada. `IM 01` siempre (no se
  puede leer; la ROM pone IM 1). `IF1` = `IF2` = IFF2.
- `[ZX81]`: `NMI 01` si estaba en SLOW (bit 6 del estado), si no `NMI 00`.
- `[MEMORY]`: `MEMRANGE 2000 FFFF`, la memoria tal como la ve el programa,
  y `ROM_PROTECTED 00` (como EightyOne con el SD81 Booster: con `01`
  protegería `$2000`-`$3FFF`).
- `[COLOUR]`: `TYPE Chroma`, `$C000`-`$FFFF`, `CHROMA_MODE` y
  `COLOUR_ENABLED 01`. Es la **sombra** de `$C000`-`$FFFF` (de donde lee el
  vídeo los atributos), leída por el índice 2, no la memoria.
- `[SD81BOOSTER]`: `CUR_DIR`, `MAPPER` (8 páginas), `DISPLAY_MODE`,
  `BORDER_INK`, `BORDER_PATTERN`, `BORDER_CHARS`, `HFILE`, `CHROMA_MODE`,
  `DBUF`, `SEL128`, `SEL256`, `WRX`, `ROMLOCK`, `SCROLL`, sacados de los
  POKEs y de `cfg_value` con las reglas de EightyOne. `SCROLL`: las
  máscaras de filas valen `FF` si nunca se escribieron (como en la FPGA).
  También las claves que el emulador añadió para el explorador
  (`snapshots_cambios_emulador.md`), solo cuando hacen falta:
  - `WIDE_COLS 46` / `50`: el último `POKE 2045` fue 173 / 174;
  - `DISP_ADDR dir 01`: el último `POKE 2098` fue 170 (dir = 2097:2096);
  - `ATTR_ADDR dir 01`: el último `POKE 2061` fue 170 (dir = 2060:2059);
    ninguna de las dos si el último `POKE 2045` fue 85;
  - `DIR_OPEN arg`: el argumento del último `OPENDIR` que abrió el listado,
    en bytes hexadecimales (`-` si estaba vacío). Es estado del MCU
    (`dbg_note_opendir`, desde `cmd_opendir2`).

  Los AY, los sprites y el estado del MCU, con las claves de EightyOne:
  - `AY1_REGS` (16) / `AY1_REG_SEL`: el AY A de la FPGA (ZonX). Se leen
    con la petición 11 (AYREAD) y el índice 4; al acabar se vuelve a
    elegir el registro que estaba;
  - `AY3_REGS` / `AY3_REG_SEL`: el AY B de la FPGA (índice 5). **Clave
    nueva**: EightyOne no guardaba el chip B;
  - `AY2_REGS` / `AY2_REG_SEL 00`, `VGM_PATH` / `VGM_PLAYING` / `VGM_LOOP`
    / `VGM_POS`, `PEG_MEM` / `PEG_PC` / `PEG_ADDR` / `PEG_RUNNING` /
    `PEG_CARRY` / `PEG_VARS` y `FILE_HANDLE n ruta posición`: el estado
    del MCU (el AY que emula, el VGM, el PEG y los ficheros abiertos), en
    `MCUSTATE.cpp`. Las rutas son absolutas en la SD; una ruta con
    espacios no se guarda;
  - `SPRITE_SEL` y `SPRITE n en x y c0..c7 p0..p7 m0..m7` (solo los
    sprites que no están todos a cero): de la copia de los sprites en la
    sombra (sección 11.4). Un sprite cuyos 28 bytes siguen siendo los de la
    ROM en esa posición no se ha escrito nunca (una ROM sin la limpieza del
    reset) y no se guarda; tampoco `SPRITE_SEL` si es el byte de la ROM
    en 2100 (se guarda 00).

  Además, dos claves propias que EightyOne ignora (para `LOAD *Z81` en el
  hardware):
  - `HW_POKES`: los 61 valores de 2038-2098 en hexadecimal, `--` si nunca
    se escribió;
  - `HW_CFG`: MC45, MODE48K, QuickSilva, FULLPAG, 128CHARS, 256CHARS,
    ROMLOCK.
- `RAM_PAGE nn` … `RAM_PAGE_END` por cada página, salvo:
  - las páginas 0 y 1 si siguen siendo la ROM tal como la cargó el MCU;
  - con `-a`, las páginas todas a `FF`;
  - la 63 (el monitor), nunca.
- `SHADOW n` … `SHADOW_END` (como EightyOne): los bloques de la sombra que
  no son la memoria que ve el programa. El snap compara cada trozo de
  `[MEMORY]` de `$2000`-`$BFFF` con su sombra (`$C000`-`$FFFF` ya va en
  `[COLOUR]`). El bloque 0 se guarda si no es la ROM, sin contar 2038-2128
  ni la copia de los sprites, que ya van en sus claves (en la práctica,
  solo con CP/M).

### 11.4 Los sprites en la sombra

Los POKEs 2101-2128 son los mismos para los 32 sprites (el 2100 elige
cuál), así que la sombra, que guarda lo último escrito en cada dirección,
no servía para reconstruirlos. Ahora la FPGA no escribe esos POKEs en su
dirección de la sombra, sino en la **copia**:

```
$0C00 + sprite * 32 + campo        (campo = dirección - 2101, de 0 a 27)
```

32 sprites × 32 bytes = `$0C00`-`$0FFF` (los bytes 28-31 de cada uno no se
usan). Son bytes de la sombra del bloque 0 que el vídeo no lee. El POKE
2100 sí va a su dirección. Solo cuenta cuando el POKE llega a los sprites
(no con ROMLOCK ni con el bloque 0 escribible).

Campos: 0 enable (bit 0), 1 X bajo, 2 X alto (bit 0), 3 Y, 4-11 color por
fila, 12-19 píxeles, 20-27 máscara. En la clave `SPRITE`, `x` es
`X bajo | (X alto & 1) << 8`.

**La ROM pone los 32 sprites a cero en cada reset** (`SD_RESET`, después
de copiar los juegos de caracteres): el reset de la FPGA solo los apaga, y
así la copia empieza limpia. El emulador tiene que hacer lo mismo: o
ejecuta esa ROM, o al reset pone sus sprites y la copia a cero.

### 11.5 Las páginas escritas

Para que un snapshot normal (sin `-a`) guarde lo que usa el programa sin
tener que elegir, la FPGA lleva un bit por página (64): se pone a 1 cuando
una escritura de la CPU llega a la SRAM en esa página (la de `sram_page`,
con la ventana del monitor incluida). Las escrituras del MCU con el Z80 en
reset no cuentan. Es una RAM distribuida de 64×1 (con 64 registros no
cabía en la FPGA). Se lee por `$3FEF`, índice 6: `OUT $06` y 64 `IN`
seguidos (el MCU usa INSEQ), uno por página, en el bit 0.

La **orden 11** del canal de configuración las borra: al pasar a 1, un
contador recorre las 64 posiciones (64 ciclos del Z80). El MCU manda 1 y
después 0. El MCU las borra:
- al cargar un programa (`LOAD`, en `dbg_note_loaded`);
- al empezar a cargar un snapshot (las escritas pasan a ser las de la
  carga);
- en el reset del Z80.

El snapshot guarda las mapeadas más las escritas (menos la 63). Con una FPGA
anterior los índices dan 0 y quedan solo las mapeadas.

En el emulador: un bit por página que se pone al escribir la CPU en ella,
el índice 6 con su puntero y la orden 11.

### 11.6 La pausa del botón QS: teclado del ZX81

Cuando la pausa la pide el botón QS (soltarlo entre 1 y 3 s con el
programa en marcha), sale la pantalla del depurador (sección 13.6): así se
ve que está parado. Con `V` (o `Q`) se cambia por la pantalla del programa,
y ahí valen las teclas de esta tabla. El MCU lee el teclado por el monitor
cada 40 ms: las 8 filas, con `IN $xxFE` (nada se escribe en la memoria del
programa). Cada tecla cuenta **al soltarla**, y solo si
se ha pulsado durante la pausa (las que ya estaban pulsadas al parar no
valen): así el programa no la ve al seguir.

| Tecla | Qué hace |
|---|---|
| `S` | snapshot (nombre automático) y el programa sigue |
| `Z` | snapshot y se queda parado (el teclado sigue activo) |
| `L` | carga el último snapshot grabado en esta sesión (desde el encendido) |
| `C`, `ESPACIO` | sigue |
| `V` | la pantalla del depurador (sección 13.6); desde ahí, `V` vuelve aquí con Superfast texto y `Q` sin él |

En la del depurador, `Z`, `L`, `C` y espacio hacen lo mismo; `S` es un paso.

`L` carga sin pasar por la ROM: el programa ya está parado en el monitor.
Como la ROM no pone el modo de interrupción, el MCU lo manda con los
registros: `SETREGS` de **31 bytes**, con el IM (0-2) en el byte 30. El
monitor lo pone al salir (`im_req`, `$FF` = no tocar). El resto de la carga
es la de la sección 12.

### 11.7 En el emulador

Lo más sencillo es generar el `.Z81` con su propio guardado, con las mismas
claves (incluidas `HW_POKES`, `HW_CFG`, `AY3_*`, `SPRITE` y `SHADOW` si
quiere ser fiel). Si emula el monitor y el protocolo, hacen falta los
índices 2-6 del puerto `$3FEF`, los registros 5-7, las peticiones 7-12, la
copia de los sprites (11.4) y las páginas escritas (11.5). `cosim.cpp` lo
modela y compara cada `.Z81` con la memoria emulada. La carga está en la
sección 12.

## 12. Carga de snapshots (`LOAD *Z81` con el monitor)

`LOAD *Z81 "f"` carga el snapshot por el monitor del depurador. El monitor
corre en la página 63, así que se puede escribir cualquier página, también
la 1, que es donde se ejecuta la ROM. La salida del monitor (`SETREGS` +
`CONT`) deja todos los registros, R incluido, como estaban. Sin monitor se
usa el cargador de siempre (comando 70, `z81_snapshot_loading.md`).

### 12.1 La FPGA

- **Registro 7 del puerto `$3FEF`:** `OUT $87` y después el dato. El dato
  se escribe en la BRAM de sombra, en el puntero de los registros 5 y 6, y
  el puntero avanza 1. Es la escritura que corresponde a la lectura del
  índice 2.
- **Orden 10 del canal de configuración (`dbg_poke`):** mientras está a 1,
  el monitor escribe como el programa:
  - el bloque 0 vuelve a estar protegido;
  - las escrituras en las direcciones de los POKEs de control disparan sus
    registros;
  - todo lo que escribe se copia a la BRAM de sombra.

  Fuera del monitor no hace nada. La pone y la quita el MCU.

### 12.2 El monitor (versión 3: `"SD81DBG",3` en `$2003`)

`SETREGS` acepta 30 bytes o 31, con el modo de interrupción en el último
(sección 11.6).

| op | Nombre | Dirección | n | Después | Qué hace |
|---|---|---|---|---|---|
| 9 | WRITEP | desplazamiento en la página | 1-256 | la página (1 byte) y n datos | escribe n bytes en la página, mapeándola un momento en el bloque 7 (la inversa de READP) |
| 10 | BRAMW | 0 | 1-256 | n datos | por cada byte, `OUT $87` y el dato al puerto `$3FEF`: la BRAM de sombra |
| 11 | AYREAD | puerto de elección del AY (`$00CF` el A, `$00C7` el B) | 16 | — | para i = 0..n-1: `OUT (puerto),i` e `IN (puerto)` → n bytes |
| 12 | AYWRITE | puerto de elección | 16 | n datos | para i = 0..n-1: `OUT (puerto),i` y el dato al puerto de datos (el mismo con A7 = 0) |
| 13 | SETI (versión 4) | 0 | I (byte bajo) | — | I mientras el monitor espera; 0 = la del programa. Se queda puesta (ver abajo) |

**SETI** es para la pantalla del depurador (fase 3b del plan). En Superfast,
la FPGA saca la fuente de I, porque `ROMTABLE[15:8]` copia el byte alto de
la dirección en cada refresco. Parado, I es la del programa, que en un WRX
puede ser cualquier cosa. Con `SETI $1E` se ve la fuente de la ROM.

- El valor se guarda en el monitor (`ui_i`) y se queda puesto: cada entrada,
  después de guardar la I del programa en el bloque (byte 24), vuelve a
  poner esa I, hasta un `SETI 0`. Así no parpadea entre pasos.
- Va después del `LD A,R`, así que `R_IN` no cambia.
- `CONT` siempre repone la I del bloque: el programa nunca la ve.
- En el emulador basta con que I salga en la dirección de los ciclos de
  refresco y la FPGA emulada la siga, como en el hardware.

Además, un `OUT` (petición 4) a un puerto con el byte bajo `$E7` vuelve a
leer las páginas de los bloques 1 y 7. Las lecturas y escrituras de
`$2000`-`$3FFF`, las de READP/WRITEP y la salida usan las páginas nuevas.

### 12.3 El comando 75 y la ROM

```
Z80 -> MCU:  75, longitud, nombre        (como el 70)
MCU -> Z80:  estado, IM
```

El MCU lee el fichero entero antes de contestar: lo valida y apunta dónde
empieza cada sección. El Z80 espera mientras tanto.

| Estado | Qué hace la ROM |
|---|---|
| 0 | `DI`, `OUT ($FD),A` (NMI apagada), `IM` del snapshot, trampa (`OUT $10` a `$3FEF`) y `JR $`. No vuelve: el MCU carga todo al parar y salta al snapshot. Si venía de SLOW, la trampa espera hasta que la FPGA ve FAST (un cuadro sin NMI, 20 ms). |
| `0xFF` | Sin monitor: vuelve a mandar el nombre con el comando 70 (el cargador de siempre). |
| 1-3 | Error, como el 70: 1 sin fichero, 2 sin `[MEMORY]`, 3 fichero mal hecho (`REPORT-G`, `-H`, `-I`). |

### 12.4 Lo que hace el MCU al parar

En este orden:

1. **Estado del MCU.**
   - Se borran las páginas escritas (orden 11): las de la carga serán las
     nuevas.
   - `CUR_DIR` pasa a ser el directorio actual.
   - **Silencio mientras carga** (`mcustate_quiet`): se paran el VGM y el
     PEG, los tres canales del AY del MCU se quedan sin volumen y los dos
     AY de la FPGA se escriben a cero (petición 12). La carga es larga y el
     MCU está ocupado mandando la memoria: un VGM que siguiera sonando se
     arrastraría. El estado del snapshot se pone al final (paso 5).
   - `DIR_OPEN` vuelve a abrir el listado (como `OPENDIR`).
   - Los bits de `HW_CFG`: FULLPAG, MC45, MODE48K, QuickSilva, 128 y 256
     caracteres. Sin `HW_CFG` valen `SEL128`/`SEL256` o, en un `.Z81`
     clásico, `[CHR$_GENERATOR]`.
   - ROMLOCK se apaga mientras dura la carga: con él los POKEs no hacen
     nada.
   - Sin `MAPPER`, el MCU lee el mapper de ahora y se usan esas páginas.
2. **Los POKEs de control,** con la orden 10 encendida y peticiones
   `WRITE` normales. Con `HW_POKES` se usan sus valores. Los `--` (nunca
   escritos) se vuelven a su valor de reset:

   | POKE | Valor de reset |
   |---|---|
   | 2038-2040, 2043-2044, 2059-2060, 2090, 2094-2097 | 0 |
   | 2045, 2047, 2057, 2058, 2061, 2062, 2098 | 85 |
   | 2046 | 15 |
   | 2048-2055 | `00 3C 42 42 7E 42 42 00` |
   | 2091-2093 | 255 |

   2041, 2042 y 2056 no se tocan. 2056 no se puede deshacer: si el bloque
   0 ya estaba desprotegido, se queda así. Sin `HW_POKES` (un `.Z81` de
   EightyOne), los valores salen de sus claves:
   - `DISPLAY_MODE` y `WIDE_COLS` van a 2045;
   - `BORDER_*` a 2046-2055 y `HFILE` a 2043-2044;
   - `DBUF` a 2057 (`80h` → `168+blk` o `200+blk`, según el bit 6);
   - `WRX` a 2058 y `SCROLL` a 2090-2093;
   - `DISP_ADDR` a 2096-2098 y `ATTR_ADDR` a 2059-2061.

   **Orden:**
   1. 2045 el primero (85 apaga los D_FILE y atributos alternativos);
   2. el resto de menor a mayor;
   3. 2038-2039 y 2040 (interrupciones simuladas);
   4. 2056 el último.

   Después, **los 32 sprites**: para cada uno, POKE 2100 = n y sus 28
   campos (2101-2128) con los de su línea `SPRITE`, o a cero si no viene.
   Al final, 2100 = `SPRITE_SEL`. Con la orden 10 encendida, todo esto
   también deja la copia de los sprites en la sombra.

   Una petición `IN` (puerto `$00E7`) al final hace de barrera: cuando
   llega su resultado, el MCU apaga la orden 10.
3. **Las páginas:** cada `RAM_PAGE` con WRITEP. La 63 nunca: es la del
   monitor.
4. **`[MEMORY]`:**
   - cada bloque cuya página no venga en `RAM_PAGE` se escribe en su
     página con WRITEP;
   - todo (`$2000`-`$FFFF`) se escribe en la BRAM de sombra con BRAMW. El
     vídeo Superfast lee de la sombra, así que tiene que ser lo que ve el
     programa.

   Después, en la sombra:
   - `[COLOUR]` en `$C000`-`$FFFF`, si está;
   - la página del bloque 0 en `$0000`-`$1FFF`, si viene en `RAM_PAGE`
     (CP/M);
   - en 2038-2098, el valor de los POKEs escritos y el byte de la ROM en
     los demás, como al arrancar. Así el siguiente `snap` los vuelve a ver
     como `--`;
   - los bloques `SHADOW`, encima de todo. Con `SHADOW 00` no hace falta
     lo de 2038-2098: el bloque 0 viene tal cual.

   La página del bloque 0 (CP/M) no pisa la copia de los sprites
   (`$0C00`-`$0FFF`).
5. **El final:**
   - el mapper con `OUT` (con FULLPAG, la página en el byte alto del
     puerto);
   - Chroma81 (`OUT $7FEF`);
   - los AY de la FPGA (petición 12) y su registro elegido (`OUT`). Al
     escribir R13 se reinicia la envolvente, igual que en EightyOne;
   - ROMLOCK como diga el snapshot;
   - `SETREGS` con los registros de `[CPU]`, `IF1` como IFF2;
   - el AY del MCU, el VGM (en su posición), el PEG y los ficheros
     abiertos (`mcustate_apply`). Sin claves de VGM o PEG se quedan
     parados; los ficheros abiertos de antes se cierran;
   - `CONT`, por el camino normal de los breakpoints.
6. **NMI.** Si `[ZX81]` trae `NMI 01`, la vuelta pasa por un trozo de
   código que el MCU escribe debajo de la pila del snapshot (S = su SP):
   - `S-8`: `D3 FE C9` (`OUT ($FE),A` / `RET`);
   - `S-2`: el PC del snapshot;
   - el MCU pone `PC = S-8` y `SP = S-2`, y descuenta 2 de R (las dos M1
     del `OUT` y el `RET`).

   La NMI no se puede encender en el monitor: con ella en marcha el
   depurador no podría parar, y la rutina de la ROM cambia AF'.

**Bytes que cambian:** como en cada parada, los 2 bytes debajo de SP (la
vuelta del monitor, PC+1). Con `NMI 01`, los 8 bytes debajo de SP.

### 12.5 En el emulador

- **Lo mínimo:** contestar el comando 75. Con `0xFF` se queda todo como
  antes (comando 70).
- **Para cargar como el hardware:**
  - el registro 7 y la orden 10 (sección 12.1);
  - las peticiones 9 a 12 del monitor y la relectura del mapper (12.2);
  - los índices 4 y 5 y la copia de los sprites (11.4);
  - el comando 75 con la lógica del MCU (12.4).

  O, si es más cómodo, el emulador puede cargar el `.Z81` con su propio
  cargador cuando recibe el 75, siempre que deje lo mismo:
  - páginas y mapper;
  - la sombra igual a la vista del programa;
  - los registros de los POKEs;
  - los registros de la CPU, saltando al PC sin pasar por la ROM.
- **Referencia:** `claude/dbgharness/cosim.cpp`, pruebas 8 a 10:
  - carga un `.Z81` propio y comprueba registros, mapper, páginas, sombra
    y POKEs (con su orden);
  - lo mismo con `NMI 01`;
  - un `.Z81` de EightyOne sin `MAPPER` ni `HW_POKES`.

## 13. Programas en SLOW (fase 4, FPGA rev 0.12)

En SLOW la NMI salta en cada línea (cada 207 ciclos) y su rutina (`$0066`)
usa AF' como contador de líneas. Si saltara mientras el monitor entra (con
AF y AF' intercambiados para guardarlos), lo corrompería. Y mientras la ROM
dibuja las 192 líneas apaga la NMI pero deja la INT habilitada (una por
línea, a `$0038`, cuya rutina hace `POP HL`): si se para ahí, la INT entra
en el monitor y le rompe la pila.

### 13.1 La FPGA

- `nmi_age`: ciclos desde la última M1 en `$0066`, 16 bits, hasta `$FFFF`.
  **SLOW** es `nmi_age != $FFFF`: ha habido una NMI hace menos de un cuadro
  (las 192 líneas sin NMI son unos 40000 ciclos). No se usan los `OUT
  ($FE)`/`($FD)`: la ROM los hace en cada cuadro.
- En SLOW **solo se rompe en la primera M1 de la NMI** (`$0066`): la rutina
  aún no ha hecho nada (AF' intacto) y la siguiente NMI está a unos 200
  ciclos, tiempo de sobra para que el monitor la apague. Mientras se dibuja
  no hay NMI y no se rompe. En FAST, en cualquier instrucción, como antes.
- Un `FF` (breakpoint) en SLOW fuera de `$0066` no se puede dejar pasar
  (sería un `RST 38h` de verdad): la FPGA sirve `JR $` (`18`, y `FE` en la
  lectura del operando), la CPU vuelve a él y, al llegar la NMI, se rompe
  en `$0066` con el motivo 7 (`spin_pend`).
- Bit 6 del estado (índice 0): el programa estaba en SLOW al parar.

(Las revisiones 0.10 y 0.11 usaban una ventana de tiempo tras la NMI; la
0.10 se colgaba al parar mientras se dibujaba y la 0.11 a veces no llegaba
a parar.)

### 13.2 El monitor (1256 bytes)

Su primera instrucción es `OUT ($FD),A`: apaga la NMI sin tocar ningún
registro. `R_IN` pasa a 10 (una M1 más).

### 13.3 El MCU

- Al parar, `stop_nmi` = bit 6 del estado. Si el PC es `$0066`, **deshace
  la NMI** antes de nada: lee `[SP]` y pone `PC = [SP]`, `SP + 2` y R - 1
  (la M1 del reconocimiento), y se lo manda al monitor con `SETREGS` (si
  no, un paso volvería a `$0066`). La consola y los snapshots ven el
  programa como si la NMI no hubiera llegado; la ROM pierde una línea de su
  cuenta (como mucho, un cuadro movido al seguir).
- `prog_slow`: el programa es SLOW (se pone al parar con el bit 6 y se
  mantiene hasta que se sigue de verdad). La consola añade `(SLOW)`.
- **Al seguir de verdad** (`c`, `g`, `o` sobre un `CALL`, `u`, `S` en la
  pausa del botón), si `prog_slow`, vuelve por `OUT ($FE),A` / `RET`
  escritos debajo de la pila (8 bytes; como la carga con `NMI 01`, sección
  12.4): `PC = SP-8`, `SP = SP-2`, R menos 2.
- **Los pasos** (`s n`, `o` sobre una instrucción normal, y el paso para
  saltar un breakpoint en el PC) **se dan con la NMI apagada**: son exactos
  (sin NMI no hay pantalla ni rutina de vídeo que se cuele). Tras 20 ms
  parado la FPGA ya ve FAST y para en la instrucción justa. Al parar de un
  paso el bit 6 sale a 0, pero `prog_slow` sigue y el siguiente `c` vuelve
  a encender la NMI. Consecuencia: un bucle que espera a la rutina de vídeo
  (el BASIC esperando una tecla en `$04CF`, que mira el bit 0 de CDFLAG) no
  sale paso a paso; hay que usar `c` o `g`.
- `g`/`o`/`u` en SLOW usan un `FF` temporal (sección 10.8), que espera en
  `JR $` a la NMI.
- El snapshot de un programa SLOW (`prog_slow`) guarda `NMI 01`.

### 13.4 Mientras está parado: la orden `v`

Parado, un programa en SLOW o en FAST no tiene imagen (la NMI está
apagada): la pantalla se queda en negro hasta continuar. Con **`v`** la FPGA
la pinta en Superfast texto desde la BRAM de sombra, donde está todo lo que
ha escrito la CPU (el D_FILE incluido). Solo con la orden: no se garantiza
que valga para todos los programas.

- **Al activarla**, el MCU lee de la sombra los POKEs de control (2038-2098,
  índice 2). Si el 2045 ya vale 170-174 (el programa es Superfast), no hace
  nada. Si no, `POKE 2045,170` con la orden 10 (sección 12.1), y una `IN`
  de barrera para quitar la orden 10.
- **Los pasos** (`s`, `o` sobre una instrucción normal) la mantienen: se ve
  cómo cambia la pantalla.
- **Se quita** con otra `v`, al seguir de verdad (`c`, `g`, `o` sobre un
  `CALL`, `u`, `S`: antes de nada, también antes de poner los breakpoints)
  y antes de un snapshot (el `.Z81` guarda el vídeo del programa):
  - `POKE 2045` con su valor de antes, o 85 si nunca se escribió (si el
    byte de la sombra es el de la ROM);
  - 85 apaga el D_FILE y los atributos alternativos: si en la sombra 2098 o
    2061 valían 170, se vuelven a poner;
  - el byte de la sombra en 2045, como estaba (BRAMW de un byte), para que
    el siguiente `snap` lo vea igual.
- La carga de un snapshot la descarta (repone los POKEs).

Limitaciones: un D_FILE comprimido (1 K) sale desordenado; un programa con
su propia rutina de vídeo (pseudo hi-res, WRX…) enseña su D_FILE, no lo que
pinta su rutina: para esos, `v dir`.

**`v dir`: Superfast HiRes.** Para los programas WRX: el mapa de bits
(32 bytes × 192 líneas seguidas, 6144 bytes) en `dir`, que se da a mano
(el depurador no lo deduce del registro I). La FPGA pinta el HiRes desde el
principio de un bloque de 8 K (`scan_addr = {vpage, hr_addr}`, con
`vpage = HFILE[15:13]`):

- **`dir` alineada a 8 K** (`$2000`, `$4000`, …): `POKE 2043/2044` (HFILE) =
  `dir` y `POKE 2045,171`, con la orden 10 y la barrera. La sombra no cambia.
- **`dir` sin alinear**: el bloque es `B = dir AND $E000`. El MCU
  1. guarda los 6144 bytes de la sombra desde `B` (24 `INSEQ` de 256, índice 2);
  2. lee el mapa de bits de la memoria desde `dir` (24 `READ` de 256);
  3. lo escribe en la sombra desde `B` (24 `BRAMW`);
  4. HFILE = `B` y `POKE 2045,171`.

  Mientras copia, la consola y el botón QS esperan (`Video: copying the
  bitmap`). Con `dir` por debajo de `$2000` solo vale `0000` (la copia
  taparía la sombra de la ROM y los POKEs).
- `dir + 6144` tiene que caber en 64 K.
- Al ser HiRes siempre se permite, aunque el programa ya sea Superfast.
- **Al quitarla**, además de lo de `v`: HFILE con su valor de antes (0 si
  nunca se escribió: el byte de la sombra es el de la ROM), la sombra de
  2043-2045 como estaba y, si hubo copia, los 6144 bytes guardados de vuelta
  a la sombra desde `B` (24 `BRAMW`).
- Con el doble búfer (`dbuf_en`) la FPGA pinta su bloque, no el de HFILE.
- El mapa de bits es el de cuando se pidió: si un paso lo cambia y estaba
  copiado, no se ve hasta otra `v dir`. Alineado sí se ve (se pinta la
  sombra).

**Encontrar `dir`.** El registro I no sirve: en WRX la rutina de vídeo
carga I (y R) en cada línea, y fuera de ella (donde para el depurador) I
vale cualquier cosa. La pista es **IX**: el vídeo del ZX81 vuelve con
`JP (IX)`, y un programa WRX apunta IX a su rutina HiRes. Desensamblando
desde IX suele aparecer un `LD HL,nnnn` (o `LD HL,(var)`) seguido de
`LD A,H` / `LD I,A`: `nnnn` es el principio del mapa de bits.

**Límites conocidos.** `v dir` supone el formato WRX básico: 32 bytes por
línea, 192 líneas seguidas. No sirve (la imagen sale inclinada, incompleta
o mezclada) con:

- líneas de otro ancho (33 o 34 bytes);
- menos líneas (los WRX de 1 K);
- varias zonas de pantalla, o rutinas que cambian I y R a valores que no
  siguen un único mapa de bits (pseudo hi-res con tablas de caracteres,
  mezcla de texto y HiRes).

Al probarlo en hardware con programas WRX reales no se encontró la
dirección (todo apunta a estas variantes): la orden se da por buena tal
como está, sin ampliarla.

### 13.5 En el emulador

- `nmi_age` (16 bits), romper solo en la M1 de `$0066` en SLOW, `JR $` para
  un `FF` y `spin_pend`, el bit 6.
- El monitor nuevo (1256 bytes) ya hace el `OUT ($FD),A`.
- `v` y `v dir` son solo del MCU: el emulador no tiene que hacer nada nuevo
  si ya pinta el Superfast desde la sombra (`2045` = 170 / 171, HFILE en
  2043/2044 con `vpage = HFILE[15:13]`), si la orden 10 hace que las
  escrituras del monitor cuenten como del programa (POKEs y sombra) y si
  `BRAMW` escribe en la sombra en su puntero. Si la consola del depurador
  está en el emulador, `v dir` es: leer 2038-2098 de la sombra; alineada,
  HFILE = `dir` y 171; sin alinear, guardar 6144 bytes de la sombra desde
  `dir AND $E000`, copiar ahí el mapa de bits desde la memoria y HFILE =
  ese bloque; al quitarla, reponer 2043-2045 (valor y sombra) y los 6144
  bytes.
- Referencia: `claude/dbgharness/cosim.cpp`, prueba 12: con una NMI cada 40
  instrucciones (una rutina en `$0066` que cuenta en A'), la pausa y un
  breakpoint en SLOW.

### 13.6 La pantalla del depurador (`ui`)

Mientras el programa está parado, una pantalla de 80 × 24 en el propio
ZX81 con lo mismo que la consola, sin necesitar el PC:
- registros, breakpoints y punto de vigilancia;
- desensamblado con el PC en inverso, y la pila;
- volcado de memoria (3 líneas);
- una línea para lo que se teclea o lo que ha pasado;
- dos de teclas.

Sale sola en la pausa del botón QS (sección 11.6), o con `ui` en la
consola. Desde ella, `V` y `Q` pasan a la pantalla del programa con el
teclado de la pausa (y `V` vuelve); `ui` la quita del todo. Una vez
abierta, al seguir se quita y en cada parada (breakpoint, paso, `G`...)
vuelve. Con el programa en marcha,
`ui` decide si sale en la parada siguiente. La carga de un snapshot la
quita. No hace falta FPGA nueva: es Superfast de 80 columnas desde
la sombra.

**Cómo se pone** (todo por el monitor, con la orden 10 para los POKEs):
1. Lee los POKEs de control de la sombra (2038-2098), el Chroma (índice 3)
   y guarda los 1945 bytes de la sombra en `$0000` (índice 2).
2. Lee 64 bytes desde la ventana del desensamblado, 24 de la pila y 64 del
   volcado, compone la pantalla y la escribe con `BRAMW` en `$0000`. Es un
   D_FILE de 80 columnas: 1 byte de relleno al principio y 24 filas de 81
   (80 caracteres y 1 de relleno). Caracteres del ZX81, inverso con el
   bit 7.
3. Detrás, el vídeo:
   - los 128/256 caracteres fuera (la fuente sería otra);
   - el Chroma sin color (bit 5 a 0);
   - `SETI $1E` (la fuente de la ROM);
   - `POKE 2045,174` (80 columnas, caracteres de 7 píxeles);
   - `POKE 2096,$00`, `2097,$00`, `2098,170` (D_FILE alternativo en
     `$0000`);
   - y la barrera.

**En cada parada** vuelve a leer y solo manda los trozos que cambian.

**Al quitarla:**
- 2096/2097 a su valor (0 si nunca se escribieron) y 2098 a 170 u 85;
- 2045 a su valor (85 si nunca se escribió); con 85, 2098 y 2061 otra vez
  si estaban;
- el Chroma, los 128/256 caracteres y `SETI 0`;
- la sombra de 2045, de 2096-2098 y de `$0000`, como estaban.

Tras un reset con la pantalla puesta, en la siguiente parada el MCU mira la
sombra de los POKEs y, si sigue como la dejó la pantalla, la repone.

**La zona `$0000`-`$0798`** (hasta 0.12, `$1000`; ahora ahí va la traza,
sección 13.9): la FPGA no lee ahí (solo las celdas del D_FILE, los
atributos, la fuente según I y el HiRes), el programa no escribe en la zona
de la ROM, y acaba antes de los POKEs (2038).

**Teclado** (8 filas cada 40 ms; cuenta al soltar, y lo que ya estaba
pulsado al abrirla no cuenta hasta soltarlo). Las direcciones se teclean en
hex: con 4 cifras se acepta sola, con menos hace falta ENTER, y cualquier
otra tecla cancela.

| Tecla | Qué hace |
|---|---|
| `S` | un paso |
| `O` | paso por encima |
| `U` | salir de la rutina |
| `C`, espacio | seguir |
| `G` dir | ir hasta `dir` |
| `B` dir | pone o quita un breakpoint (`B` ENTER: en el PC) |
| `W` `R`/`W`/`I` dir | punto de vigilancia de lectura, escritura o E/S (`W` ENTER lo quita) |
| `R` reg valor | un registro: `P` PC, `S` SP, `A` AF, `B` BC, `D` DE, `H` HL, `X` IX, `Y` IY (4 cifras), `I` I (2) |
| `E` dir bytes ENTER | escribe bytes (2 cifras cada uno) desde `dir` |
| `D` dir | desensamblar desde `dir` hasta la parada siguiente (`D` ENTER: el PC) |
| `M` dir | volcado desde `dir` (`M` ENTER: HL) |
| `5` `6` `7` `8` | volcado: −48, +16, −16, +48 |
| `V` | la pantalla del programa: la suya si es Superfast; si no, Superfast texto de su D_FILE, como `v`. Ahí, las teclas de la pausa del botón QS (sección 11.6): `V` vuelve |
| `Z` | snapshot (se queda parado; la pantalla vuelve) |
| `L` | carga el último snapshot grabado en la sesión |
| `Q` | la pantalla del programa, como estaba (sin Superfast texto); `V` vuelve |

**En el emulador** hace falta:
- el modo de 80 columnas (2045,174) con el D_FILE alternativo;
- que la fuente siga a I en los refrescos;
- `SETI` en el monitor (sección 12.2);
- y nada más.

Referencia: la prueba `ui` de `cosim.cpp`.

### 13.7 Símbolos (`sym`)

Solo el MCU; el emulador no tiene que hacer nada.

- **El fichero:** el de símbolos de pasmo (`pasmo prog.asm prog.bin
  prog.sym`), con líneas `NOMBRE<tab>EQU 0ABCDH`. También valen
  `NOMBRE: EQU $ABCD` y `NOMBRE = 0x1234`; lo que va detrás de `;` no
  cuenta.
- **Cuántos:** hasta 2048, con nombres de hasta 17 caracteres. Van en la CCM
  RAM del STM32, ordenados por valor; con valores repetidos manda el primero
  del fichero.
- **Carga automática:** al cargar un programa con `LOAD`, el MCU apunta su
  nombre con `.SYM` (`MAZOGS.P` → `MAZOGS.SYM`, en la misma carpeta). En la
  parada siguiente lo lee; si no existe, quita los símbolos que hubiera.
- **Carga a mano:** `sym fichero` (relativo a la carpeta actual), `sym -`
  los quita, y `sym` solo dice cuántos hay.
- **Dónde se ven:**
  - en el desensamblado de la consola, la etiqueta en su propia línea
    (`START:`);
  - en la pantalla, en una columna;
  - en los dos, los operandos de 4 cifras (`CALL SUB1`, `LD (DATA),A`);
  - en la línea de parada y en el título de la pantalla, `STOP at 6212
    (SUB1+2)`: el símbolo anterior a menos de 256 bytes, si vale `$0100` o
    más.
- **Direcciones por nombre:** cualquier dirección de la consola (`b`, `bc`,
  `g`, `w`, `d`, `m`, `e`, `v`, `x reg=`) puede ser un símbolo, sin
  distinguir mayúsculas, o `símbolo+n` (n en hex). El símbolo gana al hex;
  `$BEEF` fuerza el hex.

### 13.8 Traza lenta (`t`)

Solo el MCU; el emulador no tiene que hacer nada. La traza de verdad (la
FPGA apuntando cada instrucción) necesitaría BRAM, que está a 32/32: esta
la hace el MCU paso a paso, a la velocidad de un paso y una lectura por
instrucción.

- **`t [n]`** (n en hex) da pasos de uno en uno. Antes de cada instrucción
  apunta el PC, AF, BC, DE, HL, IX, IY, SP y sus 4 bytes, leídos en ese
  momento, en un anillo de las últimas 1024 (en la CCM RAM, detrás de los
  símbolos, que pasan a nombres de 17 caracteres). Cada `t` empieza una
  nueva.
- **Se para:**
  - al llegar a `n` instrucciones;
  - en un breakpoint: el PC está en uno y no se ejecuta (durante la traza
    los `FF` no están en memoria; se compara el PC);
  - con un punto de vigilancia u otra ruptura que no sea un paso;
  - con ESPACIO en el ZX81 (un `IN $7FFE` cada 100 ms);
  - con cualquier orden en la consola;
  - con el botón QS.

  Un `RST 38h` del propio programa se ejecuta como siempre. Al parar dice
  cuántas ha hecho, en cuánto tiempo y por qué, y sigue como una parada
  normal.
- **`th [n]`:** las últimas n (14 por omisión), con los registros de antes
  de cada una. La última es la que se ejecutó justo antes de parar.
- **En la pantalla:** `T` y el número (ENTER: hasta un breakpoint); `H`
  cambia el desensamblado por las 12 últimas de la traza (PC, instrucción,
  AF, HL y SP). El teclado de la pantalla no se lee mientras traza.
- En SLOW los pasos van con la NMI apagada, como siempre.

### 13.9 Historial: la traza de la FPGA (`sim_int` 0.13)

Con la **orden 12** del canal de configuración puesta (el MCU la pone al
cargar el monitor; `tron` / `troff` en la consola), cada instrucción del
programa apunta su PC en la BRAM de sombra, sin frenar nada: el programa va
a su velocidad y, al parar por lo que sea, están las últimas 1024.

- **Dónde:** un anillo de 1024 entradas de 2 bytes (bajo, alto) en
  `$1000-$17FF`, la sombra de la ROM, que nadie lee. Entrada k en
  `$1000 + 2k`. El puntero (la entrada siguiente, 10 bits) se lee por
  `$3FEF`: índice 7 los 8 bajos, índice 8 los 2 altos. No hay forma de
  borrarlo: tras encender, hasta la primera vuelta, las entradas que
  faltan tienen lo que hubiera (los bytes de la ROM).
- **Qué se apunta:** las M1 que empiezan una instrucción del programa
  (`prog_m1`: con `boundary` de `m1_tracker`, así un `CB 47` o un
  `DD 21 nn nn` es una entrada). No se apuntan:
  - las M1 que sirve la FPGA ni las del monitor;
  - las que no se ejecutan: la del `FF` de una ruptura o de una
    interrupción simulada (`take`, `sim_inj`) y el `JR $` del SLOW (`spin`);
  - las del vídeo: las que el ZX81 cambia por `NOP` (`A15` y `D6 = 0` con
    `/HALT` alto);
  - las de un `HALT` de verdad (`/HALT` bajo sin que lo baje MC45).

  Las de la rutina de la NMI y del vídeo de la ROM en SLOW sí se apuntan:
  son instrucciones de verdad.
- **Cómo** (hardware): `sim_int` decide en la subida de T2, con la misma
  muestra que la ruptura, y levanta `tr_go` hasta el final de la M1; la
  dirección es `m1_addr`, que no cambia hasta la M1 siguiente.
  `trace_wr` (a `system_clk`) sincroniza `tr_go` y, en su flanco, escribe
  el byte bajo y el alto en dos ciclos seguidos por el puerto A de la
  sombra, y avanza el puntero. La CPU no escribe en una M1; el blit del
  doble búfer espera (`sh_busy`). Acaba unos 230 ns después de T2.
- **El MCU** lee solo lo que enseña: el puntero, las últimas n entradas y,
  de la memoria, los 4 bytes de cada instrucción para desensamblarla (los
  de ahora: con código que se reescribe pueden no ser los que se
  ejecutaron).
  - `th [n]` enseña las últimas n (14; como mucho 60 de una vez). Si lo
    último fue una traza lenta (`t`) y el programa no ha seguido desde
    entonces, enseña esa, con los registros.
  - `H` en la pantalla, lo mismo con 12.
- **Snapshots:** al guardar la sombra del bloque 0, `$1000-$17FF` va como
  la ROM: la traza no se guarda como sombra.
- **En el emulador:** con la orden 12, antes de ejecutar cada instrucción
  del programa (fuera del monitor y de lo que sirve la FPGA), escribir su
  PC en la sombra en `$1000 + 2·ptr` (bajo, alto) y `ptr = (ptr + 1) mod
  1024`; los índices 7 y 8 devuelven `ptr`. Sin las M1 del vídeo ni las de
  `HALT`.

### 13.10 La interfaz web (ESP32)

Nada que hacer en el emulador: es el módulo WiFi hablando con el STM32.

- **La página** `/debug` (enlazada desde la del servidor de ficheros) es
  la consola USB en el navegador. Tiene:
  - un terminal con todo lo que escribe el depurador;
  - una línea de órdenes, con historial (flechas);
  - botones para las habituales: pausa, seguir, paso, por encima, salir,
    registros, desensamblado, historial, traza 100, breakpoints, la
    pantalla del ZX81, snapshot, símbolos y ayuda;
  - el estado en vivo: parado o en marcha, traza, snapshot, pantalla,
    historial.
- **El sondeo:** la página pide `/debug/poll?seq=N` cada 400 ms mientras
  está visible; una orden va en el mismo sondeo (`&c=...`).
- **En el STM32:** todo lo que el depurador escribe por la consola
  (`Serial.println`/`printf` en `DEBUGGER.cpp`, que pasan por `dbg_out`)
  también va a un anillo de 4 KB con un contador que solo crece.
- **`CMD_DBG` (0x10)** en `WIFI_PROTOCOL.h` lleva una orden con un número
  (el STM32 no repite el mismo número: los reintentos del protocolo son
  seguros) y trae hasta 240 bytes desde `seq` y el estado (bits: monitor
  cargado, parado, pantalla, historial, traza, snapshot). El ESP32 encadena
  hasta 8 tramas por sondeo.
- **Las órdenes** se ejecutan como las de la consola USB
  (`dbg_console`), con la primera palabra en minúsculas, y quedan en el
  terminal como `> orden`.

**Los paneles.** La página tiene:
- desensamblado (20 líneas, con etiquetas; el PC resaltado; clic en la
  columna izquierda pone o quita un breakpoint, doble clic en una línea
  corre hasta ella; seguir al PC o ir a una dirección o símbolo);
- registros (clic para cambiar uno) y flags;
- breakpoints y punto de vigilancia (añadir, quitar);
- pila;
- volcado de memoria (8 × 16, con los caracteres del ZX81; clic en un byte
  para cambiarlo);
- la consola de antes, abajo.

Las acciones son órdenes de la consola (`b`, `bc`, `g`, `x`, `e`, `w`).

**La vista estructurada.** Los paneles salen de un documento de líneas de
texto que compone el MCU:

| Línea | Qué es |
|---|---|
| `S estado parado motivo\|dónde` | estado (los bits de `CMD_DBG`), si está parado, el motivo y el símbolo más cercano |
| `R PC SP AF BC DE HL IX IY AF' BC' DE' HL' I R IFF página [SLOW]` | los registros (hex) |
| `F seguir dis mem` | si sigue al PC y dónde empiezan el desensamblado y el volcado |
| `B dir...` | los breakpoints |
| `W modo dir` | el punto de vigilancia |
| `D mm dir\|etiqueta\|bytes\|instrucción` | una línea de desensamblado (`mm`: `>` el PC, `*` breakpoint) |
| `K dir valor` | una palabra de la pila |
| `M dir b0 ... b15` | una línea del volcado |

- **Cuándo se compone:** en cada parada, al seguir (solo el estado) y
  tras cada orden. Solo mientras alguien mira (un `CMD_DBG` en los últimos
  3 s) y cuando la web empieza a mirar. Lee de la memoria 96 bytes para el
  desensamblado, 24 de la pila y 128 del volcado, con las direcciones que
  luego usa para componer.
- **Versión:** cada vez que se compone sube una versión, que viaja en la
  respuesta de `CMD_DBG`. La página, al verla cambiar, pide `/debug/view`.
- **`CMD_DBG_VIEW` (0x11):** el ESP32 lo trae en trozos de 240 bytes con la
  versión; si cambia a mitad, empieza otra vez.
- **Órdenes de la vista:** `@d dir` (desensamblar desde ahí, deja de seguir
  al PC), `@d` (seguir al PC) y `@m dir` (el volcado), sin eco en el
  terminal.
