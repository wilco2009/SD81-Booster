# Cambios de la terminal TELNET del ZX81 para llevar al TERM de CP/M Plus

La terminal `EXAMPLES/TELNET/telnet.asm` (SD81-Booster) se hizo a partir
del TERM de CP/M Plus (`CPM3_SD81`):
- el bucle de `term.z80`;
- el emulador de terminal y el teclado de `chario.z80`, sin cambios de
  lógica.

Al probarla contra BBS reales salieron seis mejoras que el TERM de CP/M no
tiene. Aquí van, con el código del ZX81 como referencia. Todas están
probadas en el emulador y en el hardware real.

En el ZX81 las rutinas se llaman `con_co` (= `?co`), `kbget` (= `?ci`),
`con_cist` (= `?cist`), `net_read` y `net_write` (las mismas de `term.z80`).

| # | Qué | Dónde tocar en CP/M | Tamaño |
|---|---|---|---|
| 1 | Filtro de la negociación telnet (IAC) | `term.z80` | ~110 B |
| 2 | Flechas ANSI solo dentro de TERM (opción B: indicador en memoria común) | `term.z80` + `chario.z80` (`kbget`) + `memmap.inc` | ~25 B + ~40 B |
| 3 | Eco local de los controles como `^X` | `term.z80` | ~35 B |
| 4 | TAB recibido | `chario.z80` (`?co`) | ~16 B |
| 5 | `ATE1` al arrancar si no hay conexión | `term.z80` | ~40 B |
| 6 | Colgar al salir si hay conexión | `term.z80` | ~40 B |

---

## 1. Filtro de la negociación telnet (IAC)

**Problema.** Muchas BBS mandan la negociación telnet al conectar: bytes
`IAC` (`FFh`), `WILL`/`DO` y opciones. Ni el ESP32 ni TERM la tratan, así
que esos bytes se pintan como caracteres raros. Además la BBS se queda
esperando respuesta.

**Solución.** Cada byte recibido pasa por una máquina de estados antes de
imprimirse:
- **Se acepta** `WILL ECHO` y `WILL SGA`, contestando `DO`: el eco lo hace
  la BBS.
- **`DO SGA`** se contesta con `WILL`.
- **Cualquier otro `WILL`** se contesta con `DONT`, y cualquier otro `DO`
  con `WONT`.
- **`WONT` y `DONT`** no se contestan.
- **Las subnegociaciones** (`SB ... IAC SE`) se saltan enteras.
- **`IAC IAC`** es el byte 255 literal.

En `term.z80`, `pr_loop` llama a `BIOS_CONOUT` con cada byte. Hay que
llamar en su lugar a `rx_byte` (A = byte). `rx_byte` usa `net_write` para
contestar, así que en `pr_loop` hay que conservar HL y BC alrededor de la
llamada. Ojo: `net_write` destruye C, que es el reloj del handshake; ver
la nota de `term.z80`.

```asm
T_SE        equ 0F0h
T_SB        equ 0FAh
T_WILL      equ 0FBh
T_WONT      equ 0FCh
T_DO        equ 0FDh
T_DONT      equ 0FEh
T_IAC       equ 0FFh
T_ECHO      equ 1
T_SGA       equ 3

; A = byte recibido. Estados (tn_state): 0 normal, 1 detras de IAC,
; 2 esperando la opcion de WILL/WONT/DO/DONT, 3 dentro de SB, 4 IAC
; dentro de SB.
rx_byte:    ld   c,a
            ld   a,(tn_state)
            or   a
            jr   nz,tn_seq
            ld   a,c
            cp   T_IAC
            jp   nz,pintar          ; <- en CP/M: C = byte, a BIOS_CONOUT
            ld   a,1
            ld   (tn_state),a
            ret
tn_seq:     dec  a
            jr   nz,tn_s2
            ld   a,c                ; estado 1: el comando
            cp   T_IAC
            jr   nz,tn1
            xor  a                  ; IAC IAC = el byte 255
            ld   (tn_state),a
            jp   pintar
tn1:        cp   T_SB
            jr   nz,tn2
            ld   a,3
            ld   (tn_state),a
            ret
tn2:        cp   T_WILL
            jr   c,tn_end           ; NOP, GA...: comandos de 2 bytes
            ld   (tn_cmd),a
            ld   a,2
            ld   (tn_state),a
            ret
tn_s2:      dec  a
            jr   nz,tn_s3
            ld   a,c                ; estado 2: la opcion
            ld   (tn_opt),a
            xor  a
            ld   (tn_state),a
            ld   a,(tn_cmd)
            cp   T_WILL
            jr   nz,tn_do
            ld   a,(tn_opt)
            cp   T_ECHO
            jr   z,tn_rdo
            cp   T_SGA
            jr   z,tn_rdo
            ld   a,T_DONT
            jr   tn_send
tn_rdo:     ld   a,T_DO
            jr   tn_send
tn_do:      cp   T_DO
            ret  nz                 ; WONT/DONT: no se contestan
            ld   a,(tn_opt)
            cp   T_SGA
            ld   a,T_WILL
            jr   z,tn_send
            ld   a,T_WONT
tn_send:    ld   (tn_resp+1),a
            ld   a,(tn_opt)
            ld   (tn_resp+2),a
            ld   hl,tn_resp
            ld   b,3
            jp   net_write
tn_s3:      dec  a
            jr   nz,tn_s4
            ld   a,c                ; estado 3: dentro de SB, hasta IAC
            cp   T_IAC
            ret  nz
            ld   a,4
            ld   (tn_state),a
            ret
tn_s4:      ld   a,c                ; estado 4: IAC SE termina; otra cosa
            cp   T_SE               ; sigue dentro
            ld   a,3
            jr   nz,tn_st
tn_end:     xor  a
tn_st:      ld   (tn_state),a
            ret

tn_resp:    defb T_IAC,0,0
tn_state:   defb 0                  ; (y tn_cmd, tn_opt: 1 byte cada una)
```

Si el log está activo, lo lógico es grabar el byte crudo **antes** del
filtro, como ahora: el log sirve para ver lo que manda la otra punta.

---

## 2. Flechas: secuencias ANSI en vez de WordStar

**Problema.** SHIFT+5/6/7/8 dan `^S ^X ^E ^D`, las flechas de WordStar.
Son lo correcto para los programas de CP/M, pero una BBS espera las
secuencias ANSI: `ESC [ A` arriba, `ESC [ B` abajo, `ESC [ C` derecha y
`ESC [ D` izquierda. Con WordStar, las flechas no hacen nada.

**En el ZX81** se cambió el mapa de teclado: SHIFT+7/6/8/5 dan los códigos
internos 81h–84h, que ninguna otra tecla genera, y el bucle los convierte
en `ESC [ letra`:

```asm
KEY_UP      equ 081h            ; -> ESC [ A
KEY_DOWN    equ 082h            ; -> ESC [ B
KEY_RIGHT   equ 083h            ; -> ESC [ C
KEY_LEFT    equ 084h            ; -> ESC [ D

            ; (tras leer la tecla en A)
            cp   KEY_UP             ; una flecha?
            jr   c,kb_send
            cp   KEY_LEFT+1
            jr   nc,kb_send
            sub  KEY_UP-'A'         ; KEY_UP..KEY_LEFT -> 'A'..'D'
            ld   (arrow_seq+2),a
            ld   hl,arrow_seq       ; ESC [ letra
            ld   b,3
            call net_write
            jp   loop
kb_send:    ...

arrow_seq:  defb 01Bh,'[',0
```

### En CP/M: las flechas del sistema NO cambian

El mapa de `chario.z80` es de todo el sistema. WordStar, Turbo Pascal y el
resto de programas esperan las flechas de WordStar (`^E ^X ^D ^S`), así
que el mapa se queda como está. Las secuencias ANSI solo se generan
**mientras TERM está funcionando**.

**Decisión: opción B**, un indicador en memoria común. Si no cabe, la
opción A, descrita al final.

#### Opción B: indicador en memoria común

1. **El indicador.** Un byte, `TERM_ARROWS`, en una dirección **fija** de
   memoria común (banco 7). Así lo ven los dos lados: TERM, desde la TPA
   (banco de usuario), y `chario.z80`, desde el banco de sistema. La
   dirección tiene que ser fija y conocida por los dos, porque TERM no
   puede saber dónde deja el ensamblador las variables de `chario`.
   Candidatos:
   - el hueco libre detrás de `@ctbl`, en el bloque de `CTBLORG` (tiene la
     guarda `ds 0F704h-$`);
   - algún otro byte libre del banco 7, definido en `memmap.inc`.

   Por ejemplo:
   ```asm
   TERM_ARROWS equ 0F7xxh   ; memmap.inc: 0 = flechas WordStar, 1 = TERM
   ```
   Arranca a 0.

2. **En `chario.z80`, en `kbget`.** Solo en el camino de SHIFT+tecla
   (`ck_nse`, el que carga `keymap_shift`): con `TERM_ARROWS` a 1, los
   códigos `05h`, `18h`, `04h` y `13h` se convierten en `81h`, `82h`,
   `83h` y `84h` (arriba, abajo, derecha, izquierda). El modo CTRL
   (SHIFT+ENTER y la tecla) va por otro camino (`ck_pwait`/`ck_ctrl`), así
   que CTRL+E, CTRL+X, CTRL+D y CTRL+S siguen llegando como son. Es la
   ventaja frente a la opción A.

   ```asm
   ck_nse:     bit  0,a                ; SHIFT -> mayusculas/funciones
               jr   z,ck_tbl
               ld   hl,keymap_shift
               ld   a,(TERM_ARROWS)
               or   a
               jr   z,ck_tbl
               ld   a,(km_idx)         ; con TERM: la flecha como 81h-84h
               ld   c,a
               ld   b,0
               add  hl,bc
               ld   a,(hl)
               call arrow_code         ; A = codigo (cambiado si es flecha)
               or   a
               jp   z,ck_top
               jr   ck_mods
   ...
   ; A = codigo de keymap_shift -> 81h-84h si es una flecha WordStar
   arrow_code: cp   005h               ; ^E arriba
               ld   c,081h
               jr   z,ac_set
               cp   018h               ; ^X abajo
               ld   c,082h
               jr   z,ac_set
               cp   004h               ; ^D derecha
               ld   c,083h
               jr   z,ac_set
               cp   013h               ; ^S izquierda
               ld   c,084h
               ret  nz
   ac_set:     ld   a,c
               ret
   ```
   Son unos 35 bytes. Si no caben en el tramo apretado del XIOS,
   `arrow_code` puede ir en la zona de `ANSIORG`. Dentro de `ck_nse` solo
   quedarían la comprobación del indicador y la llamada.

3. **En `term.z80`.**
   - Al arrancar: `ld a,1 / ld (TERM_ARROWS),a`.
   - En todas las salidas: `xor a / ld (TERM_ARROWS),a`. Hoy solo es
     `done`; si se añade otra, también ahí.
   - Al leer una tecla, 81h–84h se convierten en `ESC [ letra`, igual que
     en el ZX81 (bloque de arriba).

4. **Si TERM no sale bien** (reset, cuelgue), el indicador se quedaría a 1
   y los demás programas recibirían 81h–84h en las flechas. Conviene
   ponerlo a 0 en el arranque en frío y en el arranque en caliente del
   BIOS: son unos 4 bytes en `?wboot` o donde se reinicie la consola.

#### Opción A, si B no cabe

Traducir solo en `term.z80`: cuando llegue `05h`, `18h`, `04h` o `13h`,
mandar `ESC [ A`, `B`, `C` o `D`. No toca el BIOS. El inconveniente: esos
cuatro CTRL tecleados a propósito también saldrían como flechas, y CTRL+S
es el XOFF que usan algunas BBS.

---

## 3. Eco local de los códigos de control como `^X`

**Problema.** Con el eco local, TERM pinta la tecla con `BIOS_CONOUT`. El
emulador de terminal ignora los códigos de control, y un ESC abre una
secuencia que se come la tecla siguiente. Así que ESC y los CTRL no se ven
nunca, aunque sí se mandan, y parece que no funcionan.

**Solución.** Los controles se enseñan en notación `^X`, como hacen los
terminales de siempre: ESC es `^[`, CTRL+C es `^C` y DEL es `^?`. BS y TAB
sí se ejecutan. El CR sigue sacando además un LF en pantalla, como ahora.

```asm
; A = tecla mandada -> eco local
echo_char:  cp   00Dh
            jr   nz,ec1
            ld   c,a
            call con_co             ; <- BIOS_CONOUT en CP/M
            ld   c,00Ah
            jp   con_co
ec1:        cp   008h
            jr   z,ec_out
            cp   009h
            jr   z,ec_out
            cp   07Fh               ; DEL -> ^?
            jr   z,ec_ctl
            cp   020h
            jr   nc,ec_out
ec_ctl:     push af
            ld   c,'^'
            call con_co
            pop  af
            xor  040h               ; ^A..^_ y ^? (7Fh xor 40h = 3Fh)
ec_out:     ld   c,a
            jp   con_co
```

En CP/M, `BIOS_CONOUT` toca registros. Hay que conservar lo que haga falta
alrededor de las llamadas; `push af`/`pop af` ya protege A.

---

## 4. TAB recibido (en `chario.z80`, `?co`)

**Problema.** `?co` ignora el `09h` (`cp 20h / ret c`). En CP/M casi nunca
llega, porque las funciones 2 y 9 del BDOS expanden el TAB antes de llamar
al BIOS. Pero TERM escribe directamente en `BIOS_CONOUT`, así que los
tabuladores que mandan las BBS se pierden y el texto sale descolocado.

**Solución.** Avanzar a la siguiente columna múltiplo de 8, y como mucho
hasta la última:

```asm
            cp   08h
            jr   z,co_bs
            cp   09h                ; <- nuevo
            jr   z,co_tab           ; <- nuevo
            cp   20h
            ret  c                  ; otros controles -> ignorar
            ...
co_tab:     ld   a,(cur_col)        ; TAB: a la siguiente columna multiplo
            and  0F8h               ; de 8
            add  a,8
            cp   SCRW
            jr   c,ct_ok
            ld   a,SCRW-1
ct_ok:      ld   (cur_col),a
            ret
```

Son unos 16 bytes, pero en `chario.z80` el tramo `$8000-$89FF` del XIOS
va justo. Si no cabe, `co_tab` puede ir en la zona de `ANSIORG` con el
resto de lo que se mudó; ahí solo haría falta el `jp`.

---

## 5. `ATE1` al arrancar si no hay conexión

**Problema.** Si otro programa ha dejado el eco del módem apagado (`ATE0`,
por ejemplo la prueba de red de SD81TEST), los comandos AT se teclean a
ciegas.

**Solución.** Al arrancar:
1. Se hace un `NET_READ` con `max = 0` para leer el estado de la conexión.
2. Si no es 2 (conectado), se manda `ATE1\r`.
3. Se espera medio segundo y se tira lo que llegue (el `OK` y, si acaso, el
   eco del propio comando).

Hace falta que `net_read` guarde el estado. En `term.z80` se lee y se
descarta (`call mcu_recv ; status (se ignora)`); basta con
`ld (net_st),a` detrás.

```asm
            ld   hl,rxbuf           ; sin conexion: eco del modem encendido
            ld   b,0
            call net_read           ; solo para el estado
            ld   a,(net_st)
            cp   2
            jr   z,loop
            ld   hl,at_e1
            ld   b,5                ; "ATE1",13
            call net_write
            ; esperar ~0,5 s (en el ZX81: 25 VSYNC del puerto $AF; en CP/M
            ; vale cualquier retardo)
            ld   b,8
drain:      push bc
            ld   hl,rxbuf
            ld   b,RXMAX
            call net_read           ; y tirar la respuesta
            pop  bc
            djnz drain

at_e1:      defb 'ATE1',13
```

---

## 6. Colgar al salir si hay conexión

**Problema.** Al salir con ENTER+0, la conexión se queda abierta en el
ESP32.

**Solución.** Si el estado es 2, se cuelga como un módem:
1. Un segundo de silencio.
2. `+++`.
3. Otro segundo de silencio.
4. `ATH\r`.

```asm
done:       ld   hl,rxbuf
            ld   b,0
            call net_read           ; estado de la conexion
            ld   a,(net_st)
            cp   2
            jr   nz,dn1
            ; esperar ~1,1 s
            ld   hl,at_esc          ; "+++"
            ld   b,3
            call net_write
            ; esperar ~1,1 s
            ld   hl,at_h            ; "ATH",13
            ld   b,4
            call net_write
dn1:        ...                     ; (lo de siempre al salir)
```

En el ZX81 las esperas cuentan los VSYNC del puerto `$AF`, que en CP/M
también están disponibles: `cursor_blink_poll` los usa. Hay que respetar
el tiempo de guarda de `+++`: el ESP32 exige un segundo de silencio antes
y después, o toma los `+` como datos.

---

## Referencia

Código completo: `EXAMPLES/TELNET/telnet.asm` del repositorio SD81-Booster.
Las rutinas son `rx_byte` (1), `do_kbd`/`kb_send` y `arrow_seq` (2),
`echo_char` (3), `co_tab` (4), el arranque tras `start` (5) y `done` (6).
