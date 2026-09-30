; logo.inc - el logo "EXPLORER" de la cabecera antigua (bg_row0.bin,
; columnas 9-22), con cada pixel duplicado en horizontal: en 70
; columnas el pixel dura la mitad que en HiRes Spectrum, y asi conserva
; su tamaño en pantalla. 28 caracteres (codigos LOGO..LOGO+27, de 128
; en adelante), precomplementados porque el hardware invierte los que
; llevan el bit 7: un bit a 1 del dibujo sale en tinta (negro) y uno a 0
; en papel (rojo).
logo_glyphs:
        defb 000h,000h,0FFh,0C0h,0FFh,000h,0FFh,000h   ; 0
        defb 000h,000h,0FFh,000h,0FFh,000h,0FFh,000h   ; 1
        defb 000h,000h,0CFh,00Ch,0CCh,0CCh,0CFh,000h   ; 2
        defb 000h,000h,0FFh,000h,000h,000h,0FFh,000h   ; 3
        defb 000h,00Ch,0FCh,00Ch,00Ch,00Ch,0FCh,000h   ; 4
        defb 000h,000h,0FFh,0C0h,0FFh,0C0h,0FFh,000h   ; 5
        defb 000h,000h,0FFh,000h,0FFh,000h,0FFh,000h   ; 6
        defb 000h,000h,0CCh,0CCh,0CCh,0CCh,0CCh,000h   ; 7
        defb 000h,000h,000h,000h,000h,000h,000h,000h   ; 8
        defb 000h,030h,03Fh,030h,030h,030h,03Fh,000h   ; 9
        defb 000h,000h,0FFh,000h,000h,000h,0FFh,000h   ; 10
        defb 000h,000h,0F3h,033h,033h,033h,0F3h,000h   ; 11
        defb 000h,000h,0FFh,000h,000h,000h,0FFh,000h   ; 12
        defb 000h,000h,0FFh,003h,003h,003h,0FFh,000h   ; 13
        defb 000h,000h,03Fh,030h,030h,030h,03Fh,000h   ; 14
        defb 000h,000h,0FFh,000h,000h,000h,0FFh,000h   ; 15
        defb 000h,000h,0F3h,033h,033h,030h,0F3h,000h   ; 16
        defb 000h,000h,0FFh,000h,0FFh,000h,0FFh,000h   ; 17
        defb 000h,000h,0FFh,000h,0FFh,003h,0FFh,000h   ; 18
        defb 000h,030h,03Fh,030h,030h,030h,03Fh,000h   ; 19
        defb 000h,000h,0FFh,000h,000h,000h,0FFh,000h   ; 20
        defb 000h,000h,0F3h,003h,003h,003h,0F3h,000h   ; 21
        defb 000h,000h,0FFh,000h,0FFh,000h,0FFh,000h   ; 22
        defb 000h,000h,0FFh,003h,0FFh,000h,0FFh,000h   ; 23
        defb 000h,000h,03Fh,030h,030h,030h,030h,000h   ; 24
        defb 000h,000h,0FFh,000h,000h,000h,000h,000h   ; 25
        defb 000h,000h,0F0h,000h,000h,000h,000h,000h   ; 26
        defb 000h,000h,000h,000h,000h,000h,000h,000h   ; 27
LOGO_LEN        equ $-logo_glyphs
