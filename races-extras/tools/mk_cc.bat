@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\CoA-Build\tools
cl /nologo /EHsc /Zi crashcatch.cpp /link /out:crashcatch.exe > cc-build.log 2>&1
echo EXIT %ERRORLEVEL% >> cc-build.log
