@echo off
rem Ensambla SCROLLH (HiRes nativo). El emulador todavia no tiene los registros del scroll por hardware (puerto $3FEF,
rem registro 8 en adelante): esto es para la placa con la FPGA de la rama fpga-scroll.
cd /d "%~dp0"
pasmo scrollh.asm scrollh.bin scrollh.sym || goto error
echo OK: scrollh.bin
echo Copia SCROLLH.BIN y SCROLLH.B81 a la SD (por ejemplo en TOOLS\SCROLLH) y carga SCROLLH.B81
exit /b 0

:error
echo *** ERROR ***
exit /b 1
