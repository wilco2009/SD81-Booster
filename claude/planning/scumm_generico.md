# SCUMM: interprete generico (auditoria y plan, 2026-10-24)

Regla: el motor no contiene nada propio de un juego. Todo lo del juego va en un perfil de datos (sin codigo) que generan las herramientas.

## Lo que hoy es propio de Maniac Mansion (hay que sacarlo del motor)
- Tablas en codigo: game_data.inc (REMAP1-3, ui_short, us_preps, gf_tpl) y game_layout.inc (filas, columnas de verbos).
- Constantes fijas: NACTORS 25, NOBJ 780, NINV 48, NSLOT 24, NBOXES, tamano de tablas, numeros de script (SENTENCE_SCRIPT 2, INPUT_SCRIPT 4).
- Numeros de variable fijos en el codigo (VARS+n): VAR_EGO, VAR_TALK_ACTOR (82), VAR_KEYPRESS, VAR_CLICK_*, VAR_CURSORSTATE, VAR_OVERRIDE...: son los de v2, no de v5.
- Teclas: 1-3 -> 1-3 y 4 -> ESC (cutscene) escritos a mano en scumm.asm. Debe ser una tabla tecla de la placa -> valor de VAR_KEYPRESS.
- Herramientas: pack_data.py (range(1,25) de trajes, SYNTH_*), lfl2gfx.py y costume2.py (SCALE, REMAP, anim 4-7 para hablar), vmcommon.py (NUM_OBJECTS).
- Limites de la placa que no son del juego (64 sprites, 6100 B por traje, tiles 8x8): deben ser parametros, no literales.

## Propuesta
Perfil de juego = un fichero de configuracion (game.cfg) que leen las herramientas y que acaba en una tabla binaria dentro de DATA.BIN; el motor la lee al arrancar y solo usa esa tabla. Mismo binario para cualquier juego v2.
Secciones: version SCUMM (v2), limites (actores, objetos, inventario, ranuras), numeros de variable y de script, teclas, interfaz (filas, columnas, etiquetas, preposiciones), trajes (remapeos, escala), formato de ficheros de sala.

## Pasos
1. Perfil en DATA.BIN + lectura en el nucleo; mover limites y numeros de variable/script (lo que mas toca el codigo).
2. Teclas y tablas de interfaz (game_data/game_layout) al perfil.
3. Herramientas leyendo game.cfg (pack_data, lfl2gfx, costume2).
4. Probar con un segundo juego v2 (Zak McKracken) como criterio de aceptacion.

## Hecho (2026-10-24, paso 1 de la opcion A)
- SCUMM/games/maniac.cfg + TOOLS/gameprofile.py -> bloque PROF de 256 bytes en DATA.BIN (version 2, tablas de 6400 bytes); loader.asm lo copia a PROF (progdata.inc).
- Ya salen del motor: filas de inventario/verbos/frase, columnas de verbos, etiquetas cortas, preposiciones, nombre de los ficheros de sala y teclas de digito (valor de VAR_KEYPRESS; el valor de VAR_CUTSCENEEXIT_KEY salta la cutscene). game_data.inc y game_layout.inc eliminados; REMAP1-3 son tablas de v2 en progdata.inc.
- pack_data.py exige --game perfil.cfg.

## Hecho (2026-10-24, paso 2)
- Numeros de variable de v2: ENGINE/vars_v2.inc (VAR_TALK_ACTOR, VAR_KEYPRESS...) y ya no hay VARS+n sueltos en el motor; el binario sale identico. Decision: son de la VERSION de SCUMM (iguales en MM y Zak), no del juego; llevarlos al perfil en tiempo de ejecucion costaria codigo y velocidad en los caminos calientes. Otra version (v3+) cambia tambien los opcodes y tendra su propio nucleo.
- Limites (NACTORS 25, NOBJ 780, NINV 48, NSLOT 24) son capacidades fijas del motor: un juego con menos objetos o actores funciona igual (tablas a cero). Solo hay que comprobar que el juego cabe (pack_data afirma que las tablas caben).
- Herramientas: pack_data toma los trajes que existan (1-63) y la escala de personajes de [trajes] escala del perfil (games/maniac.cfg); DATA.BIN identico.

## Pendiente del plan
- costume2 (anims de hablar 4-7 por actor, las fija el juego con actorOps/TALK_START), lfl2gfx y vmcommon (NUM_OBJECTS 780 / NUM_ACTORS 25 son capacidades del motor) aun con constantes de MM en las herramientas: se resolveran al probar Zak, no antes.
- Revision de ui.asm hecha (2026-10-24): arreglados ui_label (DE), ui_inv_count (B) y la preposicion (DE); el resto de lecturas del perfil no pisa registros vivos.
- Probar con Zak McKracken v2 (hacen falta sus ficheros).
