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
3. **HALT** (hecho, probado en hardware): un 76 en un límite de
   instrucción (o detrás de DD/FD: `DD 76` también es HALT) se sirve como
   FF; en `$0038` la FPGA sirve `JR $` hasta el VSYNC y luego el `CALL`, y
   en `$003B` directamente `RET` (H+1 ya es detrás del HALT). Si la
   interrupción ya estaba pendiente, el `CALL` va sin esperar. Para ver el
   opcode real, el FF se decide ahora en la subida de T2, con el dato de la
   SRAM ya en el bus, y la FPGA toma el bus desde ahí.

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
instrucción tras el VSYNC. Con las interrupciones activas, `HALT` espera a
la siguiente (como en un Z80 con EI), así que el bucle típico de un juego
funciona tal cual:

```
bucle:  halt                    ; espera a la trama (la rutina ya ha corrido)
        call mover
        call pintar
        jr   bucle
```

La rutina no puede tener `HALT` (con DI, en un Z80 tampoco acabaría). Usa 4 bytes de la pila del programa (el `RST` y
el `CALL`) además de lo que guarde ella.

## Pruebas

### Paso 3: `halttest`

Con las interrupciones activas (la rutina solo cuenta):

1. 100 HALT (50 `HALT` y 50 `DD HALT`): cada uno tiene que esperar una trama
   y volver una sola vez detrás. Tienen que salir 100 vueltas, 100
   interrupciones y 100 tramas (±1). Si volviera al propio
   HALT saldrían 200 tramas; si no esperara, casi ninguna.
2. `CB 76`, `ED 76` y `DD CB d 76` no son HALT: 20000 vueltas tienen que
   tardar pocas tramas, con tantas interrupciones como tramas (±1).

Stub `HALTTEST.B81`. Deja en la primera fila la prueba en curso (columna 0)
y el carácter que incrementa la rutina (columna 2).

### Paso 2: `inttest`

En Superfast y con DI ejecuta una carga de trabajo determinista con todas
las familias de prefijos, primero sin interrupciones (checksum de
referencia) y después con ellas (la rutina solo cuenta). Tiene que salir el
mismo checksum, y las interrupciones tienen que coincidir con las tramas
que han pasado (FRAMES, ±1). Unos 3 segundos por pasada. Stub
`INTTEST.B81`.

Mientras corre deja señales en la primera fila (en Superfast se ve el DFILE
en directo), para saber dónde se para si se cuelga: columna 0, la pasada
(`1` sin interrupciones, `2` con ellas); columna 2, un carácter que
incrementa la rutina de interrupción (si cambia, entran); columna 4, el
progreso de la carga.

### Paso 1: el detector

La prueba `m1test` leía unos contadores del detector por el puerto `$3FEF`.
Se quitaron al hacer el depurador por hardware: la FPGA no lleva lógica
solo para probar. El detector queda cubierto por `inttest` y `halttest`
(si se equivocara con un límite, la inyección partiría una instrucción y el
checksum o las vueltas saldrían mal) y por el banco de pruebas
`tb_m1_tracker.v`, que cuenta en el propio banco las instrucciones del
mismo cuerpo de prueba (35 M1, 18 instrucciones).

Las pruebas comprueban la firma en el puerto `$3FEF` (índice 15): `51h`
con las interrupciones simuladas, `52h` si además está el depurador (ver
`EXAMPLES/DBGTEST`).

Los módulos están en `FPGA/SD81V2.1000/sim_int.v` (`sim_int`, que también
lleva el depurador, y `m1_tracker`), y sus bancos de pruebas en
`tb_sim_int.v`, `tb_m1_tracker.v` y `tb_dbg.v` (se simulan aparte, con
ModelSim, ISim o iverilog; no forman parte del proyecto de ISE).
