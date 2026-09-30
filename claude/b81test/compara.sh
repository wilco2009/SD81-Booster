#!/bin/sh
# Compara el conversor .B81 del STM32 (B81.cpp) con el cargador de EightyOne.
# Uso: sh compara.sh fichero.b81 [...]   (desde esta carpeta, con g++ en el PATH)
E=${EIGHTYONE_SRC:-/c/ClaudeCode/Eightyone2/src}
FW=../../Arduino/SD81BoosterV2_039_STM32
[ -x ref81.exe ] || g++ -O1 -std=c++17 -I$E -o ref81.exe ref_main.cpp $E/BasicLoader/IBasicLoader.cpp $E/zx81/zx81BasicLoader.cpp
[ -x my81.exe ]  || g++ -O1 -Wall -std=c++17 -I$FW -o my81.exe my_main.cpp $FW/B81.cpp
for f in "$@"; do
  ./ref81.exe "$f" _ref.p > _ref.log 2>&1; r=$?
  ./my81.exe  "$f" _my.p  > _my.log  2>&1; m=$?
  if [ $r -eq 0 ] && [ $m -eq 0 ] && cmp -s _ref.p _my.p; then echo "IGUAL   $f"
  elif [ $r -ne 0 ] && [ $m -ne 0 ]; then echo "ERROR   $f: $(head -1 _my.log)"
  else echo "DISTINTO $f (ref=$r my=$m)"; fi
done
rm -f _ref.p _my.p _ref.log _my.log
