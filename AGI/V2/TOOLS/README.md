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
