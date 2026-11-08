# Pruebas de la fase 0 de ESTELA

Comprobaciones en la placa antes de programar el matamarcianos (plan:
`claude/planning/juego160.md`, §9). Cuatro programas pequeños, cada uno con su
`README.md` (qué hace, qué tiene que verse y qué hay que mandar):

| Carpeta | Prueba | Qué mide |
|---|---|---|
| [`CARGA`](CARGA/README.md) | (a) | Código en `$4800` con `CLEAR 18431` desde un `.B81`: los 14 KB hasta `$7FFF` cargan y se ejecutan |
| [`VSYNC`](VSYNC/README.md) | (b) | T-estados del VSYNC a la primera línea de la imagen (la 58), con un corte de la paleta que se ve en el televisor |
| [`SONIDO`](SONIDO/README.md) | (c) | Música VGM (AY del MCU) con efectos en los AY de la FPGA, PEG y voz, a la vez |
| [`PAGINA`](PAGINA/README.md) | (d) | Código copiado a una página y ejecutado desde el bloque 1 y el 0, con la ventana de POKEs funcionando |

Las cuatro cargan su código en `$4800` (`LOAD THEN CLEAR 18431`), como lo hará el
juego, y las tres últimas usan el modo 160x100 a 256 colores (`POKE 2045,175`).

## Ensamblar y copiar

`compila.bat` ensambla las cuatro con pasmo y las copia, con `MUSICA.VGM`
(`EXAMPLES/VGM/music0002.vgm`), a `C:\ClaudeCode\Eightyone2\EightyOne\SD81\test\ESTELA`.
Para la placa, copia a una carpeta de la SD:

```
CARGA.BIN   CARGA.B81
VSYNC.BIN   VSYNC.B81
SONIDO.BIN  SONIDO.B81  MUSICA.VGM
PAGINA.BIN  PAGINA.B81
```

y carga cada `.B81` (`LOAD "VSYNC.B81"` y `RUN`).

Arnés del PC (las cuatro con el Z80 de EightyOne): `claude/mqtttest/build_estela0.sh`.

## Resultados que hacen falta para la fase 1

| Prueba | Qué mandar |
|---|---|
| CARGA | `RESULTADO`, `ERRORES DEL PATRON`, `RAMTOP` y `LIBRE DEBAJO DE 18432` |
| VSYNC | D0 (el D más pequeño con la primera línea negra), F, W, y si el corte cae en las filas 10 y 50 con D0 + 4.140 y D0 + 20.700 |
| SONIDO | Si la música y los efectos 1 y 2 suenan a la vez sin estropearse; qué pasa con el 3 (PEG) y el 4 (voz) |
| PAGINA | Qué fases (borde blanco, azul, magenta, cyan) se ven bien; el borde final y el `RESULTADO` |
