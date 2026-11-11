# Manual de usuario: lo que falta añadir o corregir

Lista viva (empezada el 14 de octubre de 2026). Se tacha lo que se hace en los dos manuales
(`MANUAL/ES/SD81_Manual_ES.docx` y `MANUAL/EN/SD81_Manual_EN.docx`: docx primero, ver la nota de memoria
`reference_manual_docx`). Cada punto lleva dónde iría y de qué depende.

## Hecho el 17 de octubre de 2026 (ES y EN, `.docx` y `.md`)

| # | Qué se hizo | Dónde quedó |
|---|---|---|
| 1, 2 | Comandos AT de red (MQTT, SHA-256, HMAC, aleatorios), estados y errores, `/SYS/NET.CFG` | Apéndice M nuevo (Red para programadores) |
| 3 | Juegos por turnos en red: cómo se juega (sala, código, volver, sala llena, relojes), el 4 en raya, el ajedrez, NETCHAT; y la librería `net.inc` / el controlador `game.inc` | sección **9.9** (usuario) y Apéndice M (programadores) |
| 4 | `snap` guarda las mapeadas más las escritas desde el último reset; `snap -a`, todas | Apéndices K y L |
| 5 | CP/M y el depurador | subsección «CP/M y el depurador» al final del Apéndice I, y una limitación en el L |
| 6 | `LOAD *Z81` sin el comando 70: exige el monitor; sin él, informe P | 7.7 (texto y aviso), sección 14 (fila P), tabla de comandos del MCU (el 70 pasa a ser el 75) |
| 7 | Web del depurador: abrirla con la IP, aviso rojo, botones Symbols y Trace, `sym !`, símbolos que no se releen solos | Apéndice L (Desde el navegador, Símbolos y la tabla de la consola) |
| 8 | `ESP32FW.BIN` (sin guion bajo), compatibilidad con módulos viejos y actualización conjunta | 9.7 |
| 9 | **Scrabble en red** | pasado al manual el 5 de noviembre (ver más abajo) |

## Hecho el 5 de noviembre de 2026 (ES y EN, `.docx` y `.md`, con el índice actualizado con Word)

| Qué se hizo | Dónde quedó |
|---|---|
| **Scrabble en red** (inglés y castellano): cómo se juega, tabla de avisos, reparto y valores de las fichas, diccionarios (no incluidos, con los enlaces), `SCRABBLE.SAV`; captura `scrabble_game.png` | sección 9.9 |
| **SPORTS** (Tennis, Hockey, Squash y Practice, en tiempo real) | sección 9.9 |
| La sección 9.9 pasa a llamarse **«Juegos en red»** (SPORTS no es por turnos) y las carpetas de los juegos son `SD81/NETGAMES/` (el manual decía `SD81/TOOLS/`) | 9.9 |
| `game.inc` con jugadas de longitud variable y estado grande (`G_MOVE_VAR`, `G_BIG`), el Scrabble como ejemplo, la capa sin esperas `netq.inc` y `EXAMPLES/NETLAT` | Apéndice M |
| El Scrabble usa tres sprites y hace parpadear el cursor con la máscara a 0 | Apéndice H |
| El joystick ya no pausa el depurador (FPGA `sim_int` rev 0.18) | Apéndices K y L |
| **Scroll por hardware** (HiRes nativo y Spectrum): puerto `$3FEF`, región, búferes, órdenes del motor | Apéndice F, «Scroll por hardware»; una nota en el Apéndice L |
| **Modo 160x100 a 256 colores**, **paleta de 16 colores**, **modo 160x200 a 4 bpp** (ventana, sprites y texto 40x25) y **visor de imágenes** (`.I16`, BMP por el comando 76, PHOTO e IMGVIEW) | Apéndice F (cuatro secciones nuevas) y la tabla de POKEs (2045 = 175 y 176, 2063-2071, 2099) |
| `.BMP` y `.I16` en el explorador | Apéndice J (fila nueva de la tabla de extensiones) |
| `LOAD`, `SAVE` y `LOAD THEN PRINT` resuelven `..` en las rutas | 8.2 |
| Tabla de comandos del MCU: 13, 31, 60-64, 71-72, 73-74 y 76, y una nota con los códigos reservados (22, 39, 51, 70) y los desactivados (46, 47) | 15.5 |

Los cinco apartados que dependen de la FPGA nueva (scroll, 256 colores, paleta de 16, 160x200 y visor) llevan la frase «Esta función necesita la FPGA de la próxima
versión (todavía sin publicar)»: **quitarla al publicar la FPGA**. El `PONG` de `EXAMPLES/NETGAME` no va al manual: es el antecesor del Tennis de SPORTS y una prueba de latencia.

## Hecho el 7 de noviembre de 2026 (ES y EN, `.docx` y `.md`, índice actualizado con Word)

- Apéndice H: los dos formatos de sprite (`POKE 2130`, página 0 = A), el formato B con hoja (`POKE 2129`, máscara en los dos formatos, color transparente en los bits 4-7 de `POKE 2103`), los sprites extra (`POKE 2131`), el límite de tiempo por línea, `LOAD *SPRPAGE` y `LOAD *SPRBASE`, la prioridad de los extra y los ejemplos SPRDBUF y SPRXTRA. Tabla de direcciones hasta 2131.
- Apéndice F: sprites en el 160x100, apartado nuevo «Scroll en anillo de los modos de 160 píxeles» (`POKE 2072-2075`, ejemplos SCRL160 y SPRXTRA), filas nuevas en la tabla de POKEs y los límites de los dos modos corregidos.
- Rangos de `LOAD *ROMLOCK` corregidos (7.5, la orden 68 y el Apéndice F): 2041-2075, 2090-2099 y los sprites 2100-2131. La captura de la web dibuja los dos formatos y los extra, sin el límite de tiempo.

## Release v1.8.0 (7 de noviembre de 2026, ES y EN)

- Fila 1.8 del historial (STM32 2.8, ROM 1.9, FPGA 1.7, WiFi 1.5); fuera los seis avisos «necesita la FPGA de la próxima versión».
- JPG en el visor (Apéndice F, título «PHOTO, IMGVIEW, BMP y JPG»), en el comando 76 y en la tabla del explorador.
- Ayuda de la SD: `SPRPAGE.txt` y `SPRBASE.txt`, citadas en `HELP.txt`.

## Pendiente

- **Ratón Bluetooth** (`LOAD *MOUSE`, puerto índice 20, comando 77): texto ya redactado en la sección «Ratón Bluetooth: texto listo para el manual» (más abajo); falta pasarlo al `.docx` ES y EN (9.10, fila 77 en 15.5 y Apéndice N).
- Sin probar en la placa (y el manual lo describe como si funcionara): el MQTT avanzado (TLS, usuario y contraseña, QoS 1, retenidos, testamento), el comando 76 y los BMP,
  la paleta de 16, el modo 160x200 y el scroll vertical. Revisar el texto si la prueba da otra cosa.
- Capturas de pantalla de los modos de 160 píxeles (no hay ninguna) y, si se quiere, `DOC/img/scrabble_start.png` (la pantalla de inicio del Scrabble).
- Para las **notas de la release** (no van al manual): el tempo del VGM (la música iba lenta por retrasos acumulados, arreglado), el sondeo del ESP32 cada 3 ms con datos y 15 ms en reposo, y los comandos 66 y 67 sin log por USB.
- Si cambia el aspecto del 4 en raya tras probarlo en el emulador, rehacer la captura (`DOC/img/connect4_win.png`).

- **ESTELA (matamarcianos del modo de 256 colores), para la próxima release:** el texto de la sección 13.7 y las líneas de los Apéndices F y H están listos en `claude/planning/manual_estela.md`, con lo que hay que comprobar al empaquetar (carpeta en Software.zip, versión mínima de la FPGA) y las capturas que faltan.

- **Sprites y scroll de los modos 160 (lo que queda tras el 7/11/2026):** los snapshots no guardan `POKE 2072-2075` (cuando se haga, nombrarlo en el apartado de snapshots); el emulador (`claude/planning/sprites_scroll160_emulador.md`).

- **SCUMM con ratón Bluetooth (10/11/2026, rama `claude/scumm-raton-232f38`, sin probar en la placa):** si el ratón está conectado (`LOAD *MOUSE` dice CONECTADO; necesita FPGA `sim_int 0.21` y ESP32 con `CMD_BLE`), Maniac Mansion y Zak McKracken lo usan: mueve el cursor, **botón izquierdo = clic** (verbos, frase, inventario, escena) y **botón derecho en la escena = cancela la frase y camina**. El teclado (Q/A/O/P, espacio, dígitos, 9 = ESC) sigue funcionando a la vez. Sin ratón no cambia nada. Sin resaltado al pasar el cursor.

## Ratón Bluetooth: texto listo para el manual (redactado el 10/11/2026, sin pasar al `.docx`)

El manual no dice nada del ratón (las únicas menciones de «ratón» son las del emulador). Dónde va cada trozo: **9.10** (usuario: nueva, tras 9.9 «Juegos en red»),
**15.5** (una fila nueva, la 77, en «Comandos de control del hardware»), **Apéndice N** (nuevo, para programadores), una fila en la tabla de POKEs/puertos si se quiere
nombrar el índice 20 del puerto `$3FEF` (Apéndice L, tabla del depurador), y una línea en la sección 14 si se decide (no hay códigos de error nuevos: los comandos devuelven estado 0).
Al pasarlo: docx primero, en ES y EN (el EN se traduce de este texto), y regenerar los `.md`. Quitar el aviso de la FPGA si no se publica con ella (ver «Comprobar al empaquetar»).

### 9.10 Ratón Bluetooth

El SD81 Booster puede usar un **ratón Bluetooth de bajo consumo (BLE)** como entrada para tus programas. Hace falta el **módulo WiFi** (el ratón lo lleva el ESP32; sin módulo no hay ratón).
Se empareja **una sola vez**: el interface lo memoriza y se reconecta solo cada vez que lo enciendes. Solo se guarda **un** ratón; emparejar otro sustituye al anterior.

| ℹ | *Funciona con los ratones Bluetooth BLE que se anuncian como dispositivo HID (la mayoría de los ratones inalámbricos modernos, con pila). Los ratones de receptor USB propio (2,4 GHz) no sirven.* |
|---|---|

**Emparejar el ratón**

1. Pon el ratón en modo de emparejado (suele ser un botón en la parte de abajo; el LED parpadea).
2. Escribe `LOAD *MOUSE THEN NEW`. Se abre una ventana de emparejado de 60 segundos.
3. Espera un par de segundos y comprueba con `LOAD *MOUSE`: debe decir `EMPAREJANDO ...S` y, al encontrar el ratón, `CONECTADO`.

Solo mientras la ventana está abierta se acepta un ratón nuevo: un móvil, un teclado o el ratón del vecino no se emparejan solos. Si hay varios dispositivos cerca en modo emparejado,
gana el que se anuncia como ratón.

**Comandos**

| Comando | Qué hace |
|---|---|
| `LOAD *MOUSE THEN NEW` | Empareja un ratón nuevo (ventana de 60 s). Sustituye al guardado: lo olvida, con sus claves |
| `LOAD *MOUSE THEN RUN` | Conecta ahora con el ratón guardado (y activa el módulo) |
| `LOAD *MOUSE THEN CLEAR` | Desempareja: olvida el ratón guardado y sus claves |
| `LOAD *MOUSE THEN STOP` o `LOAD *MOUSE STOP` | Desconecta el ratón y no vuelve a conectar hasta `THEN RUN` |
| `LOAD *MOUSE` | Muestra el estado en pantalla |
| `LOAD *MOUSE TO A$` | Guarda el estado en la variable de cadena `A$` (16 caracteres, rellenos con espacios) |

Los comandos de orden **no esperan el resultado**: el módulo WiFi recoge la orden en medio segundo. Para ver qué ha pasado, espera un segundo y mira `LOAD *MOUSE`.
Si no tienes el módulo WiFi o está apagado, las órdenes se aceptan pero no hacen nada.

**Estados que muestra `LOAD *MOUSE`**

| Texto | Significa |
|---|---|
| `CONECTADO` | El ratón está conectado y se mueve el cursor |
| `EMPAREJANDO 45S` | La ventana de emparejado está abierta (los segundos que quedan) |
| `BUSCANDO` | Hay un ratón guardado y se está buscando: enciéndelo, o acércalo |
| `PARADO` | Hay un ratón guardado pero el módulo está parado (`THEN STOP`); `THEN RUN` lo reactiva |
| `SIN RATON` | No hay ningún ratón guardado: empareja uno con `THEN NEW` |
| `SIN MODULO WIFI` | El módulo WiFi no ha contestado (apagado o no instalado) |

Ejemplo, esperar a que el ratón esté listo antes de seguir:

```
10 LOAD *MOUSE TO A$
20 IF A$(TO 9)<>"CONECTADO" THEN GOTO 10
30 PRINT "RATON LISTO"
```

(`A$` tiene siempre 16 caracteres; por eso se compara solo `A$(TO 9)`.)

**Si no se conecta:** comprueba que el ratón está en modo de emparejado y tiene pila; repite `THEN NEW`; si lo guardado no responde, `THEN CLEAR` y vuelve a emparejar.
Tras apagar y encender el ratón, o si se sale de alcance, se reconecta solo en unos segundos (`BUSCANDO` mientras tanto).

**Los programas que lo usan**

- **MOUSETEST** (`EXAMPLES/MOUSE`): un pequeño paint en ensamblador. Botón izquierdo dibuja, derecho borra, cualquier tecla sale.
- **MOUSEBLE** (`EXAMPLES/MOUSE`): menú en BASIC para emparejar, conectar, olvidar, activar y parar, y ver el estado (sirve también con la ROM antigua).
- **SCUMM** (Maniac Mansion y Zak McKracken): si el ratón está conectado, mueve el cursor; **botón izquierdo = clic** (verbos, frase, inventario, escena) y **botón derecho en la escena = cancela la frase y camina**. El teclado sigue funcionando a la vez. Sin ratón no cambia nada. (Ver la nota de SCUMM, más arriba en «Pendiente».)

**Desde la consola web del módulo WiFi** (`/debug`), por si no quieres usar BASIC: `esp ble` (estado), `esp ble pair [s]` (emparejar; de 10 a 300 s, 60 por defecto), `esp ble connect`, `esp ble forget`, `esp ble on` y `esp ble off`.

### 15.5 Fila nueva (Comandos de control del hardware)

| 77 | MOUSE | 1B: op, más los datos del op (tabla del Apéndice N) | Según el op (Apéndice N) | Lee el ratón Bluetooth (op 0), lo centra (1), configura su rango y sensibilidad (2), da órdenes de emparejado al módulo (3), lee su estado (4) y su texto (5). Equivale a `LOAD *MOUSE ...` y es lo que usa la ROM. Apéndice N. |

### Apéndice N --- Ratón Bluetooth para programadores

El ratón lo lee el ESP32 (cliente BLE), lo manda por la UART al STM32, que integra los movimientos en una **posición absoluta** (x de 0 a 255, y de 0 a 191, a 4 cuentas por píxel; el resto
fraccionario se arrastra), y la copia a la FPGA. El Z80 **solo lee**. Hay dos vías:

**1. El puerto del depurador, índice 20 (la vía de los juegos: sin esperas, unos 100 µs).** Necesita la FPGA con `sim_int` 0.21 o posterior.

```
        ld   bc,$3FEF
        ld   a,$24
        out  (c),a          ; elige el ratón, puntero a 0 y foto de 8 bytes
        in   a,(c)          ; x
        in   a,(c)          ; y
        ...                 ; 8 lecturas en total
```

| Byte | Contenido |
|---|---|
| 0 | x, 0 a 255 |
| 1 | y, 0 a 191 |
| 2 | botones: bit 0 izquierdo, bit 1 derecho, bit 2 central |
| 3 | rueda: contador de 8 bits que da la vuelta |
| 4 | rawx: movimiento horizontal sin recortar, contador de 8 bits que da la vuelta |
| 5 | rawy: igual, vertical |
| 6 | seq: cambia con cada paquete nuevo del ratón |
| 7 | estado: bit 0 = ratón conectado |

Para el movimiento relativo resta la lectura anterior (la rueda, rawx y rawy dan la vuelta a 8 bits, así que la resta en 8 bits funciona siempre). El puntero avanza al **acabar** cada `IN`.
Un nuevo `OUT $24` vuelve a empezar y toma una foto nueva. Los demás índices del puerto (Apéndice L) no cambian.
Rutina de ejemplo: `EXAMPLES/MOUSE/mouseport.inc` (`mp_read`).

**2. El comando 77 del MCU (una vez por programa: centrar, configurar, emparejar; o programas sencillos).** Lectura con espera activa, unos 0,3 ms.

| Op | Z80 → MCU | MCU → Z80 | Función |
|---|---|---|---|
| 0 | 77, 0 | x, y, botones, dx, dy, rueda, estado (7 bytes) | Leer. `dx`, `dy` y rueda: lo movido desde la lectura anterior, con signo y saturado a −128..127; se ponen a cero al leer. `estado`: bit 0, ha llegado algo del ratón desde que arrancó; bit 1, hay datos nuevos desde la lectura anterior |
| 1 | 77, 1 | estado | Centrar el cursor |
| 2 | 77, 2, max_x, max_y, div | estado | Configurar rango y sensibilidad. 0 = no cambiar. Por defecto 255, 191 y 4 cuentas por píxel |
| 3 | 77, 3, orden, argumento | 0 aceptada, 0x80 orden no válida | Orden de emparejado: 1 emparejar (argumento = segundos, 0 = 60), 2 conectar, 3 olvidar, 4 activar, 5 parar |
| 4 | 77, 4 | estado, segundos | Estado del módulo: bit 0 conectado, bit 1 emparejando, bit 2 activado, bit 3 hay ratón guardado, bit 7 el módulo ya ha contestado (con el bit 7 a 0 lo demás no vale); segundos que quedan de la ventana |
| 5 | 77, 5 | 16 caracteres ZX81 y un byte 0 | Texto de estado (el de `LOAD *MOUSE`) |
| otro | 77, op | 0x80 | Op no válido |

El handshake es el de `NET_READ`/`NET_WRITE` (un cambio de reloj por byte). El STM32 **no puede iniciar** tramas con el ESP32: deja la orden pendiente y el ESP32 la recoge en unos 0,5 s, así que el estado (op 4) tarda un momento en reflejarla.
Los caracteres del op 5 están en código ZX81 (espacio = 0, dígitos 28 a 37, letras 38 a 63), listos para imprimir. Código de ejemplo y ejemplos en `EXAMPLES/MOUSE` (`mouse.inc`, `mouseble.asm`, `mousetest.asm`).

| ℹ | *Las órdenes y el estado (ops 3 a 5) necesitan el módulo WiFi con el firmware que lleva el ratón; sin él, el op 4 devuelve el bit 7 a 0 y el op 5 responde «SIN MODULO WIFI».* |
|---|---|

### Comprobar al empaquetar la release que lleva el ratón

- Versiones mínimas: STM32 (comando 77 ops 3-5), ROM (`LOAD *MOUSE`, 12.505 bytes, termina en $30D9), FPGA (`sim_int` 0.21 + `mouse_regs.v`, para el índice 20) y WiFi (trama `CMD_BLE`). Poner la fila del historial y, si la FPGA nueva no sale con el resto, el aviso «necesita la FPGA de la próxima versión» en el puerto del índice 20.
- Copiar `EXAMPLES/MOUSE` a Software.zip (`MOUSETEST`, `MOUSEBLE`, con sus `.BIN`) y la ayuda de la SD: `/MAN/MOUSE.txt` (por crear; citarla en `HELP.txt`).
- Sin confirmar todavía en la placa: el puerto de la FPGA (índice 20) y `LOAD *MOUSE TO A$`; ya probadas el 10/11/2026 la reconexión y las cuatro órdenes (`THEN NEW/RUN/CLEAR/STOP`). El filtro de dispositivos acepta cualquier HID (teclados y mandos BLE incluidos, aunque luego no se interpretan como ratón): decidir si se endurece (solo `appearance` de ratón) antes de decirlo en el manual.
- Un ratón con dirección privada resoluble (RPA) cambiaría de dirección y no reconectaría: sin probar; si alguno falla, anotarlo aquí.

## Sprites en 70 y 80 columnas (FPGA, pendiente de probar en placa)

- Los sprites (64 normales + extra, formatos A y B) funcionan ahora tambien en los modos de 70 y 80 columnas (`LOAD *COL70` / `*COL80`). Cada pixel de sprite ocupa **dos** del texto, como en 32 columnas; el area visible son **280 pixeles de sprite de ancho**: X de **32 a 311** (columna 0 = 32; `LOAD *SPRITE` admite hasta 318). Y es la de siempre (32-223, 192 lineas). Un sprite que se sale por los lados se recorta.
- El ancho sigue a `POKE 2095` (recorte) y el origen a `POKE 2094` (desplazamiento); los modos de 160 no cambian. Quitar de los apartados de sprites y de los modos anchos cualquier frase que diga que no hay sprites en 70/80 columnas.
- Programa de prueba `EXAMPLES/SPR80` (Software.zip). La ayuda `/MAN/SPRITE.txt`, si cita la limitacion, debe cambiarse. Versiones minimas: FPGA (la que salga con esto), ROM sin cambios, STM32/WiFi solo para que el depurador dibuje los sprites en 70/80 (opcional).

## Para cada release

- Fila nueva en el historial de versiones (ES y EN), con la versión de cada parte (STM32, ROM, FPGA, WiFi).
- Los WAV de 32 bits por muestra se corrigieron (antes sonaban mal); basta con las notas de la release.

## Cómo se toca

Flujo docx primero con la skill `docx` (no Opus), en ES y EN a la vez, y regenerar los `.md` que
acompañan a cada manual. Antes de dar un punto por hecho, buscarlo en el manual (`pandoc … -t plain`)
y comprobar que los dos idiomas coinciden.
