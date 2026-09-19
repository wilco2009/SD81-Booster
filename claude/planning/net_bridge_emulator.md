# Puente de red (BBS/telnet) — especificación para el emulador

Contrato de lo que ve el **Z80**. Sirve para las dos cosas a la vez: lo que
tiene que implementar el emulador, y lo que tendrá que cumplir el firmware
real en la fase 2 (los comandos MCU **todavía no existen** en el STM32 —
ahora mismo sólo está el transporte ESP32↔STM32, ver
[telnet_esp32_bridge.md](telnet_esp32_bridge.md)).

La idea es desarrollar el software de terminal contra el emulador y que al
pasarlo al hardware no haya que tocar nada.

## Lo que NO hay que emular

Todo el transporte entre el módulo WiFi y el STM32 (`CMD_NET_POLL`, el bit de
secuencia, los buffers circulares, el control de flujo por crédito) es
**invisible para el Z80**. El emulador no necesita nada de eso: le basta con
un socket TCP del sistema anfitrión y los dos comandos MCU de abajo.

## El modelo: un módem ya conectado

El Z80 sólo ve un flujo de bytes, como si tuviera un puerto serie con un módem
Hayes delante. No sabe nada de red: ni WiFi, ni sockets, ni direcciones. Por
eso los programas de BBS existentes funcionan sin cambios, y por eso el
emulador puede sustituir toda la parte de red por un socket normal.

Hay dos planos:

1. **El tubo** — comandos MCU 66/67, que mueven bytes.
2. **El módem** — comandos AT, que viajan *por dentro* de ese tubo como texto.

## 1. El tubo: comandos MCU 66 y 67

Mismo handshake que el resto de comandos MCU (`OUT (0A7h),A` / `IN A,(0AFh)`
con toggle del bit 7 por cada byte), idéntico a `cmd_f_read`/`cmd_f_write`. Si
el emulador ya ejecuta bien las rutinas `sd_fread`/`sd_fwrite` de `sddisk.z80`,
estos dos no añaden nada nuevo en ese plano.

### 66 (0x42) — NET_READ

```
Z80 -> MCU:  66, max
MCU -> Z80:  count, data[count], avail, status
```

- `max` (0-255): cuántos bytes como mucho se quieren.
- `count`: cuántos se devuelven de verdad, `min(max, disponibles)`. Puede ser 0.
- `avail`: cuántos quedan pendientes **después** de esta lectura, saturado a
  255. Con `max=0` el comando se convierte en un "¿hay algo?" barato que además
  refresca `status`, sin transferir datos.
- `status`: ver más abajo.

### 67 (0x43) — NET_WRITE

```
Z80 -> MCU:  67, count, data[count]
MCU -> Z80:  accepted, status
```

- `accepted`: cuántos bytes se admitieron. **Puede ser menor que `count`** si
  el buffer de salida está lleno; el Z80 tiene que reenviar el resto. Un
  emulador que siempre acepte todo dejaría pasar software que luego se atraganta
  en el hardware.

### status (los dos comandos)

| Valor | Significado |
|---|---|
| 0 | sin conexión |
| 1 | conectando |
| 2 | conectado |
| 3 | error / la conexión se cayó |

## Código Z80 de referencia

Esto es lo que el emulador debe hacer funcionar tal cual.

> **CUIDADO CON `C` Y CON `A`.** `C` es el reloj del handshake: `sd_clk` lo
> carga y `mcu_send`/`mcu_recv` lo invierten con `cpl` en **cada** byte. Usarlo
> para guardar nada no sólo pierde el valor, **desincroniza todas las esperas
> de toggle posteriores** y la trama se va al garete a partir de ahí. Si el
> llamador te pasa algo en `C` (le pasa a `?co` de CP/M3, que recibe ahí el
> carácter), sálvalo en memoria antes de hablar con el MCU.
>
> Y `A` no sobrevive a `mcu_send`: sale valiendo `ClkPort XOR C`. Hay que
> recargarlo antes de cualquier comprobación posterior.
>
> Las tres rutinas preservan `DE`, `HL` y `B`; destruyen `A` y `C`.

```asm
DataPort    equ 0A7h
ClkPort     equ 0AFh
CMD_NET_RD  equ 66
CMD_NET_WR  equ 67

; --- lee hasta B bytes en (HL).
;     Devuelve B = leidos, E = pendientes, A = status. HL sin tocar. --------
net_read:   push hl
            ld   d,b                ; D = max (B hace falta para el djnz)
            call sd_clk
            ld   a,CMD_NET_RD
            call mcu_send
            ld   a,d
            call mcu_send           ; max
            call mcu_recv
            ld   b,a                ; count -> contador del bucle
            ld   d,a                ; ...y copia en D, que si sobrevive.
                                    ; NUNCA en C: ahi vive el reloj.
            or   a                  ; A sigue siendo el count (mcu_recv lo deja)
            jr   z,nr_tail
nr_loop:    call mcu_recv
            ld   (hl),a
            inc  hl
            djnz nr_loop
nr_tail:    call mcu_recv           ; avail (pendientes tras esta lectura)
            ld   e,a
            call mcu_recv           ; status
            ld   b,d
            pop  hl
            ret

; --- envia B bytes desde (HL). Devuelve B = aceptados, A = status ----------
net_write:  call sd_clk
            ld   a,CMD_NET_WR
            call mcu_send
            ld   a,b
            call mcu_send           ; count
            ld   a,b                ; RECARGAR: mcu_send se ha comido A
            or   a
            jr   z,nw_tail
nw_loop:    ld   a,(hl)
            call mcu_send
            inc  hl
            djnz nw_loop
nw_tail:    call mcu_recv
            ld   b,a                ; aceptados
            call mcu_recv           ; status
            ret
```

`sd_clk`, `mcu_send` y `mcu_recv` son las de `sddisk.z80`, sin cambios.

## 2. El módem: comandos AT

Van **por dentro del tubo**, como texto: el Z80 los escribe con NET_WRITE y
lee las respuestas con NET_READ. Ni los comandos MCU ni el emulador necesitan
tratarlos de forma especial en el plano del tubo — es el "otro extremo" (el
ESP32 en el hardware, el emulador aquí) quien los interpreta.

**Dos modos.** Sin conexión, lo que escribe el Z80 se interpreta como AT. Con
conexión, va al socket tal cual.

| Comando | Efecto |
|---|---|
| `ATDT host:puerto` | abre el socket. Responde `CONNECT` o `NO CARRIER` |
| `ATH` | cuelga. Responde `OK` |
| `ATO` | vuelve a modo datos sin colgar. Responde `CONNECT` |
| `ATE0` / `ATE1` | eco de los comandos en modo comando, apagado/encendido |
| `ATZ` | reinicia al estado inicial. Responde `OK` |
| `+++` | escape a modo comando **sin** colgar. Responde `OK` |

Respuestas terminadas en CR/LF: `OK`, `CONNECT`, `NO CARRIER`, `ERROR`.

**`+++` necesita guard time**: un segundo de silencio antes y después, o una
transferencia binaria que contenga tres símbolos de suma tira la conexión a
media faena. El emulador debe implementarlo igual, o el software que se
desarrolle allí se comportará distinto en el hardware.

Cuando el socket se cierra por el otro lado, se manda `NO CARRIER` al Z80 y se
vuelve a modo comando.

## Comportamientos que hay que reproducir sí o sí

Son los que separan "funciona en el emulador" de "funciona en la máquina":

- [ ] **Lecturas cortas**: `NET_READ` puede devolver `count` menor que `max`,
      o 0, aunque la conexión esté viva. Los datos llegan a ráfagas.
- [ ] **Escrituras parciales**: `NET_WRITE` puede aceptar menos de lo pedido.
- [ ] **El buffer de entrada es finito (2 KB)**: si el Z80 no lee, el emisor
      acaba parándose. Un emulador con buffer infinito esconde el problema de
      un terminal que no da abasto pintando.
- [ ] **Latencia no nula**: en el hardware los datos aparecen en el buffer con
      el ritmo del sondeo del ESP32 (~15 ms con conexión abierta). Si el
      emulador entrega los bytes instantáneamente, un bucle de terminal que
      dependa de temporización puede comportarse distinto.
- [ ] **`status` cambia solo**: la conexión se puede caer en cualquier momento
      sin que el Z80 haga nada.

## Por debajo, en el emulador

- Socket TCP normal del anfitrión, sin negociación telnet (IAC/DO/WILL). Si
  algún día hace falta, se mastica en el emulador igual que se hará en el
  ESP32, y al Z80 le siguen llegando datos limpios.
- El destino lo fija `ATDT`. En el hardware hay además una página web
  (`/telnet`) que hace lo mismo; el emulador puede tener su equivalente en el
  menú o en la línea de órdenes, pero **ATDT es el camino que usará el
  software**, así que es el que no puede faltar.

## Estado

| Pieza | Estado |
|---|---|
| Transporte ESP32↔STM32 (`CMD_NET_POLL`) | **validado en hardware real** |
| Comandos MCU 66/67 en el STM32 | **validado en hardware real** |
| Intérprete AT en el ESP32 | implementado (`NET_BRIDGE.cpp`), sin probar |
| Emulador | este documento |

El intérprete AT vive por completo en el ESP32 — el tubo (comandos 66/67)
no sabe nada de él, así que el Z80 escribe/lee exactamente igual con o sin
AT de por medio. Detalle de implementación en
[telnet_esp32_bridge.md](telnet_esp32_bridge.md).
