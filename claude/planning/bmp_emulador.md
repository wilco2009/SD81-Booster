# Especificación para el emulador: comando 76, un BMP abierto como .I16

El visor de fotos (`EXAMPLES/PHOTO`) y el explorador (por `EXAMPLES/IMGVIEW`, que lanza el stub BASIC al abrir un `.BMP` o un `.I16`) pueden mostrar un `.BMP` directamente: el MCU lo convierte al abrirlo
(escala a 160x100 y reduce a 256 colores) y lo ofrece como si fuera un `.I16` de 16.516 bytes. El firmware añade el comando **76 (0x4C)**,
`cmd_f_open_img`, y cambia fseek / fread / fstat / fclose para un manejador virtual. Sin esto en el emulador, el visor sigue viendo los
`.I16` pero un `IMGnnn.BMP` se salta (el comando 76 no responde).

Referencia: `Arduino/SD81BoosterV2_039_STM32/COMMANDS.cpp` (`cmd_f_open_img`, `img_buf`, `f_handle_close`), `BMP.h` / `BMP.cpp` (el
conversor) y `EXAMPLES/PHOTO/bmp2i16.py` (la misma conversión en Python).

**Estado en el emulador (28/10/2026): IMPLEMENTADO, sin probar todavia.** Es el codigo real del firmware, sin tocar: `COMMANDS.cpp/.h`, `MCUSTATE.cpp`, `GLOBALS.h`, `BMP.cpp` y `BMP.h` copiados a `src/SD81Booster/mcu/` (`BMP.cpp` en el CMake); el emulador no tiene codigo propio para este comando.

## 1. Comando 76 (0x4C): fopen de un BMP como .I16

```
Z80 -> MCU:  76, len, nombre[len], fit
MCU -> Z80:  resultado
```

- `nombre`: ASCII tal cual, relativo al directorio actual del SD salvo que empiece por `/` (como fopen, 53).
- `fit`: cómo se coloca el BMP en los 160x100 (la pantalla es 4:3): **0** estira para llenar todo, **1** conserva la proporción y rellena de
  negro, **2** conserva la proporción y recorta lo que sobra.
- `resultado`:

| Valor | Significa |
|---|---|
| 0..3 | manejador (uno de los 4 de fopen) de una imagen virtual de solo lectura |
| 0xFF | el fichero no existe o no se puede abrir (sin informe) |
| 0xFE | el BMP no se puede convertir; el motivo queda en `/MAN/BMPERR.TXT` (`LOAD THEN PRINT "*BMPERR"`) |
| 0xFD | sin memoria (el firmware reserva unos 23 KB: 16.516 de la imagen y 6.360 de trabajo) |
| 0xFC | no hay manejador libre, o **ya hay una imagen abierta** (solo una a la vez) |

- Un resultado válido borra `/MAN/BMPERR.TXT` si existía; los errores 0xFE, 0xFD y 0xFC lo escriben (cuatro líneas: `ERROR IN BMP FILE`,
  la ruta, el motivo).
- **La conversión tarda** (hay que leer el fichero dos veces: de una décima a unos segundos según el tamaño y la SD). El Z80 espera la
  respuesta sin tiempo límite: el emulador puede responder de golpe.
- El nombre no puede pasar de 99 caracteres.

## 2. El manejador virtual

El manejador devuelto funciona como un fichero de solo lectura de **16.516 bytes** (el contenido, en §3):

- **fread (55)**: igual que siempre, con `count <= 512`; lo que pasa del final se rellena con ceros y el estado es 0x01 (EOF corto).
- **fseek (54)**: valen posiciones de 0 a 16.516 (inclusive); más allá, estado 0xFF.
- **fstat (59)**: tamaño 16.516, fecha y hora 0, estado 0x00.
- **fwrite (56)**: estado 0xFF, no escribe.
- **fclose (57)**: libera la imagen (otra vez se puede abrir una).
- Un reset del Z80 cierra todos los manejadores, también este.

## 3. El contenido: formato .I16

```
0      "I160"
4      512 bytes de paleta: por entrada, byte bajo {G[3:0],B[3:0]} y byte alto {BRIGHT[3:0],R[3:0]} (lo que se escribe en POKE 2064 / 2065)
516    16.000 píxeles, 100 filas de 160, un byte por píxel = índice de la paleta
```

Lo genera `bmp_to_i16()` de **`BMP.cpp`, que no depende de Arduino y se puede compilar tal cual en el emulador** (es C++ estándar:
`BMP.h` + `BMP.cpp`). Solo hay que darle un `BmpReader` sobre el fichero (`size()`, `seek()`, `read()`; ver `FileReader` en
`claude/mqtttest/test_bmp.cpp`), un búfer de `BMP_I16_SIZE` bytes y una `BmpWork` (6.360 bytes). Si se prefiere reescribirlo, la referencia
es `bmp2i16.py`, con el mismo algoritmo entero (byte a byte igual: `claude/mqtttest/build_bmp.sh` lo comprueba con 300 casos).

Resumen del algoritmo, por si hace falta (todo en enteros):

1. **Cabecera BMP**: `BM`, cabeceras CORE (12), INFO (40) y V4/V5 (108/124); 1, 4, 8, 24 y 32 bits sin compresión, de abajo arriba o de
   arriba abajo; 32 bits con BI_BITFIELDS solo con las máscaras BGRA normales. Hasta 16.384 x 16.384 y 16 millones de píxeles. Si no, 0xFE con
   el motivo (`NOT A BMP FILE`, `BAD BMP HEADER`, `UNSUPPORTED BMP DEPTH`, `UNSUPPORTED BMP (COMPRESSED)`, `BMP TOO LARGE`, `BMP TRUNCATED`).
2. **Escalado** a 160x100 con la media por área (pesos de solape enteros). `fit` 1 y 2 usan la proporción 4:3 de la pantalla.
3. **Paleta** de 256 colores por cortes de mediana sobre un histograma de 4+4+4 bits de la imagen escalada; cada píxel va al color más cercano.
4. **Densidades**: cada color de la paleta pasa a las cuatro densidades (R, G, B, BRIGHT) de la red de la SCART (`solve_int`, como `pal160.py`).

## Cómo probarlo

`claude/mqtttest/build_bmp.sh` compara `BMP.cpp` con `bmp2i16.py` sobre BMP de todos los tipos; `claude/mqtttest/build_photo.sh` ejecuta el
visor Z80 con un MCU simulado que implementa este comando (ver `mcu_byte()` en `test_z80photo.cpp`: es una buena guía para el emulador).
En la placa o el emulador: poner `IMG006.BMP` junto a `PHOTO.BIN` y `PHOTO.B81` y cargar `PHOTO.B81`; la tecla **F** del visor cambia el `fit`. Con el explorador: `EXAMPLES/IMGVIEW`
(`claude/mqtttest/build_imgview.sh` lo prueba con el mismo MCU simulado) y el `AUTOEXEC.P` nuevo; el explorador usa `fit` 1 (con su proporción) al abrir.
