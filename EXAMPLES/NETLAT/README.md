# NETLAT: medir la latencia de la red del SD81 Booster

Para decidir si merece la pena (y por dónde) bajar la latencia de la red hacia juegos más rápidos, antes hay que **medirla capa por capa**.
Son tres herramientas que se complementan:

| Herramienta | Dónde corre | Qué mide |
|---|---|---|
| `netlat.bin` + `NETLAT.B81` | el ZX81 (la placa real) | lo que ve el Z80: el coste de un comando al MCU, la ida y vuelta de un `AT` y la de un mensaje MQTT hasta el servidor |
| `LAT_STATS` en el firmware del STM32 (`LATSTATS.h`) | el STM32, por el puerto serie | el bucle principal, cada parte del bucle, el ritmo real de las tramas del ESP32 y el desglose de las esperas del puente |
| `mqtt_rtt.py` | el PC | la ida y vuelta al servidor MQTT desde internet, sin placa: la referencia |

## 1. NETLAT en la placa

Copia `NETLAT.BIN` y `NETLAT.B81` a la misma carpeta de la SD, carga el `.B81` y `RUN`. Hace falta el módulo WiFi con la red configurada.
Tarda unos 40-60 s: conecta a un servidor (hasta 40 s), y durante la medida **la pantalla se apaga** (FAST) para no falsear los tiempos.
Al acabar saca 8 bloques de dos líneas (nombre y `N F(ail) A(vg) min-max` en ms); **mándame esas líneas** (o la captura).

```
pasmo netlat.asm netlat.bin netlat.sym        (o compila.bat: lo copia a SD81\test\NETLAT del emulador)
```

Los tiempos salen del reloj del MCU (el de `LOAD *RTC`), que da **centésimas**: cada muestra sale redondeada a 10 ms, así que lo que vale es
la **media** de muchas (40 por prueba; 30 la de MQTT). Los mínimos y máximos vienen en pasos de 10 ms.

| Prueba | Qué hace | Para qué sirve |
|---|---|---|
| 0 `RTC READ` | dos lecturas del reloj seguidas | lo que cuesta medir: se resta mentalmente de las demás |
| 1 `100 NETREAD CMDS` | 5 tandas de 100 comandos `NET_READ` con `max=0` (el «¿hay algo?» barato) | cuánto cuesta preguntar con más frecuencia: ms por 100 comandos ÷ 100 = ms por comando |
| 2 `AT ROUND TRIP, NO PAUSES` | `AT`+CR hasta el `OK`, leyendo sin pausa | la ida y vuelta Z80 - MCU - ESP32 - MCU - Z80 en el mejor caso |
| 3 `AT, AT_CMD POLL` | lo mismo con `at_cmd` de `net.inc` (lee, espera ~5 ms, vuelve a leer; antes eran 20 ms) | lo que usan los juegos |
| 4 `MQTT ECHO` | publica un mensaje en la sala de prueba (`at_raw`, que espera el `>` del módem) y pide `AT+MQTTRX` hasta que vuelve | todo el camino hasta el servidor y de vuelta |
| 5 `PUBLISH, WAIT >` | solo la publicación, hasta el `OK`, con `at_raw` | cuánto del eco es publicar y cuánto es esperar la vuelta |
| 6 `PUBLISH PIPELINED` | lo mismo con `at_raw_p`: comando, CR y dato seguidos, sin esperar el `>` | lo que se ahorra una ida y vuelta del puente |
| 7 `MQTT ECHO PIPELINED` | el eco de la prueba 4 publicando con `at_raw_p` | el eco con las dos mejoras de `net.inc` |

**Cómo leerlo.** La prueba 2 es el suelo del puente (Z80 ↔ MCU ↔ ESP32). La 3 menos la 2 es lo que se pierde por esperar entre lecturas (con la `net.inc` anterior eran 20 ms por vuelta; midió 49,25 ms frente a 32,25 ms en el emulador).
La 4 menos la 2 es, aproximadamente, internet + servidor + la espera de que el Z80 recoja el eco (compárala con `mqtt_rtt.py`). Si la 4 es poco más que la 2,
el cuello de botella es el puente y no la red. Los juegos por turnos añaden además su propio sondeo (cada ~0,4-0,5 s).

## 2. Medidas dentro del STM32 (`LAT_STATS`)

En `Arduino/SD81BoosterV2_039_STM32/LATSTATS.h` cambia `#define LAT_STATS 0` por `1`, compila, graba y abre el monitor serie. Cada 10 s salen líneas `[LAT]`:

```
[LAT] bucle: 1257688 vueltas, media 3 us, maxima 1603 us | pasan de 0,7 ms: 1, 1 ms: 1, 2 ms: 0, 5 ms: 0, 10 ms: 0
[LAT]   leer cmd / ejecutar / serie / wifi / musica / ocioso   media y maxima de cada parte
[LAT] tramas NET_POLL del ESP32: N; entre una y la siguiente: media .. us, maxima .. us (mas de 20 ms: ..); contestarlas: media .. us, maxima .. us
[LAT] puente (N pasadas) escribir->ofrecer a/b us, ofrecer->respuesta a/b us, respuesta->leer a/b us, TOTAL a/b us
```

- **`bucle`**: cuánto tarda una vuelta de `loop()`. Es lo que espera cualquier trama del ESP32 (se atienden desde `wifi_handler_poll()`, una por vuelta) y cualquier
  tarea de `loop()` mientras el STM32 hace otra cosa (por ejemplo, ejecutar un comando largo del Z80). Los contadores «pasan de» dicen cuántas vueltas llegan a
  0,7 ms (el límite histórico del búfer de recepción de 64 bytes), 1, 2, 5 y 10 ms.
- **`ejecutar`** (el comando del Z80) suele ser la parte que más estira el bucle: mientras dura, no se atiende al ESP32.
- **`tramas NET_POLL`**: el ESP32 las manda cada 15 ms con conexión (250 ms en reposo). El «entre una y la siguiente» es el ritmo **real**, con la espera del STM32
  incluida; si sale muy por encima de 15 ms, el bucle del STM32 es el cuello de botella.
- **`puente`**: las tres esperas de un comando `AT`: el Z80 escribe → el STM32 se lo ofrece al ESP32 en su siguiente trama → el ESP32 contesta en la que sigue →
  el Z80 lo lee. Si «escribir→ofrecer» y «ofrecer→respuesta» rondan los 15 ms es el ritmo de sondeo del ESP32; «respuesta→leer» es lo que tarda el Z80 en preguntar.
- Con `LAT_STATS = 0` (por defecto) no se compila ni una instrucción de esto. La vuelta en la que se imprime el informe no se cuenta (el USB tarda).

**Para ver el efecto sobre otras tareas**, repite NETLAT con el STM32 ocupado: reproduciendo un `.WAV` (`LOAD FAST "x.WAV"`), con la web del depurador abierta
y haciendo `LOAD` de un fichero grande, y compara las líneas `bucle` y `NET_POLL`.

## 3. mqtt_rtt.py: la referencia desde el PC

```
python mqtt_rtt.py                         # test.mosquitto.org:1883, 50 muestras
python mqtt_rtt.py otro.servidor.org 1883 100
```

Publica en un topic al que el mismo cliente está suscrito y cuenta hasta que llega el eco. Medida el 19 de octubre de 2026 desde el PC de desarrollo contra
`test.mosquitto.org`: **mediana 32 ms, máximo 41 ms** (TCP 132 ms y CONNECT 33 ms, una sola vez). Es la parte «internet + servidor» de la prueba 4.

## Prueba en el PC

`claude/mqtttest/test_z80netlat.cpp` (en `build_z80.sh`) ejecuta `netlat.bin` en el Z80 del arnés con el módem de verdad y un servidor falso, y comprueba que cada
prueba hace todas sus muestras sin fallos y que la aritmética (minutos, segundos y centésimas, la vuelta de la hora, mínimo, media y máximo) es correcta. Los tiempos
que salen allí son los del banco, no los de la red.
