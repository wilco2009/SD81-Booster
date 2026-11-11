# EXPLORER — explorador de archivos del SD81 Booster

Navega las carpetas de la SD, gestiona archivos (carpetas nuevas, borrar,
renombrar, copiar y mover), enseña archivos de texto, en hexadecimal y
pantallas `.SCR`, y devuelve al BASIC el archivo elegido para cargarlo o
editarlo.

Trabaja en modo **Superfast texto de 70 columnas** (`POKE 2045,173`), con
caracteres de 8×8 y un color por carácter (Chroma81 modo 1). La fuente es de
256 caracteres:

| Códigos | Contenido |
|---|---|
| 0-15 | caracteres propios: play, pausa, stop, las cuatro flechas y el disparo del joystick |
| 16-25 | los gráficos de bloque del ZX81 (1-10), copiados de la ROM al arrancar (visor hexadecimal en modo ZX81) |
| 32-127 | la fuente de Spectrum (`specfont.bin`) |
| 128-155 | el logo de la cabecera (`logo.inc`) |

## Pantalla

| Fila | Contenido |
|---|---|
| 0 | cabecera: el logo en rojo sobre negro, con la fecha y la hora en cian |
| 1-21 | el listado (`MAXVIS` filas). Con el panel de configuración abierto, la lista ocupa 46 columnas y el panel las 24 restantes |
| 22 | la carpeta actual y el filtro, si hay |
| 23 | barra de ayuda con las teclas más usadas |

El logo es el de la versión de 42 columnas (`bg_row0.bin`, columnas 9-22)
con cada píxel duplicado en horizontal: en 70 columnas el píxel dura la
mitad que en HiRes Spectrum, y así conserva su tamaño en pantalla. `logo.inc`
se genera a partir de `bg_row0.bin`. Los caracteres de 128 en adelante van
complementados, porque el hardware invierte los que llevan el bit 7.

## Teclas

La tecla **K** enseña esta misma lista en pantalla.

| Tecla | Acción |
|---|---|
| 6 / 7 | mover la selección abajo / arriba |
| 1 / 2 | página arriba / abajo |
| 8 / ENTER | entrar en la carpeta, o abrir el archivo (ver más abajo) |
| 5 | carpeta padre |
| `.` | filtro del listado (comodines); vacío, lo quita |
| SHIFT+1 | quitar el filtro |
| H | visor hexadecimal |
| L | ver cualquier archivo como texto (no solo los `.TXT`) |
| E | editar el archivo con el editor de textos (`EDIT.BIN`) |
| SHIFT+E | fichero de texto nuevo: pide el nombre y abre el editor, que lo crea al guardar |
| I | ver la dirección IP (`/MAN/IP.TXT`) |
| N | carpeta nueva |
| D | borrar (pide confirmación; las carpetas solo si están vacías) |
| R | renombrar |
| C / X | marcar para copiar / mover (su letra se pone en rojo en la barra) |
| V | pegar en la carpeta actual |
| S | abrir o cerrar el panel de configuración |
| K | pantalla de ayuda |
| ESPACIO | salir al BASIC |

Con el panel abierto:

| Tecla | Acción |
|---|---|
| W | WRX sí/no |
| M | MC45 sí/no |
| F | paginación completa sí/no |
| A | 64 / 128 caracteres. Se aplica al salir o al lanzar un programa: el explorador se ve en modo de 256 caracteres, y aplicarlo en el momento lo apagaría |
| J | teclas del joystick |
| T / Y / U | parar / pausa / seguir el VGM o PEB cargado |

En los visores: 6/7 línea, 1/2 página, ESPACIO vuelve. En el hexadecimal,
además, G salta a una dirección y Z alterna la columna de la derecha entre
ASCII y códigos del ZX81.

Al abrir un archivo con 8/ENTER:
- `.TXT` se abre en el visor de texto;
- `.SCR` se enseña a pantalla completa (el visor pasa un momento a HiRes
  Spectrum y al volver reconstruye la pantalla de texto);
- `.BMP`, `.JPG` / `.JPEG` y `.I16` se enseñan a pantalla completa en el modo de 160x100 a 256 colores
  (solo con la FPGA de la rama `fpga-mode160`): el explorador devuelve el nombre y el
  stub BASIC lanza `IMGVIEW.BIN` (`EXAMPLES/IMGVIEW`); un BMP o un JPG lo convierte el MCU al
  abrirlo, y F cambia cómo se coloca;
- `.VGM` y `.PEB` se cargan en el reproductor (T/Y/U en el panel);
- cualquier otro se devuelve al BASIC.

## Ratón

Con un ratón Bluetooth conectado (`LOAD *MOUSE`, `EXAMPLES/MOUSE`; necesita la FPGA con `sim_int 0.21`) el explorador enseña un cursor (el sprite 63) y reconoce, en el listado principal:

| Gesto | Equivale a |
|---|---|
| Girar la rueda | 6 / 7: una fila por unidad de giro (hacia delante, arriba) |
| Clic izquierdo en una fila | mover la selección hasta ella |
| Segundo clic izquierdo en la misma fila (menos de 25 fotogramas, contados con el VSYNC) | 8 / ENTER: abrir. Se atiende aunque la selección aún se esté moviendo: la apertura espera a que termine |
| Clic derecho | 5: carpeta padre |
| Clic en la barra de ayuda de abajo | la tecla de la zona pulsada: `8open` (abrir), `5up`, `Nnew`, `Ddel`, `Rren`, `Ccopy`, `Xcut`, `Vpaste`, `Eedit`, `Ssetup`, `Kkeys` y `SPC exit` (salir). Las acciones sobre un archivo (borrar, renombrar, copiar, mover, editar) actúan sobre la entrada seleccionada |

Cada gesto se convierte en los mismos códigos de tecla que da `read_key`, así que todo lo demás (desplazamiento, abrir, filtros, el panel) no cambia. En los visores y los diálogos el ratón solo mueve el cursor. Sin ratón conectado el sprite queda apagado y no cambia nada. Con el panel abierto, los clics sobre él se ignoran.

Implementación: `mouse_init` y `mouse_poll` (justo antes de los datos), con el gancho al principio de `read_key` y el apagado del cursor en `video_off`. Añade unos 560 bytes de código (termina en `$7F6D`, a 147 bytes del límite de `$8000`). Para hacer sitio, los textos de la ayuda (`help_main`, `help_back`, `help_viewer`, `help_hex`, 320 bytes de datos) se movieron de entre el código a la zona de datos.

**Clic y doble clic.** Un clic en otra fila no mueve la selección fila a fila con `vt_down` / `vt_up` (tres consultas al MCU por fila): `vt_msel` salta directamente a ella, con una sola consulta y cambiando solo el color de la fila vieja y de la nueva. Mientras el MCU atiende al Z80, la FPGA no recibe el estado del ratón, y con la ráfaga de consultas el segundo clic de un doble clic se perdía (el tercero, ya en reposo, sí llegaba). El reloj de la cabecera se refresca ahora cada 700 pasadas de `rk_wait` en vez de 2000, porque cada pasada tarda más con la lectura del ratón.

**Coordenadas.** Con los sprites en 70 columnas (columna 0 = X 32, 4 píxeles de sprite por columna, 280 de ancho), el ratón (X de 0 a 255; la FPGA solo guarda 8 bits) no llegaría a las columnas 64-69. Por eso el cursor se coloca en `xs = x·35/32` (255 → 277): `x + x/16 + x/32`. Y no se escala (24 filas de 8 líneas = 192). La columna de texto del puntero (`ms_col` = `xs/4`, 0-69) sirve para ignorar los clics sobre el panel (columna 46 en adelante) y para elegir la zona de la barra de ayuda (`ms_bar`: columna de inicio y código de tecla de cada acción, según cómo pinta `draw_help` la cadena `help_main`: si se cambia esa cadena, hay que cambiar también la tabla).

Sin probar en la placa. A comprobar: que el cursor cae sobre la fila y la columna que se pulsan, que la rueda gira en el sentido esperado (si va al revés, se cambian el 4 y el 3 en `mp_w1`) y el refresco del reloj.

## Diálogos

Carpeta nueva, renombrar, borrar, filtro, joystick e ir a una dirección
salen en un recuadro sobre la pantalla, con el título, el campo de texto y
las teclas que se pueden usar. Mientras se escribe:

| Tecla | Acción |
|---|---|
| A-Z, 0-9, `.` | el carácter |
| SHIFT+Z/X/C/V | `:` `;` `?` `/` |
| SHIFT+J/K/L | `-` `+` `=` |
| SHIFT+N/M/. | `<` `>` `,` |
| SHIFT+B | `*` |
| SHIFT+ESPACIO | `£` |
| SHIFT+5 / SHIFT+8 | cursor izquierda / derecha |
| SHIFT+0 | borrar a la izquierda |
| SHIFT+1 | volver al nombre original (al renombrar) |
| ENTER | aceptar |
| ESPACIO | cancelar (en el del joystick, ESPACIO es una tecla válida) |

## Cómo se devuelve un archivo al BASIC

El explorador es código máquina suelto (`LOAD ... CODE`) y no carga nada
por su cuenta: copia el nombre del archivo, ya en códigos del ZX81, a
`retname` (**24579**, `ORG`+3), deja el vídeo como estaba (vídeo nativo, I
de la ROM, el modo CHR elegido y el bloque 7 en su página) y vuelve con
`RET`. `USR` devuelve:

| Valor | Significado |
|---|---|
| 0 | se ha salido con ESPACIO |
| N (1-255) | abrir el archivo de N caracteres |
| N + 256 | editarlo (tecla E) |

El stub BASIC (`SD Content/explorer/EXPLORER.B81`, y su `.P`; el mismo programa va en
`AUTOEXEC.P`) monta `F$` con el nombre y decide qué hacer según la extensión. Con la tecla E salta a
la línea 2000, que carga `/EXPLORER/EDIT.BIN` en la misma dirección y lo
arranca; el editor lee el nombre de `F$`. Al salir de cualquiera de los dos,
`GOTO 10` vuelve al explorador.

## Memoria

| Dirección | Contenido |
|---|---|
| `$6000-` | el programa. **El código tiene que quedar por debajo de `$8000`**; los datos (variables, textos, fuente y logo) pueden ir por encima |
| `$E000-$E6A8` | la pantalla: 1 byte inicial y 24 filas de 71 (70 caracteres y relleno), con la dirección de pantalla alternativa (`POKE 2096-2098`) |
| `$E800-$EEA8` | los atributos, con la misma geometría (`POKE 2059-2061`) |
| `$F000-$F7FF` | la fuente (I = `$F0`) |
| `$FB00-` | buffer de nombres y de los visores (`namebuf`) |

Los cuatro últimos están en el bloque 7, que el explorador mapea a la página
8 mientras funciona (`VIDBLOCK`/`VIDPAGE`) y deja en su página (la 7) al
salir. El visor de `.SCR` usa el bloque entero como bitmap Spectrum.

## Ensamblar

```
pasmo explorer.asm explorer.bin
```

`compila.bat` lo ensambla y lo copia a la carpeta `explorer` de la SD del
emulador. En la SD va en `/EXPLORER/EXPLORER.BIN`.
