@echo off
rem Ensambla SPRDBUF y copia el binario al emulador.
cd /d "%~dp0"
set DEST=C:\ClaudeCode\Eightyone2\EightyOne\SD81\SD81\TOOLS\SPRDBUF

pasmo sprdbuf.asm sprdbuf.bin sprdbuf.sym || goto error
if not exist "%DEST%" mkdir "%DEST%"
copy /y sprdbuf.bin "%DEST%\SPRDBUF.BIN" >nul || goto error
copy /y SPRDBUF.B81 "%DEST%\SPRDBUF.B81" >nul || goto error
echo OK: copiado en %DEST%
exit /b 0

:error
echo *** ERROR ***
exit /b 1
