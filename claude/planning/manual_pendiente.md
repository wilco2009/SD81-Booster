# Manual de usuario: lo que falta añadir o corregir

Lista viva (empezada el 14 de octubre de 2026). Se tacha lo que se hace en los dos manuales
(`MANUAL/ES/SD81_Manual_ES.docx` y `MANUAL/EN/SD81_Manual_EN.docx`: docx primero, ver la nota de memoria
`reference_manual_docx`). Cada punto lleva dónde iría y de qué depende.

## Falta del todo

| # | Qué | Dónde | Depende de |
|---|---|---|---|
| 1 | **Comandos AT genéricos de red** del módulo WiFi: `AT+MQTT`, `MQTTID`, `MQTTWILL`, `MQTTSUB`, `MQTTUNSUB`, `MQTTPUB` (datos crudos tras `>`), `MQTTRX`, `MQTT?`, `MQTTCLOSE`, `AT+SHA256`, `AT+HMAC`, `AT+RAND`, con la cola de 8 mensajes, los códigos de error y los estados. Fuente: `juego_en_red.md` §3 y `emulador_pendiente.md` §5 | Apéndice nuevo para programadores (junto al de la terminal telnet) y una línea en 9.7 (módulo WiFi); la tabla de comandos del puente (`NET_READ`/`NET_WRITE`, comandos 66/67) lo cita | **Ya se puede redactar** (17 de octubre): el uso básico se ejerce con los juegos en red en la placa. TLS, usuario/contraseña, QoS 1, retenidos y testamento no se han probado: decirlo en el texto |
| 2 | **`/SYS/NET.CFG`**: formato (`broker host puerto [tls]`, `#` comentarios, máximo 6) y los tres brokers por defecto | mismo apéndice; si se decide llevar un `NET.CFG` de ejemplo en SD Content, también 9.7 | decidir si va en SD Content (por ahora no hace falta: sin fichero se usan los de siempre) |
| 3 | **Juegos por turnos en red** (salas, código de sala, qué se publica en el broker) y la **librería `net.inc` / el chat `NETCHAT`** (EXAMPLES/NETCHAT, para quien programe juegos y para probar la red entre dos ZX81). La librería ya existe, y los dos primeros juegos (el 4 en raya y el ajedrez, `EXAMPLES/NETGAME`) también: **probados en el emulador y en la placa (17 de octubre)** | apéndice nuevo o sección propia; `NETCHAT` en Software.zip (SD81/TOOLS) | **ya se puede redactar**; capa C de `juego_en_red.md` |

## Está, pero hay que corregirlo

| # | Qué | Dónde | Detalle |
|---|---|---|---|
| 4 | **Qué guarda `snap`** | "Snapshots desde el depurador" (Apéndice L) y 7.7 | Dice "por defecto solo se guardan las páginas mapeadas; `snap -a` guarda las 64". Lo cierto: las **mapeadas más las que el programa ha escrito desde el último reset** (menos la 63); `snap -a` guarda todas. Y `LOAD` ya no borra las escritas, así que un programa de varias cargas (datos desde la SD) las acumula |
| 5 | **CP/M y el depurador** | Apéndice I (CP/M) y Apéndice L | Con CP/M (`POKE 2056`) los POKEs de control y los sprites están apagados y el bloque 0 es RAM. La pantalla del depurador (QS más de 1 s) y la carga/guardado de snapshots de CP/M funcionan; decir qué se ve y qué no. **Probado en la placa el 15 de octubre: ya se puede redactar** |
| 6 | **`LOAD *Z81` sin el comando 70** | 7.7 y el texto "Sin el depurador se restauran como los de…" | Cuando se quite el 70 (antes de la próxima release): `LOAD *Z81` exige `/SYS/DEBUG.BIN`; decir el informe de error que da sin él. Ver `project_remove_cmd70` |
| 7 | **Teclado virtual y captura de pantalla de la web** | Apéndice L (ya hay un párrafo) | **Probados en la placa el 15 de octubre**: quitar cualquier "no probado"; decir que se abre mejor con la **IP** del módulo (la resolución de `sd81booster.local` en el navegador puede tardar 10-30 s por operación); comprobar que ES y EN dicen lo mismo |

## Para cada release

- Fila nueva en el historial de versiones (ES y EN), con la versión de cada parte (STM32, ROM, FPGA, WiFi).
- Los WAV de 32 bits por muestra se corrigieron (antes sonaban mal); basta con las notas de la release.

## Cómo se toca

Flujo docx primero con la skill `docx` (no Opus), en ES y EN a la vez, y regenerar los `.md` que
acompañan a cada manual. Antes de dar un punto por hecho, buscarlo en el manual (`pandoc … -t plain`)
y comprobar que los dos idiomas coinciden.
