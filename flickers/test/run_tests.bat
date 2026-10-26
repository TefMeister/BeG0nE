@echo off
rem Builds and runs every host test in this folder (no GPU, no headset). Expects the Visual Studio tools already set up
rem (build.bat calls this after vcvars64). Prints ALL TESTS PASS, or stops at the first test that fails.
setlocal
set T=%~dp0
set OUT=%T%..\build\test
if not exist "%OUT%" mkdir "%OUT%"
for %%f in ("%T%*_test.cpp") do (
    cl /nologo /EHsc /std:c++17 /W4 /O1 /I"%T%..\third_party" "%%f" /Fo"%OUT%\\" /Fe"%OUT%\%%~nf.exe" >"%OUT%\%%~nf.build.txt" 2>&1
    if errorlevel 1 (type "%OUT%\%%~nf.build.txt" & echo BUILD FAILED: %%~nf & exit /b 1)
    "%OUT%\%%~nf.exe"
    if errorlevel 1 (echo TEST FAILED: %%~nf & exit /b 1)
)
python -I "%T%..\tools\test_flicker_read.py" >"%OUT%\reader_test.txt" 2>&1
if errorlevel 1 (type "%OUT%\reader_test.txt" & echo TEST FAILED: reader & exit /b 1)
echo reader_test: OK
echo ALL TESTS PASS
