#!/bin/sh
# Capa B (EXAMPLES/NETCHAT/net.inc) de extremo a extremo en el PC: el Z80 de EightyOne ejecuta
# netchat.bin contra el modem de verdad (NET_BRIDGE.cpp + NET_MQTT.cpp) y un broker falso.
# Necesita g++/gcc, pasmo y el nucleo Z80 de EightyOne (Z80SRC).
cd "$(dirname "$0")" || exit 1
SRC=../../Arduino/Wifi_module_01
Z80SRC="${Z80SRC:-/c/ClaudeCode/Eightyone2/src}"
rm -rf tmp2 && mkdir -p tmp2/mbedtls
cp $SRC/NET_MQTT.cpp $SRC/NET_MQTT.h $SRC/NET_BRIDGE.cpp $SRC/NET_BRIDGE.h $SRC/mqtt_codec.h $SRC/WIFI_PROTOCOL.h mock.h test_z80net.cpp tmp2/
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
