@echo off

setlocal

if not exist "%~dp0configure.user.bat" ( call "%%~dp0configure.bat" || exit /b 255 )

call "%%~dp0configure.user.bat"

"%MAKENSIS_EXE%" /V4 "/Obuild.log" "/DPROJECT_ROOT=%PROJECT_ROOT%" "/DTEST_LIB_ROOT=%TEST_LIB_ROOT%" "/DBUILD_DIR_NAME=%BUILD_DIR_NAME%" "/XOutFile '%~1'" "%~2"

echo.Return code: %ERRORLEVEL%

pause
