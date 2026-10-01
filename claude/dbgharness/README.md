# Bancos de pruebas del depurador (en el PC)

Prueban el depurador por hardware sin el SD81 Booster, antes de llevarlo
al hardware. Ver `claude/planning/hw_debugger_plan.md`.

```
sh build.sh
```

Necesitan `g++`/`gcc` (MinGW), `pasmo` y los fuentes del núcleo Z80 de
EightyOne (`/c/ClaudeCode/Eightyone2/src`, o la ruta que diga la variable
`Z80SRC`). Compilan en `build/`.

## `logic.cpp`: la lógica del MCU

`Arduino/SD81BoosterV2_039_STM32/DEBUGGER.cpp` contra un Z80 de mentira que
habla el protocolo `DBG_BREAK` / `DBG_POLL` byte a byte. El programa son
instrucciones de un byte; los breakpoints `FF` y los pasos rompen como lo
haría la FPGA. Comprueba:
- los breakpoints por software: se ponen al continuar y se quitan al parar;
- el paso para saltar el breakpoint del PC y volver a ponerlo;
- los pasos;
- el `RST 38h` que no es un breakpoint (se emula al continuar);
- cambiar registros;
- un reset con un breakpoint puesto;
- las órdenes `m`, `d`, `e` e `io`.

## `cosim.cpp`: el monitor de verdad y el MCU de verdad

Cosimulación:
- **El monitor de verdad** (`z80rom/debugmon.asm`, ensamblado) corre en el
  núcleo Z80 de EightyOne.
- **`DEBUGGER.cpp`** corre en otro hilo. Se hablan por los puertos `$A7` y
  `$AF` con el mismo protocolo de reloj que en el hardware.
- **La FPGA** se modela por instrucción: la ruptura es el `RST` más el
  `CALL $2000` con la página 63 en el bloque 1, y la salida en `$003B` es el
  epílogo. También están la protección del bloque 0 y de la página 63, el
  mapper y el puerto `$3FEF`.

El programa de prueba (`prog.asm`, en `$6000`) carga todos los registros,
comprueba en un bucle que R avanza siempre 11 entre dos `LD A,R` (aunque se
pare por medio) y usa la pila. El banco lo para varias veces y comprueba:
- que el bucle no se rompe;
- los pasos;
- que `m` y `e` en `$2000` van a la página del programa y no a la del
  monitor;
- un breakpoint y su vuelta;
- que, al sacarlo de un bucle con `x pc=…` y `x hl=…`, todos los registros,
  los alternativos y SP llegan intactos (y HL cambiado).
