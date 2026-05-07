# SD81 Booster
## Manual de Usuario

**Versión 1.0**

---

> El SD81 Booster es un interface de expansión para el ordenador Sinclair ZX81 que añade carga y guardado desde tarjeta microSD, hasta 512 KB de RAM, emulación del chip de sonido AY, síntesis de voz y muchas otras funciones.

---

## Índice

1. [Introducción](#1-introducción)
2. [Descripción del hardware](#2-descripción-del-hardware)
3. [Contenido de la caja](#3-contenido-de-la-caja)
4. [Instalación](#4-instalación)
5. [Preparación de la tarjeta microSD](#5-preparación-de-la-tarjeta-microsd)
6. [Primeros pasos](#6-primeros-pasos)
7. [Carga y guardado desde la SD](#7-carga-y-guardado-desde-la-sd)
8. [Gestión de archivos y directorios](#8-gestión-de-archivos-y-directorios)
9. [Funciones adicionales](#9-funciones-adicionales)
10. [Sonido](#10-sonido)
11. [Gestión de memoria](#11-gestión-de-memoria)
12. [Extensiones BASIC avanzadas](#12-extensiones-basic-avanzadas)
13. [Ejemplos de programas](#13-ejemplos-de-programas)
14. [Códigos de error](#14-códigos-de-error)
15. [Para programadores](#15-para-programadores)
16. [Solución de problemas](#16-solución-de-problemas)
17. [Actualización del firmware](#17-actualización-del-firmware)
18. [Glosario](#18-glosario)
19. [Historial de versiones](#19-historial-de-versiones-del-firmware)
20. [Referencias](#20-referencias)

**Apéndices**
- [Apéndice A — Referencia completa del comando PLAY](#apéndice-a--referencia-completa-del-comando-play)
- [Apéndice B — Referencia del generador de efectos PEG](#apéndice-b--referencia-del-generador-de-efectos-peg)
- [Apéndice C — Diccionario del sintetizador de voz](#apéndice-c--diccionario-del-sintetizador-de-voz)
- [Apéndice D — Sistema de paginación de memoria](#apéndice-d--sistema-de-paginación-de-memoria)
- [Apéndice E — Puerto del interface Chroma81 (7FEFh)](#apéndice-e--puerto-del-interface-chroma81-7fefh)
- [Apéndice F — Modos Superfast y Spectrum](#apéndice-f--modos-superfast-y-spectrum)
- [Apéndice G — Referencia técnica de audio: chip AY, VGM y alófonos](#apéndice-g--referencia-técnica-de-audio-chip-ay-vgm-y-alófonos)

---

## Guía de inicio rápido

Si acabas de abrir la caja y quieres empezar cuanto antes, sigue estos cinco pasos:

**1. Prepara la tarjeta microSD**
- Formatea una tarjeta microSD en **FAT32**.
- Copia la carpeta **SYS** y todo su contenido en la raíz de la tarjeta. Sin esta carpeta el interface no arrancará.
- Copia tus archivos `.P` en la tarjeta, organizados en carpetas si lo deseas.

**2. Conecta el interface**
- Apaga el ZX81.
- Conecta el SD81 Booster al **puerto de expansión trasero** del ZX81.
- Inserta la tarjeta microSD en la ranura del interface.

**3. Enciende y comprueba**
- Enciende el ZX81. Debe arrancar con normalidad mostrando el cursor `K`.
- El LED **STAT** del panel superior debe iluminarse en **verde fijo**.

**4. Carga tu primer programa**
- Escribe el siguiente comando y pulsa `ENTER`:
```
LOAD FAST "NOMBRE"
```
Sustituye `NOMBRE` por el nombre del archivo sin la extensión `.P`.

**5. ¿Quieres explorar qué hay en la SD?**
```
LOAD *DIR
```

> Para más detalles sobre cualquiera de estos pasos, consulta las secciones correspondientes del manual.

---

## 1. Introducción

El **SD81 Booster** es una interfaz de expansión de hardware abierto para el Sinclair ZX81 que amplía considerablemente las capacidades originales del ordenador. Entre sus principales características se encuentran:

- **Carga y guardado desde tarjeta microSD** en formato `.P`, el mismo que utilizan los emuladores más populares.
- **Hasta 512 KB de RAM** mediante un mapeador de memoria en bloques de 8 KB.
- **Emulación del chip de sonido AY-3-8910/12** con soporte para el comando `PLAY`.
- **Reproductor de archivos VGM** en segundo plano.
- **Síntesis de voz** con muestras de audio almacenadas en la memoria interna del microcontrolador.
- **Hasta 128 caracteres definibles por el usuario**, con compatibilidad con el modo de definición de caracteres del interface QuickSilva.
- **Compatibilidad con el interface Chroma81**, que proporciona salida de vídeo RGB color para el ZX81.
- **Compatibilidad total** con las rutinas originales de cinta y la ZX Printer.

---

## 2. Descripción del hardware

El SD81 Booster es una caja compacta que se conecta al puerto de expansión trasero del ZX81. En su exterior encontrarás todos los conectores y controles necesarios para aprovechar sus funciones.

> **Nota:** Las imágenes de esta sección son renders provisionales del diseño 3D y serán sustituidas por fotografías del producto final antes del lanzamiento.

---

### 2.1 Panel lateral derecho

*(Imagen: vista lateral derecha — RESET, QSILVA, MICRO-SD, USB)*

El panel lateral derecho presenta, de izquierda a derecha:

- **Botón RESET:** Reinicia el sistema completo (ZX81 + interface).
- **Botón QSILVA:** Alterna entre el juego de caracteres del interface QuickSilva y el juego de caracteres estándar de la ROM del ZX81. Cada pulsación conmuta entre uno y otro.
- **Ranura MICRO-SD:** Inserta aquí la tarjeta microSD con los programas, ROMs y datos del sistema.
- **Puerto USB-C:** Tiene dos usos:
  - **Actualización de firmware:** en caso de necesitar una recuperación via USB (ver sección 17).
  - **Consola de depuración:** al conectar el cable USB-C al ordenador, el sistema operativo detecta un puerto serie asociado al chip **CH340**. Puede ser necesario instalar los drivers del CH340 si el sistema no lo reconoce automáticamente. Con cualquier programa de terminal serie (PuTTY, Tera Term, minicom...) es posible monitorizar los mensajes de estado y error del interface en tiempo real, lo que resulta muy útil para diagnóstico y desarrollo.

---

### 2.2 Panel lateral izquierdo y panel trasero

*(Imagen: vista lateral izquierda — conector JOYSTICK DB9)*

- **Conector JOYSTICK (DB9):** Puerto de joystick compatible con joysticks estándar de 9 pines (tipo Atari/Commodore). El mapeo de los botones del joystick a teclas del ZX81 es totalmente programable mediante el comando `LOAD *JOY` (ver sección 9.6).

*(Imagen: vista trasera — conector RGB SCART)*

- **Conector RGB (SCART):** Salida de vídeo RGB color compatible con el interface Chroma81. Permite conectar el ZX81 a monitores y televisores con entrada SCART para obtener imagen en color de alta calidad.

---

### 2.3 Panel superior — LEDs de estado

*(Imagen: vista superior — LEDs STAT y SD)*

El panel superior incorpora dos indicadores luminosos:

- **LED STAT** (estado): indica el estado general del interface mediante diferentes colores y parpadeos (ver tabla en sección 2.4).
- **LED SD** (acceso a tarjeta): parpadea durante las operaciones de lectura o escritura en la tarjeta microSD.

---

### 2.4 Tabla de estados del LED STAT

El LED **STAT** indica el estado del interface mediante combinaciones de color y parpadeo, organizadas en tres fases:

**Actualización de firmware:**

| Color / Patrón | Significado |
|----------------|-------------|
| Parpadeo Azul/Rojo | Error inicializando tarjeta SD |
| Amarillo fijo | Actualizando firmware |
| Parpadeo Blanco/Rojo | Error de actualización |

**Arranque:**

| Color / Patrón | Significado |
|----------------|-------------|
| Rojo parpadeante | Inicializando puerto serie |
| Naranja parpadeante | Esperando a la FPGA |
| Parpadeo Azul/Rojo | Error inicializando tarjeta SD |
| Parpadeo Naranja/Rojo | Error escribiendo la ROM en RAM |
| Parpadeo Amarillo | Inicializando RTC |
| Verde fijo | Sistema inicializado correctamente |

**Funcionamiento:**

| Color / Patrón | Significado |
|----------------|-------------|
| Verde fijo | Interface listo y modo Quick Silva apagado |
| Cyan fijo | Interface listo y modo Quick Silva encendido |

---

## 3. Contenido de la caja

Al abrir la caja del SD81 Booster encontrarás:

- 1× Interface SD81 Booster
- 1× Tarjeta microSD (preformateada)
- 1× Pila botón CR2032 (preinstalada en el interface)
- Este manual de usuario

> ⚠️ **Importante:** Si alguno de estos elementos falta o está dañado, contacta con el vendedor antes de conectar el interface.

---

## 4. Instalación

### 4.1 Antes de conectar el interface

- Asegúrate de que el ZX81 está **apagado** antes de conectar o desconectar el interface.
- El SD81 Booster se conecta al **puerto de expansión trasero** del ZX81.
- El interface no es compatible con dispositivos que reemplacen la ROM interna del ZX81.

### 4.2 Conexión

1. Apaga el ZX81.
2. Alinea el conector del SD81 Booster con el puerto de expansión trasero del ZX81. Asegúrate de que los pines están correctamente alineados.
3. Empuja suavemente hasta que el conector quede completamente insertado. No fuerces la conexión.
4. Inserta la tarjeta microSD en la ranura del interface (ver sección 4).
5. Enciende el ZX81.

> ⚠️ **Atención:** Conectar o desconectar el interface con el ZX81 encendido puede dañar tanto el interface como el ordenador.

### 4.3 Comprobación de la instalación

Al encender el ZX81 con el SD81 Booster correctamente instalado, el ordenador debe arrancar con normalidad mostrando el cursor `K` habitual. El interface no modifica el arranque del sistema.

Para verificar que el interface está funcionando, escribe el siguiente comando en BASIC y pulsa `ENTER`:

```
LOAD *VER
```

> **Nota:** El asterisco `*` se obtiene pulsando `SHIFT + B`. El interface mostrará la versión del firmware instalado.

Si el ordenador se queda bloqueado o no arranca correctamente, desconecta el interface y asegúrate de que el conector está bien alineado.

---

## 5. Preparación de la tarjeta microSD

### 5.1 Formato de la tarjeta

El SD81 Booster requiere una tarjeta microSD formateada en **FAT32**. La tarjeta incluida en la caja ya viene formateada y preparada correctamente.

Si utilizas una tarjeta propia, sigue estos pasos:

1. Formatea la tarjeta en FAT32:
   - **Windows:** Botón derecho sobre la tarjeta → Formatear → FAT32.
   - **macOS / Linux:** Utiliza una herramienta de formato de disco y selecciona FAT32.
2. Copia la carpeta **SYS** y todo su contenido (incluida en el paquete de software del interface) en la raíz de la tarjeta SD. Esta carpeta contiene archivos imprescindibles para el funcionamiento del interface.

> ⚠️ **Importante:** El interface **no soporta** el formato exFAT ni NTFS. La carpeta **SYS es imprescindible**: contiene los archivos de ROM necesarios para el arranque del interface. Sin ella, el interface no funcionará.

### 5.2 Caracteres permitidos en nombres de archivo

Debido a las limitaciones del teclado del ZX81, solo se pueden usar los siguientes caracteres en los nombres de archivo y carpeta:

- Letras: `A` a `Z` (siempre en mayúsculas al guardar)
- Números: `0` a `9`
- Símbolos: `. , ; $ ( ) = + -`
- El carácter `/` se utiliza como separador de directorios y **no puede** usarse en nombres de archivo.

> **Consejo:** Evita terminar un nombre de archivo con un espacio o un punto, ya que algunos sistemas operativos podrían tener problemas para leer ese archivo desde el ordenador.

### 5.3 Estructura de carpetas recomendada

El interface buscará en la raíz de la tarjeta SD por defecto. Puedes organizar tus programas en carpetas. Se recomienda la siguiente estructura:

```
/
├── AUTOEXEC.P        ← Programa que se carga automáticamente al arrancar
├── JUEGOS/
│   ├── MANIC.P
│   └── PACMAN.P
├── DEMOS/
└── SYS/              ← Carpeta del sistema (obligatoria, no modificar)
```

### 5.4 El programa AUTOEXEC

Si existe un archivo llamado `AUTOEXEC.P` en la raíz de la SD, este se cargará y ejecutará automáticamente al escribir el comando `RUN` en un ZX81 sin ningún programa cargado. Es útil para crear menús de inicio personalizados.

---

## 6. Primeros pasos

### 6.1 Modos de carga: SD o cinta

Por defecto, los comandos `LOAD` y `SAVE` del BASIC funcionan con la **cinta**, exactamente igual que en un ZX81 sin interface. Esto garantiza la compatibilidad total con el software original en cinta.

Para utilizar la tarjeta SD, tienes dos opciones:

**Opción A — Activar el modo SD de forma permanente (hasta apagar el ordenador):**

Escribe en BASIC:

```
LOAD FAST
```

> **Nota:** La palabra `FAST` se obtiene con `SHIFT + F`, no escribiendo las letras una a una.

A partir de ese momento, todos los comandos `LOAD` y `SAVE` usarán la SD por defecto, sin necesidad de añadir `FAST` en cada comando. Si en este modo necesitas cargar desde cinta, usa `LOAD SLOW "nombre"` para forzar la carga por audio en esa operación concreta.

Para volver al modo cinta por defecto:

```
LOAD SLOW
```

> **Nota:** `SLOW` se obtiene con `SHIFT + D`. A partir de ese momento todos los `LOAD` y `SAVE` vuelven a usar la cinta por defecto, y para cargar desde SD habrá que añadir `FAST` explícitamente.

> **Nota:** El modo SD/cinta también puede cambiarse usando `SAVE FAST` o `SAVE SLOW` sin nombre de archivo, de la misma manera que `LOAD FAST` y `LOAD SLOW`. Ambos comandos son equivalentes para activar o desactivar el modo SD.

**Opción B — Cargar desde SD sin cambiar el modo:**

Puedes forzar la carga desde SD en cualquier momento sin cambiar el modo por defecto, añadiendo `FAST` al comando de carga (ver sección 6).

### 6.2 Cargando tu primer programa desde la SD

Sigue estos pasos para cargar un programa desde la SD por primera vez:

1. Asegúrate de que tienes al menos un archivo `.P` en la tarjeta SD.
2. Enciende el ZX81 con el interface y la SD insertados.
3. En el cursor `K`, escribe:

```
LOAD FAST "NOMBRE"
```

Sustituye `NOMBRE` por el nombre del archivo sin la extensión `.P`. Por ejemplo, para cargar `PACMAN.P`:

```
LOAD FAST "PACMAN"
```

4. Pulsa `ENTER`. El programa se cargará en unos instantes y arrancará automáticamente si tiene autoarranque.

> **Consejo:** Si no recuerdas el nombre exacto del archivo, puedes ver el contenido de la SD con el comando `LOAD *DIR` (ver sección 7).

---

## 7. Carga y guardado desde la SD

### 7.1 Cargar un programa

Para cargar un archivo desde la SD:

```
LOAD FAST "NOMBRE"
```

El interface buscará primero el archivo con el nombre exacto. Si no lo encuentra, intentará añadirle la extensión `.P` automáticamente. Si tampoco lo encuentra así, mostrará un error.

**Cargar desde una subcarpeta:**

```
LOAD FAST "JUEGOS/PACMAN"
```

**Cargar y ejecutar desde una línea específica:**

```
LOAD FAST "NOMBRE" THEN GOTO 100
```

**Cargar sin ejecutarlo automáticamente:**

```
LOAD FAST "NOMBRE" THEN STOP
```

> **Nota:** `THEN`, `GOTO` y `STOP` son tokens del BASIC del ZX81, no se escriben letra a letra.

### 7.2 Guardar un programa

Para guardar el programa actual en la SD:

```
SAVE FAST "NOMBRE"
```

El interface añadirá automáticamente la extensión `.P` al archivo guardado. Por ejemplo, `SAVE FAST "MIPROG"` creará el archivo `MIPROG.P` en el directorio actual de la SD.

> ⚠️ **Atención:** Si ya existe un archivo con ese nombre, será sobreescrito sin aviso previo.

### 7.3 Cargar y guardar bloques de memoria (código máquina)

Para cargar un bloque de datos en una dirección de memoria específica:

```
LOAD FAST "NOMBRE" CODE 30000
```

> **Importante:** A diferencia del ZX Spectrum, la dirección después de `CODE` es **obligatoria**. Los archivos en SD no almacenan la dirección de carga en una cabecera.

> **Importante:** El token `FAST` es obligatorio para que `CODE` funcione correctamente. Si se usa `LOAD "NOMBRE" CODE 30000` sin `FAST` (incluso con el modo SD activo), el archivo se cargará igualmente en la dirección por defecto **16393** (4009h), ignorando la dirección especificada. Esto es una limitación de compatibilidad del firmware.

Para guardar un bloque de memoria en la SD:

```
SAVE FAST "NOMBRE" CODE 30000,2048
```

Donde `30000` es la dirección de inicio y `2048` es la longitud en bytes.

### 7.4 Cargar siempre desde cinta (independientemente del modo)

Si el modo SD está activo pero quieres cargar desde cinta:

```
LOAD SLOW "NOMBRE"
```

### 7.5 Notas sobre compatibilidad de juegos

Algunos juegos y programas necesitan una inicialización especial antes de ser cargados. La lista de compatibilidad incluida con el interface (archivo `SD81 Booster compatibility list`) indica los pasos necesarios para cada programa. Los casos más comunes son:

- **`LOAD *128C` antes de cargar:** El juego utiliza caracteres definibles por el usuario y necesita el modo de 128 caracteres activado.
- **`LOAD FAST` antes de cargar:** Algunos juegos tienen múltiples archivos y el primero necesita cargarse desde la SD. Ejecutar `LOAD FAST` sin nombre activa el modo SD para todos los `LOAD` y `SAVE` a partir de ese momento; así el juego puede cargar sus archivos adicionales automáticamente sin necesidad de especificar `FAST` en cada uno.

---

### 7.6 Formatos de archivo reconocidos

El comando `LOAD FAST` detecta automáticamente el tipo de archivo por su extensión y actúa de forma diferente según el caso:

| Extensión | Comportamiento |
|-----------|---------------|
| `.P` | Programa BASIC estándar del ZX81. El interface calcula el tamaño real del programa desde las variables del sistema y descarta los bytes sobrantes al final del archivo. |
| `.81` | Igual que `.P`. |
| `.P81` | Formato multi-programa. El interface salta el nombre del fichero embebido antes de leer los datos. |
| `.ROM` | **Archivo de ROM.** El interface carga el contenido en la dirección 0 del espacio de direccionamiento y resetea el sistema. El control **no vuelve** al BASIC. Ver también la sección de arranque con ROM alternativa. |
| `.WAV` | Archivo de audio sin comprimir (PCM). El interface lo reproduce directamente en lugar de cargarlo en memoria. |
| Sin extensión o desconocida | El archivo se carga íntegramente en memoria tal cual, sin ningún procesado. |

> ⚠️ **Precaución con archivos `.ROM`:** Cargar un archivo con extensión `.ROM` mediante `LOAD FAST` provoca un reset inmediato del sistema. Asegúrate de que el archivo contiene una ROM válida antes de cargarlo, ya que un archivo corrupto podría dejar el sistema en un estado irrecuperable hasta que se reinicie con otra ROM.

---

## 8. Gestión de archivos y directorios

### 8.1 Ver el contenido de la SD

Para listar los archivos del directorio actual:

```
LOAD *DIR
```

Para listar los archivos de una carpeta concreta:

```
LOAD *DIR "JUEGOS"
```

Se pueden usar comodines al estilo Unix:
- `*` representa cualquier número de caracteres (incluido ninguno).
- `?` representa exactamente un carácter.

> **Importante:** `*` casa con todos los archivos independientemente de si tienen extensión o no. En cambio, `*.*` solo casa con archivos que contienen un punto en el nombre. Para listar todos los archivos, usa `*`, no `*.*`.

Por ejemplo, para listar solo los archivos `.P`:

```
LOAD *DIR "*.P"
```

Si el listado no cabe en pantalla, aparecerá `...` en la línea inferior. Pulsa cualquier tecla para continuar, o `SPACE` para cancelar.

### 8.2 Cambiar de directorio

```
LOAD *CD "JUEGOS"
```

Para volver al directorio raíz:

```
LOAD *CD "/"
```

Para ver en qué directorio estás actualmente:

```
LOAD *PWD
```

### 8.3 Crear y eliminar carpetas

Crear una carpeta nueva en el directorio actual:

```
LOAD *MD "NUEVACARPETA"
```

Eliminar una carpeta (debe estar vacía):

```
LOAD *RD "CARPETAVACIA"
```

### 8.4 Borrar, renombrar y copiar archivos

Borrar un archivo:

```
LOAD *DEL "ARCHIVO.P"
```

> **Nota:** `LOAD *DEL` no admite comodines. El nombre del archivo debe ser exacto.

Renombrar o mover un archivo:

```
LOAD *MV "VIEJO.P" TO "NUEVO.P"
```

Copiar un archivo:

```
LOAD *CP "ORIGEN.P" TO "DESTINO.P"
```

> **Nota:** `TO` es un token del BASIC (`SHIFT + 4`), no se escribe letra a letra.
>
> **Nota:** La fecha y hora del archivo de destino no se preserva; el archivo copiado tendrá la fecha y hora del momento de la copia.

### 8.5 Espacio libre en la SD

Para ver el espacio disponible en la tarjeta:

```
LOAD *FREE
```

> **Nota:** Este cálculo puede tardar varios segundos en completarse.

---

### 8.6 Directorios T81 *(función en fase alfa)*

> ⚠️ **Aviso:** Esta funcionalidad está actualmente en **fase alfa**. Puede contener errores y su comportamiento o interfaz podría cambiar en versiones futuras. No se recomienda su uso en entornos de producción.

El SD81 Booster soporta un formato de archivo especial con extensión **`.T81`** que actúa como un contenedor de múltiples programas ZX81, de forma similar a como un archivo ZIP contiene varios archivos. Esto permite distribuir colecciones de programas en un único archivo.

Para acceder al contenido de un archivo T81, se usa el comando `LOAD *CD` como si fuera un directorio normal:

```
LOAD *CD "COLECCION.T81"
```

A partir de ese momento, los comandos `LOAD`, `LOAD *DIR` y `LOAD *OPENDIR` operan sobre el contenido del archivo T81 en lugar del sistema de archivos FAT de la SD, permitiendo navegar y cargar los programas que contiene de la misma manera que se haría con archivos normales.

Para salir del directorio T81 y volver al sistema de archivos normal:

```
LOAD *CD "/"
```

> **Limitaciones en fase alfa:** Las operaciones de escritura (`LOAD *DEL`, `LOAD *MD`, `LOAD *RD`, `LOAD *MV`, `LOAD *CP`, `SAVE`) no están soportadas dentro de un directorio T81 y devolverán un error.

---

## 9. Funciones adicionales

### 9.1 Reproducción de archivos WAV

El interface puede reproducir archivos de audio en formato **WAV sin comprimir** directamente desde la tarjeta SD, simplemente cargándolos con el comando habitual:

```
LOAD FAST "SONIDO.WAV"
```

Si el archivo tiene extensión `.WAV`, el interface lo detecta automáticamente y lo reproduce en lugar de intentar cargarlo como programa. No es necesario ningún comando especial.

> **Nota:** Solo se admiten archivos WAV sin comprimir (PCM). Otros formatos de audio no son compatibles.

---

### 9.2 Reloj en tiempo real — Comando RTC

El SD81 Booster incorpora un **reloj en tiempo real (RTC)** con batería de reserva. El comando `LOAD *RTC` permite consultar y ajustar la hora y la fecha desde BASIC.

**Mostrar la hora y fecha actuales en pantalla:**

```
LOAD *RTC
```

**Guardar la hora y fecha en una variable de cadena:**

```
LOAD *RTC TO R$
```

**Poner el reloj en hora:**

```
LOAD *RTC="cadena"
```

El comando acepta varios formatos de cadena. Puedes ajustar fecha y hora a la vez, o solo uno de los dos:

| Formato | Ejemplo | Descripción |
|---------|---------|-------------|
| `AAAA-MM-DD HH:MM:SS.CC` | `"2025-04-30 18:30:00.00"` | Fecha y hora completas con centésimas |
| `AAAA-MM-DD HH:MM:SS` | `"2025-04-30 18:30:00"` | Fecha y hora completas |
| `AAAA-MM-DD` | `"2025-04-30"` | Solo fecha |
| `HH:MM:SS.CC` | `"18:30:00.00"` | Solo hora con centésimas |
| `HH:MM:SS` | `"18:30:00"` | Solo hora |
| `HH:MM` | `"18:30"` | Hora y minutos (segundos a cero) |

**Ejemplos de uso:**

```
LOAD *RTC="2025-04-30 18:30:00"
LOAD *RTC="2025-04-30"
LOAD *RTC="18:30:00"
```

**Ejemplo de uso en un programa BASIC:**

```
10 LOAD *RTC TO R$
20 PRINT "Fecha y hora: ";R$
```

---

### 9.3 Estado de la batería del RTC — Comando BAT

El SD81 Booster incorpora una **pila botón CR2032** que mantiene el reloj en hora cuando el ZX81 está apagado. Esta pila viene preinstalada de fábrica y tiene una vida útil estimada de varios años en condiciones normales de uso. Cuando se agote, puede sustituirse por cualquier pila CR2032 estándar disponible en comercios de electrónica.

El estado de carga de la pila puede consultarse desde BASIC con el comando `LOAD *BAT`.

**Mostrar el estado de la batería en pantalla:**

```
LOAD *BAT
```

**Guardar el estado en una variable de cadena:**

```
LOAD *BAT TO B$
```

> **Consejo:** Si el reloj pierde la hora frecuentemente al apagar el ordenador, consulta el estado de la batería con este comando para saber si necesita ser reemplazada.

---

### 9.4 Modo RAM extendida — Comando RAM48

El comando `LOAD *RAM48` activa el modo de RAM extendida de 48 KB, que amplía la memoria disponible para programas BASIC y datos más allá de los límites habituales.

**Activar el modo RAM extendida:**

```
LOAD *RAM48
```

**Desactivarlo para restaurar la compatibilidad estándar:**

```
LOAD *RAM48 STOP
```

> **Nota:** `STOP` es el token del BASIC, no se escribe letra a letra. Si algún programa presenta problemas de compatibilidad con el modo RAM48 activo, desactívalo con `LOAD *RAM48 STOP` antes de cargarlo.

---

### 9.5 Visualización de archivos de texto — Comando THEN PRINT

El comando `LOAD THEN PRINT` es el equivalente al comando `TYPE` de MS-DOS o `cat` de Linux: muestra el contenido de un archivo de texto directamente en la pantalla del ZX81.

**Mostrar un archivo de texto en pantalla:**

```
LOAD THEN PRINT "FICHERO"
```

**Enviar el contenido a la impresora ZX Printer:**

```
LOAD THEN LPRINT "FICHERO"
```

**Redirigir la salida de otros comandos a la impresora ZX Printer:**

El prefijo `LOAD LPRINT` puede usarse también con los comandos que normalmente muestran texto en pantalla, para redirigir su salida directamente a la ZX Printer:

```
LOAD LPRINT DIR
LOAD LPRINT DIR "*.P"
LOAD LPRINT FREE
LOAD LPRINT PWD
LOAD LPRINT VER
```

**Sistema de ayuda integrado:**

Si se antepone un asterisco `*` al nombre del fichero, el interface buscará automáticamente un archivo con ese nombre en la carpeta `/MAN/` de la SD y le añadirá la extensión `.TXT`. Esto permite implementar un sistema de ayuda al estilo del comando `man` de Linux:

```
LOAD THEN PRINT "*PLAY"
```

Este comando buscaría el archivo `/MAN/PLAY.TXT` en la SD y mostraría su contenido en pantalla. Puedes crear tus propios archivos de ayuda en esa carpeta para documentar tus programas o comandos personalizados.

**Ejemplo de uso en un programa BASIC:**

```
10 LOAD THEN PRINT "*INSTRUCCIONES"
```

Esto mostraría el contenido de `/MAN/INSTRUCCIONES.TXT`, ideal para mostrar las instrucciones de un juego o programa.

---

### 9.6 Joystick programable — Comando JOY

El SD81 Booster incorpora un puerto de joystick DB9 cuyo mapeo de botones es totalmente configurable. El comando `LOAD *JOY` permite asignar una tecla del ZX81 a cada dirección y al botón de fuego del joystick.

**Sintaxis:**

```
LOAD *JOY "arr/aba/izq/der/fue"
```

La cadena de configuración contiene exactamente cinco caracteres, uno por cada función del joystick en este orden: **arriba / abajo / izquierda / derecha / fuego**.

**Ejemplo:**

```
LOAD *JOY "QAOP "
```

Este ejemplo asigna:
- Arriba → tecla `Q`
- Abajo → tecla `A`
- Izquierda → tecla `O`
- Derecha → tecla `P`
- Fuego → tecla espacio (` `)

> **Consejo:** Consulta los controles de cada juego antes de configurar el joystick. Muchos juegos del ZX81 usan combinaciones de teclas diferentes, y con este comando puedes adaptarlas a cualquier joystick estándar de 9 pines sin modificar el software.

---

## 10. Sonido

El SD81 Booster incorpora un emulador del chip de sonido **AY-3-8910/12**, el mismo que usaban ordenadores como el ZX Spectrum 128K o el Amstrad CPC. Esto permite reproducir música de hasta tres voces simultáneas, efectos de sonido programables y música en segundo plano, todo ello desde BASIC o desde código máquina.

---

### 10.1 Comando PLAY — Música con el chip AY

El comando `PLAY` permite reproducir música directamente desde BASIC mediante una cadena de texto que describe las notas, la duración, el tempo y otros parámetros. Admite hasta tres voces simultáneas (canales A, B y C del AY).

**Sintaxis básica:**

```
LOAD *PLAY "cadena1"
LOAD *PLAY "cadena1","cadena2","cadena3"
```

Cada cadena corresponde a una voz. Pueden usarse de una a tres cadenas simultáneamente.

**Ejemplo sencillo — melodía en una sola voz:**

```
LOAD *PLAY "T120O45C5E5G9C"
```

Esto toca las notas Do, Mi, Sol y Do (acorde de Do mayor) a 120 pulsaciones por minuto en la octava 4.

---

#### Parámetros principales de la cadena PLAY

**Notas**

Las notas se escriben con las letras `C D E F G A B` (Do Re Mi Fa Sol La Si). Se pueden modificar con:

- `=` antes de la nota: sube un semitono (sostenido). Ejemplo: `=F` es Fa sostenido.
- `£` antes de la nota: baja un semitono (bemol). Ejemplo: `£E` es Mi bemol.
- **Letra en vídeo normal:** toca la nota en la octava actual.
- **Letra en vídeo inverso** (SHIFT + 9 sobre la letra): toca la misma nota pero en la octava siguiente, sin cambiar la octava activa. Es equivalente al uso de mayúsculas en el ZX Spectrum.
- `£` (símbolo de libra) sin nota a continuación: pausa o silencio.

**Duración**

Un número del 1 al 12 antes de una nota establece su duración desde ese punto en adelante:

| Valor | Nombre | Duración a 60 bpm |
|-------|--------|-------------------|
| 1 | Semicorchea | 0.25 s |
| 3 | Corchea | 0.5 s |
| 5 | Negra | 1 s |
| 7 | Blanca | 2 s |
| 9 | Redonda | 4 s |

> **Consejo:** Puedes ligar duraciones con el signo `-`. Por ejemplo, `4-3A` combina corchea con puntillo (7/8 de negra).

**Tempo**

`T<número>` establece el tempo en pulsaciones por minuto (bpm), entre 60 y 240. El valor por defecto es 120. Solo tiene efecto en la primera voz.

```
LOAD *PLAY "T1805C5E5G"
```

**Octava**

`O<número>` selecciona la octava, de 0 (muy grave) a 8 (muy agudo). La octava por defecto es 4.

```
LOAD *PLAY "O35CO45CO55C"
```

**Repeticiones**

- `(` ... `)` repite una sección una vez más.
- `)` sin `(` correspondiente repite la cadena entera indefinidamente.
- `H` detiene el comando PLAY aunque haya voces en bucle infinito.

```
LOAD *PLAY "(5C5E5G)H",")"
```

**Efectos de volumen (envolvente)**

`W<número>` selecciona uno de los 8 efectos de volumen del chip AY (de W0 a W7), como ataque, decaimiento o tremolo. `U` activa el efecto en el canal. `X<número>` ajusta la velocidad del efecto (0–65535; a mayor valor, más lento).

> Para una descripción completa de todos los parámetros, incluyendo los efectos de envolvente y el modo de ruido, consulta el **Apéndice A** de este manual.

---

### 10.2 Reproductor VGM — Música en segundo plano

El SD81 Booster puede reproducir archivos en formato **VGM** (*Video Game Music*) en segundo plano mientras el ZX81 ejecuta cualquier otro programa. Esto permite añadir música a tus propios programas BASIC sin consumir tiempo de CPU.

> **Nota:** Solo se admiten archivos VGM que contengan datos del chip AY-3-8910 o AY-3-8912. Los VGMs con otros chips de sonido no son compatibles.

**Preparar un archivo VGM para su reproducción:**

```
LOAD *VGM "MUSICA"
```

Esto carga el archivo `MUSICA.VGM` de la SD y lo deja listo para reproducir, pero no lo inicia todavía.

**Iniciar la reproducción:**

```
LOAD *VGM THEN RUN
```

o equivalentemente:

```
LOAD *VGM THEN CONT
```

**Pausar y reanudar:**

```
LOAD *VGM THEN PAUSE
LOAD *VGM THEN CONT
```

**Detener la reproducción:**

```
LOAD *VGM THEN STOP
```

**Activar el modo bucle** (la música se reinicia al terminar):

```
LOAD *VGMLOOP
```

**Desactivar el modo bucle:**

```
LOAD *VGMLOOP STOP
```

> **Nota:** `THEN`, `RUN`, `CONT`, `PAUSE` y `STOP` son tokens del BASIC del ZX81, no se escriben letra a letra.

**Ejemplo de uso típico en un programa BASIC:**

```
10 LOAD *VGM "MUSICA"
20 LOAD *VGMLOOP
30 LOAD *VGM THEN RUN
40 REM --- aquí continúa el programa mientras suena la música ---
```

---

### 10.3 Generador de efectos PEG — Efectos de sonido programables

El **PEG** (*Programmable Effects Generator*) es una pequeña máquina virtual integrada en el interface que puede ejecutar programas de efectos de sonido de forma completamente independiente al Z80, sin consumir tiempo de CPU del ZX81.

El PEG accede directamente a los registros del chip AY y dispone de hasta **tres hilos** de ejecución paralelos, lo que permite reproducir varios efectos simultáneamente.

> El PEG está orientado principalmente a desarrolladores. Para crear programas PEG se recomienda usar el ensamblador PEG incluido en el repositorio del proyecto. Para una descripción completa del juego de instrucciones, consulta el **Apéndice B** de este manual.

**Cargar un programa PEG en memoria:**

```
LOAD *PEG <dirección>,"<hexadecimal>"
```

Donde `<dirección>` es la posición en la memoria PEG (0–255) y `<hexadecimal>` es la secuencia de instrucciones en formato hexadecimal. Cada instrucción ocupa 4 caracteres hexadecimales (2 bytes).

**Iniciar un hilo PEG:**

```
LOAD *PEG THEN RUN <hilo>,<dirección>
```

Donde `<hilo>` es el número de hilo (0, 1 o 2) y `<dirección>` es la dirección de inicio del programa PEG.

**Detener, pausar y reanudar un hilo:**

```
LOAD *PEG THEN STOP <hilo>
LOAD *PEG THEN PAUSE <hilo>
LOAD *PEG THEN CONT <hilo>
```

**Cargar un programa PEG desde la SD (`LOAD *PEB`):**

Además de la carga inline con cadena hexadecimal, es posible cargar un programa PEG compilado directamente desde un archivo binario en la SD:

```
LOAD *PEB <dirección> "<nombre>"
```

Donde `<dirección>` es la posición de inicio en la memoria PEG (0–255) y `"<nombre>"` es el nombre del archivo en la tarjeta SD. El archivo debe tener extensión `.PEB` (PEG binary); si no se especifica extensión, el interface la añade automáticamente.

**Ejemplo:**

```
LOAD *PEB 0 "EFECT"
LOAD *PEG THEN RUN 0,0
```

Este ejemplo carga el archivo `EFECT.PEB` en la memoria PEG a partir de la posición 0 y arranca el hilo 0 desde esa misma dirección.

> **Nota técnica:** El archivo `.PEB` es el resultado de ensamblar un fuente `.PEG` con el ensamblador `peg.py`, un script Python incluido en la carpeta `EXAMPLES/PEG/` del repositorio del proyecto. Para compilar un fuente:
> ```
> python peg.py efect.peg
> ```
> Esto genera el archivo `efect.peb` listo para copiar a la SD.

### 10.4 Síntesis de voz — Comando SAY

El SD81 Booster incorpora un sintetizador de voz que permite reproducir frases en inglés directamente desde BASIC. El sintetizador se basa en los fonemas del chip **SP0256**, un sintetizador de voz ampliamente utilizado en la época que formaba parte de interfaces clásicos como el **Currah MicroSpeech** para el ZX Spectrum o **The Voice** para el Videopac G7000/Odyssey 2. Las muestras de audio de los fonemas están almacenadas en la memoria interna del microcontrolador, por lo que no se necesita ningún archivo adicional en la SD.

**Sintaxis:**

```
LOAD *SAY "frase"
```

El sintetizador analiza la cadena de texto e intenta construir la pronunciación combinando fonemas del inglés. El texto debe escribirse en inglés, en mayúsculas, tal como aparecería escrito.

**Ejemplo:**

```
LOAD *SAY "HELLO WORLD"
LOAD *SAY "ZX81 COMPUTER"
LOAD *SAY "ERROR 5"
```

Los números se leen en inglés automáticamente, desde cero hasta los billones.

**Reproducción en background:**

Por defecto, la CPU espera a que la voz termine antes de continuar ejecutando el programa. Si se antepone un `*` al inicio de la cadena, la reproducción continúa en segundo plano y el ZX81 sigue ejecutando el programa inmediatamente:

```
LOAD *SAY "*HELLO WORLD"
```

> **Nota:** No es posible añadir sonidos o fonemas personalizados. El conjunto de fonemas disponibles está fijo en la memoria interna del microcontrolador.

**Cómo funciona el sintetizador:**

El sintetizador descompone el texto letra a letra, buscando primero las coincidencias más largas posibles en su diccionario interno. Por ejemplo, la palabra `HELLO` se reconoce como una palabra completa y se pronuncia correctamente; si no existiera como palabra, se descompondría en fragmentos `HE` + `LL` + `O`. Esto significa que las palabras del diccionario interno suenan mejor que las que el sintetizador debe construir fonema a fonema.

Los espacios y signos de puntuación (`,` `;` `:`) producen pausas de duración creciente entre palabras o frases.

> **Consejo:** Para mejorar la pronunciación de palabras no reconocidas, puedes escribirlas fonéticamente en inglés. Por ejemplo, `SINCLAIR` puede sonar mejor escrito como `SINCLER`. Consulta el **Apéndice C** para ver el diccionario completo de palabras y fonemas reconocidos.

---



## 11. Gestión de memoria

El SD81 Booster incorpora hasta **512 KB de RAM**, muy por encima de los 1 KB originales del ZX81 o de las expansiones de memoria convencionales. Esta memoria se gestiona mediante un **mapeador de memoria** que divide tanto la RAM como el espacio de direcciones del Z80 en bloques de 8 KB, permitiendo asignar cualquier página de memoria a cualquier bloque del mapa de direcciones.

Para la mayoría de los programas BASIC no es necesario gestionar la memoria manualmente: el interface la configura automáticamente al arrancar y el BASIC del ZX81 puede usar la RAM disponible de forma transparente.

---

### 10.1 Conceptos básicos: bloques y páginas

El espacio de direcciones del Z80 (64 KB) se divide en **8 bloques** de 8 KB cada uno:

| Bloque | Rango de direcciones | Uso habitual |
|--------|----------------------|--------------|
| 0 | 0000–1FFF | ROM del ZX81 (solo lectura) |
| 1 | 2000–3FFF | ROM de expansión del interface |
| 2 | 4000–5FFF | RAM principal / pantalla |
| 3 | 6000–7FFF | RAM principal |
| 4 | 8000–9FFF | RAM ampliada |
| 5 | A000–BFFF | RAM ampliada |
| 6 | C000–DFFF | Espejo de bloque 2 (necesario para el vídeo) |
| 7 | E000–FFFF | Espejo de bloque 3 (necesario para el vídeo) |

Los 512 KB de RAM se dividen en **64 páginas** de 8 KB. Cada bloque puede apuntar a cualquiera de estas 64 páginas, lo que permite acceder a toda la memoria simplemente cambiando qué página está asignada a qué bloque.

> **Nota:** Los bloques 6 y 7 deben mantenerse como espejo de los bloques 2 y 3 respectivamente para que el sistema de vídeo del ZX81 funcione correctamente. Modificarlos sin tener esto en cuenta puede causar que la pantalla deje de funcionar.

---

### 10.2 Comando MAP — Asignar páginas a bloques

Para asignar una página de memoria a un bloque determinado:

```
LOAD *MAP <bloque>,<página>
```

Donde `<bloque>` es un número del 0 al 7 y `<página>` es un número del 0 al 63.

Para leer qué página está asignada actualmente a un bloque:

```
LOAD *MAP <bloque> TO <variable>
```

**Ejemplo práctico — acceder a RAM adicional:**

Al arrancar, los bloques 4 y 5 contienen las páginas 4 y 5. Para cambiar el bloque 4 a la página 10:

```
LOAD *MAP 4,10
```

A partir de ese momento, cualquier lectura o escritura en las direcciones 32768–40959 accederá a la página 10 de la RAM.

**Ejemplo — leer la página asignada a un bloque:**

```
10 LOAD *MAP 4 TO A
20 PRINT "Bloque 4 apunta a la pagina ";A
```

> **Consejo:** Para una referencia completa del sistema de paginación, incluyendo el modo de paginación completa de 512 KB y el uso desde código máquina, consulta el **Apéndice D** de este manual.

---

### 10.3 Modo MC45 — Código máquina en bloques 4 y 5

Por diseño del hardware del ZX81, las instrucciones de código máquina situadas en los bloques 4 y 5 (direcciones 32768–49151) son ejecutadas de forma incorrecta: los opcodes en los rangos 00h–3Fh y 80h–BFh son interpretados como NOP, lo que hace imposible ejecutar código normal en esa zona.

El modo **MC45** (*Machine Code 4 and 5*) desactiva esta limitación, permitiendo ejecutar cualquier instrucción Z80 en esa área de memoria.

Para activar el modo MC45:

```
LOAD *MC45
```

Para desactivarlo:

```
LOAD *MC45 STOP
```

> ⚠️ **Advertencia:** El modo MC45 se implementa forzando a cero el pin M1 del Z80 de forma intermitente y durante intervalos de tiempo muy breves, lo que mantiene la carga sobre dicho pin en niveles bajos y hace que la posibilidad de daño sea muy reducida. Además, el interface ya incorpora internamente una **resistencia de protección de 33Ω** en la línea M1, por lo que **no es necesario realizar ninguna modificación en el ZX81**. (Algunos documentos mencionan una resistencia de 680Ω; ese valor corresponde a una modificación alternativa del propio ZX81, no al interface.)
>
> Pese a todo, el uso de esta característica se realiza bajo la **responsabilidad exclusiva del usuario**.
>
> Cuando MC45 está activo, el fichero de pantalla no puede situarse en los bloques 4 o 5, por lo que no es posible cargar ni escribir programas BASIC de más de 16 KB mientras este modo está activo.

---

### 10.4 Arranque con ROM alternativa

El SD81 Booster permite arrancar con una ROM diferente a la estándar del ZX81 sin necesidad de usar ningún comando. Basta con tener en la carpeta `/SYS/` de la tarjeta SD uno o más archivos de ROM con los nombres `0.ROM`, `1.ROM`, `2.ROM`... hasta `9.ROM`.

Para arrancar con una ROM alternativa:

1. Mantén pulsado el número correspondiente al archivo de ROM deseado mientras enciendes el ZX81.
2. Suelta la tecla cuando el ordenador haya arrancado.

Por ejemplo, si tienes el archivo `/SYS/1.ROM` en la SD, mantén pulsada la tecla `1` durante el arranque para cargarlo.

> **Consejo:** Esta función es muy útil para probar ROMs alternativas o modificadas sin necesidad de reprogramar ningún chip. La ROM estándar del ZX81 se carga siempre si no se pulsa ninguna tecla durante el arranque.

---

### 10.5 Caracteres definibles por el usuario (128C / 64C)

Por defecto el ZX81 dispone de 64 caracteres definidos por la ROM. El SD81 Booster permite ampliar este conjunto a **128 caracteres**, todos ellos completamente redefinibles, escribiendo en la zona de memoria entre las direcciones 15360 y 16383 (3C00h–3FFFh).

Para activar el modo de 128 caracteres:

```
LOAD *128C
```

Para volver al modo estándar de 64 caracteres:

```
LOAD *64C
```

> **Nota:** En el modo de 128 caracteres, los 64 caracteres superiores se muestran automáticamente en vídeo inverso por el hardware. Para mostrarlos en vídeo normal debes almacenar el gráfico invertido en esa posición de memoria.

Al activar `*128C` por primera vez, la zona de caracteres se inicializa con dos copias del juego de caracteres de la ROM, por lo que no hay diferencia visible hasta que se modifica esa zona de memoria.

Si necesitas restaurar el juego de caracteres original después de haberlo modificado:

```
LOAD *LDIR 7680,15360,512
LOAD *LDIR 7680,15872,512
```

> **Nota:** La activación del modo de 128 caracteres es incompatible con el generador interno de caracteres HRG (ver sección 10.6). Ambos modos no pueden estar activos simultáneamente.

---

#### Ejemplo: pantalla de inicio estilo Spectrum

El siguiente programa ilustra el uso del modo de 128 caracteres para cargar un juego de caracteres alternativo desde la SD y mostrar una pantalla al estilo del ZX Spectrum:

```basic
   5 LOAD *128C
  20 LOAD FAST "SPEC-81-128.BIN" CODE 15360
  30 CLS
  35 POKE 16418,0
  40 PRINT AT 23,1;CHR$ 8;" 1982 S[INCLAIR] R[ESEARCH] L[TD]."
  45 POKE 16418,2
  50 IF INKEY$="" THEN GOTO 50
```

> **Nota:** Los textos entre corchetes indican **vídeo inverso**: `S[INCLAIR]` significa que la S es normal y `INCLAIR` va en vídeo inverso; `R[ESEARCH]` que la R es normal y `ESEARCH` en vídeo inverso; `L[TD]` que la L es normal y `TD` en vídeo inverso. Para introducir caracteres en vídeo inverso en el ZX81, pulsa `SHIFT + 9` antes de cada carácter. `CHR$ 8` corresponde al carácter gráfico de cuadrícula del ZX81 (código de carácter 8, el símbolo © en el juego de caracteres del Spectrum).

**Explicación línea a línea:**

- **Línea 5:** activa el modo de 128 caracteres definibles.
- **Línea 20:** carga el archivo `SPEC-81-128.BIN` desde la SD en la dirección 15360 (3C00h), que es la zona de caracteres definibles. Este archivo contiene el juego de caracteres del ZX Spectrum.
- **Línea 30:** limpia la pantalla.
- **Línea 35:** activa el modo Superfast (POKE 16418,0) para liberar la CPU del control del vídeo.
- **Línea 40:** imprime en la línea 23 (última línea de la pantalla) el mensaje de copyright del Spectrum, usando el carácter 8 como símbolo © y las palabras de la firma en vídeo inverso.
- **Línea 45:** restaura el modo de vídeo estándar (POKE 16418,2).
- **Línea 50:** espera indefinidamente a que se pulse cualquier tecla.

---

## 12. Extensiones BASIC avanzadas

Esta sección recoge comandos adicionales del BASIC extendido del SD81 Booster orientados principalmente a la programación: manipulación de cadenas, acceso a memoria, comunicación con puertos de entrada/salida y acceso al directorio desde un programa.

---

### 12.1 Manipulación de cadenas

**Invertir caracteres de una cadena (`*INV`):**

Invierte el bit 7 de todos los caracteres de una variable de cadena, convirtiendo los caracteres normales en inversos y viceversa:

```
LOAD *INV A$
```

También admite porciones de cadena:

```
LOAD *INV A$(2 TO 7)
```

**Forzar vídeo inverso en una cadena (`*BOLD`):**

Fuerza el bit 7 de todos los caracteres de una variable de cadena a 1, poniendo todos los caracteres en vídeo inverso:

```
LOAD *BOLD A$
```

> **Consejo:** Para poner caracteres en vídeo normal a partir de una cadena en inverso, aplica primero `*BOLD` y luego `*INV` para invertir el resultado.

---

### 12.2 Copia y relleno de bloques de memoria

**Copiar un bloque de memoria en orden ascendente (`*LDIR`):**

```
LOAD *LDIR <origen>,<destino>,<longitud>
```

Copia `<longitud>` bytes desde `<origen>` hasta `<destino>` en orden ascendente de dirección. Equivale a la instrucción Z80 `LDIR`.

**Copiar un bloque de memoria en orden descendente (`*LDDR`):**

```
LOAD *LDDR <origen>,<destino>,<longitud>
```

Igual que `*LDIR` pero en orden descendente. Equivale a la instrucción Z80 `LDDR`.

> **Nota:** Cuando origen y destino se solapan, el orden de copia importa: usa `*LDIR` si el destino está antes del origen en memoria, y `*LDDR` si está después, para evitar que los datos se sobreescriban durante la copia.

**Cargar datos hexadecimales en memoria (`*HEX`):**

Carga una secuencia de bytes expresados en hexadecimal en una dirección de memoria:

```
LOAD *HEX <dirección>,"<hexadecimal>"
```

Por ejemplo, `LOAD *HEX 30000,"0A014020"` carga los valores 10 (0Ah), 1, 64 (40h) y 32 (20h) a partir de la dirección 30000.

---

### 12.3 Ejecución de código máquina

**Ejecutar una rutina en código máquina (`LOAD USR`):**

```
LOAD USR <dirección>
```

Equivale a `RAND USR <dirección>` pero con tres ventajas importantes:

- No modifica el generador de números aleatorios.
- La rutina se llama desde el nivel superior del BASIC, dejando los registros alternativos `BC'`, `DE'` y `HL'` disponibles y las pilas limpias.
- El resto de la línea no se analiza sintácticamente, lo que permite que la rutina realice su propio análisis de parámetros. Esto también significa que **todo lo que aparece a la derecha del `USR` se trata como la dirección a llamar**: por ejemplo, `LOAD USR 1+1` equivale a `LOAD USR 2` (llama a la dirección 2), en lugar de calcular `USR 1` y sumar 1 al valor de retorno como haría el `USR` normal del BASIC.

Al entrar en la rutina, el registro `BC` contiene la dirección llamada.

> **Limitación con literales numéricos:** Si la rutina utiliza la rutina SCANNING de la ROM para analizar parámetros, las expresiones con literales numéricos directos (por ejemplo `USR 40000`) no funcionarán correctamente, ya que los números necesitan ser preprocesados durante la fase de verificación sintáctica del BASIC. Como solución, usa `VAL "número"` o `CODE "carácter"` en lugar del literal: por ejemplo, `USR VAL "40000"` en lugar de `USR 40000`.

---

### 12.4 Acceso a puertos de entrada/salida

**Escribir en un puerto (`*OUT`):**

```
LOAD *OUT <puerto>,<valor>
```

**Leer de un puerto (`*IN`):**

```
LOAD *IN <puerto> TO <variable>
```

> **Nota:** `TO` es el token del BASIC (`SHIFT + 4`), no se escribe letra a letra.

---

### 12.5 Acceso a memoria de 16 bits

**Leer un valor de 16 bits de memoria (`LOAD PEEK`):**

```
LOAD PEEK <dirección> TO <variable>
```

Lee dos bytes consecutivos de memoria a partir de `<dirección>` y los almacena como un valor de 16 bits en `<variable>`.

**Escribir un valor de 16 bits en memoria (`LOAD THEN POKE`):**

```
LOAD THEN POKE <dirección>,<valor16>
```

Escribe un valor de 16 bits en dos bytes consecutivos a partir de `<dirección>`.

**Establecer el límite superior de memoria del BASIC (`LOAD THEN CLEAR`):**

```
LOAD THEN CLEAR <dirección>
```

Establece la última dirección de RAM disponible para el BASIC. A diferencia del comando `CLEAR` estándar, este **no borra las variables**; solo limpia la pila de `GOSUB`. Usa un `CLEAR` separado si también quieres borrar las variables.

---

### 12.6 Acceso al directorio desde un programa

Estos comandos permiten leer el contenido de un directorio de la SD desde dentro de un programa BASIC, útil para construir menús de selección de archivos.

**Abrir un directorio para lectura (`*OPENDIR`):**

```
LOAD *OPENDIR <cadena>
```

La cadena puede incluir una ruta completa y comodines. Si se usan comodines, el número máximo de entradas recuperables es 512. Este comando deja el directorio preparado para usar `*ROW`.

**Leer una entrada del directorio (`*ROW`):**

```
LOAD *ROW <número> TO <variable$>
```

Lee la entrada de directorio con el número indicado (empezando desde 1) y la almacena en la variable de cadena. Si el número está fuera de rango, devuelve una cadena vacía.

**Ejemplo — menú de selección de archivos en BASIC:**

```
10 LOAD *OPENDIR "JUEGOS/*.P"
20 FOR I=1 TO 10
30 LOAD *ROW I TO A$
40 IF A$="" THEN GOTO 70
50 PRINT I;". ";A$
60 NEXT I
70 INPUT "Selecciona: ";N
80 LOAD *ROW N TO A$
90 LOAD FAST A$
```

---

## 13. Ejemplos de programas

Esta sección recoge programas de ejemplo que ilustran el uso de las funciones principales del SD81 Booster. Todos están escritos en BASIC estándar del ZX81 con las extensiones del interface.

**Nota sobre el formato de los listados:** Los textos entre corchetes indican **vídeo inverso**. Por ejemplo, `S[INCLAIR]` significa que la `S` es carácter normal y `INCLAIR` va en vídeo inverso. Para introducir caracteres en vídeo inverso en el ZX81, pulsa `SHIFT + 9` antes de cada carácter. `CHR$ 8` corresponde al carácter gráfico de cuadrícula del ZX81 (código 8).

---

### 13.1 Reloj en tiempo real (RTC)

Demuestra los tres formatos de ajuste del reloj: fecha y hora completas, solo fecha y solo hora.

```basic
  10 LET A$="2025-11-10 13:48:00.00"
  15 LOAD *RTC
  16 PRINT
  20 LOAD *RTC=A$
  25 LOAD *RTC
  26 PRINT
  30 LOAD *RTC="2026-11-10"
  35 LOAD *RTC
  36 PRINT
  40 LOAD *RTC="12:20"
  45 LOAD *RTC
  46 PRINT
  50 LOAD *RTC="13:20:35"
  55 LOAD *RTC
  56 PRINT
```

**Explicación:** Muestra la hora actual (línea 15), luego la ajusta mediante una variable de cadena (línea 20), después cambia solo la fecha (línea 30), luego solo la hora con formato corto (línea 40) y finalmente con hora, minutos y segundos (línea 50). Tras cada ajuste imprime el resultado para verificarlo.

---

### 13.2 Estado de la batería del RTC (BAT)

```basic
  10 LOAD *BAT TO A$
  20 PRINT A$
```

**Explicación:** Lee el estado de la batería del reloj en tiempo real y lo muestra en pantalla. Si la batería está baja, el valor mostrado lo indicará.

---

### 13.3 Comprobación del modo MC45

Comprueba si el modo de código máquina en bloques 4 y 5 está activo cargando una pequeña rutina en ensamblador y ejecutándola:

```basic
  10 LOAD *HEX 40000,"010203C9"
  20 IF USR 40000=770 THEN GOTO 100
  30 PRINT "MC45 INACTIVE"
  40 STOP
 100 PRINT "MC45 ACTIVE"
```

**Explicación:** Carga en la dirección 40000 (bloques 4-5) una rutina Z80 que devuelve el valor de BC al salir (`C9` = RET). Si MC45 no está activo, las instrucciones en esa zona no se ejecutarán correctamente y el valor devuelto no será 770 (0302h, los valores iniciales de BC y C cargados por `01 02 03`). Si MC45 está activo, la rutina se ejecuta correctamente y salta a la línea 100.

---

### 13.4 Modo Superfast — demostración de velocidad

Compara visualmente la velocidad de refresco en modo estándar ZX81 frente al modo Superfast del SD81 Booster:

```basic
   4 SLOW
   5 PRINT "ZX81 SLOW MODE"
   6 PAUSE 250
   7 POKE 2045,85
   8 POKE 16418,0
   9 CLS
  10 GOSUB 1000
  15 CLS
  20 POKE 2045,170
  30 PRINT "SD81-BOOSTER SUPER FAST MODE"
  31 PAUSE 250
  35 CLS
  36 FAST
  37 GOSUB 1000
  40 GOTO 40
  75 POKE 1024,2
  76 POKE 1024,3
  77 POKE 1024,4
1000 PRINT "[CHR$ 0]123456789ABCDEFGHIJKLMNOPQRSTUV";
1010 FOR N=1 TO 22
1020 PRINT "0123456789ABCDEFGHIJKLMNOPQRSTUV";
1030 NEXT N
1040 PRINT "0123456789ABCDEFGHIJKLMNOQRSTUV[CHR$ 0]";
1050 RETURN
2000 PRINT "[CHR$ 0]123456789ABCDEFGHIJKLMNOPQRSTUV";
2010 FOR N=1 TO 22
2020 PRINT "0123456789ABCDEFGHIJKLMNOPQRSTUV";
2030 NEXT N
2040 PRINT "0123456789ABCDEFGHIJKLMNOQRSTUV[CHR$ 0]";
2050 RETURN
```

> **Nota:** `[CHR$ 0]` en las líneas 1000, 1040, 2000 y 2040 representa el carácter en vídeo inverso visible en el listado original (carácter 0 en vídeo inverso).

**Explicación:** El programa primero muestra una pantalla llena de texto en modo SLOW estándar del ZX81 (subrutina 1000), donde la CPU dedica tiempo al refresco de vídeo. Luego activa el modo Superfast (`POKE 2045,170`) y repite el mismo relleno de pantalla (subrutina 1000 de nuevo con FAST), mostrando la diferencia de velocidad. Las líneas 75-77 y la subrutina 2000 son variantes del patrón de prueba.

---

### 13.5 Carga de imagen en modo Spectrum

Carga una imagen en formato Spectrum (`.SCR`) desde la carpeta `/SCR/` de la SD y la muestra usando el modo HiRes Spectrum del interface:

```basic
   5 LOAD *CD "/SCR"
  15 LET HFILE=32768
  20 POKE 2044,HFILE/256
  25 POKE 2045,172
  30 LOAD *OUT 32751,39
  40 LOAD *OUT 251,6
  50 LOAD FAST "Z.SCR" CODE HFILE
 190 IF INKEY$="" THEN GOTO 15
 200 POKE 2045,85
 210 LOAD *OUT 32751,0
```

**Explicación línea a línea:**

- **Línea 5:** cambia al directorio `/SCR` de la SD.
- **Línea 15:** define la variable `HFILE` con la dirección 32768 (8000h), inicio del bloque 4.
- **Línea 20:** escribe la parte alta de la dirección del fichero de pantalla en el registro del interface (`POKE 2044`).
- **Línea 25:** activa el modo Superfast HiRes Spectrum (`POKE 2045,172`).
- **Líneas 30-40:** configura los registros de color del borde mediante el puerto de salida del interface.
- **Línea 50:** carga el archivo `Z.SCR` en la dirección `HFILE` (32768).
- **Línea 190:** espera a que se pulse cualquier tecla; al pulsarla vuelve a la línea 15 para recargar.
- **Línea 200:** desactiva el modo Superfast.
- **Línea 210:** restaura el registro de salida a 0.

---

## 14. Códigos de error

Cuando se produce un error, el ZX81 muestra un código en la parte inferior de la pantalla seguido del número de línea donde ocurrió. Los códigos relacionados con el SD81 Booster son:

| Código | Significado |
|--------|-------------|
| `A` | Argumento inválido (nombre de archivo incorrecto, parámetro fuera de rango, o cadena hexadecimal con longitud incorrecta en `*PEG` o `*HEX`). |
| `D` | El usuario pulsó BREAK para interrumpir una operación (por ejemplo, durante un listado de directorio). |
| `G` | Archivo no encontrado en la SD. |
| `H` | Error al acceder a la tarjeta SD. Comprueba que está insertada correctamente y formateada en FAT32. |
| `I` | Error de E/S en la tarjeta SD durante una operación de lectura o escritura. |
| `J` | Disco lleno: no hay espacio suficiente en la SD para guardar el archivo. |
| `K` | Archivo o directorio ya existe con ese nombre. |
| `L` | Nombre de archivo demasiado largo o con caracteres no permitidos. |
| `M` | El directorio no está vacío (al intentar eliminar con `*RD`). |
| `N` | Permiso denegado o archivo protegido contra escritura. |

> **Consejo:** Si obtienes el error `H` de forma repetida, extrae la tarjeta SD, comprueba el formato FAT32 y vuelve a insertarla. Si el error persiste, prueba con otra tarjeta.

---

## 15. Para programadores

Esta sección está dirigida a desarrolladores que deseen aprovechar las capacidades avanzadas del SD81 Booster desde código máquina Z80, incluyendo las rutinas de la ROM de expansión y el sistema de comunicación con el microcontrolador.

---

### 15.1 Mapa de la ROM de expansión

La ROM de expansión del SD81 Booster ocupa el bloque 1, a partir de la dirección **8192 (2000h)**. Las primeras posiciones contienen una firma de identificación y una tabla de saltos a las rutinas de uso más frecuente:

| Dirección (hex) | Contenido |
|-----------------|-----------|
| 2000 | Cadena de identificación `SD81` (en codificación de caracteres ZX81) |
| 2004 | Byte de versión de la ROM (p.ej. `10h` = versión 1.0) |
| 2005 | Rutina que devuelve en `HL` la dirección de retorno del llamador (útil para código reubicable) |
| 2006 | Rutina que ejecuta `JP (HL)` — emula la instrucción inexistente `CALL (HL)` |

**Tabla de rutinas (a partir de 2007h):**

| Dirección | Nombre | Descripción |
|-----------|--------|-------------|
| 2007h | `GetMCUVersion` | Devuelve la versión del MCU en `BC` (B=0, C=versión). Destruye `A`. |
| 200Ah | `WaitClkDiff` | Espera a que el bit de reloj sea distinto del bit 7 de `C`. Entrada: `C` bit 7 = bit a comparar. Salida: `A` destruido. |
| 200Dh | `WaitClkEq` | Como `WaitClkDiff` pero espera a que el reloj sea igual al bit 7 de `C`. |
| 2010h | `OutWaitDiff` | Envía `A` al puerto de datos y espera cambio de reloj (diferente). |
| 2013h | `OutWaitEq` | Como `OutWaitDiff` pero espera igualdad de reloj. |
| 2016h | `WaitDiffBrk` | Como `WaitClkDiff` pero permite interrumpir con BREAK. Destruye `A`, `C`. No retorna si se pulsa BREAK (resetea el MCU). |
| 2019h | `WaitEqBrk` | Como `WaitDiffBrk` pero espera igualdad. |
| 201Ch | `SendString` | Espera cambio de reloj y envía una cadena al MCU con longitud prefijada (8 bits). Entrada: `C` bit 7 = inverso del reloj actual; `B` = longitud; `DE` = dirección. Si `B`=0 solo envía la longitud. |
| 201Fh | `SendStrLoop` | Envía `B` bytes al MCU desde `DE` sin esperar cambios de reloj. `B`=0 envía 256 bytes. |
| 2022h | `ReportStatus` | Lee un byte del MCU. Si es 0 no hace nada; si no, genera un error BASIC (1=error G, 2=error H, etc.). |
| 2025h | `PrintBPaged` | Imprime un carácter con paginación automática: si no cabe, muestra `...` y espera tecla. SPACE interrumpe con error 5. Entrada: `B` = carácter (0–63 o 128–191). |
| 2028h | `Cmd64C` | Activa el modo de 64 caracteres. Equivale a `LOAD *64C`. Destruye `A`, `C`. |
| 202Bh | `Cmd128C` | Activa el modo de 128 caracteres. Equivale a `LOAD *128C`. Destruye `A`, `C`. |
| 202Eh | `GetPhase` | Devuelve el estado actual del reloj en el bit 7 de `C`. Modifica los flags. |
| 2031h | `GetData` | Lee el puerto de datos del MCU. Resultado en `A`. No modifica los flags. |
| 2034h | `SD81_RESET` | Punto de entrada para un RESET. Requiere: NMI deshabilitada con `OUT ($FD),A`, interrupciones enmascarables deshabilitadas con `DI`, y `HL` apuntando a la dirección donde continuar tras la inicialización. Salta a `HL` con `BC=7FFFh`. No usa la pila ni ninguna RAM. |
| 2037h | `SD81LOADCMD` | Punto de entrada para la ROM modificada del ZX81: ejecuta el comando LOAD con todas las extensiones. |
| 203Ah | `SD81SAVECMD` | Punto de entrada para el comando SAVE con todas las extensiones. |
| 203Dh | `SD81RUNCMD` | Punto de entrada para el comando RUN con todas las extensiones. |

**Obtener la versión desde BASIC:**

```
10 LET V=USR 8199
20 LET MAJ=INT(V/16)
30 LET MIN=V-16*MAJ
40 PRINT "VERSION MCU: ";CHR$(MAJ+28);".";CHR$(MIN+28)
```

La versión de la ROM también puede leerse con `PEEK 8196` (2004h): los 4 bits superiores son la versión mayor y los 4 inferiores la menor.

---

### 15.2 Comunicación con el microcontrolador (MCU)

El SD81 Booster utiliza tres puertos de E/S para la comunicación entre el Z80 y el MCU:

| Puerto | Función |
|--------|---------|
| `E7h` | Mapeador de memoria |
| `A7h` | Puerto de datos MCU (lectura y escritura). **El bit 0 en lectura indica el estado de la interrupción VSYNC.** |
| `AFh` | Puerto de control MCU (escritura = reset del MCU; lectura = bit de reloj en bit 7) |

**Sincronización con VSYNC:**

El bit 0 del puerto `A7h` refleja el estado de la interrupción de sincronismo vertical (VSYNC). Esto permite a la CPU esperar al inicio del refresco de pantalla de forma precisa, sin necesidad de interrupciones ni del comando `HALT`.

En el ZX Spectrum, muchos juegos usaban `HALT` o una rutina de interrupción para sincronizarse con el barrido vertical de la pantalla. En el SD81 Booster este mecanismo sustituye esa funcionalidad y es especialmente útil al portar juegos de Spectrum al ZX81 con SD81 Booster.

Ejemplo de bucle de espera a VSYNC en ensamblador Z80:

```
WAIT_VSYNC:
        in      a,(0A7h)        ; leer puerto de datos MCU
        and     01h             ; aislar bit 0 (VSYNC)
        jr      nz,WAIT_VSYNC   ; esperar hasta que VSYNC = 0
WAIT_VSYNC2:
        in      a,(0A7h)
        and     01h
        jr      z,WAIT_VSYNC2   ; esperar flanco (VSYNC = 1)
        ; En este punto estamos sincronizados con el inicio del frame
```

**Protocolo de comunicación:**

La comunicación se sincroniza mediante el **bit de reloj** (bit 7 del puerto `AFh`), que se invierte automáticamente cada vez que se realiza una lectura o escritura en el puerto de datos `A7h`. El Z80 debe esperar a que el bit cambie antes de realizar la siguiente operación, para evitar desincronización.

> ⚠️ **Importante:** Escribir cualquier valor en el puerto `AFh` provoca un reset software del MCU. No debe hacerse mientras el MCU esté guardando o copiando un archivo, ya que podría causar corrupción en la tarjeta SD.

**Ejemplo de secuencia de comunicación** (comando GETBYTE, recupera el byte en el índice 16 de la memoria interna del MCU):

```
        in      a,(0AFh)        ; leer el bit de reloj inicial
        ld      c,a             ; guardarlo en C

        ld      a,20h           ; código del comando GETBYTE
        out     (0A7h),a        ; enviar comando

WAIT1:  in      a,(0AFh)        ; leer el reloj
        xor     c               ; comparar con valor inicial
        jp      p,WAIT1         ; esperar hasta que sea diferente

        ld      a,16            ; índice del byte a leer
        out     (0A7h),a        ; enviar parámetro

WAIT2:  in      a,(0AFh)
        xor     c
        jp      m,WAIT2         ; esperar hasta que sea igual al inicial

        in      a,(0A7h)        ; leer el byte de respuesta
        ld      b,a             ; guardarlo en B

WAIT3:  in      a,(0AFh)
        xor     c
        jp      p,WAIT3         ; esperar cambio final

        ; B contiene el byte solicitado.
        ; El MCU está listo para recibir otro comando.
```

---

### 15.3 Mapeador de memoria (puerto E7h)

En modo de paginación simple (hasta 256 KB), los 8 bits escritos en el puerto `E7h` se interpretan así:

| Bits | Función |
|------|---------|
| D2, D1, D0 | Número de bloque (0–7) |
| D7, D6, D5, D4, D3 | Número de página (0–31) |

Para acceder a las 64 páginas del modo completo (512 KB), se usa la instrucción `OUT (C),r` con el número de página en `B` y el número de bloque en otro registro. Ejemplo: si `A` contiene el número de bloque (0–7) y `B` el de página (0–63):

```asm
        ld      c,0E7h
        out     (c),a           ; selecciona página B en bloque A
```

El cambio entre modo simple y completo se realiza mediante un comando al MCU.

---

### 15.4 Consola de depuración (puerto USB-C)

El puerto USB-C del interface no es solo para actualización de firmware — también funciona como **puerto serie de depuración**. Al conectarlo al ordenador, el sistema operativo detecta un puerto serie virtual asociado al chip **CH340G**. En algunos sistemas puede ser necesario instalar los drivers del CH340, disponibles en el repositorio del fabricante.

**Parámetros de conexión:**

| Parámetro | Valor |
|-----------|-------|
| Velocidad | 115200 baudios |
| Bits de datos | 8 |
| Paridad | Ninguna |
| Bits de parada | 1 |
| Control de flujo | Ninguno |

Con cualquier programa de terminal serie (PuTTY en Windows, minicom en Linux, CoolTerm en macOS) es posible monitorizar en tiempo real los mensajes de estado y error que genera el MCU, incluyendo:

- Progreso del arranque y resultado de cada fase de inicialización.
- Errores de acceso a la tarjeta SD con códigos de diagnóstico.
- Progreso y resultado de las actualizaciones de firmware.
- Mensajes de depuración del sistema de archivos, VGM, PEG y síntesis de voz.

> **Nota:** El firmware de producción emite mensajes básicos de estado por el puerto serie. Recompilando el firmware con la macro `DEBUG` activa se obtiene una salida mucho más detallada, útil para diagnóstico avanzado y desarrollo.

---

### 15.5 Tabla completa de comandos MCU

Los comandos se envían al MCU escribiendo su código en el puerto de datos `A7h`, siguiendo el protocolo de sincronización por bit de reloj descrito en la sección 15.2. Todos los parámetros de cadena van precedidos de un byte con la longitud.

#### Códigos de error devueltos por los comandos

| Código | Significado |
|--------|-------------|
| 0 | Éxito |
| 1 | Archivo o directorio no encontrado |
| 2 | No es un directorio |
| 3 | Error de operación (no se pudo crear/borrar/renombrar) |
| 4 | El archivo o directorio ya existe |
| 5 | Archivo demasiado grande |
| 6 | No se pudo crear el archivo destino |
| 7 | Error de escritura |
| 8 | Error de lectura parcial |
| 12 | No hay ningún archivo VGM abierto |
| 13 | Operación no permitida en directorio T81 |
| 14 | Parámetro de joystick inválido |

---

#### Comandos de sistema

| Cód. | Nombre | Parámetros enviados | Respuesta | Descripción |
|------|--------|---------------------|-----------|-------------|
| 0 | NOP | — | — | Sin operación. Solo sincroniza el reloj. |
| 1 | VERSION | — | 1 byte: versión del firmware | Devuelve la versión del MCU. Mismo formato que el byte en 2004h. |
| 32 | GETBYTE | 1 byte: índice (0–255) | 1 byte: valor | Lee un byte de la memoria interna del MCU. Índices 0–127: variables de sistema volátiles. Índices 128–255: EEPROM (persistente). |
| 33 | SETBYTE | 1 byte: índice, 1 byte: valor | — | Escribe un byte en la memoria interna. Misma división que GETBYTE. |

---

#### Comandos de sistema de archivos

| Cód. | Nombre | Parámetros enviados | Respuesta | Descripción |
|------|--------|---------------------|-----------|-------------|
| 2 | PWD | — | String + EOT + status | Devuelve el directorio actual en codificación ZX81. |
| 3 | CD | String: ruta | Status | Cambia el directorio actual. Admite rutas absolutas (empezando por `/`) y relativas. |
| 4 | DEL | String: nombre de archivo | Status | Borra un archivo del directorio actual. Sin comodines. |
| 5 | MKDIR | String: nombre | Status | Crea un subdirectorio en el directorio actual. |
| 6 | RMDIR | String: nombre | Status | Elimina un directorio vacío. |
| 7 | MOVE | String: origen, String: destino | Status | Renombra o mueve un archivo. |
| 8 | COPY | String: origen, String: destino | Status | Copia un archivo. La fecha/hora no se preserva. |
| 9 | LOAD | String: nombre de archivo | 2 bytes: longitud (little-endian), N bytes: datos, Status | Carga un archivo desde la SD. Si la extensión es `.P` o `.81`, calcula el tamaño real del programa desde las variables del sistema ZX81. Si es `.ROM`, lo carga en dirección 0 y resetea la CPU. Si es `.WAV`, lo reproduce. |
| 10 | SAVE | String: nombre, 2 bytes: longitud, N bytes: datos | Status | Guarda un bloque de datos como archivo en la SD. |
| 11 | TYPE | String: nombre de archivo | String char a char + EOT + Status | Envía el contenido de un archivo de texto carácter a carácter. Si el nombre empieza por `*`, busca en `/MAN/` y añade extensión `.TXT`. Si empieza por `*?`, el archivo se trata como texto nativo ZX81. |
| 12 | DIR | String: ruta/comodín | String char a char + EOT + Status | Lista el directorio. Envía el contenido línea a línea. |
| 14 | FREE\_TXT | — | String + EOT + Status | Devuelve el espacio total y libre de la SD como texto. |
| 15 | FREE | — | 4 bytes: total (KB), 4 bytes: libre (KB), Status | Devuelve el espacio total y libre de la SD como valores binarios de 32 bits en little-endian. |
| 16 | OPENDIR | String: ruta/comodín | Status | Abre un directorio y construye un array interno de entradas (máx. 512). Necesario antes de usar GETROWLEN/GETROW. |
| 17 | GETROWLEN | 2 bytes: índice (little-endian) | 1 byte: longitud, Status | Devuelve la longitud del nombre de la entrada `índice` del array abierto con OPENDIR. |
| 18 | GETROW | 2 bytes: índice (little-endian) | 1 byte: longitud, N bytes: nombre en ZX81, Status | Devuelve el nombre de la entrada `índice`. Índice 0 devuelve el directorio actual. Los directorios se devuelven entre `<` y `>`. |

---

#### Comandos de control del hardware

| Cód. | Nombre | Parámetros enviados | Respuesta | Descripción |
|------|--------|---------------------|-----------|-------------|
| 19 | ENABLE\_MC45 | — | — | Activa el modo de ejecución de código máquina en bloques 4 y 5. |
| 20 | DISABLE\_MC45 | — | — | Desactiva el modo MC45. |
| 21 | JOY | String: 5 bytes de configuración | Status | Configura el mapeo del joystick. Los 5 bytes son los códigos ZX81 de las teclas para: arriba, abajo, izquierda, derecha, fuego. |
| 27 | SEL\_128CHARS | — | — | Activa el modo de 128 caracteres definibles. Equivale a `LOAD *128C`. |
| 28 | SEL\_64CHARS | — | — | Activa el modo estándar de 64 caracteres. Equivale a `LOAD *64C`. |
| 29 | FULLPAGING | — | — | Activa el modo de paginación completa (512 KB, 64 páginas). |
| 30 | HALFPAGING | — | — | Activa el modo de paginación simple (256 KB, 32 páginas). |
| 48 | ENABLE\_48K | — | — | Activa el modo RAM extendida de 48 KB. Equivale a `LOAD *RAM48`. |
| 49 | DISABLE\_48K | — | — | Desactiva el modo RAM extendida. Equivale a `LOAD *RAM48 STOP`. |

---

#### Comandos de síntesis de voz

| Cód. | Nombre | Parámetros enviados | Respuesta | Descripción |
|------|--------|---------------------|-----------|-------------|
| 22 | BINARY\_SAY | String: array de bytes de alófonos | Status | Reproduce una secuencia de alófonos directamente en formato binario. Síncrono (bloquea hasta terminar). |
| 23 | SAY | String: texto en ASCII | Status | Convierte el texto a fonemas y los reproduce. Si el primer carácter es `*`, la reproducción es en background. Equivale a `LOAD *SAY`. |

---

#### Comandos AY / sonido

| Cód. | Nombre | Parámetros enviados | Respuesta | Descripción |
|------|--------|---------------------|-----------|-------------|
| 24 | AY\_SET\_REG | 1 byte: registro (0–15), 1 byte: valor | — | Escribe un valor directamente en un registro del emulador AY. |
| 25 | AY\_GET\_REG | 1 byte: registro (0–15) | 1 byte: valor | Lee el valor actual de un registro del emulador AY. |
| 26 | AY\_PLAY | String: canal A, String: canal B, String: canal C | Status | Reproduce hasta tres cadenas PLAY simultáneas. Si el canal A empieza por `*`, la reproducción es en background. Equivale a `LOAD *PLAY`. |

---

#### Comandos VGM

| Cód. | Nombre | Parámetros enviados | Respuesta | Descripción |
|------|--------|---------------------|-----------|-------------|
| 34 | PLAY\_VGM | String: nombre de archivo | Status | Abre un archivo VGM y lo prepara para reproducir en background, pero no inicia la reproducción todavía. Si no tiene extensión, añade `.vgm`. Para iniciar la reproducción usar CONT\_VGM (cmd 37). |
| 35 | STOP\_VGM | — | — | Detiene la reproducción VGM y reinicia el emulador AY. |
| 36 | PAUSE\_VGM | — | — | Pausa la reproducción VGM. |
| 37 | CONT\_VGM | — | — | Reanuda la reproducción VGM pausada. |
| 38 | LOOP\_VGM | 1 byte: modo (0=no bucle, 1=bucle) | — | Establece el modo de bucle del reproductor VGM. |

---

#### Comandos PEG (Programmable Effects Generator)

| Cód. | Nombre | Parámetros enviados | Respuesta | Descripción |
|------|--------|---------------------|-----------|-------------|
| 40 | LOAD\_PEG | 1 byte: dirección, String: datos hex | — | Carga instrucciones PEG en la memoria del generador. Los datos se envían como pares de bytes (little-endian, 2 bytes por instrucción). |
| 41 | PLAY\_PEG | 1 byte: hilo (0–2), 1 byte: dirección | — | Inicia la ejecución de un programa PEG en el hilo indicado desde la dirección dada. |
| 42 | STOP\_PEG | 1 byte: hilo (0–2) | — | Detiene y reinicia el hilo PEG indicado. |
| 43 | PAUSE\_PEG | 1 byte: hilo (0–2) | — | Pausa el hilo PEG indicado. |
| 44 | CONT\_PEG | 1 byte: hilo (0–2) | — | Reanuda el hilo PEG indicado. |
| 45 | SDLOAD\_PEG | String: nombre de archivo, 1 byte: dirección | Status | Carga un archivo `.PEB` desde la SD en la memoria PEG a partir de la dirección indicada. Tamaño máximo: 512 bytes. |

---

#### Comandos RTC y batería

| Cód. | Nombre | Parámetros enviados | Respuesta | Descripción |
|------|--------|---------------------|-----------|-------------|
| 50 | RTC | String: fecha/hora (o vacío para leer) | Si lectura: String en ZX81 + Status. Si escritura: Status | Sin parámetros: devuelve la fecha/hora actual. Con parámetros: ajusta el reloj. Formatos admitidos: `AAAA-MM-DD HH:MM:SS.CC`, `AAAA-MM-DD HH:MM:SS`, `AAAA-MM-DD`, `HH:MM:SS.CC`, `HH:MM:SS`, `HH:MM`. |
| 52 | BAT | — | 5 bytes: voltaje en ASCII (formato `V.mmm`) + Status | Devuelve el nivel de batería del RTC como string ASCII de 5 caracteres en codificación ZX81. |

---

## 16. Solución de problemas

### 16.1 El interface no arranca o el ZX81 se queda bloqueado

| Síntoma | Solución |
|---------|----------|
| El ZX81 no muestra nada al encender | Comprueba que el interface está correctamente insertado en el puerto de expansión. Desconéctalo y vuelve a conectarlo con el ZX81 apagado. |
| El LED STAT no se ilumina | Comprueba que la tarjeta SD está insertada y que contiene la carpeta SYS con su contenido completo. Sin ella el interface no arranca. |
| LED STAT parpadea en Azul/Rojo durante el arranque | Error inicializando la tarjeta SD. Comprueba que está insertada correctamente y formateada en FAT32. |
| LED STAT parpadea en Naranja/Rojo durante el arranque | Error escribiendo la ROM en RAM. Comprueba que la carpeta SYS contiene los archivos de ROM necesarios. |
| LED STAT parpadea en Naranja durante el arranque | El MCU está esperando respuesta de la FPGA. Si el parpadeo no termina, puede indicar un problema de hardware. |
| Pantalla en blanco o con ruido | Asegúrate de que los pines del conector de expansión no están doblados o sucios. |
| El ZX81 arranca pero los comandos del interface no funcionan | Verifica la versión del firmware con `LOAD *VER`. Para actualizar, copia `firmware.bin` en la raíz de la SD y enciende el ZX81. |

### 16.2 Problemas con la tarjeta microSD

| Síntoma | Solución |
|---------|----------|
| Error `H` al intentar cargar | Comprueba que la SD está correctamente insertada. Extráela y vuélvela a insertar. |
| La SD no se reconoce | Verifica que está formateada en FAT32 (no exFAT ni NTFS). |
| Error `G` al cargar un programa | El archivo no existe con ese nombre. Usa `LOAD *DIR` para ver los nombres exactos. |
| Error `J` al guardar | La tarjeta SD está llena. Usa `LOAD *FREE` para comprobar el espacio disponible. |

### 16.3 El reloj pierde la hora al apagar

El reloj en tiempo real se alimenta de la pila botón **CR2032**. Si el reloj pierde la hora sistemáticamente al apagar el ZX81, probablemente la pila necesita ser reemplazada.

Comprueba el nivel de carga con:
```
LOAD *BAT
```

Si el voltaje es inferior a 2.5V aproximadamente, sustituye la pila por una CR2032 nueva. La pila se encuentra en la placa del interface y puede extraerse con una herramienta plana fina.

### 16.4 El joystick no responde

| Síntoma | Solución |
|---------|----------|
| El joystick no hace nada | Configura el mapeo con `LOAD *JOY "QAOP "` (u otro mapeo según el juego) antes de lanzar el programa. |
| Solo funciona alguna dirección | Comprueba que la cadena de configuración tiene exactamente 5 caracteres. |
| El joystick mueve pero no dispara | Verifica que el quinto carácter de la cadena JOY corresponde a la tecla de fuego del juego. |

### 16.5 El sonido no funciona

| Síntoma | Solución |
|---------|----------|
| No hay sonido con `LOAD *PLAY` | Comprueba que el televisor o monitor está conectado al conector SCART del interface y que el canal de audio del SCART no está silenciado. |
| La voz no se entiende | Prueba con frases cortas y en inglés. Escribe las palabras fonéticamente si el resultado no es satisfactorio. |
| El VGM no suena | Verifica que el archivo es un VGM con datos del chip AY únicamente. Los VGMs con otros chips no son compatibles. |

### 16.6 Errores de actualización de firmware

| Síntoma | Solución |
|---------|----------|
| LED STAT parpadea en Azul/Rojo al arrancar con `firmware.bin` en la SD | Error inicializando la tarjeta SD antes de la actualización. Extrae la SD, comprueba el formato FAT32 y vuelve a intentarlo. |
| LED STAT parpadea en Blanco/Rojo tras intentar actualizar | Error durante la actualización. El archivo `firmware.bin` permanece en la SD. Comprueba que el archivo no está corrupto y vuelve a encender el ZX81 para reintentar. |
| El LED STAT se queda en Amarillo fijo indefinidamente | La actualización está en curso pero tarda más de lo esperado. Espera al menos 2 minutos antes de considerar que hay un problema. No apagues el ZX81. |
| Tras la actualización el interface no responde | Comprueba con `LOAD *VER` que la versión es correcta. Si el interface no arranca, sigue el procedimiento de recuperación de emergencia via USB descrito en la sección 17. |

### 16.7 Uso de la consola de depuración como herramienta de diagnóstico

Cuando el LED STAT muestra un error pero no está claro cuál es la causa, la consola de depuración por USB-C puede proporcionar información adicional muy valiosa.

**Cómo conectarse:**

1. Conecta un cable USB-C entre el interface y el ordenador.
2. Abre un programa de terminal serie (PuTTY, Tera Term, minicom...) y conéctate al puerto COM/serie del CH340 con los parámetros: **115200 baudios, 8N1, sin control de flujo**.
3. Enciende el ZX81 con el interface conectado.
4. Observa los mensajes que aparecen en la terminal durante el arranque y la operación normal.

El firmware de producción emite mensajes básicos de estado que permiten identificar en qué fase falla el arranque, si la tarjeta SD se reconoce correctamente, el resultado de las actualizaciones de firmware y otros eventos relevantes.

> **Consejo para desarrolladores:** Recompilando el firmware con la macro `DEBUG` activa se obtiene una salida mucho más detallada, incluyendo el progreso byte a byte de las operaciones de flash, el estado de los registros del AY, y los detalles de cada comando recibido del Z80.

---

## 17. Actualización del firmware

El SD81 Booster tiene dos componentes de firmware actualizables: el **microcontrolador (MCU)** y la **FPGA**.

> ⚠️ **Importante:** No interrumpas el proceso de actualización una vez iniciado. Una actualización incompleta puede dejar el interface en un estado no operativo.

---

### 17.1 Actualización del microcontrolador (MCU)

El SD81 Booster incorpora un **bootloader** que permite actualizar el firmware de forma muy sencilla, sin herramientas externas ni cables especiales.

**Requisitos:**
- Tarjeta microSD del interface.
- Archivo de firmware `firmware.bin`, disponible en el repositorio del proyecto.

**Proceso:**

1. Descarga el archivo `firmware.bin` desde el repositorio del proyecto.
2. Copia `firmware.bin` en la **raíz** de la tarjeta microSD (no en ninguna subcarpeta).
3. Inserta la tarjeta en el interface con el ZX81 **apagado**.
4. Enciende el ZX81. El bootloader detectará automáticamente el archivo, realizará la actualización y lo borrará de la SD al terminar.

> ⚠️ **No apagues el ZX81 ni extraigas la tarjeta SD durante la actualización.**

Verifica la versión instalada con:
```
LOAD *VER
```

**Recuperación de emergencia via USB:**

En caso de que el interface quede inoperativo y no sea posible actualizar via SD, existe un procedimiento de recuperación via USB-C orientado a usuarios avanzados. Este proceso requiere acceder al interior de la carcasa y manipular el jumper **JP7**. Las instrucciones detalladas están disponibles en el repositorio del proyecto:

[https://codeberg.org/Retrostuff/SD81-Booster](https://codeberg.org/Retrostuff/SD81-Booster)

---

### 17.2 Actualización de la FPGA

La FPGA (Xilinx Spartan-6 XC6SLX9) carga su configuración en cada arranque desde una memoria flash SPI auxiliar (25Q128). La actualización consiste en grabar un archivo MCS en dicha flash mediante JTAG, usando la herramienta **Xilinx ISE iMPACT** y un cable **Xilinx Platform Cable USB**. Este proceso está orientado exclusivamente a **personal técnico especializado**. Las instrucciones completas y los archivos necesarios están disponibles en el repositorio del proyecto.

> ⚠️ **Advertencia:** Una programación incorrecta de la flash SPI puede dejar el interface permanentemente inoperativo.

---

## 18. Glosario

| Término | Definición |
|---------|-----------|
| **Alófono** | Unidad mínima de sonido del habla usada por el sintetizador de voz. El SD81 Booster usa los alófonos del chip SP0256. |
| **Bloque** | División de 8 KB del espacio de direccionamiento del Z80. El SD81 Booster divide los 64 KB del Z80 en 8 bloques (0–7). |
| **FPGA** | Circuito lógico programable (Xilinx Spartan-6 XC6SLX9) que implementa por hardware la lógica de vídeo, el mapeador de memoria y otras funciones del interface. |
| **FAT32** | Sistema de archivos requerido por la tarjeta microSD del interface. Incompatible con exFAT y NTFS. |
| **FAST** | Token del BASIC del ZX81 (SHIFT+F). En el SD81 Booster, activa el modo de carga/guardado desde la SD. |
| **Fichero de pantalla (HFILE)** | Bloque de memoria que contiene los datos de la pantalla en los modos Superfast. |
| **HRG** | *High Resolution Graphics*. Modo de alta resolución del ZX81. |
| **MCU** | Microcontrolador. El chip que gestiona la SD, el sonido, el RTC y la comunicación con el Z80 en el SD81 Booster. |
| **Página** | División de 8 KB de la RAM del interface. Los 512 KB de RAM se dividen en 64 páginas (0–63) que pueden mapearse a cualquier bloque. |
| **PEG** | *Programmable Effects Generator*. Máquina virtual del interface para reproducir efectos de sonido en background sin usar la CPU del ZX81. |
| **RTC** | *Real Time Clock*. Reloj en tiempo real incorporado en el SD81 Booster, alimentado por una pila CR2032. |
| **SLOW** | Token del BASIC del ZX81 (SHIFT+D). En el SD81 Booster, activa el modo de carga/guardado desde cinta (audio). |
| **SP0256** | Chip sintetizador de voz de General Instrument, base del sintetizador del SD81 Booster. Usado también en el Currah MicroSpeech y The Voice. |
| **Superfast** | Modo en el que el hardware del SD81 Booster gestiona el refresco de pantalla liberando la CPU del ZX81 para otras tareas. |
| **T81** | Formato de archivo contenedor que agrupa múltiples programas ZX81. El interface puede navegar su contenido como si fuera un directorio. *(fase alfa)* |
| **Token** | En el BASIC del ZX81, cada palabra reservada (LOAD, PRINT, IF...) se almacena como un único byte en lugar de como letras individuales. Se introducen con combinaciones de SHIFT. |
| **VGM** | *Video Game Music*. Formato de archivo de música que el SD81 Booster puede reproducir en background usando el emulador AY. |
| **Vídeo inverso** | Modo de visualización del ZX81 en el que el fondo y el carácter intercambian colores (fondo negro, letra blanca). Se activa con SHIFT+9 antes del carácter. |

---

## 19. Historial de versiones del firmware

| Versión | Fecha | Novedades principales |
|---------|-------|-----------------------|
| 1.0 | 2025 | Primera versión de lanzamiento público. |

> **Nota:** Este historial se actualizará con cada nueva versión del firmware. Consulta el repositorio del proyecto para ver el registro completo de cambios.

---

## 20. Referencias

### El proyecto SD81 Booster

- **Repositorio oficial** (código fuente, firmware, esquemas, documentación técnica):
  [https://codeberg.org/Retrostuff/SD81-Booster](https://codeberg.org/Retrostuff/SD81-Booster)

- **Va de Retro** — foro de retroinformática en español donde se anunció una versión anterior del interface:
  [https://www.va-de-retro.com/foros/portal](https://www.va-de-retro.com/foros/portal)

---

### Software de ejemplo

- **Mazogs para ZX81 con versión en color para Chroma81** — por Pedro Gimeno (pgimeno). Implementación del clásico juego Mazogs para ZX81, incluyendo una versión en color que aprovecha la funcionalidad Chroma81 compatible con el SD81 Booster:
  [https://codeberg.org/pgimeno/Mazogs](https://codeberg.org/pgimeno/Mazogs)

---

### Herramientas

- **STM32CubeProgrammer** — herramienta de programación del MCU STM32 (necesaria para actualización vía USB en caso de recuperación):
  [https://www.st.com/en/development-tools/stm32cubeprog.html](https://www.st.com/en/development-tools/stm32cubeprog.html)

---

### Documentación técnica de referencia

- **Chip sintetizador de voz SP0256-AL2** (General Instrument) — hoja de características del chip en el que se basa el sintetizador de voz del SD81 Booster:
  [https://rarewaves.net/wp-content/uploads/2018/09/SP0256-AL2.pdf](https://rarewaves.net/wp-content/uploads/2018/09/SP0256-AL2.pdf)

- **Interface Chroma81** — documentación del interface de color para ZX81 cuya funcionalidad implementa el SD81 Booster. El enlace original ya no está disponible; puede encontrarse en archivos web:
  `http://www.fruitcake.plus.com/Sinclair/ZX81/Chroma/ChromaInterface_Documentation.htm`

- **Formato de archivo .P y .P81** — especificación técnica de los formatos de programa del ZX81:
  [https://k1.spdns.de/Develop/Projects/zasm/Info/O80%20and%20P81%20Format.txt](https://k1.spdns.de/Develop/Projects/zasm/Info/O80%20and%20P81%20Format.txt)

- **Especificación del formato VGM** (*Video Game Music*) — formato de archivo de música utilizado por el reproductor VGM del interface:
  [https://vgmrips.net/wiki/VGM_Specification](https://vgmrips.net/wiki/VGM_Specification)

---

### ROM del ZX81

El SD81 Booster incluye una ROM modificada basada en la disassembly original del ZX81. Créditos:

- **Geoff Wearmouth** — disassembly comentada de la ROM del ZX81
  (preservada en: [https://web.archive.org/web/20150815035607/http://www.wearmouth.demon.co.uk/zx81.htm](https://web.archive.org/web/20150815035607/http://www.wearmouth.demon.co.uk/zx81.htm))

- **Tomaž Šolc** — preservación de la disassembly:
  [https://www.tablix.org/~avian/spectrum/rom/](https://www.tablix.org/~avian/spectrum/rom/)

---

### Créditos del proyecto

- **Diseño del hardware y firmware del MCU:** Alejandro Valero (wilco2009)
- **Código Z80 / ROM modificada:** Pedro Gimeno (pgimeno)

---

## Apéndice A — Referencia completa del comando PLAY

### Tabla de duraciones

| Valor | Nombre | Duración a 60 bpm |
|-------|--------|-------------------|
| 1 | Semicorchea | 0.25 s |
| 2 | Semicorchea con puntillo | 0.375 s |
| 3 | Corchea | 0.5 s |
| 4 | Corchea con puntillo | 0.75 s |
| 5 | Negra | 1 s |
| 6 | Negra con puntillo | 1.5 s |
| 7 | Blanca | 2 s |
| 8 | Blanca con puntillo | 3 s |
| 9 | Redonda | 4 s |
| 10 | Tresillo de semicorchea | 0.1667 s |
| 11 | Tresillo de corchea | 0.3333 s |
| 12 | Tresillo de negra | 0.6667 s |

### Efectos de envolvente (W)

| Código | Forma | Descripción |
|--------|-------|-------------|
| W0 | `\|______` | Decaimiento, luego silencio |
| W1 | `/\|______` | Ataque, luego silencio |
| W2 | `\|\|‾‾‾‾` | Decaimiento, luego sostenido |
| W3 | `/‾‾‾‾‾‾` | Ataque, luego sostenido |
| W4 | `\|\|\|\|` | Decaimiento repetido (tremolo) |
| W5 | `/\|/\|/\|` | Ataque repetido |
| W6 | `/\/\/\/` | Ataque y decaimiento repetidos |
| W7 | `\/\/\/\` | Decaimiento y ataque repetidos |

- `U` activa la envolvente en el canal. La envolvente es compartida por todos los canales que tengan `U` activo.
- `X<número>` ajusta la velocidad (0–65535; 6927 ≈ 1 segundo).
- `V<número>` establece el volumen (0–15). Usar `V` después de `U` desactiva la envolvente en ese canal.

### Modo de ruido (M)

`M<número>` selecciona qué canales están activos y en qué modo:

| Valor | Canal | Modo |
|-------|-------|------|
| 1 | A | Tono |
| 2 | B | Tono |
| 4 | C | Tono |
| 8 | A | Ruido |
| 16 | B | Ruido |
| 32 | C | Ruido |

Los valores se suman para combinar modos. M0 = todo apagado. M63 = todos los canales en tono y ruido.

### Tabla resumen de parámetros

| Parámetro | Descripción |
|-----------|-------------|
| `C`..`B` | Nota en octava actual |
| Inv(`C`..`B`) | Nota en octava siguiente (vídeo inverso) |
| `=` | Sostenido (siguiente nota) |
| `£` | Bemol (siguiente nota) o silencio |
| `1`..`12` | Duración desde este punto |
| `-` | Ligadura de duración |
| `N` / espacio | Separador de números |
| `O<n>` | Octava (0–8, defecto 4) |
| `T<n>` | Tempo en bpm (60–240, defecto 120) — solo canal A |
| `V<n>` | Volumen (0–15) |
| `W<n>` | Efecto de envolvente (0–7) |
| `U` | Activa envolvente en el canal |
| `X<n>` | Velocidad de envolvente (0–65535) |
| `M<n>` | Selección de canales activos y modo (0–63) |
| `(` `)` | Repetir sección una vez más |
| `)` | Repetir desde el inicio indefinidamente |
| `H` | Detener PLAY en todos los canales |
| `*` | (Solo canal A) Reproducción en background |

---

## Apéndice B — Referencia del generador de efectos PEG

El PEG es una máquina virtual de 16 bits con acceso a los 16 registros del chip AY (R0–R15) y 16 variables de propósito general (V0–V15). Soporta hasta 3 hilos de ejecución paralelos y hasta 256 palabras (instrucciones de 16 bits) de programa.

### Juego de instrucciones

| Instrucción | Codificación | Descripción |
|-------------|--------------|-------------|
| `LD R,XX` | `0R XX` | Carga un registro AY con un valor de 8 bits |
| `ADD R,XX` | `1R XX` | Suma un valor de 8 bits a un registro AY |
| `LD V,XX` | `2R XX` | Carga una variable con un valor de 8 bits (resto a cero) |
| `ADD V,XX` | `3R XX` | Suma un valor de 8 bits a una variable |
| `LD R,R` | `40 RR` | Carga un registro con otro registro |
| `LD R,V` | `41 RV` | Carga un registro con una variable |
| `LD V,R` | `42 VR` | Carga una variable con un registro |
| `LD V,V` | `43 VV` | Carga una variable con otra variable |
| `ADD V,V` | `44 VV` | Suma dos variables |
| `SUB V,V` | `45 VV` | Resta la segunda variable de la primera |
| `ADC V,V` | `46 VV` | Suma con acarreo |
| `SBC V,V` | `47 VV` | Resta con acarreo |
| `NOT V,V` | `48 VV` | Carga la primera con el complemento a 1 de la segunda |
| `AND V,V` | `49 VV` | AND bit a bit |
| `OR V,V` | `4A VV` | OR bit a bit |
| `XOR V,V` | `4B VV` | XOR bit a bit |
| `MUL V,V` | `4C VV` | Multiplica dos variables (resultado 32 bits en V y V+1) |
| `DIV V,V` | `4D VV` | Divide (cociente en V, resto en V+1) |
| `SHR V,X` | `4E VX` | Desplazamiento a la derecha |
| `SHL V,X` | `4F VX` | Desplazamiento a la izquierda |
| `MUL V,XX` | `5V XX` | Multiplica una variable por una constante |
| `DIV V,XX` | `6V XX` | Divide una variable por una constante |
| `SUB V,XX` | `7V XX` | Resta una constante de una variable |
| `DJNZ V,XX` | `8V XX` | Decrementa y salta si no es cero |
| `WAIT XXX` | `9X XX` | Espera el tiempo indicado en ms antes de continuar |
| `WAIT V` | `A0 0V` | Espera el tiempo indicado en la variable (ms) |
| `HALT` | `A0 10` | Detiene el efecto |
| `JR XX` | `A1 XX` | Salto relativo |

> Los offsets de salto son relativos a la instrucción siguiente. El ensamblador PEG incluido en el repositorio gestiona esto automáticamente.

---

## Apéndice C — Diccionario del sintetizador de voz

El sintetizador se basa en los fonemas del chip **SP0256** de General Instrument, el mismo utilizado por el Currah MicroSpeech (ZX Spectrum) y The Voice (Videopac G7000/Odyssey 2).

El sintetizador analiza el texto de izquierda a derecha buscando la coincidencia más larga posible. Las entradas más largas tienen prioridad.

### Palabras reconocidas (selección por longitud)

**13 caracteres:** INVESTIGATORS, IRRESPONSIBLE

**12 caracteres:** INVESTIGATOR

**11 caracteres:** INVESTIGATE

**10 caracteres:** CORRECTING

**9 caracteres:** COGNITIVE, CORRECTED, SEPTEMBER, SINCERELY, SINCERITY, INTERFACE

**8 caracteres:** CHECKERS, CHECKING, COMPUTER, CORRECTS, DAUGHTER, DECEMBER, EIGHTEEN, FEBRUARY, FREEZERS, FREEZING, NINETEEN, NOVEMBER, PLEDGING, SATURDAY

**7 caracteres:** BOOSTER, CHECKED, CHECKER, CORRECT, FREEZER, JANUARY, MINUTES, OCTOBER, PLASTIC, SIXTEEN, TUESDAY, COLLIDE

**6 caracteres:** AUGUST, COOKIE, EQUALS, EXTENT, FRIDAY, FROZEN, MONDAY, SUNDAY, TALKED, TALKER, TWENTY

**5 caracteres:** APRIL, CHECK, CROWN, EIGHT, EQUAL, ERROR, FIFTY, HELLO, MARCH, MONTH, SIXTY, TALKS, THREE, WORLD

**4 caracteres:** DATE, FIVE, FOUR, HAVE, JUNE, NINE, RAYS, TALK, THIS, TIME, WHAT, WHOA, WILL, ZX81

**3 caracteres:** ACK, ACT, ADD, AMP, ASH, ASK, BAD, BED, BIG, BOX, BUT, CAR, END, EST, GET, HAS, HIM, ICK, IMP, ING, INK, JOB, KEY, MAY, NOT, NOW, OLD, OUR, OUT, RAY, RED, SIX, SUN, TEN, THE, TOP, TWO, YES — además de todas las sílabas con dígrafos DH, NG, SH, TH, WH.

### Letras individuales

Cuando no se encuentra ninguna coincidencia mayor, cada letra se pronuncia por su nombre en inglés (A="EY", B="BEE", C="SEE", etc.).

### Puntuación y pausas

| Carácter | Efecto |
|----------|--------|
| Espacio | Pausa corta |
| `,` | Pausa media |
| `;` `:` | Pausa larga |

### Números

Los números se leen automáticamente en inglés desde 0 hasta los billones.

### Alófonos directos (uso avanzado)

**Pausas:** PA1, PA2, PA3, PA4, PA5

**Vocales:** AA, AE, AH, AO, AW, AX, AY, EH, ER1, ER2, EY, IH, IY, OW, OY, UH, UW1, UW2, XR, YR

**Consonantes:** BB1, BB2, CH, DD1, DD2, DH1, DH2, EL, FF, GG1, GG2, GG3, HH1, HH2, JH, KK1, KK2, KK3, LL, MM, NG, NN1, NN2, OR, PP, RR1, RR2, SH, SS, TH, TT1, TT2, VV, WH, WW, YY1, YY2, ZH, ZZ

---

## Apéndice D — Sistema de paginación de memoria

### Asignación inicial de páginas

| Bloque | Página inicial | Rango de direcciones |
|--------|---------------|----------------------|
| 0 | 0 | 0000–1FFF (ROM, solo lectura) |
| 1 | 1 | 2000–3FFF (ROM de expansión) |
| 2 | 2 | 4000–5FFF (RAM principal) |
| 3 | 3 | 6000–7FFF (RAM principal) |
| 4 | 4 | 8000–9FFF (RAM ampliada) |
| 5 | 5 | A000–BFFF (RAM ampliada) |
| 6 | 2 | C000–DFFF (espejo de bloque 2) |
| 7 | 3 | E000–FFFF (espejo de bloque 3) |

> **Nota:** La asignación anterior es la del **modo normal**. Con el modo RAM48 activo (ver sección 8.4), al arrancar los bloques 6 y 7 se inicializan con las páginas 6 y 7 respectivamente, en lugar de 2 y 3. Esto es necesario para que los programas que usan las expansiones de RAM de 48 KB accedan al área 48–64 KB correctamente.

### Reglas de uso

- El bloque 0 es siempre de solo lectura. Los bloques 1–7 son siempre de lectura/escritura.
- Una misma página puede estar mapeada en más de un bloque simultáneamente.
- Los bloques 6 y 7 deben espejar los bloques 2 y 3 respectivamente para que el sistema de vídeo funcione. Excepción: si el fichero de pantalla está completamente en el bloque 2, el bloque 7 puede mapearse libremente; si está completamente en el bloque 3, el bloque 6 puede mapearse libremente.
- Los bloques 4 y 5 siempre pueden mapearse libremente.
- Por las peculiaridades del hardware del ZX81, los bloques 4–7 solo pueden usarse para datos, no para ejecutar código (salvo con el modo MC45 activo para los bloques 4 y 5).

### Modo de paginación simple (hasta 256 KB)

Puerto `E7h`, 8 bits:

```
D7  D6  D5  D4  D3  D2  D1  D0
 \   \   \   \   /   \   \   /
  Página (0–31)    Bloque (0–7)
```

### Modo de paginación completa (hasta 512 KB)

Se usa la instrucción `OUT (C),r` con la dirección del puerto conteniendo los bits de página adicionales en A13–A8. El número de página completo (0–63) va en el registro `B` y el número de bloque en el registro que se envía:

```asm
        ld      c,0E7h
        out     (c),a       ; A = bloque (0-7), B = página (0-63)
```

### Modificación de la ROM

Es posible modificar la ROM mapeando la página 0 a cualquier bloque con escritura habilitada y realizando los cambios allí, o sustituir completamente la ROM cargando el contenido deseado en cualquier página y mapeándola al bloque 0.

---

## Apéndice E — Puerto del interface Chroma81 (7FEFh)

El SD81 Booster implementa la interfaz de color del **Chroma81** a través del puerto **7FEFh** (01111111 11101111 en binario). Este puerto puede leerse y escribirse.

---

### Escritura (OUT 7FEFh)

Permite configurar el modo de color y el color del borde:

```
Bit 7  Bit 6  Bit 5  Bit 4  Bit 3  Bit 2  Bit 1  Bit 0
  |      |      |      |      |      |      |      |
  |      |      |      |      |      +------+------+---- Color del borde (formato GRB)
  |      |      |      |      +------------------------  Bit de brillo del borde
  |      |      |      +-------------------------------  Modo (0=código de carácter, 1=fichero de atributos)
  |      |      +-------------------------------------   1=Activar modo color
  +------+--------------------------------------------   Reservado (siempre a 0)
```

| Bits | Función |
|------|---------|
| 7–6 | Reservado para uso futuro. Siempre a 0. |
| 5 | Activar modo color: `1` = color activado. |
| 4 | Modo de color: `0` = código de carácter, `1` = fichero de atributos. |
| 3 | Bit de brillo del color del borde. |
| 2–0 | Color del borde en formato GRB (Green-Red-Blue). |

**Formato del color de borde (bits 2–0, formato GRB):**

| Valor | Color |
|-------|-------|
| 000 | Negro |
| 001 | Azul |
| 010 | Rojo |
| 011 | Magenta |
| 100 | Verde |
| 101 | Cian |
| 110 | Amarillo |
| 111 | Blanco |

---

### Lectura (IN 7FEFh)

Permite detectar si el modo color está disponible y leer el estado del VSync:

```
Bit 7  Bit 6  Bit 5  Bit 4  Bit 3  Bit 2  Bit 1  Bit 0
  |      |      |      |      |      |      |      |
  |      |      |      +------+------+------+------+---- No usado (reservado)
  |      |      +-------------------------------------   0 = Modos de color disponibles (siempre 0)
  +------+--------------------------------------------   No usado (reservado)
                                                     \-- Estado interrupción VSync
```

| Bit | Función |
|-----|---------|
| 7–6 | No usado (reservado). |
| 5 | `0` = modos de color disponibles. Este bit es siempre 0, lo que indica que el modo color está siempre activo. |
| 4–1 | No usado (reservado). |
| 0 | **Estado de la interrupción VSync.** `1` = el haz de pantalla está en el periodo de blanking vertical (pantalla pintada). |

> **Nota:** La lectura del bit 5 a 0 permite a los programas detectar automáticamente la presencia del interface Chroma81 y activar los modos de color si está disponible.

> **Sincronización con VSync:** El bit 0 permite a la CPU esperar a que la pantalla haya terminado de pintarse antes de actualizar su contenido, evitando parpadeos y artefactos visuales. Muchos juegos del ZX Spectrum usaban la instrucción `HALT` o una rutina de interrupción para sincronizarse con el VSync; en el SD81 Booster este mecanismo es el equivalente directo para esa funcionalidad:
> ```asm
>         ; Esperar al inicio del VSync
> WAIT:   in   a,($A7)
>         rrca              ; bit 0 al carry
>         jr   nc,WAIT      ; si carry=0, pantalla todavía pintándose
>         ; aquí ya se ha completado el refresco, seguro actualizar vídeo
> ```

---

## Apéndice F — Modos Superfast y Spectrum

### El problema del vídeo en el ZX81 original

El ZX81 en modo SLOW gestiona el vídeo por software: durante el refresco de la pantalla, la CPU debe ejecutar instrucciones NOP mientras el hardware genera la señal de vídeo, lo que consume una parte importante del tiempo de procesador disponible. El SD81 Booster resuelve esto con el **modo Superfast**.

---

### Modo Superfast

En el modo Superfast, el hardware del interface toma el control del bus de datos durante el refresco de pantalla, colocando los datos de vídeo directamente sin intervención de la CPU. Esto libera al procesador para ejecutar código útil durante todo el ciclo, aumentando significativamente la velocidad efectiva del sistema.

Existen tres variantes del modo Superfast, seleccionables mediante POKEs en la dirección 2045:

| POKE | Modo |
|------|------|
| `POKE 2045,170` | Superfast texto (modo texto estándar acelerado) |
| `POKE 2045,171` | Superfast HiRes nativo (alta resolución en formato ZX81) |
| `POKE 2045,172` | Superfast HiRes Spectrum (alta resolución en formato Spectrum) |
| `POKE 2045,85`  | Desactivar modo Superfast |

Antes de activar el modo, es necesario indicar la dirección del fichero de pantalla (HFILE) mediante dos POKEs:

```
POKE 2043, HFILE_low    : REM parte baja de la dirección del fichero de pantalla
POKE 2044, HFILE_high   : REM parte alta de la dirección del fichero de pantalla
```

---

### Modo Spectrum

El modo Spectrum (`POKE 2045,172`) reordena las líneas de pantalla para que coincidan con la organización de la pantalla del ZX Spectrum, facilitando la conversión de programas entre ambas plataformas. En el ZX81 estándar las líneas se organizan de forma diferente a como lo hace el Spectrum; este modo elimina esa diferencia por hardware.

> ⚠️ **Importante:** En el modo Spectrum se emula el puerto del beeper y borde del Spectrum (`ULA write port = FBh`), donde los bits 2–0 controlan el color del borde y los bits 4–3 controlan el beeper. Esto implica que **en este modo el puerto FBh queda ocupado y se pierde la compatibilidad con la ZX Printer**, que también usa ese puerto.

---

### Control del borde

**Cambiar los atributos del borde:**

```
POKE 2046, <attr>    : REM establece los atributos de color del borde
```

**Definir y activar un patrón de borde:**

Es posible definir un patrón de 8 bytes que se repetirá a lo largo del borde de la pantalla, permitiendo rellenarlo completamente con un diseño personalizado:

```
POKE 2048, byte0     : REM primer byte del patrón
POKE 2049, byte1
...
POKE 2055, byte7     : REM último byte del patrón (8 bytes en total)
POKE 2047, 170       : REM activar patrón de borde
POKE 2047, 85        : REM desactivar patrón de borde
```

**Puerto ULA (modo Spectrum) — FBh:**

| Bits | Función |
|------|---------|
| 4–3 | Control del beeper |
| 2–0 | Color del borde |

---

### Sincronización con VSYNC

En el ZX Spectrum, muchos juegos utilizaban la instrucción `HALT` o una rutina de interrupción IM1/IM2 para sincronizarse con el barrido vertical de la pantalla y conseguir animaciones fluidas sin parpadeo.

En el SD81 Booster esta funcionalidad se sustituye mediante la lectura del **bit 0 del puerto A7h**, que refleja el estado de la interrupción de sincronismo vertical (VSYNC). La CPU puede esperar a este bit para sincronizarse con el inicio del refresco de pantalla sin necesidad de interrupciones ni de `HALT`. Consulta el apartado de puertos de E/S en la sección 14.2 para el ejemplo de código.

---

### Resumen de POKEs de control

| Dirección | Valor | Función |
|-----------|-------|---------|
| 2043 | `<bajo>` | Parte baja de la dirección del fichero de pantalla |
| 2044 | `<alto>` | Parte alta de la dirección del fichero de pantalla |
| 2045 | 170 | Activar Superfast texto |
| 2045 | 171 | Activar Superfast HiRes nativo |
| 2045 | 172 | Activar Superfast HiRes Spectrum |
| 2045 | 85 | Desactivar Superfast |
| 2046 | `<attr>` | Cambiar atributos del borde |
| 2047 | 170 | Activar patrón de borde |
| 2047 | 85 | Desactivar patrón de borde |
| 2048–2055 | `<datos>` | Definir patrón de borde (8 bytes) |

---

*Manual de Usuario SD81 Booster v1.0 — Hardware y software de código abierto*

---

## Apéndice G — Referencia técnica de audio: chip AY, VGM y alófonos

### Registros del chip AY-3-8910/12

El emulador AY del SD81 Booster es compatible a nivel de registro con el chip original. Soporta tres voces independientes, envolvente y ruido.

| Registro | Descripción | B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
|----------|-------------|----|----|----|----|----|----|----|----|
| R0  | Canal A — período de tono (byte bajo)  | B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
| R1  | Canal A — período de tono (byte alto)  | — | — | — | — | B3 | B2 | B1 | B0 |
| R2  | Canal B — período de tono (byte bajo)  | B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
| R3  | Canal B — período de tono (byte alto)  | — | — | — | — | B3 | B2 | B1 | B0 |
| R4  | Canal C — período de tono (byte bajo)  | B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
| R5  | Canal C — período de tono (byte alto)  | — | — | — | — | B3 | B2 | B1 | B0 |
| R6  | Período de ruido | — | — | — | B4 | B3 | B2 | B1 | B0 |
| R7  | Habilitación de canales | — | — | Ruido C | Ruido B | Ruido A | Tono C | Tono B | Tono A |
| R8  | Amplitud canal A | — | — | — | Modo env. | L3 | L2 | L1 | L0 |
| R9  | Amplitud canal B | — | — | — | Modo env. | L3 | L2 | L1 | L0 |
| R10 | Amplitud canal C | — | — | — | Modo env. | L3 | L2 | L1 | L0 |
| R11 | Período de envolvente (byte bajo) | B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
| R12 | Período de envolvente (byte alto) | B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
| R13 | Forma de envolvente | — | — | — | — | — | B2 | B1 | B0 |

En R7, un bit a **0** activa el canal; a **1** lo desactiva. En R8–R10, si el bit de Modo env. está activo, la amplitud la controla la envolvente (R11–R13) en lugar de L3–L0.

---

### Opcodes del reproductor VGM

El reproductor VGM del interface solo interpreta los opcodes referentes al chip AY. El resto se ignoran sin producir error.

| Opcode | Parámetros | Descripción |
|--------|-----------|-------------|
| `61h` | `nn nn` | Esperar n muestras (little-endian, 0–65535; ≈ 1,49 s máximo). Pausas largas se representan con varios comandos de espera consecutivos. |
| `62h` | — | Esperar 735 muestras (1/60 de segundo). Equivale a `61h DFh 02h`. |
| `63h` | — | Esperar 882 muestras (1/50 de segundo). Equivale a `61h 72h 03h`. |
| `A0h` | `aa dd` | Escribir el valor `dd` en el registro AY número `aa`. |

---

### Tabla de alófonos SP0256-AL2

Los alófonos se usan con el comando MCU `BINARY SAY` (16h) para síntesis fonética directa de precisión. El acceso de alto nivel mediante texto (`LOAD *SAY "texto"`) no requiere conocer estos códigos.

| Código | Alófono | Ejemplo | Código | Alófono | Ejemplo |
|--------|---------|---------|--------|---------|---------|
| `$00` | PA1 | pausa 10 ms | `$20` | AW | **ou**t |
| `$01` | PA2 | pausa 30 ms | `$21` | DD2 | **d**o |
| `$02` | PA3 | pausa 50 ms | `$22` | GG3 | wi**g** |
| `$03` | PA4 | pausa 100 ms | `$23` | VV | **v**est |
| `$04` | PA5 | pausa 200 ms | `$24` | GG1 | **g**ot |
| `$05` | OY | b**oy** | `$25` | SH | **sh**ip |
| `$06` | AY | sk**y** | `$26` | ZH | a**z**ure |
| `$07` | EH | **e**nd | `$27` | RR2 | b**r**ain |
| `$08` | KK3 | **c**omb | `$28` | FF | **f**ood |
| `$09` | PP | **p**ow | `$29` | KK2 | **sk**y |
| `$0A` | JH | dod**ge** | `$2A` | KK1 | **c**an't |
| `$0B` | NN1 | thi**n** | `$2B` | ZZ | **z**oo |
| `$0C` | IH | s**i**t | `$2C` | NG | a**ng**chor |
| `$0D` | TT2 | **t**o | `$2D` | LL | **l**ake |
| `$0E` | RR1 | **r**ural | `$2E` | WW | **w**ool |
| `$0F` | AX | s**u**cceed | `$2F` | XR | r**epair** |
| `$10` | MM | **m**ilk | `$30` | WH | **wh**ig |
| `$11` | TT1 | par**t** | `$31` | YY1 | **y**es (corto) |
| `$12` | DH1 | **th**ey | `$32` | CH | **ch**urch |
| `$13` | IY | s**ee** | `$33` | ER1 | f**ir** (corto) |
| `$14` | EY | b**ei**ge | `$34` | ER2 | f**ir** (largo) |
| `$15` | DD1 | coul**d** | `$35` | OW | b**eau** |
| `$16` | UW1 | t**oo** | `$36` | DH2 | **th**ey |
| `$17` | AO | **au**ght | `$37` | SS | ve**s**t |
| `$18` | AA | h**o**t | `$38` | NN2 | **n**o |
| `$19` | YY2 | **y**es (largo) | `$39` | HH2 | **h**oe |
| `$1A` | AE | h**a**t | `$3A` | OR | st**ore** |
| `$1B` | HH1 | **h**e | `$3B` | AR | al**ar**m |
| `$1C` | BB1 | **b**usiness (corto) | `$3C` | YR | cl**ear** |
| `$1D` | TH | **th**in | `$3D` | GG2 | **g**uest |
| `$1E` | UH | b**oo**k | `$3E` | EL | sadd**le** |
| `$1F` | UW2 | f**oo**d | `$3F` | BB2 | **b**usiness (largo) |

*Manual de Usuario SD81 Booster v1.0 — Hardware y software de código abierto*
