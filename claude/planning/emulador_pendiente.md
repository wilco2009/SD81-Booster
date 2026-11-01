# Lo que queda para el emulador (12 de octubre de 2026)

Resumen de todo lo que ha cambiado en el SD81 Booster desde `hw_debugger_emulator.md` (4 de octubre,
FPGA 0.13) y que el emulador tiene que imitar. El detalle de cada punto está en la sección o el
documento que se indica; la referencia de cómo encaja todo es `claude/dbgharness/cosim.cpp`
(`sh build.sh`), que tiene un modelo de la FPGA con todos estos cambios.

Lo que no está probado todavía en el hardware se marca con **(sin probar)**.

**Sobre los números "0.NN":** los "FPGA 0.14", "rev 0.15"... de estos documentos son la **revisión
interna del módulo del depurador** (`sim_int.v`, su cabecera de comentarios), no la versión de la FPGA
que ve el usuario. Esa está en `FPGA/SD81V2.1000/FPGAVERSION.TXT` (hoy **1.5**, la de la release
v1.6.0) y la subirá la próxima release.

## Estado (12 de octubre de 2026)

**El emulador ya tiene todo lo de este documento excepto el teclado virtual (punto 5); el modo 160x200 a 4 bpp (punto 13) se implementó el 1/11/2026 y falta probarlo.** Los puntos 9 a 12 de la sección 6 se hicieron el 26-28 de octubre.

El emulador no tiene acceso al hardware real, así que su fuente se ha migrado para que **use los
fuentes del hardware real en la medida de lo posible** (el firmware tal cual, no una copia
reescrita). Con eso se han probado, con cierta fidelidad, `NET_MQTT.cpp` (los comandos AT de la red,
sección 5) y la corrección de los snapshots con CP/M (puntos 4, 7 y 8). Es una prueba en el
emulador, **no en el hardware**: la FPGA 0.15 y el firmware nuevos siguen sin probarse en la placa
(el depurador dentro de CP/M, el snapshot de CP/M y la red con un broker real desde el ESP32).

Lo que sigue es la lista completa, para consulta.

## 1. FPGA: lo que el emulador tiene que emular

| # | Qué | Dónde está descrito | Cómo se comprueba |
|---|---|---|---|
| 1 | **Sprites por línea y 64 sprites.** Los sprites se leen por línea de la copia de la sombra (no de registros). Máximo **12 por línea**; si hay más, se pierden los de índice más bajo. Segunda tabla de 32 sprites en `$1800-$19FF` de la sombra (la primera, `$0C00-$0FFF`). `LOAD *SPRITE` hasta el 63. No hay sprites en los modos de 70 y 80 columnas | `sprites_por_linea_emulador.md`; `hw_debugger_emulator.md` 11.4 | prueba de sprites del arnés; `EXAMPLES/SPRDBUF` |
| 2 | **Índices 9, 10 y 11 de `$3FEF`** (FPGA 0.14): estado del vídeo (bit 0 Superfast, 1 HiRes, 2 Spectrum, 3 ancho, 4 caracteres de 7 píxeles, 5 D_FILE alternativo activo, 6 atributos alternativos activos, 7 doble buffer), bloque de delante del doble buffer, y `C4h` como marca. Hay que dar lo que de verdad hay activo, no lo último que dejó un POKE | `hw_debugger_emulator.md` tabla de índices y 13.11 | `tb_dbg.v`, arnés (`vid_vs`) |
| 3 | **Índice 12 de `$3FEF`** (FPGA 0.15): `A8h` \| 1 si el bloque 0 es RAM (`POKE 2056`, CP/M; el reset lo borra) | `hw_debugger_emulator.md` 13.12 | `tb_dbg.v` |
| 3b | **Índices 13 y 14 de `$3FEF`** (FPGA 0.17, diagnóstico): 13 = los registros de configuración (bit 0 MC45, 1 `nMODE48K`, 2 `nQS_en`, 3 `FULL_PAGING`, 4 128C, 5 256C, 6 ROMLOCK, 7 monitor cargado); 14 = byte bajo del registro de la última orden (bits 3-0 orden, bit 4 valor). Antes daban 0 | `hw_debugger_emulator.md` tabla de índices | `tb_dbg.v` |
| 4b | **FPGA 0.16: con la orden 10 (`dbg_poke`) el bloque 0 vuelve a ser ROM aunque CP/M lo haya puesto en RAM**: `blk0_ram = (block0Writable & ~dbg_poke) \| dbg_mon_ram`. Los POKEs de control actúan como puertos, la sombra los apunta, y las escrituras en `$0000-$1FFF` no tocan la RAM de CP/M. Sin esto la pantalla del depurador salía con basura en CP/M (lo encontró la sesión del emulador) | `hw_debugger_emulator.md` 13.12 | arnés (bloque 7c) |
| 4 | **Con el bloque 0 en RAM, la CPU no escribe en la sombra de `$0000`-`$1FFF`** (FPGA 0.15). La memoria sí cambia. La traza, `BRAMW` y el doble buffer siguen escribiendo la sombra. Antes CP/M pisaba ahí la fuente de la ROM (la que lee la pantalla del depurador), los POKEs y los sprites | `hw_debugger_emulator.md` 13.12 | arnés (bloque 7c) |
| 5 | **Teclado virtual**: orden 13 del canal de configuración, 8 filas × 5 bits que se suman (a 0) a las lecturas del puerto `$FE` junto con el teclado real y el joystick | `teclado_remoto_emulador.md` | `tb_vkeys.v` **(sin probar en hardware)** |

## 2. MCU: lo que el emulador imita de `DEBUGGER.cpp`

| # | Qué | Dónde |
|---|---|---|
| 6 | **`LOAD` ya no borra las páginas escritas** (antes lo hacía en `dbg_note_loaded`). Un programa de varias cargas (datos desde la SD) va sumando páginas y el snapshot tiene que guardarlas todas. Solo las borran el reset de la máquina y el empezar a cargar un snapshot | `hw_debugger_emulator.md` 11.5 |
| 7 | **El snapshot describe lo que se ve, no el historial de POKEs**: con los índices 9-11, `DISPLAY_MODE`, `WIDE_COLS`, `DISP_ADDR`, `ATTR_ADDR` y `DBUF` salen del estado real. Un `POKE 2045,85` apaga el D_FILE y los atributos alternativos, y un `2045,170` posterior no los enciende; la sombra de 2098 y 2061 sigue con el 170 viejo | `snapshots_cambios_emulador.md` sección 2 |
| 8 | **Bloque 0 en RAM en el snapshot**: si el índice 12 dice RAM, `HW_POKES` lleva el `POKE 2056` puesto aunque el valor de la sombra coincida con la ROM (el MCU toma por "nunca escrito" lo que iguala a la ROM). Sin el índice 12, como antes | `hw_debugger_emulator.md` 13.12 |

La captura de pantalla de la web (`cap`, `CMD_DBG_CAP`), el teclado de la web (`CMD_DBG_KEYS`) y el
la web del módulo WiFi **no son cosa del emulador** (el módem de red, sí: sección 5).

## 3. Ficheros que hay que volver a sincronizar

El emulador mantiene copias del firmware. Desde la última sincronización han cambiado en el firmware:

| Fichero | Cambio |
|---|---|
| `voice_synthesizer.cpp` | `voice_synthesizer_isr()` devuelve la muestra mientras suena. Antes no tenía `return` (comportamiento indefinido): en ARM funcionaba por casualidad y en el PC se desbocaba. **Es lo que bloquea probar `SAY`** |
| `WAV.cpp` | las muestras de 32 bits por muestra usaban `x >> 24 + 128` (se evalúa como `x >> 152`); ahora `(x >> 24) + 128`. Solo afecta a WAV de 32 bits |
| `DEBUGGER.cpp` | los puntos 6, 7 y 8 |
| ROM (`z80rom`) | los sprites se ponen a cero en el reset (ahora 64) y PLOT/TAB funcionan en 70/80 columnas (ROM 1.7). La ROM sale de `/SYS/SDBOOST.ROM` de la SD |

## 4. Lo que el emulador ya había anunciado como suyo

- **Commit del sonido en el hilo de la MCU.** `PLAY`, VGM y PEG están probados. Falta `SAY`, que
  depende del arreglo de `voice_synthesizer.cpp` de arriba. Se puede commitear ya con `SAY` pendiente
  o esperar a probarlo.
- **Retardo del audio**: los ~30 ms de la cola del `PLAY` se pueden bajar si se nota en efectos sueltos.

## 5. La red: comandos AT nuevos del módem

Son parte de lo que el emulador tiene que ofrecer: los programas de red (el juego por turnos y
cualquier otro de publicar / suscribir) los usan por el mismo canal del módem que `EXAMPLES/TELNET`
(`NET_READ` / `NET_WRITE`, el intérprete AT de `NET_BRIDGE.cpp`). Sintaxis completa, errores y
decisiones: `juego_en_red.md` §3. Implementación de referencia: `Arduino/Wifi_module_01/NET_MQTT.cpp`
y `mqtt_codec.h` (un cliente MQTT 3.1.1 sin librerías). Pruebas de referencia con un broker falso:
`claude/mqtttest` (`sh build.sh`).

| Comando | Qué hace |
|---|---|
| `AT+MQTT=host,puerto[,tls[,usuario,clave]]` | conecta a ese broker |
| `AT+MQTT` | conecta al primero que funcione de `/SYS/NET.CFG` (líneas `broker host puerto [tls]`; si falta, `test.mosquitto.org`, `broker.emqx.io` y `broker.hivemq.com`, puerto 1883) |
| `AT+MQTTID=cadena` | clientid (por defecto `sd81-` y 8 hexadecimales aleatorios) |
| `AT+MQTTWILL=topic,retain,texto` | *last will*; antes de conectar |
| `AT+MQTTSUB=topic[,qos]` / `AT+MQTTUNSUB=topic` | suscribe (espera el SUBACK; `ERROR` si el broker lo rechaza) o cancela; admite `+` y `#` |
| `AT+MQTTPUB=topic,qos,retain,len` | contesta `>` y a continuación el Z80 manda `len` bytes crudos (1-120); con `len` 0 publica vacío sin pedir datos. QoS 1 espera el PUBACK |
| `AT+MQTTRX` | saca un mensaje de la cola: `+MQTTRX:topic,len`, los bytes, CR LF y `OK`; `OK` solo si no hay |
| `AT+MQTT?` | `+MQTT:estado,broker,en_cola,perdidos,error` (estado: 0 apagado, 1 conectando, 2 conectado, 3 reconectando) |
| `AT+MQTTCLOSE` | cierra la sesión y olvida suscripciones, cola, will, clientid y usuario |
| `AT+SHA256=len` | tras `>` y `len` bytes: `+SHA256:` y 64 caracteres hexadecimales, `OK` |
| `AT+HMAC=keylen,datalen` | tras `>` y la clave seguida de los datos: `+HMAC:` y 64 caracteres hexadecimales (HMAC-SHA256), `OK` |
| `AT+RAND=n` | `+RAND:` y `n` bytes (1-32) en hexadecimal, `OK` |

Reglas que el emulador tiene que respetar:

- **Las mayúsculas se conservan** en los comandos `AT+`: el intérprete pasaba toda la línea a
  mayúsculas, y los topics, las claves y los nombres distinguen. El resto (`ATDT`, `ATH`...) sigue igual.
- **Datos crudos tras `>`:** sin eco y sin interpretar, `len` bytes exactos; si el Z80 no los manda en
  5 s, `ERROR`. Hasta 256 bytes en `SHA256` y `HMAC`.
- **Recepción por sondeo** (`AT+MQTTRX`), nunca mensajes no solicitados. Cola de **8** mensajes de
  hasta 120 bytes y topic de hasta 64; si se llena, se pierde el **más antiguo** y suma a `perdidos`.
  Un mensaje mayor se descarta y suma a `perdidos`. Si en la cola de salida del módem (256 bytes) no
  cabe la respuesta, `ERROR` y el mensaje sigue en la cola.
- **Una sesión MQTT a la vez,** independiente del socket TCP de `ATDT`. `ATH` cuelga el TCP,
  `AT+MQTTCLOSE` el MQTT y `ATZ` los dos.
- **Reconexión automática** con las suscripciones (estado 3 mientras reintenta). QoS 0 y 1;
  retained; *last will*.
- **Errores** (último campo de `AT+MQTT?`, el último que ocurrió; se borra al conectar): 1 sin red,
  2 no abre el TCP, 3 el broker rechaza, 4 tiempo agotado, 5 argumentos malos, 6 cola o búfer lleno,
  7 sin sesión, 8 sin PUBACK.
- El emulador puede hablar MQTT de verdad por los sockets del PC, o contra un broker simulado. Lo
  que tiene que ver el Z80 es lo mismo.

## 6. Novedades de la rama `fpga-mode160` (28 de octubre de 2026)

Lo que ha entrado desde la lista de arriba (12 de octubre). Cada punto tiene su propio documento con el detalle. **Estado (1/11/2026): los puntos 9 a 12 están hechos en el emulador y probados por el usuario; queda el 13 (y el 5, el teclado virtual).**

| # | Qué | Documento | Estado |
|---|---|---|---|
| 9 | **Hardware de scroll** (HiRes nativo, Spectrum, motor vertical): fino Y, filas superior e inferior, columna 32 real | `scroll_hardware_emulador.md` | ✅ **hecho en el emulador** (26-27/10/2026, con el motor vertical); probado en el emulador, falta compararlo con la placa |
| 10 | **Modo 160x100 a 256 colores** (`POKE 2045,175`): paleta de 256 (`POKE 2063/2064/2065`), posición (`POKE 2099`), salida PDM de 4 pines | `mode160_emulador.md` | probado en la placa (27/10). ✅ **Hecho en el emulador** (28/10/2026) y probado |
| 11 | **Paleta de 16 colores** para los modos normales y los sprites (`POKE 2066`, `2067`): índice `{BRIGHT,G,R,B}`, desactivada = colores de siempre | `mode160_emulador.md` §3 | sintetizada, **sin probar en la placa**. ✅ **Hecha en el emulador** (28/10/2026) y probada en él |
| 12 | **Comando 76 del MCU**: un `.BMP` abierto como `.I16` (escala a 160x100 y 256 colores), con un manejador virtual en fseek / fread / fstat / fclose | `bmp_emulador.md` | ✅ **Hecho en el emulador** (28/10/2026): `BMP.cpp` y el firmware real copiados tal cual; probado en el emulador (el firmware sigue sin probar en la placa) |
| 13 | **Modo 160x200 a 4 bpp con ventana de imagen, sprites y texto 40x25** (`POKE 2045,176`, `2068-2071`): 4 bpp por `pal16`, filas de texto con imagen `[win_first, win_last)`, sprites 8x8 sobre la imagen, plano de texto con glifos de `ROMTABLE` | `mode160_emulador.md` §5 | sintetizado (rama `fpga-mode160-4bpp`), **sin probar en la placa**; emulador: ✅ **HECHO el 1/11/2026** (`RenderMode160x4`, línea `MODE4` en las instantáneas), pendiente de probar con `MODE160W`/`mode1604` |

Qué no es del emulador de esto: los sprites en el 8 bpp y los modos mixtos de `modos_160_sprites_mixto.md` siguen siendo un estudio; lo que sí hay de sprites y texto con el 4 bpp es el punto 13.
`FPGAVERSION.TXT` sigue en 1.5 y la subirá la próxima release; los ficheros de la FPGA de esta rama (`SD81.bit`, `sd81boster.mcs`) tampoco llevan la versión estampada.

## Orden sugerido (hecho, salvo el último)

1. Sincronizar `voice_synthesizer.cpp`, `WAV.cpp` y la ROM, y probar `SAY` (desbloquea el commit del sonido).
2. Puntos 2-4 (índices 9-12 y la regla del bloque 0): son pocas líneas y los usa el snapshot.
3. Puntos 6-8 (el snapshot).
4. Punto 1 (los 12 sprites por línea y los 64): es lo más grande.
5. La red (§5): los comandos del módem, que necesita cualquier programa de red.
6. Punto 5 (el teclado virtual), cuando se haya probado en el hardware.
7. Puntos 10-12 (el modo 160x100, la paleta de 16 y el comando 76): ✅ hechos. El 13 (el modo 160x200 a 4 bpp): ✅ hecho el 1/11/2026, a falta de probarlo.
