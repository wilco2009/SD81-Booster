#ifndef IHEX_PARSER_H
#define IHEX_PARSER_H
#include <stdint.h>

// Streaming Intel HEX parser (the format Xilinx iMPACT/ISE exports as .mcs
// for Spartan-6 PROM images). Reads the file from the SD card one line at a
// time — never loads the whole file into RAM — and invokes a callback for
// each data record found, with its absolute 32-bit address already resolved
// (record type 04, "extended linear address", sets the high 16 bits).
//
// Independent of any flash-programming code: this module only decodes the
// text format and hands decoded bytes to whoever supplies the callback.

enum IHexStatus {
  IHEX_OK = 0,          // reached an End Of File record (type 01), all good
  IHEX_ERR_FILE,        // could not open the file
  IHEX_ERR_FORMAT,      // malformed line (missing ':', bad hex digit, short line)
  IHEX_ERR_CHECKSUM,    // record checksum did not match
};

// Called once per data record (type 00), already decoded.
// 'addr' is the absolute 32-bit address of data[0].
typedef void (*IHexDataCallback)(uint32_t addr, const uint8_t* data, uint8_t len, void* ctx);

// Parses 'path' from the SD card, calling 'cb' for every data record.
// Returns IHEX_OK only if an EOF record (type 01) was reached without errors.
IHexStatus ihex_parse_file(const char* path, IHexDataCallback cb, void* ctx);

#endif
