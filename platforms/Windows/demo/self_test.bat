@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0diagnose.ps1"
set "HiFaceResult=%ERRORLEVEL%"
echo.
echo Test exit code: %HiFaceResult% ^(0 = passed^)
echo Reports: %%LOCALAPPDATA%%\HiFaceDemo\logs
pause
exit /b %HiFaceResult%
