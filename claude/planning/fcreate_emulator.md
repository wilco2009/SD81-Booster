# Especificación para el emulador: fcreate (71/72) y dos ajustes de fwrite y SAVE

El editor de textos EDIT (`EXAMPLES/EDIT`, se abre con la tecla E del
explorador) guarda con los comandos de ficheros del MCU. Hacía falta uno
para crear ficheros: `fopen` (53/58) solo abre los que ya existen, y
`fwrite` sobre un fichero existente no lo trunca. El firmware 2.6 añade
**fcreate**. Sin él en el emulador, EDIT abre y edita, pero al guardar
dice "Can't create the file".

Referencia: `Arduino/SD81BoosterV2_039_STM32/COMMANDS.cpp`, `do_f_open()`
(ahora con el parámetro `create`), `cmd_f_create()`,
`cmd_f_create_zx81()` y `cmd_f_write()`.

## 1. fcreate: comandos 71 (0x47) y 72 (0x48)

Igual que fopen (53/0x35 y 58/0x3A), con el mismo protocolo:

```
Z80 -> MCU:  cmd, len, nombre[len]
MCU -> Z80:  handle (0..3, o 0xFF si no se puede)
```

- **71 (0x47)**: nombre en ASCII tal cual, como 53.
- **72 (0x48)**: nombre en código ZX81, que se convierte a ASCII, como 58.
- Diferencia con fopen: crea el fichero si no existe y **lo deja vacío si
  ya existía**. En C sería `fopen(ruta, "w+b")`; en el STM32,
  `O_RDWR|O_CREAT|O_TRUNC`. El handle queda abierto para `fwrite` (56),
  `fseek` (54), `fread` (55) y `fclose` (57), como uno de fopen.
- La ruta se resuelve igual que en fopen: relativa al directorio actual
  del SD, salvo que empiece por `/`.
- Si el directorio actual es un `.T81` (de solo lectura), devuelve 0xFF.
- Sin handles libres (4 como máximo), 0xFF.

En `SD81Booster.cpp` basta con copiar lo de 0x35/0x3A: el `case` de la
recepción del comando (`StartRecvStr`, con `m_recvRaw = true` para 0x47) y
el del final (`case 0x3A: case 0x35:` → abrir con `"w+b"` en vez de
`"r+b"`, sin la comprobación de que exista).

## 2. fwrite con más de 512 bytes

El STM32 recibe los datos de `fwrite` en `copy_buffer`, de 512 bytes, y no
comprobaba `count`: con más de 512 se salía del buffer. Ahora recibe todos
los bytes, para no perder el paso con el Z80, pero descarta lo que pase de
512, no escribe nada y devuelve el estado **0xFF**.

En el emulador (`FWRITE_RECV_DATA`) se escribe lo que llegue. Para que se
comporte igual: si `m_fCount > 512`, recibir los bytes igualmente y
devolver 0xFF sin escribir. Ningún programa debería mandar más de 512 (el
explorador, CP/M y EDIT mandan como mucho 512), así que no es urgente.

## 3. SAVE de 0 bytes

`cmd_save` (comando 10) devolvía el error 1 con 0 bytes, aunque dejaba el
fichero creado y vacío. Ahora 0 bytes es válido: fichero vacío y estado 0.
En el emulador ya se comporta así (`RECV_SAVE_SIZE_HI`, `m_saveSize == 0`
→ `SaveFile` con los datos vacíos y `err = 0`), así que no hay nada que
cambiar. Lo apunto para que conste que ahora coinciden.

## Cómo probarlo

1. Con EDIT (`EXAMPLES/EDIT/compila.bat` lo copia a `SD81\explorer`) y el
   explorador nuevo, pulsar E sobre un `.TXT`, cambiar algo y ^K S. Debe
   salir "Saved" y el fichero debe cambiar.
2. Borrar líneas hasta que el texto sea más corto que antes y guardar: el
   fichero tiene que quedar con el tamaño nuevo, sin la cola del texto
   anterior (eso es lo que no se podía hacer solo con fopen + fwrite).
3. Desde el prompt, con el stub `EDIT.B81`: sin `F$` se guarda con ^K S
   pidiendo el nombre, y crea el fichero.
