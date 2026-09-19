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

// Lo ofrecido al STM32 que aun no ha ACEPTADO (bit de secuencia). No se
// puede soltar hasta el ack: si hubo timeout, la siguiente llamada tiene
// que ofrecer exactamente los mismos bytes.
static uint8_t  pending[WIFI_PROTO_NET_CHUNK];
static uint16_t pending_len = 0;

// Credito de sitio libre en el buffer de entrada del STM32, en trozos de 16
// bytes. Arranca optimista: la primera respuesta lo corrige.
static uint8_t rx_free_chunks = WIFI_PROTO_NET_CHUNK / 16;

// --- Cola de salida hacia el Z80 (todo lo que no sea "leido tal cual del
// socket" pasa por aqui: eco de comandos AT, respuestas OK/CONNECT/...).
// Un unico canal para las dos fuentes: net_bridge_loop() solo sabe volcarla
// en `pending`, no le importa si el byte vino del socket o lo genero el
// interprete AT. Tamano en potencia de 2 para que la mascara salga gratis.
#define TXQ_SIZE 256
#define TXQ_MASK (TXQ_SIZE-1)
static uint8_t  txq_buf[TXQ_SIZE];
static uint16_t txq_head = 0, txq_tail = 0;

static uint16_t txq_used() { return (uint16_t)((txq_head - txq_tail) & TXQ_MASK); }
static uint16_t txq_free() { return (uint16_t)(TXQ_MASK - txq_used()); }
static void txq_push(uint8_t b) {
  if (txq_free() == 0) return;   // cola llena: se pierde (no deberia pasar,
                                 // las respuestas AT son cortas y el Z80 las
                                 // lee mucho antes de llenar 256 bytes)
  txq_buf[txq_head] = b;
  txq_head = (uint16_t)((txq_head + 1) & TXQ_MASK);
}
static void txq_push_str(const char* s) { while (*s) txq_push((uint8_t)*s++); }

// --- Interprete de comandos AT (fase 2.5) -----------------------------------
// Modelo Hayes de toda la vida: MODO COMANDO (lo que el Z80 escribe se
// interpreta como texto AT) o MODO DATOS (va directo al socket). Por
// defecto uno depende del otro (sin conexion = comando, conectado = datos),
// pero "+++" permite pasar a modo comando SIN colgar -- por eso es una
// bandera aparte y no simplemente "state==NET_ST_CONNECTED".
//
// at_command_mode se controla en UN SOLO SITIO para las dos vias que pueden
// cambiarlo (la pagina web /telnet Y los comandos AT): dentro de
// net_bridge_connect()/net_bridge_disconnect(). Asi da igual como se abrio
// la conexion, el resultado es consistente.
static bool at_command_mode = true;
static bool at_echo         = true;    // ATE1 por defecto, como un modem real

static char    at_cmdbuf[80];
static uint8_t at_cmdlen = 0;

// Deteccion de "+++": necesita silencio de verdad antes Y despues (guard
// time), o cualquier transferencia binaria que contenga tres simbolos de
// suma seguidos cortaria la conexion sin que el usuario lo pidiera.
#define ESC_GUARD_MS 1000
static uint8_t  esc_plus_count    = 0;    // 0-3, "+" vistos sin confirmar
static uint32_t esc_last_byte_ms  = 0;    // ultimo byte de datos procesado
static uint32_t esc_plus3_ms      = 0;    // cuando llego el 3er "+" (arranca el guard "despues")

static void at_reset_command_state() {
  at_cmdlen = 0;
  esc_plus_count = 0;
}

static void at_execute_command();  // fwd decl

// Procesa UN byte en MODO COMANDO: eco opcional, acumula hasta CR.
static void at_feed_command_byte(uint8_t b) {
  if (at_echo) txq_push(b);
  if (b == 13) {                         // CR: fin de linea, ejecutar
    at_execute_command();
    at_cmdlen = 0;
    return;
  }
  if (b == 10) return;                   // LF suelto: se ignora (CR ya cerro la linea)
  if (at_cmdlen >= sizeof(at_cmdbuf)) {   // linea demasiado larga: se descarta
    txq_push_str("ERROR\r\n");
    at_cmdlen = 0;
    return;
  }
  at_cmdbuf[at_cmdlen++] = (char)b;
}

// Procesa UN byte en MODO DATOS: pasa al socket tal cual, salvo que forme
// parte de una secuencia de escape "+++" real (con silencio antes y
// despues). Los "+" candidatos se retienen (no se mandan) hasta saber si
// la secuencia es de verdad o hay que soltarlos como dato normal.
static void at_feed_data_byte(uint8_t b) {
  uint32_t now = millis();

  if (esc_plus_count == 3) {
    // Ya se habian visto 3 "+" y se esperaba el silencio de despues; ha
    // llegado un byte nuevo antes de completarlo -> no era un escape de
    // verdad. Se sueltan los 3 "+" como datos y se sigue con este byte.
    client.write((const uint8_t*)"+++", 3);
    esc_plus_count = 0;
  }

  if (b == '+' && esc_plus_count < 3) {
    bool ok_to_start = (esc_plus_count > 0) ||
                        ((uint32_t)(now - esc_last_byte_ms) >= ESC_GUARD_MS);
    if (ok_to_start) {
      esc_plus_count++;
      if (esc_plus_count == 3) esc_plus3_ms = now;
      esc_last_byte_ms = now;
      return;                    // retenido, no se manda todavia
    }
  }

  if (esc_plus_count > 0) {      // habia "+" sueltos (1 o 2) que no cuajaron
    client.write((const uint8_t*)"+++", esc_plus_count);
    esc_plus_count = 0;
  }
  client.write(&b, 1);
  esc_last_byte_ms = now;
}

static void at_process_from_z80(const uint8_t* data, uint16_t len) {
  for (uint16_t i = 0; i < len; i++) {
    if (at_command_mode) at_feed_command_byte(data[i]);
    else                 at_feed_data_byte(data[i]);
  }
}

// Comprueba el guard time DE DESPUES del "+++" -- hace falta llamarla desde
// el bucle principal aunque no llegue ningun byte nuevo, porque es un
// vencimiento de tiempo, no un evento.
static void at_check_escape_timeout() {
  if (at_command_mode) return;
  if (esc_plus_count != 3) return;
  if ((uint32_t)(millis() - esc_plus3_ms) < ESC_GUARD_MS) return;
  esc_plus_count = 0;
  at_command_mode = true;
  txq_push_str("OK\r\n");
  Serial.println("NET: +++ -> modo comando");
}

// at_parse_hostport: "host:puerto" en (line, `len` caracteres) -> *out_host
// (buffer del LLAMADOR, no el host_buf global -- net_bridge_connect() copia
// a host_buf por su cuenta, y pasarle el mismo buffer como origen y destino
// seria un snprintf(buf,...,buf), comportamiento no definido) y *out_port.
// Devuelve false si no hay ":" o el puerto no es numerico.
static bool at_parse_hostport(const char* line, uint8_t len,
                               char* out_host, size_t out_host_size,
                               uint16_t* out_port) {
  int colon = -1;
  for (uint8_t i = 0; i < len; i++) {
    if (line[i] == ':') { colon = i; break; }
  }
  if (colon < 0 || colon == 0 || colon == len - 1) return false;

  uint8_t hlen = (uint8_t)colon;
  if (hlen >= out_host_size) hlen = (uint8_t)(out_host_size - 1);
  memcpy(out_host, line, hlen);
  out_host[hlen] = 0;

  uint16_t p = 0;
  for (uint8_t i = colon + 1; i < len; i++) {
    if (line[i] < '0' || line[i] > '9') return false;
    p = (uint16_t)(p * 10 + (line[i] - '0'));
  }
  *out_port = p;
  return true;
}

static void at_execute_command() {
  if (at_cmdlen == 0) return;             // linea vacia: los modems reales no contestan nada

  // Normalizar a mayusculas para comparar -- un modem de verdad es
  // insensible a mayusculas/minusculas, y esto no afecta al host de ATDT
  // (DNS tampoco distingue mayusculas).
  for (uint8_t i = 0; i < at_cmdlen; i++) {
    if (at_cmdbuf[i] >= 'a' && at_cmdbuf[i] <= 'z') at_cmdbuf[i] -= 32;
  }

  if (at_cmdlen < 2 || at_cmdbuf[0] != 'A' || at_cmdbuf[1] != 'T') {
    txq_push_str("ERROR\r\n");
    return;
  }
  if (at_cmdlen == 2) {                   // "AT" a secas
    txq_push_str("OK\r\n");
    return;
  }

  const char* args = at_cmdbuf + 2;
  uint8_t alen = (uint8_t)(at_cmdlen - 2);

  if (alen >= 2 && args[0] == 'D' && args[1] == 'T') {
    const char* hp = args + 2;
    uint8_t     hplen = (uint8_t)(alen - 2);
    while (hplen > 0 && *hp == ' ') { hp++; hplen--; }   // "ATDT host:port" o "ATDThost:port", los dos valen
    char     dial_host[64];
    uint16_t dial_port;
    if (!at_parse_hostport(hp, hplen, dial_host, sizeof(dial_host), &dial_port)) {
      txq_push_str("ERROR\r\n");
      return;
    }
    Serial.printf("NET: ATDT %s:%u\n", dial_host, dial_port);
    if (net_bridge_connect(dial_host, dial_port)) {
      txq_push_str("CONNECT\r\n");        // net_bridge_connect ya puso modo datos
    } else {
      txq_push_str("NO CARRIER\r\n");
    }
    return;
  }
  if (alen >= 1 && args[0] == 'H') {       // ATH / ATH0
    net_bridge_disconnect();
    txq_push_str("OK\r\n");
    return;
  }
  if (alen >= 1 && args[0] == 'O') {       // ATO / ATO0 -- reanudar sin colgar
    if (state == NET_ST_CONNECTED) {
      at_command_mode = false;
      txq_push_str("CONNECT\r\n");
    } else {
      txq_push_str("NO CARRIER\r\n");
    }
    return;
  }
  if (alen == 2 && args[0] == 'E' && args[1] == '0') {
    at_echo = false;
    txq_push_str("OK\r\n");
    return;
  }
  if (alen == 2 && args[0] == 'E' && args[1] == '1') {
    at_echo = true;
    txq_push_str("OK\r\n");
    return;
  }
  if (alen == 1 && args[0] == 'Z') {       // ATZ -- reset a estado inicial
    net_bridge_disconnect();
    at_echo = true;
    txq_push_str("OK\r\n");
    return;
  }

  txq_push_str("ERROR\r\n");
}

// -----------------------------------------------------------------------

void net_bridge_init() {
  state = NET_ST_IDLE;
  pending_len = 0;
  last_poll = millis();
  at_command_mode = true;
  at_reset_command_state();
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
  at_command_mode = false;       // conectado (por AT o por /telnet) = modo datos
  at_reset_command_state();
  Serial.println("NET: conectado");
  return true;
}

void net_bridge_disconnect() {
  if (client.connected()) client.stop();
  // pending/txq NO se limpian: si quedaban bytes del servidor sin entregar,
  // el Z80 tiene derecho a leerlos aunque la conexion ya se haya cerrado.
  if (state == NET_ST_CONNECTED || state == NET_ST_CONNECTING) {
    state = NET_ST_IDLE;
    Serial.println("NET: desconectado");
  }
  at_command_mode = true;        // sin conexion = modo comando, siempre
  at_reset_command_state();
}

uint8_t     net_bridge_state() { return state; }
const char* net_bridge_host()  { return host_buf; }
uint16_t    net_bridge_port()  { return port_num; }

void net_bridge_loop() {
  at_check_escape_timeout();     // vencimiento del guard time de "+++", no depende de que llegue nada

  uint32_t now = millis();
  uint32_t interval = (state == NET_ST_CONNECTED) ? NET_POLL_ACTIVE_MS : NET_POLL_IDLE_MS;
  if ((uint32_t)(now - last_poll) < interval) return;
  last_poll = now;

  // El servidor puede cortar por su cuenta (idle timeout, /quit del BBS...).
  if (state == NET_ST_CONNECTED && !client.connected()) {
    net_bridge_disconnect();
    txq_push_str("NO CARRIER\r\n");
    Serial.println("NET: el servidor cerro la conexion");
  }

  // Alimentar la cola de salida desde el socket -- SOLO en modo datos: en
  // modo comando (incluido tras un "+++") se deja de leer del socket a
  // proposito, sin perder nada (se queda en el buffer TCP del sistema,
  // esperando a ATO), igual que hace un modem de verdad mientras repasas
  // comandos con la linea todavia abierta.
  if (!at_command_mode && client.connected() && client.available() > 0) {
    uint16_t room = txq_free();
    if (room > 0) {
      uint8_t tmp[64];
      uint16_t want = room < sizeof(tmp) ? room : sizeof(tmp);
      int got = client.read(tmp, want);
      for (int i = 0; i < got; i++) txq_push(tmp[i]);
    }
  }

  // Volcar la cola de salida en `pending` solo cuando el hueco anterior ya
  // fue confirmado -- mismo credito/secuencia de siempre, la cola no lo
  // cambia, solo decide QUE se ofrece.
  if (pending_len == 0 && txq_used() > 0) {
    uint16_t room = (uint16_t)rx_free_chunks * 16;
    if (room > WIFI_PROTO_NET_CHUNK) room = WIFI_PROTO_NET_CHUNK;
    uint16_t n = txq_used();
    if (n > room) n = room;
    for (uint16_t i = 0; i < n; i++) {
      pending[i] = txq_buf[txq_tail];
      txq_tail = (uint16_t)((txq_tail + 1) & TXQ_MASK);
    }
    pending_len = n;
  }

  bool     accepted;
  uint16_t from_z80_len;
  uint8_t  rxbuf[WIFI_PROTO_NET_CHUNK];
  if (!wifi_client_net_poll(state, pending, pending_len, &accepted,
                            rxbuf, &from_z80_len, &rx_free_chunks)) {
    return;   // fallo de transporte: se reintenta lo mismo en la proxima vuelta
  }
  if (accepted) pending_len = 0;

  // Lo que el Z80 haya escrito: en modo comando se interpreta como AT: en
  // modo datos va al socket tal cual (con la deteccion de "+++" por medio).
  if (from_z80_len > 0) {
    at_process_from_z80(rxbuf, from_z80_len);
  }
}
