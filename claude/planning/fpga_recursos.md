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
| **Sprites** (32 × `sprite_slot`) | 1.312 (41 por slot) | 1.536 (768 RAM16X1D) | 576 | **≈ 42 %** (~1.060 lógica + ~775 memoria) |
| **Lógica principal `SD81`** (vídeo, mapper, configuración, Chroma, doble búfer, MCU...) | 1.890 | 0 | 665 | **≈ 35 %** |
| **2 chips AY** (`ay_3_8192` × 2) | 717 (358 cada uno) | 40 | 554 | **≈ 14 %** |
| **Depurador y simuladas** (`sim_int` + `trace_wr` + `m1_tracker`) | 380 | 2 | 230 | **≈ 7 %** |
| Beeper + DAC I2S | 62 | 0 | 81 | ≈ 1 % |
| Total | 4.361 (XST: 4.422) | 1.581 | ~2.100 (MAP: 2.103) | |

Lo más llamativo: **los sprites se llevan el 42 % de las LUTs** y casi toda la
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

### 3.1 Opciones de las herramientas (sin tocar el diseño)

- **XST `-opt_mode Area` y `-opt_level 1`: medido.** Lógica 4.422 → 4.277 LUTs
  (−145, −3 %) y registros 2.026 → 1.954. Poco, pero gratis. Como sobran 22 ns de
  margen, no hay riesgo de temporización.
- **MAP `-lc auto` (combinar LUTs) y `-global_opt area`**: hoy están apagados
  (`-lc off`, `-global_opt off`). No lo he medido; lo habitual es un 3-5 % más,
  y también reduce los slices.
- Coste: unos minutos y una síntesis completa. Riesgo: cambia la colocación, hay
  que volver a probarlo en el hardware.

### 3.2 Sprites: sacar la decodificación de configuración de cada slot

Cada `sprite_slot` decodifica por su cuenta `cfg_field` (4 comparadores de 5 bits
y 2 restadores de 3 bits, 32 veces). Esa decodificación es idéntica para todos y
se puede hacer una sola vez fuera de los slots. Ahorro estimado **~10-15 LUTs por
slot: 320-480 LUTs de XST, 260-390 finales (6-9 %)**. Esfuerzo bajo (un fichero de
105 líneas), riesgo bajo, sin pérdida funcional.

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
| 8 | ~450 | **~1.400 (−32 % del total)** | ~300 |
| 16 | ~750 | ~1.100 (−25 %) | ~550 |
| 32 (sin límite por línea) | ~1.350 | ~480 | ~1.050 |

Con K = 8 o 16 es, con diferencia, la mayor palanca de todo el informe. A cambio:

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

Las medidas no se suman sin más: 3.4 sustituye los slots de sprites, así que 3.2 y
3.3 desaparecen (su ahorro ya está dentro del de 3.4), y las opciones de herramienta
(3.1) se aplican sobre una lógica más pequeña, con menos margen.

**Camino A: sin sprites desde la sombra** (solo lo que no cambia el comportamiento)

| Paso | Ahorro estimado (LUTs) |
|---|---:|
| Opciones de XST/MAP (3.1) | 150-300 |
| Decodificación fuera de los slots (3.2) | 260-390 |
| Patrón del borde y sumadores (3.7) | 100-200 |
| **Total** | **~500-900 (12-20 %)**, slices ~93 → ~80-85 % |

**Camino B: sprites desde la sombra (3.4) más lo independiente**

| Paso | K = 8 | K = 16 |
|---|---:|---:|
| Sprites desde la sombra (3.4; incluye 3.2 y 3.3) | ~1.400 | ~1.100 |
| Opciones de XST/MAP (3.1), sobre la lógica restante | 100-200 | 100-200 |
| Patrón del borde y sumadores (3.7) | 100-200 | 100-200 |
| **Total** | **~1.600-1.800** | **~1.300-1.500** |
| LUTs resultantes (hoy 4.397) | ~2.600-2.800 (46-49 %) | ~2.900-3.100 (51-54 %) |

**Slices.** Hoy hay 4.700 pares LUT-registro en 1.336 slices (de 4 pares cada uno):
una eficiencia de empaquetado del 88 %, alta porque la herramienta está apretando
para que quepa. Con menos lógica el empaquetado se relaja, así que el slice no
baja tanto como los pares. Estimación con eficiencia entre el 88 % (la actual) y
el 70-75 % (un diseño holgado):

| Caso | Pares LUT-registro | Slices estimados | % de 1.430 |
|---|---:|---:|---:|
| Hoy | 4.700 | 1.336 | 93 % |
| Camino A | ~3.800-4.200 | ~1.080-1.350 | ~75-85 % |
| B con K = 16 | ~3.200-3.400 | ~910-1.130 | ~64-79 % |
| B con K = 8 | ~2.900-3.100 | ~820-1.050 | ~57-73 % |

Los registros también bajan en B (los sprites pasan de 576 a ~300 o ~550) y casi
toda la memoria distribuida desaparece (de 797 a ~25 LUTs): se acaba la presión
sobre los slices de tipo M, que hoy tienen un 55 % ocupado solo por los sprites.

**Aparte, y se suma a cualquiera de los dos:** segundo AY fuera (~290 LUTs, pierde un
chip) y variante sin depurador (~380, quita el depurador de esa variante).

Las cifras son estimaciones; la de 3.4 es la más incierta, porque depende de
diseñar el evaluador. Se puede afinar sintetizando solo un `sprite_slot` de línea
de prototipo.

## Cómo repetir las medidas

1. En una copia del proyecto en la VM: poner `-keep_hierarchy Yes` en `SD81.xst`
   y ejecutar `xst -ifn SD81.xst -ofn SD81.syr` (unos 6 minutos).
2. `netgen -ofmt verilog -w -sim SD81.ngc SD81_h.v` genera la lista de módulos con
   sus primitivas.
3. `claude/planning/fpga_hier_report.py SD81_h.v` cuenta LUTs, registros y RAM
   distribuida de cada módulo.
4. Para el reparto por función dentro de `SD81` hace falta primero separarlo en
   módulos (3.7).
