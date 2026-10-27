@echo off
rem Ensambla el visor de fotos PHOTO (FPGA de la rama fpga-mode160; el emulador no tiene el modo).
rem   compila.bat                      -> photo.bin
rem   compila.bat FOTO1.png FOTO2.png  -> ademas convierte los PNG a IMG001.I16, IMG002.I16... (con png2i16.py)
cd /d "%~dp0"
pasmo photo.asm photo.bin photo.sym || goto error
if not "%~1"=="" python png2i16.py %* || goto error
echo OK: photo.bin
echo Copia PHOTO.BIN, PHOTO.B81 y las IMGnnn.I16 a la misma carpeta de la SD y carga PHOTO.B81
exit /b 0

:error
echo *** ERROR ***
exit /b 1
