# Manual: texto de ESTELA para la próxima release

Escrito el 9 de noviembre de 2026, con el juego entero probado en la placa (fases 0-8d). La sección del manual
(fase 8c del plan `juego160.md`) se aplaza a la próxima release; aquí queda el texto listo para pasarlo.
Al pasarlo: flujo docx primero con la skill `docx` (ver `manual_pendiente.md`, «Cómo se toca»), en ES y en EN
(el EN se traduce de este texto), y regenerar los `.md` de los dos manuales.

## Dónde va

| Qué | Dónde |
|---|---|
| Sección de usuario (lo de abajo, «13.7») | capítulo 13, «Ejemplos de programas», detrás de la 13.6 |
| Una línea de referencia | Apéndice F, «Modo de 256 colores de 160x100», junto a «Ejemplo: EXAMPLES/MODE160»: *Juego completo con este modo, los sprites de formato B y el scroll en anillo: ESTELA (13.7, fuentes en EXAMPLES/ESTELA).* |
| Otra línea | Apéndice H (sprites), donde se habla del formato B y la hoja: *ESTELA (13.7) usa una hoja de 128 celdas de formato B en el bloque 3, 64 sprites y la paleta de 16.* |
| Índice | entrada nueva para la 13.7 (actualizar el índice con Word, como siempre) |
| Historial de versiones | en la fila de la release: «ESTELA, matamarcianos para el modo de 256 colores (13.7)» |

**Comprobar al empaquetar** (no lo sé hoy y el texto lo deja entre corchetes):
- La carpeta del juego en `Software.zip`. `compila.bat` lo copia al emulador en `SD81\SD81\GAMES\ESTELA`, así que lo esperable es `SD81/GAMES/ESTELA`.
- La versión mínima de la FPGA. El juego necesita el modo 175, el scroll en anillo (`POKE 2072-2075`) y la hoja de formato B (`POKE 2129`).
- Qué músicas van de verdad en la carpeta: las de `out/` o las provisionales `ledstorm-*` de `EXAMPLES/VGM`.

**Capturas** (no hay ninguna): el título, una partida con oleadas, el jefe con WARNING y el GAME OVER. Hay que sacarlas de la
placa, porque el emulador todavía no mueve bien el fondo. Irían en `DOC/img/estela_*.png`.

---

## 13.7 ESTELA: un matamarcianos de 256 colores

ESTELA es un matamarcianos vertical hecho para enseñar lo que da el modo de 160x100 a 256 colores (Apéndice F). Hay cinco niveles con
escenarios de tiles, y el fondo baja con el scroll en anillo de la FPGA. Las naves son sprites de formato B y la música es VGM. Los
efectos de sonido van por los chips AY del interface.

### Instalación y carga

ESTELA está en Software.zip, en la carpeta [SD81/GAMES/ESTELA]. Copia la carpeta entera a la SD y carga `ESTELA.B81`. Este cargador
lee `ESTELA.BIN` y el resto de ficheros de la misma carpeta, así que todos tienen que estar juntos:

| Fichero | Qué es |
|---|---|
| `ESTELA.B81`, `ESTELA.BIN` | el cargador BASIC y el juego |
| `HOJA.BIN` | los gráficos de las naves (sprites) |
| `NIVEL1.I16` … `NIVEL5.I16` | el escenario de cada nivel (imagen `.I16` con los tiles y su paleta) |
| `NIVEL1.MAP` … `NIVEL5.MAP` | el mapa y las oleadas de cada nivel |
| `TITULO.I16` | la pantalla del título (opcional: sin ella, la partida empieza directamente) |
| `NIVEL1.VGM` … `NIVEL5.VGM`, `TITULO.VGM`, `JEFE.VGM`, `FINAL.VGM`, `GAMEOVER.VGM` | las músicas (todas opcionales) |
| `ESTELA.REC` | el récord; lo crea el propio juego la primera vez que lo superas |

Hace falta la FPGA [versión x.y] o posterior. El emulador EightyOne todavía no tiene el scroll en anillo de los modos de 160 píxeles,
así que allí el fondo no se mueve bien: el juego está pensado para la placa.

### El título

Si está `TITULO.I16`, el juego empieza en el título, con el récord debajo y la música `TITULO.VGM`. ESPACIO empieza la partida,
X vuelve a BASIC y M quita o pone la música.

### Teclas

| Tecla | Acción |
|---|---|
| O P Q A | mover la nave (izquierda, derecha, arriba, abajo) |
| ESPACIO | disparar; si la mantienes pulsada, dispara sola |
| B | soltar una bomba (si llevas alguna) |
| M | música sí / no (vale también en el título; dura hasta salir del juego) |
| R | volver a empezar el nivel actual, con 3 vidas y el marcador a 0 |
| S | velocidad del scroll: la del nivel → la mitad → el doble → parado → la del nivel. El color del borde indica cuál está puesta: verde, cyan, amarillo o rojo |
| I | vidas infinitas sí / no, para practicar (el borde se pone brillante) |
| X | salir a BASIC |

Con el joystick DB9: antes de cargar el juego, `LOAD *JOY "OPQA "` (9.6). Así el mando mueve la nave y el botón dispara.

### Cómo se juega

- **Vidas y marcador.** Empiezas con 3 vidas. Cada enemigo derribado da de 10 a 60 puntos, según el tipo. Para que no tape el
  escenario, el marcador solo sale arriba un momento: cuando pierdes una vida, cuando recoges una mejora y al acabar el nivel. En ese
  momento salen también las vidas y las bombas que te quedan. Después de perder una vida, la nave parpadea un rato y no la pueden
  derribar.
- **Mejoras.** De vez en cuando baja una mejora; pasa por encima para recogerla:
  - **Arma:** disparo doble hasta que pierdas una vida.
  - **Escudo:** unos 5 segundos sin que te puedan derribar.
  - **Bomba:** se guarda para usarla con B; puedes llevar hasta dos. Al soltarla, la pantalla tiembla, todos los enemigos que están a
    la vista explotan (y dan sus puntos) y el jefe pierde 10 impactos de vida. Una bomba nunca acaba con el jefe sola.
- **El jefe.** Al final de cada nivel parpadea el aviso WARNING y entra el jefe, con la música `JEFE.VGM`. El jefe va de lado a lado
  y dispara ráfagas. En el primer nivel aguanta 40 impactos, y cada nivel aguanta más. Al derribarlo sumas 1.000 puntos y sale
  WELL DONE.
- **Niveles.** Al empezar cada nivel sale su número un par de segundos y, tras el jefe, se carga el siguiente. Mientras se carga, la
  pantalla se queda quieta un momento. Conservas el marcador y las vidas, pero el arma y las bombas se pierden. Al vencer al jefe del
  nivel 5 suena `FINAL.VGM` con un WELL DONE largo, de casi un minuto. Después el juego vuelve al nivel 1 con una vuelta más de
  dificultad: los enemigos disparan más a menudo y el jefe aguanta más y dispara más deprisa. Hay dos vueltas extra como mucho.
- **Fin de la partida.** Sin vidas sale GAME OVER y suena `GAMEOVER.VGM`. Pasados unos 2 segundos, ESPACIO empieza otra partida
  desde el nivel 1, con la dificultad inicial.
- **Récord.** Si al acabar la partida has superado el récord, el juego lo guarda en `ESTELA.REC`, en la carpeta del juego. Para
  ponerlo a cero, borra ese fichero.

### Las músicas

Todas son opcionales. Si falta la de un nivel, suena la del nivel 1. Si falta la del jefe o la del final, sigue la del nivel. Si
falta la del título o la de fin de partida, no suena nada. Puedes poner tus propias músicas copiando ficheros VGM del chip AY
(Apéndice G) con esos nombres.

### Si no arranca

Si falta un fichero o está mal, el juego vuelve a BASIC y escribe `ERROR DE CARGA n`:

| n | Qué pasa |
|---|---|
| 1 | no está `NIVEL1.I16` |
| 2 | `NIVEL1.I16` no es una imagen `.I16` |
| 3 | no está `NIVEL1.MAP` |
| 4 | `NIVEL1.MAP` no es un nivel de ESTELA |
| 5 | el nivel ocupa más de 6 KB |
| 6 | el nivel está mal formado |
| 7 | no está `HOJA.BIN` |

Si a mitad de partida falla la carga de un nivel posterior al 1 (porque falta o está mal), el juego no se para: vuelve al nivel 1
con una vuelta más de dificultad. Solo vuelve a BASIC con el error si tampoco puede cargar el nivel 1.

### Para programadores

Las fuentes están en `EXAMPLES/ESTELA`:
- `estela.asm`: el juego, ensamblado con pasmo (`compila.bat`).
- `tools/`: las herramientas del PC, en Python, que generan los niveles, la hoja de sprites y el título a partir de PNG y de ficheros de
  texto. `make.py` lo genera todo; `tools/README.md` explica los formatos, y `map2bin.py --check` comprueba que un nivel no pone más
  enemigos por línea de los que el motor de sprites puede dibujar.

Es un buen ejemplo del modo de 256 colores, por varias cosas:
- la paleta con ciclos de color (agua, lava, luces);
- el scroll en anillo, escrito en cada VSYNC;
- los 64 sprites de formato B, con la hoja en el bloque 3 y la paleta de 16;
- el mapa del nivel en una página de la SRAM, mapeada en el bloque 1;
- los ficheros de la SD leídos y escritos con los comandos del MCU (15.5).
