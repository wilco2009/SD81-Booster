@echo off
rem Ensambla DICTTEST y lo copia a SD81\TOOLS\DICTTEST del emulador (con DICT.DIC si lo encuentra en ..\..\build\dict).
cd /d "%~dp0"
set BASE=\ClaudeCode\Eightyone2\EightyOne\SD81\SD81\TOOLS
pasmo dicttest.asm dicttest.bin dicttest.sym || goto error
if not exist "%BASE%\DICTTEST" mkdir "%BASE%\DICTTEST"
copy /y dicttest.bin "%BASE%\DICTTEST" >nul || goto error
copy /y DICTTEST.B81 "%BASE%\DICTTEST" >nul || goto error
if exist ..\..\build\dict\DICT.DIC copy /y ..\..\build\dict\DICT.DIC "%BASE%\DICTTEST" >nul
echo OK: DICTTEST copiado en %BASE%\DICTTEST
exit /b 0

:error
echo *** ERROR: se para aqui ***
exit /b 1
