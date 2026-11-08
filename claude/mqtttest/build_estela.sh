#!/bin/sh
# EXAMPLES/ESTELA/estela.asm en el PC: el Z80 de EightyOne ejecuta el juego con la memoria paginada, el VSYNC, el teclado y el scroll
# en anillo del modo 175 simulados, y compara lo visible con un modelo del mundo escrito aparte (test_z80estela.cpp).
# Necesita g++/gcc, pasmo y el nucleo Z80 de EightyOne (Z80SRC).
cd "$(dirname "$0")" || exit 1
Z80SRC="${Z80SRC:-/c/ClaudeCode/Eightyone2/src}"
rm -rf tmp_estela && mkdir tmp_estela
# Se ensambla en EXAMPLES/ESTELA, como compila.bat, y se prueba ese mismo estela.bin: el que va a la placa
# no puede ser otro distinto del que pasa el arnes (en la primera prueba en placa lo fue).
(cd ../../EXAMPLES/ESTELA && pasmo estela.asm estela.bin estela.sym) || exit 1
cp ../../EXAMPLES/ESTELA/estela.bin ../../EXAMPLES/ESTELA/estela.sym tmp_estela/ || exit 1
cp test_z80estela.cpp tmp_estela/ && cd tmp_estela || exit 1
gcc -c -O1 -I"$Z80SRC/z80" -I"$Z80SRC" "$Z80SRC/z80/z80.c" -o z80.o || exit 1
gcc -c -O1 -I"$Z80SRC/z80" -I"$Z80SRC" "$Z80SRC/z80/z80_ops.c" -o z80_ops.o || exit 1
g++ -std=gnu++17 -O2 -I"$Z80SRC/z80" -I"$Z80SRC" test_z80estela.cpp z80.o z80_ops.o -o test_z80estela.exe || exit 1
./test_z80estela.exe estela.bin estela.sym
