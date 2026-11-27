# Teclado y gamepad BLE: plan de implementación (26/11/2026)

Base: el estudio `teclado_gamepad_bluetooth.md` (latencia, límites del C3, traducción del teclado, rollover) y lo que ya funciona del ratón (`raton_bluetooth.md`).
Material del usuario: **Logitech Pebble Keys 2 K380s** (BLE según Logitech) y **mando Amazon Luna** (protocolo **sin confirmar**: Amazon no dice si es BLE o clásico;
lo decide la fase 0). Si el Luna resultara clásico, el lado del gamepad sigue sirviendo para el puente propio `Arduino/Gamepad_BLE_megadrive` (mando Mega Drive + C3).

## Decisiones (por defecto; se pueden cambiar antes de la fase de la ROM)

| Pregunta del estudio (§7) | Decisión |
|---|---|
| Distribución de los signos | **española** por defecto (Pebble comprado en España), con la inglesa (US) como opción; en `KEYMAP_ZX81`, elegida con un byte |
| Comandos BASIC | **separados y como `LOAD *MOUSE`**: `LOAD *KEYB` y `LOAD *PAD`, con `THEN NEW/RUN/CLEAR/STOP`, `STOP` y `TO A$` (fase 5) |
| ¿Analógico? | sí, pero **sin FPGA**: el gamepad se lee con el comando 78 (op 0). El puerto de la FPGA (índice 21 de `$3FEF`) queda descartado salvo que haga falta |
| Gamepad como teclas | **activado por defecto**, con la configuración de `LOAD *JOY` (cruceta o stick izquierdo → las 4 direcciones; A → fuego) |
| Comando del MCU | **nuevo, el 78** (no ampliar el 77: añadir un byte rompería el handshake de los programas que ya lo usan) |
| Cambio de modo texto/juego del teclado | **Ctrl+Alt+Espacio** alterna (el Pebble no tiene Bloq Despl), y por BASIC (comando 78 op 6) |

## Contratos comunes (ya escritos; las sesiones NO los cambian sin avisar)

### Tramas ESP32 → STM32: `WIFI_PROTOCOL.h` (las dos copias, idénticas)

Hecho el 26/11/2026: `CMD_KEYB = 0x17`, `CMD_PAD = 0x18` y la ampliación de `CMD_BLE` (req de 9 B con el estado de los tres dispositivos y la clave; resp de 4 B con el
dispositivo). El formato exacto está en los comentarios del propio fichero. Resumen:
- `CMD_KEYB`: filas[8] + flags = 9 B; al cambiar y cada 100 ms; el STM32 la suelta a los 300 ms sin trama.
- `CMD_PAD`: botones(2B) + cruceta(1B) + lx, ly, rx, ry (con signo) + lt, rt + flags = 10 B (corregido el 26/11: ponía 12); al cambiar y cada 100 ms; soltar a los 300 ms.
- `CMD_BLE`: compatible en los dos sentidos con el formato de hoy (un STM32 viejo lee solo los 2 primeros bytes; un ESP32 viejo ignora el 4.º de la respuesta).
  Órdenes nuevas para el teclado: 6 modo texto, 7 modo juego. Una orden pendiente por dispositivo.

### Comando 78 (0x4E) del MCU: `BTDEV` (teclado y gamepad)

Handshake idéntico al del 77 (`cmd_mouse`): `ToggleClock` de ACK, un `GetByteFromZ80_IT` por byte recibido con su `ToggleClock`, `SendByteToZ80` por byte enviado,
`reset_commands()` y el `ToggleClock` final. **Subir `LAST_COMMAND` a 78 en `COMMANDS.h`** (si no, el Z80 se cuelga).

| op | Z80 → MCU | MCU → Z80 |
|---|---|---|
| 0 PAD LEER | — | botones lo, botones hi, cruceta, lx, ly, rx, ry, lt, rt, estado (10 B). Estado: bit0 conectado (trama hace < 300 ms), bit1 datos nuevos desde la lectura anterior |
| 1 PAD TECLAS | n (1-16), n códigos | estado. Código de tecla para cada botón, del bit 0 al n-1, con la misma codificación que `LOAD *JOY` (`send_joycfg`) y 0 = ninguna. Por defecto: A y B = el fuego de `LOAD *JOY`, START = NEW LINE (la sesión del STM32 documenta cómo se codifica NEW LINE) |
| 2 PAD COMO TECLAS | 0/1 | estado (activado por defecto) |
| 3 BLE ORDEN | dispositivo (0 ratón, 1 teclado, 2 gamepad), orden, argumento | 0 aceptada / 0x80 no válida. Órdenes como las del 77 (1 pair, 2 connect, 3 forget, 4 on, 5 off) más 6/7 (modo del teclado) |
| 4 BLE ESTADO | dispositivo | estado, segundos (mismos bits que el op 4 del 77) |
| 5 BLE TEXTO | dispositivo | 16 caracteres ZX81 + estado = 0. Textos: CONECTADO, EMPAREJANDO 45S, **CLAVE 123456** (teclado esperando la clave), BUSCANDO, PARADO, SIN TECLADO / SIN MANDO, SIN MODULO WIFI |
| 6 TECLADO MODO | 0 texto / 1 juego | estado (deja la orden 6/7 para el ESP32) |
| otro | — | 0x80 |

El comando 77 sigue igual (ops 3-5 = ratón).

### `KEYMAP_ZX81` (ESP32, C++ puro, sin Arduino; se prueba en el PC)

```cpp
// Wifi_module_01/KEYMAP_ZX81.h
void    keymap_reset();                                   // todo suelto, cola vacía (al conectar/desconectar)
void    keymap_set_layout(uint8_t layout);                // 0 español (defecto), 1 US
void    keymap_set_mode(uint8_t mode);                    // 0 texto (defecto), 1 juego
uint8_t keymap_mode();
// Un informe del teclado: modificadores (byte 0 del informe boot: bit0 Ctrl izq... bit7 GUI der) y los códigos de uso (página 0x07) pulsados AHORA.
// El que lee el BLE convierte el formato NKRO (mapa de bits) a esta lista.
void    keymap_report(uint8_t modifiers, const uint8_t* keys, uint8_t nkeys, uint32_t now_ms);
// Avanza la cola del modo texto y deja en rows[8] la matriz que debe ver el ZX81 ahora (5 bits por fila, 1 = pulsada).
// Devuelve true si la matriz ha cambiado desde la llamada anterior. Llamarla cada ~5-10 ms y tras cada keymap_report.
bool    keymap_tick(uint32_t now_ms, uint8_t rows[8]);
```

Reglas (del estudio, §4): letras, cifras, espacio y Enter por **posición** (Mayús = SHIFT del ZX81, también con letras y cifras: SHIFT+1 = EDIT, etc.); los signos
del PC que no son letras ni cifras **por carácter** según la distribución (`"` → SHIFT+P, `;` → SHIFT+X, `+` → SHIFT+K, etc.); Retroceso → SHIFT+0, cursores →
SHIFT+5/6/7/8, Esc → espacio (BREAK). Teclas F (si llegan como página 0x07): F1 EDIT, F2 FUNCTION (SHIFT+NEW LINE), F3 GRAPHICS (SHIFT+9). Ctrl+Alt+Espacio alterna el modo.
**Modo texto**: cola; cada pulsación se mantiene `KM_HOLD_MS` = 60 ms y se suelta `KM_GAP_MS` = 40 ms antes de la siguiente; nunca dos teclas a la vez salvo SHIFT
(cola de 32; si se llena, se descarta lo nuevo). **Modo juego**: matriz tal cual, con pulsación mínima `KM_MIN_PRESS_MS` = 40 ms. Los tiempos, en `#define` para ajustarlos en la placa.

La correspondencia fila/columna de la matriz es la del ZX81 (fila 0: SHIFT Z X C V ... fila 7: SPACE . M N B), igual que el teclado virtual de la web.

## Fases

| Fase | Qué | Quién | Depende de |
|---|---|---|---|
| **0** | Con `Arduino/Gamepad_BLE_test` en un C3: ¿el Luna y el Pebble salen como HID BLE? Copiar la salida (lista del escaneo, *Report Map* en hex, unos informes pulsando cada botón/tecla) y si el emparejado pide clave | **usuario**, en la placa | — |
| **1** | ESP32: `MOUSE_BLE` → multidispositivo (ratón + teclado + gamepad), lectura de informes de teclado y gamepad, tramas `CMD_KEYB`/`CMD_PAD`, `CMD_BLE` ampliado, clave de emparejado, consola `esp ble kbd/pad ...` | sesión **S1** | contratos (hechos) |
| **2** | ESP32: `KEYMAP_ZX81.cpp/.h` + prueba en el PC | sesión **S2** | contratos (hechos) |
| **3** | STM32: teclado virtual por fuentes (OR), `CMD_KEYB`/`CMD_PAD`/`CMD_BLE`, gamepad → teclas con `LOAD *JOY`, comando 78 | sesión **S3** | contratos (hechos) |
| **4** | Prueba en la placa (usuario compila y graba los dos firmwares): el ratón sin regresiones; emparejar el teclado (clave en `esp ble`) y escribir BASIC en modo texto; un juego en modo juego; el mando con un juego por `LOAD *JOY` | usuario | 1, 2, 3 |
| **5** | ROM (`LOAD *KEYB`, `LOAD *PAD` en `z80rom/sdhandler.inc.asm`, como `CmdMOUSE`; bloque 0 intacto), `EXAMPLES/PADTEST` (lee el op 0 del 78), entrada en `manual_pendiente.md`, tabla de comandos de `CLAUDE.md`, nota para el emulador (`teclado_gamepad_emulador.md`) | sesión S4 | 4 |
| 6 | Ajustes tras la placa: tiempos del modo texto, tabla de botones del Luna, coexistencia WiFi/BLE con MQTT (pendiente 6 del ratón) | — | 4 |

S1, S2 y S3 van **en paralelo** en la misma carpeta: ficheros distintos, ninguna toca `WIFI_PROTOCOL.h` ni este plan, ninguna hace commit ni cambia de rama,
ninguna compila con arduino-cli (el usuario compila). S1 usa la API de `KEYMAP_ZX81.h` tal como está arriba aunque S2 no haya terminado.

## Riesgos conocidos

- **El Luna puede ser Bluetooth clásico** → fase 0 antes de la prueba en la placa.
- Tres conexiones y tres vínculos son el máximo del C3 (`MAX_CONNECTIONS = 3`, `MAX_BONDS = 3`); hoy `pair`/`forget` del ratón llaman a `ble_store_clear()`, que
  borra **todos** los vínculos: S1 lo cambia a borrar solo el del dispositivo.
- Botones del gamepad: el número de botón HID no es estándar entre mandos (el Xbox BLE usa 1 A, 2 B, 4 X, 5 Y, 7 LB, 8 RB, 11 View, 12 Menu, 14 L3, 15 R3). S1 deja una
  tabla por mando (por nombre); la del Luna se rellena con la salida de la fase 0.
- Tecla pegada si se pierde una trama: lo cubren el latido de 100 ms y el plazo de 300 ms del STM32.

## Fase 3: hecho (26/11/2026, sesión S3, STM32; sin compilar con arduino-cli, sin placa, sin commit)

Ficheros: `COMMS.cpp/.h` (teclado virtual por fuentes), `PAD.cpp/.h` (nuevos: gamepad, teclado BLE, gamepad como teclas), `MOUSE.cpp/.h` (estado BLE de los
tres dispositivos), `WIFI_HANDLER.cpp` (`CMD_KEYB`, `CMD_PAD`, `CMD_BLE` ampliado), `COMMANDS.cpp/.h` (comando 78 `cmd_btdev`, `LAST_COMMAND` = 78).

- **Teclado virtual por fuentes**: una matriz por fuente (`VK_SRC_WEB` 1,5 s, `VK_SRC_KEYB` 300 ms, `VK_SRC_PAD` 300 ms) y a la FPGA va la unión (OR), solo
  las filas que cambian. `vkeys_set()` (la web) es ahora `vkeys_set_src(VK_SRC_WEB, …)`: mismo comportamiento de antes.
- **`CMD_KEYB`** (≥ 9 B) y **`CMD_PAD`** (≥ 10 B): trama corta = no se contesta (que reintente). Con flags bit0 = 0 se suelta todo.
  **Ojo, contrato**: `WIFI_PROTOCOL.h` dice que `CMD_PAD` son 12 B, pero sus campos suman **10 B** (2+1+4+2+1). El STM32 lee los campos en el orden
  documentado (flags = byte 9) y acepta 10 o más; lo que sobre se ignora. Avisado a S1 en las notas compartidas.
- **`CMD_BLE`**: req de 2 B (ESP32 viejo: solo ratón; teclado y mando quedan «SIN TECLADO»/«SIN MANDO» y sus órdenes esperan) o de 9 B (tres
  estados y la clave). Respuesta siempre de 4 B. Una orden pendiente por dispositivo; se entrega una por trama, por turno entre dispositivos.
  Órdenes 6/7 solo para el teclado. El 77 (ops 3-5) sigue sobre el ratón sin cambios (su op 5 usa ahora `ble_text()`, mismo texto).
- **Clave**: el op 5 del 78 con el dispositivo 1 da `CLAVE 123456` mientras la clave del req sea ≠ 0, **antes** que CONECTADO: el ESP32 debe ponerla a 0 al
  acabar el emparejado.
- **Gamepad como teclas** (activado por defecto): cruceta o stick izquierdo (|eje| ≥ 64) → arriba/abajo/izquierda/derecha de `LOAD *JOY` (se lee `joy_cfg`
  en cada trama, así que sigue a `LOAD *JOY` si cambia); cada botón → su código.
- **Codificación de las teclas (op 1 del 78)**: el carácter ZX81 de `LOAD *JOY` con estas reglas: **0 = ninguna**; 1-63 la tecla de ese carácter (letras,
  cifras, **`<` = 19 = NEW LINE**, `*` = 23 = SHIFT, como ya hacía `LOAD *JOY`); el bit 7 no cuenta (128-191 = 0-63), así que el **ESPACIO va como 128**;
  **118** (CHR$ 118) también vale NEW LINE; **255 = el fuego de `LOAD *JOY`**. Otro código: el op contesta 0x80 y ese botón queda sin tecla.
  Por defecto A y B = 255, START = 19. `n = 0` vuelve a los de por defecto; los botones ≥ n no cambian.
- **Comando 78**: ops 0-6 como en la tabla del plan. «Estado» de los ops 1, 2 y 6: bit0 = mando (1, 2) o teclado (6) conectado, bit7 = parámetro no válido.
  Op 4 con dispositivo no válido: 0, 0; op 5: «SIN MODULO WIFI» y estado 0x80.
- RAM: ~70 B estáticos (matrices por fuente, estado del mando, 16 códigos, estado BLE ×3).

Probado: sintaxis con g++ en el PC (stubs de Arduino) y una prueba de comportamiento en el PC (OR de fuentes, plazos 300 ms / 1,5 s, cruceta y stick,
códigos 128/38/200, como teclas apagado, órdenes por turno y con ESP32 viejo, textos). **Sin probar**: compilación real con STM32duino, la placa, el
handshake del 78 con el Z80 (no hay ROM todavía: fase 5), el emulador (necesitará el comando 78).

## Fase 2: hecho (26/11/2026, sesión S2, ESP32; sin compilar con arduino-cli, sin placa, sin commit)

Ficheros: `Arduino/Wifi_module_01/KEYMAP_ZX81.h/.cpp` (C++ puro, sin Arduino ni memoria dinámica; ~100 B de RAM estática) y `claude/keymaptest/`
(`test_keymap.cpp` + `build.sh`). API **exactamente** la del contrato; solo se añaden `#define` (`KM_HOLD_MS` 60, `KM_GAP_MS` 40, `KM_MIN_PRESS_MS` 40,
`KM_QUEUE_LEN` 32, `KM_MAX_KEYS` 16, `KM_LAYOUT_ES/US`, `KM_MODE_TEXT/GAME`). Matriz comprobada contra `vkeys.v` y `ZXROWS` de `DEBUG_PAGE.h` (las mismas).

**Prueba en el PC** (`sh claude/keymaptest/build.sh`, g++ `-Wall -Wextra` sin avisos): **63 030 comprobaciones, 0 fallos**. Cubre cada letra y cifra con
y sin Mayús (y la fila/bit de Z, B, 6, Enter), los signos de las dos distribuciones, teclas especiales, Ctrl/Alt/GUI, `0xE1` en la lista, ErrorRollOver,
escritura rápida con solapes (4 textos × 5 velocidades, hasta 28 teclas en 28 ms: no se pierde ninguna y nunca hay dos teclas a la vez salvo SHIFT; cada
pulsación ≥ 60 ms y cada hueco ≥ 40 ms), cola llena (40 → salen 32), tecla mantenida, modo juego (varias a la vez, Mayús suelta, toques de 2-3 ms → 40 ms),
cambio de modo, reset y el valor de vuelta de `keymap_tick`.

Decisiones (todas documentadas en la cabecera de `KEYMAP_ZX81.cpp`):
- **Posición** para letras, cifras, Espacio y Enter, con Mayús = SHIFT siempre (Mayús+2 = AND, Mayús+Espacio = £, Mayús+Enter = FUNCTION). En la española
  `" $ ( ) = /` están en el PC sobre las cifras, así que **no salen por carácter**: se escriben en su sitio del ZX81 (Mayús+P, U, I, O, L, V) o el `/` en el
  numérico. Se eligió así (es lo que dice el plan) porque por carácter se perderían AND y TO, que solo están en Mayús+2/4.
- **Signos por carácter** (resto de teclas): `: ; ? / " ( ) $ = + - , < > *` a su SHIFT del ZX81, `.` al punto y **`^` → SHIFT+H (`**`, potencia)**.
  Sin equivalente (no hacen nada): `' ¡ ¿ ` ç ñ ´ ¨ º ª _ [ ] { } \ | ~`. Por tecla: española `'?` `¡¿` `` `^ `` `+*` `ç` `ñ` `´¨` `ºª` `,;` `.:` `-_` `<>`
  (ISO 0x64); US `-_ =+ [{ ]} \| ;: '" `~ ,< .> /?`; numérico `/ * - + .` en las dos.
- Retroceso **y Supr** → RUBOUT; Esc → SPACE (BREAK, sin SHIFT: SHIFT+SPACE es £); F1/F2/F3 → EDIT/FUNCTION/GRAPHICS; numérico: cifras y Enter sin SHIFT.
- **Con Ctrl, Alt, GUI o AltGr pulsados, en modo texto la tecla no escribe** (atajos del PC; lo de AltGr no está en el ZX81). En modo juego se ignoran.
- **Modo juego**: cursores → **5/6/7/8 sin SHIFT** (los juegos leen `INKEY$="5"`; con SHIFT darían el código del cursor); la Mayús del PC entra como
  SHIFT suelto; los signos llevan su SHIFT.
- **Autorrepetición**: aquí no se genera ninguna. El informe HID solo dice «sigue pulsada» y la ROM del ZX81 ya repite sola mientras ve la tecla: pasado el
  `KM_HOLD_MS`, la tecla que se está mostrando se **mantiene en la matriz mientras siga pulsada en el PC y la cola esté vacía**. En cuanto llega otra pulsación
  se suelta (hueco de 40 ms) y pasa la nueva; la anterior no vuelve aunque siga pulsada (manda la última, como en el PC). Así mantener Retroceso o un cursor
  repite al ritmo de la ROM.
- **Cambio de modo** (Ctrl+Alt+Espacio o `keymap_set_mode`): vacía la cola y las teclas que siguen pulsadas no cuentan hasta soltarlas (el Espacio del atajo
  no llega como BREAK). `keymap_set_mode` con el mismo modo no hace nada. `keymap_reset` no cambia modo ni distribución.
- Informe con ErrorRollOver (0x01): se ignora entero. Códigos 0xE0-0xE7 en la lista se toman como modificadores.

**Para S3 / fase 4 (riesgo, no tocado)**: SHIFT y su tecla salen juntos en la misma matriz, pero si el STM32 escribe en la FPGA solo las filas que cambian,
una a una, el ZX81 podría leer entre las dos escrituras la tecla sin SHIFT (`P` en vez de `"`). Si se ve en la placa: al pulsar, escribir la fila 0 primero;
al soltar, la fila 0 la última. **Sin probar**: compilación con el core del ESP32, la placa, y los tiempos 60/40 con la ROM real (fase 6).

## Fase 1: hecho (26/11/2026, sesión S1, ESP32; sin compilar con arduino-cli, sin placa, sin commit)

Ficheros: `Arduino/Wifi_module_01/MOUSE_BLE.cpp/.h` (se conservan el nombre, `mouse_ble_begin()`, `mouse_ble_console()` y el interruptor `ESP_MOUSE_BLE`,
así que `Wifi_module_01.ino` y `WIFI_CLIENT.cpp` no cambian) y la ayuda de `esp help` en `LOGCFG.cpp`. No se tocan `WIFI_PROTOCOL.h`, `KEYMAP_ZX81.*` ni el
STM32. Revisado a mano (llaves, paréntesis, orden de declaraciones, firmas de la librería BLE del core 3.3.10 con NimBLE).

- **Tres huecos** (ratón 0, teclado 1, mando 2), cada uno con su `BLEClient`, estado, ventana de emparejado y NVS. El ratón conserva su espacio y sus claves
  (`sd81mouse`: `ble`, `addr`, `name`): no se pierde el emparejado. Teclado y mando: `sd81kbd`, `sd81pad`. Nueva clave `ida` (dirección de identidad del
  vínculo, 7 B) por hueco, para olvidar solo el suyo; el teclado guarda además `lay` (distribución). Teclado y mando vienen **parados** de fábrica.
- **Una sola tarea BLE** (`ble_task`) escanea para todos los huecos que buscan y conecta de uno en uno. Clasificación: *appearance* (0x03C1 teclado, 0x03C2
  ratón, 0x03C3/0x03C4 mando); un anuncio de otro tipo no entra en el hueco. Si no trae *appearance*, tras conectar se miran las colecciones de aplicación
  del *Report Map* (0x02 ratón, 0x06/0x07 teclado, 0x04/0x05 mando): si dice otro tipo, se descarta, se borra el vínculo de ese intento y no se vuelve a
  probar en esa ventana. Un dispositivo guardado (o conectado) en un hueco no se acepta en otro. La regla del M240 (mismo nombre y 5 primeros bytes = el
  mismo) vale para los tres.
- **Vínculos**: ya no se llama a `ble_store_clear()`. `forget` borra solo los vínculos del hueco (por dirección guardada, identidad o dirección actual);
  `pair` borra los del hueco **y los huérfanos** (los que no son de ningún otro hueco), para que no se llene el almacén de 3 (si se llenase, la pila borraría
  el más antiguo, que podría ser el de otro hueco). Si la pila BLE aún no está arrancada (todo parado), el borrado queda pendiente y se hace justo al
  arrancarla, antes de conectar nada. `esp ble` dice cuántos vínculos hay (n de 3).
- **Seguridad**: bonding, sin MITM propio, conexiones seguras (como antes). Antes de cada conexión se fija la capacidad: **DisplayOnly** (`ESP_IO_CAP_OUT`)
  para el teclado; **sin E/S** (Just Works) para ratón y mando, como siempre. Clave aleatoria nueva en cada emparejado (`setPassKey(false)` +
  `regenPassKeyOnConnect`); el callback `onPassKeyNotify` la deja en el estado («CLAVE 123456: tecleala en el teclado y pulsa Enter», en `esp ble`) y en
  `CMD_BLE` (3 B); se pone a 0 en cuanto termina el emparejado, bien o mal. Dos arreglos frente a la librería: `BLESecurity::resetSecurity()` antes de cada
  conexión (su indicador «seguridad iniciada» es global: con otro dispositivo ya conectado no se iniciaría la seguridad de la nueva y `secureConnection()`
  esperaría para siempre) y no se llama a `secureConnection()` si la conexión ya está cifrada. Como `secureConnection()` no tiene plazo, `hid_task` corta la
  conexión si el emparejado no termina en 90 s (teclado) o 30 s (ratón, mando).
- **Teclado**: parser de la página 0x07 (modificadores 0xE0-0xE7, arrays de códigos tipo boot y mapas de bits NKRO, con o sin Report ID); respaldo al informe
  boot 0x2A22 (con Protocol Mode = 0). El callback de la notificación (tarea del host) solo convierte el informe a (modificadores, lista de usos) y lo pone en
  una cola de 16; **solo `hid_task` toca `KEYMAP_ZX81`** (no es reentrante). ErrorRollOver: informe ignorado. `hid_task` llama a `keymap_report()` con cada
  informe, a `keymap_tick()` tras cada uno y cada 5 ms (`BLE_KBD_TICK_MS`), y manda `CMD_KEYB` al cambiar la matriz y cada 100 ms (`BLE_HEARTBEAT_MS`): flags
  bit0 conectado, bit1 = `keymap_mode()` juego. Al conectar y al desconectar, `keymap_reset()`; al desconectar, una trama con todo suelto y flags 0 (hasta 5
  intentos). Modo y distribución pasan por `hid_task` (órdenes de la consola o de `CMD_BLE`).
- **Mando**: parser de ejes X/Y/Z/Rx/Ry/Rz, hat 0x39 (8 o 4 posiciones; fuera de rango = suelto), cruceta como botones (usos 0x90-0x93), botones 1-16 de la
  página 9 y gatillos de la página 2 (Freno = LT, Acelerador = RT). Elige el informe con más cosas. Stick derecho: Z/Rz si están los dos (Android, Xbox BLE),
  si no Rx/Ry; sin gatillos de la página 2, los ejes sobrantes hacen de gatillos. Ejes a -127..127 con sus límites lógicos (hacia abajo positivo), gatillos a
  0..255 y además bits 6/7 (LT/RT digital) si pasan de 100. Tablas de botones por nombre: **Xbox** (1 A, 2 B, 4 X, 5 Y, 7 LB, 8 RB, 11 View, 12 Menu,
  13 Home sin confirmar, 14 L3, 15 R3), **SD81 MD Pad** (el puente `Gamepad_BLE_megadrive`: 1 A, 2 B, 3 X, 4 START) y **por defecto** la numeración de
  Android (la del Xbox más 9/10 = LT/RT digital), que siguen muchos mandos. La del **Luna** queda comentada, pendiente de la fase 0. `esp ble` enseña qué ejes
  ha elegido y qué tabla. `CMD_PAD` al cambiar y cada 100 ms; al desconectar, una con todo a 0 y flags 0.
- **`CMD_PAD` va con 10 B** (los campos del contrato: botones 2, cruceta 1, 4 ejes, 2 gatillos, flags), no 12: el total «12 B» de `WIFI_PROTOCOL.h` y de este
  plan no cuadra con sus campos (lo vio también S3; el STM32 acepta ≥ 10). **Falta corregir el «= 12 B» en `WIFI_PROTOCOL.h`** (las dos copias).
- **`CMD_BLE`**: req de 9 B (estado y segundos de los tres + clave), respuesta de 4 B; con 3 B (STM32 viejo) = dispositivo 0. Órdenes 1-5 por dispositivo y
  6/7 (modo del teclado). Se manda al cambiar algo (estado de cualquiera o la clave) y como mucho cada 0,5 s; si llega una orden, se pregunta otra vez
  enseguida (hasta 4 seguidas), porque el STM32 entrega una por trama.
- **Consola**: `esp ble` (los tres, los HID vistos y los vínculos), `esp ble [mouse|kbd|pad] pair [s]|connect|forget|on|off` (sin dispositivo = ratón, como
  antes), `esp ble kbd text|game`, `esp ble kbd layout es|us` (se guarda), `esp ble reset`. También valen `raton`, `teclado`, `mando`.
- **Radio**: si ya hay algo conectado y nadie está emparejando, las búsquedas de los que faltan son más cortas (ventana 30 de 100) y más espaciadas (8 s en
  vez de 2,5 s), para no quitar radio al que está conectado. Con solo el ratón activado, todo es como antes.

**Sin probar** (nada compilado ni probado en la placa): la compilación con el core del ESP32 (la hace el usuario); el emparejado con clave del Pebble Keys 2
(¿la pide?, ¿sale bien?); el Luna (fase 0); dos y tres conexiones a la vez con el WiFi; la memoria con las tres conectadas.

**Qué mirar en la placa (fase 4)**:
1. **Ratón sin regresiones** (primero, con teclado y mando parados): que se reconecte solo al arrancar sin volver a emparejarlo (mismas claves de la NVS),
   latencia en `esp ble` como antes, `LOAD *MOUSE THEN NEW/RUN/CLEAR/STOP`. Tras la primera conexión con este firmware, el ratón guarda su `ida`.
2. `esp ble kbd pair` y el Pebble en modo emparejado: si pide clave, `esp ble` la enseña («CLAVE …») y el op 5 del 78 también; teclearla y Enter. Después
   `esp ble` debe decir «conectado … (informes)» y el mapa (casillas de código / bits NKRO). Escribir BASIC en modo texto; Ctrl+Alt+Espacio y un juego en
   modo juego. Comprobar que el ratón sigue conectado mientras tanto y que «vinculos guardados» pasa a 2.
3. `esp ble pad pair` con el mando: `esp ble` dice qué ejes y qué tabla ha elegido; pulsar cada botón y mirar los bits («botones XXXX»). Si salen cambiados,
   se añade la tabla de ese mando en `PAD_TABLES` (por nombre).
4. `esp ble kbd forget`: el ratón y el mando siguen emparejados (vínculos 2) y se reconectan tras reiniciar.
5. `esp diag` / memoria libre con los tres conectados y la web abierta.

## Integración (26/11/2026, sesión principal)

- `CMD_PAD` = **10 B** corregido en las dos copias de `WIFI_PROTOCOL.h` (siguen idénticas) y arriba en el plan. ESP32 y STM32 ya usaban 10.
- Riesgo del SHIFT visto por S2, resuelto en el STM32 (`COMMS.cpp`, `vkeys_update`): al pulsar SHIFT la fila 0 se escribe la primera y al soltarlo la última,
  así el ZX81 nunca ve la tecla sin su SHIFT entre dos escrituras (como mucho, SHIFT solo, que la ROM ignora).
- API de `KEYMAP_ZX81.h` comprobada contra las llamadas de `MOUSE_BLE.cpp`: coinciden.
- Siguiente: fase 0 y fase 4 en la placa (usuario); con eso, la fase 5 (ROM, `EXAMPLES/PADTEST`, manual, emulador).

**Primera prueba en la placa (26/11/2026)**: el Pebble Keys 2 sale en nRF Connect (es BLE) y al emparejar pide clave; `esp ble kbd` la enseña, pero al
teclearla no se empareja. Causa sin confirmar. Lo más probable es el plazo del protocolo (30 s desde que se enseña la clave) o que se tecleara la clave de un
intento anterior: cada reintento generaba otra. Cambios (sin compilar ni probar):
- La clave es **la misma en todos los intentos de una ventana** (`regenPassKeyOnConnect(false)`), y cada `esp ble kbd pair` genera una nueva.
- `esp ble kbd` da los segundos que quedan para teclearla («quedan N s», contados desde que se enseñó) y una línea **«ultimo emparejado: …»** que no se
  borra al volver a buscar: «la clave no coincide», «el teclado cancelo o no acepto la clave», «se acabo el tiempo (30 s)», «error 0x…», «se desconecto
  (motivo 0x…)» o «bien», con los segundos desde que se enseñó la clave. Sale también en el log («BLE teclado: emparejado: …»). Los motivos se leen con un
  *listener* de eventos GAP de NimBLE (`ble_gap_event_listener_register`), que ve el resultado del cifrado y el de la desconexión de esa conexión.

**Segunda prueba (26/11/2026, firmware de la fase 1 sin los cambios de arriba)**: el emparejado con clave **sí funciona** («ultimo emparejado: bien: cifrado
con clave, 24 s despues de ensenar la clave»), pero el teclado no llega a quedar conectado («conexiones: 0») y se reintenta con otra clave. El fallo está
después del cifrado: servicio HID, lectura del *Report Map* o suscripción a los informes. Por qué, sin saber. Además, mientras el teclado se reintentaba
dejó de ir la transferencia de ficheros por la web; tras reiniciar, va. Lo más probable: las búsquedas BLE continuas quitan radio al WiFi (el C3 comparte
antena). Pendiente de confirmar y, si es eso, de limitar.
- Cambio (sin compilar): `esp ble kbd` (y `mouse`/`pad`) enseña un **historial** con los 10 últimos estados del dispositivo y el segundo en que se
  pusieron, para ver en qué paso falla la conexión. `esp ble` a secas no lo enseña, para no alargarlo.

**Tercera prueba (26/11/2026, con el historial)**: tras enseñar la clave, el **ESP32 se reinicia** (el historial vuelve a empezar a los 10 s del arranque).
En la prueba anterior no se reinició: el emparejado acabó bien y falló después. Causa desconocida. Candidatos: caída de tensión al usar BLE y WiFi a la vez,
falta de memoria al descubrir el GATT del teclado (con el ratón ya conectado) o un fallo en lo que corre tras cifrar.
- Cambio (sin compilar): el historial pasa a una memoria RTC que **sobrevive al reinicio** (`RTC_NOINIT_ATTR`, 24 entradas de los tres dispositivos; se
  pierde al quitar la alimentación). `esp ble kbd` marca las entradas de arranques anteriores («(1 arranque antes)»). `esp ble` y `esp ble kbd` dicen además
  el **motivo del último reinicio** (`esp_reset_reason()`: fallo del programa, perro guardián, caída de tensión, encendido...).

**Cuarta prueba (26/11/2026)**: motivo del reinicio = **FALLO DEL PROGRAMA** (`ESP_RST_PANIC`). Ocurre después de enseñar la clave y antes del siguiente
estado del teclado, es decir, durante el emparejado o en lo que corre tras cifrar. El core del C3 guarda un volcado del fallo en la partición `coredump`
(0x3F0000).
- Cambio (sin compilar): `esp ble crash` lee el resumen del volcado (`esp_core_dump_get_summary`): tarea, PC, RA, SP, mcause, mtval, a0-a7, los 9 primeros
  caracteres del SHA del .elf y las palabras de la pila que parecen direcciones de código. `esp ble crash clear` lo borra. `esp ble` saca una línea si el
  último reinicio fue un fallo. Con el .elf del firmware que falló, `riscv32-esp-elf-addr2line -pfiaC -e <elf> <direcciones>` da las líneas.
- El .elf del firmware que falló (compilado a las 17:37, SHA 90f491e8f…) está copiado fuera de `build`, en el scratchpad de la sesión S1: al recompilar se
  sobrescribe el de `build`. El volcado sigue en la flash tras grabar el firmware nuevo, así que no hace falta repetir el fallo.

**Quinta prueba: causa encontrada (26/11/2026)**. `esp ble crash`: tarea `nimble_host`, `abort()` (assert). Pila traducida con el .elf (19:31, SHA dd88f18ac):
`ble_att_clt_rx_find_info` → `BLERemoteCharacteristic::descriptorDiscCB` → `new BLERemoteDescriptor` → `FreeRTOS::Semaphore` → `xSemaphoreCreateBinary()`
devuelve NULL y `xSemaphoreGive(NULL)` salta el assert. Es decir, **falta de RAM** al descubrir los descriptores del Pebble con el ratón ya conectado (más
WiFi y web): la librería BLE crea dos o tres semáforos por cada característica y descriptor y no comprueba el malloc. Cambios (sin compilar):
- Antes de conectar un 2.º o 3.er dispositivo se mira la RAM: con menos de `BLE_MIN_FREE_HEAP` (30 000 B) libres o un bloque libre de menos de
  `BLE_MIN_FREE_BLOCK` (8 KB), no se intenta y el estado dice «POCA MEMORIA: N B libres (bloque mayor M)». El ratón solo, como siempre.
- `esp ble` enseña la memoria libre, la mínima desde el arranque y el bloque mayor, y cada dispositivo conectado dice cuánta RAM se llevó al conectar.
- Menos RAM propia: la pila de `hid_task` baja de 6144 a 4096 B, y los búferes de los parsers de teclado y mando (antes ~3,3 KB estáticos) se piden solo
  mientras se analiza el descriptor.
- Pendiente según los números de la placa: si con el ratón conectado no llega a 30 KB libres, hay que buscar RAM en otra parte del ESP32 (web, MQTT, pilas
  de las tareas) o ajustar el umbral con lo que se lleve de verdad el Pebble.

**Sexta prueba (26/11/2026, firmware con el umbral de memoria)**: otro `abort()` en `nimble_host`, ahora `std::bad_alloc` en `operator new` al copiar un
anuncio en `AdvCb::onResult` durante una búsqueda. Dos fallos de memoria en sitios distintos: el montón libre real va al límite. El usuario duda (el
compilador dice 270 KB libres), pero esa cifra no cuenta lo que reservan el WiFi y NimBLE al arrancar. Falta el número real: lo da `esp ble`
(«memoria libre») desde el firmware anterior. Cambios (sin compilar):
- Cada línea del historial persistente lleva la memoria libre y la mínima en KB («[libre/minima KB]»). `ble_task` añade una línea «baja la memoria libre»
  cuando cae 4 KB o más, así que tras un fallo se ve la tendencia. `BLE_PH_MAGIC` cambia (la entrada es distinta).
- Con menos de `BLE_MIN_SCAN_HEAP` (16 KB) libres no se busca («POCA MEMORIA: no se busca»): cada anuncio oído se copia en memoria.

**Séptima prueba (26/11/2026, con la memoria en el historial)**: al arrancar hay ~58 KB libres; durante la **primera búsqueda** (4 s) la mínima cae a **2 KB**,
y al acabar vuelve a ~48-50 KB. Es `BLEScan`: guarda una copia (con String, vectores y mapa) de cada dispositivo que oye durante toda la búsqueda, y con
móviles y balizas cerca eso son ~40 KB de golpe. Es el `bad_alloc` del volcado anterior. Después el emparejado acaba bien y llega un informe del teclado
(«informes: 1»), pero el ESP32 vuelve a reiniciarse antes de «conectado» (sin volcado todavía: pedido `esp ble crash`; .elf guardado, SHA aae4113e6…).
- Cambio (sin compilar): **búsqueda propia sobre NimBLE** (`ble_gap_disc` + `ble_hs_adv_parse_fields`), sin `BLEScan`. Cada anuncio se analiza al vuelo;
  solo se guardan, en una tabla fija de 16 (~800 B), los que interesan (HID, apariencia de ratón/teclado/mando o dirección guardada), juntando el anuncio y
  la respuesta al escaneo (el nombre). La elección de candidato es la misma de antes (`scan_consider`). Mismos parámetros: activa, 100 ms de intervalo,
  ventana de 50 ms (30 si hay algo conectado), duplicados filtrados. **Afecta también al ratón**: hay que comprobar que sigue encontrándolo.

**Volcado de la séptima prueba** (.elf aae4113e6): `std::logic_error` en `nimble_host`, en `BLERemoteCharacteristic::descriptorDiscCB`, al construir un
`std::string` con un puntero nulo. Es `getUUID().toString().c_str()`: el `String` del UUID no pudo reservar memoria (queda sin búfer y `c_str()` da nulo).
Otra vez falta de memoria, pero esta vez **en el descubrimiento de descriptores del Pebble tras emparejar**, con ~47 KB libres justo antes. Esa fase se
los come: la librería crea un objeto con dos o tres semáforos y varios `String` por cada servicio, característica y descriptor.
- Cambio (sin compilar): puntos de control de memoria en el historial (`ph_memf`, «[libre/minima KB]»): tras cifrar, antes y después de los servicios, tras
  las características HID (con cuántas son), tras leer el mapa y en cada informe del teclado (id, tipo y suscripción). El historial pasa de 24 a 40
  entradas (`BLE_PH_MAGIC` cambia). Con eso se verá qué paso se lleva la memoria; luego, el arreglo (no crear objetos que no hacen falta).
- Propuesta del usuario (26/11/2026), hecha (sin compilar): en la ventana de emparejado, un dispositivo **nuevo** solo es candidato si su anuncio lleva la
  bandera de «descubrible» (general o limitado, `BLE_HS_ADV_F_DISC_GEN/LTD`): es decir, si está en modo emparejado. Uno que solo quiere volver a su PC no
  la lleva. Si el anuncio no trae banderas, se acepta como antes. El guardado (y el mismo con otra dirección, tipo M240) no se filtra. «Limitado» suma
  10 puntos. La lista «visto:» de `esp ble` marca «EMPAREJABLE» o «(no emparejable)». Comprobar en la placa que el ratón en modo emparejado sigue saliendo.

**Octava prueba: dónde se va la memoria (26/11/2026)**. Puntos de control: tras cifrar 45 KB; servicios −3; las 14 características HID −10 (32 KB);
el Report Reference y la suscripción del informe 1 **−12** (19 KB); el del informe 3 **−9** (10 KB), y se cae en el siguiente. La búsqueda nueva ya no
pasa de ~7 KB de mínima (antes 2). Causa: **un fallo de la librería BLE de Arduino** (3.3.10, NimBLE). `BLERemoteCharacteristic::getDescriptor()` y
`subscribe()` llaman a `retrieveDescriptors()`, que hace `ble_gattc_disc_all_dscs` desde la característica **hasta el final del servicio**, no hasta la
siguiente. Find Information devuelve todos los atributos de ese tramo (declaraciones y valores de las características siguientes) y la librería crea un
`BLERemoteDescriptor` (dos semáforos y varios `String`) por cada uno. Con el ratón no se notaba (GATT pequeño).
- Cambio (sin compilar): teclado y mando sin `getDescriptor`/`subscribe`. GATT a mano con NimBLE (`subscribe_raw`): `ble_gattc_disc_all_dscs` solo hasta
  la característica siguiente (handle del valor siguiente − 2; la última se para en la primera declaración 0x2803/0x2800/0x2801), `ble_gattc_read` del
  Report Reference y `ble_gattc_write_flat` del CCCD (0x0001), sin crear objetos. Las notificaciones llegan por el *listener* GAP (`gap_listener`,
  `BLE_GAP_EVENT_NOTIFY_RX`; comprobado en `libbt.a` que `ble_gap_notify_rx_event` llama a los listeners antes que a la conexión) con una tabla
  conexión+handle → dispositivo (`s_nsub`). El ratón sigue con la librería, sin cambios.

**Novena prueba (26/11/2026)**: con el GATT a mano, el Pebble Keys 2 **empareja con clave, queda conectado y se escribe en el BASIC** del ZX81
(modo texto). Falta: comprobar el ratón a la vez que el teclado (que no empeore), el modo juego (Ctrl+Alt+Espacio), la reconexión tras reiniciar sin
volver a emparejar, `esp ble kbd forget` (los otros vínculos siguen), la memoria libre con los dos conectados y el mando (Luna o el puente Mega Drive).

**El mando Luna es BLE** (sale en nRF Connect, 26/11/2026). Para su tabla de botones, `esp ble pad` enseña ahora también los botones HID pulsados
sin traducir («botones HID pulsados ahora»). Memoria con el teclado conectado: 31 KB libres (el teclado se llevó 15 KB, casi todo en las 14
características; los informes ya no cuestan nada). Pendiente de explicar: una mínima de ~6 KB a los ~15 s del arranque (`esp ble mouse` dirá si es la
conexión del ratón, que sigue con la librería y su fallo de los descriptores).
- Primera prueba del Luna (26/11/2026): empareja y cifra (Just Works), 10 características HID, mapa de 493 B, informes id 1, 2, 245 y 240 de entrada;
  el mando se lleva ~14 KB. A media suscripción, «desconectado» y aun así salió «conectado a  (informes)» con el nombre vacío (fallo: no se miraba si
  seguía conectado tras suscribirse). Arreglado (sin compilar): `subscribe_raw` para si se pierde la conexión y `connect_slot` lo cuenta como fallo; el
  historial dice el motivo de cada desconexión («desconectado (motivo 0x213)»: 0x213 la cortó el otro, 0x208 se perdió la señal, 0x216 la cortamos).
- **Luna funcionando** (26/11/2026): conecta (se lleva 12 KB), 0 fallos en ~2800 tramas `CMD_PAD`. Botón a botón con la tabla por defecto: HID 1 A,
  2 B, 4 X, 5 Y, 7 LB, 8 RB, 12 Menu (START), 14 L3, 15 R3; LT/RT analógicos por freno/acelerador (255) con sus bits digitales; cruceta por hat
  (arriba 1, derecha 8, abajo 2, izquierda 4); sticks X/Y y Z/Rz. Es la numeración de Android: tabla «luna» = la de por defecto (añadida por nombre).
  El mapa solo trae 9 botones de la página 9: falta ver View (¿11?) y los botones Luna y micrófono (¿página de consumo en el informe 2?, no suscrito).
  Pendiente: la mínima de memoria llega a 4 KB en el arranque (antes de conectar el mando); `esp ble mouse` dirá si es la conexión del ratón.
- Botones View, Luna y micrófono del Luna: llegan en el informe 1 (sube «informes») pero no son de la página 9. Añadido (sin compilar): botones de la
  página de consumo en el mando, como bits sueltos o como array de códigos: 0x224 AC Back → SELECT, 0x223 AC Home → HOME, 0x40 Menu → START,
  0xCF/0x221 → micrófono (bit 13, libre en `CMD_PAD`), 0xB2 Record → bit 14. `esp ble pad` enseña los usos de consumo encontrados y el último informe en bruto.
- **Regresión del ratón** (no reconectaba tras encender): la búsqueda nueva usaba `ble_hs_adv_parse_fields()`, que rechaza el anuncio entero si un
  campo está mal formado, y el del M240 lo está (el nombre guardado era basura: el analizador de `BLEScan` leía más allá del paquete). Arreglado (sin
  compilar): analizador tolerante propio (`adv_parse`: banderas, UUID16, nombre, apariencia; un campo demasiado largo se corta al final del paquete);
  los nombres se quedan con su parte imprimible y la regla del M240 compara solo esa parte (`name_match`), así que vale el nombre viejo con basura.
- Prueba con ratón + mando (26/11/2026): el ratón se llevaba **20 KB** al conectar (seguía con `subscribe()` de la librería: ~10 KB de descriptores
  falsos) y el mando quedaba fuera por el umbral («POCA MEMORIA: 26180 B libres»). Cambio (sin compilar): el ratón también con GATT a mano
  (`subscribe_mouse` → `subscribe_raw`, informes por `gap_listener` → `on_input`); su análisis del descriptor, la acumulación y el envío no cambian.
  `BLE_MIN_FREE_HEAP` baja de 30 000 a 22 000 B (medido: ratón ~10 KB, mando ~12, teclado ~15). Protocol Mode a boot solo si hay informe boot.
- Tras reemparejar, el M240 sale con nombre limpio («LOGI M240», antes basura) y otra dirección (…:71): la regla de «el mismo con otra dirección» ya
  podrá funcionar la próxima vez. Pendiente: «HID vistos: 0» mientras buscaba el guardado (…:70) sin estar en modo emparejado.
- **Regresión: no se reconecta nada tras reiniciar** (27/11/2026). Causa probable: la búsqueda nueva decidía solo con el anuncio y tiraba la respuesta al
  escaneo de los que no anunciaban HID ni la dirección exacta. La regla del M240 (5 primeros bytes + nombre) necesita el nombre, que suele venir en la
  respuesta. Cambio (sin compilar): se guardan también los de prefijo conocido (`prefix_of_saved`) y las respuestas que traen HID o apariencia; la
  regla «el mismo con otra dirección» = prefijo + nombre igual, o, sin nombre, prefijo + (HID, su tipo o anuncio dirigido a nosotros). La lista
  «visto:» marca «DIRIGIDO a nosotros». Falta ver en la placa si el teclado y el Luna reconectan (con otra dirección, el Pebble pediría otra vez la clave).
- Prueba tras reiniciar (27/11/2026): vuelve el Luna (dirección fija), pero el M240 y el Pebble **no aparecen en la búsqueda** («HID vistos: 0»), ni con
  la dirección guardada ni con el prefijo, aunque se usaban. Encaja con la privacidad BLE: vinculados, se anuncian con una dirección privada resoluble
  (RPA) que solo se reconoce con su IRK. La pila tiene lo necesario (`ble_store_write_peer_sec` → `ble_hs_pvcy_add_entry`; `ble_hs_misc_restore_irks`
  al arrancar). Cambio (sin compilar): `ble_hs_pvcy_set_resolve_enabled(1)` al iniciar (sale en el historial); se reconoce a cada uno también por su
  dirección de identidad (`ida`) y se guardan los anuncios de tipo 2/3 (ya resueltos); y, si el controlador no resuelve, resolución propia con las IRK
  de los vínculos (AES de mbedtls, prueba los dos órdenes de la clave). `esp ble` dice cuántas privadas se oyeron, cuántas se resolvieron y cuántas
  IRK hay. Si se conecta a una privada sin resolver por el controlador, NimBLE no encontrará el vínculo y habrá que reemparejar (el Pebble pediría clave).
- Sigue sin reconectar el ratón ni el teclado tras reiniciar (27/11/2026), y el usuario recuerda que **antes de la fase 1 el ratón sí reconectaba**
  (commit 850f92c). Para una dirección exacta la búsqueda nueva acepta lo mismo que la vieja: o el ratón no se anuncia con la dirección guardada, o no
  oímos sus anuncios. Añadido (sin compilar) el modo escucha: `esp ble sniff` apunta todos los anuncios de la búsqueda siguiente sin filtro (dirección,
  tipo y si es privada, clase de anuncio, banderas, HID, apariencia, nombre, veces) y `esp ble sniff show` los enseña.
- `esp ble sniff` tras reiniciar y moviendo el ratón (27/11/2026): **no se oye nada** del M240 ni del Pebble (ningún d4:8e:… ni d4:b7:…, ni con el byte
  nuevo); solo otros aparatos lejanos (−79 a −98 dBm). Diferencia con `BLEScan`: buscaba con la dirección pública del ESP32 (`m_ownAddrType`) y la
  búsqueda nueva la elegía con `ble_hs_id_infer_auto()`; si un vinculado vuelve con anuncios DIRIGIDOS a nuestra dirección, el controlador solo los pasa
  si buscamos con esa dirección. Cambio (sin compilar): `own_addr_type` fijo a `BLE_OWN_ADDR_PUBLIC`.
- Prueba A/B preparada: `build/prueba-raton-viejo/Wifi_module_01/` = el sketch de hoy con `MOUSE_BLE.cpp/.h` y `LOGCFG.cpp` de HEAD (850f92c, antes de la
  fase 1) y sin `KEYMAP_ZX81.*`. Si con ese el ratón tampoco reconecta, el problema es el estado del ratón o de su vínculo, no el código nuevo. Ojo: su
  `esp ble pair` borra TODOS los vínculos (`ble_store_clear`).
- Con la dirección pública **el ratón y el teclado ya reconectan solos** (27/11/2026). Nuevos fallos: (a) si ya están el ratón y el teclado, el Luna
  no vuelve tras apagarlo y encenderlo (la búsqueda «ligera» de cuando hay algo conectado, ventana 30 ms cada 8 s, se perdía su anuncio corto);
  (b) con los tres conectados la web va muy lenta y la página del depurador deja de ir (memoria: quedaban ~8 KB; y radio compartida con el WiFi).
  Cambios (sin compilar): búsqueda siempre completa y cada 2,5 s; **descubrimiento GATT a mano para los tres** (`gatt_discover_hid`:
  `ble_gattc_disc_svc_by_uuid` + `ble_gattc_disc_all_chrs` en una tabla de handles; mapa con `ble_gattc_read_long`; Protocol Mode con
  `ble_gattc_write_no_rsp_flat`): la librería ya solo conecta y cifra, sin objetos de servicios ni características (~10 KB por dispositivo menos);
  al teclado se le pide intervalo de 15-30 ms (`updateConnParams(12, 24, 0, 400)`) para dejar radio al WiFi.
- Con los tres conectados la página `/debug` salía en blanco o a medias (y sin ella no hay consola). Causa: `server.send(200, "text/html", DEBUG_PAGE)`
  convierte los ~27 KB de la página en un `String` (necesita un bloque libre de 27 KB seguidos; la propia librería avisa «Use send_P for long arrays»).
  Arreglo (sin compilar, en `Wifi_module_01.ino`): `server.send_P(200, "text/html", DEBUG_PAGE, sizeof(DEBUG_PAGE) - 1)`, que la manda desde la flash.
  El usuario propone adelantar la parte del Z80 (`LOAD *KEYB`/`LOAD *PAD` con STOP) para poder parar dispositivos desde el ZX81: es la fase 5.
- **Depurador con los tres conectados: funciona** con `send_P` (27/11/2026). Estado: ratón, teclado y mando conectan y reconectan solos, el WiFi va
  bien. Quedan por probar el modo juego del teclado, `forget` de uno solo y el mando como teclas con `LOAD *JOY`. La copia `build/prueba-raton-viejo/` ya
  no hace falta (se puede borrar).

## Fase 5: hecho (27/11/2026, sesión S4, ROM; ensamblado con pasmo, sin placa, sin commit)

Solo la parte de la ROM y la documentación; **quedan sin hacer** `EXAMPLES/PADTEST` (leer el op 0 del 78) y la nota para el emulador
(`teclado_gamepad_emulador.md`).

- `z80rom/sdhandler.inc.asm`: `CMD_btdev equ 0x4E`; rutinas `CmdKEYB` (E = 1) y `CmdPAD` (E = 2), con código común `BtDev`, **delante de `CmdList`**
  (detrás de `SD81RUNCMD`, como los sprites); entradas `KEYB` y `PAD` en `CmdList` detrás de `MOUSE`. ROM 12 505 → 12 716 B; **los 8192 primeros bytes
  idénticos** (cmp contra el ensamblado de antes; `SD_RESET`, `SD81RUNCMD` y `Z81Monitor` no se mueven).
- Bytes al MCU (C es el reloj y no se toca salvo para resincronizarlo como `MouseRead`; el dispositivo va en E, que se guarda en la pila alrededor de
  `CLASS_1` y de `BC_SPACES`):
  - `THEN NEW / RUN / CLEAR / STOP`, `STOP`: 78, 3, dispositivo, orden (1 / 2 / 3 / 5), 0 → resultado (bit 7 = no válida → REPORT-B) y toggle final.
    **`THEN RUN` manda solo la orden 2 (connect)**: en el ESP32 `connect` ya pone `enabled = true`, y el STM32 guarda **una** orden por dispositivo
    (`s_ble_order[dev]`), así que «4 y luego 2» seguidas perdería el 4 si el ESP32 no lo ha recogido aún.
  - Sin nada o `TO A$`: 78, 5, dispositivo → 16 caracteres + estado; a partir del primer carácter se reutiliza el final de `CmdMOUSE` (`MouseRead`:
    lectura, `ReportStatus`, imprimir o `BattStore`). Como el 78 tiene un byte más (el dispositivo), tras mandarlo se pone C al reloj actual.
  - `LOAD *KEYB TEXT` / `GAME` (no son tokens: se comparan letra a letra; con `*PAD` dan error C): 78, 6, 0/1 → estado (bit 0 conectado, que no es
    error; bit 7 = no válido → REPORT-B) y toggle final.
- Documentado: filas nuevas en la tabla de comandos de `CLAUDE.md` y entrada en `claude/planning/manual_pendiente.md`.
- **Sin probar**: la placa (el handshake del 78 con el Z80 es nuevo), el emulador (todavía no implementa el 78).

## Ampliación: teclas del mando desde BASIC, `LOAD *PAD "cadena"` (contrato, 27/11/2026)

Petición del usuario: configurar todas las teclas del mando, **direcciones incluidas**, para que el mando y el joystick DB9 (`LOAD *JOY`) vayan por
separado. Hoy las direcciones del mando siguen siempre a `LOAD *JOY`, y solo se pueden cambiar los botones con el op 1 del 78, desde código máquina.

**Comando 78, op 7 `PAD TECLAS` (nuevo, en el STM32):**
- Z80 → MCU: `n` (0-17) y `n` códigos, en este orden: **ARRIBA, ABAJO, IZQUIERDA, DERECHA**, y luego los botones en el orden de `CMD_PAD`:
  **A, B, X, Y, LB, RB, LT, RT, SELECT, START, L3, R3, HOME** (bits 0-12). Los que pasan de `n` no cambian; **`n = 0` vuelve a los de por defecto**.
- MCU → Z80: estado (bit 7 = algún código no válido; ese queda sin tecla; los demás se aplican).
- Códigos: los del op 1 (0 = ninguna; 1-63 la tecla de ese carácter ZX81 como en `LOAD *JOY`; 128-191 = lo mismo sin el bit 7, **ESPACIO = 128**;
  118 = NEW LINE) más **255 = «como `LOAD *JOY`»**: en una dirección, la tecla de esa dirección de `LOAD *JOY`; en un botón, el disparo de `LOAD *JOY`.
- Por defecto (y con `n = 0`): las cuatro direcciones = 255 (siguen a `LOAD *JOY`, como hoy), A y B = 255, START = NEW LINE, el resto 0.
  Es decir, sin configurar nada todo se comporta igual que ahora.
- El op 1 sigue igual (sus botones son los mismos que las posiciones 5-17 del op 7).
- Las direcciones del mando salen de la cruceta **o** del stick izquierdo (umbral de hoy), cada una con su código.

**BASIC (ROM): `LOAD *PAD "cadena"`**, un carácter por posición, en el orden de arriba (hasta 17; si es más corta, el resto no cambia):
- Letra, cifra, `.`, `<` (NEW LINE), `*` (SHIFT): esa tecla, con la misma conversión que `LOAD *JOY`. **Espacio** = la tecla ESPACIO (código 128).
- **`-` = sin tecla** (código 0). **`=` = como `LOAD *JOY`** (código 255).
- `LOAD *PAD ""` = los de por defecto (op 7 con `n = 0`).
- Ejemplo: `LOAD *PAD "QAOPMM--<"` → arriba Q, abajo A, izquierda O, derecha P, A y B = M, X e Y sin tecla, LB = NEW LINE; el resto como estaba.
- Error si la cadena tiene más de 17 caracteres o algún carácter no válido (REPORT-B, como el resto de `LOAD *PAD`).

## Op 7: hecho (27/11/2026, sesión S3, STM32; probado con g++ en el PC, sin placa, sin commit)

`PAD.cpp/.h` y `COMMANDS.cpp` (`cmd_btdev`), con el contrato de la sección anterior tal cual:
- Las 4 direcciones tienen ahora su código (`s_dir_code`, por defecto 255 = la de `LOAD *JOY`), salen de la cruceta o del stick izquierdo como antes.
  255 en un botón sigue siendo el fuego de `LOAD *JOY`.
- `pad_set_all(n, codes)`: posiciones 1-4 direcciones y 5-17 los botones 0-12; `n = 0` vuelve a los de por defecto (direcciones y botones); con
  `n > 17` se leen todos los bytes (no se pierde el handshake), se aplican los 17 primeros y el estado lleva el bit 7. Estado: bit0 mando conectado.
- El op 1 no cambia (su `n = 0` sigue reponiendo solo los botones).
- **`.` (carácter 27)**: el contrato lo da por válido «como `LOAD *JOY`», pero `keyb_coords` (la tabla de `LOAD *JOY`) no lo tiene: `LOAD *JOY` lo
  rechaza hoy. En el mando se trata aparte (fila 7, columna 1), en los ops 1 y 7. `LOAD *JOY "."` sigue dando error (no lo he tocado).
- Probado en el PC: defecto igual que antes, `"QAOPMM--<"`, `.`, 255 en una dirección, código no válido, `n = 0`, las 17 posiciones.

## LOAD *PAD cadena: hecho (27/11/2026, sesión S4, ROM; ensamblado con pasmo, sin placa, sin commit)

- `z80rom/sdhandler.inc.asm`, junto a `CmdPAD`: con el mando (E = 2), lo que no es `THEN`, `STOP`, `TO` ni fin de línea se lee como **expresión de
  cadena** (`GetStrExpr`, así que vale también `LOAD *PAD A$`); con el teclado sigue siendo solo `TEXT`/`GAME` (otra cosa da error C).
- Primero se comprueba toda la cadena (más de 17 → REPORT-B; un carácter no válido → REPORT-B) y **no se manda nada** si falla; la cadena no se
  modifica (puede ser la de una variable): la conversión se hace al vuelo al mandar.
- Conversión (`PadCode`): espacio → 128, `-` → 0, `=` → 255, `<` (19), `*` (23), `.` (27), cifras y letras (28-63) → su código ZX81; el resto
  (signos sin tecla, gráficos, vídeo inverso) no vale.
- Al MCU: 78, 7, n, n códigos (cada byte confirmado con un toggle; C se pone al reloj tras cada uno, como `MouseRead`) → estado (bit 7 → REPORT-B; el
  bit 0 «conectado» no es error) y el toggle final. `""` manda n = 0.
- ROM 12 716 → 12 829 B; los 8192 primeros bytes siguen idénticos a los de antes de la fase 5.
- **Sin probar**: la placa ni el op 7 del STM32 (lo hace S3 en paralelo con este mismo contrato), ni el emulador.
- Botón Luna (27/11/2026): no llega por el informe principal (id 1): «informes» no cambia al pulsarlo. El mapa declara otros informes de entrada
  (id 2, 245, 240, 241) y solo se suscribía el 1. Cambio (sin compilar): el mando se suscribe a todos sus informes de entrada; los botones de consumo
  de los secundarios (`aux`/`auxa` en `PadLayout`) se suman a los del principal (`pad_on_aux`); `esp ble pad` enseña el último contenido de cada informe
  por su id. Al reconectar sin nombre en el anuncio se usa el nombre guardado (salía «(sin nombre)»). La conexión del mando cuesta ya solo ~0,6 KB.
- **Botón Luna funcionando** como HOME (posición 17 de `LOAD *PAD "cadena"`) tras suscribir todos los informes del mando (27/11/2026, probado en la
  placa). El botón del círculo, a la izquierda del Luna, es SELECT; el Luna no tiene «View». El del micrófono sigue sin tecla.

## Ampliación: distribución del teclado desde BASIC, `LOAD *KEYB ES` / `LOAD *KEYB US` (27/11/2026, hecho y **probado en la placa**, con la ayuda de la SD)

- **Comando 78, op 8 `TECLADO DISTRIBUCION`** (STM32, `cmd_btdev`): Z80 → MCU 0 española / 1 US; MCU → Z80 estado (bit0 teclado conectado, bit7 no
  válido). Deja para el ESP32 la orden **8** (española) o **9** (US) del dispositivo 1 (`BLE_ORD_KB_ES/US` en `MOUSE.h`; `ble_order` las admite).
- **`CMD_BLE`**: órdenes nuevas 8 y 9 para el teclado (comentario de las dos copias de `WIFI_PROTOCOL.h`). El ESP32 las convierte en `esp ble kbd layout
  es|us` (`ble_stm_sync_once`): la guarda en su NVS (`lay`) y se la pasa a `keymap_set_layout`. Un ESP32 viejo las ignora.
- **ROM**: `LOAD *KEYB ES` / `LOAD *KEYB US`, letra a letra como `TEXT`/`GAME` (`BtKbdWord` recibe ahora el op en D: 6 modo, 8 distribución).
  Ensamblado con pasmo: sin errores, bloque 0 idéntico al del commit 063167d, ROM 12 829 → 12 860 B (el `jr z,BtDevDo` del principio pasa a `jp`).
- Ayuda de la SD (`SD Content/MAN`, fuera de git): `KEYB.txt` (comandos y combinaciones de teclas), `PAD.txt`, `MOUSE.txt` (no existía) y sus líneas
  en `HELP.txt`. Mayúsculas, inglés y 32 caracteres por línea, como las demás.
