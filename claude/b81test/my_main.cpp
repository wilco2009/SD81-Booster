#include "B81.h"
#include <stdio.h>
class FileReader : public B81Reader {
public:
  FILE* f;
  int getByte() override { return fgetc(f); }
  bool rewind() override { return fseek(f, 0, SEEK_SET) == 0; }
};
int main(int argc, char** argv)
{
  FileReader r; r.f = fopen(argv[1], "rb");
  if (!r.f) { printf("ERROR: can not open\n"); return 2; }
  static uint8_t out[B81_MAX_P];
  B81Error e;
  uint16_t n = b81_to_p(r, out, e);
  if (!n) { printf("ERROR: %s (file line %d, basic line %d) [%s]\n", e.msg, e.fileLine, e.basicLine, e.text); return 1; }
  FILE* o = fopen(argv[2], "wb"); fwrite(out, 1, n, o); fclose(o);
  return 0;
}
