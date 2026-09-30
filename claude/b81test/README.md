# Pruebas del conversor .B81

`B81.cpp` (firmware del STM32) no depende de Arduino y reproduce el cargador
de listados de EightyOne (`IBasicLoader` + `zx81BasicLoader`) con sus
opciones por defecto. Aquí se compila en el PC junto al de EightyOne y se
comparan los `.P` que salen:

```
sh compara.sh casos.b81 ../../EXAMPLES/SD81TEST/SD81TEST.B81
```

- `ref_main.cpp`: el cargador de EightyOne (fuentes en `EIGHTYONE_SRC`,
  por defecto `C:\ClaudeCode\Eightyone2\src`). Como el SD81, solo arranca
  el programa si hay `#!basic-start=N`.
- `my_main.cpp`: `B81.cpp` leyendo de un fichero.
- `lister.py`: listador `.P` → `.B81` como el de EightyOne, para sacar
  listados de programas reales (`python lister.py PROG.P PROG.B81`).
- `dump.py`: muestra las líneas de un `.P` en hexadecimal.
- `casos.b81`: etiquetas, continuación de línea, escapes, comillas,
  `LOAD *128C`, `LOAD *MAP 7,63` y `#!basic-start`.

Con los 166 programas de la carpeta del emulador salieron 164 `.P`
idénticos y 2 errores iguales en los dos (números de línea repetidos).
Las diferencias a propósito con EightyOne están al principio de `B81.cpp`.
