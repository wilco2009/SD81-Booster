# Ayudas hardware para scroll (HiRes / Spectrum) — diseño

Estado (2026-10-25): rama `fpga-scroll`, fase 1 escrita y probada en simulación (ver el final); sin sintetizar ni probar en hardware. Decidido con el usuario:

- Sin anillo: el scroll mete datos nuevos por un lado, como el texto (columna extra).
- El movimiento "puro" de memoria lo hace la FPGA; el Z80 solo regenera lo que entra.
- El origen Y/X en anillo se descarta (obligaría a tener un mundo grande en la BRAM).
- Solo **Spectrum** (no HiRes nativo). Vertical con **registros de región** (fila superior e inferior que se mueven).
- **Bit de ocupado legible por el Z80**, por el puerto del depurador `$3FEF` (ver más abajo).
- Los 64 sprites con K = 12 **ya están en master** (`fa0dd12`, `97fd8d4`; también en origin y github): no hay nada que
  fusionar antes. Medido con esa síntesis: 3.082 LUTs (53 %) y 1.045 slices (73 %), así que caben las ~200-300 LUTs.

## Lo que ya existe (SD81.v)

- `POKE 2090` (0-7 px) + máscara de filas 2091-2093: scroll fino en los tres submodos Superfast. Desplaza la
  **visualización** (la secuencia char/atributo/píxel se adelanta N px), así que el atributo viaja con su byte.
  En HiRes/Spectrum la columna 33 hoy da la vuelta a la 0 (`scr_col1` tiene 5 bits): es lo que hay que cambiar.
- La BRAM de sombra (`bramdp_w`, 64 KB, toda la memoria lógica) tiene el puerto A de escritura (CPU, blit, traza,
  depurador) y el B de solo lectura (vídeo, motor de sprites, lectura del blit).
- El blit del doble buffer (`blit_run`, líneas 254..61, ~7,6 ms de ventana) copia 8 KB en ~630 µs: 2 ciclos por byte a
  26 MHz, la CPU tiene prioridad (`sh_busy`). **Es la plantilla del motor de scroll.**
- Direcciones libres para registros: 2063-2089 (las 2048-2055 son `border_char`, 2100+ sprites).

## Modelo

La pantalla es una **ventana** sobre la memoria: desplazamiento fino (fx, fy = 0-7 px) por visualización, más una
**columna extra a la derecha** (la 33) y una **fila extra abajo** (la 25) que no están en el bitmap sino en búferes de
entrada. Cuando el fino llega a 8, la FPGA mueve la memoria un byte (o una fila de 8 líneas) y el fino vuelve a 0.
Con la columna extra solo a la derecha basta para los dos sentidos: ir a la izquierda del mundo es un movimiento
grueso hacia la derecha (entra la columna 0 desde su búfer) y fino 7.

### Búferes de entrada (sin BRAM nueva)

El bloque de 8 KB de HiRes/Spectrum usa 6.912 bytes (bitmap 6.144 + atributos 768, atributos en `$1800`); sobran **1.280**
en `$1B00-$1FFF`. Caben:

| Dirección (en el bloque) | Bytes | Contenido |
|---|---|---|
| `$1B00-$1BBF` | 192 | derecha, bitmap (uno por línea) |
| `$1BC0-$1BD7` | 24 | derecha, atributos (uno por fila) |
| `$1C00-$1CBF`, `$1CC0-$1CD7` | 192 + 24 | izquierda, bitmap y atributos (los lee el motor, no el vídeo) |
| `$1D00-$1DFF` | 256 | abajo, bitmap: 8 líneas de 32 bytes (se usan 7) |
| `$1E00-$1E1F` | 32 | abajo, atributos |
| `$1E20-$1E3F` | 32 | arriba, atributos (motor) |
| `$1E40-$1E47`, `$1E48` | 7 + 1 | esquina abajo-derecha: bitmap (una por línea de abajo) y atributo |
| `$1F00-$1FFF` | 256 | arriba, bitmap (motor) |

Alineados a propósito (potencias de 2), de modo que las direcciones salen por concatenación, sin sumadores. Todo cabe en
`$1B00-$1FFF`. La esquina existe para que el scroll diagonal tenga un valor definido. Implementado en
`FPGA/SD81V2.1000/spec_scroll_addr.v`.

Los escribe el Z80 con escrituras normales (la sombra las captura). HiRes nativo: el atributo es un área paralela de
6 KB (`attr_addr_m0 = {3'b110,hr_addr}`, color por byte de línea); el motor tendría que moverla también (a revisar).

### Dirección de lectura del vídeo

- Ranura 0-31: la de siempre, con la compensación de fino.
- Ranura 32 (y la fila 25): dirección fija en el búfer de entrada en vez de dar la vuelta. Atributo de la línea de
  píxel: fila de origen `(línea+fy)>>3`, no de pantalla: así el color sigue a su bitmap también en vertical.

### Motor de movimiento (por puerto, en el blanking)

Una orden por puerto: dirección (izq/der/arriba/abajo). Arranca en la línea 254 como el blit, lee por el puerto B y
escribe por el A (en sitio: ascendente si el destino va antes, descendente si va después).

- Horizontal: por cada línea, `mem[c] = mem[c±1]` en 31 bytes; el byte que sale se **guarda en el búfer opuesto** (así
  ir y volver no obliga a regenerar) y entra el del búfer del lado de entrada. Mismo trato para los 24 atributos.
- Vertical: por fila de texto, 8 líneas x 32 bytes + 32 de atributo.
- Debe respetar la máscara de filas (2091-2093) para el horizontal. El vertical mueve solo la **región** de filas
  entre dos registros (fila superior e inferior, 0-23), para dejar marcadores fijos arriba o abajo.
- Coste: ~6.912 bytes x 2 ciclos = ~530 µs; en doble buffer AUTO va **antes** del blit (+630 µs): 1,2 ms de 7,6 ms.
- Ni el motor ni el blit se pisan con la CPU: la CPU manda (`sh_busy`), pero el Z80 **no debe escribir en la pantalla
  mientras corre**: para eso está el bit de ocupado.

### Bit de ocupado y registros: por el puerto del depurador `$3FEF`

Decidido con el usuario (2026-10-25): se usa el puerto `$3FEF` (`sim_int.v`) en vez de un puerto nuevo. Ya está decodificado
con los 16 bits, ya está enlazado al bus de datos (`m1t_rd` en `data_dir` y en los 8 multiplexores de `D7..D0`), así
que el bit de ocupado no cuesta lógica de bus; solo un valor más en el selector.

Lo que **no** hay libre hoy, mirado en `sim_int.v`:

- IN: `ridx` es de 4 bits y los 16 índices están ocupados (0-14 y el 15 = firma `$52`).
- OUT de datos: `wreg` es de 3 bits (`$80+r`, los bits 6-3 se ignoran) y los 8 registros (0-7) están ocupados.
- Lo que sí está libre es el espacio de códigos del OUT: `data[7]=0` y distinto de `$00-$10` (`$11-$7F`). No rompe nada
  que ya use el puerto.

Extensión propuesta (unas pocas LUTs: un bit más en `ridx`, otro selector de registro):

| OUT | Efecto |
|---|---|
| `$20+n` (n 0-15) | IN devuelve el índice 16+n |
| `$40+r` (r 0-15) | el OUT siguiente es el dato del registro 8+r |

Índices IN nuevos: 16 = estado del scroll (bit 7 `busy` = pendiente o en marcha, bit 6 orden pendiente, bit 5 en
marcha; el resto, a 0), 17 = fino Y, 18 = fila superior, 19 = fila inferior (para que los snapshots del depurador
puedan leer lo que los registros de solo escritura no dicen, como ya hacen los índices 13 y 14).

Registros de escritura nuevos: 8 = fino Y, 9 = orden (1 izquierda, 2 derecha, 3 arriba, 4 abajo; escribirla deja la
orden **pendiente**), 10 = fila superior de la región, 11 = fila inferior. El fino X sigue en `POKE 2090` (y la máscara
de filas en 2091-2093); abierto si se quiere también un alias como registro 12.

Uso desde el Z80: `LD BC,$3FEF`, `OUT (C),$20` (elige el estado), y un bucle `IN A,(C)` + `RLA` hasta que `busy = 0`.

**Cuidado:** `ridx` es único para todo el puerto. Si entra el monitor del depurador entre el `OUT` del selector y el `IN`,
el índice cambia: el programa debe volver a elegir el índice (`OUT $20`) antes de cada sondeo, no una sola vez.
Con el depurador armado el puerto lo comparte con el monitor; sin él, solo el programa lo toca. El conflicto con ZonX
que ya se anotó para `$3FEF` ([[project_dbg_port_3fef]]) también afecta a esto.

## Consecuencias que hay que asumir

- Tras el primer movimiento, la **RAM externa del Z80 ya no coincide con la sombra** (lo que se ve). La pantalla pasa a
  ser de solo escritura para el Z80: nada de leer píxeles ni de XOR sobre lo desplazado. Encaja con "regenerar lo que
  entra". Los sprites de hardware no se ven afectados (van aparte, en coordenadas de pantalla).
- El Z80 lleva la cuenta del desplazamiento grueso (en qué columna del mundo está).
- Hay que replicarlo en el emulador (EightyOne) y documentarlo en el manual (`manual_pendiente.md`).

## Costes estimados (sin sintetizar)

Fino Y + dirección de ranura 32/fila 25: ~50-80 LUTs. Motor (contadores, FSM, captura del byte que sale, máscara):
~150-220 LUTs. Total ~200-300 LUTs más el bit de ocupado (~15), sin BRAM nueva. Con la síntesis actual (64 sprites, K = 12: 53 % de LUTs
y 73 % de slices) queda margen; medir de todos modos tras cada fase.

## Orden de trabajo

1. Rama `fpga-scroll`: fino X con columna 33 real + fino Y + fila 25 (registros 2063+), solo Spectrum. Prueba: el Z80
   mueve los bytes a mano (referencia del resultado esperado).
2. Motor horizontal (der/izq) con captura del byte que sale, y el bit de ocupado. Simulación en ModelSim local
   (a partir de los `tb_*`).
3. Motor vertical + región.
4. Emulador (EightyOne), manual (`manual_pendiente.md`), ejemplo en `EXAMPLES/`.

## Abierto

- ¿Alias del fino X como registro 12 del puerto, o se queda solo en `POKE 2090`?
- Si el motor, además de la orden, debe poder lanzarse sin esperar al blanking (por defecto: siempre en el blanking).

## Fase 1: hecho (rama `fpga-scroll`, sin commitear)

- `spec_scroll_addr.v` (nuevo): bitmap y atributo del Spectrum con fino Y, región, columna 32 y fila de abajo.
  `tb_spec_scroll_addr.v` lo prueba entero contra un modelo aritmético (1.165.824 casos, y detecta una mutación a propósito).
- `SD81.v`: `scr_col1x` (6 bits), la instancia y las dos direcciones del Spectrum. Con fino Y = 0 y columna < 32 son
  idénticas a las de antes.
- `sim_int.v` 0.19 (puerto `$3FEF`): `ridx` de 5 bits (`$20+n` -> índice 16+n); `$40+r` -> registro 8+r. Registro 8 = fino Y
  (OUT `$40`, dato), 10 = fila superior (`$42`), 11 = fila inferior (`$43`); IN 17/18/19 los leen y el 16 (estado) da 0 de
  momento. `tb_dbg.v` prueba todo eso y que lo de siempre sigue igual.
- Entrada en `SD81V2.1000.xise` y `SD81.prj` (el `.xise` está marcado `assume-unchanged` en git: no sale en `git status`).
- Sintetizado (25/10): 3.180 LUTs (55 %), 1.124 slices (78 %), temporización cumplida (referencia anterior: 3.082 / 1.045,
  con cambios de `sim_int` entre medias). Programa de prueba: `EXAMPLES/SCROLLT` (README con lo que se tiene que ver) y
  `claude/mqtttest/build_scrollt.sh` (lo ejecuta en el Z80 de EightyOne con todo simulado: 35 comprobaciones).
- Pendiente: probar en la placa con SCROLLT, y el emulador. Sin simulación del vídeo completo: la temporización real de la columna 32 solo se confirma en la placa.

## HiRes nativo (POKE 2045,171): scroll — fase 1b (2026-10-26, sin sintetizar)

Pedido del usuario: el scroll tambien en el HiRes nativo. Comprobado en `SD81.v` lo que ya existe en Superfast:

- Bitmap lineal `{fila, linea, columna}` (= `y*32 + col`) en el bloque `vpage`, 6 KB; **sobran 2 KB** (`$1800-$1FFF`).
- Chroma modo 0 (`color_mode = 0`, `attr_addr_m0`): un atributo por byte de linea (8x1), area paralela de 6 KB en el
  bloque 6 (`{3'b110, off}`), leida en directo (sin doble buffer). Sobran otros 2 KB (`$D800-$DFFF`).
- Chroma modo 1 (`color_mode = 1`, `attr_addr_m1`): un atributo por celda 8x8 y por posicion, relativo al DFILE
  (o a la base de POKE 2059-2061), paso 33 por fila. No mira `sfHR_en`: vale tambien en HiRes.
- Antes de esto, en HiRes la columna 33 daba la vuelta (`scr_col1`/`scr_col2` de 5 bits).

Hecho: `lin_scroll_addr.v` (gemelo de `spec_scroll_addr.v`) con la misma ventana, fino Y y region:

| Salida | Uso |
|---|---|
| `off` (13 bits) | bitmap `{vpage, off}` **y** atributo de modo 0 `{3'b110, off}` (misma direccion por byte de linea) |
| `src_row`, `below` | atributo de modo 1: fila de origen (24 = fila de entrada de abajo) |

Bufers de entrada (en las dos areas, bitmap y atributos de modo 0): derecha `$1800-$18BF`, abajo `$1900-$19DF` (7 lineas de
32), esquina `$1A00-$1A06`; reservados para el motor: izquierda `$1C00-$1CFF`, arriba `$1D00-$1DFF`. Aqui sobra sitio, no
hace falta el reparto apretado del Spectrum.

Atributo de modo 1: la columna 32 es el hueco del NEWLINE (paso 33), asi que el bufer de la derecha ya existe; el de abajo
es una fila 25 fija (fila 24 de la rejilla, `DFILE_eff + 1 + 24*33 + col`). El software la rellena como la fila 24 de
una pantalla de 25 filas.

Cambios en `SD81.v`: instancia `lin_scroll_addr_inst`, `scan_addr` y `attr_addr_m0` usan `hr_off` (ya no `hr_addr`),
`attr_rel_addr` usa `hr_src_row` y `scr_col1x` solo con `sfHR_en`; el resto de modos queda igual. Con fino Y = 0 y
columna < 32 son las direcciones de siempre (probado en el testbench).

Probado: `tb_lin_scroll_addr.v`, 1.165.824 casos, 0 errores, y detecta una mutacion a proposito
(`vlog lin_scroll_addr.v tb_lin_scroll_addr.v`). Sin sintetizar y sin probar en la placa.

Observaciones:
- Los atributos de HiRes (modo 0 y 1) **no pasan por el doble buffer**: con dbuf el bitmap cambia de pagina y el color no.
  Igual que antes de esto; hay que tenerlo en cuenta al mover memoria (el motor tendra que mover tambien el atributo).
- Pendiente de comprobar en el texto Superfast con fino X y Chroma modo 1: `scr_col2` es de 5 bits, asi que el atributo de la
  columna 33 parece dar la vuelta a la 0 (el caracter no, que usa `sf_col_idx` de 6 bits). Por el codigo; no probado.
- Motor de movimiento (fases 2-3): en HiRes mueve 6 KB de bitmap mas 6 KB de atributos de modo 0 (o 768 de modo 1).
- Prueba en la placa: falta un programa tipo SCROLLT para HiRes (bitmap lineal + atributos 8x1 y 8x8).

## Pendientes de comprobar en la placa (apuntado 2026-10-26; NO se tocan antes de la prueba)

Decidido con el usuario: se dejan tal cual y se prueban sobre la placa real cuando se pueda.

1. **Texto Superfast con fino X y Chroma modo 1:** `scr_col2` es de 5 bits; el atributo de la columna 33 parece dar la vuelta a la 0
   (el carácter sí usa `sf_col_idx`, de 6 bits). Por el código, no probado.
2. **Spectrum con `color_mode = 1`:** la FPGA lee el atributo por posición del DFILE y no el del Spectrum; solo es coherente con Chroma modo 0.
3. Los atributos de HiRes (modo 0 y modo 1) no pasan por el doble buffer (solo el bitmap).
4. Fase 1 (SCROLLT) y fase 1b (SCROLLH) sin probar en la placa: la temporización real de la columna 32 (y de la fila de abajo en HiRes) solo se confirma allí.

## Fino y grueso (aclaración, 2026-10-26)

- **Fino** (implementado, fases 1 y 1b): no mueve memoria. Solo cambia desde dónde lee el vídeo; el atributo se lee de la línea / fila / columna de origen,
  así que viaja con su bitmap. En vertical, el atributo sigue a su fila de origen (`src_row` en HiRes modo 1, `ys[7:3]` en Spectrum, la misma dirección que el bitmap en HiRes modo 0).
- **Grueso** (motor de movimiento, fases 2 y 3, **sin escribir**): mueve bitmap **y atributos** en memoria (HiRes modo 0: 6 KB más; modo 1: la rejilla 33 x 24; Spectrum: 24 bytes por fila).
- Prueba en la placa de HiRes: `EXAMPLES/SCROLLH` (tecla M para alternar los atributos de modo 0 y de modo 1).

## Fase 2: motor de movimiento HORIZONTAL (2026-10-26, rama `fpga-scroll`, sin sintetizar ni probar en la placa)

Pedido del usuario: "vamos con el motor". El scroll **grueso** es una acción separada del Z80: cuando el fino ha llegado a 7 (o a 0 al ir a la
derecha), el Z80 manda una orden por el puerto `$3FEF` y la FPGA mueve la memoria un byte.

### Qué hace

- Orden: `OUT $41` y el dato en el puerto `$3FEF` (registro 9): **1 = izquierda, 2 = derecha**; 3 y 4 (arriba / abajo) quedan reservadas para la fase 3 y se
  aceptan sin hacer nada. La orden 0 no hace nada. Una orden dada con el motor **ocupado se ignora**.
- Estado: `OUT $20` y `IN` (índice 16): bit 7 **ocupado** (pendiente o en marcha), bit 6 pendiente, bit 5 en marcha. Sube en cuanto se escribe la orden.
- Cuándo: el motor arranca en el flanco de la **línea 254** (como el blit) si hay una orden pendiente; si el doble buffer AUTO está activo, el blit **espera** a que
  acabe el motor. Una orden dada después de la línea 254 espera al cuadro siguiente.
- Qué mueve: el bloque que escribe el Z80 (**HFILE**, no el de delante del doble buffer), un byte por línea (o por fila de atributos), respetando la **máscara
  de filas** (`POKE 2091-2093`). Sin efecto en el texto (ni Spectrum ni HiRes) ni sin Superfast.

  | Modo | Qué se mueve |
  |---|---|
  | Spectrum | bitmap (192 líneas) y los 24 x 32 atributos del `$1800` |
  | HiRes nativo | bitmap (192 líneas) y, **si el color está activo** (`$7FEF` bit 5): modo 0 (8x1) los 6 KB paralelos de `$C000`; modo 1 (8x8) la rejilla de 33 bytes por fila desde la base de atributos |

  Con el color apagado en HiRes no se mueven atributos: `$C000+` puede ser memoria del programa.
- Cadena de nodos por línea: `L m0 ... m31 R`. **Izquierda:** cada nodo toma el valor del de su derecha (`m31 <- R`) y `m0` sale a `L`. **Derecha:** al revés
  (`m0 <- L`) y `m31` sale a `R`. El byte que sale se guarda en el búfer opuesto: así **ir y volver no obliga a regenerar**. Lo que entra lo tiene que haber
  escrito el Z80 (y rellena después el búfer del lado de entrada). En HiRes modo 1 la columna 32 de la rejilla (el hueco del NEWLINE) hace de `R`; **no hay `L`**: al ir a la derecha `m0` se queda como estaba y el Z80
  tiene que escribir la columna 0 de la rejilla.
- Coste: 3 ciclos por byte (dirección, dato, escritura; la CPU manda con `sh_busy`). Spectrum completo: 21.435 ciclos = 0,82 ms a 26 MHz; HiRes con atributos de modo 0, unos 1,5 ms.
  Cabe de sobra en la ventana de borrado (7,6 ms) con el blit (0,63 ms).
- **El Z80 no debe escribir en la pantalla mientras corre** (bit de ocupado). Después del movimiento la RAM externa del Z80 ya no coincide con la sombra.

### Protocolo desde el Z80 (sin saltos)

El fino y la memoria tienen que cambiar a la vez. El motor corre al final del cuadro n, así que: **mandar la orden en el cuadro en que el fino llega a 7 (izquierda) o a 0
(derecha), dejar el fino como está, y en el cuadro siguiente, con el estado libre, poner el fino a 0 (izquierda) o a 7 (derecha)** y rellenar el búfer de entrada.
Es lo que hace `EXAMPLES/SCROLLM`.

### Piezas (FPGA/SD81V2.1000)

| Fichero | Qué |
|---|---|
| `scroll_motor.v` | FSM del motor (nuevo); direcciones por concatenación (Spectrum, HiRes bitmap y modo 0) o con un sumador de 16 bits (modo 1) |
| `scroll_sync.v` | cruce de dominios de la orden y del estado (tres "toggles"), y el arranque en la línea 254 (nuevo) |
| `sim_int.v` 0.20 | registro 9 (orden) e índice 16 (estado) del puerto `$3FEF` |
| `SD81.v` | arbitraje de los dos puertos de la BRAM (el motor lee por B y escribe por A), el blit espera al motor, el motor de sprites no lee mientras corre, la instancia |
| `tb_scroll_motor.v` | 69 casos contra un modelo, memoria completa: Spectrum, HiRes bitmap, modo 0 y modo 1; izquierda y derecha; máscaras; bloques 4, 0 y 7; bases de la rejilla (con vuelta de 16 bits); paradas de la CPU; órdenes sin efecto; segunda orden con el motor en marcha |
| `tb_dbg.v` | la cadena entera: orden por el puerto, ocupado al instante, orden ignorada, arranque en el flanco, memoria movida |

Sin simular `SD81.v` entero (ModelSim no lo compila: referencias hacia delante que ISE acepta). Probado en el PC con `EXAMPLES/SCROLLM` y `claude/mqtttest/build_scrollm.sh`
(modelo en software del motor, 1.124 cuadros comprobados).

### Lo que NO está

- Vertical (fase 3): movimiento por filas de la región con el búfer de arriba y de abajo. Y que **R y L se muevan en vertical** (hoy el Z80 los rellena tras un movimiento vertical).
- El emulador (ver `scroll_hardware_emulador.md`) y el manual.
- Síntesis y prueba en la placa: sin ver todavía los recursos del motor (estimación 100-200 LUTs; los slices estaban al 78 %).

## Pruebas del scroll (2026-10-26)

| Dónde | Prueba | Qué comprueba |
|---|---|---|
| ModelSim | `tb_spec_scroll_addr.v` | direcciones del Spectrum: fino Y, región, columna 32 y fila de abajo (1.165.824 casos) |
| ModelSim | `tb_lin_scroll_addr.v` | lo mismo en HiRes nativo |
| ModelSim | `tb_scroll_motor.v` | el motor aislado: 69 casos contra un modelo, memoria completa de 64 KB |
| ModelSim | `tb_dbg.v` | el puerto `$3FEF` (registros 8-11, orden en el 9, estado en el IN 16) con `sim_int`, `scroll_sync` y el motor encadenados |
| Z80 en el PC | `build_scrollt.sh`, `build_scrollh.sh` | los programas de fase 1 y 1b (35 comprobaciones el primero) |
| Z80 en el PC | `build_scrollm.sh` | SCROLLM entero con un modelo del motor: HiRes 0 y 1, Spectrum, HUD, doble buffer, ruido y sprites (2.133 cuadros) |
| Placa | `EXAMPLES/SCROLLT`, `SCROLLH`, `SCROLLM` | **sin ejecutar todavía** |

Lo que solo se ve en la placa: el arbitraje de los puertos de la BRAM entre el motor, la CPU, el blit y los sprites en `SD81.v` (no se puede simular entero con ModelSim), la
temporización real de la columna 32 y de la orden en la línea 254, y los recursos de la síntesis. SCROLLM ejerce todo eso con las teclas H, D, S, N y T.
