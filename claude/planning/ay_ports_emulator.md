# Especificación para el emulador: puertos AY, ROMLOCK y doble buffer

Dos cosas que el emulador tiene que reproducir como el hardware real, y que
SD81TEST comprueba: los puertos de los dos chips AY (primera parte),
`LOAD *ROMLOCK` (segunda parte) y el doble buffer (tercera parte).

# 1. Puertos de los chips AY

Qué tiene que reproducir el emulador para que los dos AY se comporten como en
el hardware real. La referencia es la FPGA:

- Decodificación: `FPGA/SD81V2.1000/SD81.v`, sección "AY-8912"
  (`ay_port_access`, `ay_cs`, `ay_bc1`, `ay_bdir`).
- Registros: `FPGA/SD81V2.1000/ay38912.v` (módulo `ay_3_8192`).

Estos puertos son los dos AY físicos de la FPGA. No tienen nada que ver con el
AY que emula el MCU (comandos `AY_SET_REG`/`AY_GET_REG`, `PLAY`, VGM), que va
por el protocolo del MCU y no por estos puertos.

## Decodificación del puerto

Solo se miran cinco bits de la parte baja de la dirección. A0, A4, A6 y toda
la parte alta (A8–A15) se ignoran:

| Bit | Valor | Significado |
|---|---|---|
| A1 | 1 | obligatorio |
| A2 | 1 | obligatorio |
| A5 | 0 | obligatorio |
| A3 | 1 / 0 | chip A (ZonX estándar) / chip B |
| A7 | 1 / 0 | selección de registro / dato (es la línea BC1 del AY) |

Y tiene que ser un ciclo de E/S (`/IORQ` bajo) de lectura o de escritura.

Con BC2 fijo a 1, BDIR = escritura (`/RD` alto) y BC1 = A7, los cuatro casos
son:

| Ciclo | A7 | Estado del AY | Qué hace |
|---|---|---|---|
| `OUT` | 1 | latch (BDIR=1, BC1=1) | selecciona el registro: guarda el byte **entero** |
| `OUT` | 0 | escritura (BDIR=1, BC1=0) | escribe el dato en el registro seleccionado |
| `IN` | 1 | lectura (BDIR=0, BC1=1) | devuelve el registro seleccionado |
| `IN` | 0 | inactivo | el AY no conduce el bus |

### Todos los puertos equivalentes

Como A0, A4 y A6 no importan, cada función responde en ocho puertos:

| | Selección / lectura (A7=1) | Dato (A7=0) |
|---|---|---|
| **Chip A** (A3=1) | `$8E $8F $9E $9F $CE $CF $DE $DF` | `$0E $0F $1E $1F $4E $4F $5E $5F` |
| **Chip B** (A3=0) | `$86 $87 $96 $97 $C6 $C7 $D6 $D7` | `$06 $07 $16 $17 $46 $47 $56 $57` |

Los habituales son `$CF`/`$DF` + `$0F`/`$1F` (ZonX, las dos revisiones) para el
chip A y `$C6` + `$06` para el chip B, que son los que documenta el manual.
Pero cualquier programa puede usar cualquiera de la tabla, y SD81TEST usa
`$C7` para leer el chip B (ver más abajo).

No se solapan con los puertos del propio interface: `$A7`, `$AF`, `$E7` y
`$FB` tienen A5=1. Con el SD81 conectado no puede haber a la vez otro
periférico en esos puertos (por ejemplo, el ZXpand en `$07`), así que con el
SD81 activo `$07` tiene que ir al chip B, como en el hardware.

## Registro seleccionado

- El latch guarda los 8 bits del dato, no solo los 4 bajos.
- Si el nibble alto del registro seleccionado no es 0 (registro "16–255"),
  **las escrituras se ignoran** y **las lecturas devuelven `$FF`**.
- Escribir en el registro 13 reinicia la envolvente (como en un AY real).

## Escritura: bits que se guardan

| Registro | Se guarda |
|---|---|
| 0, 2, 4 (periodo fino A/B/C) | 8 bits |
| 1, 3, 5 (periodo grueso A/B/C) | bits 3–0 |
| 6 (periodo del ruido) | bits 4–0 |
| 7 (mezclador) | 8 bits |
| 8, 9, 10 (volumen A/B/C) | bits 4–0 (bit 4 = modo envolvente) |
| 11, 12 (periodo de la envolvente) | 8 bits |
| 13 (forma de la envolvente) | bits 3–0 |
| 14, 15 (puertos de E/S A y B) | 8 bits |

## Lectura: qué se devuelve

Lo guardado, con los bits que no existen a 0:

| Registro | Valor leído |
|---|---|
| 0, 2, 4, 7, 11, 12 | el byte completo |
| 1, 3, 5, 13 | `0000` + los 4 bits guardados |
| 6, 8, 9, 10 | `000` + los 5 bits guardados |
| 14 | si el puerto A está como entrada (bit 6 del registro 7 a 0): lo que haya en el puerto A del chip; si está como salida: lo último escrito |
| 15 | si el puerto B está como entrada (bit 7 del registro 7 a 0): `$FF` (el AY-3-8912 no tiene puerto B); si está como salida: lo último escrito |
| 16–255 | `$FF` |

En el hardware, un `IN` a un puerto **par** (A0=0) también lo contesta la ULA
del ZX81 con el teclado, y los dos chocan en el bus. Por eso SD81TEST lee el
chip B por `$C7` (impar) y no por `$C6`. El emulador puede limitarse a
devolver el valor del AY en cualquiera de los puertos de la tabla; no hace
falta simular el choque.

## Efectos laterales de la ULA

Son los mismos que en cualquier otro puerto del ZX81, y el emulador ya debería
aplicarlos de forma general:

- `OUT` con A0=0 (por ejemplo `$06`) enciende el generador de NMI.
- `OUT` con A1=0 lo apaga. En los puertos AY A1 siempre es 1, así que nunca
  ocurre.
- Cualquier `OUT` termina el VSYNC.

## Reset

Al reiniciar la FPGA, el módulo vuelve a su estado inicial: registros a 0 y
registro seleccionado 0.

## Cómo comprobarlo con SD81TEST

- **6 → 1 (registros de los AY):** escribe y relee los registros 0, 2, 4, 11
  y 12 de los dos chips con 12 patrones. Chip A por `$CF`/`$0F`; chip B por
  `$C7` (selección y lectura) y `$47` (dato: `$C7 AND 7Fh`). Tiene que dar 0
  errores.
- **6 → 2 (tonos y voz):** tres tonos en el chip A (`$CF`/`$0F`) y tres en el
  chip B (`$C6`/`$06`), cada vez más agudos. Tienen que oírse los seis.

# 2. ROMLOCK (`LOAD *ROMLOCK` / `LOAD *ROMLOCK STOP`)

Referencias: `FPGA/SD81V2.1000/SD81.v` (`PORTS_LOCKED`, y los `*_wr` que lo
usan), `Arduino/SD81BoosterV2_039_STM32/COMMANDS.cpp` (`cmd_romlock_on`,
`cmd_romlock_off`) y `z80rom/sdhandler.inc.asm` (`CmdROMLOCK`).

## Para qué sirve

Algunos programas antiguos escriben en direcciones de la ROM creyendo que no
pasa nada, pero en el SD81 algunas de ellas son puertos de configuración
mapeados en memoria. Con ROMLOCK activo, esas escrituras vuelven a no tener
ningún efecto, como en una ROM real.

## Cómo se activa

Solo con los comandos del MCU, nunca con una escritura en memoria:

| Comando MCU | BASIC | Efecto |
|---|---|---|
| `$44` (68) | `LOAD *ROMLOCK` | bloqueo activado |
| `$45` (69) | `LOAD *ROMLOCK STOP` | bloqueo desactivado |

- Son comandos de un solo byte: sin parámetros y sin respuesta. El MCU
  cambia el reloj (bit 7 de `$AF`) para confirmarlo, como en cualquier otro
  comando.
- El MCU se lo pasa a la FPGA con `send_bit_config(cfgcmd_ROMLOCK = 7,
  valor)`. En la FPGA es el registro `PORTS_LOCKED`.
- Al encender vale 0 (desbloqueado). La FPGA no lo toca en el reset del
  Z80: solo cambia con el comando.
- Activarlo no cambia el estado actual de los puertos: solo congela los
  valores que ya tengan. Por ejemplo, si Superfast ya estaba encendido,
  sigue encendido.

## Qué bloquea

Con el bloqueo activo, la FPGA ignora las escrituras en:

| Direcciones | Qué son |
|---|---|
| 2041–2058 | los `POKE` de configuración: 2041–2042, HFILE (2043–2044), Superfast (2045), patrón de borde (2046–2055), 2056–2058 |
| 2059–2061 | redirección de atributos |
| 2062 | extensión de MC45 a los bloques 6/7 |
| 2090–2098 | scroll fino, 80 columnas y redirección de D_FILE |
| 2100–2128 | sprites (selección y registros) |

Lo que **no** bloquea:

- 2038–2040 (interrupciones simuladas).
- El mapper (`OUT $E7`), los puertos del MCU (`$A7`/`$AF`), los AY y el
  puerto de Chroma (`$7FEF`).
- Las variables del sistema que la FPGA copia al escribirse, como FRAMES
  (16436) o D_FILE (16396). Están en RAM y no son puertos del bloque 0.
- Las lecturas. El bloqueo solo afecta a las escrituras.

Todos esos puertos, además, solo responden cuando el bloque 0 está
protegido (`!block0Writable`). Eso ya pasa sin ROMLOCK.

## Cómo lo comprueba SD81TEST (menú 3 → 2, `USR 20498`)

En FAST, 10 veces:

1. `$45` (sin bloqueo). Luego `POKE 2045,170`: Superfast tiene que
   encenderse. Después `POKE 2045,85`: tiene que apagarse.
2. `$44` (con bloqueo). Luego `POKE 2045,170`: **no tiene que pasar nada**.

Al final manda `$45` y deja `POKE 2045,85`.

Para saber si Superfast está encendido (`probe_sf`):

1. Escribe 1000 en FRAMES.
2. Espera un bucle de unos 3 cuadros.
3. Vuelve a leer FRAMES.

Con Superfast, la FPGA decrementa FRAMES en cada VSYNC aunque estemos en
FAST, y es la FPGA la que contesta la lectura de 16436/16437. Si hay
Superfast, lo leído tiene que ser entre 1 y 3 menos que 1000; sin
Superfast, exactamente 1000.

Resultado en pantalla:

| Fila | Texto | Qué indica si falla |
|---|---|---|
| 5 | `UNLOCKED (MUST WORK) FAILS: n` (de 20) | el POKE 2045 o FRAMES en Superfast. Tiene que fallar igual en la prueba 3 → 1 (`POKE 2045`) |
| 6 | `LOCKED (NO EFFECT) FAILS: n` (de 10) | el emulador no aplica el bloqueo: no trata los comandos `$44`/`$45`, o no ignora la escritura en 2045 |

Las dos filas tienen que dar 0.

# 3. Doble buffer (`POKE 2057`)

Referencia: `FPGA/SD81V2.1000/SD81.v`: `dbuf_en`, `front_blk`,
`auto_blit_en`, `dbuf_wr_mask`, `vpage` y la FSM del blit (`blit_run`).

## Modelo

La FPGA tiene una memoria interna de vídeo de 64 KB que es un espejo de
los 8 bloques. Cada escritura de la CPU va a la SRAM y además al espejo,
en la misma dirección del Z80. **El vídeo Superfast lee siempre el
espejo, nunca la SRAM.** Hay que emularlo con dos memorias, aunque la
mayor parte del tiempo coincidan.

| POKE 2057 | Efecto |
|---|---|
| `168+B` | doble buffer AUTO, front = bloque B |
| `200+B` | doble buffer MANUAL, front = bloque B |
| `85` | apagado |

Con el doble buffer activo (AUTO o MANUAL):

- **El vídeo lee el espejo del bloque front** en lugar del bloque de
  HFILE (`vpage = dbuf_en ? front_blk : HFILE[15:13]`), en HiRes nativo y
  en modo Spectrum. El modo texto no usa el doble buffer.
- **Las escrituras de la CPU en el bloque front no llegan a su espejo:**
  `dbuf_wr_mask = dbuf_en & (A15-A13 == front_blk)`. Sí llegan a la SRAM,
  y la CPU las relee bien, pero **no se ven**. Esto vale igual en AUTO y en
  MANUAL.

Solo en AUTO: en cada VSYNC, al terminar el área visible, la FPGA copia
los 8 KB del espejo del bloque HFILE al espejo del bloque front. Tarda
unos 630 µs. Entre dos copias, lo que se ve es una foto fija.

En MANUAL no hay copia: se ve el espejo del front tal como quedó, y el
programa solo cambia de front (`POKE 2057,200+B`).

Consecuencia importante: al cambiar de front en MANUAL, el bloque nuevo
se ve con lo que su espejo tenía. Mientras no era front, su espejo se
actualizaba con cada escritura, así que muestra lo que se pintó en él.
Lo que se escribió en un bloque **mientras era front** no está en su
espejo.

## Cómo lo comprueba SD81TEST (menú 5 → 9, `USR 20570`)

HiRes nativo (`POKE 2045,171`) con HFILE en `$8000` (bloque 4):

1. **AUTO, front = bloque 5.**
   - Se llena de ruido la SRAM del bloque 5 (`$A000`). **No tiene que
     verse.**
   - Se pintan barras en el bloque 4. Van apareciendo cuadro a cuadro.
2. **MANUAL** (el doble buffer se apaga antes con `POKE 2057,85`).
   1. Se pinta el tablero en el bloque 4, con el doble buffer apagado, así
      que su espejo se actualiza.
   2. Se activa `POKE 2057,204`, con front = bloque 4. Se ve el tablero.
   3. Se pintan barras en el bloque 5, que no es front: su espejo se
      actualiza.
   4. **Se llena de ruido el bloque 4, que es el front. No tiene que
      verse:** la imagen sigue siendo el tablero.
   5. Con `POKE 2057,205`, front = bloque 5, las barras aparecen de golpe.

Si en el paso 2.4 se ve el ruido, el emulador no aplica la máscara de
escritura en modo MANUAL, o en MANUAL muestra la memoria del bloque
front en lugar de su espejo.

