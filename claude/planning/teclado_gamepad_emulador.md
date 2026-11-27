# Teclado y mando Bluetooth: qué tiene que hacer el emulador (27/11/2026)

En la placa, el ESP32 del módulo WiFi conecta por BLE un ratón, un teclado y un mando, y se los pasa al STM32. El STM32 mete las teclas en la matriz
del ZX81 (teclado virtual de la FPGA) y el Z80 lo controla todo con el **comando 78 (0x4E, `BTDEV`)** del MCU. En el emulador no hay ESP32 ni BLE.

Recomendación:
- **Teclado:** el del PC ya es el teclado del ZX81, así que no hace falta emular el teclado BLE. Basta con contestar al comando 78 como si no lo hubiera:
  estado 0x80 y «SIN TECLADO» (ver op 5). Opcional: hacer de teclado «conectado».
- **Mando:** usar un mando del PC (XInput o DirectInput) como mando BLE, siempre «conectado», y emular dos cosas: la lectura directa (op 0) y el
  **mando como teclas** (ops 1, 2 y 7, y `LOAD *PAD "cadena"`), que es lo que usan los juegos de siempre.

La ROM nueva (`z80rom/sdhandler.inc.asm`: `CmdKEYB`, `CmdPAD`, `BtDev`) implementa `LOAD *KEYB` y `LOAD *PAD` solo con el comando 78. El emulador **no
necesita código propio para los comandos BASIC**: basta con el comando 78. Fuente de verdad: `cmd_btdev` en `Arduino/SD81BoosterV2_039_STM32/COMMANDS.cpp`
y `PAD.cpp/.h`. Contrato completo: `claude/planning/teclado_gamepad_plan.md` (secciones «Comando 78» y «Ampliación: teclas del mando desde BASIC»).

## 1. Comando 78: protocolo

El handshake es idéntico al del comando 77 (`cmd_mouse`), que el emulador ya tiene:
- `ToggleClock` de ACK del comando;
- por cada byte que manda el Z80, `GetByteFromZ80` y su `ToggleClock` (el último lo confirma la primera respuesta);
- `SendByteToZ80` por cada byte de respuesta;
- `reset_commands()` y el `ToggleClock` final.

| op | Z80 → MCU (tras el op) | MCU → Z80 | Qué hace |
|---|---|---|---|
| 0 PAD LEER | — | 10 B: botones lo, botones hi, cruceta, lx, ly, rx, ry, lt, rt, estado | estado del mando. Estado: bit0 conectado, bit1 ha cambiado algo desde la lectura anterior (se borra al leer) |
| 1 PAD TECLAS | n, n códigos | estado | tecla de los botones 0..n-1 (bits de `botones`); los demás no cambian; n = 0 → por defecto |
| 2 PAD COMO TECLAS | 0/1 | estado | desactiva/activa el mando como teclas (activado por defecto) |
| 3 BLE ORDEN | dispositivo, orden, argumento | 0 aceptada / 0x80 no válida | orden para el ESP32 (ver abajo). En el emulador: aceptar (0) y no hacer nada, o simular el estado |
| 4 BLE ESTADO | dispositivo | estado, segundos | bits del estado BLE (abajo) y segundos que quedan de la ventana de emparejado |
| 5 BLE TEXTO | dispositivo | 16 caracteres ZX81 + estado (0; 0x80 si el dispositivo no vale) | estado en texto (abajo), relleno con espacios, en el juego de caracteres del ZX81 |
| 6 TECLADO MODO | 0 texto / 1 juego | estado (bit0 = teclado conectado) | modo del teclado BLE. En el emulador: aceptar y no hacer nada |
| 8 TECLADO DISTRIBUCION | 0 española / 1 US | estado (bit0 = teclado conectado) | distribución del teclado BLE (`LOAD *KEYB ES` / `US`). En el emulador: aceptar y no hacer nada |
| 7 PAD TECLAS (con direcciones) | n (0-17), n códigos | estado | teclas del mando en el orden ARRIBA, ABAJO, IZQUIERDA, DERECHA, A, B, X, Y, LB, RB, LT, RT, SELECT, START, L3, R3, HOME; las que pasan de n no cambian; n = 0 → por defecto |
| otro | — | 0x80 | |

- **Dispositivo:** 0 ratón, 1 teclado, 2 mando. **Órdenes del op 3:** 1 pair (argumento = segundos, 0 = 60), 2 connect, 3 forget, 4 on, 5 off; solo el
  teclado, 6 modo texto, 7 modo juego, 8 distribución española y 9 US.
- **Estado de los ops 1, 2, 6, 7 y 8:** bit0 = el mando (ops 1, 2 y 7) o el teclado (ops 6 y 8) está conectado; bit7 = algún parámetro no válido (`LOAD *PAD` da
  el error B). En los ops 1 y 7 **se leen todos los bytes aunque sobren**, para no perder el paso del handshake.
- **Estado BLE (op 4):** bit0 conectado, bit1 emparejando (ventana abierta), bit2 activado, bit3 hay uno guardado, bit7 el ESP32 ya ha contado su estado
  (si es 0, no hay módulo WiFi).
- **Texto (op 5)**, por orden de prioridad: `SIN MODULO WIFI` (bit7 a 0 o dispositivo no válido), `CLAVE 123456` (solo el teclado, mientras espera la
  clave de emparejado), `CONECTADO`, `EMPAREJANDO 45S`, `BUSCANDO` (activado y guardado), `PARADO` (guardado), y si no `SIN RATON` / `SIN TECLADO` /
  `SIN MANDO`.
- Para el emulador basta: mando = estado 0x8D (conocido, guardado, activado y conectado) y texto `CONECTADO` si hay un mando del PC; si no,
  `SIN MANDO`; teclado = 0x80 y `SIN TECLADO`.

## 2. Estado del mando (op 0)

| Campo | Contenido |
|---|---|
| botones (16 bits) | bit0 A (sur), 1 B (este), 2 X (oeste), 3 Y (norte), 4 LB, 5 RB, 6 LT digital, 7 RT digital, 8 SELECT/View, 9 START/Menu, 10 L3, 11 R3, 12 HOME, 13 micrófono (Luna), 14 compartir; 15 libre |
| cruceta | bit0 arriba, bit1 abajo, bit2 izquierda, bit3 derecha |
| lx, ly, rx, ry | −127..127 con signo, **Y hacia abajo positivo** |
| lt, rt | 0..255. En la placa, los bits 6/7 de botones también se ponen si el gatillo pasa de 100 |

Con XInput: A/B/X/Y, LB/RB, Back = SELECT, Start, LS/RS = L3/R3, Guide = HOME si está disponible, gatillos 0..255, sticks de −32768..32767 a
−127..127 (la Y de XInput va hacia arriba: hay que cambiarle el signo).

## 3. El mando como teclas (lo que usan los juegos)

Si está activado (op 2, por defecto sí) y hay mando, en cada cambio el STM32 calcula una matriz y la **suma (OR) al teclado del ZX81** (en la placa,
por el teclado virtual de la FPGA, como otra fuente más: la web, el teclado BLE y el mando):
- **Direcciones:** arriba si `cruceta.bit0` o `ly <= −64`; abajo si `cruceta.bit1` o `ly >= 64`; izquierda si `cruceta.bit2` o `lx <= −64`; derecha si
  `cruceta.bit3` o `lx >= 64`. Cada dirección pulsa la tecla de su código (op 7).
- **Botones:** cada botón pulsado (bits 0-12) pulsa la tecla de su código (ops 1 y 7).
- **Códigos de tecla** (los mismos caracteres ZX81 que `LOAD *JOY`):

| Código | Tecla |
|---|---|
| 0 | ninguna |
| 1..63 | la tecla de ese carácter ZX81, como en `LOAD *JOY`: letras, cifras, 19 (`<`) = NEW LINE, 23 (`*`) = SHIFT, 27 (`.`) = punto; los demás signos no son teclas (no válido) |
| 128..191 | lo mismo que 0..63 (el bit 7 no cuenta): **128 = ESPACIO** |
| 118 | NEW LINE |
| 255 | «como `LOAD *JOY`»: en una dirección, la tecla de esa dirección de `LOAD *JOY`; en un botón, el disparo de `LOAD *JOY` (sigue a `LOAD *JOY` si cambia) |
| otro | no válido: el op contesta con el bit 7 y esa posición queda sin tecla |

- **Por defecto** (al arrancar y con n = 0): las 4 direcciones = 255, A y B = 255, START = 19 (NEW LINE), el resto 0. Sin configurar nada, el mando se
  porta como el joystick de `LOAD *JOY` más NEW LINE en START.
- **`LOAD *PAD "cadena"`** (ROM): un carácter por posición, en el orden del op 7 (hasta 17). La ROM convierte cada carácter y manda el op 7: letra, cifra,
  `.`, `<`, `*` como en `LOAD *JOY`; espacio → 128; `-` → 0 (ninguna); `=` → 255; `""` → n = 0 (por defecto). Ejemplo: `LOAD *PAD "QAOPMM--<"`.
- En la placa la fuente del mando se suelta sola si pasan 300 ms sin trama del ESP32; en el emulador, soltar todo al desconectar el mando del PC.

## 4. Lo que no hace falta emular

- Las tramas `CMD_KEYB`, `CMD_PAD` y `CMD_BLE` entre el ESP32 y el STM32 (`WIFI_PROTOCOL.h`): son internas.
- La traducción del teclado de PC (`KEYMAP_ZX81`, modos texto y juego con cola y tiempos): solo existe porque el teclado BLE llega como códigos HID; el
  emulador ya tiene su teclado.
- El emparejado, la clave, los vínculos y la búsqueda BLE: con contestar a los ops 3, 4 y 5 como en §1 basta.

## 5. Para probar en el emulador

- `LOAD *PAD` → `CONECTADO` (con mando del PC) o `SIN MANDO`; `LOAD *PAD TO A$`; `LOAD *KEYB` → `SIN TECLADO`; `LOAD *KEYB THEN NEW` → sin error.
- Con `LOAD *JOY "OPQA "` y sin `LOAD *PAD "..."`: la cruceta y el stick izquierdo mueven con O/P/Q/A y A o B del mando = ESPACIO; START = NEW LINE.
- `LOAD *PAD "QAOPMM--<"`: ahora la cruceta da Q/A/O/P aunque `LOAD *JOY` tenga otras, A y B = M, LB = NEW LINE. `LOAD *PAD ""` vuelve a lo anterior.
- `LOAD *PAD "ABCDEFGHIJKLMNOPQR"` (18) → error B; `LOAD *PAD "Q?"` → error B.
- Ejemplo pendiente (no hecho todavía): `EXAMPLES/PADTEST`, que leerá el op 0 en un bucle y enseñará botones y ejes.
