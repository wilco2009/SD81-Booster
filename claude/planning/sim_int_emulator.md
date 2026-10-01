# Especificación para el emulador: interrupciones simuladas y puerto `$3FEF`

La FPGA del SD81 Booster da interrupciones a 50 Hz a los programas en código
máquina que trabajan en los modos **Superfast** (`POKE 2045,170..174`) con
**DI**. No puede usar /INT (en el ZX81 está cableada a A6) ni /NMI, así que
las hace **sirviendo bytes en el bus**: cambia el opcode de una M1 por `FF`
(RST 38h) y después sirve un `CALL` en `$0038` y un epílogo en `$003B`. También
cambia los `HALT` por una espera al siguiente VSYNC.

Todo está probado en el hardware, con las tres pruebas de `EXAMPLES/SIMINT`
(sección 7). El emulador tiene que dar los mismos resultados.

Referencia: `FPGA/SD81V2.1000/sim_int.v` (módulos `sim_int` rev 0.05 y
`m1_tracker`), su instancia en `SD81.v` y el banco de pruebas
`tb_sim_int.v`, que recorre todos los casos ciclo a ciclo.

**Recomendación:** emularlo **a nivel de byte**, en la función que sirve las
lecturas de memoria de la CPU (distinguiendo M1 de lectura normal), igual que
se hace con los NOP del ULA. No lo emules "por semántica" (meter PC en la
pila y saltar): la pila tiene que quedar exactamente como en el hardware.
Con bytes reales, los tiempos (11 T del RST, 17 T del CALL, etc.) salen
solos.

## 1. Registros que escribe el Z80

Se capturan de las **escrituras de memoria** a estas direcciones, que están
en la zona de la ROM. La FPGA las saca del bus, igual que el resto de POKEs
de control de 2041-2047.

| Dirección | Qué hace |
|---|---|
| 2038 | `int_addr` bajo: la rutina de interrupción |
| 2039 | `int_addr` alto |
| 2040 | bit 0 = 1: **activa** (`enabled = 1`); bit 0 = 0: desactiva |

- `int_addr` vale 16514 al encender y **no** se borra con el reset.
- `enabled` se pone a 0 con el reset.

El modo Superfast (`superfast` en lo que sigue) es el `sfast_mode_en` que
el emulador ya lleva: `POKE 2045` con 170, 171, 172, 173 o 174 lo enciende,
y con 85 lo apaga.

## 2. El detector de límites de instrucción (`m1_tracker`)

Sigue **todas las M1** con el byte que recibe la CPU. Ese byte es el de la
memoria, o el que sirva la FPGA si lo cambia. Con él sabe si la M1
siguiente empieza una instrucción.

```
estado NORMAL  (0): la proxima M1 empieza instruccion  -> boundary
estado SECOND  (1): la proxima M1 es el 2o byte de CB xx / ED xx
estado INDEX   (2): la proxima M1 va detras de DD/FD    -> index_prefix

al acabar cada M1, con su opcode op:
  NORMAL: instrucciones++
          CB, ED -> SECOND
          DD, FD -> INDEX
  SECOND: -> NORMAL
  INDEX:  CB     -> NORMAL, ddcb++    (DD CB d op: d y op NO son M1)
          ED     -> SECOND            (DD ED xx)
          DD, FD -> INDEX             (prefijos repetidos)
          otro   -> NORMAL
  siempre: m1++, ultimo_op = op
```

Ojo con `DD CB d op` y `FD CB d op`: la d y el op son **lecturas normales,
no M1**. Si el núcleo Z80 los lee con la función de fetch de opcodes, hay
que distinguirlos. Una forma de comprobarlo: el número de M1 es lo que
incrementa R (`DD CB d op` suma 2).

## 3. Interrupciones: estado y bytes servidos

Estado de `sim_int`:

```
fase:      IDLE, ENTRY, ISR, EPI
pending:   VSYNC sin atender
halted:    la interrupcion en curso viene de un HALT
call_now:  = pending | !halted   (se evalua ANTES de cada M1)
took_call: lo que se sirvio en la ultima M1 en $0038 (CALL o JR)
count:     interrupciones entregadas (16 bits)
```

**VSYNC.** Es el VSYNC del modo Superfast, el mismo evento que decrementa
FRAMES:
- con `enabled && superfast`, cada VSYNC pone `pending = 1`;
- si `!enabled` o `!superfast`, `pending = 0`.

**En cada M1**, antes de servir el byte, con `op` el byte de la memoria:

```
arm     = enabled && superfast && pending && fase==IDLE && boundary
halt_ok = enabled && superfast && fase==IDLE && (boundary || index_prefix)
          && op == 0x76
inj     = arm || halt_ok
```

**Bytes que se sirven.** Solo en estas lecturas; todas las demás van a la
memoria:

| Fase | Ciclo | Dirección | Byte |
|---|---|---|---|
| cualquiera | M1 con `inj` | cualquiera | `FF` (RST 38h) |
| ENTRY | M1 | `$0038` | `call_now ? CD : 18` (CALL nn / JR) |
| ENTRY | lectura | `$0039` | `took_call ? int_addr bajo : FE` |
| ENTRY | lectura | `$003A` | `int_addr alto`, solo si `took_call` |
| ISR | M1 | `$003B` | `halted ? C9 : E3` (RET / EX (SP),HL) |
| EPI | M1 | `$003C` | `2B` (DEC HL) |
| EPI | M1 | `$003D` | `E3` (EX (SP),HL) |
| EPI | M1 | `$003E` | `C9` (RET) |

"Lectura" es una lectura de memoria que **no** es M1: los operandos del
CALL o del JR. Fuera de su fase, esas direcciones se leen de la ROM como
siempre.

**Transiciones al acabar cada M1**, según su dirección:

```
IDLE:  si inj -> ENTRY; halted = halt_ok
ENTRY: M1 en $0038:  took_call = call_now
                     si call_now: count++, pending = 0   (interrupcion entregada)
       M1 en otra:   -> ISR                  (primera M1 de la rutina)
ISR:   M1 en $003B:  -> halted ? IDLE : EPI
EPI:   M1 en $003E:  -> IDLE
```

Un VSYNC que llegue en el mismo instante en que se entrega una interrupción
deja `pending` a 1: gana la puesta a 1.

### Lo que pasa en la CPU

**Interrupción normal**, en la M1 de la instrucción en X:

1. `FF`: la CPU mete **X+1** en la pila y va a `$0038`.
2. `CD lo hi`: mete **`$003B`** y va a la rutina.
3. La rutina acaba con un `RET` normal y vuelve a `$003B`.
4. En `$003B`-`$003E` se sirve `EX (SP),HL / DEC HL / EX (SP),HL / RET`,
   que convierte X+1 en X y vuelve a la instrucción interrumpida, que se
   ejecuta entera.

**HALT** en H, o `DD 76` con el 76 en H:

1. `FF`: la CPU mete **H+1**, que ya es la instrucción de detrás, y va a
   `$0038`.
2. En `$0038` se sirve `JR $` (18 FE), una vuelta tras otra, hasta que
   haya VSYNC pendiente.
3. Entonces se sirve el CALL, igual que en una interrupción normal.
4. Al volver, en `$003B` se sirve directamente `RET`, a H+1.

Si la interrupción ya estaba pendiente al llegar al HALT, el CALL va sin
ninguna vuelta de JR.

Por eso, con las interrupciones activas, un HALT **nunca** llega a
ejecutarse en la CPU. Con las interrupciones desactivadas o fuera del modo
Superfast, `76` es un HALT normal (por ejemplo el de `SET_FAST` de la
ROM). `CB 76`, `ED 76` y `DD CB d 76` no son HALT y no se tocan.

**Dentro de la rutina**, la pila tiene `[SP] = $003B` y
`[SP+2] = X+1` (o H+1). No se anidan: mientras la fase no es IDLE, no se
inyecta ninguna otra. Si llega un VSYNC durante la rutina, queda pendiente
y se sirve en cuanto acaba. La rutina no puede tener HALT.

## 4. Puerto de depuración `$3FEF`

Decodifica los **16 bits** de la dirección, así que se usa con `OUT (C)` e
`IN (C)` y BC = `$3FEF`.

| OUT | Qué hace |
|---|---|
| bit 6 (`40h`) | copia m1, instrucciones, ddcb, último op y estado a la copia congelada, que es la que se lee |
| bit 7 (`80h`) | pone a 0 m1, instrucciones y ddcb (los contadores vivos) |
| bits 7 y 6 a 0 (`0`-`15`) | elige el índice de lectura (bits 3-0) |

Con `C0h` hace las dos cosas: primero congela y después borra.

| Índice | IN devuelve |
|---|---|
| 0 / 1 | M1, bajo / alto (copia congelada) |
| 2 / 3 | instrucciones (M1 en estado NORMAL), bajo / alto (congelada) |
| 4 / 5 | `DD CB` / `FD CB`, bajo / alto (congelada) |
| 6 | último opcode de M1 (congelado) |
| 7 | estado del detector: 0, 1 o 2 (congelado) |
| 8 | **en vivo**: `enabled, pending, arm, superfast, halted, call_now, fase(2 bits)`, del bit 7 al 0 |
| 9 / 10 | **en vivo**: `count` bajo / alto |
| 15 | `51h`: firma de que el detector existe |
| otros | 0 |

Con el reset se ponen a 0 los contadores, el índice, `count` y todo el
estado de `sim_int`.

## 5. Detalles que cuentan

- El detector ve el byte **servido**, no el de la memoria: tras el `FF`
  sigue en NORMAL, y el `CD` de `$0038` también cuenta como instrucción.
  Así lo cuenta el hardware en los índices 0-3.
- La decisión de inyectar se toma en cada M1 con el estado de antes de esa
  M1. Un VSYNC que llegue a mitad de una instrucción se atiende en la M1
  siguiente que sea límite. En el hardware hay 2 o 3 ciclos de reloj de
  sincronización, y a nivel de instrucción da igual.
- `ED 76` es el IM 1 no documentado: la CPU lo ejecuta como tal.

## 6. Coste en T-states

Sale solo si se emula a nivel de byte, pero sirve para comprobarlo:

| Caso | Coste añadido |
|---|---|
| Interrupción normal, entrada | RST 11 T + CALL 17 T |
| Interrupción normal, epílogo | EX (SP),HL 19 T + DEC HL 6 T + EX (SP),HL 19 T + RET 10 T = 54 T |
| HALT | RST 11 T + 12 T por cada vuelta de JR hasta el VSYNC + CALL 17 T + RET 10 T |

A todo eso se suma lo que tarde la propia rutina.

## 7. Pruebas (`EXAMPLES/SIMINT`)

Hay que ensamblarlas con `pasmo x.asm x.bin` y cargar el stub `.B81`, que
lee `X.BIN` en 24576.

| Prueba | Qué comprueba | Resultado en el hardware |
|---|---|---|
| `M1TEST.B81` / `m1test.asm` | Detector: la diferencia entre dos cuerpos | **35 M1, 18 instrucciones, 2 DD/FD CB** → OK |
| `INTTEST.B81` / `inttest.asm` | Carga de trabajo con todas las familias de prefijos, sin y con interrupciones | checksum **igual** en las dos pasadas (56048); interrupciones de la rutina = de la FPGA = tramas (155) → OK |
| `HALTTEST.B81` / `halttest.asm` | 50 `HALT` + 50 `DD HALT`, y 20000 vueltas de `CB 76` / `ED 76` / `DD CB d 76` | **100 / 100 / 100 / 100**, y **20 / 20 / 20** → OK |

En el emulador los números de `INTTEST` y la segunda parte de `HALTTEST`
dependen de la velocidad efectiva. Lo que tiene que cuadrar:
- los dos checksums iguales;
- las interrupciones de la rutina iguales a las de la FPGA;
- las tramas iguales a las interrupciones ±1.

Lo de `M1TEST` y la primera parte de `HALTTEST` tiene que salir **exacto**.

Mientras corren, `INTTEST` y `HALTTEST` dejan señales en la primera fila de
la pantalla, que en Superfast se ve en directo:
- columna 0: la prueba o pasada en curso;
- columna 2: un carácter que incrementa la rutina de interrupción;
- columna 4 (solo `INTTEST`): el progreso de la carga.

## 8. Uso desde un programa (para la documentación)

```
        di                      ; imprescindible
        ld   a,170
        ld   (2045),a           ; un modo Superfast (170-174)
        ld   hl,rutina
        ld   (2038),hl          ; la rutina
        ld   a,1
        ld   (2040),a           ; activas (0: desactivas)
bucle:  halt                    ; espera a la trama (la rutina ya ha corrido)
        call mover
        call pintar
        jr   bucle

rutina: push af                 ; guardar lo que se use
        ...
        pop  af
        ret                     ; RET normal: la FPGA corrige la vuelta
```
