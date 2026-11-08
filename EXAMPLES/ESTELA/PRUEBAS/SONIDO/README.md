# SONIDO — fase 0 (c) de ESTELA

Comprueba que la **música VGM** y los **efectos** suenan **a la vez**, y por qué
camino conviene sacar los efectos del juego.

La música (`MUSICA.VGM`, de la misma carpeta, en bucle) la toca el MCU en **su**
AY emulado, sin gastar CPU (comandos 38 `LOOP_VGM` y 34 `PLAY_VGM`). El firmware
usa ese mismo AY para los efectos PEG y para la voz, así que los efectos se
prueban por cuatro caminos:

| Tecla | Efecto | Por dónde sale |
|-------|--------|----------------|
| 1 | láser (tono que baja, 0,3 s) | canal C del **AY A de la FPGA** (ZonX, puertos `$CF`/`$0F`): el Z80 escribe los registros en cada imagen |
| 2 | explosión (ruido, 0,6 s) | canal C del **AY B de la FPGA** (`$C7`/`$07`), igual |
| 3 | "coin" (3 notas, PEG) | **AY del MCU**, el mismo de la música (comandos 40 y 41) |
| 4 | voz "WARNING" | MCU, comando 23 en segundo plano (`*WARNING`) |
| 5 | los cuatro a la vez | |
| 6 | **diagnóstico**: acorde de 1 s, volumen 15 | los **tres canales de los dos AY de la FPGA**, como el test 23 de SD81TEST |
| M | parar / seguir la música | comandos 36 y 37 |
| X | salir | para la música y calla los AY de la FPGA |

Los efectos van por el **canal C**, que en la FPGA sale por los dos lados (el A solo
por la izquierda y el B por la derecha: `sample_l` / `sample_r` de `SD81.v`). En la
primera versión iban por el canal A y no se oyeron; no está demostrado que fuera
por eso (el arnés del PC ve las escrituras de los registros en las dos versiones),
y para salir de dudas está la tecla 6.

Funciona en el modo 160x100 a 256 colores, con el bucle a 50 imágenes por segundo
esperando el VSYNC, como el juego.

## Pantalla

Cuatro barras, una por efecto (1 a 4, de izquierda a derecha), en gris oscuro;
cada una se enciende mientras suena su efecto (la 3 y la 4 durante un tiempo fijo,
porque el programa no sabe cuándo acaban). **Borde:** verde = música sonando,
amarillo = parada con M, **rojo = el MCU no ha podido abrir `MUSICA.VGM`** (los
efectos se pueden probar igual).

## Cómo se prueba

Copia `SONIDO.BIN`, `SONIDO.B81` y un VGM de AY con el nombre **`MUSICA.VGM`** a la
misma carpeta de la SD (`compila.bat` copia `EXAMPLES/VGM/music0002.vgm`, de un
minuto). Carga `SONIDO.B81` y, con la música sonando, pulsa 1, 2, 3, 4, 5 y 6,
varias veces, y M dos veces.

## Qué hay que escuchar y mandar

1. ¿Suena la música? ¿Se encienden las barras 1 y 2 al pulsar 1 y 2? (Si se
   encienden y no suena nada, el programa ha escrito los registros; si no se
   encienden, no ha visto la tecla.) ¿Suena el acorde de la tecla 6?
2. **1 y 2 (AY de la FPGA):** ¿se oyen a la vez que la música, sin que la música se
   corte, cambie de tono o tartamudee? ¿Se oyen 1 y 2 a la vez entre sí (tecla 5)?
   Es el camino previsto para los efectos del juego.
3. **3 (PEG):** lo esperado es que **se pise** con la música, porque escriben el
   mismo AY: el efecto cambia el mezclador y el canal A. Di qué pasa: si se oye el
   efecto, si la música pierde un canal, y si se recupera sola después.
4. **4 (voz):** ¿se oye la voz con la música? ¿La música se para o se ralentiza
   mientras habla? ¿Vuelve bien?
5. **M:** ¿para y sigue? (Al seguir, la música puede tardar un poco en recuperar
   todos los canales: la pausa reinicia el AY del MCU.)
6. Si el volumen relativo de la música y de los efectos de la FPGA es razonable.

## Comprobado en el PC

`claude/mqtttest/build_estela0.sh`: con el Z80 de EightyOne y un MCU simulado,
manda `LOAD_PEG` (coin en la dirección 0), `LOOP_VGM 1` y `PLAY_VGM "MUSICA.VGM"`
al empezar; cada tecla una sola vez por pulsación; los registros del láser y la
explosión imagen a imagen (volumen, periodo y ruido); `PAUSE_VGM`/`CONT_VGM` con
M y `STOP_VGM` al salir; los AY de la FPGA callados al salir; y, sin el VGM, borde
rojo y ningún comando de música. Lo que no puede comprobar es cómo suena.
**Sin probar en la placa.**
