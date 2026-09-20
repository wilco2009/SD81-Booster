#!/bin/bash
# SD81 Booster -- sintesis por lotes en la VM de ISE 14.7, via SSH.
#
# Uso: ssh ise-vm "cd ClaudeCode/SD81-Booster/FPGA/SD81V2.1000 && ./build.sh"
#
# Hace lo mismo que la GUI de Project Navigator (build.tcl) y despues
# regenera sd81boster.mcs con promgen, EXACTOS mismos parametros que el
# ultimo build manual registrado en sd81boster.prm (flash SPI de 16 MB,
# formato mcs86, relleno 0xFF).
#
# NO estampa la version de la FPGA (append_fpga_version.py necesita
# Python 3, que esta VM no tiene -- Python 2.6.6 solamente). Ese paso se
# hace en Windows sobre el .mcs recien generado, que ya es visible ahi por
# la carpeta compartida.
set -e

source /opt/Xilinx/14.7/ISE_DS/settings64.sh > /dev/null

cd "$(dirname "$0")"

echo "=== Sintesis (XST -> Translate -> Map -> PAR -> BitGen) ==="
xtclsh build.tcl

if [ ! -f SD81.bit ]; then
  echo "ERROR: no se genero SD81.bit -- revisa SD81.syr/SD81_map.mrp/SD81.par" >&2
  exit 1
fi

echo
echo "=== PROMGen (SD81.bit -> sd81boster.mcs) ==="
promgen -w -p mcs -c FF -o sd81boster.mcs -s 16384 -u 0 SD81.bit -spi

echo
echo "Listo. Pendiente en Windows:"
echo "  python append_fpga_version.py sd81boster.mcs FPGAVERSION.TXT"
echo "  cp sd81boster.mcs sd81.mcs   # sd81.mcs es el nombre que reconoce el firmware"
