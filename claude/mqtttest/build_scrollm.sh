#!/bin/sh
# EXAMPLES/SCROLLM/scrollm.asm en el PC: el Z80 de EightyOne ejecuta el programa de prueba del scroll por hardware con la
# pantalla, los POKEs de configuracion, el puerto $3FEF, el teclado y el VSYNC simulados (test_z80scrollm.cpp).
# Necesita g++/gcc, pasmo y el nucleo Z80 de EightyOne (Z80SRC).
cd "$(dirname "$0")" || exit 1
Z80SRC="${Z80SRC:-/c/ClaudeCode/Eightyone2/src}"
rm -rf tmp_scrollm && mkdir tmp_scrollm
(cd ../../EXAMPLES/SCROLLM && pasmo scrollm.asm "$OLDPWD/tmp_scrollm/scrollm.bin" "$OLDPWD/tmp_scrollm/scrollm.sym") || exit 1
cp test_z80scrollm.cpp tmp_scrollm/ && cd tmp_scrollm || exit 1
gcc -c -O1 -I"$Z80SRC/z80" -I"$Z80SRC" "$Z80SRC/z80/z80.c" -o z80.o || exit 1
gcc -c -O1 -I"$Z80SRC/z80" -I"$Z80SRC" "$Z80SRC/z80/z80_ops.c" -o z80_ops.o || exit 1
g++ -std=gnu++17 -O1 -I"$Z80SRC/z80" -I"$Z80SRC" test_z80scrollm.cpp z80.o z80_ops.o -o test_z80scrollm.exe || exit 1
./test_z80scrollm.exe scrollm.bin scrollm.sym
