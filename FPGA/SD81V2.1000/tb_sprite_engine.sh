#!/bin/sh
# Con LOG=fichero cada resultado va a ese fichero segun sale.
# Simula sprite_engine.v con ModelSim: formato A contra los sprite_slot (sprite_slot.v) y la especificacion; formato B y sprites mezclados, contra la especificacion.
# Uso: sh tb_sprite_engine.sh [frames]      (desde esta carpeta)
MS=/c/intelFPGA_lite/17.0/modelsim_ase/win32aloem
T=${TMPDIR:-/tmp}/tb_engine
FR=${1:-20}
rm -rf "$T" && mkdir -p "$T"
"$MS/vlib" "$T/work" > /dev/null
"$MS/vlog" -work "$T/work" sprite_slot.v sprite_engine.v tb_sprite_engine.v | grep -i "error\|warning"
corre() { "$MS/vsim" -c -lib "$T/work" tb_sprite_engine "$@" +frames=$FR -do "run -all; quit" | grep -i "DIF\|RESULTADO\|^# OK\|FALLO\|error" | tee -a "${LOG:-/dev/null}"; }
for m in 0 1; do
  for f in 0 1 2; do
    for d in 0 1; do
      corre +mode80=$m +fmt=$f +dense=$d
    done
    corre +mode80=$m +fmt=$f +worst=1
    corre +mode80=$m +fmt=$f +dense=1 +mono=1
  done
  corre +mode80=$m +fmt=0 +mono=1
  # sprites extra (POKE 2131): franjas con lo que garantiza el tiempo, y el caso peor
  corre +mode80=$m +fmt=2 +extra=16
  corre +mode80=$m +fmt=1 +extra=64
  corre +mode80=$m +fmt=0 +extra=64 +mono=1
  corre +mode80=$m +fmt=0 +extra=64 +worst=1
  corre +mode80=$m +fmt=1 +extra=64 +worst=1
  corre +mode80=$m +fmt=2 +extra=127 +kb=1
done
