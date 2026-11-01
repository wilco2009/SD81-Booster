#!/bin/sh
# BMP.cpp del STM32 en el PC: lo compila con g++ (no depende de Arduino) y lo compara, byte a byte, con la referencia en Python
# (EXAMPLES/PHOTO/bmp2i16.py) sobre BMP de todos los tipos que admite (test_bmp.py). Necesita g++ y python.
cd "$(dirname "$0")" || exit 1
FW=../../Arduino/SD81BoosterV2_039_STM32
rm -rf tmp_bmp && mkdir tmp_bmp
g++ -std=gnu++17 -O2 -Wall -Wextra -I"$FW" test_bmp.cpp "$FW/BMP.cpp" -o tmp_bmp/test_bmp.exe || exit 1
python test_bmp.py tmp_bmp tmp_bmp/test_bmp.exe
