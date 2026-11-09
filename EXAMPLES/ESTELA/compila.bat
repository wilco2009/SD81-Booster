@echo off
rem Ensambla ESTELA y lo copia al emulador con el nivel, los sprites y la musica (out\NIVEL1.I16, NIVEL1.MAP, HOJA.BIN y NIVEL1.VGM).
cd /d "%~dp0"
set DEST=C:\ClaudeCode\Eightyone2\EightyOne\SD81\SD81\GAMES\ESTELA

pasmo estela.asm estela.bin estela.sym || goto error
rem El ZX81 no ejecuta codigo en $8000-$FFFF; $6F60-$7FFF son las letras W N I L D y la hoja de los sprites: el binario
rem (desde $4800; PALBUF y la paleta del titulo estan en la pagina del nivel, no aqui) no puede pasar de $6F60 - $4800 = 10080 bytes
for %%F in (estela.bin) do if %%~zF GTR 10080 (echo *** estela.bin pasa de 10080 bytes: no cabe antes de $6F60 *** & goto error)
if not exist "%DEST%" mkdir "%DEST%"
copy /y estela.bin "%DEST%\ESTELA.BIN" >nul || goto error
copy /y ESTELA.B81 "%DEST%\ESTELA.B81" >nul || goto error
rem el nivel (lo genera tools\make.py en out\): NIVEL1.MAP va a la pagina del nivel (bloque 1); si pasa de 7 KB, el juego
rem vuelve con el error 5
copy /y out\NIVEL1.I16 "%DEST%\NIVEL1.I16" >nul || goto error
copy /y out\NIVEL1.MAP "%DEST%\NIVEL1.MAP" >nul || goto error
copy /y out\HOJA.BIN "%DEST%\HOJA.BIN" >nul || goto error
rem el titulo (opcional: sin el, el juego empieza directamente): out\TITULO.I16 si existe. ESTELA.REC (el record) lo crea el juego
if exist out\TITULO.I16 (copy /y out\TITULO.I16 "%DEST%\TITULO.I16" >nul || goto error)
rem los niveles 2 a 5 (fase 8a): out\NIVELn.I16 y out\NIVELn.MAP si existen los dos (los hace tools\make.py); out\NIVELn.VGM, si existe,
rem es la musica de ese nivel (si falta, suena la del 1). Si falta un nivel, tras el jefe se vuelve al 1 con una vuelta mas de dificultad
for %%N in (2 3 4 5) do (
  if exist out\NIVEL%%N.I16 if exist out\NIVEL%%N.MAP (
    copy /y out\NIVEL%%N.I16 "%DEST%\NIVEL%%N.I16" >nul || goto error
    copy /y out\NIVEL%%N.MAP "%DEST%\NIVEL%%N.MAP" >nul || goto error
    if exist out\NIVEL%%N.VGM (copy /y out\NIVEL%%N.VGM "%DEST%\NIVEL%%N.VGM" >nul || goto error)
  )
)
rem la musica del nivel (opcional: sin ella el juego va sin musica): out\NIVEL1.VGM si existe; si no, de momento
rem la de la prueba SONIDO de la fase 0 (..\VGM\music0002.vgm)
if exist out\NIVEL1.VGM (copy /y out\NIVEL1.VGM "%DEST%\NIVEL1.VGM" >nul || goto error) else (copy /y ..\VGM\music0002.vgm "%DEST%\NIVEL1.VGM" >nul || goto error)
rem la del fin de partida (suena una vez): out\GAMEOVER.VGM si existe; si no, de momento ..\VGM\ledstorm-2.vgm (la mas corta)
if exist out\GAMEOVER.VGM (copy /y out\GAMEOVER.VGM "%DEST%\GAMEOVER.VGM" >nul || goto error) else (copy /y ..\VGM\ledstorm-2.vgm "%DEST%\GAMEOVER.VGM" >nul || goto error)
rem las de la fase 8d (todas opcionales; si falta una, se sigue con la del nivel o sin musica): TITULO.VGM en bucle en el titulo, JEFE.VGM en
rem bucle desde el aviso WARNING hasta que el jefe explota y FINAL.VGM una vez en el WELL DONE del ultimo nivel. out\NAME.VGM si existe; si no,
rem de momento ledstorm-3 (titulo), ledstorm-1 (jefe) y ledstorm-2 (final) de ..\VGM
if exist out\TITULO.VGM (copy /y out\TITULO.VGM "%DEST%\TITULO.VGM" >nul || goto error) else (copy /y ..\VGM\ledstorm-3.vgm "%DEST%\TITULO.VGM" >nul || goto error)
if exist out\JEFE.VGM (copy /y out\JEFE.VGM "%DEST%\JEFE.VGM" >nul || goto error) else (copy /y ..\VGM\ledstorm-1.vgm "%DEST%\JEFE.VGM" >nul || goto error)
if exist out\FINAL.VGM (copy /y out\FINAL.VGM "%DEST%\FINAL.VGM" >nul || goto error) else (copy /y ..\VGM\ledstorm-2.vgm "%DEST%\FINAL.VGM" >nul || goto error)
echo OK: copiado en %DEST%
exit /b 0

:error
echo *** ERROR ***
exit /b 1
