@echo off
rem Ensambla SOLITARIO. Comprueba que el codigo no pasa de 12288 bytes ($5000-$7FFF) y copia
rem SOLITARIO.BIN y SOLITARIO.B81 a la carpeta de pruebas del emulador (mismo nombre en la SD de la placa).
cd /d "%~dp0"
pasmo -I ..\MOUSE solitario.asm solitario.bin solitario.sym || goto error
for %%F in (solitario.bin) do set SIZE=%%~zF
if %SIZE% GTR 12288 (
    echo *** ERROR: solitario.bin mide %SIZE% bytes y el maximo es 12288 ^(no puede pasar de $8000^)
    goto error
)
echo OK: solitario.bin, %SIZE% bytes de 12288
set DEST=C:\ClaudeCode\Eightyone2\EightyOne\SD81\SD81\GAMES\SOLITARIO
if not exist "%DEST%" mkdir "%DEST%"
copy /y solitario.bin "%DEST%\SOLITARIO.BIN" >nul
copy /y SOLITARIO.B81 "%DEST%\SOLITARIO.B81" >nul
echo Copiado a %DEST%
exit /b 0

:error
echo *** ERROR: se para aqui ***
exit /b 1
