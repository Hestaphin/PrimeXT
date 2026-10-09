@echo off
setlocal
cd /d "%~dp0"
if not exist "build\Release\primext-weapon-validate.exe" if not exist "build\primext-weapon-validate.exe" (
  cmake -S . -B build
  if errorlevel 1 exit /b 1
  cmake --build build --config Release
  if errorlevel 1 exit /b 1
)
py launch.py %*
