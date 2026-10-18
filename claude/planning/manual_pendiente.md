# Manual de usuario: lo que falta añadir o corregir

Lista viva (empezada el 14 de octubre de 2026). Se tacha lo que se hace en los dos manuales
(`MANUAL/ES/SD81_Manual_ES.docx` y `MANUAL/EN/SD81_Manual_EN.docx`: docx primero, ver la nota de memoria
`reference_manual_docx`). Cada punto lleva dónde iría y de qué depende.

## Hecho el 17 de octubre de 2026 (ES y EN, `.docx` y `.md`)

| # | Qué se hizo | Dónde quedó |
|---|---|---|
| 1, 2 | Comandos AT de red (MQTT, SHA-256, HMAC, aleatorios), estados y errores, `/SYS/NET.CFG` | Apéndice M nuevo (Red para programadores) |
| 3 | Juegos por turnos en red: cómo se juega (sala, código, volver, sala llena, relojes), el 4 en raya, el ajedrez, NETCHAT; y la librería `net.inc` / el controlador `game.inc` | sección **9.9** (usuario) y Apéndice M (programadores) |
| 4 | `snap` guarda las mapeadas más las escritas desde el último reset; `snap -a`, todas | Apéndices K y L |
| 5 | CP/M y el depurador | subsección «CP/M y el depurador» al final del Apéndice I, y una limitación en el L |
| 6 | `LOAD *Z81` sin el comando 70: exige el monitor; sin él, informe P | 7.7 (texto y aviso), sección 14 (fila P), tabla de comandos del MCU (el 70 pasa a ser el 75) |
| 7 | Web del depurador: abrirla con la IP, aviso rojo, botones Symbols y Trace, `sym !`, símbolos que no se releen solos | Apéndice L (Desde el navegador, Símbolos y la tabla de la consola) |
| 8 | `ESP32FW.BIN` (sin guion bajo), compatibilidad con módulos viejos y actualización conjunta | 9.7 |
| 9 | **Scrabble en red** (inglés y castellano), con el cursor por sprites, el diccionario en la SD (no incluido: enlaces a los repositorios originales y `EXAMPLES/NETGAME/DICCIONARIOS.md`), `SCRABBLE.SAV`, y para programadores `G_MOVE_VAR`/`G_BIG` de `game.inc` | **Redactado abajo** («Texto redactado: Scrabble»): sección 9.9 (usuario), Apéndice M (programadores), una línea en el Apéndice H y la fila del historial de versiones. Falta pasarlo al `.docx` (ES y EN) y las capturas |

## Pendiente

- ~~El 4 en raya con interfaz gráfica~~ **hecho el 17 de octubre**: 9.9 describe cómo se ve y lleva su captura. Si cambia el aspecto
  tras probarlo en el emulador, rehacer la captura (`DOC/img/connect4_win.png`).
- **Fila nueva en el historial de versiones** (sección 19, ES y EN) al preparar la release, con la versión de cada parte.
- El MQTT avanzado (TLS, usuario y contraseña, QoS 1, retenidos, testamento) **no se ha probado en la placa**: el Apéndice M
  lo describe tal como está implementado.
- Los comandos 68, 69 y 71-74 del MCU no están en la tabla de comandos de la sección 15 (ROMLOCK, `fcreate` y el depurador).

## Para cada release

- Fila nueva en el historial de versiones (ES y EN), con la versión de cada parte (STM32, ROM, FPGA, WiFi).
- Los WAV de 32 bits por muestra se corrigieron (antes sonaban mal); basta con las notas de la release.

## Texto redactado: Scrabble (para pasar al manual, en español)

Estado: redactado el 18 de octubre de 2026, con el juego probado en el PC, en el emulador y en la placa (una partida completa). Falta pasarlo
al `.docx` (flujo docx primero, ES y EN) y las dos capturas de pantalla reales (`DOC/img/scrabble_game.png` y `scrabble_start.png`, a su
tamaño 1:1; se sacan del emulador o con el arnés del PC, `SCRDIR=<carpeta>` y `scr2png.py`). La ruta de la carpeta del juego en `Software.zip`
(`SD81/TOOLS/SCRABBLE` o `SD81/NETGAMES/SCRABBLE`) la decide el empaquetado de la release.

### Cambios en el texto que ya existe de 9.9

- Primer párrafo: «Hay **tres** juegos, el 4 en raya (CONNECT4), el ajedrez (CHESS) y el Scrabble (SCRABBLE), y un chat de prueba (NETCHAT). Están en
  Software.zip, en las carpetas CONNECT4, CHESS, SCRABBLE y NETCHAT.» (el resto del párrafo igual).
- «Partida nueva (1)»: añadir «en el Scrabble empieza el jugador 1 y antes de crear la sala se elige el idioma».
- «Guardar y seguir otro día»: «(CONNECT4.SAV, CHESS.SAV o SCRABBLE.SAV)».
- Colocar los bloques nuevos después de «Relojes del ajedrez» y antes de «Chat de prueba (NETCHAT)».

### Bloque nuevo de 9.9: Scrabble

**Scrabble:**

Scrabble para dos jugadores, en **inglés o en castellano**. Todo lo hace el Z80 del ZX81: las reglas, la puntuación y la comprobación de cada palabra
en un **diccionario que está en la SD**; no hay ningún servidor que juegue ni que compruebe nada. Se carga como los otros juegos (el programa .P es un
cargador pequeño que lee el .BIN de la misma carpeta) y, además de los dos ficheros del juego, necesita el diccionario en esa misma carpeta (ver más
abajo, «Diccionarios del Scrabble»). Si falta, el juego avisa con NO DICTIONARY FILE IN THE SD y no deja jugar.

- **Idioma.** Al crear la sala (1) se elige 1 = ENGLISH o 2 = SPANISH. El idioma va en la primera letra del código de sala (A inglés, B castellano),
  así que quien se une no tiene que elegir nada. La bolsa es siempre de 100 fichas (98 y 2 comodines) con el reparto y los valores oficiales de cada
  idioma (tabla más abajo).
- **La bolsa y los atriles.** Los dos ordenadores barajan la misma bolsa a partir de una semilla que sale del código de sala, y roban en el mismo
  orden: no se mandan fichas por la red, solo las jugadas. Por eso cada programa sabe también qué fichas tiene el rival; **el programa no las
  muestra, pero no son un secreto frente a quien modifique el programa**. Se juega por honradez, entre amigos que se pasan el código de sala. «Otra
  partida» (ENTER al acabar) baraja de nuevo.
- **Tu turno.** Con la partida en marcha, el panel dice YOUR TURN o RIVAL TURN. Una jugada es una **palabra entera** puesta en una fila o en una
  columna, con las fichas de tu atril y las letras que ya haya en el tablero. Se teclea de dos formas, que se pueden mezclar:

  1. **Con el cursor.** Las flechas del ZX81 mueven un marco negro por el tablero: **5** izquierda, **6** abajo, **7** arriba, **8** derecha (con o
     sin SHIFT). El marco es un sprite por hardware (Apéndice H) y parpadea, así que se ve sobre cualquier casilla sin tapar las letras. **0** elige
     la casilla de salida, y después **8** (a la derecha) o **6** (hacia abajo) elige el sentido: aparece una flecha negra en la casilla de al lado.
     A continuación se teclea la palabra completa, tal como quedará en el tablero, y ENTER.
  2. **Escribiéndola**, como en el ajedrez: columna A-O, fila 1-15 (A10, O15), R (derecha) o D (abajo) y la palabra: `H8 R QUIZ` y ENTER. Si se
     teclea la casilla a mano, el cursor se coloca en ella.

  SHIFT+0 borra la última tecla, hasta volver a mover el cursor. Una tecla que no cuadra en esa posición no entra (una columna que no existe, una
  palabra que se saldría del tablero...). Si una letra de la palabra ya está en el tablero se escribe igual: `H8 R QUIZ` puede usar una Q que ya esté
  en H8 y poner solo U, I y Z.
- **El comodín** (la ficha blanca, de valor 0; sale como «?» en el atril). Si te falta una letra y tienes un comodín, el programa lo usa solo. Para
  gastarlo aunque tengas la letra, pon un **punto antes de ella** (`.A`). En el tablero y en el atril el comodín se ve con la tinta azul.
- **Pasar y cambiar.** P y ENTER pasan. X seguida de las fichas que quieres cambiar (de 1 a 7, con un punto para el comodín) y ENTER las cambia por
  otras de la bolsa; hacen falta al menos 7 fichas en la bolsa. Pasar y cambiar apagan el cursor.
- **Lo que se comprueba.** Antes de mandar la jugada, el programa comprueba que pones fichas que tienes, que la primera jugada cubre la casilla
  central (H8), que las demás tocan alguna ficha del tablero, que la palabra no se alarga por los extremos y que **la palabra y todas las palabras
  cruzadas que forma están en el diccionario**. Si no, no la manda y avisa del motivo; no hay impugnación, la jugada que no vale no se hace:

  | **Aviso** | **Significa** |
  |---|---|
  | BAD POSITION OR LETTER | casilla o ficha que no existe, o palabra que se sale del tablero |
  | YOU DO NOT HAVE THOSE TILES | pones una ficha que no tienes (y sin comodín para sustituirla) |
  | LETTER DOES NOT MATCH THE BOARD | una letra de la palabra choca con la que ya está en esa casilla |
  | PLACE AT LEAST ONE TILE | la palabra solo usa fichas que ya estaban |
  | THE WORD WOULD BE LONGER | hay letras pegadas por delante o por detrás: la palabra tendría que incluirlas |
  | FIRST WORD MUST COVER THE CENTER | la primera jugada no pasa por H8 |
  | THE WORD MUST TOUCH THE TILES | la palabra no toca ninguna ficha del tablero |
  | *XXXX* IS NOT A WORD | la palabra (con la que falla) no está en el diccionario |
  | *XXXX* (CROSSING) IS NOT A WORD | una palabra cruzada no está en el diccionario |
  | A WORD HAS 2 TO 15 TILES | palabras de 2 a 15 fichas |
  | THE BAG HAS LESS THAN 7 TILES | no se puede cambiar |
  | IT IS NOT YOUR TURN / THE GAME IS OVER | no te toca / la partida ha acabado |
  | NETWORK ERROR: R RESENDS | falló la red: la jugada está hecha en local; R la vuelve a mandar |
  | FINISH THE MOVE FIRST | ENTER con la jugada a medias (falta la palabra, o tiene una sola ficha) |

- **Puntuación.** La oficial: cada ficha vale lo que dice la tabla, la letra doble o triple (cian y azul) multiplica solo la ficha que acabas de poner,
  la palabra doble o triple (magenta y rojo, y el centro, que es doble) multiplica toda la palabra, y se cuentan **la palabra principal y todas las
  cruzadas**. El comodín vale 0. Usar las 7 fichas del atril en una jugada da **50 puntos más**. La partida acaba cuando un jugador se queda sin fichas
  con la bolsa vacía (se queda con el valor de las fichas que le quedan al otro, y el otro lo pierde) o tras **seis pases o cambios seguidos** (cada
  uno resta el valor de su atril). Gana quien tiene más puntos; el panel dice YOU WIN, THE RIVAL WINS o DRAW.
- **La pantalla** (alta resolución, Superfast HiRes Spectrum). A la izquierda, el tablero de 15×15 casillas con las letras A-O arriba y los números 1-15 a
  la izquierda; el centro lleva una estrella. Las casillas especiales van de color, con la leyenda en el panel: **DL** (letra doble) cian, **TL** (letra
  triple) azul, **DW** (palabra doble) magenta y **TW** (palabra triple) rojo. Las fichas son blancas; las de **la última jugada** (la tuya o la del
  rival) se resaltan en **amarillo**. Debajo, **tu atril** con el valor de cada ficha, y a la derecha el panel: tus puntos (YOU) y los del rival (RIVAL),
  las fichas que quedan en la bolsa (BAG), de quién es el turno, la última jugada con su autor y sus puntos (LAST MOVE), la leyenda de colores, el código
  de sala y Q = QUIT. Abajo, el estado, los avisos, la ayuda de la tecla que toca y lo que vas tecleando.

  ![](media/scrabble_game.png){width="2.6666666666666665in" height="2.0in"}

  *Scrabble: una partida en curso, con el cursor y la última jugada en amarillo (captura de la pantalla real, a su tamaño).*
- **Castellano.** Se juega con el Scrabble en castellano tradicional: **CH, LL y RR son una sola ficha** (no se pueden hacer con C+H, L+L o R+R) y la
  **Ñ** es otra. Al teclear, C H, L L y R R se juntan solos en una ficha (borrar la H deja la C sola) y la tecla **1** es la Ñ. En el tablero y en el
  atril se dibujan con su propio símbolo (CH, LL, RR y Ñ ocupan una casilla). No hay K ni W en la bolsa. El diccionario no distingue tildes.
- **Teclas.** Con nada tecleado: 5 6 7 8 mueven el cursor, 0 elige la casilla, **R** reenvía la última jugada si falló la red, **Q** sale (los dígitos
  son las flechas y las filas del tablero, por eso estas dos órdenes son letras), y **ENTER** propone otra partida cuando la actual ha acabado. P pasa y X
  cambia fichas.
- **Guardar y seguir otro día.** Como en los otros juegos: el programa guarda la partida en `SCRABBLE.SAV` (unos 400 bytes) en la carpeta del juego, y el
  menú ofrece «3 = CONTINUE SAVED GAME». Para seguirla hace falta tener el diccionario del idioma de la partida.

**Reparto y valores de las fichas:**

| **Inglés** (98 + 2 comodines) | **Castellano** (98 + 2 comodines) |
|---|---|
| 1 punto: A×9 E×12 I×9 O×8 N×6 R×6 T×6 L×4 S×4 U×4 | 1 punto: A×12 E×12 O×9 I×6 S×6 N×5 R×5 U×5 L×4 T×4 |
| 2 puntos: D×4 G×3 | 2 puntos: D×5 G×2 |
| 3 puntos: B×2 C×2 M×2 P×2 | 3 puntos: B×2 C×4 M×2 P×2 |
| 4 puntos: F×2 H×2 V×2 W×2 Y×2 | 4 puntos: F×1 H×2 V×1 Y×1 |
| 5 puntos: K×1 | 5 puntos: CH×1 Q×1 |
| 8 puntos: J×1 X×1 | 8 puntos: J×1 X×1 LL×1 Ñ×1 RR×1 |
| 10 puntos: Q×1 Z×1 | 10 puntos: Z×1 |
| comodín ×2: 0 puntos | comodín ×2: 0 puntos |

**Diccionarios del Scrabble:**

El Scrabble necesita un diccionario por idioma en la carpeta del juego, con estos nombres exactos: **DICTEN.DIC** (inglés, 2,6 MB) y **DICTES.DIC**
(castellano, 9,7 MB). **No vienen incluidos**: se generan una vez con el script `TOOLS/make_dict.py` del repositorio a partir de listas de palabras
públicas, que hay que descargar de su origen (consulta sus condiciones de uso):

- Inglés: la lista **ENABLE** (de dominio público), <https://github.com/dolph/dictionary> (`enable1.txt`).
- Castellano: las listas oficiales **FISE-2** y **FILE 2017**, recogidas en <https://github.com/kamilmielnik/scrabble-dictionaries/tree/master/spanish>
  (`fise-2.txt`, `file-2017.txt`; traen las formas conjugadas y los plurales). Ese repositorio no declara licencia para ellas, por eso este proyecto no
  las redistribuye.

Con Python 3, desde una carpeta con las listas descargadas:

> python make_dict.py enable1.txt DICTEN.DIC
>
> python make_dict.py --lang es fise-2.txt DICTES.DIC

Después se copian los `.DIC` a la carpeta del juego en la SD. Las instrucciones completas, con las órdenes para descargar las listas, están en
`EXAMPLES/NETGAME/DICCIONARIOS.md`. Cada palabra se busca con una **búsqueda binaria directamente en la SD**: unos 20 accesos y unos 40 milisegundos
por palabra, así que una jugada con varias palabras cruzadas se comprueba en menos de medio segundo.

### Bloque nuevo del Apéndice M (programadores)

En «La librería net.inc y el controlador game.inc», después de la tabla de rutinas y del párrafo de los mensajes, añadir:

> **Jugadas de longitud variable y estado grande.** El Scrabble no cabe en el formato de arriba: una jugada mide hasta 21 bytes y el estado, 355. Por
> eso game.inc tiene dos opciones más, que los otros juegos dejan a 0. Con **G_MOVE_VAR = 1**, G_MOVE_LEN es el máximo y el primer byte de la jugada es
> su longitud (contada entera): solo se manda esa parte, `[1][jugada][hash]`. Con **G_BIG = 1** el estado no cabe en un mensaje de 100 bytes: se manda
> en **trozos de 96 bytes**, `[3][número de trozo][cuántos][datos]`, que el receptor junta en un búfer y carga cuando están todos; el resumen del estado lo
> calcula el juego (la rutina g_hash, sin copiar el estado). Al terminar de mandar el estado, si la última jugada fue del que lo manda, la vuelve a
> mandar, para que sea lo último que queda retenido en el servidor. No se pueden usar con G_CLOCK.
>
> El Scrabble (EXAMPLES/NETGAME/scrabble.asm, con scrabble.inc, dict.inc, scrabble_ui.inc, scrabble_in.inc y scrabble_game.inc) es un ejemplo de las
> dos opciones. Su jugada es `[L][tipo][datos]`: tipo 1 jugar (x, y, sentido, n y las fichas), 2 pasar, 3 cambiar (n y las fichas). **El idioma y la
> semilla de la bolsa no se mandan**: salen del código de sala (la primera letra y las 10 letras, con el número de partida de la sala), así que los
> dos ordenadores barajan la misma bolsa sin decirse nada. El programa ocupa unos 13 KB y se carga en $4400 (CLEAR 17407), no en $5000, porque no
> cabría; todo el código tiene que quedar por debajo de $8000.

### Otros sitios del manual

- **Historial de versiones (sección 19):** la fila de la versión que incluya el Scrabble debe decir «Scrabble en red, en inglés y castellano, con el
  diccionario en la SD, y cursor por sprites».
- **Apéndice H (Sprites):** una línea en «Capacidad» o al final: «El Scrabble usa tres sprites (el cursor y dos flechas) y hace parpadear el cursor
  cambiando su máscara a 0, de modo que no borra ningún punto del fondo mientras está apagado.»

## Cómo se toca

Flujo docx primero con la skill `docx` (no Opus), en ES y EN a la vez, y regenerar los `.md` que
acompañan a cada manual. Antes de dar un punto por hecho, buscarlo en el manual (`pandoc … -t plain`)
y comprobar que los dos idiomas coinciden.
