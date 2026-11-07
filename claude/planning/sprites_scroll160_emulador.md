# Para el emulador: sprites A y B, sprites extra y scroll de los modos 160 (7 de noviembre de 2026)

Lo que ha entrado en `master` del SD81 Booster con la fusión de la rama `fpga-sprites-hoja` y que el
emulador (EightyOne, `C:\ClaudeCode\Eightyone2`) todavía no tiene. Todo está **probado en la placa**.

| Commit | Qué |
|---|---|
| `c606819` | Sprites de formato A y B a la vez (`POKE 2130`) y hasta 127 sprites extra (`POKE 2131`): FPGA, ROM, `DEBUGGER.cpp`, `DEBUG_PAGE.h`, SPRDBUF y SPRXTRA |
| `93e7324` | Scroll en anillo de los modos 160x100 y 160x200 (`POKE 2072-2075`) y el ejemplo SCRL160 |
| `34d8b5e` | SPRXTRA mueve el fondo con O P Q A (usa el scroll del punto anterior) |

**Punto de partida del emulador:** su último commit de sprites es `112c35e`, que tiene la **primera** versión
del formato B (`3a7c997`). En ella la página iba en el campo 4 (`POKE 2105`), el índice 0 era siempre
transparente y el formato A no existía. **Eso cambia:** hay que volver a poner el formato A tal como estaba
antes de `112c35e` y rehacer el B como se describe aquí.

Referencia exacta: `FPGA/SD81V2.1000/sprite_engine.v` (cabecera y máquina de estados), `SD81.v` (POKEs) y
`mode160.v` (scroll). Los bancos de ModelSim (`tb_sprite_engine.v` con su `.sh`, y `tb_mode160*.v`) tienen
modelos de comportamiento que se pueden traducir casi línea a línea.

---

## 1. Sprites

### 1.1 Tabla y POKEs

La tabla no cambia de sitio: 64 sprites, 32 bytes cada uno, en la sombra. Los sprites 0-31 están en
`$0C00 + n*32` y los 32-63 en `$1800 + (n-32)*32`. Se escribe con la ventana de siempre (`POKE 2100` elige
el sprite).

| POKE | Campo | Qué es |
|---|---|---|
| `2101` | 0 | bit 0 = activo (como siempre) |
| `2102` | 1 | X, bits 7-0 |
| `2103` | 2 | bit 0 = X bit 8. **Nibble alto (bits 7-4) = color transparente en formato B** (nuevo; el reset y la ROM lo dejan a 0) |
| `2104` | 3 | Y |
| `2105-2112` | 4-11 | **formato A:** color de cada fila (tinta en el nibble alto, papel en el bajo), como antes de `112c35e`. En B no se usan |
| `2113-2120` | 12-19 | **formato A:** píxeles de cada fila (bit 7 = píxel de la izquierda). En B no se usan |
| `2121-2128` | 20-27 | **máscara de cada fila, en los dos formatos** (bit a 0 = píxel invisible). Antes de `112c35e` ya existía para A; ahora vale también para B |
| `2130` | **29** | **página de la hoja: 0 = formato A, otra = formato B** (nuevo; antes era el campo 4) |
| `2129` | — | bloque de 8 KB de las hojas (0-7, por omisión 2), **latcheado en el VSYNC**. No cambia |
| `2131` | — | **número de sprites extra, M (0-127; 0 = ninguno).** Es un registro de la FPGA, **sin latch** y **a 0 tras el reset** (el byte de la sombra no se borra; ver 3.2) |

El campo 28 no se usa.

### 1.2 Formato A (página 0)

Es el formato de siempre. Para la fila `r` del sprite, el píxel `x` (0-7, `x=0` a la izquierda) vale
`pix = bit (7-x) del campo 12+r`. Su color es `pix ? tinta : papel`, con `tinta = campo(4+r) >> 4` y
`papel = campo(4+r) & 15`. Se ve si el bit `(7-x)` del campo `20+r` (la máscara) está a 1.

Sin color (salida monocroma, `color_enable` a 0), el formato A se carga como tinta 15 y papel 0: el píxel
sale negro donde `pix` = 1.

### 1.3 Formato B (página ≠ 0)

El gráfico es una celda de 32 bytes de la sombra: 8 filas de 4 bytes, y en cada byte el nibble alto es el
píxel de la izquierda.

- **Dirección del byte `k` (0-3) de la fila `r`:** `(base << 13) | (página << 5) | (r << 2) | k`, con
  `base` = el `POKE 2129` latcheado.
- **Color del píxel:** el nibble es directamente el color, sin transparente fijo; el 0 es un color más.
- **Visibilidad:** el píxel se ve si el bit de máscara (campo `20+r`, bit `7-x`) está a 1 **y** el nibble no
  es el color transparente del sprite (nibble alto del campo 2).
- **Celdas compartidas:** varios sprites pueden usar la misma celda, y la máscara y el transparente son de
  cada sprite. Animar es cambiar la página.

### 1.4 Cómo sale el color

El motor da un índice de 4 bits `{BRIGHT,G,R,B}` por píxel (en A, la tinta o el papel de la fila) y lo pone
como tinta y papel a la vez.

| Modo | Cómo sale |
|---|---|
| Modos normales con color | ese índice es el color Chroma; con la paleta de 16 activa (`POKE 2067,1`), pasa por `pal16` |
| Modos 160 (8 y 4 bpp) | siempre por `pal16` |
| Monocromo | el píxel es el **bit 0** del índice: 1 = negro |

Es lo mismo que hacía `112c35e` con el B; lo nuevo es que el A también llega aquí ya resuelto a su color.

### 1.5 Sprites extra (`POKE 2131 = M`)

Son M sprites más que no están en las tablas sino en la propia hoja.

- **Dónde está su entrada:** la del extra `j` (0 a M-1) son los 32 bytes de la página `j` del bloque de las
  hojas, `(base << 13) | (j << 5) | campo`, con **los mismos campos** que un sprite normal (incluido el 29,
  que elige A o B).
- **Cómo se escriben:** el programa las escribe como RAM normal, sin `POKE 2100`.
- **Sin bit de activo:** el campo 0 no se mira, y con Y = 0 no se ven.
- **Cuidado:** un extra B no puede ser su propia celda.
- **Prioridad:** se miran **después del sprite normal 0**, del 0 al M-1, así que tienen menos prioridad que
  todos los normales. Entre ellos gana el de **número más bajo** (al revés que en los normales).

### 1.6 Prioridad y límite por línea

Orden de recorrido en cada línea: **normales del 63 al 0 y luego extra del 0 al M-1**. Se cargan en ese
orden en hasta **K = 12** huecos de línea. Cuando se pisan dos sprites, se ve el que se cargó antes (el
primero del orden).

Además del límite de 12 hay un **límite de tiempo**: el motor tiene un número de pasos por línea, y cada
sprite cuesta algo aunque no esté en esa línea. Hay que emularlo, o con muchos extra se verán sprites que en
la placa no salen (SPRXTRA lo enseña a propósito). Este modelo coincide con el RTL en todos los casos que se
han comparado (SPRXTRA en 32 columnas y en el modo 160, con 0, 32, 48 y 64 extra):

```
pasos = 2;  cargados = 0
para cada sprite s en el orden (normales 63..0, luego extra 0..M-1):
    pasos += 1                       # mirarlo cuesta un paso, este activo o no (los normales inactivos tambien)
    si s es normal y no esta activo: siguiente
    si la linea no corta s (dy = (Y_linea - Y_s) & 511 >= 8): siguiente
    coste = 6 si s es formato A, 8 si es formato B
    si cargados == 12 o pasos + coste > LIMITE: PARAR (este y todos los que faltan no salen en esta linea)
    pasos += coste;  cargar s;  cargados += 1
```

`LIMITE` vale **174** en la temporización de 32 columnas (HiRes, Spectrum, texto) y **171** en la de 80
columnas (los modos 160x100 y 160x200; en los modos de texto de 70 y 80 columnas no hay sprites). En el RTL
el motor se para cuando `pixel_cnt` llega a `limit` (120 en 32 columnas, 195 en 80). Si se para a mitad de
cargar un sprite, ese tampoco sale.

Lo que da: con 64 extra se garantizan **5 sprites B** por línea en las dos temporizaciones, y **7 A** en 32
columnas o **6 A** en 80. Con M = 0 y los 64 normales, los 12 siempre caben. Mientras el doble buffer copia
(`blit_busy`) el motor no lee, igual que antes.

### 1.7 Coordenadas en los modos 160

No cambian respecto a `112c35e`.

| Modo | Coordenadas del sprite |
|---|---|
| 8 bpp (160x100) | píxel de la imagen + 32 (X 32-191, Y 32-131); cada fila de sprite ocupa dos líneas de TV |
| 4 bpp (160x200) | Y = fila de la imagen de la ventana + 32 |

---

## 2. Scroll en anillo de los modos 160 (`POKE 2072-2075`)

El detalle completo está en `claude/planning/mode160_emulador.md`, **§6**. Resumen:

| POKE | Registro | Qué es |
|---|---|---|
| `2072` | `vofs` | fila de la imagen que sale en la primera fila de la pantalla |
| `2073` | `vring` | filas del anillo, `hc`. 0 = las visibles (100 en 8 bpp, `8*(win_last-win_first)` en 4 bpp); si no, al menos las visibles |
| `2074` | `hofs` | píxel de la imagen que sale en la primera columna |
| `2075` | `hring` | bytes por fila del anillo, `wc`. 0 = 160 (8 bpp) u 80 (4 bpp) |

- **Dirección de las filas:** la fila `f` del anillo empieza en `base + f*wc`, con `base = HFILE[15:13] << 13`.
  El anillo puede pasar de 16 KB.
- **8 bpp:** el píxel `x` (0-159) de la fila de pantalla `r` (0-99) es el byte
  `base + ((r + vofs) mod hc)*wc + ((x + hofs) mod wc)`.
- **4 bpp:** sea `q = (x + hofs) mod (2*wc)`. El píxel es el nibble de `base + ((r + vofs) mod hc)*wc + q/2`:
  el alto si `q` es par, el bajo si es impar. Aquí `r` es la fila de la imagen dentro de la ventana; un
  `hofs` impar desplaza medio byte.
- **Texto (4 bpp):** los códigos van en `base + hc*wc + fila*40 + columna`. Sin scroll es lo de antes,
  `base + (win_last-win_first)*640`. El texto no se mueve.
- **Sprites y borde:** no se mueven.
- **Sin latch:** el valor cuenta desde la línea siguiente a la escritura. Los programas lo escriben en el
  VSYNC, así que aplicarlo por línea o por cuadro da lo mismo en ellos.
- **Reset:** pone los cuatro a 0, que es el comportamiento de siempre.

---

## 3. Ficheros que hay que sincronizar

| Fichero | Qué cambia |
|---|---|
| ROM (`z80rom`, `/SYS/SDBOOST.ROM`) | Vuelven `LOAD *SPRCOL`, `*SPRPIX` y `*SPRMASK` (campos 4-27 por la ventana). `LOAD *SPRPAGE n,p` escribe en **2130**. El borrado de sprites del arranque también pone la página a 0. **El bloque 0 no cambia.** Se ensambla desde `master` (`z80rom`) o se copia `build/scroll160/SDBOOST.ROM` |
| `Arduino/SD81BoosterV2_039_STM32/DEBUGGER.cpp` | Ver 3.1 y 3.2. **Sin compilar en el STM32** todavía (lo compila el usuario); en el emulador se usa tal cual |
| `Arduino/Wifi_module_01/DEBUG_PAGE.h` | La web pinta A y B (con máscara y transparente) y los extra (byte 22 de la cabecera). No modela el límite de tiempo |

### 3.1 Snapshots

- `SPRITE n ...`: vuelve a ser **color, píxel y máscara** (campos 4-27), como antes de `112c35e`.
- **Línea nueva `SPRITE_PAGE n página transparente`** (en hexadecimal, detrás de su `SPRITE`). Solo sale si
  la página o el transparente no son 0. Al cargar, escribe 2130 y el nibble alto de 2103. Sin esa línea, la
  página es 0 (formato A).
- **Línea nueva `SPRITE_EXTRA m`**: el byte de `POKE 2131`. Solo sale si difiere del de la ROM y se carga
  escribiendo 2131. Las entradas de los extra no necesitan nada más: viven en la hoja, que ya va con la
  memoria y la sombra.
- `SPRITE_BASE` no cambia.

### 3.2 Captura del depurador

- **Lectura:** lee 3 bytes desde 2129 (2129, 2130 sin usar, 2131).
- **Cabecera:** el byte **22** lleva los extra; el 19 sigue siendo el bloque de la hoja.
- **Al cargar o capturar:** `shadow0_is_rom` ignora hasta 2131.
- **Limitación conocida (también en la placa):** el byte de 2131 en la sombra no se borra en el reset (el
  registro de la FPGA sí), así que un snapshot hecho tras un reset puede guardar un valor viejo.

---

## 4. Lo que falta también en el firmware (no es solo del emulador)

- **Los snapshots no guardan `POKE 2072-2075`** (el scroll de los modos 160). En `DEBUGGER.cpp` el rango de
  los POKEs de vídeo se salta. Hay que decidir una línea (por ejemplo `SCROLL160 vofs vring hofs hring`) y
  hacerla a la vez en el firmware y en el emulador.
- **Páginas de ayuda de la SD (`/MAN`)** para `*SPRPAGE` y `*SPRBASE`.
- **El manual** (lista en `claude/planning/manual_pendiente.md`).

---

## 5. Programas de prueba (todos en `EXAMPLES`, con su README de lo que se tiene que ver)

| Programa | Qué prueba | Notas |
|---|---|---|
| `SPRDBUF` | A y B en la misma línea, máscara, transparente, celda compartida, límite de 12, prioridad entre las dos tablas, 3 extra (Y=116) y el doble buffer | Modo Spectrum. Con el borde rojo (doble buffer apagado) la pelota desaparece en la parte de arriba **a propósito** |
| `SPRXTRA` | B con 4 bpp (arco iris animado de 15 colores), 64 normales y hasta 64 extra, el límite de tiempo (con 64 extra, en la fila de abajo solo se ven 6 de 15) | Modo 160x100 con `FONDO.JPG` (comando 76; sin él, fondo negro). E cambia M (64/0/32/48), O P Q A mueven el fondo con el scroll, S para, X sale |
| `SCRL160` | El scroll en los dos modos 160: anillo con una fila y una columna ocultas, medio byte en 4 bpp | Mundo de baldosas; Q A O P, S automático, M cambia de modo, X sale |

El README de SPRXTRA trae, franja a franja, cuántos sprites deben verse con 0, 32, 48 y 64 extra. Es la
mejor prueba del límite de tiempo del punto 1.6.
