; Generado por gen_cards.py a partir de cartas.png: no se edita a mano (retoca la hoja y vuelve a
; ejecutar "python gen_cards.py"). El formato y como lo usa draw_card estan en la cabecera de
; gen_cards.py. Rango 0..12 = A 2..9 10 J Q K; palo 0..3 = picas, corazones, diamantes, treboles.

CD_W            equ 3           ; ancho de la carta en bytes (24 px)
CD_H            equ 32          ; alto en lineas
CD_IDX_LINE     equ 2           ; primera linea del indice (rango + palo pequeno), 7 lineas
CD_BIG_LINE     equ 12          ; primera linea del palo / letra grande, 16 lineas
CD_BACK_LINE    equ 2           ; primera linea del patron del dorso, 28 lineas
CD_ATTR_FELT    equ 040h        ; tapete liso (bitmap 0)
CD_ATTR_BACK    equ 015h        ; dorso: papel azul, tinta cian
CD_ATTR_HOLE    equ 040h        ; hueco vacio

; atributo de la cara por palo (papel blanco brillante; tinta negra o roja brillante)
cd_suitattr:
            defb 0F0h,0FAh,0FAh,0F0h

; lineas 0, 1, 30 y 31 (bordes redondeados): bitmap (3 bytes por linea) y atributo
cd_edge:
            defb 03Fh,0FFh,0FCh   ; linea 0
            defb 07Fh,0FFh,0FEh   ; linea 1
            defb 07Fh,0FFh,0FEh   ; linea 30
            defb 03Fh,0FFh,0FCh   ; linea 31
cd_edgeattr:
            defb 040h,04Fh,04Fh,04Fh   ; cara
cd_backattr:
            defb 040h,041h,041h,041h   ; dorso

; indices de rango: 13 x 7 bytes (A 2 3 4 5 6 7 8 9 10 J Q K), alineados a la izquierda de la carta
cd_rank:
            ; A
            defb 038h,044h,044h,07Ch,044h,044h,044h
            ; 2
            defb 038h,044h,004h,008h,010h,020h,07Ch
            ; 3
            defb 07Ch,008h,010h,008h,004h,044h,038h
            ; 4
            defb 008h,018h,028h,048h,07Ch,008h,008h
            ; 5
            defb 07Ch,040h,078h,004h,004h,044h,038h
            ; 6
            defb 018h,020h,040h,078h,044h,044h,038h
            ; 7
            defb 07Ch,004h,008h,010h,020h,020h,020h
            ; 8
            defb 038h,044h,044h,038h,044h,044h,038h
            ; 9
            defb 038h,044h,044h,03Ch,004h,008h,030h
            ; 10
            defb 027h,065h,025h,025h,025h,025h,077h
            ; J
            defb 01Ch,008h,008h,008h,008h,048h,030h
            ; Q
            defb 038h,044h,044h,044h,054h,048h,034h
            ; K
            defb 044h,048h,050h,060h,050h,048h,044h

; palos pequenos: 4 x 7 bytes, a la derecha de la carta (byte 2)
cd_ssuit:
            ; picas
            defb 010h,038h,07Ch,0FEh,0FEh,010h,038h
            ; corazones
            defb 06Ch,0FEh,0FEh,0FEh,07Ch,038h,010h
            ; diamantes
            defb 010h,038h,07Ch,0FEh,07Ch,038h,010h
            ; treboles
            defb 038h,038h,07Ch,0FEh,0FEh,010h,038h

; palos grandes: 4 x 48 bytes (16 lineas x 3 bytes), a partir de la linea CD_BIG_LINE
cd_bsuit:
            ; picas
            defb 000h,018h,000h
            defb 000h,03Ch,000h
            defb 000h,07Eh,000h
            defb 000h,0FFh,000h
            defb 001h,0FFh,080h
            defb 003h,0FFh,0C0h
            defb 007h,0FFh,0E0h
            defb 00Fh,0FFh,0F0h
            defb 00Fh,0FFh,0F0h
            defb 00Fh,0FFh,0F0h
            defb 00Fh,0FFh,0F0h
            defb 007h,0DBh,0E0h
            defb 000h,03Ch,000h
            defb 000h,07Eh,000h
            defb 000h,0FFh,000h
            defb 000h,000h,000h
            ; corazones
            defb 003h,0C3h,0C0h
            defb 007h,0E7h,0E0h
            defb 00Fh,0FFh,0F0h
            defb 00Fh,0FFh,0F0h
            defb 00Fh,0FFh,0F0h
            defb 00Fh,0FFh,0F0h
            defb 00Fh,0FFh,0F0h
            defb 007h,0FFh,0E0h
            defb 003h,0FFh,0C0h
            defb 001h,0FFh,080h
            defb 000h,0FFh,000h
            defb 000h,07Eh,000h
            defb 000h,03Ch,000h
            defb 000h,018h,000h
            defb 000h,000h,000h
            defb 000h,000h,000h
            ; diamantes
            defb 000h,018h,000h
            defb 000h,03Ch,000h
            defb 000h,07Eh,000h
            defb 000h,0FFh,000h
            defb 001h,0FFh,080h
            defb 003h,0FFh,0C0h
            defb 007h,0FFh,0E0h
            defb 00Fh,0FFh,0F0h
            defb 00Fh,0FFh,0F0h
            defb 007h,0FFh,0E0h
            defb 003h,0FFh,0C0h
            defb 001h,0FFh,080h
            defb 000h,0FFh,000h
            defb 000h,07Eh,000h
            defb 000h,03Ch,000h
            defb 000h,018h,000h
            ; treboles
            defb 000h,018h,000h
            defb 000h,07Eh,000h
            defb 000h,07Eh,000h
            defb 000h,0FFh,000h
            defb 000h,0FFh,000h
            defb 000h,07Eh,000h
            defb 003h,0FFh,0C0h
            defb 007h,0E7h,0E0h
            defb 007h,0FFh,0E0h
            defb 007h,0FFh,0E0h
            defb 007h,0FFh,0E0h
            defb 007h,0FFh,0E0h
            defb 001h,0DBh,080h
            defb 000h,03Ch,000h
            defb 000h,03Ch,000h
            defb 000h,07Eh,000h

; letras grandes J Q K: 3 x 48 bytes, mismo formato que cd_bsuit
cd_bletter:
            ; J
            defb 000h,000h,000h
            defb 000h,003h,080h
            defb 000h,003h,080h
            defb 000h,003h,080h
            defb 000h,003h,080h
            defb 000h,003h,080h
            defb 000h,003h,080h
            defb 000h,003h,080h
            defb 000h,003h,080h
            defb 000h,003h,080h
            defb 001h,0C3h,080h
            defb 001h,0C3h,080h
            defb 001h,0FFh,080h
            defb 000h,0FFh,000h
            defb 000h,07Eh,000h
            defb 000h,000h,000h
            ; Q
            defb 000h,000h,000h
            defb 000h,07Ch,000h
            defb 001h,0FFh,000h
            defb 003h,0FFh,080h
            defb 003h,0C7h,080h
            defb 007h,083h,0C0h
            defb 007h,001h,0C0h
            defb 007h,001h,0C0h
            defb 007h,001h,0C0h
            defb 007h,009h,0C0h
            defb 007h,09Dh,0C0h
            defb 003h,0CFh,080h
            defb 003h,0FFh,080h
            defb 001h,0FFh,0C0h
            defb 000h,07Ch,0E0h
            defb 000h,000h,000h
            ; K
            defb 000h,000h,000h
            defb 003h,083h,0C0h
            defb 003h,087h,080h
            defb 003h,08Fh,000h
            defb 003h,09Eh,000h
            defb 003h,09Ch,000h
            defb 003h,0B8h,000h
            defb 003h,0FCh,000h
            defb 003h,0FEh,000h
            defb 003h,0CEh,000h
            defb 003h,087h,000h
            defb 003h,087h,000h
            defb 003h,083h,080h
            defb 003h,081h,0C0h
            defb 003h,081h,0C0h
            defb 000h,000h,000h

; dorso: 28 lineas x 3 bytes (lineas 2-29 de la carta)
cd_back:
            defb 000h,000h,000h
            defb 03Fh,0FFh,0FCh
            defb 022h,022h,024h
            defb 035h,055h,054h
            defb 028h,088h,08Ch
            defb 035h,055h,054h
            defb 022h,022h,024h
            defb 035h,055h,054h
            defb 028h,088h,08Ch
            defb 035h,055h,054h
            defb 022h,022h,024h
            defb 035h,055h,054h
            defb 028h,088h,08Ch
            defb 035h,055h,054h
            defb 022h,022h,024h
            defb 035h,055h,054h
            defb 028h,088h,08Ch
            defb 035h,055h,054h
            defb 022h,022h,024h
            defb 035h,055h,054h
            defb 028h,088h,08Ch
            defb 035h,055h,054h
            defb 022h,022h,024h
            defb 035h,055h,054h
            defb 028h,088h,08Ch
            defb 035h,055h,054h
            defb 03Fh,0FFh,0FCh
            defb 000h,000h,000h

; hueco vacio: 32 lineas x 3 bytes
cd_hole:
            defb 02Ah,0AAh,0A8h
            defb 040h,000h,002h
            defb 080h,000h,001h
            defb 000h,000h,000h
            defb 080h,000h,001h
            defb 000h,000h,000h
            defb 080h,000h,001h
            defb 000h,000h,000h
            defb 080h,000h,001h
            defb 000h,000h,000h
            defb 080h,000h,001h
            defb 000h,000h,000h
            defb 080h,000h,001h
            defb 000h,000h,000h
            defb 080h,000h,001h
            defb 000h,000h,000h
            defb 080h,000h,001h
            defb 000h,000h,000h
            defb 080h,000h,001h
            defb 000h,000h,000h
            defb 080h,000h,001h
            defb 000h,000h,000h
            defb 080h,000h,001h
            defb 000h,000h,000h
            defb 080h,000h,001h
            defb 000h,000h,000h
            defb 080h,000h,001h
            defb 000h,000h,000h
            defb 080h,000h,001h
            defb 000h,000h,000h
            defb 040h,000h,002h
            defb 02Ah,0AAh,0A8h
