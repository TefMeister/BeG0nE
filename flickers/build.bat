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
if not exist "%HERE%build\layer" mkdir "%HERE%build\layer"
set CORE="%HERE%src\core\fb_core.cpp" "%HERE%src\core\fb_views.cpp" "%HERE%src\core\fb_gpu.cpp" "%HERE%src\core\fb_dump.cpp" "%HERE%src\core\fb_calm.cpp"
set LIBS=d3d12.lib dxgi.lib d3dcompiler.lib user32.lib

rem the OpenXR layer (any OpenXR game; the dev PC tests with it)
cl /nologo /O2 /EHsc /std:c++17 /LD /MT /W4 /I"%HERE%third_party" %CORE% "%HERE%src\layer\fb_layer.cpp" ^
   /Fo"%HERE%build\layer\\" /Fe"%HERE%dist\flickers_begone_layer.dll" /link /NOLOGO /IMPLIB:"%HERE%build\layer\fb_layer.lib" %LIBS% >"%HERE%build\layer.build.txt" 2>&1
if errorlevel 1 (type "%HERE%build\layer.build.txt" & echo LAYER BUILD FAILED & exit /b 1)
findstr /c:"warning" "%HERE%build\layer.build.txt"
copy /y "%HERE%src\layer\flickers_begone_layer.json" "%HERE%dist\" >nul

rem the REFramework plugin: the RE4 fix, one file for reframework\plugins\
if not exist "%HERE%build\plugin" mkdir "%HERE%build\plugin"
cl /nologo /O2 /EHsc /std:c++17 /LD /MT /W4 /I"%HERE%third_party" %CORE% "%HERE%src\plugin\fb_iat.cpp" "%HERE%src\plugin\fb_plugin.cpp" ^
   /Fo"%HERE%build\plugin\\" /Fe"%HERE%dist\flickers_begone.dll" /link /NOLOGO /IMPLIB:"%HERE%build\plugin\fb_plugin.lib" %LIBS% psapi.lib >"%HERE%build\plugin.build.txt" 2>&1
if errorlevel 1 (type "%HERE%build\plugin.build.txt" & echo PLUGIN BUILD FAILED & exit /b 1)
findstr /c:"warning" "%HERE%build\plugin.build.txt"

rem the test program (no game, no headset: run it on the OpenXR Simulator); delay-loads the loader like REFramework
if not exist "%HERE%build\testapp" mkdir "%HERE%build\testapp"
cl /nologo /O2 /EHsc /std:c++17 /MT /W4 /I"%HERE%third_party" "%HERE%test\xr_testapp.cpp" /Fo"%HERE%build\testapp\\" ^
   /Fe"%HERE%build\testapp\xr_testapp.exe" /link /NOLOGO "%HERE%third_party\openxr_lib\openxr_loader.lib" d3d12.lib dxgi.lib ^
   delayimp.lib /DELAYLOAD:openxr_loader.dll >"%HERE%build\testapp.build.txt" 2>&1
if errorlevel 1 (type "%HERE%build\testapp.build.txt" & echo TEST PROGRAM BUILD FAILED & exit /b 1)
findstr /c:"warning" "%HERE%build\testapp.build.txt"

call "%HERE%test\run_tests.bat"
if errorlevel 1 (echo TESTS FAILED & exit /b 1)
