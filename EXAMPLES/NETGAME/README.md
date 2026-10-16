# NETGAME: juegos por turnos en red (capa C): 4 en raya y ajedrez

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
  llena unos ~90 s, hasta que el broker lo detecta). Un código que nadie ha creado da `THERE IS NO SUCH ROOM`.
  **Si te sales (0/Q) puedes volver** con el mismo código en «unirse»: el programa entra en la ranura que
  esté libre, así que el creador vuelve como jugador 1 (mismo color) y el otro como jugador 2. Mientras estás
  fuera, cualquiera con el código puede ocupar tu sitio. Si alguien se cae sin despedirse, su ranura parece
  ocupada ~90 s, hasta que el broker lo detecta.
- **1-7** la columna donde cae la ficha. **R** reenvía la última jugada si falló la red. **Q** sale.
- **ENTER** otra partida, cuando ha acabado (se lo dice al rival).

La pantalla es Superfast texto de 32 columnas con la pantalla alternativa en `$8000` (como `TELNET`): el
vídeo lo da la FPGA y no parpadea. Sin color: los gráficos buenos se ponen aparte. Mientras es el turno del
rival sondea el broker cada ~0,5 s. Si el rival no está, su última jugada espera retenida en el broker y la
recibes al entrar (juego asíncrono).

**Sin probar en el emulador ni en la placa**: probado en el PC (ver abajo) con el Z80 de verdad, el teclado
y la pantalla simulados, el módem de verdad y un broker falso.

## Jugar al ajedrez

```
pasmo chess.asm chess.bin chess.sym        (o compila.bat, que hace los dos juegos)
```

Se carga igual que el 4 en raya (`CHESS.B81`, pásalo a `.P`). La sala, el menú y el teclado son los mismos
(`lobby.inc`). Quien crea la sala juega con las blancas y abajo; el que se une, con las negras y el tablero
girado. Las blancas se ven como letras (P peón, N caballo, B alfil, R torre, Q dama, K rey) y las negras en
inverso; una casilla oscura vacía lleva un punto.

- **Una jugada** se teclea con las casillas de origen y destino: letra, cifra, letra, cifra (por ejemplo
  `E2 E4`) y **ENTER**. Una tecla que no cuadra en esa posición no entra. **SHIFT+0** borra la última.
- **Coronar**: una quinta letra elige la pieza (Q dama, R torre, B alfil, N caballo); sin ella, dama.
- **9** reenvía la última jugada si falló la red. **0** sale. **ENTER** sin nada tecleado, con la partida
  acabada, pide otra.
- Se ve la última jugada (`LAST: E2E4`), de quién es el turno, `CHECK` cuando hay jaque y el final
  (`CHECKMATE - YOU WIN`, `CHECKMATE - THE RIVAL WINS`, `DRAW`).

Reglas (`chess.inc`): todo el ajedrez salvo la **triple repetición**: enroque, al paso, coronación, jaque,
mate, ahogado, material insuficiente (rey solo, o con un caballo o un alfil) y la regla de las 50 jugadas.
Una jugada son 2 bytes (`[origen | corona << 6] [destino]`, casillas 0-63) y el estado completo 67 bytes (las 64
casillas, los enroques, el peón al paso y el reloj de medias jugadas). Tablero 0x88 en la RAM. Generar las
jugadas legales de una posición cuesta ~0,1-0,3 s en un Z80 a 3,25 MHz.

**Sin probar en el emulador ni en la placa.**

## Cómo añadir otro juego

`game.inc` no sabe nada de las reglas; el juego define seis rutinas, `G_STATE_LEN` (el tamaño del estado) y
`G_MOVE_LEN` (1 o 2 bytes por jugada):

| Rutina | Qué hace |
|---|---|
| `g_reset` | posición inicial |
| `g_valid` | HL = la jugada, B = jugador: CF = 1 si no es legal |
| `g_apply` | HL = la jugada, B = jugador: la hace |
| `g_winner` | A = 0 sigue, 1 o 2 gana ese jugador, 3 tablas |
| `g_save` / `g_load` | HL = destino / origen: los `G_STATE_LEN` bytes del estado |

Empieza el jugador 1; mueve el 1 con un número de jugada par y el 2 con uno impar.
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
- `c4test.asm` / `chtest.asm`, `test_z80chess.cpp`: **el ajedrez** con `perft` (contar las posiciones a
  profundidad N desde las posiciones de referencia: inicial hasta 4 = 197.281, Kiwipete hasta 3 = 97.862,
  posición 3 hasta 4 = 43.238, posición 4 hasta 3 = 9.467, posición 5, enroques y coronaciones; todos
  coinciden), los efectos del enroque, el al paso y la coronación, mate, ahogado y tablas.
- `test_z80chessui.cpp`: el ajedrez entero con teclado y pantalla simulados: tecleo con borrado, jugada
  ilegal, mate del pastor, jaque, otra partida, y como negras con el tablero girado.
- `test_z80ui.cpp`: el 4 en raya entero, con pulsaciones de teclado, la pantalla leída de `$8000`, una partida
  hasta ganar, otra partida, el rival que se va, y unirse tecleando el código con un borrado.

**Notas:** la parte común de los dos juegos (arranque del vídeo, menú, sala, teclado y texto) está en
`lobby.inc`. `a2z` (ASCII a código del ZX81) usa `HL`, así que `print_at` lo guarda; `kbd_scan` destruye `BC`
(hay que guardar la tecla); el ZX81 no tiene `!` ni `|`.
