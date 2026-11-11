@echo off
rem Ensambla MOUSETEST (para la placa, copia MOUSETEST.BIN y MOUSETEST.B81 a la misma carpeta de la SD).
cd /d "%~dp0"
pasmo mousetest.asm mousetest.bin mousetest.sym || goto error
pasmo mouseble.asm mouseble.bin mouseble.sym || goto error
pasmo mltest.asm mltest.bin mltest.sym || goto error
echo OK: mousetest.bin, mouseble.bin y mltest.bin
exit /b 0

:error
echo *** ERROR: se para aqui ***
exit /b 1
