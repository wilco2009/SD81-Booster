# Juegos por turnos en red (sin servidor propio): diseño

Estado: **propuesta**, sin implementar. Punto de partida: el puente de red del módulo WiFi
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

## 4. Capa B: la librería del Z80 (`net.inc`)

Todo lo que sigue se hace con los comandos de la capa A.

### 4.1 Salas y seguridad (la justa)

Los topics de un broker público los ve cualquiera que se suscriba a `#`. Por eso el código de sala
**no va en el topic**:

- **Código de sala:** 10 caracteres base 32 (50 bits), sin letras ambiguas, generados con `AT+RAND`.
  La pantalla lo muestra al crear la sala (`K7QX-M2PD-9T`); es lo que el jugador le dice al rival.
- **Topic:** `sd81/` + los primeros 12 hex de `SHA256("topic" + código)`. Quien ve el topic no
  obtiene el código.
- **Clave:** `HMAC-SHA256("key" + código)` (no sale del Z80).
- **Cada mensaje** lleva un MAC de 8 bytes (HMAC truncado). El receptor descarta los que no
  verifican. Un intruso que no conoce el código no puede inyectar jugadas.
- Esto protege de **travesuras**, no de un atacante dedicado (50 bits se pueden romper offline).
  El ajedrez no lo necesita más. Los datos van en claro; no mandar nada sensible.

### 4.2 Mensajes en el broker

Cada jugador tiene su ranura (0 = quien creó la sala, 1 = quien se unió) y publica en su topic:

| Topic | Contenido | Retained |
|---|---|---|
| `sd81/<h>/0`, `sd81/<h>/1` | mensaje de juego del jugador 0 / 1 | sí (la última jugada espera al rival) |
| `sd81/<h>/p0`, `sd81/<h>/p1` | presencia: `1` al conectar; el *last will* publica `0` | sí |

Formato: `ver(1) seq(2 LE) turno(2 LE) datos(0..100) mac(8)`.

- `seq` crece por emisor: el receptor ignora repetidos y viejos.
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

## 5. Protocolo del juego (capa C)

Reglas comunes a todos los juegos:

1. Cada mensaje lleva el **número de turno** y un **resumen del estado** (hash de 1-2 bytes de la
   posición). Si no coincide con el propio, hay desincronización → se pide el estado completo.
2. El **estado completo** cabe en un mensaje (ajedrez: 64 casillas en 4 bits, 32 bytes; damas,
   reversi y 4 en raya, menos).
3. La jugada se **valida en el receptor**, siempre. Un mensaje bueno de un cliente tramposo no
   se acepta si no es legal.
4. Al entrar en una sala con partida empezada, el que llega pide el estado.

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

Una librería `net.inc` de ~1-2 KB: sala, topic, firma, `net_send(buf, n)` y `net_recv(buf)`. Usa
`NET_READ` / `NET_WRITE` como `telnet.asm`.

## 7. Plan

| Fase | Contenido | Se sabe que está hecha cuando |
|---|---|---|
| **0. Medidas** | Sketch para ESP32-C3 que se conecta a cada broker candidato: tiempo de conexión, latencia publicar→recibir, heap libre, comportamiento en 48 h de conexión permanente y al caerse el WiFi | tabla de resultados y lista de brokers elegida |
| **0b. Prototipo sin firmware** (opcional) | Un MQTT mínimo en el Z80 sobre `ATDT broker:1883` (TCP crudo, ya existe): no hace falta tocar el ESP32 | un mensaje de ida y vuelta entre un SD81 y el script de Python |
| **1. Capa A** | `NET_MQTT.cpp`, comandos de §3, `NET.CFG`, reconexión | desde el terminal (`EXAMPLES/TELNET`) se hace `AT+MQTT`, `AT+MQTTSUB`, `AT+MQTTPUB` y `AT+MQTTRX` a mano |
| **2. Librería Z80** | `net.inc` y una pantalla de lobby (crear / unirse, mostrar el código) | prueba con el script de Python como rival |
| **3. Primer juego** | Uno sencillo (reversi, 4 en raya o hundir la flota; este último luce la información oculta) | partida completa entre dos SD81 |
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
