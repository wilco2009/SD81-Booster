# Especificación para el emulador: depurador por hardware (fase 1)

El SD81 Booster tiene un depurador por hardware. La FPGA para el programa en
un límite de instrucción y entra en un **monitor** que vive en la **página
63** de la SRAM. Lo hace con el mismo mecanismo que las interrupciones
simuladas (`sim_int_emulator.md`): sirve `FF` (RST 38h), un `CALL` en `$0038`
y un epílogo en `$003B`. La diferencia es que, mientras corre el monitor, el
bloque 1 enseña la página 63.

Esta es la **fase 1**: el motor de la FPGA y la carga del monitor desde el
MCU. Todo está probado en el hardware con `EXAMPLES/DBGTEST` (sección 9). El
protocolo entre el monitor y el MCU (`DBG_BREAK`/`DBG_POLL`) y la consola
llegarán en la fase 2, con otra especificación.

Referencias:
- `FPGA/SD81V2.1000/sim_int.v` (rev 0.06): el depurador va dentro del módulo
  `sim_int`.
- `FPGA/SD81V2.1000/SD81.v`: la ventana, la carga, la protección y el bloque
  0 (busca `dbg_`, `blk0_ram`, `p63_lock` y `sram_page`).
- `FPGA/SD81V2.1000/tb_dbg.v`: banco de pruebas, ciclo a ciclo, de todos los
  casos.
- `Arduino/SD81BoosterV2_039_STM32/SD_handle.cpp`, `load_debug_monitor()`.
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

| IN (índice) | Qué devuelve |
|---|---|
| 0 | estado: bit 7 armado, bit 6 NMI encendida, bit 5 `lvl`, bits 2-0 motivo de la última ruptura |
| 1 | interrupciones simuladas: `enabled, pending, arm, superfast, halted, call_now, fase(2 bits)` |
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

## 8. Qué no está en la fase 1

- El protocolo monitor ↔ MCU (`DBG_BREAK`, `DBG_POLL`), la consola de
  verdad, los breakpoints por software gestionados por el MCU, el botón
  QuickSilva (mantener 1 s: pausa; 3 s: snapshot) y el LED magenta: fase 2.
- El monitor en SLOW (con la NMI encendida): fase 4.

## 9. Prueba (`EXAMPLES/DBGTEST`)

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
