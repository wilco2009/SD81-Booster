# Juegos por turnos en red (sin servidor propio): diseño

Estado: **capa A en el ESP32 (§3.5), capa B en el Z80 (§4.4) y capa C con el primer juego, el 4 en raya
(§5.1)**, todas probadas de extremo a extremo en el PC (el Z80 de verdad contra el módem de verdad y un broker
falso) y **sin probar en el emulador ni en hardware**. Punto de partida: el puente de red del módulo WiFi
(`NET_BRIDGE.cpp`, `NET_READ` / `NET_WRITE`, el intérprete AT) y el terminal `EXAMPLES/TELNET`.

## 1. Objetivo y límites

- Dos ZX81 con SD81 Booster juegan por turnos por internet **sin que el proyecto mantenga un servidor**.
- Los comandos AT que se añaden al ESP32 son **genéricos** (MQTT, resumen criptográfico, aleatorios):
  sirven a cualquier aplicación futura de publicar / suscribir, no solo a este juego.
- Lo específico del juego (salas, turnos, firma de mensajes) vive en una **librería del Z80**.
- Por turnos: se aguantan 1-3 s de latencia y caídas cortas. Vale también el juego **asíncrono**
  (el rival no está conectado y ve tu jugada al volver).
- Fuera de alcance: más de dos jugadores (se puede ampliar), voz, tiempo real.

Qué no resuelve: el NAT. Por eso el transporte es un **buzón público** al que los dos se conectan
como clientes (nadie escucha, no hay que abrir puertos).

## 2. Capas

```
  C. el juego (Z80)                 reglas, pantalla, jugadas
  B. librería de red (Z80)          salas, topic, turnos, duplicados, firma, reconexión
  A. comandos AT del ESP32          MQTT, SHA-256 / HMAC, aleatorios      <- genéricos
     (NET_MQTT.cpp, nuevo)
  0. puente existente               NET_POLL (STM32 <-> ESP32), intérprete AT
```

Solo la capa A toca firmware, y solo el del ESP32: `NET_POLL` ya es un puente de bytes y el STM32
no interpreta nada. Las capas B y C son ensamblador Z80.

## 3. Capa A: comandos AT genéricos

### 3.1 MQTT

| Comando | Qué hace |
|---|---|
| `AT+MQTT=host,puerto[,tls[,usuario,clave]]` | conecta a ese broker |
| `AT+MQTT` (sin argumentos) | conecta al primero que funcione de `/SYS/NET.CFG` (ver 3.4) |
| `AT+MQTTID=cadena` | fija el *clientid* (por defecto, uno aleatorio por sesión) |
| `AT+MQTTWILL=topic,retain,texto` | *last will*; se manda **antes** de conectar |
| `AT+MQTTSUB=topic[,qos]` / `AT+MQTTUNSUB=topic` | suscribe o cancela; admite comodines `+` y `#` |
| `AT+MQTTPUB=topic,qos,retain,len` | publica: el módem contesta `>` y a continuación se mandan `len` bytes crudos |
| `AT+MQTTRX` | saca un mensaje de la cola: `+MQTTRX:topic,len` y los `len` bytes; o solo `OK` si no hay |
| `AT+MQTT?` | estado: `+MQTT:estado,broker,en_cola,perdidos,error` |
| `AT+MQTTCLOSE` | cierra la sesión MQTT |

Reglas que hacen que sirva para cualquier aplicación:

- **El payload va por longitud, binario seguro.** No pasa por el búfer de línea AT (`at_cmdbuf[80]`,
  80 bytes) ni se interpretan sus bytes. Es el mismo mecanismo que `AT+CIPSEND`. Máximo 120 bytes
  por mensaje (`WIFI_PROTO_NET_CHUNK` es 128 y hay que dejar sitio al encabezado).
- **La recepción es por sondeo (`AT+MQTTRX`), nunca con mensajes no solicitados.** Un mensaje que
  aparece de golpe en medio de la respuesta a otro comando complicaría mucho el analizador del Z80.
  El ESP32 guarda hasta 8 mensajes de 128 bytes; si se llena, descarta el **más antiguo** y suma a
  `perdidos`. `AT+MQTT?` lo dice.
- **No hay modo de datos.** Los comandos MQTT funcionan en modo comando; `at_command_mode` no
  cambia. Solo durante los `len` bytes que siguen a `>` el intérprete deja de interpretar.
- **Una sesión MQTT a la vez,** independiente del socket TCP del puente (`ATDT`). `ATH` cuelga el TCP,
  `AT+MQTTCLOSE` cierra el MQTT y `ATZ` cierra los dos.
- **Reconexión automática** al broker y a las suscripciones, sin que el Z80 haga nada. Se nota en
  `AT+MQTT?` (`estado` pasa a "reconectando" y vuelve a "conectado").
- **Latencia del puente:** el sondeo es de 250 ms en reposo y de 15 ms con conexión abierta
  (`NET_POLL_IDLE_MS`, `NET_POLL_ACTIVE_MS`). Cada comando con su respuesta cuesta al menos un ciclo.
  Mientras haya sesión MQTT, el ESP32 debe pasar a la cadencia rápida. Para turnos es irrelevante.
- **Errores:** `ERROR` y el último código en `AT+MQTT?` (sin WiFi, DNS, broker rechaza, tiempo agotado,
  topic inválido, cola llena).

### 3.2 Resumen criptográfico y aleatorios

El ESP32 tiene SHA-256 y HMAC por hardware; hacerlos en el Z80 sería código largo y lento.

| Comando | Qué hace |
|---|---|
| `AT+SHA256=len` + `>` y datos | contesta `+SHA256:` y 64 caracteres hexadecimales |
| `AT+HMAC=keylen,datalen` + `>` clave y datos | contesta `+HMAC:` y 64 caracteres hexadecimales (HMAC-SHA256) |
| `AT+RAND=n` | contesta `+RAND:` y `n` bytes en hexadecimal (n ≤ 32), del generador del ESP32 |

La respuesta es texto hexadecimal porque el Z80 lo lee con una rutina sencilla y se puede ver a
mano desde el terminal.

### 3.3 Nombres

Siguen la forma de los comandos de Espressif (`AT+MQTTCONN`, `AT+MQTTSUB`, `AT+MQTTPUB`…). Quiero
mantener `AT+MQTTSUB` y `AT+MQTTPUB` con su forma, por la documentación que ya los conoce. Es de
memoria: comprobarlo contra la documentación de ESP-AT antes de fijar la sintaxis.

### 3.4 Brokers: `/SYS/NET.CFG`

Como `NTP.CFG`, un fichero de texto (se edita en la SD y se sirve por `READ_OPEN`):

```
broker test.mosquitto.org 1883
broker broker.emqx.io 1883
broker broker.hivemq.com 1883
```

`AT+MQTT` sin argumentos los prueba por orden, recuerda el último que funcionó y pasa al siguiente si
uno falla tres veces seguidas. Por defecto **sin TLS** (las jugadas no son secretas y TLS cuesta heap y
tiempo de conexión); una línea `tls` al final del broker lo activa. Con argumentos, `AT+MQTT=` ignora
el fichero.

Brokers candidatos (a medir, ver §7): `test.mosquitto.org`, `broker.emqx.io`, `broker.hivemq.com`,
`mqtt.eclipseprojects.io`. Ninguno da garantías de servicio ni publica límites claros. Como
reserva, ntfy.sh (HTTP en streaming, nunca sondeando por su límite de peticiones) sería un backend
distinto con la misma interfaz AT.

### 3.5 Implementación (ESP32)

Ficheros en `Arduino/Wifi_module_01/`:

| Fichero | Qué es |
|---|---|
| `mqtt_codec.h` | codificador y analizador de paquetes MQTT 3.1.1, sin dependencias (CONNECT con will / usuario / clave, PUBLISH QoS 0 y 1, SUBSCRIBE, UNSUBSCRIBE, PUBACK, PINGREQ, DISCONNECT) |
| `NET_MQTT.h/.cpp` | la sesión, la cola de recibidos, la reconexión y los comandos AT de §3 |
| `NET_BRIDGE.cpp/.h` | despacha `AT+...` a `net_mqtt_at()`, conserva las mayúsculas del comando original (el intérprete pasaba la línea a mayúsculas), recibe los datos crudos tras `>` y llama a `net_mqtt_loop()` |

Cliente MQTT propio, sin librería: así no hay dependencia que instalar y el comportamiento es
el que se ha especificado. Decisiones que no estaban en §3:

- **Códigos de error** (último campo de `AT+MQTT?`): 0 ninguno, 1 sin WiFi, 2 no abre el TCP,
  3 el broker rechaza (CONNACK ≠ 0), 4 tiempo agotado, 5 argumentos malos, 6 cola o búfer lleno,
  7 sin sesión, 8 sin PUBACK. Es el **último** error: se borra al conectar con éxito, no al
  acertar otro comando.
- **Estados** (primer campo): 0 apagado, 1 conectando, 2 conectado, 3 reconectando.
- **Conectar bloquea** hasta ~8 s por broker (4 s el TCP y 4 s el CONNACK), como `ATDT`. Con la
  lista por defecto y todos caídos, hasta ~24 s hasta el `ERROR`. El Z80 debe esperar la
  respuesta sin tiempo límite corto.
- **`AT+MQTTSUB`** espera el SUBACK (3 s). Si el broker lo rechaza (`0x80`), `ERROR` y no se guarda.
  El `qos` por defecto es 0. Si se pide durante una reconexión, se guarda y se suscribe al volver.
- **`AT+MQTTPUB` QoS 1** espera el PUBACK (3 s) y reintenta una vez con DUP; si no llega, `ERROR 8`.
  Con `len` = 0 publica un mensaje vacío sin pedir datos (borra el retenido).
- **Reconexión:** reintenta a los 2 s y luego con espera creciente (4, 8, 16 s, máximo 30 s),
  pasando al siguiente broker de la lista en cada fallo. Cada intento puede bloquear el puente
  hasta ~8 s.
- **Ping:** `PINGREQ` a los 30 s sin tráfico (keepalive de 60 s); si no hay `PINGRESP` en 15 s,
  se considera caída.
- **`AT+MQTTCLOSE` y `ATZ`** cierran la sesión y olvidan suscripciones, mensajes, will, clientid y
  usuario. Volver a conectar con `AT+MQTT` sin cerrar antes conserva el will, el clientid y el
  usuario puestos.
- **Mensajes recibidos** mayores de 120 bytes o con topic de más de 64 se descartan y cuentan en
  `perdidos`.
- **Clientid por defecto:** `sd81-` y 8 caracteres hexadecimales aleatorios.
- **`/SYS/NET.CFG`:** líneas `broker host puerto [tls]`; se ignoran las vacías, las que empiezan por
  `#` y las mal formadas. Sin fichero, o sin ningún broker válido, se usan los tres de §3.4.
  Máximo 6.

**Pruebas** (`claude/mqtttest/`, `sh build.sh`, sin hardware):

1. `test_codec.cpp`: los paquetes con los bytes de la especificación, el analizador byte a byte, un
   PUBLISH enorme que se descarta sin perder el hilo, y una longitud inválida.
2. `test_net_mqtt.cpp`: compila `NET_MQTT.cpp` tal cual contra Arduino / WiFi / mbedTLS falsos y un
   broker falso: comandos y argumentos malos, will, clientid y topics con mayúsculas, SUB, recibir
   QoS 1 retenido, datos binarios, cola llena (8 + 2 perdidos), falta de sitio en la cola del puente,
   PUB QoS 1 con y sin PUBACK (dos intentos, DUP), mensaje vacío, ping, caída y reconexión con
   resuscripción, CLOSE, CONNACK rechazado, lista por defecto, `NET.CFG`, usuario y clave, SHA-256,
   HMAC (separación de clave y datos) y RAND.

No cubren: las clases reales de `WiFiClient` / `WiFiClientSecure`, mbedTLS ni el tiempo real
(comprobar en hardware, ver §7 fase 1).

**Primera prueba en hardware** (con `EXAMPLES/TELNET`, que ya es un terminal de módem):

```
AT+MQTT=test.mosquitto.org,1883
AT+MQTTSUB=sd81test/hola,1
AT+MQTTPUB=sd81test/hola,1,0,5
>hello
AT+MQTTRX
AT+MQTT?
```

## 4. Capa B: la librería del Z80 (`net.inc`)

Todo lo que sigue se hace con los comandos de la capa A.

### 4.1 Salas y seguridad (la justa)

Los topics de un broker público los ve cualquiera que se suscriba a `#`. Por eso el código de sala
**no va en el topic**:

- **Código de sala:** 10 caracteres base 32 (50 bits), sin letras ambiguas, generados con `AT+RAND`.
  La pantalla lo muestra al crear la sala (`K7QX-M2PD-9T`); es lo que el jugador le dice al rival.
- **Topic:** `sd81/` + los primeros 12 hex de `SHA256("topic" + código)`. Quien ve el topic no
  obtiene el código.
- **Clave:** los 64 caracteres hexadecimales de `SHA256("key" + código)`, usados tal cual como los 64
  bytes de la clave del HMAC (no sale del Z80).
- **Cada mensaje** lleva un MAC de 8 bytes (HMAC truncado). El receptor descarta los que no
  verifican. Un intruso que no conoce el código no puede inyectar jugadas.
- Esto protege de **travesuras**, no de un atacante dedicado (50 bits se pueden romper offline).
  El ajedrez no lo necesita más. Los datos van en claro; no mandar nada sensible.

### 4.1b Sala llena (dos jugadores como mucho)

Hay dos ranuras. Antes de entrar como la 1, `room_join` se suscribe solo a `<sala>/p1` y mira la presencia
retenida: si dice `1`, ya hay otro jugador, devuelve el error 6 y cierra la sesión con `AT+MQTTCLOSE` (una
desconexión limpia no dispara el *last will*, así que la presencia del otro no se toca). Si dice `0` o no hay
nada, se desuscribe, entra y publica la suya. Límites: si el otro jugador se cae sin despedirse, su `1` dura
hasta que el broker detecta la caída (~90 s con el keepalive de 60 s) y en ese tiempo la sala parece llena; y
dos que entren a la vez pueden pasar los dos la comprobación (muy improbable).

### 4.2 Mensajes en el broker

Cada jugador tiene su ranura (0 = quien creó la sala, 1 = quien se unió) y publica en su topic:

| Topic | Contenido | Retained |
|---|---|---|
| `sd81/<h>/0`, `sd81/<h>/1` | mensaje de juego del jugador 0 / 1 | sí (la última jugada espera al rival) |
| `sd81/<h>/p0`, `sd81/<h>/p1` | presencia: `1` al conectar; el *last will* publica `0` | sí |

Formato: `ver(1) seq(2 LE) turno(2 LE) datos(0..100) mac(8)`.

- `seq` crece por emisor, desde un valor aleatorio (`AT+RAND=2`) al entrar: el receptor descarta un
  mensaje con la misma secuencia que el último (un QoS 1 repetido). No ordena: con un único mensaje
  retenido por ranura no hace falta, y así un rival que reinicia no se queda bloqueado.
- `turno` lo pone el juego (número de jugada).
- Con *retained* + `seq`, si el rival vuelve tras varios turnos el juego ve que se ha saltado alguno
  y pide el estado completo (§5).

### 4.3 Flujos

- **Crear sala:** `AT+RAND=7` → código → topic y clave (`AT+SHA256`, `AT+HMAC`) → `AT+MQTTWILL` →
  `AT+MQTT` → `AT+MQTTSUB=sd81/<h>/+` (mensajes y presencia del rival) → publicar presencia `1`.
- **Unirse:** igual, con el código tecleado.
- **Enviar una jugada:** `AT+HMAC` (firmar) → `AT+MQTTPUB` con `>` y los bytes (con *retain*).
- **Recibir:** el bucle del juego llama a `AT+MQTTRX`; por cada mensaje, `AT+HMAC` para verificar,
  y descartar si falla o si el `seq` no es nuevo.
- Con el sondeo rápido, cada paso cuesta ~15-30 ms: enviar y recibir son dos o tres idas y vueltas.

Si más adelante el coste de la firma en el Z80 molestara, se puede pasar a un comando que firme y
verifique de una vez; no hace falta de partida.

### 4.4 Implementación (Z80)

`EXAMPLES/NETCHAT/net.inc` (la librería), `netchat.asm` (tabla de saltos para BASIC) y `NETCHAT.B81` (un
chat de prueba). Rutinas: `net_init`, `room_new`, `room_join`, `room_send`, `room_poll`, `room_leave`;
el buzón `NB_*` (resultado, ranura, rival, turno, longitud, código de sala, buffers de 100 bytes) está
en una RAM de trabajo de ~1 KB que define el que la incluye (`NB_BASE`). Detalle en
`EXAMPLES/NETCHAT/README.md`.

Cosas que salieron al probarla:
- **Plazos:** un bucle de ~20 ms. No sirve el contador de VSYNC del puerto `$AF`: leerlo lo pone a
  cero y el handshake lo lee todo el rato.
- `net_read` destruye `D` (el contador de `flush_rx` va en `E`).
- La cabecera `+MQTTRX:topic,len` termina en CR LF y `rx_line` corta en el CR: hay que saltar el LF antes
  de los datos, o el primer byte del mensaje sería el LF.
- Cada `AT+...` tarda 0,25-0,5 s antes de que haya sesión MQTT (el puente sondea cada 250 ms en reposo)
  y ~30 ms después. Entrar en una sala son ~5 s; mandar o recibir, ~0,1 s.
- El broker devuelve a quien se suscribe con comodín lo suyo: se ignoran los mensajes de la propia ranura.
- **Funciona en SLOW y en FAST.** Al probarla en el emulador se colgó en "INIT..." y se atribuyó a la NMI de
  SLOW (que tocaría el reloj `C` del handshake); la NMI de la ROM no toca los registros principales y el
  cuelgue era un `netchat.bin` viejo con el fallo de `flush_rx` ya arreglado. No hace falta `SET_FAST` (se
  probó y se quitó: cambiar de modo en cada llamada hacía que la imagen diera vueltas).

## 5. Protocolo del juego (capa C)

Reglas comunes a todos los juegos:

1. Cada mensaje lleva el **número de turno** y un **resumen del estado** (hash de 1-2 bytes de la
   posición). Si no coincide con el propio, hay desincronización → se pide el estado completo.
2. El **estado completo** cabe en un mensaje (ajedrez: 64 casillas en 4 bits, 32 bytes; damas,
   reversi y 4 en raya, menos).
3. La jugada se **valida en el receptor**, siempre. Un mensaje bueno de un cliente tramposo no
   se acepta si no es legal.
4. Al entrar en una sala con partida empezada, el que llega pide el estado.

### 5.1 Implementación (Z80): `EXAMPLES/NETGAME`

`game.inc` es el controlador común (jugada, turno, hash del estado, estado completo, otra partida) y el
juego pone seis rutinas (`g_reset`, `g_valid`, `g_apply`, `g_winner`, `g_save`, `g_load`). `connect4.inc` son
las reglas del 4 en raya y `connect4.asm` el juego (Superfast texto de 32 columnas con la pantalla
alternativa en `$8000`, teclado leído a mano). Mensajes: `1` jugada con el hash del estado resultante, `2`
pido el estado, `3` estado completo, `4` otra partida. Cualquier desfase se arregla pidiendo el estado
completo. Detalle y cómo añadir otro juego en `EXAMPLES/NETGAME/README.md`.

Pruebas (`claude/mqtttest/build_z80.sh`): las reglas contra una referencia en C++ con ~11.000 jugadas al
azar, el controlador contra un rival hecho en la prueba, y el juego entero con teclado y pantalla simulados.
Salieron dos fallos de la propia pantalla que solo se veían así: `a2z` pisaba `HL` y `kbd_scan` pisa `BC`.

## 6. Cambios en el firmware

### ESP32 (nuevo `NET_MQTT.cpp`; `NET_BRIDGE.cpp` solo enruta)

- Intérprete AT: los comandos de §3, con su modo "recibir `len` bytes crudos tras `>`".
- Cliente MQTT: una librería ligera (PubSubClient o `esp-mqtt`; decidir tras la fase 0), **un
  único** `WiFiClient` reutilizado, cola de recepción de 8 × 128 bytes, reconexión y
  re-suscripción.
- `/SYS/NET.CFG` y el cambio de broker.
- SHA-256, HMAC y aleatorios con mbedTLS y el generador del ESP32.

### STM32

Ninguno. La orden `NET_POLL` y su control de flujo ya sirven.

### Z80

Hecha: `EXAMPLES/NETCHAT/net.inc` (unos 2 KB, ver §4.4).

## 7. Plan

| Fase | Contenido | Se sabe que está hecha cuando |
|---|---|---|
| **0. Medidas** | Sketch para ESP32-C3 que se conecta a cada broker candidato: tiempo de conexión, latencia publicar→recibir, heap libre, comportamiento en 48 h de conexión permanente y al caerse el WiFi | tabla de resultados y lista de brokers elegida |
| **0b. Prototipo sin firmware** (opcional) | Un MQTT mínimo en el Z80 sobre `ATDT broker:1883` (TCP crudo, ya existe): no hace falta tocar el ESP32 | un mensaje de ida y vuelta entre un SD81 y el script de Python |
| **1. Capa A** | `NET_MQTT.cpp`, comandos de §3, `NET.CFG`, reconexión | desde el terminal (`EXAMPLES/TELNET`) se hace `AT+MQTT`, `AT+MQTTSUB`, `AT+MQTTPUB` y `AT+MQTTRX` a mano |
| **2. Librería Z80** | `net.inc` y una pantalla de lobby (crear / unirse, mostrar el código). **Hecha en el PC**: `NETCHAT` (chat de prueba en BASIC con la librería por `USR`) | probada en el PC con un rival propio (`claude/mqtttest/build_z80.sh`); falta el emulador y el hardware |
| **3. Primer juego** (hecho en el PC: `EXAMPLES/NETGAME`, el 4 en raya) | Uno sencillo (reversi, 4 en raya o hundir la flota; este último luce la información oculta) | partida completa entre dos SD81 |
| **4. Ajedrez** | Reglas desde cero (generador y validador de jugadas); motor opcional para jugar en solitario (§9) | partida con jugadas ilegales rechazadas, enroque y al paso |
| **5. Gráficos** | Modo Spectrum / HiRes con Chroma; piezas como sprites (64 en total, 12 por línea) | se hace aparte con otra fuente de arte |

El **rival en Python** (un script con `paho-mqtt` que habla el mismo formato) permite probar con
**un solo** SD81, y sirve de rival automático para las pruebas.

## 8. Qué más se puede hacer con la capa A

Sin comandos nuevos: chat entre ZX81, marcadores globales, domótica (publicar / suscribir a un broker
casero), telemetría de sensores, notificaciones, cualquier juego por turnos.

Lo que **no** cubre: HTTP (APIs, tiempo, descargas), UDP y DNS. Si hicieran falta, serían otra familia
(`AT+HTTP=GET,url`…) con el mismo estilo: argumentos cortos, respuesta por longitud y sondeo.

## 9. Motor de ajedrez (opcional, para jugar contra la máquina)

Para humano contra humano no hace falta. Para jugar en solitario:

- **Escribir uno** con un alfa-beta sencillo: sin problemas de licencia, el código encaja con el resto.
- **Portar micro-Max** (motor de ajedrez diminuto en C, de licencia libre según su autor; comprobar
  los términos) con SDCC o z88dk.
- **Delegar en el STM32** (168 MHz, 192 KB): más fuerte, pero requiere una orden nueva del MCU.
- **Sargon en CP/M** con `CPM3_SD81`: casi sin cambios, con gráficos de texto.

Desaconsejado reutilizar los desensamblados de Cyrus o Psion Chess: derivan de código comercial y
dependen de la pantalla y la ROM del Spectrum. Las autorías y los ELO de los listados que circulan
no están verificados.

## 10. Riesgos

| Riesgo | Mitigación |
|---|---|
| Un broker público desaparece o cambia | lista en `NET.CFG`, cambio automático; ntfy de reserva; en el peor caso, un relay propio (Cloudflare Workers) sin tocar el juego |
| Fugas de heap con TLS / reconexiones (se han visto en arduino-esp32) | un solo cliente reutilizado, sin TLS por defecto, medir 48 h en la fase 0 |
| Cualquiera puede escribir en el topic | MAC con clave derivada del código (§4.1) |
| Latencia alta o mensajes perdidos con QoS 0 | QoS 1 + `seq` y repetidos descartados + *retained* |
| La cola de 8 mensajes se llena si el Z80 tarda | descarta el más antiguo y lo cuenta; el juego lo detecta por `turno` y pide el estado (§5) |
| Cada comando cuesta un ciclo del puente | sondeo rápido mientras haya sesión; irrelevante por turnos |
| Dos jugadas simultáneas / desincronización | turno + hash del estado en cada mensaje (§5) |
| El código de sala se filtra | quien lo conoce puede jugar; cambiarlo es crear otra sala |
