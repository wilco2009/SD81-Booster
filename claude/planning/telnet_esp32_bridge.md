# Comunicaciones tipo telnet (BBS) — planteamiento

Puente de datos entre el módulo WiFi (ESP32-C3) y el Z80, pasando por el
STM32. Acordado en conversación; **nada implementado todavía**.

## Modelo: el ESP32 es un módem ya conectado

El ESP32 gestiona WiFi, socket TCP, destino y reconexión. Al Z80 le llega
un flujo de bytes crudo, exactamente como si tuviera un puerto serie con
un módem delante. Así los programas de terminal/BBS existentes funcionan
sin saber nada de red.

| Componente | Responsabilidad |
|---|---|
| **ESP32** | WiFi, socket TCP, intérprete AT, destino. Sondea el UART. |
| **STM32** | Dos buffers circulares de 2 KB. No interpreta nada, sólo almacena. |
| **Z80** | Dos comandos MCU: leer bytes, escribir bytes, mirar estado. |

**Socket TCP crudo**, sin negociación telnet (IAC/DO/WILL). Si algún día
hace falta, se mastica en el ESP32 y al Z80 le siguen llegando datos
limpios.

Nada de FPGA. Nada de ROM Z80 hasta la capa de usuario (fase 3).

## El condicionante que manda sobre todo

De `WIFI_PROTOCOL.h`: *"Todas las peticiones las inicia SIEMPRE el ESP32
— el STM32 nunca manda una trama sin que se la hayan pedido, para no
necesitar arbitraje en el UART"*.

Eso parte el problema en dos mitades asimétricas:

- **Entrante** (socket → Z80): el ESP32 recibe y empuja cuando quiere.
  Encaja de forma natural.
- **Saliente** (Z80 → socket): el STM32 tiene los bytes pero no puede
  avisar. El ESP32 **tiene que sondear**.

Por eso el sondeo es periódico y permanente, no sólo con sesión abierta:
con ATDT, el propio comando de marcado viaja por el canal de datos, así
que en reposo también hay que recoger lo que el Z80 escribe.

**Cadencia adaptativa**: ~250 ms en reposo, ~10-20 ms con conexión
abierta. Importa afinarlo porque el STM32 también hace audio (AY, voz,
WAV) y cada trama le roba tiempo de `loop()`.

## Trama fusionada, con bit de secuencia

Una sola trama lleva los dos sentidos: cada trama ya es petición +
respuesta, y desperdiciarla en un solo sentido dobla la latencia.

```
CMD_NET_POLL  req:  flags(1B)      bit0 = seq de estos datos, bit1 = ack del otro sentido
                    status(1B)     0=sin conexion 1=conectando 2=conectado 3=error
                    len(1B), data(len)          <- del socket al Z80
              resp: flags(1B)      mismos bits, en el sentido contrario
                    rx_free(1B)    hueco libre en net_rx, en trozos de 16B
                    len(1B), data(len)          <- del Z80 al socket
```

El `status` va en la **petición**, no en la respuesta: quien conoce el estado
del socket es el ESP32. El STM32 sólo lo memoriza para poder contestárselo al
Z80 cuando pregunte con los comandos MCU. (En la primera versión de este
planteamiento estaba al revés; se corrigió al implementarlo.)

**Inicialización del bit de secuencia**: el "último visto" arranca al
contrario del primero que llegará (`net_seq_in = true` en el STM32,
`net_seq_in_last = true` en el ESP32, frente a `seq_out = false` en ambos). Si
empezaran iguales, la primera trama con datos se tomaría por un reintento y se
descartaría.

**Bug real encontrado y corregido al validar en hardware**: la primera
implementación, además de fijar ese valor inicial, *también* sincronizaba
el "último visto" con el bit ajeno en **cada trama vacía** (`else if
(len==0) net_seq_in = in_seq;`), no solo al arrancar. Durante los sondeos
de silencio previos a que exista ningún dato real, eso hacía converger a
las dos partes a secuencia `0` sin que hubiera pasado nada — y como el
primer mensaje de verdad *también* sale con secuencia `0` (el emisor
tampoco ha tocado su propio bit todavía), el receptor lo confundía con
ese `0` ya "visto" durante el silencio y lo descartaba como duplicado
para siempre, sin ACK posible: bloqueo permanente. La trampa: **el bit
de secuencia solo tiene sentido cuando hay datos que aceptar o
rechazar**; con `len==0` no hay nada que decidir, así que no hay que
tocar el rastreador en absoluto. Arreglado quitando esa rama en
los dos lados (`WIFI_HANDLER.cpp` del STM32 y `WIFI_CLIENT.cpp` del
ESP32) — la inicialización sigue haciendo falta, pero la sincronización
"de cortesía" en tramas vacías no.

**Si no caben todos los bytes entrantes, no se acepta ninguno** y no se mueve
la secuencia, para que el emisor reintente. Aceptar sólo una parte avanzando
la secuencia perdería el resto en silencio.

**El bit de secuencia (alternating bit) no es opcional.**
`wifi_handler_poll()` se llama desde `loop()`, así que mientras el Z80
está dentro de un comando MCU largo el `loop()` del STM32 no corre y el
ESP32 hace timeout. Con sondeo cada 10-20 ms eso pasará constantemente.
El STM32 sólo consume los datos entrantes si el bit **cambió**, y no
descarta los salientes de `net_tx` hasta ver el ack: un reintento repite
la trama tal cual, sin duplicar ni perder.

`rx_free` es control de flujo por crédito: el ESP32 nunca manda más de lo
que cabe. Sin esto, un Z80 que no lee a tiempo pierde datos en silencio.

**128 bytes máximo por sentido**, no 256: `WIFI_PROTO_MAX_FRAME_PAYLOAD`
vale 264 y dimensiona buffers fijos en los dos lados (el fichero debe
seguir siendo idéntico en ambos proyectos). 128+128 más cabeceras entra
sin tocarlo. A 10 ms son 12,8 KB/s por sentido; un BBS a 2400 baudios son
240 B/s.

## Comandos MCU (Z80 ↔ STM32)

Libres a partir del **66 (0x42)** — `LAST_COMMAND` está en 65; los huecos
22/39/51 están reservados, no reutilizarlos. Mismo patrón que
`cmd_f_read`/`cmd_f_write`:

```
66 (0x42) cmd_net_read   Z80: max(1B)
                         MCU: count(1B), data(count), avail(1B), status(1B)
67 (0x43) cmd_net_write  Z80: count(1B), data(count)
                         MCU: accepted(1B), status(1B)
```

- `net_read` devuelve **el count real primero**: el Z80 no sabe cuántos
  hay. `avail` son los que quedan pendientes tras la lectura (saturado a
  255), así que con `max=0` el comando es un "¿hay algo?" barato que
  además refresca el estado — no hace falta un tercer comando.
- `net_write` devuelve **cuántos aceptó**, no sólo OK/error: si `net_tx`
  está lleno, el Z80 tiene que saber cuántos reenviar.

El contrato completo, con el código Z80 de referencia y los casos que hay
que reproducir, está en [net_bridge_emulator.md](net_bridge_emulator.md).

No hay bit libre en el puerto `$AF` para señalizar "hay datos": bit 7 es
el handshake, bits 6-1 el contador de vsync, bit 0 la interrupción. De
ahí que el sondeo del Z80 sea por comando.

## Configuración

**Por web**, en el servidor que ya corre en el ESP32, siguiendo el patrón
de `/ntp`:

```
/telnet              GET   host, puerto, estado de la conexion
/telnet/connect      POST  abre el socket
/telnet/disconnect   POST  lo cierra
```

**NO hay TELNET.CFG.** `WIFI.CFG` y `NTP.CFG` son ficheros porque el
ESP32 los necesita al arrancar sin que nadie se lo pida (el de WiFi por
el huevo y la gallina; el de NTP porque sincroniza nada más conectar).
Conectarse a un BBS es siempre una acción deliberada, y cuando la haces
ya hay red y por tanto web. Si algún día se quiere una lista de
favoritos persistente, la web puede guardarla con
`wifi_client_write_open/chunk/close`, igual que hace `handleNtpSave()`.

## Comandos AT (fase 2.5)

Vía principal para el uso desde CP/M; la web queda como comodidad. Un
terminal de CP/M no sabe nada de la página web, pero mandar
`ATDT host:23` por el puerto serie lo sabe hacer cualquiera — y es el
estándar de facto del retro-BBS (WiFi232, Zimodem).

No cuesta nada en el protocolo: los comandos AT viajan por el mismo canal
de datos (comandos 66/67), el STM32 sigue siendo un tubo tonto y el UART
no cambia. Todo el trabajo es un intérprete de texto en el ESP32.

Puntos donde esto se implementa mal normalmente:

- **Modo comando vs modo datos**: sin conexión, lo que llega del Z80 se
  interpreta como AT; con conexión, va al socket tal cual.
- **La secuencia de escape `+++` necesita guard time**: un segundo de
  silencio antes y después, o una transferencia binaria que contenga tres
  símbolos de suma tira la conexión a media faena.
- **`ATE1`/`ATE0`** para el eco, o en modo comando se teclea a ciegas.

Respuestas de módem (`OK`, `CONNECT`, `NO CARRIER`, `ERROR`) para que el
programa del ZX81 ni tenga que mirar el byte de estado.

Web y AT manipulan el mismo estado del ESP32 (`net_bridge_connect`/
`net_bridge_disconnect`, ver `NET_BRIDGE.h`), conviven sin más — da igual
si la conexión se abrió desde `/telnet` o con `ATDT`, el resultado deja
el intérprete en modo datos igual.

**Implementado en `NET_BRIDGE.cpp`** (`ATDT host:puerto`, `ATDL`, `ATH`,
`ATO`, `ATE0`/`ATE1`, `ATZ`, `+++` con guard time de 1s antes y después,
eco en modo comando). `ATDL` (redial) es lo único que el modelo Hayes
hace "por su cuenta" ante una conexión perdida: nada de reconexión
automática de verdad, ni aquí ni en un módem real ni en WiFi232/Zimodem
— siempre es el software (o el usuario) quien decide volver a marcar.
Detalles que se resolvieron al escribirlo:

- El comando se acumula byte a byte hasta un `CR` (se ignora un `LF`
  suelto detrás); mientras tanto cada byte se eco-envía si `ATE1`.
- Modo comando y modo datos son una bandera aparte de `state`
  (`at_command_mode`), no un simple `state==CONNECTED`: hace falta para
  que `+++` pueda entrar en modo comando SIN colgar la conexión.
- Mientras se está en modo comando con una conexión todavía viva (tras un
  `+++`), el ESP32 deja de leer del socket a propósito — los datos se
  quedan en el buffer TCP del sistema operativo, sin perderse, hasta que
  `ATO` retoma la lectura.
- Las respuestas del intérprete (`OK`/`CONNECT`/`NO CARRIER`/`ERROR`) y el
  eco de los comandos comparten una cola con lo que llega del socket de
  verdad (`txq` en `NET_BRIDGE.cpp`) — así `net_bridge_loop()` no necesita
  saber de dónde salió cada byte, solo drenar la cola hacia `pending`.
- `ATDT` NO puede analizar el host directamente sobre el buffer de
  conexión activa (`host_buf`): sería un `snprintf(host_buf,...,host_buf)`,
  origen y destino solapados — se analiza a un buffer local aparte y
  solo entonces se llama a `net_bridge_connect()`.

## Fases

1. **ESP32 + STM32 solos** — *validada en hardware real* (`test/echo_server.py`
   como servidor de eco). `CMD_NET_POLL` (0x0F), los dos circulares, y la
   página `/telnet` para abrir la conexión sin Z80. El modo de prueba
   `NET_BRIDGE_TEST` en `WIFI_HANDLER.cpp` hace de Z80 simulado; se deja
   en `0` ahora que empieza la fase 2, para no competir por los buffers
   con el tráfico real del Z80.
2. **Comandos MCU 66/67** — *validada en hardware real*, con
   `test/net_test.asm` (terminal mínimo en modo nativo, sin Superfast).
   El contrato y el código Z80 de referencia, en
   [net_bridge_emulator.md](net_bridge_emulator.md).
3. **2.5 — Intérprete AT** — *implementado, sin probar en hardware*
   (`NET_BRIDGE.cpp`, ver más arriba).
4. **Página web** (ya existe, `/telnet`), reconexión automática, y la
   capa de usuario en BASIC/ROM.

## Riesgos

- **Timeout/reintento por comandos MCU largos** — resuelto por diseño con
  el bit de secuencia, pero es lo primero que hay que validar.
- **Interferencia con el audio del STM32**: el sondeo constante consume
  `loop()`. De ahí la cadencia adaptativa.
- Telnet no toca la SD, así que **no se reabre** el problema de
  concurrencia que obligó a retirar `CMD_GET_WIFI_CFG`.
