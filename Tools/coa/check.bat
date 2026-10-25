@echo off
rem Build out\build\x64-Release and run both test suites (CoATests and Tools/coa unittest).
rem Output: the build log goes to out\check-build.log; the exit code is non-zero on any failure.
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cmake --build out\build\x64-Release > out\check-build.log 2>&1
if errorlevel 1 (
    powershell -NoProfile -Command "Get-Content out\check-build.log | Select-String -Pattern 'error|FAILED' | Select-Object -First 30"
    echo BUILD FAILED
    exit /b 1
)
echo BUILD OK
out\build\x64-Release\bin\CoATests.exe
if errorlevel 1 exit /b 1
python -m unittest discover -s Tools/coa/tests -t Tools/coa
if errorlevel 1 exit /b 1
