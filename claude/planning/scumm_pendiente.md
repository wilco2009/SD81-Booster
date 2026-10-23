# SCUMM: pendiente de hacer y de probar en placa (lista viva, 2026-10-24)

Regla del proyecto: el interprete es generico; lo propio de un juego va solo en el perfil (SCUMM/games/*.cfg). Ver scumm_generico.md.

## Por hacer
1. Generico, siguientes pasos (scumm_generico.md): limites (actores, objetos, inventario, ranuras) y numeros de variable/script en el perfil; herramientas (pack_data, costume2, lfl2gfx) leyendo el perfil.
2. Siguiente juego de prueba: Indiana Jones y la Ultima Cruzada EGA o Monkey Island EGA (versiones v3/v4 de SCUMM, no v2): estudiar diferencias (ficheros de recursos, imagen y mascara, trajes, opcodes, sonido) antes de empezar.
3. SONIDO (efectos y musica; hoy nada).
4. Mas poses de caminar (hoy 2 por direccion); trajes grandes (2, 6, 7, 9, 10, 15, 23) hablan con una sola pose de boca y el 11 sin boca (no caben en 6100 bytes).
5. lights: oscuridad y linterna (hoy solo la variable 12).
6. roomOps (1 uso en MM) y saveLoadGame (5 usos, script 163): sin implementar.
7. Sin hacer y sin uso en MM: setObjPreposition, ifClassOfIs, switchCostumeSet.
8. NMI/RST dentro del overlay.

## Probado en placa
- 2026-10-23: boca al hablar, cutscene con tecla 4, apagado de sprites al cambiar de sala, prioridad de personajes por pixel (perfecta, sin perdida de velocidad), teclas 1-3 (cambian de nino), arreglo "I can't reach it", congelar scripts en cutscenes, nombres de actor en la frase.
- 2026-10-24: boca alternando, constantes de verbos, y todo tras pasar el perfil de juego a DATA.BIN: sigue funcionando.

## Hecho pero SIN probar en placa
- Inventario por orden de recogida (solo hay la llave; probar cuando haya mas objetos: 'give' entre ninos mantiene la ranura).
- Boca con dos poses sin mover las piernas (trajes 1, 3, 4, 5, 8, 12-14, 16-22, 24).
- restart (game over): hay que acabar el juego; el usuario lo probara mas adelante.
- setObjectName ('jar of water'), getClosestObjActor, setBoxFlags (salas 6, 16, 24, 30), getObjPreposition (verbo Use).
