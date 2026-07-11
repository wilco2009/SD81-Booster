#include <Arduino.h>
#include <string.h>
#include "FLASH_UPDATE.h"
#include "FLASH_SPI.h"
#include "IHEX_PARSER.h"
#include "SD_handle.h"
#include "GLOBALS.h"

// ------------------------------------------------------------------
// Pass 1 — erase + program
// Accumulates contiguous bytes into a 256B buffer and programs it when
// the physical page boundary is reached (page programs must not cross a
// 256B boundary) or when the incoming address is discontiguous. Each 4KB
// sector is erased the first time a page inside it is programmed; .mcs
// files are strictly ascending, so remembering the last erased sector is
// enough to erase each one exactly once.
// ------------------------------------------------------------------
struct ProgCtx {
  uint32_t pageAddr;               // flash address of pageBuf[0]
  uint16_t pageFill;               // bytes accumulated (contiguous from pageAddr)
  int32_t  lastErased;             // last erased sector number, -1 = none
  bool     error;
  uint32_t bytesTotal;             // programmed so far (for progress/report)
  uint32_t nextDot;                // next bytesTotal milestone for a progress dot
  uint8_t  pageBuf[FLASH_PAGE_SIZE];
};

static void prog_flush(ProgCtx* c) {
  if (c->error || c->pageFill == 0) return;

  uint32_t sector = c->pageAddr / FLASH_SECTOR_SIZE;
  if ((int32_t)sector != c->lastErased) {
    if (!flash_sector_erase(sector * FLASH_SECTOR_SIZE)) {
      log_0("❌ flash update: sector erase failed at %08lX", sector * FLASH_SECTOR_SIZE);
      c->error = true;
      return;
    }
    c->lastErased = sector;
  }

  if (!flash_page_program(c->pageAddr, c->pageBuf, c->pageFill)) {
    log_0("❌ flash update: page program failed at %08lX", c->pageAddr);
    c->error = true;
    return;
  }

  c->bytesTotal += c->pageFill;
  c->pageAddr   += c->pageFill;    // stays valid for the contiguous case
  c->pageFill    = 0;

  if (c->bytesTotal >= c->nextDot) {   // one dot per 32KB, keeps serial quiet
    Serial.print(".");
    c->nextDot += 32768;
  }
}

static void prog_cb(uint32_t addr, const uint8_t* data, uint8_t len, void* vctx) {
  ProgCtx* c = (ProgCtx*)vctx;
  if (c->error) return;

  for (uint8_t i = 0; i < len; i++) {
    uint32_t a = addr + i;
    if (c->pageFill > 0 && a != c->pageAddr + c->pageFill) {
      prog_flush(c);                 // discontiguous address: close current page
      if (c->error) return;
    }
    if (c->pageFill == 0) c->pageAddr = a;
    c->pageBuf[c->pageFill++] = data[i];
    // Never let the buffer cross a physical 256B page boundary.
    if (((c->pageAddr + c->pageFill) & (FLASH_PAGE_SIZE - 1)) == 0) {
      prog_flush(c);
      if (c->error) return;
    }
  }
}

// ------------------------------------------------------------------
// Pass 2 — verify (read back and compare against the file)
// ------------------------------------------------------------------
struct VerifyCtx {
  bool     error;
  uint32_t failAddr;
};

static void verify_cb(uint32_t addr, const uint8_t* data, uint8_t len, void* vctx) {
  VerifyCtx* c = (VerifyCtx*)vctx;
  if (c->error) return;

  uint8_t buf[255];                  // len is a uint8_t: 255 max
  flash_read(addr, buf, len);
  if (memcmp(buf, data, len) != 0) {
    c->error = true;
    c->failAddr = addr;
  }
}

// ------------------------------------------------------------------
// Public entry point
// ------------------------------------------------------------------
FlashUpdateStatus flash_update_from_mcs(const char* path) {
  if (!sd.exists(path)) return FLASHUPD_NO_FILE;   // nothing to do, not an error

  long startTime = millis();

  flash_begin();

  // Sanity check before touching anything: right chip, bus really ours.
  uint32_t id = flash_read_jedec_id();
  if (id != FLASH_JEDEC_ID) {
    log_0("❌ flash update: bad JEDEC id %06lX (expected %06lX) — bus not acquired?",
          id, (uint32_t)FLASH_JEDEC_ID);
    flash_end();
    return FLASHUPD_ERR_NO_FLASH;
  }

  // ---- pass 1: erase + program ----
  Serial.print("ℹ️ Programming FPGA flash");
  ProgCtx pc = {};
  pc.lastErased = -1;
  pc.nextDot    = 32768;
  IHexStatus hs = ihex_parse_file(path, prog_cb, &pc);
  prog_flush(&pc);                   // flush the final partial page
  Serial.println("");

  if (hs != IHEX_OK) {
    log_0("❌ flash update: HEX parse failed (status %d)", hs);
    flash_end();
    return FLASHUPD_ERR_HEX;
  }
  if (pc.error) {
    flash_end();
    return FLASHUPD_ERR_PROGRAM;
  }
  log_0("ℹ️ Programmed %lu bytes, verifying...", pc.bytesTotal);

  // ---- pass 2: verify ----
  VerifyCtx vc = {};
  hs = ihex_parse_file(path, verify_cb, &vc);
  flash_end();

  if (hs != IHEX_OK) {
    log_0("❌ flash update: HEX parse failed on verify (status %d)", hs);
    return FLASHUPD_ERR_HEX;
  }
  if (vc.error) {
    log_0("❌ flash update: verify MISMATCH at %08lX", vc.failAddr);
    return FLASHUPD_ERR_VERIFY;
  }

  log_0("✅ FPGA flash updated and verified (%lu bytes in %ld ms)",
        pc.bytesTotal, millis() - startTime);
  return FLASHUPD_OK;
}
