// Prueba de extremo a extremo de la capa B (EXAMPLES/NETCHAT/net.inc) en el PC, sin hardware.
// Ver el entorno (Z80, tubo, modem de verdad, broker de mentira, rival) en z80net_env.h.
#include "z80net_env.h"

int main(int argc, char** argv) {
  if (argc < 2) { printf("uso: test_z80net netchat.bin\n"); return 2; }
  FILE* f = fopen(argv[1], "rb");
  if (!f) { printf("no encuentro %s\n", argv[1]); return 2; }
  size_t sz = fread(mem + 0x6000, 1, 0x1000, f); fclose(f);
  printf("netchat.bin: %zu bytes en $6000\n", sz);
  mem[0x02E7] = 0xC9; mem[0x0207] = 0xC9;       // SET_FAST y SLOW_FAST de la ROM: aqui, un RET
  z80_init(); z80_reset();
  reset_world();

  // --- 1. iniciar el modem
  CHECK(call_ep(EP_INIT) == 0 && mem[NB] == 0, "net_init: ATZ y ATE0 bien");
  CHECK(!at_echo, "net_init: el eco del modem queda apagado");

  // --- 2. crear una sala
  CHECK(call_ep(EP_NEW) == 0 && mem[NB] == 0, "room_new bien");
  std::string code = room_code_ascii();
  printf("  codigo de sala: %s\n", code.c_str());
  const char* alpha = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  bool alpha_ok = code.size() == 10;
  for (char ch : code) if (!strchr(alpha, ch)) alpha_ok = false;
  CHECK(alpha_ok, "el codigo son 10 caracteres del alfabeto de 32");
  std::string base = topic_base(code);
  CHECK(mem[NB + 1] == 0, "soy la ranura 0");
  const Seen* c = last_seen("CONNECT");
  CHECK(c && c->topic == base + "/p0", "CONNECT con el last will en <sala>/p0 (el topic sale de SHA256(topic+codigo))");
  CHECK(has_will && will_data == "0" && will_retain, "el will es '0' y retenido");
  CHECK(last_seen("SUBSCRIBE") && last_seen("SUBSCRIBE")->topic == base + "/+" && last_seen("SUBSCRIBE")->qos == 1, "suscrito a <sala>/+ con QoS 1");
  const Seen* pp = last_seen("PUBLISH", base + "/p0");
  CHECK(pp && pp->data == "1" && pp->retain && pp->qos == 1, "mi presencia '1' retenida en <sala>/p0");
  CHECK(mem[NB + 2] == 0, "sin rival todavia");

  // --- 3. nada que leer
  CHECK(call_ep(EP_POLL) == 0, "room_poll: no hay nada (mi propio eco se ignora)");

  // --- 4. el rival entra
  peer_publish(base + "/p1", "1", true);
  CHECK(call_ep(EP_POLL) == 2 && mem[NB + 2] == 1, "room_poll: el rival ha entrado");
  CHECK(call_ep(EP_POLL) == 0, "y no se repite");

  // --- 5. un mensaje del rival (con CR, LF y 0 en los datos)
  std::string d1("AB\r\nC\0D", 7);
  peer_publish(base + "/1", make_msg(code, 0x1234, 7, d1), true);
  CHECK(call_ep(EP_POLL) == 1, "room_poll: llega un mensaje");
  CHECK(mem[NB + 5] == 7 && mem[NB + 3] == 7 && mem[NB + 4] == 0, "longitud 7 y turno 7");
  CHECK(memcmp(mem + NB + 144, d1.data(), 7) == 0, "datos intactos (binario seguro)");

  // --- 6. repetido, firma mala, version mala y otra sala: se tiran
  peer_publish(base + "/1", make_msg(code, 0x1234, 8, "REP"), false);
  CHECK(call_ep(EP_POLL) == 0, "mismo numero de secuencia: se descarta");
  peer_publish(base + "/1", make_msg(code, 0x1235, 8, "MALA", true), false);
  CHECK(call_ep(EP_POLL) == 0, "firma mala: se descarta");
  { std::string m = make_msg(code, 0x1236, 8, "V2"); m[0] = 2; peer_publish(base + "/1", m, false);
    CHECK(call_ep(EP_POLL) == 0, "cuerpo alterado tras firmar: se descarta"); }
  peer_publish(base + "/0", make_msg(code, 0x2000, 1, "ECO"), false);
  CHECK(call_ep(EP_POLL) == 0, "un mensaje en MI ranura: se ignora");
  peer_publish(base + "/1", make_msg(code, 0x1237, 9, "OK2"), false);
  CHECK(call_ep(EP_POLL) == 1 && mem[NB + 5] == 3 && memcmp(mem + NB + 144, "OK2", 3) == 0 && mem[NB + 3] == 9, "y el siguiente bueno llega");
  peer_publish(base + "/1", make_msg(code, 0x1238, 10, ""), false);
  CHECK(call_ep(EP_POLL) == 1 && mem[NB + 5] == 0 && mem[NB + 3] == 10, "mensaje sin datos");

  // --- 6b. el sondeo en dos pasos: pedir, hacer otra cosa y recoger
  peer_publish(base + "/1", make_msg(code, 0x3000, 11, "DOS"), false);
  { long i0 = instr_total;
    CHECK(call_ep(EP_PSEND) == 0, "room_poll_send bien");
    CHECK(instr_total - i0 < 20000, "room_poll_send vuelve al momento (no espera al modem)");
    for (int k = 0; k < 100; k++) { time_extra++; mock_now++; net_bridge_loop(); }   // pasa el tiempo: el modem responde
    i0 = instr_total;
    CHECK(call_ep(EP_PRECV) == 1 && mem[NB + 5] == 3 && memcmp(mem + NB + 144, "DOS", 3) == 0 && mem[NB + 3] == 11, "room_poll_recv recoge el mensaje");
    // (con un mensaje, recv pide ademas el HMAC al modem: un comando mas, ~30 ms; sin mensajes es inmediato)
    call_ep(EP_PSEND);
    for (int k = 0; k < 100; k++) { time_extra++; mock_now++; net_bridge_loop(); }
    i0 = instr_total;
    CHECK(call_ep(EP_PRECV) == 0, "pedir y recoger sin nada que leer: 0");
    CHECK(instr_total - i0 < 20000, "y recoger no espera si la respuesta ya estaba");
    CHECK(call_ep(EP_PRECV) == 0, "room_poll_recv sin pedir antes: pregunta y espera; no hay nada"); }

  // --- 7. mandar
  for (int i = 0; i < 5; i++) mem[NB + 32 + i] = (uint8_t)"ABCDE"[i];
  mem[NB + 5] = 5; mem[NB + 3] = 9; mem[NB + 4] = 0;
  CHECK(call_ep(EP_SEND) == 0, "room_send bien");
  const Seen* sm = last_seen("PUBLISH", base + "/0");
  CHECK(sm && sm->qos == 1 && sm->retain && sm->data.size() == 5 + 5 + 8, "PUBLISH en <sala>/0, QoS 1, retenido, 18 bytes");
  if (sm && sm->data.size() == 18) {
    std::string body = sm->data.substr(0, 10), mac = sm->data.substr(10);
    uint8_t exp[32]; std::string k = key_for(code);
    hmac256((const uint8_t*)k.data(), k.size(), (const uint8_t*)body.data(), body.size(), exp);
    CHECK((uint8_t)body[0] == 1 && (uint8_t)body[3] == 9 && (uint8_t)body[4] == 0 && body.substr(5) == "ABCDE", "cabecera (version 1, turno 9) y datos");
    CHECK(memcmp(mac.data(), exp, 8) == 0, "el HMAC que calculo el ESP32 coincide con el independiente");
  }
  mem[NB + 5] = 101;
  CHECK(call_ep(EP_SEND) == 5, "mas de 100 bytes: error 5");
  mem[NB + 5] = 0;
  { size_t before = seen.size();
    CHECK(call_ep(EP_SEND) == 0 && seen.size() > before, "un mensaje sin datos tambien sale"); }

  // --- 8. dos envios seguidos: la secuencia crece
  mem[NB + 5] = 1; mem[NB + 32] = 'Z';
  CHECK(call_ep(EP_SEND) == 0, "otro envio");
  { auto& m1 = seen[seen.size() - 1]; (void)m1; }

  // --- 9. el rival se va
  peer_publish(base + "/p1", "0", true);
  CHECK(call_ep(EP_POLL) == 3 && mem[NB + 2] == 0, "room_poll: el rival se ha ido");

  // --- 10. salir
  CHECK(call_ep(EP_LEAVE) == 0, "room_leave bien");
  pp = last_seen("PUBLISH", base + "/p0");
  CHECK(pp && pp->data == "0" && pp->retain, "mi presencia pasa a '0'");
  CHECK(count_seen("DISCONNECT") == 1, "DISCONNECT al broker");

  // --- 11. unirse a una sala con un codigo (otra sesion), con NET_WRITE aceptando poco a poco
  write_limit = 17;
  reset_world();
  CHECK(call_ep(EP_INIT) == 0, "net_init otra vez");
  std::string code2 = "MNPQRSTUVW";
  peer_publish(topic_base(code2) + "/p0", "1", true);
  peer_publish(topic_base(code2) + "/0", make_msg(code2, 77, 3, "HOLA"), true);
  set_code(code2);
  CHECK(call_ep(EP_JOIN) == 0 && mem[NB + 1] == 1, "room_join bien, soy la ranura 1 (con escrituras parciales de 17 bytes)");
  c = last_seen("CONNECT");
  CHECK(c && c->topic == topic_base(code2) + "/p1", "el will, en <sala>/p1");
  // el broker entrega lo retenido en el orden de sus topics: primero la jugada y luego la presencia
  CHECK(call_ep(EP_POLL) == 1 && mem[NB + 5] == 4 && memcmp(mem + NB + 144, "HOLA", 4) == 0 && mem[NB + 3] == 3,
        "al unirme, la ultima jugada retenida del rival me llega (juego asincrono)");
  CHECK(call_ep(EP_POLL) == 2 && mem[NB + 2] == 1, "y su presencia retenida tambien");
  write_limit = 255;

  // --- 12. un codigo malo
  reset_world();
  CHECK(call_ep(EP_INIT) == 0, "net_init otra vez");
  set_code("MNPQRSTUVI");   // la I no esta en el alfabeto
  mem[NB + 8 + 9] = 46;     // 'I' = 38 + 8
  CHECK(call_ep(EP_JOIN) == 4, "room_join con un caracter fuera del alfabeto: error 4");
  CHECK(count_seen("CONNECT") == 0, "y no llega a conectar");

  // --- 13. sala llena: ya hay otro jugador en la ranura 1
  reset_world();
  CHECK(call_ep(EP_INIT) == 0, "net_init otra vez");
  {
    std::string code3 = "ABCDEFGHJK"; std::string b3 = topic_base(code3);
    peer_publish(b3 + "/p0", "1", true);
    peer_publish(b3 + "/p1", "1", true);            // el segundo jugador ya esta
    set_code(code3);
    CHECK(call_ep(EP_JOIN) == 6, "room_join con la ranura 1 ocupada: error 6 (sala llena)");
    CHECK(count_seen("DISCONNECT") == 1, "y cierra la sesion limpiamente (asi el last will no sale)");
    bool pub = false; for (auto& sn : seen) if (sn.kind == "PUBLISH") pub = true;
    CHECK(!pub && retained[b3 + "/p1"] == "1", "no publica nada: la presencia del otro jugador queda como estaba");
    reset_world();
    CHECK(call_ep(EP_INIT) == 0, "net_init otra vez");
    peer_publish(b3 + "/p1", "0", true);            // el otro jugador se fue sin dejar rastro
    CHECK(call_ep(EP_JOIN) == 0 && mem[NB + 1] == 1, "con la presencia del otro en '0' si se puede entrar");
    { const Seen* pp2 = last_seen("PUBLISH", b3 + "/p1");
      CHECK(pp2 && pp2->data == "1" && pp2->retain, "y publico mi presencia en la ranura 1"); }
    CHECK(count_seen("SUBSCRIBE") == 2 && count_seen("UNSUBSCRIBE") == 0, "(se suscribio a p1 y luego a todo; sin dobles)");
  }

  printf("  instrucciones del Z80: %ld (%.1f s a 3,25 MHz)\n", instr_total, instr_total * 5.0 / 3250000.0);
  if (errors) printf("%d ERRORES\n", errors); else printf("TODO OK\n");
  return errors ? 1 : 0;
}
