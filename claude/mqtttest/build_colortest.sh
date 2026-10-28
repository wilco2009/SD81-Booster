#!/bin/sh
# EXAMPLES/COLORTEST/colortest.asm en el PC: el Z80 de EightyOne ejecuta el programa con los POKEs de la paleta de 16 (2063, 2064, 2066, 2067)
# y el teclado simulados (test_z80colortest.cpp). Necesita g++/gcc, python, pasmo y el nucleo Z80 de EightyOne (Z80SRC).
cd "$(dirname "$0")" || exit 1
Z80SRC="${Z80SRC:-/c/ClaudeCode/Eightyone2/src}"
rm -rf tmp_colortest && mkdir tmp_colortest
(cd ../../EXAMPLES/COLORTEST && python -I gen_pal16.py >/dev/null && pasmo colortest.asm "$OLDPWD/tmp_colortest/colortest.bin" "$OLDPWD/tmp_colortest/colortest.sym") || exit 1
cp ../../EXAMPLES/COLORTEST/pal16.inc test_z80colortest.cpp tmp_colortest/ && cd tmp_colortest || exit 1
gcc -c -O1 -I"$Z80SRC/z80" -I"$Z80SRC" "$Z80SRC/z80/z80.c" -o z80.o || exit 1
gcc -c -O1 -I"$Z80SRC/z80" -I"$Z80SRC" "$Z80SRC/z80/z80_ops.c" -o z80_ops.o || exit 1
g++ -std=gnu++17 -O1 -I"$Z80SRC/z80" -I"$Z80SRC" test_z80colortest.cpp z80.o z80_ops.o -o test_z80colortest.exe || exit 1
./test_z80colortest.exe colortest.bin colortest.sym pal16.inc
