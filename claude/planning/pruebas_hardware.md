# Pruebas pendientes en el hardware (15 de octubre de 2026)

> ## Dónde nos quedamos (16 de octubre de 2026)
> Sin acceso al hardware por ahora. Pruebas hechas el 15 de octubre: **1.1 sprites** (A, B y C),
> **1.2** índices 9-11 (faltan HiRes, Spectrum, doble buffer y `POKE 2057,170`), captura web, **1.3** CP/M
> (todo menos la traza), **1.4** teclado virtual. **Siguiente al volver a la placa: sección 2 (snapshots)**,
> empezando por «El snapshot describe lo que se ve» y las páginas escritas; después los paneles de la web
> (los probará el usuario en detalle), la autoactualización del ESP32 y las secciones 3 a 6.
> Sin probar todavía: 2, 3, 4 (red con broker real), 5 (juegos en red entre dos placas) y 6.

Todo lo que se ha hecho **después de la release v1.6.0** (tag `v1.6.0`, commit `eedc6f4`; 29 commits) y
no se ha probado en la placa, más lo que ya estaba pendiente de antes. Se marca con `[ ]` lo que falta y
con `[x]` lo que ya se probó, para ir tachando.

**Qué hay que cargar en la placa para probarlo todo** (los cuatro componentes a la vez, el depurador los
necesita al mismo nivel):

| Componente | Cambios desde v1.6.0 |
|---|---|
| FPGA (`sd81.mcs`) | sim_int 0.14 (índices 9-11), 0.15 (índice 12, filtro del bloque 0), 0.16 (orden 10 devuelve la ROM al bloque 0), sprites por línea K = 12 y 64 sprites, teclado virtual (`vkeys.v`) |
| STM32 (`firmware.bin`) | DEBUGGER.cpp (snapshots del estado de vídeo, páginas escritas, bloque 0 en RAM), COMMS (teclado virtual), WIFI_HANDLER, WAV.cpp, voice_synthesizer.cpp |
| ROM (`SDBOOST.ROM`) | sprites a cero en el reset (64), cambios en `extracmdcode`/`sdhandler` |
| ESP32 (`ESP32_FW.BIN`) | comandos AT de MQTT, SHA-256/HMAC/aleatorios, web del depurador (paneles, teclado virtual, captura) |
| SD | `SYS/DEBUG.BIN`, `SYS/NET.CFG` (opcional: sin él usa los brokers por defecto), los `.bin`/`.B81` de los ejemplos |

Orden recomendado: 1 → 2 → 3 (lo que ya se probó parcialmente y puede romper todo) y después 4 → 5 (red).

### Ya comprobado el 15 de octubre
- [x] Con `firmware.bin` compilado con el FQBN del IDE (`usb=CDCgen`...), el ESP32 conecta al WiFi y `LOAD *VER` da la ROM bien (1.7). El primer intento, compilado sin `usb=CDCgen`, rompió el enlace con el ESP32 (el `Serial` de depuración y el `Serial1` del ESP32 eran la misma USART1); la ROM 0.0 de ese intento no se ha explicado, pero desapareció con el binario correcto.
- [x] **Autoactualización del ESP32 desde la SD** (probada el 17 de octubre: **36 s**, dentro del plazo de 60 s; antes no terminaba porque se paraba la comunicación con el STM32 y no llegaba a grabar el fichero que marca el fin): (`ESP32_FW.BIN` en la raíz): con el STM32 bueno, ¿termina antes de 60 s y borra el fichero (LED verde)? Mide cuánto tarda; el plazo de 60 s del STM32 es total y puede quedarse justo con 1,3 MB (ver el análisis del 15 de octubre).

---

## 1. FPGA y vídeo

### 1.1 Sprites (ya fusionada en `master`, K = 12) — A, B y C probados el 15 de octubre; faltan la captura web con sprites y las posiciones en los tres modos
Ya probado por el usuario con `SPRDBUF` y el Mario (2026-10-06). Queda:
- [x] **Más de 12 sprites en una línea**: se pierden los de índice más bajo, sin glitches en el resto.
- [x] **Doble buffer** con sprites (`SPRDBUF`: sprites 30-39 y 62-63).
- [x] `LOAD *SPRITE` con índices 32-63 (segunda tabla en la sombra `$1800-$19FF`).
- [x] Los sprites se ponen a cero en el **reset** (64, no 32): encender sin ninguno basura en pantalla.
- [x] Vídeo sin perturbar en Superfast con sprites activos (visto con `SPRDBUF` y el Mario). En 70 y 80 columnas los sprites no están implementados: no aplica.
- [x] (probado el 17 de octubre) Posiciones en los tres modos (normal, HiRes, Spectrum) con la misma calibración de siempre.

### 1.2 Estado de vídeo legible: índices 9, 10, 11 de `$3FEF` (sim_int 0.14)
- [x] Índice 11 da `$C4`.
- [x] (probado el 17 de octubre desde BASIC: 170→`01`, 171→`03`, 172→`05`, 173→`09`, 174→`19`, 85→`00`) Índice 9 refleja lo que hay activo de verdad (hecho: BASIC `00`, CP/M `79` = Superfast + ancho + 7 píxeles + D_FILE y atributos alternativos; EDIT `79` (correcto); falta HiRes, Spectrum, doble buffer y el apagado con `POKE 2045,85`) (Superfast, HiRes, Spectrum, ancho, 7 píxeles, D_FILE alternativo, atributos alternativos, doble buffer): probar un programa que encienda cada uno y que lo apague con `POKE 2045,85`.
- [x] (probado el 17 de octubre: `POKE 2057,170`→`02`, el bit 7 del índice 9 se pone y se quita con `POKE 2057,85`) Índice 10: bloque de delante del doble buffer (`05` con EDIT y con `SPRDBUF`: es correcto, el registro arranca en 5 y `SPRDBUF` usa `POKE 2057,173` = AUTO con front 5, que no cambia de bloque. Falta ver otro valor: `POKE 2057,170` debe dar `02`, y `POKE 2057,85` apaga).
- [x] Captura de pantalla de la web (`cap`) exacta en cada modo (D_FILE alternativo o del sistema). Probada en BASIC, CP/M y el Mario.

### 1.3 CP/M y bloque 0 en RAM (FPGA 0.15 y 0.16) — probado el 15 de octubre, solo falta la traza
- [x] **Depurador dentro de CP/M**: pulsar QS > 1 s con CP/M corriendo; la pantalla del depurador sale con texto legible (antes: letras sin sentido; con 0.15 todavía salía basura al pulsar QS, lo arregla la 0.16).
- [x] Volver de la pantalla del depurador a CP/M sin que se estropee la RAM del bloque 0 (CP/M sigue funcionando).
- [x] **Snapshot de CP/M**: guardar (QS > 3 s o `snap`) y volver a cargar con `LOAD *Z81`; CP/M retoma donde estaba.
- [x] Índice 12 de `$3FEF` da `$A8 | 1` con CP/M y `$A8` tras un reset.
- [x] Sin CP/M, con `POKE 2056,0`: nada cambia respecto a la v1.6.0 (sprites, POKEs de control, fuente de la ROM).
- [x] La traza (`th`) sigue registrando con el bloque 0 en RAM (probado el 17 de octubre).

### 1.4 Teclado virtual (`vkeys.v`, orden 13) — probado el 15 de octubre
Sin probar en hardware (`tb_vkeys.v` solo en simulación).
- [x] Teclas desde la web del ESP32 llegan al ZX81 (lectura del puerto `$FE`).
- [x] Se suman al teclado real y al joystick sin pisarlos (varias teclas a la vez, SHIFT + letra).
- [x] Soltar tecla: no se queda pegada si se cierra la web o se pierde el WiFi.

---

## 2. Snapshots y depurador (STM32)

- [x] **`LOAD` ya no borra las páginas escritas**: un programa de varias cargas (datos desde la SD) guarda todas las páginas en el snapshot.
- [x] **El snapshot describe lo que se ve**: modo, `DISP_ADDR`, `ATTR_ADDR`, `DBUF` salen del estado real. Probar: programa Superfast → `POKE 2045,85` → snapshot → cargar: sigue en el modo normal, no en el alternativo.
- [x] Snapshot y carga con **doble buffer** activo.
- [x] Sonido callado mientras carga un snapshot (ya en v1.6.0; recomprobar con los AY).
- [ ] **Web del depurador**: paneles (desensamblado con breakpoints por clic, registros, pila, volcado, consola) — pendiente de probar en detalle. El teclado virtual, también en la pantalla del depurador, y la captura de pantalla ya están probados.
- [ ] `v dir` (HiRes WRX a dirección manual): **sin confirmar**; el usuario no ha sido capaz de dar con la dirección del mapa de bits para probarlo (en la v1.6.0 tampoco, y de nuevo el 17 de octubre). No se sabe si funciona; no bloquea la release.
- Riesgos del §9 de `hw_debugger_plan.md` (parar en mitad de un `LOAD`; breakpoints `FF` que borra un `LOAD`): son **limitaciones conocidas**, no pruebas pendientes (aclarado el 17 de octubre).
- [ ] FPGA anterior a 0.08 con el monitor cargado deja la ROM en `Z81MonWait` (comprobación opcional).

---

## 2b. Regresión con los programas de prueba del repositorio (con la FPGA 0.16 y el firmware nuevo)

- [x] **`SD81TEST` pasado OK el 17 de octubre** (segunda pasada, con el firmware del pull-up en `/WE`). **`SD81TEST`** (`EXAMPLES/SD81TEST`, `SD81TEST.B81`): el menú completo, los nueve apartados. Los que más pueden notar los cambios: 1 (memoria y mapper, incluida la protección del bloque 0), 2 (ejecución MC45), 3 (puertos mapeados), 5 y 6 (vídeo y Superfast, con los 64 sprites), 8 (teclado y joystick, con el teclado virtual).
  - **Primera pasada (16 de octubre, 19:35; MCU 2.6, FPGA 1.5, resumen `SD81TEST (1).TXT`): 19 OK y 2 FALLOS.** OK: MC45 6-7, mapper, `POKE 2045`, **interrupciones simuladas** (con el monitor), ROMLOCK, protocolo MCU, memoria sin paginar, registros del mapper, **protección del bloque 0**, velocidad de cuadros, RTC, AY, escritura en la SD, páginas de sistema, AY del MCU, reloj de la CPU, **WiFi (IP.TXT), TCP (HTTP) e hora de Internet**. **FALLAN: `01 MC45 BLOCKS 4-5` (FAIL 5) y `15 SD READ` (FAIL 4)**, y el resumen dice `ROM 0.0` (el test lee el byte de versión en `$2004`). Hipótesis: la ROM de expansión, que el MCU deja en RAM en el bloque 1, tiene unos bytes machacados (la lectura de la SD la compara con `/SYS/SDBOOST.ROM`: 4 diferencias, y el byte `$2004` a 0). El test de MC45 escribe `01 02 03 C9` en cinco puntos de los bloques 4-5, y si esos bloques apuntan a la página 1 (la ROM) en lugar de a las 4 y 5, se machaca justo ahí. Posible causa: la paginación completa (`LOAD *FULLPAG`) que se dejó puesta para `DBGTEST`. **Repetir tras apagar y encender del todo** y, si persiste, leer las pantallas de detalle del 1 y del 15 (la del 15 dice la primera dirección distinta).
  - **Aclarado el 17 de octubre: ninguno de los dos fallos es del interface.** `15 SD READ` y `ROM 0.0`: **fallo del firmware, arreglado el 17 de octubre** (se creyó primero que eran ficheros antiguos en la placa). Nada más arrancar, `$0000`, `$0004` y `$2004` de la ROM en RAM estaban a 0. Causa: `disableMem()` dejaba `nWRb` (el `/WE` de la SRAM) y `nMEM_OEb` como entradas flotantes, y mientras el Z80 sigue en reset la FPGA también suelta `/WE` (`nWRx = z`); los flancos de `send_config()` —con el teclado virtual, 8 órdenes más (`vkeys_clear()`)— se acoplaban y hacían escrituras espurias de ceros en direcciones con pocos bits a 1. Bisección: sin `vkeys_clear()` iba bien; con **pull-up interno** en `nWRb` y `nMEM_OEb` (`MEM.cpp`, `disableMem()`) y `vkeys_clear()` puesto, también (probado en la placa). Para la próxima revisión del PCB: pull-up externo en el `/WE` de la SRAM. `01 MC45 BLOCKS 4-5` (falla solo la columna `MC45 OFF`, que devuelve `0302`): **el ordenador de pruebas no es un ZX81 sino un clon de ZX80 diseñado por el usuario, con el mod M1NOT configurable en su propia ULA (selector)**; con el selector puesto, la máquina ya ejecuta código en `$8000-$BFFF` sin NOPs forzados, haga lo que haga MC45. Comprobado por el camino: `EN_MC45` sigue a `LOAD *MC45`/`STOP` al instante (sonda en `nBUSRQ`/`nBUSAK`, y lectura por la consola), `nHALT` no baja nunca con MC45 apagado, y el netlist de XST hace bien la lógica de `nHALT`. Con el selector de M1NOT quitado, el test tendría que pasar (pendiente de repetir).
  - De paso: XST convierte las órdenes de un bit del canal de configuración (`EN_MC45`, `nMODE48K`, `FULL_PAGING`, los del depurador... y `joy_cnf`) en **latches** con la puerta en una LUT (`~CFG_RESET & orden`): avisos `Xst:737`. Funcionan, pero son frágiles; una de las sondas pareció aplicar una orden tarde. Si alguna vez una orden «no entra», pasar el decodificado a un `always @(negedge CFG_RESET)` aparte (flip-flops, como ya hace `vkeys.v`).
- [x] **`SIMINT`** (`inttest`, `halttest`): cubierto por el apartado 5 de `SD81TEST` (`SIM. INTERRUPTS`: inyección, HALT y conteo de tramas), que pasó el 16 de octubre con la FPGA 1.5. Los dos programas sueltos solo añaden verlos en pantalla (opcional).
- [x] **`DBGTEST` no vale con el monitor de verdad (aclarado el 16 de octubre).** Lo que se probó en la placa y se creyó que era `SD81TEST` era `DBGTEST`: la prueba de la fase 1 del depurador, que exige el monitor de juguete. Con el monitor real, su prueba 1 (`OUT $10` a `$3FEF`, la trampa) para en el depurador con el LED magenta (`*** STOP at 0216: trap (OUT $10) (SLOW)`; con `c` sigue y acaba con `TEST1 WRONG`). Es lo esperado: **no hay ningún fallo**. El usuario lo borra de la tarjeta porque ya no tiene utilidad. `SD81TEST` y `SIMINT` siguen pendientes de probar (las dos líneas de arriba).

---

## 3. Arreglos pequeños del firmware

- [x] **Voz** (probado el 17 de octubre): `LOAD *SAY "hello"` y que termine sin colgarse ni quedarse sonando (`voice_synthesizer_isr` ahora devuelve la muestra). En ARM funcionaba «por casualidad»; confirmar que no ha cambiado nada audible.
- [x] **WAV de 32 bits** (probado el 17 de octubre): `LOAD FAST "fichero.WAV"` con un WAV de 32 bits por muestra suena bien (antes `x >> 24 + 128`). Los de 8 y 16 bits no deben cambiar.

---

## 4. Red: módulo WiFi (capa A)

Hace falta la placa con ESP32 y WiFi configurado (`/SYS/WIFI.CFG`). Sin probar con un broker real.

- [ ] (Pendiente de un programa de prueba propio: TLS, usuario/contraseña, QoS 1, retenidos, testamento, cola y 48 h no salen de jugar; el cliente MQTT básico sí se ejerce con los juegos en red.) Conectar: `AT+MQTT` con el broker por defecto (`test.mosquitto.org`, `broker.emqx.io`, `broker.hivemq.com`) y con un `/SYS/NET.CFG` propio; `AT+MQTT?` da el estado 0-3.
- [ ] **Conexión TLS** (puerto 8883) y con usuario / contraseña.
- [ ] `AT+MQTTSUB`, `AT+MQTTPUB=topic,qos,retain,len` (con `>` y datos crudos) y `AT+MQTTRX` entre **dos placas** o una placa y un cliente de escritorio (`mosquitto_pub`).
- [ ] QoS 1 y retenidos; el último testamento (`MQTTWILL`) al desconectar mal.
- [ ] Cola de 8 mensajes × 128 bytes: sin pérdida a ritmo normal; con la cola llena, el código de error 4-8 que toque.
- [ ] `AT+SHA256`, `AT+HMAC`, `AT+RAND` coinciden con los valores del PC (`claude/mqtttest` tiene vectores).
- [ ] `ATZ` cierra la sesión MQTT.
- [ ] **Interacción con el resto del puente**: el terminal telnet (`EXAMPLES/TELNET`) y el servidor web del ESP32 siguen yendo suaves mientras MQTT está conectado (heap libre, memoria del ESP32).
- [ ] Caída del WiFi y reconexión automática; 48 h de conexión permanente (fase 0 del plan).
- [ ] Tubo Z80 ↔ MCU: comandos 66/67 (`NET_READ` / `NET_WRITE`) a SLOW y a FAST; en SLOW el `NETCHAT` ya se vio en el emulador, no en la placa.

---

## 5. Juegos por turnos en red (capas B y C)

Solo se han probado en el emulador (4 en raya, NETCHAT) y en el PC (todo). Hace falta **dos placas** o una placa y el emulador.

> **Juegos en red probados en la placa el 17 de octubre** (NETCHAT, 4 en raya y ajedrez). Los apartados de abajo se dan por buenos; si algún caso concreto (caída de red, sala llena, reentrada) no se ejerció, se anota aparte.

### 5.1 NETCHAT (`EXAMPLES/NETCHAT`)
- [x] Crear sala, unirse con el código, mandar y recibir mensajes en las dos direcciones.
- [x] Parpadeo de pantalla en SLOW (el comportamiento estándar de `PAUSE`, no un fallo; comprobar que no empeora en la placa).
- [x] Salir y volver a entrar a la misma sala.

### 5.2 4 en raya (`EXAMPLES/NETGAME/CONNECT4.B81`)
- [x] Partida completa entre dos placas.
- [x] Sala llena: un tercer jugador recibe «THE ROOM IS FULL» y no rompe la partida de los otros dos.
- [x] Cortar el WiFi de uno a mitad de partida y volver: se resincroniza (petición de estado, tecla 9 para reenviar).
- [x] Revancha (ENTER al acabar).

### 5.3 Ajedrez (`EXAMPLES/NETGAME/CHESS.B81`)
Probado en el **emulador** (dos instancias) el 16 de octubre: pantalla, teclado, jugadas especiales, final de partida, red y rendimiento (detalle en `ajedrez_pruebas.md`). Lo de abajo es lo que falta **en la placa**.
- [x] Partida completa entre dos placas: blancas abajo, negras con el tablero girado.
- [x] Teclear `E2 E4` + ENTER; jugada ilegal rechazada; SHIFT+0 borra.
- [x] Enroque (corto y largo), al paso, coronación con quinta letra Q/R/B/N.
- [x] Jaque, mate, ahogado y tablas por las 50 jugadas.
- [x] Tiempo de la comprobación de jugadas legales en el Z80 real (~0,1-0,3 s previstos).
- [x] Revancha.
- [x] Reentrar tras una caída de red: «stale» de la sala (~90 s con la presencia retenida a `1`).

---

## 6. Heredado de antes (no se ha probado en la placa)

- [x] (probado) ESP32 definitivo (**Supermini**): no tiene el problema del 5 V de la placa de pruebas ESP32-C3-Mini; comprobar alimentación y log a la SD (`/SYS/ESP32LOG.TXT`).
- [x] (probado) NTP: `LOAD *NTP`, `*NTP="servidor"`, `*NTP +n`, `*SUMMER` con módulo WiFi (el comando Z80↔STM32 para el explorador es otro proyecto).
- [x] (probado) Búfer RX UART a 512 con la web del depurador y la red MQTT activas a la vez.
- [x] (aclarado el 17 de octubre) `manual_pendiente.md` no contiene pruebas, solo textos del manual que esperaban a las de arriba; ya están desbloqueados (ver ese archivo).

---

## Antes de la próxima release (no es una prueba, pero va con ellas)

1. Quitar el comando 70 (ROM + MCU) — `project_remove_cmd70`.
1b. **Renombrar `ESP32_FW.BIN` sin guion bajo** (p. ej. `ESP32FW.BIN`; decidido el 15 de octubre, después de todas las pruebas). El ZX81 no tiene `_`, así que el usuario no puede borrar desde el ZX81 un `ESP32_FW.BIN` que se haya quedado a medias; hoy solo se arregla sacando la SD. Tocar a la vez:
   - `FW_UPDATE_PATH` y `FW_UPDATE_FAILED_PATH` en `Wifi_module_01.ino` (y de paso `ESP32_FW_FAILED.TXT`, también con guion bajo),
   - `ESP32_FW_PATH` en `WIFI_HANDLER.cpp` (STM32),
   - `TOOLS/build_release.py`, `FIRMWARE/README_update.md` y `_ES.md`, los dos manuales y las notas de release,
   - `NTP_SYNC_NOW.FLAG` (`COMMANDS.cpp` y `Wifi_module_01.ino`) lleva el mismo problema, aunque el usuario no lo ve; decidir si se renombra también,
   - los comentarios de `SD_LOG.h` y del `.ino` hablan de `ESP32_LOG.TXT`, pero el fichero real es `ESP32LOG.TXT`.
   Hay que cambiar ESP32 y STM32 a la vez: con un nombre distinto en cada lado la autoactualización no encuentra el fichero.
2. Subir las cuatro versiones (`project_firmware_versioning`): FPGA a 1.6 y regenerar `sd81.mcs` con `append_fpga_version.py`.
3. Quitar «sin probar» de `manual_pendiente.md`, `emulador_pendiente.md` y `juego_en_red.md` según lo que salga.
