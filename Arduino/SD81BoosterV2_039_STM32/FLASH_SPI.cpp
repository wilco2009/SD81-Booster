#include "FLASH_SPI.h"
#include <SPI.h>
#include "PINS.h"
#include "GLOBALS.h"

// Winbond W25Q128JVSIQ command set (standard JEDEC SPI NOR flash opcodes).
#define CMD_WRITE_ENABLE   0x06
#define CMD_READ_STATUS1   0x05
#define CMD_PAGE_PROGRAM   0x02
#define CMD_SECTOR_ERASE   0x20   // 4KB
#define CMD_READ_DATA      0x03
#define CMD_JEDEC_ID       0x9F

#define STATUS_BUSY_BIT    0x01

static SPIClass FlashSPI(FLASH_MOSI, FLASH_MISO, FLASH_SCLK);
static const SPISettings flashSpiSettings(20000000, MSBFIRST, SPI_MODE0);

static inline void cs_low()  { digitalWrite(FLASH_CS, LOW); }
static inline void cs_high() { digitalWrite(FLASH_CS, HIGH); }

// Sends a 1-byte opcode plus a 3-byte big-endian address (24-bit addressing;
// 16 MB is fully covered by 24 bits, so no 4-byte-address mode is needed).
static void send_cmd_addr(uint8_t cmd, uint32_t addr) {
  FlashSPI.transfer(cmd);
  FlashSPI.transfer((uint8_t)(addr >> 16));
  FlashSPI.transfer((uint8_t)(addr >> 8));
  FlashSPI.transfer((uint8_t)addr);
}

void flash_begin() {
  pinMode(FLASH_CS, OUTPUT);
  cs_high();
  FlashSPI.begin();
  FlashSPI.beginTransaction(flashSpiSettings);
}

void flash_end() {
  FlashSPI.endTransaction();
  FlashSPI.end();
  // Tri-state again so the FPGA can safely drive the bus once released.
  pinMode(FLASH_CS, INPUT);
  pinMode(FLASH_SCLK, INPUT);
  pinMode(FLASH_MISO, INPUT);
  pinMode(FLASH_MOSI, INPUT);
}

uint32_t flash_read_jedec_id() {
  cs_low();
  FlashSPI.transfer(CMD_JEDEC_ID);
  uint32_t id = 0;
  id  = (uint32_t)FlashSPI.transfer(0x00) << 16;
  id |= (uint32_t)FlashSPI.transfer(0x00) << 8;
  id |=  FlashSPI.transfer(0x00);
  cs_high();
  return id;
}

uint8_t flash_read_status() {
  cs_low();
  FlashSPI.transfer(CMD_READ_STATUS1);
  uint8_t status = FlashSPI.transfer(0x00);
  cs_high();
  return status;
}

bool flash_wait_ready(uint32_t timeout_ms) {
  uint32_t start = millis();
  while (flash_read_status() & STATUS_BUSY_BIT) {
    if ((millis() - start) > timeout_ms) {
      log_0("❌ flash_wait_ready: timeout");
      return false;
    }
  }
  return true;
}

void flash_write_enable() {
  cs_low();
  FlashSPI.transfer(CMD_WRITE_ENABLE);
  cs_high();
}

bool flash_sector_erase(uint32_t addr) {
  flash_write_enable();
  cs_low();
  send_cmd_addr(CMD_SECTOR_ERASE, addr);
  cs_high();
  return flash_wait_ready(500);   // datasheet: typ. 45ms, max ~400ms
}

bool flash_page_program(uint32_t addr, const uint8_t* data, uint16_t len) {
  if (len == 0 || len > FLASH_PAGE_SIZE) return false;
  flash_write_enable();
  cs_low();
  send_cmd_addr(CMD_PAGE_PROGRAM, addr);
  for (uint16_t i = 0; i < len; i++) FlashSPI.transfer(data[i]);
  cs_high();
  return flash_wait_ready(50);    // datasheet: typ. 0.4ms, max ~3ms
}

void flash_read(uint32_t addr, uint8_t* buf, uint32_t len) {
  cs_low();
  send_cmd_addr(CMD_READ_DATA, addr);
  for (uint32_t i = 0; i < len; i++) buf[i] = FlashSPI.transfer(0x00);
  cs_high();
}
