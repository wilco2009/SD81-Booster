# Juegos por turnos en red (sin servidor propio): diseño

Estado: **propuesta**, sin implementar. Punto de partida: el puente de red del módulo WiFi
(`NET_BRIDGE.cpp`, `NET_READ` / `NET_WRITE`, el intérprete AT) y el terminal `EXAMPLES/TELNET`.

## 1. Objetivo y límites

- Dos ZX81 con SD81 Booster juegan por turnos por internet **sin que el proyecto mantenga un servidor**.
- El juego en el Z80 no sabe nada de la red: ve un canal de mensajes cortos, fiable y con el rival
  identificado. Cambiar de transporte no toca el juego.
- Por turnos: se aguantan 1-3 s de latencia y caídas cortas. Vale también el juego **asíncrono**
  (el rival no está conectado y ve tu jugada al volver).
- Fuera de alcance: más de dos jugadores (se puede ampliar), voz, tiempo real.

Qué no resuelve: el NAT. Por eso el transporte es un **buzón público** al que los dos se conectan
como clientes (nadie escucha, no hay que abrir puertos).

## 2. Transporte

Hay un solo backend por defecto y uno de reserva. El Z80 no distingue.

| Backend | Uso | Por qué |
|---|---|---|
| **MQTT** en un broker público | por defecto | conexión permanente (sin sondeo), mensajes *retained* (juego asíncrono), *last will* (aviso de desconexión), mensajes de pocos bytes |
| **ntfy.sh** (HTTP, suscripción en streaming) | reserva | HTTP puro, con caché (`since=`). Hay un límite de peticiones por IP, así que **nunca sondear**: suscripción en streaming |

Brokers candidatos (a medir, ver §7): `test.mosquitto.org`, `broker.emqx.io`, `broker.hivemq.com`,
`mqtt.eclipseprojects.io`. Ninguno da garantías de servicio ni publica límites claros.

### Lista de brokers: `/SYS/NET.CFG`

Como `NTP.CFG`, un fichero de texto (se puede editar en la SD y se sirve por `READ_OPEN`):

```
broker test.mosquitto.org 1883
broker broker.emqx.io 1883
broker broker.hivemq.com 1883
```

El ESP32 prueba por orden, recuerda el último que funcionó y pasa al siguiente si uno falla tres
veces seguidas. Por defecto **sin TLS** (las jugadas no son secretas y TLS cuesta heap y
tiempo de conexión). Una línea `tls` al final del broker lo activa.

## 3. Salas y seguridad (la justa)

Los topics de un broker público los ve cualquiera que se suscriba a `#`. Por eso el código de sala
**no va en el topic**:

- **Código de sala:** 10 caracteres base 32 (50 bits), sin letras ambiguas, que muestra la pantalla
  al crear la sala (`K7QX-M2PD-9T`). Es lo que el jugador le dice al rival.
- **Topic:** `sd81/` + los primeros 12 hex de `SHA256("topic" + código)`. Quien ve el topic no
  obtiene el código.
- **Clave:** `HMAC-SHA256("key" + código)`.
- **Cada mensaje** lleva un MAC de 8 bytes (HMAC truncado). El receptor descarta los que no
  verifican. Un intruso que no conoce el código no puede inyectar jugadas.
- Esto protege de **travesuras**, no de un atacante dedicado (50 bits se pueden romper offline).
  El ajedrez no lo necesita más. Los datos van en claro; no mandar nada sensible.

El ESP32 tiene SHA y HMAC por hardware en mbedTLS.

## 4. Mensajes

### 4.1 En el broker

Cada jugador tiene su ranura (0 = quien creó la sala, 1 = quien se unió) y publica en su topic:

| Topic | Contenido | Retained |
|---|---|---|
| `sd81/<h>/0`, `sd81/<h>/1` | mensaje de juego del jugador 0 / 1 | sí (la última jugada espera al rival) |
| `sd81/<h>/p0`, `sd81/<h>/p1` | presencia: `1` al conectar; el *last will* publica `0` | sí |

Formato del mensaje: `ver(1) seq(2 LE) turno(2 LE) datos(0..100) mac(8)`.

- `seq` crece por emisor: el receptor ignora repetidos y viejos.
- `turno` lo pone el juego (número de jugada). Con *retained* + `seq`, si el rival vuelve tras
  varios turnos el juego ve que se ha saltado alguno y pide el estado completo (§5).

### 4.2 Entre el Z80 y el ESP32

En modo datos el canal es **orientado a tramas** (no a bytes como en telnet):

```
trama = len(1)  tipo(1)  datos(len-1)         len = 1..120, incluye el tipo
```

| Tipo | Sentido | Significado |
|---|---|---|
| 0 | los dos | mensaje de juego (el ESP32 lo firma / verifica y numera) |
| 1 | ESP32 → Z80 | el rival se ha conectado |
| 2 | ESP32 → Z80 | el rival se ha desconectado (last will) |
| 3 | ESP32 → Z80 | se perdió el broker; el ESP32 reintenta solo. Al volver llega otro 1 si el rival sigue |
| 4 | Z80 → ESP32 | "pide el estado completo al rival" (lo reenvía como tipo 0 con una marca) |

El ESP32 se ocupa de MAC, `seq`, duplicados, reconexión y presencia. El Z80 solo ve mensajes buenos.

## 5. Protocolo del juego (Z80)

Fuera del alcance de este documento, pero con unas reglas comunes para todos los juegos:

1. Cada mensaje lleva el **número de turno** y un **resumen del estado** (hash de 1-2 bytes de la
   posición). Si no coincide con el propio, hay desincronización → se pide el estado completo.
2. El **estado completo** cabe en una trama (ajedrez: 64 casillas en 4 bits, 32 bytes; damas,
   reversi y 4 en raya, menos).
3. La jugada se **valida en el receptor**, siempre. Un mensaje bueno de un cliente tramposo no
   se acepta si no es legal.
4. Al entrar en una sala con partida empezada, el que llega pide el estado.

## 6. Cambios en el firmware

### ESP32 (`NET_BRIDGE.cpp`, nuevo `NET_GAME.cpp`)

Nuevo modo de conexión además de `ATDT`: el mismo `client`/`state`/`pending`, pero los datos que
escribe el Z80 pasan por un analizador de tramas y salen por MQTT; lo que llega de MQTT vuelve al
Z80 como tramas. Comandos AT nuevos:

| Comando | Qué hace |
|---|---|
| `AT+ROOM` | crea una sala: contesta `ROOM K7QXM29T9T` y `OK` |
| `AT+ROOM=K7QXM29T9T` | se une a una sala; `CONNECT` (modo datos con tramas) o `NO CARRIER` |
| `AT+GAME=ajd` | espacio de nombres del juego (1-4 caracteres) en el topic; se hashea con el código, solo evita cruces entre juegos |
| `ATH` | sale de la sala (publica la presencia `0`) |
| `+++` / `ATO` | como siempre |

El intérprete ya pasa a mayúsculas todo el argumento. Los códigos son de mayúsculas y cifras, así
que no estorba.

### STM32 y Z80

**No hace falta tocar el STM32:** `NET_POLL` ya es un puente de bytes. El Z80 usa
`NET_READ` / `NET_WRITE` como en `telnet.asm`; una librería `net.inc` de ~1 KB con
`net_send(tipo, buf, n)` y `net_recv(buf)` basta.

## 7. Plan

| Fase | Contenido | Se sabe que está hecha cuando |
|---|---|---|
| **0. Medidas** | Sketch para ESP32-C3 que se conecta a cada broker candidato: tiempo de conexión, latencia publicar→recibir, heap libre, comportamiento en 48 h de conexión permanente y al caerse el WiFi | tabla de resultados y lista de brokers elegida |
| **1. Transporte** | `NET_GAME.cpp`, `AT+ROOM`, firma / verificación, `NET.CFG`, reconexión | dos ESP32 (o uno + un script Python con `paho-mqtt` que hace de rival) intercambian tramas |
| **2. Librería Z80** | `net.inc` y una pantalla de lobby (crear / unirse, mostrar el código) | prueba con el script de Python como rival |
| **3. Primer juego** | Uno sencillo (reversi, 4 en raya o hundir la flota; este último luce la información oculta) | partida completa entre dos SD81 |
| **4. Ajedrez** | Reglas desde cero (generador y validador de jugadas); motor opcional para jugar en solitario (§8) | partida con jugadas ilegales rechazadas, enroque y al paso |
| **5. Gráficos** | Modo Spectrum / HiRes con Chroma; piezas como sprites (64 en total, 12 por línea) | se hace aparte con otra fuente de arte |

El **rival en Python** (un script con `paho-mqtt` que habla el mismo formato) permite probar con
**un solo** SD81, y sirve de rival automático para las pruebas.

## 8. Motor de ajedrez (opcional, para jugar contra la máquina)

Para humano contra humano no hace falta. Para jugar en solitario:

- **Escribir uno** con un alfa-beta sencillo: sin problemas de licencia, el código encaja con el resto.
- **Portar micro-Max** (motor de ajedrez diminuto en C, de licencia libre según su autor; comprobar
  los términos) con SDCC o z88dk.
- **Delegar en el STM32** (168 MHz, 192 KB): más fuerte, pero requiere una orden nueva del MCU.
- **Sargon en CP/M** con `CPM3_SD81`: casi sin cambios, con gráficos de texto.

Desaconsejado reutilizar los desensamblados de Cyrus o Psion Chess: derivan de código comercial y
dependen de la pantalla y la ROM del Spectrum. Las autorías y los ELO de los listados que circulan
no están verificados.

## 9. Riesgos

| Riesgo | Mitigación |
|---|---|
| Un broker público desaparece o cambia | lista en `NET.CFG`, cambio automático; ntfy de reserva; en el peor caso, un relay propio (Cloudflare Workers) sin tocar el juego |
| Fugas de heap con TLS / reconexiones (se han visto en arduino-esp32) | un solo cliente reutilizado, sin TLS por defecto, medir 48 h en la fase 0 |
| Cualquiera puede escribir en el topic | MAC con clave derivada del código (§3) |
| Latencia alta o mensajes perdidos con QoS 0 | QoS 1 + `seq` y repetidos descartados + *retained* |
| Dos jugadas simultáneas / desincronización | turno + hash del estado en cada mensaje (§5) |
| El código de sala se filtra | quien lo conoce puede jugar; cambiarlo es crear otra sala |
