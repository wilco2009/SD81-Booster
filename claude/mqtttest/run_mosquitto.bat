@echo off
rem Broker local para las pruebas de red. Sin -v: el log de conexiones va a mosquitto.log (ver mosquitto.conf)
"C:\Program Files\mosquitto\mosquitto.exe" -c C:\ClaudeCode\SD81-Booster\claude\mqtttest\mosquitto.conf
