@echo off
rem Ensambla TELNET y lo copia a la carpeta SD81\TELNET del emulador.
cd /d "%~dp0"
set DEST=\ClaudeCode\Eightyone2\EightyOne\SD81\TELNET

pasmo telnet.asm telnet.bin telnet.sym || goto error
if not exist "%DEST%" mkdir "%DEST%"
copy /y telnet.bin "%DEST%" >nul || goto error
echo OK: telnet.bin copiado en %DEST%
exit /b 0

:error
echo *** ERROR: se para aqui ***
exit /b 1
