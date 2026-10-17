# 4 en raya gráfico: lista de pruebas (17 de octubre de 2026)

> **Estado:** pasado en el PC (el Z80 de verdad con teclado y pantalla simulados, `claude/mqtttest/test_z80ui.cpp`).
> **Sin probar en el emulador ni en la placa.** El ajedrez ya está probado en los dos (`ajedrez_pruebas.md`); el 4 en raya
> comparte con él el lobby, la sala, la red y `gfx.inc`, y lo nuevo es el dibujo del tablero.

Programa: `EXAMPLES/NETGAME/CONNECT4.B81` + `connect4.bin` (`compila.bat` lo ensambla y lo copia al emulador).
Hace falta **dos instancias** (emulador o placa). El programa empieza en `$6000` y ocupa 6.262 bytes (tope: 8.192).

Qué ha cambiado respecto a la versión de texto (la del 15 y 16 de octubre):

- La partida se dibuja en **Superfast HiRes Spectrum**; el lobby y los menús siguen en texto.
- Las variables y la pantalla de texto del lobby están ahora como en el ajedrez (`$8800` y `$9000`), porque el código
  ya no cabía bajo las variables de antes (`$7800`).
- `gfx.inc` es común: `GFX_SQUARES` (las casillas del ajedrez) es 1 en el ajedrez y 0 aquí; `ds_cell` pinta una celda de 8×8.
- `g_winner` (`connect4.inc`) guarda dónde está la línea ganadora (`C4_WON/WX/WY/WD`) y `c4_inwin` dice si una casilla está en ella.

## 1. Arranque y sala (igual que antes; el lobby no ha cambiado)
- [ ] Cargar `CONNECT4.B81` en las dos instancias; menú; crear sala en la 1 y unirse en la 2 con el código.
- [ ] Sala llena (un tercero), sala que no existe, salir y volver, sala vacía tras caerse los dos: como en el ajedrez.

## 2. Pantalla
- [ ] Al empezar la partida la pantalla pasa de texto a gráfico sin basura; el borde es negro.
- [ ] El título («NETWORK CONNECT 4») en una barra amarilla; los números **1-7** sobre las columnas, cada uno centrado en la suya.
- [ ] El tablero: 7 columnas × 6 filas de **azul brillante**, con los huecos en **negro** (discos de 20×20 en casillas de 24×24).
- [ ] El panel de la derecha: `ROOM` y el código, `YOU ARE` y **RED** (rojo) o **YELLOW** (amarillo), `RIVAL` y `HERE` / `AWAY`, `LAST MOVE` y `COLUMN n` (o `--------` al empezar).
- [ ] El jugador 1 (quien crea la sala) tiene las fichas **rojas** y empieza; el 2, las **amarillas**.
- [ ] Debajo: el estado en una caja verde con letra negra (`YOUR TURN: PRESS 1 TO 7`, `THE RIVAL IS THINKING...`, `YOU WIN`...), el aviso en rojo y la ayuda en cian.
- [ ] La **última ficha** (la que acaba de caer, tuya o del rival) tiene el fondo **blanco**; al jugar otra, el blanco pasa a ella.
- [ ] Al ganar, las **cuatro en línea** quedan en blanco (vertical, horizontal y las dos diagonales: probar las cuatro) y las demás fichas no.
- [ ] Al acabar en tablas (tablero lleno), `DRAW` y sin cuatro resaltadas.
- [ ] La velocidad: repintar las 42 casillas tras cada jugada no se nota (si parpadea, habría que repintar solo la columna que cambia).
- [ ] Al salir con `Q` vuelve el menú de texto de siempre (sin restos del tablero ni colores).

## 3. Jugadas
- [ ] Teclas 1-7: la ficha cae en la columna elegida, sobre las que ya hay.
- [ ] Columna llena: `THAT COLUMN IS FULL` y no se pierde el turno. Fuera de turno: `IT IS NOT YOUR TURN`. Con la partida acabada: `THE GAME IS OVER`.
- [ ] Rebote del teclado: una tecla mantenida no repite la jugada.

## 4. Fin y otra partida
- [ ] `ENTER` con la partida acabada pide otra; el rival la acepta y las dos pantallas quedan con el tablero vacío y sin resaltados.
- [ ] El rival pide otra partida a medias: el tablero se vacía y me vuelve a tocar.

## 4b. Guardar y seguir otro día (nuevo, 17 de octubre)
- [ ] Tras jugar un par de jugadas aparece `CONNECT4.SAV` en la carpeta del juego (66 bytes). No se escribe en cada sondeo, solo cuando cambia la partida.
- [ ] Apagar y encender (o `RUN`): el menú ofrece `3 = CONTINUE SAVED GAME`; con él, vuelve a la misma sala con el mismo color y el tablero como estaba.
- [ ] Si la última jugada fue mía, se vuelve a mandar (se ve en el rival, que la ignora por repetida). Si fue del rival y estaba retenida, llega al seguir.
- [ ] **Asíncrono de verdad:** A juega y apaga; B juega y apaga; A vuelve otro día, sigue y recibe la de B. Los dos nunca coinciden en línea.
- [ ] Dos instancias del emulador, cada una con **su carpeta de juego**: cada una guarda y sigue su partida sin pisar a la otra (con la misma carpeta, comparten fichero y las dos serían la misma ranura).
- [ ] Sin fichero, el menú no ofrece el 3. Con un fichero que no es de este juego (o roto), el 3 vuelve al menú sin colgarse.
- [ ] Empezar otra partida o unirse a otra sustituye el guardado.

## 5. Red (como el ajedrez)
- [ ] Una partida completa entre dos instancias, sin desincronizarse.
- [ ] `R` reenvía la última jugada tras cortar la red de uno; se resincroniza.
- [ ] El rival se va (`THE RIVAL HAS LEFT`, `AWAY`) y vuelve (`THE RIVAL HAS ENTERED`, `HERE`).
- [ ] Reentrar a mitad de partida (salir con `Q` y volver con el código): el tablero sale con las fichas que había y con la última resaltada.
