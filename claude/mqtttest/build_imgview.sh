#!/bin/sh
# EXAMPLES/IMGVIEW en el PC: el Z80 de EightyOne ejecuta el visor del explorador con F$ en las variables BASIC, el teclado (la F rebota), un MCU
# simulado (fopen, fread, fclose y la orden 76, que usa el BMP.cpp del firmware) y la paleta (test_z80imgview.cpp). Necesita g++/gcc, python, pasmo
# y el nucleo Z80 de EightyOne (Z80SRC).
cd "$(dirname "$0")" || exit 1
Z80SRC="${Z80SRC:-/c/ClaudeCode/Eightyone2/src}"
FW=../../Arduino/SD81BoosterV2_039_STM32
P=../../EXAMPLES/PHOTO
rm -rf tmp_imgview && mkdir -p tmp_imgview/sd
python -I photo_testdata.py make tmp_imgview/png >/dev/null || exit 1
python -I $P/png2i16.py -o tmp_imgview/sd --start 1 tmp_imgview/png/t1.png >/dev/null || exit 1
python -I photo_testdata.py bmp tmp_imgview/sd >/dev/null || exit 1          # IMG006.BMP (300x150), IMG007.BMP (mala), IMG008.BMP (200x200, 32 bits)
head -c 16516 /dev/urandom > tmp_imgview/sd/IMG003.I16                        # existe pero no es un .I16
echo "hola" > tmp_imgview/sd/NOTE.TXT
(cd ../../EXAMPLES/IMGVIEW && pasmo imgview.asm "$OLDPWD/tmp_imgview/imgview.bin" "$OLDPWD/tmp_imgview/imgview.sym") || exit 1
cd tmp_imgview || exit 1
gcc -c -O1 -I"$Z80SRC/z80" -I"$Z80SRC" "$Z80SRC/z80/z80.c" -o z80.o || exit 1
gcc -c -O1 -I"$Z80SRC/z80" -I"$Z80SRC" "$Z80SRC/z80/z80_ops.c" -o z80_ops.o || exit 1
g++ -std=gnu++17 -O1 -I"$Z80SRC/z80" -I"$Z80SRC" -I"../$FW" ../test_z80imgview.cpp "../$FW/BMP.cpp" z80.o z80_ops.o -o test_z80imgview.exe || exit 1
run() { echo "--- $1: $2 (F x$3, BC=$4, cargas $5) ---"; ./test_z80imgview.exe imgview.bin sd "$1" "$3" "$4" "$5" | grep -E "ERROR|FALLOS|TODO|MCU:" || fail=1; }
fail=0
run IMG001.I16 .I16 0 0 -
run IMG001.I16 ".I16 con F (no hace nada)" 2 0 -
run IMG006.BMP .BMP 0 0 1
run IMG006.BMP ".BMP con F" 3 0 1,2,0,1
run IMG008.BMP ".BMP de 32 bits" 1 0 1,2
run IMG007.BMP "BMP malo" 0 2 ""
run IMG003.I16 "I16 malo" 0 1 ""
run NOFILE.I16 "I16 inexistente" 0 1 ""
run NOFILE.BMP "BMP inexistente" 0 1 ""
run NOTE.TXT "ni .I16 ni .BMP" 0 1 ""
run - "sin F\$" 0 1 ""
exit $fail
