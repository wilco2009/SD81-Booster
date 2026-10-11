#!/bin/sh
# Pruebas de MQTT en el PC (sin hardware):
#  1. mqtt_codec.h: paquetes con los bytes de la especificacion
#  2. NET_MQTT.cpp contra un broker falso (comandos AT, cola, QoS 1, reconexion, ping)
cd "$(dirname "$0")" || exit 1
SRC=../../Arduino/Wifi_module_01

echo "== codec"
g++ -std=c++11 -Wall -I$SRC test_codec.cpp -o test_codec.exe && ./test_codec.exe || exit 1

echo "== NET_MQTT.cpp"
rm -rf tmp && mkdir -p tmp/mbedtls
cp $SRC/NET_MQTT.cpp $SRC/NET_MQTT.h $SRC/NET_BRIDGE.h $SRC/mqtt_codec.h mock.h tmp/
for h in Arduino.h WiFi.h WiFiClientSecure.h esp_system.h SD_LOG.h WIFI_CLIENT.h; do
  echo '#include "mock.h"' > tmp/$h
done
echo '#include "../mock.h"' > tmp/mbedtls/md.h
cp test_net_mqtt.cpp tmp/
(cd tmp && g++ -std=gnu++11 -Wall -Wno-unused-function -I. test_net_mqtt.cpp NET_MQTT.cpp -o test_net_mqtt.exe) || exit 1
./tmp/test_net_mqtt.exe
