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
| 9 | **Scrabble en red** (inglés y castellano): cómo se juega (idioma al crear la sala, el cursor con las flechas 5-8, 0 y el sentido 8/6, o teclear `H8 R QUIZ`, `.` = comodín, `P`, `X`, `R`, `Q`; CH/LL/RR y la N con tilde), el diccionario en la SD (`DICTEN.DIC`/`DICTES.DIC`, `TOOLS/make_dict.py`; el castellano no va en la release por la licencia), `SCRABBLE.SAV`, el atril del rival no es secreto frente a quien toque el programa, y para programadores `G_MOVE_VAR`/`G_BIG` de `game.inc` | sección **9.9** (usuario) y Apéndice M (programadores). Pendiente de redactar tras probarlo en el emulador y la placa |

## Pendiente

- ~~El 4 en raya con interfaz gráfica~~ **hecho el 17 de octubre**: 9.9 describe cómo se ve y lleva su captura. Si cambia el aspecto
  tras probarlo en el emulador, rehacer la captura (`DOC/img/connect4_win.png`).
- **Fila nueva en el historial de versiones** (sección 19, ES y EN) al preparar la release, con la versión de cada parte.
- El MQTT avanzado (TLS, usuario y contraseña, QoS 1, retenidos, testamento) **no se ha probado en la placa**: el Apéndice M
  lo describe tal como está implementado.
- Los comandos 68, 69 y 71-74 del MCU no están en la tabla de comandos de la sección 15 (ROMLOCK, `fcreate` y el depurador).

## Para cada release

- Fila nueva en el historial de versiones (ES y EN), con la versión de cada parte (STM32, ROM, FPGA, WiFi).
- Los WAV de 32 bits por muestra se corrigieron (antes sonaban mal); basta con las notas de la release.

## Cómo se toca

Flujo docx primero con la skill `docx` (no Opus), en ES y EN a la vez, y regenerar los `.md` que
acompañan a cada manual. Antes de dar un punto por hecho, buscarlo en el manual (`pandoc … -t plain`)
y comprobar que los dos idiomas coinciden.
