@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Compile.ps1" %*
set "result=%errorlevel%"
if not defined OPENUCC_NO_PAUSE pause
exit /b %result%
