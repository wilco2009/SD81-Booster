# Manual: teclado y mando Bluetooth (borrador, 27/11/2026)

Texto para el manual (`MANUAL/ES/SD81_Manual_ES.md` y su traducción), junto a la sección del ratón Bluetooth (9.10). Recoge todo lo que ve el usuario
del teclado y el mando BLE: los comandos de BASIC, la consola web, el emparejado, la traducción de las teclas y los límites. Probado en la placa el
26-27/11/2026 con un Logitech Pebble Keys 2 K380s, un mando Amazon Luna y el ratón Logitech M240. El detalle técnico está en `teclado_gamepad_plan.md`.

## 1. Qué es

Con el módulo WiFi (ESP32-C3), el SD81 Booster puede usar a la vez, por Bluetooth Low Energy:
- un **ratón** (`LOAD *MOUSE`, sección 9.10);
- un **teclado** (`LOAD *KEYB`): escribe en el ZX81 como su propio teclado;
- un **mando** (`LOAD *PAD`): hace de joystick para los juegos de siempre, por teclas, y los programas nuevos pueden leer sus botones y sticks.

Requisitos y límites:
- **Solo Bluetooth Low Energy** (el C3 no tiene Bluetooth clásico). Valen, por ejemplo, Logitech Pebble Keys 2 K380s, POP Keys, MX Keys; el mando Amazon
  Luna, los Xbox Wireless con firmware 5.x o posterior. No valen el K380 original, los mandos de PlayStation ni Switch, ni casi todos los 8BitDo.
  Para saber si un aparato es BLE: la app nRF Connect del móvil lo ve con el servicio HID (0x1812).
- **Uno de cada tipo y tres como mucho** (el C3 admite 3 conexiones y 3 vínculos).
- Teclado y mando vienen **parados** de fábrica: hay que emparejarlos una vez.
- Con los tres conectados el ESP32 va justo de memoria y de radio (WiFi y Bluetooth comparten antena): la web funciona, pero algo más lenta.

## 2. Comandos de BASIC

| Comando | Qué hace |
|---|---|
| `LOAD *KEYB THEN NEW` | Empareja un teclado nuevo (ventana de 60 s; sustituye al guardado). Si el teclado pide clave, `LOAD *KEYB` la enseña |
| `LOAD *KEYB THEN RUN` | Conecta con el teclado guardado (y lo activa) |
| `LOAD *KEYB THEN CLEAR` | Olvida el teclado guardado (solo el suyo: ratón y mando siguen emparejados) |
| `LOAD *KEYB THEN STOP` / `LOAD *KEYB STOP` | Para el teclado (no vuelve a conectar hasta `THEN RUN`) |
| `LOAD *KEYB` / `LOAD *KEYB TO A$` | Estado en texto de 16 caracteres (ver abajo) |
| `LOAD *KEYB TEXT` / `LOAD *KEYB GAME` | Modo del teclado: texto (para escribir) o juego (varias teclas a la vez) |
| `LOAD *PAD THEN NEW` / `RUN` / `CLEAR` / `STOP`, `LOAD *PAD STOP` | Lo mismo para el mando |
| `LOAD *PAD` / `LOAD *PAD TO A$` | Estado del mando |
| `LOAD *PAD "cadena"` | Las teclas del mando (sección 5) |

Textos de estado: `CONECTADO`, `EMPAREJANDO 45S` (segundos que quedan de la ventana), `CLAVE 123456` (el teclado espera esa clave), `BUSCANDO` (guardado y
activado, pero no está cerca o está apagado), `PARADO`, `SIN TECLADO` / `SIN MANDO` (ninguno guardado), `SIN MODULO WIFI`.

## 3. Emparejar

1. `LOAD *KEYB THEN NEW` (o `LOAD *PAD THEN NEW`): se abre una ventana de 60 s.
2. Pon el aparato en modo emparejado: en los Logitech, mantener pulsada 3 s la tecla de un canal Easy-Switch (la luz parpadea deprisa); en el Luna,
   su botón de emparejado.
3. **Teclado con clave:** muchos teclados piden teclear un número de 6 cifras. `LOAD *KEYB` lo enseña como `CLAVE 123456` (y la consola web, `esp ble kbd`).
   Tecléalo **en el propio teclado** y pulsa Enter, **antes de 30 s**. La clave es la misma en todos los intentos de esa ventana.
4. Cuando `LOAD *KEYB` dice `CONECTADO`, ya está. Queda guardado: al encender el ZX81, o cuando el teclado o el mando se despiertan (al pulsar una tecla o
   moverlo), vuelve a conectar solo en unos segundos.

Solo se empareja un aparato que esté **en modo emparejado** (el que solo busca volver a otro PC no se acepta) y del tipo que toca: un teclado no entra
como ratón ni al revés.

## 4. El teclado

**Modo texto** (por defecto): para escribir BASIC. Cada tecla que se pulsa entra en una cola y se le pasa al ZX81 de una en una (60 ms pulsada y 40 ms
suelta), porque la ROM del ZX81 no admite dos teclas a la vez: así no se pierden letras al escribir deprisa.
**Modo juego:** el teclado tal cual, varias teclas a la vez (para juegos); cada pulsación dura al menos 40 ms.
Se cambia con **Ctrl+Alt+Espacio**, con `LOAD *KEYB TEXT` / `GAME` o con `esp ble kbd text` / `game`.

Traducción (distribución **española** por defecto; la inglesa US con `esp ble kbd layout us`):
- **Por posición**, como un teclado de ZX81: letras, cifras, Espacio y Enter (NEW LINE). **Mayús = SHIFT** también con ellas: Mayús+P = `"`, Mayús+1 = EDIT,
  Mayús+2 = AND, Mayús+Enter = FUNCTION, Mayús+Espacio = `£`. En modo K la P da PRINT, como en la máquina.
- **Por carácter**, el resto de signos del PC: `:` `;` `?` `/` `,` `(` `)` `$` `"` `<` `>` `-` `+` `=` `*` `.` van a su combinación del ZX81, y `^` a `**`
  (potencia). En la española, `" $ ( ) = /` están en el PC sobre las cifras: esas teclas dan lo del ZX81 (AND, TO...), y esos signos se escriben en su
  sitio del ZX81 (Mayús+P, U, I, O, L, V) o, el `/`, en el teclado numérico.
- **Teclas especiales:** Retroceso y Supr = RUBOUT; cursores = SHIFT+5/6/7/8 (en modo juego, 5/6/7/8 sin SHIFT, como los leen los juegos);
  Esc = BREAK (Espacio); **F1 = EDIT, F2 = FUNCTION, F3 = GRAPHICS**. Con Ctrl, Alt o AltGr pulsados, en modo texto la tecla no escribe nada.
- Las teclas del teclado Bluetooth se suman a las del teclado del ZX81 y a las del teclado virtual de la web.

## 5. El mando

**Como teclas** (por defecto; es lo que usan los juegos de siempre): la cruceta **o** el stick izquierdo (pasada la mitad del recorrido) dan las cuatro
direcciones y los botones dan teclas. Sin configurar nada:

| Mando | Tecla |
|---|---|
| Cruceta / stick izquierdo | las direcciones de `LOAD *JOY` |
| A y B | el disparo de `LOAD *JOY` |
| START (Menu) | NEW LINE |
| el resto | nada |

**`LOAD *PAD "cadena"`** asigna una tecla a cada posición, **por separado del joystick DB9** (`LOAD *JOY`). Un carácter por posición, en este orden:
ARRIBA, ABAJO, IZQUIERDA, DERECHA, A, B, X, Y, LB, RB, LT, RT, SELECT, START, L3, R3, HOME (hasta 17; si la cadena es más corta, el resto no cambia).
- Cada carácter como en `LOAD *JOY` (letra, cifra, `.`, `<` = NEW LINE, `*` = SHIFT); **espacio** = la tecla ESPACIO.
- **`-`** = ninguna tecla; **`=`** = la misma que `LOAD *JOY` (su dirección, o el disparo en un botón).
- `LOAD *PAD ""` vuelve a lo de por defecto. Más de 17 caracteres o uno no válido: error B.
- Ejemplo: `LOAD *PAD "QAOPMM--<"` → arriba Q, abajo A, izquierda O, derecha P, A y B = M, X e Y nada, LB = NEW LINE.

Botones del **mando Amazon Luna** (comprobado en la placa): A, B, X, Y, LB, RB, LT y RT (gatillos analógicos), el del círculo (a la izquierda del
botón Luna) = SELECT, Menu (≡) = START, clic de los sticks = L3/R3, **botón Luna = HOME**. El del micrófono no da ninguna tecla. **Xbox Wireless:** la misma
numeración según su documentación (View = SELECT, Menu = START), sin probar.
(Pendiente: una figura del mando con los nombres.)

**Lectura directa** (programas nuevos): el comando 78 del MCU, op 0, devuelve botones (16 bits), cruceta, los dos sticks (−127..127) y los gatillos
(0..255). Para el apéndice de comandos del MCU (contrato en `teclado_gamepad_plan.md`). Ejemplo pendiente: `EXAMPLES/PADTEST`.

## 6. Consola web (`/debug`, órdenes `esp ble ...`)

| Orden | Qué hace |
|---|---|
| `esp ble` | Estado de los tres (y la memoria libre del ESP32) |
| `esp ble mouse\|kbd\|pad` | Estado de uno, con su historial (últimos estados, también de antes de un reinicio) |
| `esp ble mouse\|kbd\|pad pair [s]` | Emparejar uno nuevo (ventana de `s` segundos, 60 por defecto). Sin dispositivo, el ratón |
| `esp ble mouse\|kbd\|pad connect\|forget\|on\|off` | Conectar con el guardado, olvidarlo, activarlo o pararlo (se guarda) |
| `esp ble kbd text\|game` | Modo del teclado |
| `esp ble kbd layout es\|us` | Distribución del teclado (se guarda) |
| `esp ble reset` | Pone a cero las estadísticas |
| `esp ble crash [clear]`, `esp ble sniff [show]` | Diagnóstico: último fallo del programa guardado en la flash; anuncios BLE que se oyen alrededor |

## 7. Problemas frecuentes

- **No aparece al emparejar** (`EMPAREJANDO` hasta el final): el aparato no está en modo emparejado, o no es BLE (compruébalo con nRF Connect).
- **El teclado pide clave y no se empareja:** hay que teclearla en el teclado, con Enter, en menos de 30 s. `esp ble kbd` dice el motivo del último fallo.
- **Teclado o mando guardados que no vuelven:** pulsa una tecla o mueve el mando para despertarlo; si sigue en `BUSCANDO`, `THEN CLEAR` y emparejarlo otra vez.
- **Letras perdidas o repetidas al escribir deprisa:** modo texto (`LOAD *KEYB TEXT`).
- **Un juego no responde a dos teclas a la vez:** modo juego (Ctrl+Alt+Espacio).
