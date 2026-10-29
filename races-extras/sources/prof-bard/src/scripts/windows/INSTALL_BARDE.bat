@echo off
setlocal DisableDelayedExpansion
title Barde - installation dans la copie du serveur propre
set "HERE=%~dp0"
set "ROOT=%~dp0.."
set "CLEAN=M:\conquest\CoA Server"
set "REPACK=M:\conquest\CoA Server Barde"
set "CLIENT=M:\conquest\conquest"
if not exist "%REPACK%\Scripts\manage.py" ( echo ERREUR : lancez d'abord COPIER_SERVEUR.bat & pause & exit /b 1 )
if not exist "%HERE%worldserver-barde\worldserver.exe" ( echo ERREUR : lancez d'abord BUILD_BARDE.bat & pause & exit /b 1 )
echo Les serveurs utilisent les memes ports : arret du serveur propre et de la copie.
"%CLEAN%\Runtime\python\python.exe" -B "%CLEAN%\Scripts\manage.py" stop-all
"%REPACK%\Runtime\python\python.exe" -B "%HERE%install_barde.py" "%REPACK%" "%HERE%worldserver-barde" "%ROOT%\mod-barde" "%CLIENT%"
if errorlevel 1 ( pause & exit /b 1 )
echo.
echo Quand "ready" apparait, attendez 1 minute puis lancez le jeu depuis %CLIENT%\Ascension.exe
echo En jeu : .barde luth  puis  /barde
pause
