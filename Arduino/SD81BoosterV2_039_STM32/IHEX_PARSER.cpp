#include "IHEX_PARSER.h"
#include "SD_handle.h"
#include "GLOBALS.h"

#define IHEX_MAX_LINE   524   // ':' + 2(len)+4(addr)+2(type)+2*255(data)+2(cksum)+CRLF, rounded up
#define IHEX_MAX_DATA   255   // record byte-count field is 1 byte

#define REC_DATA               0x00
#define REC_END_OF_FILE        0x01
#define REC_EXT_SEGMENT_ADDR   0x02   // unused by Xilinx .mcs, tolerated/ignored
#define REC_EXT_LINEAR_ADDR    0x04
#define REC_START_LINEAR_ADDR  0x05   // unused by Xilinx .mcs, tolerated/ignored

static int8_t hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

static bool hexByte(const char* s, uint8_t* out) {
  int8_t hi = hexNibble(s[0]);
  int8_t lo = hexNibble(s[1]);
  if (hi < 0 || lo < 0) return false;
  *out = (uint8_t)((hi << 4) | lo);
  return true;
}

// Decodes one Intel HEX line (already read into 'line', NUL-terminated,
// no leading ':' consumed yet). Returns false on any format/checksum error.
static bool decode_line(char* line, uint8_t* count, uint16_t* addr16,
                         uint8_t* type, uint8_t* data) {
  if (line[0] != ':') return false;
  char* p = line + 1;

  uint8_t sum = 0;

  uint8_t b;
  if (!hexByte(p, &b)) return false; p += 2;
  *count = b; sum += b;

  uint8_t ah, al;
  if (!hexByte(p, &ah)) return false; p += 2; sum += ah;
  if (!hexByte(p, &al)) return false; p += 2; sum += al;
  *addr16 = ((uint16_t)ah << 8) | al;

  if (!hexByte(p, &b)) return false; p += 2;
  *type = b; sum += b;

  for (uint8_t i = 0; i < *count; i++) {
    if (!hexByte(p, &b)) return false; p += 2;
    data[i] = b; sum += b;
  }

  uint8_t checksum;
  if (!hexByte(p, &checksum)) return false;

  // Record is valid iff sum of all bytes (incl. checksum) is 0 mod 256.
  return (uint8_t)(sum + checksum) == 0;
}

IHexStatus ihex_parse_file(const char* path, IHexDataCallback cb, void* ctx) {
  FsFile f;
  if (!f.open(path)) {
    log_0("❌ ihex_parse_file: cannot open %s", path);
    return IHEX_ERR_FILE;
  }

  char line[IHEX_MAX_LINE + 1];
  uint8_t data[IHEX_MAX_DATA];
  uint32_t extLinearBase = 0;   // high 16 bits of address, from type-04 records
  IHexStatus status = IHEX_ERR_FORMAT;   // set to OK only when EOF record seen

  while (true) {
    int n = f.fgets(line, sizeof(line));
    if (n <= 0) break;   // end of file reached without an EOF record
    // Trim trailing CR/LF.
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
    if (n == 0) continue;   // skip blank lines

    uint8_t count, type;
    uint16_t addr16;
    if (!decode_line(line, &count, &addr16, &type, data)) {
      log_0("❌ ihex_parse_file: bad line \"%s\"", line);
      status = IHEX_ERR_CHECKSUM;
      break;
    }

    if (type == REC_DATA) {
      if (cb) cb(extLinearBase + addr16, data, count, ctx);
    } else if (type == REC_EXT_LINEAR_ADDR) {
      if (count != 2) { status = IHEX_ERR_FORMAT; break; }
      extLinearBase = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16);
    } else if (type == REC_END_OF_FILE) {
      status = IHEX_OK;
      break;
    }
    // REC_EXT_SEGMENT_ADDR / REC_START_LINEAR_ADDR: not used by Xilinx .mcs
    // files; ignored if encountered rather than treated as an error.
  }

  f.close();
  return status;
}
