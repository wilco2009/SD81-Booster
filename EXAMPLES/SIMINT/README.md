# Interrupciones simuladas

Interrupciones a 50 Hz para programas en código máquina que trabajan en los
modos **Superfast** con **DI**. La FPGA no tiene acceso a /INT (en el ZX81
está cableada a A6) ni a /NMI, así que la interrupción se consigue
inyectando un `RST 38h` en la M1 que empieza una instrucción. En el vídeo
nativo no se puede usar: allí la INT y los HALT forman parte de la
generación de la imagen.

Plan:

1. **Detector de límites de instrucción** (hecho): la FPGA sigue las M1 y
   sabe cuándo la siguiente empieza una instrucción (prefijos CB, ED, DD,
   FD, `DD CB d op`, prefijos repetidos, `DD ED`).
2. Inyección: en la primera M1 que sea límite tras el VSYNC, `RST 38h`; en
   `$0038`, `CALL rutina`; y en `$003B` el epílogo
   `EX (SP),HL / DEC HL / EX (SP),HL / RET`, que corrige la dirección de
   retorno (la CPU guarda X+1) y marca el final de la rutina.
3. HALT: con las interrupciones simuladas activas, un HALT se sirve como
   `JR $` hasta que llega el VSYNC, y entonces se inyecta el `RST` (ahí la
   dirección de retorno ya es la buena).

## Paso 1: `m1test`

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
| 15 | firma `51h`: el detector está presente |

`m1test.asm` mide dos veces con el mismo camino de código, con un cuerpo
vacío y con uno que tiene todas las familias de instrucciones, y compara la
diferencia con lo esperado: **35 M1, 18 instrucciones y 2 DD/FD CB**. Se
carga con el stub `M1TEST.B81` y dice OK o WRONG; si la FPGA no tiene el
detector, lo dice también.

```
pasmo m1test.asm m1test.bin
```

El módulo está en `FPGA/SD81V2.1000/sim_int.v` (`m1_tracker`), y su banco
de pruebas en `tb_m1_tracker.v` (se simula aparte, no forma parte del
proyecto de ISE).
