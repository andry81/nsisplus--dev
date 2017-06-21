@echo off

setlocal

rem clean PATH to windows 7 defaults
set "PATH=C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem;C:\Windows\System32\WindowsPowerShell\v1.0\"
set "PATH=%PATH%;c:\Python27\Scripts;c:\Python27"

rem registrate the Visual Studio 2005 C++ compiler
call "%%VS80COMNTOOLS%%\vsvars32.bat"

set "ZLIB_W32=c:\builds\zlib_win32"
rem set CCFLAGS=/SUBSYSTEM:CONSOLE
rem set MSDEVDIR=c:\Programs\Dev\Microsoft\Visual Studio 8.0

pushd "%~dp0"

call scons.bat MSVS_VERSION=8.0 TARGET_ARCH=x86 UNICODE=yes "SKIPUTILS=NSIS Menu"
rem "APPEND_CCFLAGS=/MT" "APPEND_LINKFLAGS=msvcrt.lib /NODEFAULTLIB:libcmt.lib"
if %ERRORLEVEL% EQU 0 (
 scons PREFIX="%~dp0nsis_install" install
)

popd

pause
