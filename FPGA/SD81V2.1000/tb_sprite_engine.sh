#!/bin/sh
# Simula sprite_engine.v contra los 32 sprite_slot (sprite_slot.v) con ModelSim.
# Uso: sh tb_sprite_engine.sh [frames]      (desde esta carpeta)
MS=/c/intelFPGA_lite/17.0/modelsim_ase/win32aloem
T=${TMPDIR:-/tmp}/tb_engine
FR=${1:-20}
rm -rf "$T" && mkdir -p "$T"
"$MS/vlib" "$T/work" > /dev/null
"$MS/vlog" -work "$T/work" sprite_slot.v sprite_engine.v tb_sprite_engine.v | grep -i "error\|warning"
for m in 0 1; do
  for d in 0 1; do
    "$MS/vsim" -c -lib "$T/work" tb_sprite_engine +mode80=$m +dense=$d +frames=$FR -do "run -all; quit" | grep -i "DIF\|RESULTADO\|^# OK\|FALLO\|error"
  done
done
