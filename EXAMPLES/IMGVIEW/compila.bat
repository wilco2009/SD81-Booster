@echo off
rem Ensambla IMGVIEW y lo copia, como imgview.bin, a la carpeta del explorador del emulador y a SD Content, junto con los stubs BASIC
rem (AUTOEXEC.P y EXPLORER.P de SD Content, que ya lo lanzan con los .BMP y .I16) para que el explorador lo use.
cd /d "%~dp0"
set EMU=\ClaudeCode\Eightyone2\EightyOne\SD81
set SDC=..\..\SD Content

pasmo imgview.asm imgview.bin imgview.sym || goto error
if not exist "%EMU%\explorer" mkdir "%EMU%\explorer"
copy /y imgview.bin "%EMU%\explorer\imgview.bin" >nul || goto error
copy /y imgview.bin "%SDC%\explorer\imgview.bin" >nul || goto error
copy /y "%SDC%\explorer\EXPLORER.P" "%EMU%\explorer" >nul || goto error
copy /y "%SDC%\AUTOEXEC.P" "%EMU%" >nul || goto error
echo OK: imgview.bin y los stubs copiados en %EMU% y en SD Content
exit /b 0

:error
echo *** ERROR: se para aqui ***
exit /b 1
