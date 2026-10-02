# Especificación para el emulador: depurador por hardware (fases 1, 2 y 2b, y la carga de snapshots)

> **Ampliada con la fase 2** (1 de octubre de 2026): el monitor de verdad
> (`z80rom/debugmon.asm`) y la parte del MCU (`DEBUGGER.cpp`): los comandos
> `DBG_BREAK` y `DBG_POLL`, la consola y el botón QuickSilva. Es la sección
> 10. Las secciones 1-9 (la FPGA y la carga) no cambian.

> **Ampliada con la fase 2b** (1 de octubre de 2026): snapshots `.Z81` desde
> el depurador. La FPGA (rev 0.07) añade dos lecturas al puerto `$3FEF`
> (índices 2 y 3) y dos registros (5 y 6); el monitor, las peticiones 7 y 8.
> Es la sección 11, más los cambios marcados en las secciones 6 y 10.

> **Ampliada con la carga de snapshots** (2 de octubre de 2026): `LOAD *Z81`
> carga por el monitor. FPGA rev 0.08 (registro 7 del puerto `$3FEF`, orden
> 10 del canal de configuración), monitor versión 3 (peticiones 9 y 10),
> comando 75 del MCU y la ROM nueva. Es la sección 12. **Importante:** la
> ROM nueva manda el comando 75 antes que el 70; el emulador tiene que
> contestarlo (aunque sea con `0xFF`, "sin monitor") o `LOAD *Z81` se queda
> esperando.

El SD81 Booster tiene un depurador por hardware. La FPGA para el programa en
un límite de instrucción y entra en un **monitor** que vive en la **página
63** de la SRAM. Lo hace con el mismo mecanismo que las interrupciones
simuladas (`sim_int_emulator.md`): sirve `FF` (RST 38h), un `CALL` en `$0038`
y un epílogo en `$003B`. La diferencia es que, mientras corre el monitor, el
bloque 1 enseña la página 63.

La **fase 1** es el motor de la FPGA y la carga del monitor desde el MCU,
probada en el hardware con `EXAMPLES/DBGTEST` (sección 9). La **fase 2**
(sección 10) es el monitor de verdad y la parte del MCU: con ella el
depurador ya se usa desde la consola. También está probada en el hardware.

Referencias:
- `FPGA/SD81V2.1000/sim_int.v` (rev 0.07): el depurador va dentro del módulo
  `sim_int`.
- `FPGA/SD81V2.1000/SD81.v`: la ventana, la carga, la protección y el bloque
  0 (busca `dbg_`, `blk0_ram`, `p63_lock` y `sram_page`).
- `FPGA/SD81V2.1000/tb_dbg.v`: banco de pruebas, ciclo a ciclo, de todos los
  casos.
- `Arduino/SD81BoosterV2_039_STM32/SD_handle.cpp`, `load_debug_monitor()`.
- Fase 2: `z80rom/debugmon.asm` (el monitor, que va en la SD como
  `/SYS/DEBUG.BIN`) y `Arduino/SD81BoosterV2_039_STM32/DEBUGGER.cpp` (la
  parte del MCU).
- Bancos de pruebas en el PC de la fase 2: `claude/dbgharness`. `cosim.cpp`
  corre el monitor de verdad en el mismo núcleo Z80 que usa EightyOne, contra
  el `DEBUGGER.cpp` de verdad; sirve de modelo.
- El plan y el porqué de cada decisión: `claude/planning/hw_debugger_plan.md`.

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
puede escribir. Normalmente está protegido: es la ROM. Así el monitor puede
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

## 3. Cuándo se rompe

Solo con **`armado`** (= `dbg_loaded`) y con el **generador de NMI apagado**
(FAST o Superfast). El emulador ya sabe si la NMI está encendida. En el
hardware se sigue con los `OUT`, como la ULA:
- `A1 = 0` (`OUT ($FD)`) la apaga;
- si no, `A0 = 0` (`OUT ($FE)`) la enciende;
- al reset, apagada.

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

rompe = armado && !nmi && prog_m1 &&
        ( swbp || step_brk || (!skip && (pausa || vigilancia || cmp_exec)) )
```

**Si rompe:**
- se sirve `FF` (aunque ya fuera `FF` en memoria) y `sim_int` no inyecta en
  esta M1;
- `motivo` = el primero que se cumpla de: 7 (`swbp`), 5 (`cmp_exec` sin
  `skip`), 6 (vigilancia sin `skip`), 4 (`step_brk`), o el de la pausa (1, 2
  o 3);
- `lvl` = 1 si la fase de `sim_int` no es IDLE (se ha roto dentro de una
  interrupción simulada);
- se borran la pausa, la vigilancia, `step_on` y `skip`.

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
| 5 / 6 | (2b) puntero de la BRAM de sombra, bajo / alto. Se carga al escribir el alto |
| 7 | (rev 0.08) escribe el dato en la BRAM de sombra, en el puntero, y el puntero avanza |

| IN (índice) | Qué devuelve |
|---|---|
| 0 | estado: bit 7 armado, bit 6 NMI encendida, bit 5 `lvl`, bits 2-0 motivo de la última ruptura |
| 1 | interrupciones simuladas: `enabled, pending, arm, superfast, halted, call_now, fase(2 bits)` |
| 2 | (2b) el byte de la BRAM de sombra en el puntero; el puntero avanza 1 al acabar cada `IN` |
| 3 | (2b) el registro de Chroma81 (lo último escrito con `OUT $7FEF`) |
| 4 / 5 | (rev 0.08) el registro elegido del AY A (A3=1, ZonX) / del AY B: el latch de dirección, que sus puertos no dejan leer |
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
NMI. `dbg_loaded` no se toca.

## 7. Detalles que cuentan

- Los `OUT` al puerto (y los de la NMI) se procesan **una vez por
  instrucción**: el registro r espera exactamente el `OUT` siguiente.
- La decisión de romper se toma en cada M1 con el estado de antes de esa
  M1, igual que la de `sim_int`.
- Si en la misma M1 querrían inyectar el depurador y `sim_int`, gana el
  depurador. Al volver, `sim_int` inyecta en X si sigue teniendo una
  interrupción pendiente.
- Mientras el monitor está activo, los VSYNC siguen poniendo `pending` en
  `sim_int` (se acumulan en uno solo) y FRAMES sigue corriendo.
- Un `RST 38h` de verdad del programa (un `FF` en un límite de instrucción),
  con el depurador armado y en FAST o Superfast, entra en el monitor como
  breakpoint por software (motivo 7). En SLOW es un RST normal.

## 8. Lo que queda para después

- Snapshots (botón mantenido 3 s, `snap` en la consola): fase 2b.
- Paso por encima, ejecutar hasta un punto y puntos de vigilancia desde la
  consola: fase 3. La FPGA ya los tiene (sección 6).
- El monitor en SLOW (con la NMI encendida): fase 4.

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
- **R:** el monitor descuenta 9 M1 a la entrada y 22 a la salida, contando
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
| 27 | estado del depurador (`$3FEF`, índice 0: armado, NMI, nivel, motivo) |
| 28 | estado de las interrupciones simuladas (`$3FEF`, índice 1) |
| 29 | página del programa en el bloque 1 |

Todo en little endian. 27-29 son solo información: con `SETREGS` el monitor
las ignora.

### 10.3 Los dos comandos

El handshake es el de siempre (puertos `$A7` y `$AF`, como `fread`/`fwrite`,
comandos 55/56):
- cada byte que manda el Z80 lo confirma el MCU cambiando el reloj;
- cada byte que manda el MCU va en el latch antes del cambio de reloj que
  confirma lo anterior;
- al final hay un cambio de reloj más.

`LAST_COMMAND` pasa a 74.

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
| 3 | SETREGS | 0 | 30 | el bloque de registros | — |
| 4 | OUT | puerto (16 bits) | valor en el byte bajo | — | — |
| 5 | IN | puerto (16 bits) | 0 | — | 1 byte |
| 6 | CONT | 0 | 0 | — | (el monitor sale) |
| 7 | INSEQ (2b) | puerto (16 bits) | 1-256 | — | m `IN` seguidos del mismo puerto |
| 8 | READP (2b) | desplazamiento en la página | 1-256 bytes | — | m bytes de la página p |

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
interrupt)". El `*` delante de una dirección marca un breakpoint.

### 10.5 La consola

Son las órdenes en minúsculas de la consola USB del STM32. Las de
mayúsculas (`DBG_PAUSE`, `DBG_RELOAD` y las que ya había) siguen igual. Los
números van en hexadecimal (con `$` o sin él). Con el programa en marcha
solo valen `p` y `h`.

| Orden | Qué hace |
|---|---|
| `p` | pausa: cambia el bit de la orden 8 (fuente "MCU") |
| `r` | registros e instrucción en el PC |
| `s [n]` | n pasos (1) |
| `c` | continuar |
| `b dir` / `bc [dir]` / `bl` | poner, quitar (uno o todos) y listar breakpoints |
| `d [dir] [n]` | desensamblar n instrucciones (10, como mucho 60) desde dir (PC) |
| `m dir [n]` | volcado de n bytes (64, como mucho 256) |
| `e dir b1 b2 …` | escribir bytes (hasta 32) |
| `x reg=val` | cambiar un registro: `af bc de hl ix iy af' bc' de' hl' sp pc i r` (`SETREGS`) |
| `io puerto [val]` | leer o escribir un puerto |
| `h`, `?` | ayuda |

En el emulador lo natural es una ventana de consola, o reutilizar el
depurador que ya tenga, enviando las mismas órdenes.

### 10.6 Botón QuickSilva

Ya no bloquea el bucle del MCU mientras se mantiene. **Todo pasa al
soltarlo** (cambiado en la fase 2b); el LED avisa de lo que hará:
- **menos de 1 s:** cambia QuickSilva;
- **de 1 a 3 s** (LED magenta al llegar a 1 s): si el programa corre,
  pausa; si está parado, continúa;
- **3 s o más** (LED amarillo al llegar a 3 s): snapshot con el nombre
  automático (sección 11). Si el programa corría, sigue al acabar.

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
  6. al final `bc` y `c`: el programa sigue sin rastro de los `FF`.

## 11. Fase 2b: snapshots

### 11.1 Qué lo lanza

- La consola: `snap [-a] [nombre]`. Sin `-a`, solo las páginas mapeadas en
  los 8 bloques; con `-a`, todas las páginas 0-62 (sin FULL_PAGING, solo
  hasta la 31: el mapper no llega más allá).
- El botón QuickSilva mantenido 3 s (sección 10.6).

Si el programa corre, el MCU lo pausa, graba y lo deja seguir. Si ya estaba
parado, se queda parado. Mientras graba, el LED parpadea en amarillo y la
consola contesta `Snapshot in progress`.

**Nombre:** el que se dé (con `.Z81` si no lleva extensión), en el
directorio actual. Si no, el nombre (sin extensión) del último fichero
cargado con `LOAD` más `001.Z81`, `002.Z81`… (sin separador: el ZX81
no tiene `_`; `EXPLORER.P` → `EXPLORER001.Z81`); sin nada cargado, `NONAME001.Z81`,
`NONAME002.Z81`… Se usa el primer número libre.

### 11.2 Cómo lee el MCU el estado

Todo con peticiones al monitor, con el programa parado:
1. el mapper: 8 `IN` del puerto `(b << 8) | $E7`, b = 0-7 (6 bits);
2. Chroma81: `OUT $03` y `IN` del puerto `$3FEF` (índice 3);
3. los POKEs de control: puntero de la BRAM a 2038 (`$85`, bajo, `$86`,
   alto), `OUT $02` e `INSEQ` de 61 bytes (2038-2098). La BRAM de sombra
   guarda lo último que escribió la CPU en esas direcciones. Si el valor es
   el byte de la ROM en esa dirección, se toma como **nunca escrito**: la
   BRAM se carga con la ROM al arrancar;
4. `[MEMORY]` y `[COLOUR]`: `READ` de 256 bytes, de `$2000` a `$FFFF` y de
   `$C000` a `$FFFF` (en `$2000`-`$3FFF`, la página del programa, no la del
   monitor);
5. las páginas: `READP`. El monitor pone la página en el bloque 7 un
   momento (`OUT (C),A` con B = página y A = `(página & 31) << 3 | 7`, que
   vale con y sin FULL_PAGING) y lo devuelve como estaba.

El MCU no puede leer los bits de configuración en la FPGA: guarda el último
valor que mandó con cada orden de configuración (`cfg_value`).

### 11.3 El fichero

El formato de EightyOne (`.Z81`), con las secciones `[MACHINE]`, `[CPU]`,
`[ZX81]`, `[MEMORY]`, `[COLOUR]`, `[SD81BOOSTER]` y `[EOF]`. El RLE es el
suyo: `VV ` o `*NNNN VV `, 16 tokens por línea.

- `[CPU]`: los registros del bloque de la parada. `IM 01` siempre (no se
  puede leer; la ROM pone IM 1). `IF1` = `IF2` = IFF2.
- `[ZX81]`: `NMI 00` (parado en FAST).
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
    sombra (sección 11.5). Un sprite cuyos 28 bytes siguen siendo los de la
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

### 11.5 Los sprites en la sombra (FPGA rev 0.08)

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

### 11.4 En el emulador

Lo más sencillo es generar el `.Z81` directamente con su propio guardado,
añadiendo las claves `HW_POKES`/`HW_CFG` si quiere ser fiel. Si se emula el
monitor y el protocolo, hacen falta los índices 2 y 3 del puerto `$3FEF`,
los registros 5 y 6 y las peticiones 7 y 8. `cosim.cpp` lo modela y
compara cada `.Z81` con la memoria emulada.

La carga está en la sección 12.

## 12. Carga de snapshots (`LOAD *Z81` con el monitor)

`LOAD *Z81 "f"` carga el snapshot por el monitor del depurador. El monitor
corre en la página 63, así que se puede escribir cualquier página, también
la 1, que es donde se ejecuta la ROM. La salida del monitor (`SETREGS` +
`CONT`) deja todos los registros, R incluido, como estaban. Sin monitor se
usa el cargador de siempre (comando 70, `z81_snapshot_loading.md`).

### 12.1 La FPGA (rev 0.08)

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

### 12.2 El monitor (versión 3: `"SD81DBG",3` en `$2003`; 1222 bytes)

| op | Nombre | Dirección | n | Después | Qué hace |
|---|---|---|---|---|---|
| 9 | WRITEP | desplazamiento en la página | 1-256 | la página (1 byte) y n datos | escribe n bytes en la página, mapeándola un momento en el bloque 7 (la inversa de READP) |
| 10 | BRAMW | 0 | 1-256 | n datos | por cada byte, `OUT $87` y el dato al puerto `$3FEF`: la BRAM de sombra |
| 11 | AYREAD | puerto de elección del AY (`$00CF` el A, `$00C7` el B) | 16 | — | para i = 0..n-1: `OUT (puerto),i` e `IN (puerto)` → n bytes |
| 12 | AYWRITE | puerto de elección | 16 | n datos | para i = 0..n-1: `OUT (puerto),i` y el dato al puerto de datos (el mismo con A7 = 0) |

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
| 0 | `DI`, `OUT ($FD),A` (NMI apagada: con ella el depurador no para), `IM` del snapshot, trampa (`OUT $10` a `$3FEF`) y `JR $`. No vuelve: el MCU carga todo al parar y salta al snapshot. |
| `0xFF` | Sin monitor: vuelve a mandar el nombre con el comando 70 (el cargador de siempre). |
| 1-3 | Error, como el 70: 1 sin fichero, 2 sin `[MEMORY]`, 3 fichero mal hecho (`REPORT-G`, `-H`, `-I`). |

### 12.4 Lo que hace el MCU al parar

En este orden:

1. **Estado del MCU.**
   - `CUR_DIR` pasa a ser el directorio actual.
   - El AY del MCU, el VGM, el PEG y los ficheros abiertos
     (`mcustate_apply`). Sin claves de VGM o PEG se paran los que hubiera;
     los ficheros abiertos de antes se cierran.
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
  - los índices 4 y 5 y la copia de los sprites (11.5);
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
