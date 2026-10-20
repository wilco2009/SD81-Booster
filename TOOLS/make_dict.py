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
    python make_dict.py --lang es fise-2.txt DICTES.DIC       # castellano (lista en UTF-8, sin tildes, con la n con tilde)
    python make_dict.py --lang es --no-enclitics hunspell.txt DICTES.DIC   # lista que trae damelo, lavandose... (se quitan)
    opciones: --minlen 2  --maxlen 15  --seed 1  --lang en|es  --no-enclitics

Alfabeto: en ingles, A-Z. En castellano (--lang es) cada FICHA es un byte: A-Z, la N con tilde = '[', CH = backslash,
LL = ']' y RR = '^' (los bytes que usa EXAMPLES/NETGAME/scrabble.inc). Como en el Scrabble en castellano, CH, LL y RR
son siempre una sola ficha (no se pueden hacer con C+H, L+L o R+R), asi que se trocea la palabra en fichas de forma
voraz. Los limites de longitud (2 a 15) son en FICHAS. Se descarta lo que tenga otras letras (tildes, guiones...).
El orden que importa es el de los bytes.
"""
import argparse
import random
import string
import struct
import sys

REC = 16
MAGIC = b"SDICT01\0"


def tiles_en(line):
    w = line.strip().upper()
    return w.encode("ascii") if w and all("A" <= c <= "Z" for c in w) else None


DIGRAPHS = {"CH": chr(92), "LL": "]", "RR": "^"}


def tiles_es(line):
    w = line.strip().upper()
    out = []
    i = 0
    while i < len(w):
        if w[i:i + 2] in DIGRAPHS:
            out.append(DIGRAPHS[w[i:i + 2]])
            i += 2
            continue
        c = w[i]
        if c == "Ñ":
            out.append("[")
        elif "A" <= c <= "Z":
            out.append(c)
        else:
            return None
        i += 1
    return "".join(out).encode("ascii") if out else None


# --- Formas verbales con pronombres enclíticos (dámelo, lavándose...) -----------------------------
# El Scrabble en castellano no las admite (FISE-2 no lleva ninguna), pero listas como las de Hunspell las
# generan todas (un tercio de la lista). Se detectan sin diccionario de verbos: un tema que parece una forma
# verbal (infinitivo, gerundio, imperativo) con el que el propio listado trae pronombres dobles (-melo, -selo...).
# Sobre FISE-2 marca 33 palabras de 636.599 (falsos positivos); la lista se pasa ya sin tildes ni mayúsculas.
_PRON = ["me", "te", "se", "nos", "os", "lo", "la", "los", "las", "le", "les"]
_PRON2 = [a + b for a in ("me", "te", "se", "nos", "os", "le", "les") for b in ("lo", "la", "los", "las")]
_PSUF = sorted(set(_PRON + _PRON2), key=len, reverse=True)
_SHAPES = ("ar", "er", "ir", "ndo", "ad", "ed", "id", "a", "e", "an", "en", "amos", "emos", "imos")
_DBL = ["melo", "mela", "melos", "melas", "telo", "tela", "selo", "sela", "selos", "selas", "noslo", "nosla", "oslo", "osla"]


_ACC = str.maketrans("áéíóúü", "aeiouu")


def strip_accents(w):
    return w.translate(_ACC)


def plural(w):
    """Plural regular, para las entradas marcadas /S| de la lista."""
    if w.endswith("z"):
        return w[:-1] + "ces"
    if w.endswith(("ón", "án", "én", "ín", "ún")):
        return w[:-2] + strip_accents(w[-2]) + "nes"
    return w + ("s" if w[-1] in "aeiouáéíóú" else "es")


def _stems(w):
    for p in _PSUF:
        if len(w) < len(p) + 3 or not w.endswith(p):
            continue
        s = w[:-len(p)]
        yield s, p
        if p[:3] == "nos" or p[:2] == "se":      # vamonos = vamos + nos (se pierde la s)
            yield s + "s", p
        if p[:2] == "os":                        # sentaos = sentad + os (se pierde la d)
            yield s + "d", p


def is_enclitic(raw, words, rec=True):
    """raw: la palabra como viene en la lista (puede llevar tilde); words: todas las palabras sin tildes."""
    w = strip_accents(raw)
    for p in _PSUF:
        # gerundio + pronombre (lavandose, preveniendolas): con tilde no hay palabras normales asi
        if w != raw and w.endswith(p) and w[:-len(p)].endswith(("ando", "iendo", "yendo")) and len(w) > len(p) + 5:
            return True
        # voseo (servila, erigilo, presentime): tema en -i que admite casi todos los pronombres
        if w.endswith(p) and len(w) > len(p) + 3 and w[-len(p) - 1] == "i":
            s = w[:-len(p)]
            if s not in words and sum(1 for q in ("me", "te", "se", "nos", "os", "le", "les") if s + q in words) >= 2:
                return True
    if w != raw:
        # con tilde (dámelo, lavándose, mudémonos): el tema, sin tilde, existe y parece una forma verbal
        for p in _PSUF:
            if not (w.endswith(p) and len(w) > len(p) + 2):
                continue
            s = w[:-len(p)]
            for c in (s, s + "s", s + "d"):
                if c not in words or not c.endswith(_SHAPES):
                    continue
                if c.endswith(("ar", "er", "ir")) and p == "se":     # lavarse: infinitivo reflexivo
                    continue
                if c.endswith("ndo") or (c.endswith("mos") and p in ("nos", "se")):
                    return True
                if sum(1 for q in _DBL if c + q in words) >= 1 or sum(1 for q in _PRON if c + q in words) >= 3:
                    return True
        return False
    for p in ("se", "me", "te", "nos", "os"):
        if w.endswith("ando" + p) and len(w) >= 9:
            return True
    for p in _PSUF:
        if w.endswith(p) and w[:-len(p)].endswith(("iendo", "yendo")) and len(w) > len(p) + 5:
            return True
    for s, p in _stems(w):
        if s not in words or not s.endswith(_SHAPES):
            continue
        if p == "se" and s.endswith(("ar", "er", "ir")):      # lavarse: infinitivo reflexivo, palabra del lexico
            continue
        if p == "se" and s.endswith("a"):                     # ensuciase: subjuntivo
            continue
        if s.endswith(("an", "en")) and p[:2] in ("te", "os"):  # cortante, culminante
            continue
        if s.endswith(("ad", "ed", "id")) and p in ("os", "se", "te"):
            continue
        if s.endswith("mos") and p in ("nos", "se") and len(s) >= 5:
            return True
        # plural de otra palabra (formales, tiranos): no es un enclitico
        if w.endswith("s") and w[:-1] in words and not (rec and len(w) > 6 and is_enclitic(w[:-1], words, False)):
            continue
        if sum(1 for q in _DBL if (s + q) in words) >= 2:
            return True
        if s.endswith(("an", "en")) and sum(1 for q in _PRON if (s + q) in words) >= 5:
            return True
    return False


def load_words(path, minlen, maxlen, lang="en", no_enclitics=False, keep=(), drop=()):
    words = set()
    dropped = 0
    conv = tiles_es if lang == "es" else tiles_en
    encoding = "utf-8" if lang == "es" else "latin-1"
    with open(path, encoding=encoding) as f:
        lines = [ln.strip() for ln in f]
    if lang == "es":
        # Listas de un corrector ortografico: palabras con tilde, nombres propios en mayuscula y entradas "palabra/S|"
        # (plural regular sin formar). Solo valen las minusculas; las tildes se quitan (el Scrabble no las usa).
        out = []
        for ln in lines:
            if ln.endswith("/S|"):
                ln = ln[:-3]
                out.append(plural(ln))
            elif ln != ln.lower():
                continue
            out.append(ln)
        lines = out
        if no_enclitics:
            plain = {strip_accents(ln) for ln in lines}
            kept = [ln for ln in lines if strip_accents(ln) in keep or not is_enclitic(ln, plain)]
            print(f"enclíticos quitados: {len(lines) - len(kept)}", file=sys.stderr)
            lines = kept
        lines = [strip_accents(ln) for ln in lines]
        if drop:
            lines = [ln for ln in lines if ln not in drop]
    for line in lines:
        w = conv(line)
        if w is None or not (minlen <= len(w) <= maxlen):
            dropped += 1
            continue
        words.add(w)
    return sorted(words), dropped


def synthetic(n, minlen, maxlen, seed):
    rng = random.Random(seed)
    words = set()
    while len(words) < n:
        k = rng.randint(minlen, maxlen)
        words.add("".join(rng.choice(string.ascii_uppercase) for _ in range(k)).encode("ascii"))
    return sorted(words)


def read_set(path):
    if not path:
        return frozenset()
    with open(path, encoding="utf-8") as f:
        return frozenset(strip_accents(ln.strip().lower()) for ln in f if ln.strip() and not ln.startswith("#"))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("paths", nargs="+", help="lista de palabras y fichero de salida (con --synthetic, solo el de salida)")
    ap.add_argument("--synthetic", type=int, metavar="N", help="en vez de una lista, N palabras al azar")
    ap.add_argument("--minlen", type=int, default=2)
    ap.add_argument("--maxlen", type=int, default=15)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--lang", choices=["en", "es"], default="en", help="alfabeto de la lista de palabras")
    ap.add_argument("--keep", metavar="FICHERO", help="(es) palabras que --no-enclitics no debe quitar (una por linea)")
    ap.add_argument("--drop", metavar="FICHERO", help="(es) palabras que se quitan siempre (abreviaturas...; una por linea)")
    ap.add_argument("--no-enclitics", action="store_true", help="(es) quita las formas verbales con pronombres pegados (damelo, lavandose)")
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
        words, dropped = load_words(a.paths[0], a.minlen, a.maxlen, a.lang, a.no_enclitics, read_set(a.keep), read_set(a.drop))
    with open(dst, "wb") as f:
        f.write(MAGIC + struct.pack("<I", len(words)) + bytes(REC - 12))
        for w in words:
            f.write(w + bytes(REC - len(w)))
    size = REC * (len(words) + 1)
    print(f"{dst}: {len(words)} palabras, {size} bytes ({size / 1024 / 1024:.2f} MB), {dropped} descartadas", file=sys.stderr)


if __name__ == "__main__":
    main()
