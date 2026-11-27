#!/bin/sh
# Prueba en el PC de Arduino/Wifi_module_01/KEYMAP_ZX81.cpp (teclado BLE -> matriz del ZX81).
# KEYMAP_ZX81.cpp no depende de Arduino: se compila tal cual con g++ y se ejecuta test_keymap.cpp.
cd "$(dirname "$0")" || exit 1
SRC=../../Arduino/Wifi_module_01
mkdir -p tmp
g++ -std=gnu++11 -O1 -Wall -Wextra -I"$SRC" test_keymap.cpp "$SRC/KEYMAP_ZX81.cpp" -o tmp/test_keymap.exe || exit 1
./tmp/test_keymap.exe
