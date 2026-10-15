# NETGAME: juegos por turnos en red (capa C)

Capa C de `claude/planning/juego_en_red.md`: el controlador común de una partida por turnos entre dos ZX81
(`game.inc`), las reglas del primer juego (`connect4.inc`) y el juego (`connect4.asm`). Va encima de la
librería de red de `EXAMPLES/NETCHAT/net.inc` (sala, mensajes firmados, MQTT sin servidor propio).

## Jugar al 4 en raya

```
pasmo connect4.asm connect4.bin connect4.sym        (o compila.bat)
```

Copia `connect4.bin` y `CONNECT4.B81` (pásalo a `.P`) a la misma carpeta de la SD, carga el programa y `RUN`.
Hace falta el módulo WiFi con la red configurada (`/SYS/WIFI.CFG`) y la FPGA con Superfast.

- **1** partida nueva: sale un código de 10 caracteres y espera al rival. Eres el jugador 1 (bloque entero)
  y empiezas.
- **2** unirse: teclea el código (SHIFT+0 borra) y ENTER. Eres el jugador 2 (gris). Una sala admite solo dos
  jugadores: un tercero ve `THE ROOM IS FULL` y no entra (si el segundo se cayó sin despedirse, la sala parece
  llena unos ~90 s, hasta que el broker lo detecta).
- **1-7** la columna donde cae la ficha. **R** reenvía la última jugada si falló la red. **Q** sale.
- **ENTER** otra partida, cuando ha acabado (se lo dice al rival).

La pantalla es Superfast texto de 32 columnas con la pantalla alternativa en `$8000` (como `TELNET`): el
vídeo lo da la FPGA y no parpadea. Sin color: los gráficos buenos se ponen aparte. Mientras es el turno del
rival sondea el broker cada ~0,5 s. Si el rival no está, su última jugada espera retenida en el broker y la
recibes al entrar (juego asíncrono).

**Sin probar en el emulador ni en la placa**: probado en el PC (ver abajo) con el Z80 de verdad, el teclado
y la pantalla simulados, el módem de verdad y un broker falso.

## Cómo añadir otro juego

`game.inc` no sabe nada de las reglas; el juego define seis rutinas (y `G_STATE_LEN`, el tamaño del estado):

| Rutina | Qué hace |
|---|---|
| `g_reset` | posición inicial |
| `g_valid` | A = jugada, B = jugador: CF = 1 si no es legal |
| `g_apply` | A = jugada, B = jugador: la hace |
| `g_winner` | A = 0 sigue, 1 o 2 gana ese jugador, 3 tablas |
| `g_save` / `g_load` | HL = destino / origen: los `G_STATE_LEN` bytes del estado |

Las jugadas son de un byte; empieza el jugador 1; mueve el 1 con un número de jugada par y el 2 con uno impar.
Lo demás lo hace el controlador: `game_start`, `game_move`, `game_poll` (con sus códigos de resultado en la
cabecera de `game.inc`), `game_resend`, `game_rematch`.

**Mensajes** (payload; el turno es `NB_TURN` = número de jugada, que ya firma `net.inc`): `1` jugada
`[1][jugada][hash 2 bytes del estado después de la jugada]`, `2` pido el estado, `3` estado completo
`[3][bytes]`, `4` otra partida. Cualquier desfase (jugada que no es la siguiente, ilegal, o con otro hash) se
arregla pidiendo el estado completo; las jugadas viejas (un retenido) se ignoran.

## Pruebas en el PC

`claude/mqtttest/build_z80.sh`:
- `c4test.asm`: las reglas del Z80 contra una implementación en C++ con ~11.000 jugadas al azar (legales e
  ilegales), ganadores, tablas, y guardar / cargar el estado.
- `test_z80game.cpp`: el controlador contra un rival hecho en la prueba: jugadas, turnos, jugadas viejas,
  hash que no cuadra, estado completo, fin de partida, otra partida, y unirse como jugador 2.
- `test_z80ui.cpp`: el juego entero, con pulsaciones de teclado, la pantalla leída de `$8000`, una partida
  hasta ganar, otra partida, el rival que se va, y unirse tecleando el código con un borrado.

**Notas:** `a2z` (ASCII a código del ZX81) usa `HL`, así que `print_at` lo guarda; `kbd_scan` destruye `BC`
(hay que guardar la tecla); el ZX81 no tiene `!` ni `|`.
