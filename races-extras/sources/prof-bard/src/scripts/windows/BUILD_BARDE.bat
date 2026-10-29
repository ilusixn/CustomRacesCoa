@echo off
setlocal EnableExtensions EnableDelayedExpansion
rem Compile worldserver (noyau b95c4368, celui du build propre) avec mod-barde ajoute.
rem Tout est sur le disque M: ; seules les bibliotheques vcpkg restent dans C:\coa-build\vcpkg.
set "HERE=%~dp0"
set "ROOT=%~dp0.."
set "WORK=M:\coa-build-barde"
set "SRC=%WORK%\src"
set "BLD=%WORK%\build"
set "VCPKG=C:\coa-build\vcpkg"
set "LOG=%WORK%\build-log.txt"

echo ===== BUILD BARDE (build propre) %DATE% %TIME% ===== > "%LOG%"
echo Journal : %LOG%
echo.

echo [1/6] Recherche de Visual Studio...
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" ( echo ERREUR : Visual Studio introuvable & goto :fail )
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR ( echo ERREUR : le composant "Developpement Desktop en C++" n'est pas installe & goto :fail )
set "VSCMD_SKIP_SENDTELEMETRY=1"
call "%VSDIR%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul 2>&1
if errorlevel 1 ( echo ERREUR VsDevCmd >> "%LOG%" & goto :fail )
for %%t in (git cmake ninja) do (
  where %%t >> "%LOG%" 2>&1
  if errorlevel 1 ( echo ERREUR : %%t introuvable & goto :fail )
)

echo [2/6] Code du serveur : %SRC%...
if not exist "%SRC%\CMakeLists.txt" ( echo ERREUR : le code n'est pas dans %SRC% & goto :fail )
pushd "%SRC%"
git log --oneline -1 >> "%LOG%" 2>&1
rem Le serveur du gestionnaire est livre avec libmysql 8.4 : on n'exige que la meme version majeure (ACE00046).
git apply --check "%HERE%mysql-version.patch" >nul 2>&1 && git apply "%HERE%mysql-version.patch" >> "%LOG%" 2>&1
popd

echo [3/6] Copie de mod-barde dans les modules...
robocopy "%ROOT%\mod-barde" "%SRC%\modules\mod-barde" /MIR /XD addon tools docs /NFL /NDL /NP >> "%LOG%" 2>&1
if errorlevel 8 ( echo ERREUR copie du module & goto :fail )

echo [4/6] Bibliotheques vcpkg...
set "INST=%VCPKG%\installed\x64-windows"
if not exist "%INST%\lib\libmysql.lib" ( echo ERREUR : bibliotheques vcpkg absentes dans %VCPKG% & goto :fail )
set "MYINC=%INST%\include\mysql"
if not exist "%MYINC%\mysql.h" set "MYINC=%INST%\include"

echo [5/6] Configuration...
cmake -S "%SRC%" -B "%BLD%" -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE="%VCPKG%\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows ^
  -DSCRIPTS=static -DMODULES=static -DAPPS_BUILD=world-only -DTOOLS_BUILD=none ^
  -DMYSQL_INCLUDE_DIR="%MYINC%" -DMYSQL_LIBRARY="%INST%\lib\libmysql.lib" >> "%LOG%" 2>&1
if errorlevel 1 ( echo ERREUR cmake configure & goto :fail )

echo [6/6] Compilation de worldserver ^(30 a 90 minutes la premiere fois, quelques minutes ensuite^)...
cmake --build "%BLD%" --target worldserver >> "%LOG%" 2>&1
if errorlevel 1 ( echo ERREUR compilation & goto :fail )

set "OUT=%HERE%worldserver-barde"
if not exist "%OUT%" mkdir "%OUT%"
copy /y "%BLD%\bin\worldserver.exe" "%OUT%\" >> "%LOG%" 2>&1
copy /y "%BLD%\bin\*.dll" "%OUT%\" >> "%LOG%" 2>&1
echo.
echo TERMINE. Le serveur avec mod-barde est dans le dossier worldserver-barde.
echo TERMINE >> "%LOG%"
call :publish
pause
exit /b 0

:fail
echo.
echo ECHEC. Gardez cette fenetre ouverte et dites-moi : "le build barde a echoue".
echo ECHEC >> "%LOG%"
call :publish
pause
exit /b 1

:publish
set "STAMP=%TIME:~0,2%%TIME:~3,2%%TIME:~6,2%"
set "STAMP=%STAMP: =0%"
copy /y "%LOG%" "%HERE%build-log-%STAMP%.txt" >nul 2>&1
echo Copie du journal : build-log-%STAMP%.txt
exit /b 0
