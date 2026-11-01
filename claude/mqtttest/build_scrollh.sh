#!/bin/sh
# EXAMPLES/SCROLLH/scrollh.asm en el PC: el Z80 de EightyOne ejecuta el programa de prueba del scroll por hardware con la
# pantalla, los POKEs de configuracion, el puerto $3FEF, el teclado y el VSYNC simulados (test_z80scrollh.cpp).
# Necesita g++/gcc, pasmo y el nucleo Z80 de EightyOne (Z80SRC).
cd "$(dirname "$0")" || exit 1
Z80SRC="${Z80SRC:-/c/ClaudeCode/Eightyone2/src}"
rm -rf tmp_scrollh && mkdir tmp_scrollh
(cd ../../EXAMPLES/SCROLLH && pasmo scrollh.asm "$OLDPWD/tmp_scrollh/scrollh.bin" "$OLDPWD/tmp_scrollh/scrollh.sym") || exit 1
cp test_z80scrollh.cpp tmp_scrollh/ && cd tmp_scrollh || exit 1
gcc -c -O1 -I"$Z80SRC/z80" -I"$Z80SRC" "$Z80SRC/z80/z80.c" -o z80.o || exit 1
gcc -c -O1 -I"$Z80SRC/z80" -I"$Z80SRC" "$Z80SRC/z80/z80_ops.c" -o z80_ops.o || exit 1
g++ -std=gnu++17 -O1 -I"$Z80SRC/z80" -I"$Z80SRC" test_z80scrollh.cpp z80.o z80_ops.o -o test_z80scrollh.exe || exit 1
./test_z80scrollh.exe scrollh.bin scrollh.sym
