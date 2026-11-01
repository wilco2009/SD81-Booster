# GALAXIAN -- prueba visual con sprites 8x8

Prueba para ver cómo quedaría un Galaxian hecho solo con los 64 sprites 8×8 del
SD81 Booster. No es un juego: no hay colisiones ni puntuación.

## Qué muestra

| Sprites | Qué son |
|---------|---------|
| 0-45    | Formación de 46 aliens como el arcade: 2 comandantes, 6 rojos, 8 púrpuras y 3 filas de 10 azules. Se balancea ±24 px y bate las alas (2 frames). |
| 46-48   | Un comandante con dos escoltas en picado, siguiendo una curva senoidal. |
| 61, 62  | Disparo y nave del jugador. |

El fondo son 48 estrellas fijas en el bitmap (modo Superfast HiRes Spectrum). No
se usa el doble buffer: todo lo que se mueve es un sprite.

## Teclas

| Tecla | Acción |
|-------|--------|
| O / P | Nave a izquierda / derecha |
| SPACE | Disparo |
| Q     | Salir a BASIC (apaga los sprites) |

## Qué mirar

- Si 8×8 se lee bien como Galaxian (`preview.png` muestra los dos frames de
  aleteo con la formación, el picado y la nave).
- Que el balanceo y el picado van fluidos y sin cortes (los sprites se mueven
  nada más empezar el VSYNC).
- El límite de **12 sprites por línea**: la fila más ancha tiene 10, y el picado
  empieza por debajo de la formación para no pasar de 12. Si se cambia el
  picado para que cruce una fila de 10, el índice más bajo de esa fila
  desaparece mientras coinciden.

## Cambiar los dibujos

Los aliens se leen de `GALAXIANS.png` (64×16, sprites de 8×8): tipo rojo en x 0-15,
magenta en 16-31 y cian en 32-47 (2 frames de aleteo cada uno) y el comandante en
x 48-55 (un solo frame, sin aleteo). Cada fila de un sprite admite **dos colores**
(tinta y papel con la máscara a 1); si la imagen trae más, `gen_gfx.py` avisa y
simplifica la fila (ahora ocurre en la fila 3 del comandante, que pierde los
píxeles naranja). Los colores de la imagen se traducen a códigos Chroma81 en la
tabla `COL` de `gen_gfx.py`. La nave y el disparo están allí como arte ASCII.
Al ejecutarlo genera `galaxian_gfx.inc` y `preview.png`:

```
python gen_gfx.py
pasmo galaxian.asm galaxian.bin galaxian.sym
```

o simplemente `compila.bat`, que además copia el binario al emulador.
Cargar con `GALAXIAN.B81` o a mano:

```basic
10 FAST
20 LOAD THEN CLEAR 29999
30 LOAD FAST "GALAXIAN.BIN" CODE 30000
40 RAND USR 30000
50 SLOW
```

El código ocupa ~1 KB desde 30000, muy por debajo de `$8000`.
