@echo off
rem Ensambla EDIT y lo copia a la carpeta del explorador en el emulador.
cd /d "%~dp0"
set DEST=\ClaudeCode\Eightyone2\EightyOne\SD81\explorer

pasmo edit.asm edit.bin edit.sym || goto error
if not exist "%DEST%" mkdir "%DEST%"
copy /y edit.bin "%DEST%" >nul || goto error
echo OK: edit.bin copiado en %DEST%
exit /b 0

:error
echo *** ERROR: se para aqui ***
exit /b 1
