@echo off

rmdir /S /Q "%~dp0build"
rmdir /S /Q "%~dp0.sconf_temp"
del /F /Q /A:-D "%~dp0.sconsign.dblite"
