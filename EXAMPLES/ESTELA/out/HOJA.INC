; HOJA.INC -- generado por spr2cel.py a partir de sprites.png. No editar a mano.
;
; Hoja de sprites del formato B (celdas de 8x8 a 4 bpp, 32 bytes): HOJA.BIN se carga en SHEET_ADDR.
; La celda de la pagina p esta en bloque*8192 + p*32 = SHEET_ADDR + (p - SHEET_PAGE0)*32.
;
SHEET_BLOCK	equ 3		; POKE 2129
SHEET_ADDR	equ $7000	; direccion Z80 de la primera celda
SHEET_PAGE0	equ 128		; pagina de la primera celda
SHEET_CELLS	equ 121		; celdas usadas (HOJA.BIN mide 3872 bytes)
SHEET_END	equ $7F20	; primera direccion libre despues de la hoja
;
; Cada grafico: NOMBRE = pagina de su primera celda; NOMBRE_W / _H = celdas de ancho y alto; NOMBRE_N = fases;
; NOMBRE_STEP = celdas de una fase (W*H): la pagina de la fase f es NOMBRE + f*NOMBRE_STEP.
; Las celdas de un grafico van por filas: TL, TR, BL, BR en uno de 16x16 (paginas p, p+1, p+2, p+3).
; Con mas de una fase, ademas NOMBRE_0, NOMBRE_1, ... = pagina de la primera celda de cada fase.

; NAVE: 2x2 celdas (16x16 px), 3 fases, paginas 128-139
NAVE	equ 128
NAVE_W	equ 2
NAVE_H	equ 2
NAVE_N	equ 3
NAVE_STEP	equ 4
NAVE_0	equ 128
NAVE_1	equ 132
NAVE_2	equ 136

; LLAMA: 1x1 celdas (8x8 px), 2 fases, paginas 140-141
LLAMA	equ 140
LLAMA_W	equ 1
LLAMA_H	equ 1
LLAMA_N	equ 2
LLAMA_STEP	equ 1
LLAMA_0	equ 140
LLAMA_1	equ 141

; ENEMY1: 2x2 celdas (16x16 px), 2 fases, paginas 142-149
ENEMY1	equ 142
ENEMY1_W	equ 2
ENEMY1_H	equ 2
ENEMY1_N	equ 2
ENEMY1_STEP	equ 4
ENEMY1_0	equ 142
ENEMY1_1	equ 146

; ENEMY2: 2x2 celdas (16x16 px), 2 fases, paginas 150-157
ENEMY2	equ 150
ENEMY2_W	equ 2
ENEMY2_H	equ 2
ENEMY2_N	equ 2
ENEMY2_STEP	equ 4
ENEMY2_0	equ 150
ENEMY2_1	equ 154

; ENEMY3: 2x2 celdas (16x16 px), 2 fases, paginas 158-165
ENEMY3	equ 158
ENEMY3_W	equ 2
ENEMY3_H	equ 2
ENEMY3_N	equ 2
ENEMY3_STEP	equ 4
ENEMY3_0	equ 158
ENEMY3_1	equ 162

; ENEMY4: 2x2 celdas (16x16 px), 2 fases, paginas 166-173
ENEMY4	equ 166
ENEMY4_W	equ 2
ENEMY4_H	equ 2
ENEMY4_N	equ 2
ENEMY4_STEP	equ 4
ENEMY4_0	equ 166
ENEMY4_1	equ 170

; ENEMY5: 2x2 celdas (16x16 px), 2 fases, paginas 174-181
ENEMY5	equ 174
ENEMY5_W	equ 2
ENEMY5_H	equ 2
ENEMY5_N	equ 2
ENEMY5_STEP	equ 4
ENEMY5_0	equ 174
ENEMY5_1	equ 178

; ENEMY6: 2x2 celdas (16x16 px), 2 fases, paginas 182-189
ENEMY6	equ 182
ENEMY6_W	equ 2
ENEMY6_H	equ 2
ENEMY6_N	equ 2
ENEMY6_STEP	equ 4
ENEMY6_0	equ 182
ENEMY6_1	equ 186

; EXPL: 2x2 celdas (16x16 px), 4 fases, paginas 190-205
EXPL	equ 190
EXPL_W	equ 2
EXPL_H	equ 2
EXPL_N	equ 4
EXPL_STEP	equ 4
EXPL_0	equ 190
EXPL_1	equ 194
EXPL_2	equ 198
EXPL_3	equ 202

; BALAJ: 1x1 celdas (8x8 px), 2 fases, paginas 206-207
BALAJ	equ 206
BALAJ_W	equ 1
BALAJ_H	equ 1
BALAJ_N	equ 2
BALAJ_STEP	equ 1
BALAJ_0	equ 206
BALAJ_1	equ 207

; BALAE: 1x1 celdas (8x8 px), 2 fases, paginas 208-209
BALAE	equ 208
BALAE_W	equ 1
BALAE_H	equ 1
BALAE_N	equ 2
BALAE_STEP	equ 1
BALAE_0	equ 208
BALAE_1	equ 209

; CIFRA: 1x1 celdas (8x8 px), 10 fases, paginas 210-219
CIFRA	equ 210
CIFRA_W	equ 1
CIFRA_H	equ 1
CIFRA_N	equ 10
CIFRA_STEP	equ 1
CIFRA_0	equ 210
CIFRA_1	equ 211
CIFRA_2	equ 212
CIFRA_3	equ 213
CIFRA_4	equ 214
CIFRA_5	equ 215
CIFRA_6	equ 216
CIFRA_7	equ 217
CIFRA_8	equ 218
CIFRA_9	equ 219

; VIDA: 1x1 celdas (8x8 px), 1 fase, paginas 220-220
VIDA	equ 220
VIDA_W	equ 1
VIDA_H	equ 1
VIDA_N	equ 1
VIDA_STEP	equ 1

; BOMBA: 1x1 celdas (8x8 px), 1 fase, paginas 221-221
BOMBA	equ 221
BOMBA_W	equ 1
BOMBA_H	equ 1
BOMBA_N	equ 1
BOMBA_STEP	equ 1

; ITEM1: 1x1 celdas (8x8 px), 1 fase, paginas 222-222
ITEM1	equ 222
ITEM1_W	equ 1
ITEM1_H	equ 1
ITEM1_N	equ 1
ITEM1_STEP	equ 1

; ITEM2: 1x1 celdas (8x8 px), 1 fase, paginas 223-223
ITEM2	equ 223
ITEM2_W	equ 1
ITEM2_H	equ 1
ITEM2_N	equ 1
ITEM2_STEP	equ 1

; ITEM3: 1x1 celdas (8x8 px), 1 fase, paginas 224-224
ITEM3	equ 224
ITEM3_W	equ 1
ITEM3_H	equ 1
ITEM3_N	equ 1
ITEM3_STEP	equ 1

; JEFE: 4x3 celdas (32x24 px), 2 fases, paginas 225-248
JEFE	equ 225
JEFE_W	equ 4
JEFE_H	equ 3
JEFE_N	equ 2
JEFE_STEP	equ 12
JEFE_0	equ 225
JEFE_1	equ 237

; pal16 de los sprites (el indice 0 es el transparente: nibble alto de POKE 2103 a 0).
; Por entrada: byte bajo {G,B} -> POKE 2064; byte alto {BRIGHT,R} -> POKE 2066 (escribe y avanza el puntero POKE 2063).
PAL16:
	defb $00,$00	;  0  RGB   0   0   0
	defb $45,$23	;  1  RGB  48  52  66
	defb $34,$E2	;  2  RGB 110 116 132
	defb $CD,$9B	;  3  RGB 176 183 198
	defb $FF,$FF	;  4  RGB 252 252 252
	defb $4E,$02	;  5  RGB  24  44 150
	defb $4F,$B0	;  6  RGB  60 110 232
	defb $BF,$F4	;  7  RGB 140 210 255
	defb $23,$0D	;  8  RGB 140  24  30
	defb $00,$AF	;  9  RGB 236  52  52
	defb $82,$40	; 10  RGB  24 112  48
	defb $E2,$A2	; 11  RGB  84 212  84
	defb $E0,$DF	; 12  RGB 255 236  72
	defb $70,$AF	; 13  RGB 255 142  28
	defb $0A,$A8	; 14  RGB 150  60 172
	defb $3B,$DF	; 15  RGB 244 116 196
PAL16_END:
