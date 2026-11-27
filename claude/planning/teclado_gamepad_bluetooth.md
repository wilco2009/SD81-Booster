# Teclado y gamepad Bluetooth: estudio de viabilidad (13 de noviembre de 2026)

Pregunta: ¿se pueden añadir un **gamepad** y un **teclado** Bluetooth, igual que el ratón, teniendo en cuenta lo medido de latencia?
Base: `raton_bluetooth.md` (cadena del ratón y su medida), `latencia_red.md` (bucle del STM32, tramas del ESP32, pérdidas en el UART) y el código actual.

**Conclusión: viable, sin cambiar la FPGA para lo esencial.** El camino de las teclas ya existe entero (teclado virtual de la web: ESP32 → `CMD_DBG_KEYS` →
`vkeys_set()` → orden 13 → `vkeys.v` suma teclas al puerto `$FE`). La latencia añadida es del orden de un intervalo de conexión BLE (7,5-15 ms), menor que el
fotograma de 20 ms con el que el ZX81 lee el teclado. Las limitaciones reales son otras: **el ESP32-C3 solo tiene BLE** (fuera mandos de PlayStation y Switch),
**3 conexiones y 3 vínculos como máximo** (ratón + teclado + gamepad justos) y que **el ZX81 no admite dos teclas a la vez** al escribir (hay que serializar).

## 1. Qué hay ya (comprobado en el código)

| Pieza | Dónde | Sirve para |
|---|---|---|
| Cliente BLE HID: escaneo, bonding en NVS, *Report Map*, suscripción | `Wifi_module_01/MOUSE_BLE.cpp` (598 líneas) | base de los dos; hoy un solo cliente y un `Layout` de ratón (X, Y, rueda, 8 botones) |
| Teclado virtual 8×5 sumado al real y al joystick | `FPGA/.../vkeys.v`, orden 13; `COMMS.cpp` `vkeys_set/clear/poll` | el teclado y el gamepad «como teclas» **sin tocar la FPGA** |
| Trama de la matriz completa (8 bytes) del ESP32 al STM32 | `CMD_DBG_KEYS` (0x13), `WIFI_HANDLER.cpp` `handle_dbg_keys` | idempotente: si se pierde una, la siguiente lo arregla |
| Mapeo del joystick DB9 a teclas | FPGA `joy_cnf` (orden 0), `LOAD *JOY "OPQA "` → `cmd_joy`/`send_joycfg` | el gamepad puede usar la misma configuración (el STM32 la conoce) |
| Lectura sin esperas desde el Z80 | índice 20 de `$3FEF` (`mouse_regs.v`) | modelo para un puerto del gamepad, si algún día hiciera falta |
| Emparejado desde BASIC | comando 77 op 3/4/5, `CMD_BLE` (0x16), `LOAD *MOUSE THEN NEW/RUN/CLEAR/STOP` | se generaliza con un selector de dispositivo |

## 2. Latencia (con los números ya medidos)

Tramo a tramo, de pulsar a que el Z80 lo vea:

| Tramo | Ratón (medido) | Teclado / gamepad (estimado) |
|---|---|---|
| Pulsación → notificación BLE | intervalo de conexión del periférico, 7,5-30 ms | gamepads 7,5-11,25 ms típico; teclados 7,5-30 ms (los de bajo consumo, más) |
| ESP32 → STM32 (UART 921.600) | media **0,74 ms**, máx. 5,3 ms | igual (trama de 8-12 bytes) |
| STM32 → FPGA (canal de configuración) | ~0,4 ms por paquete de 31 bits | orden 13: ~0,15 ms por fila que cambie (1-2 filas por pulsación) |
| FPGA → Z80 | `IN` cuando el programa lee | la ROM y los juegos leen el teclado **una vez por fotograma**: 0-20 ms, media 10 |

- **Media ≈ 15-20 ms, peor caso ≈ 35-45 ms.** El joystick DB9 da 0-20 ms (solo el fotograma): el BLE añade de media ~5-10 ms. Para juegos a 50 o 25 fps es
  imperceptible; es lo mismo que un mando BLE en un PC.
- No pesa el bucle del STM32: en `LAT_STATS` (partida de SPORTS) media 20 µs, máximo 6,5 ms. Excepción conocida: mientras el Z80 ejecuta un comando largo del MCU
  (un `LOAD`), no se atiende al ESP32; ahí da igual.
- **Riesgo 1: pérdida de una trama en el UART.** Se vio una cada ~10.000 peticiones (`latencia_red.md`, quinta tanda). Con el ratón es un movimiento perdido;
  con un teclado sería **una tecla que se queda pulsada**. Por eso: mandar siempre la matriz entera (como `CMD_DBG_KEYS`), un latido cada ~100 ms aunque no cambie
  nada, y que el STM32 suelte todo si pasan ~300 ms sin latido (hoy `vkeys_poll` espera 1,5 s: hay que bajarlo para esta fuente). Al desconectarse un dispositivo,
  el ESP32 manda «todo suelto».
- **Riesgo 2: WiFi y BLE comparten radio** en el C3 (coexistencia por software, `CONFIG_ESP_COEX_SW_COEXIST_ENABLE = 1`). Con MQTT a 25-50 mensajes/s (juegos en
  red) se perderán eventos de conexión BLE y subirá el jitter. Es el pendiente 6 del ratón, sin medir: hay que medirlo antes de prometer gamepad en juegos en red.

## 3. Límites del ESP32-C3 (comprobados en el `sdkconfig` del core)

- **Solo BLE (HID over GATT)**: el C3 no tiene Bluetooth clásico. Quedan fuera DualShock 4, DualSense, Switch Pro y Joy-Con (todos BT clásico). Funcionan los
  mandos **Xbox Wireless (Series X|S y los One con firmware actualizado)**, que van por BLE, y mandos genéricos BLE; los 8BitDo según modelo y modo (**por confirmar**
  uno a uno). Teclados: casi todos los modernos «multi-dispositivo» son BLE o duales; los muy antiguos solo clásico.
- `CONFIG_BT_NIMBLE_MAX_CONNECTIONS = 3` y `CONFIG_BT_NIMBLE_MAX_BONDS = 3`: **ratón + teclado + gamepad es el máximo**, y las librerías del core vienen precompiladas
  (subirlo exige recompilar las librerías del IDF). Un cuarto vínculo expulsaría a otro.
- **Hay que cambiar el «olvidar» del ratón**: `ble_store_clear()` (MOUSE_BLE.cpp, `pair`/`forget`) borra las claves de **todos** los dispositivos. Con varios, borrar
  solo el del dispositivo (`ble_store_util_delete_peer`).
- Memoria: app 1.570 KB de 1.920 KB de partición (82 %); el código nuevo son decenas de KB, cabe. RAM: cada conexión con su GATT cliente son unos KB; mirar el mínimo
  libre que ya da `DIAG` con las tres conectadas y la web abierta.

## 4. Teclado BLE

**Lado ESP32** (generalizar `MOUSE_BLE`):
- Parser del *Report Map* para el teclado: página 0x07, los dos formatos (array de 6 códigos tipo *boot*, y mapa de bits NKRO con rango de usos); respaldo al
  informe *boot* de teclado (0x2A22). Ignorar *consumer control* (multimedia).
- **Emparejado con clave**: muchos teclados piden teclear un número de 6 cifras. Con `ESP_IO_CAP_NONE` (el del ratón) algunos aceptan *Just Works* y otros no;
  hay que declarar `ESP_IO_CAP_OUT` (DisplayOnly) y **enseñar la clave** en el estado (`LOAD *KEYB` → «CLAVE 123456», 16 caracteres como el del ratón) y en `esp ble`.
- Traducción a la matriz del ZX81 aquí (hay memoria de sobra y es donde se interpreta el HID); al STM32 llega solo la matriz.

**Traducción** (posición, no carácter: es un teclado de ZX81, en modo K la «P» da PRINT y así debe ser):
- Letras, cifras, espacio, Enter → NEW LINE, Mayúsculas → SHIFT, `.` → `.`.
- Atajos con SHIFT implícito: Retroceso → SHIFT+0 (RUBOUT), cursores → SHIFT+5/6/7/8, Esc → BREAK (espacio), y los signos del PC a su combinación
  (`"` → SHIFT+P, `;` → SHIFT+X, `+` → SHIFT+K, `-` → SHIFT+J, `*` → SHIFT+B, `/` → SHIFT+V, `=` → SHIFT+L, `(`/`)` → SHIFT+I/O...). EDIT = SHIFT+1, FUNCTION =
  SHIFT+NEW LINE, GRAPHICS = SHIFT+9, con teclas F o con Alt+tecla (a decidir).
- Pendiente de decidir: distribución española o inglesa de los signos (dónde están en el PC), configurable en un `.CFG`.

**El problema de verdad: el *rollover***. La ROM del ZX81 descarta la lectura si hay dos teclas (que no sean SHIFT) a la vez; al escribir rápido en un teclado de PC
se solapan casi siempre y se perderían letras. Dos modos:
- **Texto** (por defecto): cola de pulsaciones; cada una se mantiene ~3 fotogramas (60 ms, a ajustar con el *debounce* real de la ROM) y se suelta 1-2 fotogramas antes
  de la siguiente; la última pulsada manda. Las combinaciones con SHIFT implícito van aquí sin problema (SHIFT y tecla en la misma lectura).
- **Juego**: la matriz tal cual, varias teclas a la vez (Q + espacio), con una pulsación mínima de 2 fotogramas para no perder toques cortos.
- Cambio de modo: una tecla (Bloq Despl / F12) y la opción en BASIC. Riesgo: que el usuario no sepa en qué modo está; LED de Bloq Despl del teclado como testigo
  (informe de salida) si sale barato.

**Lado STM32**: `vkeys_set()` hoy **sustituye** la matriz; con web + teclado + gamepad hay que guardar una matriz por fuente y mandar a la FPGA la unión (OR), cada
una con su plazo de latido. Trama nueva (o `CMD_DBG_KEYS` con un byte de fuente).

## 5. Gamepad BLE

**Lado ESP32**: parser de *gamepad/joystick* (página 0x01 usos 0x04/0x05; ejes X, Y, Z, Rx, Ry, Rz 0x30-0x35, *hat* 0x39, botones de la página 0x09 hasta 16).
El mando Xbox da ejes de 16 bits, gatillos de 10 y *hat*. Se manda al STM32 el estado crudo normalizado (~10 bytes: botones, *hat*, 4 ejes a 8 bits, gatillos),
solo si cambia y con latido.

**Tres formas de usarlo desde el ZX81, de menos a más trabajo:**
1. **Como teclas (todos los juegos existentes)**: el STM32 convierte cruceta/stick (umbral ~50 %, zona muerta) y botones a teclas con la **misma configuración de
   `LOAD *JOY`** (que ya pasa por `send_joycfg`; basta guardarla) y los mete por la matriz virtual. Botones extra (B, X, Y, Start) a teclas configurables: un segundo
   disparo, NEW LINE, el 0... Sin FPGA. Equivale al joystick DB9 + botones.
2. **Lectura directa por comando del MCU** (juegos nuevos: analógico, 16 botones): una op nueva del comando 77 o un comando propio. Coste medido para comandos de
   este tipo: ~0,2-0,3 ms con espera activa, una vez por fotograma = 1-1,5 % del fotograma. Sin FPGA.
3. **Puerto de la FPGA** (índice 21 de `$3FEF`, como el ratón): sin esperas, ~100 µs. Solo si 2 no basta: **gasta LUTs** y la FPGA anda justa (rama de sprites al 96 %).

## 6. Fases propuestas

| Fase | Trabajo | FPGA | Prueba |
|---|---|---|---|
| 0 | Medir: intervalo de conexión de los candidatos (nRF Connect en el móvil o el log del ESP32) y coexistencia WiFi/BLE con el ratón bajo MQTT (pendiente 6) | no | placa |
| 1 | `MOUSE_BLE` → `BLE_HID` multidispositivo: 3 huecos por tipo (por *appearance* 0x03C1 teclado / 0x03C2 ratón / 0x03C3-0x03C4 joystick/gamepad, y por el *Report Map*), dirección y nombre por tipo en la NVS, olvidar por dispositivo, ventana de emparejado por tipo | no | placa (el ratón no debe empeorar) |
| 2 | Teclado: parser, clave de emparejado, traducción, modos texto/juego, latido; STM32 con matrices por fuente y plazo | no | PC (traducción y cola) + placa |
| 3 | Gamepad como teclas con la configuración de `LOAD *JOY` + botones extra | no | placa con un mando Xbox |
| 4 | Gamepad por comando del MCU (analógico, botones) + ejemplo en `EXAMPLES/` | no | PC + placa |
| 5 | ROM (`LOAD *KEYB`/`LOAD *PAD` con `THEN NEW/RUN/CLEAR/STOP` y `TO A$`, o un selector en `LOAD *MOUSE`), manual, emulador | no | — |
| (6) | Puerto de la FPGA para el gamepad, solo si la 4 se queda corta | sí | ModelSim + placa |

Comando 77, op 3/4/5: añadir un byte de dispositivo (0 ratón, 1 teclado, 2 gamepad) manteniendo la compatibilidad con lo de hoy (sin byte = ratón).
`CMD_BLE` (0x16) llevaría el estado de los tres. Tramas nuevas: p. ej. 0x17 teclado (matriz) y 0x18 gamepad (estado crudo).

## 7. Preguntas para el usuario

1. ~~¿Qué gamepad y qué teclado tienes para probar?~~ Contestada: ver §8.
2. ¿Teclado en distribución española o inglesa para los signos?
3. ¿Comandos BASIC separados (`LOAD *KEYB`, `LOAD *PAD`) o uno común para los tres (`LOAD *BT`)?
4. ¿Hace falta el gamepad analógico (fases 4/6) o basta como teclas (fase 3)?

## 8. Los dispositivos propuestos por el usuario (13/11/2026)

### Gamepad: 8BitDo Micro (ASIN B0CDG2HKBF) — **casi seguro que NO vale con el ESP32-C3**

- Modos: Switch, D-input y teclado (K). La página de 8BitDo no da versión de Bluetooth ni menciona BLE.
- Bluepad32 (la referencia de mandos en ESP32) clasifica la familia 8BitDo como **BR/EDR** (clásico) y avisa de que el S3/C3 no tienen BR/EDR. Hay usuarios que dicen que
  el Micro solo les funciona con el ESP32 original y no con el S3. No hay confirmación oficial, pero todo apunta a **Bluetooth clásico** en los tres modos (el modo
  teclado también: iOS y Windows aceptan teclados clásicos, así que eso no prueba nada).
- Alternativas: un mando que vaya por BLE de verdad. Seguro: **Xbox Wireless con firmware 5.x o posterior** (Series X|S, y los One actualizados). Steam y Stadia, solo
  con su actualización a BLE. «Android» genéricos: unos BLE y otros clásicos, hay que probarlos. Otra vía sería cambiar el C3 por un ESP32 original (tiene BR/EDR y BLE),
  pero eso es otra placa y otra pila (Bluepad32/BTstack), fuera de este estudio.

### Teclado: Hoopond retro de 84 teclas redondas (ASIN B0F6TVCBHW; parece el modelo «308i») — **sin confirmar**

- Los anuncios dicen «2.4GHz wireless Bluetooth» y pilas AAA; el «Bluetooth 5.0» solo sale en anuncios genéricos del mismo diseño, no de Hoopond. No hay hoja técnica.
- El receptor USB de 2,4 GHz no sirve (es propietario). Que el chip sea «Bluetooth 5.0» **no garantiza** que el teclado use HID sobre BLE: muchos teclados baratos
  con chip 5.0 se anuncian como HID clásico.
- Distribución de las teclas: probablemente inglesa (ANSI); sin confirmar.

### Cómo comprobarlo antes de comprar o de programar nada

1. **nRF Connect** (gratis, Android/iOS) en el móvil: poner el dispositivo en modo emparejado y escanear. Si sale con el servicio **0x1812 (HID)** o con *appearance*
   Keyboard / Gamepad / Joystick, es BLE y vale. Si el móvil lo ve en los ajustes de Bluetooth pero nRF Connect no, es clásico y no vale.
2. Con el firmware de hoy del ESP32: `esp ble pair` y luego `esp ble` enseña los HID vistos («visto: ... HID»). Ojo: `pair` **olvida el ratón guardado** y,
   si el teclado es el HID más cercano, intentará conectarse con él (fallará al no encontrar X/Y). Mejor la opción 1.

## 9. Alternativas: mando retro, solo cruceta y botones (13/11/2026)

No se ha encontrado **ningún mando comercial retro (sin analógicos) confirmado como BLE**: los 8BitDo (Micro, Zero 2, SN30, Lite...), los de Nintendo Switch Online y
casi todos los «retro» son Bluetooth clásico. Opciones, de menos a más trabajo:

1. **Mando con cable por el DB9 (hoy, sin código)**. Un mando **pasivo** (solo interruptores a masa): de Master System, Competition Pro, o cualquier compatible Atari.
   Funciona ya con `LOAD *JOY`. Latencia: solo el fotograma.
   Lo que da el conector del SD81 (esquema `zx80 SD_12 (smd) build006.9(STM32).sch`, J2): pines 1-4 direcciones, 6 FIRE, 8 GND, **directos a la FPGA** (Spartan-6, 3,3 V).
   **No hay +5 V en el pin 7 ni segundo botón en el 9**: un mando con dos botones solo da el del pin 6.
2. **Mando BLE casero**: un ESP32-C3 Supermini (o S3) dentro de una carcasa de mando retro (un mando SNES o Master System con cable, o un clon USB vaciado) con la librería
   `ESP32-BLE-Gamepad` de lemmingDev, que admite C3/S3 y cruceta como *hat*. Es BLE con seguridad, y además el *Report Map* lo decidimos nosotros. Hay proyectos parecidos:
   `invisiblesushi/ESP32-BLE-SNES-controller` (SNES → ESP32) y `terenceang/SNES_BLE_HID` (SNES → BLE con otro chip).
3. **Receptor inalámbrico enchufado al DB9** (Atari **CX78+ Wireless**, que trae un receptor DB9 de 2,4 GHz; o un **BlueRetro** de Atari 2600/Master System, con ESP32
   original, que aceptaría el 8BitDo Micro): **no con la placa de hoy**. Necesitan +5 V del pin 7, que no está conectado, y meten señales activas directamente en pines
   de la FPGA que no aguantan 5 V. Exigiría una revisión de la placa: +5 V con polifusible en el pin 7, protección o resistencias serie en las líneas, y el pin 9 como
   segundo botón (otro pin de la FPGA). Un adaptador externo que inyecte 5 V en el pin 7 solo valdría si se comprueba que el receptor no saca más de 3,3 V por las
   líneas: **riesgo de dañar la FPGA**.
4. Mando BLE comercial no retro: Xbox Wireless (firmware 5.x o posterior).

## 10. Teclados recomendados (13/11/2026)

Criterio: que el fabricante diga **Bluetooth Low Energy** en la hoja técnica (no basta «Bluetooth 5.0»). Confirmados por Logitech:

| Teclado | Por qué | Notas |
|---|---|---|
| **Logitech POP Keys** | BLE 5.1 según la hoja de Logitech; teclas redondas de máquina de escribir (aspecto retro) | mecánico, 2 AAA, 3 canales Easy-Switch (el ZX81 en uno y el PC en otro), receptor Bolt opcional |
| **Logitech Pebble Keys 2 K380s** | BLE según Logitech («BLE required for Bluetooth setup») | compacto y plano, 2 AAA, 3 canales |
| Logitech MX Keys / MX Keys Mini / Keys-To-Go 2 | BLE | no retro |

**No confundir**: el **K380 original** (sin «s») es Bluetooth **clásico**: no vale. El POP **Icon** Keys es otro modelo (también BLE, «Low Energy PRO»).
Pendiente con cualquiera: confirmar con nRF Connect (servicio 0x1812) y ver si al emparejar pide clave (Logitech a veces la pide en Windows): por eso la fase 2
incluye enseñar la clave. Sobre las respuestas de otras IA: la clasificación de los 8BitDo como Bluetooth clásico sale de Bluepad32 y de informes de usuarios, no de
8BitDo; nRF Connect lo resuelve en un minuto si se tiene el mando a mano.

## 11. Pruebas del 18/11/2026

- **ShanWan Bluetooth Gamepad Mini**: no aparece en el escaneo BLE del C3 (`Arduino/Gamepad_BLE_test`) ni sale con HID 0x1812: Bluetooth clasico. No sirve.
- **8BitDo Micro** y mandos mini genericos de AliExpress "Switch/Android/iOS/PC": casi seguro clasicos (sin confirmar). **Xbox Wireless** descartado por aspecto y precio (>60 EUR).
- **Camino elegido**: mando clasico (Mega Drive/SNES) + ESP32-C3 como puente BLE, alimentado con LiPo+cargador o 2 AAA. Banco de pruebas: `Arduino/Gamepad_BLE_megadrive` (la placa de pruebas se anuncia como gamepad BLE leyendo el DB9; cableado en la cabecera del sketch; compila, sin probar en hardware). Requiere las librerias ESP32-BLE-Gamepad 0.8.0 y NimBLE-Arduino.
- **Siguiente paso**: comprobar con nRF Connect que "SD81 MD Pad" aparece con HID y sus informes; luego `Gamepad_BLE_test` en otro C3 (o el firmware real del modulo) como receptor.
