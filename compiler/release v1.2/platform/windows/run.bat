@echo off
setlocal
if exist "C:\msys64\ucrt64\bin\gcc.exe" set "PATH=C:\msys64\ucrt64\bin;%PATH%"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0run.ps1" %*
exit /b %ERRORLEVEL%
