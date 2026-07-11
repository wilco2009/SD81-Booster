#ifndef FLASH_UPDATE_H
#define FLASH_UPDATE_H

// Orchestrates reprogramming the FPGA configuration flash (W25Q128) from an
// Intel HEX (.mcs) file on the SD card, combining FLASH_SPI + IHEX_PARSER.
//
// Two streaming passes over the file (nothing is held in RAM):
//   pass 1 — erase each 4KB sector on first touch, program in 256B pages
//   pass 2 — re-read the file and compare every byte against the flash
//
// Preconditions (caller's responsibility):
//   - SD card already mounted (SD_Init() done)
//   - Called AFTER the normal boot sequence (FPGAPROG already released back
//     to HIGH). Once DONE, Spartan-6 releases its dedicated config pins
//     back to high-Z since this design doesn't reuse them as user I/O, so
//     the STM32 can drive the SPI2 bus to the flash chip without contention
//     — confirmed working on real hardware with FPGAPROG left HIGH.
//   - Caller decides what to do with the result: on FLASHUPD_OK, delete the
//     .mcs file and reset the MCU so the FPGA reloads the new bitstream on
//     the next boot (this module does not reset anything itself).

enum FlashUpdateStatus {
  FLASHUPD_OK = 0,
  FLASHUPD_NO_FILE,        // path doesn't exist — informational, not an error
  FLASHUPD_ERR_NO_FLASH,   // JEDEC id mismatch: bus not acquired / wrong chip
  FLASHUPD_ERR_HEX,        // file unreadable, malformed or bad checksum
  FLASHUPD_ERR_PROGRAM,    // an erase or page-program operation failed
  FLASHUPD_ERR_VERIFY,     // flash contents do not match the file
};

FlashUpdateStatus flash_update_from_mcs(const char* path);

#endif
