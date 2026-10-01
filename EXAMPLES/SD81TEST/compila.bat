@echo off
rem Ensambla SD81TEST (nucleo y modulos) y copia los binarios (y el VGM
rem de la prueba de sonido) al emulador.
rem El nucleo va primero: genera sd81test.sym, que incluyen los modulos.
cd /d "%~dp0"
set DEST=C:\ClaudeCode\Eightyone2\EightyOne\SD81\SD81\TOOLS\SD81TEST

pasmo sd81test.asm sd81test.bin sd81test.sym || goto error
pasmo sd81mem.asm sd81mem.bin sd81mem.sym || goto error
pasmo sd81sys.asm sd81sys.bin sd81sys.sym || goto error
pasmo sd81av.asm sd81av.bin sd81av.sym || goto error
pasmo sd81net.asm sd81net.bin sd81net.sym || goto error

copy /y sd81test.bin "%DEST%" >nul || goto error
copy /y sd81mem.bin "%DEST%" >nul || goto error
copy /y sd81sys.bin "%DEST%" >nul || goto error
copy /y sd81av.bin "%DEST%" >nul || goto error
copy /y sd81net.bin "%DEST%" >nul || goto error
copy /y SD81TEST.VGM "%DEST%" >nul || goto error
echo OK: binarios copiados en %DEST%
exit /b 0

:error
echo *** ERROR: se para aqui ***
exit /b 1
