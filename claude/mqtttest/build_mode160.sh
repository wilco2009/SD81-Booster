#!/bin/sh
# EXAMPLES/MODE160/mode160.asm en el PC: el Z80 de EightyOne ejecuta el programa con la paleta (POKEs 2063-2065), el teclado y el
# VSYNC simulados (test_z80mode160.cpp). Necesita g++/gcc, python, pasmo y el nucleo Z80 de EightyOne (Z80SRC).
cd "$(dirname "$0")" || exit 1
Z80SRC="${Z80SRC:-/c/ClaudeCode/Eightyone2/src}"
rm -rf tmp_mode160 && mkdir tmp_mode160
(cd ../../EXAMPLES/MODE160 && python pal160.py >/dev/null && pasmo mode160.asm "$OLDPWD/tmp_mode160/mode160.bin" "$OLDPWD/tmp_mode160/mode160.sym") || exit 1
cp ../../EXAMPLES/MODE160/pal160.bin test_z80mode160.cpp tmp_mode160/ && cd tmp_mode160 || exit 1
gcc -c -O1 -I"$Z80SRC/z80" -I"$Z80SRC" "$Z80SRC/z80/z80.c" -o z80.o || exit 1
gcc -c -O1 -I"$Z80SRC/z80" -I"$Z80SRC" "$Z80SRC/z80/z80_ops.c" -o z80_ops.o || exit 1
g++ -std=gnu++17 -O1 -I"$Z80SRC/z80" -I"$Z80SRC" test_z80mode160.cpp z80.o z80_ops.o -o test_z80mode160.exe || exit 1
./test_z80mode160.exe mode160.bin pal160.bin
