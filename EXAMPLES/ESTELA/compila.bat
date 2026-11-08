@echo off
rem Ensambla ESTELA y lo copia al emulador.
cd /d "%~dp0"
set DEST=C:\ClaudeCode\Eightyone2\EightyOne\SD81\SD81\GAMES\ESTELA

pasmo estela.asm estela.bin estela.sym || goto error
rem El ZX81 no ejecuta codigo en $8000-$FFFF, y $7000-$7FFF es la hoja de los sprites:
rem el binario (codigo y tablas, desde $4800) no puede pasar de $6FFF = 10240 bytes
for %%F in (estela.bin) do if %%~zF GTR 10240 (echo *** estela.bin pasa de $6FFF: no cabe antes de la hoja de sprites ni de $8000 *** & goto error)
if not exist "%DEST%" mkdir "%DEST%"
copy /y estela.bin "%DEST%\ESTELA.BIN" >nul || goto error
copy /y ESTELA.B81 "%DEST%\ESTELA.B81" >nul || goto error
echo OK: copiado en %DEST%
exit /b 0

:error
echo *** ERROR ***
exit /b 1
