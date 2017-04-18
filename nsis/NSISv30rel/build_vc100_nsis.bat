@echo off

setlocal

rem clean PATH to windows 7 defaults
set "PATH=C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem;C:\Windows\System32\WindowsPowerShell\v1.0\"
set "PATH=%PATH%;c:\Python27\Scripts"

rem registrate the Visual Studio 2010 C++ compiler
call "%%VS100COMNTOOLS%%\vsvars32.bat"

set "ZLIB_W32=c:\builds\zlib_win32"
rem set CCFLAGS=/SUBSYSTEM:CONSOLE
rem set MSDEVDIR=c:\Program Files\Microsoft\Visual Studio 8.0

pushd "%~dp0"

call scons.bat MSVS_VERSION=10.0 TARGET_ARCH=x86 UNICODE=yes "SKIPUTILS=NSIS Menu"
rem "APPEND_CCFLAGS=/MT" "APPEND_LINKFLAGS=msvcrt.lib /NODEFAULTLIB:libcmt.lib"
if %ERRORLEVEL% EQU 0 (
 scons PREFIX="%~dp0nsis_install" install
)

popd

pause
