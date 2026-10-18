# COLORTEST: prueba de los colores y del brillo del modo Superfast HiRes Spectrum

Pinta, en las mismas 8 columnas (0 negro, 1 azul, 2 rojo, 3 magenta, 4 verde, 5 cian, 6 amarillo, 7 blanco):

| Fila | Qué es |
|---|---|
| `PAPER BRIGHT 1` y debajo `PAPER BRIGHT 0` | el color en el **papel** (bitmap a 0), con el bit 6 del atributo puesto y sin ponerlo |
| `INK BRIGHT 1` y debajo `INK BRIGHT 0` | el color en la **tinta** (bitmap a `$FF`, papel negro), con y sin el bit 6 |
| `BIT 7 SET, BIT 6 CLEAR` / `SET` | diagnóstico: los mismos papeles con el bit 7 (FLASH en el Spectrum) puesto, sin y con el bit 6 |

Si el brillo funciona, cada fila `BRIGHT 1` se ve más viva que la `BRIGHT 0` que tiene debajo (el negro es igual en las dos).
Cualquier tecla sale a BASIC.

```
pasmo colortest.asm colortest.bin colortest.sym        (o compila.bat: lo copia a SD81\TOOLS\COLORTEST del emulador)
```

Cargar `COLORTEST.B81` (pásalo a `.P`) y `RUN`. Usa los mismos ajustes de vídeo que `EXAMPLES/NETGAME/gfx.inc`: bitmap en `$A000`,
atributos en `$B800`, chroma en `$7FEF` (`$20`: color, modo 0), `POKE 2045,172`.
