@echo off
rem Builds the Skinshifter client patch (class 33 in the class list, class table, colours and icon) from the
rem CoA client's own newest files, into <game folder>\Data\patch-ZZZZZZ-SKIN.MPQ. Close the game first.
setlocal
set "PY=<repack folder>\Runtime\python\python.exe"
tasklist /fi "imagename eq Ascension64.exe" | find /i "Ascension64.exe" >nul && (echo Close the game first. & pause & exit /b 1)
tasklist /fi "imagename eq Ascension32.exe" | find /i "Ascension32.exe" >nul && (echo Close the game first. & pause & exit /b 1)
tasklist /fi "imagename eq Ascension.exe" | find /i "Ascension.exe" >nul && (echo Close the game first. & pause & exit /b 1)
"%PY%" "%~dp0mkskinclient.py" "<game folder>\Data" "<game folder>\Data\patch-ZZZZZZ-SKIN.MPQ"
if errorlevel 1 (echo. & echo Building the client patch failed - see the messages above. & pause & exit /b 1)
echo.
echo To remove Skinshifter from the client again: delete <game folder>\Data\patch-ZZZZZZ-SKIN.MPQ
pause
