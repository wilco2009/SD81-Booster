## What's new

- **Network turn-based games, without any server of our own.** Two SD81 Booster
  can play over the internet through the WiFi module: both interfaces connect
  as clients to a public MQTT server and send each other the moves. New in
  `Software.zip`: **Connect 4** and **chess** (full rules except threefold
  repetition, with an optional 5/10/15/30-minute clock per player), both drawn
  in high resolution (Superfast HiRes Spectrum), and a test chat, `NETCHAT`.
  You create a room, get a 10-character code, and give it to your opponent.
  - **Play on another day.** Each game saves its state to the SD card
    (`CONNECT4.SAV`, `CHESS.SAV`, in the game's folder) every time it changes.
    After switching the ZX81 off, the menu offers *3 = CONTINUE SAVED GAME*: it
    goes back into the same room and the same seat, and the game carries on.
    The two players never need to be online at the same time (the server keeps
    each player's last move until the other picks it up).
  - Documented in the manual: new section 9.9 (how to play) and new Appendix M
    (for programmers: the MQTT and cryptographic AT commands, `/SYS/NET.CFG`,
    rooms and messages, and the `net.inc` and `game.inc` libraries to write
    your own game).
- **WiFi module: MQTT, SHA-256, HMAC and random numbers as AT commands**
  (`AT+MQTT`, `AT+MQTTSUB`, `AT+MQTTPUB`, `AT+MQTTRX`, `AT+SHA256`, `AT+HMAC`,
  `AT+RAND`...), usable from the TELNET terminal or from your own program. The
  servers are listed in the optional `/SYS/NET.CFG` (three public ones by
  default).
- **64 hardware sprites** (it was 32): `LOAD *SPRITE` up to 63, a second table
  in the shadow RAM, and up to **12 sprites per line** (it was 8). The ROM
  clears all 64 sprites at reset.
- **Debugger, now also from the web page and with CP/M.**
  - The `/debug` page has a **screen capture** of what the ZX81 is showing (all
    video modes, with Chroma81 colour and sprites) and a **ZX81 keyboard**
    (clicks, or your own keyboard; it also drives the debugger screen). The
    video state is now read from the FPGA, so captures and snapshots are exact.
  - `sym !` (and the Symbols button of the page) reloads the symbol file; a
    warning in red appears if you try to use a stopped-program command while
    the program is running. Opening the page with the module's **IP address**
    is much faster than `sd81booster.local` on some computers.
  - **CP/M**: the debugger screen, snapshots and the trace work with block 0 in
    RAM (CP/M's), and the control POKEs and sprites are left alone while the
    debugger is on screen.
  - Snapshots keep every page the program has written since the last reset
    (a `LOAD` no longer clears the record), so programs that load their data
    from the SD card in several goes are saved complete.
- **`LOAD *Z81` only through the debugger monitor.** The old stand-alone loader
  (command 70) is gone: without `/SYS/DEBUG.BIN`, with the debugger stopped or
  with a snapshot in progress, `LOAD *Z81` now gives **report P**.
- **WiFi module update, in one go.** The update file is now **`ESP32FW.BIN`**
  (no underscore: the ZX81 has no such character and could not delete a half
  copied file). An older module still finds it (the new interface hands it the
  file under the old name). The new module waits up to 2 minutes for the
  interface, so you can update STM32 + FPGA + WiFi module at the same time, and
  `/MAN/IP.TXT` is written without needing a second reset.
- **Fixes.** WAV files with 32 bits per sample now play correctly (an operator
  precedence bug); the voice synthesizer interrupt returns the sample while it
  speaks; deleting a watchpoint from the web page refreshes the panel.
- **Manual:** sections 9.9 and Appendix M (network games and MQTT), *LOAD *Z81*
  without the stand-alone loader and report P, CP/M and the debugger, what `snap`
  saves, `ESP32FW.BIN`, the web debugger, and the version history up to v1.7.

## Included firmware

| Component     | Version | File            |
|---------------|---------|-----------------|
| STM32         | 2.7     | `firmware.bin`  |
| ROM (Z80)     | 1.8     | `SDBOOST.ROM`   |
| FPGA          | 1.6     | `sd81.mcs`      |
| WiFi (ESP32)  | 1.4     | `ESP32FW.BIN`   |

All four components changed in this release, and the debugger needs the four of
them at the same version: update the whole set from this release. The ROM and
the STM32 firmware must go together (the ROM expects the new answer of
`LOAD *Z81`). Also copy `SYS/DEBUG.BIN` (in `SD Content.zip`) to the SD card:
without it `LOAD *Z81` no longer works.

A WiFi module older than 1.4 waits only 15 seconds for the interface at start-up:
if the same start-up also updates the STM32 firmware and the FPGA, it does not
get to see `ESP32FW.BIN`, and updates on the next start-up (the file stays on
the SD card).

Normal updates are done 100% via the SD card, no cables or external
programming tools required — see `Firmware.zip` below. `bootloader.bin` is
only needed for the separate, rarely-used USB recovery procedure (see
`Tools.zip`).
