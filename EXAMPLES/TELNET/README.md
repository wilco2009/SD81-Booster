# TELNET — terminal telnet para el ZX81

Terminal para conectarse a BBS y servidores telnet desde el ZX81, a través
del módulo WiFi del SD81 Booster. Pantalla de 80×24 en Superfast, con
caracteres de 7 píxeles, la fuente CP437 de 256 caracteres y colores ANSI.

Está hecha a partir del terminal de CP/M Plus (`CPM3_SD81`):

| De CP/M | Qué es |
|---|---|
| `term.z80` | el bucle: `NET_READ` de hasta 128 bytes, pintarlos, mirar el teclado y mandar cada tecla con `NET_WRITE` |
| `chario.z80` | el emulador de terminal (ADM-3A y ANSI: CUP, CUU/CUD/CUF/CUB, ED, EL, SGR, SCP/RCP, DSR), el teclado del ZX81 y el cursor parpadeante. En CP/M es el CONOUT/CONIN del BIOS; aquí se llama directamente, sin cambios de lógica |
| `system.z80` | la puesta en marcha del vídeo |
| `font_image.z80` | la fuente (`font.inc`) |

Lo único nuevo es el tratamiento de la **negociación telnet (IAC)**. Ni el
ESP32 ni el TERM de CP/M la tratan, y muchas BBS la mandan al conectar.
Sin filtrarla aparecen bytes raros en pantalla. La terminal acepta ECHO y
SUPPRESS-GO-AHEAD, que es lo que espera una BBS (el eco lo hace ella), y
rechaza el resto de opciones. Las subnegociaciones se saltan enteras.

## Ensamblar y cargar

```
pasmo telnet.asm TELNET.BIN
```

`compila.bat` lo ensambla y lo copia a `SD81\TELNET` en el emulador.
Copia `TELNET.BIN` y el stub BASIC (`TELNET.B81`, pásalo a `.P`) a la
misma carpeta de la SD. El stub baja RAMTOP (`CLEAR 24575`), carga el
programa en 24576 y lo arranca con `RAND USR 24576`. Al salir, la
terminal devuelve el vídeo tal como estaba y vuelve al BASIC.

## Uso

Funciona como un módem Hayes: los comandos AT se teclean en la propia
terminal.

| Comando | Qué hace |
|---|---|
| `ATDT host:puerto` | conecta (por ejemplo `ATDT bbs.ejemplo.org:23`). Responde `CONNECT` o `NO CARRIER` |
| `ATH` | cuelga |
| `+++` | vuelve a modo comando sin colgar (con 1 s de silencio antes y después) |
| `ATO` | vuelve a la conexión |

Al arrancar, si no hay conexión, activa el eco del módem (`ATE1`). Así se
ve lo que se teclea aunque otro programa lo hubiera dejado apagado.

| Tecla | Qué hace |
|---|---|
| ENTER+0 | salir (cuelga si hay conexión) |
| ENTER+9 | eco local sí/no, para cuando el otro lado no hace eco; arranca apagado |
| SHIFT+1 | ESC |
| SHIFT+ENTER y luego una tecla | CTRL+tecla (el cursor pasa a subrayado mientras espera) |
| ENTER+tecla | símbolos: los serigrafiados, más `@ \ \| ~ \` { } [ ] _ ! # % &` |
| SHIFT+0 / SHIFT+9 | DEL / BS |
| SHIFT+. | TAB |
| SHIFT+5/6/7/8 | flechas WordStar (^S ^X ^E ^D) |

El teclado es el mismo que el de CP/M (ver "Tabla de teclado de CP/M" en
el manual).

## Memoria

Funciona en modo 32K y en 48K:

| Dirección | Contenido |
|---|---|
| `$6000-` | el programa (con la fuente dentro, que se copia arriba al arrancar) |
| `$8000-$8798` | la pantalla: 1 byte inicial y 24 filas de 81 (80 caracteres y relleno), mostrada con la dirección de pantalla alternativa (`POKE 2096-2098`) |
| `$8800-$8F98` | los atributos, con la misma geometría (`POKE 2059-2061`) |
| `$9000-$97FF` | la fuente. El modo de 256 caracteres pide 2 KB alineados, e I = `$90` |

Machaca lo que hubiera en esas direcciones de los bloques 4 y 5.

La terminal funciona en FAST: el vídeo lo genera la FPGA, y el parpadeo
del cursor y las esperas cuentan los VSYNC del puerto `$AF`.
