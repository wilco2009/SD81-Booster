# ESTELA — matamarcianos vertical (en desarrollo)

Juego original para el modo **160x100 a 256 colores** del SD81 Booster
(`POKE 2045,175`): una nave que sube por un escenario de tiles que pasa por
debajo con el scroll en anillo de la FPGA, sprites de formato B y música VGM.
El plan entero (fases, mapa de memoria, sprites) está en
`claude/planning/juego160.md`.

| Carpeta / fichero | Qué es |
|---|---|
| `estela.asm`, `ESTELA.B81`, `compila.bat` | el juego (núcleo Z80) |
| `PRUEBAS/` | pruebas de la fase 0 en la placa (carga, VSYNC, sonido, paginación) |
| `tools/`, `assets/`, `out/` | herramientas del PC y gráficos del nivel (las lleva otra sesión; §8 del plan) |

## Estado: fase 2 (el fondo del nivel)

Lo que hay ahora es el **fondo del nivel 1 con scroll**, todavía sin nave ni
enemigos:

- **Carga de la SD** (de la misma carpeta que `ESTELA.BIN`), con la pantalla en
  negro:
  - `NIVEL1.I16`: la paleta de 256 del nivel y el tileset (60 tiles de 16x16), que
    va tal cual a `$C000` (los bloques 6 y 7 se mapean a las páginas 6 y 7 mientras
    corre el juego y se devuelven al salir);
  - `NIVEL1.MAP`: cabecera, oleadas y mapa de 8 tiles de ancho, detrás del código.

  Los dos los genera `tools/make.py` en `out/` (§8 del plan).
- **Modo 175**, **anillo de 160x102** en `$8000` (100 filas visibles y 2 ocultas
  donde se pinta lo que va a entrar por arriba) y **paneles laterales** de 16
  píxeles sacados de la fila 96 del tileset, iguales en todas las filas: se ven
  **quietos**.
- **Scroll vertical** del mapa del nivel desde su principio (la línea de abajo del
  texto de `NIVEL1.LVL`) a la velocidad del nivel (`LVL_SCROLL`: 1 píxel por
  imagen). Al llegar al final del nivel (`LVL_END`, 4.380 píxeles: algo menos de un
  minuto y medio) el scroll **se para**.
- **Ciclo de paleta**: el agua (colores 224-239) gira una posición cada 4 imágenes,
  la lava (240-247) cada 6 y las luces (248-255) cada 8, en el borrado. El resto de
  la paleta no cambia.

## Teclas

| Tecla | Acción |
|-------|--------|
| S | Velocidad: la del nivel al empezar (1 píxel por imagen, borde **verde**) → 1 cada 2 imágenes (**cyan**) → 2 por imagen (**amarillo**) → parado (**rojo**) → 1 por imagen |
| R | Volver al principio del nivel (la pantalla se queda en negro unas 4 imágenes mientras se repinta) |
| X | Salir a BASIC (modo normal, color apagado, páginas de antes) |

Si falta algo, el programa vuelve a BASIC enseguida y escribe `ERROR DE CARGA n`:

| n | Qué pasa |
|---|---|
| 1 | no hay `NIVEL1.I16` en la carpeta |
| 2 | `NIVEL1.I16` no es un `.I16` (no empieza por `I160`) |
| 3 | no hay `NIVEL1.MAP` |
| 4 | `NIVEL1.MAP` no es un nivel (no empieza por `ELV1`) |
| 5 | el nivel no cabe entre el código y `$7000` (caben 5.343 bytes; el de ejemplo tiene 2.488) |
| 6 | la cabecera del nivel no cuadra (menos de 7 filas, o el mapa se sale del fichero) |

## Qué tiene que verse

- El fondo del nivel (hierba, agua, lava, metal...) baja **suave, sin saltos ni
  temblores**, a las tres velocidades, con los bordes de los tiles **continuos**:
  ninguna fila repetida, cortada o de otro color al pasar por el borde del anillo
  (cada 102 filas) ni al cambiar de fila de tiles (cada 16).
- El **agua, la lava y las luces se mueven** solas (ciclo de paleta) y el resto de
  colores se queda fijo.
- Por **arriba no entra basura**: lo que aparece ya está pintado.
- Los **paneles** de los lados no se mueven nada.
- R vuelve al principio; al final del nivel el fondo se para.
- Al salir, el BASIC se lee.

**Qué hay que mandar:** si se ve así; si no, qué falla (una foto ayuda) y a qué
velocidad. Y, a ojo, si la velocidad del ciclo del agua, la lava y las luces está
bien (se cambia con `TCYW`, `TCYL` y `TCYG`).

## Comprobado en el PC

`claude/mqtttest/build_estela.sh`: el Z80 de EightyOne con un modelo de la memoria
paginada, el VSYNC (64.584 T por imagen, 8 líneas, como lo midió `PRUEBAS/VSYNC` en
la placa), el teclado, la paleta, el direccionamiento del anillo de la FPGA y un MCU
que sirve los ficheros de `out/`. El modelo del nivel lee `NIVEL1.I16` y
`NIVEL1.MAP` por su cuenta, escrito aparte del Z80:

- el tileset de `$C000` es el del `.I16` y el nivel está entero detrás del código;
- en **472 instantes al azar**, lo visible es **exactamente** el nivel desde la
  posición que había cuando se escribió `POKE 2072`, con los paneles;
- nunca se escribe una fila del anillo que se esté viendo (salvo con R, en negro);
- `POKE 2072` sale como mucho 71 T después del flanco del VSYNC, una vez por imagen;
- la paleta, en **cada imagen**: la del `.I16` con el agua, la lava y las luces
  giradas lo que toca; los giros se escriben antes de 5.700 T (la imagen empieza
  hacia los 12.600);
- velocidad del nivel y S (1, 1 cada 2, 2 y 0 filas por imagen); R vuelve al
  principio;
- un nivel corto de 10 filas: el scroll se para en `LVL_END` y no pinta más allá
  del mapa;
- los 7 errores de carga vuelven con su número y todo restaurado;
- al salir: modo 85, anillo a 0, `$7FEF = $0C`, páginas de los bloques 6 y 7 como
  estaban, IX, y nada escrito fuera de su sitio;
- la vuelta más larga del bucle (2 píxeles por imagen y los tres ciclos de paleta a
  la vez): 13.854 T de los 64.584 de una imagen.

**Sin probar en la placa.** La fase 1 (fondo generado en el código) sí se probó en
la placa el 2026-11-08; la primera prueba falló por un `estela.bin` viejo, y desde
entonces el arnés ensambla y prueba el `estela.bin` de esta carpeta, el mismo que va
a la SD.

## Ensamblar y ejecutar

`compila.bat` ensambla con pasmo, comprueba que el binario no pasa de `$6FFF`
(la hoja de los sprites está en `$7000` y el ZX81 no ejecuta código por encima de
`$8000`) y lo copia al emulador (`SD81\SD81\GAMES\ESTELA`) con `out\NIVEL1.I16` y
`out\NIVEL1.MAP`. Para la placa, copia `ESTELA.BIN`, `ESTELA.B81`, `NIVEL1.I16` y
`NIVEL1.MAP` a una misma carpeta de la SD y carga `ESTELA.B81`.

Memoria: código y tablas en `$4800-$5920` (las trayectorias de `LEVEL.INC` ya van
dentro), paleta del nivel en `$5921-$5B20`, `NIVEL1.MAP` desde `$5B21`, anillo en
`$8000-$BFBF`, tileset en `$C000-$FE7F` (páginas 6 y 7).

**El emulador** todavía no tiene el scroll en anillo de los modos 160
(`sprites_scroll160_emulador.md`): ahí el fondo no se moverá bien. Hay que probarlo
en la placa.
