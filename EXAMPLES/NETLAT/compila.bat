@echo off
rem Ensambla NETLAT y lo copia a SD81\test\NETLAT del emulador (para la placa, copia NETLAT.BIN y NETLAT.B81 a la SD).
cd /d "%~dp0"
set BASE=\ClaudeCode\Eightyone2\EightyOne\SD81\test
pasmo netlat.asm netlat.bin netlat.sym || goto error
for %%F in (netlat.bin) do if %%~zF GTR 4096 goto big
if not exist "%BASE%\NETLAT" mkdir "%BASE%\NETLAT"
copy /y netlat.bin "%BASE%\NETLAT" >nul || goto error
copy /y NETLAT.B81 "%BASE%\NETLAT" >nul || goto error
echo OK: NETLAT copiado en %BASE%\NETLAT
exit /b 0

:big
echo *** netlat.bin pisa el buzon de net.inc - 7000h - ***
:error
echo *** ERROR: se para aqui ***
exit /b 1
