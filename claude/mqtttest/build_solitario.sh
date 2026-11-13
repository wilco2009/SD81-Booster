#!/bin/sh
# EXAMPLES/SOLITARIO/solitario.asm en el PC (fase 1): el Z80 de EightyOne ejecuta el arranque hasta ml_init y las rutinas
# de dibujo (draw_card con paso 32 y 3, mesa de muestra, barra) y cmp_solitario.py compara el resultado con preview_mesa.png
# y con el modelo de gen_cards.py. Necesita g++/gcc, pasmo, Python con Pillow (PY) y el nucleo Z80 de EightyOne (Z80SRC).
cd "$(dirname "$0")" || exit 1
Z80SRC="${Z80SRC:-/c/ClaudeCode/Eightyone2/src}"
PY="${PY:-/c/Users/aleja/AppData/Local/Programs/Python/Python312/python.exe}"
rm -rf tmp_solitario && mkdir tmp_solitario
(cd ../../EXAMPLES/SOLITARIO && pasmo -I ../MOUSE solitario.asm "$OLDPWD/tmp_solitario/solitario.bin" "$OLDPWD/tmp_solitario/solitario.sym") || exit 1
cp test_z80solitario.cpp tmp_solitario/ && cp "../../SD Content/SYS/zx81v2.rom" tmp_solitario/ && cd tmp_solitario || exit 1
gcc -c -O1 -I"$Z80SRC/z80" -I"$Z80SRC" "$Z80SRC/z80/z80.c" -o z80.o || exit 1
gcc -c -O1 -I"$Z80SRC/z80" -I"$Z80SRC" "$Z80SRC/z80/z80_ops.c" -o z80_ops.o || exit 1
g++ -std=gnu++17 -O1 -I"$Z80SRC/z80" -I"$Z80SRC" test_z80solitario.cpp z80.o z80_ops.o -o test_z80solitario.exe || exit 1
./test_z80solitario.exe solitario.bin solitario.sym zx81v2.rom || exit 1
"$PY" ../cmp_solitario.py
