@echo off
rem Ensambla NETCHAT y lo copia a la carpeta SD81\TOOLS\NETCHAT del emulador.
cd /d "%~dp0"
set DEST=\ClaudeCode\Eightyone2\EightyOne\SD81\SD81\TOOLS\NETCHAT

pasmo netchat.asm netchat.bin netchat.sym || goto error
if not exist "%DEST%" mkdir "%DEST%"
copy /y netchat.bin "%DEST%" >nul || goto error
copy /y NETCHAT.B81 "%DEST%" >nul || goto error
echo OK: netchat.bin y NETCHAT.B81 copiados en %DEST%
exit /b 0

:error
echo *** ERROR: se para aqui ***
exit /b 1
