@echo off
rem Ensambla CONNECT4 y lo copia a la carpeta SD81\TOOLS\CONNECT4 del emulador.
cd /d "%~dp0"
set DEST=\ClaudeCode\Eightyone2\EightyOne\SD81\SD81\TOOLS\CONNECT4

pasmo connect4.asm connect4.bin connect4.sym || goto error
if not exist "%DEST%" mkdir "%DEST%"
copy /y connect4.bin "%DEST%" >nul || goto error
copy /y CONNECT4.B81 "%DEST%" >nul || goto error
echo OK: connect4.bin y CONNECT4.B81 copiados en %DEST%
exit /b 0

:error
echo *** ERROR: se para aqui ***
exit /b 1
