# Diccionarios del Scrabble: cómo generarlos / How to build the Scrabble dictionaries

El Scrabble (`scrabble.bin`) comprueba cada palabra en un diccionario de la SD. **No se distribuyen con el programa**: se generan una sola
vez a partir de listas de palabras públicas con `TOOLS/make_dict.py`. Hacen falta dos ficheros, con **estos nombres exactos**, en la
**misma carpeta de la SD que el juego** (`SCRABBLE.P` y `scrabble.bin`):

| Fichero | Idioma | Tamaño | Lista de origen |
|---|---|---|---|
| `DICTEN.DIC` | inglés | 2,6 MB (168.551 palabras) | **ENABLE** (`enable1.txt`) |
| `DICTES.DIC` | castellano | 9,7 MB (636.599 palabras) | **FISE-2** (`fise-2.txt`) o **FILE 2017** |

Si solo vas a jugar en un idioma, basta el diccionario de ese idioma. Si falta, el juego avisa con `NO DICTIONARY FILE IN THE SD`.

## Dónde están las listas (los repositorios originales)

- **Inglés — ENABLE** (Enhanced North American Benchmark Lexicon, lista de dominio público):
  <https://github.com/dolph/dictionary> (el fichero `enable1.txt`).
- **Castellano — Lexicón FISE-2 (2009) y Lexicón FILE 2017**, las listas oficiales de la Federación Internacional de Scrabble en
  Español y de la Federación Internacional de Léxico en Español (<https://www.filexico.com/>), recogidas por Kamil Mielnik en
  <https://github.com/kamilmielnik/scrabble-dictionaries/tree/master/spanish> (`fise-2.txt` y `file-2017.txt`). Traen las formas
  conjugadas y los plurales, en minúscula, sin tildes y con la ñ.

> **Licencia.** Ese repositorio no declara ninguna licencia para las listas del castellano y estas son léxicos oficiales de las federaciones
> (FISE-2 viene de redeletras.com). Por eso **este proyecto no las incluye ni las redistribuye**: descárgalas tú de su origen y consulta sus
> condiciones. Aquí solo se publican el convertidor y estas instrucciones.

## Pasos

Hace falta Python 3 y el fichero `TOOLS/make_dict.py` de este repositorio.

**1. Descargar las listas** (a una carpeta de trabajo):

```
# Windows (PowerShell)
curl.exe -L -o enable1.txt https://raw.githubusercontent.com/dolph/dictionary/master/enable1.txt
curl.exe -L -o fise-2.txt  https://raw.githubusercontent.com/kamilmielnik/scrabble-dictionaries/master/spanish/fise-2.txt

# Linux / macOS
curl -L -o enable1.txt https://raw.githubusercontent.com/dolph/dictionary/master/enable1.txt
curl -L -o fise-2.txt  https://raw.githubusercontent.com/kamilmielnik/scrabble-dictionaries/master/spanish/fise-2.txt
```

**2. Convertirlas:**

```
python TOOLS/make_dict.py enable1.txt DICTEN.DIC
python TOOLS/make_dict.py --lang es fise-2.txt DICTES.DIC
```

Cada orden termina con una línea como `DICTES.DIC: 636599 palabras, 10185600 bytes (9.71 MB), 0 descartadas`. Con FISE-2 no se descarta
ninguna palabra; con enable1 se descartan unas 4.000 (las de menos de 2 o más de 15 letras y las que no son A-Z). Con `file-2017.txt` en vez de
`fise-2.txt` sale `DICTES.DIC` con 639.291 palabras (una se descarta: tiene 20 fichas).

**3. Copiar** `DICTEN.DIC` y/o `DICTES.DIC` a la carpeta del juego en la SD.

## Castellano con una lista de corrector ortográfico (Hunspell)

El convertidor también acepta listas de un corrector ortográfico, con tildes, nombres propios en mayúscula, entradas `palabra/S|` (plural sin
formar) y, sobre todo, **todas las formas verbales con pronombres pegados** (`dámelo`, `lavándose`...), que el Scrabble no admite (FISE-2 no
lleva ninguna). La lista usada en las pruebas viene del diccionario ortográfico de castellano de Hunspell (`dictionaries/es` en
<https://github.com/wooorm/dictionaries>), © Santiago Bosio, con licencia **GPL-3+ o LGPL-3+ o MPL-1.1+** (a elegir). La lista de las pruebas es una
versión derivada (palabras ya conjugadas y plurales formados, con algunos defectos corregidos a mano). Para una lista así:

```
python TOOLS/make_dict.py --lang es --no-enclitics --keep TOOLS/dict_es_keep.txt --drop TOOLS/dict_es_drop.txt lista.txt DICTES.DIC
```

- Se descartan las líneas con mayúsculas, se expanden los `/S|` y se quitan las tildes (la ñ se conserva).
- `--no-enclitics` quita los enclíticos: detecta una forma con pronombre pegado cuyo tema existe en la lista y admite pronombres dobles
  (`-melo`, `-selo`...), más los gerundios y el voseo. Los infinitivos reflexivos (`lavarse`, `arrepentirse`) **se conservan**. Es una
  heurística: contrastada con FISE-2 quitaría 978 palabras válidas (`parte`, `regalos`, `tranquilo`...), que se recuperan con
  `--keep` (`TOOLS/dict_es_keep.txt`); quedan sin detectar unos miles de enclíticos de raíz irregular.
- `--drop` quita siempre las palabras de `TOOLS/dict_es_drop.txt`: abreviaturas (`km`, `etc`, `pag`), extranjerismos dudosos, interjecciones y
  enclíticos cortos (`dale`, `irse`), validados a mano.
- Con la lista usada en las pruebas salen unas 562.800 palabras (8,6 MB).

## Qué hace el convertidor

- **Formato del `.DIC`:** registros de 16 bytes. El primero es la cabecera (`SDICT01`, el número de palabras) y los demás, una palabra cada
  uno, en mayúsculas, rellena con ceros y **ordenada por sus bytes**, para que el Z80 la busque con una búsqueda binaria directamente en la
  SD (unas 19 lecturas por palabra, unos 40 ms en la placa).
- **Castellano:** cada *ficha* es un byte. **CH, LL y RR** son siempre una sola ficha (como en el Scrabble en castellano: no se pueden hacer con
  C+H, L+L o R+R), así que se trocean las palabras de forma voraz; la **ñ** es otra ficha. Los límites (2 a 15) se cuentan en fichas. Todo lo
  que tenga otras letras (tildes, guiones...) se descarta.
- **Otras listas:** vale cualquier lista con una palabra por línea (inglés: solo A-Z; castellano: UTF-8, minúsculas, sin tildes). Con
  `--minlen`, `--maxlen` se cambian los límites y `--synthetic N` genera N palabras al azar para medir tiempos.

---

## English summary

`scrabble.bin` checks every word against a dictionary file on the SD card. The dictionaries are **not distributed** with the program:
build them once with `TOOLS/make_dict.py` and copy `DICTEN.DIC` (English) and/or `DICTES.DIC` (Spanish) to the game's folder.

- English: the public-domain **ENABLE** word list, <https://github.com/dolph/dictionary> (`enable1.txt`).
- Spanish (free licence): the Hunspell Spanish spelling dictionary, <https://github.com/wooorm/dictionaries> (`dictionaries/es`,
  © Santiago Bosio, GPL-3+ / LGPL-3+ / MPL-1.1+). Expand it to a plain word list and convert it with
  `--lang es --no-enclitics --keep TOOLS/dict_es_keep.txt --drop TOOLS/dict_es_drop.txt` (see the Spanish section above).
- Spanish: the official **FISE-2** (2009) / **FILE 2017** word lists, collected in
  <https://github.com/kamilmielnik/scrabble-dictionaries/tree/master/spanish>. That repository declares no license for them, so they
  are not redistributed here: download them from their source and check their terms.

```
python TOOLS/make_dict.py enable1.txt DICTEN.DIC
python TOOLS/make_dict.py --lang es fise-2.txt DICTES.DIC
```
