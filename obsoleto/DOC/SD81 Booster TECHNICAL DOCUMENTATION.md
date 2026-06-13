# SD81 Booster — Technical Documentation

## Command Description

The Z80 communicates with the MCU via I/O ports. Each command starts with a command byte sent from the Z80 to the SD81, optionally followed by parameters. The MCU then responds with data and/or an error code.

**Pascal string format:** `< len (1 byte) > < char array up to 255 bytes >`

---

### NOP (00h)

No operation.

Z80 → SD81: `00h`

Z80 ← SD81: nothing

---

### VER (01h)

Get firmware version.

Z80 → SD81: `01h`

Z80 ← SD81: `< version (1 byte, BCD) >`

- Major version = value / 16
- Minor version = value % 16

---

### PWD (02h)

Get current directory path.

Z80 → SD81: `02h`

Z80 ← SD81: string transfer sequence (see GET NEXT CHAR). String terminated with `FFh`.

---

### CD (03h)

Change directory.

Z80 → SD81: `03h, < dir name (pascal string) >`

Z80 ← SD81: `< error code >`

---

### DEL (04h)

Delete file.

Z80 → SD81: `04h, < file name (pascal string) >`

Z80 ← SD81: `< error code >`

---

### MD (05h)

Make directory.

Z80 → SD81: `05h, < dir name (pascal string) >`

Z80 ← SD81: `< error code >`

---

### RD (06h)

Remove directory.

Z80 → SD81: `06h, < dir name (pascal string) >`

Z80 ← SD81: `< error code >`

---

### MOVE (07h)

Move or rename a file.

Z80 → SD81: `07h, < current name (pascal string) >, < new name (pascal string) >`

Z80 ← SD81: `< error code >`

---

### COPY (08h)

Copy a file.

Z80 → SD81: `08h, < source name (pascal string) >, < destination name (pascal string) >`

Z80 ← SD81: `< error code >`

---

### LOAD (09h)

Load file. The behaviour depends on the file extension:

| Extension | Behaviour |
|-----------|-----------|
| `.P`, `.81` | Loads ZX81 program. File size is calculated from the system variables stored in bytes 11–12 of the file. |
| `.P81` | Multi-program format. Skips the filename header before reading the system variables. |
| `.ROM` | Loads at address 0, resets Z80 and FPGA. Does not return control to BASIC. |
| `.WAV` | Plays audio instead of loading data. |
| *(none / other)* | Loads the entire file verbatim. |

Z80 → SD81: `09h, < len (1 byte) >, < file name (string[len]) >`

Z80 ← SD81: `< low byte of file len > < high byte of file len > < bytes > < error code >`

---

### SAVE (0Ah)

Save file.

Z80 → SD81: `0Ah, < len (1 byte) >, < file name (string[len]) >, < low byte of file len >, < high byte of file len >, < bytes >`

Z80 ← SD81: `< error code >`

---

### TYPE (0Bh)

Display a text file on screen (implements `LOAD THEN PRINT`).

- Prefix `*` in the filename: searches in `/MAN/<name>.TXT` on the SD (built-in help system, similar to the `man` command in Linux).
- Prefix `?` in the filename: treats the file content as native ZX81 character encoding (no conversion).

Z80 → SD81: `0Bh, < len (1 byte) >, < file name (string[len]) >`

Z80 ← SD81: string transfer sequence. String terminated with `FFh`, followed by `< error code >`.

---

### DIR OPEN (0Ch)

Start a directory listing operation. After this command, use GET NEXT CHAR (0Dh) to retrieve the listing string. Directories are shown between `<` and `>`.

Z80 → SD81: `0Ch, < dir + wildcards (pascal string) >`

Z80 ← SD81: `< error code >`, then string transfer sequence.

---

### GET NEXT CHAR (0Dh)

Get the next character in a string transfer sequence.

Z80 → SD81: `0Dh`

Z80 ← SD81: `< next char >`

`FFh` signals end of string; a status byte follows immediately after `FFh`.

A non-`0Dh` command terminates the sequence immediately.

---

### FREE TXT (0Eh)

Return free SD space as a human-readable string.

Z80 → SD81: `0Eh`

Z80 ← SD81: string transfer sequence. String terminated with `FFh`.

---

### FREE (0Fh)

Return free SD space as raw 32-bit values.

Z80 → SD81: `0Fh`

Z80 ← SD81:
```
< SD total size (4 bytes, little-endian) >
< SD free space (4 bytes, little-endian) >
```

---

### OPENDIR2 (10h)

Build an internal indexed array of directory entries (up to 512 entries). Used together with GETROWLEN and GETROW. The array stores internal SdFat directory indices, not filenames directly.

Z80 → SD81: `10h, < dir + wildcards (pascal string) >`

Z80 ← SD81: `< error code >`

---

### GETROWLEN (11h)

Return the filename length for a given entry in the directory array built by OPENDIR2.

Z80 → SD81: `11h, < index low (1 byte) >, < index high (1 byte) >`

The index is the position in the internal array (0-based), not a directory entry number.

Z80 ← SD81: `< len (1 byte) > < error code >`

---

### GETROW (12h)

Return the filename for a given entry in the directory array built by OPENDIR2.

- Index 0: returns the **current directory path** (not a directory entry).
- Other indices: returns the filename. Directories are enclosed between `<` and `>`.

Z80 → SD81: `12h, < index low (1 byte) >, < index high (1 byte) >`

Z80 ← SD81: `< filename (pascal string) >`

---

### ENABLE_M1NOT (13h)

Enable MC45 mode (allows running any Z80 opcode in the 8000h–BFFFh range).

Z80 → SD81: `13h`

Z80 ← SD81: nothing

---

### DISABLE_M1NOT (14h)

Disable MC45 mode.

Z80 → SD81: `14h`

Z80 ← SD81: nothing

---

### JOY (15h)

Configure the DB9 joystick port key mapping.

Z80 → SD81: `15h, < 5 bytes in native ZX81 key codes >`

The 5 bytes correspond to: left / right / up / down / fire, in that order. Unlike most other commands that receive strings in ZX81 encoding and convert them to ASCII internally, these bytes are used as-is (native ZX81 codes, not ASCII).

Z80 ← SD81: `< error code >`

---

### BINARY SAY (16h)

Send a raw array of SP0256 allophone bytes to the speech synthesizer.

Z80 → SD81: `16h, < len (1 byte) >, < array of allophone bytes >`

Z80 ← SD81: `< error code >`

See `speak & AY synthetiser.md` for the allophone table.

---

### SAY (17h)

Send an English text string to the speech synthesizer. The MCU converts the text to SP0256 phonemes internally using a built-in dictionary. Numbers are read aloud automatically.

Prefix the string with `*` for background playback (the Z80 continues executing while speech plays).

Z80 → SD81: `17h, < len (1 byte) >, < ASCII text string >`

Z80 ← SD81: `< error code >`

---

### AY SET REG (18h)

Write a value to an AY-3-8910/12 register.

Z80 → SD81: `18h, < register (1 byte) >, < value (1 byte) >`

Z80 ← SD81: `< error code >`

---

### AY GET REG (19h)

Read the value of an AY-3-8910/12 register.

Z80 → SD81: `19h, < register (1 byte) >`

Z80 ← SD81: `< value (1 byte) > < error code >`

---

### PLAY (1Ah)

Play a music string using the AY synthesizer (PLAY command).

Z80 → SD81: `1Ah, < len (1 byte) >, < music string >`

Z80 ← SD81: `< error code >`

---

### SET 128CHARS (1Bh)

Activate standard 128-character user-defined character mode. In this mode the character bitmap address is taken from the Z80 **I register**, as in the original ZX81 UDG mechanism extended to 128 characters.

This is distinct from the QuickSilva mode, which uses a fixed memory address independent of the I register and can only be toggled by pressing the **QSILVA button** (located below the reset button on the interface).

Z80 → SD81: `1Bh`

Z80 ← SD81: nothing

---

### SET 64CHARS (1Ch)

Switch back to standard 64-character user-defined character mode.

Z80 → SD81: `1Ch`

Z80 ← SD81: nothing

---

### SET FULLPAGING (1Dh)

Activate full memory paging mode: 512 KB RAM, 64 pages of 8 KB each.

This command is used internally by the ROM extension when `LOAD *MAP` is called with a page number greater than 32. It is not directly accessible as a standalone BASIC command, but can be issued independently from machine code.

Z80 → SD81: `1Dh`

Z80 ← SD81: nothing

---

### HALFPAGING (1Eh)

Activate simplified memory paging mode: 256 KB RAM, 32 pages of 8 KB each.

Z80 → SD81: `1Eh`

Z80 ← SD81: nothing

---

### GETBYTE (20h)

Read a byte from the MCU internal storage.

- Indices 0–127: volatile variables (`sys_vars[]`), lost on reset.
- Indices 128–255: EEPROM (persistent across resets). Index 128 maps to EEPROM position 0.

Z80 → SD81: `20h, < index (1 byte) >`

Z80 ← SD81: `< value (1 byte) >`

---

### SETBYTE (21h)

Write a byte to the MCU internal storage. Same index mapping as GETBYTE.

Z80 → SD81: `21h, < index (1 byte) >, < value (1 byte) >`

Z80 ← SD81: nothing

---

### PLAY_VGM (22h)

Play a VGM music file in the background. Only AY-3-8910/12 opcodes are supported.

Z80 → SD81: `22h, < len (1 byte) >, < file name (string[len]) >`

Z80 ← SD81: `< error code >`

---

### STOP_VGM (23h)

Stop the currently playing VGM file.

Z80 → SD81: `23h`

Z80 ← SD81: nothing

---

### PAUSE_VGM (24h)

Pause the currently playing VGM file.

Z80 → SD81: `24h`

Z80 ← SD81: nothing

---

### CONT_VGM (25h)

Resume a paused VGM file.

Z80 → SD81: `25h`

Z80 ← SD81: nothing

---

### LOOP_VGM (26h)

Set the VGM playback loop mode.

Z80 → SD81: `26h, < mode (1 byte) >`

- `0` = no loop (play once)
- Any other value = loop continuously

Z80 ← SD81: nothing

---

### LOAD_PEG (28h)

Load PEG (Programmable Effect Generator) bytecode into PEG memory at the specified address.

Z80 → SD81: `28h, < addr (1 byte) >, < len (1 byte) >, < PEG bytecode >`

Z80 ← SD81: nothing

---

### PLAY_PEG (29h)

Start a PEG thread executing the code at the specified address.

Z80 → SD81: `29h, < thread (1 byte) >, < addr (1 byte) >`

Z80 ← SD81: nothing

---

### STOP_PEG (2Ah)

Stop the specified PEG thread.

Z80 → SD81: `2Ah, < thread (1 byte) >`

Z80 ← SD81: nothing

---

### PAUSE_PEG (2Bh)

Pause the specified PEG thread.

Z80 → SD81: `2Bh, < thread (1 byte) >`

Z80 ← SD81: nothing

---

### CONT_PEG (2Ch)

Resume a paused PEG thread.

Z80 → SD81: `2Ch, < thread (1 byte) >`

Z80 ← SD81: nothing

---

### SDLOAD_PEG (2Dh)

Load a PEG bytecode file from the SD card into PEG memory.

Z80 → SD81: `2Dh, < len (1 byte) >, < file name (string[len]) >, < addr (1 byte) >`

Z80 ← SD81: `< error code >`

---

### ENABLE_ICHR (2Eh) *(obsolete)*

Previously used to activate the internal HRG character generator. The function body is completely commented out in the current firmware — this command does nothing. HRG mode is now transparent and requires no activation.

Z80 → SD81: `2Eh`

Z80 ← SD81: nothing

---

### DISABLE_ICHR (2Fh) *(obsolete)*

Previously used to deactivate the internal HRG character generator. Same as above — does nothing in the current firmware.

Z80 → SD81: `2Fh`

Z80 ← SD81: nothing

---

### ENABLE_48K (30h)

Activate extended 48 KB RAM mode.

Z80 → SD81: `30h`

Z80 ← SD81: nothing

---

### DISABLE_48K (31h)

Deactivate extended 48 KB RAM mode, restoring standard compatibility.

Z80 → SD81: `31h`

Z80 ← SD81: nothing

---

### RTC (32h)

Real-time clock access.

**Read current date/time:**

Z80 → SD81: `32h` (no parameters)

Z80 ← SD81: string transfer sequence with current date/time. String terminated with `FFh`.

**Set date/time:**

Z80 → SD81: `32h, < len (1 byte) >, < date/time string >`

Accepted formats:
| Format | Example |
|--------|---------|
| `YYYY-MM-DD HH:MM:SS.CC` | `2025-05-04 13:45:00.00` |
| `YYYY-MM-DD HH:MM:SS` | `2025-05-04 13:45:00` |
| `YYYY-MM-DD` | `2025-05-04` |
| `HH:MM:SS.CC` | `13:45:00.00` |
| `HH:MM:SS` | `13:45:00` |
| `HH:MM` | `13:45` |

Z80 ← SD81: `< error code >`

---

### BAT (34h)

Read the RTC backup battery voltage.

Z80 → SD81: `34h`

Z80 ← SD81: 5 ASCII bytes in the format `V.mmm` (e.g. `3.012` for 3.012 V), followed by `< error code >`.

---

## Error Codes

| Code | Value | Description |
|------|-------|-------------|
| `0` | 0 | OK — no error |
| `C` | — | Command not recognised |
| `G` | 1 | File or directory does not exist |
| `H` | 2 | Not a directory |
| `I` | 3 | Generic error |
| `J` | 4 | Destination file already exists |
| `K` | 5 | File too large |
| `L` | 6 | Cannot create destination file |
| `M` | 7 | Write error |
| `N` | 8 | Read error |
| `O` | 9 | Out of range in a PLAY command |
| `P` | 10 | Invalid note name |
| `Q` | 11 | Invalid file format |
| `R` | 12 | File not opened |
| `S` | 13 | Forbidden operation (e.g. write attempt inside a T81 directory) |

---

## MCU Software

The MCU firmware is written using the **Arduino framework for STM32** (STM32duino), targeting the **STM32F407VET** microcontroller. The Arduino IDE or Arduino CLI can be used to build and modify it.

Board package required: **STM32 by STMicroelectronics** (install via Board Manager).

Target board: `Generic STM32F4 Series` → `Generic F407VETx`

The current firmware source is located in:
`FIRMWARE/SD81BoosterV2_038_STM32/`

### Library dependencies

- **SdFat** 2.2.0 by Bill Greiman — SD card access (install via Library Manager).
- **STM32duino RTC** — Real-time clock support for STM32 (install via Library Manager).

### Firmware binaries

The compiled firmware consists of two parts:

| Part | File | Flash address |
|------|------|--------------|
| Bootloader | `FIRMWARE/bootloader.bin` | `0x08000000` |
| Application | `FIRMWARE/firmware.bin` | `0x0800C000` |

---

## Updating the Firmware

See [FIRMWARE/README_update.md](../FIRMWARE/README_update.md) for full instructions.

**Normal update (via SD bootloader):**
1. Copy the application binary renamed as `firmware.bin` to the root of the SD card.
2. Power on the ZX81. The bootloader detects the file, flashes it at `0x0800C000`, deletes it, and boots the application.

**Emergency recovery (via USB, for advanced users):**
1. Open the interface case.
2. Bridge the two upper pins of jumper **JP7** (on the component side, next to the USB-C port).
3. Connect USB-C to a PC.
4. Use **STM32CubeProgrammer** to flash both parts at their respective addresses.
5. Remove the JP7 bridge before closing the case.

---

## Programming the FPGA

The FPGA (Xilinx Spartan-6 XC6SLX9) loads its configuration at power-up from an auxiliary SPI flash memory chip (25Q128). Programming is done by writing an MCS file to this SPI flash via JTAG, using the FPGA as a bridge. This is only required for manufacturing or repair — normal users do not need to do this.

### JTAG connector

Connect a Xilinx USB Platform Cable (or compatible clone) to the JTAG header on the board, in this order from top to bottom:

TMS — TDI — TDO — TCK — GND — VREF

The board must be powered during programming (e.g. via the USB-C connector).

### Using Xilinx ISE iMPACT — SPI flash indirect programming

1. Open iMPACT and double-click **Boundary Scan**.
2. Right-click in the Boundary Scan window and select **Initialize Chain**. The FPGA (`XC6SLX9`) should appear.
3. Right-click the FPGA and select **Add SPI/BPI Flash**. Browse to `FIRMWARE/SD81Booster.mcs` and select it.
4. When prompted, select the flash device: **SPI PROM → 25Q128** (128 Mbit).
5. Right-click the flash and select **Program**.
6. If successful, **PROGRAM SUCCEEDED** is displayed.
7. Power-cycle the board. The FPGA will load its configuration from the SPI flash automatically.

ISE Design Suite (which includes iMPACT) is available for Windows and Linux from the [Xilinx archive](https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/vivado-design-tools/archive-ise.html).

---

## Status LED (STAT)

The STAT LED is located on the **top panel** of the interface.

### During firmware update

| LED state | Meaning |
|-----------|---------|
| Blue/Red fast blinking | Error initialising SD |
| Yellow fixed | Flashing firmware |
| White/Red fast blinking | Firmware update error |

### During boot

| LED state | Meaning |
|-----------|---------|
| Red blinking | Initialising serial port |
| Orange blinking | Waiting for FPGA |
| Blue/Red fast blinking | Error initialising SD |
| Orange/Red fast blinking | Error writing ROM to RAM |
| Yellow blinking | Initialising RTC |
| Green fixed | System initialised correctly |

### During normal operation

| LED state | Meaning |
|-----------|---------|
| Green fixed | Interface ready, QuickSilva mode off |
| Cyan fixed | Interface ready, QuickSilva mode on |
