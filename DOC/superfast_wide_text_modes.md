# Modos Superfast de texto ancho (70 y 80 columnas) — especificación

Especificación de los dos modos de texto ancho añadidos al SD81 Booster,
pensada para poder implementarlos en un emulador sin acceso al RTL.
Todos los valores están tomados de `FPGA/SD81V2.1000/SD81.v` (rama
`feature/superfast-80col`) y verificados sobre hardware real.

Ambos modos están **validados en hardware**: se ven las 70/80 columnas
completas, con las cuatro esquinas de la rejilla y los dos modos de color
de Chroma funcionando.

---

## 1. Resumen

| | 70 columnas | 80 columnas |
|---|---|---|
| Activación | `POKE 2045,173` | `POKE 2045,174` |
| Ancho de carácter | 8 píxeles | **7 píxeles** |
| Columnas | 70 | 80 |
| Filas | 24 | 24 |
| Alto de carácter | 8 líneas | 8 líneas |
| Paso de fila en el DFILE | 71 bytes | 81 bytes |
| Ancho activo | 70×8 = **560 px** | 80×7 = **560 px** |

Los dos ocupan **exactamente el mismo ancho físico**, así que comparten
toda la temporización y al alternar entre ellos la imagen no cambia de
posición ni de tamaño: solo cambia cuántas columnas caben dentro.

`POKE 2045,85` vuelve al modo nativo. Los valores 170/171/172 son los
modos Superfast que ya existían (texto 32 columnas, HiRes nativo y HiRes
Spectrum) y no se han tocado.

**Por qué 7 píxeles para las 80 columnas**: 80 caracteres de 8 píxeles
serían 640 ciclos a 13 MHz = 49,2 µs de imagen activa, y en un televisor
real solo se ven unos 620 ciclos (~77,5 columnas). 560 ciclos son 43 µs,
con margen de sobra para el overscan.

---

## 2. Temporización

En modo ancho el reloj de píxel pasa de 6,5 MHz a **13 MHz**
(`clk6_5 = sf80_en ? cnt26[0] : cnt26[1]`, es decir 26 MHz dividido entre
2 en vez de entre 4). La duración total de la línea **no cambia**: son
828 ciclos a 13 MHz, el doble exacto de los 414 a 6,5 MHz.

El contador de píxel (`pixel_cnt`) va de 0 a **827** y se reparte así:

| Zona | `pixel_cnt` | ciclos | µs |
|---|---|---|---|
| Front porch | 0 – 21 | 22 | 1,7 |
| **HSYNC** | 22 – 85 | 64 | 4,9 |
| **Back porch** | 86 – 153 | 68 | 5,2 |
| Zona útil | 154 – 827 | 674 | 51,8 |

En el RTL las comparaciones se hacen sobre `HSYNCcnt = pixel_cnt[9:1]`
(la mitad), de ahí que aparezcan como `HSYNCcnt 11-42` para el sync y
`43-76` para el back porch.

> **Nota**: el sync está *adelantado* respecto al modo de 32 columnas. Ese
> modo deja 32 ciclos de front porch (4,9 µs a 6,5 MHz), tres veces lo que
> pide PAL (1,65 µs); al doblarlo para el modo ancho salían 64 ciclos
> desperdiciados y la zona útil se quedaba en 632 ciclos, menos que los
> necesarios. Adelantando el sync se recuperan 42 ciclos sin alterar la
> duración de la línea.

### Ventana activa

```
SCR_START_X_80 = 202 + (POKE 2094) * 8
SCR_END_X_80   = SCR_START_X_80 + N * W - 1
      donde  W = 7,  N = 81 - (POKE 2095)     en 80 columnas
             W = 8,  N = 71 - (POKE 2095)     en 70 columnas
```

**202 está medido sobre hardware** (un televisor Samsung) para que la
imagen quede centrada con `POKE 2094,0`. La zona útil empieza en 154,
pero un televisor no muestra los primeros ciclos.

`202` debe cumplir `202 mod 8 = 2`, la misma congruencia que
`SCR_START_X = 122` del modo de 32 columnas — ver §6.

### El grupo de más

Fíjate en que **N es una unidad mayor que el número de columnas** (81
grupos para 80 columnas, 71 para 70). No es un error: la captura del
primer grupo de cada fila se pierde en el calentamiento del pipeline,
porque su pulso de carga cae antes del umbral `SCR_START_X_80 + 12`. Es
la misma razón por la que el modo de 32 columnas reserva 33 grupos
(`SCR_END_X = SCR_START_X + 33*8 - 1`). Sin ese grupo extra, la última
columna no llega a dibujarse nunca, se encuadre como se encuadre.

### Filas

Sin cambios respecto al resto de modos Superfast:

```
SCR_START_Y = 62
SCR_END_Y   = 62 + 191 = 253      (24 filas × 8 líneas = 192)
```

---

## 3. Memoria de pantalla (DFILE)

```
dirección del carácter (fila, col) = DFILE + 1 + fila * PASO + col

    PASO = 81   en 80 columnas
    PASO = 71   en 70 columnas
```

Dos detalles importantes:

- **El `+1` es real**: el primer byte de la zona apuntada por la variable
  de sistema `D_FILE` (16396/16397) **no** es la columna 0 de la fila 0,
  se salta. Es la misma convención del modo de 32 columnas.
- **El último byte de cada fila no se usa.** El paso es una unidad mayor
  que el número de columnas (81 = 80+1, 71 = 70+1). Ese byte sobrante es
  el equivalente al NEWLINE (`$76`) del DFILE estándar: el hardware no lo
  trata de forma especial, simplemente no lo dibuja. Eso permite que las
  rutinas de la ROM sigan colocando ahí su NEWLINE.

El tamaño total es 24 × 81 = 1944 bytes (80 col) o 24 × 71 = 1704 (70 col).

---

## 4. Juego de caracteres

La tabla de dibujo se direcciona con el registro **`I`** de la CPU, que el
hardware captura en cada ciclo de refresco. Vale cualquier ubicación,
incluida la propia ROM en `$1E00`.

```
dirección del bitmap = (I << 8) + código * 8 + línea
```

Con los modos de 128/256 caracteres (`SEL_128CHARS`/`SEL_256CHARS`) la
tabla se alinea a 1K/2K y usa también los bits 7 y 6 del código.

### Los 7 píxeles: qué se pierde exactamente

El registro de desplazamiento emite el **bit 7 primero** y va desplazando
a la izquierda. Con 7 ciclos por carácter emite los bits **7..1**, y se
pierde el **bit 0** (la columna de píxeles de la derecha).

Esto tiene dos consecuencias que conviene conocer para reproducirlo bien:

1. **Ningún carácter de texto del ZX81 pierde un solo píxel**: en el
   charset estándar la columna del bit 0 está vacía en todos ellos. Los
   gráficos de bloque sí la usan, pero como son sólidos de borde a borde,
   el carácter siguiente arranca con su bit 7 encendido y la continuidad
   se mantiene sin hueco.
2. **La separación entre caracteres la aporta el bit 7** (la columna
   izquierda), que está vacía en todo el charset **salvo en la `T` y la
   `Y`**, las dos únicas de 7 píxeles de ancho. Sin corregirlas se pegan
   al carácter anterior en 32 combinaciones (`ET`, `HT`, `MY`, `TT`…).

   El software las estrecha a 6 píxeles quitándoles la columna izquierda:

   ```
   T:  00 7C 10 10 10 10 10 00     ; barra en bits 6..2, palo en el bit 4
   Y:  00 44 28 10 10 10 10 00     ; brazos en bits 6 y 2, hacia el bit 4
   ```

   Esto es cosa del software, no del hardware: el emulador no tiene que
   hacer nada, solo dibujar 7 píxeles.

---

## 5. Color (Chroma)

Los dos modos de Chroma funcionan en modo ancho. Se seleccionan por el
puerto **`$7FEF`**: bit 5 = color activo, bit 4 = modo, bits 3-0 = color
del borde.

**Modo 0 — por código de carácter** (bit 4 = 0)

```
dirección del atributo = $C000 + código * 8 + línea
```

No depende de la geometría de pantalla, así que es idéntico al del modo de
32 columnas. Con 256 caracteres la tabla ocupa 2K (`$C000-$C7FF`); con
64/128, 1K (`$C000-$C3FF`).

**Modo 1 — por posición** (bit 4 = 1)

```
dirección del atributo = $8000 | (dirección del carácter & $7FFF)
```

Es decir, **la misma dirección del carácter con el bit 15 forzado a 1**,
con el paso de fila que corresponda (81 o 71). Para un DFILE en `$6100`,
sus atributos están en `$E100`.

Formato del byte de atributo: **nibble alto = papel, nibble bajo = tinta**.

---

## 6. Detalles de implementación que no son evidentes

Estos puntos costaron encontrarlos en hardware y conviene tenerlos en
cuenta si se quiere reproducir el comportamiento exacto:

**Congruencia de fase.** El contador que marca la cadencia de búsqueda
(`col_cnt_b`) está anclado a `SCR_START_X` (122) **módulo 8, globalmente
para todos los submodos** — no se reinicia por modo. Por eso
`SCR_START_X_80` debe cumplir `mod 8 = 2`, igual que `122 mod 8 = 2`. Los
desplazamientos de `POKE 2094` van en pasos de 8, así que preservan la
congruencia sea cual sea el valor.

**Cadencia de 7 pasos.** En 8 píxeles la máquina de búsqueda recorre 8
estados y el índice de columna sale de dividir por 8 (ambas cosas gratis
por truncamiento de bits). Con 7 píxeles no hay truncamiento que valga:
hace falta un contador explícito de 0 a 6 y otro de columna. Los estados
con contenido son exactamente siete —el octavo está vacío—, así que la
secuencia de accesos a memoria es la misma, solo que sin el hueco.

**Retardo del pipeline.** Entre que se lanza la búsqueda de un carácter y
que su primer píxel sale por el pin hay un retardo fijo. En el RTL se
compensa con dos constantes sobre la ventana activa:

| | primer píxel | último píxel |
|---|---|---|
| 8 píxeles | `SCR_START_X_80 + 20` | `SCR_END_X_80 + 13` |
| 7 píxeles | `SCR_START_X_80 + 18` | `SCR_END_X_80 + 12` |

En ambos casos la ventana resultante mide exactamente 560 píxeles. Un
emulador que dibuje directamente desde el DFILE no necesita reproducir
este retardo; se documenta por si se compara con capturas reales.

---

## 7. Registros de ajuste

| POKE | Rango | Función |
|---|---|---|
| 2094 | 0-15 | Desplaza el inicio de la ventana, en pasos de 1 carácter (8 ciclos). Útil de 0 a 7 antes de salirse por la derecha. Solo desplaza hacia la derecha |
| 2095 | 0-15 | Recorta el ancho activo en N caracteres |

Ambos se escriben como POKE normales; el hardware los intercepta. Con la
calibración actual (`SCR_START_X_80 = 202`) el valor bueno es **0** en los
dos.

---

## 8. Lo que todavía no está cubierto

- **Sprites**: sin comprobar en modo ancho.
- **Scroll horizontal fino** (`POKE 2090`): sigue anclado a la geometría
  de 32 columnas, no funciona en modo ancho.
- **Rutinas de la ROM**: `PRINT`, `PRINT AT`, `CLS`, `SCROLL` y `TAB`
  siguen asumiendo 32 columnas. Se está trabajando en unos comandos
  `LOAD *80COL` / `*70COL` / `*32COL` que parcheen en caliente las 8
  constantes de ancho de la ROM (que vive en RAM). Mientras tanto, el modo
  ancho solo es utilizable desde código propio que escriba directamente en
  el DFILE.
