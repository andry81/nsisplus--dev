@echo off

setlocal

call "%%~dp0..\build.impl.bat" "%%~dp0test.exe" "%%~dp0main.nsi" "%%~dp0"
