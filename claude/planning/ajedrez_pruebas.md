# Ajedrez en red: lista de pruebas (16 de octubre de 2026)

> **Estado:** secciones 2, 3, 4, 5, 6 y 7 probadas en el emulador por el usuario el 16 de
> octubre; en la 1 funcionan la sala, la sala llena y el error de sala inexistente. Sin probar: reentrar tras salir, la
> regla de las 50 jugadas y el material insuficiente (solo en el PC), el error sin WiFi y la entrada simultánea
> de dos jugadores. En la placa, nada.

Programa: `EXAMPLES/NETGAME/CHESS.B81` + `chess.bin` (`compila.bat` lo ensambla y lo copia al emulador).
Hace falta **dos instancias** (emulador o placa). Estado: verificado en el PC (perft, partida por teclado
simulado); **sin probar en el emulador ni en la placa**.

Las jugadas se teclean como en la UI: origen + destino (`E2E4`) + ENTER; una quinta letra elige la pieza al
coronar (Q, R, B, N). Las secuencias de abajo van «blancas / negras» por turno. **Todas están pasadas por el
motor del Z80 en el PC** (16 de octubre): son legales de principio a fin, el mate del pastor da victoria a las
blancas y la de Loyd acaba en tablas.

## 1. Arranque y sala
- [x] Cargar `CHESS.B81` en las dos instancias (`LOAD "CHESS.B81"`, o `LOAD THEN CLEAR 24575` + `LOAD "CHESS.BIN" CODE 24576` + `RAND USR 24576`).
- [x] Menú: crear sala en la 1 (sale un código de 10 caracteres), unirse en la 2 con ese código.
- [x] Sala llena: una **tercera** instancia con el mismo código recibe «THE ROOM IS FULL» y la partida de las otras dos no se altera.
- [x] Código mal escrito / sala que no existe: `THERE IS NO SUCH ROOM` y vuelta al menú, sin colgarse (arreglado el 16 de octubre).
- [x] **Salir y volver**: con `0` sal siendo el que se unió y vuelve con el mismo código (entra como jugador 2, recibe la posición). Sal siendo el creador y vuelve con «unirse» y el mismo código: entras como jugador 1 (blancas) y la partida sigue donde estaba.
- [ ] Un tercero con el código mientras uno está fuera puede ocupar su sitio (limitación conocida).
- [ ] **Se cayeron los dos** (cortar la red de las dos instancias, esperar a que el broker dé por caídas las dos conexiones —hasta ~90 s— y volver a unirse con el código): sale `THE ROOM IS EMPTY` y se elige `1` (blancas) o `2` (negras); el segundo que vuelve entra solo en la que queda y la partida sigue donde estaba.
- [x] Sin WiFi o sin broker: mensaje de error legible (`NETWORK ERROR`, sin colgarse; probado el 16 de octubre).

## 2. Pantalla (gráfica desde el 16 de octubre: HiRes Spectrum; probada en el emulador por el usuario el 16 de octubre; los gráficos se quedan como están)
- [x] Al empezar la partida la pantalla pasa de texto a gráfico sin basura; el borde es negro.
- [x] El tablero: 8×8 casillas de 16×16 puntos, **a1 roja** (esquina de abajo a la izquierda con las blancas), alternando rojo oscuro y verde claro, todo brillante; coordenadas A-H bajo el tablero y 1-8 a la izquierda, **centradas** en cada casilla.
- [x] Piezas: macizas, las blancas con tinta blanca y las negras con tinta negra (dibujo de Cyrus II reducido); los seis tipos se distinguen (peón, caballo, alfil, torre, dama, rey) y las blancas se leen bien sobre el verde.
- [x] Blancas (quien crea la sala): abajo; negras (quien se une): el tablero **girado** (1 arriba, columnas H..A).
- [x] Panel de la derecha: `ROOM` y el código, `YOU ARE` + `WHITE`/`BLACK`, `LAST MOVE` y la jugada como `E2-E4` (`-----` al empezar).
- [x] **Resaltados**: las dos casillas de la última jugada en **magenta**; el rey en jaque, con el papel contrario a su tinta (el rey **blanco** sobre **negro**, el rey **negro** sobre **blanco**).
- [x] Debajo: el estado en una caja verde con letra negra (`YOUR MOVE`, `THE RIVAL IS THINKING...`, `CHECKMATE...`), el aviso en rojo si es un error, `MOVE: ` con lo tecleado y la ayuda en cian; arriba, el título en una barra amarilla.
- [x] Pulsar ENTER fuera de turno: aviso «IT IS NOT YOUR TURN», sin romper nada; una jugada rechazada **no cambia** `LAST MOVE` ni el resaltado.
- [x] Al salir con `0` vuelve el menú de texto de siempre (sin restos del tablero ni colores).
- [x] Rebote del teclado: una tecla mantenida no repite letras.
- [x] La velocidad: repintar el tablero entero tras cada jugada no se nota (si parpadea, habría que repintar solo las casillas que cambian).

## 2b. Una carrera corregida el 16 de octubre (jugar justo cuando el sondeo tiene una petición pendiente)
- [ ] Jugar (o ENTER para otra partida, o `9`) en cualquier momento, también justo después de que el rival haya jugado: sin pausa de unos segundos y sin el aviso `MODEM ERROR`, y sin perder la jugada del rival. (Antes, si el envío caía entre la petición del sondeo y su recogida, el módem tiraba esa respuesta y luego se esperaba en balde: `gm_drain` en `game.inc`.)

## 3. Teclado
- [x] Una letra o cifra que no cuadra en esa posición no entra (p. ej. `Z` o `9` como columna).
- [x] **SHIFT+0** borra la última tecla; borrar con la línea vacía no hace nada.
- [x] `0` sale al menú (probado); `9` reenvía la última jugada (probado).
- [x] ENTER con 4 caracteres juega; con menos, no hace nada.

## 4. Jugadas
- [x] **Ilegales rechazadas** (mensaje, sin perder el turno): mover una pieza del rival; una casilla vacía; una pieza pinada; dejar al propio rey en jaque; el rey a una casilla atacada; un peón hacia atrás o de lado.
- [x] **Enroque corto** (blancas): `E2E4 E7E5 / G1F3 B8C6 / F1C4 F8C5 / E1G1`. El rey va a G1 y la torre a F1, y se ve en las dos pantallas.
- [x] **Enroque largo** (blancas): `D2D4 D7D5 / B1C3 B8C6 / C1F4 C8F5 / D1D2 D8D7 / E1C1`. Rey en C1, torre en D1.
- [x] Enroque ilegal: con el rey en jaque, atravesando una casilla atacada, o tras haber movido el rey o la torre.
- [x] **Al paso**: `E2E4 A7A6 / E4E5 D7D5 / E5D6`. El peón negro de D5 desaparece. Y solo vale en la jugada inmediatamente siguiente.
- [x] **Coronación**: `A2A4 B7B5 / A4B5 A7A6 / B5A6 G8F6 / A6A7 F6G8 / A7B8Q`. Prueba también `A7B8N` (subpromoción a caballo) y sin quinta letra (dama).
- [x] La coronación sin quinta letra a otra pieza que no sea Q/R/B/N se rechaza.

## 5. Fin de partida
- [x] **Jaque**: `E2E4 F7F6 / D1H5` muestra `CHECK` al negro.
- [x] **Mate del pastor**: `E2E4 E7E5 / F1C4 B8C6 / D1H5 G8F6 / H5F7`. Blancas: «CHECKMATE - YOU WIN»; negras: «CHECKMATE - THE RIVAL WINS».
- [x] **Ahogado** (mate de Loyd, 10 jugadas): `E2E3 A7A5 / D1H5 A8A6 / H5A5 H7H5 / H2H4 A6H6 / A5C7 F7F6 / C7D7 E8F7 / D7B7 D8D3 / B7B8 D3H7 / B8C8 F7G6 / C8E6`. Debe acabar en tablas (`DRAW`), no en mate.
- [ ] Regla de las 50 jugadas y material insuficiente (rey solo contra rey, o con un caballo o un alfil): comprobadas en el PC; en el emulador, solo si hay paciencia.
- [x] **No** hay triple repetición: repetir la misma posición tres veces no acaba la partida (es una limitación conocida).
- [x] ENTER con la partida acabada pide **otra**; el rival la acepta y empieza una nueva con las mismas posiciones.

## 6. Red
- [x] Una partida larga (30 jugadas o más) sin desincronizarse.
- [x] Desconectar el WiFi de una instancia a mitad de una partida y volver: se recupera (petición de estado), o con `9`.
- [x] Una jugada duplicada o reenviada no se aplica dos veces.
- [x] Cerrar una instancia a medias y reentrar a la misma sala: «stale» de la presencia hasta unos 90 s; después entra.
- [ ] Dos jugadores que se unen exactamente a la vez (limitación conocida; no hace falta forzarla).

## 7. Rendimiento
- [x] Tiempo desde ENTER hasta que se pinta la jugada propia y la del rival (la comprobación de jugadas legales cuesta ~0,1-0,3 s en un Z80 real; en el emulador depende de la velocidad).
- [x] La pantalla no parpadea al esperar (en SLOW se ve el parpadeo estándar del `PAUSE`; el programa usa Superfast).
- [x] Salir con `0` devuelve el BASIC en su estado normal (modo de vídeo, `SLOW`/`FAST`, sin basura en pantalla).
