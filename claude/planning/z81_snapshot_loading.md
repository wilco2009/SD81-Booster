# Carga de snapshots .Z81 — especificación para el emulador

Contrato de lo que ve el **Z80**. El comando MCU y la ROM ya están implementados
en el hardware real:

- MCU: `cmd_loadZ81` en
  [`Arduino/SD81BoosterV2_039_STM32/COMMANDS.cpp`](../../Arduino/SD81BoosterV2_039_STM32/COMMANDS.cpp)
  (comando 70 / `0x46`, tabla `commands[]`).
- ROM: `CmdZ81` en
  [`z80rom/sdhandler.inc.asm`](../../z80rom/sdhandler.inc.asm) (`LOAD *Z81
  "fichero"`, entrada en `CmdList`).

Este documento es lo que hay que reproducir en `SD81Booster.cpp` del
emulador (`src/SD81Booster/SD81Booster.cpp`, la máquina de estados
`ProcessByte`). **Importante**: solo hace falta el lado MCU. La ROM que
ejecuta el Z80 emulado es la misma que corre en el hardware real (o debería
serlo — asegúrate de que el emulador use un `.ROM` reconstruido con `pasmo`
después de este cambio), así que el aparcado en páginas, la copia final al
destino real y el stub de restauración de registros los hace **la propia
ROM**, no el emulador. El emulador solo tiene que contestar al comando 0x46
exactamente igual que el STM32.

## Por qué existe

`LOAD *Z81 "fichero"` restaura un snapshot completo (registros + memoria) en
formato `.Z81` de EightyOne. Sirve para saltarse en caliente el arranque de
programas antiguos que, si se dejan ejecutar desde el principio con el
SD81 Booster activo, corrompen la ROM extendida (p. ej. YMIR, que hace un
`LDIR` de 8K sin comprobar nada contra `$2000-$3FFF`, que es justo donde vive
la ROM extendida del interface). Capturando un snapshot en el punto justo
antes de que el programa haga eso, y cargándolo directamente, ese código
peligroso nunca se llega a ejecutar en el interface real.

## Lo que ve el Z80: protocolo del comando 70 (0x46)

Mismo handshake que el resto de comandos MCU (`OUT`/`IN` con toggle del bit 7
por cada byte), igual que `CMD_load`/0x09.

```
Z80 -> MCU:  70, longitud(1), nombre[longitud]         (igual que CMD_load)
MCU -> Z80:  dir_destino(2, LE), longitud_memoria(2, LE),
             bloque_registros(30 bytes, ver tabla abajo),
             memoria[longitud_memoria],
             NMI(1), WRX(1), generador_caracteres(1),
             status(1)
```

- `dir_destino`/`longitud_memoria` salen de la línea `MEMRANGE inicio fin` de
  la sección `[MEMORY]` del `.Z81` (`longitud = fin - inicio + 1`).
- `memoria[]` son los bytes **ya decodificados** (el RLE `*NNNN VV` del
  formato de texto se expande en el propio comando MCU, el Z80 solo ve bytes
  planos).
- `NMI`: de `[ZX81]`/`NMI` (0/1). `WRX`: de `[SD81BOOSTER]`/`WRX` (0/1).
  `generador_caracteres`: 0=64 (Sinclair), 1=128, 2=256, calculado a partir
  de `[SD81BOOSTER]`/`SEL128`/`SEL256`. Los tres valen **`0xFF`** si la
  sección de origen no aparece en el fichero (p. ej. un `.Z81` sin la
  extensión `[SD81BOOSTER]`) — la ROM no toca ese modo de hardware cuando ve
  `0xFF`, para no des-configurar nada en snapshots que no traigan esta
  información.
- `status`: 0 = ok. Distinto de 0 = error (fichero no encontrado, sección
  `[MEMORY]` ausente, o fichero truncado a medio volcado). **En cualquier
  caso hay que mandar la cabecera y los 30 bytes de registros igual** (a
  ceros si hace falta) antes del `status`, y también los tres bytes
  NMI/WRX/generador de caracteres (a `0xFF`) — para no dejar al Z80 esperando
  bytes que no van a llegar — el ROM real hace exactamente esto
  (`cmd_loadZ81`, rama `if (!have_header)`).

## El bloque de registros (30 bytes, orden fijo)

| Offset | Campo | Tamaño |
|---|---|---|
| 0 | PC | 2 (LE) |
| 2 | SP | 2 (LE) |
| 4 | HL | 2 (LE) |
| 6 | DE | 2 (LE) |
| 8 | BC | 2 (LE) |
| 10 | AF | 2 (LE) |
| 12 | HL' | 2 (LE) |
| 14 | DE' | 2 (LE) |
| 16 | BC' | 2 (LE) |
| 18 | AF' | 2 (LE) |
| 20 | IX | 2 (LE) |
| 22 | IY | 2 (LE) |
| 24 | I | 1 |
| 25 | R | 1 |
| 26 | IM | 1 (0/1/2) |
| 27 | IFF1 | 1 |
| 28 | IFF2 | 1 (no se restaura aparte, ver más abajo) |
| 29 | HALT | 1 (no se usa, ver más abajo) |

Estos valores salen tal cual de la sección `[CPU]` del `.Z81`
(`save_snap_zx81` en `Source/zx81/snap.cpp`, el propio código del emulador
que genera estos ficheros): claves `PC SP HL DE BC AF HL_ DE_ BC_ AF_ IX IY
IR IM IF1 IF2 HT`, todo en hex ASCII. El campo `IR` guardado ya trae R7
plegado en el byte bajo (`(I<<8)|(R7&128)|(R&127)`), así que el byte bajo de
`IR` es directamente el valor de `R` a mandar, sin más proceso.

## El fichero .Z81: formato de texto a parsear

Es texto ASCII con secciones `[TAG]` y pares `CLAVE valor` separados por
espacios/saltos de línea (ver `get_token`/`hex2dec`/`save_snap_zx81` en
`Source/zx81/snap.cpp` para la referencia canónica — el emulador ya sabe leer
y escribir este formato, así que reutilizar ese mismo parser es lo más
sencillo). Solo interesan dos secciones:

- **`[CPU]`**: los 17 campos de la tabla de arriba, un `CLAVE valor` por
  línea (pueden ir varios por línea, separados por espacios).
- **`[ZX81]`**: solo `NMI` (0/1). Viene **antes** que `[MEMORY]` en el
  fichero, así que se captura de paso en la misma pasada.
- **`[MEMORY]`**: `RAM_PACK`, `8K_RAM_ENABLED` y `ROM_PROTECTED` (ignorar),
  luego `MEMRANGE inicio fin`, y después el volcado: tokens hex de un byte, o
  `*NNNN VV` (repetir el byte `VV` `NNNN` veces, ambos en hex).
- **`[SD81BOOSTER]`** (extensión propia del interface, puede no estar
  presente): solo interesan `WRX` y `SEL128`/`SEL256` (0/1 cada uno), que
  vienen al principio de la sección. Viene **después** de `[MEMORY]`, y va
  seguida de bloques `RAM_PAGE nn ... RAM_PAGE_END` con volcados de página
  completos que no hacen falta para este comando — hay que parar de
  tokenizar en cuanto se vea el primer `RAM_PAGE` (o antes, si ya se
  encontraron `WRX`/`SEL128`/`SEL256`), para no perder tiempo con esos
  volcados.

El resto de secciones (`[INTERFACES]`, `[SOUND]`, `[COLOUR]`,
`[CHR$_GENERATOR]`, `[HIGH_RESOLUTION]`, `[JOYSTICK]`...) describen
configuración del emulador o quedan obsoletas por `[SD81BOOSTER]`, y se
ignoran.

## Comportamientos que hay que reproducir sí o sí

- [ ] **Nunca dejar al Z80 colgado esperando bytes.** Si el fichero no abre,
      o no tiene `[MEMORY]`/`MEMRANGE`, o se acaba a medio volcado: completar
      igualmente la cabecera + registros (a cero), NMI/WRX/generador de
      caracteres (a `0xFF`) y mandar un `status` distinto de 0 al final.
- [ ] **El `status` va SIEMPRE el último**, después de NMI/WRX/generador de
      caracteres, que a su vez van después de toda la memoria — igual que en
      `CMD_load`/`ReportStatus`.
- [ ] **No hace falta tocar el bloque 1 ni ninguna página con cuidado**: eso
      es cosa de la ROM (que se ejecuta igual en el emulador). El emulador
      solo entrega bytes; puede escribirlos donde le convenga internamente
      (o ni siquiera "escribirlos" hasta que la ROM se los pida, si el
      emulador prefiere ir generándolos al vuelo desde el fichero, como hace
      el propio `cmd_loadZ81` en el STM32 real para no tener que cargar los
      56K en RAM del microcontrolador — el emulador no tiene esa limitación
      de memoria, así que puede simplemente decodificar el fichero entero de
      una vez si es más cómodo).

## Estado

| Pieza | Estado |
|---|---|
| `LOAD *Z81` en la ROM (`sdhandler.inc.asm`) | implementado y **probado con éxito** (registros + memoria, vía reasignación de página) contra el emulador (`YMirorg.z81`) |
| `cmd_loadZ81` en el STM32 (`COMMANDS.cpp`) | implementado, incluye NMI/WRX/generador de caracteres; **sin compilar/probar en hardware real** (sí contra el emulador) |
| Comando 0x46 en el emulador (`SD81Booster.cpp`) | ya funciona para cabecera+registros+memoria; **pendiente** de mandar los 3 bytes nuevos NMI/WRX/generador de caracteres si el emulador todavía no lo hace |

Nota: la carga de memoria dejó de ser una copia byte a byte -- la ROM
reasigna páginas directamente (ver el propio `sdhandler.inc.asm`,
`Z81DoRestore`), así que si el emulador alguna vez generó un `.Z81` con la
sección `[MEMORY]` vacía/trivial para snapshots con paginación propia del
interface (visto en pruebas reales: `MEMRANGE` a puro `$FF` mientras los
datos de verdad estaban en `[SD81BOOSTER]`/`RAM_PAGE`), ese es un bug del
propio guardado del emulador, no de este protocolo — `[MEMORY]`/`MEMRANGE`
debe reflejar siempre el contenido real de los bloques 1-7.
