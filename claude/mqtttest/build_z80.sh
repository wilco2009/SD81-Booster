#!/bin/sh
# Capa B (EXAMPLES/NETCHAT/net.inc) de extremo a extremo en el PC: el Z80 de EightyOne ejecuta
# netchat.bin contra el modem de verdad (NET_BRIDGE.cpp + NET_MQTT.cpp) y un broker falso.
# Necesita g++/gcc, pasmo y el nucleo Z80 de EightyOne (Z80SRC).
cd "$(dirname "$0")" || exit 1
SRC=../../Arduino/Wifi_module_01
Z80SRC="${Z80SRC:-/c/ClaudeCode/Eightyone2/src}"
rm -rf tmp2 && mkdir -p tmp2/mbedtls
cp $SRC/NET_MQTT.cpp $SRC/NET_MQTT.h $SRC/NET_BRIDGE.cpp $SRC/NET_BRIDGE.h $SRC/mqtt_codec.h $SRC/WIFI_PROTOCOL.h mock.h z80net_env.h test_z80net.cpp tmp2/
for h in Arduino.h WiFi.h WiFiClientSecure.h esp_system.h SD_LOG.h WIFI_CLIENT.h; do
  echo '#include "mock.h"' > tmp2/$h
done
echo '#include "../mock.h"' > tmp2/mbedtls/md.h
(cd ../../EXAMPLES/NETCHAT && pasmo netchat.asm "$OLDPWD/tmp2/netchat.bin" ) || exit 1
cd tmp2 || exit 1
gcc -c -O1 -I"$Z80SRC/z80" -I"$Z80SRC" "$Z80SRC/z80/z80.c" -o z80.o || exit 1
gcc -c -O1 -I"$Z80SRC/z80" -I"$Z80SRC" "$Z80SRC/z80/z80_ops.c" -o z80_ops.o || exit 1
g++ -std=gnu++17 -O1 -Wno-unused-function -DMOCK_REAL_MD -I. -I"$Z80SRC/z80" -I"$Z80SRC" test_z80net.cpp NET_MQTT.cpp z80.o z80_ops.o -o test_z80net.exe || exit 1
./test_z80net.exe netchat.bin

echo "== 4 en raya (EXAMPLES/NETGAME): reglas y controlador"
cd .. || exit 1
cp z80net_env.h mock.h test_z80game.cpp tmp2/
pasmo c4test.asm tmp2/c4test.bin || exit 1
cd tmp2 || exit 1
g++ -std=gnu++17 -O1 -Wno-unused-function -DMOCK_REAL_MD -I. -I"$Z80SRC/z80" -I"$Z80SRC" test_z80game.cpp NET_MQTT.cpp z80.o z80_ops.o -o test_z80game.exe || exit 1
./test_z80game.exe c4test.bin

echo "== el juego entero (EXAMPLES/NETGAME/connect4.asm): pantalla, teclado y red"
cd .. || exit 1
cp test_z80ui.cpp tmp2/
(cd ../../EXAMPLES/NETGAME && pasmo connect4.asm "$OLDPWD/tmp2/connect4.bin") || exit 1
[ "$(stat -c %s tmp2/connect4.bin)" -le 8192 ] || { echo "ERROR connect4.bin pasa de \$8000"; exit 1; }
cd tmp2 || exit 1
g++ -std=gnu++17 -O1 -Wno-unused-function -DMOCK_REAL_MD -I. -I"$Z80SRC/z80" -I"$Z80SRC" test_z80ui.cpp NET_MQTT.cpp z80.o z80_ops.o -o test_z80ui.exe || exit 1
./test_z80ui.exe connect4.bin

echo "== ajedrez (EXAMPLES/NETGAME/chess.inc): perft, mate, ahogado, enroque, al paso, coronacion"
cd .. || exit 1
cp test_z80chess.cpp tmp2/
pasmo chtest.asm tmp2/chtest.bin || exit 1
cd tmp2 || exit 1
g++ -std=gnu++17 -O1 -Wno-unused-function -DMOCK_REAL_MD -I. -I"$Z80SRC/z80" -I"$Z80SRC" test_z80chess.cpp NET_MQTT.cpp z80.o z80_ops.o -o test_z80chess.exe || exit 1
./test_z80chess.exe chtest.bin

echo "== el ajedrez entero (EXAMPLES/NETGAME/chess.asm): pantalla, teclado y red"
cd .. || exit 1
cp test_z80chessui.cpp tmp2/
cp ../../EXAMPLES/NETGAME/pieces.inc tmp2/
(cd ../../EXAMPLES/NETGAME && pasmo chess.asm "$OLDPWD/tmp2/chess.bin") || exit 1
# el ZX81 no ejecuta codigo por encima de $8000: chess empieza en $5000 y connect4 en $6000
[ "$(stat -c %s tmp2/chess.bin)" -le 12288 ] || { echo "ERROR chess.bin pasa de \$8000"; exit 1; }
cd tmp2 || exit 1
g++ -std=gnu++17 -O1 -Wno-unused-function -DMOCK_REAL_MD -I. -I"$Z80SRC/z80" -I"$Z80SRC" test_z80chessui.cpp NET_MQTT.cpp z80.o z80_ops.o -o test_z80chessui.exe || exit 1
./test_z80chessui.exe chess.bin

echo "== diccionario (EXAMPLES/DICTTEST): busqueda binaria en un fichero de la SD, por el Z80"
cd .. || exit 1
cp test_z80dict.cpp tmp2/
(cd ../../EXAMPLES/DICTTEST && pasmo dicttest.asm "$OLDPWD/tmp2/dicttest.bin") || exit 1
cd tmp2 || exit 1
g++ -std=gnu++17 -O1 -Wno-unused-function -DMOCK_REAL_MD -I. -I"$Z80SRC/z80" -I"$Z80SRC" test_z80dict.cpp NET_MQTT.cpp z80.o z80_ops.o -o test_z80dict.exe || exit 1
./test_z80dict.exe dicttest.bin

echo "== Scrabble (EXAMPLES/NETGAME/scrabble.inc): bolsa, puntuacion, validacion y partidas enteras contra una referencia"
cd .. || exit 1
cp test_z80scrabble.cpp scrabble_ref.h tmp2/
pasmo scrtest.asm tmp2/scrtest.bin || exit 1
cd tmp2 || exit 1
g++ -std=gnu++17 -O1 -Wno-unused-function -DMOCK_REAL_MD -I. -I"$Z80SRC/z80" -I"$Z80SRC" test_z80scrabble.cpp NET_MQTT.cpp z80.o z80_ops.o -o test_z80scrabble.exe || exit 1
./test_z80scrabble.exe scrtest.bin ../../../build/dict/enable1.txt $ESLIST   # ESLIST=<lista en castellano en UTF-8> usa la real

echo "== Scrabble: la pantalla (EXAMPLES/NETGAME/scrabble_ui.inc), con la fuente de la ROM de verdad"
cd .. || exit 1
cp test_z80scrabbleui.cpp tmp2/
pasmo scrui.asm tmp2/scrui.bin || exit 1
cd tmp2 || exit 1
g++ -std=gnu++17 -O1 -Wno-unused-function -DMOCK_REAL_MD -I. -I"$Z80SRC/z80" -I"$Z80SRC" test_z80scrabbleui.cpp NET_MQTT.cpp z80.o z80_ops.o -o test_z80scrabbleui.exe || exit 1
./test_z80scrabbleui.exe scrui.bin "../../../SD Content/SYS/SDBOOST.ROM" ../../../build/dict/enable1.txt

echo "== el Scrabble entero (EXAMPLES/NETGAME/scrabble.asm): menu, idioma, jugadas, red, estado por trozos, guardar y seguir"
cd .. || exit 1
cp test_z80scrabblegame.cpp scrabble_ref.h tmp2/
(cd ../../EXAMPLES/NETGAME && pasmo scrabble.asm "$OLDPWD/tmp2/scrabble.bin") || exit 1
# el ZX81 no ejecuta codigo por encima de $8000: scrabble empieza en $4400 (CLEAR 17407), asi que caben 15.360 bytes
[ "$(stat -c %s tmp2/scrabble.bin)" -le 15360 ] || { echo "ERROR scrabble.bin pasa de \$8000"; exit 1; }
cd tmp2 || exit 1
g++ -std=gnu++17 -O1 -Wno-unused-function -DMOCK_REAL_MD -I. -I"$Z80SRC/z80" -I"$Z80SRC" test_z80scrabblegame.cpp NET_MQTT.cpp z80.o z80_ops.o -o test_z80scrabblegame.exe || exit 1
./test_z80scrabblegame.exe scrabble.bin "../../../SD Content/SYS/SDBOOST.ROM" ../../../build/dict/enable1.txt

echo "== NETLAT (EXAMPLES/NETLAT): medir la latencia de la red desde el Z80"
cd .. || exit 1
cp test_z80netlat.cpp tmp2/
(cd ../../EXAMPLES/NETLAT && pasmo netlat.asm "$OLDPWD/tmp2/netlat.bin") || exit 1
cd tmp2 || exit 1
g++ -std=gnu++17 -O1 -Wno-unused-function -DMOCK_REAL_MD -I. -I"$Z80SRC/z80" -I"$Z80SRC" test_z80netlat.cpp NET_MQTT.cpp z80.o z80_ops.o -o test_z80netlat.exe || exit 1
./test_z80netlat.exe netlat.bin

echo "== NETQ (EXAMPLES/NETCHAT/netq.inc): mensajes de red sin esperas para juegos en tiempo real"
cd .. || exit 1
cp test_z80nq.cpp tmp2/
pasmo nqtest.asm tmp2/nqtest.bin || exit 1
cd tmp2 || exit 1
g++ -std=gnu++17 -O1 -Wno-unused-function -DMOCK_REAL_MD -I. -I"$Z80SRC/z80" -I"$Z80SRC" test_z80nq.cpp NET_MQTT.cpp z80.o z80_ops.o -o test_z80nq.exe || exit 1
./test_z80nq.exe nqtest.bin

echo "== PONG (EXAMPLES/NETGAME/pong.asm): el Pong en red, creador e invitado"
cd .. || exit 1
cp test_z80pong.cpp tmp2/
(cd ../../EXAMPLES/NETGAME && pasmo pong.asm "$OLDPWD/tmp2/pong.bin" "$OLDPWD/tmp2/pong.sym") || exit 1
[ "$(stat -c %s tmp2/pong.bin)" -le 8192 ] || { echo "ERROR pong.bin pasa de \$8000"; exit 1; }
cd tmp2 || exit 1
g++ -std=gnu++17 -O1 -Wno-unused-function -DMOCK_REAL_MD -I. -I"$Z80SRC/z80" -I"$Z80SRC" test_z80pong.cpp NET_MQTT.cpp z80.o z80_ops.o -o test_z80pong.exe || exit 1
./test_z80pong.exe pong.bin pong.sym

echo "== SPORTS (EXAMPLES/NETGAME/sports.asm): Tennis, Hockey, Squash y Practice en red, con opciones (tarda: unos 10 min por escenario)"
cd .. || exit 1
cp test_z80sports.cpp tmp2/
(cd ../../EXAMPLES/NETGAME && pasmo sports.asm "$OLDPWD/tmp2/sports.bin" "$OLDPWD/tmp2/sports.sym") || exit 1
[ "$(stat -c %s tmp2/sports.bin)" -le 15360 ] || { echo "ERROR sports.bin pasa de \$8000"; exit 1; }
cd tmp2 || exit 1
g++ -std=gnu++17 -O1 -Wno-unused-function -DMOCK_REAL_MD -I. -I"$Z80SRC/z80" -I"$Z80SRC" test_z80sports.cpp NET_MQTT.cpp z80.o z80_ops.o -o test_z80sports.exe || exit 1
./test_z80sports.exe sports.bin sports.sym
