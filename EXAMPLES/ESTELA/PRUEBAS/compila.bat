@echo off
rem Ensambla las pruebas de la fase 0 de ESTELA y las copia al emulador.
cd /d "%~dp0"
set DEST=C:\ClaudeCode\Eightyone2\EightyOne\SD81\test\ESTELA
if not exist "%DEST%" mkdir "%DEST%"

cd CARGA
pasmo carga.asm carga.bin carga.sym || goto error
copy /y carga.bin "%DEST%\CARGA.BIN" >nul || goto error
copy /y CARGA.B81 "%DEST%\CARGA.B81" >nul || goto error
cd ..\VSYNC
pasmo vsync.asm vsync.bin vsync.sym || goto error
copy /y vsync.bin "%DEST%\VSYNC.BIN" >nul || goto error
copy /y VSYNC.B81 "%DEST%\VSYNC.B81" >nul || goto error
cd ..\SONIDO
pasmo sonido.asm sonido.bin sonido.sym || goto error
copy /y sonido.bin "%DEST%\SONIDO.BIN" >nul || goto error
copy /y SONIDO.B81 "%DEST%\SONIDO.B81" >nul || goto error
copy /y ..\..\..\VGM\music0002.vgm "%DEST%\MUSICA.VGM" >nul || goto error
cd ..\PAGINA
pasmo pagina.asm pagina.bin pagina.sym || goto error
copy /y pagina.bin "%DEST%\PAGINA.BIN" >nul || goto error
copy /y PAGINA.B81 "%DEST%\PAGINA.B81" >nul || goto error
cd ..
echo OK: copiado en %DEST%
exit /b 0

:error
echo *** ERROR ***
exit /b 1
