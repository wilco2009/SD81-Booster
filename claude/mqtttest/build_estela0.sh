#!/bin/sh
# EXAMPLES/ESTELA/PRUEBAS (fase 0 de ESTELA) en el PC: el Z80 de EightyOne ejecuta CARGA, VSYNC, SONIDO y PAGINA con la memoria paginada,
# el VSYNC, el teclado, los AY de la FPGA y un MCU simulados (test_z80estela0.cpp). Necesita g++/gcc, pasmo y el nucleo Z80 de EightyOne (Z80SRC).
cd "$(dirname "$0")" || exit 1
Z80SRC="${Z80SRC:-/c/ClaudeCode/Eightyone2/src}"
rm -rf tmp_estela0 && mkdir tmp_estela0
for p in CARGA VSYNC SONIDO PAGINA; do
  n=$(echo $p | tr 'A-Z' 'a-z')
  (cd ../../EXAMPLES/ESTELA/PRUEBAS/$p && pasmo $n.asm "$OLDPWD/tmp_estela0/$n.bin" "$OLDPWD/tmp_estela0/$n.sym") || exit 1
done
cp test_z80estela0.cpp tmp_estela0/ && cd tmp_estela0 || exit 1
gcc -c -O1 -I"$Z80SRC/z80" -I"$Z80SRC" "$Z80SRC/z80/z80.c" -o z80.o || exit 1
gcc -c -O1 -I"$Z80SRC/z80" -I"$Z80SRC" "$Z80SRC/z80/z80_ops.c" -o z80_ops.o || exit 1
g++ -std=gnu++17 -O1 -I"$Z80SRC/z80" -I"$Z80SRC" test_z80estela0.cpp z80.o z80_ops.o -o test_z80estela0.exe || exit 1
r=0
./test_z80estela0.exe carga carga.bin || r=1
./test_z80estela0.exe vsync vsync.bin vsync.sym || r=1
./test_z80estela0.exe sonido sonido.bin || r=1
./test_z80estela0.exe pagina pagina.bin pagina.sym || r=1
exit $r
