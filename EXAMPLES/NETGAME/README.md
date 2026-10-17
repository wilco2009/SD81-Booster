# NETGAME: juegos por turnos en red (capa C): 4 en raya y ajedrez

Capa C de `claude/planning/juego_en_red.md`: el controlador común de una partida por turnos entre dos ZX81
(`game.inc`), las reglas del primer juego (`connect4.inc`) y el juego (`connect4.asm`, con la pantalla gráfica). Va encima de la
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
  fuera, cualquiera con el código puede ocupar tu sitio. **Si se cayeron los dos** (por ejemplo, se corta la red
  de los dos: el broker deja las dos presencias a `0`), la sala sigue existiendo pero vacía: el programa ofrece
  elegir `1 = PLAYER 1 (THE CREATOR)` o `2 = PLAYER 2` (en el ajedrez, blancas o negras) y el que vuelva después
  ya entra solo en la que queda. Si alguien se cae sin despedirse, su ranura parece
  ocupada ~90 s, hasta que el broker lo detecta.
- **3** seguir la partida guardada (solo sale si hay un fichero): ver «Guardar y seguir otro día», más abajo.
- **1-7** la columna donde cae la ficha. **R** reenvía la última jugada si falló la red. **Q** sale.
- **ENTER** otra partida, cuando ha acabado (se lo dice al rival).

La partida se dibuja en **Superfast HiRes Spectrum** (256×192, un atributo por celda de 8×8, como el ajedrez;
el lobby y los menús siguen en texto): el tablero de 7×6 casillas de 24×24 puntos (3×3 celdas) en azul
brillante, con discos de 20×20: **rojos** (jugador 1), **amarillos** (jugador 2) y los huecos vacíos en negro.
Los números 1-7 van sobre las columnas. La **última ficha** y, al ganar, las **cuatro en línea** se resaltan
con el papel blanco. A la derecha, el panel (sala y su código, tu color, si el rival está, la última jugada) y
debajo el estado en una caja verde, el aviso en rojo y la ayuda en cian. Capturas de la pantalla real a 1:1
(generadas con el arnés del PC): [inicio](../../DOC/img/connect4_start.png) y
[victoria](../../DOC/img/connect4_win.png). El dibujo del disco (`disc_cells`, 9 celdas de 8×8) está en
`connect4.asm`; `gfx.inc` es el mismo del ajedrez (sin `draw_square`: `GFX_SQUARES equ 0`). Mientras es el
turno del rival sondea el broker cada ~0,5 s. Si el rival no está, su última jugada espera retenida en el broker y la
recibes al entrar (juego asíncrono).

**Sin probar en el emulador ni en la placa**: probado en el PC (ver abajo) con el Z80 de verdad, el teclado
y la pantalla simulados, el módem de verdad y un broker falso.

## Jugar al ajedrez

```
pasmo chess.asm chess.bin chess.sym        (o compila.bat, que hace los dos juegos)
```

Se carga igual que el 4 en raya (`CHESS.B81`, pásalo a `.P`), pero en `$5000` (`CLEAR 20479`, `CODE 20480`) y no en `$6000`: el ZX81 no ejecuta código por encima de `$8000` y, con los relojes, el programa ya no cabe en `$6000-$7FFF`; `compila.bat` y `build_z80.sh` avisan si algún juego se pasa. La sala, el menú y el teclado son los mismos
(`lobby.inc`). Quien crea la sala juega con las blancas y abajo; el que se une, con las negras y el tablero
girado. La partida se dibuja en **Superfast HiRes Spectrum** (256×192, un atributo por celda de 8×8; el lobby
y los menús siguen en texto): el tablero de 8×8 casillas de 16×16 puntos, verdes y blancas, con las
coordenadas, un panel a la derecha (sala, tu color, última jugada) y el estado, los avisos y la entrada
debajo. Casillas **verde claro y rojo oscuro, brillantes**; piezas blancas y negras macizas (el mismo dibujo
con tinta blanca o negra: el bit de brillo es de la celda entera, así que todo el tablero va en brillante, que
es lo que permite la tinta blanca). La última jugada se resalta en magenta (contrasta con las dos tintas) y el rey en jaque tiene el papel del
color contrario a su tinta: el blanco sobre negro y el negro sobre blanco; el título va en una barra amarilla
y el estado en una caja verde. Capturas de la pantalla real a 1:1
(generadas con el arnés del PC):
[inicio](../../DOC/img/chess_start.png), [jaque](../../DOC/img/chess_check.png) y
[las negras](../../DOC/img/chess_black.png).

Las piezas (`pieces.inc`) salen de la hoja `piezas_16x16.png` (seis casillas de 16×16 pegadas, a escala 1:1:
peón, caballo, alfil, torre, dama y rey; negro = pieza). Para retocarlas se edita ese PNG (sin entrelazar) y
se ejecuta `python gen_pieces.py`; el arnés de pruebas usa el mismo `pieces.inc`. `scr2png.py` convierte
un `.SCR` del Spectrum en PNG (lo usa el arnés de las pruebas, `SCRDIR=<carpeta>`, para ver la pantalla real).

- **Una jugada** se teclea con las casillas de origen y destino: letra, cifra, letra, cifra (por ejemplo
  `E2 E4`) y **ENTER**. Una tecla que no cuadra en esa posición no entra. **SHIFT+0** borra la última.
- **Coronar**: una quinta letra elige la pieza (Q dama, R torre, B alfil, N caballo); sin ella, dama.
- **9** reenvía la última jugada si falló la red. **0** sale. **ENTER** sin nada tecleado, con la partida
  acabada, pide otra.
- Se ve la última jugada (`LAST MOVE`, en el panel), de quién es el turno, `CHECK` cuando hay jaque y el final
  (`CHECKMATE - YOU WIN`, `CHECKMATE - THE RIVAL WINS`, `DRAW`).

**Relojes.** Al crear la sala (`1` en el menú) se elige el tiempo de cada jugador: sin reloj, 5, 10, 15 o 30
minutos (sin incremento). El control viaja en la **primera letra del código de sala** (`A` sin reloj, `B` 5 min,
`C` 10, `D` 15, `E` 30), así que el otro jugador lo hereda al teclear el código. Los dos relojes salen en el
panel (`WHITE 09:58` / `BLACK 10:00`): el del que mueve, en amarillo (en rojo con 30 s o menos). El tiempo sale del
reloj del MCU (el de `LOAD *RTC`), no de los fotogramas: el handshake con el MCU pone a cero el contador de VSYNC
en cada lectura. Cada programa es la autoridad de **su** reloj: en cada jugada manda los segundos que le quedan
(el rival se los apunta) y es el que dice «se me acabó el tiempo» (`TIME OUT - THE RIVAL WINS` / `TIME OUT - YOU
WIN`); entre jugada y jugada cada uno lleva una estimación local del reloj del otro. Los relojes arrancan cuando
están los dos y paran al acabar la partida; otra partida los repone.

Reglas (`chess.inc`): todo el ajedrez salvo la **triple repetición**: enroque, al paso, coronación, jaque,
mate, ahogado, material insuficiente (rey solo, o con un caballo o un alfil) y la regla de las 50 jugadas.
Una jugada son 2 bytes (`[origen | corona << 6] [destino]`, casillas 0-63) y el estado completo 67 bytes (las 64
casillas, los enroques, el peón al paso y el reloj de medias jugadas). Tablero 0x88 en la RAM. Generar las
jugadas legales de una posición cuesta ~0,1-0,3 s en un Z80 a 3,25 MHz.

**Sin probar en el emulador ni en la placa.**

## Guardar y seguir otro día (`savegame.inc`)

Los mensajes del broker solo conservan la **última** jugada de cada jugador (retenida) y el tablero vive en la RAM de cada
ordenador: sin esto, apagar el ZX81 perdía la partida. Ahora cada juego guarda su estado en la SD cada vez que cambia
(`GM_PLY` o `GM_OVER`; lo comprueba `draw_all`, y no escribe si solo cambia un aviso) y el menú ofrece **`3 = CONTINUE SAVED
GAME`** cuando existe el fichero. Seguir lee el fichero, vuelve a entrar en la **misma sala y la misma ranura**
(`room_join_as`, sin sonda) y, si la última jugada fue mía, **la vuelve a mandar** (por si el broker la había olvidado). No
hace falta que los dos estén conectados a la vez: cada uno solo necesita su estado guardado y que el broker conserve el último
mensaje del rival hasta que este lo recoja.

- **Fichero:** `CONNECT4.SAV` (66 bytes) o `CHESS.SAV` (91), en la **carpeta actual**, que es la del juego (el cargador ya lee de ahí el
  `.BIN`): cada carpeta de juego tiene su partida, y dos ZX81 (o dos instancias del emulador) con carpetas distintas no se pisan. Los dos
  nombres, sin `_`, se pueden borrar desde el ZX81. Un fichero por carpeta: empezar o unirse a otra partida lo sustituye.
- **Formato:** `SV_ID` (1 = 4 en raya, 2 = ajedrez), versión 1, mi ranura, control de tiempo, el código de sala (10, en códigos
  del ZX81), `GM_PLY` (2), `GM_OVER`, `GM_MOVE` (2), los relojes `GM_REM` (4) y `GM_WHY`, y el estado del juego (`g_save`).
  Cada juego define `SV_BASE` (RAM, unos 110 bytes; `$BC00`), `SV_ID`, `SV_NAMELEN` y la etiqueta `sv_name`.
- **Los relojes** del ajedrez se restauran con lo que quedaba y arrancan al seguir: el tiempo que se está fuera no cuenta.
- **Por la SD:** los comandos del MCU 71 (crear o vaciar y abrir), 53 (abrir), 55 (leer), 56 (escribir) y 57 (cerrar). El
  arnés del PC los simula con una SD de mentira (`sdfiles` en `z80net_env.h`) que sobrevive a «apagar el ZX81» (`reboot()`).
- **No retroceder:** el controlador (`game.inc`, `gp_state`) ignora un estado completo **más viejo** que la partida propia. Antes
  el creador, que vuelve el primero con su guardado, podía hacer retroceder al otro, que va por delante.
- Si el broker olvida un mensaje retenido, quien vuelva primero vuelve a publicar su última jugada; las ranuras son de cada
  jugador (`<sala>/0` y `<sala>/1`), así que nadie pisa la del otro, y el que recibe ignora lo viejo o repetido por el número de turno.
- Límite: el otro jugador tiene que seguir teniendo su estado (guardado en su SD, o en marcha). Si alguien pierde el suyo, hace
  falta que el rival esté conectado para pedirle el estado completo.

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
Para guardar y seguir hay que añadir `savegame.inc` (ver arriba). Lo demás lo hace el controlador: `game_start`, `game_move`, `game_poll` (con sus códigos de resultado en la
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
