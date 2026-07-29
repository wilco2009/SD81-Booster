#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
append_fpga_version.py — stamps the FPGA version into a compiled .mcs file.

Run this AFTER bitgen/promgen produces the real SD81.mcs (or sd81boster.mcs),
right before publishing it as the file that ships / gets copied to the SD
card for the "LOAD *FGA" self-update flow.

What it does
------------
Reads FPGAVERSION.TXT ("MAJOR.MINOR", e.g. "1.0"), packs it into a single
byte (high nibble = major, low nibble = minor — same convention as the ROM
and MCU version bytes, see LOAD *VER / LOAD *FGA), and appends it to the
.mcs as two extra Intel HEX records at a fixed address near the very end of
the 16 MB (W25Q128) SPI flash chip: 0xFFFFFF, the chip's last byte.

Why the last byte and not right after the bitstream: the real bitstream is
only a few hundred KB, nowhere near 16 MB, so the marker sits far outside
anything flash_update_from_mcs() (the streaming MCU-side updater) would
otherwise touch — it only erases/programs the 4KB sectors the .mcs file
itself references, so adding data at 0xFFFFFF is 100% additive and safe.
Nothing else in the design uses that region.

The MCU reads this same byte directly over SPI from the config flash during
boot (see FLASH_SPI.h / FPGA_VERSION_FLASH_ADDR) — no FPGA-side logic or new
Z80-visible I/O port is involved at all.

Usage
-----
    python append_fpga_version.py [mcs_path] [version_txt_path]

Both arguments are optional:
    mcs_path          default: SD81.mcs in the current directory
    version_txt_path  default: FPGAVERSION.TXT in the current directory

Idempotent: running it twice on the same file (e.g. after bumping the
version and re-running) replaces the previous marker instead of duplicating
it, by recognising and stripping a record pair that matches what this
script itself would have written.
"""
import sys
import os

FLASH_LAST_BYTE = 0xFFFFFF  # last byte of the 16 MB (W25Q128) config flash


def ihex_checksum(byte_count, addr_hi_lo, rec_type, data):
    total = byte_count + (addr_hi_lo >> 8) + (addr_hi_lo & 0xFF) + rec_type + sum(data)
    return (0x100 - (total & 0xFF)) & 0xFF


def make_record(addr16, rec_type, data):
    byte_count = len(data)
    cksum = ihex_checksum(byte_count, addr16, rec_type, data)
    payload = "".join(f"{b:02X}" for b in data)
    return f":{byte_count:02X}{addr16:04X}{rec_type:02X}{payload}{cksum:02X}"


def make_marker_records(version_byte):
    addr_hi16 = (FLASH_LAST_BYTE >> 16) & 0xFFFF   # extended linear address (bits 31:16)
    addr_lo16 = FLASH_LAST_BYTE & 0xFFFF            # low 16 bits used by the data record
    ext_addr_record = make_record(0x0000, 0x04, [(addr_hi16 >> 8) & 0xFF, addr_hi16 & 0xFF])
    data_record = make_record(addr_lo16, 0x00, [version_byte])
    return ext_addr_record, data_record


def read_version_byte(version_txt_path):
    with open(version_txt_path, "r", encoding="utf-8") as f:
        text = f.read().strip()
    major_s, _, minor_s = text.partition(".")
    major, minor = int(major_s), int(minor_s or "0")
    if not (0 <= major <= 15 and 0 <= minor <= 15):
        raise ValueError(
            f"Version {text!r} does not fit in a nibble each (0-15.0-15)."
        )
    return (major << 4) | minor, major, minor


def strip_previous_marker(lines):
    """Removes a previously-inserted (ext-addr, data) record pair, if present,
    so re-running this script updates the marker instead of duplicating it."""
    out = []
    i = 0
    while i < len(lines):
        line = lines[i]
        if (
            i + 1 < len(lines)
            and line.startswith(":02000004")
            and lines[i + 1].startswith(f":01{FLASH_LAST_BYTE & 0xFFFF:04X}00")
        ):
            # Only strip if the extended address record actually points at
            # our target's high 16 bits (avoid touching unrelated 04 records).
            hi16 = (FLASH_LAST_BYTE >> 16) & 0xFFFF
            if line[9:13].upper() == f"{hi16:04X}":
                i += 2  # drop both lines
                continue
        out.append(line)
        i += 1
    return out


def main():
    mcs_path = sys.argv[1] if len(sys.argv) > 1 else "SD81.mcs"
    version_txt_path = sys.argv[2] if len(sys.argv) > 2 else "FPGAVERSION.TXT"

    if not os.path.isfile(mcs_path):
        print(f"error: {mcs_path} not found", file=sys.stderr)
        sys.exit(1)
    if not os.path.isfile(version_txt_path):
        print(f"error: {version_txt_path} not found", file=sys.stderr)
        sys.exit(1)

    version_byte, major, minor = read_version_byte(version_txt_path)

    with open(mcs_path, "r", encoding="ascii") as f:
        lines = [l.rstrip("\r\n") for l in f]

    lines = strip_previous_marker(lines)

    eof_idx = next((i for i, l in enumerate(lines) if l.upper().startswith(":00000001FF")), None)
    if eof_idx is None:
        print(f"error: no Intel HEX end-of-file record (:00000001FF) found in {mcs_path}", file=sys.stderr)
        sys.exit(1)

    ext_addr_record, data_record = make_marker_records(version_byte)
    lines[eof_idx:eof_idx] = [ext_addr_record, data_record]

    with open(mcs_path, "w", encoding="ascii", newline="\r\n") as f:
        f.write("\n".join(lines) + "\n")

    print(
        f"OK: stamped FPGA version {major}.{minor} (byte 0x{version_byte:02X}) "
        f"at flash address 0x{FLASH_LAST_BYTE:06X} in {mcs_path}"
    )


if __name__ == "__main__":
    main()
