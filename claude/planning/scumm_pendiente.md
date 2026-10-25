# SCUMM: pendiente de hacer y de probar en placa (lista viva, 2026-10-25)

Regla del proyecto: el interprete es generico; lo propio de un juego va solo en el perfil (SCUMM/games/*.cfg). Ver scumm_generico.md.

## Por hacer
1. Generico (scumm_generico.md): hecho variables con nombre, limites como capacidades y pack_data con trajes/escala del perfil. Falta costume2 (anims de hablar), lfl2gfx y vmcommon sin constantes de MM, y la prueba con Zak McKracken v2 (hay que conseguir sus ficheros).
2. Siguiente juego de prueba: Indiana Jones y la Ultima Cruzada EGA o Monkey Island EGA (versiones v3/v4 de SCUMM, no v2): estudiar diferencias (ficheros de recursos, imagen y mascara, trajes, opcodes, sonido) antes de empezar.
3. SONIDO: HECHO el camino completo (sound2vgm.py -> VGM del AY por sonido; snd.asm: prioridad y cola de Player_V2; MCU reproduce PLAY_VGM/STOP_VGM/LOOP_VGM). Suena en placa. Pendiente: VAR_MUSIC_TIMER (los scripts que sincronizan con la musica), sonidos que no son PCjr (47, 48, 53), tamano de los VGM largos (el 70 ocupa 1,5 MB a 600 s: ahora 180 s) y revisar que el AY suene como el PCjr (ruido en canal libre).
4. HECHO 2026-10-25 (sin probar en placa): trajes con fondo de celdas (costume2.py; actors.asm lee referencias). Todos con las dos poses de boca (el 11 ya habla) y 2-4 poses de caminar (el mayor que quepa en 6100 bytes: 4 en 8, 9, 12-16, 18-22, 24; 3 en 1, 3-6, 10; 2 en 2, 7, 11, 17, 23). Mejora posible: mas poses en los de 2 (paginar fotogramas).
5. lights: oscuridad y linterna HECHOS (dark.asm), sin probar en placa.
6. roomOps: MM no lo usa (el unico 'uso' esta en bytes muertos tras el final del script 59); lo usa Zak (paleta). saveLoadGame hecho.
7. Sin hacer y sin uso en MM: setObjPreposition, ifClassOfIs, switchCostumeSet.
8. NMI/RST dentro del overlay.

## Probado en placa
- 2026-10-23: boca al hablar, cutscene con tecla 4, apagado de sprites al cambiar de sala, prioridad de personajes por pixel (perfecta, sin perdida de velocidad), teclas 1-3 (cambian de nino), arreglo "I can't reach it", congelar scripts en cutscenes, nombres de actor en la frase.
- 2026-10-24: SONIDO suena en placa (la causa era el nombre del fichero: PLAY_VGM espera codigos ZX81, no ASCII; la cabecera VGM va a 0x100 por precaucion). Falta afinar: prioridades/cola, ruido, VAR_MUSIC_TIMER.
- 2026-10-24: boca alternando, constantes de verbos, y todo tras pasar el perfil de juego a DATA.BIN: sigue funcionando.

## Hecho pero SIN probar en placa
- Cache de paginas por demanda (2026-10-24): DATA.BIN se queda abierto (handle del MCU) y map_page trae cada pagina virtual de 6 KB con F_SEEK+F_READ; 47 paginas fisicas (16-62 menos la 50 del overlay), desalojo por reloj. Probada en el arnes con MM y Zak (pantalla y variables identicas con una cache de 8 paginas). En placa: comprobar el tiempo de los cambios de sala y que no cuelgue (handle abierto, 4 como maximo en el MCU).
- Sonido: copiar a la SD SCUMM\SND\*.VGM (preview\snd) y el DATA.BIN nuevo (--sounds). Efectos y musica deben sonar; prioridad: un sonido bajo no corta uno alto; los repetibles vuelven despues.
- Inventario por orden de recogida (solo hay la llave; probar cuando haya mas objetos: 'give' entre ninos mantiene la ranura).
- Boca con dos poses sin mover las piernas (trajes 1, 3, 4, 5, 8, 12-14, 16-22, 24).
- restart (game over): hay que acabar el juego; el usuario lo probara mas adelante.
- setObjectName ('jar of water'), getClosestObjActor, setBoxFlags (salas 6, 16, 24, 30), getObjPreposition (verbo Use).
- 2026-10-24 (Zak): VAR_MUSIC_TIMER (la musica lo sube; sound2vgm.py + 2a tabla en DATA.BIN), personajes ordenados por Y, elevacion de los actores aplicada al dibujar (jefe de Zak), ESC fija en la tecla 9 para todos los juegos (ya no va en el perfil) y nueva disposicion de la SD: SCUMM\SCUMM.BIN comun, SCUMM\<JUEGO>\<JUEGO>.B81 y SCUMM\<JUEGO>\DATA\ (DATA.BIN, ROOMnn.GFX, SND\); rutas relativas a la carpeta actual (el FOPEN y PLAY_VGM del MCU ya lo hacen). Probado solo en el arnes: falta la placa. Hay que borrar de la SD lo antiguo (SCUMM\*.GFX, DATA.BIN, SND, TOOLS\SCUMM). VIEWER/roomview sigue leyendo /SCUMM/ROOMnn.GFX.
- 2026-10-24 (tarde): HECHO y en el arnes; probado en placa por el usuario: tecla 9 = ESC, rutas relativas, jefe de Zak, orden de personajes por Y, VAR_MUSIC_TIMER, endCutscene vuelve a la sala del protagonista (escena de Sandy), guardar/cargar (tecla 8 = F5, SAVEnn.SAV en la carpeta del juego: nucleo $E000-$FFFF + variables del overlay $CD00-$CF8F + inventario; la cabecera lleva DATA.BIN y la direccion de ev_handler: un guardado de otra compilacion se rechaza), menu de la sala 50 legible (gfx_rooms.cfg: protect + pool=70). SIN probar en placa: oscuridad y linterna (dark.asm, lights c=0/1), bucle principal con VAR_TIMER_NEXT (cada scummLoop dura TIMER_NEXT jiffies: el juego puede ir mas lento que antes, es lo correcto).
- Captura por el profesor (script 37 de MM): en el arnes el nino aparece en la celda (sala 4) y se puede jugar con el; el usuario dice que 'se escapa directamente': falta saber que debe impedirlo (puerta cerrada = estado 159 apagado; cajas de la sala 4).
- Pendiente tras guardar/cargar: restaurar el sonido que sonaba (hoy se para), comprobar que la ranura del script del menu es la misma al guardar y al cargar (si no, el script se duplica), guardar con tecla fuera del menu (en Zak el script es el 7).

## 2026-10-25
- Puerta de seguridad (objetos 182/193): ahora por perfil (maniac.cfg [objetos] abiertos = 182, 193; PROF+40..43); getState las da por abiertas y gf_load_room las marca al cargar la sala. No se sabe como lo hace el original (la causa real de que se dibujara cerrada): pendiente de investigar; si se encuentra, quitar el apano de gf_load_room.
- compila_scumm.bat regenera DATA.BIN con pack_data.py antes de copiar (Maniac y Zak): un cambio en el .cfg llega a la SD.
- Frase (fila 16) fija: no se desplaza con la ventana de 40 columnas.
- Llave de la sala 3 (fondo, 357-367,121-126; el objeto 76 es el cuadro azul que la tapa): gfx_rooms.cfg protect + keep=15 solo en esa zona.
- sound2vgm: tope de 1 hora (antes 3 min). El 70 dura 30 min (4,6 MB); Zak sube a 23 MB de VGM.
- Gancho de depuracion DBG_START ($DC1C, palabra): escribir un numero de script desde el emulador lo arranca en el siguiente fotograma. Final de MM: script 43 (meteorito, sala 51); el 136 lo arranca tras delay(18000).
- Pendiente: probar en placa todo lo anterior; la equivalencia (build_test.sh, 2912/2912) tras el cambio de state_get/vmmodel.
