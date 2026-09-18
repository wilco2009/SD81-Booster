#include "NET_BRIDGE.h"
#include "WIFI_CLIENT.h"
#include "WIFI_PROTOCOL.h"
#include <WiFi.h>
#include "SD_LOG.h"   // ultimo a proposito, ver Wifi_module_01.ino

// Solo se loguea en eventos (conectar/desconectar/cortes), NUNCA en el camino
// de datos: con el sondeo a 15 ms, un log por trama llenaria la SD y robaria
// el tiempo que hace falta para mover bytes.

#define NET_POLL_IDLE_MS     250   // sin conexion: lo justo para recoger un ATDT
#define NET_POLL_ACTIVE_MS    15   // con conexion: ~8,5 KB/s por sentido de techo

static WiFiClient client;
static uint8_t    state = NET_ST_IDLE;
static char       host_buf[96] = "";
static uint16_t   port_num = 0;
static uint32_t   last_poll = 0;

// Lo leido del socket que aun no ha ACEPTADO el STM32. No se puede soltar
// hasta que lo confirme: si hubo timeout, la siguiente llamada tiene que
// ofrecer exactamente los mismos bytes (ver el bit de secuencia en
// wifi_client_net_poll).
static uint8_t  pending[WIFI_PROTO_NET_CHUNK];
static uint16_t pending_len = 0;

// Credito de sitio libre en el buffer de entrada del STM32, en trozos de 16
// bytes. Arranca optimista: la primera respuesta lo corrige.
static uint8_t rx_free_chunks = WIFI_PROTO_NET_CHUNK / 16;

void net_bridge_init() {
  state = NET_ST_IDLE;
  pending_len = 0;
  last_poll = millis();
}

bool net_bridge_connect(const char* host, uint16_t port) {
  net_bridge_disconnect();
  if (WiFi.status() != WL_CONNECTED) { state = NET_ST_ERROR; return false; }

  snprintf(host_buf, sizeof(host_buf), "%s", host);
  port_num = port;
  state = NET_ST_CONNECTING;
  Serial.printf("NET: conectando a %s:%u\n", host_buf, port_num);

  if (!client.connect(host_buf, port_num)) {
    state = NET_ST_ERROR;
    Serial.println("NET: fallo de conexion");
    return false;
  }
  client.setNoDelay(true);       // sin Nagle: en un terminal interactivo la
                                 // latencia importa mas que el troceo
  state = NET_ST_CONNECTED;
  Serial.println("NET: conectado");
  return true;
}

void net_bridge_disconnect() {
  if (client.connected()) client.stop();
  // pending_len NO se limpia: si quedaban bytes del servidor sin entregar,
  // el Z80 tiene derecho a leerlos aunque la conexion ya se haya cerrado.
  if (state == NET_ST_CONNECTED || state == NET_ST_CONNECTING) {
    state = NET_ST_IDLE;
    Serial.println("NET: desconectado");
  }
}

uint8_t     net_bridge_state() { return state; }
const char* net_bridge_host()  { return host_buf; }
uint16_t    net_bridge_port()  { return port_num; }

void net_bridge_loop() {
  uint32_t now = millis();
  uint32_t interval = (state == NET_ST_CONNECTED) ? NET_POLL_ACTIVE_MS : NET_POLL_IDLE_MS;
  if ((uint32_t)(now - last_poll) < interval) return;
  last_poll = now;

  // El servidor puede cortar por su cuenta (idle timeout, /quit del BBS...).
  if (state == NET_ST_CONNECTED && !client.connected()) {
    client.stop();
    state = NET_ST_IDLE;
    Serial.println("NET: el servidor cerro la conexion");
  }

  // Coger del socket solo si no hay nada pendiente de confirmar, y nunca mas
  // de lo que quepa al otro lado (control de flujo por credito).
  if (pending_len == 0 && client.available() > 0) {
    uint16_t room = (uint16_t)rx_free_chunks * 16;
    if (room > WIFI_PROTO_NET_CHUNK) room = WIFI_PROTO_NET_CHUNK;
    if (room > 0) {
      int got = client.read(pending, room);
      if (got > 0) pending_len = (uint16_t)got;
    }
  }

  bool     accepted;
  uint16_t from_z80_len;
  uint8_t  rxbuf[WIFI_PROTO_NET_CHUNK];
  if (!wifi_client_net_poll(state, pending, pending_len, &accepted,
                            rxbuf, &from_z80_len, &rx_free_chunks)) {
    return;   // fallo de transporte: se reintenta lo mismo en la proxima vuelta
  }
  if (accepted) pending_len = 0;

  // Lo que el Z80 haya escrito. En fase 1 va al socket tal cual; en la 2.5
  // aqui es donde se mete el interprete AT (modo comando vs modo datos).
  if (from_z80_len > 0 && state == NET_ST_CONNECTED) {
    client.write(rxbuf, from_z80_len);
  }
}
