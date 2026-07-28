------------------------------------------------------------------------

![](media/image1.png){width="4.375in" height="3.125in"}

**Manual de Usuario**

*Interfaz de expansión para Sinclair ZX81*

**SD · AY Sound · Speech · 512KB RAM · RTC**

Versión 1.0

*Hardware y software de código abierto*

**Índice de contenidos**

[Guía de inicio rápido](#guía-de-inicio-rápido)

[1. Introducción](#introducción)

[2. Descripción del hardware](#descripción-del-hardware)

[2.1 Panel lateral derecho](#panel-lateral-derecho)

[2.2 Panel lateral izquierdo y panel trasero](#panel-lateral-izquierdo-y-panel-trasero)

[2.3 Panel superior --- LEDs de estado](#panel-superior-leds-de-estado)

[2.4 Tabla de estados del LED STAT](#tabla-de-estados-del-led-stat)

[3. Contenido de la caja](#contenido-de-la-caja)

[4. Instalación](#instalación)

[4.1 Antes de conectar el interface](#antes-de-conectar-el-interface)

[4.2 Conexión](#conexión)

[4.3 Comprobación de la instalación](#comprobación-de-la-instalación)

[5. Preparación de la tarjeta microSD](#preparación-de-la-tarjeta-microsd)

[5.1 Formato de la tarjeta](#formato-de-la-tarjeta)

[5.2 Caracteres permitidos en nombres de archivo](#caracteres-permitidos-en-nombres-de-archivo)

[5.3 Estructura de carpetas recomendada](#estructura-de-carpetas-recomendada)

[5.4 El programa AUTOEXEC](#el-programa-autoexec)

[6. Primeros pasos](#primeros-pasos)

[6.1 Modos de carga: SD o cinta](#modos-de-carga-sd-o-cinta)

[6.2 Cargando tu primer programa desde la SD](#cargando-tu-primer-programa-desde-la-sd)

[7. Carga y guardado desde la SD](#carga-y-guardado-desde-la-sd)

[7.1 Cargar un programa](#cargar-un-programa)

[7.2 Guardar un programa](#guardar-un-programa)

[7.3 Cargar y guardar bloques de memoria (código máquina)](#cargar-y-guardar-bloques-de-memoria-código-máquina)

[7.4 Cargar siempre desde cinta (independientemente del modo)](#cargar-siempre-desde-cinta-independientemente-del-modo)

[7.5 Notas sobre compatibilidad de juegos](#notas-sobre-compatibilidad-de-juegos)

[7.6 Formatos de archivo reconocidos](#formatos-de-archivo-reconocidos)

[8. Gestión de archivos y directorios](#gestión-de-archivos-y-directorios)

[8.1 Ver el contenido de la SD](#ver-el-contenido-de-la-sd)

[8.2 Cambiar de directorio](#cambiar-de-directorio)

[8.3 Crear y eliminar carpetas](#crear-y-eliminar-carpetas)

[8.4 Borrar, renombrar y copiar archivos](#borrar-renombrar-y-copiar-archivos)

[8.5 Espacio libre en la SD](#espacio-libre-en-la-sd)

[8.6 Directorios T81 (función en fase alfa)](#directorios-t81-función-en-fase-alfa)

[9. Funciones adicionales](#funciones-adicionales)

[9.1 Reproducción de archivos WAV](#reproducción-de-archivos-wav)

[9.2 Reloj en tiempo real --- Comando RTC](#reloj-en-tiempo-real-comando-rtc)

[9.3 Estado de la batería del RTC --- Comando BAT](#estado-de-la-batería-del-rtc-comando-bat)

[9.4 Modo RAM extendida --- Comando RAM48](#modo-ram-extendida-comando-ram48)

[9.5 Visualización de archivos de texto --- Comando THEN PRINT](#visualización-de-archivos-de-texto-comando-then-print)

[Sistema de ayuda integrado](#sistema-de-ayuda-integrado)

[9.6 Joystick programable --- Comando JOY](#joystick-programable-comando-joy)

[10. Sonido](#sonido)

[10.1 Comando PLAY --- Música con el chip AY](#comando-play-música-con-el-chip-ay)

[Notas](#notas)

[Duración](#duración)

[Tempo](#tempo)

[Octava](#octava)

[Repeticiones](#repeticiones)

[Efectos de volumen (envolvente)](#efectos-de-volumen-envolvente)

[10.2 Reproductor VGM --- Música en segundo plano](#reproductor-vgm-música-en-segundo-plano)

[10.3 Generador de efectos PEG --- Efectos de sonido programables](#generador-de-efectos-peg-efectos-de-sonido-programables)

[10.4 Síntesis de voz --- Comando SAY](#síntesis-de-voz-comando-say)

[Reproducción en background](#reproducción-en-background)

[Cómo funciona el sintetizador](#cómo-funciona-el-sintetizador)

[11. Gestión de memoria](#gestión-de-memoria)

[11.1 Conceptos básicos: bloques y páginas](#conceptos-básicos-bloques-y-páginas)

[11.2 Comando MAP --- Asignar páginas a bloques](#comando-map-asignar-páginas-a-bloques)

[11.3 Modo MC45 --- Código máquina en bloques 4 y 5](#modo-mc45-código-máquina-en-bloques-4-y-5)

[11.4 Arranque con ROM alternativa](#arranque-con-rom-alternativa)

[11.5 Caracteres definibles por el usuario (128C / 64C)](#caracteres-definibles-por-el-usuario-128c-64c)

[Ejemplo: pantalla de inicio estilo Spectrum](#ejemplo-pantalla-de-inicio-estilo-spectrum)

[12. Extensiones BASIC avanzadas](#extensiones-basic-avanzadas)

[12.1 Manipulación de cadenas](#manipulación-de-cadenas)

[12.2 Copia y relleno de bloques de memoria](#copia-y-relleno-de-bloques-de-memoria)

[12.3 Ejecución de código máquina](#ejecución-de-código-máquina)

[12.4 Acceso a puertos de entrada/salida](#acceso-a-puertos-de-entradasalida)

[12.5 Acceso a memoria de 16 bits](#acceso-a-memoria-de-16-bits)

[12.6 Acceso al directorio desde un programa](#acceso-al-directorio-desde-un-programa)

[13. Ejemplos de programas](#ejemplos-de-programas)

[13.1 Reloj en tiempo real (RTC)](#reloj-en-tiempo-real-rtc)

[13.2 Estado de la batería del RTC (BAT)](#estado-de-la-batería-del-rtc-bat)

[13.3 Comprobación del modo MC45](#comprobación-del-modo-mc45)

[13.4 Modo Superfast --- demostración de velocidad](#modo-superfast-demostración-de-velocidad)

[13.5 Carga de imagen en modo Spectrum](#carga-de-imagen-en-modo-spectrum)

[14. Códigos de error](#códigos-de-error)

[15. Para programadores](#para-programadores)

[15.1 Mapa de la ROM de expansión](#mapa-de-la-rom-de-expansión)

[15.2 Puertos de E/S y protocolo MCU](#puertos-de-es-y-protocolo-mcu)

[Sincronización con VSYNC](#sincronización-con-vsync)

[15.3 Mapeador de memoria (puerto E7h)](#mapeador-de-memoria-puerto-e7h)

[15.4 Consola de depuración (puerto USB-C)](#consola-de-depuración-puerto-usb-c)

[15.5 Tabla completa de comandos MCU](#tabla-completa-de-comandos-mcu)

[Códigos de error devueltos por los comandos](#códigos-de-error-devueltos-por-los-comandos)

[Comandos de sistema](#comandos-de-sistema)

[Comandos de sistema de archivos](#comandos-de-sistema-de-archivos)

[Comandos de control del hardware](#comandos-de-control-del-hardware)

[Comandos de síntesis de voz](#comandos-de-síntesis-de-voz)

[Comandos AY / sonido](#comandos-ay-sonido)

[Comandos VGM](#comandos-vgm)

[Comandos PEG](#comandos-peg)

[Comandos RTC y batería](#comandos-rtc-y-batería)

[16. Solución de problemas](#solución-de-problemas)

[16.1 El interface no arranca o el ZX81 se queda bloqueado](#el-interface-no-arranca-o-el-zx81-se-queda-bloqueado)

[16.2 Problemas con la tarjeta microSD](#problemas-con-la-tarjeta-microsd)

[16.3 El reloj pierde la hora al apagar](#el-reloj-pierde-la-hora-al-apagar)

[16.4 El joystick no responde](#el-joystick-no-responde)

[16.5 El sonido no funciona](#el-sonido-no-funciona)

[16.6 Errores de actualización de firmware](#errores-de-actualización-de-firmware)

[16.7 Uso de la consola de depuración como herramienta de diagnóstico](#uso-de-la-consola-de-depuración-como-herramienta-de-diagnóstico)

[17. Actualización del firmware](#actualización-del-firmware)

[17.1 Actualización del microcontrolador (MCU)](#actualización-del-microcontrolador-mcu)

[17.2 Actualización de la FPGA](#actualización-de-la-fpga)

[18. Glosario](#glosario)

[19. Historial de versiones del firmware](#historial-de-versiones-del-firmware)

[20. Referencias](#referencias)

[El proyecto SD81 Booster](#el-proyecto-sd81-booster)

[Herramientas](#herramientas)

[Documentación técnica de referencia](#documentación-técnica-de-referencia)

[ROM del ZX81](#rom-del-zx81)

[Créditos del proyecto](#créditos-del-proyecto)

[Apéndice A --- Referencia completa del comando PLAY](#apéndice-a-referencia-completa-del-comando-play)

[Tabla de duraciones](#tabla-de-duraciones)

[Efectos de envolvente (W)](#efectos-de-envolvente-w)

[Tabla resumen de parámetros](#tabla-resumen-de-parámetros)

[Apéndice B --- Referencia del generador de efectos PEG](#apéndice-b-referencia-del-generador-de-efectos-peg)

[Apéndice C --- Diccionario del sintetizador de voz](#apéndice-c-diccionario-del-sintetizador-de-voz)

[Palabras reconocidas (selección por longitud)](#palabras-reconocidas-selección-por-longitud)

[Puntuación y pausas](#puntuación-y-pausas)

[Alófonos directos (uso avanzado)](#alófonos-directos-uso-avanzado)

[Apéndice D --- Sistema de paginación de memoria](#apéndice-d-sistema-de-paginación-de-memoria)

[Asignación inicial de páginas](#asignación-inicial-de-páginas)

[Reglas de uso](#reglas-de-uso)

[Modificación de la ROM](#modificación-de-la-rom)

[Apéndice E --- Puerto del interface Chroma81 (7FEFh)](#apéndice-e-puerto-del-interface-chroma81-7fefh)

[Escritura (OUT 7FEFh)](#escritura-out-7fefh)

[Formato del color de borde (bits 2--0, formato GRB)](#formato-del-color-de-borde-bits-20-formato-grb)

[Lectura (IN 7FEFh)](#lectura-in-7fefh)

[Apéndice F --- Modos Superfast y Spectrum](#apéndice-f-modos-superfast-y-spectrum)

[El problema del vídeo en el ZX81 original](#el-problema-del-vídeo-en-el-zx81-original)

[Modo Superfast](#modo-superfast)

[Modo Spectrum](#modo-spectrum)

[Control del borde](#control-del-borde)

[Sincronización con VSYNC](#sincronización-con-vsync-1)

[Doble buffer (present-blit)](#doble-buffer-present-blit)

[Cómo genera la imagen el interface (sin doble buffer)](#cómo-genera-la-imagen-el-interface-sin-doble-buffer)

[Qué cambia el doble buffer](#qué-cambia-el-doble-buffer)

[Resumen de POKEs de control](#resumen-de-pokes-de-control)

[Apéndice G --- Referencia técnica de audio: chip AY, VGM y alófonos](#apéndice-g-referencia-técnica-de-audio-chip-ay-vgm-y-alófonos)

[Registros del chip AY-3-8910/12](#registros-del-chip-ay-3-891012)

[Puertos de E/S --- dos chips AY compatibles ZonX-81](#puertos-de-es-dos-chips-ay-compatibles-zonx-81)

[Opcodes del reproductor VGM](#opcodes-del-reproductor-vgm)

[Tabla de alófonos SP0256-AL2](#tabla-de-alófonos-sp0256-al2)

*Al abrir el documento, haz clic derecho sobre el índice y selecciona «Actualizar campo» para ver los números de página.*

# Guía de inicio rápido

Si acabas de abrir la caja y quieres empezar cuanto antes, sigue estos cinco pasos:

**1. Prepara la tarjeta microSD**

- Formatea una tarjeta microSD en FAT32.

- Copia la carpeta SYS y todo su contenido en la raíz de la tarjeta. Sin esta carpeta el interface no arrancará.

- Copia tus archivos .P en la tarjeta, organizados en carpetas si lo deseas.

**2. Conecta el interface**

- Apaga el ZX81.

- Conecta el SD81 Booster al puerto de expansión trasero del ZX81.

- Inserta la tarjeta microSD en la ranura del interface.

**3. Enciende y comprueba**

- Enciende el ZX81. Debe arrancar con normalidad mostrando el cursor K.

- El LED STAT del panel superior debe iluminarse en verde fijo.

**4. Carga tu primer programa**

> LOAD FAST \"NOMBRE\"

Sustituye NOMBRE por el nombre del archivo sin la extensión .P.

**5. ¿Quieres explorar qué hay en la SD?**

> LOAD \*DIR

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *Para más detalles sobre cualquiera de estos pasos, consulta las secciones correspondientes del manual.* |

# 1. Introducción

El SD81 Booster es una interfaz de expansión de hardware abierto para el Sinclair ZX81 que amplía considerablemente las capacidades originales del ordenador. Entre sus principales características:

- Carga y guardado desde tarjeta microSD en formato .P, el mismo que utilizan los emuladores más populares.

- Hasta 512 KB de RAM mediante un mapeador de memoria en bloques de 8 KB.

- Emulación del chip de sonido AY-3-8910/12 con soporte para el comando PLAY.

- Reproductor de archivos VGM en segundo plano.

- Síntesis de voz con muestras de audio almacenadas en la memoria interna del microcontrolador.

- Hasta 128 caracteres definibles por el usuario, con compatibilidad con el modo de definición de caracteres del interface QuickSilva.

- Compatibilidad con el interface Chroma81, que proporciona salida de vídeo RGB color para el ZX81.

- Compatibilidad total con las rutinas originales de cinta y la ZX Printer.

# 2. Descripción del hardware

El SD81 Booster es una caja compacta que se conecta al puerto de expansión trasero del ZX81. En su exterior encontrarás todos los conectores y controles necesarios para aprovechar sus funciones.

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *Las imágenes de esta sección son renders provisionales del diseño 3D y serán sustituidas por fotografías del producto final antes del lanzamiento.* |

## 2.1 Panel lateral derecho

![](media/image2.png){width="5.0in" height="3.125in"}

*Vista lateral derecha: RESET, QSILVA, MICRO-SD y USB*

El panel lateral derecho presenta, de izquierda a derecha:

- Botón RESET: reinicia el sistema completo (ZX81 + interface).

- Botón QSILVA: alterna entre el juego de caracteres del interface QuickSilva y el juego de caracteres estándar de la ROM del ZX81. Cada pulsación conmuta entre uno y otro.

- Ranura MICRO-SD: inserta aquí la tarjeta microSD con los programas, ROMs y datos del sistema.

- Puerto USB-C: tiene dos usos. Como puerto de actualización de firmware en caso de recuperación via USB (ver sección 17). Como consola de depuración: al conectarlo al ordenador aparece un puerto serie asociado al chip CH340 (puede requerir drivers). Con un programa de terminal serie es posible monitorizar mensajes de estado y error del interface en tiempo real.

## 2.2 Panel lateral izquierdo y panel trasero

![](media/image3.png){width="5.0in" height="2.5520833333333335in"}

*Vista lateral izquierda: conector JOYSTICK DB9*

- Conector JOYSTICK (DB9): puerto de joystick compatible con joysticks estándar de 9 pines (tipo Atari/Commodore). El mapeo de botones a teclas del ZX81 es totalmente programable mediante el comando LOAD \*JOY (ver sección 9.6).

![](media/image4.png){width="5.0in" height="3.1354166666666665in"}

*Vista trasera: conector RGB SCART*

- Conector RGB (SCART): salida de vídeo RGB color compatible con el interface Chroma81. Permite conectar el ZX81 a monitores y televisores con entrada SCART para obtener imagen en color de alta calidad.

## 2.3 Panel superior --- LEDs de estado

![](media/image5.png){width="3.75in" height="3.59375in"}

*Vista superior: LEDs STAT y SD*

El panel superior incorpora dos indicadores luminosos:

- LED STAT (estado): indica el estado general del interface mediante diferentes colores y parpadeos (ver tabla en sección 2.4).

- LED SD (acceso a tarjeta): parpadea durante las operaciones de lectura o escritura en la tarjeta microSD.

## 2.4 Tabla de estados del LED STAT

El LED STAT indica el estado del interface mediante combinaciones de color y parpadeo, organizadas en tres fases:

<table style="width:100%;">
<colgroup>
<col style="width: 36%" />
<col style="width: 63%" />
</colgroup>
<thead>
<tr>
<th style="text-align: center;"><strong>Color / Patrón</strong></th>
<th style="text-align: center;"><strong>Significado</strong></th>
</tr>
</thead>
<tbody>
<tr>
<td colspan="2" style="text-align: center;"><strong>Actualización de firmware</strong></td>
</tr>
<tr>
<td style="text-align: center;"><strong>Parpadeo Azul/Rojo</strong></td>
<td style="text-align: center;">Error inicializando tarjeta SD</td>
</tr>
<tr>
<td style="text-align: center;"><strong>Amarillo fijo</strong></td>
<td style="text-align: center;">Actualizando firmware</td>
</tr>
<tr>
<td style="text-align: center;"><strong>Parpadeo Blanco/Rojo</strong></td>
<td style="text-align: center;">Error de actualización</td>
</tr>
<tr>
<td colspan="2" style="text-align: center;"><strong>Arranque</strong></td>
</tr>
<tr>
<td style="text-align: center;"><strong>Rojo parpadeante</strong></td>
<td style="text-align: center;">Inicializando puerto serie</td>
</tr>
<tr>
<td style="text-align: center;"><strong>Naranja parpadeante</strong></td>
<td style="text-align: center;">Esperando a la FPGA</td>
</tr>
<tr>
<td style="text-align: center;"><strong>Parpadeo Azul/Rojo</strong></td>
<td style="text-align: center;">Error inicializando tarjeta SD</td>
</tr>
<tr>
<td style="text-align: center;"><strong>Parpadeo Naranja/Rojo</strong></td>
<td style="text-align: center;">Error escribiendo la ROM en RAM</td>
</tr>
<tr>
<td style="text-align: center;"><strong>Parpadeo Amarillo</strong></td>
<td style="text-align: center;">Inicializando RTC</td>
</tr>
<tr>
<td style="text-align: center;"><strong>Verde fijo</strong></td>
<td style="text-align: center;">Sistema inicializado correctamente</td>
</tr>
<tr>
<td colspan="2" style="text-align: center;"><strong>Funcionamiento</strong></td>
</tr>
<tr>
<td style="text-align: center;"><strong>Verde fijo</strong></td>
<td style="text-align: center;">Interface listo y modo Quick Silva apagado</td>
</tr>
<tr>
<td style="text-align: center;"><strong>Cyan fijo</strong></td>
<td style="text-align: center;">Interface listo y modo Quick Silva encendido</td>
</tr>
</tbody>
</table>

# 3. Contenido de la caja

Al abrir la caja del SD81 Booster encontrarás:

- 1× Interface SD81 Booster

- 1× Tarjeta microSD (preformateada)

- 1× Pila botón CR2032 (preinstalada en el interface)

- Este manual de usuario

|  |  |
|:----:|------------------------------------------------------------------|
| **⚠** | *Si alguno de estos elementos falta o está dañado, contacta con el vendedor antes de conectar el interface.* |

# 4. Instalación

### 4.1 Antes de conectar el interface

- Asegúrate de que el ZX81 está apagado antes de conectar o desconectar el interface.

- El SD81 Booster se conecta al puerto de expansión trasero del ZX81.

- El interface no es compatible con dispositivos que reemplacen la ROM interna del ZX81.

### 4.2 Conexión

1.  Apaga el ZX81.

2.  Alinea el conector del SD81 Booster con el puerto de expansión trasero del ZX81. Asegúrate de que los pines están correctamente alineados.

3.  Empuja suavemente hasta que el conector quede completamente insertado. No fuerces la conexión.

4.  Inserta la tarjeta microSD en la ranura del interface (ver sección 4).

5.  Enciende el ZX81.

|  |  |
|:----:|------------------------------------------------------------------|
| **⚠** | *Conectar o desconectar el interface con el ZX81 encendido puede dañar tanto el interface como el ordenador.* |

### 4.3 Comprobación de la instalación

Al encender el ZX81 con el SD81 Booster correctamente instalado, el ordenador debe arrancar con normalidad mostrando el cursor K habitual. El interface no modifica el arranque del sistema.

Para verificar que el interface está funcionando, escribe el siguiente comando en BASIC y pulsa ENTER:

> LOAD \*VER

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *El asterisco \* se obtiene pulsando SHIFT + B. El interface mostrará la versión del firmware instalado.* |

Si el ordenador se queda bloqueado o no arranca correctamente, desconecta el interface y asegúrate de que el conector está bien alineado.

# 5. Preparación de la tarjeta microSD

### 5.1 Formato de la tarjeta

El SD81 Booster requiere una tarjeta microSD formateada en FAT32. La tarjeta incluida en la caja ya viene formateada y preparada correctamente.

Si utilizas una tarjeta propia, sigue estos pasos:

6.  Formatea la tarjeta en FAT32: en Windows, botón derecho sobre la tarjeta → Formatear → FAT32. En macOS o Linux, utiliza una herramienta de formato de disco y selecciona FAT32.

7.  Copia la carpeta SYS y todo su contenido (incluida en el paquete de software del interface) en la raíz de la tarjeta SD. Esta carpeta contiene archivos imprescindibles para el funcionamiento del interface.

|  |  |
|:----:|------------------------------------------------------------------|
| **⚠** | *El interface no soporta los formatos exFAT ni NTFS. La carpeta SYS es imprescindible: contiene los archivos de ROM necesarios para el arranque del interface. Sin ella, el interface no funcionará.* |

### 5.2 Caracteres permitidos en nombres de archivo

Debido a las limitaciones del teclado del ZX81, solo se pueden usar los siguientes caracteres en los nombres de archivo y carpeta:

- Letras: A a Z (siempre en mayúsculas al guardar)

- Números: 0 a 9

- Símbolos: . , ; \$ ( ) = + -

- El carácter / se utiliza como separador de directorios y no puede usarse en nombres de archivo.

|  |  |
|:----:|------------------------------------------------------------------|
| **💡** | *Evita terminar un nombre de archivo con un espacio o un punto, ya que algunos sistemas operativos podrían tener problemas para leer ese archivo.* |

### 5.3 Estructura de carpetas recomendada

El interface buscará en la raíz de la tarjeta SD por defecto. Puedes organizar tus programas en subcarpetas. Se recomienda la siguiente estructura:

> /
>
> ├── AUTOEXEC.P ← Programa que se carga al arrancar
>
> ├── JUEGOS/
>
> │ ├── MANIC.P
>
> │ └── PACMAN.P
>
> ├── DEMOS/
>
> └── SYS/ ← Carpeta del sistema (obligatoria, no modificar)

### 5.4 El programa AUTOEXEC

Si existe un archivo llamado AUTOEXEC.P en la raíz de la SD, este se cargará y ejecutará automáticamente al escribir el comando RUN en un ZX81 sin ningún programa cargado. Es útil para crear menús de inicio personalizados.

# 6. Primeros pasos

### 6.1 Modos de carga: SD o cinta

Por defecto, los comandos LOAD y SAVE del BASIC funcionan con la cinta, exactamente igual que en un ZX81 sin interface. Para utilizar la tarjeta SD tienes dos opciones:

**Opción A --- Activar el modo SD de forma permanente:** Escribe en BASIC:

> LOAD FAST

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *La palabra FAST se obtiene con SHIFT + F, no escribiendo las letras una a una. A partir de ese momento todos los LOAD y SAVE usarán la SD por defecto. Si necesitas cargar desde cinta estando en modo SD, usa LOAD SLOW \"nombre\" para esa operación concreta.* |

Para volver al modo cinta por defecto:

> LOAD SLOW

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *SLOW se obtiene con SHIFT + D. A partir de ese momento todos los LOAD y SAVE vuelven a usar la cinta, y para cargar desde SD habrá que añadir FAST explícitamente.* |

**Opción B --- Cargar desde SD sin cambiar el modo:** Añade FAST directamente al comando de carga (ver sección 6).

### 6.2 Cargando tu primer programa desde la SD

8.  Asegúrate de que tienes al menos un archivo .P en la tarjeta SD.

9.  Enciende el ZX81 con el interface y la SD insertados.

10. En el cursor K, escribe el siguiente comando, sustituyendo NOMBRE por el nombre del archivo sin extensión:

> LOAD FAST \"NOMBRE\"

11. Pulsa ENTER. El programa se cargará en unos instantes y arrancará automáticamente si tiene autoarranque.

|  |  |
|:----:|------------------------------------------------------------------|
| **💡** | *Si no recuerdas el nombre exacto del archivo, puedes ver el contenido de la SD con el comando LOAD \*DIR (ver sección 7).* |

# 7. Carga y guardado desde la SD

### 7.1 Cargar un programa

Para cargar un archivo desde la SD:

> LOAD FAST \"NOMBRE\"

El interface buscará primero el archivo con el nombre exacto. Si no lo encuentra, intentará añadir la extensión .P automáticamente.

Cargar desde una subcarpeta:

> LOAD FAST \"JUEGOS/PACMAN\"

Cargar y ejecutar desde una línea específica:

> LOAD FAST \"NOMBRE\" THEN GOTO 100

Cargar sin ejecutarlo automáticamente:

> LOAD FAST \"NOMBRE\" THEN STOP

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *THEN, GOTO y STOP son tokens del BASIC del ZX81, no se escriben letra a letra.* |

### 7.2 Guardar un programa

Para guardar el programa actual en la SD:

> SAVE FAST \"NOMBRE\"

El interface añadirá automáticamente la extensión .P al archivo guardado.

|  |  |
|:----:|------------------------------------------------------------------|
| **⚠** | *Si ya existe un archivo con ese nombre, será sobreescrito sin aviso previo.* |

### 7.3 Cargar y guardar bloques de memoria (código máquina)

Para cargar un bloque de datos en una dirección de memoria específica:

> LOAD FAST \"NOMBRE\" CODE 30000

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *A diferencia del ZX Spectrum, la dirección después de CODE es obligatoria. Los archivos en SD no almacenan la dirección de carga en una cabecera.* |

|  |  |
|:----:|------------------------------------------------------------------|
| **⚠** | *El token FAST es obligatorio para que CODE funcione correctamente. Si se usa LOAD \"NOMBRE\" CODE 30000 sin FAST (incluso con el modo SD activo), el archivo se cargará en la dirección por defecto 16393 (4009h), ignorando la dirección especificada. Esto es una limitación de compatibilidad del firmware.* |

Para guardar un bloque de memoria en la SD:

> SAVE FAST \"NOMBRE\" CODE 30000,2048

Donde 30000 es la dirección de inicio y 2048 es la longitud en bytes.

### 7.4 Cargar siempre desde cinta (independientemente del modo)

Si el modo SD está activo pero quieres cargar un archivo concreto desde cinta:

> LOAD SLOW \"NOMBRE\"

### 7.5 Notas sobre compatibilidad de juegos

Algunos juegos necesitan una inicialización especial antes de ser cargados. Los casos más comunes son:

- **LOAD \*128C** antes de cargar: el juego utiliza el modo de 128 caracteres definibles por el usuario.

- **LOAD FAST** antes de cargar: algunos juegos tienen múltiples archivos. Ejecutar LOAD FAST sin nombre activa el modo SD para todos los LOAD y SAVE a partir de ese momento, permitiendo que el juego cargue sus archivos adicionales automáticamente.

### 7.6 Formatos de archivo reconocidos

El comando LOAD FAST detecta automáticamente el tipo de archivo por su extensión y actúa de forma diferente según el caso:

| **Extensión** | **Comportamiento** |
|-----------|-------------------------------------------------------------|
| **.P** | Programa BASIC estándar del ZX81. El interface calcula el tamaño real del programa desde las variables del sistema y descarta los bytes sobrantes al final del archivo. |
| **.81** | Igual que .P. |
| **.P81** | Formato multi-programa. El interface salta el nombre del fichero embebido antes de leer los datos. |
| **.ROM** | Archivo de ROM. El interface carga el contenido en la dirección 0 del espacio de direccionamiento y resetea el sistema. El control no vuelve al BASIC. |
| **.WAV** | Archivo de audio sin comprimir (PCM). El interface lo reproduce directamente en lugar de cargarlo en memoria. |
| **Otras** | El archivo se carga íntegramente en memoria tal cual, sin ningún procesado. |

|  |  |
|:----:|------------------------------------------------------------------|
| **⚠** | *Cargar un archivo con extensión .ROM mediante LOAD FAST provoca un reset inmediato del sistema. Asegúrate de que el archivo contiene una ROM válida antes de cargarlo, ya que un archivo corrupto podría dejar el sistema en un estado irrecuperable hasta que se reinicie con otra ROM.* |

# 8. Gestión de archivos y directorios

### 8.1 Ver el contenido de la SD

Para listar los archivos del directorio actual:

> LOAD \*DIR

Para listar los archivos de una carpeta concreta:

> LOAD \*DIR \"JUEGOS\"

Se pueden usar comodines al estilo Unix:

- \* representa cualquier número de caracteres (incluido ninguno).

- ? representa exactamente un carácter.

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *\* casa con todos los archivos independientemente de si tienen extensión o no. En cambio, \*.\* solo casa con archivos que contienen un punto en el nombre. Para listar todos los archivos, usa \*, no \*.\*.* |

Por ejemplo, para listar solo los archivos .P:

> LOAD \*DIR \"\*.P\"

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *Si el listado no cabe en pantalla, aparecerá \... en la línea inferior. Pulsa cualquier tecla para continuar, o SPACE para cancelar.* |

### 8.2 Cambiar de directorio

Cambiar al directorio JUEGOS:

> LOAD \*CD \"JUEGOS\"

Volver al directorio raíz:

> LOAD \*CD \"/\"

Ver el directorio actual:

> LOAD \*PWD

### 8.3 Crear y eliminar carpetas

Crear una carpeta nueva:

> LOAD \*MD \"NUEVACARPETA\"

Eliminar una carpeta (debe estar vacía):

> LOAD \*RD \"CARPETAVACIA\"

### 8.4 Borrar, renombrar y copiar archivos

Borrar un archivo:

> LOAD \*DEL \"ARCHIVO.P\"

Renombrar o mover un archivo:

> LOAD \*MV \"VIEJO.P\" TO \"NUEVO.P\"

Copiar un archivo:

> LOAD \*CP \"ORIGEN.P\" TO \"DESTINO.P\"

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *TO es un token del BASIC (SHIFT + 4), no se escribe letra a letra.* |
| **ℹ** | *La fecha y hora del archivo de destino no se preserva; el archivo copiado tendrá la fecha y hora del momento de la copia.* |

### 8.5 Espacio libre en la SD

> LOAD \*FREE

|       |                                                             |
|:-----:|-------------------------------------------------------------|
| **ℹ** | *Este cálculo puede tardar varios segundos en completarse.* |

### 8.6 Directorios T81 (función en fase alfa)

|  |  |
|:----:|------------------------------------------------------------------|
| **⚠** | *Esta funcionalidad está actualmente en fase alfa. Puede contener errores y su comportamiento o interfaz podría cambiar en versiones futuras. No se recomienda su uso en entornos de producción.* |

El SD81 Booster soporta un formato de archivo especial con extensión .T81 que actúa como un contenedor de múltiples programas ZX81, de forma similar a como un archivo ZIP contiene varios archivos. Esto permite distribuir colecciones de programas en un único archivo.

Para acceder al contenido de un archivo T81, se usa el comando LOAD \*CD como si fuera un directorio normal:

> LOAD \*CD \"COLECCION.T81\"

A partir de ese momento, los comandos LOAD, LOAD \*DIR y LOAD \*OPENDIR operan sobre el contenido del archivo T81 en lugar del sistema de archivos FAT, permitiendo navegar y cargar los programas que contiene de la misma manera que se haría con archivos normales.

Para salir del directorio T81 y volver al sistema de archivos normal:

> LOAD \*CD \"/\"

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *Limitaciones en fase alfa: Las operaciones de escritura (LOAD \*DEL, LOAD \*MD, LOAD \*RD, LOAD \*MV, LOAD \*CP, SAVE) no están soportadas dentro de un directorio T81 y devolverán un error.* |

# 9. Funciones adicionales

## 9.1 Reproducción de archivos WAV

El interface puede reproducir archivos de audio en formato WAV sin comprimir directamente desde la tarjeta SD, simplemente cargándolos con el comando habitual:

> LOAD FAST \"SONIDO.WAV\"

Si el archivo tiene extensión .WAV, el interface lo detecta automáticamente y lo reproduce en lugar de intentar cargarlo como programa. No es necesario ningún comando especial.

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *Solo se admiten archivos WAV sin comprimir (PCM). Otros formatos de audio no son compatibles.* |

## 9.2 Reloj en tiempo real --- Comando RTC

El SD81 Booster incorpora un reloj en tiempo real (RTC) con batería de reserva. El comando LOAD \*RTC permite consultar y ajustar la hora y la fecha desde BASIC.

**Mostrar la hora y fecha actuales en pantalla:**

> LOAD \*RTC

**Guardar la hora y fecha en una variable de cadena:**

> LOAD \*RTC TO R\$

**Poner el reloj en hora:**

> LOAD \*RTC=\"cadena\"

El comando acepta varios formatos de cadena. Puedes ajustar fecha y hora a la vez, o solo uno de los dos:

| **Formato** | **Ejemplo** | **Descripción** |
|----------------------|----------------------|----------------------------|
| AAAA-MM-DD HH:MM:SS.CC | 2025-04-30 18:30:00.00 | Fecha y hora completas con centésimas |
| AAAA-MM-DD HH:MM:SS | 2025-04-30 18:30:00 | Fecha y hora completas |
| AAAA-MM-DD | 2025-04-30 | Solo fecha |
| HH:MM:SS.CC | 18:30:00.00 | Solo hora con centésimas |
| HH:MM:SS | 18:30:00 | Solo hora |
| HH:MM | 18:30 | Hora y minutos (segundos a cero) |

**Ejemplos:**

> LOAD \*RTC=\"2025-04-30 18:30:00\"
>
> LOAD \*RTC=\"2025-04-30\"
>
> LOAD \*RTC=\"18:30:00\"

**Ejemplo en un programa BASIC:**

> 10 LOAD \*RTC TO R\$
>
> 20 PRINT \"Fecha y hora: \";R\$

## 9.3 Estado de la batería del RTC --- Comando BAT

El SD81 Booster incorpora una pila botón CR2032 que mantiene el reloj en hora cuando el ZX81 está apagado. Esta pila viene preinstalada de fábrica y tiene una vida útil estimada de varios años en condiciones normales de uso. Cuando se agote, puede sustituirse por cualquier pila CR2032 estándar disponible en comercios de electrónica.

El estado de carga de la pila puede consultarse desde BASIC con el comando LOAD \*BAT.

**Mostrar el estado de la batería en pantalla:**

> LOAD \*BAT

**Guardar el estado en una variable de cadena:**

> LOAD \*BAT TO B\$

|  |  |
|:----:|------------------------------------------------------------------|
| **💡** | *Si el reloj pierde la hora frecuentemente al apagar el ordenador, consulta el estado de la batería con este comando para saber si necesita ser reemplazada.* |

## 9.4 Modo RAM extendida --- Comando RAM48

El comando LOAD \*RAM48 activa el modo de RAM extendida de 48 KB, que amplía la memoria disponible para programas BASIC y datos más allá de los límites habituales.

**Activar el modo RAM extendida:**

> LOAD \*RAM48

**Desactivarlo para restaurar la compatibilidad estándar:**

> LOAD \*RAM48 STOP

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *STOP es el token del BASIC, no se escribe letra a letra. Si algún programa presenta problemas de compatibilidad con el modo RAM48 activo, desactívalo con LOAD \*RAM48 STOP antes de cargarlo.* |

## 9.5 Visualización de archivos de texto --- Comando THEN PRINT

El comando LOAD THEN PRINT es el equivalente al comando TYPE de MS-DOS o cat de Linux: muestra el contenido de un archivo de texto directamente en la pantalla del ZX81.

**Mostrar un archivo de texto en pantalla:**

> LOAD THEN PRINT \"FICHERO\"

**Enviar el contenido a la impresora ZX Printer:**

> LOAD THEN LPRINT \"FICHERO\"

**Redirigir la salida de otros comandos a la ZX Printer:**

El prefijo LOAD LPRINT puede usarse también con los comandos que normalmente muestran texto en pantalla, para redirigir su salida directamente a la ZX Printer:

> LOAD LPRINT DIR
>
> LOAD LPRINT DIR \"\*.P\"
>
> LOAD LPRINT FREE
>
> LOAD LPRINT PWD
>
> LOAD LPRINT VER

### Sistema de ayuda integrado

Si se antepone un asterisco \* al nombre del fichero, el interface busca automáticamente un archivo con ese nombre en la carpeta /MAN/ de la SD y le añade la extensión .TXT. Esto permite implementar un sistema de ayuda al estilo del comando man de Linux:

> LOAD THEN PRINT \"\*PLAY\"

Este comando buscaría el archivo /MAN/PLAY.TXT en la SD y mostraría su contenido en pantalla. Puedes crear tus propios archivos de ayuda en esa carpeta para documentar tus programas o comandos.

**Ejemplo --- mostrar instrucciones de un juego:**

> 10 LOAD THEN PRINT \"\*INSTRUCCIONES\"

Esto mostraría el contenido de /MAN/INSTRUCCIONES.TXT, ideal para mostrar las instrucciones de un juego o programa desde dentro del propio programa BASIC.

## 9.6 Joystick programable --- Comando JOY

El SD81 Booster incorpora un puerto de joystick DB9 cuyo mapeo de botones es totalmente configurable. El comando LOAD \*JOY permite asignar una tecla del ZX81 a cada dirección y al botón de fuego del joystick.

**Sintaxis:**

> LOAD \*JOY \"arr/aba/izq/der/fue\"

La cadena de configuración contiene exactamente cinco caracteres, uno por cada función del joystick en este orden: arriba / abajo / izquierda / derecha / fuego.

**Ejemplo:**

> LOAD \*JOY \"QAOP \"

- Arriba → tecla Q

- Abajo → tecla A

- Izquierda → tecla O

- Derecha → tecla P

- Fuego → tecla espacio

|  |  |
|:----:|------------------------------------------------------------------|
| **💡** | *Consulta los controles de cada juego antes de configurar el joystick. Muchos juegos del ZX81 usan combinaciones de teclas diferentes, y con este comando puedes adaptarlas a cualquier joystick estándar de 9 pines sin modificar el software.* |

## 9.7 Módulo WiFi (opcional)

El módulo WiFi es un accesorio opcional basado en un microcontrolador ESP32-C3 que se conecta al SD81 Booster y añade un servidor de archivos accesible por WiFi: permite listar, subir, descargar, borrar y organizar el contenido de la tarjeta microSD desde el navegador de un móvil, tablet u ordenador, sin sacar la tarjeta del interface.

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *El módulo WiFi no se incluye de fábrica en el SD81 Booster --- es un accesorio opcional que se instala aparte. Si tu interface no lo lleva instalado, esta sección no aplica.* |

**Configuración de la red WiFi:**

Para que el módulo se conecte a tu red, crea un archivo de texto llamado WIFI.CFG dentro de la carpeta SYS de la tarjeta microSD, con el nombre de la red en una línea y la contraseña en la siguiente:

> MiRedWiFi
>
> MiContraseña

Puedes añadir más de una red repitiendo el mismo patrón (nombre, luego contraseña), una tras otra en el mismo archivo:

> CasaWifi
>
> passwordcasa
>
> TrabajoWifi
>
> passwordtrabajo

El módulo prueba las redes en el orden en que aparecen, hasta conectar con la primera disponible --- útil para no tener que editar el archivo cada vez que te mueves de sitio (hasta 4 redes).

|  |  |
|:----:|------------------------------------------------------------------|
| **💡** | *Guarda el archivo como texto plano, sin formato. Las redes deben ser de 2.4GHz --- el módulo WiFi no es compatible con redes de 5GHz.* |

**Acceso al servidor de archivos:**

Con el módulo conectado a tu red, necesitas su dirección para acceder desde un navegador. Tienes varias formas de consultarla, de más a menos cómoda:

**Desde el propio ZX81 (más sencillo, no necesita ningún otro dispositivo):**

> LOAD THEN PRINT \"\*IP\"

Muestra en pantalla la dirección IP actual del módulo.

**Por nombre, sin teclear la IP:**

Si tu dispositivo soporta mDNS (de serie en macOS, iOS, Android y Linux), accede directamente a http://sd81booster.local

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *En Windows, si no tienes instalado nada de Apple, es posible que el navegador no reconozca direcciones \".local\". No es necesario para nada más --- puedes usar LOAD THEN PRINT \"\*IP\" o la IP directa sin problema.* |

**Alternativa: panel del router:**

También puedes consultar la dirección IP asignada en la lista de dispositivos conectados del panel de administración de tu router (búscalo como "esp32" o similar).

**Funciones disponibles:**

- Listar y navegar el contenido de la SD por carpetas.

- Subir archivos, incluidos varios a la vez o una carpeta completa (en navegadores de escritorio).

- Descargar cualquier archivo al dispositivo desde el que navegas.

- Borrar archivos o carpetas.

- Crear carpetas nuevas.

**Actualización del firmware del módulo WiFi:**

El módulo WiFi puede actualizar su propio firmware desde la SD, sin necesidad de conectarlo a un ordenador. Copia el archivo ESP32_FW.BIN (disponible en el repositorio del proyecto) en la raíz de la SD --- por ejemplo subiéndolo con el propio servidor de archivos --- y reinicia el interface.

Durante la actualización, el LED STAT parpadea en rosa. Si termina con éxito, pasa a verde fijo. Si el proceso falla, el LED se queda en amarillo fijo --- vuelve a intentarlo copiando de nuevo el archivo.

|  |  |
|:----:|------------------------------------------------------------------|
| **⚠** | *No apagues el interface ni extraigas la tarjeta SD mientras el LED STAT parpadea en rosa.* |

**Programación inicial / de recuperación (vía USB):**

El módulo necesita programarse por USB **la primera vez** (o si deja de responder y no se puede usar la actualización por SD de arriba). A partir de ahí, todas las actualizaciones posteriores pueden hacerse por tarjeta SD.

Requiere el **IDE de Arduino** con el soporte de placas ESP32 instalado, seleccionando la placa **ESP32C3 Dev Module**. Los ajustes exactos de la placa (velocidad de subida, tamaño de flash, etc.) están documentados en **FIRMWARE/README_update.md**, dentro del repositorio del proyecto.

# 10. Sonido

El SD81 Booster incorpora un emulador del chip de sonido AY-3-8910/12, el mismo que usaban ordenadores como el ZX Spectrum 128K o el Amstrad CPC. Esto permite reproducir música de hasta tres voces simultáneas, efectos de sonido programables y música en segundo plano, todo ello desde BASIC o desde código máquina.

## 10.1 Comando PLAY --- Música con el chip AY

El comando PLAY permite reproducir música directamente desde BASIC mediante una cadena de texto que describe las notas, la duración, el tempo y otros parámetros. Admite hasta tres voces simultáneas (canales A, B y C del AY).

**Sintaxis básica:**

> LOAD \*PLAY \"cadena1\"
>
> LOAD \*PLAY \"cadena1\",\"cadena2\",\"cadena3\"

Cada cadena corresponde a una voz. Pueden usarse de una a tres cadenas simultáneamente.

**Ejemplo sencillo --- melodía en una sola voz:**

> LOAD \*PLAY \"T120O45C5E5G9C\"

Esto toca las notas Do, Mi, Sol y Do (acorde de Do mayor) a 120 pulsaciones por minuto en la octava 4.

### Notas

Las notas se escriben con las letras C D E F G A B (Do Re Mi Fa Sol La Si). Se pueden modificar con:

- = antes de la nota: sube un semitono (sostenido). Ejemplo: =F es Fa sostenido.

- £ antes de la nota: baja un semitono (bemol). Ejemplo: £E es Mi bemol.

- Letra en vídeo normal: toca la nota en la octava actual.

- Letra en vídeo inverso (SHIFT + 9 sobre la letra): toca la misma nota en la octava siguiente, sin cambiar la octava activa. Es equivalente al uso de mayúsculas en el ZX Spectrum.

- £ sin nota a continuación: pausa o silencio.

### Duración

Un número del 1 al 12 antes de una nota establece su duración desde ese punto en adelante:

| **Valor** | **Nombre**  | **Duración a 60 bpm** |
|-----------|-------------|-----------------------|
| **1**     | Semicorchea | 0.25 s                |
| **3**     | Corchea     | 0.5 s                 |
| **5**     | Negra       | 1 s                   |
| **7**     | Blanca      | 2 s                   |
| **9**     | Redonda     | 4 s                   |

|  |  |
|:----:|------------------------------------------------------------------|
| **💡** | *Puedes ligar duraciones con el signo -. Por ejemplo, 4-3A combina corchea con puntillo (7/8 de negra).* |

### Tempo

T\<número\> establece el tempo en pulsaciones por minuto (bpm), entre 60 y 240. El valor por defecto es 120. Solo tiene efecto en la primera voz.

> LOAD \*PLAY \"T1805C5E5G\"

### Octava

O\<número\> selecciona la octava, de 0 (muy grave) a 8 (muy agudo). La octava por defecto es 4.

> LOAD \*PLAY \"O35CO45CO55C\"

### Repeticiones

- ( \... ) repite una sección una vez más.

- ) sin ( correspondiente repite la cadena entera indefinidamente.

- H detiene el comando PLAY aunque haya voces en bucle infinito.

> LOAD \*PLAY \"(5C5E5G)H\",\")\"

### Efectos de volumen (envolvente)

W\<número\> selecciona uno de los 8 efectos de volumen del chip AY (W0 a W7), como ataque, decaimiento o tremolo. U activa el efecto en el canal. X\<número\> ajusta la velocidad del efecto (0--65535; a mayor valor, más lento).

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *Para una descripción completa de todos los parámetros, incluyendo los efectos de envolvente y el modo de ruido, consulta el Apéndice A de este manual.* |

## 10.2 Reproductor VGM --- Música en segundo plano

El SD81 Booster puede reproducir archivos en formato VGM (Video Game Music) en segundo plano mientras el ZX81 ejecuta cualquier otro programa. Esto permite añadir música a tus propios programas BASIC sin consumir tiempo de CPU.

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *Solo se admiten archivos VGM que contengan datos del chip AY-3-8910 o AY-3-8912. Los VGMs con otros chips de sonido no son compatibles.* |

**Preparar un archivo VGM:**

> LOAD \*VGM \"MUSICA\"

Carga el archivo MUSICA.VGM de la SD y lo deja listo para reproducir, sin iniciarlo todavía.

**Iniciar la reproducción:**

> LOAD \*VGM THEN RUN

**Pausar y reanudar:**

> LOAD \*VGM THEN PAUSE
>
> LOAD \*VGM THEN CONT

**Detener la reproducción:**

> LOAD \*VGM THEN STOP

**Activar el modo bucle** (la música se reinicia al terminar):

> LOAD \*VGMLOOP

**Desactivar el modo bucle:**

> LOAD \*VGMLOOP STOP

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *THEN, RUN, CONT, PAUSE y STOP son tokens del BASIC del ZX81, no se escriben letra a letra.* |

**Ejemplo de uso típico en un programa BASIC:**

> 10 LOAD \*VGM \"MUSICA\"
>
> 20 LOAD \*VGMLOOP
>
> 30 LOAD \*VGM THEN RUN
>
> 40 REM \-\-- el programa continúa mientras suena la música \-\--

## 10.3 Generador de efectos PEG --- Efectos de sonido programables

El PEG (Programmable Effects Generator) es una pequeña máquina virtual integrada en el interface que ejecuta programas de efectos de sonido de forma completamente independiente al Z80, sin consumir tiempo de CPU del ZX81.

El PEG accede directamente a los registros del chip AY y dispone de hasta tres hilos de ejecución paralelos, lo que permite reproducir varios efectos simultáneamente.

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *El PEG está orientado principalmente a desarrolladores. Para crear programas PEG se recomienda usar el ensamblador PEG incluido en el repositorio del proyecto. Para una descripción completa del juego de instrucciones, consulta el Apéndice B de este manual.* |

**Cargar un programa PEG en memoria:**

> LOAD \*PEG \<dirección\>,\"\<hexadecimal\>\"

Donde \<dirección\> es la posición en la memoria PEG (0--255) y \<hexadecimal\> es la secuencia de instrucciones en formato hexadecimal. Cada instrucción ocupa 4 caracteres hexadecimales (2 bytes).

**Iniciar un hilo PEG:**

> LOAD \*PEG THEN RUN \<hilo\>,\<dirección\>

Donde \<hilo\> es el número de hilo (0, 1 o 2) y \<dirección\> es la dirección de inicio del programa PEG.

**Detener, pausar y reanudar un hilo:**

> LOAD \*PEG THEN STOP \<hilo\>
>
> LOAD \*PEG THEN PAUSE \<hilo\>
>
> LOAD \*PEG THEN CONT \<hilo\>

**Cargar un programa PEG desde la SD (LOAD \*PEB):**

Además de la carga inline con cadena hexadecimal, es posible cargar un programa PEG compilado directamente desde un archivo en la SD:

> LOAD \*PEB \<dirección\>,\"\<nombre\>\"

Donde \<dirección\> es la posición de inicio en la memoria PEG (0--255) y \<nombre\> es el nombre del archivo en la SD. El archivo debe tener extensión .PEB (PEG binary); si no se especifica extensión, el interface la añade automáticamente.

**Ejemplo:**

> LOAD \*PEB 0,\"EFECT\"
>
> LOAD \*PEG THEN RUN 0,0

Este ejemplo carga el archivo EFECT.PEB en la memoria PEG a partir de la posición 0 y arranca el hilo 0 desde esa misma dirección.

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *El archivo .PEB es el resultado de ensamblar un fuente .PEG con el ensamblador peg.py, un script Python incluido en la carpeta EXAMPLES/PEG/ del repositorio del proyecto. Para compilar: python peg.py efect.peg* |

## 10.4 Síntesis de voz --- Comando SAY

El SD81 Booster incorpora un sintetizador de voz que permite reproducir frases en inglés directamente desde BASIC. El sintetizador se basa en los fonemas del chip SP0256, un sintetizador de voz ampliamente utilizado en la época que formaba parte de interfaces clásicos como el Currah MicroSpeech para el ZX Spectrum o The Voice para el Videopac G7000/Odyssey 2. Las muestras de audio de los fonemas están almacenadas en la memoria interna del microcontrolador, por lo que no se necesita ningún archivo adicional en la SD.

**Sintaxis:**

> LOAD \*SAY \"frase\"

El sintetizador analiza la cadena de texto e intenta construir la pronunciación combinando fonemas del inglés. El texto debe escribirse en inglés, en mayúsculas.

**Ejemplos:**

> LOAD \*SAY \"HELLO WORLD\"
>
> LOAD \*SAY \"ZX81 COMPUTER\"
>
> LOAD \*SAY \"ERROR 5\"

Los números se leen en inglés automáticamente, desde cero hasta los billones.

### Reproducción en background

Por defecto, la CPU espera a que la voz termine antes de continuar ejecutando el programa. Si se antepone un \* al inicio de la cadena, la reproducción continúa en segundo plano y el ZX81 sigue ejecutando el programa inmediatamente:

> LOAD \*SAY \"\*HELLO WORLD\"

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *No es posible añadir sonidos o fonemas personalizados. El conjunto de fonemas disponibles está fijo en la memoria interna del microcontrolador.* |

### Cómo funciona el sintetizador

El sintetizador descompone el texto de izquierda a derecha, buscando siempre la coincidencia más larga posible en su diccionario interno. Las palabras reconocidas como tales suenan mejor que las que el sintetizador debe construir sílaba a sílaba.

Los espacios y signos de puntuación producen pausas de duración creciente: el espacio produce una pausa corta, la coma una pausa media, y el punto y coma o los dos puntos una pausa larga.

|  |  |
|:----:|------------------------------------------------------------------|
| **💡** | *Para mejorar la pronunciación de palabras no reconocidas, escríbelas fonéticamente en inglés. Por ejemplo, SINCLAIR puede sonar mejor como SINCLER. Consulta el Apéndice C para ver el diccionario completo de palabras y fonemas reconocidos.* |

# 11. Gestión de memoria

El SD81 Booster incorpora hasta 512 KB de RAM, muy por encima de los 1 KB originales del ZX81 o de las expansiones de memoria convencionales. Esta memoria se gestiona mediante un mapeador de memoria que divide tanto la RAM como el espacio de direcciones del Z80 en bloques de 8 KB, permitiendo asignar cualquier página de memoria a cualquier bloque del mapa de direcciones.

Para la mayoría de los programas BASIC no es necesario gestionar la memoria manualmente: el interface la configura automáticamente al arrancar y el BASIC del ZX81 puede usar la RAM disponible de forma transparente.

## 11.1 Conceptos básicos: bloques y páginas

El espacio de direcciones del Z80 (64 KB) se divide en 8 bloques de 8 KB cada uno:

| **Bloque** | **Rango de direcciones** | **Uso habitual** |
|----------|-----------------------|----------------------------------------|
| **0** | 0000--1FFF | ROM del ZX81 (solo lectura) |
| **1** | 2000--3FFF | ROM de expansión del interface |
| **2** | 4000--5FFF | RAM principal / pantalla |
| **3** | 6000--7FFF | RAM principal |
| **4** | 8000--9FFF | RAM ampliada |
| **5** | A000--BFFF | RAM ampliada |
| **6** | C000--DFFF | Espejo de bloque 2 (necesario para el vídeo) |
| **7** | E000--FFFF | Espejo de bloque 3 (necesario para el vídeo) |

Los 512 KB de RAM se dividen en 64 páginas de 8 KB. Cada bloque puede apuntar a cualquiera de estas 64 páginas, lo que permite acceder a toda la memoria simplemente cambiando qué página está asignada a qué bloque.

|  |  |
|:----:|------------------------------------------------------------------|
| **⚠** | *Los bloques 6 y 7 deben mantenerse como espejo de los bloques 2 y 3 respectivamente para que el sistema de vídeo del ZX81 funcione correctamente. Modificarlos sin tener esto en cuenta puede causar que la pantalla deje de funcionar.* |

## 11.2 Comando MAP --- Asignar páginas a bloques

Para asignar una página de memoria a un bloque determinado:

> LOAD \*MAP \<bloque\>,\<página\>

Donde \<bloque\> es un número del 0 al 7 y \<página\> es un número del 0 al 63.

Para leer qué página está asignada actualmente a un bloque:

> LOAD \*MAP \<bloque\> TO \<variable\>

**Ejemplo --- cambiar el bloque 4 a la página 10:**

> LOAD \*MAP 4,10

A partir de ese momento, cualquier lectura o escritura en las direcciones 32768--40959 accederá a la página 10 de la RAM.

**Ejemplo --- leer la página asignada a un bloque:**

> 10 LOAD \*MAP 4 TO A
>
> 20 PRINT \"Bloque 4 apunta a la pagina \";A

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *Para una referencia completa del sistema de paginación, incluyendo el modo de paginación completa de 512 KB y el uso desde código máquina, consulta el Apéndice D de este manual.* |

## 11.3 Modo MC45 --- Código máquina en bloques 4 y 5

Por diseño del hardware del ZX81, las instrucciones de código máquina situadas en los bloques 4 y 5 (direcciones 32768--49151) son ejecutadas de forma incorrecta: los opcodes en los rangos 00h--3Fh y 80h--BFh son interpretados como NOP, lo que hace imposible ejecutar código normal en esa zona.

El modo MC45 (Machine Code 4 and 5) desactiva esta limitación, permitiendo ejecutar cualquier instrucción Z80 en esa área de memoria.

**Activar el modo MC45:**

> LOAD \*MC45

**Desactivarlo:**

> LOAD \*MC45 STOP

|  |  |
|:----:|------------------------------------------------------------------|
| **⚠** | *El modo MC45 se implementa forzando a cero el pin M1 del Z80 de forma intermitente y durante intervalos de tiempo muy breves, lo que mantiene la carga sobre dicho pin en niveles bajos y hace que la posibilidad de daño sea muy reducida. Además, el interface ya incorpora internamente la resistencia de protección necesaria, por lo que no es necesario realizar ninguna modificación en el ZX81. Pese a todo, el uso de esta característica se realiza bajo la responsabilidad exclusiva del usuario. Cuando MC45 está activo no es posible cargar ni escribir programas BASIC de más de 16 KB.* |

## 11.4 Arranque con ROM alternativa

El SD81 Booster permite arrancar con una ROM diferente a la estándar del ZX81 sin necesidad de usar ningún comando. Basta con tener en la carpeta /SYS/ de la tarjeta SD uno o más archivos de ROM con los nombres 0.ROM, 1.ROM, 2.ROM\... hasta 9.ROM.

**Para arrancar con una ROM alternativa:**

12. Mantén pulsado el número correspondiente al archivo de ROM deseado mientras enciendes el ZX81.

13. Suelta la tecla cuando el ordenador haya arrancado.

|  |  |
|:----:|------------------------------------------------------------------|
| **💡** | *Esta función es muy útil para probar ROMs alternativas o modificadas sin necesidad de reprogramar ningún chip. La ROM estándar del ZX81 se carga siempre si no se pulsa ninguna tecla durante el arranque.* |

## 11.5 Caracteres definibles por el usuario (128C / 64C)

Por defecto el ZX81 dispone de 64 caracteres definidos por la ROM. El SD81 Booster permite ampliar este conjunto a 128 caracteres, todos ellos completamente redefinibles, escribiendo en la zona de memoria entre las direcciones 15360 y 16383 (3C00h--3FFFh).

**Activar el modo de 128 caracteres:**

> LOAD \*128C

**Volver al modo estándar de 64 caracteres:**

> LOAD \*64C

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *En el modo de 128 caracteres, los 64 caracteres superiores se muestran automáticamente en vídeo inverso por el hardware. Para mostrarlos en vídeo normal debes almacenar el gráfico invertido en esa posición de memoria.* |

Si necesitas restaurar el juego de caracteres original después de haberlo modificado:

> LOAD \*LDIR 7680,15360,512
>
> LOAD \*LDIR 7680,15872,512

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *La activación del modo de 128 caracteres es incompatible con el generador interno de caracteres HRG (ver sección 10.6). Ambos modos no pueden estar activos simultáneamente.* |

### Ejemplo: pantalla de inicio estilo Spectrum

El siguiente programa ilustra el uso del modo de 128 caracteres para cargar un juego de caracteres alternativo desde la SD y mostrar una pantalla al estilo del ZX Spectrum:

> 5 LOAD \*128C
>
> 20 LOAD FAST \"SPEC-81-128.BIN\" CODE 15360
>
> 30 CLS
>
> 35 POKE 16418,0
>
> 40 PRINT AT 23,1;CHR\$ 8;\" 1982 S\[INCLAIR\] R\[ESEARCH\] L\[TD\].\"
>
> 45 POKE 16418,2
>
> 50 IF INKEY\$=\"\" THEN GOTO 50

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *Los textos entre corchetes indican vídeo inverso: S\[INCLAIR\] significa que la S es normal e INCLAIR va en vídeo inverso; R\[ESEARCH\] que la R es normal y ESEARCH en vídeo inverso; L\[TD\] que la L es normal y TD en vídeo inverso. Para introducir caracteres en vídeo inverso pulsa SHIFT + 9 antes de cada carácter. CHR\$ 8 corresponde al carácter gráfico de cuadrícula del ZX81 (código 8, símbolo © en el juego de caracteres del Spectrum).* |

**Explicación línea a línea:**

- Línea 5: activa el modo de 128 caracteres definibles.

- Línea 20: carga el archivo SPEC-81-128.BIN desde la SD en la dirección 15360 (3C00h), zona de caracteres definibles. Este archivo contiene el juego de caracteres del ZX Spectrum.

- Línea 30: limpia la pantalla.

- del vídeo.

- Línea 40: imprime en la línea 23 el mensaje de copyright del Spectrum, usando CHR\$ 8 como símbolo © y las palabras de la firma en vídeo inverso.

- Línea 50: espera indefinidamente a que se pulse cualquier tecla.

# 12. Extensiones BASIC avanzadas

Esta sección recoge comandos adicionales del BASIC extendido del SD81 Booster orientados principalmente a la programación: manipulación de cadenas, acceso a memoria, comunicación con puertos de entrada/salida y acceso al directorio desde un programa.

## 12.1 Manipulación de cadenas

**Invertir caracteres de una cadena (\*INV):**

Invierte el bit 7 de todos los caracteres de una variable de cadena, convirtiendo los caracteres normales en inversos y viceversa:

> LOAD \*INV A\$

También admite porciones de cadena:

> LOAD \*INV A\$(2 TO 7)

**Forzar vídeo inverso en una cadena (\*BOLD):**

Fuerza el bit 7 de todos los caracteres de una variable de cadena a 1, poniendo todos los caracteres en vídeo inverso:

> LOAD \*BOLD A\$

|  |  |
|:----:|------------------------------------------------------------------|
| **💡** | *Para poner caracteres en vídeo normal a partir de una cadena en inverso, aplica primero \*BOLD y luego \*INV para invertir el resultado.* |

## 12.2 Copia y relleno de bloques de memoria

**Copiar un bloque de memoria en orden ascendente (\*LDIR):**

> LOAD \*LDIR \<origen\>,\<destino\>,\<longitud\>

Copia \<longitud\> bytes desde \<origen\> hasta \<destino\> en orden ascendente. Equivale a la instrucción Z80 LDIR.

**Copiar en orden descendente (\*LDDR):**

> LOAD \*LDDR \<origen\>,\<destino\>,\<longitud\>

Igual que \*LDIR pero en orden descendente. Equivale a la instrucción Z80 LDDR.

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *Cuando origen y destino se solapan, el orden de copia importa: usa \*LDIR si el destino está antes del origen en memoria, y \*LDDR si está después, para evitar que los datos se sobreescriban durante la copia.* |

**Cargar datos hexadecimales en memoria (\*HEX):**

> LOAD \*HEX \<dirección\>,\"\<hexadecimal\>\"

Por ejemplo, LOAD \*HEX 30000,\"0A014020\" carga los valores 10 (0Ah), 1, 64 (40h) y 32 (20h) a partir de la dirección 30000.

## 12.3 Ejecución de código máquina

**Ejecutar una rutina en código máquina (LOAD USR):**

> LOAD USR \<dirección\>

Equivale a RAND USR pero con tres ventajas importantes:

- No modifica el generador de números aleatorios.

- La rutina se llama desde el nivel superior del BASIC, dejando los registros alternativos BC\', DE\' y HL\' disponibles y las pilas limpias.

- El resto de la línea no se analiza sintácticamente, lo que permite que la rutina realice su propio análisis de parámetros mediante RST 18h y RST 20h.

Al entrar en la rutina, el registro BC contiene la dirección llamada.

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *Limitación con literales numéricos: si la rutina usa la rutina SCANNING de la ROM para analizar parámetros, las expresiones con literales numéricos directos (ej. USR 40000) no funcionarán correctamente. Usa VAL \"número\" o CODE \"carácter\" como alternativa: por ejemplo, USR VAL \"40000\" en lugar de USR 40000.* |

## 12.4 Acceso a puertos de entrada/salida

**Escribir en un puerto (\*OUT):**

> LOAD \*OUT \<puerto\>,\<valor\>

**Leer de un puerto (\*IN):**

> LOAD \*IN \<puerto\> TO \<variable\>

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *TO es el token del BASIC (SHIFT + 4), no se escribe letra a letra.* |

## 12.5 Acceso a memoria de 16 bits

**Leer un valor de 16 bits de memoria (LOAD PEEK):**

> LOAD PEEK \<dirección\> TO \<variable\>

Lee dos bytes consecutivos de memoria a partir de \<dirección\> y los almacena como un valor de 16 bits en \<variable\>.

**Escribir un valor de 16 bits en memoria (LOAD THEN POKE):**

> LOAD THEN POKE \<dirección\>,\<valor16\>

**Establecer el límite superior de memoria del BASIC (LOAD THEN CLEAR):**

> LOAD THEN CLEAR \<dirección\>

Establece la última dirección de RAM disponible para el BASIC. A diferencia del comando CLEAR estándar, este no borra las variables; solo limpia la pila de GOSUB. Usa un CLEAR separado si también quieres borrar las variables.

## 12.6 Acceso al directorio desde un programa

Estos comandos permiten leer el contenido de un directorio de la SD desde dentro de un programa BASIC, útil para construir menús de selección de archivos.

**Abrir un directorio para lectura (\*OPENDIR):**

> LOAD \*OPENDIR \<cadena\>

La cadena puede incluir una ruta completa y comodines. Si se usan comodines, el número máximo de entradas recuperables es 512. Este comando deja el directorio preparado para usar \*ROW.

**Leer una entrada del directorio (\*ROW):**

> LOAD \*ROW \<número\> TO \<variable\$\>

Lee la entrada de directorio con el número indicado (empezando desde 1) y la almacena en la variable de cadena. Si el número está fuera de rango, devuelve una cadena vacía.

**Ejemplo --- menú de selección de archivos en BASIC:**

> 10 LOAD \*OPENDIR \"JUEGOS/\*.P\"
>
> 20 FOR I=1 TO 10
>
> 30 LOAD \*ROW I TO A\$
>
> 40 IF A\$=\"\" THEN GOTO 70
>
> 50 PRINT I;\". \";A\$
>
> 60 NEXT I
>
> 70 INPUT \"Selecciona: \";N
>
> 80 LOAD \*ROW N TO A\$
>
> 90 LOAD FAST A\$

# 13. Ejemplos de programas

Esta sección recoge programas de ejemplo que ilustran el uso de las funciones principales del SD81 Booster. Todos están escritos en BASIC estándar del ZX81 con las extensiones del interface.

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *Los textos entre corchetes (por ejemplo \[SINCLAIR\]) se introducen en vídeo inverso en el ZX81, pulsando SHIFT + 9 antes de cada carácter. CHR\$ 8 corresponde al carácter gráfico de cuadrícula del ZX81 (código 8).* |

## 13.1 Reloj en tiempo real (RTC)

Demuestra los tres formatos de ajuste del reloj: fecha y hora completas, solo fecha y solo hora.

> 10 LET A\$=\"2025-11-10 13:48:00.00\"
>
> 15 LOAD \*RTC
>
> 16 PRINT
>
> 20 LOAD \*RTC=A\$
>
> 25 LOAD \*RTC
>
> 26 PRINT
>
> 30 LOAD \*RTC=\"2026-11-10\"
>
> 35 LOAD \*RTC
>
> 36 PRINT
>
> 40 LOAD \*RTC=\"12:20\"
>
> 45 LOAD \*RTC
>
> 46 PRINT
>
> 50 LOAD \*RTC=\"13:20:35\"
>
> 55 LOAD \*RTC
>
> 56 PRINT

Muestra la hora actual (línea 15), luego la ajusta mediante una variable de cadena (línea 20), después cambia solo la fecha (línea 30), luego solo la hora con formato corto (línea 40) y finalmente con hora, minutos y segundos (línea 50). Tras cada ajuste imprime el resultado para verificarlo.

## 13.2 Estado de la batería del RTC (BAT)

> 10 LOAD \*BAT TO A\$
>
> 20 PRINT A\$

Lee el estado de la batería del reloj en tiempo real y lo muestra en pantalla. Si la batería está baja, el valor mostrado lo indicará.

## 13.3 Comprobación del modo MC45

Comprueba si el modo de código máquina en bloques 4 y 5 está activo cargando una pequeña rutina en ensamblador y ejecutándola:

> 10 LOAD \*HEX 40000,\"010203C9\"
>
> 20 IF USR 40000=770 THEN GOTO 100
>
> 30 PRINT \"MC45 INACTIVE\"
>
> 40 STOP
>
> 100 PRINT \"MC45 ACTIVE\"

Carga en la dirección 40000 (bloques 4-5) una rutina Z80 que devuelve el valor de BC al salir (C9=RET). Si MC45 no está activo, las instrucciones en esa zona no se ejecutarán correctamente y el valor devuelto no será 770. Si MC45 está activo, la rutina se ejecuta correctamente y salta a la línea 100.

## 13.4 Modo Superfast --- demostración de velocidad

Compara visualmente la velocidad de refresco en modo estándar ZX81 frente al modo Superfast del SD81 Booster:

> 4 SLOW
>
> 5 PRINT \"ZX81 SLOW MODE\"
>
> 6 PAUSE 250
>
> 7 POKE 2045,85
>
> 8 POKE 16418,0
>
> 9 CLS
>
> 10 GOSUB 1000
>
> 15 CLS
>
> 20 POKE 2045,170
>
> 30 PRINT \"SD81-BOOSTER SUPER FAST MODE\"
>
> 31 PAUSE 250
>
> 35 CLS
>
> 36 FAST
>
> 37 GOSUB 1000
>
> 40 GOTO 40
>
> 75 POKE 1024,2
>
> 76 POKE 1024,3
>
> 77 POKE 1024,4
>
> 1000 PRINT \"\[CHR\$ 0\]123456789ABCDEFGHIJKLMNOPQRSTUV\";
>
> 1010 FOR N=1 TO 22
>
> 1020 PRINT \"0123456789ABCDEFGHIJKLMNOPQRSTUV\";
>
> 1030 NEXT N
>
> 1040 PRINT \"0123456789ABCDEFGHIJKLMNOQRSTUV\[CHR\$ 0\]\";
>
> 1050 RETURN
>
> 2000 PRINT \"\[CHR\$ 0\]123456789ABCDEFGHIJKLMNOPQRSTUV\";
>
> 2010 FOR N=1 TO 22
>
> 2020 PRINT \"0123456789ABCDEFGHIJKLMNOPQRSTUV\";
>
> 2030 NEXT N
>
> 2040 PRINT \"0123456789ABCDEFGHIJKLMNOQRSTUV\[CHR\$ 0\]\";
>
> 2050 RETURN

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *\[CHR\$ 0\] en las líneas 1000, 1040, 2000 y 2040 representa el carácter 0 en vídeo inverso visible en el listado original.* |

El programa muestra primero una pantalla llena de texto en modo SLOW estándar del ZX81, donde la CPU dedica tiempo al refresco de vídeo. Luego activa el modo Superfast (POKE 2045,170) y repite el mismo relleno de pantalla con FAST, mostrando la diferencia de velocidad.

## 13.5 Carga de imagen en modo Spectrum

Carga una imagen en formato Spectrum (.SCR) desde la carpeta /SCR/ de la SD y la muestra usando el modo HiRes Spectrum del interface:

> 5 LOAD \*CD \"/SCR\"
>
> 15 LET HFILE=32768
>
> 20 POKE 2044,HFILE/256
>
> 25 POKE 2045,172
>
> 30 LOAD \*OUT 32751,39
>
> 40 LOAD \*OUT 251,6
>
> 50 LOAD FAST \"Z.SCR\" CODE HFILE
>
> 190 IF INKEY\$=\"\" THEN GOTO 15
>
> 200 POKE 2045,85
>
> 210 LOAD \*OUT 32751,0

**Explicación línea a línea:**

- Línea 5: cambia al directorio /SCR de la SD.

- Línea 15: define HFILE=32768 (8000h), inicio del bloque 4.

- Línea 20: escribe la parte alta de la dirección del fichero de pantalla en el registro del interface.

- Línea 25: activa el modo Superfast HiRes Spectrum (POKE 2045,172).

- Líneas 30-40: configura los registros de color del borde mediante el puerto de salida del interface.

- Línea 50: carga el archivo Z.SCR en la dirección HFILE (32768).

- Línea 190: espera a que se pulse cualquier tecla; al pulsarla vuelve a la línea 15 para recargar.

- Línea 200: desactiva el modo Superfast.

- Línea 210: restaura el registro de salida a 0.

# 14. Códigos de error

Cuando se produce un error, el ZX81 muestra un código en la parte inferior de la pantalla seguido del número de línea donde ocurrió. Los códigos relacionados con el SD81 Booster son:

| **Código** | **Significado** |
|---------|---------------------------------------------------------------|
| **A** | Argumento inválido (nombre de archivo incorrecto, parámetro fuera de rango, o cadena hexadecimal con longitud incorrecta en \*PEG o \*HEX). |
| **D** | El usuario pulsó BREAK para interrumpir una operación (por ejemplo, durante un listado de directorio). |
| **G** | Archivo no encontrado en la SD. |
| **H** | Error al acceder a la tarjeta SD. Comprueba que está insertada correctamente y formateada en FAT32. |
| **I** | Error de E/S en la tarjeta SD durante una operación de lectura o escritura. |
| **J** | Disco lleno: no hay espacio suficiente en la SD para guardar el archivo. |
| **K** | Archivo o directorio ya existe con ese nombre. |
| **L** | Nombre de archivo demasiado largo o con caracteres no permitidos. |
| **M** | El directorio no está vacío (al intentar eliminar con \*RD). |
| **N** | Permiso denegado o archivo protegido contra escritura. |

|  |  |
|:----:|------------------------------------------------------------------|
| **💡** | *Si obtienes el error H de forma repetida, extrae la tarjeta SD, comprueba el formato FAT32 y vuelve a insertarla. Si el error persiste, prueba con otra tarjeta.* |

# 15. Para programadores

Esta sección está dirigida a desarrolladores que deseen aprovechar las capacidades avanzadas del SD81 Booster desde código máquina Z80, incluyendo las rutinas de la ROM de expansión y el sistema de comunicación con el microcontrolador.

## 15.1 Mapa de la ROM de expansión

La ROM de expansión ocupa el bloque 1, a partir de la dirección 8192 (2000h):

| **Dirección** | **Contenido / Rutina** |
|----------|--------------------------------------------------------------|
| 2000h | Cadena \'SD81\' en codificación ZX81 |
| 2004h | Byte de versión ROM (nibble alto = mayor, nibble bajo = menor). PEEK 8196. |
| 2005h | Rutina: devuelve en HL la dirección de retorno del llamador |
| 2006h | Rutina: ejecuta JP (HL) --- emula CALL (HL) |
| 2007h | GetMCUVersion --- versión MCU en BC (B=0, C=versión). USR 8199. |
| 200Ah | WaitClkDiff --- espera bit de reloj diferente al bit 7 de C |
| 200Dh | WaitClkEq --- espera bit de reloj igual al bit 7 de C |
| 2010h | OutWaitDiff --- envía A al puerto de datos y espera reloj diferente |
| 2013h | OutWaitEq --- envía A al puerto de datos y espera reloj igual |
| 2016h | WaitDiffBrk --- como WaitClkDiff pero permite BREAK (no retorna si se pulsa) |
| 2019h | WaitEqBrk --- como WaitDiffBrk pero espera igualdad |
| 201Ch | SendString --- envía cadena al MCU con longitud prefijada. B=longitud, DE=dirección |
| 201Fh | SendStrLoop --- envía B bytes al MCU desde DE sin esperar cambios de reloj |
| 2022h | ReportStatus --- lee byte del MCU; si ≠0 genera error BASIC (1=G, 2=H, \...) |
| 2025h | PrintBPaged --- imprime carácter con paginación. B=carácter (0--63 o 128--191) |
| 2028h | Cmd64C --- activa modo 64 caracteres (= LOAD \*64C) |
| 202Bh | Cmd128C --- activa modo 128 caracteres (= LOAD \*128C) |
| 202Eh | GetPhase --- estado del reloj en bit 7 de C |
| 2031h | GetData --- lee puerto de datos del MCU. Resultado en A. No modifica flags. |
| 2034h | SD81_RESET --- punto de entrada RESET. Requiere NMI y DI deshabilitados; HL=dirección de continuación. |
| 2037h | SD81LOADCMD --- punto de entrada para el comando LOAD extendido |
| 203Ah | SD81SAVECMD --- punto de entrada para el comando SAVE extendido |
| 203Dh | SD81RUNCMD --- punto de entrada para el comando RUN extendido |

**Obtener la versión desde BASIC:**

> 10 LET V=USR 8199
>
> 20 LET MAJ=INT(V/16)
>
> 30 LET MIN=V-16\*MAJ
>
> 40 PRINT \"VERSION MCU: \";CHR\$(MAJ+28);\".\";CHR\$(MIN+28)

## 15.2 Puertos de E/S y protocolo MCU

| **Puerto** | **Función** |
|---------|---------------------------------------------------------------|
| **E7h** | Mapeador de memoria |
| **A7h** | Puerto de datos MCU (lectura y escritura). |
| **AFh** | Puerto de control MCU (escritura=reset MCU; bit 7 en lectura=bit de reloj, los bits 6..1 son el contador de VSYNC desde la ultima lectura y el bit 0 en lectura indica el estado instantáneo de VSYNC) |

### Sincronización con VSYNC

El bit 0 del puerto AFh refleja el estado de la interrupción de sincronismo vertical (VSYNC). Esto permite a la CPU esperar al inicio del refresco de pantalla de forma precisa, sin necesidad de interrupciones ni del comando HALT.

Los bits 1 a 6 son un contador del numero de pulsos de VSYNC que se han producido desde la ultima lectura del puerto.

En el ZX Spectrum, muchos juegos usaban HALT o una rutina de interrupción IM1/IM2 para sincronizarse con el barrido vertical. En el SD81 Booster este mecanismo sustituye esa funcionalidad y es especialmente útil al portar juegos de Spectrum.

**Ejemplo de bucle de espera a VSYNC en ensamblador Z80:**

> WAIT_VSYNC:
>
> in a,(0AFh) ; leer puerto de datos MCU
>
> and 01h ; aislar bit 0 (VSYNC)
>
> jr nz,WAIT_VSYNC ; esperar hasta que VSYNC = 0
>
> WAIT_VSYNC2:
>
> in a,(0AFh)
>
> and 01h
>
> jr z,WAIT_VSYNC2 ; esperar flanco (VSYNC = 1)
>
> ; Sincronizados con el inicio del frame

|  |  |
|:----:|------------------------------------------------------------------|
| **⚠** | *Escribir cualquier valor en el puerto AFh provoca un reset software del MCU. No debe hacerse mientras el MCU esté guardando o copiando un archivo.* |

La comunicación se sincroniza mediante el bit de reloj (bit 7 del puerto AFh), que se invierte con cada lectura o escritura en A7h. El Z80 debe esperar a que el bit cambie antes de la siguiente operación.

**Ejemplo --- comando GETBYTE (índice 16 de la memoria interna del MCU):**

> in a,(0AFh) ; leer bit de reloj inicial
>
> ld c,a
>
> ld a,20h ; código GETBYTE
>
> out (0A7h),a ; enviar comando
>
> WAIT1: in a,(0AFh)
>
> xor c
>
> jp p,WAIT1 ; esperar reloj diferente
>
> ld a,16 ; índice
>
> out (0A7h),a ; enviar parámetro
>
> WAIT2: in a,(0AFh)
>
> xor c
>
> jp m,WAIT2 ; esperar reloj igual
>
> in a,(0A7h) ; leer respuesta --- resultado en A

## 15.3 Mapeador de memoria (puerto E7h)

En modo de paginación simple (hasta 256 KB), los 8 bits escritos en el puerto E7h se interpretan así:

| **Bits**               | **Función**              |
|------------------------|--------------------------|
| **D2, D1, D0**         | Número de bloque (0--7)  |
| **D7, D6, D5, D4, D3** | Número de página (0--31) |

Para acceder a las 64 páginas del modo completo (512 KB), se usa la instrucción OUT (C),r con el número de página en B y el número de bloque en otro registro. Si A contiene el número de bloque (0--7) y B el de página (0--63):

> ld c,0E7h ; puerto del mapeador
>
> out (c),a ; selecciona página B en bloque A

El cambio entre modo simple y completo se realiza mediante un comando al MCU (comandos FULLPAGING y HALFPAGING, ver sección 15.5).

## 15.4 Consola de depuración (puerto USB-C)

El puerto USB-C del interface también funciona como puerto serie de depuración. Al conectarlo al ordenador, el sistema operativo detecta un puerto serie virtual asociado al chip CH340G. En algunos sistemas puede ser necesario instalar los drivers del CH340.

**Parámetros de conexión:**

| **Parámetro**        | **Valor**      |
|----------------------|----------------|
| **Velocidad**        | 115200 baudios |
| **Bits de datos**    | 8              |
| **Paridad**          | Ninguna        |
| **Bits de parada**   | 1              |
| **Control de flujo** | Ninguno        |

Con cualquier programa de terminal serie (PuTTY en Windows, minicom en Linux, CoolTerm en macOS, o el propio Monitor Serie del IDE de Arduino) es posible monitorizar en tiempo real los mensajes del MCU, incluyendo: progreso del arranque, errores de acceso a la SD, progreso de actualizaciones de firmware, y mensajes de depuración del sistema de archivos, VGM, PEG y síntesis de voz.

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *El firmware de producción emite mensajes básicos de estado por el puerto serie. Recompilando el firmware con la macro DEBUG activa se obtiene una salida mucho más detallada, útil para diagnóstico avanzado y desarrollo.* |

## 15.5 Tabla completa de comandos MCU

Los comandos se envían al MCU escribiendo su código en el puerto de datos A7h, siguiendo el protocolo de sincronización por bit de reloj descrito en la sección 15.2. Todos los parámetros de cadena van precedidos de un byte con la longitud.

### Códigos de error devueltos por los comandos

| **Código** | **Significado**                                        |
|------------|--------------------------------------------------------|
| **0**      | Éxito                                                  |
| **1**      | Archivo o directorio no encontrado                     |
| **2**      | No es un directorio                                    |
| **3**      | Error de operación (no se pudo crear/borrar/renombrar) |
| **4**      | El archivo o directorio ya existe                      |
| **5**      | Archivo demasiado grande                               |
| **6**      | No se pudo crear el archivo destino                    |
| **7**      | Error de escritura                                     |
| **8**      | Error de lectura parcial                               |
| **12**     | No hay ningún archivo VGM abierto                      |
| **13**     | Operación no permitida en directorio T81               |
| **14**     | Parámetro de joystick inválido                         |

### Comandos de sistema

| **Cód.** | **Nombre** | **Parámetros** | **Respuesta** | **Descripción** |
|------|---------|--------------|----------|-----------------------------------|
| **0** | **NOP** | **---** | **---** | **Sin operación. Solo sincroniza el reloj.** |
| 1 | VERSION | --- | 1 byte: versión | Devuelve la versión del MCU. Mismo formato que el byte en 2004h. |
| 32 | GETBYTE | 1 byte: índice (0--255) | 1 byte: valor | Lee un byte de la memoria interna. Índices 0--127: variables volátiles. Índices 128--255: EEPROM (persistente). |
| 33 | SETBYTE | 1 byte: índice + 1 byte: valor | --- | Escribe un byte en la memoria interna. |

### Comandos de sistema de archivos

| **Cód.** | **Nombre** | **Parámetros** | **Respuesta** | **Descripción** |
|------|------------|--------------------|------------|------------------------|
| **2** | **PWD** | **---** | **String + EOT + status** | **Devuelve el directorio actual en codificación ZX81.** |
| 3 | CD | String: ruta | Status | Cambia el directorio actual. Admite rutas absolutas (/) y relativas. |
| 4 | DEL | String: archivo | Status | Borra un archivo del directorio actual. Sin comodines. |
| 5 | MKDIR | String: nombre | Status | Crea un subdirectorio. |
| 6 | RMDIR | String: nombre | Status | Elimina un directorio vacío. |
| 7 | MOVE | String: origen + String: destino | Status | Renombra o mueve un archivo. |
| 8 | COPY | String: origen + String: destino | Status | Copia un archivo. La fecha/hora no se preserva. |
| 9 | LOAD | String: nombre | 2B longitud + N bytes + Status | Carga un archivo. .P/.81: calcula tamaño real. .ROM: carga en dirección 0 y resetea. .WAV: reproduce. |
| 10 | SAVE | String: nombre + 2B longitud + N bytes | Status | Guarda un bloque de datos como archivo en la SD. |
| 11 | TYPE | String: nombre | String char a char + EOT + Status | Envía el contenido de un archivo de texto. Con \* busca en /MAN/ con extensión .TXT. |
| 12 | DIR | String: ruta/comodín | String char a char + EOT + Status | Lista el directorio, incluyendo tamaños de archivo. |
| 14 | FREE_TXT | --- | String + EOT + Status | Devuelve espacio total y libre de la SD como texto. |
| 15 | FREE | --- | 4B total + 4B libre + Status | Espacio total y libre en KB como valores de 32 bits little-endian. |
| 16 | OPENDIR | String: ruta/comodín | Status | Abre un directorio y construye un array interno (máx. 512 entradas). |
| 17 | GETROWLEN | 2B: índice | 1B: longitud + Status | Longitud del nombre de la entrada índice del array abierto con OPENDIR. |
| 18 | GETROW | 2B: índice | 1B: longitud + N bytes + Status | Nombre de la entrada índice en codificación ZX81. Índice 0 = directorio actual. Directorios entre \< y \>. |
| 53 | F_OPEN | Handle(0..3)+nombre en ASCII | 1B: status | Abre un fichero grande (tamaño 32 bits) en un handle de archivo especificado (0..3). El nombre es una cadena Pascal en ASCII. |
| 58 | F_OPEN_ZX81 | Handle(0..3)+nombre en ZX81 | 1B: status | Abre un fichero grande (tamaño 32 bits) en un handle de archivo especificado (0..3). El nombre es una cadena Pascal en ZX81. |
| 54 | F_SEEK | Handle(0..3)+Offset (4 bytes Little endian) | 1B: status | Desplaza el puntero de lectura/escritura a la posición indicada en offset |
| 55 | F_READ | Handle(0..3)+Count(2B Little Endian) | count bytes + 1B:status | Lee count bytes. Siempre envia count bytes, si se termina el archivo rellena con ceros |
| 56 | F_WRITE | Handle(0..3)+Count(2B Little Endian)+info to write (count bytes) | 1B:status | escribe count bytes. |
| 57 | F_CLOSE | Handle(0..3) | 1B:status | Cierra el fichero |
| 59 | F_STAT | Handle(0..3) | 4B:tamaño+2B:fecha+2B:hora+1B:status | Devuelve el tamaño (32 bits) y la fecha/hora de creación (formato FAT) de un fichero ya abierto con F_OPEN. |

### Comandos de control del hardware

| **Cód.** | **Nombre** | **Parámetros** | **Respuesta** | **Descripción** |
|------|-------------|----------------|---------|------------------------------|
| **19** | **ENABLE_MC45** | **---** | **---** | **Activa el modo MC45 (código máquina en bloques 4 y 5).** |
| 20 | DISABLE_MC45 | --- | --- | Desactiva el modo MC45. |
| 21 | JOY | String: 5 bytes de teclas ZX81 | Status | Configura el mapeo del joystick: izquierda, derecha, arriba, abajo, fuego. |
| 27 | SEL_128CHARS | --- | --- | Activa el modo de 128 caracteres definibles. Equivale a LOAD \*128C. |
| 28 | SEL_64CHARS | --- | --- | Activa el modo estándar de 64 caracteres. Equivale a LOAD \*64C. |
| 29 | FULLPAGING | --- | --- | Activa el modo de paginación completa (512 KB, 64 páginas). |
| 30 | HALFPAGING | --- | --- | Activa el modo de paginación simple (256 KB, 32 páginas). |
| 48 | ENABLE_48K | --- | --- | Activa el modo RAM extendida de 48 KB. Equivale a LOAD \*RAM48. |
| 49 | DISABLE_48K | --- | --- | Desactiva el modo RAM extendida. Equivale a LOAD \*RAM48 STOP. |

### Comandos de síntesis de voz

| **Cód.** | **Nombre** | **Parámetros** | **Respuesta** | **Descripción** |
|------|-----------|---------------|---------|---------------------------------|
| **22** | **BINARY_SAY** | **String: bytes de alófonos** | **Status** | **Reproduce alófonos en formato binario. Síncrono (bloquea hasta terminar).** |
| 23 | SAY | String: texto ASCII | Status | Convierte texto a fonemas y reproduce. Con \* como primer carácter: background. Equivale a LOAD \*SAY. |

### Comandos AY / sonido

| **Cód.** | **Nombre** | **Parámetros** | **Respuesta** | **Descripción** |
|------|-----------|----------------|---------|-------------------------------|
| **24** | **AY_SET_REG** | **1B: registro (0--15) + 1B: valor** | **---** | **Escribe un valor en un registro del emulador AY.** |
| 25 | AY_GET_REG | 1B: registro (0--15) | 1B: valor | Lee el valor actual de un registro del emulador AY. |
| 26 | AY_PLAY | String: canal A + String: B + String: C | Status | Reproduce hasta tres cadenas PLAY simultáneas. Con \* en canal A: background. Equivale a LOAD \*PLAY. |

### Comandos VGM

| **Cód.** | **Nombre** | **Parámetros** | **Respuesta** | **Descripción** |
|------|-----------|--------------|---------|---------------------------------|
| **34** | **PLAY_VGM** | **String: nombre de archivo** | **Status** | **Abre y comienza a reproducir un archivo VGM en background. Añade .vgm si no tiene extensión.** |
| 35 | STOP_VGM | --- | --- | Detiene la reproducción VGM y reinicia el emulador AY. |
| 36 | PAUSE_VGM | --- | --- | Pausa la reproducción VGM. |
| 37 | CONT_VGM | --- | --- | Reanuda la reproducción VGM pausada. |
| 38 | LOOP_VGM | 1B: modo (0=no bucle, 1=bucle) | --- | Establece el modo de bucle del reproductor VGM. |

### Comandos PEG

| **Cód.** | **Nombre** | **Parámetros** | **Respuesta** | **Descripción** |
|------|------------|---------------|---------|--------------------------------|
| **40** | **LOAD_PEG** | **1B: dirección + String: datos hex** | **---** | **Carga instrucciones PEG en la memoria del generador. 2 bytes por instrucción en little-endian.** |
| 41 | PLAY_PEG | 1B: hilo (0--2) + 1B: dirección | --- | Inicia la ejecución de un programa PEG en el hilo indicado. |
| 42 | STOP_PEG | 1B: hilo (0--2) | --- | Detiene y reinicia el hilo PEG indicado. |
| 43 | PAUSE_PEG | 1B: hilo (0--2) | --- | Pausa el hilo PEG indicado. |
| 44 | CONT_PEG | 1B: hilo (0--2) | --- | Reanuda el hilo PEG indicado. |
| 45 | SDLOAD_PEG | String: nombre + 1B: dirección | Status | Carga un archivo .PEB desde la SD en la memoria PEG. Tamaño máximo: 512 bytes. |

### Comandos RTC y batería

| **Cód.** | **Nombre** | **Parámetros** | **Respuesta** | **Descripción** |
|------|--------|--------------|----------------|-------------------------------|
| **50** | **RTC** | **String: fecha/hora (o vacío para leer)** | **Si lectura: String ZX81 + Status. Si escritura: Status** | **Sin parámetros: devuelve fecha/hora. Con parámetros: ajusta el reloj. Formatos: AAAA-MM-DD HH:MM:SS.CC / AAAA-MM-DD HH:MM:SS / AAAA-MM-DD / HH:MM:SS.CC / HH:MM:SS / HH:MM.** |
| 52 | BAT | --- | 5 bytes ASCII + Status | Devuelve el nivel de batería del RTC como string de 5 caracteres en formato V.mmm (codificación ZX81). |

# 16. Solución de problemas

## 16.1 El interface no arranca o el ZX81 se queda bloqueado

| **Síntoma** | **Solución** |
|---------------------------|---------------------------------------------|
| **El ZX81 no muestra nada al encender** | Comprueba que el interface está correctamente insertado en el puerto de expansión. Desconéctalo y vuelve a conectarlo con el ZX81 apagado. |
| **El LED STAT no se ilumina** | Comprueba que la tarjeta SD está insertada y que contiene la carpeta SYS con su contenido completo. Sin ella el interface no arranca. |
| **LED STAT parpadea en Azul/Rojo durante el arranque** | Error inicializando la tarjeta SD. Comprueba que está insertada correctamente y formateada en FAT32. |
| **LED STAT parpadea en Naranja/Rojo durante el arranque** | Error escribiendo la ROM en RAM. Comprueba que la carpeta SYS contiene los archivos de ROM necesarios. |
| **LED STAT parpadea en Naranja durante el arranque** | El MCU está esperando respuesta de la FPGA. Si el parpadeo no termina, puede indicar un problema de hardware. |
| **Pantalla en blanco o con ruido** | Asegúrate de que los pines del conector de expansión no están doblados o sucios. |
| **El ZX81 arranca pero los comandos del interface no funcionan** | Verifica la versión del firmware con LOAD \*VER. Para actualizar, copia firmware.bin en la raíz de la SD y enciende el ZX81. |

## 16.2 Problemas con la tarjeta microSD

| **Síntoma** | **Solución** |
|----------------|--------------------------------------------------------|
| **Error H al intentar cargar** | Comprueba que la SD está correctamente insertada. Extráela y vuélvela a insertar. Si el error persiste, reformatea la tarjeta en FAT32. Si sigue fallando, puede ser una tarjeta SD defectuosa. |
| **La SD no se reconoce** | Verifica que está formateada en FAT32 (no exFAT ni NTFS). |
| **Error G al cargar un programa** | El archivo no existe con ese nombre. Usa LOAD \*DIR para ver los nombres exactos. |
| **Error J al guardar** | La tarjeta SD está llena. Usa LOAD \*FREE para comprobar el espacio disponible. |

## 16.3 El reloj pierde la hora al apagar

El reloj en tiempo real se alimenta de la pila botón CR2032. Si el reloj pierde la hora sistemáticamente al apagar el ZX81, probablemente la pila necesita ser reemplazada.

Comprueba el nivel de carga con:

> LOAD \*BAT

Si el voltaje es inferior a 2.5V aproximadamente, sustituye la pila por una CR2032 nueva. La pila se encuentra en la placa del interface y puede extraerse con una herramienta plana fina.

## 16.4 El joystick no responde

| **Síntoma** | **Solución** |
|-----------------|-------------------------------------------------------|
| **El joystick no hace nada** | Configura el mapeo con LOAD \*JOY \"QAOP \" (arriba/abajo/izquierda/derecha/fuego) u otro mapeo según el juego, antes de lanzar el programa. |
| **Solo funciona alguna dirección** | Comprueba que la cadena de configuración tiene exactamente 5 caracteres. |
| **El joystick mueve pero no dispara** | Verifica que el quinto carácter de la cadena JOY corresponde a la tecla de fuego del juego. |

## 16.5 El sonido no funciona

| **Síntoma** | **Solución** |
|------------------|------------------------------------------------------|
| **No hay sonido con LOAD \*PLAY** | Comprueba que el televisor o monitor está conectado al conector SCART del interface y que el canal de audio del SCART no está silenciado. |
| **La voz no se entiende** | Prueba con frases cortas en inglés. Escribe las palabras fonéticamente si el resultado no es satisfactorio. |
| **El VGM no suena** | Verifica que el archivo es un VGM con datos del chip AY únicamente. Los VGMs con otros chips no son compatibles. |

## 16.6 Errores de actualización de firmware

| **Síntoma** | **Solución** |
|----------------------------|--------------------------------------------|
| **LED STAT parpadea en Azul/Rojo al arrancar con firmware.bin en la SD** | Error inicializando la tarjeta SD. Extrae la SD, comprueba el formato FAT32 y vuelve a intentarlo. |
| **LED STAT parpadea en Blanco/Rojo tras intentar actualizar** | Error durante la actualización. El firmware.bin permanece en la SD. Comprueba que el archivo no está corrupto y vuelve a encender el ZX81 para reintentar. |
| **El LED STAT se queda en Amarillo fijo indefinidamente** | La actualización está en curso. Espera al menos 2 minutos antes de considerar que hay un problema. No apagues el ZX81. |
| **Tras la actualización el interface no responde** | Comprueba con LOAD \*VER que la versión es correcta. Si el interface no arranca, sigue el procedimiento de recuperación de emergencia via USB descrito en la sección 17. |

## 16.7 Uso de la consola de depuración como herramienta de diagnóstico

Cuando el LED STAT muestra un error pero no está claro cuál es la causa, la consola de depuración por USB-C puede proporcionar información adicional muy valiosa.

**Cómo conectarse:**

14. Conecta un cable USB-C entre el interface y el ordenador.

15. Abre un programa de terminal serie (PuTTY, Tera Term, minicom, el Monitor Serie del IDE de Arduino\...) y conéctate al puerto COM/serie del CH340 con los parámetros: 115200 baudios, 8N1, sin control de flujo.

16. Enciende el ZX81 con el interface conectado.

17. Observa los mensajes que aparecen en la terminal durante el arranque y la operación normal.

El firmware de producción emite mensajes básicos de estado que permiten identificar en qué fase falla el arranque, si la tarjeta SD se reconoce correctamente, el resultado de las actualizaciones de firmware y otros eventos relevantes.

|  |  |
|:----:|------------------------------------------------------------------|
| **💡** | *Para desarrolladores: recompilando el firmware con la macro DEBUG activa se obtiene una salida mucho más detallada, incluyendo el progreso de las operaciones de flash, el estado de los registros del AY y los detalles de cada comando recibido del Z80.* |

# 17. Actualización del firmware

El SD81 Booster tiene dos componentes de firmware actualizables: el microcontrolador (MCU) y la FPGA.

|  |  |
|:----:|------------------------------------------------------------------|
| **⚠** | *No interrumpas el proceso de actualización una vez iniciado. Una actualización incompleta puede dejar el interface en un estado no operativo.* |

## 17.1 Actualización del microcontrolador (MCU)

El SD81 Booster incorpora un bootloader que permite actualizar el firmware sin herramientas externas ni cables especiales.

**Requisitos:**

- Tarjeta microSD del interface.

- Archivo de firmware firmware.bin, disponible en el repositorio del proyecto.

**Proceso:**

18. Descarga firmware.bin desde el repositorio del proyecto.

19. Copia firmware.bin en la raíz de la tarjeta microSD (no en ninguna subcarpeta).

20. Inserta la tarjeta en el interface con el ZX81 apagado.

21. Enciende el ZX81. El bootloader detectará el archivo, realizará la actualización y lo borrará de la SD al terminar.

|  |  |
|:----:|------------------------------------------------------------------|
| **⚠** | *No apagues el ZX81 ni extraigas la tarjeta SD durante la actualización.* |

Verifica la versión instalada con:

> LOAD \*VER

**Recuperación de emergencia via USB:**

En caso de que el interface quede inoperativo, existe un procedimiento de recuperación via USB-C orientado a usuarios avanzados que requiere acceder al interior de la carcasa y manipular el jumper JP7. Las instrucciones detalladas están disponibles en el repositorio del proyecto:

https://codeberg.org/Retrostuff/SD81-Booster

## 17.2 Actualización de la FPGA

La FPGA (Xilinx Spartan-6 XC6SLX9) carga su configuración en cada arranque desde una memoria flash SPI auxiliar (W25Q128). Al igual que el MCU, el SD81 Booster puede reprogramar esta flash automáticamente desde la tarjeta microSD, sin herramientas ni cables especiales --- ya no hace falta un cable Xilinx Platform Cable USB ni la herramienta iMPACT.

**Requisitos:**

- Tarjeta microSD del interface.

- Archivo SD81.MCS, disponible en el repositorio del proyecto.

**Proceso:**

22. Descarga SD81.MCS desde el repositorio del proyecto.

23. Copia SD81.MCS en la raíz de la tarjeta microSD (no en ninguna subcarpeta).

24. Inserta la tarjeta en el interface con el ZX81 apagado.

25. Enciende el ZX81. El sistema detectará el archivo, reprogramará la flash de la FPGA y lo borrará de la SD al terminar.

|  |  |
|:----:|------------------------------------------------------------------|
| **⚠** | *No apagues el ZX81 ni extraigas la tarjeta SD durante la actualización. Si el proceso se interrumpe, el sistema lo detectará y reintentará automáticamente en el siguiente arranque --- el archivo SD81.MCS no se borra de la SD hasta confirmar que la actualización se completó correctamente.* |

# 18. Glosario

| **Término** | **Definición** |
|----------------|--------------------------------------------------------|
| **Alófono** | Unidad mínima de sonido del habla usada por el sintetizador de voz. El SD81 Booster usa los alófonos del chip SP0256. |
| **Bloque** | División de 8 KB del espacio de direccionamiento del Z80. El SD81 Booster divide los 64 KB del Z80 en 8 bloques (0--7). |
| **FPGA** | Circuito lógico programable (Xilinx Spartan-6 XC6SLX9) que implementa por hardware la lógica de vídeo, el mapeador de memoria y otras funciones del interface. |
| **FAT32** | Sistema de archivos requerido por la tarjeta microSD del interface. Incompatible con exFAT y NTFS. |
| **FAST** | Token del BASIC del ZX81 (SHIFT+F). En el SD81 Booster, activa el modo de carga/guardado desde la SD. |
| **Fichero de pantalla (HFILE)** | Bloque de memoria que contiene los datos de la pantalla en los modos Superfast. |
| **HRG** | High Resolution Graphics. Modo de alta resolución del ZX81. |
| **MCU** | Microcontrolador. El chip que gestiona la SD, el sonido, el RTC y la comunicación con el Z80. |
| **Página** | División de 8 KB de la RAM del interface. Los 512 KB se dividen en 64 páginas (0--63) que pueden mapearse a cualquier bloque. |
| **PEG** | Programmable Effects Generator. Máquina virtual para reproducir efectos de sonido en background sin usar la CPU del ZX81. |
| **RTC** | Real Time Clock. Reloj en tiempo real incorporado en el SD81 Booster, alimentado por una pila CR2032. |
| **SLOW** | Token del BASIC del ZX81 (SHIFT+D). En el SD81 Booster, activa el modo de carga/guardado desde cinta (audio). |
| **SP0256** | Chip sintetizador de voz de General Instrument, base del sintetizador del SD81 Booster. Usado también en el Currah MicroSpeech y The Voice. |
| **Superfast** | Modo en el que el hardware del SD81 Booster gestiona el refresco de pantalla liberando la CPU del ZX81 para otras tareas. |
| **T81** | Formato de archivo contenedor que agrupa múltiples programas ZX81. El interface puede navegar su contenido como si fuera un directorio. (fase alfa) |
| **Token** | En el BASIC del ZX81, cada palabra reservada se almacena como un único byte. Se introducen con combinaciones de SHIFT. |
| **VGM** | Video Game Music. Formato de archivo de música que el SD81 Booster puede reproducir en background usando el emulador AY. |
| **Vídeo inverso** | Modo de visualización del ZX81 en el que el fondo y el carácter intercambian colores. Se activa con SHIFT+9 antes del carácter. |

# 19. Historial de versiones del firmware

| **Versión** | **Fecha** | **Novedades principales**               |
|-------------|-----------|-----------------------------------------|
| **1.0**     | 2025      | Primera versión de lanzamiento público. |

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *Este historial se actualizará con cada nueva versión del firmware. Consulta el repositorio del proyecto para ver el registro completo de cambios.* |

# 20. Referencias

## El proyecto SD81 Booster

**Repositorio oficial** --- código fuente, firmware, esquemas y documentación técnica:

https://codeberg.org/Retrostuff/SD81-Booster

**Va de Retro** --- foro de retroinformática en español donde se anunció una versión anterior del interface:

https://www.va-de-retro.com/foros/portal

## Herramientas

**STM32CubeProgrammer** --- herramienta de programación del MCU STM32, necesaria para recuperación via USB en caso de emergencia:

https://www.st.com/en/development-tools/stm32cubeprog.html

## Documentación técnica de referencia

**Chip sintetizador de voz SP0256-AL2** (General Instrument) --- hoja de características del chip en el que se basa el sintetizador de voz del SD81 Booster:

https://rarewaves.net/wp-content/uploads/2018/09/SP0256-AL2.pdf

**Interface Chroma81** --- documentación del interface de color para ZX81 cuya funcionalidad implementa el SD81 Booster. El enlace original ya no está disponible; puede encontrarse en archivos web:

http://www.fruitcake.plus.com/Sinclair/ZX81/Chroma/ChromaInterface_Documentation.htm

**Formato de archivo .P y .P81** --- especificación técnica de los formatos de programa del ZX81:

https://k1.spdns.de/Develop/Projects/zasm/Info/O80%20and%20P81%20Format.txt

**Especificación del formato VGM** (Video Game Music) --- formato de archivo de música utilizado por el reproductor VGM del interface:

https://vgmrips.net/wiki/VGM_Specification

## ROM del ZX81

El SD81 Booster incluye una ROM modificada basada en la disassembly original del ZX81. Créditos:

**Geoff Wearmouth** --- disassembly comentada de la ROM del ZX81 (preservada en archive.org):

https://web.archive.org/web/20150815035607/http://www.wearmouth.demon.co.uk/zx81.htm

**Tomaž Šolc** --- preservación de la disassembly:

https://www.tablix.org/\~avian/spectrum/rom/

## Créditos del proyecto

- Diseño del hardware y firmware del MCU: Alejandro Valero (wilco2009)

- Código Z80 / ROM modificada: Pedro Gimeno (pgimeno)

# Apéndice A --- Referencia completa del comando PLAY

## Tabla de duraciones

| **Valor** | **Nombre**               | **Duración a 60 bpm** |
|-----------|--------------------------|-----------------------|
| **1**     | Semicorchea              | 0.25 s                |
| **2**     | Semicorchea con puntillo | 0.375 s               |
| **3**     | Corchea                  | 0.5 s                 |
| **4**     | Corchea con puntillo     | 0.75 s                |
| **5**     | Negra                    | 1 s                   |
| **6**     | Negra con puntillo       | 1.5 s                 |
| **7**     | Blanca                   | 2 s                   |
| **8**     | Blanca con puntillo      | 3 s                   |
| **9**     | Redonda                  | 4 s                   |
| **10**    | Tresillo de semicorchea  | 0.1667 s              |
| **11**    | Tresillo de corchea      | 0.3333 s              |
| **12**    | Tresillo de negra        | 0.6667 s              |

## Efectos de envolvente (W)

| **Código** | **Forma**        | **Descripción**                |
|------------|------------------|--------------------------------|
| **W0**     | \\\|\_\_\_\_\_\_ | Decaimiento, luego silencio    |
| **W1**     | /\|\_\_\_\_\_\_  | Ataque, luego silencio         |
| **W2**     | \\\|‾‾‾‾‾        | Decaimiento, luego sostenido   |
| **W3**     | /‾‾‾‾‾‾          | Ataque, luego sostenido        |
| **W4**     | \\\|\\\|\\\|\\\| | Decaimiento repetido (tremolo) |
| **W5**     | /\|/\|/\|/\|     | Ataque repetido                |
| **W6**     | /\\/\\/\\/       | Ataque y decaimiento repetidos |
| **W7**     | \\/\\/\\/\\      | Decaimiento y ataque repetidos |

## Tabla resumen de parámetros

| **Parámetro** | **Descripción**                                      |
|---------------|------------------------------------------------------|
| C..B          | Nota en octava actual                                |
| Inv(C..B)     | Nota en octava siguiente (vídeo inverso)             |
| =             | Sostenido (siguiente nota)                           |
| £             | Bemol (siguiente nota) o silencio                    |
| 1..12         | Duración desde este punto                            |
| \-            | Ligadura de duración                                 |
| N / espacio   | Separador de números                                 |
| O\<n\>        | Octava (0--8, defecto 4)                             |
| T\<n\>        | Tempo en bpm (60--240, defecto 120) --- solo canal A |
| V\<n\>        | Volumen (0--15)                                      |
| W\<n\>        | Efecto de envolvente (0--7)                          |
| U             | Activa envolvente en el canal                        |
| X\<n\>        | Velocidad de envolvente (0--65535; 6927 ≈ 1 s)       |
| M\<n\>        | Selección de canales activos y modo (0--63)          |
| ( )           | Repetir sección una vez más                          |
| )             | Repetir desde el inicio indefinidamente              |
| H             | Detener PLAY en todos los canales                    |
| \*            | (Solo canal A) Reproducción en background            |

# Apéndice B --- Referencia del generador de efectos PEG

El PEG es una máquina virtual de 16 bits con acceso a los 16 registros del chip AY (R0--R15) y 16 variables de propósito general (V0--V15). Soporta hasta 3 hilos de ejecución paralelos y hasta 256 palabras de programa.

| **Instrucción** | **Codificación** | **Descripción** |
|---------------|-----------------|-----------------------------------------|
| LD R,XX | 0R XX | Carga registro AY con valor 8 bits |
| ADD R,XX | 1R XX | Suma valor 8 bits a registro AY |
| LD V,XX | 2R XX | Carga variable con valor 8 bits (resto cero) |
| ADD V,XX | 3R XX | Suma valor 8 bits a variable |
| LD R,R | 40 RR | Carga registro con otro registro |
| LD R,V | 41 RV | Carga registro con variable |
| LD V,R | 42 VR | Carga variable con registro |
| LD V,V | 43 VV | Carga variable con otra variable |
| ADD V,V | 44 VV | Suma dos variables |
| SUB V,V | 45 VV | Resta segunda de primera |
| ADC V,V | 46 VV | Suma con acarreo |
| SBC V,V | 47 VV | Resta con acarreo |
| NOT V,V | 48 VV | Primera = complemento a 1 de segunda |
| AND V,V | 49 VV | AND bit a bit |
| OR V,V | 4A VV | OR bit a bit |
| XOR V,V | 4B VV | XOR bit a bit |
| MUL V,V | 4C VV | Multiplica (resultado 32 bits en V y V+1) |
| DIV V,V | 4D VV | Divide (cociente en V, resto en V+1) |
| SHR V,X | 4E VX | Desplazamiento a la derecha |
| SHL V,X | 4F VX | Desplazamiento a la izquierda |
| MUL V,XX | 5V XX | Multiplica variable por constante |
| DIV V,XX | 6V XX | Divide variable por constante |
| SUB V,XX | 7V XX | Resta constante de variable |
| DJNZ V,XX | 8V XX | Decrementa y salta si no es cero |
| WAIT XXX | 9X XX | Espera tiempo en ms |
| WAIT V | A0 0V | Espera tiempo indicado en variable (ms) |
| HALT | A0 10 | Detiene el efecto |
| JR XX | A1 XX | Salto relativo |

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *Los offsets de salto son relativos a la instrucción siguiente. El ensamblador PEG incluido en el repositorio gestiona esto automáticamente.* |

# Apéndice C --- Diccionario del sintetizador de voz

El sintetizador se basa en los fonemas del chip SP0256 de General Instrument (Currah MicroSpeech, The Voice). El texto se analiza de izquierda a derecha buscando la coincidencia más larga posible.

## Palabras reconocidas (selección por longitud)

13 caracteres: INVESTIGATORS, IRRESPONSIBLE

12 caracteres: INVESTIGATOR

11 caracteres: INVESTIGATE

10 caracteres: CORRECTING

9 caracteres: COGNITIVE, CORRECTED, SEPTEMBER, SINCERELY, SINCERITY, INTERFACE

8 caracteres: CHECKERS, CHECKING, COMPUTER, CORRECTS, DAUGHTER, DECEMBER, EIGHTEEN, FEBRUARY, FREEZERS, FREEZING, NINETEEN, NOVEMBER, PLEDGING, SATURDAY

7 caracteres: BOOSTER, CHECKED, CHECKER, CORRECT, FREEZER, JANUARY, MINUTES, OCTOBER, PLASTIC, SIXTEEN, TUESDAY, COLLIDE

6 caracteres: AUGUST, COOKIE, EQUALS, EXTENT, FRIDAY, FROZEN, MONDAY, SUNDAY, TALKED, TALKER, TWENTY

5 caracteres: APRIL, CHECK, CROWN, EIGHT, EQUAL, ERROR, FIFTY, HELLO, MARCH, MONTH, SIXTY, TALKS, THREE, WORLD

4 caracteres: DATE, FIVE, FOUR, HAVE, JUNE, NINE, RAYS, TALK, THIS, TIME, WHAT, WHOA, WILL, ZX81

3 caracteres: ACK, ACT, ADD, AMP, ASH, ASK, BAD, BED, BIG, BOX, BUT, CAR, END, EST, GET, HAS, HIM, ICK, IMP, ING, INK, JOB, KEY, MAY, NOT, NOW, OLD, OUR, OUT, RAY, RED, SIX, SUN, TEN, THE, TOP, TWO, YES --- y todas las sílabas con dígrafos DH, NG, SH, TH, WH.

## Puntuación y pausas

| **Carácter** | **Efecto**  |
|--------------|-------------|
| Espacio      | Pausa corta |
| ,            | Pausa media |
| ; :          | Pausa larga |

## Alófonos directos (uso avanzado)

Pausas: PA1, PA2, PA3, PA4, PA5

Vocales: AA, AE, AH, AO, AW, AX, AY, EH, ER1, ER2, EY, IH, IY, OW, OY, UH, UW1, UW2, XR, YR

Consonantes: BB1, BB2, CH, DD1, DD2, DH1, DH2, EL, FF, GG1, GG2, GG3, HH1, HH2, JH, KK1, KK2, KK3, LL, MM, NG, NN1, NN2, OR, PP, RR1, RR2, SH, SS, TH, TT1, TT2, VV, WH, WW, YY1, YY2, ZH, ZZ

# Apéndice D --- Sistema de paginación de memoria

## Asignación inicial de páginas

| **Bloque** | **Página inicial** | **Rango / Uso**                 |
|------------|--------------------|---------------------------------|
| **0**      | 0                  | 0000--1FFF (ROM, solo lectura)  |
| **1**      | 1                  | 2000--3FFF (ROM de expansión)   |
| **2**      | 2                  | 4000--5FFF (RAM principal)      |
| **3**      | 3                  | 6000--7FFF (RAM principal)      |
| **4**      | 4                  | 8000--9FFF (RAM ampliada)       |
| **5**      | 5                  | A000--BFFF (RAM ampliada)       |
| **6**      | 2                  | C000--DFFF (espejo de bloque 2) |
| **7**      | 3                  | E000--FFFF (espejo de bloque 3) |

## Reglas de uso

- El bloque 0 es siempre de solo lectura. Los bloques 1--7 son siempre de lectura/escritura.

- Una misma página puede estar mapeada en más de un bloque simultáneamente.

- Los bloques 6 y 7 deben espejar los bloques 2 y 3 respectivamente para que el sistema de vídeo funcione. Excepción: si el fichero de pantalla está completamente en el bloque 2, el bloque 7 puede mapearse libremente, y viceversa.

- Los bloques 4 y 5 siempre pueden mapearse libremente.

- Por las peculiaridades del hardware del ZX81, los bloques 4--7 solo pueden usarse para datos, no para ejecutar código (salvo con el modo MC45 activo para los bloques 4 y 5).

## Modificación de la ROM

Es posible modificar la ROM mapeando la página 0 a cualquier bloque con escritura habilitada y realizando los cambios allí. También puede sustituirse completamente la ROM cargando el contenido deseado en cualquier página y mapeándola al bloque 0.

# Apéndice E --- Puerto del interface Chroma81 (7FEFh)

El SD81 Booster implementa la interfaz de color del Chroma81 a través del puerto 7FEFh (01111111 11101111 en binario). Este puerto puede leerse y escribirse.

## Escritura (OUT 7FEFh)

Permite configurar el modo de color y el color del borde:

| **Bits** | **Función**                                                      |
|--------|----------------------------------------------------------------|
| **7--6** | Reservado para uso futuro. Siempre a 0.                          |
| **5**    | Activar modo color: 1 = color activado.                          |
| **4**    | Modo de color: 0 = código de carácter, 1 = fichero de atributos. |
| **3**    | Bit de brillo del color del borde.                               |
| **2--0** | Color del borde en formato GRB (Green-Red-Blue).                 |

### Formato del color de borde (bits 2--0, formato GRB)

| **Valor (GRB)** | **Color** |
|-----------------|-----------|
| **000**         | Negro     |
| **001**         | Azul      |
| **010**         | Rojo      |
| **011**         | Magenta   |
| **100**         | Verde     |
| **101**         | Cian      |
| **110**         | Amarillo  |
| **111**         | Blanco    |

## Lectura (IN 7FEFh)

Permite detectar si el modo color está disponible y leer el estado del VSync:

| **Bit** | **Función** |
|------|------------------------------------------------------------------|
| **7--6** | No usado (reservado). |
| **5** | 0 = modos de color disponibles. Siempre 0, lo que indica que el modo color está siempre activo. |
| **4--1** | No usado (reservado). |

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *La lectura del bit 5 a 0 permite a los programas detectar automáticamente la presencia del interface Chroma81 y activar los modos de color si está disponible.* |

Comando equivalente:

LOAD \*COLOR : REM activar color, borde blanco (7) por defecto

LOAD \*COLOR \<color\> : REM activar color, borde = \<color\> (0-15)

LOAD \*COLOR STOP : REM desactivar color

(equivalente al OUT 7FEFh anterior; activa el bit 5 y fija el modo de código de carácter automáticamente. **\<color\>** son los 4 bits bajos del puerto, brillo + GRB combinados en 0-15.

# Apéndice F --- Modos Superfast y Spectrum

## El problema del vídeo en el ZX81 original

El ZX81 en modo SLOW gestiona el vídeo por software: durante el refresco de la pantalla, la CPU debe ejecutar instrucciones NOP mientras el hardware genera la señal de vídeo, lo que consume una parte importante del tiempo de procesador disponible. El SD81 Booster resuelve esto con el modo Superfast.

## Modo Superfast

En el modo Superfast, el hardware del interface toma el control del bus de datos durante el refresco de pantalla, colocando los datos de vídeo directamente sin intervención de la CPU. Esto libera al procesador para ejecutar código útil durante todo el ciclo, aumentando significativamente la velocidad efectiva del sistema.

Antes de activar el modo es necesario indicar la dirección del fichero de pantalla (HFILE):

> POKE 2043, HFILE_low : REM parte baja de la dirección del fichero de pantalla
>
> POKE 2044, HFILE_high : REM parte alta de la dirección del fichero de pantalla

Las tres variantes del modo Superfast se seleccionan mediante POKE en la dirección 2045:

| **POKE** | **Modo** |
|------------------|------------------------------------------------------|
| POKE 2045, 170 | Superfast texto (modo texto estándar acelerado) |
| POKE 2045, 171 | Superfast HiRes nativo (alta resolución en formato ZX81) |
| POKE 2045, 172 | Superfast HiRes Spectrum (alta resolución en formato Spectrum) |
| POKE 2045, 85 | Desactivar modo Superfast |

Con los comandos equivalentes:

LOAD \*SFAST : REM Superfast texto

LOAD \*SFHR \<direccion\> : REM Superfast HiRes nativo, HFILE = \<direccion\>

LOAD \*SFSP \<direccion\> : REM Superfast HiRes Spectrum, HFILE = \<direccion\>

LOAD \*SFAST STOP : REM desactivar (equivalente a \*SFHR STOP / \*SFSP STOP)

(equivalentes a los POKE anteriores, por si se prefiere desde código máquina). **LOAD \*SFHR** y **LOAD \*SFSP** escriben HFILE (2043/2044) y activan el modo en un solo paso; no hace falta indicar HFILE para **LOAD \*SFAST** porque el modo texto no usa RAM extendida.

## Modo Spectrum

El modo Spectrum (POKE 2045,172) reordena las líneas de pantalla para que coincidan con la organización de la pantalla del ZX Spectrum, facilitando la conversión de programas entre ambas plataformas. En el ZX81 estándar las líneas se organizan de forma diferente a como lo hace el Spectrum; este modo elimina esa diferencia por hardware.

|  |  |
|:----:|------------------------------------------------------------------|
| **⚠** | *En el modo Spectrum se emula el puerto del beeper y borde del Spectrum (ULA write port = FBh), donde los bits 2--0 controlan el color del borde y los bits 4--3 controlan el beeper. En este modo el puerto FBh queda ocupado y se pierde la compatibilidad con la ZX Printer.* |

## Control del borde

**Cambiar los atributos del borde:**

> POKE 2046, \<attr\>

**Definir y activar un patrón de borde:**

Es posible definir un patrón de 8 bytes que se repetirá a lo largo del borde de la pantalla, permitiendo rellenarlo completamente con un diseño personalizado:

> POKE 2048, byte0 : REM primer byte del patrón
>
> POKE 2049, byte1
>
> \...
>
> POKE 2055, byte7 : REM último byte del patrón
>
> POKE 2047, 170 : REM activar patrón de borde
>
> POKE 2047, 85 : REM desactivar patrón de borde

**Puerto ULA en modo Spectrum (FBh):**

| **Bits** | **Función**        |
|----------|--------------------|
| **4--3** | Control del beeper |
| **2--0** | Color del borde    |

Comando equivalente (tinta del patrón + borde en modo Spectrum):

LOAD \*BORDER \<color\> : REM tinta del patron (0-15) y borde en modo Spectrum (bits 2-0)

(equivalente a **POKE 2046,\<color\>** más el puerto ULA FBh anterior en un solo paso; no activa el patrón por sí solo, hace falta **POKE 2047,170**. El color de fondo del borde en modo nativo se controla con **LOAD \*COLOR**, no con este comando.)

## Sincronización con VSYNC

En el ZX Spectrum, muchos juegos utilizaban la instrucción HALT o una rutina de interrupción IM1/IM2 para sincronizarse con el barrido vertical de la pantalla y conseguir animaciones fluidas sin parpadeo.

En el SD81 Booster esta funcionalidad se sustituye mediante la lectura del bit 0 del puerto AFh, que refleja el estado de la interrupción de sincronismo vertical (VSYNC). La CPU puede esperar a este bit para sincronizarse con el inicio del refresco de pantalla sin necesidad de interrupciones ni de HALT. Consulta la sección 14.2 para el ejemplo de código completo.

## Doble buffer (present-blit)

### Cómo genera la imagen el interface (sin doble buffer)

Toda la RAM del sistema (hasta 512 KB) vive en un único chip de memoria externo (la **SRAM**), organizado en páginas de 8 KB que se asignan a los 8 bloques del mapa de memoria del Z80 mediante el mapper (E7h). Ahora bien, el circuito de vídeo **no lee esa SRAM directamente** --- si lo hiciera, competiría con la CPU por el mismo chip de memoria en cada ciclo. Para evitarlo, el interface mantiene dentro de la propia FPGA una **memoria interna de vídeo** (64 KB) que actúa como un \*\*espejo\*\* de los 8 bloques: cada vez que la CPU escribe un byte en la SRAM, ese mismo byte se copia automáticamente al espejo. El circuito de vídeo genera la imagen leyendo siempre este espejo interno, nunca la SRAM.

El espejo es fiel en todo momento --- demasiado fiel, de hecho: si en un instante dado la CPU acaba de borrar un sprite, el espejo ya refleja el hueco vacío, y si el haz de vídeo pasa por esa zona justo entonces, dibuja ese hueco. Como CPU y vídeo comparten el mismo dato \"en vivo\", el haz puede pillar estados intermedios del dibujado --- de ahí el parpadeo y el tearing que el doble buffer viene a resolver.

### Qué cambia el doble buffer

### 

POKE 2057, 168+B : REM activar doble buffer; front buffer = bloque B (0-7)

POKE 2057, 85 : REM desactivar doble buffer

Al activarlo, el interface deja de mantener el espejo en tiempo real **solo para el bloque \`B\`** de la memoria interna de vídeo (el resto de bloques del espejo siguen funcionando como siempre). A partir de ese momento:

\- El vídeo deja de mirar el espejo del bloque HFILE (la pantalla habitual) y pasa a mirar el espejo del bloque \`B\`.

\- El espejo del bloque \`B\` ya **no se actualiza escritura a escritura**. En cada parpadeo vertical (VSYNC), el hardware copia de golpe, byte a byte, todo el contenido actual del espejo del bloque HFILE dentro del espejo del bloque \`B\`. Entre un VSYNC y el siguiente, el espejo del bloque \`B\` queda **congelado**: es una fotografía fija, no un reflejo en vivo.

\> **El punto que más confunde --- léelo dos veces:** el bloque lógico \`B\` sigue existiendo también como memoria SRAM normal, tan accesible y escribible por la CPU como cualquier otro bloque. Pero mientras el doble buffer está activo, **esa SRAM deja de tener ninguna relación con lo que ves en pantalla**: es el espejo interno de ese bloque el que ya no la refleja. Lo que escribas en la SRAM del bloque \`B\` no aparecerá en la imagen, y lo que ves en pantalla no es esa SRAM sino la fotografía que la FPGA renovó en el último VSYNC, copiada del bloque HFILE. Por eso el bloque \`B\` conviene tratarlo como \"reservado para el hardware\" mientras el doble buffer está activo, aunque técnicamente sigas pudiendo usarlo como RAM para otras cosas (variables, código) que nada tengan que ver con la pantalla.

Dicho de otro modo, bajo el mismo número de bloque conviven tres cosas distintas que no hay que confundir:

| Capa | Qué contiene | Quién la actualiza |
|------------------------|------------------|-------------------------------|
| SRAM física del bloque \`B\` | La memoria real del sistema (8 KB) | La CPU, con cada lectura/escritura normal |
| Espejo interno del bloque \`B\` --- doble buffer OFF | Copia en vivo de esa SRAM | Automáticamente, byte a byte, en cada escritura de la CPU |
| Espejo interno del bloque \`B\` --- doble buffer ON | Fotografía fija del bloque HFILE | Solo la FPGA, de golpe, una vez por VSYNC |

El resultado práctico:

\- La pantalla muestra siempre una **instantánea completa** tomada en el último VSYNC: nunca se ven borrados ni dibujados a medias.

\- El programa dibuja siempre sobre **una única superficie** (la página HFILE de siempre): no hay que alternar entre dos páginas ni redibujar \"el frame de hace dos\".

\- Lo que se relee de la pantalla (la página HFILE, no el bloque \`B\`) es siempre lo último escrito --- operaciones de leer-modificar-escribir coherentes.

**Uso correcto:** espera el flanco de subida del VSYNC (bit 0 del puerto AFh) y realiza todo el borrado/dibujado a continuación. Desde ese momento dispones de unos **16 ms** antes de que el hardware tome la siguiente instantánea (la copia comienza justo al terminar el área visible). Si el dibujado excede ese tiempo, la instantánea podría capturar un estado intermedio --- el mismo comportamiento que un doble buffer clásico.

**Elección del bloque front (\`B\`):** como el espejo de ese bloque deja de reflejar su SRAM, **no debe apuntarse a él ningún elemento de vídeo** (HFILE, DFILE, atributos Chroma81) mientras el doble buffer esté activo --- su espejo ya no serviría para mostrarlos. Bloques recomendados: **4 o 5** (\$8000-\$9FFF / \$A000-\$BFFF, si no coinciden con tu HFILE). Evita: 0 (glyphs ROM del modo texto), 1 (chr RAM), 2-3 (DFILE), 6-7 (atributos Chroma81). Si el bloque front coincide con el de HFILE, la pantalla se queda congelada para siempre: el blit se limita a copiar el bloque sobre sí mismo cada VSYNC (sin efecto) y la máscara de escritura bloquea permanentemente que tus dibujos nuevos lleguen al mirror \-\-- la SRAM sí se actualiza, pero nada de eso llega a verse.

\> **Nota:** en modo HiRes nativo solo se doble-bufferiza el bitmap; los atributos (zona \$C000) se leen en directo. En modo Spectrum se doble-bufferiza el bloque completo (bitmap + atributos). El modo texto no usa el doble buffer.

**Ejemplo típico** (HFILE en \$8000 = bloque 4, front en bloque 5):

POKE 2043,0 : REM HFILE bajo

POKE 2044,128 : REM HFILE alto (\$8000)

POKE 2045,172 : REM Superfast HiRes Spectrum

POKE 2057,173 : REM doble buffer ON, front = bloque 5 (168+5)

Con el comando equivalente:

LOAD \*DBUF 5 : REM doble buffer ON, front = bloque 5

LOAD \*DBUF STOP : REM doble buffer OFF

**Control por puerto de E/S (pseudo-bloque 8):** el \`POKE 2057\` deja de funcionar si el programa ha desactivado la ventana de POKEs de control escribiendo en 2056 (modo \"RAM plana\", p. ej. CP/M). Para esos casos el doble buffer también se controla a través del **mapper port (E7h)** usando el bloque ficticio 8:

asm

ld a,08h ; pseudo-bloque 8

ld b,32+5 ; valor: bit5=activar, bits2:0=bloque front (aquí 5)

ld c,0e7h

out (c),a ; doble buffer ON, front = bloque 5

ld b,0 ; valor 0 = desactivar

out (c),a

Esta vía está disponible cuando el modo **full paging** está activo o cuando se ha escrito en 2056. Restricción: en half paging tras 2056, no asignes una página impar al bloque 0 con el mapper port (ese patrón de datos, x8h, coincide con el pseudo-bloque 8).

**Modo MANUAL: doble buffer sin copia automática**

Además del modo AUTO descrito arriba (con blit automático en cada VSYNC), existe un **modo MANUAL** en el que la FPGA no copia nada: el propio Z80 dibuja el frame completo directamente en el bloque que en cada momento no es el front, y solo conmuta cuál de los dos bloques se muestra. Se ahorra el coste de la copia automática (\~630 µs por frame), a cambio de que el redibujado completo corre por cuenta del Z80 (viable en modos Superfast, donde la CPU está libre de refresco de vídeo). A diferencia del modo AUTO, aquí no hay un único HFILE que la FPGA copie: hacen falta dos bloques reales (por ejemplo 4 y 5).

POKE 2057, 200+B : REM doble buffer ON modo MANUAL; front = bloque B (0-7)

Los valores 168+B (modo AUTO) y 200+B (modo MANUAL) comparten el mismo POKE 2057; usa 85 para desactivar en ambos casos.

**Uso típico:**

1\. Elige dos bloques reales para los dos buffers (p. ej. 4 y 5). HFILE deja de ser relevante para este mecanismo mientras el doble buffer esté activo.

2\. Actívalo con el bloque inicial como front (POKE 2057, 200+4) y dibuja el primer frame en el otro bloque (5).

3\. En cada frame: dibuja el frame completo en el bloque que no es el front actual; espera VSYNC (bit 0 del puerto AFh); conmuta el front con POKE 2057, 200+B (B = el bloque que acabas de pintar).

4\. En el frame siguiente, pinta el bloque que ha quedado libre (el antiguo front).

La máscara de escritura de la FPGA sigue protegiendo automáticamente al bloque front frente a escrituras de la CPU, en ambos modos.

**Por puerto de E/S (pseudo-bloque 8):** igual que en modo AUTO, pero con un bit adicional (bit4 del valor B) para seleccionar MANUAL:

ld a,08h ; pseudo-bloque 8

ld b,32+16+5 ; bit5=activar, bit4=modo MANUAL, bits2:0=bloque front (aqui 5)

ld c,0e7h

out (c),a ; doble buffer ON modo MANUAL, front = bloque 5

Consulta el ejemplo completo en código máquina en \`EXAMPLES/DBUF/\` (pelota rebotando con conmutación del doble buffer en tiempo real).

## WRX con la RAM de 8-16K

Algunos programas de alta resolución WRX colocan sus gráficos en la zona de **8-16K** (\$2000-\$3FFF) y apuntan ahí el registro I (por ejemplo, **Hi-res Chess** de Psion). En un ZX81 real esto requiere una ampliación de RAM en esa zona; en el SD81 Booster esa región existe, pero por defecto el interface la trata como **generador de caracteres en RAM** (la función de LOAD \*128C y los juegos con caracteres definibles), que necesita justo el comportamiento contrario durante el refresco de vídeo. Son dos usos legítimos de la misma zona y no es posible distinguirlos automáticamente, así que se selecciona con el comando:

LOAD \*WRX : REM activar modo WRX en 8-16K

LOAD \*WRX STOP : REM desactivar (modo generador de caracteres, por defecto)

(equivalente a POKE 2058,170 / POKE 2058,85, por si se prefiere desde código máquina). Activa el modo WRX **antes de cargar** el programa (por ejemplo LOAD \*WRX y después LOAD \"HRCHESS\"). Tras un reset vuelve al modo por defecto. No afecta a los programas WRX que sitúan sus gráficos en \$4000 o superior, que funcionan siempre sin necesidad de este comando.

## Resumen de POKEs de control

| **Dirección** | **Valor** | **Función** |
|------------|----------|--------------------------------------------------|
| 2043 | \<bajo\> | Parte baja de la dirección del fichero de pantalla |
| 2044 | \<alto\> | Parte alta de la dirección del fichero de pantalla |
| 2045 | 170 | Activar Superfast texto |
| 2045 | 171 | Activar Superfast HiRes nativo |
| 2045 | 172 | Activar Superfast HiRes Spectrum |
| 2045 | 85 | Desactivar Superfast |
| 2046 | \<attr\> | Cambiar atributos del borde |
| 2047 | 170 | Activar patrón de borde |
| 2047 | 85 | Desactivar patrón de borde |
| 2048--2055 | \<datos\> | Definir patrón de borde (8 bytes) |
| 2056 | \- | Desactiva los pokes de control y activa la escritura en el bloque 0 |
| 2057 | 168+B | Activar doble buffer (front buffer = bloque B, 0-7) |
| 2057 | 200+B | Activar doble buffer modo MANUAL (front buffer = bloque B, 0-7) |
| 2057 | 85 | Desactivar doble buffer |
| 2058 | 170 | Activar WRX en la RAM de 8-16K |
| 2058 | 85 | Desactivar WRX (modo generador de caracteres) |

# Apéndice G --- Referencia técnica de audio: chip AY, VGM y alófonos

## Registros del chip AY-3-8910/12

El emulador AY del SD81 Booster es compatible a nivel de registro con el chip original. Soporta tres voces independientes, envolvente y ruido.

| **Reg** | **Descripción** | **B7** | **B6** | **B5** | **B4** | **B3** | **B2** | **B1** | **B0** |
|-----|---------------------|:--:|:--:|:-----:|:-----:|:-----:|:-----:|:-----:|:-----:|
| **R0** | Canal A --- período tono (bajo) | B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
| **R1** | Canal A --- período tono (alto) | --- | --- | --- | --- | B3 | B2 | B1 | B0 |
| **R2** | Canal B --- período tono (bajo) | B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
| **R3** | Canal B --- período tono (alto) | --- | --- | --- | --- | B3 | B2 | B1 | B0 |
| **R4** | Canal C --- período tono (bajo) | B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
| **R5** | Canal C --- período tono (alto) | --- | --- | --- | --- | B3 | B2 | B1 | B0 |
| **R6** | Período de ruido | --- | --- | --- | B4 | B3 | B2 | B1 | B0 |
| **R7** | Habilitación de canales | --- | --- | Ruido C | Ruido B | Ruido A | Tono C | Tono B | Tono A |
| **R8** | Amplitud canal A | --- | --- | --- | Env. | L3 | L2 | L1 | L0 |
| **R9** | Amplitud canal B | --- | --- | --- | Env. | L3 | L2 | L1 | L0 |
| **R10** | Amplitud canal C | --- | --- | --- | Env. | L3 | L2 | L1 | L0 |
| **R11** | Período envolvente (bajo) | B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
| **R12** | Período envolvente (alto) | B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
| **R13** | Forma de envolvente | --- | --- | --- | --- | --- | B2 | B1 | B0 |

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *En R7, un bit a 0 activa el canal; a 1 lo desactiva. En R8--R10, si el bit Env. está activo, la amplitud la controla la envolvente (R11--R13) en lugar de L3--L0.* |

## Puertos de E/S --- dos chips AY compatibles ZonX-81

El SD81 Booster implementa dos chips AY físicos, compatibles con el interface estándar ZonX-81, decodificados de forma parcial: solo se comprueban los bits A1, A2, A3, A5 y A7 de la dirección; los bits A0, A4 y A6 son indiferentes.

| **Chip** | **Selección de registro (latch)** | **Escritura de dato** |
|----------------------------|----------------------|---------------|
| Chip A (ZonX-81 estándar, A3=1) | \$CFh / \$DFh | \$0Fh / \$1Fh |
| Chip B (extensión SD81 Booster, A3=0) | \$C6h | \$06h |

El bit A7 hace de línea BC1 del AY: durante una escritura, A7=1 selecciona el registro (latch de dirección) y A7=0 escribe el dato en el registro ya seleccionado. La lectura de estado del PSG (BC1=1, BDIR=0) está implementada en la FPGA pero no expuesta actualmente por el firmware/BASIC.

## Opcodes del reproductor VGM

El reproductor VGM del interface solo interpreta los opcodes referentes al chip AY. El resto se ignoran sin producir error.

| **Opcode** | **Parámetros** | **Descripción** |
|---------|------------|---------------------------------------------------|
| **61h** | nn nn | Esperar n muestras (little-endian, 0--65535; ≈ 1,49 s máximo). Pausas largas se representan con varios comandos consecutivos. |
| **62h** | --- | Esperar 735 muestras (1/60 de segundo). Equivale a 61h DFh 02h. |
| **63h** | --- | Esperar 882 muestras (1/50 de segundo). Equivale a 61h 72h 03h. |
| **A0h** | aa dd | Escribir el valor dd en el registro AY número aa. |

## Tabla de alófonos SP0256-AL2

Los alófonos se usan con el comando MCU BINARY SAY (16h) para síntesis fonética directa de precisión. El acceso de alto nivel mediante texto (LOAD \*SAY \"texto\") no requiere conocer estos códigos.

| **Código** | **Alófono** | **Ejemplo** | **Código** | **Alófono** | **Ejemplo** |
|----------|----------|-----------------|----------|----------|-----------------|
| **\$00** | PA1 | pausa 10 ms | **\$20** | AW | out |
| **\$01** | PA2 | pausa 30 ms | **\$21** | DD2 | do |
| **\$02** | PA3 | pausa 50 ms | **\$22** | GG3 | wig |
| **\$03** | PA4 | pausa 100 ms | **\$23** | VV | vest |
| **\$04** | PA5 | pausa 200 ms | **\$24** | GG1 | got |
| **\$05** | OY | boy | **\$25** | SH | ship |
| **\$06** | AY | sky | **\$26** | ZH | azure |
| **\$07** | EH | end | **\$27** | RR2 | brain |
| **\$08** | KK3 | comb | **\$28** | FF | food |
| **\$09** | PP | pow | **\$29** | KK2 | sky |
| **\$0A** | JH | dodge | **\$2A** | KK1 | can\'t |
| **\$0B** | NN1 | thin | **\$2B** | ZZ | zoo |
| **\$0C** | IH | sit | **\$2C** | NG | anchor |
| **\$0D** | TT2 | to | **\$2D** | LL | lake |
| **\$0E** | RR1 | rural | **\$2E** | WW | wool |
| **\$0F** | AX | succeed | **\$2F** | XR | repair |
| **\$10** | MM | milk | **\$30** | WH | whig |
| **\$11** | TT1 | part | **\$31** | YY1 | yes (corto) |
| **\$12** | DH1 | they | **\$32** | CH | church |
| **\$13** | IY | see | **\$33** | ER1 | fir (corto) |
| **\$14** | EY | beige | **\$34** | ER2 | fir (largo) |
| **\$15** | DD1 | could | **\$35** | OW | beau |
| **\$16** | UW1 | too | **\$36** | DH2 | they |
| **\$17** | AO | aught | **\$37** | SS | vest |
| **\$18** | AA | hot | **\$38** | NN2 | no |
| **\$19** | YY2 | yes (largo) | **\$39** | HH2 | hoe |
| **\$1A** | AE | hat | **\$3A** | OR | store |
| **\$1B** | HH1 | he | **\$3B** | AR | alarm |
| **\$1C** | BB1 | business (corto) | **\$3C** | YR | clear |
| **\$1D** | TH | thin | **\$3D** | GG2 | guest |
| **\$1E** | UH | book | **\$3E** | EL | saddle |
| **\$1F** | UW2 | food | **\$3F** | BB2 | business (largo) |

*Manual de Usuario SD81 Booster v1.0 --- Hardware y software de código abierto*
