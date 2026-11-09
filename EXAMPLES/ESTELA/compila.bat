@echo off
rem Ensambla ESTELA y lo copia al emulador con el nivel, los sprites y la musica (out\NIVEL1.I16, NIVEL1.MAP, HOJA.BIN y NIVEL1.VGM).
cd /d "%~dp0"
set DEST=C:\ClaudeCode\Eightyone2\EightyOne\SD81\SD81\GAMES\ESTELA

pasmo estela.asm estela.bin estela.sym || goto error
rem El ZX81 no ejecuta codigo en $8000-$FFFF; $6F60-$7FFF son las letras W N I L D y la hoja de los sprites, y detras
rem del binario va PALBUF (512 bytes): el binario (desde $4800) no puede pasar de $6F60 - 512 - $4800 = 9568 bytes
for %%F in (estela.bin) do if %%~zF GTR 9568 (echo *** estela.bin pasa de 9568 bytes: con PALBUF no cabe antes de $6F60 *** & goto error)
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
rem la del fin de partida (suena una vez): out\GAMEOVER.VGM si existe; si no, de momento ..\VGM\ledstorm-2.vgm (la mas corta)
if exist out\GAMEOVER.VGM (copy /y out\GAMEOVER.VGM "%DEST%\GAMEOVER.VGM" >nul || goto error) else (copy /y ..\VGM\ledstorm-2.vgm "%DEST%\GAMEOVER.VGM" >nul || goto error)
echo OK: copiado en %DEST%
exit /b 0

:error
echo *** ERROR ***
exit /b 1
