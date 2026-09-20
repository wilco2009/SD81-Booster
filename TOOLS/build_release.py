#!/usr/bin/env python3
"""SD81 Booster - build_release.py

Compila los componentes que hagan falta y los deja ya renombrados en las
carpetas de siempre (FIRMWARE/ y SD Content/SYS/), listas para empaquetar
exactamente igual que se ha venido haciendo a mano. No toca Software.zip
(lo sigue preparando el usuario) ni sube versiones (para eso,
TOOLS/set_versions.py).

Uso:
    python build_release.py                    # STM32 app + ESP32 + ROM + FPGA
    python build_release.py --only stm32,rom    # solo lo indicado
    python build_release.py --skip fpga         # todo menos lo indicado
    python build_release.py --bootloader        # ademas, recompila el bootloader STM32
    python build_release.py --package           # ademas, empaqueta los 3 zips (Firmware/SD.Content/Tools)

Componentes reconocidos: stm32, esp32, rom, fpga, bootloader.

Requisitos ya validados en esta maquina (ver memoria del proyecto):
- arduino-cli en PATH, usando la config del propio Arduino IDE
  (~/.arduinoIDE/arduino-cli.yaml) para compartir sketchbook y cores.
- pasmo en PATH (assembler Z80 para la ROM).
- acceso SSH al alias "ise-vm" (VM VirtualBox con ISE 14.7) para la FPGA
  -- ver memoria reference_ise_vm_ssh.md.

Lo que este script NO hace (sigue siendo el usuario):
- Flashear nada.
- Tocar Software.zip.
- Decidir los numeros de version (usar set_versions.py antes si hace falta).
"""
import argparse
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
FIRMWARE_DIR = REPO_ROOT / "FIRMWARE"
SD_CONTENT_DIR = REPO_ROOT / "SD Content"
FPGA_DIR = REPO_ROOT / "FPGA" / "SD81V2.1000"
ROM_DIR = REPO_ROOT / "z80rom"
STM32_SKETCH = REPO_ROOT / "Arduino" / "SD81BoosterV2_039_STM32"
ESP32_SKETCH = REPO_ROOT / "Arduino" / "Wifi_module_01"

ARDUINO_IDE_CONFIG = Path.home() / ".arduinoIDE" / "arduino-cli.yaml"

# FQBN validadas a mano el 2026-09-20 contra el menu Tools real del usuario
# (ver reference en memoria / esta misma conversacion). Si cambia algo en
# el menu Tools del IDE, hay que actualizar esto a la vez o arduino-cli y
# el IDE dejaran de dar binarios equivalentes.
FQBN_STM32_APP = (
    "STMicroelectronics:stm32:GenF4:pnum=SD81_BOOSTER,upload_method=dfuMethod,"
    "xserial=generic,usb=CDCgen,xusb=FS,opt=osstd,dbg=none,rtlib=nano"
)
FQBN_STM32_BOOTLOADER = FQBN_STM32_APP.replace("pnum=SD81_BOOSTER", "pnum=SD81_BOOTLOADER")
FQBN_ESP32 = (
    "esp32:esp32:esp32c3:CDCOnBoot=cdc,CPUFreq=160,DebugLevel=none,EraseFlash=none,"
    "FlashFreq=80,FlashMode=qio,FlashSize=4M,JTAGAdapter=default,"
    "PartitionScheme=min_spiffs,UploadSpeed=921600,ZigbeeMode=default"
)

# NOTA sobre ESP32 y cmd.exe: el core ESP32 copia partitions.csv con
# "cmd /c ..." en Windows durante el prebuild. En un terminal normal esto
# no da ningun problema (probado). Si este script lo ejecuta un agente con
# el propio subproceso restringido (p.ej. una sesion de Claude Code en su
# entorno de herramientas en vez de un terminal real del usuario), esa
# invocacion de cmd.exe puede fallar con "requires elevation" -- no es un
# fallo del proyecto ni de arduino-cli, es una restriccion de ESE entorno
# de ejecucion en concreto. Se probo parchear el hook via
# --build-property/platform.local.txt para usar bash en su lugar, pero
# arduino-cli borra las barras invertidas de las rutas sustituidas
# ({build.path} etc.) en cuanto el patron pasa por ahi -- reproducido con
# y sin comillas alrededor del placeholder, asi que no es un problema de
# escapado por nuestra parte, parece un bug/limitacion del propio
# properties-parser de arduino-cli. Si aparece ese error, ejecutar este
# script (o al menos el paso ESP32) desde un terminal real en vez de desde
# el entorno de herramientas de un agente.

# Ficheros que van en Firmware.zip: solo lo que el flujo de actualizacion
# normal necesita (ver memoria feedback_release_zip_packaging.md).
FIRMWARE_ZIP_FILES = ["firmware.bin", "sd81.mcs", "ESP32_FW.BIN"]

# Ficheros/scripts que van en Tools.zip (kit de recuperacion/build, NO CP/M).
TOOLS_ZIP_FILES = [
    "bootloader.bin", "bootloader.bat", "firmware.bat", "stm32CubeProg.sh",
    "busybox.exe", "append_fpga_version.py", "README_update.md",
    "README_update_ES.md", "COMPILING.md",
]

# Excluido siempre de SD Content.zip -- placeholder inservible, sobrescribe
# la config real del usuario al actualizar. NTP.CFG SI se incluye (ver
# memoria feedback_release_zip_packaging.md).
SD_CONTENT_EXCLUDE = {"SYS/WIFI.CFG"}


def run(cmd, cwd=None, check=True):
    print(f"$ {' '.join(str(c) for c in cmd)}")
    result = subprocess.run(cmd, cwd=cwd)
    if check and result.returncode != 0:
        raise SystemExit(f"Comando fallido (exit {result.returncode}): {cmd}")
    return result.returncode


def build_stm32(bootloader=False):
    fqbn = FQBN_STM32_BOOTLOADER if bootloader else FQBN_STM32_APP
    out_name = "bootloader.bin" if bootloader else "firmware.bin"
    label = "STM32 bootloader" if bootloader else "STM32 aplicacion"
    print(f"\n=== {label} ===")
    build_dir = STM32_SKETCH / "build_tmp"
    if build_dir.exists():
        shutil.rmtree(build_dir)
    run([
        "arduino-cli", "compile",
        "--config-file", str(ARDUINO_IDE_CONFIG),
        "--fqbn", fqbn,
        "--output-dir", str(build_dir),
        str(STM32_SKETCH),
    ])
    produced = list(build_dir.glob("*.ino.bin"))
    if not produced:
        raise SystemExit(f"No se encontro ningun .ino.bin en {build_dir}")
    FIRMWARE_DIR.mkdir(exist_ok=True)
    dest = FIRMWARE_DIR / out_name
    shutil.copy2(produced[0], dest)
    shutil.rmtree(build_dir)
    print(f"-> {dest} ({dest.stat().st_size} bytes)")


def build_esp32():
    print("\n=== ESP32 (modulo WiFi) ===")
    build_dir = ESP32_SKETCH / "build_tmp"
    if build_dir.exists():
        shutil.rmtree(build_dir)
    run([
        "arduino-cli", "compile",
        "--config-file", str(ARDUINO_IDE_CONFIG),
        "--fqbn", FQBN_ESP32,
        "--output-dir", str(build_dir),
        str(ESP32_SKETCH),
    ])
    produced = list(build_dir.glob("*.ino.bin"))
    if not produced:
        raise SystemExit(f"No se encontro ningun .ino.bin en {build_dir}")
    FIRMWARE_DIR.mkdir(exist_ok=True)
    dest = FIRMWARE_DIR / "ESP32_FW.BIN"
    shutil.copy2(produced[0], dest)
    shutil.rmtree(build_dir)
    print(f"-> {dest} ({dest.stat().st_size} bytes)")


def build_rom():
    print("\n=== ROM Z80 (pasmo) ===")
    sys_dir = SD_CONTENT_DIR / "SYS"
    sys_dir.mkdir(parents=True, exist_ok=True)
    dest = sys_dir / "SDBOOST.ROM"
    run(["pasmo", "sdmodrom.asm", str(dest)], cwd=ROM_DIR)
    print(f"-> {dest} ({dest.stat().st_size} bytes)")


def build_fpga():
    print("\n=== FPGA (ISE 14.7 via SSH a ise-vm) ===")
    remote_dir = "ClaudeCode/SD81-Booster/FPGA/SD81V2.1000"
    run(["ssh", "ise-vm", f"cd {remote_dir} && ./build.sh"])

    mcs_boster = FPGA_DIR / "sd81boster.mcs"
    version_file = FPGA_DIR / "FPGAVERSION.TXT"
    if not mcs_boster.exists():
        raise SystemExit(f"No aparecio {mcs_boster} tras el build remoto")

    run([
        sys.executable, "append_fpga_version.py",
        str(mcs_boster), str(version_file),
    ], cwd=FPGA_DIR)

    mcs_deploy = FPGA_DIR / "sd81.mcs"
    shutil.copy2(mcs_boster, mcs_deploy)
    print(f"-> {mcs_boster} (canonico, git) y {mcs_deploy} (nombre de despliegue)")

    FIRMWARE_DIR.mkdir(exist_ok=True)
    dest = FIRMWARE_DIR / "sd81.mcs"
    shutil.copy2(mcs_deploy, dest)
    print(f"-> {dest} ({dest.stat().st_size} bytes)")


def zip_dir(zip_path, base_dir, filenames=None, exclude=None, root_name=None):
    """Empaqueta ficheros de base_dir (recursivo si filenames es None) en
    zip_path. root_name es la carpeta raiz dentro del zip (por defecto
    base_dir.name); pasar root_name="" deja los ficheros sueltos en la raiz
    del zip (asi va TOOLS.zip, a diferencia de Firmware.zip/SD Content.zip)."""
    exclude = exclude or set()
    if zip_path.exists():
        zip_path.unlink()
    prefix = base_dir.name if root_name is None else root_name
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        if filenames is not None:
            for name in filenames:
                src = base_dir / name
                if not src.exists():
                    print(f"  AVISO: {src} no existe, se omite")
                    continue
                arc = f"{prefix}/{name}" if prefix else name
                zf.write(src, arc)
        else:
            for path in sorted(base_dir.rglob("*")):
                if path.is_dir():
                    continue
                rel = path.relative_to(base_dir).as_posix()
                if rel in exclude:
                    print(f"  excluido a proposito: {rel}")
                    continue
                arc = f"{prefix}/{rel}" if prefix else rel
                zf.write(path, arc)
    print(f"-> {zip_path}")


def package():
    print("\n=== Empaquetado ===")
    zip_dir(REPO_ROOT / "Firmware.zip", FIRMWARE_DIR, filenames=FIRMWARE_ZIP_FILES)
    zip_dir(REPO_ROOT / "SD Content.zip", SD_CONTENT_DIR, exclude=SD_CONTENT_EXCLUDE)
    tools_dir = REPO_ROOT / "FIRMWARE"  # los ficheros de Tools.zip viven junto a los de Firmware
    zip_dir(REPO_ROOT / "TOOLS.zip", tools_dir, filenames=TOOLS_ZIP_FILES, root_name="")


COMPONENTS = ["stm32", "esp32", "rom", "fpga"]


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("--only", help="lista separada por comas: stm32,esp32,rom,fpga")
    ap.add_argument("--skip", help="lista separada por comas a omitir")
    ap.add_argument("--bootloader", action="store_true",
                     help="ademas, recompila el bootloader STM32 (normalmente no hace falta)")
    ap.add_argument("--package", action="store_true",
                     help="ademas, empaqueta Firmware.zip/SD Content.zip/TOOLS.zip")
    args = ap.parse_args()

    selected = set(COMPONENTS)
    if args.only:
        selected = {c.strip() for c in args.only.split(",")}
    if args.skip:
        selected -= {c.strip() for c in args.skip.split(",")}

    unknown = selected - set(COMPONENTS)
    if unknown:
        ap.error(f"componentes desconocidos: {', '.join(unknown)}")

    if "stm32" in selected:
        build_stm32(bootloader=False)
    if args.bootloader:
        build_stm32(bootloader=True)
    if "esp32" in selected:
        build_esp32()
    if "rom" in selected:
        build_rom()
    if "fpga" in selected:
        build_fpga()

    if args.package:
        package()

    print("\nHecho.")


if __name__ == "__main__":
    main()
