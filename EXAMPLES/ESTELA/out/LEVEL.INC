; LEVEL.INC -- generado por map2bin.py a partir de NIVEL1.LVL. No editar a mano.
; Formatos del fichero de nivel (NIVELn.MAP), de las trayectorias y del tileset. Ver claude/planning/juego160.md, seccion 8.

; ---- cabecera del fichero de nivel (16 bytes, todo lo de 16 bits en little endian)
LVL_MAGIC	equ 0		; "ELV1" (4 bytes)
LVL_SCROLL	equ 4		; ticks de juego por pixel de scroll (1 o 2)
LVL_NWAVES	equ 5		; numero de oleadas (0-255)
LVL_ROWS	equ 6		; filas del mapa (u16)
LVL_WAVES	equ 8		; desplazamiento de la tabla de oleadas (u16)
LVL_MAP	equ 10		; desplazamiento del mapa (u16)
LVL_LEN	equ 12		; longitud del fichero (u16)
LVL_END	equ 14		; scroll final = 16*filas - 100 (u16): el nivel acaba al llegar ahi
LVL_HDR_SIZE	equ 16

; ---- oleadas: registros de 8 bytes ordenados por WAVE_POS; el ultimo lleva WAVE_POS = $FFFF
WAVE_POS	equ 0		; u16: posicion del scroll (pixeles) en que sale
WAVE_TYPE	equ 2		; tipo de enemigo (TYPE_*)
WAVE_X	equ 3		; X de pantalla del lado izquierdo del primero (sprite X = WAVE_X + 32)
WAVE_PATH	equ 4		; trayectoria (PATH_*)
WAVE_COUNT	equ 5		; cuantos enemigos
WAVE_SPACING	equ 6		; separacion: ticks entre uno y el siguiente, o pixeles en X con WF_ROW
WAVE_FLAGS	equ 7		; WF_*
WAVE_SIZE	equ 8
WF_MIRROR	equ 1		; dx de la trayectoria con el signo cambiado
WF_ROW	equ 2		; los enemigos salen a la vez, en fila (X + i*SPACING)
WF_GROUND	equ 4		; va pegado al suelo: ademas de la trayectoria baja con el scroll

; ---- tipos de enemigo (WAVE_TYPE) y su sprite en HOJA.INC (ENEMY1... = tipo + 1)
TYPE_E1	equ 0
TYPE_E2	equ 1
TYPE_E3	equ 2
TYPE_E4	equ 3
TYPE_E5	equ 4
TYPE_E6	equ 5
TYPE_BOSS	equ 6
TYPE_ITEM1	equ 8
TYPE_ITEM2	equ 9
TYPE_ITEM3	equ 10

; ---- trayectorias: PATH_* = numero; PATHTAB = tabla de punteros; cada una es [n pasos][dx dy]*n, con dx, dy de 8 bits
; con signo; el paso k se aplica en el tick k de la vida del enemigo; pasado el ultimo se repite el ultimo paso.
PATH_STILL	equ 0
PATH_DOWN	equ 1
PATH_SLOW	equ 2
PATH_SINE	equ 3
PATH_ZIGZAG	equ 4
PATH_DIVE	equ 5
PATH_SWEEP	equ 6
PATH_HOVER	equ 7
PATH_ENTER	equ 8
PATH_COUNT	equ 9
PATHTAB:
	defw PD_STILL,PD_DOWN,PD_SLOW,PD_SINE,PD_ZIGZAG,PD_DIVE,PD_SWEEP,PD_HOVER,PD_ENTER
PD_STILL:	defb 1		; 1 pasos
	defb 0,0
PD_DOWN:	defb 120		; 120 pasos
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
PD_SLOW:	defb 200		; 200 pasos
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
PD_SINE:	defb 180		; 180 pasos
	defb 3,2,2,1,2,1,3,2,2,2,2,1,2,1,2,2
	defb 1,2,2,1,1,1,1,2,0,2,1,1,0,1,0,2
	defb -1,2,0,1,-1,1,-1,2,-2,2,-1,1,-2,1,-2,2
	defb -2,2,-2,1,-3,1,-2,2,-2,2,-3,1,-3,1,-2,2
	defb -2,2,-3,1,-2,1,-2,2,-2,2,-2,1,-1,1,-2,2
	defb -1,2,-1,1,0,1,-1,2,0,2,0,1,1,1,0,2
	defb 1,2,1,1,2,1,1,2,2,2,2,1,2,1,2,2
	defb 3,2,2,1,2,1,3,2,3,2,2,1,2,1,3,2
	defb 2,2,2,1,2,1,2,2,1,2,2,1,1,1,1,2
	defb 0,2,1,1,0,1,0,2,-1,2,0,1,-1,1,-1,2
	defb -2,2,-1,1,-2,1,-2,2,-2,2,-2,1,-3,1,-2,2
	defb -2,2,-3,1,-3,1,-2,2,-2,2,-3,1,-2,1,-2,2
	defb -2,2,-2,1,-1,1,-2,2,-1,2,-1,1,0,1,-1,2
	defb 0,2,0,1,1,1,0,2,1,2,1,1,2,1,1,2
	defb 2,2,2,1,2,1,2,2,3,2,2,1,2,1,3,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2
PD_ZIGZAG:	defb 168		; 168 pasos
	defb 2,2,2,1,2,1,2,2,2,2,2,1,2,1,2,2
	defb 2,2,2,1,2,1,2,2,-2,2,-2,1,-2,1,-2,2
	defb -2,2,-2,1,-2,1,-2,2,-2,2,-2,1,-2,1,-2,2
	defb -2,2,-2,1,-2,1,-2,2,-2,2,-2,1,-2,1,-2,2
	defb -2,2,-2,1,-2,1,-2,2,2,2,2,1,2,1,2,2
	defb 2,2,2,1,2,1,2,2,2,2,2,1,2,1,2,2
	defb 2,2,2,1,2,1,2,2,2,2,2,1,2,1,2,2
	defb 2,2,2,1,2,1,2,2,-2,2,-2,1,-2,1,-2,2
	defb -2,2,-2,1,-2,1,-2,2,-2,2,-2,1,-2,1,-2,2
	defb -2,2,-2,1,-2,1,-2,2,-2,2,-2,1,-2,1,-2,2
	defb -2,2,-2,1,-2,1,-2,2,2,2,2,1,2,1,2,2
	defb 2,2,2,1,2,1,2,2,2,2,2,1,2,1,2,2
	defb 2,2,2,1,2,1,2,2,2,2,2,1,2,1,2,2
	defb 2,2,2,1,2,1,2,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
PD_DIVE:	defb 160		; 160 pasos
	defb 0,2,0,1,0,1,0,2,0,2,0,1,0,1,0,2
	defb 0,2,0,1,0,1,0,2,0,2,0,1,0,1,0,2
	defb 0,2,0,1,0,1,0,2,0,2,0,1,0,1,0,2
	defb 0,2,0,1,0,1,0,2,0,2,0,1,0,1,0,2
	defb 0,2,0,1,0,1,0,2,0,2,0,1,0,1,0,2
	defb 1,2,1,3,1,3,1,2,1,2,1,3,1,3,1,2
	defb 1,2,1,3,1,3,1,2,1,2,1,3,1,3,1,2
	defb 1,2,1,3,1,3,1,2,2,3,3,3,3,3,2,3
	defb 2,3,3,3,3,3,2,3,2,3,3,3,3,3,2,3
	defb 2,3,3,3,3,3,2,3,2,3,3,3,3,3,2,3
	defb 2,3,3,3,3,3,2,3,2,3,3,3,3,3,2,3
	defb 2,3,3,3,3,3,2,3,2,3,3,3,3,3,2,3
	defb 2,3,3,3,3,3,2,3,2,3,3,3,3,3,2,3
	defb 2,3,3,3,3,3,2,3,2,3,3,3,3,3,2,3
	defb 2,3,3,3,3,3,2,3,2,3,3,3,3,3,2,3
	defb 2,3,3,3,3,3,2,3,2,3,3,3,3,3,2,3
	defb 2,3,3,3,3,3,2,3,2,3,3,3,3,3,2,3
	defb 2,3,3,3,3,3,2,3,2,3,3,3,3,3,2,3
	defb 2,3,3,3,3,3,2,3,2,3,3,3,3,3,2,3
	defb 2,3,3,3,3,3,2,3,2,3,3,3,3,3,2,3
PD_SWEEP:	defb 240		; 240 pasos
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,1,0,1,1,2,0,1,1
	defb 1,0,1,0,1,1,2,0,1,1,1,0,1,0,1,1
	defb 2,0,1,1,1,0,1,0,1,1,2,0,1,1,1,0
	defb 1,0,1,1,2,0,1,1,1,0,1,0,1,1,2,0
	defb 1,1,1,0,1,0,1,1,2,0,1,1,1,0,1,0
	defb 1,1,2,0,1,1,1,0,1,0,1,1,2,0,1,1
	defb 1,0,1,0,1,1,2,0,1,1,1,0,1,0,1,1
	defb 2,0,1,1,1,0,1,0,1,1,2,0,1,1,1,0
	defb 1,0,1,1,2,0,1,1,1,0,1,0,1,1,2,0
	defb 1,1,1,0,1,0,1,1,2,0,1,1,1,0,1,0
	defb 1,1,2,0,1,1,1,0,1,0,1,1,2,0,1,1
	defb 1,0,1,0,1,1,2,0,1,1,1,0,1,0,1,1
	defb 2,0,1,1,1,0,1,0,1,1,2,0,1,1,1,0
	defb 1,0,1,1,2,0,1,1,1,0,1,0,1,1,2,0
	defb 1,1,1,0,1,0,1,1,2,0,1,1,1,0,1,0
	defb 1,1,2,0,1,1,1,0,1,2,1,2,2,2,1,2
	defb 1,2,1,2,1,2,2,2,1,2,1,2,1,2,1,2
	defb 2,2,1,2,1,2,1,2,1,2,2,2,1,2,1,2
	defb 1,2,1,2,2,2,1,2,1,2,1,2,1,2,2,2
	defb 1,2,1,2,1,2,1,2,2,2,1,2,1,2,1,2
	defb 1,2,2,2,1,2,1,2,1,2,1,2,2,2,1,2
	defb 1,2,1,2,1,2,2,2,1,2,1,2,1,2,1,2
	defb 2,2,1,2,1,2,1,2,1,2,2,2,1,2,1,2
	defb 1,2,1,2,2,2,1,2,1,2,1,2,1,2,2,2
	defb 1,2,1,2,1,2,1,2,2,2,1,2,1,2,1,2
	defb 1,2,2,2,1,2,1,2,1,2,1,2,2,2,1,2
	defb 1,2,1,2,1,2,2,2,1,2,1,2,1,2,1,2
	defb 2,2,1,2,1,2,1,2,1,2,2,2,1,2,1,2
PD_HOVER:	defb 230		; 230 pasos
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,2,0,2,0,2,0,2,0,2,0,2,0,2,0,2
	defb 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	defb 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	defb 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	defb 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	defb 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	defb 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	defb 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	defb 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	defb 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	defb 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	defb 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	defb 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	defb 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	defb 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	defb 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	defb 0,-2,0,-2,0,-2,0,-2,0,-2,0,-2,0,-2,0,-2
	defb 0,-2,0,-2,0,-2,0,-2,0,-2,0,-2,0,-2,0,-2
	defb 0,-2,0,-2,0,-2,0,-2,0,-2,0,-2,0,-2,0,-2
	defb 0,-2,0,-2,0,-2,0,-2,0,-2,0,-2,0,-2,0,-2
	defb 0,-2,0,-2,0,-2,0,-2,0,-2,0,-2,0,-2,0,-2
	defb 0,-2,0,-2,0,-2,0,-2,0,-2,0,-2,0,-2,0,-2
	defb 0,-2,0,-2,0,-2,0,-2,0,-2,0,-2,0,-2,0,-2
	defb 0,-2,0,-2,0,-2,0,-2,0,-2,0,-2,0,-2,0,-2
	defb 0,-2,0,-2,0,-2,0,-2,0,-2,0,-2
PD_ENTER:	defb 51		; 51 pasos
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1
	defb 0,1,0,1,0,0

; ---- tileset (los 16.000 pixeles de NIVELn.I16 en $C000)
TILESET_BASE	equ $C000
TILESET_STRIDE	equ 160		; bytes de una fila de pixeles
; direccion del tile t: TILESET_BASE + (t/10)*2560 + (t mod 10)*16; la fila y del tile esta y*160 bytes mas adelante
TILEADR:			; 60 palabras, indexadas por el numero de tile
	defw $C000
	defw $C010
	defw $C020
	defw $C030
	defw $C040
	defw $C050
	defw $C060
	defw $C070
	defw $C080
	defw $C090
	defw $CA00
	defw $CA10
	defw $CA20
	defw $CA30
	defw $CA40
	defw $CA50
	defw $CA60
	defw $CA70
	defw $CA80
	defw $CA90
	defw $D400
	defw $D410
	defw $D420
	defw $D430
	defw $D440
	defw $D450
	defw $D460
	defw $D470
	defw $D480
	defw $D490
	defw $DE00
	defw $DE10
	defw $DE20
	defw $DE30
	defw $DE40
	defw $DE50
	defw $DE60
	defw $DE70
	defw $DE80
	defw $DE90
	defw $E800
	defw $E810
	defw $E820
	defw $E830
	defw $E840
	defw $E850
	defw $E860
	defw $E870
	defw $E880
	defw $E890
	defw $F200
	defw $F210
	defw $F220
	defw $F230
	defw $F240
	defw $F250
	defw $F260
	defw $F270
	defw $F280
	defw $F290
TILE_GRASS1	equ 0
TILE_GRASS2	equ 1
TILE_GRASS3	equ 2
TILE_DIRT	equ 3
TILE_DIRTST	equ 4
TILE_SAND	equ 5
TILE_ROCK1	equ 6
TILE_ROCK2	equ 7
TILE_ROCKCR	equ 8
TILE_SPACE	equ 9
TILE_WATER	equ 10
TILE_WATERW	equ 11
TILE_BANKL	equ 12
TILE_BANKR	equ 13
TILE_BRIDGEH	equ 14
TILE_BRIDGEE	equ 15
TILE_SHORET	equ 16
TILE_SHOREB	equ 17
TILE_ISLAND	equ 18
TILE_LILY	equ 19
TILE_LAVA	equ 20
TILE_LAVACR	equ 21
TILE_LAVAL	equ 22
TILE_LAVAR	equ 23
TILE_LAVAT	equ 24
TILE_LAVAB	equ 25
TILE_OBSID	equ 26
TILE_OBSIDS	equ 27
TILE_LAVABU	equ 28
TILE_EMBER	equ 29
TILE_METAL	equ 30
TILE_METALR	equ 31
TILE_GRATE	equ 32
TILE_HAZARD	equ 33
TILE_LIGHTS	equ 34
TILE_PIPEH	equ 35
TILE_PIPEV	equ 36
TILE_PIPEC	equ 37
TILE_HATCH	equ 38
TILE_PAD	equ 39
TILE_TURRET	equ 40
TILE_CRATER	equ 41
TILE_ROADH	equ 42
TILE_ROADV	equ 43
TILE_ROADX	equ 44
TILE_DUNES	equ 45
TILE_TREE	equ 46
TILE_BUSH	equ 47
TILE_TREES2	equ 48
TILE_BOULDER	equ 49
TILE_GRDIRT	equ 50
TILE_DIRTGR	equ 51
TILE_GRROCKL	equ 52
TILE_GRROCKR	equ 53
TILE_LIGHTS2	equ 54
TILE_MLIGHT	equ 55
TILE_STARS	equ 56
TILE_DARK	equ 57
TILE_CHECK	equ 58
TILE_SANDST	equ 59
PANEL_ADDR	equ $FC00		; fila de los paneles: +0..+15 el izquierdo, +144..+159 el derecho

; ---- rangos de la paleta de 256
PAL_PANEL	equ 0		; 0-15
PAL_PANEL_N	equ 16
PAL_GRASS	equ 16		; 16-31
PAL_GRASS_N	equ 16
PAL_FOREST	equ 32		; 32-47
PAL_FOREST_N	equ 16
PAL_DIRT	equ 48		; 48-63
PAL_DIRT_N	equ 16
PAL_SAND	equ 64		; 64-79
PAL_SAND_N	equ 16
PAL_ROCK	equ 80		; 80-95
PAL_ROCK_N	equ 16
PAL_ROCKW	equ 96		; 96-111
PAL_ROCKW_N	equ 16
PAL_METAL	equ 112		; 112-127
PAL_METAL_N	equ 16
PAL_OBSID	equ 128		; 128-143
PAL_OBSID_N	equ 16
PAL_ROAD	equ 144		; 144-159
PAL_ROAD_N	equ 16
PAL_ACCENT	equ 160		; 160-175
PAL_ACCENT_N	equ 16
PAL_FOAM	equ 176		; 176-191
PAL_FOAM_N	equ 16
PAL_HAZARD	equ 192		; 192-207
PAL_HAZARD_N	equ 16
PAL_SPACE	equ 208		; 208-223
PAL_SPACE_N	equ 16
PAL_WATER	equ 224		; 224-239
PAL_WATER_N	equ 16
PAL_LAVA	equ 240		; 240-247
PAL_LAVA_N	equ 8
PAL_LIGHTS	equ 248		; 248-255
PAL_LIGHTS_N	equ 8
