@echo off
setlocal

set "ROOT=%~dp0"
set "ROOT=%ROOT:~0,-1%"
set "BUILD=%ROOT%\out\build\x64-Release"
set "BIN=%ROOT%\bin"
set "STAGE=%TEMP%\coa64_pkg"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

for /f "usebackq delims=" %%i in (`git -C "%ROOT%" rev-parse --short^=7 HEAD`) do set "HASH=%%i"
if not defined HASH goto :nohash

git -C "%ROOT%" diff --quiet HEAD
if errorlevel 1 echo WARNING: uncommitted changes, the zip will not match commit %HASH%

if not exist "%VSWHERE%" goto :novs
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -version [17.0^,18.0^) -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR goto :novs

set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 goto :novs

if exist "%BUILD%\build.ninja" goto :build
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -S "%ROOT%" -B "%BUILD%"
if errorlevel 1 goto :fail

:build
cmake --build "%BUILD%" --target Wow XvidDecoder
if errorlevel 1 goto :fail

rem DXVK's x64 d3d9.dll (ThirdParty/dxvk): only when configure set up the target (WOWDECOMP_BUILD_DXVK and the tools).
findstr /b /c:"build dxvk:" "%BUILD%\build.ninja" >nul
if errorlevel 1 goto :nodxvk
cmake --build "%BUILD%" --target dxvk
if errorlevel 1 goto :fail
:nodxvk

if not exist "%BIN%\Win\AFK64.exe" goto :missing
if not exist "%BIN%\Win\AFK64.map" goto :missing
if not exist "%BIN%\Win\XvidDecoder.dll" goto :missing

if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%"
copy /y "%BIN%\Win\AFK64.exe" "%STAGE%\AFK64.exe" >nul
copy /y "%BIN%\Win\AFK64.map" "%STAGE%\AFK64.map" >nul
copy /y "%BIN%\Win\XvidDecoder.dll" "%STAGE%\XvidDecoder.dll" >nul
if exist "%BIN%\Win\x64\d3d9.dll" (
    mkdir "%STAGE%\x64"
    copy /y "%BIN%\Win\x64\d3d9.dll" "%STAGE%\x64\d3d9.dll" >nul
) else (
    echo WARNING: %BIN%\Win\x64\d3d9.dll missing, the zip has no DXVK
)

set "ZIP=%BIN%\AFK64-%HASH%.zip"
powershell -NoProfile -Command "Compress-Archive -Path '%STAGE%\*' -DestinationPath '%ZIP%' -Force"
if errorlevel 1 goto :fail
rmdir /s /q "%STAGE%"

echo Created %ZIP%
exit /b 0

:nohash
echo ERROR: could not read the git commit hash
exit /b 1

:novs
echo ERROR: Visual Studio 2022 with the C++ x64 tools was not found
exit /b 1

:missing
echo ERROR: build output missing in %BIN%\Win
exit /b 1

:fail
echo ERROR: build or packaging failed
exit /b 1

