#!/bin/sh
# JPG.cpp del STM32 en el PC: lo compila con g++ (no depende de Arduino) y lo compara, byte a byte, con la referencia en Python
# (EXAMPLES/PHOTO/jpg2i16.py) sobre JPG de todos los tipos que admite (test_jpg.py). Necesita g++ y un python con Pillow y numpy.
# BMP y JPG comparten I16Common.cpp, asi que conviene ejecutar tambien build_bmp.sh.
cd "$(dirname "$0")" || exit 1
FW=../../Arduino/SD81BoosterV2_039_STM32
PY=python
$PY -c "import PIL, numpy" 2>/dev/null || PY="/c/Users/aleja/AppData/Local/Programs/Python/Python312/python.exe"
rm -rf tmp_jpg && mkdir tmp_jpg
g++ -std=gnu++17 -O2 -Wall -Wextra -I"$FW" test_jpg.cpp "$FW/JPG.cpp" "$FW/I16Common.cpp" -o tmp_jpg/test_jpg.exe || exit 1
$PY test_jpg.py tmp_jpg tmp_jpg/test_jpg.exe
