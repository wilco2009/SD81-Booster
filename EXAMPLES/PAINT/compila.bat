@echo off
rem Ensambla PAINT (para la placa, copia PAINT.BIN y PAINT.B81 a la misma carpeta de la SD).
cd /d "%~dp0"
pasmo -I ..\MOUSE paint.asm paint.bin paint.sym || goto error
echo OK: paint.bin
exit /b 0

:error
echo *** ERROR: se para aqui ***
exit /b 1
