@echo off
rem Ensambla el SPORTS (Tennis, Hockey, Squash y Practice) y lo copia a SD81\NETGAMES\SPORTS (y a SPORTS2, la segunda instancia del emulador) del emulador.
cd /d "%~dp0"
set BASE=\ClaudeCode\Eightyone2\EightyOne\SD81\SD81\NETGAMES
pasmo sports.asm sports.bin sports.sym || goto error
rem El ZX81 no ejecuta codigo en $8000-$FFFF: el programa empieza en $4400 y tiene que acabar antes
for %%F in (sports.bin) do if %%~zF GTR 15360 goto big
if not exist "%BASE%\SPORTS" mkdir "%BASE%\SPORTS"
copy /y sports.bin "%BASE%\SPORTS" >nul || goto error
copy /y SPORTS.B81 "%BASE%\SPORTS" >nul || goto error
if not exist "%BASE%\SPORTS2" mkdir "%BASE%\SPORTS2"
copy /y sports.bin "%BASE%\SPORTS2" >nul || goto error
copy /y SPORTS.B81 "%BASE%\SPORTS2" >nul || goto error
echo OK: SPORTS copiado en %BASE%\SPORTS y SPORTS2
exit /b 0

:big
echo *** sports.bin pasa de $8000 ***
:error
echo *** ERROR: se para aqui ***
exit /b 1
