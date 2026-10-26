@echo off
rem Ensambla SCROLLM. El emulador todavia no tiene los registros del scroll por hardware (puerto $3FEF,
rem registro 8 en adelante): esto es para la placa con la FPGA de la rama fpga-scroll.
cd /d "%~dp0"
pasmo scrollm.asm scrollm.bin scrollm.sym || goto error
echo OK: scrollm.bin
echo Copia SCROLLM.BIN y SCROLLM.B81 a la SD (por ejemplo en TOOLS\SCROLLM) y carga SCROLLM.B81
exit /b 0

:error
echo *** ERROR ***
exit /b 1
