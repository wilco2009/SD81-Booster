# Investigación: modo Superfast 80 columnas + modo bitmap 256x192x16 colores

Rama: `feature/superfast-80col`. Solo planificación por ahora, nada
implementado. Objetivo declarado de la rama: investigar mejoras al modo
de vídeo Superfast, partiendo de que el interface sigue saliendo por
RGB SCART (no se cambia el tipo de conector/monitor) pero asumiendo TV
LCD moderna con entrada PAL RGB (no hay que preocuparse de compatibilidad
con tubos CRT de época).

## 1. Modo Superfast texto a 80 columnas

### Restricción de fondo

La salida es RGB analógico de 1 bit + sync compuesto
([SD81.v:1261-1266](../../FPGA/SD81V2.1000/SD81.v#L1261)), pensada para
un monitor/TV PAL por SCART. `pixel_clk=6.5MHz`, 414 ciclos/línea ≈
63,7 µs (línea PAL estándar, 15.625 kHz horizontal).

### A 6,5 MHz (reloj actual, sin tocar)

Reparto actual de los 414 ciclos/línea:

| Tramo | Ciclos | Qué es |
|---|---|---|
| Sync | 32 | `pixel_cnt` 32-63 |
| Back porch | 34 | `pixel_cnt` 64-97 |
| Margen izquierdo | 24 | borde, antes de `SCR_START_X`=122 |
| Activo (texto) | 264 | 33×8, incluye la columna extra del scroll fino |
| Margen derecho + front porch | 60 | hasta el sync de la línea siguiente |

Presupuesto reutilizable sin tocar sync/back porch (esos arriesgan perder
el enganche del monitor): 24+60 = **84 ciclos**.

- Techo teórico (márgenes a 0, no fabricable de verdad): activo = 264+84
  = 348 ciclos.
  - Fuente 8 px: 348/8 = **43 columnas**.
  - Fuente 5 px: 348/5 = **69 columnas**.
- **Ninguna combinación llega a 80 a 6,5 MHz.** Techo realista de verdad
  (con algo de margen de seguridad, sin probar en monitor real): más
  cerca de 65-69 columnas con fuente de 5 px.

### A 13 MHz (doblando el reloj de píxel)

**El reloj ya existe en el diseño, no hace falta generarlo.** Cadena
actual: 3,25 MHz → DCM ×4 → 13 MHz intermedio → segundo DCM → 26 MHz
(`system_clk`) → `cnt26` (contador libre a 26 MHz) →
`clk6_5 = cnt26[1]` ([SD81.v:352](../../FPGA/SD81V2.1000/SD81.v#L352)).
Cambiar a `cnt26[0]` da 13 MHz, ya en fase con todo lo demás.

Manteniendo la duración real de línea (~63,7 µs, para seguir siendo PAL
válido) hay que doblar también sync y back porch en ciclos:

| Tramo | Ciclos a 13 MHz |
|---|---|
| Sync | 64 |
| Back porch | 68 |
| Presupuesto reutilizable (márgenes) | 168 |
| Activo sin tocar márgenes | 528 |

- Sin tocar márgenes, fuente 8 px: 528/8 = **66 columnas**.
- Sin tocar márgenes, fuente 5 px: 528/5 = **105 columnas** — de sobra
  para 80 sin recortar ni un ciclo de borde.
- Para 80 con fuente 8 px: hacen falta 640 ciclos activos → hay que
  recuperar 112 de los 168 ciclos reutilizables (~67% del margen
  disponible) — recorte notable pero lejos del extremo de margen 0.

**Conclusión: 80 columnas es alcanzable a 13 MHz**, con la fuente 8×8
actual (recortando márgenes de forma importante) o sobrando margen con
fuente 5 px. Dado por bueno que la TV LCD moderna con entrada PAL RGB no
va a tener problema de ancho de banda a 13 MHz (decisión del usuario:
"asumimos que la TV no es un problema").

### Aviso de alcance (importante para cuando se implemente)

El cambio de reloj es una línea, pero **todo el generador de
temporización de vídeo está construido sobre `pixel_cnt` como contador
absoluto**: `SCR_START_X`, `SCR_END_X`, los umbrales de `HSYNCcnt`, el
patrón de borde, las fórmulas de posición de sprites (`SPR_X_FUDGE` y
compañía), etc. Doblar el reloj sin retocar todas esas constantes a la
vez simplemente dobla la duración de línea real (dejaría de ser PAL).
Es un trabajo bastante más invasivo que el del scroll horizontal fino
— tocar con cuidado y por pasos, no de una vez.

### Ancho de banda de memoria a 13 MHz

El número de lecturas por carácter no cambia (siguen siendo 3: carácter,
atributo, bitmap), pero al doblar `pixel_clk` sin tocar `system_clk`
(26 MHz) la relación entre ambos relojes pasa de 4:1 a 2:1 — cada uno de
los 8 estados de `col_cnt_b` pasa de 4 ciclos de `system_clk` a solo 2.
Pendiente comprobar si la latencia real de lectura de la BRAM de sombra
cabe en 2 ciclos o va justa.

Si hiciera falta más margen, la solución es **doblar también
`system_clk`** (26→52 MHz) para dejar el mismo margen 4:1 de hoy — un
ajuste del multiplicador del DCM que ya existe en cascada, no lógica
nueva. Importante: `system_clk`/`pixel_clk` no salen de un oscilador
suelto, salen de un DCM cuya entrada es literalmente `nCLOCK`
([SD81.v:329](../../FPGA/SD81V2.1000/SD81.v#L329), `.CLKIN(nCLOCK)`) —
todo el pipeline de vídeo ya está enganchado en fase al reloj real de la
CPU, así que subir el multiplicador no introduce ningún cruce de
dominio de reloj.

Hay un reloj físico de 100 MHz en un pin (`CLK100`) con algo de lógica ya
escrita alrededor (`CLK104`, un contador de pulsos de `nCLOCK`,
`newCLK6_5`/`newCLK3_25`) — **descartado como opción**: esas señales no
se usan en ningún sitio hoy (código muerto de un enfoque abandonado), y
al no estar sincronizado con `nCLOCK` cualquier uso suyo exigiría
sincronizadores de cruce de dominio, justo la complicación que evita
seguir con la cadena de DCMs ya enganchada a `nCLOCK`.

### Por qué solo tiene sentido en Superfast, no en modo nativo

La distinción correcta no es "nativo vs Superfast", sino **si el
software usa las rutinas de impresión de la ROM (`PRINT`/`INPUT`/editor)
o pincha el DFILE directamente**:

| | Rutinas de la ROM | Software propio (poke directo a DFILE) |
|---|---|---|
| **Nativo** | Limitado a 32 (`$21`=33 grabado en la ROM) | Sigue sin poder — el hardware de vídeo nativo genera la señal ejecutando los bytes del DFILE como opcodes de la CPU (el truco del HALT), y esto exige la temporización de vídeo completa (sync, bordes...) que ya se descartó rediseñar |
| **Superfast** | Limitado a 32 igual (misma ROM) | **80 columnas reales, viable** — la FPGA direcciona el DFILE por aritmética de fila/columna, no ejecuta nada como opcode, así que ensanchar esa aritmética no choca con la ROM ni con la CPU |

O sea: 80 columnas de verdad solo sirven para software con su propia
rutina de impresión, y solo en Superfast texto.

### Posibilidad: parchear la ROM para que `PRINT`/`INPUT` also usen 80

Investigado directamente en `z80rom/sdmodrom.asm` (el listado completo
desensamblado). El ancho de fila NO se calcula por multiplicación en
ningún sitio relevante — `LOC-ADDR` (rutina que encuentra la dirección
de una fila/columna) busca hacia atrás en el DFILE contando bytes NEWLINE
(`0x76`) uno a uno, así que es agnóstico al ancho de fila mientras los
NEWLINE estén bien colocados. Esto abarata mucho la idea frente a lo que
parecía al principio.

**Constantes con dependencia real del ancho — sustitución directa,
sin trucos de bits:**

| Constante | Significado | Rutina | Nuevo valor (80 col) |
|---|---|---|---|
| `$21` (33) | ancho de fila con NEWLINE | `ENTER-CH`, `LINE-ENDS`/`CLS`, `SCROLL` | `$51` (81) |
| `$22` (34) | ancho de fila + 1 | `LOC-ADDR` | `$52` (82) |
| `$1F` (31) | columna válida máxima (0-31) | `PRINT-AT` (validación de argumento) | `$4F` (79) |

**Dependencia real que SÍ exige código nuevo, no solo un valor
distinto:**

- **`TAB`**: usa `AND $1F` para hacer módulo 32 vía máscara de bits
  (aprovecha que 32 es potencia de 2). 80 no lo es — habría que
  reescribirlo como resta/comparación en bucle.

**Coincidencias de la ROM — mismo byte, otro significado, NO tocar**
(comprobado con cuidado para no confundirlas con dependencias reales):

- `SHIFT-FP` y la rutina de multiplicación en coma flotante: `$21`=33
  ahí es "bits de la mantisa", nada que ver con la pantalla.
- `CLEAR-PRB` (buffer de la ZX Printer): `$20`=32 son las columnas
  **fijas del hardware de la impresora**, no de la pantalla.
- Varias entradas `DEFB` en tablas de funciones matemáticas (`atn`) y en
  la tabla de caracteres — el valor 33 es dato arbitrario de la tabla.

**Conclusión**: parche pequeño y acotado — 3 constantes en 4 rutinas,
más una reescritura contenida de `TAB`. Mucho más manejable de lo que
parecía, siempre que se respete la distinción entre dependencia real y
coincidencia de byte.

### Inventario verificado contra el binario (2026-09-07)

Al ir a implementarlo se barrieron TODOS los operandos inmediatos con
valor 31/32/33/34 en `$0800-$0C60` (el rango de rutinas de pantalla),
descartando los precedidos de prefijo `FD`/`DD` — porque `(IY+$22)` es
el *offset* de la variable DF_SZ, no la constante 34, y ese falso
positivo aparece por todas partes. Resultado: **8 sitios, no 3**. La
tabla de arriba se quedaba corta: `ENTER-CH` usa la constante en tres
puntos y `WRITE-N/L` no estaba en la lista.

| Dirección | Valor | Rutina | 80 col | 70 col |
|---|---|---|---|---|
| `$080F` | 33 | `ENTER-CH` (`CP $21`) | 81 | 71 |
| `$0826` | 33 | `ENTER-CH` / TEST-N/L (`LD C,$21`) | 81 | 71 |
| `$0848` | 33 | `WRITE-N/L` (`LD C,$21`) | 81 | 71 |
| `$0903` | 31 | `TEST-VAL` / PRINT-AT (`LD A,$1F`) | 79 | 69 |
| `$0921` | 34 | `LOC-ADDR` (`LD A,$22`) | 82 | 72 |
| `$0A31` | 33 | `B-LINES` / CLS (`LD C,$21`) | 81 | 71 |
| `$0B22` | 33 | `TAB-TEST` (`CP $21`) | 81 | 71 |
| `$0C12` | 33 | `SCROLL` (`LD C,$21`) | 81 | 71 |

Aparte, `AND $1F` en **`$0B0C`** (TAB) sigue necesitando reescritura, no
cambio de valor.

Confirmadas también las coincidencias que NO se tocan: `$08E8` (32) es
`CLEAR-PRB`, las columnas fijas de la ZX Printer; `$171D` y `$17E4` son
coma flotante; y `$1AC8` ni siquiera es una instrucción, es la cola de un
`CALL $0EA7` seguido de `LD HL,$1520`.

### Cómo aplicarlo sin tocar la ROM en disco

Idea del usuario, y funciona: la "ROM" del bloque 0 es RAM. La protección
de escritura es **por dirección, no por página** (`assign nWRx = ... |
((~A13&~A14&~A15) & ~block0Writable)`), así que basta con mapear esa
página en un bloque alto y escribirla desde ahí:

```asm
OUT ($E7),A               ; A = (pagina << 3) | bloque   (half paging)
LD  BC,$00E7 : IN A,(C)   ; leer que pagina tiene el bloque N (A10:A8 = N)
```

Ventaja sobre `POKE 2056`: ese desprotege el bloque 0 pero pone
`block0Writable`, y **todos los POKE-trick exigen `!block0Writable`** —
con él activo no se podría ni seleccionar el modo de vídeo.

El modo FAST no necesita parche: como el comando se ejecuta después del
arranque, basta con repetir lo que hace el comando `FAST` del BASIC
(`CALL $02E7` + `RES 6,(IY+$3B)`).

Plan: tres comandos, `LOAD *80COL`, `LOAD *70COL` y `LOAD *32COL`. El
último restaura los valores estándar (33/34/31), que son conocidos, así
que no hace falta guardar los originales en ningún buffer.

## 2. Modo bitmap 256×192, 16 colores (2 píxeles/byte)

### Motivación y problema de partida

Un framebuffer así ocupa 256×192/2 = 24.576 bytes = 3 páginas de 8 KB.
La CPU solo ve 8 KB a la vez (el bloque que tenga paginado), pero el
hardware de vídeo necesita las 3 páginas a la vez durante todo el
refresco. Con 512 KB de RAM disponibles, la limitación no es de espacio
sino de cómo dar acceso simultáneo a las 3 páginas al vídeo sin que la
CPU necesite verlas todas a la vez.

### Restricción de recursos (comprobada, no estimada)

BRAM del XC6SLX9 al **100% usada hoy**: 32/32 `RAMB16BWER`
(`FPGA/SD81V2.1000/SD81_summary.html`). Esos 32 son exactamente los 8
bloques de sombra de 8 KB que usa Superfast (4 BRAM × 8 KB × 8 bloques).
**No hay BRAM libre** para añadir una sombra nueva de 24 KB de la forma
obvia.

### Por qué no basta con paginar y usar la sombra normal

La sombra de cada bloque de CPU refleja "lo último que la CPU escribió
en esa dirección", **sin importar qué página física estaba paginada en
ese momento**. Si se pagina la página A, se escribe el tercio superior,
y luego se pagina B en el MISMO bloque de CPU para escribir el tercio
medio, la sombra de ese bloque pasa a reflejar B — el contenido de A ya
escrito se pierde de la sombra (sigue intacto en la RAM física, pero el
vídeo ya no lo ve). Con un único bloque de CPU visible, la sombra normal
no puede sostener 3 páginas independientes a la vez.

### Diseño acordado

**Decisiones ya tomadas por el usuario:**
1. Las 3 páginas físicas del framebuffer son **fijas** (constantes en el
   Verilog, como los bloques 4-5 "seguros" del doble buffer) — nada
   configurable por POKE.
2. Un único POKE dispara el blit de las 3 páginas de golpe (no hay
   refresco automático por frame ni refresco parcial por tercio).

**Memoria:**
- 3 páginas físicas de 8 KB consecutivas, fijas.
- Cada byte = 2 píxeles de 4 bits. Cada nibble = `{brillo,verde,rojo,azul}`
  — mismo convenio que ya usan sprites y Chroma, sin paleta nueva.
- Pendiente decidir: qué nibble (alto/bajo) es el píxel par y cuál el
  impar.

**Cómo escribe la CPU:** pagina la página física que quiera (una de las
3 fijas) en cualquier bloque del mapper existente (puerto E7h, el de
siempre). No hace falta ningún mecanismo nuevo de paginación.

**Cómo lo ve el vídeo:** se reutilizan 3 de los 8 bloques de sombra que
ya existen (mismas BRAM, cero recursos nuevos) como las 3 franjas del
framebuffer. Su puerto de escritura pasa a tener dos fuentes conmutadas
por si este modo está activo:
- Modo apagado: como hoy, espejo de lo que la CPU escribe en ese bloque
  de CPU.
- Modo encendido: reciben lo que blitea el FSM desde su página física
  fija correspondiente — igual que el doble buffer, pero de 3 páginas en
  vez de 1.

**El POKE:** activa el modo de vídeo y dispara un blit único de las 3
páginas (24 KB) de golpe. Mismo patrón que el modo MANUAL del doble
buffer (`POKE 2057,200+B`), extendido a 3 páginas.

- Tiempo de blit: el actual copia 8 KB en ~630 µs dentro de una ventana
  de ~7,6 ms (uso real ~8%). Triplicar a 24 KB serían ~1,9 ms — sigue
  dejando de sobra (~25% de la ventana). No es cuello de botella.

### Cambios de hardware necesarios (lista, sin diseñar el detalle todavía)

1. FSM de blit: generalizar el actual (1 página → 1 sombra) a un bucle
   de 3 iteraciones, cada una apuntando a su página física fija.
2. Mux en el puerto de escritura de los 3 bloques de sombra reutilizados,
   gobernado por si este modo está activo.
3. Generador de direcciones de vídeo nuevo: no es texto ni atributos, es
   un recorrido lineal de 128 bytes/fila a través de las 3 sombras
   concatenadas — una rama nueva, no una variante de `col_cnt_b`.
4. Descodificador byte→2 píxeles a la cadencia de píxel (6,5 MHz, o
   13 MHz si algún día convive con el modo 80 columnas — son features
   independientes, pero conviene tenerlo en la cabeza al diseñar el
   datapath).

### Preguntas abiertas (no bloquean el plan, pero hay que resolverlas al implementar)

- Empaquetado exacto del nibble alto/bajo.
- Si el registro de activación reutiliza el esquema de `POKE 2045` (un
  submodo Superfast más) o es un puerto aparte.
- En qué bloque de CPU se espera que el usuario tenga paginada la franja
  que está editando (¿libre, o uno recomendado, como los "4-5 seguros"
  del doble buffer?).

## 3. Color por píxel dentro del carácter (16 colores/píxel, 64 bytes/carácter)

Variante de Chroma modo 0 descartada de momento por complejidad, anotada
por si se retoma. Idea: en vez de 8 bytes de color por carácter (1 por
fila, como ya hacen sprites y el propio modo 0 hoy vía `line_cnt_b` en
`attr_addr_m0`), 64 bytes por carácter (1 por píxel). Sigue indexado por
**código de carácter** (no por posición de pantalla), igual que el modo
0 actual.

- Memoria: 128 caracteres × 64 = 8 KB exactos (1 página fija, mismo
  truco de la página tipo blit); 256 caracteres × 64 = 16 KB (2 páginas).
- El problema no es el tamaño, es que hoy el color se lee **una vez por
  fila** y se aplica a los 8 píxeles; esto exige **8 lecturas por fila**
  en vez de 1. Solución con precedente ya en el propio fichero: un
  `color_shift_buffer` gemelo de `shift_register` (que ya carga 8 bits de
  golpe una vez por fila y los saca de 1 en 1 por `pixel_clk`) — leer los
  8 bytes de color en ráfaga contra la BRAM de sombra (a `system_clk`,
  32 ciclos disponibles por carácter de 8 píxeles a 6,5 MHz) y sacarlos
  igual, 1 por ciclo de píxel.
- Viable en cuanto a ancho de banda, pero es una pieza de RTL nueva de
  verdad (buffer + máquina de ráfaga), más grande que nada de lo tocado
  hasta ahora en vídeo. Aparcado detrás de las 80 columnas por esto.

## Estado

Solo planificación, nada implementado todavía en ninguna de las tres
ideas. Rama `feature/superfast-80col` creada (obsoleta, hay que
recrearla desde `master` porque ha habido commits desde entonces) y sin
commits propios de esta investigación.
