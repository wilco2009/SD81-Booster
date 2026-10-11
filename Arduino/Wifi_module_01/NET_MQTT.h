#ifndef NET_MQTT_H
#define NET_MQTT_H

#include <Arduino.h>

// Comandos AT genericos de MQTT y utilidades (AT+SHA256, AT+HMAC, AT+RAND),
// para cualquier aplicacion del Z80 (juegos en red, chat, domotica...). No
// saben nada de juegos: ver claude/planning/juego_en_red.md.
//
//   AT+MQTT=host,puerto[,tls[,usuario,clave]]   conecta a ese broker
//   AT+MQTT                                     conecta al primero que funcione de /SYS/NET.CFG
//   AT+MQTTID=cadena                            clientid (por defecto, uno aleatorio)
//   AT+MQTTWILL=topic,retain,texto              last will (antes de conectar)
//   AT+MQTTSUB=topic[,qos]   AT+MQTTUNSUB=topic
//   AT+MQTTPUB=topic,qos,retain,len             contesta ">" y se mandan len bytes crudos
//   AT+MQTTRX                                   saca un mensaje de la cola
//   AT+MQTT?                                    estado
//   AT+MQTTCLOSE                                cierra la sesion
//   AT+SHA256=len   AT+HMAC=keylen,datalen   AT+RAND=n    (len bytes crudos tras ">")

void net_mqtt_init();

// Llamar en cada vuelta del loop (lo hace net_bridge_loop): atiende el socket,
// el ping y la reconexion.
void net_mqtt_loop();

// true si hay sesion (o intento de reconexion): el puente sondea al STM32 rapido
bool net_mqtt_active();

// Un comando AT que empieza por "+" (args apunta justo despues de "AT", con las
// mayusculas / minusculas ORIGINALES). Devuelve false si no es un comando de
// esta familia (el puente contesta ERROR). Si lo es, deja la respuesta en la
// cola de salida del puente.
bool net_mqtt_at(const char* args, uint8_t alen);

// ATZ: cierra la sesion MQTT y suelta todo
void net_mqtt_close();

#endif
