@echo off
rem Ensambla el PONG y lo copia a SD81\test\PONG (y a PONG2, la segunda instancia del emulador, si existe) del emulador. Es una prueba de latencia: carpeta de tests.
cd /d "%~dp0"
set BASE=\ClaudeCode\Eightyone2\EightyOne\SD81\test
pasmo pong.asm pong.bin pong.sym || goto error
for %%F in (pong.bin) do if %%~zF GTR 8192 goto big
if not exist "%BASE%\PONG" mkdir "%BASE%\PONG"
copy /y pong.bin "%BASE%\PONG" >nul || goto error
copy /y PONG.B81 "%BASE%\PONG" >nul || goto error
if exist "%BASE%\PONG2" copy /y pong.bin "%BASE%\PONG2" >nul
if exist "%BASE%\PONG2" copy /y PONG.B81 "%BASE%\PONG2" >nul
echo OK: PONG copiado en %BASE%\PONG
exit /b 0

:big
echo *** pong.bin pasa de $8000 ***
:error
echo *** ERROR: se para aqui ***
exit /b 1
