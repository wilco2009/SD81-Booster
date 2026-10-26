# Herramientas AGI v2

`agi_read.py` lee los datos de un juego AGI v2 (carpeta con `LOGDIR`, `PICDIR`, `VIEWDIR`, `SNDDIR`, `VOL.n`, `WORDS.TOK`, `OBJECT`), solo con la biblioteca estandar de Python:

```
python -I TOOLS/agi_read.py KQ1 list                  # recursos por tipo (volumen, desplazamiento, tamano)
python -I TOOLS/agi_read.py KQ1 pics dump/KQ1/pics    # imagenes a PNG: visual | prioridad (pixel doble en horizontal)
python -I TOOLS/agi_read.py KQ1 views dump/KQ1/views  # vistas a PNG, una fila por bucle
python -I TOOLS/agi_read.py KQ1 logic 1               # mensajes de la logica 1 (descifrados con "Avis Durgan")
python -I TOOLS/agi_read.py KQ1 words                 # vocabulario
python -I TOOLS/agi_read.py KQ1 objects               # objetos del inventario
```

Estado (fase 1): probado con KQ1, KQ2, Larry1, PQ1, SQ1 y SQ2 (los 6 abren; imagenes, vistas y mensajes salen bien a simple vista).
Pendiente: el pincel (`F9`/`FA`) es aproximado, no reproduce el salpicado exacto; no hay aun volcado del bytecode de las logicas ni del sonido.
`dump/` y las carpetas de juego estan ignorados por git (datos de Sierra).

`medir8x1.py JUEGO [SALIDA]` mide el choque de color al pasar las imagenes al HiRes 8x1 (2 colores por 8 pixeles) con dos geometrias (160 de ancho o estirado a 256) y deja el PNG de la peor y la mediana. Resultados en `claude/planning/agi_plan.md`.

`agi2hr.py JUEGO SALIDA [--png] [n ...]` convierte las imagenes a `PICnnn.GFX` (formato `AGIP`, descrito en la cabecera del script): visual de 256x168 en HiRes 8x1 (bitmap + atributos, listos para `$A000` y `$C000`) y prioridad a 160x168 con RLE. Cada fichero se relee y se comprueba contra la conversion. Tamano medio: unos 13,5 KB por imagen (algo mas de 1 MB por juego); pixeles cambiados 1,5-2,1 %.

## Visor en el Z80 (`../ENGINE/`)

`agiview.asm` carga `PICnnn.GFX` de la carpeta actual y lo pone en pantalla (HiRes 8x1, bitmap en `$A000`, atributos en `$C000`); N = siguiente, B = anterior, ESPACIO = salir. Comparte el MCU (`mcu.asm`, copia de la de SCUMM) y se prueba en el arnes de PC de SCUMM con `sh test.sh KQ1` (comprueba que la pantalla final coincide con el fichero, incluido el salto de los huecos). `compila.bat KQ1` ensambla y copia el visor y las imagenes a la SD del emulador (`AGI\V2\`); se carga con `AGIVIEW.B81` desde la carpeta del juego.
