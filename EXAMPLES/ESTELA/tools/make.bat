@echo off
rem Genera los datos de ESTELA (hoja de sprites, NIVEL1.I16, NIVEL1.MAP, LEVEL.INC) en ..\out. Con --regen regenera los graficos provisionales.
python "%~dp0make.py" %*
