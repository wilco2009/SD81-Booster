# SD81 Booster — Actualización de firmware

Este documento explica cómo actualizar el firmware del SD81 Booster.

> Si quieres recompilar el firmware desde el código fuente, consulta [COMPILING.md](COMPILING.md) en su lugar.

---

## Estructura del firmware

El SD81 Booster tiene dos componentes programables, cada uno actualizable de forma independiente:

### MCU (STM32F407VET)

El firmware del MCU consta de dos partes independientes, cada una grabada en una región distinta:

| Parte | Binario | Dirección flash | Descripción |
|------|--------|--------------|-------------|
| **Bootloader** | `bootloader.bin` | `0x08000000` | Se ejecuta primero al encender. Comprueba si hay un fichero `firmware.bin` en la tarjeta SD y lo graba automáticamente. Ocupa los sectores 0–2 (48 KB). |
| **Aplicación** | `firmware.bin` | `0x0800C000` | El firmware principal. Lo carga el bootloader tras una actualización, o directamente en un arranque normal. |

En circunstancias normales, **solo hace falta actualizar la aplicación**. El bootloader rara vez cambia.

### FPGA (Xilinx Spartan-6 XC6SLX9)

| Fichero | Descripción |
|------|-------------|
| `SD81.mcs` | Imagen MCS que se graba en la flash SPI auxiliar (25Q128) conectada a la FPGA. |

La FPGA carga su configuración desde esta flash SPI al encender. Consulta [Actualización de la FPGA (vía tarjeta SD)](#actualización-de-la-fpga-vía-tarjeta-sd) más abajo para ver las instrucciones de actualización.

Todos los binarios están incluidos en esta carpeta.

---

## Actualización normal (vía tarjeta SD)

Este es el método recomendado para todos los usuarios.

1. Copia `firmware.bin` desde esta carpeta a la raíz de la tarjeta microSD.
2. Inserta la tarjeta SD en el interface.
3. Enciende el ZX81.
4. El bootloader detecta `firmware.bin`, lo graba en `0x0800C000`, borra el fichero de la tarjeta SD y arranca automáticamente la nueva aplicación.

El LED STAT muestra lo siguiente durante el proceso:

| Estado del LED | Significado |
|-----------|---------|
| Amarillo fijo | Grabación en curso — **no apagues el equipo** |
| Verde fijo | Actualización correcta, aplicación en ejecución |
| Azul/Rojo parpadeando | Error al leer la tarjeta SD |
| Blanco/Rojo parpadeando | Error al escribir en la flash |

### Verificar la actualización

Una vez que el ZX81 haya arrancado, ejecuta el siguiente comando para comprobar la versión del firmware:

```
LOAD *VER
```

---

## Recuperación de emergencia (vía USB)

Usa este método solo si la actualización normal por SD falla o si la aplicación está corrupta y el interface no arranca correctamente. Requiere abrir la carcasa.

**Requisitos:**
- [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html) (descarga gratuita de ST)
- Un cable USB-C

**Pasos:**

1. **Desconecta** el interface del ZX81.
2. Abre la carcasa del interface para acceder a la placa.
3. Localiza el jumper **JP7** en la cara de componentes de la placa, junto al puerto USB-C.
4. Puentea los **dos pines superiores** de JP7 para poner el MCU en modo de programación.
5. Conecta el cable USB-C al interface y al PC.
6. Espera a que el PC reconozca el dispositivo (debería aparecer como dispositivo DFU en STM32CubeProgrammer).
7. Graba los dos binarios en orden:

| Paso | Binario | Dirección |
|------|--------|---------|
| 1 | `bootloader.bin` | `0x08000000` |
| 2 | `firmware.bin` | `0x0800C000` |

   Dos formas de hacerlo — en ninguna de las dos hace falta el IDE de Arduino ni compilar desde el código, solo STM32CubeProgrammer:

   - **Con script (recomendado):** haz doble clic en `bootloader.bat` y/o `firmware.bat`, incluidos en esta carpeta. Cada uno llama a la herramienta de línea de comandos de STM32CubeProgrammer (a través de `stm32CubeProg.sh`, ejecutado con el `busybox.exe` incluido) para grabar el binario correspondiente en su dirección, automáticamente, por DFU.
   - **Manual (interfaz gráfica de STM32CubeProgrammer):** conéctate en modo DFU y descarga cada binario en su dirección según la tabla de arriba.

8. Una vez terminada la grabación, **retira el puente de JP7**.
9. Cierra la carcasa y vuelve a conectar el interface al ZX81.

> **Nota:** Graba el bootloader únicamente si se sabe que está corrupto. En la mayoría de los casos basta con grabar solo la aplicación (`0x0800C000`) — simplemente ejecuta `firmware.bat` (o descarga solo `firmware.bin` desde la interfaz gráfica).

---

## Diagnóstico del MCU vía consola serie por USB

Si una actualización falla, o el interface se comporta de forma extraña y quieres ver qué está pasando realmente, conecta el puerto USB-C a un PC (**conexión normal, no hace falta puentear JP7 para esto**) y abre una terminal serie — sirve el propio Monitor Serie del IDE de Arduino, o cualquier otro programa de terminal (PuTTY, Tera Term, minicom, CoolTerm, `screen`...).

**Configuración de la terminal:**

| Parámetro | Valor |
|---------|-------|
| Velocidad (baud rate) | 115200 |
| Bits de datos | 8 |
| Paridad | Ninguna |
| Bits de parada | 1 |

(es decir, **115200 8N1**.)

La consola muestra en tiempo real el progreso del arranque, errores de acceso a la SD, el progreso de las actualizaciones de firmware y otros mensajes de estado/depuración del MCU.

---

## Actualización de la FPGA (vía tarjeta SD)

Este es el método recomendado — no hace falta cable JTAG ni herramientas de Xilinx.

Igual que con el MCU, el SD81 Booster puede reprogramar automáticamente la flash SPI auxiliar de la FPGA (25Q128) desde la tarjeta microSD.

1. Copia `SD81.mcs` desde esta carpeta a la raíz de la tarjeta microSD.
2. Inserta la tarjeta SD en el interface con el ZX81 apagado.
3. Enciende el ZX81. El sistema detecta el fichero, reprograma la flash de la FPGA y lo borra de la tarjeta SD al terminar.

> ⚠️ **Aviso:** No apagues el ZX81 ni retires la tarjeta SD durante la actualización. Si el proceso se interrumpe, el sistema lo detecta y reintenta automáticamente en el siguiente arranque — `SD81.mcs` no se borra de la tarjeta SD hasta que la actualización se ha completado correctamente.

---

## Programación de la FPGA vía JTAG

Usa este método solo si la actualización por tarjeta SD de arriba falla o no está disponible. Está pensado **solo para usuarios avanzados y fabricantes**, y requiere un cable Xilinx USB Platform Cable (o un clon compatible) y Xilinx ISE iMPACT.

La FPGA (Spartan-6 XC6SLX9) no almacena su configuración internamente. La carga desde un chip de flash SPI auxiliar (25Q128) en cada encendido. Programarla significa escribir `SD81Booster.mcs` en esa flash mediante programación indirecta por JTAG.

> ⚠️ **Aviso:** Una programación incorrecta puede dejar el interface permanentemente inoperativo.

**Pasos:**

1. Conecta el cable JTAG al conector JTAG de la placa (TMS — TDI — TDO — TCK — GND — VREF).
2. Alimenta la placa vía USB-C.
3. Abre **Xilinx ISE iMPACT** y haz doble clic en **Boundary Scan**.
4. Clic derecho en la ventana de Boundary Scan → **Initialize Chain**. Debería aparecer la FPGA (`XC6SLX9`).
5. Clic derecho sobre la FPGA → **Add SPI/BPI Flash**. Busca `SD81Booster.mcs` y selecciónalo.
6. Cuando se solicite el dispositivo de flash, selecciona **SPI PROM → 25Q128** (128 Mbit).
7. Clic derecho sobre la flash → **Program**.
8. Espera a que aparezca **PROGRAM SUCCEEDED**.
9. Reinicia la alimentación de la placa. La FPGA cargará su nueva configuración automáticamente.

---

## Actualización del módulo WiFi (accesorio opcional, ESP32-C3)

Solo aplica si tu interface tiene instalado el módulo WiFi opcional.

### Actualización normal (vía tarjeta SD)

1. Copia `ESP32_FW.BIN` a la raíz de la tarjeta microSD.
2. Inserta la tarjeta SD en el interface y haz un ciclo de apagado/encendido del ZX81 (o simplemente inserta la tarjeta si ya está encendido — el módulo comprueba si existe el fichero tras un arranque nuevo).
3. Espera a que el LED STAT se quede en **verde fijo**. Durante la actualización el LED parpadea en rosa; se pone verde fijo si termina con éxito, o se queda en amarillo fijo si falla (vuelve a copiar el fichero para reintentarlo).

> ⚠️ **Aviso:** No apagues el interface ni retires la tarjeta SD mientras el LED STAT parpadea en rosa.

### Programación inicial / de recuperación (vía USB, IDE de Arduino)

El módulo necesita programarse por USB **la primera vez** (o si deja de responder y no se puede usar la actualización por SD de arriba). A partir de ahí, todas las actualizaciones posteriores pueden hacerse por tarjeta SD.

**Requisitos:**
- [IDE de Arduino](https://www.arduino.cc/en/software) con el soporte de placas ESP32 instalado
- Un cable USB al módulo ESP32-C3

**Configuración de la placa** (menú Tools/Herramientas):

| Parámetro | Valor |
|---------|-------|
| Board (placa) | **ESP32C3 Dev Module** (dentro del grupo esp32, instalado vía Boards Manager) |
| USB CDC On Boot | Disabled |
| CPU Frequency | 160MHz (WiFi) |
| Flash Frequency | 80MHz |
| Flash Mode | QIO |
| Flash Size | 4MB (32Mb) |
| Upload Speed | 921600 |
| Core Debug Level | None |
| JTAG Adapter | Disabled |
| Zigbee Mode | Disabled |

El resto de opciones (Erase All Flash Before Sketch Upload, Partition Scheme, etc.) déjalas en el valor por defecto que trae el IDE de Arduino para esta placa. Selecciona el **Port** correcto para el módulo y sube el sketch del firmware del módulo WiFi como de costumbre.
