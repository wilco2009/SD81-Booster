# Interrupciones simuladas

Interrupciones a 50 Hz para programas en código máquina que trabajan en los
modos **Superfast** con **DI**. La FPGA no tiene acceso a /INT (en el ZX81
está cableada a A6) ni a /NMI, así que la interrupción se consigue
inyectando un `RST 38h` en la M1 que empieza una instrucción. En el vídeo
nativo no se puede usar: allí la INT y los HALT forman parte de la
generación de la imagen.

Plan:

1. **Detector de límites de instrucción** (hecho, probado en hardware): la
   FPGA sigue las M1 y sabe cuándo la siguiente empieza una instrucción
   (prefijos CB, ED, DD, FD, `DD CB d op`, prefijos repetidos, `DD ED`).
2. **Inyección** (hecho, probado en hardware): en la primera M1 que
   sea límite tras el VSYNC, `RST 38h`; en `$0038`, `CALL rutina`; y en
   `$003B` el epílogo `EX (SP),HL / DEC HL / EX (SP),HL / RET`, que
   corrige la dirección de retorno (la CPU guarda X+1) y marca el final de
   la rutina: hasta entonces no se inyecta otra.
3. HALT: un 76 en un límite de instrucción (o detrás de DD/FD) se sirve
   como FF; en `$0038` la FPGA sirve `JR $` hasta el VSYNC y luego el
   `CALL`, y el epílogo vuelve sin `DEC HL` (X+1 ya es detrás del HALT).

## Cómo se usa (paso 2)

```
        di                      ; imprescindible: con EI, las INT reales
                                ; de A6 inundarian $0038
        ld   a,170
        ld   (2045),a           ; un modo Superfast (170-174)
        ld   hl,rutina
        ld   (2038),hl          ; POKE 2038/2039: la rutina
        ld   a,1
        ld   (2040),a           ; POKE 2040,1: activas (0: desactivas)
        ...
rutina: push af                 ; guardar lo que se use
        ...
        pop  af
        ret                     ; RET normal: el epilogo de la FPGA
                                ; corrige la direccion de vuelta
```

La rutina se llama una vez por trama (50 Hz) en el primer límite de
instrucción tras el VSYNC. Usa 4 bytes de la pila del programa (el `RST` y
el `CALL`) además de lo que guarde ella.

## Pruebas

### Paso 2: `inttest`

En Superfast y con DI ejecuta una carga de trabajo determinista con todas
las familias de prefijos, primero sin interrupciones (checksum de
referencia) y después con ellas (la rutina solo cuenta). Tiene que salir el
mismo checksum, y las interrupciones tienen que coincidir con las que
cuenta la FPGA y con las tramas que han pasado (FRAMES, ±1). Unos 3
segundos por pasada. Stub `INTTEST.B81`.

Mientras corre deja señales en la primera fila (en Superfast se ve el DFILE
en directo), para saber dónde se para si se cuelga: columna 0, la pasada
(`1` sin interrupciones, `2` con ellas); columna 2, un carácter que
incrementa la rutina de interrupción (si cambia, entran); columna 4, el
progreso de la carga.

### Paso 1: `m1test`

El detector se lee por el puerto `$3FEF` (dirección completa de 16 bits):

| Escritura | Qué hace |
|---|---|
| `80h` | borra los contadores |
| `40h` | los congela en una copia, que es lo que se lee |
| `0`-`15` | elige qué devuelve la lectura |

| Índice | Lectura |
|---|---|
| 0 / 1 | M1 (bajo / alto) |
| 2 / 3 | instrucciones: M1 que empiezan instrucción |
| 4 / 5 | instrucciones `DD CB` / `FD CB` |
| 6 | último opcode leído en una M1 |
| 7 | estado (0 normal, 1 segundo byte de CB/ED, 2 tras DD/FD) |
| 8 | interrupciones simuladas: activas, pendiente, arm, Superfast, -, -, fase (en vivo) |
| 9 / 10 | interrupciones inyectadas, bajo / alto (en vivo) |
| 15 | firma `51h`: el detector está presente |

`m1test.asm` mide dos veces con el mismo camino de código, con un cuerpo
vacío y con uno que tiene todas las familias de instrucciones, y compara la
diferencia con lo esperado: **35 M1, 18 instrucciones y 2 DD/FD CB**. Se
carga con el stub `M1TEST.B81` y dice OK o WRONG; si la FPGA no tiene el
detector, lo dice también.

```
pasmo m1test.asm m1test.bin
```

Los módulos están en `FPGA/SD81V2.1000/sim_int.v` (`sim_int` y
`m1_tracker`), y sus bancos de pruebas en `tb_sim_int.v` y
`tb_m1_tracker.v` (se simulan aparte, con ISim o iverilog; no forman parte
del proyecto de ISE).
