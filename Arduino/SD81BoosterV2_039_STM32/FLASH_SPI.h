#ifndef FLASH_SPI_H
#define FLASH_SPI_H
#include <stdint.h>

// Low-level driver for the FPGA configuration SPI flash (Winbond W25Q128JVSIQ,
// 16 MB, JEDEC ID EF4018h). Uses a dedicated SPI bus (FLASH_SCLK/MISO/MOSI/CS,
// PB10/PC2/PC3/PC4 = STM32 SPI2 pins) separate from the SD card's SPI bus.
//
// Only usable while the FPGA is held in configuration reset (FPGAPROG=LOW),
// which tri-states its own connections to this bus and lets the STM32 act as
// SPI master towards the flash chip.

#define FLASH_PAGE_SIZE     256UL
#define FLASH_SECTOR_SIZE   4096UL
#define FLASH_JEDEC_ID      0xEF4018UL   // expected manufacturer/type/capacity

// FPGA bitstream version marker: a single byte (high nibble=major, low
// nibble=minor) stamped at the very last address of the 16 MB chip by
// FPGA/SD81V2.1000/append_fpga_version.py, right after bitgen/promgen
// produce the real .mcs. Far outside anything the real bitstream (a few
// hundred KB) or flash_update_from_mcs() ever touches.
#define FPGA_VERSION_FLASH_ADDR 0xFFFFFFUL

// Acquire/release the SPI2 bus and FLASH_CS pin. Call flash_begin() only
// while FPGAPROG is held LOW (FPGA not driving these lines).
void flash_begin();
void flash_end();

uint32_t flash_read_jedec_id();
uint8_t  flash_read_status();
bool     flash_wait_ready(uint32_t timeout_ms = 1000);
void     flash_write_enable();

// Erases the 4KB sector containing 'addr'. Blocks until complete.
bool flash_sector_erase(uint32_t addr);

// Programs up to FLASH_PAGE_SIZE bytes at 'addr'. 'addr'..'addr'+len-1 must
// not cross a page boundary (caller's responsibility). Blocks until complete.
bool flash_page_program(uint32_t addr, const uint8_t* data, uint16_t len);

// Plain read, no page-size restriction.
void flash_read(uint32_t addr, uint8_t* buf, uint32_t len);

#endif
