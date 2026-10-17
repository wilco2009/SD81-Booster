#!/usr/bin/env python3
"""SD81 Booster - make_dict.py

Convierte una lista de palabras (una por linea) en el diccionario binario que busca el Z80 con una
busqueda binaria directamente en la SD (EXAMPLES/DICTTEST, y despues el Scrabble).

Formato (todo en registros de 16 bytes, para que el Z80 no tenga que analizar texto):
    registro 0      cabecera: "SDICT01" + 0, el numero de palabras N (4 bytes, little-endian) y ceros
    registros 1..N  una palabra cada uno, en MAYUSCULAS, de 2 a 15 letras, rellena con ceros (el 0 es
                    menor que cualquier letra, asi que "CAT" va antes que "CATS"), sin repetidas y
                    ordenadas por sus bytes
La palabra n esta en el desplazamiento 16 * (n + 1) del fichero.

Uso:
    python make_dict.py enable1.txt DICT.DIC                  # una lista real (solo A-Z; el resto se descarta)
    python make_dict.py --synthetic 500000 DICT.DIC           # N palabras al azar: para medir tiempos
    opciones: --minlen 2  --maxlen 15  --seed 1

Alfabeto: por ahora solo A-Z (ingles). Para el castellano se anadira la N con tilde y se decidira que bytes
usan los digrafos: el orden que importa es el de los bytes.
"""
import argparse
import random
import string
import struct
import sys

REC = 16
MAGIC = b"SDICT01\0"


def load_words(path, minlen, maxlen):
    words = set()
    dropped = 0
    with open(path, encoding="latin-1") as f:
        for line in f:
            w = line.strip().upper()
            if not (minlen <= len(w) <= maxlen) or not all("A" <= c <= "Z" for c in w):
                dropped += 1
                continue
            words.add(w.encode("ascii"))
    return sorted(words), dropped


def synthetic(n, minlen, maxlen, seed):
    rng = random.Random(seed)
    words = set()
    while len(words) < n:
        k = rng.randint(minlen, maxlen)
        words.add("".join(rng.choice(string.ascii_uppercase) for _ in range(k)).encode("ascii"))
    return sorted(words)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("paths", nargs="+", help="lista de palabras y fichero de salida (con --synthetic, solo el de salida)")
    ap.add_argument("--synthetic", type=int, metavar="N", help="en vez de una lista, N palabras al azar")
    ap.add_argument("--minlen", type=int, default=2)
    ap.add_argument("--maxlen", type=int, default=15)
    ap.add_argument("--seed", type=int, default=1)
    a = ap.parse_args()
    if a.maxlen > REC - 1:
        ap.error("maxlen no puede pasar de 15")
    if a.synthetic:
        if len(a.paths) != 1:
            ap.error("con --synthetic solo se da el fichero de salida")
        dst = a.paths[0]
        words = synthetic(a.synthetic, a.minlen, a.maxlen, a.seed)
        dropped = 0
    else:
        if len(a.paths) != 2:
            ap.error("hacen falta la lista de palabras y el fichero de salida")
        dst = a.paths[1]
        words, dropped = load_words(a.paths[0], a.minlen, a.maxlen)
    with open(dst, "wb") as f:
        f.write(MAGIC + struct.pack("<I", len(words)) + bytes(REC - 12))
        for w in words:
            f.write(w + bytes(REC - len(w)))
    size = REC * (len(words) + 1)
    print(f"{dst}: {len(words)} palabras, {size} bytes ({size / 1024 / 1024:.2f} MB), {dropped} descartadas", file=sys.stderr)


if __name__ == "__main__":
    main()
