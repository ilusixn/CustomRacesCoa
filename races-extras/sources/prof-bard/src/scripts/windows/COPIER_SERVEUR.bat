@echo off
setlocal DisableDelayedExpansion
title Barde - copie du serveur propre
set "SRC=M:\conquest\CoA Server"
set "DST=M:\conquest\CoA Server Barde"
if exist "%DST%\Scripts\manage.py" (
  echo La copie existe deja : "%DST%"
  pause
  exit /b 0
)
if not exist "%SRC%\Scripts\manage.py" ( echo ERREUR : "%SRC%" introuvable & pause & exit /b 1 )
echo Arret du serveur propre pour copier une base de donnees coherente...
"%SRC%\Runtime\python\python.exe" -B "%SRC%\Scripts\manage.py" stop-all
echo Copie de "%SRC%" vers "%DST%" (environ 9 Go, plusieurs minutes)...
robocopy "%SRC%" "%DST%" /E /R:1 /W:1 /NFL /NDL /NP /XD "%SRC%\Core\Logs" "%SRC%\mysql\logs" "%SRC%\BugReport\Logs"
if errorlevel 8 ( echo ERREUR robocopy & pause & exit /b 1 )
echo.
echo TERMINE : %DST%
pause
