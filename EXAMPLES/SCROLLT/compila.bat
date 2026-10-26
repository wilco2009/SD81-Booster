@echo off
rem Ensambla SCROLLT. El emulador todavia no tiene los registros del scroll por hardware (puerto $3FEF,
rem registro 8 en adelante): esto es para la placa con la FPGA de la rama fpga-scroll.
cd /d "%~dp0"
pasmo scrollt.asm scrollt.bin scrollt.sym || goto error
echo OK: scrollt.bin
echo Copia SCROLLT.BIN y SCROLLT.B81 a la SD (por ejemplo en TOOLS\SCROLLT) y carga SCROLLT.B81
exit /b 0

:error
echo *** ERROR ***
exit /b 1
