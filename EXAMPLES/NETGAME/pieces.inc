; Generado por gen_pieces.py a partir de piezas_16x16.png: no se edita a mano.
; Cada pieza son 32 bytes: celdas arriba-izquierda, arriba-derecha, abajo-izquierda y abajo-derecha
; (8 bytes cada una), en el orden P N B R Q K. Blancas y negras usan el mismo dibujo.
pc_solid:
            ; P
            defb 000h,000h,003h,007h,007h,007h,007h,003h
            defb 000h,000h,0C0h,0E0h,0E0h,0E0h,0E0h,0C0h
            defb 001h,001h,003h,003h,006h,00Fh,00Fh,000h
            defb 080h,080h,0C0h,0C0h,060h,0F0h,0F0h,000h
            ; N
            defb 000h,003h,007h,01Dh,07Fh,07Fh,01Fh,03Fh
            defb 000h,0F8h,0F0h,0F0h,0F8h,0FCh,0FEh,0FEh
            defb 003h,003h,007h,00Fh,01Ch,03Fh,03Fh,000h
            defb 0FEh,0FCh,0F8h,0F0h,018h,0FCh,0FCh,000h
            ; B
            defb 001h,003h,007h,00Bh,01Dh,03Eh,03Fh,03Fh
            defb 080h,0C0h,0E0h,0F0h,0F8h,0FCh,0FCh,0FCh
            defb 01Fh,00Fh,007h,007h,00Ch,01Fh,01Fh,000h
            defb 0F8h,0F0h,0E0h,0E0h,030h,0F8h,0F8h,000h
            ; R
            defb 000h,071h,071h,07Fh,07Fh,03Fh,018h,01Fh
            defb 000h,08Eh,08Eh,0FEh,0FEh,0FCh,018h,0F8h
            defb 01Fh,01Fh,01Fh,01Fh,038h,07Fh,07Fh,000h
            defb 0F8h,0F8h,0F8h,0F8h,01Ch,0FEh,0FEh,000h
            ; Q
            defb 001h,007h,00Eh,01Fh,05Bh,071h,04Eh,03Fh
            defb 080h,0E0h,070h,0F8h,0DAh,08Eh,072h,0FCh
            defb 01Fh,00Fh,008h,01Fh,03Ah,07Fh,07Fh,000h
            defb 0F8h,0F0h,010h,0F8h,05Ch,0FEh,0FEh,000h
            ; K
            defb 001h,003h,03Ah,06Eh,0C7h,0BAh,0BDh,0DDh
            defb 080h,0C0h,05Ch,076h,0E3h,05Dh,0BDh,0BBh
            defb 06Fh,02Fh,010h,01Fh,034h,03Fh,03Fh,000h
            defb 0F6h,0F4h,008h,0F8h,02Ch,0FCh,0FCh,000h
