# Snapshots del depurador: lo que ha cambiado en el emulador

Para la sesión de la FPGA/MCU. Al probar en el emulador (EightyOne2) el
`snap` del depurador y la carga de un `.Z81` con el **explorador de ficheros**
(modo de texto de 70 columnas), la carga salía mal: borde azul y pantalla en
blanco, y después de arreglarlo las flechas no respondían. Eran **tres cosas
que el `.Z81` no guardaba**. Hay que replicarlas en el `snap` del MCU
(`DEBUGGER.cpp`, sección 11 de `hw_debugger_emulator.md`) y en la carga
(`LOAD *Z81`, `z81_snapshot_loading.md`), porque el hardware tiene el mismo
problema: un snapshot del explorador no volvería a funcionar.

El emulador ya escribe y lee las tres claves; sirve de referencia, y su `.Z81`
se puede comparar con el del MCU.

Todas son claves nuevas dentro de `[SD81BOOSTER]`. EightyOne original y los
cargadores que no las conocen las ignoran, así que un `.Z81` con ellas sigue
valiendo en un ZX81 sin SD81 Booster.

## 1. Modo de texto ancho: `WIDE_COLS`

**Qué faltaba:** `DISPLAY_MODE` solo lleva tres bits (1 Superfast, 2 HiRes,
4 Spectrum). El modo ancho (`POKE 2045,173` = 70 columnas y `174` = 80) se
perdía: el explorador volvía como texto de 32 columnas.

**Formato:**
```
WIDE_COLS 46        ; 70 columnas
WIDE_COLS 50        ; 80 columnas
```
- Un byte en hexadecimal (`$46` = 70 y `$50` = 80). El valor es el número de
  columnas, no el del POKE.
- **Solo se escribe cuando hay modo ancho.** Sin la clave, la carga lo deja
  en modo normal.
- `DISPLAY_MODE` sigue valiendo `01` con el modo ancho (es texto Superfast).

**Para el MCU al hacer `snap`:** el último valor escrito en `POKE 2045` (está
en la BRAM de sombra, índice 2 del puerto `$3FEF`; 2045 queda dentro de las
2038-2098 que ya se leen) dice el modo: 173 → `WIDE_COLS 46`, 174 →
`WIDE_COLS 50`, y 85, 170, 171 o 172 → sin la clave.

**Para la carga (`LOAD *Z81`):** si trae `WIDE_COLS`, hay que escribir el
`POKE 2045` con 173 (46) o 174 (50) en vez de 170. La nota de
`z81_snapshot_loading.md` sobre `LOAD *Z81` y los modos de vídeo extendidos
(no compatible hoy) pasa a ser esto.

## 2. Direcciones alternativas de pantalla y de atributos: `DISP_ADDR`, `ATTR_ADDR`

**Qué faltaba:** el explorador no usa D_FILE para dibujar, sino el
**D_FILE alternativo** (`POKE 2096-2098`) y la **base de atributos
alternativa** (`POKE 2059-2061`). Con ellos sin restaurar, la pantalla se
leía de otro sitio y salía en blanco.

**Formato:**
```
DISP_ADDR E000 01     ; dirección y "activo"
ATTR_ADDR E800 01
```
- Dirección de 16 bits en hexadecimal y un byte de estado (siempre `01`).
- **Solo se escriben si están activos.** Sin la clave, el override queda
  desactivado (el comportamiento por defecto).

**Para el MCU al hacer `snap`:** los valores salen de los POKEs que ya se
leen con `HW_POKES` (2038-2098), con la convención de siempre (170 = activo,
85 = inactivo, el último valor escrito manda):

| Clave | Dirección | Activo si el último valor de… |
|---|---|---|
| `DISP_ADDR` | `POKE 2096` (bajo), `2097` (alto) | `2098` = 170 |
| `ATTR_ADDR` | `POKE 2059` (bajo), `2060` (alto) | `2061` = 170 |

Si alguno de los POKEs nunca se escribió (`--`), el override no está activo.
El estado se pierde también con `POKE 2045,85` (pasa a D_FILE normal), así
que si el último modo escrito en 2045 fue 85, tampoco se escriben.

**Para la carga:** escribir 2096/2097 (o 2059/2060) con la dirección y luego
2098 (o 2061) con 170, en este orden y después de fijar el modo con `POKE
2045` (que los apaga si recibe 85, y no los toca con 170-174).

## 3. El listado de directorio abierto: `DIR_OPEN`

**Qué faltaba:** esta es la que dejaba sin responder las flechas. El
explorador pide las filas al MCU con `GETROWLEN` y `GETROW`, y esas filas
salen del listado que construyó el último `OPENDIR` (`file_array[]`). Eso es
**estado del MCU, no de la RAM**: tras cargar el snapshot el listado no
existía y las flechas no hacían nada. Pulsar `5` (directorio anterior) lo
volvía a abrir y todo funcionaba.

**Formato:**
```
DIR_OPEN 2A          ; el argumento del último OPENDIR, en bytes hexadecimales
DIR_OPEN -           ; el argumento vacío
```
- El argumento es el patrón con el que se llamó a `OPENDIR` (`*`, `*.Z81`,
  una ruta…). Va en hexadecimal para que valgan espacios y caracteres raros.
  `-` es el argumento vacío.
- **Solo se escribe si había un listado abierto.**

**Para el MCU al hacer `snap`:** hay que **guardar el argumento del último
`OPENDIR`** (una variable nueva en el firmware, por ejemplo
`last_opendir_arg[]`, puesta en `cmd_opendir`) y volcarlo. Hoy no se guarda,
así que es la única de las tres que necesita código nuevo en el firmware y
no solo leer la BRAM.

**Para la carga:** después de restaurar el directorio actual (`CUR_DIR`),
volver a ejecutar el `OPENDIR` con ese argumento. **El orden importa:** el
listado depende del directorio actual, así que primero `CUR_DIR` y luego
`DIR_OPEN`.

## Resumen de las claves nuevas

| Clave | Cuándo se escribe | De dónde la saca el MCU |
|---|---|---|
| `WIDE_COLS 46` / `50` | solo con 70 / 80 columnas | último valor de `POKE 2045` (173 / 174) |
| `DISP_ADDR dir 01` | solo con el override activo | POKEs 2096, 2097 y 2098 = 170 |
| `ATTR_ADDR dir 01` | solo con el override activo | POKEs 2059, 2060 y 2061 = 170 |
| `DIR_OPEN arg` | solo con listado abierto | variable nueva del firmware (último `OPENDIR`) |

Las tres primeras se derivan de los POKEs que `snap` ya lee; la cuarta es
estado del MCU que hay que recordar.

## Cómo comprobarlo

1. Arrancar el explorador (70 columnas), pausar con `p` y hacer `snap`.
2. Cargar el `.Z81` con `LOAD *Z81` (o desde el menú del emulador, que ya lo
   hace bien).
3. Tiene que verse el explorador igual que antes, y las flechas tienen que
   moverse por la lista **sin pulsar `5`** antes.
4. Comparar el `.Z81` del MCU con el que genera el emulador desde el mismo
   punto: las cuatro claves tienen que coincidir.

## Otras claves que el emulador guarda y el MCU todavía no

Por si se quiere afinar más la fidelidad. Ninguna ha hecho falta para el
explorador:

- `AY1_REGS`, `AY1_REG_SEL`, `AY2_REGS`, `AY2_REG_SEL`: los registros de los
  dos AY y el seleccionado.
- `VGM_PATH`, `VGM_PLAYING`, `VGM_LOOP`, `VGM_POS`: reproducción VGM.
- `PEG_MEM`, `PEG_PC`, `PEG_ADDR`, `PEG_RUNNING`, `PEG_CARRY`, `PEG_VARS`:
  el estado de los hilos PEG.
- `FILE_HANDLE`: los ficheros abiertos por el programa.
- `SPRITE_SEL` y `SPRITE`: los 32 sprites por hardware.
- `SHADOW`: los bloques de la BRAM de sombra que difieren de la RAM mapeada.

## Cosas del emulador que no hay que replicar

- El tamaño del búfer de vídeo y la paleta de la emulación de TV (en el
  emulador se recalculan al entrar y salir del modo ancho): son internos del
  emulador y no existen en el hardware.
- El comando `DBG_FPGA` de la consola del emulador, que vuelca el estado de
  la FPGA y del mapper para comparar antes y después de cargar. Solo sirve
  para depurar el emulador.
