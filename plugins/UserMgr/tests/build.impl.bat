@echo off

setlocal

call :GET_FILE_NAME TEST_TITLE "%~dpf3"

if not exist "%~dp0configure.user.bat" ( call "%%~dp0configure.bat" || exit /b 255 )

call "%%~dp0configure.user.bat"

"%MAKENSIS_EXE%" /V4 "/Obuild.log" "/DTEST_TITLE=%TEST_TITLE%" "/DPROJECT_ROOT=%PROJECT_ROOT%" "/DTEST_LIB_ROOT=%TEST_LIB_ROOT%" "/DBUILD_DIR_NAME=%BUILD_DIR_NAME%" "/XOutFile '%~1'" "%~2"

echo.Return code: %ERRORLEVEL%

pause

exit /b %ERRORLEVEL%

:GET_FILE_NAME
set "FILE_NAME=%~2"
if "%FILE_NAME:~-1%" == "\" set "FILE_NAME=%FILE_NAME:~0,-1%"

call :GET_FILE_NAME_IMPL %1 "%FILE_NAME%"

exit /b 0

:GET_FILE_NAME_IMPL
set "%~1=%~nx2"
