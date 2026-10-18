@echo off
rem Ensambla CONNECT4, CHESS y SCRABBLE y los copia a las carpetas SD81\NETGAMES\CONNECT4, \CHESS y \SCRABBLE del emulador.
cd /d "%~dp0"
set BASE=\ClaudeCode\Eightyone2\EightyOne\SD81\SD81\NETGAMES

pasmo connect4.asm connect4.bin connect4.sym || goto error
pasmo chess.asm chess.bin chess.sym || goto error
pasmo scrabble.asm scrabble.bin scrabble.sym || goto error
rem El ZX81 no ejecuta codigo en $8000-$FFFF: el programa tiene que acabar antes (scrabble en $4400, chess en $5000, connect4 en $6000)
for %%F in (scrabble.bin) do if %%~zF GTR 15360 (echo *** scrabble.bin pasa de $8000 *** & goto error)
for %%F in (chess.bin) do if %%~zF GTR 12288 (echo *** chess.bin pasa de $8000 *** & goto error)
for %%F in (connect4.bin) do if %%~zF GTR 8192 (echo *** connect4.bin pasa de $8000 *** & goto error)
if not exist "%BASE%\CONNECT4" mkdir "%BASE%\CONNECT4"
if not exist "%BASE%\CHESS" mkdir "%BASE%\CHESS"
if not exist "%BASE%\SCRABBLE" mkdir "%BASE%\SCRABBLE"
copy /y connect4.bin "%BASE%\CONNECT4" >nul || goto error
copy /y CONNECT4.B81 "%BASE%\CONNECT4" >nul || goto error
copy /y chess.bin "%BASE%\CHESS" >nul || goto error
copy /y CHESS.B81 "%BASE%\CHESS" >nul || goto error
copy /y scrabble.bin "%BASE%\SCRABBLE" >nul || goto error
copy /y SCRABBLE.B81 "%BASE%\SCRABBLE" >nul || goto error
rem la segunda instancia del emulador (con su propia partida guardada), si existe la carpeta
if exist "%BASE%\SCRABBLE2" copy /y scrabble.bin "%BASE%\SCRABBLE2" >nul
if exist "%BASE%\SCRABBLE2" copy /y SCRABBLE.B81 "%BASE%\SCRABBLE2" >nul
rem los diccionarios (TOOLS\make_dict.py, en ..\..\build\dict) solo si faltan: pesan 2,7 y 9,7 MB
if not exist "%BASE%\SCRABBLE\DICTEN.DIC" if exist "..\..\build\dict\DICTEN.DIC" copy /y "..\..\build\dict\DICTEN.DIC" "%BASE%\SCRABBLE" >nul
if not exist "%BASE%\SCRABBLE\DICTES.DIC" if exist "..\..\build\dict\DICTES.DIC" copy /y "..\..\build\dict\DICTES.DIC" "%BASE%\SCRABBLE" >nul
echo OK: CONNECT4, CHESS y SCRABBLE copiados en %BASE%
exit /b 0

:error
echo *** ERROR: se para aqui ***
exit /b 1
