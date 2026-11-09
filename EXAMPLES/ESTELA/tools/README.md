# ESTELA: herramientas del PC

Datos del matamarcianos vertical ESTELA (modo 160x100 a 256 colores) a partir de ficheros editables.
Solo Python 3 con la biblioteca estándar (los PNG con `zlib`, sin Pillow, como `EXAMPLES/PHOTO/png2i16.py`, que se reutiliza).
Los formatos finales están en `claude/planning/juego160.md`, sección 8.

```
assets/                       lo que se edita (fuentes)
    sprites.png, sprites.def      hoja de sprites (PNG indexado de 16 colores, rejilla de 8x8) y lista de gráficos
    tileset.png, tiles.def        tileset del nivel (PNG indexado 160x100, 10x6 tiles de 16x16) y nombres de los tiles
    titulo.png                    pantalla de título (PNG indexado 160x100, paleta de 256)
    tileset2..5.png, tiles2..5.def    tilesets de los niveles 2-5 (hielo, desierto, ciudad, nave alienígena)
    NIVEL2..5.LVL                 los niveles 2-5
    paths.def                     trayectorias de los enemigos
    NIVEL1.LVL                    el nivel: mapa legible + oleadas
out/                          lo que se genera (se copia a la SD o se incluye en el ensamblado)
    HOJA.BIN  HOJA.PAL  HOJA.INC      celdas del formato B, pal16 y etiquetas (pasmo)
    NIVEL1.I16                        tileset + paleta de 256 (formato .I16)
    TITULO.I16                        pantalla de título (formato .I16; ESTELA la enseña si existe)
    NIVEL2..5.I16  NIVEL2..5.MAP      tileset+paleta y mapa+oleadas de los niveles 2-5
    NIVEL1.MAP  LEVEL.INC             mapa y oleadas; formatos, trayectorias y tabla de tiles (pasmo)
tools/
```

## Uso rápido

```bash
python make.py            # genera todo lo de out/ desde assets/ y lo comprueba (o tools\make.bat)
python make.py --regen    # además regenera los gráficos PROVISIONALES (sobreescribe sprites.png, tileset*.png y titulo.png)
python make.py --regen-niveles   # regenera NIVEL2..5.LVL con genniveles.py (sobreescribe lo editado a mano)
python selftest.py        # pruebas de las herramientas en una carpeta temporal (también prueba con pasmo si está en el PATH)
```

`make.py` no regenera `NIVEL1.LVL` (se edita a mano). Para sustituir los gráficos provisionales basta con dibujar otros
`sprites.png` / `tileset.png` con las mismas reglas y volver a ejecutar `python make.py` (sin `--regen`).

## Las herramientas

### `spr2cel.py`: hoja de sprites → celdas del formato B

```bash
python spr2cel.py ../assets/sprites.png [-d ../assets/sprites.def] [-o ../out] [--name HOJA] [--block 3] [--first-page 128] [--gamma 1.0] [--check]
```

PNG indexado de hasta 16 colores (el índice 0 es el transparente) en una rejilla de celdas de 8x8, más `sprites.def`:

```
# nombre  x y  w h  fases       (x y, w h: en celdas)
NAVE      0 0  2 2  3
CIFRA     0 6  1 1  10
```

Cada línea es un gráfico: su esquina en el PNG, su tamaño en celdas y sus fases de animación (una detrás de otra en
horizontal). **El orden de las líneas es el orden de las páginas** (desde la 128). Salidas:

- `HOJA.BIN`: celdas de 32 bytes (8 filas de 4 bytes, nibble alto = píxel de la izquierda) para `$7000` (`POKE 2129,3`).
- `HOJA.PAL`: la `pal16`, 16 entradas de 2 bytes (bajo `{G,B}`, alto `{BRIGHT,R}`) calculadas con `pal160.solve`.
- `HOJA.INC`: etiquetas para pasmo (`NAVE`, `NAVE_W/_H/_N/_STEP`, `NAVE_0..`), `SHEET_ADDR`, `SHEET_CELLS` y `PAL16` como `defb`.

`--check`: vuelve a leer lo escrito y lo compara con el PNG (cada celda, la pal16 y cada etiqueta).

### `gensprites.py`: sprites provisionales

```bash
python gensprites.py [-o ../assets] [--preview vista.png] [--check]
```

Escribe `sprites.png` y `sprites.def` con los gráficos de la tabla 4.2 del plan (121 celdas de 128): `NAVE` (3 inclinaciones),
`LLAMA`, `ENEMY1..6` (2 fases), `EXPL` (4), `BALAJ`, `BALAE`, `CIFRA` (0-9), `VIDA`, `BOMBA`, `ITEM1..3` y `JEFE` (32x24, 2 fases).
El arte son cadenas ASCII en el propio fichero (cada carácter es un dígito hexadecimal de la paleta de 16). `--preview` saca una
vista ampliada x4.

### `gentiles.py`: tileset provisional

```bash
python gentiles.py [-o ../assets] [--preview vista.png] [--check]
```

`tileset.png` (160x100 indexado, paleta de 256) y `tiles.def` (nombre de cada tile, rangos de paleta y fila de los paneles).
Rangos reservados: 224-239 agua, 240-247 lava, 248-255 luces (cada uno con una rampa en coseno, para ciclarlos); 0-15 colores de
los paneles. Los tiles se repiten en vertical sin costuras. La fila 96 del PNG lleva el dibujo de los paneles laterales.
Después se convierte con `png2i16.py` (lo hace `make.py`): `NIVEL1.I16`. `--check` regenera los 60 tiles y los compara con el PNG.

### `gentitulo.py`: pantalla de título provisional

```bash
python gentitulo.py [-o ../assets] [--preview vista.png] [--check]
```

`titulo.png` (160x100 indexado, paleta de 256): «ESTELA» grande arriba (letras 5x7 ampliadas 4x3 con degradado, contorno y sombra),
espacio con nebulosa y estrellas, la nave y unos enemigos copiados de `sprites.png` (por eso se regenera después de cambiar la
hoja de sprites; `make.py --regen` lo hace en orden), «RECORD» en y 75-81 y «ESPACIO: JUGAR   X: SALIR» en y 94-98.
**La franja de las cifras del récord (x 56-103, y 84-91) es de índice 0 y está lisa** (con un marco fino fuera): `--check` falla
si se ensucia. `make.py` la convierte en `out/TITULO.I16` (formato .I16 normal) y comprueba la ida y vuelta.

### `map2bin.py`: nivel → `NIVEL1.MAP` + `LEVEL.INC`

```bash
python map2bin.py ../assets/NIVEL1.LVL [-o ../out] [--max-weight 3] [--check]
```

Lee `NIVEL1.LVL` (que incluye `tiles.def` y `paths.def`; sintaxis en la cabecera del script y en `NIVEL1.LVL`):

```
wave pos=r22 type=E6 x=88 path=STILL count=1 flags=ground
wave pos=60  type=E1 x=48 path=SINE  count=4 spacing=16
map
  GRASS1 GRASS2 ...   ; ocho tiles por fila; la línea de arriba es el FINAL del nivel
end
```

`--check` (1) decodifica `NIVEL1.MAP` y `LEVEL.INC` y los compara con el texto y (2) simula tick a tick todas las oleadas con
las trayectorias decodificadas y **falla si en alguna línea de la zona coinciden más de 3 enemigos de 16x16** (regla de §4.1 del
plan: jefe = 2, mejora = 0,5) o si algún enemigo aéreo no sale nunca de la pantalla.

### `gentemas.py` + `genniveles.py`: niveles 2-5

```bash
python gentemas.py [-o ../assets] [--preview CARPETA] [--check]      # tileset2..5.png + tiles2..5.def
python genniveles.py [-o ../assets] [--only 2 3]                      # NIVEL2..5.LVL
python vernivel.py ../assets/NIVEL2.LVL tileset2.png mapa2.png --filas 56   # dibuja el mapa para revisarlo
```

Cuatro temas (2 hielo, 3 desierto, 4 ciudad, 5 nave alienígena). Cada uno tiene su paleta de 256 y sus 60 tiles con nombres
propios (`tiles2.def`...), pero **los rangos de la paleta que cicla el código (agua 224-239, lava 240-247, luces 248-255) y la
fila 96 de los paneles están donde en el nivel 1**; solo cambian sus colores. `make.py` comprueba que el `LEVEL.INC` de cada nivel
es igual al del 1 salvo los nombres de los tiles, que cada `.MAP` ocupa como mucho 7 KB y la regla de 3 enemigos por línea.
Los niveles solo pueden usar las 9 trayectorias de `paths.def` (el código las lleva compiladas). `genniveles.py` reparte las oleadas
con una semilla fija y las retrasa hasta que cumplen la regla; luego se editan a mano.

### `gennivel.py`: nivel de ejemplo

Escribió una vez `assets/NIVEL1.LVL` (280 filas, 28 oleadas: pradera, río con puente, desierto, cañón de lava, base y jefe).
Es determinista; el texto resultante es lo que se edita.

## Lo que usa el Z80 (resumen; detalle en `juego160.md` §8)

```asm
        include "HOJA.INC"      ; páginas de los gráficos, PAL16 (defb)
        include "LEVEL.INC"     ; cabecera del nivel, oleadas, PATHTAB, TILEADR, rangos de paleta
```

Ojo: los dos `.inc` llevan **datos** (`PAL16`, trayectorias y `TILEADR`), así que se incluyen donde deban quedar esos bytes
(los equ no ocupan nada). `HOJA.BIN` se carga en `SHEET_ADDR` (`$7000`) y `NIVELn.I16`/`NIVELn.MAP` se leen de la SD.
Todos los nombres de fichero de la SD están sin "_" (el ZX81 no lo tiene).
