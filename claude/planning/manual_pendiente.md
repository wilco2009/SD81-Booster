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

## Pendiente

- **Fila nueva en el historial de versiones** (sección 19, ES y EN) al preparar la release, con la versión de cada parte (STM32, ROM, FPGA, WiFi). Debe nombrar el Scrabble, SPORTS,
  el scroll por hardware, los modos de 160 píxeles, la paleta de 16 colores, los BMP y los JPG, las rutas con `..` y que el joystick ya no pausa el depurador.
- Sin probar en la placa (y el manual lo describe como si funcionara): el MQTT avanzado (TLS, usuario y contraseña, QoS 1, retenidos, testamento), el comando 76 y los BMP,
  la paleta de 16, el modo 160x200 y el scroll vertical. Revisar el texto si la prueba da otra cosa.
- **JPG en el comando 76 (5/11/2026, sin probar en la placa)**: el comando 76 y el visor del explorador (IMGVIEW) y PHOTO aceptan también **JPG baseline** (`.JPG` / `.JPEG`;
  PHOTO solo `IMGnnn.JPG`), con las mismas colocaciones (F). Texto del manual: dónde pone «BMP» en el comando 76, los visores y el Apéndice del explorador, añadir «o JPG baseline»; lista de lo que
  admite (secuencial 8 bits, gris o YCbCr, 4:4:4/4:2:2/4:4:0/4:2:0/4:1:1, aplica la orientación EXIF; **progresivo solo con el DC**: sale más borroso, y una foto de menos de 1280x800
  se amplía) y de lo que se rechaza (aritmético, CMYK, baseline de varios scans, progresivo sin scan de DC entrelazado) con el motivo en `*BMPERR`; en la tabla de memoria del comando 76, ~28 KB en vez de ~23 KB; `EXAMPLES/PHOTO/jpg2i16.py` como herramienta del PC. Mencionarlo en las notas de la release.
  **No se ha tocado ningún .docx**: hay otra sesión editando el manual.
- Capturas de pantalla de los modos de 160 píxeles (no hay ninguna) y, si se quiere, `DOC/img/scrabble_start.png` (la pantalla de inicio del Scrabble).
- Para las **notas de la release** (no van al manual): el tempo del VGM (la música iba lenta por retrasos acumulados, arreglado), el sondeo del ESP32 cada 3 ms con datos y 15 ms en reposo, y los comandos 66 y 67 sin log por USB.
- Si cambia el aspecto del 4 en raya tras probarlo en el emulador, rehacer la captura (`DOC/img/connect4_win.png`).

## Para cada release

- Fila nueva en el historial de versiones (ES y EN), con la versión de cada parte (STM32, ROM, FPGA, WiFi).
- Los WAV de 32 bits por muestra se corrigieron (antes sonaban mal); basta con las notas de la release.

## Cómo se toca

Flujo docx primero con la skill `docx` (no Opus), en ES y EN a la vez, y regenerar los `.md` que
acompañan a cada manual. Antes de dar un punto por hecho, buscarlo en el manual (`pandoc … -t plain`)
y comprobar que los dos idiomas coinciden.
