# DICTTEST: ¿puede el Z80 buscar una palabra en un diccionario de la SD?

Ensayo para el Scrabble: una **búsqueda binaria hecha por el Z80** en un fichero de la SD, usando solo los comandos de fichero de
siempre (`fopen` 53, `fseek` 54, `fread` 55, `fclose` 57), **sin ninguna orden nueva del MCU**. Cada paso de la búsqueda es un `fseek`
y un `fread` de 16 bytes. El programa mide cuánto tarda con la hora del MCU (la de `LOAD *RTC`, al centisegundo).

## El diccionario

`TOOLS/make_dict.py` lo genera a partir de una lista de palabras (una por línea):

```
python TOOLS/make_dict.py enable1.txt DICT.DIC           # una lista real (ENABLE, inglés, dominio público: 168.551 palabras, 2,6 MB)
python TOOLS/make_dict.py --synthetic 500000 DICT.DIC    # 500.000 palabras al azar, 7,6 MB: el tamaño de un diccionario en castellano
```

Formato: registros de **16 bytes**. El registro 0 es la cabecera (`SDICT01`, 0, el número de palabras en 4 bytes); los demás, una palabra cada
uno, en mayúsculas, de 2 a 15 letras, rellena con ceros, sin repetidas y ordenadas por sus bytes (el 0 es menor que cualquier letra, así
`CAT` va antes que `CATS`). La palabra n está en el desplazamiento `16 * (n + 1)`: no hay que analizar texto.

## Probarlo en la placa

1. Ensambla con `compila.bat` (o `pasmo dicttest.asm dicttest.bin`).
2. En una carpeta de la SD pon `dicttest.bin`, `DICTTEST.B81` y un `DICT.DIC` (en `build\dict` están `DICT.DIC`, el de ENABLE, y
   `DICT500K.DIC`, el sintético; renómbralo a `DICT.DIC` para probarlo).
3. `LOAD "DICTTEST.B81"` y `RUN`. Imprime el número de palabras, cuántas de las 32 búsquedas han salido bien, los pasos y el tiempo
   (`MS PER WORD`, `MS PER STEP`).

Qué hace: elige 16 palabras repartidas por todo el fichero y las mismas con un `~` detrás (que no pueden estar), y busca las 32. Las 16
primeras tienen que encontrarse y las otras no (`FOUND OK 16/16`, `ABSENT OK 16/16`).

## Qué esperar (PC)

`claude/mqtttest/test_z80dict.cpp` (con `build_z80.sh`): el programa de verdad en el Z80 de EightyOne, con la SD simulada y una referencia
en C++: encuentra todas las palabras, descarta las que no están, extremos, prefijos y 300.000 palabras. En el PC, el coste **del Z80** es de
unas 7.000 a 9.000 instrucciones por búsqueda (unos 10-13 ms a 3,25 MHz); el resto (la espera del MCU y de la SD en cada paso) solo se
mide en la placa, y es lo que decide si hace falta algo más (por ejemplo, tener en RAM los primeros niveles de la búsqueda).

## Entradas del programa (se carga en `$6000`)

| Dirección | Qué hace |
|---|---|
| 24576 | `run_test`: la prueba de tiempos; resultados en `$7C00` para el BASIC |
| 24579 | `dict_open`: abre `DICT.DIC` y lee la cabecera (BC = 0, o error 1 sin fichero, 2 no es un diccionario, 3 vacío) |
| 24582 | `dict_lookup`: busca la palabra de `$7C40` (16 bytes, mayúsculas, con ceros): BC = 1 si está |
