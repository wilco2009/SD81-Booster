# FPGA: informe de recursos y alternativas para liberarlos

Fecha: 2026-10-05. Diseño analizado: la FPGA de la release v1.6.0 (`sim_int` 0.13,
`FPGAVERSION.TXT` 1.5), sintetizada el 4 de octubre con ISE 14.7 (XST + MAP + PAR)
para el XC6SLX9-TQG144-3.

Fuentes: `FPGA/SD81V2.1000/SD81_map.mrp` (MAP) y `SD81.syr` (XST) del build de la
release, más dos síntesis hechas en una copia de trabajo de la VM de ISE (el
proyecto no se tocó): una con `-keep_hierarchy Yes`, para repartir por módulo, y
otra con `-opt_mode Area`, para medir esa alternativa.

## 1. Informe de recursos (MAP, el resultado final)

| Recurso | Usado | Disponible | % | Comentario |
|---|---:|---:|---:|---|
| **RAMB16 (BRAM)** | 32 | 32 | **100 %** | Todo lo ocupa la RAM de sombra (64 K × 8) |
| **IOB** | 98 | 102 | **96 %** | Los pines del Z80, la SRAM, el MCU, el vídeo... no hay margen |
| **Slices ocupados** | 1.336 | 1.430 | **93 %** | El límite real de la colocación |
| Slice LUTs | 4.397 | 5.720 | 76 % | 3.566 como lógica, 797 como memoria distribuida, 34 de paso |
| LUTs usadas como memoria | 797 | 1.440 | 55 % | La memoria distribuida solo cabe en los slices tipo M |
| Slice registers | 2.103 | 11.440 | 18 % | Sobran: no es el problema |
| RAMB8 | 0 | 64 | 0 % | Inaccesibles mientras las RAMB16 estén todas ocupadas |
| DSP48A1 | 0 | 16 | 0 % | Sin usar |
| BUFG / DCM / PLL | 6 / 2 / 0 | 16 / 4 / 2 | 37 / 50 / 0 % | Sin problema |

Tiempos: sobran unos 22 ns de margen en el reloj más ajustado. El diseño no está
limitado por temporización, así que se puede cambiar velocidad por área.

**Los recursos saturados, de más a menos urgente:** slices/LUTs (lo que de verdad
impide añadir lógica), BRAM (impide añadir memoria) y pines (no se puede tocar).

## 2. Reparto por bloques

Cifras de la síntesis con jerarquía (XST, antes de que MAP empaquete). Sirven para
repartir, no para sumar con MAP: MAP reduce la lógica un 19 % (3.566 de 4.422 LUTs)
y la memoria distribuida casi a la mitad (797 de 1.581, porque une dos RAM16X1D en
las mismas LUTs). En la última columna he aplicado esos mismos factores para
estimar el reparto de las 4.397 LUTs finales.

| Bloque | LUTs lógica | LUTs memoria (RAM16X1D) | Flip-flops | Reparto estimado de las LUTs finales |
|---|---:|---:|---:|---:|
| **Sprites** (32 × `sprite_slot`) | 1.312 (41 por slot) | 1.536 (768 RAM16X1D) | 576 | **≈ 39 %** (~940 lógica + ~775 memoria; ver 3.2) |
| **Lógica principal `SD81`** (vídeo, mapper, configuración, Chroma, doble búfer, MCU...) | 1.890 | 0 | 665 | **≈ 35 %** |
| **2 chips AY** (`ay_3_8192` × 2) | 717 (358 cada uno) | 40 | 554 | **≈ 14 %** |
| **Depurador y simuladas** (`sim_int` + `trace_wr` + `m1_tracker`) | 380 | 2 | 230 | **≈ 7 %** |
| Beeper + DAC I2S | 62 | 0 | 81 | ≈ 1 % |
| Total | 4.361 (XST: 4.422) | 1.581 | ~2.100 (MAP: 2.103) | |

Lo más llamativo: **los sprites se llevan alrededor del 39 % de las LUTs** y casi toda la
memoria distribuida. Cada slot guarda 24 bytes (8 de píxel, 8 de máscara y 8 de
color, uno por fila) y lleva dos restadores de 9 bits, comparadores y multiplexores
de 8 a 1, y todo eso se repite 32 veces. Y 6.144 bits de datos cuestan 775 LUTs
porque no hay BRAM libre.

**La lógica principal no se puede repartir con esta herramienta**: es un único
módulo y los nombres de las LUTs son anónimos. Lo que sí se ve por los registros
(665): mapper y bloques (46), patrón del borde (46 + sus multiplexores), registros
de configuración (34), configuración del joystick (30), direcciones y contadores de
vídeo (unos 200), 24 + 13 + 9 registros "FRB" que XST añade al reequilibrar
registros, y los de la FPGA de doble búfer (blit) y de pantalla ancha. Para repartir
sus LUTs hace falta separarlo en submódulos (ver 3.7).

La BRAM (32 RAMB16) es una sola instancia, `bramdp_w`: la RAM de sombra de 64 K × 8
que espeja todo lo que escribe la CPU, sirve el vídeo y contiene también el búfer
frontal del doble búfer. Por eso no hay forma barata de liberar BRAM sin perder
cobertura de memoria.

## 3. Alternativas, de menos a más esfuerzo

Los ahorros son estimaciones salvo donde se indica "medido".

### 3.1 Opciones de las herramientas (sin tocar el diseño): medido

Medido el 5 de octubre en la rama `fpga-camino-a`, con síntesis completa (XST, MAP,
PAR) de cada variante. Todas cumplen la temporización.

| Variante | LUTs | Slices | Registros en pines (IOB) |
|---|---:|---:|---:|
| Release v1.6.0 (referencia) | 4.397 | 1.336 (93 %) | 34 |
| Solo MAP combinación de LUTs en área | 4.362 (−35) | 1.380 | 34 |
| Solo XST optimización en área (esfuerzo normal) | 4.298 (−99) | 1.373 | 31 |
| Solo sin equilibrado de registros | 4.415 (+18) | 1.396 | 31 |
| Las tres juntas, con duplicación de registros activa | 4.255 (−142) | 1.350 | 31 |
| Las tres juntas y sin duplicación de registros | 4.257 (−140) | 1.301 | 27 |

Conclusiones:

- **Ahorro real: ~140 LUTs (3 %)**, casi todo de la optimización de XST en área.
- **Los slices no mejoran de forma fiable.** Varían entre 1.301 y 1.396 con casi las
  mismas LUTs: con el diseño tan lleno, la colocación se mueve ±3 % por cualquier
  cambio. No se puede prometer que bajen.
- **Bajan los registros dentro de los pines** (de 34 a 31 o 27). Esos registros fijan
  cuándo se muestrea el bus del Z80, y el proyecto ya pasó por problemas de
  temporización en ese punto. El análisis de tiempos pasa, pero habría que
  validar en hardware.
- Veredicto: **no merece la pena** por sí solo; el riesgo es mayor que el ahorro.

Un efecto lateral útil: la VM de ISE, con 3 núcleos y proveedor KVM, ha pasado de
13 min 46 s por síntesis a unos 4 min 15 s con tres a la vez, y de 7 min 21 s de
tiempo de sistema a unos 45 s.

### 3.2 Sprites: sacar la decodificación de configuración de cada slot: descartado

Se probó (rama `fpga-camino-a`, `sprite_cfg_decode`, con un banco de pruebas en
ModelSim que confirmaba el mismo comportamiento) y **no ahorra nada**: con la misma
configuración de herramienta, 4.255 LUTs sin el cambio y 4.282 con él. XST ya
fusionaba esa lógica idéntica entre los 32 slots en el diseño plano. La estimación
de 260-390 LUTs venía de la síntesis con jerarquía, que cuenta cada slot por
separado y no ve esa fusión. Revertido.

Esto corrige también el reparto de la sección 2: la síntesis con jerarquía suma
143 LUTs más que la plana (6.146 frente a 6.003 de XST), y buena parte son lógica de
slots que en el diseño plano se comparte. Los sprites pesan entonces unas **1.700
LUTs (~39 %)** y no 1.832 (42 %). Es una estimación: no se ha medido el módulo
aislado en el diseño plano.

### 3.3 Menos sprites o menos memoria por sprite

| Cambio | Ahorro estimado (LUTs finales) | Qué se pierde |
|---|---:|---|
| 32 → 24 sprites (`NUM_SPRITES`) | ~460 (−10 %) | 8 sprites |
| 32 → 16 sprites | ~915 (−21 %) | 16 sprites |
| Un color por sprite en vez de por fila | ~260 (−6 %) | Sprites de varios colores |
| Sin máscara (el color 0 es transparente) | ~260 (−6 %) | Transparencia independiente del píxel |

Es una decisión de producto, no técnica: hay que ver cuántos sprites usa el
software real (el manual y los ejemplos de `EXAMPLES/SPRITES`).

### 3.4 Sprites: leer la tabla de la RAM de sombra (la idea buena)

La FPGA ya guarda una copia completa de los 32 sprites en la RAM de sombra, en
`$0C00 + sprite*32 + campo` (32 bytes por sprite: activo, X, Y, 8 colores, 8 de
píxel y 8 de máscara), para los snapshots. Esa misma información está duplicada hoy
en la memoria distribuida de los slots (~775 LUTs). Usar la copia de la sombra
elimina la duplicación, pero no es cambiar la RAM de sitio: los 32 slots comparan en
paralelo y cada uno necesita su dato en cada píxel, y la BRAM solo tiene dos puertos.
Hay que pasar a un esquema **por línea**:

1. Durante el borde horizontal de cada línea (hay de sobra: unos 150 relojes de
   píxel libres en 32 columnas, y el puerto B de la BRAM no lo usa el vídeo),
   un pequeño evaluador recorre los 32 sprites en la sombra (activo e Y), y para
   los que cortan la línea siguiente lee X, color, píxel y máscara de su fila.
   Son como mucho 32 × 7 = 224 lecturas, unos 250 ciclos de 26 MHz.
2. Esos datos se cargan en **K slots de línea**: registros de desplazamiento de 8
   bits (píxel y máscara), color y un contador de X. Mientras la línea se dibuja,
   los K slots hacen lo que hoy hacen los 32.
3. Se recorren los sprites del 31 al 0 para que, si hay más de K en una línea,
   sobrevivan los de índice alto, que son los que hoy ganan en prioridad.

Coste de cada slot de línea: ~33 registros y ~35 LUTs (hoy ~65 LUTs más 18
registros, contando su memoria), más el evaluador (~120-150 LUTs) y la mezcla de
prioridad.

| K (sprites por línea) | LUTs de sprites | Ahorro estimado | Registros |
|---|---:|---:|---:|
| 8 | ~450 | **~1.100-1.250 (−25 a −28 % del total)** | ~300 |
| 16 | ~750 | ~800-950 (−18 a −22 %) | ~550 |
| 32 (sin límite por línea) | ~1.350 | ~200-350 | ~1.050 |

Con K = 8 sigue siendo la mayor palanca de todo el informe (con K = 16 baja bastante). A cambio:

- **Límite de K sprites por línea de pantalla** (los demás no se dibujan en esa
  línea). Hay 32 sprites en total; lo que cambia es cuántos pueden coincidir en
  la misma línea. Hay que comprobar con el software real cuántos usa.
- **La calibración del desplazamiento** (`SPR_X_FUDGE`, `SPR_Y_FUDGE` y las dos
  variantes de Superfast, ajustados sobre hardware) hay que rehacerla con la
  herramienta de calibración, porque cambia la latencia.
- **El reset**: la RAM de sombra no se borra con el reset del Z80, y hoy el reset
  pone a 0 el bit "activo" de los 32 slots. Haría falta un bit de validez por
  sprite (32 registros) que se borre con el reset.
- **Lo que mejora**: la restauración de snapshots pasa a ser escribir la tabla en la
  sombra (BRAMW), sin los ~900 POKEs de sprites que hace ahora el monitor. Y la
  FPGA y el snapshot pasan a ser una sola copia, no dos.
- Una escritura de la CPU justo cuando el evaluador lee el mismo sprite puede dar
  una línea con datos a medias: igual que hoy cuando se cambia un sprite a mitad de
  imagen.

Alternativa sin límite por línea: dibujar las líneas en un búfer de línea de dos
líneas (~200-300 LUTs de memoria distribuida). El ahorro es parecido al de K = 16
pero con más complejidad; solo merece la pena si el límite por línea no es aceptable.
Esfuerzo alto en ambos casos (rehacer `sprite_slot`, el compositor y la calibración)
y riesgo medio.

### 3.5 AY

- **Quitar el segundo AY de la FPGA: −290 LUTs y −277 registros**, pero se pierde
  ese chip (el del MCU, el de PLAY/VGM/PEG, es otro). Hay que decidir si algo usa
  el segundo chip de la FPGA.
- Compartir la lógica entre los dos chips, alternando ciclos (el reloj de un AY es
  mucho más lento que el de sistema de 26 MHz): ahorro posible de ~100-150 LUTs, pero el
  estado (registros y contadores) no se comparte. Esfuerzo alto para poco: no lo
  recomiendo.

### 3.6 BRAM: lo que habría que liberar

Con una RAMB16 libre (2 KB) se podría colocar otra tabla (con 3.4 ya no hace falta para los sprites). La RAM de sombra
cubre los 64 KB y el depurador usa $0000 (pantalla) y $1000-$17FF (traza), así que no
hay un trozo evidente que sobre. Ideas, a investigar:

- Si el vídeo nunca lee de algún bloque (por ejemplo $C000-$FFFF), no hace falta
  espejarlo. Hay que comprobarlo con el software real: un `HFILE` o un `D_FILE`
  en esa zona dejaría de verse.
- Cambiar la RAM de sombra a 9 bits de ancho da 64 Kbit extra (el bit de paridad
  ya está en el silicio, sin usar), pero con la misma dirección que el byte: sirve
  para banderas por byte, no para tablas independientes.

### 3.7 Lógica principal

- Separarla en submódulos (vídeo, mapper, configuración, doble búfer, comunicación
  con el MCU) y volver a medir con jerarquía. No ahorra nada por sí mismo, pero
  convierte las 1.890 LUTs en cifras por bloque y permite apuntar a lo caro. Es el
  paso previo a cualquier optimización seria en esta parte.
- Candidatos ya visibles: el patrón del borde (8 × 8 en 46 registros con dos
  multiplexores 8:1 de 16 LUTs; en una RAM16X1D serían ~8 LUTs; ahorro ~40), los
  ~20 sumadores de 16 bits para direcciones de pantalla (el cálculo de `row_stride`
  y `char_addr` en 80 columnas) que se pueden simplificar con desplazamientos y
  sumar menos bits, y el reequilibrado de registros de XST
  (`-register_balancing No`, que quita los 46 registros "FRB").
- Los 16 DSP48A1 están sin usar: algunos sumadores grandes se podrían mover a
  ellos con instanciación manual (ahorro posible de ~100-150 LUTs; esfuerzo medio).

### 3.8 Variantes de compilación

Un `define` para compilar sin el depurador y la traza (−380 LUTs, ≈ −8 %), con
menos sprites, o sin el segundo AY. No libera nada en la versión completa, pero
permite que una función nueva grande entre en una variante concreta.

## 4. Resumen y propuesta

**Camino A (medido en la rama `fpga-camino-a`): lo que no cambia el comportamiento**

| Paso | Resultado |
|---|---|
| Opciones de XST/MAP (3.1) | −140 LUTs (3 %), slices sin mejora fiable, registros de pines alterados |
| Decodificación fuera de los slots (3.2) | 0: la herramienta ya lo hacía |
| Patrón del borde y sumadores (3.7) | Sin tocar: ~10 LUTs el borde, y los sumadores de dirección y el mezclador de audio tienen reglas de ancho y signo delicadas para un ahorro pequeño |
| **Total** | **~140-250 LUTs (3-6 %)**, y no baja los slices |

El camino A casi no libera nada, así que no lo propongo para fusionar.

**Camino B: sprites desde la sombra (3.4)**

| Concepto | K = 8 | K = 16 |
|---|---:|---:|
| Sprites desde la sombra | ~1.100-1.250 | ~800-950 |
| Opciones de XST/MAP (3.1), si se aceptan | ~100 | ~100 |
| **Total** | **~1.100-1.350** | **~800-1.050** |
| LUTs resultantes (hoy 4.397) | ~3.050-3.300 (53-58 %) | ~3.350-3.600 (59-63 %) |

**Slices.** Hoy 4.700 pares LUT-registro en 1.336 slices (88 % de eficiencia de
empaquetado, alta porque la herramienta aprieta para que quepa). Con menos lógica
el empaquetado se relaja. Con eficiencia entre el 88 % y el 70-75 %, y teniendo
en cuenta que los slices varían ±3 % con cualquier cambio:

| Caso | Slices estimados | % de 1.430 |
|---|---:|---:|
| Hoy | 1.336 | 93 % |
| B con K = 16 | ~1.000-1.250 | ~70-87 % |
| B con K = 8 | ~920-1.150 | ~64-80 % |

Casi toda la memoria distribuida desaparece (de 797 a ~25 LUTs), lo que quita la
presión sobre los slices de tipo M.

**Aparte, y se suma a cualquiera:** segundo AY fuera (~290 LUTs, pierde un chip) y
variante sin depurador (~380, quita el depurador de esa variante).

Las cifras de B son estimaciones: dependen de cómo salga el evaluador. Lo medido
ha corregido dos veces mis estimaciones a la baja, así que conviene tratarlas como
techo.

## 5. Resultado del camino B (rama `fpga-camino-b`, K = 8): medido

Síntesis completa (XST, MAP, PAR) con las opciones de herramienta de la release, sin
tocar ninguna. Temporización cumplida.

| | Release v1.6.0 | Camino B (K = 8) | Diferencia |
|---|---:|---:|---:|
| Slice LUTs | 4.397 (76 %) | **2.803 (49 %)** | **−1.594 (−36 %)** |
| LUTs como lógica | 3.566 | 2.732 | −834 |
| LUTs como memoria distribuida | 797 | 35 | −762 |
| Registros | 2.103 | 1.860 | −243 |
| Slices ocupados | 1.336 (93 %) | **1.014 (71 %)** | −322 |
| BRAM | 32 / 32 | 32 / 32 | sin cambio |

Es más de lo que estimé (1.100-1.250 LUTs): la lógica por slot era mayor de lo que
calculé. La memoria distribuida de los sprites desaparece, tal como se esperaba.

El motor (`sprite_engine.v`) se comprobó en ModelSim contra los 32 `sprite_slot` de la
v1.6.0: idéntico, píxel a píxel, con tablas al azar de hasta 8 sprites por línea, y igual a
la especificación (los 8 de índice más alto de cada línea) con tablas sin límite, en modo de
32 y de 70/80 columnas (más de 2 millones de píxeles comparados, 0 diferencias; los sprites
no están implementados en 70/80 columnas, esas pruebas solo confirman que el motor da lo mismo
que los slots antiguos allí; un banco que
falla si se altera el motor a propósito). El caso peor (32 sprites activos y los 8 que cortan
la línea siendo los últimos en recorrerse) termina en el píxel 109 (32 columnas) o 99 (80);
el motor se corta en el 120.

**Lo que no se ha podido comprobar:** el montaje real (arbitraje del puerto B de la BRAM con
el vídeo, la calibración de las posiciones en los tres modos, y los sprites con el doble
búfer) solo se ve en el hardware. Cuatro entradas (`nM1`, `nMREQ`, `nRFSH` y `CFG_DATA`) han
perdido su registro dentro del pin como efecto de la nueva síntesis; no hay restricciones de
tiempo sobre ellas y la temporización pasa, pero conviene confirmarlo en la placa.

## 6. 64 sprites (rama `fpga-sprites64`, K = 8): medido

Sobre la rama B: el motor pasa a `system_clk` (un paso de lectura cada 2 ciclos, 4 pasos de
lectura por reloj de píxel en 32 columnas y 2 en 80), `spr_en` pasa a 64 bits y la segunda
tabla de sprites (32-63) va en la sombra en `$1800-$1BFF` (libre).

| | Release v1.6.0 | B (32 sprites) | 64 sprites |
|---|---:|---:|---:|
| Slice LUTs | 4.397 (76 %) | 2.803 (49 %) | **2.852 (49 %)** |
| Registros | 2.103 | 1.860 | 1.911 |
| Slices ocupados | 1.336 (93 %) | 1.014 (71 %) | **1.027 (72 %)** |
| BRAM | 32 / 32 | 32 / 32 | 32 / 32 |

Pasar de 32 a 64 sprites cuesta **49 LUTs** (el bit de activo de los 32 sprites nuevos y los
contadores), tal como se estimó. Temporización cumplida.

Margen de tiempo del motor (caso peor: 64 sprites activos y los 8 que cortan la línea son los
últimos que recorre): en 32 columnas basta un límite de lectura en el píxel 90 y el hardware
usa el 120; en 80 columnas (donde los sprites no están implementados) hace falta entre 120 y 150
y el hardware usa el 195 (el vídeo no lee la sombra hasta el 206). Comprobado en ModelSim con 64 sprites contra los slots antiguos (0
diferencias con hasta 8 por línea y contra la especificación con más) en los dos modos.

Fuera de la FPGA: la ROM acepta sprites 0-63 y pone a cero los 64 en el arranque (`SPR_COUNT`),
el MCU guarda y carga las dos tablas en los snapshots (`SPRITE n` hasta `3F`, comprobado con
una prueba en el banco del depurador) y el manual (Apéndice H) y la especificación del emulador
(`sprites_por_linea_emulador.md`) lo recogen. Hace falta actualizar la FPGA, el MCU y la ROM a la
vez: con la FPGA de 32 sprites, un snapshot con sprites 32-63 los escribiría sobre la copia de
los 0-31.

## 7. Sprites por línea (K): 8, 12 y 16, con 64 sprites: medido

Mismo diseño de 64 sprites, solo cambia K (los slots de línea). Síntesis completa de cada
uno, temporización cumplida en los tres.

| | K = 8 | **K = 12** | K = 16 |
|---|---:|---:|---:|
| Slice LUTs | 2.852 (49 %) | **3.082 (53 %)** | 3.306 (57 %) |
| Registros | 1.911 | 2.046 | 2.183 |
| Slices ocupados | 1.027 (72 %) | **1.045 (73 %)** | 1.144 (80 %) |
| Ventana de lectura necesaria en el peor caso (32 columnas) | ~90 | ~100 | ~110 |
| Margen frente a los 120 del hardware | ~30 | ~20 | ~10 |

Cada slot de línea extra cuesta unas 57 LUTs. K no tiene que ser potencia de 2. Se eligió
**K = 12** porque el Mario del usuario se quedaba justo con 8 sprites por línea. (En 80 columnas
los sprites no están implementados; con K = 16 el peor caso no cabría allí.)

## Cómo repetir las medidas

1. En una copia del proyecto en la VM: poner `-keep_hierarchy Yes` en `SD81.xst`
   y ejecutar `xst -ifn SD81.xst -ofn SD81.syr` (unos 6 minutos).
2. `netgen -ofmt verilog -w -sim SD81.ngc SD81_h.v` genera la lista de módulos con
   sus primitivas.
3. `claude/planning/fpga_hier_report.py SD81_h.v` cuenta LUTs, registros y RAM
   distribuida de cada módulo.
4. Para el reparto por función dentro de `SD81` hace falta primero separarlo en
   módulos (3.7).
