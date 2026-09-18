# clean_toc.py -- uso: python clean_toc.py archivo.md
#
# pandoc convierte los campos PAGEREF del indice de Word en enlaces anidados
# "[TEXTO [N](#ancla)](#ancla)" -- basura para un .md no paginado. Este
# script los deja en "[TEXTO](#ancla)". Es un no-op si el .docx no tiene los
# campos PAGEREF recalculados (p.ej. si se edito el XML a mano sin abrir en
# Word), pero hace falta en cuanto alguien abra el .docx en Word y pulse F9
# (refresca TOC) antes de la siguiente regeneracion.
import re
import sys

path = sys.argv[1]
text = open(path, encoding='utf-8').read()
pattern = re.compile(r'\[([^\[\]]*?)\s*\n?\[\d+\]\(#([^)]+)\)\]\(#([^)]+)\)')
text = pattern.sub(lambda m: f'[{m.group(1)}](#{m.group(3)})', text)
open(path, 'w', encoding='utf-8').write(text)
