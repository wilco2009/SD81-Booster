@echo off
rem Ensambla SCRL160 y copia el binario al emulador.
cd /d "%~dp0"
set DEST=C:\ClaudeCode\Eightyone2\EightyOne\SD81\SD81\TOOLS\SCRL160

pasmo scrl160.asm scrl160.bin scrl160.sym || goto error
if not exist "%DEST%" mkdir "%DEST%"
copy /y scrl160.bin "%DEST%\SCRL160.BIN" >nul || goto error
copy /y SCRL160.B81 "%DEST%\SCRL160.B81" >nul || goto error
echo OK: copiado en %DEST%
exit /b 0

:error
echo *** ERROR ***
exit /b 1
