# NETCHAT: la capa B del juego en red

`net.inc` es la **librería Z80** del juego en red (capa B de `claude/planning/juego_en_red.md`): un canal
de mensajes cortos, firmados y entre dos ZX81, por MQTT y **sin servidor propio**. Usa los comandos AT
genéricos del módulo WiFi (capa A, `NET_MQTT.cpp`). `netchat.asm` es la tabla de saltos para llamarla
desde BASIC, y `NETCHAT.B81` es un chat de prueba.

## Probarlo

```
pasmo netchat.asm netchat.bin netchat.sym        (o compila.bat)
```

Copia `netchat.bin` y `NETCHAT.B81` (pásalo a `.P`) a la misma carpeta de la SD, carga el programa y:

1. En un ZX81 elige `1` (sala nueva). Sale un código de 10 caracteres (por ejemplo `7S63V4V3WH`).
2. En el otro elige `2` y teclea el código.
3. `S` para mandar una línea; `Q` para salir. El que no está conectado recibe la última jugada al volver
   (los mensajes van retenidos en el broker).

**Funciona en SLOW** (con la NMI activa) y en FAST: la librería no cambia de modo. (Al principio se creyó que
la NMI rompía el handshake con el MCU; era un `netchat.bin` viejo con un fallo ya arreglado, en `flush_rx`.)
El bucle del chat hace `PAUSE 25` entre sondeos para que la pantalla se vea en FAST, y vale igual en SLOW.
El parpadeo que se ve cada medio segundo en el chat es el del propio `PAUSE` del ZX81 (un bucle de
`PAUSE 25: GOTO 10` hace lo mismo): no lo causa la librería. Un juego no usa `PAUSE`.

El BASIC es solo una prueba (no está probado en la placa ni en el emulador): `INPUT` bloquea y no se
sondea mientras se teclea. Los mensajes llevan los códigos de carácter del ZX81, así que se entienden de
ZX81 a ZX81.

## Cómo se usa desde ensamblador (juegos)

```asm
NB_BASE     equ 7000h          ; RAM de trabajo de la librería (1,2 KB)
            org 24576
            ...
            include "net.inc"
```

Resultados: 0 bien, 1 el módem dijo ERROR, 2 plazo agotado, 3 respuesta inesperada, 4 código inválido, 5 mensaje
demasiado largo, 6 sala llena (ya hay otro jugador en la ranura 1: no entra y no toca su presencia).

Rutinas (CALL; A = resultado, también en `NB_RES`): `net_init`, `room_new`, `room_join`, `room_send`,
`room_poll`, `room_leave`, y `room_poll_send` / `room_poll_recv` (el sondeo en dos pasos: `_send` solo manda
la pregunta y vuelve al momento, y `_recv` recoge la respuesta sin esperar si ya ha llegado; entre las dos el
programa pinta o espera un fotograma. Es lo que debe usar un bucle de juego). El buzón (`NB_*`) y los códigos de resultado están al principio de `net.inc`.

## Protocolo (resumen; el detalle, en `juego_en_red.md` §4)

- Código de sala: 10 caracteres de un alfabeto de 32 (50 bits), del `AT+RAND`.
- Topic: `sd81/` + 12 hexadecimales de `SHA256("topic" + código)`; clave: los 64 hexadecimales de
  `SHA256("key" + código)`, usados como bytes de la clave HMAC.
- Mensaje (`<sala>/<ranura>`, QoS 1, retenido): versión (1), secuencia (2), turno (2), datos (0-100) y
  los 8 primeros bytes de `HMAC-SHA256(clave, el resto)`.
- Presencia (`<sala>/p<ranura>`, retenida): `1` al entrar, `0` al salir o, por el *last will*, al caerse.
- El receptor descarta lo de otra ranura que no sea el rival, lo que no verifica y la secuencia repetida.
- La presencia no lleva firma: es solo informativa.

## Pruebas en el PC

`claude/mqtttest/build_z80.sh` ejecuta `netchat.bin` en el Z80 de EightyOne contra el módem de verdad
(`NET_BRIDGE.cpp` y `NET_MQTT.cpp`) y un broker falso, con un rival que usa SHA-256 y HMAC propios:
sala nueva, unirse, presencia, mensajes con CR, LF y 0 en los datos, repetidos, firma mala, eco propio,
mensajes retenidos (juego asíncrono), escrituras parciales del tubo y un código malo.

**Notas de implementación**
- Los plazos usan un bucle de ~20 ms y no los VSYNC: leer el puerto `$AF` pone a cero su contador y el
  handshake lo lee todo el rato.
- `net_read` destruye `D`; `C` es el reloj del handshake y `A` no sobrevive a `mcu_send`.
- `rx_line` corta en CR o LF; tras la cabecera `+MQTTRX:topic,len` hay que saltar el LF antes de los datos.
