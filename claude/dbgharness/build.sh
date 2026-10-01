#!/bin/sh
# Compila y pasa los dos bancos de pruebas del depurador (ver README.md).
# Uso: sh build.sh   (desde esta carpeta; g++/gcc y pasmo en el PATH)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)
FW="$REPO/Arduino/SD81BoosterV2_039_STM32"
Z80SRC="${Z80SRC:-/c/ClaudeCode/Eightyone2/src}"    # nucleo Z80 de EightyOne
OUT="$HERE/build"
mkdir -p "$OUT"
cd "$OUT"

# Todo se compila desde build/: DEBUGGER.cpp incluye "GLOBALS.h" etc. entre
# comillas y tiene que encontrar las cabeceras de mentira, no las del
# firmware (y un mismo Arduino.h para todos)
cp "$HERE/Arduino.h" "$HERE/GLOBALS.h" "$HERE/PINS.h" "$HERE/SD_handle.h" .
cp "$HERE/logic.cpp" "$HERE/cosim.cpp" .
cp "$FW/DEBUGGER.cpp" "$FW/DEBUGGER.h" "$FW/COMMS.h" "$FW/COMMANDS.h" .
cp "$FW/z80-disassembler.h" "$FW/z80-disassembler.cpp" .

echo "== logica del MCU (logic.cpp)"
g++ -std=gnu++17 -O1 -I. -o logic.exe logic.cpp DEBUGGER.cpp
./logic.exe | grep -E "ERROR|TODO OK|ERRORES"

echo "== cosimulacion (cosim.cpp)"
pasmo "$REPO/z80rom/debugmon.asm" debugmon.bin
pasmo "$HERE/prog.asm" prog.bin
gcc -c -O1 -I"$Z80SRC/z80" -I"$Z80SRC" "$Z80SRC/z80/z80.c" -o z80.o
gcc -c -O1 -I"$Z80SRC/z80" -I"$Z80SRC" "$Z80SRC/z80/z80_ops.c" -o z80_ops.o
g++ -std=gnu++17 -O1 -I. -I"$Z80SRC/z80" -I"$Z80SRC" -o cosim.exe cosim.cpp DEBUGGER.cpp z80-disassembler.cpp z80.o z80_ops.o -pthread
./cosim.exe debugmon.bin prog.bin | grep -E "ERROR|TODO OK|ERRORES"
