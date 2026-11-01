; paletas de 16 colores de prueba (gen_pal16.py), 32 bytes cada una: bajo {G,B}, alto {BRIGHT,R}
; GREY
            defb 000h,000h           ;  0  R 0 G 0 B 0 W 0
            defb 011h,011h           ;  1  R 1 G 1 B 1 W 1
            defb 022h,022h           ;  2  R 2 G 2 B 2 W 2
            defb 033h,033h           ;  3  R 3 G 3 B 3 W 3
            defb 044h,044h           ;  4  R 4 G 4 B 4 W 4
            defb 055h,055h           ;  5  R 5 G 5 B 5 W 5
            defb 066h,066h           ;  6  R 6 G 6 B 6 W 6
            defb 077h,077h           ;  7  R 7 G 7 B 7 W 7
            defb 088h,088h           ;  8  R 8 G 8 B 8 W 8
            defb 099h,099h           ;  9  R 9 G 9 B 9 W 9
            defb 0AAh,0AAh           ; 10  R10 G10 B10 W10
            defb 0BBh,0BBh           ; 11  R11 G11 B11 W11
            defb 0CCh,0CCh           ; 12  R12 G12 B12 W12
            defb 0DDh,0DDh           ; 13  R13 G13 B13 W13
            defb 0EEh,0EEh           ; 14  R14 G14 B14 W14
            defb 0FFh,0FFh           ; 15  R15 G15 B15 W15
; RED
            defb 000h,000h           ;  0  R 0 G 0 B 0 W 0
            defb 000h,001h           ;  1  R 1 G 0 B 0 W 0
            defb 000h,002h           ;  2  R 2 G 0 B 0 W 0
            defb 000h,003h           ;  3  R 3 G 0 B 0 W 0
            defb 000h,004h           ;  4  R 4 G 0 B 0 W 0
            defb 000h,005h           ;  5  R 5 G 0 B 0 W 0
            defb 000h,006h           ;  6  R 6 G 0 B 0 W 0
            defb 000h,007h           ;  7  R 7 G 0 B 0 W 0
            defb 000h,008h           ;  8  R 8 G 0 B 0 W 0
            defb 000h,009h           ;  9  R 9 G 0 B 0 W 0
            defb 000h,00Ah           ; 10  R10 G 0 B 0 W 0
            defb 000h,00Bh           ; 11  R11 G 0 B 0 W 0
            defb 000h,00Ch           ; 12  R12 G 0 B 0 W 0
            defb 000h,00Dh           ; 13  R13 G 0 B 0 W 0
            defb 000h,00Eh           ; 14  R14 G 0 B 0 W 0
            defb 000h,00Fh           ; 15  R15 G 0 B 0 W 0
; GREEN
            defb 000h,000h           ;  0  R 0 G 0 B 0 W 0
            defb 010h,000h           ;  1  R 0 G 1 B 0 W 0
            defb 020h,000h           ;  2  R 0 G 2 B 0 W 0
            defb 030h,000h           ;  3  R 0 G 3 B 0 W 0
            defb 040h,000h           ;  4  R 0 G 4 B 0 W 0
            defb 050h,000h           ;  5  R 0 G 5 B 0 W 0
            defb 060h,000h           ;  6  R 0 G 6 B 0 W 0
            defb 070h,000h           ;  7  R 0 G 7 B 0 W 0
            defb 080h,000h           ;  8  R 0 G 8 B 0 W 0
            defb 090h,000h           ;  9  R 0 G 9 B 0 W 0
            defb 0A0h,000h           ; 10  R 0 G10 B 0 W 0
            defb 0B0h,000h           ; 11  R 0 G11 B 0 W 0
            defb 0C0h,000h           ; 12  R 0 G12 B 0 W 0
            defb 0D0h,000h           ; 13  R 0 G13 B 0 W 0
            defb 0E0h,000h           ; 14  R 0 G14 B 0 W 0
            defb 0F0h,000h           ; 15  R 0 G15 B 0 W 0
; BLUE
            defb 000h,000h           ;  0  R 0 G 0 B 0 W 0
            defb 001h,000h           ;  1  R 0 G 0 B 1 W 0
            defb 002h,000h           ;  2  R 0 G 0 B 2 W 0
            defb 003h,000h           ;  3  R 0 G 0 B 3 W 0
            defb 004h,000h           ;  4  R 0 G 0 B 4 W 0
            defb 005h,000h           ;  5  R 0 G 0 B 5 W 0
            defb 006h,000h           ;  6  R 0 G 0 B 6 W 0
            defb 007h,000h           ;  7  R 0 G 0 B 7 W 0
            defb 008h,000h           ;  8  R 0 G 0 B 8 W 0
            defb 009h,000h           ;  9  R 0 G 0 B 9 W 0
            defb 00Ah,000h           ; 10  R 0 G 0 B10 W 0
            defb 00Bh,000h           ; 11  R 0 G 0 B11 W 0
            defb 00Ch,000h           ; 12  R 0 G 0 B12 W 0
            defb 00Dh,000h           ; 13  R 0 G 0 B13 W 0
            defb 00Eh,000h           ; 14  R 0 G 0 B14 W 0
            defb 00Fh,000h           ; 15  R 0 G 0 B15 W 0
; YELLOW
            defb 000h,000h           ;  0  R 0 G 0 B 0 W 0
            defb 010h,001h           ;  1  R 1 G 1 B 0 W 0
            defb 020h,002h           ;  2  R 2 G 2 B 0 W 0
            defb 030h,003h           ;  3  R 3 G 3 B 0 W 0
            defb 040h,004h           ;  4  R 4 G 4 B 0 W 0
            defb 050h,005h           ;  5  R 5 G 5 B 0 W 0
            defb 060h,006h           ;  6  R 6 G 6 B 0 W 0
            defb 070h,007h           ;  7  R 7 G 7 B 0 W 0
            defb 080h,008h           ;  8  R 8 G 8 B 0 W 0
            defb 090h,009h           ;  9  R 9 G 9 B 0 W 0
            defb 0A0h,00Ah           ; 10  R10 G10 B 0 W 0
            defb 0B0h,00Bh           ; 11  R11 G11 B 0 W 0
            defb 0C0h,00Ch           ; 12  R12 G12 B 0 W 0
            defb 0D0h,00Dh           ; 13  R13 G13 B 0 W 0
            defb 0E0h,00Eh           ; 14  R14 G14 B 0 W 0
            defb 0F0h,00Fh           ; 15  R15 G15 B 0 W 0
; CYAN
            defb 000h,000h           ;  0  R 0 G 0 B 0 W 0
            defb 011h,000h           ;  1  R 0 G 1 B 1 W 0
            defb 022h,000h           ;  2  R 0 G 2 B 2 W 0
            defb 033h,000h           ;  3  R 0 G 3 B 3 W 0
            defb 044h,000h           ;  4  R 0 G 4 B 4 W 0
            defb 055h,000h           ;  5  R 0 G 5 B 5 W 0
            defb 066h,000h           ;  6  R 0 G 6 B 6 W 0
            defb 077h,000h           ;  7  R 0 G 7 B 7 W 0
            defb 088h,000h           ;  8  R 0 G 8 B 8 W 0
            defb 099h,000h           ;  9  R 0 G 9 B 9 W 0
            defb 0AAh,000h           ; 10  R 0 G10 B10 W 0
            defb 0BBh,000h           ; 11  R 0 G11 B11 W 0
            defb 0CCh,000h           ; 12  R 0 G12 B12 W 0
            defb 0DDh,000h           ; 13  R 0 G13 B13 W 0
            defb 0EEh,000h           ; 14  R 0 G14 B14 W 0
            defb 0FFh,000h           ; 15  R 0 G15 B15 W 0
; MAGENTA
            defb 000h,000h           ;  0  R 0 G 0 B 0 W 0
            defb 001h,001h           ;  1  R 1 G 0 B 1 W 0
            defb 002h,002h           ;  2  R 2 G 0 B 2 W 0
            defb 003h,003h           ;  3  R 3 G 0 B 3 W 0
            defb 004h,004h           ;  4  R 4 G 0 B 4 W 0
            defb 005h,005h           ;  5  R 5 G 0 B 5 W 0
            defb 006h,006h           ;  6  R 6 G 0 B 6 W 0
            defb 007h,007h           ;  7  R 7 G 0 B 7 W 0
            defb 008h,008h           ;  8  R 8 G 0 B 8 W 0
            defb 009h,009h           ;  9  R 9 G 0 B 9 W 0
            defb 00Ah,00Ah           ; 10  R10 G 0 B10 W 0
            defb 00Bh,00Bh           ; 11  R11 G 0 B11 W 0
            defb 00Ch,00Ch           ; 12  R12 G 0 B12 W 0
            defb 00Dh,00Dh           ; 13  R13 G 0 B13 W 0
            defb 00Eh,00Eh           ; 14  R14 G 0 B14 W 0
            defb 00Fh,00Fh           ; 15  R15 G 0 B15 W 0
