@echo off
rem Ensambla COLORTEST y lo copia a SD81\TOOLS\COLORTEST del emulador.
cd /d "%~dp0"
set BASE=\ClaudeCode\Eightyone2\EightyOne\SD81\SD81\TOOLS
pasmo colortest.asm colortest.bin colortest.sym || goto error
if not exist "%BASE%\COLORTEST" mkdir "%BASE%\COLORTEST"
copy /y colortest.bin "%BASE%\COLORTEST" >nul || goto error
copy /y COLORTEST.B81 "%BASE%\COLORTEST" >nul || goto error
echo OK: COLORTEST copiado en %BASE%\COLORTEST
exit /b 0

:error
echo *** ERROR: se para aqui ***
exit /b 1
