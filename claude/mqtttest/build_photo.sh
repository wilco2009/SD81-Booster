#!/bin/sh
# EXAMPLES/PHOTO en el PC: genera PNG de prueba (cuatro tipos), los convierte con png2i16.py, comprueba los .I16 contra un calculo independiente
# y ejecuta el visor en el Z80 de EightyOne (test_z80photo.cpp) con N / B / ESPACIO, un fichero que no es .I16 y un hueco de numeracion.
# Necesita g++/gcc, python, pasmo y el nucleo Z80 de EightyOne (Z80SRC).
cd "$(dirname "$0")" || exit 1
Z80SRC="${Z80SRC:-/c/ClaudeCode/Eightyone2/src}"
rm -rf tmp_photo && mkdir -p tmp_photo/sd tmp_photo/vacia
python -I photo_testdata.py make tmp_photo/png || exit 1
P=../../EXAMPLES/PHOTO
python -I $P/png2i16.py -o tmp_photo/sd --start 1 tmp_photo/png/t1.png || exit 1
python -I $P/png2i16.py -o tmp_photo/sd --start 2 tmp_photo/png/t2.png || exit 1
python -I $P/png2i16.py -o tmp_photo/sd --start 4 tmp_photo/png/t3.png || exit 1
python -I $P/png2i16.py -o tmp_photo/sd --start 5 tmp_photo/png/t4.png || exit 1
python -I photo_testdata.py verify tmp_photo/sd || exit 1
head -c 16516 /dev/urandom > tmp_photo/sd/IMG003.I16       # existe, pero no es un .I16: el visor lo salta
(cd $P && pasmo photo.asm "$OLDPWD/tmp_photo/photo.bin" "$OLDPWD/tmp_photo/photo.sym") || exit 1
cd tmp_photo || exit 1
gcc -c -O1 -I"$Z80SRC/z80" -I"$Z80SRC" "$Z80SRC/z80/z80.c" -o z80.o || exit 1
gcc -c -O1 -I"$Z80SRC/z80" -I"$Z80SRC" "$Z80SRC/z80/z80_ops.c" -o z80_ops.o || exit 1
g++ -std=gnu++17 -O1 -I"$Z80SRC/z80" -I"$Z80SRC" ../test_z80photo.cpp z80.o z80_ops.o -o test_z80photo.exe || exit 1
echo "--- con 1, 2, (3 malo), 4, 5: N N N N B B B ESPACIO ---"
./test_z80photo.exe photo.bin sd 1,2,4,5,1,5,4,2 NNNNBBBS || exit 1
echo "--- carpeta sin imagenes ---"
./test_z80photo.exe photo.bin vacia "" || exit 1
