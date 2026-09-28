# SD81TEST — test de hardware del SD81 Booster

Programa en ensamblador para comprobar de forma sistemática el interface,
sobre todo al probarlo en máquinas nuevas (TS1500, TS1000, clones...). Va
creciendo por fases (ver el plan al final).

## Ensamblar

El programa está partido en un núcleo y tres módulos. Primero hay que
ensamblar el núcleo, que genera `sd81test.sym`, y después los módulos, que
incluyen ese `.sym` para usar las rutinas del núcleo:

```
pasmo sd81test.asm SD81TEST.BIN sd81test.sym
pasmo sd81mem.asm SD81MEM.BIN
pasmo sd81sys.asm SD81SYS.BIN
pasmo sd81av.asm SD81AV.BIN
```

Cualquier cambio en el núcleo obliga a reensamblar los tres módulos. Si
no se hace, el núcleo lo detecta y no los ejecuta (ver más abajo).

`compila.bat` hace los cuatro pasos, se para en el primer error y copia los
binarios y `SD81TEST.VGM` a la carpeta `SD81\TEST` del emulador EightyOne.
También genera el `.sym` de cada módulo.

| Fichero | Dirección | Contenido |
|---|---|---|
| `SD81TEST.BIN` (`sd81test.asm`) | 22528 (`$5800`) | núcleo: tabla de saltos, rutinas comunes (pantalla, teclado, MCU, mapper), textos y variables compartidas |
| `SD81MEM.BIN` (`sd81mem.asm`) | 24576 (`$6000`) | memoria, mapper, ejecución (MC45) y puertos mapeados en memoria |
| `SD81SYS.BIN` (`sd81sys.asm`) | 24576 (`$6000`) | protocolo MCU, información, frecuencia de cuadro, RTC y SD |
| `SD81AV.BIN` (`sd81av.asm`) | 24576 (`$6000`) | AY, sprites, borde, Chroma81, Superfast, teclado y sonido |

## Uso

Copia los cuatro `.BIN`, `SD81TEST.VGM` (lo usa la prueba de VGM) y el
stub BASIC (`SD81TEST.B81`, pásalo a `.P` con el emulador) a la misma
carpeta de la SD. El stub carga el núcleo, y antes
de cada prueba carga su módulo si no está ya cargado. Muestra un menú por
categorías, cada una con su submenú:

```
SD81 BOOSTER HARDWARE TEST

1 MEMORY AND MAPPER
2 EXECUTION (MC45)
3 MEMORY-MAPPED PORTS
4 MCU, RTC AND SD
5 VIDEO
6 SUPERFAST VIDEO
7 SOUND
8 INPUT (KEYBOARD, JOYSTICK)
9 INFO AND SUMMARY
0 EXIT
```

El menú está en BASIC y en SLOW. No espera las teclas con `PAUSE`, porque
en SLOW `PAUSE` pasa a FAST y genera la imagen por su cuenta, y la imagen
salta al entrar y al salir. Tampoco usa un bucle con `INKEY$`, porque en
SLOW da una vuelta cada muchos milisegundos y se come las pulsaciones
cortas. Usa dos rutinas del núcleo que leen el teclado sin parar:
`USR 22600` espera una cifra y devuelve su valor, y `USR 22603` espera una
tecla cualquiera. Las pruebas solo pasan a FAST cuando
lo necesitan: si miden con FRAMES, si tocan la ROM o la página del vídeo,
o si remapean bloques que en modo 32K son el espejo del vídeo. Cada prueba
tiene un punto de entrada fijo en la tabla de saltos del principio del
núcleo (prueba N en `USR 22528+3*N`, así añadir pruebas no cambia las
anteriores), deja sus resultados en pantalla y devuelve su número de
fallos. La entrada del núcleo comprueba que en `$6000` está el módulo de
esa prueba, ensamblado con este núcleo: la cabecera del módulo lleva su
número y el `bss_end` del núcleo. Si no, escribe `MODULE MISSING OR
OUTDATED` y devuelve 9999. Además apunta el resultado de cada prueba que
se ejecuta, para el resumen (ver más abajo). Para llamar a una prueba a mano con `USR`, hay
que cargar antes su módulo con `LOAD FAST "SD81xxx.BIN" CODE 24576`.
Las entradas 24 y 25 (`USR 22600` y `USR 22603`) son las rutinas de
teclado del menú; las pruebas nuevas irán a partir de la 26.

| N | Entrada | Módulo | Prueba | Devuelve |
|---|---|---|---|---|
| 0 | `USR 22528` | MEM | Memoria (destructiva) | páginas con errores (255 = no se ejecutó, modo 32K) |
| 1 | `USR 22531` | MEM | MC45 bloques 4–5 | comprobaciones fallidas (0–10) |
| 2 | `USR 22534` | MEM | MC45 bloques 6–7 | comprobaciones fallidas (0–8; 255 = no se ejecutó) |
| 3 | `USR 22537` | MEM | Estrés del mapper | errores de relectura + enrutado |
| 4 | `USR 22540` | MEM | Captura de `POKE 2045` | fallos (de 200) |
| 5 | `USR 22543` | — | *(reservada: interrupciones simuladas, quitada de momento)* | siempre 9999 |
| 6 | `USR 22546` | MEM | ROMLOCK | fallos (de 30) |
| 7 | `USR 22549` | SYS | Protocolo con el MCU | valores erróneos (9999 = el MCU dejó de contestar) |
| 8 | `USR 22552` | SYS | Información de la máquina | 0 |
| 9 | `USR 22555` | MEM | Memoria no paginada en los bloques 4–7 | comprobaciones que no son PAGED (0–16) |
| 10 | `USR 22558` | MEM | Registros del mapper | lecturas erróneas |
| 11 | `USR 22561` | MEM | Protección del bloque 0 | direcciones que se han podido escribir (0–10) |
| 12 | `USR 22564` | SYS | Frecuencia de cuadro | medidas fuera de rango (9999 = el RTC no contesta) |
| 13 | `USR 22567` | SYS | RTC y batería | fallos (9999 = el RTC no contesta) |
| 14 | `USR 22570` | AV | Registros de los AY | lecturas erróneas |
| 15 | `USR 22573` | SYS | Lectura de la SD | bytes distintos (9999 = timeout o no hay fichero) |
| 16 | `USR 22576` | SYS | Escritura de la SD | fallos (9999 = timeout o no se puede abrir) |
| 17 | `USR 22579` | MEM | Páginas de sistema (bloques 0–3) | bytes que no se pueden escribir |
| 18 | `USR 22582` | AV | Sprites (interactiva) | respuestas N (0–3) |
| 19 | `USR 22585` | AV | Patrón de borde (interactiva) | respuestas N (0–1) |
| 20 | `USR 22588` | AV | Chroma81 (interactiva) | respuestas N (0–1) |
| 21 | `USR 22591` | AV | Modos Superfast (interactiva) | respuestas N (0–3) |
| 22 | `USR 22594` | AV | Teclado y joystick (interactiva) | respuestas N (0–1) |
| 23 | `USR 22597` | AV | Sonido (interactiva) | respuestas N (0–2) |
| 24 | `USR 22600` | — | *(utilidad del menú: espera una cifra)* | la cifra |
| 25 | `USR 22603` | — | *(utilidad del menú: espera una tecla)* | — |
| 26 | `USR 22606` | AV | Juegos de caracteres 128C y 256C (interactiva) | respuestas N (0–3) |
| 27 | `USR 22609` | AV | 70 y 80 columnas (interactiva) | respuestas N (0–2) |
| 28 | `USR 22612` | AV | Scroll fino (interactiva) | respuestas N (0–1) |
| 29 | `USR 22615` | AV | Chroma81 modo 1 (interactiva) | respuestas N (0–2) |
| 30 | `USR 22618` | AV | Doble buffer (interactiva) | respuestas N (0–3) |
| 31 | `USR 22621` | AV | Registros del AY del MCU | lecturas erróneas (9999 = el MCU no contesta) |
| 32 | `USR 22624` | AV | Beeper del modo Spectrum (interactiva) | respuestas N (0–2) |
| 33 | `USR 22627` | AV | Reproductor VGM (interactiva) | respuestas N o error del MCU (9999 = no contesta) |
| 34 | `USR 22630` | AV | Generador de efectos PEG (interactiva) | respuestas N (9999 = el MCU no contesta) |
| 35 | `USR 22633` | AV | Alta resolución WRX (interactiva) | respuestas N (0–3) |
| 36 | `USR 22636` | SYS | Reloj de la CPU | 0 = 3,20–3,30 MHz, 1 = fuera (9999 = el RTC no contesta o no da centésimas) |
| 37 | `USR 22639` | — | *(utilidad: borra los resultados apuntados)* | — |
| 38 | `USR 22642` | SYS | Resumen (y `SD81TEST.TXT`) | pruebas con FAIL |

- Todo vive en los bloques 2 y 3, por encima de RAMTOP, y tiene que quedar
  por debajo de `$8000`, porque las pruebas remapean los bloques 4–7:
  - El BASIC (stub, pantalla y variables) va hasta 22527 (`CLEAR 22527`).
    Con el núcleo en 20480 se quedaba sin memoria (informe 4).
  - El núcleo va de 22528 (`$5800`) a `$5FFF`. Su final real, con los
    buffers, que no van en el `.bin`, es `bss_end` en `sd81test.sym`.
  - Cada módulo va de `$6000` a `$7FFF`, el bloque 3 entero. Su final es
    `mbss_end`.
- Necesita el **modo 48K** (el de defecto) para el test de memoria y el de
  MC45 en los bloques 6–7: en modo 32K (`LOAD *RAM48 STOP`) los bloques 6/7
  son el espejo que lee el vídeo; esos tests lo detectan y no arrancan.
- Para probar las 64 páginas (512K), ejecuta antes `LOAD *FULLPAG`. En
  paginación simple solo se prueban 32.
- Las pruebas que necesitan FAST usan las rutinas de la ROM `SET_FAST`
  (`$02E7`) y `SLOW_FAST` (`$0207`), las mismas que el `LOAD` del
  interface. Da igual llamarlas desde FAST o desde SLOW. Las de protocolo
  MCU e información van en SLOW; las de SD, en FAST, porque tardan.

**El test de memoria es destructivo** para todas las páginas que no son de
sistema: discos RAM de CP/M, pantallas guardadas, etc. Lo normal es
ejecutarlo tras un reset. Los de MC45 solo machacan unos pocos bytes de
los bloques 4 a 7. El resto no pierde datos (el estrés del mapper guarda y
restaura los bytes que usa como firma).

## Test de memoria

1. Guarda el mapeo de los 8 bloques (puerto `$E7`) y detecta paginación
   simple (32 páginas) o completa (64).
2. Las páginas mapeadas en los bloques 0–3 (ROM, ROM de expansión, BASIC,
   el propio programa y su pila) son de **sistema** y no se tocan.
3. **Pasada A:** rellena todas las demás páginas, vistas una a una por el
   bloque 6 (`$C000`), con `(H AND 1Fh) XOR L XOR página`, y después las
   verifica todas. Como el patrón depende de la dirección y de la página,
   detecta bits de datos, líneas de dirección dentro de la página y páginas
   que se solapan (escribir en una machaca otra).
4. **Pasada B:** lo mismo con el patrón invertido, para que cada bit se
   pruebe a 0 y a 1.
5. **Enrutado:** vuelve a leer cada página a través de los bloques 4, 5, 6
   y 7 (32 muestras) para comprobar que cada bloque lleva sus accesos a la
   página que indica el mapper.
6. Restaura el mapeo de los bloques 4–7 y deja el resumen en pantalla.

### Pantalla

```
SD81 BOOSTER TEST - MEMORY V0.9
PAGES: 64 (FULL PAGING)
S=SYSTEM .=OK X=FAIL R=ROUTING

00 S S S S . . . .
08 . . . . . . . .
...

VERIFYING                      <- fase en curso

PASS A (DIRECT): OK
PASS B (INVERTED): OK
ROUTING (BLOCKS 4-7): OK

PAGES WITH ERRORS: 00
```

- La página que se está probando aparece en **vídeo inverso** en la rejilla.
- `-`: pendiente. `S`: sistema (no se prueba). `.`: correcta.
- `X`: falla en el relleno/verificación (pasada A o B).
- `R`: pasa las pasadas A y B, pero falla al leerla a través de algún
  bloque del 4 al 7.
- Cada fase muestra `OK` o el número de páginas que han empezado a fallar
  en ella.

Si hay errores, al final se añade el primero encontrado: página, bloque por
el que se leía, dirección, valor esperado y valor leído.

La pantalla se escribe directamente en `D_FILE` (sin `RST 10h`). Como el
programa corre con el vídeo encendido, no toca IX, IY, I ni AF' ni
deshabilita las interrupciones: los usa la ROM para generar la imagen.

## Test de MC45

Escribe `LD BC,0302h / RET` (`01 02 03 C9`) en cinco puntos de los bloques
4 y 5 (`$8000`, `$9C40`, `$9FFC`, `$A000` y `$BFFC`) y lo ejecuta con BC=0:

1. Con **MC45 activo** (comando MCU `$13`) tiene que devolver `0302`: el
   código se ejecuta con normalidad.
2. Con **MC45 apagado** (comando `$14`) tiene que devolver `0000`: `01`,
   `02` y `03` tienen el bit 6 a 0 y A15=1, así que llegan a la CPU como
   NOPs forzados, y solo se ejecuta el `RET` (`C9`, bit 6 a 1).

La primera mitad comprueba que MC45 funciona. La segunda, que la máquina
fuerza de verdad los NOPs por encima de 32K, que es justo lo que MC45 tiene
que anular. Al terminar deja MC45 apagado.

```
SD81 BOOSTER TEST - MC45 V0.9

CODE IN BLOCKS 4-5: LD BC,0302
MC45 ON:  MUST RETURN 0302
MC45 OFF: FORCED NOPS, 0000

ADDR  MC45 ON     MC45 OFF
8000  0302 OK     0000 OK
...

RESULT: OK
```

## Test de MC45 en los bloques 6 y 7

Con MC45 activo y `POKE 2062,170` se puede ejecutar código también en los
bloques 6 y 7, siempre que estén mapeados a páginas propias (modo 48K) y no
al espejo de los bloques 2/3. La prueba escribe `LD BC,0302h / RET` en
cuatro direcciones de esos bloques y lo ejecuta:

1. **MC45 + extensión** (`POKE 2062,170`): tiene que devolver `0302`, el
   código corre desde las páginas de los bloques 6/7.
2. **MC45 sin extensión** (`POKE 2062,85`): tiene que devolver `0000`. En
   modo 48K la FPGA manda las búsquedas de opcode de `$C000-$FFFF` al mismo
   desplazamiento de los bloques 2/3, así que la CPU ejecuta lo que haya
   ahí.

Por eso solo se prueba en direcciones cuyo "espejo" en los bloques 2/3
controla el programa y contiene un `RET`: `$C03C` y `$C040` (su espejo es el
buffer de impresora, `$403C`/`$4040`, que se guarda y se restaura) y dos
direcciones del bloque 6 o 7 cuyo espejo (la misma dirección menos `$8000`)
cae dentro del propio programa.
Saltar a cualquier otra dirección ejecutaría variables de sistema o el
BASIC y colgaría la máquina.

- Se ejecuta en **FAST**: con la extensión activa el vídeo nativo, que
  ejecuta `D_FILE+$8000`, ya no iría a los bloques 2/3. El programa usa
  las rutinas de la ROM `SET_FAST` (`$02E7`) y `SLOW_FAST` (`$0207`), las
  mismas que el `LOAD` del interface, así que funciona se llame desde FAST
  o desde SLOW.
- Si los bloques 6/7 son espejo de 2/3 (modo 32K o un `MAP` manual) no se
  ejecuta: escribir el código de prueba machacaría el sistema.
- Necesita ROMLOCK apagado: con `LOAD *ROMLOCK` el `POKE 2062` no tiene
  efecto y la primera mitad falla.
- Al terminar deja MC45 y la extensión apagados.

```
SD81 TEST - MC45 BLOCKS 6-7 V0.9

CODE IN BLOCKS 6-7: LD BC,0302
MC45+POKE 2062,170: 0302
MC45+POKE 2062,85: MIRROR 0000

ADDR  EXT ON      EXT OFF
C03C  0302 OK     0000 OK
...

RESULT: OK
```

## Estrés del mapper

Busca escrituras de puerto (`OUT $E7`) perdidas o corrompidas:

1. Escribe en cada página que no es de sistema una firma (número de página
   y su complemento en los dos primeros bytes; los originales se guardan y
   se restauran al final).
2. **2048 `OUT` sueltos:** bloque (4–7) y página al azar.
3. **512 ráfagas:** los cuatro bloques seguidos, y después se comprueban
   los cuatro.
4. En cada comprobación se relee el registro del mapper (**READBACK**) y se
   lee la firma a través del bloque (**ROUTING**): la primera cuenta
   registros mal escritos; la segunda, bloques que no llevan a la página
   que dice su registro.

## Captura de `POKE 2045`

En Superfast la FPGA decrementa su propia copia de `FRAMES` en cada VSYNC,
y es la que se lee en 16436; con vídeo nativo se lee la RAM, que en FAST
nadie toca. Eso permite saber si un `POKE 2045` ha tenido efecto:

- 100 veces `POKE 2045,170`: `FRAMES` tiene que empezar a correr.
- 100 veces `POKE 2045,85`: `FRAMES` tiene que quedarse quieto.

Cada comprobación escribe 1000 en `FRAMES`, espera 1,5 cuadros y mira si
ha bajado de 1 a 3. Al terminar se restaura `FRAMES` y se deja el vídeo
nativo.

## Interrupciones simuladas (quitada de momento)

La prueba se ha quitado: la FPGA solo sustituye el `RST 38h` en Superfast
y no genera `/INT` (en el ZX81 sale de A6), así que de momento no sirven
como interrupción por cuadro. `USR 22543` sigue reservada y devuelve 9999.

## ROMLOCK

10 veces: sin bloqueo (`LOAD *ROMLOCK STOP`, comando MCU `$45`)
`POKE 2045,170` y `POKE 2045,85` tienen que funcionar; con bloqueo
(`LOAD *ROMLOCK`, comando `$44`) `POKE 2045,170` no tiene que tener
efecto. Se mide con `FRAMES`, igual que en la prueba anterior. Deja
ROMLOCK apagado.

## Protocolo con el MCU

2000 veces `SETBYTE` + `GETBYTE` en un índice volátil al azar (64–127, que
no usa nadie) con un valor al azar, comparando lo que se lee. Cada espera
del handshake tiene límite de tiempo (~1 s): si el MCU deja de contestar,
la prueba se para y dice en qué transferencia.

## Información de la máquina

- Versiones del MCU, de la ROM y de la FPGA.
- Bit 6 del puerto FE (puente 50/60 Hz de la ULA) y `MARGIN`.
- Paginación simple o completa.
- Bloques 6/7: páginas propias (48K) o espejo de 2/3 (32K).
- Tabla del mapper (página de cada bloque) y `RAMTOP`.

## Memoria no paginada en los bloques 4–7

Busca algo dentro de la máquina que conteste en `$8000-$FFFF` a la vez que
el interface y no dependa de la página: por ejemplo, la RAM interna de 16K
del TS1500 o un espejo de la ROM. Para cada bloque del 4 al 7 y cuatro
desplazamientos, con dos páginas libres P y Q, escribe `A5` con P mapeada y
`5A` con Q, y relee con P (1) y con Q (2):

| 1 | 2 | Resultado | Significado |
|---|---|---|---|
| `A5` | `5A` | PAGED | Lo sirve el interface, como debe |
| `5A` | `5A` | FIXED | Algo sin paginar se queda con la última escritura |
| otro | otro | CONFL | Dos dispositivos pelean por el bus |

Por bloque se muestran dos líneas:

```
BLK4 PAGED:4 FIXED:0 CONFL:0
 0:00 F:FF 1:A5 2:5A L:xx R:xx
```

La segunda es el detalle del primer desplazamiento: `0`/`F`, lo que se lee
tras escribir `00` y `FF` (la forma del choque: AND, OR, gana uno...);
`1`/`2`, lo leído con P y con Q; `L`/`R`, lo que hay en ese momento en el
espejo de RAM baja (`$4000`/`$6000`) y de ROM (`$0000`). Machaca esos pocos
bytes de las páginas P y Q.

## Registros del mapper

Cada página en cada bloque del 4 al 7, con el formato del modo activo. El
campo que no debe usarse lleva basura aleatoria, para comprobar que se
ignora:

- **Paginación simple:** la página va en D7–D3 del dato y B lleva basura.
- **Paginación completa:** la página va en B y D7–D3 llevan basura.

Tras cada escritura se releen los cuatro bloques: el escrito tiene que
tener su página nueva y los otros tres, la suya de antes. Así se detectan
escrituras que acaban en el bloque equivocado.

## Protección del bloque 0

El bloque 0 es de solo lectura. La prueba escribe el complemento en diez
direcciones de la ROM que no son puertos de configuración (se evita el
rango 2038–2130) y comprueba que no cambian. Si alguna cambia, se restaura
en el acto. Se hace en FAST y con las interrupciones deshabilitadas.

## Frecuencia de cuadro

Cuenta los VSYNC durante un segundo del RTC del MCU. El puerto `$AF` da en
D6–D1 los VSYNC desde su lectura anterior y se pone a cero al leerlo, así
que se acumulan todas sus lecturas, incluidas las del propio protocolo con
el MCU.

- **Vídeo nativo:** necesita SLOW (sin vídeo no hay VSYNC). Tiene que dar
  50 o 60 según `MARGIN` (55 o 31), con ±2 de margen.
- **Superfast:** la FPGA genera su propio cuadro PAL; tiene que dar 50 ±2.

## Resumen y pasada automática

La entrada común del núcleo (`run_t`) apunta en `rep_res` el resultado de
cada prueba que se ejecuta. `USR 22639` borra esos resultados; el stub lo
hace al arrancar. En el menú **9 INFO AND SUMMARY**:

- **2 RUN ALL AUTOMATIC TESTS:** borra los resultados y ejecuta, una tras
  otra, las 17 pruebas automáticas, cargando el módulo que toque. Son
  todas menos la de memoria, que es destructiva, las interactivas y la
  ficha de la máquina: MC45 4–5 y 6–7, estrés del mapper, `POKE 2045`,
  ROMLOCK, protocolo MCU, memoria no paginada, registros del mapper,
  bloque 0, frecuencia de cuadro, RTC, AY, lectura y escritura de SD,
  páginas de sistema, AY del MCU y reloj de la CPU. Al acabar muestra el
  resumen. Algunas machacan unos pocos bytes de los bloques 4–7.
- **3 SUMMARY OF LAST RESULTS:** el resumen de lo que se haya ejecutado
  desde entonces, también a mano desde los otros menús.

El resumen (`USR 22642`) muestra una línea por prueba automática con
`OK`, `FAIL` y el valor que devolvió, o `NOT RUN`, y los totales. Además
lo guarda en `SD81TEST.TXT`, en la carpeta actual: ASCII con CR LF, con
la fecha del RTC y las versiones de MCU, ROM y FPGA en la cabecera, para
comparar máquinas. El texto se monta con las mismas rutinas de pantalla,
en códigos ZX81 en `$8000`, y se pasa a ASCII al mandarlo. Devuelve el
número de pruebas con `FAIL`.

## Reloj de la CPU

Mide a qué velocidad va el Z80. En FAST, sin NMI ni vídeo, la CPU va a
toda velocidad:
1. Se sincroniza con un cambio de segundo del RTC.
2. Ejecuta un bucle de exactamente 16.248.960 T (4960 vueltas de 3276 T),
   que son 5,0 s a 3,25 MHz.
3. Vuelve a leer el RTC.

El resultado es MHz = T / transcurrido. Con las centésimas del RTC, que el
del STM32 da de verdad, la resolución es de 1/100 s en 5 s, un 0,2 %. La
latencia de las dos lecturas es la misma y se cancela. Tiene que salir
3,25 MHz ± 1,5 %: sirve para clones o máquinas modificadas a otra
velocidad. Si el RTC no da centésimas (siempre `.00`, como el emulador de
momento), lo dice y no mide, porque solo con segundos el error sería de
hasta un 20 %.

## RTC y batería

- Muestra la fecha y la hora del RTC.
- Comprueba que los segundos avanzan.
- Lee la tensión de la batería del RTC (`LOAD *BAT`) y comprueba que está
  entre 2,5 y 3,6 V.

## Registros de los chips AY

Escribe y relee los registros de 8 bits (0, 2, 4, 11 y 12) de los dos AY
de la FPGA con 12 patrones, y al final los restaura:

- **Chip A (ZonX):** selección y lectura en `$CF`, dato en `$0F`.
- **Chip B:** selección y lectura en `$C7`, dato en `$07`. La FPGA ignora
  A0, y se usan puertos impares porque un `IN`/`OUT` a un puerto par
  también lo decodifica la ULA (teclado y NMI).

## Lectura de la SD

Lee `/SYS/SDBOOST.ROM` con `F_OPEN`, `F_STAT` y `F_READ` (bloques de 256
bytes) y lo compara byte a byte con la ROM cargada en memoria, que el MCU
pone en la dirección 0 (bloques 0 y 1). Cuenta las diferencias de cada
bloque y muestra la dirección de la primera. Después lo lee otra vez y
compara la suma de control de las dos lecturas.

Si en el bloque 1 aparecen diferencias concentradas en una zona, pueden ser
variables que la ROM de expansión guarda dentro de su propio espacio: la
primera dirección distinta ayuda a saberlo.

## Escritura de la SD

1. `SAVE` de `/SD81TEST.TMP` con 4096 bytes pseudoaleatorios.
2. `F_OPEN` + `F_STAT` (tiene que medir 4096) + `F_READ`: el contenido
   tiene que coincidir.
3. `F_SEEK` a 1000 + `F_WRITE` de 256 bytes nuevos, y `F_SEEK` + `F_READ`
   para releerlos.
4. `F_CLOSE` + `DEL`. Volver a abrirlo tiene que fallar.

Todos los comandos usan el protocolo de un cambio de reloj por byte (el del
explorador), con límite de tiempo.

## Páginas de sistema

Prueba no destructiva de las páginas mapeadas en los bloques 0–3 (ROM, ROM
de expansión, BASIC y este programa), vistas por el bloque 5: cada byte se
lee, se escribe su complemento, se relee y se restaura. Se hace en FAST y
con las interrupciones deshabilitadas.

La página de este programa se prueba con una copia del bucle de prueba en
el buffer de impresora, para no modificar nunca los bytes que se están
ejecutando. El bucle solo usa saltos relativos, así que funciona en
cualquier dirección.

## Pruebas interactivas

El programa pinta o suena algo y pregunta; se contesta con **Y** o **N**
(cada N cuenta como un fallo). Necesitan SLOW, porque la pantalla tiene que
verse mientras esperan tecla y se temporizan con `FRAMES`; en FAST avisan y
no se ejecutan.

### Sprites

Configura los 32 sprites por hardware como cajas de 8×8 en una rejilla de
8×4 (selección, activación, posición, color, datos y máscara por `POKE`
2100–2128) y hace tres preguntas: si se ven los 32, si se desplazan 40
píxeles a la derecha y vuelven, y si desaparecen al desactivarlos. Si falla
alguna escritura se ve en qué sprite y en qué campo.

### Patrón de borde

Tablero de ajedrez en el borde (`POKE 2048-2055` y `POKE 2047,170`) en
Superfast texto, y después borde liso (`POKE 2047,85`). El patrón es solo
de Superfast: en vídeo nativo no se prueba.

### Chroma81

Modo 0 (color por código de carácter): tabla de color en `$C000`, con 8
bytes por código, uno por línea de píxeles, papel en el nibble alto y
tinta en el bajo. Todo en papel blanco salvo las letras A–H, con papel de
0 a 7; se pintan 8 filas con esas letras, que tienen que verse como barras
negra, azul, roja, magenta, verde, cian, amarilla y blanca. Una novena fila
con la I tiene un papel distinto en cada línea de píxeles: se ven 8 franjas
finas dentro de la fila. Escribe en la
página del bloque 6, así que no se ejecuta en modo 32K.

### Modos Superfast

- **Texto:** la misma pantalla, generada por la FPGA.
- **HiRes nativo:** bitmap lineal en `$E000` (HFILE); arriba tablero de
  ajedrez, abajo barras verticales.
- **Spectrum:** bitmap con el orden de líneas del Spectrum en `$E000` y
  atributos en `$F800`: 8 bandas de color y una X (comprueba el reordenado
  de líneas). El color del modo Spectrum necesita Chroma activado.

Los dos modos gráficos tapan la pantalla de texto: se entra con una tecla y
se vuelve con otra, y después se pregunta. Escribe en la página del bloque
7, así que no se ejecuta en modo 32K.

### Teclado y joystick

Matriz de las 40 teclas en vivo: la que está pulsada sale en vídeo inverso
(SHIFT se muestra como `*`, ENTER como `>` y SPACE como `-`). El joystick
se ve como las teclas que tiene asignadas, porque la FPGA las inyecta en la
lectura del teclado. También muestra en vivo los bits 6 (puente 50/60 Hz)
y 7 (entrada de cinta) del puerto FE. Se sale con SHIFT+SPACE.

### Sonido

Seis tonos cada vez más agudos, unos 0,4 s cada uno: canales A, B y C del
chip A (`$CF`/`$0F`) y después del chip B (`$C6`/`$06`, los puertos
documentados). Luego `SAY "HELLO"` con el sintetizador de voz del MCU. En
el emulador el chip A solo suena con el tipo de AY configurado como ZonX.

### Registros del AY del MCU

Es el AY que emula el MCU, el que suena con `PLAY`, VGM, PEG y `SAY`, no
los dos de la FPGA. Escribe R0–R15 con `AY_SET_REG` (24), usando los 12
valores de la prueba de los AY, y los relee con `AY_GET_REG` (25). Tiene
que salir el byte entero, porque el MCU guarda lo escrito tal cual. R14 y
R15 necesitan un firmware posterior al arreglo de `cmd_AY_get_reg`. Antes
comparaba el registro con `015`, que en C es octal y vale 13, y R14/R15
salían siempre a 0: con un firmware anterior hay unos 22 fallos. Al final
deja el mezclador apagado y los volúmenes a 0.

### Beeper del modo Spectrum

En modo Spectrum (`POKE 2045,172`) el puerto `FBh` hace de ULA del
Spectrum: el bit 4 (EAR) es el beeper, y los bits 2–0 son el color del
borde, que necesita Chroma. El bit 3 (MIC) apenas suena. Toca tres tonos
cada vez más agudos, de unos 500, 665 y 900 Hz y unos 0,4 s cada uno, con
el borde en azul, rojo y verde. Los tonos se generan en FAST, porque sin
las NMI el periodo es estable; la imagen la sigue dando la FPGA. La
pantalla Spectrum, en blanco, va en `$8000`.

### Reproductor VGM

Reproduce con `PLAY_VGM` (34), que lo abre y empieza a sonar,
`SD81TEST.VGM`, un fichero de 380 bytes que va en la misma carpeta que los
binarios. Tras unas 4 notas lo pausa 1 s (36) y lo reanuda (37); al final
lo para (35). Tiene que oírse la escala con un silencio a la mitad. Si el
fichero no está, sale `VGM ERROR, STATUS n`.

El fichero lleva una cabecera completa de 256 bytes: versión 1.51, fin
del fichero, número de muestras, reloj del AY y desplazamiento de los
datos en `$34`. Los datos empiezan en `$100`, como en la mayoría de VGM
reales: el firmware lee el desplazamiento de `$34`, pero otros
reproductores, como el emulador, saltan siempre a `$100`. Los datos son la
escala de do mayor en el canal A, a 0,25 s por nota. Cada nota vuelve a
escribir el mezclador y el volumen: la pausa reinicia el AY del MCU y la
reanudación no lo restaura, así que un VGM que solo los escribiera al
principio seguiría en silencio.

El VGM no se crea ni se borra en cada prueba:
- **Firmware:** `STOP_VGM` no cierra el fichero, solo lo rebobina. Borrar
  un fichero abierto no es seguro, porque el reproductor se queda
  apuntando a clusters liberados. El fichero solo se cierra si llega al
  final sin STOP.
- **Emulador:** no lo cierra nunca, ni siquiera al acabar, así que en
  Windows se queda bloqueado.

### Generador de efectos PEG

Carga con `LOAD_PEG` (40) dos programas de `EXAMPLES/PEG`, con los bytes
de sus `.peb`: `coin` en la dirección 0 y `siren` en la 16. Son palabras
de 16 bits, con el byte bajo primero, y como los saltos son relativos
sirven en cualquier dirección. Lanza `coin` en el hilo 0 y, cuando acaba,
`siren` en el hilo 1 con `PLAY_PEG` (41). Tienen que oírse las tres notas
de la moneda y después la sirena, unos 1,8 s.

### Pantalla alternativa

Las pruebas de 256 caracteres, 70/80 columnas y scroll fino montan su
propia pantalla en `$8000` (bloque 4) y la muestran con la dirección de
pantalla alternativa (`POKE 2096/2097` y `POKE 2098,170`). El BASIC y la
ROM siguen con su D_FILE, que en SLOW se ejecuta: no admite códigos con el
bit 6 a 1 ni otro ancho de fila. `POKE 2045,85` apaga la dirección
alternativa sola. Como en los modos Superfast, se entra con una tecla y se
vuelve con otra. Estas pruebas y la de doble buffer machacan el contenido
de los bloques 4 y 5.

### Juegos de caracteres (128C y 256C)

- **128C, en vídeo nativo:** pinta los códigos 0–63 y 128–191 en las filas
  3–6, manda el comando `SEL_128CHARS` (`$1B`) y pone I = `$3C`. La tabla
  de `$3C00-$3FFF` viene precargada con los caracteres de la ROM, así que
  la pantalla tiene que verse igual. Después da la vuelta a los 128
  glifos, y toda la pantalla, textos incluidos, tiene que salir boca
  abajo. Luego los deja como estaban y vuelve a 64C (`$1C`, I = `$1E`).
- **256C, en Superfast texto:** los 256 códigos en 16 filas de 16, en la
  pantalla alternativa, con `SEL_256CHARS` (`$41`) e I = `$38`. Se da la
  vuelta a los grupos 1 (`$3A00`, códigos 64–127) y 3 (`$3E00`,
  192–255). Las cuatro bandas de 4 filas tienen que salir así: normal,
  boca abajo, inversa, e inversa boca abajo. El bit 7 invierte también en
  este modo.

Al final restaura las tablas, el registro I y el modo de caracteres que
hubiera antes. Apaga WRX (`POKE 2058,85`), porque con WRX la zona de 8–16K
deja de ser la tabla de caracteres.

### 70 y 80 columnas

Superfast texto ancho sobre la pantalla alternativa: `POKE 2045,173` da 70
columnas de 8 píxeles, con filas de 71 bytes, y `POKE 2045,174` da 80
columnas de 7 píxeles, con filas de 81 bytes. Cada fila lleva un byte de
relleno al final, como el NEWLINE del modo de 32 columnas, y la pantalla
empieza con un byte inicial; el hardware no lee ninguno de los dos. Cada
fila lleva un bloque en la primera y en la última columna. Arriba y
abajo hay una regla con las unidades (filas 0 y 23) y las decenas (filas 1
y 22) de cada columna. Tiene que verse entera, sin cortes por ningún lado.
No usa `LOAD *COL70/*COL80`, que además parchean la ROM: aquí solo se
prueba el vídeo.

### Scroll fino

Barras verticales, un bloque cada 4 columnas, en Superfast texto sobre la
pantalla alternativa. También hay bloque en la columna 32, que es la que
asoma al desplazar. Con `POKE 2091-2093,55h` solo se desplazan las filas
pares, y `POKE 2090` va y viene de 0 a 7 cada 2 cuadros hasta que se pulsa
una tecla. Al terminar deja el scroll a 0 y las tres máscaras a 255, como
tras un reset.

### Chroma81 modo 1

Superfast texto con color en modo 1, por posición (`OUT $7FEF,37h`):
- **Tabla de siempre:** en D_FILE+`$8000` (bloque 6), con papel =
  columna/4, que da 8 barras verticales.
- **Tabla alternativa** (`POKE 2059/2060` = `$A000`, `POKE 2061,170`): con
  papel = fila/3, que da 8 barras horizontales.

La tinta es blanca sobre los papeles oscuros y negra sobre los claros, así
que los textos de la prueba se siguen leyendo. No se ejecuta en modo 32K.

### Alta resolución WRX

Usa el controlador de Wilf Rigter de 1996, que viene en la documentación
de EightyOne. En cada una de las 192 líneas pone I = byte alto de la
dirección de la línea y R = byte bajo, y ejecuta en la mitad alta 32 bytes
a 0. El vídeo los convierte en NOP y, en el refresco de cada uno, el
pixel sale de la dirección I:R. Cada línea dura 207 T. El controlador se
engancha con IX, el vector del vídeo de la ROM, y se sale volviendo a
IX = `$0281`. Tiene tres partes, cada una con tecla para ver y tecla para
volver:

| Parte | Bitmap | `POKE 2058` | Qué se tiene que ver |
|---|---|---|---|
| A | `$A000` (bloque 5) | — | el marco y la X |
| B | la misma página, vista en `$2000` (bloque 1) | 170 (WRX en 8–16K) | lo mismo |
| C | igual que B | 85 (generador de caracteres) | una imagen revuelta, sin la X |

La parte A comprueba el WRX normal: con I ≥ `$40`, la FPGA siempre deja
pasar la dirección de refresco. B y C comprueban el conmutador de 8–16K
(`POKE 2058`). En C se ven columnas de puntos: todas las columnas de una
línea leen el mismo byte, porque el código de carácter es el del NOP.
En el emulador, de momento, la parte A sale en negro: con el SD81 aplica
`POKE 2058` a cualquier valor de I (ver la especificación del emulador).

El bloque 1 guarda la ROM de expansión (de `$2000` a unos `$331B`), así que
durante B y C la prueba pone en el bloque 1 la página del bloque 5, y al
terminar lo devuelve a su página. En ese tiempo no se usa nada de la ROM
de expansión: la rutina de vídeo de la ROM está en el bloque 0 y es
idéntica a la original.

### Doble buffer

HiRes nativo con HFILE en `$8000` (bloque 4). Tiene dos partes, y cada una
se ve con una tecla y se deja con otra.

**AUTO** (`POKE 2057,173`, front = bloque 5). En cada VSYNC la FPGA copia el
bloque HFILE (4) al espejo del front, que es lo que se ve. La secuencia es:
1. Se muestra un tablero de ajedrez.
2. Al activar el doble buffer, la imagen no tiene que cambiar.
3. Se llena de ruido la SRAM del bloque 5. No tiene que verse: la máscara
   de escritura impide que llegue al espejo del front.
4. Se pintan barras en el bloque 4. Van apareciendo a medida que se
   dibujan: cada cuadro es una foto completa del bloque 4 en ese VSYNC, y
   el dibujo dura varios cuadros.

**MANUAL** (`POKE 2057,200+B`). No hay copia: se ve el espejo del front. La
secuencia es:
1. Se pinta un tablero en el bloque 4 y se pone como front.
2. Se pintan barras en el bloque 5, que no se ve.
3. Se llena de ruido el bloque 4, el front. No tiene que verse.
4. Al pasar el front al bloque 5, las barras tienen que aparecer de golpe.

Al final pregunta tres cosas: si en AUTO se vio el tablero y luego las
barras, si en MANUAL las barras aparecieron de golpe, y si en ningún
momento apareció ruido.

## Plan de pruebas

Estado: **hecha**, *pendiente*.

| # | Categoría | Prueba | Tipo | Estado |
|---|---|---|---|---|
| 1.1 | Memoria y mapper | Memoria (páginas, patrones, enrutado) | Auto, destructiva | **hecha** |
| 1.2 | Memoria y mapper | Páginas de sistema (0–3), no destructiva | Auto | **hecha** |
| 1.3 | Memoria y mapper | Registros del mapper, todos los valores y formatos | Auto | **hecha** |
| 1.4 | Memoria y mapper | Estrés del mapper | Auto | **hecha** |
| 1.5 | Memoria y mapper | Protección del bloque 0 | Auto | **hecha** |
| 1.6 | Memoria y mapper | Modo 32K/48K (espejos de 6/7) | Auto | *descartada*: no se puede comprobar sin ejecutar código arriesgado en 6/7 |
| 1.7 | Memoria y mapper | Memoria no paginada en los bloques 4–7 | Auto | **hecha** |
| 2.1 | Ejecución | MC45 bloques 4–5 | Auto | **hecha** |
| 2.2 | Ejecución | MC45 bloques 6–7 | Auto | **hecha** |
| 2.3 | Ejecución | Espejo de vídeo en 48K | Auto | cubierta por la 2.2 (extensión apagada) |
| 3.1 | Puertos en memoria | Captura de `POKE 2045` | Auto | **hecha** |
| 3.2 | Puertos en memoria | Interrupciones simuladas | Auto | *quitada de momento*: sin `/INT` no sirven como interrupción por cuadro |
| 3.3 | Puertos en memoria | ROMLOCK | Auto | **hecha** |
| 3.4 | Puertos en memoria | Ráfagas de POKEs con `LDIR` | Auto/visual | *descartada*: no hay un efecto legible para comprobarla (la captura ya es síncrona) |
| 3.5 | Puertos en memoria | Sprites (rejilla de 32) | Visual | **hecha** |
| 4.3 | Vídeo | Chroma81 modo 0 | Visual | **hecha** |
| 4.4 | Vídeo | Patrón de borde | Visual | **hecha** |
| 4.5 | Vídeo | Superfast texto, HiRes nativo y Spectrum | Visual | **hecha** |
| 4.8 | Vídeo | Juegos de caracteres 128C (nativo) y 256C (Superfast) | Visual | **hecha** |
| 4.9 | Vídeo | 70 y 80 columnas, pantalla alternativa (2096–2098) | Visual | **hecha** |
| 4.10 | Vídeo | Scroll fino por filas (2090–2093) | Visual | **hecha** |
| 4.11 | Vídeo | Chroma81 modo 1, tabla normal y alternativa (2059–2061) | Visual | **hecha** |
| 4.12 | Vídeo | Doble buffer AUTO y MANUAL (2057) | Visual | **hecha** |
| 4.13 | Vídeo | WRX en `$A000` y en 8–16K (2058) | Visual | **hecha** |
| 4.7 | Vídeo | Frecuencia de cuadro (contador VSYNC del puerto `$AF`) | Auto | **hecha** |
| 5.1 | MCU y SD | Protocolo (`SETBYTE`/`GETBYTE`) | Auto | **hecha** |
| 5.2 | MCU y SD | Lectura de SD (`SDBOOST.ROM` contra la ROM en memoria) | Auto | **hecha** |
| 5.3 | MCU y SD | Escritura de SD (fichero temporal) | Auto | **hecha** |
| 5.4 | MCU y SD | RTC y batería | Auto | **hecha** |
| 6.1 | Sonido | Registros de los AY | Auto | **hecha** |
| 6.2 | Sonido | Tonos AY (2 chips × 3 canales) y SAY | Audio | **hecha** |
| 6.3 | Sonido | Registros del AY del MCU (`AY_SET_REG`/`AY_GET_REG`) | Auto | **hecha** |
| 6.4 | Sonido | Beeper y borde del modo Spectrum (puerto `FBh`) | Audio/visual | **hecha** |
| 6.5 | Sonido | Reproductor VGM (reproducir, pausar, reanudar, parar) | Audio | **hecha** |
| 6.6 | Sonido | Generador de efectos PEG (carga y dos hilos) | Audio | **hecha** |
| 7.x | Entrada | Teclado, joystick, bits del puerto FE en vivo | Visual | **hecha** |
| 8.1 | Información | Ficha de la máquina | Auto | **hecha** |
| 8.2 | Información | Reloj de la CPU (bucle en FAST contra las centésimas del RTC) | Auto | **hecha** |
| 9.1 | Resumen | Pasada automática, tabla OK/FAIL e informe `SD81TEST.TXT` | Auto | **hecha** |
