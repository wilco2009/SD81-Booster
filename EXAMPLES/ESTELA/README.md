# ESTELA — matamarcianos vertical (en desarrollo)

Juego original para el modo **160x100 a 256 colores** del SD81 Booster
(`POKE 2045,175`): una nave que sube por un escenario de tiles que pasa por
debajo con el scroll en anillo de la FPGA, sprites de formato B y música VGM.
El plan entero (fases, mapa de memoria, sprites) está en
`claude/planning/juego160.md`.

| Carpeta / fichero | Qué es |
|---|---|
| `estela.asm`, `ESTELA.B81`, `compila.bat` | el juego (núcleo Z80) |
| `PRUEBAS/` | pruebas de la fase 0 en la placa (carga, VSYNC, sonido, paginación) |
| `tools/`, `assets/`, `out/` | herramientas del PC y gráficos del nivel (las lleva otra sesión; §8 del plan) |

## Estado: fase 1 (esqueleto)

Lo que hay ahora es solo el **fondo con scroll**, sin nave ni enemigos:

- **Modo 175** con una paleta de prueba: los colores 0-239 son los RGB 3-3-2 de
  fábrica y los 240-255 una rampa de grises.
- **Anillo de 160x102** en `$8000` (`POKE 2073,102`): 100 filas visibles y 2 ocultas
  donde se pinta lo que va a entrar por arriba antes de moverlo.
- **Paneles laterales de 16 píxeles**, grises con relieve, iguales en todas las
  filas del anillo: se ven **quietos** aunque el fondo se mueva.
- **Scroll vertical continuo** de un fondo de **tiles de 16x16** (8 de ancho), hacia
  abajo, como si la nave subiera. El tileset (60 tiles) y el mapa los genera el
  propio programa: en la fase 2 serán el `.I16` y el mapa del nivel.
  - El tileset está en `$C000` con el formato de un `.I16` (160 bytes por fila, 10
    tiles por fila). Los bloques 6 y 7 se mapean a las páginas 6 y 7 mientras
    corre el juego y se devuelven al salir.
  - Cada tile tiene un borde negro arriba y a la izquierda (una rejilla de 16x16),
    una diagonal blanca y un damero de 8x8 en dos colores que dependen del tile.

El programa tarda un segundo y medio en arrancar (genera el tileset con la
pantalla en negro).

## Teclas

| Tecla | Acción |
|-------|--------|
| S | Velocidad: 1 píxel por imagen (borde **verde**, al empezar) → 1 cada 2 imágenes (**cyan**) → 2 por imagen (**amarillo**) → parado (**rojo**) → 1 por imagen |
| X | Salir a BASIC (modo normal, color apagado, páginas de antes) |

## Qué tiene que verse

- El fondo baja **suave, sin saltos ni temblores**, a las tres velocidades.
- La **rejilla negra y las diagonales blancas siguen rectas y continuas**: ninguna
  fila repetida, cortada o de otro color al pasar por el borde del anillo (cada 102
  filas) ni al cambiar de fila de tiles (cada 16).
- Por **arriba no entra basura**: lo que aparece ya está pintado.
- Los **paneles** de los lados no se mueven nada.
- Al salir, el BASIC se lee.

**Qué hay que mandar:** si se ve así a las tres velocidades; si no, qué falla
(una foto ayuda) y a qué velocidad.

## Comprobado en el PC

`claude/mqtttest/build_estela.sh`: el Z80 de EightyOne con un modelo de la memoria
paginada, el VSYNC (64.584 T por imagen, 8 líneas, como lo midió `PRUEBAS/VSYNC` en
la placa), el teclado y el direccionamiento del anillo de la FPGA:

- el tileset de `$C000` coincide con un modelo escrito aparte en C++;
- en **466 instantes al azar** (también mientras se pinta la fila oculta), lo
  visible es **exactamente** el mundo del modelo desde la fila que había cuando se
  escribió `POKE 2072`, con los paneles;
- nunca se escribe una fila del anillo que se esté viendo;
- `POKE 2072` sale como mucho 71 T después del flanco del VSYNC (en la placa
  hay unos 12.600 hasta la primera línea de la imagen), una vez por imagen;
- S da 1, 1 cada 2, 2 y 0 filas por imagen, con 8 vueltas al anillo;
- al salir: modo 85, anillo a 0, `$7FEF = $0C`, páginas de los bloques 6 y 7 como
  estaban.

El arnés encontró un fallo de verdad antes de llegar a la placa: la rutina del
mapa machacaba el destino de la fila al cambiar de fila de tiles. La primera prueba
en la placa (basura y cuelgue a los pocos segundos) se hizo con un `estela.bin` de
antes de ese arreglo; ahora el arnés ensambla y prueba el `estela.bin` de esta
carpeta, el mismo que va a la SD. Con el binario nuevo **funciona en la placa**
(2026-11-08).

## Ensamblar y ejecutar

`compila.bat` ensambla con pasmo, comprueba que el binario no pasa de `$6FFF`
(la hoja de los sprites está en `$7000` y el ZX81 no ejecuta código por encima de
`$8000`) y lo copia al emulador (`SD81\SD81\GAMES\ESTELA`). Para la placa, copia
`ESTELA.BIN` y `ESTELA.B81` a una carpeta de la SD y carga `ESTELA.B81`.

Memoria: código y tablas en `$4800-$4D87`, anillo en `$8000-$BFBF`, tileset en
`$C000-$FBFF` (páginas 6 y 7).

**El emulador** todavía no tiene el scroll en anillo de los modos 160
(`sprites_scroll160_emulador.md`): ahí el fondo no se moverá bien. Hay que probarlo
en la placa.
