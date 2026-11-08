# PAGINA — fase 0 (d) de ESTELA

Comprueba el plan B de memoria del juego: si el código no cabe en `$4800-$7FFF`,
copiarlo a una página libre de la SRAM y mapearla en el **bloque 1** (`$2000`) o en
el **bloque 0** (`$0000`, en lugar de la ROM). Lo importante es que, desde ahí, la
**ventana de POKEs de control** (paleta, sprites, modo) siga funcionando y que el
bloque 0 siga siendo de solo lectura.

El código que se copia ("blob") hace, durante 2 segundos (100 imágenes, con el
VSYNC):

- **gira los colores de las cuatro barras** cada 8 imágenes (`POKE 2063-2065`,
  paleta de 256);
- **mueve un cuadrado blanco** (el sprite 0) de izquierda a derecha
  (`POKE 2100` y `2102`, la ventana de los sprites).

Se ejecuta cuatro veces, y el **borde** dice cuál:

| Borde | Fase | Desde dónde |
|---|---|---|
| blanco | 0 | en su sitio (`$4Bxx`), con la ROM: la referencia |
| azul | 1 | página 20 mapeada en el **bloque 1**, en `$2000` |
| magenta | 2 | página 21 mapeada en el **bloque 0**, en `$1000` (sin ROM) |
| cyan | 3 | la misma página 21, en `$07F0`: se ejecuta **dentro de la ventana de POKEs** (`$07F6-$0853`), y sus escrituras a 2063-2065 caen sobre sus propios bytes |
| **verde** | fin | todo ha vuelto a su sitio (2 s) |
| **rojo** | fin | algo no cuadra |

Durante las fases 2 y 3 no hay ROM: el programa trabaja con `DI` y en FAST (lo pone
el `.B81`). Las páginas se rellenan antes, mapeándolas en el bloque 7 (`$E000`).

## Cómo se prueba

Copia `PAGINA.BIN` y `PAGINA.B81` a una carpeta de la SD y carga `PAGINA.B81`.
Dura unos 10 segundos. Al acabar vuelve a BASIC y escribe:

```
ESTELA - PRUEBA D: PAGINA

RESULTADO: 0
BYTES CAMBIADOS EN LA PAGINA 21: 0
BIEN
```

`RESULTADO` es la suma de: 1 = la página 21 ha cambiado mientras estaba en el bloque
0 (el bloque 0 no era de solo lectura), 2 = el bloque 0 no ha vuelto a tener la ROM,
4 = el bloque 1 no ha vuelto a tener su página.

## Qué tiene que verse y qué hay que mandar

Lo que se ve es siempre lo mismo, **cuatro veces seguidas**: las cuatro franjas
cambiando de color y el cuadrado blanco avanzando de la izquierda hasta algo más de
la mitad de la pantalla (100 de los 160 píxeles), y vuelta a empezar. Lo que cambia
de una vez a otra es **desde dónde se ejecuta**, y eso solo lo dice **el color del
borde**: blanco, azul, magenta y cyan. Si una fase falla, en ella las franjas se
quedan quietas, el cuadrado no se mueve o se cuelga. Después, 2 s de borde verde
(o rojo) con todo quieto, y vuelve a BASIC.

- En **las cuatro fases** las franjas cambian de color y el cuadrado avanza, igual
  en todas, y el borde pasa por blanco, azul, magenta y cyan.
- Al final, borde verde y `RESULTADO: 0`.

La primera versión dejaba encendido el color de Chroma81 al salir y el BASIC no se
leía; ahora sale con `$7FEF = $0C` (color apagado), como el explorador.

**Qué hay que mandar:** en qué fases se ve todo bien; si en alguna las barras no
cambian o el cuadrado no se mueve, cuál; si se cuelga, **el color del borde** en ese
momento; y lo que escribe al final.

Si la fase 3 falla y la 2 no, el juego solo tiene que evitar poner código en la
ventana de POKEs (`$07F6-$0853`). Si fallan la 2 y la 3, el código de reserva irá
solo al bloque 1 (8 KB).

## Comprobado en el PC

`claude/mqtttest/build_estela0.sh`: con el Z80 de EightyOne y un modelo del
mapeador (32 páginas, bloque 0 de solo lectura cuyas escrituras van a la ventana de
POKEs): cada fase se ejecuta desde donde toca (el arnés encontró que el primer
`CALL` del blob era absoluto y volvía a la copia original: corregido), en cada fase
100 imágenes con el sprite en X = 32..131 y 4 colores por imagen, las páginas vuelven
a su sitio, devuelve 0, y 1 con un byte de la página 21 cambiado a propósito.
El modelo **supone** que la ventana de POKEs no depende de la página del bloque 0:
eso es lo que comprueba la placa. **Sin probar en la placa.**
