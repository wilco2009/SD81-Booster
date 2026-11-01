# IMGVIEW: visor de imágenes del explorador (.BMP y .I16)

Al abrir un `.BMP` o un `.I16` con 8 / ENTER en el explorador, el stub BASIC (`AUTOEXEC.P` / `EXPLORER.P`, ver abajo) carga `/EXPLORER/IMGVIEW.BIN`, lo arranca
y la imagen sale a pantalla completa en el modo de 160x100 a **256 colores** (FPGA de la rama `fpga-mode160`, `POKE 2045,175`).
Lo hace igual que con los `.SCR`, los `.VGM` o el editor: el explorador no cambia, solo devuelve el nombre del archivo y el stub decide qué lanzar.

| Archivo | Cómo se lee |
|---|---|
| `.I16` | con `fopen` (53): `I160`, 512 bytes de paleta y 16.000 píxeles (ver `EXAMPLES/PHOTO`) |
| `.BMP` | con el comando **76** del MCU, que lo convierte al abrirlo (escala a 160x100 y reduce a 256 colores; tarda unos segundos según el tamaño y la SD) y lo ofrece como un `.I16` |

BMP admitidos: sin compresión, de 1, 4, 8, 24 y 32 bits, de abajo arriba o de arriba abajo (ver `EXAMPLES/PHOTO/README.md`).

## Teclas

| Tecla | Acción |
|---|---|
| **F** | solo en un BMP: cambia la colocación y vuelve a cargarlo: **con su proporción** (bandas negras; la de salida), **recortado** (llena la pantalla sin distorsionar) o **estirado** (llena la pantalla). Con un `.I16` no hace nada |
| cualquier otra | volver al explorador |

## Si no se puede

Vuelve al BASIC con `BC = 1` (no hay nombre, no es `.BMP` ni `.I16`, no existe, o el `.I16` no empieza por `I160`) o `BC = 2` (el MCU no puede convertir el BMP). El stub
muestra `CANNOT SHOW <nombre>` y, en el caso 2, el motivo (`/MAN/BMPERR.TXT`), espera una tecla y vuelve al explorador. En los errores no llega a tocar la pantalla de vídeo.

## El stub BASIC

El explorador devuelve el nombre del archivo al BASIC (ver `EXAMPLES/EXPLORER/README.md`). Las líneas nuevas de `SD Content/explorer/EXPLORER.B81`, que también están en `AUTOEXEC.P` y `EXPLORER.P`:

```
  92 IF (E$ = ".BMP") OR (E$ = ".I16") THEN GOTO 3000
3000 LOAD FAST "/EXPLORER/IMGVIEW.BIN" CODE ORG
3010 LET R=USR ORG
3020 IF R=0 THEN GOTO 10
3030 SLOW 
3040 CLS 
3050 PRINT "CANNOT SHOW ";F$
3060 IF R=2 THEN LOAD THEN PRINT "*BMPERR"
3070 PRINT "PRESS ANY KEY"
3080 IF INKEY$="" THEN GOTO 3080
3090 GOTO 10
```

El nombre va en `F$`, que el visor busca en las variables BASIC y pasa del código ZX81 a ASCII (como `EDIT`). Los `.P` se regeneran desde el `.B81` con el conversor del PC
(`claude/b81test`: `my81.exe EXPLORER.B81 EXPLORER.P`; `EXPLORER.P` lleva `#!basic-start=2` delante, `AUTOEXEC.P` no).

## Memoria

El programa y sus variables desde 24576 (unos 1,4 KB, incluida la paleta de 512 bytes) y la imagen en `$8000-$BE7F` (HFILE = `$8000`). El código queda por debajo de `$8000`. El explorador
se vuelve a cargar al volver, así que no importa lo que pise.

## Ensamblar y probar

```
compila.bat          (pasmo imgview.asm imgview.bin imgview.sym; copia imgview.bin y los stubs al emulador y a SD Content)
```

`claude/mqtttest/build_imgview.sh`: el Z80 de EightyOne ejecuta el visor con `F$` en las variables BASIC, el teclado (con el rebote de la F), un MCU simulado que usa el mismo `BMP.cpp` del
firmware y la paleta de la FPGA. Casos: `.I16`, `.BMP` de 24 y 32 bits, F (1 → 2 → 0 → 1), `.I16` con F, BMP malo (BC = 2), `.I16` malo, archivos que no existen, otra extensión y sin `F$`
(BC = 1). Siete errores puestos a propósito en el visor fallan todos. **No prueba la FPGA ni la placa.**
