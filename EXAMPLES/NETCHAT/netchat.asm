; =====================================================================
;  NETCHAT.ASM - prueba de la capa B del juego en red (net.inc): un
;  chat entre dos ZX81 por MQTT, sin servidor propio. Es la tabla de
;  saltos que usa el BASIC de prueba (NETCHAT.B81) y, a la vez, la forma
;  de llamar a la libreria desde BASIC con LET R=USR.
;
;  Memoria:
;      $6000-          este programa y net.inc (unos 3 KB)
;      $7000-$73EF     el buzon y las variables de net.inc (NB_BASE)
;
;  Tabla de saltos (BC vuelve con el resultado: LET R = USR 24576):
;      24576  net_init      ATZ, ATE0
;      24579  room_new      crea la sala; el codigo queda en 28680 (10 bytes)
;      24582  room_join     se une al codigo que haya en 28680
;      24585  room_send     manda PEEK 28677 bytes de 28704, turno en 28675
;      24588  room_poll     0 nada, 1 mensaje (28816, PEEK 28677), 2 el
;                           rival entra, 3 sale, 4 error del modem
;      24591  room_leave
;      24594  room_poll_send  manda la pregunta del sondeo y vuelve al momento
;      24597  room_poll_recv  recoge la respuesta (como room_poll)
;
;  Ensamblar: pasmo netchat.asm netchat.bin netchat.sym   (compila.bat)
; =====================================================================

NB_BASE     equ 7000h

            org  24576

t_init:     jp   e_init
t_new:      jp   e_new
t_join:     jp   e_join
t_send:     jp   e_send
t_poll:     jp   e_poll
t_leave:    jp   e_leave
t_psend:    jp   e_psend            ; 24594: manda la pregunta y vuelve
t_precv:    jp   e_precv            ; 24597: recoge la respuesta

; Cada entrada llama a la rutina de la libreria y devuelve el resultado en BC (LET R=USR ...). Funciona en SLOW
; y en FAST: no hace falta cambiar de modo.

e_init:     ld   hl,net_init
            jr   run
e_new:      ld   hl,room_new
            jr   run
e_join:     ld   hl,room_join
            jr   run
e_send:     ld   hl,room_send
            jr   run
e_poll:     ld   hl,room_poll
            jr   run
e_leave:    ld   hl,room_leave
            jr   run
e_psend:    ld   hl,room_poll_send
            jr   run
e_precv:    ld   hl,room_poll_recv
run:        call jp_hl
            ld   c,a                ; USR devuelve BC
            ld   b,0
            ret
jp_hl:      jp   (hl)

            include "net.inc"
