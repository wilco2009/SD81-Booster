# SCUMM en HiRes 8x1 de 16 colores (decidido 2026-10-26)

Motivo: con tiles de 8x8 (208 por ventana) Zak pierde mucha calidad. Medido con `SCUMM/TOOLS/medir8x1.py` sobre las 58 salas: el HiRes 8x1 (2 colores por
franja de 8x1, sin tiles) mejora mucho las salas que hoy se pasan del límite (12: 14,9 -> 4,6 %; 13: 21 -> 8; 39: 21 -> 7; 42: 12 -> 4) y casi nada
las demás. El suelo de 3-5 % es la regla de 2 colores por 8x1, que es la misma. Ventaja extra: los objetos ya no compiten por tiles con el fondo
(adiós a pool/protect/keep/objprio).

## Reparto de memoria propuesto

| Bloque | Rango | Contenido |
|---|---|---|
| 0 | `$0000-$1FFF` | overlay (solo lectura), igual |
| 1 | `$2000-$3FFF` | ventana de datos de **8 KB** (ya no hay tabla de caracteres) |
| 2-3 | `$4000-$7FFF` | núcleo y `scumm.asm` (~17 KB de código) |
| 4 | `$8000-$9FFF` | cola del código + `ODIRBUF`, variables, pila y `BACKBUF` reubicados (~5-6 KB) |
| 5 | `$A000-$BFFF` | bitmap HiRes (6 KB, `vpage` = HFILE[15:13], elegible por POKE) + búferes de entrada del scroll (`$1800-$1FFF`) |
| 6 | `$C000-$DFFF` | atributos de modo 0 (6 KB, **fijos** en `$C000`) + búferes de entrada de atributos |
| 7 | `$E000-$FFFF` | estado del núcleo, igual |

Desaparecen `COLORS`, `DFILE`, la tabla de caracteres y `GFXBUF`. La sala actual vive en páginas de datos (hasta 960 px = 120 columnas x 256 B = 30 KB).

## Modelo de datos de la sala

- Zona de sala: 128 líneas (16 filas), la interfaz ocupa las 8 filas de abajo (máscara de filas del scroll: la interfaz no se mueve).
- Una columna de 8 px = 128 B de bitmap + 128 B de atributos = 256 B; el `.GFX` nuevo se guarda **por columnas**, así que entrar una columna por el
  scroll es copiar 128 B + 128 B a los búferes de entrada.
- Objetos: imágenes por celdas (bitmap + atributo), sin tiles; se redibujan recomponiendo desde los datos de la sala (el Z80 no puede leer la pantalla
  tras el primer movimiento del motor).
- Texto de interfaz: glifos de 8x8 dibujados en bitmap + atributos (sin tabla de caracteres).
- Actores: sprites hardware, igual.

## Fases

1. **Conversor**: formato `.GFX` por columnas (fondo + objetos por celdas + máscara de actores), verificador y PNG. Primero esto, es lo que se ve.
2. **Vídeo y scroll**: arranque HiRes (POKE 2045,171, atributos modo 0, `vpage`), integrar el motor de movimiento horizontal (`EXAMPLES/SCROLLM`).
3. **Motor**: `cam.asm` (columnas), carga de sala, objetos (`obj_drawn` se mantiene), texto/interfaz (`ui.asm`), oscuridad (`dark.asm`).
4. **Arnés de pruebas**: `render_screen.py` y el modelo de vídeo en HiRes; la equivalencia (VM) no cambia.

## Riesgos / pendientes

- El motor de scroll y SCROLLM están **sin probar en la placa** (ver `project_scroll_board_checks`): conviene probarlos antes de fiarse de la fase 2.
- Los atributos de HiRes no pasan por el doble buffer: componer solo con el motor libre (bit de ocupado).
- Tras un movimiento, la RAM del Z80 deja de coincidir con la pantalla (solo escritura).
- Scroll vertical no existe (Zak/Maniac no lo necesitan).
- Cargar de la SD una sala de ~30 KB: medir el tiempo de cambio de sala.
