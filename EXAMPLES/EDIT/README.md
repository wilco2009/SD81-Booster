# EDIT — editor de textos para el ZX81

Editor de ficheros de texto ASCII para el SD81 Booster: 80×24 en Superfast
(caracteres de 7 píxeles), fuente CP437 de 256 caracteres y teclas de
WordStar. La pantalla y el teclado son los de TELNET (y los del CP/M).

## Cómo se abre

El editor es código máquina: se carga con `LOAD ... CODE` y se arranca con
`RAND USR`. Como no se carga ningún programa BASIC, las variables siguen
ahí, y el fichero se le pasa en **`F$`**:

| `F$` | Qué hace |
|---|---|
| existe y el fichero también | lo abre |
| existe pero el fichero no | empieza uno nuevo con ese nombre |
| no existe, o está vacía | empieza uno sin nombre, y lo pide al guardar |

**Desde el explorador:** tecla **E** sobre un fichero. El explorador vuelve
al BASIC con el nombre, igual que al abrir un fichero, pero `USR` devuelve
la longitud + 256. Su stub lo reconoce (línea 32), monta `F$` como siempre y
salta a la línea 2000, que carga `/EXPLORER/EDIT.BIN` y lo arranca. Al salir
del editor, `GOTO 10` vuelve al explorador.

**Desde el prompt:** con el stub `EDIT.B81` (o su `.P`):

```
LOAD "EDIT.B81"
LET F$="NOTAS.TXT"
GOTO 10
```

`GOTO`, no `RUN`: `RUN` borra las variables, y con ellas `F$`.

## Teclas

SHIFT+ENTER y luego una letra es CTRL+letra (el cursor pasa a subrayado
mientras espera). Las teclas se repiten si se mantienen pulsadas, salvo
ENTER (que solo actúa al soltarla, porque también es modificador).

| Tecla | Qué hace |
|---|---|
| SHIFT+5/6/7/8, o ^S ^X ^E ^D | cursor izquierda / abajo / arriba / derecha |
| ^A / ^F | palabra anterior / siguiente |
| ^R / ^C | página arriba / abajo |
| ^Q S / ^Q D | principio / final de la línea |
| ^Q R / ^Q C | principio / final del texto |
| SHIFT+0, SHIFT+9 | borrar a la izquierda |
| ^G | borrar a la derecha |
| ^Y | borrar la línea |
| SHIFT+. | tabulador |
| ENTER+tecla | símbolos (los serigrafiados, y `@ \ \| ~ \` { } [ ] _ ! # % &`) |
| ^K S | guardar |
| ^K X, ^K D | guardar y salir |
| ^K Q, ENTER+0 | salir (pregunta si hay cambios sin guardar) |
| ENTER+9, ^J | pantalla de ayuda: el teclado del ZX81 con lo que hace cada tecla (sola, con SHIFT y con ENTER) y las órdenes |
| SHIFT+1 (ESC) | cancela ^K / ^Q y las preguntas |

La última fila muestra el nombre (con `*` si hay cambios), la línea, la
columna y cómo abrir la ayuda (ENTER+9). Los tabuladores se ven hasta la siguiente
columna múltiplo de 8. Las líneas de más de 80 columnas se desplazan en
horizontal.

## Ratón

Con un ratón Bluetooth conectado (`LOAD *MOUSE`, `EXAMPLES/MOUSE`; necesita la FPGA con `sim_int 0.21` y los sprites en 80 columnas) el editor enseña un cursor (el sprite 63) y reconoce:

| Gesto | Qué hace |
|---|---|
| Clic izquierdo en el texto | pone el cursor ahí (cuenta los tabuladores y el desplazamiento horizontal; más allá del final de la línea o del texto, se queda en el final) |
| Rueda | mueve el cursor una línea por unidad de giro (hacia delante, arriba) |
| Clic en `ENTER+9: keys & commands` (la última fila, que ahora se ve como un botón) | abre la pantalla de ayuda |
| Clic o rueda en la pantalla de ayuda | vuelve al texto |

Los gestos se convierten en códigos de tecla que entrega `kbget`: `KEY_UP` y `KEY_DOWN` para la rueda, `K_HELP` para el botón de la ayuda y `K_MOUSE` (el código 0, que ninguna tecla genera) para el clic en el texto, que atiende `k_mouse`. El ratón se lee en `wait_scan`, junto al parpadeo del cursor. En las preguntas de la última fila (guardar, ¿Y/N?) el ratón solo mueve el cursor.

La posición se escala: el área de texto mide 280 píxeles de sprite de ancho y el ratón solo llega a X = 255, así que el cursor se coloca en `x·35/32` (la columna de texto es `x·5/16`, de 0 a 79). Y es `y/8`.

Sin probar en la placa. A comprobar: que el cursor cae sobre el carácter que se pulsa en las 80 columnas, con tabuladores y con texto desplazado en horizontal, y el sentido de la rueda (si va al revés, se cambian `KEY_UP` y `KEY_DOWN` en `mo_w1`).

## Ficheros

- Se lee y se escribe con los comandos de ficheros del MCU: 53 (fopen),
  59 (fstat), 55 (fread), 71 (fcreate), 56 (fwrite) y 57 (fclose), en
  trozos de 512 bytes. No mira la extensión, así que sirve también para
  los listados `.B81`.
- **fcreate (71) es del firmware 2.6.** Con uno anterior se puede abrir y
  editar, pero al guardar sale "Can't create the file".
- Si el fichero trae CRLF, se edita con LF y se vuelve a guardar con CRLF.
  Un CR suelto se queda como está y se ve como un cuadradito.
- Tamaño máximo: 26623 bytes.

## Memoria

| Dirección | Contenido |
|---|---|
| `$6000-$7F31` | el programa (con la fuente, que se copia a `$9000`, y el búfer de ficheros). **Todo debe quedar por debajo de `$8000`**, donde empieza la pantalla: quedan 207 bytes |
| `$8000-$8798` | la pantalla: 1 byte inicial y 24 filas de 81 |
| `$8800-$8F98` | los atributos, con la misma geometría |
| `$9000-$97FF` | la fuente (I = `$90`) |
| `$9800-$FFFE` | el texto |

Los bloques 6 y 7 (`$C000-$FFFF`) son por defecto un espejo de la RAM del
sistema. Mientras se edita se mapean a las páginas 8 y 9, y al salir vuelven
a su página (6 y 7), como hace el explorador con el bloque 7.

El texto es un *gap buffer*, y todo el acceso pasa por las rutinas `tb_*` e
`it_*`, además de la carga y el guardado. Para editar ficheros más grandes
(paginando más páginas de la RAM del SD81) basta con cambiar esas rutinas.

## Ensamblar

```
pasmo edit.asm edit.bin
```

`compila.bat` lo ensambla y lo copia a la carpeta `explorer` de la SD del
emulador. En la SD va en `/EXPLORER/EDIT.BIN`, junto al explorador.
