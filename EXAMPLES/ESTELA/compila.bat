@echo off
rem Ensambla ESTELA y lo copia al emulador con el nivel y los sprites (out\NIVEL1.I16, NIVEL1.MAP y HOJA.BIN).
cd /d "%~dp0"
set DEST=C:\ClaudeCode\Eightyone2\EightyOne\SD81\SD81\GAMES\ESTELA

pasmo estela.asm estela.bin estela.sym || goto error
rem El ZX81 no ejecuta codigo en $8000-$FFFF, y $7000-$7FFF es la hoja de los sprites:
rem el binario (codigo y tablas, desde $4800) no puede pasar de $6FFF = 10240 bytes
for %%F in (estela.bin) do if %%~zF GTR 10240 (echo *** estela.bin pasa de $6FFF: no cabe antes de la hoja de sprites ni de $8000 *** & goto error)
if not exist "%DEST%" mkdir "%DEST%"
copy /y estela.bin "%DEST%\ESTELA.BIN" >nul || goto error
copy /y ESTELA.B81 "%DEST%\ESTELA.B81" >nul || goto error
rem el nivel (lo genera tools\make.py en out\); detras del binario van la paleta (512 bytes) y NIVEL1.MAP, tambien
rem antes de $7000: si no cabe, el juego vuelve con el error 5
copy /y out\NIVEL1.I16 "%DEST%\NIVEL1.I16" >nul || goto error
copy /y out\NIVEL1.MAP "%DEST%\NIVEL1.MAP" >nul || goto error
copy /y out\HOJA.BIN "%DEST%\HOJA.BIN" >nul || goto error
echo OK: copiado en %DEST%
exit /b 0

:error
echo *** ERROR ***
exit /b 1
