@echo off
rem Ensambla MINAS (para la placa, copia MINAS.BIN y MINAS.B81 a la misma carpeta de la SD).
cd /d "%~dp0"
pasmo -I ..\MOUSE minas.asm minas.bin minas.sym || goto error
echo OK: minas.bin
exit /b 0

:error
echo *** ERROR: se para aqui ***
exit /b 1
