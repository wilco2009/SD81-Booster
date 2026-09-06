#!/usr/bin/env python3
"""Actualiza de una sola vez las versiones de los 4 componentes del
firmware del SD81 Booster: STM32, ROM Z80, FPGA y modulo WiFi (ESP32).

Uso:
    python set_versions.py --stm32 2.2 --rom 1.4 --fpga 1.1 --wifi 1.1

Cualquier version omitida se deja tal cual esta -- no hace falta pasar
las 4 cada vez. Si un fichero ya tiene el valor pedido, no se toca (el
script es idempotente, se puede volver a ejecutar sin miedo).

Formato de todas las versiones: MAJOR.MINOR, cada uno 0-15 (STM32 y ROM
empaquetan los dos nibbles en un solo byte; FPGA y WiFi lo guardan como
texto, pero se valida el mismo formato por consistencia).

Lo que este script NO hace (pasos manuales, los hace el usuario):
- Compilar/flashear el STM32.
- Sintetizar la FPGA y ejecutar append_fpga_version.py sobre el .mcs
  recien generado (necesita el .mcs real, este script no sintetiza).
- Compilar el modulo ESP32.

Referencia completa de donde vive cada version: memoria
'project_firmware_versioning' (o pregunta por ella).
"""
import argparse
import re
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

STM32_GLOBALS = REPO_ROOT / "Arduino/SD81BoosterV2_039_STM32/GLOBALS.h"
ROM_HANDLER = REPO_ROOT / "z80rom/sdhandler.inc.asm"
FPGA_VERSION_FILE = REPO_ROOT / "FPGA/SD81V2.1000/FPGAVERSION.TXT"
WIFI_INO = REPO_ROOT / "Arduino/Wifi_module_01/Wifi_module_01.ino"


def parse_major_minor(value, label):
    m = re.fullmatch(r"(\d+)\.(\d+)", value)
    if not m:
        raise SystemExit(
            f"{label}: formato esperado MAJOR.MINOR (p.ej. 2.2), recibido {value!r}"
        )
    major, minor = int(m.group(1)), int(m.group(2))
    if not (0 <= major <= 15 and 0 <= minor <= 15):
        raise SystemExit(
            f"{label}: mayor y menor deben caber en un nibble (0-15), recibido {value!r}"
        )
    return major, minor


def update_stm32(version):
    major, minor = parse_major_minor(version, "STM32")
    byte_val = (major << 4) | minor
    text = STM32_GLOBALS.read_text(encoding="utf-8")
    pattern = re.compile(r"(#define VERSION )0x[0-9A-Fa-f]{2}")
    match = pattern.search(text)
    if not match:
        raise SystemExit(f"No se encontro '#define VERSION 0x..' en {STM32_GLOBALS}")
    if match.group(0) == f"#define VERSION 0x{byte_val:02X}":
        print(f"STM32: ya esta en 0x{byte_val:02X} ({version}), sin cambios")
        return
    new_text, n = pattern.subn(rf"\g<1>0x{byte_val:02X}", text, count=1)
    if n != 1:
        raise SystemExit(f"Coincidencias inesperadas ({n}) en {STM32_GLOBALS}, revisar a mano")
    STM32_GLOBALS.write_text(new_text, encoding="utf-8")
    print(f"STM32: GLOBALS.h -> 0x{byte_val:02X} ({version})")


def update_rom(version):
    major, minor = parse_major_minor(version, "ROM")
    byte_val = (major << 4) | minor
    text = ROM_HANDLER.read_text(encoding="utf-8")
    pattern = re.compile(r"(VERSION:\s*db\s*)\$[0-9A-Fa-f]{2}(\s*;\s*ROM version )[\d.]+")
    match = pattern.search(text)
    if not match:
        raise SystemExit(f"No se encontro la linea 'VERSION: db $..' esperada en {ROM_HANDLER}")
    if match.group(0) == f"{match.group(1)}${byte_val:02X}{match.group(2)}{version}":
        print(f"ROM: ya esta en ${byte_val:02X} ({version}), sin cambios")
        return
    new_text, n = pattern.subn(rf"\g<1>${byte_val:02X}\g<2>{version}", text, count=1)
    if n != 1:
        raise SystemExit(f"Coincidencias inesperadas ({n}) en {ROM_HANDLER}, revisar a mano")
    ROM_HANDLER.write_text(new_text, encoding="utf-8")
    print(f"ROM: sdhandler.inc.asm -> ${byte_val:02X} ({version})")


def update_fpga(version):
    parse_major_minor(version, "FPGA")  # solo valida el formato
    current = FPGA_VERSION_FILE.read_text(encoding="utf-8").strip()
    if current == version:
        print(f"FPGA: FPGAVERSION.TXT ya esta en {version}, sin cambios")
        return
    FPGA_VERSION_FILE.write_text(version + "\n", encoding="utf-8")
    print(f"FPGA: FPGAVERSION.TXT -> {version}")
    print("      RECUERDA: ejecutar append_fpga_version.py sobre el .mcs "
          "recien sintetizado antes de publicarlo (paso manual).")


def update_wifi(version):
    parse_major_minor(version, "WiFi")
    text = WIFI_INO.read_text(encoding="utf-8")
    pattern = re.compile(r'(#define WIFI_FW_VERSION ")[\d.]+(")')
    match = pattern.search(text)
    if not match:
        raise SystemExit(f"No se encontro '#define WIFI_FW_VERSION \"..\"' en {WIFI_INO}")
    if match.group(0) == f'#define WIFI_FW_VERSION "{version}"':
        print(f"WiFi: ya esta en {version}, sin cambios")
        return
    new_text, n = pattern.subn(rf"\g<1>{version}\g<2>", text, count=1)
    if n != 1:
        raise SystemExit(f"Coincidencias inesperadas ({n}) en {WIFI_INO}, revisar a mano")
    WIFI_INO.write_text(new_text, encoding="utf-8")
    print(f"WiFi: Wifi_module_01.ino -> {version}")


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("--stm32", help="version STM32, MAJOR.MINOR (p.ej. 2.2)")
    ap.add_argument("--rom", help="version ROM Z80, MAJOR.MINOR (p.ej. 1.4)")
    ap.add_argument("--fpga", help="version FPGA, MAJOR.MINOR (p.ej. 1.1)")
    ap.add_argument("--wifi", help="version modulo WiFi/ESP32, MAJOR.MINOR (p.ej. 1.1)")
    args = ap.parse_args()

    if not any([args.stm32, args.rom, args.fpga, args.wifi]):
        ap.error("indica al menos una version a actualizar (--stm32/--rom/--fpga/--wifi)")

    if args.stm32:
        update_stm32(args.stm32)
    if args.rom:
        update_rom(args.rom)
    if args.fpga:
        update_fpga(args.fpga)
    if args.wifi:
        update_wifi(args.wifi)

    print("\nHecho. Pasos manuales que siguen dependiendo del usuario:")
    print("- Compilar y flashear el STM32 con el nuevo binario.")
    print("- Sintetizar la FPGA y ejecutar append_fpga_version.py sobre el .mcs.")
    print("- Compilar el modulo ESP32 si se toco su version.")


if __name__ == "__main__":
    main()
