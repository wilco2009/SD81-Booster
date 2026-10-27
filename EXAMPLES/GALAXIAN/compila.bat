@echo off
rem Genera los graficos, ensambla GALAXIAN y copia el binario al emulador.
cd /d "%~dp0"
set DEST=C:\ClaudeCode\Eightyone2\EightyOne\SD81\SD81\TOOLS\GALAXIAN

python -I gen_gfx.py || goto error
pasmo galaxian.asm galaxian.bin galaxian.sym || goto error
if not exist "%DEST%" mkdir "%DEST%"
copy /y galaxian.bin "%DEST%\GALAXIAN.BIN" >nul || goto error
copy /y GALAXIAN.B81 "%DEST%\GALAXIAN.B81" >nul || goto error
echo OK: copiado en %DEST%
exit /b 0

:error
echo *** ERROR ***
exit /b 1
