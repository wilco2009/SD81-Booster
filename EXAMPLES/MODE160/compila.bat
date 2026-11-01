@echo off
rem Ensambla MODE160: primero la paleta (pal160.py) y despues el programa. Es para la placa con la FPGA de la rama
rem fpga-mode160; el emulador no tiene este modo.
cd /d "%~dp0"
python pal160.py || goto error
pasmo mode160.asm mode160.bin mode160.sym || goto error
pasmo mode1604.asm mode1604.bin mode1604.sym || goto error
pasmo mode160w.asm mode160w.bin mode160w.sym || goto error
echo OK: mode160.bin, mode1604.bin y mode160w.bin
echo Copia MODE160.BIN y MODE160.B81 a la SD (por ejemplo en TOOLS\MODE160) y carga MODE160.B81
exit /b 0

:error
echo *** ERROR ***
exit /b 1
