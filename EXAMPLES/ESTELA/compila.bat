@echo off
rem Ensambla ESTELA y lo copia al emulador con el nivel, los sprites y la musica (out\NIVEL1.I16, NIVEL1.MAP, HOJA.BIN y NIVEL1.VGM).
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
rem la musica del nivel (opcional: sin ella el juego va sin musica): out\NIVEL1.VGM si existe; si no, de momento
rem la de la prueba SONIDO de la fase 0 (..\VGM\music0002.vgm)
if exist out\NIVEL1.VGM (copy /y out\NIVEL1.VGM "%DEST%\NIVEL1.VGM" >nul || goto error) else (copy /y ..\VGM\music0002.vgm "%DEST%\NIVEL1.VGM" >nul || goto error)
echo OK: copiado en %DEST%
exit /b 0

:error
echo *** ERROR ***
exit /b 1
