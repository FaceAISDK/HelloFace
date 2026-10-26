@echo off
setlocal
cd /d "%~dp0"
if not exist "bin\hiface_demo.exe" (
  echo Please extract the complete ZIP before running this launcher.
  pause
  exit /b 1
)
start "HiFace Windows Demo" /wait "%~dp0bin\hiface_demo.exe"
if errorlevel 1 (
  echo Demo failed. Run self_test.bat and check %%LOCALAPPDATA%%\HiFaceDemo\logs.
  pause
  exit /b 1
)
