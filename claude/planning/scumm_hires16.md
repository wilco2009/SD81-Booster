# SCUMM en HiRes 8x1 de 16 colores (decidido 2026-10-26)

Motivo: con tiles de 8x8 (208 por ventana) Zak pierde mucha calidad. Medido con `SCUMM/TOOLS/medir8x1.py` sobre las 58 salas: el HiRes 8x1 (2 colores por
franja de 8x1, sin tiles) mejora mucho las salas que hoy se pasan del límite (12: 14,9 -> 4,6 %; 13: 21 -> 8; 39: 21 -> 7; 42: 12 -> 4) y casi nada
las demás. El suelo de 3-5 % es la regla de 2 colores por 8x1, que es la misma. Ventaja extra: los objetos ya no compiten por tiles con el fondo
(adiós a pool/protect/keep/objprio).

## Reparto de memoria (implementado en la rama `hires16` de SCUMM)

El codigo NO puede pasar de `$8000` (la ULA del ZX81 mete NOPs): el bloque 4 es solo de datos y variables.

| Bloque | Rango | Contenido |
|---|---|---|
| 0 | `$0000-$1FFF` | overlay (solo lectura): interfaz, camara, personajes, sonido, guardar |
| 1 | `$2000-$3FFF` | ventana de datos: paginas de DATA.BIN (6 KB) y, un momento, las paginas de la sala (8 KB) |
| 2-3 | `$4200-$7FFF` | nucleo y `scumm.asm`, `hr.asm` (codigo hasta `$7E4D`) |
| 4 | `$8000-$864C` | `tables.inc`, `progdata.inc` |
| 4 | `$8700-$9AFF` | variables: lo que estaba en `$CC00-$DFFF` desplazado -`$4500` (SV `$8700`, UI `$8800`, pila `$8A00-$8AEF`, `ODIRBUF` `$8B00`, `$9500-$9AFF` resto) |
| 4 | `$9B00-$9E18` | `DFILE`: rejilla de caracteres de la interfaz (33x24+1), en RAM, aun sin pintar |
| 4 | `$9E20-$9F9F` | formas de los glifos 208-255 (`TILES`) |
| 5 | `$A000-$B7FF` | bitmap (linea y * 32 + columna); al arrancar, `OVERLAY_IMAGE` (`org $A000`) hasta que `ov_install` la copia a su pagina |
| 5 | `$B800-$B8BF` / `$BC00-$BCBF` | entrada derecha / izquierda del scroll (un byte por linea) |
| 5 | `$BA07-$BBFF`, `$BE00-$BEFF`, `$BF00-$BFFF` | libres del hardware: registros de objetos (6 B x 84), lista de objetos dibujados, columna en composicion |
| 6 | `$C000-$D7FF` | atributos (papel alto, tinta bajo), `$D800` / `$DC00` entradas de atributos |
| 7 | `$E000-$FFFF` | estado del nucleo, igual |

Paginas fisicas: la sala usa la 8-13 (48 KB; los .GFX miden hasta 44 KB); la cache de DATA.BIN, la 16-62 (la 50 es el overlay).

## Estado (2026-10-26)

- Hecho y verificado en el arnes (`ENGINE/test/test_scumm`, `check_hr.py`): reubicacion de variables, `hr.asm` (carga de sala en paginas, columnas con
  objetos, dibujado completo, scroll fino + motor con relleno de los bufers, salto de camara), modelo del motor en el arnes, `render_screen.py` en HiRes.
- Sin hacer (fase 3): capa de texto (interfaz, mensajes, verbos, inventario, errores) sobre la rejilla `DFILE`, oscuridad (`dark.asm`), el cursor de tinta,
  actualizar solo el rectangulo de un objeto al cambiar de estado (hoy se redibuja la ventana entera).

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
- Cargar de la SD una sala de ~30-44 KB: medir el tiempo de cambio de sala.
- La cabecera del .GFX ocupa 256 bytes para que la columna k este en `256 * (k + 1)` y 32 columnas llenen una pagina.
