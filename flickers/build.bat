@echo off
rem Builds Flickers BeG0nE into dist\: the REFramework plugin (the RE4 fix) and the OpenXR layer (any OpenXR game),
rem then builds and runs the host tests (test\run_tests.bat) and stops if one fails. Needs Visual Studio 2022 Build Tools.
setlocal
set HERE=%~dp0
set VCVARS=
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`call "%VSWHERE%" -latest -products * -property installationPath`) do set "VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (echo Visual Studio Build Tools not found & exit /b 1)
call "%VCVARS%" >nul 2>&1
if not exist "%HERE%dist" mkdir "%HERE%dist"
if not exist "%HERE%build" mkdir "%HERE%build"

call "%HERE%test\run_tests.bat"
if errorlevel 1 (echo TESTS FAILED & exit /b 1)
