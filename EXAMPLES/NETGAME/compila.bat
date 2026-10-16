@echo off
rem Ensambla CONNECT4 y CHESS y los copia a las carpetas SD81\TOOLS\CONNECT4 y SD81\TOOLS\CHESS del emulador.
cd /d "%~dp0"
set BASE=\ClaudeCode\Eightyone2\EightyOne\SD81\SD81\TOOLS

pasmo connect4.asm connect4.bin connect4.sym || goto error
pasmo chess.asm chess.bin chess.sym || goto error
rem El ZX81 no ejecuta codigo en $8000-$FFFF: el programa tiene que acabar antes (chess en $5000, connect4 en $6000)
for %%F in (chess.bin) do if %%~zF GTR 12288 (echo *** chess.bin pasa de $8000 *** & goto error)
for %%F in (connect4.bin) do if %%~zF GTR 8192 (echo *** connect4.bin pasa de $8000 *** & goto error)
if not exist "%BASE%\CONNECT4" mkdir "%BASE%\CONNECT4"
if not exist "%BASE%\CHESS" mkdir "%BASE%\CHESS"
copy /y connect4.bin "%BASE%\CONNECT4" >nul || goto error
copy /y CONNECT4.B81 "%BASE%\CONNECT4" >nul || goto error
copy /y chess.bin "%BASE%\CHESS" >nul || goto error
copy /y CHESS.B81 "%BASE%\CHESS" >nul || goto error
echo OK: CONNECT4 y CHESS copiados en %BASE%
exit /b 0

:error
echo *** ERROR: se para aqui ***
exit /b 1
