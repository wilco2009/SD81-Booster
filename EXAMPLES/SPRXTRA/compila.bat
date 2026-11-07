@echo off
rem Ensambla SPRXTRA y copia el binario al emulador.
cd /d "%~dp0"
set DEST=C:\ClaudeCode\Eightyone2\EightyOne\SD81\SD81\TOOLS\SPRXTRA

pasmo sprxtra.asm sprxtra.bin sprxtra.sym || goto error
if not exist "%DEST%" mkdir "%DEST%"
copy /y sprxtra.bin "%DEST%\SPRXTRA.BIN" >nul || goto error
copy /y SPRXTRA.B81 "%DEST%\SPRXTRA.B81" >nul || goto error
if exist FONDO.JPG copy /y FONDO.JPG "%DEST%\FONDO.JPG" >nul
echo OK: copiado en %DEST%
exit /b 0

:error
echo *** ERROR ***
exit /b 1
