# SOLITARIO — Klondike con ratón en HiRes 8x1 (color)

Solitario Klondike (robo de 1 o de 3) para el SD81 Booster, en el modo **Superfast HiRes nativo**
(`POKE 2045,171`) con **Chroma modo 0** (un atributo por cada byte de línea, 8x1): así las cartas
rojas y negras se solapan a cualquier altura sin choque de color. Se juega entero con el **ratón
Bluetooth** (arrastre con sprites de hardware); el teclado solo tiene N, D y Q de repuesto. El plan
completo (decisiones, geometría, memoria, fases) está en `claude/planning/solitario.md`.

| Carpeta / fichero | Qué es |
|---|---|
| `solitario.asm`, `SOLITARIO.B81`, `compila.bat` | el juego (todo en una página, `$5000-$7FFF`) |
| `gen_cards.py`, `cartas.png` | generador y hoja de las cartas (1:1; se puede retocar a mano y volver a ejecutar `gen_cards.py`) |
| `cards.inc` | tablas de `gen_cards.py` que usa `draw_card` (no se edita a mano) |
| `preview_mesa.png` | mesa de ejemplo de la fase 0/1, para comparar a ojo |

## Cómo se juega

- **Arrastrar** una carta destapada del tablero (se lleva ella y las que tenga encima), la de arriba
  del descarte o de una fundación: mantener el botón izquierdo y soltar sobre el sitio. Si el
  movimiento es legal se queda ahí; si no, vuelve deslizándose a su sitio. Un clic sin mover (menos
  de 3 px) no cuenta como arrastre.
- **Doble clic o clic derecho** sobre la carta de arriba de una columna o del descarte: la manda
  directamente a la fundación si cabe (como el "enviar a la fundación" de las versiones de PC).
- **Clic en el mazo**: roba 1 o 3 cartas (según el modo) al descarte; con el mazo vacío, un clic
  recicla el descarte entero, sin límite de vueltas.
- Al quedar sin nada encima, la carta tapada de una columna **se destapa sola**.
- Reglas de siempre: tablero alternando color y bajando de uno en uno (columna vacía solo admite un
  Rey); fundación del mismo palo subiendo desde el As (se puede bajar una carta de la fundación al
  tablero).
- **Autocompletar**: en cuanto el mazo y el descarte están vacíos y todo el tablero boca arriba, el
  juego se pone solo a mandar las cartas a las fundaciones (una detrás de otra, deslizándose).
- **Victoria**: al vaciarse el tablero, las cartas saltan de las fundaciones de la K al As, caen con
  gravedad y rebotan dejando una estela de color. Un clic o cualquier tecla reparte una partida nueva
  (no hace falta esperar a que acabe).

### Barra de estado (abajo)

`SCORE 0000  00:00   NEW  D3  QUIT` — puntos, tiempo mm:ss y tres botones con zona de clic: **NEW**
(partida nueva), **D3/D1** (cambia el robo de 1 a 3 cartas o viceversa y reparte) y **QUIT** (sale a
BASIC). Las mismas acciones están en el teclado: **N** nueva, **D** cambia el robo, **Q** sale
(lectura directa del puerto `$FE`, como en MINAS: en FAST la ROM no puede ayudar a leerlo).

### Puntos y tiempo (estilo Windows)

Descarte → tablero +5, descarte/tablero → fundación +10, destapar una carta +5, fundación → tablero
−15, reciclar el descarte −100 (robo de 1) / −20 (robo de 3); −2 cada 10 s; nunca por debajo de 0 ni
por encima de 9999. El reloj (`FRAMES`, 50 Hz) empieza con el primer movimiento o el primer clic en
el mazo.

## Estado y qué falta probar

Hecho (fases 0-5, ver `claude/planning/solitario.md`): gráficos aprobados por el usuario, vídeo y
dibujo, modelo y reparto, arrastre con sprites y reglas, doble clic/clic derecho + puntos + tiempo +
barra, y autocompletar + animación de victoria. Todo comprobado en el arnés del PC
(`claude/mqtttest/build_solitario.sh`, Z80 de EightyOne con un modelo de referencia en Python para las
fases 1-2 y llamadas directas a las rutinas del Z80 para las fases 3-5: arrastre legal e ilegal,
destapar sola, clic derecho/doble clic, puntos, reloj, `auto_check`/`auto_step` y `win_step`, siempre
sin escrituras fuera de sitio).

**Falta probar**: el aspecto real en pantalla (colores, el deslizamiento al arrastrar y al
autocompletar, la caída con rebote y la estela de la animación de victoria), el ratón Bluetooth real
(el arnés del PC llama a `pickup`/`drop`/`drag_loop` directamente porque ese protocolo necesita el
MCU) y todo en el emulador EightyOne y en la placa. Sin probar tampoco si `FRAMES` avanza como se
espera en modo FAST (afecta al reloj de la partida y al doble clic). No hay deshacer (decisión de la
v1) ni fase 6 de documentación de usuario en el manual todavía (pendiente de anotar en
`claude/planning/manual_pendiente.md`).

## Memoria

Código y tablas en `$5000-$7FFF` (12 KB; `compila.bat` lo comprueba). Variables y modelo en el bloque 4
(`$8000-$86FF`: buzón del ratón, modelo de las 52 cartas, búfer de arrastre, puntos/tiempo,
autocompletar/victoria). Bitmap en `$A000-$B7FF`, atributos en `$C000-$D7FF`. Necesita la ROM sin
`LOAD *ROMLOCK` (usa los sprites 2100-2131).

## Ensamblar y ejecutar

```
compila.bat
```

Ensambla con `pasmo` (usa `..\MOUSE\mouselib.inc`), comprueba que `solitario.bin` no pasa de 12288
bytes y copia `SOLITARIO.BIN`/`SOLITARIO.B81` a `SD81\SD81\GAMES\SOLITARIO` del emulador (como MINAS y
ESTELA: es un juego de un jugador, no va en `NETGAMES`). Para la placa, copiar esos dos ficheros (con
la carpeta `SYS` de siempre) a la SD y cargar `SOLITARIO.B81`.
