# SD81 Booster — Firmware Update

This document explains how to update the SD81 Booster firmware.

> If you want to recompile the firmware from source, see [COMPILING.md](COMPILING.md) instead.

---

## Firmware structure

The SD81 Booster has two programmable components, each updated independently:

### MCU (STM32F407VET)

The MCU firmware consists of two independent parts, each flashed to a separate region:

| Part | Binary | Flash address | Description |
|------|--------|--------------|-------------|
| **Bootloader** | `bootloader.bin` | `0x08000000` | Runs first on power-on. Checks the SD card for a `firmware.bin` file and flashes it automatically. Occupies sectors 0–2 (48 KB). |
| **Application** | `firmware.bin` | `0x0800C000` | The main firmware. Loaded by the bootloader after update, or directly on normal boot. |

Under normal circumstances, **only the application needs to be updated**. The bootloader rarely changes.

### FPGA (Xilinx Spartan-6 XC6SLX9)

| File | Description |
|------|-------------|
| `SD81.mcs` | MCS image to be written to the auxiliary SPI flash (25Q128) connected to the FPGA. |

The FPGA loads its configuration from this SPI flash at power-up. See [FPGA update (via SD card)](#fpga-update-via-sd-card) below for update instructions.

All binaries are included in this folder.

---

## Normal update (via SD card)

This is the recommended method for all users.

1. Copy `firmware.bin` from this folder to the root of the microSD card.
2. Insert the SD card into the interface.
3. Power on the ZX81.
4. The bootloader detects `firmware.bin`, flashes it to `0x0800C000`, deletes the file from the SD card, and boots the new application automatically.

The STAT LED shows the following during the process:

| LED state | Meaning |
|-----------|---------|
| Yellow fixed | Flashing in progress — **do not power off** |
| Green fixed | Update successful, application running |
| Blue/Red blinking | Error reading SD card |
| White/Red blinking | Flash write error |

### Verifying the update

Once the ZX81 has booted, run the following command to check the firmware version:

```
LOAD *VER
```

---

## Emergency recovery (via USB)

Use this method only if the normal SD update fails or the application is corrupted and the interface does not boot correctly. Requires opening the case.

**Requirements:**
- [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html) (free download from ST)
- A USB-C cable

**Steps:**

1. **Disconnect** the interface from the ZX81.
2. Open the interface case to access the PCB.
3. Locate jumper **JP7** on the component side of the board, next to the USB-C port.
4. Bridge the **two upper pins** of JP7 to put the MCU in programming mode.
5. Connect the USB-C cable to the interface and to the PC.
6. Wait for the PC to recognise the device (it should appear as a DFU device in STM32CubeProgrammer).
7. Flash the two binaries in order:

| Step | Binary | Address |
|------|--------|---------|
| 1 | `bootloader.bin` | `0x08000000` |
| 2 | `firmware.bin` | `0x0800C000` |

   Two ways to do this — no Arduino IDE or source build needed either way, only STM32CubeProgrammer:

   - **Scripted (recommended):** double-click `bootloader.bat` and/or `firmware.bat` in this folder. Each one calls STM32CubeProgrammer's command-line tool (via `stm32CubeProg.sh`, run through the included `busybox.exe`) to flash the matching binary to the right address over DFU automatically.
   - **Manual (STM32CubeProgrammer GUI):** connect in DFU mode and download each binary to its address from the table above.

8. Once flashing is complete, **remove the JP7 bridge**.
9. Close the case and reconnect the interface to the ZX81.

> **Note:** Only flash the bootloader if it is known to be corrupted. In most cases, flashing only the application (`0x0800C000`) is sufficient — just run `firmware.bat` (or download only `firmware.bin` in the GUI).

---

## RTC initialization (required after updating the MCU firmware)

On the first boot after updating the MCU firmware (application), the RTC (Real Time Clock) needs to be initialized:

1. Power off the interface.
2. Press and hold the **QuickSilva button** (left side of the interface — the second button from the top, i.e. the one closest to the user).
3. Power on the interface while keeping the button held, for **at least 5 seconds**.
4. Release the button. The STAT LED lights solid yellow for half a second, then solid blue for half a second, and — if everything goes well — the interface continues the normal boot process until the STAT LED turns solid green.

The date and time are set to a fixed value in 2025, which you can then update with `LOAD *RTC` from the ZX81.

---

## MCU diagnostics via USB serial console

If an update fails, or the interface behaves oddly and you want to see what is actually happening, connect the USB-C port to a PC (**normal connection, JP7 does not need to be bridged for this**) and open a serial terminal — the Arduino IDE's own Serial Monitor works, or any other terminal program (PuTTY, Tera Term, minicom, CoolTerm, `screen`...).

**Terminal settings:**

| Setting | Value |
|---------|-------|
| Baud rate | 115200 |
| Data bits | 8 |
| Parity | None |
| Stop bits | 1 |

(i.e. **115200 8N1**.)

The console shows boot progress, SD access errors, firmware update progress, and other MCU status/debug messages in real time.

---

## FPGA update (via SD card)

This is the recommended method — no JTAG cable or Xilinx tools required.

Just like the MCU, the SD81 Booster can reprogram the FPGA's auxiliary SPI flash (25Q128) automatically from the microSD card.

1. Copy `SD81.mcs` from this folder to the root of the microSD card.
2. Insert the SD card into the interface with the ZX81 powered off.
3. Turn on the ZX81. The system detects the file, reprograms the FPGA flash, and deletes it from the SD card once finished.

> ⚠️ **Warning:** Do not power off the ZX81 or remove the SD card during the update. If the process is interrupted, the system detects this and automatically retries on the next boot — `SD81.mcs` is not deleted from the SD card until the update has been confirmed to complete successfully.

---

## FPGA programming via JTAG

Use this method only if the SD card update above fails or is not available. It is intended for **advanced users and manufacturers only**, and requires a Xilinx USB Platform Cable (or compatible clone) and Xilinx ISE iMPACT.

The FPGA (Spartan-6 XC6SLX9) does not store its configuration internally. It loads from an auxiliary SPI flash chip (25Q128) at every power-up. Programming means writing `SD81Booster.mcs` to that flash via JTAG indirect programming.

> ⚠️ **Warning:** Incorrect programming may render the interface permanently inoperative.

**Steps:**

1. Connect the JTAG cable to the JTAG header on the board (TMS — TDI — TDO — TCK — GND — VREF).
2. Power the board via USB-C.
3. Open **Xilinx ISE iMPACT** and double-click **Boundary Scan**.
4. Right-click in the Boundary Scan window → **Initialize Chain**. The FPGA (`XC6SLX9`) should appear.
5. Right-click the FPGA → **Add SPI/BPI Flash**. Browse to `SD81Booster.mcs` and select it.
6. When prompted for the flash device, select **SPI PROM → 25Q128** (128 Mbit).
7. Right-click the flash → **Program**.
8. Wait for **PROGRAM SUCCEEDED**.
9. Power-cycle the board. The FPGA will load its new configuration automatically.

---

## WiFi module update (optional accessory, ESP32-C3)

Only applies if your interface has the optional WiFi module installed.

### Normal update (via SD card)

1. Copy `ESP32_FW.BIN` to the root of the microSD card.
2. Insert the SD card into the interface and power-cycle the ZX81 (or just insert the card if it is already powered — the module checks for the file after a fresh boot).
3. Wait for the STAT LED to turn **solid green**. During the update the LED blinks pink; it turns solid green once finished successfully, or stays solid yellow if it failed (try again by copying the file once more).

> ⚠️ **Warning:** Do not power off the interface or remove the SD card while the STAT LED is blinking pink.

### Initial / recovery programming (via USB, Arduino IDE)

The module needs to be programmed over USB **the first time** (or if it becomes unresponsive and the SD update above can't run). After that, all further updates can use the SD card method.

**Requirements:**
- [Arduino IDE](https://www.arduino.cc/en/software) with ESP32 board support installed
- A USB cable to the ESP32-C3 module

**Board settings** (Tools menu):

| Setting | Value |
|---------|-------|
| Board | **ESP32C3 Dev Module** (under esp32 boards, installed via Boards Manager) |
| USB CDC On Boot | Disabled |
| CPU Frequency | 160MHz (WiFi) |
| Flash Frequency | 80MHz |
| Flash Mode | QIO |
| Flash Size | 4MB (32Mb) |
| Upload Speed | 921600 |
| Core Debug Level | None |
| JTAG Adapter | Disabled |
| Zigbee Mode | Disabled |

Leave every other option (Erase All Flash Before Sketch Upload, Partition Scheme, etc.) at the Arduino IDE's own default for this board. Select the correct **Port** for the module, then upload the WiFi module firmware sketch as usual.
