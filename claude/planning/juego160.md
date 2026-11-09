# Juego para el modo 160x100 a 256 colores: matamarcianos vertical

Decidido el 2026-11-07 con el usuario, entre cinco candidatos (matamarcianos vertical, plataformas en 8 direcciones,
rally/cueva sobre una imagen grande, captura de área sobre un JPG y artillería en red). **Elegido: matamarcianos vertical.**
Juego original: solo el género es conocido; sin nombres, personajes ni gráficos de ningún juego concreto.

**Nombre provisional:** `ESTELA` (carpeta `EXAMPLES/ESTELA`, sin "_"). Se puede cambiar antes de la fase 1.

**Estado:** fases 0 a 4 hechas y probadas en la placa; la 5 hecha en el arnés (no cambia el juego); la 6 hecha y probada en la placa; la 7a (vidas, fin de partida, nivel en el bloque 1) hecha y probada en la placa; la 7b (jefe) y la 7c (mejoras, bomba con temblor) hechas y probadas en la placa (2026-11-09). Detalle por fase en el §11.

---

## 1. Lo que da el hardware (resumen con números)

Fuentes: `FPGA/SD81V2.1000/mode160.v`, `sprite_engine.v`, `sprites_scroll160_emulador.md`, `mode160_emulador.md`,
`pdm_color.md`, Apéndices D, F, G y H del manual, `EXAMPLES/SPRXTRA` y `EXAMPLES/SCRL160`.

| Tema | Dato |
|---|---|
| Modo | `POKE 2045,175`: 160x100, 1 byte por píxel = índice de la paleta de 256. Cada fila sale en 2 líneas de TV (líneas 58-257) |
| Paleta de 256 | 4 densidades por entrada (R, G, B, BRIGHT; `POKE 2063/2064/2065`). Límite: canal más alto - más bajo <= 0,455 V (los primarios brillantes salen algo lavados). Ciclarla cuesta 3 escrituras por entrada |
| Borde | 4 bits de Chroma (`$7FEF`), no pasa por ninguna paleta |
| Scroll en anillo | `POKE 2072` fila de arriba, `2073` filas del anillo, `2074` píxel de la izquierda, `2075` bytes por fila. Sin latch: escribirlos en el VSYNC |
| Lo que NO hay en 8 bpp | doble buffer, blit, texto de 40x25. Marcador: con sprites o con bandas fijas del anillo (ver 3) |
| Sprites | 8x8 píxeles de imagen, coordenadas = píxel + 32 (X 32-191, Y 32-131). 64 normales + 0-127 extra. No se mueven con el scroll |
| Color de los sprites | paleta de 16 (`pal16`, `POKE 2063/2064/2066`), común a todos e independiente de la de 256. Un transparente por sprite (nibble alto de 2103) y máscara por fila: 15 colores visibles |
| Formato B | celda de 32 bytes (8 filas x 4 bytes, nibble alto = píxel de la izquierda) en `bloque*8192 + página*32`. Bloque de la hoja: `POKE 2129` (2-7). Animar = cambiar la página (`POKE 2130`) |
| Límite por línea | K = 12 y 171 pasos: 2 + 1 por sprite mirado (64 normales siempre + M extra) + 8 por cada B cargado (6 por A). B garantizados = mín(12, (105 - M) / 8): M <= 9 → 12; 17 → 11; 33 → 9; 64 → 5 |
| Prioridad | el normal de índice más alto gana y se carga primero; los extra van por debajo (gana el de número más bajo) |
| Z80 | 3,25 MHz, ~64.600 T por imagen (312 líneas de ~207 T) |
| Costes | mover el scroll: gratis. Una fila nueva de 160 B: ~3.300-4.000 T. Una columna de 100 B: ~3.000 T. Redibujar la pantalla: ~336.000 T (5 imágenes): **nunca durante el juego**. Mover un sprite normal por la ventana de POKEs: ~80 T |
| Borrado vertical | entre el VSYNC y la línea 58 hay ~58 líneas (~12.000 T) sin imagen: ahí van el scroll, los sprites y la paleta. **A medir en la placa** |
| Sonido | 2 AY (ZonX), VGM en segundo plano tocado por el MCU (sin CPU), efectos PEG, voz (`LOAD *SAY`) |
| Mandos | teclado; DB9 mapeado a teclas con `LOAD *JOY` |
| Memoria paginada | SRAM de 256/512 KB en páginas de 8 KB (puerto `E7h`). En los modos del SD81 los bloques 6 y 7 son libres: lo de que el 6 y el 7 espejen el 2 y el 3 es solo para el vídeo nativo del ZX81. El espejo de la FPGA solo copia lo que **escribe** el Z80: paginar no cambia lo que se ve |
| Espacio para código | `$0000-$7FFF` (32 KB) es del juego. El bloque 0 es de solo lectura: se copia el código a otra página (mapeada en otro bloque) y después se mapea esa página en el bloque 0 (corrección del usuario, 2026-11-07) |
| Imágenes | `.I16`: "I160" + 512 B de paleta + 16.000 B de píxeles en bruto (160x100 a 8 bpp). Se carga mucho más deprisa que un JPG (que el MCU tiene que decodificar): es el formato para el juego |

---

## 2. Concepto

Nave que sube por un escenario que pasa por debajo (scroll vertical por hardware), con oleadas de enemigos en formación,
enemigos de tierra que se mueven con el fondo, disparos de los dos lados, mejoras de arma y un jefe al final de cada nivel.

- **Lo que luce del hardware:** el fondo es un mosaico de tiles de 16x16 a 256 colores (degradados, texturas, luces),
  con **agua, lava y luces animadas ciclando la paleta**, sin tocar un píxel. Nave y enemigos de 16x16 en formato B con
  15 colores cada uno, animados cambiando la página. Música VGM en segundo plano y voz para los avisos.
- **Zona de juego:** 128x100 píxeles en el centro (x 16-143), con dos **paneles laterales de 16 píxeles** que son iguales
  en todas las filas: aunque el anillo se mueva en vertical, se ven quietos. Los números y los iconos del marcador son
  sprites encima de los paneles (los sprites no se mueven con el scroll).
- **Velocidad del scroll:** 1 px por imagen (50 px/s, la pantalla en 2 s) o 1 px cada 2 imágenes, según el nivel. Dos
  filas ocultas en el anillo para poder dar acelerones de 2 px por imagen.
- **Controles:** O P Q A + tecla de disparo y de bomba, configurables; DB9 con `LOAD *JOY`.

---

## 3. Mapa de memoria (propuesta, a confirmar en la fase 0)

| Zona | Uso |
|---|---|
| `$0000-$3FFF` (bloques 0 y 1) | **reserva de código** si no cabe abajo: el código se copia a una página libre (mapeada en otro bloque) y esa página se mapea en el bloque 0 (solo lectura) o en el 1. Mientras tanto no hay ROM: nada de rutinas de la ROM (el diálogo con el MCU con código propio), `DI`, y al salir se devuelven las páginas 0 y 1 |
| `$0C00`, `$1800` (sombra) | tablas de los 64 sprites, por la ventana de POKEs (`POKE 2100-2131`). Con otra página en el bloque 0 no cambian: viven en la sombra, y la página de código no se escribe |
| `$4000-~$47FF` | variables del sistema, cargador BASIC (`.B81`) y D-FILE. `CLEAR 18431` |
| `$4800-$5FFF` | código (6 KB) |
| `$6000-$60FF` | entradas de hasta 8 sprites extra (páginas 0-7 de la hoja), si se usan. Si M = 0, código |
| `$6100-$6FFF` | código y datos (3,75 KB): trayectorias, oleadas del nivel en curso, mapa del nivel, variables |
| `$7000-$7FFF` | **hoja de los sprites** (`POKE 2129,3`): celdas de las páginas 128-255 (128 celdas de 8x8 a 4 bpp) |
| `$8000-$BFBF` | **anillo** de 160 x 102 = 16.320 B (`HFILE = $8000`, `POKE 2073,102`, `2075` a 0 = 160). 2 filas ocultas |
| `$C000-$FE7F` | **tileset del nivel**: los 16.000 píxeles de un `.I16` (bloques 6 y 7, libres en los modos del SD81) |
| `$FE80-$FFFF` | libre (384 B) |

- **Tileset = un `.I16`:** una imagen de 160x100 son 10 x 6 = **60 tiles de 16x16** (filas 0-95; las 4 últimas, libres).
  El tile `t` empieza en `$C000 + (t / 10) * 16 * 160 + (t mod 10) * 16`, con 160 bytes de una fila a la siguiente.
  La paleta del `.I16` es la del nivel. Sin paginar nada durante el juego.
- **Pintar una fila:** 8 tiles x 16 bytes con LDI (~2.600 T) y los 32 bytes de los paneles (~700 T): unos **3.300 T por
  fila**, el 5 % de la imagen.
- **Mapa del nivel:** 8 tiles de ancho; un nivel de 3 minutos a 25 px/s son ~280 filas de tiles, ~2,3 KB.
- **Cambio de nivel:** el Z80 lee el `.I16` del nivel de la SD (protocolo del MCU), carga la paleta y deja los píxeles en
  `$C000`. Lo mismo para la pantalla de título (directamente al anillo, en `$8000`).
- El código ejecutable no pasa de `$8000` (comprobación de tamaño en `compila.bat`, como en el ajedrez). Código estimado:
  8-10 KB, que caben en `$4800-$6FFF`. Si se pasa, al bloque 1 o al 0 (fila de arriba).
- Los 512 KB paginados quedan para lo que sobre (más niveles en memoria, música, etc.).

---

## 4. Sprites

### 4.1 Reparto de los 64 normales (índice más alto = más prioridad y se carga antes)

| Índices | Uso | Huecos por línea |
|---|---|---|
| 63-54 | marcador: 6 cifras en una fila arriba (x 56-103, y 2-9), solo tras una muerte y al final del nivel (58-63); vidas y bombas | 6 en las líneas del marcador, mientras sale |
| 53-50 | nave del jugador (16x16) | 2 |
| 49-42 | balas enemigas (8x8) | según el reparto |
| 41-34 | balas del jugador (8x8) | normalmente 1-2 (salen en columna) |
| 33-0 | enemigos (8 de 16x16 = 32 sprites), explosiones y mejoras | 2 por enemigo |

- **Extra:** M = 0 al principio. Si hace falta más (partículas, más balas), hasta M = 9 se mantienen los 12 B por línea.
- **Regla de diseño de las oleadas:** como mucho 3 enemigos de 16x16 en la misma franja de 16 filas
  (6 huecos + 2 del marcador + 2 de la nave + 2 de balas = 12).
- **Si se pasa:** el motor pierde los de menos prioridad (los enemigos de índice bajo) en esas líneas. Opción de la
  fase 5: rotar los índices de los enemigos en cada imagen, para que parpadeen en vez de desaparecer. **No hizo
  falta** (fase 5, §11): el nivel 1 da como mucho 3 enemigos por línea aunque no se derribe ninguno, y el arnés
  falla si un nivel pasa de 3.
- **Jefe:** 32x24 = 12 sprites (4 por línea) usando los índices de los enemigos.

### 4.2 Hoja (128 celdas, páginas 128-255 del bloque 3)

| Gráfico | Celdas |
|---|---|
| Nave: 3 inclinaciones x 4 | 12 |
| Llama del motor (2 fases), si va aparte | 2-4 |
| Enemigos: 6 tipos x 2 fases x 4 | 48 |
| Explosión: 4 fases x 4 | 16 |
| Balas (jugador y enemigas, 2 tipos cada una) | 4 |
| Cifras 0-9 | 10 |
| Iconos (vida, bomba) y mejoras (3) | 5 |
| Jefe (32x24, 2 fases) | 24 |
| **Total** | **~121 de 128** |

- **Paleta de 16 de los sprites:** el 0 es el transparente en todos los sprites (nibble alto de 2103 a 0). Los otros 15
  se eligen para nave, enemigos y fuego (grises metálicos, azul, rojos y verdes, amarillo y naranja, blanco) y se
  calculan con `solve` de `pal160.py`. Es independiente de la de 256 del fondo.

---

## 5. Fondo y scroll

- **Anillo de 102 filas:** `vofs` (`POKE 2072`) baja de uno en uno (la nave sube). La fila que va a entrar por arriba es
  `(vofs - 1) mod 102`, oculta; se pinta antes de mover. Con 2 filas ocultas se pueden dar 2 px en una imagen.
- **Paneles:** los 16 primeros y los 16 últimos bytes de cada fila son siempre iguales (bandas verticales):
  se ven quietos.
- **Paleta de 256 por nivel:** unos 200 colores para los tiles; rangos reservados para ciclar (por ejemplo 224-239
  agua, 240-247 lava, 248-255 luces) y unos 16 para los paneles.
- **Enemigos de tierra:** sprites cuya Y se corrige con la cámara en cada paso del scroll.
- **Pantalla de título y de fin de nivel:** un `.I16` (en bruto: se carga mucho más deprisa que un JPG con el comando 76).

---

## 6. Bucle principal (una imagen, ~64.600 T)

1. Esperar el VSYNC (puerto `$AF`, bit 0, como `waitvs` de SPRXTRA).
2. **En el borrado** (~12.000 T antes de la línea 58): `POKE 2072` (scroll), posiciones y páginas de los sprites que
   han cambiado (~80 T cada uno; los 64 en el peor caso ~5.000 T) y el ciclo de la paleta.
3. Después, con la imagen en marcha: teclado, nave, balas, enemigos y trayectorias, colisiones (cajas, por software),
   aparición de oleadas según la posición del scroll, marcador, efectos de sonido.
4. Pintar la fila oculta que va a entrar (~3.300 T).

Si la lógica no cabe a 50 Hz, el juego va a 25 Hz con el scroll a 1 px por cada dos imágenes.

---

## 7. Sonido

- Música: un VGM por nivel (`LOAD *VGM` desde el código, el MCU lo toca solo).
- Efectos: PEG o escrituras directas al segundo AY. **A comprobar:** que VGM y efectos convivan (un chip para cada uno).
- Voz: **descartada** (decisión del usuario, 2026-11-08). En SONIDO (tecla 4) "WARNING" se oía con la música, pero en el juego "GAME OVER" no se oyó (solo ralentizaba la música), ni pidiéndola después del ruido de la explosión. Los avisos van con carteles de sprites (GAME OVER: letras propias en las celdas libres de la hoja).

---

## 8. Herramientas del PC (Python sin Pillow, con `zlib` como `png2i16.py`)

**Hecho (2026-11-07):** las herramientas están en `EXAMPLES/ESTELA/tools/` (con su README): `spr2cel.py`, `map2bin.py`,
`gentiles.py`, `gensprites.py`, `gennivel.py`, `make.py` (todo de una vez) y `selftest.py`. Fuentes en `EXAMPLES/ESTELA/assets/`,
resultado en `EXAMPLES/ESTELA/out/`. Solo Python estándar; cada herramienta lleva `--check` (ida y vuelta) y `selftest.py` prueba
además que las comprobaciones saben fallar. Los ficheros de la SD van sin "_" (`NIVEL1.I16`, `NIVEL1.MAP`, `NIVEL1.VGM`,
título `TITULO.I16`). **Gráficos:** los actuales son provisionales (`gensprites.py`, `gentiles.py`); se sustituyen dibujando
otros `sprites.png` / `tileset.png` con las mismas reglas.

### 8.1 Tileset: `NIVEL1.I16` (de `tileset.png` con `png2i16.py`)

- PNG **indexado** de 160x100: 10 x 6 tiles de 16x16 (tile `t` = fila `t/10`, columna `t mod 10`); con un PNG indexado
  `png2i16.py` respeta el orden de la paleta, así que los rangos para ciclar los fija el dibujante.
- En memoria (`$C000`), tile `t` en `TILESET_BASE + (t/10)*2560 + (t mod 10)*16`, y la fila `y` del tile `y*160` bytes más
  adelante. `LEVEL.INC` trae la tabla `TILEADR` (60 palabras, indexada por `t`) y `TILE_<NOMBRE>` con el número de cada tile.
- **Paleta de 256 del nivel de ejemplo:** 0-15 paneles; 16-223 terreno (hierba, bosque, tierra, arena, rocas, metal, obsidiana,
  asfalto, acentos, espuma, peligro, espacio); **224-239 agua** (16 pasos), **240-247 lava** (8), **248-255 luces** (8), cada
  rango con una rampa en coseno para que rotar la paleta mueva las olas, la lava y las luces. `LEVEL.INC`: `PAL_WATER`,
  `PAL_WATER_N`, `PAL_LAVA`, `PAL_LIGHTS`... (los rangos salen de `tiles.def`).
- **Paneles laterales:** la fila 96 del PNG (libre: los tiles ocupan 0-95) lleva el dibujo de los dos paneles: bytes 0-15 el
  izquierdo y 144-159 el derecho (los mismos 16 bytes en todas las filas del anillo). En el juego es `$C000 + 96*160 = $FC00`
  (`PANEL_ADDR`): izquierdo en `$FC00-$FC0F`, derecho en `$FC90-$FC9F`. Las filas 97-99 quedan libres.

### 8.2 Hoja de sprites: `HOJA.BIN`, `HOJA.PAL`, `HOJA.INC` (de `sprites.png` + `sprites.def` con `spr2cel.py`)

- `sprites.png`: PNG indexado de hasta 16 colores, rejilla de celdas de 8x8; el índice 0 es el transparente
  (nibble alto de `POKE 2103` a 0). `sprites.def`: una línea por gráfico `nombre x y w h [fases]` (en celdas); **el orden de las
  líneas es el orden de las páginas**, desde la 128. Las fases van una detrás de otra en horizontal en el PNG.
- **`HOJA.BIN`:** una celda de 32 bytes por página (8 filas de 4 bytes, nibble alto = píxel de la izquierda; formato B del
  Apéndice H). Se carga en `SHEET_ADDR` = `$7000` (bloque 3, `POKE 2129,3`): la celda de la página `p` está en
  `$7000 + (p-128)*32`. Los sprites provisionales: 121 celdas, páginas 128-248, 3.872 bytes, `$7000-$7F1F`.
- **Gráficos de más de una celda:** por filas (arriba-izquierda, arriba-derecha, abajo-izquierda, abajo-derecha...) y, con
  fases, una fase entera detrás de otra. Un 16x16 son 4 sprites de las páginas `p`, `p+1`, `p+2`, `p+3` en `(x,y)`, `(x+8,y)`,
  `(x,y+8)`, `(x+8,y+8)`. Coordenadas de hardware = píxel de la imagen + 32 (la zona de juego empieza en x = 16).
- **`HOJA.INC`** (pasmo, `NOMBRE<TAB>equ valor`): `SHEET_BLOCK`, `SHEET_ADDR`, `SHEET_PAGE0`, `SHEET_CELLS`, `SHEET_END` y, por
  gráfico: `NOMBRE` (página de la primera celda), `NOMBRE_W`, `NOMBRE_H` (celdas), `NOMBRE_N` (fases) y `NOMBRE_STEP`
  (celdas por fase, `W*H`); la página de la fase `f` es `NOMBRE + f*NOMBRE_STEP`, y con más de una fase también
  `NOMBRE_0`, `NOMBRE_1`... Además `PAL16` (16 pares `defb lo,hi`), que **ocupa datos**: incluirlo donde deba quedar.
- **Gráficos provisionales (páginas):** `NAVE` 128 (3 fases: 0 izquierda, 1 recta, 2 derecha; 4 celdas cada una), `LLAMA`
  140 (2 fases de 8x8, debajo de la nave), `ENEMY1..ENEMY6` 142, 150, 158, 166, 174, 182 (2 fases de 16x16: el tipo de enemigo
  `n` usa `ENEMY(n+1)`), `EXPL` 190 (4 fases 16x16), `BALAJ` 206 y `BALAE` 208 (2 tipos cada una), `CIFRA` 210 (`CIFRA_0`...
  `CIFRA_9` = 219), `VIDA`, `BOMBA`, `ITEM1` (arma), `ITEM2` (escudo), `ITEM3` (bomba) 220-224, `JEFE` 225 (4x3 celdas, 2 fases
  de 12). Las etiquetas exactas están en `out/HOJA.INC`.
- **`HOJA.PAL` / `PAL16`:** 16 entradas de 2 bytes `lo, hi` (`lo` = `{G,B}`, `hi` = `{BRIGHT,R}`), calculadas con `pal160.solve`
  (con `--gamma` se ajusta). Cargar: `POKE 2063,i` (puntero), `POKE 2064,lo`, `POKE 2066,hi` (escribe y avanza). El índice 0 es el
  transparente (negro en la paleta).

### 8.3 Nivel: `NIVEL1.MAP` + `LEVEL.INC` (de `NIVEL1.LVL` con `map2bin.py`)

`NIVEL1.LVL` es texto: `include tiles.def` / `include paths.def`, `name`, `scroll`, líneas `wave ...` y el bloque `map ... end` con
ocho tiles por línea (nombres o números 0-59). **La línea de arriba del texto es el final del nivel y la de abajo el
principio.** Se escribe en `out/NIVEL1.MAP` (todo en little endian):

| Desplazamiento | Tamaño | Campo |
|---|---|---|
| 0 | 4 | magia `"ELV1"` |
| 4 | 1 | `LVL_SCROLL`: ticks de juego por píxel de scroll (1 = un píxel por tick; 2 = un píxel cada 2 ticks) |
| 5 | 1 | `LVL_NWAVES`: número de oleadas (0-255) |
| 6 | 2 | `LVL_ROWS`: filas de tiles del mapa |
| 8 | 2 | `LVL_WAVES`: desplazamiento de la tabla de oleadas (siempre 16) |
| 10 | 2 | `LVL_MAP`: desplazamiento del mapa = 16 + 8*(NWAVES+1) |
| 12 | 2 | `LVL_LEN`: longitud del fichero |
| 14 | 2 | `LVL_END`: scroll final = 16*FILAS - 100 (el nivel acaba al llegar ahí) |
| 16 | 8*(N+1) | oleadas, ordenadas por `WAVE_POS`; la última lleva `WAVE_POS = $FFFF` (centinela) |
| `LVL_MAP` | 8*FILAS | **mapa**: filas de 8 bytes (un número de tile 0-59 por byte), en el orden en que se recorren |

- **Mapa:** la fila 0 del binario es la primera en entrar (la de abajo del texto). La fila `r` ocupa las líneas de scroll
  `16r .. 16r+15` contando desde abajo (0 = línea de abajo de la fila 0); con `s` píxeles de scroll hechos, la línea de arriba
  de la pantalla es la `99 + s`, y la que entra por arriba en el siguiente paso es la `100 + s` (la línea `j` del tile, 0 = arriba,
  de la fila `r` es la `16r + 15 - j`). Al empezar se ven las filas 0 a 6. El Z80 lee 8 bytes seguidos por fila y los
  pasa por `TILEADR` (unos 65 T por tile).
- **Oleadas (8 bytes):**

| Byte | Campo | Significado |
|---|---|---|
| 0-1 | `WAVE_POS` | scroll (píxeles desde el inicio) en el que sale; se dispara en el primer tick con `scroll >= POS`. En el texto, `rNN` = `16*NN - 100` (la fila NN entera justo por encima de la pantalla, para enemigos de suelo); `rNN+k` suma k |
| 2 | `WAVE_TYPE` | `TYPE_E1`..`E6` = 0-5 (16x16), `TYPE_BOSS` = 6 (32x24), `TYPE_ITEM1`..`3` = 8-10 (8x8); el resto, libres |
| 3 | `WAVE_X` | X de **pantalla** del lado izquierdo del primero (0-255); sprite X = `WAVE_X + 32`; la zona de juego va de 16 a 143 |
| 4 | `WAVE_PATH` | número de trayectoria (`PATH_*`, 0 = `STILL`) |
| 5 | `WAVE_COUNT` | cuántos enemigos (1-255) |
| 6 | `WAVE_SPACING` | separación: **ticks** entre uno y el siguiente; con `WF_ROW`, **píxeles** de X entre ellos |
| 7 | `WAVE_FLAGS` | bit 0 `WF_MIRROR`: `dx` de la trayectoria con el signo cambiado; bit 1 `WF_ROW`: salen a la vez en fila, el enemigo `i` en `X + i*SPACING`; bit 2 `WF_GROUND`: va pegado al suelo (además de la trayectoria baja 1 px cada vez que avanza el scroll) |

  Los enemigos salen en `Y = -alto` (justo por encima de la pantalla); Y de hardware = `Y + 32`. Un enemigo desaparece al salir
  de la zona. La tabla se recorre con un puntero: se compara `scroll` con `WAVE_POS` del siguiente registro.
- **Trayectorias (en `LEVEL.INC`, `PATHTAB`):** `PATH_<NOMBRE>` es el número; `PATHTAB` es una tabla de punteros y cada una es
  `[n][dx dy]*n` con `dx`, `dy` de 8 bits con signo, en píxeles por **tick**. El paso `k` se aplica en el tick `k` de la vida del
  enemigo; pasado el último se repite el último. Se definen en `paths.def` (`line N VX VY`, `sine N AMP CICLOS VY`, con
  decimales: se reparten en pasos enteros) y llevan la trayectoria 0 `STILL` (un paso `0,0`) delante. Nivel de ejemplo: `DOWN`,
  `SLOW`, `SINE`, `ZIGZAG`, `DIVE`, `SWEEP`, `HOVER`, `ENTER` (el jefe; hasta 255 pasos cada una).
- **Regla de diseño (§4.1), comprobada por `map2bin.py --check`:** simula cada enemigo tick a tick y exige que en ninguna línea de
  la zona (0-99) sumen más de 3 (16x16 = 1, jefe = 2, mejora = 0,5), y que ningún enemigo aéreo se quede para siempre (el jefe
  está exento). `NIVEL1.MAP`: 2.488 bytes (280 filas = 4.480 px, 28 oleadas); en el nivel de ejemplo el máximo es exactamente 3.

### 8.4 Resumen de lo que genera `make.py`

| Fichero | Contenido | Uso |
|---|---|---|
| `out/HOJA.BIN` | celdas de la hoja (3.872 B) | cargar en `$7000` |
| `out/HOJA.PAL` | `pal16`, 32 B | alternativa al `PAL16` de `HOJA.INC` |
| `out/HOJA.INC` | páginas de cada gráfico y fase, `PAL16` | `include` en el `.asm` |
| `out/NIVEL1.I16` | `"I160"` + paleta de 256 + 16.000 px | leer de la SD; píxeles a `$C000` |
| `out/NIVEL1.MAP` | cabecera, oleadas, mapa | leer de la SD (2,4 KB) |
| `out/LEVEL.INC` | offsets de cabecera/oleada, `TYPE_*`, `WF_*`, `PATH_*` + datos, `TILEADR`, `TILE_*`, `PAL_*` | `include` en el `.asm` |

Los `.inc` llevan **datos** además de `equ` (`PAL16`, trayectorias, `TILEADR`): incluirlos donde deban quedar esos bytes.
Probados con pasmo 0.5.4 (`selftest.py`).

---

## 9. Fases

| Fase | Qué | Cómo se prueba |
|---|---|---|
| **0** | Comprobaciones en la placa antes de programar: (a) código cargado en `$4800` con `CLEAR 18431` desde un `.B81`; (b) T-estados entre el VSYNC y la línea 58; (c) VGM y efectos AY a la vez; (d) solo si el código no cabe: copiar código a una página, mapearla en el bloque 0 o 1 y que la ventana de POKEs de control siga funcionando. Decidir el nombre y los controles | programas de prueba mínimos y el usuario en la placa |
| **1** | Esqueleto: modo 175, paleta de prueba, anillo 160x102, paneles fijos, scroll vertical continuo de un fondo de tiles generado en el código. `compila.bat` con comprobación de `$8000`, README y `.B81` | arnés del PC como el de SCRL160 (lo visible = el mundo desde el origen escrito en el VSYNC); el usuario en la placa |
| **2** | Herramientas del PC (8); carga del tileset `.I16` del nivel en `$C000` y de su paleta; las filas salen de los tiles del nivel; ciclo de paleta | arnés con un MCU simulado, comparando byte a byte las filas del anillo |
| **3** | Nave del jugador (16x16, 3 inclinaciones), movimiento, disparo y balas | placa |
| **4** | Enemigos: tabla de oleadas por posición del scroll, trayectorias, disparos, colisiones, explosiones, marcador con sprites | arnés (presupuesto de sprites por línea con el modelo de `sprites_scroll160_emulador.md` §1.6) + placa |
| **5** | Reparto por línea: comprobar en el arnés que ninguna oleada pasa de 12; si hace falta, rotación de índices | arnés |
| **6** | Sonido: VGM, efectos y voz | placa |
| **7** | Jefe, mejoras, vidas, fin de partida, título con un `.I16`, récord en un fichero de la SD (`ESTELA.REC`; la EEPROM se descartó) | placa |
| **8** | Pulido: dificultad, más niveles, gráficos definitivos, manual (`manual_pendiente.md`) | placa |

---

## 10. Riesgos

- **Límite de 12 por línea:** es el que manda en el diseño de las oleadas (fase 5).
- **Memoria por debajo de `$8000`:** ~10 KB de código en `$4800-$6FFF`; si se pasa, código en los bloques 0 y 1 (16 KB
  más, sin ROM mientras el juego corre) y tablas en la SRAM paginada.
- **Borrado vertical:** si entre el VSYNC y la línea 58 hay menos T de lo previsto, los sprites se actualizan en
  dos tandas o el juego va a 25 Hz.
- **Gráficos:** el juego vale lo que valgan los tiles y los sprites; hay que decidir quién los hace.
- **Sin lógica nueva en la FPGA** (96 % de slices): todo lo de arriba usa solo lo que ya hay.

---

## 11. Estado de las fases

| Fase | Estado |
|---|---|
| **0** | **Hecha y probada en la placa (2026-11-08).** CARGA: bien, 1.415 bytes libres entre `E_LINE` y `$4800`. VSYNC: F = 64.574 medido (= 312 lineas x 207 = 64.584 T), W = 1.650 (VSYNC de 8 lineas); con D = 48.658 el corte cae hacia la fila 87, asi que **D0 ~ 12.600 T** (unas 61 lineas desde el flanco; estimado de una foto, falta afinarlo). SONIDO: bien (musica VGM + efectos en el canal C de los AY de la FPGA). PAGINA: bien en las cuatro fases (bloque 1, bloque 0 y dentro de la ventana de POKEs), `RESULTADO 0`. Programas en `EXAMPLES/ESTELA/PRUEBAS`, arnes `claude/mqtttest/build_estela0.sh` |
| 1 | **Hecha y probada en la placa** (2026-11-08). `EXAMPLES/ESTELA/estela.asm` (1.416 bytes, `$4800-$4D87`), `ESTELA.B81`, `compila.bat` (comprueba que el binario no pasa de `$6FFF`) y README. Modo 175, paleta de prueba (RGB 3-3-2 + grises 240-255), anillo 160x102 en `$8000`, paneles de 16 px, scroll de 0, 1/2, 1 o 2 px por imagen (tecla S). Tileset generado en `$C000` con el formato del `.I16` (paginas 6 y 7 en los bloques 6 y 7; al salir se devuelven y se reescribe `$4000-$7FFF` sobre `$C000` para rehacer la sombra) y mapa generado de 8 tiles. Fila nueva: ~2.600 T (8 tiles con LDI; los paneles se pintan una vez). Arnes `claude/mqtttest/build_estela.sh`: lo visible = el mundo del modelo C++ en 466 instantes, sin escrituras en filas visibles, POKE 2072 a <= 71 T del flanco. El arnes encontro un fallo (getmap machacaba DE). **Primera prueba en la placa (2026-11-08): basura y cuelgue**, pero con un `estela.bin` viejo, de antes de ese arreglo (el arnes ensamblaba su propia copia). El snapshot `ESTELA001.Z81` lo confirma: cada fila y = 15 se escribia en DE = ((5f+24) mod 256)*256 + f/4, que es lo que deja getmap (tileset, anillo, ventana de POKEs y por fin el propio codigo). `build_estela.sh` ahora ensambla y prueba el `estela.bin` de la carpeta. Con el binario nuevo funciona en la placa (usuario, 2026-11-08) |
| 2 | **Hecha y probada en la placa** (2026-11-08, commit cc616fc). `estela.asm` carga de la SD `NIVEL1.I16` (paleta a `PALBUF`, pixeles a `$C000`) y `NIVEL1.MAP` (a `LVLBUF`, detras del codigo, hasta `$7000`), con errores 1-6 devueltos en BC (`ESTELA.B81` los escribe). Lineas como en el 8.3 (fila k de la pantalla = linea 99+s-k; se pinta la 100+s), `TILEADR` de `LEVEL.INC`, paneles de la fila 96 del tileset, scroll parado en `LVL_END`, velocidad inicial de `LVL_SCROLL`, R vuelve al principio. Ciclo de paleta en el borrado: agua cada 4 imagenes, lava cada 6, luces cada 8 (`TCYW/TCYL/TCYG`). Codigo y tablas `$4800-$5920` (4.385 B con las trayectorias). Arnes `build_estela.sh` con un MCU que sirve `out/`: lo visible = el nivel del modelo en 472 instantes, paleta exacta en cada imagen, nivel corto (parada en `LVL_END`, R), 7 errores de carga; vuelta mas larga 13.854 T |
| 3 | **Hecha y probada en la placa** (2026-11-08). `HOJA.BIN` de la SD a `$7000` (error 7 si falta), `POKE 2129,3`, `PAL16` de `HOJA.INC` con `POKE 2063/2064/2066` y `POKE 2067,1`. Nave 16x16 en los sprites 50-53 (TL, TR, BL, BR; páginas `NAVE + inclinación*4`, inclinación = dx + 1), **llama en el sprite 49** (fuera del reparto del 4.1: las balas enemigas quedan en 48-42), disparos del jugador en 34-41 (`BALAJ_0`). Formato B con las 8 filas de la máscara a `$FF` y transparente 0. Zona x 16-128, y 0-76 (la llama, hasta la fila 99), 1 px por imagen con O P Q A, ESPACIO = disparo cada 6 imágenes (`TFIRE`), 3 px por imagen (`VBALA`), se borran con y < -7. En el borrado: `POKE 2072`, sprites (hasta ~2.700 T), ciclo de paleta (hasta ~8.300 T) y scroll; después `update` (teclas y movimiento, que se ven en la imagen siguiente). Al salir: 64 sprites apagados y en página 0, sin extra, `POKE 2129,2`, `POKE 2067,0`. Código `$4800-$5B6E`; caben niveles de 4.753 B. Arnés: modelo de la nave escrito con la especificación comparado con los registros de los sprites en las 893 imágenes; vuelta más larga 16.741 T |
| 4 | **Hecha y probada en la placa** (2026-11-08, también el marcador nuevo). Oleadas por `WAVE_POS` (en fila con `WF_ROW`; si no, una cada `SPACING` ticks, hasta 4 oleadas soltando a la vez), tipos 6+ (jefe, mejoras) saltados hasta la fase 7. Hasta 8 enemigos (sprites 0-31) en coordenadas de hardware: salen en Y = 16, paso de `PATHTAB` por tick (`WF_MIRROR`, `WF_GROUND` + pasos de scroll de la imagen), fuera con X < 33, X > 175, Y < 16 o Y > 131; 2 fases cada 8 ticks. Disparos enemigos (7, sprites 42-48) con cadencia por tipo (`eftab` 96/0/80/72/36/60), 2 px por tick, dx de -1/0/+1 hacia la nave. Colisiones por cajas (ver la cabecera de `estela.asm`); explosión de 16 ticks (4 fases de `EXPL`); la nave explota, vuelve a su sitio e invulnerable 100 ticks (parpadea); sin vidas todavía. Marcador BCD de 6 cifras (sprites 58-63): primero en el panel izquierdo (2x3); **al usuario le pareció antiestético** y pasó a una fila centrada arriba (x 56-103, y 2) que **solo sale tras cada muerte** (explosión + parpadeo) y al final del nivel. El modo 175 no tiene plano de texto (solo el de 4 bpp) y una franja fija arriba no es posible con el scroll vertical. Ciclo de paleta más rápido (copia seguida) y escalonado: nunca dos rangos en la misma imagen. Código `$4800-$616A`; caben niveles de 3.221 B. Arnés: modelo del juego entero (oleadas de `NIVEL1.MAP`, trayectorias leídas de `LEVEL.INC`) contra los 64 sprites en cada imagen, y una partida del nivel entero (4.389 imágenes: 63 enemigos, 35 derribados, 0 sin hueco, máximo 9 sprites por línea). Sprites escritos antes de 9.800 T, paleta antes de 10.700 T |
| 5 | **Hecha en el arnés** (2026-11-08; `estela.asm` no cambia). `test_z80estela.cpp` cuenta cada imagen con el modelo del motor (§1.6 de `sprites_scroll160_emulador.md`: normales del 63 al 0, 12 huecos, límite 171 pasos) las líneas de sprite que no saldrían, por clase. Nivel entero disparando y dos partidas sin disparar (quieta y moviéndose; 12-13 muertes, marcador en ~1.400-1.500 imágenes): **0 líneas perdidas**, máximo 10 sprites por línea. Caso peor solo con el modelo (enemigos inmortales, nivel entero): **3 enemigos por línea** como mucho (oleada 7, fila de 3): con nave + llama + disparo + 2 disparos enemigos = 12. Solo en las líneas del marcador (y 2-9) y tras una muerte podría perderse algo (las líneas de arriba del enemigo de índice más bajo al entrar, unas imágenes). **No hace falta rotar índices.** Comprobación para niveles nuevos: el arnés falla con más de 3 enemigos en una línea. Pruebas en negativo: 14 sprites en una línea (pierde los 2 de índice más bajo) y la oleada 7 en fila de 5 |
| 6 | **Hecha y probada en la placa** (2026-11-08). Música: `NIVEL1.VGM` (opcional) con `LOOP_VGM 1` + `PLAY_VGM` tras cargar y `STOP_VGM` al salir; `compila.bat` copia `out\NIVEL1.VGM` o, si no hay, `EXAMPLES/VGM/music0002.vgm`. Efectos en los AY de la FPGA (no en el del MCU, que pisaría la música): 4 voces con tablas (valor, volumen) por imagen y prioridad (`sfxgo`/`sfx`, después de `update`): disparo (AY A canal C), disparo enemigo por el canal A o B del AY A según el lado, explosión (ruido, AY B canal C) y la de la nave (prioridad 2, 48 imágenes). **Voz:** pasa a la fase 7, porque sus avisos ("warning" antes del jefe, "game over") son de ella. Arnés: modelo de las voces contra los registros de los dos AY en cada imagen (0 diferencias; detecta el cambio de lado de los disparos enemigos), órdenes de música con y sin `NIVEL1.VGM` y con errores de carga, AY callados al salir. Código `$4800-$6384` (7.045 B); caben niveles de 2.683 B (el 1 tiene 2.488): **la fase 7 necesitará sitio** (código en los bloques 0/1 o el nivel en otro sitio) |
| 7a | **Hecha y probada en la placa** (2026-11-08). La fase 7 va por partes: 7a vidas y fin de partida, 7b jefe (con aviso en cartel), 7c mejoras, 7d título `.I16` y récord en un fichero (`ESTELA.REC`; primero se pensó en la EEPROM). **Memoria:** `NIVEL1.MAP` pasa a la página 20 (la de PAGINA) mapeada en el bloque 1 (`LVLBUF = $2000`, hasta 8 KB; error 5 = más de 8 KB); al salir el bloque 1 vuelve a la ROM y se reescribe sobre sí mismo (sombra de la FPGA). Código `$4800-$65B1` + `PALBUF`: 2.126 B libres hasta `$7000`. **Vidas:** `LIVES0` = 3, una menos por muerte; iconos `VIDA` (sprites 55-57) en x 68-91, y 12, con el marcador. **Fin de partida** (pstate 3) al acabar la explosión sin vidas: sin nave, enemigos sin disparar, marcador fijo, cartel **GAME OVER** (dos filas de 4 letras en x 64-95, y 40 y 50; sprites de la nave y la llama + 54, 32 y 33; las 7 letras G A M E O V R de 7x7 están en el código y se copian detrás de `HOJA.BIN`, páginas `LETRAS` = `SHEET_PAGE0 + SHEET_CELLS` en adelante: la hoja tiene que dejar 7 celdas libres; primero se probó la voz `*GAME OVER` y no se oía). **Música del fin de partida** (petición del usuario): `GAMEOVER.VGM` una vez (`STOP_VGM`, `LOOP_VGM 0`, `PLAY_VGM`); al volver a empezar (ESPACIO o R), otra vez `NIVEL1.VGM` en bucle; `compila.bat` copia `out\GAMEOVER.VGM` o, de momento, `EXAMPLES/VGM/ledstorm-2.vgm` y, pasados `GOT` = 100 ticks, una pulsación nueva de ESPACIO llama a `restart`. Arnés: modelo con vidas y fin de partida contra los 64 sprites en cada imagen, cartel en cada imagen del fin de partida y solo entonces, letras detrás de la hoja, vuelta a empezar con ESPACIO, nivel en la página 20, la "ROM" del bloque 1 sin cambiar al salir |
| 7b | **Hecha y probada en la placa** (2026-11-09). Jefe = oleada de tipo 6 (`wsboss`: `bpend`, desde entonces los enemigos solo usan los huecos 0-4). Con los huecos 5-7 libres: cartel **WARNING** parpadeando `WARNT` = 100 ticks (sprites 24-30), entrada en (`WAVE_X`, -24) con su trayectoria (`ENTER`), pelea de lado a lado 1 px/tick (X de hardware 48-144), dos tiros cada `BFT0` = 30 ticks (`ebfire` con un registro falso `bfake`), caja [X+2,X+29] x [Y+2,Y+21] contra los disparos (`BHP0` = 40, parpadeo 6 ticks, chasquido `fxhit`) y contra la nave; al morir dos explosiones `BEXT` = 48 ticks, +1000 (BCD), cartel **WELL DONE** `CLEART` = 150 ticks y `lap` (restart con el mismo marcador y vidas). Sprites del jefe 20-31 (12, 2 fases de `JEFE`). Letras nuevas W N I L D en las páginas 123-127 (`$6F60`; las páginas de menos de 128 también son celdas). Código `$4800-$6998` + `PALBUF`: 967 B libres hasta `$6F60` (`compila.bat`: binario de 9.568 B como mucho). Arnés: modelo del jefe; partida de 4.000 imágenes con un nivel de solo el jefe y vidas infinitas (4 jefes, 160 impactos, 4 derribados, 3 vueltas); sin líneas perdidas (máx. 11); sprites escritos antes de 11.300 T (el margen hasta los ~12.600 se estrecha). **Tecla I** (petición del usuario, para probar el final): vidas infinitas sí / no hasta salir, borde brillante |
| 7c | **Hecha y probada en la placa** (2026-11-09). Oleadas de tipo 8-10: `wsitem` (una mejora a la vez; si hay una, la nueva se pierde) y `upitem` (trayectoria `SLOW` como los enemigos, fuera con Y >= 132, caja [X+1,X+6] x [Y+1,Y+6] contra la de la nave viva o parpadeando) con el sprite 54 (`ITEM1`-`ITEM3`; en el fin de partida es la V del cartel y no se dibuja). **Arma** = disparo doble (x + 0 y x + 8, dos huecos de los 8; `weap` hasta `pdie`); **escudo** = pstate 2 con `SHLDT` = 250 ticks; **bomba** = +1 (`BOMBMAX` 2) con iconos `BOMBA` en los sprites 32-33 (x 100-115, y 12, con el marcador y no en el fin de partida: el cartel usa el 32 y el 33). **Tecla B** (`pbomb`, pulsacion nueva, justo despues de `pfire`): todos los enemigos vivos explotan (`ekill`, con sus puntos), el jefe entrando o peleando pierde `BOMBHP` = 10 vidas sin bajar de 1 y parpadea, suena `fxdie`. Sonido al recoger: `fxitem` (tono que sube, voz `FX_SHOT`); al recoger, el marcador (con vidas y bombas) sale `SCT` = 60 ticks (`stimer`, a peticion del usuario). Las bombas, el arma y la mejora estan en `gvars` (se pierden al empezar de nuevo). **La bomba se nota** (a peticion del usuario): `hofs` (POKE 2074) con la tabla `shktab` de 24 desplazamientos 3,-3,... que se apagan (modulo 160, `hofv` se escribe en el VSYNC junto al 2072; `shk` cuenta; a 0 al acabar, en el fin de partida, con R/lap y al salir) y `fxbomb` (voz `FX_BOOM`, prioridad 3 > `fxdie`, 64 pares de ruido de periodo 24-31 con volumen 15 que baja). Codigo `$4800-$6C16` (9.239 B, +638 respecto a la 7b) + `PALBUF`: **329 B libres hasta `$6F60`**. Arnes: modelo con mejoras, disparo doble, escudo, bombas e iconos contra los 64 sprites y los AY en cada imagen; partida A (nivel de 150 filas con el jefe al final, hecho en el arnes con las tres mejoras, la nave en x = 72 y luego al borde derecho: 2 armas, 2 escudos, 3 bombas con el tope, una mejora que se va por abajo, 108 salvas dobles, arma perdida al morir, B sin bombas y con ellas) y partida B (jefe + una bomba cada 100 de scroll + B cada 30 imagenes: 15 bombas sobre el jefe, 5 de ellas dejandolo en 1, 7 jefes derribados). En A el arnes compara tambien `POKE 2074` en cada imagen, lo visible con el desplazamiento, `bhp` del jefe (memoria del Z80 contra el modelo, nunca < 1) y la tabla de `fxbomb`. **Seis pruebas en negativo** con el binario cambiado a proposito (escudo de 200, tope de 3 bombas, segundo disparo en x + 9, temblor de 23 imagenes, primer desplazamiento 4, bomba de 9 vidas al jefe): el arnes ve las seis. Sprites escritos antes de **11.617 T** (con jefe, marcador, bombas y mejora a la vez; la 7b: 11.300; hasta ~12.000 T). **Limite nuevo:** el escudo deja el marcador a la vista 250 ticks; con 3 enemigos entrando por arriba justo bajo las cifras (y 2-9) y una salva doble, 14 sprites en la linea: 1 imagen pierde 2 lineas de arriba de un enemigo (solo en la partida A hecha a proposito; el nivel entero real sigue sin perdidas). Pendiente de la placa: ver los iconos, el tono de recogida y si 250 ticks / 10 vidas de la bomba / un solo icono por mejora estan bien |
| 7d | **Hecha en el PC, sin probar en la placa** (2026-11-09). **Título y récord; el récord va en un fichero de la SD (`ESTELA.REC`), no en la EEPROM** (petición del usuario). `TITULO.I16` (opcional, 160x100): se lee a `IMG` con su paleta (a `TPALBUF`, `$3C00`) tras cargar el nivel y la hoja y antes de pintar el anillo, con el récord en las 6 cifras de los sprites 58-63 (x 56-103, **y 84** = `RECY`: la imagen tiene que dejar libre esa franja), ESPACIO empieza y X sale a BASIC (`title`; sin fichero o sin firma `I160` se salta). Después `panels`, `fillring` y `setpal` como al empezar (`blackpal` al salir del título). **Récord:** `ESTELA.REC` (carpeta actual), 7 bytes: `EREC` + 3 BCD (cifra alta primero). `recload` al empezar (órdenes 53, 55, 57; sin fichero, más corto o firma mala = 0); `recsave` en cuanto empieza el fin de partida (`pp2`), solo si el marcador supera estrictamente el récord: `f_create` (orden 71, nombre ASCII, longitud `NMRECL` = 10), `f_write` (56: manejador, cuántos en 2 bytes, bytes, estado) y `f_close` (57); rutinas nuevas en `estela.asm` (`mcu.asm` de PHOTO no se toca). Sin cambios en el firmware. **Memoria:** `PALBUF` (512 B) pasa del final del binario a los últimos 512 bytes de la página del nivel (`$3E00-$3FFF`, bloque 1) y `TPALBUF` va justo antes (`$3C00-$3DFF`); `MAXLVL` baja de 8 KB a 7 KB (error 5 = más de 7 KB; el nivel 1 tiene 2.488 B). Código `$4800-$6D9D` (9.630 B, el binario ya no lleva `PALBUF` detrás: `compila.bat` deja 10.080 como máximo): **450 B libres hasta `$6F60`**. Sprites escritos antes de **11.618 T** (como en la 7c) y paleta antes de 11.977 T (las cifras del marcador no cambian: el título tiene su propio bucle, porque compartirlas añadía 63 T y el ciclo de la paleta pasaba de 12.000 T). Arnés: MCU con la 71 y la 56; el modelo lee `ESTELA.REC` al empezar y sube el récord en cada fin de partida que lo supera; en `exit_checks` (todas las ejecuciones): tantas órdenes 71 y 56 como subidas del modelo y el contenido exacto, o el fichero intacto. Pruebas: título con y sin `TITULO.I16` (paleta, 16.000 píxeles y cifras 001234 de los sprites 58-63 en la primera espera de teclas; ESPACIO empieza y el juego sigue igual que el modelo; X sale sin jugar; ESPACIO ya pulsado; fichero que no es un `.I16`), y el nivel entero con cuatro `ESTELA.REC` (ninguno, 999999 = ni una escritura, firma mala, 000100). **Dos pruebas en negativo** (firma `FREC` en el binario; guardar siempre): el arnés ve las dos |
| 8 | Sin empezar |

### 11.1 Fase 0: qué mide cada prueba y qué decide

- **CARGA (a):** los 14 KB de `$4800-$7FFF` cargan desde un `.B81` y se ejecutan hasta `$7FFF`; da el margen entre
  `E_LINE` y `$4800`. Decide si el código va en `$4800`.
- **VSYNC (b):** corte del color 0 de la paleta D T-estados después del flanco (`D = 116 + 26n`, exacto en el arnés);
  D0 = el primer D con la primera línea de la imagen negra. Mide también F (T por imagen) y W (anchura del VSYNC).
  Decide cuánto cabe en el borrado (§6, paso 2): scroll, sprites y paleta.
- **SONIDO (c):** VGM en el AY del MCU + efectos en los AY de la FPGA (`$CF/$0F` y `$C7/$07`, escritos por el Z80 en cada
  imagen) + PEG + voz en segundo plano (`*WARNING`). **Visto en el firmware:** PEG y la voz usan el mismo AY emulado
  que el VGM (`ay_registers[]` de `ay_emulator`), así que un efecto PEG pisa la música; por eso el camino previsto para
  los efectos son los AY de la FPGA. La prueba lo confirma de oído.
- **PAGINA (d):** el código copiado a una página y ejecutado desde el bloque 1 (`$2000`), el bloque 0 (`$1000`) y dentro de
  la ventana de POKEs (`$07F0`), girando la paleta y moviendo el sprite 0 por la ventana. Se ha hecho porque el código
  (8-10 KB estimados) va justo en `$4800-$6FFF`. Lecciones para el juego: el código que se copia no puede llamar a sí
  mismo con `CALL` (es absoluto: el arnés lo pilló); las páginas se rellenan mapeándolas en el bloque 7 (`$E000`), que no
  se ve, y **antes** de cargar el anillo y el tileset (escribir en un bloque escribe la sombra de esas direcciones).

## 12. Plan de lo que falta (modo ahorro: planifica Opus, implementan sesiones Sonnet)

Reglas para cada sesión: una fase cada vez, en esta carpeta (sin worktree); `estela.asm` + modelo en
`claude/mqtttest/test_z80estela.cpp` (todo lo nuevo, comprobado imagen a imagen contra los sprites/AY/órdenes del MCU, con una
prueba en negativo); `build_estela.sh` tiene que dar TODO OK; README y §11 al día; sin commit hasta que el usuario lo pida tras
probarlo en la placa; no tocar `tools/`, `assets/`, `out/` ni el §8 (son de estela-herramientas); sin "_" en nombres de la SD;
nada de lógica FPGA. Memoria: el código acaba antes de `$6F60` (tras la 7d quedan 450 B; `PALBUF` y `TPALBUF` ya viven en el final de la página del
nivel, bloque 1: `$3C00-$3FFF`, y el nivel puede ocupar hasta 7 KB de ella); para más sitio, mover tablas/código a esa página
(código ahí solo con saltos relativos o ensamblado en `$2000+`; hasta que se carga el nivel esa zona está vacía).
Sprites escritos antes de ~12.000 T tras el flanco (hoy 11.618).

**7c Mejoras** (oleadas de tipo 8-10 de `NIVEL1.MAP`: `s` = 700, 2310, 3980, trayectoria `SLOW`). Una mejora a la vez, 8x8,
sprite 54 (en el fin de partida lo usa el cartel: ahí sin mejora), sale en (`WAVE_X`, -8) con su trayectoria, no se puede
derribar, se recoge si toca la caja de la nave (viva o parpadeando), se borra al salir por abajo. Páginas `ITEM1` 222 (arma),
`ITEM2` 223 (escudo), `ITEM3` 224 (bomba). Efectos: **arma** = disparo doble (dos disparos en x+0 y x+8, mismos 8 huecos) hasta
perder la nave; **escudo** = invulnerable 250 ticks (como pstate 2: parpadea); **bomba** = +1 bomba (máx. 2), iconos `BOMBA` 221
en los sprites 32-33 junto a las vidas (fila y 12, x 100-115, solo cuando sale el marcador; en el fin de partida los usa el
cartel). Tecla **B** (fila B N M . ESPACIO, bit 4, pulsación nueva): gasta una bomba = todos los enemigos vivos explotan (con
sus puntos) y el jefe pierde 10 vidas (sin bajar de 1); efecto `fxdie`. Sonido al recoger: tabla nueva corta (tono que sube) en la
voz `FX_SHOT`. Arnés: partida con las tres mejoras recogidas (la nave colocada con teclas en su X), comprobar disparo doble,
escudo, bombas e iconos; la regla de 3 por línea cuenta la mejora como 0,5 (ya lo hace `map2bin --check`).

**7d Título y récord.** Antes de jugar: `TITULO.I16` (160x100, opcional; si no está, se salta) en el anillo con `vofs` 0 y su
paleta, el récord con las cifras (sprites 58-63, como el marcador) y "ESPACIO" para empezar (X sale; el texto, en la imagen).
**Récord (decidido por el usuario: en un fichero, no en la EEPROM):** `ESTELA.REC` en la carpeta actual (la del juego), 7 bytes:
la firma `EREC` y 3 bytes BCD. Al empezar se lee con las órdenes 53, 55 y 57; sin fichero o con otra firma, 0. Al acabar la
partida (fin de partida), si el marcador **lo supera**, se escribe con la 71 `fcreate` (nombre ASCII; crea o vacía el fichero y
lo abre, responde con el manejador como `fopen`), la 56 `fwrite` (manejador, cuántos en 2 bytes, los bytes -> estado) y la 57. Sin
cambios en el firmware. Después del título se carga el nivel como ahora (la imagen del título va al anillo, que luego se repinta).
Arnés: MCU con la 71 y la 56 simuladas (el fichero se escribe en `sdfiles`), récord leído al empezar, guardado solo si se supera
(contenido exacto del fichero), sin fichero o con otra firma, con y sin `TITULO.I16`.

**8 Pulido** (mayormente del usuario y de estela-herramientas): dificultad (`eftab`, `BHP0`, `BFT0`), más niveles (`NIVEL2.*`:
el código tendría que pasar de nivel tras el jefe en vez de `lap`), gráficos definitivos, músicas propias (`out/NIVEL1.VGM`,
`out/GAMEOVER.VGM`), y el manual (`manual_pendiente.md`).

### 12.1 Fase 8: alcance decidido por el usuario (2026-11-09)

**5 niveles**, gráficos generados por estela-herramientas, y la sección del manual al final. Tres trabajos:

- **8a Código: varios niveles** (sesión Sonnet; `estela.asm` + arnés). Ficheros `NIVEL1..NIVEL5` (`.I16`, `.MAP`, y `.VGM`
  opcional: si falta, la del 1); la hoja `HOJA.BIN` es común. Tras el jefe (WELL DONE) se carga el nivel siguiente (mismo
  `loadlvl` con la cifra del nombre cambiada) con el marcador y las vidas; tras el 5, o si falta el fichero del siguiente, vuelta al
  1 con una **vuelta más** de dificultad. R vuelve al principio del nivel actual; el fin de partida y ESPACIO, al nivel 1 con la
  dificultad inicial. Dificultad por nivel y vuelta en una tabla del código: vidas del jefe (`BHP0` 40 + 10 por nivel), salvas del jefe
  (`BFT0` 30 - 3 por nivel, mínimo 15) y cadencia de los enemigos (`eftab` x 7/8 por nivel y vuelta, mínimo la mitad). Cartel del
  número de nivel al empezar cada uno: la cifra del nivel en el sprite 54 centrada (y 46) unas 100 imágenes, sin letras nuevas. Los
  errores de carga a mitad de partida (un nivel que no cabe o está mal): volver al 1 en lugar de salir. Memoria: lo que no quepa
  en `$4800-$6F5F`, a la página del bloque 1 (detrás del nivel, como hizo la 7d). Arnés: niveles 2-5 de prueba (copias del 1 con
  otro `LVL_SCROLL`/paleta, o los de herramientas si ya están), paso de nivel, vuelta al 1, dificultad, fallo del nivel siguiente.
- **8b Niveles 2-5 y gráficos definitivos** (estela-herramientas): un tileset y paleta por nivel (temas distintos: p. ej. hielo,
  desierto, ciudad, nave alienígena), oleadas cada vez más difíciles con su jefe al final (tipo 6, trayectoria `ENTER`), mejoras
  repartidas; cada nivel <= 7 KB, la regla de 3 por línea (`map2bin --check`); la hoja sigue dejando 7 celdas libres al final y el
  mismo orden de páginas (las etiquetas de `HOJA.INC` pueden cambiar de valor, no de nombre). Mejorar los gráficos provisionales.
- **8c Manual** (al final, sesión Sonnet con la skill docx, flujo docx-first): sección de ESTELA (carga, teclas, mejoras, jefe,
  récord, ficheros de la SD), con lo apuntado en `manual_pendiente.md`.
