# Latencia de la red y juegos en tiempo real (análisis del 19 de octubre de 2026)

Pregunta del usuario: ¿se podría montar con el hardware actual algo con menos latencia que permita juegos en tiempo real? Y: ¿cómo quedarían las tareas
del STM32 con un sondeo más rápido, y se podría despertar al STM32 desde el ESP32 sin sondeo?

## Cómo está hecho hoy (comprobado en el código)

Camino: Z80 → MCU (comandos 66/67 `NET_READ`/`NET_WRITE`, por interrupciones) → búferes `net_rx_buf`/`net_tx_buf` del STM32 → UART a 921.600 baudios →
ESP32 → WiFi → servidor MQTT público → ... y la vuelta.

- **Sondeo Z80 → MCU**: decisión de software de los juegos (pregunta cada ~0,4-0,5 s con `AT+MQTTRX`); dentro de las rutinas de `net.inc`, `rx_get` lee y espera 20 ms
  (`delay20`) entre lecturas. `NET_READ` solo copia del búfer del MCU: casi gratis para el STM32.
- **Sondeo ESP32 → STM32**: `NET_BRIDGE.cpp` (ESP32) manda una trama `CMD_NET_POLL` cada **15 ms con conexión o MQTT activo** y cada 250 ms en reposo
  (`NET_POLL_ACTIVE_MS`/`NET_POLL_IDLE_MS`). El STM32 **solo contesta** (protocolo maestro-esclavo con bit de secuencia y crédito de recepción, `handle_net_poll`):
  no puede hablar primero, así que lo que el Z80 escribe espera en `net_tx_buf` hasta la siguiente trama del ESP32.
- Un `AT` son dos pasadas por ese bucle de 15 ms más el proceso del ESP32: `room_poll` da ~30 ms de plazo.
- Las tramas se atienden en `wifi_handler_poll()`, **dentro de `loop()`** y **una por vuelta**; los comandos del Z80 se ejecutan también en `loop()` (síncronos): mientras
  dura un comando largo, no se atiende al ESP32. El búfer de recepción del UART es de 512 bytes (`build_opt.h`; con 64 se perdían tramas si `loop()` pasaba de ~0,7 ms).
- El audio (WAV, AY, voz) y el protocolo con el Z80 van por interrupciones y mandan sobre `loop()`.

## Opciones para bajar la latencia (de más a menos rentable)

1. **Quitar el sondeo lento del Z80** (una vez por fotograma, 20 ms): casi gratis para el STM32; cuesta tiempo de Z80 (lo mide NETLAT, prueba 1).
2. **Protocolo simétrico entre STM32 y ESP32**: que cualquiera mande una trama cuando tenga algo (el ESP32 al llegar un mensaje, el STM32 cuando el Z80 escribe). El UART
   ya recibe por interrupción y es full-duplex, no hace falta un cable de «dato listo» (el STM32 no duerme). Se mantienen el bit de secuencia y el crédito. Quita las
   tramas vacías y la espera de hasta 15 ms por sentido. Riesgo: `loop()` tiene que sacar las tramas a tiempo (512 bytes de búfer ≈ 5,5 ms).
3. **Camino rápido para paquetes pequeños** (2-8 bytes binarios sin análisis AT y sin firmar cada paquete: firmar la sesión o cada N).
4. **ESP32**: desactivar el ahorro de energía del WiFi (modem sleep suele añadir decenas de ms), QoS 0, sin retener.
5. **Bit de estado en la FPGA** (como el del VSYNC en el puerto `$AF`) para que el Z80 sepa con un `IN` que hay datos, sin comando al MCU. Exige cambio de FPGA.
6. Otro transporte (UDP directo en LAN, o un servidor de reenvío propio y cercano por internet) quitaría saltos pero iría contra «sin servidor propio».

Tipo de juegos: con ~100 ms y 10-25 envíos por segundo, «intercambio de entradas con retardo fijo» (lockstep) sirve para Pong, Tron, serpiente, hockey de mesa; no para
lucha o disparos rápidos.

## Medición (preparada el 19 de octubre de 2026; sin resultados todavía)

Ver `EXAMPLES/NETLAT/README.md`:
- `netlat.bin` + `NETLAT.B81` (Z80): coste del reloj, de 100 `NET_READ`, `AT` sin pausa, `AT` con pausa de 20 ms y eco MQTT al servidor. Probado en el PC (`test_z80netlat.cpp`).
- `LAT_STATS` en el STM32 (`LATSTATS.h/.cpp`, ganchos en `loop()`, `handle_net_poll` y los comandos 66/67; **sin compilar con arduino-cli**, solo probado el módulo en el PC
  con un `Arduino.h` de mentira): tiempos del bucle y de sus partes, ritmo real de las tramas del ESP32 y desglose de las esperas del puente.
- `mqtt_rtt.py` (PC): ida y vuelta al servidor desde internet. Referencia del 19/10/2026 contra `test.mosquitto.org`: **mediana 32 ms**, máximo 41 ms.

Siguiente paso: que el usuario ejecute NETLAT en la placa (con el STM32 en reposo y con el STM32 ocupado: WAV sonando, web del depurador, un `LOAD` grande), con el firmware normal
y con `LAT_STATS = 1`, y mandar las líneas. Con eso se decide entre 1, 2 y 5.

## Resultados en el emulador (19/10/2026)

Antes de tocar `net.inc` (con la pausa de 20 ms): `AT` sin pausas 32,25 ms, `AT` con `at_cmd` 49,25 ms, eco MQTT 163 ms.

Con las dos mejoras de `net.inc` (espera de ~5 ms en `rx_get`; `at_raw_p`, que publica sin esperar el `>`):

| Prueba | ms (media, min-max) |
|---|---|
| 1 100 NET_READ | 22 por 100 comandos (0,22 ms por comando) |
| 2 AT sin pausas | 35,7 (20-60) |
| 3 AT con at_cmd | 36,5 (20-60) |
| 4 eco MQTT (at_raw) | 150 (120-200) |
| 5 publicar, esperando el `>` | 77,3 (60-100) |
| 6 publicar encadenado | ~45 (20-?); una muestra salio negativa y desbordo la suma (ver abajo) |
| 7 eco encadenado | 109,3 (80-140) |

- Esperar 5 ms en vez de 20 ms entre lecturas quita ~13 ms por comando AT (49 -> 36 ms): el AT ya esta casi en el suelo del puente (2 x 15 ms de sondeo del ESP32).
- Publicar sin esperar el `>` ahorra una ida y vuelta del puente: ~32 ms por publicacion (77 -> ~45) y ~41 ms en el eco (150 -> 109).
- El eco sigue en ~110 ms: ~45 de publicar + internet (~32, `mqtt_rtt.py`) + recoger el eco con `AT+MQTTRX` (otra ida y vuelta de ~36 ms). Lo que queda es el sondeo de 15 ms del ESP32 (opcion 2: protocolo simetrico).
- Hallazgo: una muestra del reloj del MCU (comando 50) dio las centesimas ya vueltas a 0 con el segundo todavia sin incrementar (diferencia de -0,96 s). Parece una carrera
  entre el segundo del RTC y las centesimas en el MCU, rara. NETLAT ya la corrige (suma 100 cs si la diferencia sale negativa), pero conviene mirarlo en `cmd_50` (RTC).

## Capa sin esperas para tiempo real: `EXAMPLES/NETCHAT/netq.inc` (19/10/2026)

Paso previo a un Pong en red. `net.inc` es bloqueante (cada AT espera su respuesta: ~17 ms por comando en la placa); un juego a 50 fps no puede congelarse asi.
`netq.inc` se incluye despues de `net.inc` (la sala se sigue creando con `room_new`/`room_join`) y pasa mensajes cortos sin esperar nunca:

- `nq_open` (con espera, una vez): se da de baja de `<sala>/+` y se suscribe solo a `<sala>/<ranura del rival>` (asi no llega el eco de lo propio ni las presencias) y tira lo pendiente.
- `nq_send` (HL datos, A longitud 1-100): mete `AT+MQTTPUB=<sala>/<mi ranura>,0,0,n` + dato en una cola en RAM; si la cola esta ocupada, CF = 1 y se pierde ese mensaje (en un flujo de estado, lo normal).
- `nq_pump` (una vez por fotograma): añade un `AT+MQTTRX` si no hay ya 2 sin contestar, vuelca la cola al tubo (NET_WRITE, lo que acepte) y lee lo llegado (NET_READ). Un analizador
  de lineas (CR ignorado, `>` inicial ignorado) cuenta los `OK`/`ERROR` contra una cola de lo mandado y, tras `+MQTTRX:topic,n`, guarda los n bytes siguientes (los bytes 13, 10, `>` y 0 del dato no molestan).
  Perro guardian: 50 fotogramas sin respuesta -> se vacia la cuenta (una respuesta tardia no hace daño).
- `nq_get` (A = longitud, 0 = nada; HL = NQ_RXBUF): el ultimo mensaje completo del rival, una sola vez.
- Sin HMAC: no se firma nada; el topic va ligado al codigo (aleatorio) de la sala.

Probada en el PC (`claude/mqtttest/nqtest.asm` + `test_z80nq.cpp`, en `build_z80.sh`) con el modem de verdad y el cadencia real de sondeo de 15 ms: 100 mensajes mandados y 67 recibidos enteros y en orden, mensajes de
100 bytes con NET_WRITE de 23 en 23, nunca mas de 2 `AT+MQTTRX` sin contestar, recuperacion tras un apagon del modem. **Latencia medida en el banco, del broker al Z80** (sin contar internet): media 47 ms,
maxima 75 (esperar a que llegue un AT+MQTTRX al ESP32 en su siguiente sondeo, volver en el siguiente, y que el Z80 bombee: fotogramas de 20 ms). Con la subida (~7,5 ms de sondeo + internet ~16 ms) un
sentido ronda los 70 ms y el viaje de ida y vuelta unos 140 ms.

Para bajarlo mas, en el ESP32: mandar la trama NET_POLL en cuanto haya datos que entregar (sin esperar los 15 ms), ahorraria ~15 ms por sentido.

## Pong en red: `EXAMPLES/NETGAME/pong.asm` (19/10/2026)

Prueba de si la latencia da para tiempo real. Lobby, sala y teclado de los demas juegos (lobby.inc con unas rutinas de mentira en lugar de game.inc: sin partida por turnos ni guardado); la red va por `netq.inc`.

- **El creador simula** (izquierda): bola, tantos y las dos palas; manda cada 2 fotogramas `[1][fr][bx][by][vx][vy][mi pala][tantos izq][tantos der][flags][sello del invitado]` (11 bytes).
  **El invitado** (derecha) manda `[2][fr][mi pala][sello del creador]` (4 bytes) y dibuja lo que recibe; entre mensajes mueve la bola con la misma fisica (paredes y palas, sin tantos) y al recibir un estado la adelanta la mitad del ping.
- **Ping**: cada lado sella sus mensajes con su contador de fotogramas y devuelve el ultimo sello del otro; ida y vuelta = fotogramas desde que salio el sello (en el banco: 4-5 fotogramas = 80-100 ms; incluye hasta 2 fotogramas de espera del eco). Sale abajo en ms (x 20).
- Graficos: HiRes Spectrum con sprites por hardware (palas de 4 sprites: 0-3 izquierda, 4-7 derecha; bola, el 8; mi pala verde, la del rival roja). Teclas: Q / 7 arriba, A / 6 abajo, X salir, ENTER otra partida (el creador, a los 9 tantos).
- Un fotograma ~19 ms en el banco (`FRDLY` en pong.asm ajusta la pausa; en la placa cada fotograma suma el coste de los comandos al MCU).
- Al salir, `nq_close` espera a que el modem conteste lo pendiente (sus `>` y `OK` atrasados se tomarian por la respuesta del siguiente AT de `room_leave`).
- Prueba en el PC: `claude/mqtttest/test_z80pong.cpp` (en `build_z80.sh`): sprites, saque, limites y avance de la bola, tantos, fin de partida y ENTER, teclas, ping, rival que no contesta, salir; y el lado del invitado.
- Para probar con dos emuladores: `EXAMPLES/NETGAME/compila_pong.bat` copia a `SD81\test\PONG` y `PONG2`.

### Pong: suavizado y ping (primera prueba en el emulador: ping ~200 ms, la bola "retrocedia")

- Causa de los retrocesos del invitado: el estado llega viejo y con jitter; el invitado fijaba la bola a "estado + adelanto" cada vez (saltos atras) y, ademas, rebotaba en local (paredes y su pala) y el
  siguiente estado, anterior al rebote, la devolvia a la trayectoria vieja (volvia a rebotar: "retrocede y vuelve a avanzar").
- Ahora: un estado mas viejo que el ultimo se ignora (sello de fotograma); si el estado esta de acuerdo con la direccion de la bola local, esta se corrige la MITAD del error (y de golpe si pasa de 24 px);
  tras un rebote local (`P_PEND`, 12 fotogramas) se ignoran los estados que lo contradicen hasta que el creador lo confirme; la pala del rival se acerca a su posicion a 6 px por fotograma como mucho.
- Prueba en el PC con jitter de 0-120 ms por mensaje: retrocesos en el 0,8 % de los fotogramas (peor 13 px), frente a saltos mayores antes.
- Ping mas honesto: cada mensaje lleva la "edad" del sello que devuelve (fotogramas que el que contesta tardo en devolverlo) y el otro lo resta; antes el ping incluia hasta 2 fotogramas de espera propia.
  Formatos: creador [1]...[sello][edad] (12 bytes), invitado [2][fr][pala][sello][edad] (5 bytes).

### Segunda tanda (19/10/2026): estado cada fotograma y sondeo del ESP32 en rafaga

- **Pong**: el estado (creador) y la pala (invitado) salen en cada fotograma (50 por segundo en vez de 25). `netq.inc`: hasta 4 `AT+MQTTRX` sin contestar (antes 2) y hasta 2 pedidos nuevos por `nq_pump`:
  con un mensaje del rival por fotograma, una peticion por fotograma no dejaba vaciarse la cola del modem y los mensajes llegaban viejos.
- **ESP32 (`NET_BRIDGE.cpp`)**: el sondeo al STM32 pasa de 15 ms a **3 ms en rafaga**: mientras viajen datos en cualquier sentido (o haya algo por entregar o sin aceptar), y durante 12 sondeos despues del ultimo dato;
  en reposo sigue a 15 ms (y sin conexion a 250). Ahorra del orden de 7 ms de espera a lo que escribe el Z80 y de ~15 ms a cada respuesta. El STM32 solo contesta: mas tramas por segundo solo mientras se juega.
  **Hay que compilar y grabar el ESP32** (Wifi_module_01); el STM32 no cambia. Si el STM32 se resintiera (musica, web), `LAT_STATS=1` mostrara el ritmo de `NET_POLL` y el bucle.

### Tercera tanda (19/10/2026): de donde salen los 80-140 ms, y que se puede quitar

Medido por el usuario en el Pong con ESP32 en rafaga: ping 80-140 ms. Suelo de internet: `mqtt_rtt.py` desde el PC da ida y vuelta al broker de **~40 ms** (mosquitto 41, hivemq 43, **emqx 180**: no
usarlo). Dos jugadores por el mismo broker = ida y vuelta de uno a otro >= 2 x 40 = **~80 ms**: justo el minimo medido. Lo que queda por encima es jitter y espera propia:
- **Ahorro de energia del WiFi del ESP32** (modem sleep, el de por defecto): el punto de acceso retiene lo que llega hasta la siguiente baliza (~100 ms) -> llegadas tardias y a tirones.
  Ahora `NET_BRIDGE.cpp` llama a `WiFi.setSleep(false)` mientras hay conexion/MQTT y lo vuelve a poner al acabar.
- **El Z80 solo recogia una vez por fotograma** (media 10 ms de espera): el Pong llama a `pg_rx` (nq_pump + tratar lo recibido) 3 veces por fotograma (una al mandar y dos durante la pausa).
- **No se baja desde aqui**: internet y el broker. Dos jugadores en la misma red podrian usar un broker local (se elige con /SYS/NET.CFG) y el ida y vuelta bajaria a ~15-20 ms.

### Cuarta tanda (19/10/2026): el juego iba a golpes aunque la latencia bajo a 20-100 ms

Con las 3 recogidas por fotograma la latencia bajo (20-100 ms en la placa) pero la bola iba a golpes. Causa probable: el fotograma era trabajo + pausa fija, y cada comando al MCU espera su turno en
el bucle del STM32 (ahora con tramas del ESP32 cada 3 ms), asi que el trabajo era largo y variable y el periodo del fotograma iba bailando.
- El fotograma se sincroniza con el **VSYNC** (bit 0 del puerto $AF, 50 Hz exactos, alto 8 lineas = 0,5 ms: se mira en un bucle corto): periodo fijo de 20 ms mientras el trabajo no pase de 20 ms (si pasa,
  se pierde ese fotograma). En el banco, `z80net_env.h` simula el bit 0 de $AF.
- Recogidas: una justo tras mandar y otra hacia la mitad (`MIDDLY`, ~6 ms de pausa): lo que tarde la segunda, variable o no, lo absorbe la espera del VSYNC. Si aun va a golpes, probar `MIDDLY` mas corto o quitar
  la segunda (`call pg_rx` de `pg_delay`).

### Quinta tanda (19/10/2026): "va a golpes, y antes, con mas latencia, iba mejor"

Tras la 4a tanda (espera al VSYNC) seguia a golpes. Hipotesis: con el ESP32 sondeando cada 3 ms, cada comando del Z80 al MCU espera su turno en el bucle del STM32 y un fotograma con muchos comandos
(2 recogidas + 4 AT+MQTTRX + el mensaje, a 50 Hz) se pasaba de 20 ms; con la espera al VSYNC, cada pasada de 20 ms se convertia en un fotograma de 40 ms. Lo de antes (25 Hz, una recogida, sondeo a 15 ms) iba mas
holgado.
- **Ritmo con FRAMES** (`16436`, que la FPGA decrementa en cada VSYNC en Superfast; no lo pisa el handshake, que si usa el puerto $AF): `pg_tick` espera a que cambie y da cuantos fotogramas han pasado;
  la fisica y el teclado se ejecutan una vez por cada uno (hasta 3), asi la bola va a velocidad constante aunque una vuelta tarde mas de 20 ms, y no se pierde un fotograma entero.
- **Menos trafico**: el estado sale cada 2 fotogramas (`SEND_AND`, 25 por segundo, como cuando iba bien); `netq.inc` pide 1 `AT+MQTTRX` por recogida (hasta 3 sin contestar).
- **Diagnostico en pantalla**: arriba, `D nn` = % de las ultimas 50 vueltas que tardaron mas de un fotograma. Alto (>20): el trabajo de red del Z80 se pasa; 0 y sigue a golpes: es otra cosa (llegada irregular de estados).
- El arnes simula FRAMES (16436/16437) y el bit 0 de $AF.

### Sexta tanda (19/10/2026): "ahora va muy lento"

Causa: `FRAMES` (16436) no se movia en el equipo del usuario (la FPGA solo lo da con ciertas condiciones: `sfast_mode_en`, `sfSP_en` = 0...), y `pg_tick` esperaba su cambio ~116 ms por vuelta.
Ahora espera como mucho ~40 ms; si no cambia, pasa para siempre a marcar el ritmo con el bit 0 del puerto $AF (VSYNC: espera a que baje y suba, un fotograma por vuelta; no ve los fotogramas
que se pierden). Arriba sale `F nn` (ritmo por FRAMES) o `V nn` (por VSYNC), con nn = % de las ultimas 50 vueltas que se pasaron de un fotograma (en V siempre 0: no puede medirlo).
Arnes: `NOFRAMES=1 ./test_z80pong.exe pong.bin pong.sym` prueba el modo V.

### Septima tanda (19/10/2026): en modo V, a golpes otra vez

En el equipo del usuario `FRAMES` no se mueve (sale `V`), y con el ritmo por VSYNC a 20 ms el trabajo (comandos al MCU con el ESP32 sondeando cada 3 ms) se pasaba a veces y esa vuelta duraba 40 ms:
una mezcla de fotogramas de 20 y 40 ms se ve a golpes; uno constante de 40 ms, no. Ahora, con VSYNC, **cada vuelta dura exactamente `FRDIV` = 2 fotogramas (25 por segundo)**, la fisica se hace 2 pasos por vuelta,
se manda el estado en cada vuelta y hay 3 recogidas (tras mandar y dos mas, separadas ~6 ms). Con `FRAMES` (si algun equipo lo mueve) sigue el esquema anterior. Si ni asi va fino, `FRDIV` = 3 (16,7 fps) o aligerar
el trabajo por vuelta; si va fino y se quiere mas fps, `FRDIV` = 1 con trabajo mas corto. La espera de silencio y el ping cuentan fotogramas reales (P_FR += FRDIV por vuelta).

### Octava tanda (19/10/2026): "cada vez va peor": vuelta al esquema que mejor iba

Orden de los cambios y lo que dijo el usuario en la placa: (1) 25 Hz, 1 recogida, pausa fija, ESP32 a 15 ms: "mejorado mucho, no del todo fino" (ping ~200); (2) + estado cada fotograma, 2 asks por recogida, ESP32 en rafaga (3 ms),
una recogida por fotograma, pausa fija: **"bastante bien"**, ping 80-140; (3) + `WiFi.setSleep(false)` en el ESP32 y 3 recogidas por fotograma (en la pausa): ping 20-100 pero **"a golpes"**; despues, FRAMES / VSYNC / 25 fps constantes: peor.
`pong.asm` vuelve con `CLASSIC = 1` a (2): pausa fija `FRDLY`, estado en cada vuelta, una recogida por vuelta (`pg_net`), `netq.inc` con 2 asks por recogida y hasta 4 sin contestar. Arriba sale `C nn`. (`CLASSIC = 0` devuelve los ritmos por FRAMES/VSYNC.)
**Experimento A/B**: el ESP32 del usuario sigue con `setSleep(false)`. Si con esta version va fina: la culpa eran las 3 recogidas / el ritmo; si va a golpes: sospechar de `setSleep(false)` (quitarlo en `NET_BRIDGE.cpp`).

**Comparativa 2 / 3 (19/10/2026)**: `pong.asm` tiene `PUMPS3` (con `CLASSIC = 1`): 0 = version 2 (`pong.bin`/`PONG.B81`, letra `C` arriba), 1 = version 3 (tres recogidas por vuelta, la pausa al final en dos mitades; `pong3.bin`/`PONG3.B81`, letra `T`).
Se genera con `sed 's/^PUMPS3      equ 0 /PUMPS3      equ 1 /' pong.asm > pong3.asm; pasmo pong3.asm pong3.bin` (no se guarda el .asm). Las dos estan en `SD81\test\PONG` y `PONG2`. El usuario prueba primero la 2.

**Novena (19/10/2026)**: el usuario se queda con la version 2 (va mejor que la 3, pero ninguna va fina). Para aislar el `WiFi.setSleep(false)` del ESP32 (lo unico que cambio en la placa entre "bastante bien" y los golpes):
`NET_BRIDGE.cpp` lo tiene ahora tras `#define NET_WIFI_NOSLEEP 0` (desactivado). Pendiente: que el usuario recompile el ESP32 y pruebe la version 2.

**Decima (19/10/2026): "algo acumulativo que ralentiza"**. Prueba de resistencia en el PC (`claude/mqtttest/test_z80nqsoak.cpp`: 50 mensajes/s en cada sentido durante 60 s, con 1 y con 3 recogidas por fotograma): sin degradacion
(edad del mensaje constante, 0 ERROR, 0 perdidos, cola de pendientes estable) -> `netq.inc` no acumula. Sospechoso real en el STM32: `GLOBALS.h` tiene `LOG0`, `LOG1` y `LOG2` activados, y `loop()` saca dos lineas por el USB
(`COMMAND=n` y `AFTER COMMAND=n`) por CADA comando del Z80: un juego en tiempo real manda decenas por fotograma (~500 comandos/s -> ~1000 lineas/s), con el coste de formatear y escribir cada una (probablemente buena parte de los 0,25 ms
por comando medidos con NETLAT) y, si el USB no se lee o va lento, esperas que se acumulan. Cambio: los comandos 66 y 67 (tubo de red) no se loguean (`SD81BoosterV2_039_STM32.ino`, `log_cmd`).
Pendiente: que el usuario compile y grabe el STM32 y repita las versiones 2 y 3 (el ESP32 se queda como esta). Si ayuda, valorar quitar tambien `LOG1` del `GLOBALS.h` en las versiones publicadas.

### Undecima (20/10/2026): la bola es de quien la recibe (en vez de que lo simule todo el creador)

Resultados del usuario (placa real contra emulador, ya arreglado el emulador): version 2 va algo mejor que la 3 (la 2 con 100-120 ms, la 3 con 40-60 ms, las dos con algun pico), y se ve a menudo
que el invitado golpea y el anfitrion da la bola por mala. Causa: el creador juzgaba el golpe del invitado con la bola adelantada y con la pala del invitado vieja (retraso de un sentido, ~25-60 ms;
a 6 px por fotograma, 15 px de bola mas lo que se mueva la pala, con una tolerancia de +-20 px). Decidido con el usuario (20/10/2026):
1. **Autoridad que cambia de manos**: el jugador hacia cuya mitad viaja la bola decide golpe o tanto, con su pala y su bola al dia. Al golpear, la bola pasa a ser del otro (contador de golpes `hc` en el mensaje
   con posicion, velocidad y golpes seguidos); el otro la adelanta lo que tardo el mensaje y sigue. Quien no la tiene la sigue y la mueve con la misma fisica (paredes), con la correccion y el `P_PEND` de siempre.
2. **Saque**: como el Pong original (no verificado: no se encontro la fuente; se eligio "saca el que ha perdido el tanto", `SERVE_TO_LOSER` = 0 lo cambia a "el que marco"). La bola sale del centro hacia el que saca a los 50 fotogramas.
3. Suavizado de la pala del rival: igual (6 px por fotograma).
4. **15 tantos** (como el Pong original); lo decide el creador (ENTER, con un numero de partida `P_GEN`); los marcadores salen con dos cifras.
Mensaje simetrico de 15 bytes cada fotograma (ver `pong.asm`). Fallo encontrado y corregido con la prueba: los mensajes de la partida anterior (ya en camino cuando el creador empieza otra) hacian volver atras a quien los
recibia: se ignora todo mensaje con un numero de partida mas viejo. Las pruebas del PC llevan ahora un rival escrito en C++ con las mismas reglas (golpes de los dos, tantos, 15, ENTER, jitter, silencio).

**Detalles esteticos del Pong (20/10/2026)**: (1) la bola (sprite de 8x8, posicion = esquina superior izquierda) ya estaba centrada: en reposo en (124, 92), es decir su centro en (128, 96) = el centro entre las palas (12-19 y 236-243) y las paredes (16-175);
lo que estaba descentrado era la RED del centro, dibujada en x = 123-124 (cuatro puntos a la izquierda): ahora son los puntos 127 y 128 (ultimo de la celda 15, primero de la 16), 2 de ancho. (2) La fila de abajo parpadeaba: se borraba y se
pintaba entera cada vez que cambiaba el ping (casi cada fotograma). Ahora: el aviso (a partir de la columna 12) solo se pinta cuando cambia (con la fila borrada, una vez), los marcadores solo si cambian, y el ping ("PING nnnn MS",
12 celdas fijas a la izquierda) solo se ve con la tecla P (alterna) y se repinta, en su sitio y sin borrar, solo si cambia. El aviso normal es "Q/A  P=PING  X=QUIT". Tambien baja el trabajo por fotograma (20,4 ms en el banco antes 21,8).
