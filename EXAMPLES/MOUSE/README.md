# MOUSE: leer el ratón Bluetooth desde el Z80

Dos formas de leerlo:

- **Puerto de la FPGA** (`mouseport.inc`, `mp_read`): sin comando y **sin esperas**. Para el bucle de un juego. `OUT ($3FEF),$24` y 8 `IN` (x, y, botones, rueda, rawx, rawy, seq, estado), unos 100 µs. `rawx`, `rawy` y la rueda son contadores de 8 bits que dan la vuelta: restando la lectura anterior sale el movimiento relativo; `seq` cambia con cada paquete nuevo. Necesita la FPGA con `sim_int 0.21` (índice 20 de `$3FEF`).
- **Comando 77 del MCU** (`mouse.inc`): lectura con espera activa (~0,3 ms) y el único modo de **centrar** y **configurar** (rango y sensibilidad), cosas que no hacen falta en cada fotograma. `MOUSETEST` lo usa así: configura y centra con el comando y lee con el puerto.

El ratón lo lleva el ESP32 (cliente BLE) y el STM32 integra sus movimientos en una posición; el Z80 solo la **lee**.

## MOUSELIB: la librería para programas

`mouselib.inc` incluye `mouse.inc` y `mouseport.inc` y añade lo que casi todo programa necesita: eventos de botón (pulsado, soltado, **doble clic**), la rueda como incremento con signo, `ml_hit` (¿está el puntero en este rectángulo?), `ml_cell` (¿en qué celda?) y un **cursor por hardware** (el sprite 63, con flecha por defecto o la forma que se le pase), que no borra ni repinta nada. El detalle de variables y rutinas está en la cabecera del fichero.

```
MS_BASE     equ 7C00h              ; 28 bytes de buzón
            ...
            ld   bc,191*256+255    ; Y máximo, X máximo
            call ml_init
            call ml_cur_init
bucle:      call ml_poll           ; una vez por vuelta
            call ml_cur_show
            ld   a,(ML_PRESS)      ; bit0 = clic izquierdo nuevo, bit1 = derecho, bit2 = central
            ...
            include "mouselib.inc"
```

`MLTEST.B81` + `MLTEST.BIN` (`mltest.asm`) la prueban: tres botones de texto (clic = enciende/apaga, doble clic en el tercero = sale), los botones del ratón en pantalla y un contador que mueve la rueda. Sin probar en la placa. El doble clic mide con `FRAMES` (la ROM lo actualiza en SLOW y la FPGA lo simula en FAST). Necesita la ROM sin `LOAD *ROMLOCK` (el sprite escribe en 2100-2131).

## Comando 77 (0x4D) `MOUSE` del MCU

## Probarlo

1. Graba el ESP32, el STM32 y la **FPGA** (`sim_int 0.21` + `mouse_regs.v`) con la versión que lleva el ratón (ver `claude/planning/raton_bluetooth.md`).
2. Empareja el ratón (una sola vez): consola web del módulo WiFi (`/debug`), `esp ble pair`, y pon el ratón en modo emparejado. `esp ble` debe decir "conectado". Después se reconecta solo; `esp ble connect` y `esp ble forget` conectan y olvidan el guardado.
3. Copia `MOUSETEST.BIN` y `MOUSETEST.B81` a la misma carpeta de la SD, carga el `.B81` y `RUN`. Es un pequeño paint **entero en ensamblador** (`mousetest.asm`): **botón izquierdo dibuja, derecho borra, cualquier tecla sale**.
   (`compila.bat` regenera el `.bin` con pasmo.) Necesita modo SLOW (el `.B81` lo pone) y el D-FILE expandido (el `.B81` lo prepara con `PRINT AT I,31`).
   No toca IX, IY, I ni los registros alternativos, que el NMI y la ROM usan en SLOW.

## Usarlo desde tu programa

```
MS_BASE     equ 7C00h          ; buzón: 7 bytes de resultado + 3 de configuración
            include "mouse.inc"
            ...
            call mouse_read    ; MS_X, MS_Y, MS_BTN, MS_DX, MS_DY, MS_WHEEL, MS_STATUS
```

| Campo | Qué es |
|---|---|
| `MS_X`, `MS_Y` | posición absoluta, 0..255 y 0..191 |
| `MS_BTN` | bit0 izquierdo, bit1 derecho, bit2 central |
| `MS_DX`, `MS_DY`, `MS_WHEEL` | lo movido **desde la lectura anterior**, con signo (-128..127); se ponen a cero al leer |
| `MS_STATUS` | bit0: el MCU ha recibido algo del ratón desde que arrancó; bit1: hay datos nuevos desde la lectura anterior |

`mouse_center` centra el cursor; `mouse_config` fija el rango y la sensibilidad (`MS_CFG_X`, `MS_CFG_Y`, `MS_CFG_DIV`; 0 = no cambiar; por defecto 255, 191 y 4 cuentas por píxel).

## Protocolo del comando

```
Z80 -> MCU:  77, op
op 0  LEER     MCU -> Z80:  x, y, botones, dx, dy, rueda, estado    (7 bytes)
op 1  CENTRAR  MCU -> Z80:  estado
op 2  CONFIG   Z80 -> MCU:  max_x, max_y, div     MCU -> Z80:  estado
op 3  BLE      Z80 -> MCU:  orden, argumento      MCU -> Z80:  0 = aceptada, 0x80 = orden no valida
op 4  BLE      MCU -> Z80:  estado, segundos
otro op:       MCU -> Z80:  0x80
```

Handshake igual que `NET_READ`/`NET_WRITE` (C es el reloj; `A` no sobrevive a `mu_send`). Cada lectura cuesta ~0,3 ms.

## Emparejar desde BASIC: `LOAD *MOUSE` (ROM modificada)

| Comando | Hace |
|---|---|
| `LOAD *MOUSE THEN NEW` | empareja un ratón nuevo (ventana de 60 s, sustituye al guardado) |
| `LOAD *MOUSE THEN RUN` | conecta con el ratón guardado |
| `LOAD *MOUSE THEN CLEAR` | desempareja (olvida el guardado y sus claves) |
| `LOAD *MOUSE THEN STOP` o `LOAD *MOUSE STOP` | desconecta y no vuelve a conectar hasta `THEN RUN` |
| `LOAD *MOUSE` / `LOAD *MOUSE TO A$` | muestra / guarda el estado en 16 caracteres: `CONECTADO`, `EMPAREJANDO 45S`, `BUSCANDO`, `PARADO`, `SIN RATON`, `SIN MODULO WIFI` (op 5) |

Necesita la ROM nueva (`z80rom/SDBOOST.ROM`, comando `*MOUSE`). Es el comando 77, op 3; no espera el resultado (el ESP32 recoge la orden en ~0,5 s). Para ver el resultado, espera un segundo y usa `LOAD *MOUSE`.

### Sin la ROM nueva (ops 3 y 4 con `USR`)

`MOUSEBLE.B81` / `MOUSEBLE.BIN` (`mouseble.asm`): menú de BASIC con dos `USR`, sin tocar la ROM.

```
POKE 31755,orden : POKE 31756,arg : LET R=USR 24576     R = 0 aceptada, 128 orden no válida
LET S=USR 24579 : LET T=PEEK 31758                      estado y segundos de ventana
```

| orden | hace | arg |
|---|---|---|
| 1 | emparejar (sustituye al guardado) | segundos de la ventana, 0 = 60 |
| 2 | conectar con el guardado | |
| 3 | olvidar el guardado | |
| 4 / 5 | activar / parar el módulo | |

Estado `S`: bit0 conectado, bit1 emparejando, bit2 activado, bit3 hay ratón guardado, bit7 el módulo WiFi ya ha contestado (con el bit7 a cero lo demás no vale).
El STM32 no puede iniciar tramas con el ESP32: deja la orden pendiente y el ESP32 la recoge en ~0,5 s (trama `CMD_BLE`), así que el estado tarda un momento en reflejarla. Desde ensamblador: `mouse_ble_order` y `mouse_ble_status` de `mouse.inc`.

## Pendiente

- Un `LOAD *MOUSE` en la ROM modificada (`z80rom/`), que use estos mismos ops 3 y 4.
- Emulador: ver `claude/planning/raton_mcu_emulador.md`.
