// Prueba de resistencia de netq.inc: 50 mensajes por segundo en cada sentido durante 60 s, con 1 o 3 recogidas por fotograma (argv[2]); mide si la latencia
// o los contadores se van degradando con el tiempo (lo que se ve en la placa: "empieza bien y acaba a tirones").
#include "z80net_env.h"
static const uint16_t NQ = 0x7500, MB = 0x7F00;
static const uint16_t EP_OPEN = 24585, EP_SENDQ = 24588, EP_PUMP = 24591, EP_GET = 24594;
static uint8_t nqb(int off) { return mem[NQ + off]; }
static int nq16(int off) { return mem[NQ + off] | (mem[NQ + off + 1] << 8); }
static void frame() { uint32_t t0 = mock_now; while (mock_now < t0 + 20) { time_extra += 2; mock_now = 100000 + time_extra + (uint32_t)(instr_total * 5 / 3250); net_bridge_loop(); } }
static void half_frame() { uint32_t t0 = mock_now; while (mock_now < t0 + 7) { time_extra += 1; mock_now = 100000 + time_extra + (uint32_t)(instr_total * 5 / 3250); net_bridge_loop(); } }
int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  int pumps = argc > 2 ? atoi(argv[2]) : 1;
  FILE* f = fopen(argv[1], "rb"); fread(mem + 24576, 1, 0x2000, f); fclose(f);
  z80_init(); z80_reset(); reset_world();
  call_ep(24576); call_ep(24579);
  std::string code = room_code_ascii(), base = topic_base(code);
  call_ep(EP_OPEN); seen.clear();
  std::deque<uint32_t> tpub; uint32_t win_sum = 0; int win_n = 0, win_max = 0, got = 0, sent_peer = 0;
  for (int fr = 0; fr < 3000; fr++) {
    mem[MB] = 12; for (int i = 0; i < 12; i++) mem[MB + 16 + i] = (uint8_t)i; call_ep(EP_SENDQ);
    std::string d(12, 'x'); d[0] = (char)(fr & 255);
    peer_publish(base + "/1", d, false); tpub.push_back(mock_now); sent_peer++;
    for (int p = 0; p < pumps; p++) {
      call_ep(EP_PUMP);
      while (true) { int n = call_ep(EP_GET); if (!n) break; got++; /* el mensaje mas viejo aun sin recoger */ int fr0 = (uint8_t)mem[NQ + 288]; (void)fr0; }
      if (p + 1 < pumps) half_frame();
    }
    // latencia: del mensaje que se acaba de recibir (el ultimo contado) frente al instante de su publicacion, por el contador de fotograma que lleva dentro
    { int fr_in = (uint8_t)mem[NQ + 288]; int age = (uint8_t)(fr - fr_in); win_sum += age; win_n++; win_max = std::max(win_max, age); }
    frame();
    if ((fr + 1) % 500 == 0) {
      printf("  t=%2ds: edad media del ultimo mensaje %.1f fotogramas (max %d), recibidos %d de %d, FC=%d ASKS=%d ERR=%d LOST=%d DROP=%d\n", (fr + 1) / 50, win_n ? (double)win_sum / win_n : 0.0, win_max, nq16(16), sent_peer, nqb(3), nqb(5), nqb(20), nqb(22), nqb(21));
      win_sum = 0; win_n = 0; win_max = 0;
    }
  }
  return 0;
}
