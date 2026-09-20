# SD81 Booster -- sintesis por lotes (ISE 14.7 / xtclsh)
#
# Reproduce exactamente lo mismo que pulsar "Generate Programming File" en
# Project Navigator: usa las propiedades ya guardadas en SD81V2.1000.xise
# (dispositivo xc6slx9-3-tqg144, constraints SD81XC6.ucf, opciones de XST/
# MAP/PAR tal cual estan configuradas) -- no se fija nada a mano aqui para
# no arriesgarse a que la compilacion por consola use ajustes distintos a
# los de la GUI.
#
# Uso (con el entorno de ISE ya cargado -- ver build.sh):
#   xtclsh build.tcl
#
# "-force rerun_all" fuerza una reconstruccion completa (Synthesize ->
# Translate -> Map -> Place & Route -> Generate Programming File) en vez de
# reutilizar resultados intermedios que puedan haber quedado de una
# ejecucion anterior.

project open SD81V2.1000.xise
process run "Generate Programming File" -force rerun_all
project close
