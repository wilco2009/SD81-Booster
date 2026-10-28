# COLORTEST: prueba de los colores y del brillo del modo Superfast HiRes Spectrum

Pinta, en las mismas 8 columnas (0 negro, 1 azul, 2 rojo, 3 magenta, 4 verde, 5 cian, 6 amarillo, 7 blanco):

| Fila | Qué es |
|---|---|
| `PAPER BRIGHT 1` y debajo `PAPER BRIGHT 0` | el color en el **papel** (bitmap a 0), con el bit 6 del atributo puesto y sin ponerlo |
| `INK BRIGHT 1` y debajo `INK BRIGHT 0` | el color en la **tinta** (bitmap a `$FF`, papel negro), con y sin el bit 6 |
| `BIT 7 SET, BIT 6 CLEAR` / `SET` | diagnóstico: los mismos papeles con el bit 7 (FLASH en el Spectrum) puesto, sin y con el bit 6 |

Si el brillo funciona, cada fila `BRIGHT 1` se ve más viva que la `BRIGHT 0` que tiene debajo (el negro es igual en las dos).
Cualquier otra tecla sale a BASIC.

**P** recorre las **paletas de 16 colores** de la FPGA con paleta (rama `fpga-mode160`). La pantalla no se toca (los atributos siguen siendo los mismos 16 colores de Chroma, índice `{BRIGHT,G,R,B}`), pero lo que sale por la SCART para cada índice pasa a ser el de la paleta. Con cada pulsación se pasa a la siguiente de `pal16.inc` (la genera `gen_pal16.py`): **gris, rojo, verde, azul, amarillo, cian y magenta**, cada una una rampa de 16 niveles (índice `k` = nivel `k/15`, de negro a la tinta más clara), y tras la última, **OFF**: vuelven los colores por defecto. Con una rampa puesta, todas las muestras pasan a tonos de ese color, de oscuro (índice 0) a claro (15), y los colores con brillo (8-15) quedan más claros que los normales (0-7), en orden. El gris llega al blanco brillante; los colores, hasta lo que da la red de la SCART sin brillo (el rojo, verde y azul puros, igual que sin paleta). La fila de abajo dice `PALETTE: <nombre>`. Al salir se deja desactivada. Cualquier otra tecla (menos la P) sale a BASIC; tras cada pulsación de P hay 20 ms de espera antes y después de soltarla para que el rebote de la tecla no cuente como otra pulsación ni como otra tecla. Protocolo: `POKE 2063` puntero, `2064` byte bajo `{G,B}`, `2066` byte alto `{BRIGHT,R}` (escribe y avanza), `2067,1` / `2067,0`.

```
pasmo colortest.asm colortest.bin colortest.sym        (o compila.bat: lo copia a SD81\TOOLS\COLORTEST del emulador)
```

Cargar `COLORTEST.B81` (pásalo a `.P`) y `RUN`. Usa los mismos ajustes de vídeo que `EXAMPLES/NETGAME/gfx.inc`: bitmap en `$A000`,
atributos en `$B800`, chroma en `$7FEF` (`$20`: color, modo 0), `POKE 2045,172`.
