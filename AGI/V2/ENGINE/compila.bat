@echo off
rem Ensambla el visor de imagenes AGI y lo deja en la SD del emulador:
rem   compila.bat KQ1      (necesita las imagenes convertidas: python -I TOOLS\agi2hr.py KQ1 dump\KQ1\hr, desde AGI\V2)
rem   SD81\SD81\AGI\V2\AGIVIEW.BIN                   el programa, comun a todos los juegos
rem   SD81\SD81\AGI\V2\KQ1\AGIVIEW.B81 y PICnnn.GFX   el BASIC que lo carga y las imagenes (la carpeta actual debe ser la del juego)
rem Cambia BASE si la SD del emulador esta en otro sitio.
set BASE=C:\ClaudeCode\Eightyone2\EightyOne\SD81\SD81
set G=%1
if "%G%"=="" set G=KQ1
cd /d "%~dp0"
pasmo agiview.asm agiview.bin agiview.sym || goto error
set S=%BASE%\AGI\V2
set D=%S%\%G%
if not exist "%D%" mkdir "%D%"
copy /y agiview.bin "%S%\AGIVIEW.BIN" >nul || goto error
copy /y AGIVIEW.B81 "%D%\AGIVIEW.B81" >nul || goto error
copy /y ..\dump\%G%\hr\*.GFX "%D%" >nul || goto error
echo OK: %S%\AGIVIEW.BIN y el juego en %D%
exit /b 0

:error
echo *** ERROR ***
exit /b 1
