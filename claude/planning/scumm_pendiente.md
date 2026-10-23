# SCUMM: pendiente de hacer y de probar en placa (lista viva)

## Por hacer
1. HECHO (sin probar en placa): freezeScripts en cutscene y cursorCommand (bit 7 del estado de ranura; unfreeze en endCutscene).
2. HECHO (sin probar en placa): nombres de actor en la frase (actorOps 3 guarda pagina+direccion de la cadena en ACTNAME).
3. Orden del inventario por recogida.
4. Prioridad de personajes vs objetos: el hardware solo dibuja sprites sobre el fondo; haria falta ocultar celdas de sprite sobre las zonas con mascara (datos de mascara por sala + recorte por celda). Sin hacer, a decidir.
5. Mas poses y boca alternando (TALK_STOP).
6. Host ops: HECHOS setObjectName, getClosestObjActor, setBoxFlags, lights (solo la variable 12; sin efecto de oscuridad en pantalla), getObjPreposition, restart.
   Sin hacer y sin uso en MM: setObjPreposition, ifClassOfIs (no aparece en scripts globales), switchCostumeSet; roomOps (1 uso), saveLoadGame (5 usos, script 163), oscuridad/linterna de lights.
7. NMI/RST dentro del overlay.
8. Constantes de columnas/filas de verbos a game_layout.inc.
9. SONIDO (efectos y musica; hoy nada).

## Probado en placa (2026-10-23)
- Boca al hablar, cutscene con tecla 4, apagado de sprites al cambiar de sala: OK.
- restart: el usuario lo probara mas adelante (hay que acabar el juego).

## Hecho pero SIN probar en placa
- Arreglo GD_RAW (sin inicializar: getDist no devolvia nada -> "I can't reach it" al cambiar de sala). Comprobar que se puede salir por las puertas.
- Teclas 1-3: reenviaban ASCII y elegian objeto; ahora mandan 1-3 (el script 4 cambia de niño con VAR_KEYPRESS 1-3 si VAR_CURSORSTATE = 1).
- Congelar scripts en cutscenes: en una cutscene los demas scripts se paran (p.ej. no deben moverse otros personajes) y siguen al acabar.
- Nombres de actor: al poner el cursor sobre un personaje la frase debe decir 'Walk to Dave' / 'Give ... to Syd'.
- Datos por juego (game_layout/game_data): debe verse igual que antes.
- setObjectName: nombre nuevo en frase e inventario (p.ej. 'jar of water' al llenar el frasco).
- getClosestObjActor, setBoxFlags (puertas/zonas bloqueadas), getObjPreposition (verbo Use en script 4).
- restart (game over).
