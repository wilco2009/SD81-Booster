# Etapa 2: el motor en la placa, con texto y actores visibles (2026-10-21)

Hasta ahora el núcleo (`SCUMM/V2/ENGINE`) solo se ha ejecutado en el PC, y el visor (`SCUMM/V2/VIEWER`) solo enseña
salas. Esta etapa los junta: **un programa para el ZX81 que ejecuta los scripts de Maniac Mansion y lo
enseña**. Se hace por pasos, cada uno comprobable en el PC antes de probarlo en el emulador.

## Decisiones de diseño

**Mapa de memoria** (los números son el objetivo; cada paso lo comprueba):

| Zona | Uso |
|---|---|
| `$0000-$1FFF` | ROM del ZX81 (bloque 0), sin tocar: sprites y POKEs de control siguen activos |
| `$2000-$37FF` | **Ventana de datos** (bloque 1): scripts, código de objetos y bloques de sala paginados, **6 KB por página** |
| `$3800-$3FFF` | Tabla de 256 caracteres (`I = $38`) |
| `$4000-$7FFF` | Núcleo (`vm.asm`, `game.asm`, `geom.asm`, `walk.asm`, 9,5 KB) y el programa: carga, vídeo, texto, actores, teclado |
| `$8800-$BDFF` | Buffer del `.GFX` de la sala (hasta 13,8 KB; el mayor mide 12,9 KB: si un `.GFX` pasa de `GFXMAX` hay que bajarle `tiles`) |
| `$C000-$C7FF` | Tabla de color del modo 0 (256C) |
| `$C800-$DFFF` | Pantalla alternativa (`DFILE`, 793 B) y datos del programa |
| `$E000-$FFFF` | Estado del núcleo (variables, objetos, ranuras, actores, `ROOMDIR`, mensaje, cajas de la sala) |

La ventana pasó del bloque 4 al **bloque 1** (2026-10-21; `WINBLK` y `WINDOW` en `vm.asm`, `WINDOW` y
`PAGE_USABLE` en `vmmodel.py`): así el bloque 4-5 queda entero para el `.GFX`. Los 2382 escenarios del núcleo
siguen idénticos. Se acabaron los overlays de código: sobra sitio en `$4000-$7FFF`.

**Datos.** Todo lo que no es gráfico (scripts, objetos, cajas, ~121 KB) se carga **una vez al arrancar** desde
`/SCUMM/DATA.BIN` a páginas de la RAM extendida (de la 16 en adelante, con `FULLPAGING`). Los `.GFX` se cargan
por sala. El MCU solo sirve ficheros.

**Fichero `DATA.BIN`** (generado por `SCUMM/V2/TOOLS/pack_data.py`; todo little endian): cabecera de 16 bytes
(`"SDAT"`, versión, número de páginas, primera página, bytes por página) y, tal cual se copian en memoria:
`SCRDIR` (256 x 3), `OBJOWN` (780), `OBJST` (780) y `ROOMDIR` (64 x 16); relleno hasta 512 y las páginas de 6 KB.

**Salida del núcleo (puerto `$F2`).** El núcleo emite sucesos y el programa los atiende: sala nueva
(carga el `.GFX`, pinta), texto (lo pinta en las filas 16-23, que no hacen scroll), sonido (de momento se ignora).
El texto que se imprime está en el búfer de mensaje del núcleo; el suceso solo da un resumen para la traza.

**Actores.** Los trajes de v2 son piezas (hasta 16 por traje) con animaciones. Se dibujan con los **sprites por
hardware** (64 de 8x8, color por fila, máscara, 12 por línea). Un traje de 16x32 son 8 sprites. Los trajes se
convierten **offline** a datos de sprite (como las salas a `.GFX`). Primero, para ver el movimiento sin esperar al
conversor, cada actor se dibuja como un rectángulo de sprites del color de su texto.

## Pasos

1. **Ventana de datos en el bloque 1.** Hecho.
2. **Carga de datos (`DATA.BIN`).** Hecho: `TOOLS/pack_data.py` y `ENGINE/loader.asm` (+ `mcu.asm`); `ENGINE/test/test_loader.sh` ejecuta los 2382 escenarios del núcleo con los datos cargados por el Z80 desde un MCU simulado: idénticos.
3. **Programa principal** (`ENGINE/scumm.asm`): hecho y comprobado en el arnés de PC (`test/test_scumm.cpp` + `render_screen.py`, que dibuja lo que vería la FPGA). Arranque (modos de vídeo, `FULLPAGING`, carga), bucle de fotograma (VSYNC, `vm_frame`), sucesos del núcleo por `EVVEC`, sala (`.GFX` con scroll por la cámara que sigue a `VAR_EGO`), texto en las filas 17-20 y 46 glifos en los códigos 210-255. Con `DEMO = 0` ejecuta el script de arranque real del juego (llega a la pantalla de título, sala 45, y espera al jugador); con `DEMO = 1` (por omisión) se elige sala y actor con las teclas. **Falta probarlo en el emulador.**
4. **Actores como rectángulos** de sprites (16 x 32, 8 sprites por actor, hasta 8 actores), con la posición del núcleo; en el modo de prueba, `O`/`P` caminan (`start_walk_actor`, con las cajas de caminar), `N`/`B` cambian de sala, `T` hace hablar al actor 1 y `ESPACIO` sale. Hecho en el arnés; falta el emulador.
5. **Trajes de v2:** lector y decodificador en el PC (referencia, con PNG), conversión a sprites, y dibujo en Z80
   con las animaciones del núcleo (`animateActor`, hablar, caminar y los giros).
6. Verbos, frase, cursor e inventario (etapa 3).

## Riesgos

- Que el bloque 1 como ventana tenga algún efecto en la placa (el manual lo da por RAM normal). Se prueba en el
  paso 3 en el emulador.
- El tiempo de carga de `DATA.BIN` (121 KB por el handshake del MCU): estimado en segundos; se mide.
- Que 64 sprites y 12 por línea no alcancen con varios actores grandes (se verá en el paso 5).
- Los trajes de v2 con piezas de tamaño y posición arbitrarios no encajan en una rejilla de 8x8: la conversión
  puede tener que recortar o partir piezas.

## Entrada y frases (2026-10-21)

Para poder jugar hace falta lo que en ScummVM son `checkExecVerbs`, `runInputScript`, `checkAndRunSentenceScript`,
`findObject` y `doSentence`. Está en el núcleo (`ENGINE/input.asm`, modelo en `vmgame.py`) y se compara con el modelo
igual que lo demás:

- **Clic o tecla pendiente** (`INP_FLAG`, `INP_AREA`, `INP_VAL`): `vm_frame` pone `VAR_CLICK_AREA` (32) y, según la zona,
  `VAR_CLICK_VERB` (33), `VAR_CLICK_OBJECT` (35) o `VAR_KEYPRESS` (39), y arranca el **script 4**. Zonas: 1 verbo,
  2 escena, 3 inventario, 4 tecla, 5 frase. La posición del ratón la pone el programa cada fotograma en
  `VAR_VIRT_MOUSE_X` (30, en unidades de 8 píxeles) y `VAR_VIRT_MOUSE_Y` (31, de 2 píxeles).
- **`findObject(x, y)`** (opcode `o2_findObject`): primer objeto de la sala, en orden, que no sea intocable
  (estado 2), cuyos padres tengan el estado pedido (el estado 8 de la cadena de padres) y que contenga el punto.
  El bloque de la sala lleva ahora 11 bytes por objeto (id, caminar x/y, dirección, x, y, ancho, alto en 8 px, padre y
  estado del padre). `actorFromPos` es una aproximación (rectángulo de 16 x 32 del actor).
- **`doSentence`**: cola de 6 frases (`SENTQ`); subopcode 0 apila, 1 ejecuta (`runObjectScript` del objeto A con el
  verbo; reutiliza la ranura si ya corre ese objeto), 2 la escribe (suceso `EV_SENTENCE` = 7 para el programa), `$FB`
  reinicia la frase y `$FC` vacía la cola y para el script 2; el verbo 254 hace `stopObjectScript`.
- **`checkAndRunSentenceScript`**: si el script 2 no está en marcha, saca la última frase, pone `VAR_ACTIVE_VERB` (8),
  `VAR_ACTIVE_OBJECT1` (9), `VAR_ACTIVE_OBJECT2` (10) y `VAR_VERB_ALLOWED` (18) y lo arranca.
- El código de los objetos se localiza con `ODIR` (cargada en `$D000` por `loader.asm`).

En el programa de la placa (`scumm.asm`, modo real): `Q A O P` mueven un cursor (sprite 63), `ESPACIO` o `M` hacen
clic en la escena y `X` sale. Falta la zona de verbos (cuando exista `verbOps`), la línea de frase y el inventario.

**Estado tras la entrada (2026-10-21).** En el arnés, el modo real llega a la pantalla de título, el cursor se mueve y un
clic sobre un retrato ejecuta el script de entrada del juego, que lanza la frase y el script del objeto: sale la
descripción del niño («SYD - AN ASPIRING MUSICIAN TRYING TO START HIS OWN NEW-WAVE BAND»). **No se mueve el recuadro de
selección** porque ese recuadro (y todo lo que cambia de estado en la sala) es la imagen de un *objeto* (OBIM) y el
conversor solo hace el fondo: los objetos con imagen son lo que falta, junto con la zona de verbos y los trajes.

## Objetos con imagen y confirmar la seleccion (2026-10-22)

- `o2_drawObject` (modelo y `input.asm`): marca el objeto como dibujado (estado 8), apaga los demas objetos de la sala con el
  mismo rectangulo y emite `EV_OBJSTATE`. No cambia de sitio al objeto (x, y distintos de 255). Equivalencia: 2912/2912.
- Pantalla de titulo (sala 45): se eligen tres personajes (Dave + dos) con el cursor y ESPACIO/M. Se **confirma** pulsando el
  objeto 403 (x 272-311, y 56-63, arriba a la derecha): con `Var[111] < 3` dice «Please select two kids besides Dave», con 3 pone
  `Var[110] = 1` y el juego arranca. Esa zona queda fuera de los 256 px de la pantalla con la camara a 0, asi que el cursor
  **desplaza la camara** al tocar los bordes (`cu_edge` en `scumm.asm`: `cur_x >= 240` derecha, `<= 8` izquierda).
- Las salas con `protect` o `keep` reservan solo 12 tiles para objetos (`PROTECT_POOL` en `lfl2gfx.py`; `pool=N` en el cfg
  lo cambia): con 64 se llevaban tiles de las zonas protegidas (el esqueleto de la sala 4 salia roto).
- Medido: entre el 30 % y el 75 % de los tiles de objeto (exactos) ya estan en el fondo de su sala. Reutilizarlos exige que el
  tile siga vivo en el slot de la ventana donde esta el objeto; sin hacer.
- **Cuelgue en «Twenty years ago today» (2026-10-22):** el codigo del programa (`$4200-$7BC8`) ya pasaba de `$7B00`, donde
  estaba la pantalla provisional de arranque (`DFILE_TMP`), que lo pisaba con espacios: en cuanto se dibujaba el primer
  actor (`ac_draw`) el Z80 ejecutaba basura. `DFILE_TMP` esta ahora en `$8000` (GFXBUF, aun sin usar al arrancar). **Margen:**
  el codigo acaba en `$7BC8` y la pila esta en `$7F00` (unos 800 bytes libres). Si el programa crece mas, mover la pila o el
  codigo. `test_scumm` detecta ahora un atasco (50 M de instrucciones) e imprime los ultimos PC.

## Camara de los scripts y «title» (2026-10-22)

- El script 120 (creditos) espera `VAR_CAMERA_POS_X >= 100` tras `panCameraTo(220)`: faltaba la camara de los scripts. Esta en el
  **programa** (`scumm.asm`, `cam_update`, `host_ops` colgado de `HOSTVEC`), no en el nucleo: `cam_cx` es el centro de una
  pantalla de 320 px y `VAR_CAMERA_POS_X` (var 2) = `cam_cx / 8`, acotado por `VAR_CAMERA_MIN_X/MAX_X` (160, ancho - 160). Modos: 0
  `setCameraAt` (salta), 1 `panCameraTo` (8 px por fotograma), 2 `actorFollowCamera` (por omision VAR_EGO). Nuestra ventana de
  256 px se ve desde `cam_cx - 128`. Sin actor al que seguir (pantalla de seleccion) el cursor en los bordes mueve `cam_cx`.
- `actorFollowCamera(a)` en el **nucleo** (`h_o5_actorFollowCamera`, modelo `g_actorFollowCamera`): si el actor esta en otra sala,
  empieza esa escena (como `setCameraFollows` de ScummVM) y luego pasa la operacion al programa. Sin esto la intro se quedaba en la
  sala 49 y los mensajes de los actores (que estan en la 44) se perdian, y el script esperaba `VAR_CHARCOUNT > 28` para siempre.
- `test_scumm` tiene `TS_WATCH=lo-hi,fotograma` (imprime las escrituras a esas direcciones, con el PC) y para si se atasca.

## Interfaz: verbos y frase (2026-10-22)

- **Memoria (corregido):** el codigo NO puede pasar de `$7FFF` (por encima la ULA mete NOPs al captar instrucciones; el usuario lo diagnostico
  al ver NOPs en `$809E`: la primera version llego a `$8161` y `ui_click` se descodificaba mal). Ahora: nucleo `$4200-$68BF` (sin las
  tablas), interfaz (`ui.asm`) en el hueco `$6A76-$6FFF`, programa desde `$7000` hasta `$7D58`, y las **tablas del nucleo** (`tables.inc`, solo
  datos) a partir de `$8000` (`vm_core.asm` es el nucleo sin tablas; `vm.asm` lo incluye con ellas para las pruebas del PC). `scumm.asm` da error
  de ensamblado si el programa pasa de `$8000` o `ui.asm` de `$7000`. Margen actual: unos 680 bytes; despues, mas codigo al bloque 0 (overlay) o
  al hueco de datos. El `.GFX` empieza en `$8800` (`GFXBUF`) y la pila esta en `$CFF0`. `test_scumm` ahora para si ejecuta por encima de `$8000`.
- (antes) el programa llegaba a `$80D7`; el `.GFX` empieza en `$8800` (`GFXBUF`) y la pila esta en `$CFF0`. El arnes
  `test_scumm` pone la direccion de retorno inicial en `$CFF8` (antes `$7FF0`, que ahora es codigo). `TS_POKE=fotograma:dir:valor,...`
  escribe bytes en el arnes (para forzar variables).
- **Pantalla (24 filas):** 0-15 sala, 16 frase, 17-19 verbos, 20-21 inventario (pendiente), 20-23 texto de los mensajes (tapa
  al inventario mientras sale). El original usa 25 filas: frase en la 18, verbos en 19-21, inventario en 22-23.
- `SCUMM/V2/ENGINE/ui.asm` (incluido al final de `scumm.asm`): `o2_verbOps` (`ui_verbops`: 15 ranuras de 16 bytes con id, columna,
  fila, modo y texto; borrar, modo y nuevo), `o2_cursorCommand` (`ui_cursorcmd`: bits `USERSTATE_*` de ScummVM: interfaz de frase /
  inventario / verbos y cursor activo; el bloqueo de scripts no esta), `o2_drawSentence`, `ui_update` (cada fotograma: vuelve a
  escribir la frase si cambian `VAR_SENTENCE_VERB/OBJECT1/OBJECT2/PREPOSITION`), `ui_click` (cur_y >= 128: fila 16 zona 5, filas
  17-19 zona 1 con el verbo bajo el cursor).
- **40 -> 32 columnas:** columna = `(x * 13 + 4) / 16` (0, 5, 12, 19, 26) y en la zona de verbos se escriben etiquetas cortas
  (`ui_short`: WALK, PICK, WHAT, KID, ON, OFF); la linea de frase usa el texto completo del verbo. Sin colores: todo el texto es blanco
  (no hay codigos de caracter libres para un segundo juego de glifos), asi que no hay resalte al pasar el cursor.
- **Nombres de objeto** (frase e inventario): `ui_objname` busca el bloque del objeto con `ODIR`, mapea su pagina y lee el nombre en
  `bloque + bloque[14]`; el bit 7 de un caracter es «seguido de un espacio». Los nombres de actores todavia no.
- Pendiente de esta etapa: inventario (filas 20-21, flechas, `o2_pickupObject`), formas del cursor segun el contexto, teclas
  (script 4: area 4, p. ej. 1-3 cambian de niño), trajes reales y el bloqueo de `cursorCommand`.
- **Clics en los verbos (2026-10-22):** `ui_label` machacaba `C` (la fila en `ui_click` y el sitio disponible al dibujar) y el area de clic
  incluia los `@` de relleno, que con las columnas comprimidas pisaban a los verbos vecinos: solo acertaba la columna 0. Ahora `ui_label` conserva
  BC y el area de clic es solo el texto visible (hasta el ultimo caracter que no sea `@` ni espacio). Comprobado en el arnes con los 15 verbos.
- **Bordes de la sala (2026-10-22):** el centro de la camara se acotaba a 160..ancho-160 (pantalla de 320), con lo que nuestra ventana de 256 no veia los
  32 px de cada borde (la puerta de la casa en la sala 44 esta en x 0-16). `cam_cx` va ahora de 128 a ancho-128; `VAR_CAMERA_POS_X` sigue acotada a
  160..ancho-160 como en el original (los scripts que esperan `>= 100` no cambian).
- **`o2_loadRoomWithEgo` (2026-10-22):** estaba como operacion del anfitrion sin hacer nada (la puerta izquierda de la sala 44, objeto 258, no cambiaba de sala).
  Ahora esta en el nucleo (`h_o2_loadRoomWithEgo` en `walk.asm`, modelo `g_loadRoomWithEgo`), con la semantica de ScummVM v2: `putActor(x, y, sala)` conservando la
  posicion, `startScene` (siempre, aunque ya este en esa sala), el protagonista va al punto de caminar del objeto destino ajustado a una caja y con la direccion
  del objeto + 180, la camara pasa al programa (`ho_ego`: sigue a VAR_EGO y salta), `resetSentence`, camina a (x, y) si ambos son >= 0 y arranca el script 5.
  Al cambiar de sala el programa marca la interfaz para redibujar (`gf_load_room` borra la pantalla entera). Para salir por la izquierda de la sala 44 hay que hacer
  clic con la camara en el borde (objeto 258, x 0-16 de la sala): el primer clic acerca al personaje y la camara llega al borde, el segundo (sobre el objeto) lo ejecuta.
- **Doble bufer de la sala y objetos con padre (2026-10-22):** la sala se compone (fondo + objetos) en `BACKBUF` (`$DA00`, 528 bytes) y se copia de golpe al DFILE justo despues de un VSYNC
  (`gf_flip`; `wait_vs` no espera otro si el redibujado ya lo hizo: `vs_extra`). Antes se veia el fondo sin los objetos mientras se redibujaba (las tumbas parpadeaban al
  hacer scroll). El salto de camara (`cm_snap`) copia sin esperar y hace una sola copia con VSYNC al final; los cambios de estado de objetos solo piden el redibujado
  (`gd_pending`, `gd_update`) y se hace una vez por fotograma. **Regla de dibujo de objetos** (ScummVM v2): se dibuja si el estado propio no es 0 (la llave tiene el
  estado 1) y los padres cumplen `estado & 8 == estado del padre pedido`; antes exigia el bit 8 en el propio objeto y la llave (hija del felpudo) nunca salia.
- **Objetos: dueno y recoger (2026-10-22):** `o5_getObjectOwner`, `o2_setOwnerOf` y `o2_pickupObject` estaban como operaciones del anfitrion sin hacer nada (devolvian 0:
  «Pick up» siempre descartaba el objeto porque el script exige dueno 15). Ahora estan en el nucleo (`input.asm`, modelo `g_getObjectOwner` / `g_setOwnerOf` /
  `g_pickupObject`): recoger pone el dueno del protagonista y los bits 8 y 2 del estado y emite `EV_OBJSTATE`.
  **Regla de dibujo** (`obj_drawn`): un objeto se dibuja si sigue en la sala (dueno 15) y tiene el bit 8, o, siendo hijo de otro, un estado distinto de 0 (la llave
  bajo el felpudo, el sobre del buzon); y los padres tienen el estado pedido. La primera version dibujaba todo lo que tuviera estado != 0 y la puerta de la casa
  (estado 4 = cerrada con llave, sin padre) salia abierta.
- **Pendiente de operaciones del anfitrion sin implementar** (siguen siendo `h_host`/`h_host_r`: no hacen nada o devuelven lo ultimo): `getDist` (los scripts de caminar a
  un objeto lo usan: «I can't reach it»), `getRandomNr`, `getObjPreposition` / `setObjPreposition`, `getClosestObjActor`, `ifClassOfIs` (salto condicional),
  `setBoxFlags`, `cutscene` / `endCutscene` / `beginOverride` (congelar scripts, saltar con ESC), `restart`, `lights`, `roomOps`, `setObjectName`, `switchCostumeSet`.
- **Inventario, `getDist` y contraste (2026-10-22):**
  - **Inventario** (`ui.asm`, filas 20-21): 4 objetos en dos columnas (0 y 17) con los objetos cuyo dueno es el protagonista (`VARS+0`), en orden de numero de objeto (ScummVM usa el de
    recogida: pendiente), flechas `UP`/`DN` en la columna 15 (no hay glifos de flecha libres), clic en un objeto = zona 3 con el objeto; se redibuja con `EV_OBJSTATE`, al cambiar de
    protagonista y al borrarse un mensaje (el texto ocupa las filas 20-23 y tapa el inventario).
  - **`getDist`** en el nucleo (`h_o5_getDist`, modelo `obj_act_dist`): max(|dx|, |dy|) entre objeto/actor, con la caja del actor si es actor -> objeto; $FF sin posicion.
  - **`cursorCommand`** guarda su byte bajo en `VAR_CURSORSTATE` (variable 21): los scripts la usan (`Var[21] == 3` se salta el calculo de distancia en el script 2).
  - **Contraste de objetos pequenos:** el EGA 8 (gris oscuro) sale negro en la placa; en los objetos de area <= 512 px el EGA 8 pasa a blanco brillante (`OBJ_BOOST` en `lfl2gfx.py`;
    en `gfx_rooms.cfg`: `objboost=8=15`, `objboost=0` para desactivarlo, `objarea=N`). Hay que regenerar los `.GFX`.
  - **Memoria:** la entrada del binario es ahora `$4200` (`jp ENTRY_POINT`; `SCUMM.B81` hace `RAND USR 16896`), no hay hueco antes del programa y los datos puros (`progdata.inc`: fuente, tablas de
    direcciones, cadenas) van detras de las tablas, por encima de `$8000`. El codigo acaba en `$7F0F`: **quedan unos 240 bytes**; lo siguiente que crezca debe ir al overlay del bloque 0.
- **Personajes con trajes reales (2026-10-22):**
  - `TOOLS/costume2.py` decodifica los trajes de v2 (formato 0x58) con la semantica de ScummVM (cabecera, miembros, animaciones, imagenes de 12 bytes, RLE por columnas; origen del traje
    en (80, 100) = centro de los pies; dir 0 = izquierda se dibuja en espejo). La cabeza viene de la animacion de inicio (fotograma 2) y se combina con la de quieto (4-7) y la de
    caminar (0-3): `Costume.merged`. `costume2.py DIR n sal.png` saca una hoja de contacto.
  - `costume_blob` convierte cada traje a celdas de sprite 8x8 (tinta y papel por fila + mascara; 2 poses de caminar por direccion, quieto = la primera): ~5 KB por traje, 28 trajes
    en `DATA.BIN` (`COSTDIR` en el relleno de las tablas, cargado en `$FF40` por `loader.asm`). Los trajes de mas de 6100 bytes (el 11) no se incluyen.
  - `actors_update` / `ac_draw` (`scumm.asm`) dibujan los sprites a partir del traje, la direccion y el fotograma del actor (0 = caminar: pose cada 8 fotogramas). **Limite de hardware:**
    63 sprites y ~27 por personaje = 2 personajes completos; el que no cabe no se dibuja. Pendiente: boca al hablar (fotogramas 4/5), prioridad respecto a objetos, mas poses, reducir celdas.
  - Codigo: acaba en `$7FC5`, **quedan 59 bytes**. Lo siguiente debe ir al bloque 0 (overlay de codigo) o liberar espacio.
- **Overlay de codigo en el bloque 0 (2026-10-22):** `overlay.asm` (org $0000: `ui.asm` + `cam.asm` + `actors.asm`, 2,4 KB de 8) se ensambla aparte, va incbin al final de `scumm.bin` (`OVERLAY_IMAGE`) y en el
  arranque `ov_install` lo copia a la pagina fisica 63 a traves del bloque 1 y la mapea en el bloque 0 (despues de copiar la fuente de la ROM; `sc_quit` devuelve la ROM). Tabla de saltos al principio
  (`ovl_entries.inc`: `OVL_UI_INIT`, `OVL_UI_UPDATE`, `OVL_UI_CLICK`, `OVL_CAM_UPDATE`, `OVL_ACTORS_UPDATE`, `OVL_HOST_OPS`, `OVL_OV_INIT`); las variables estan en RAM (`ovl_vars.inc`, `$CE30`; la pagina es de
  solo lectura). **Montaje en tres pasos** (`build_scumm.sh`, `compila_scumm.bat`): scumm.asm -> overlay.asm (incluye `scumm.sym`) -> scumm.asm. Programa principal: acaba en `$76D1` (2,3 KB libres). Todo el
  codigo nuevo del programa va al overlay (5,6 KB libres); mas adelante, mas paginas de overlay con trampolines. Riesgo a vigilar en la placa: el vector NMI ($0066) y RST caen dentro del overlay.
  El arnes (`test_scumm`) ya emula el bloque 0; sus atajos de la ROM solo valen mientras `blk[0] == 0`; `DFILE_TMP` esta en `$A800` (debe quedar fuera de `OVERLAY_IMAGE`).
- **Personajes a escala 3/4** (`SCALE` en `costume2.py`): <= 18 celdas por fotograma, 3 personajes caben (54 sprites). Todos los trajes caben en una pagina (el 11 tambien): `DATA.BIN` = 27 paginas.
- **Imagenes de objetos retocadas a mano:** `lfl2gfx.py SALA --dump-objects DIR` escribe un PNG por objeto (colores de Chroma, 1:1); se devuelve retocado con `objpng=ID,fichero.png` en `gfx_rooms.cfg`
  (sin cambios automaticos de color: solo `objcolor=ID,de,a` y `objpng` por objeto concreto).
- Pendiente (usuario, 2026-10-22): sala01_obj054-bis.png (objeto 54, felpudo con la llave pintada); el objeto 55 es solo un cuadro negro: hay que dibujar el felpudo (54) DESPUES de la llave (55) para que se vea; objpng=54,... y orden de dibujo. La pagina 63 es del depurador: no usarla (overlay en la 50).
- **Regla de dibujo de objetos, definitiva (2026-10-22):** igual que ScummVM v2: se dibuja si el estado propio tiene el bit 8 y los padres tienen el estado pedido, **sin mirar el dueno**. Recoger un objeto
  le pone los bits 8 y 2 (`pickupObject`), asi que el cuadro negro del objeto 55 pasa a dibujarse (encima del felpudo, el 54, por tener menor indice) y tapa la llave que el felpudo retocado
  (`objpng=54,sala01_obj054-bis.png`) lleva pintada. Las reglas anteriores (estado != 0, dueno 15) estaban mal.
- **Cambio de sala sin sprites (2026-10-23):** `gf_load_room` apaga todos los sprites (`sc_hide_sprites`) y pone `ac_hold = 4`; `actors_update` no dibuja personajes mientras `ac_hold > 0`, y el salto de camara
  de `cm_fol` lo pone a 3. Comprobado en el arnes (44 -> 1): 64 sprites apagados en el fotograma de la carga, solo el cursor 2 fotogramas, y el protagonista vuelve ya en su sitio a los 3 fotogramas.
- **`getRandomNr` y cutscenes (2026-10-23):** en el nucleo (`input.asm`, modelo `vmgame.py`):
  - `getRandomNr(max)`: generador de 16 bits `x = x * 5 + 1` (semilla `$ACE1` en `vm_init`, `RNDST`), resultado `(byte alto * (max + 1)) >> 8` en 0..max.
  - `cutscene()`: vacia la cola de frases, para el script 2 y reinicia la frase (`o2_cutscene` de ScummVM); `endCutscene()`: `VAR_OVERRIDE = 0`; `beginOverride()`: guarda ranura y puntero
    (`CUTSLOT`, `CUTPTR` = el salto que sigue). La interfaz va en el overlay (`ui_cutscene`): durante la cutscene se ocultan verbos, frase, inventario y cursor y `VAR_CURSORSTATE = 200`; al acabar se
    restauran y la camara vuelve a seguir al protagonista.
  - **Saltar una cutscene:** `vm_abort_cutscene` (el script sigue en su salto y `VAR_OVERRIDE = 1`); en la placa, tecla **4** (el juego usa ESC, `VAR_CUTSCENEEXIT_KEY = 27`). Comprobado: en los creditos la tecla 4 lleva a la sala 44.
  - Pendiente: `freezeScripts` durante la cutscene (el resto de scripts deberian quedar congelados), reenviar teclas al script 4 (zona 4).

## Datos por juego fuera del motor (2026-10-23)
- `ENGINE/game_layout.inc` (filas de frase/verbos/inventario) y `ENGINE/game_data.inc` (etiquetas cortas `ui_short`, preposiciones `us_preps`,
  tablas `REMAP1-3` de trajes, plantilla `gf_tpl` de salas) son lo unico propio de Maniac Mansion. Para otro juego se sustituyen esos dos ficheros.
- Pendiente: las constantes de columnas (x*13/16) y `fila = y-2` de `ui_verbops` siguen en `ui.asm`; pasarlas a `game_layout.inc` cuando haya un segundo juego.

## Teclas 1-3 y boca al hablar (2026-10-23)
- `keys` (scumm.asm) detecta las teclas 1, 2 y 3 recien pulsadas y deja una entrada de zona 4 con su ASCII (49-51) -> `process_input` pone VAR_KEYPRESS y lanza el script 4 (cambio de niño).
  El arnes acepta '1','2','3' en el script de teclas.
- Trajes: el blob lleva 8 animaciones si el traje tiene la 20+ (TALK_START): 4-7 = cabeza de inicio + quieto + boca (una sola pose). `ac_draw` la usa cuando HAVEMSG != 0 y VAR_TALK_ACTOR
  (variables+82) es el actor (`ac_id`); si el traje no la tiene, sigue con quieto. Los trajes que con la boca pasan de 6100 bytes se empaquetan sin ella (pack_data.py). DATA.BIN: 29 paginas (16-44).
- Sin probar en la placa: la boca solo abre (sin alternar con TALK_STOP) y las teclas dependen de que el script 4 de MM espere ASCII.
